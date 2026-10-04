"""Native names and preview/apply agreement, using only disposable save copies."""

import argparse
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_names import NativeNames, save_language
from breeding_native_cat import NativeCatReference, decode_cat_blob, read_save_file
from breeding_save import cat_fields, copy_database
from breeding_save_application import apply_result
from breeding_web import Workbench


def check_names(game, source, folder):
    seed = struct.pack("<4Q", 1, 2, 3, 4)
    before = read_save_file(source, "name_gen_history_w")
    examples = {}
    for language in ("zh-cn", "en", "fr"):
        names = NativeNames(game / "Mewgenics.exe", game / "resources.gpak", source, language, seed)
        generated = []
        for sex in (0, 1, 2):
            locale = language if language != "fr" else "en"
            category = ("male", "female", "neutral")[sex]
            pool = set(names._lines(names.texts[f"data/catnames_{category}_{locale}.txt"]))
            pool.update(names._lines(names.texts[f"data/catnames_neutral_{locale}.txt"]))
            for _ in range(12):
                value = names.generate(sex)
                assert value in pool and not value.startswith("培养-")
                generated.append(value)
        assert len(generated) == len(set(generated))
        assert struct.unpack_from("<Q", names.history())[0] <= 500
        examples[language] = generated[:4]
        if language == "zh-cn":
            first = generated[0]
            history_save = folder / "recent-names.sav"
            copy_database(source, history_save)
            with closing(sqlite3.connect(history_save)) as db, db:
                db.execute("UPDATE files SET data=? WHERE key='name_gen_history_w'", (names.history(),))
            again = NativeNames(game / "Mewgenics.exe", game / "resources.gpak", history_save, language, seed)
            assert again.generate(0) != first
    assert read_save_file(source, "name_gen_history_w") == before
    print("PASS native sex/language pools, history retry and 500-name cap:", examples, flush=True)


def check_flow(game, source, folder):
    account = folder / "account"
    saves = account / "saves"
    saves.mkdir(parents=True)
    working = saves / "steamcampaign01.sav"
    copy_database(source, working)
    (account / "settings.txt").write_text("current_language zh-cn\n", encoding="utf-8")
    assert save_language(working) == "zh-cn"
    with closing(sqlite3.connect(working)) as db:
        originals = dict(db.execute("SELECT key,data FROM cats"))
    workbench = Workbench(game, folder / "jobs")
    workbench.run(working, 3, 2, 0, True, True, stop_when_stable=False)
    job = workbench.job
    check_result(game, job, originals, folder)


def check_result(game, job, originals, folder):
    working = Path(job["source"])
    assert job["status"] == "completed", job.get("message")
    assert job.get("export"), job.get("export_error")
    preview = {cat["id"]: cat["name"] for cat in job["preview"]["added"]}
    assert preview and all(name and not name.startswith("培养-") for name in preview.values())
    native = NativeCatReference(game / "Mewgenics.exe")
    with closing(sqlite3.connect(job["export"]["file"])) as db:
        for key, name in preview.items():
            raw = decode_cat_blob(db.execute("SELECT data FROM cats WHERE key=?", (key,)).fetchone()[0])
            cat = native.deserialize(raw)
            assert cat_fields(key, native.serialize(cat))["name"] == name
    apply_result(job)
    with closing(sqlite3.connect(working)) as db:
        after = dict(db.execute("SELECT key,data FROM cats"))
    assert {key: cat_fields(key, after[key])["name"] for key in preview} == preview
    removed = set(job["export"]["removed_population_ids"])
    assert all(after[key] == blob for key, blob in originals.items() if key not in removed)
    history = read_save_file(working, "name_gen_history_w")
    for name in preview.values():
        assert name.encode("utf-16-le") in history
    print("PASS export -> native deserialize/serialize -> preview -> confirmed save:", preview, flush=True)
    (folder / "name-check.json").write_text(json.dumps(preview, ensure_ascii=False, indent=2), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output) as temporary:
        folder = Path(temporary)
        check_names(args.game, args.source, folder)
        check_flow(args.game, args.source, folder)


if __name__ == "__main__":
    main()
