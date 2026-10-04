"""Target pair planning and assist persistence across workbench restarts."""

import argparse
from http.server import ThreadingHTTPServer
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_save import read_snapshot
from breeding_web import Workbench, make_handler


class PlanningTests(unittest.TestCase):
    def plan(self, target, rooms=3, candidates=5, suppressed=False):
        simulation = NativeDailySimulation.__new__(NativeDailySimulation)
        simulation.config = SimulationConfig(breeding_pairs=target)
        simulation.room_ids = [str(index) for index in range(rooms)]
        simulation.cats = {key: key for key in range(1, 13)}
        simulation.cat_rooms = {key: "0" for key in simulation.cats}
        simulation._dead = lambda cat: False
        simulation._adult = lambda cat: cat != 12
        pairs = [(key, key + 1) for key in range(1, candidates * 2, 2)]
        simulation.select_pairs = lambda count: pairs[:count]
        effects = {room: {"Comfort": int(room), "BreedSuppression": int(suppressed)}
                   for room in simulation.room_ids}
        return simulation, simulation.plan(effects)

    def test_four_pairs_use_two_breeding_rooms_and_leave_nursery(self):
        simulation, selected = self.plan(4)
        self.assertEqual(len(selected), 4)
        for first, second in selected:
            self.assertEqual(simulation.cat_rooms[first], simulation.cat_rooms[second])
        self.assertEqual({simulation.cat_rooms[key] for pair in selected for key in pair}, {"1", "2"})
        self.assertEqual(simulation.cat_rooms[12], "0")

    def test_one_two_and_five_targets_are_respected(self):
        for target in (1, 2, 5):
            with self.subTest(target=target):
                _, selected = self.plan(target)
                self.assertEqual(len(selected), target)

    def test_insufficient_candidates_and_suppression_remain_real(self):
        _, selected = self.plan(4, candidates=2)
        self.assertEqual(len(selected), 2)
        _, selected = self.plan(4, suppressed=True)
        self.assertEqual(selected, [])
        _, selected = self.plan(4, rooms=1)
        self.assertEqual(len(selected), 4)


class PreferenceTests(unittest.TestCase):
    def test_http_changes_survive_new_server_port_and_keep_false(self):
        with tempfile.TemporaryDirectory() as folder, patch("breeding_web.discover_saves", return_value=[]), \
                patch("breeding_web.game_is_running", return_value=False):
            def serve():
                app = Workbench(folder, folder)
                server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(app))
                worker = threading.Thread(target=server.serve_forever, daemon=True)
                worker.start()
                return app, server, worker, f"http://127.0.0.1:{server.server_port}"

            app, server, worker, url = serve()
            try:
                self.assertEqual(app.state()["preferences"], {"assist": True, "food_assist": True})
                for selected in ({"assist": False, "food_assist": False},
                                 {"assist": False, "food_assist": True}):
                    request = Request(url + "/api/preferences", json.dumps(selected).encode(),
                                      {"Content-Type": "application/json", "X-AutoCattery-Token": app.token})
                    with urlopen(request) as response:
                        self.assertTrue(json.load(response)["ok"])
                with urlopen(url + "/api/state") as response:
                    self.assertEqual(json.load(response)["preferences"], selected)
            finally:
                server.shutdown()
                server.server_close()
                worker.join()
            app, server, worker, url = serve()
            try:
                with urlopen(url + "/api/state") as response:
                    self.assertEqual(json.load(response)["preferences"], {"assist": False, "food_assist": True})
                app.saves = [Path(folder) / "source.sav"]
                with patch("breeding_web.require_game_closed"), patch("breeding_web.threading.Thread") as thread:
                    app.start({"save": 0, "pairs": 4})
                    self.assertEqual(thread.call_args.kwargs["args"][2], 4)
                    self.assertEqual(thread.call_args.kwargs["args"][4:], (False, True))
            finally:
                server.shutdown()
                server.server_close()
                worker.join()


def native_check(game, source):
    snapshot = read_snapshot(source, game / "resources.gpak")
    simulation = NativeDailySimulation(game / "Mewgenics.exe", game / "resources.gpak", source, snapshot,
        SimulationConfig(breeding_pairs=4, offspring_all_seven_assist=True, food_supply_assist=True))
    try:
        result = simulation.step()
        assert len(result.selected_pairs) == 4, result.selected_pairs
        print(f"PASS native full day: rooms={len(snapshot['rooms'])}, target=4, "
              f"planned={len(result.selected_pairs)}, births={len(result.births)}", flush=True)
    finally:
        simulation.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path)
    parser.add_argument("--native-source", type=Path)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        sys.exit(1)
    if args.native_source:
        native_check(args.game, args.native_source)
