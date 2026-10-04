"""Confirm/apply HTTP flow using disposable SQLite saves, never player saves."""

import json
from contextlib import closing
from http.server import ThreadingHTTPServer
from pathlib import Path
import sqlite3
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_save_application import apply_result, save_stamp
from breeding_web import Workbench, make_handler


def write_save(path, ids):
    with closing(sqlite3.connect(path)) as db, db:
        db.execute("CREATE TABLE cats(key INTEGER PRIMARY KEY, data BLOB)")
        db.execute("CREATE TABLE files(key TEXT PRIMARY KEY, data BLOB)")
        db.executemany("INSERT INTO cats VALUES(?, ?)", [(key, bytes([key])) for key in ids])
        db.execute("INSERT INTO files VALUES('house_state', ?)", (b"fixture-house",))


def cat_ids(path):
    with closing(sqlite3.connect(path)) as db:
        return [row[0] for row in db.execute("SELECT key FROM cats ORDER BY key")]


class ApplyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        self.source = self.folder / "steamcampaign01.sav"
        self.trained = self.folder / "trained.sav"
        self.report = self.folder / "report.json"
        write_save(self.source, [1, 2])
        write_save(self.trained, [1, 3])
        self.report.write_text('{"passed": false}', encoding="utf-8")
        self.job = {
            "status": "completed", "source": str(self.source),
            "source_stamp": save_stamp(self.source), "report": str(self.report),
            "preview": {"target": str(self.source), "added": [{"id": 3}], "removed": [{"id": 2}]},
            "export": {"file": str(self.trained), "imported_ids": [3], "removed_population_ids": [2]},
        }
        self.closed = patch("breeding_save_application.game_is_running", return_value=False)
        self.closed.start()

    def tearDown(self):
        self.closed.stop()
        self.temporary.cleanup()

    def test_apply_backs_up_replaces_and_is_repeatable(self):
        result = apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 3])
        self.assertEqual(cat_ids(result["backup"]), [1, 2])
        self.assertEqual(json.loads(self.report.read_text())["application"]["status"], "applied")
        self.assertEqual(json.loads((self.folder / "application.json").read_text())["removed_population_ids"], [2])
        self.assertEqual(apply_result(self.job), result)
        self.assertEqual(cat_ids(result["backup"]), [1, 2])
        self.assertFalse(list(self.folder.glob(".autocattery-*.tmp")))

    def test_running_game_blocks_write(self):
        with patch("breeding_save_application.game_is_running", return_value=True):
            with self.assertRaisesRegex(ValueError, "退出 Mewgenics"):
                apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 2])
        self.assertFalse((self.folder / "before-apply.sav").exists())

    def test_changed_source_blocks_write(self):
        with closing(sqlite3.connect(self.source)) as db, db:
            db.execute("INSERT INTO cats VALUES(4, ?)", (b"later",))
        with self.assertRaisesRegex(ValueError, "原存档已"):
            apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 2, 4])

    def test_game_started_during_preparation_blocks_replace(self):
        with patch("breeding_save_application.game_is_running", side_effect=[False, True]):
            with self.assertRaisesRegex(ValueError, "退出 Mewgenics"):
                apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 2])
        self.assertEqual(cat_ids(self.folder / "before-apply.sav"), [1, 2])
        self.assertFalse(list(self.folder.glob(".autocattery-*.tmp")))

    def test_failed_replace_preserves_original_and_backup(self):
        with patch("breeding_save_application.os.replace", side_effect=PermissionError("busy")):
            with self.assertRaises(PermissionError):
                apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 2])
        self.assertEqual(cat_ids(self.folder / "before-apply.sav"), [1, 2])
        self.assertNotIn("applied", self.job)

    def test_empty_and_unfinished_results_are_not_applied(self):
        self.job["status"] = "running"
        with self.assertRaisesRegex(ValueError, "完成培养"):
            apply_result(self.job)
        self.job["status"] = "completed"
        self.job["export"] = None
        with self.assertRaisesRegex(ValueError, "完成培养"):
            apply_result(self.job)
        self.assertEqual(cat_ids(self.source), [1, 2])

    def test_http_requires_confirmation_and_current_result(self):
        with patch("breeding_web.discover_saves", return_value=[self.source]):
            app = Workbench(self.folder, self.folder)
        app.job = self.job
        server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(app))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        address = f"http://127.0.0.1:{server.server_port}"

        def request(options, token=None):
            headers = {"Content-Type": "application/json", "X-AutoCattery-Token": token or app.token}
            call = Request(address + "/api/apply", json.dumps(options).encode(), headers)
            try:
                with urlopen(call) as response:
                    return response.status, json.loads(response.read())
            except HTTPError as error:
                return error.code, json.loads(error.read())

        try:
            self.assertEqual(request({"confirmed": True, "result": str(self.report)}, "invalid")[0], 403)
            self.assertEqual(request({"result": str(self.report)})[0], 400)
            self.assertEqual(request({"confirmed": True, "result": "old-result"})[0], 400)
            self.assertEqual(cat_ids(self.source), [1, 2])
            # A browser cannot redirect the write to a different path.
            code, response = request({"confirmed": True, "result": str(self.report), "source": "other.sav"})
            self.assertEqual(code, 200)
            self.assertEqual(response["application"]["target"], str(self.source))
            self.assertEqual(cat_ids(self.source), [1, 3])
            self.assertEqual(request({"confirmed": True, "result": str(self.report)})[0], 200)
        finally:
            server.shutdown()
            worker.join()
            server.server_close()

    def test_game_running_blocks_simulation_start(self):
        with patch("breeding_web.discover_saves", return_value=[self.source]):
            app = Workbench(self.folder, self.folder)
        with patch("breeding_save_application.game_is_running", return_value=True):
            with self.assertRaisesRegex(ValueError, "退出 Mewgenics"):
                app.start({"save": 0})
        self.assertIsNone(app.thread)


if __name__ == "__main__":
    unittest.main()
