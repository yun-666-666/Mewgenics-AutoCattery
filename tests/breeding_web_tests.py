"""Real-save workbench integration: live discovery, native births, export/reload."""
import argparse
from pathlib import Path
import sqlite3
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_save import (discover_saves, read_snapshot, copy_database, export_population,
                           read_table, cat_fields)
from breeding_native_cat import read_cat_blobs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--days", type=int, default=3)
    parser.add_argument("--slots", nargs="+", type=int, default=[1, 2, 3])
    args = parser.parse_args()
    sources = discover_saves()
    main_account = sources[0].parent
    args.output.mkdir(parents=True, exist_ok=True)
    for slot in args.slots:
        source = main_account / f"steamcampaign0{slot}.sav"
        working = args.output / f"slot{slot}-source.sav"
        copy_database(source, working)
        snapshot = read_snapshot(working, args.game / "resources.gpak")
        config = SimulationConfig(offspring_all_seven_assist=True)
        simulation = NativeDailySimulation(args.game / "Mewgenics.exe", args.game / "resources.gpak",
                                           working, snapshot, config)
        try:
            effects, _ = simulation._room_effects()
            assert all(effects[r["id"]] == r["effects"] for r in snapshot["rooms"])
            assert not config.suppress_nonbreeding_rooms and config.daily_food_refill is None
            births = 0
            for elapsed in range(args.days):
                result = simulation.step()
                births += len(result.births)
                assert all(simulation._all_seven(simulation.cats[k]) for k in result.births
                           if k in simulation.cats)
                print(f"slot {slot} day {result.day}: births={len(result.births)} "
                      f"assisted={result.assisted_births} food={result.food_remaining} "
                      f"population={len(simulation.cats)}", flush=True)
            target = args.output / f"slot{slot}-trained.sav"
            original_ids = {key for key, _ in read_cat_blobs(working)}
            exportable = [key for key, cat in simulation.cats.items()
                          if key not in original_ids and not simulation._dead(cat)
                          and simulation._all_seven(cat)]
            if not exportable:
                try:
                    export_population(simulation, target)
                except ValueError as error:
                    assert str(error) == "当前结果还没有可导出的全七后代"
                else:
                    raise AssertionError("Empty cultivation must not produce an export")
                assert not target.exists()
                print(f"NO EXPORT slot {slot}: {births} births, no surviving new all-seven cats; "
                      "empty-result refusal verified, sustained breeding NOT established", flush=True)
                continue
            exported = export_population(simulation, target)
            after = read_snapshot(target, args.game / "resources.gpak")
            assert len(after["cats"]) <= config.population_limit
            assert after["day"] == snapshot["day"] and after["food"] == snapshot["food"]
            assert after["rooms"] == snapshot["rooms"]
            with sqlite3.connect(working) as original, sqlite3.connect(target) as trained:
                assert original.execute("select * from furniture").fetchall() == trained.execute("select * from furniture").fetchall()
                for key, blob in original.execute("select key,data from cats"):
                    row = trained.execute("select data from cats where key=?", (key,)).fetchone()
                    if key in exported["removed_population_ids"]:
                        assert row is None
                    else:
                        assert row[0] == blob
                raw_pedigree = trained.execute("select data from files where key='pedigree'").fetchone()[0]
            # Ask the game's actual lookup routine to find every exported ID
            # in the serialized table. This exercises real probe/clone layout.
            native = simulation.native
            _, size, capacity = struct.unpack_from("<3Q", raw_pedigree)
            controls = native.allocate(capacity + 17)
            rows = native.allocate(capacity * 32)
            native.uc.mem_write(controls, raw_pedigree[24:24 + capacity + 17])
            native.uc.mem_write(rows, raw_pedigree[24 + capacity + 17:24 + capacity + 17 + capacity * 32])
            table = native.allocate(0x38)
            native.uc.mem_write(table, struct.pack("<4Q", controls, rows, size, capacity))
            key_ptr = native.allocate(8)
            all_rows, _ = read_table(raw_pedigree, 0, "qqqd")
            for key, parent_a, parent_b, coi in all_rows:
                native.uc.mem_write(key_ptr, struct.pack("<q", key))
                value = native.call(0x183e70, table, key_ptr)
                assert struct.unpack("<qqd", native.uc.mem_read(value, 24)) == (parent_a, parent_b, coi)
            raw_cats = dict(read_cat_blobs(target))
            for key in exported["imported_ids"]:
                cat = native.deserialize(raw_cats[key])
                assert simulation._all_seven(cat)
                assert cat_fields(key, raw_cats[key])["name"]
            print(f"PASS slot {slot}: real attributes, {births} births, retained originals unchanged, "
                  f"{len(exported['imported_ids'])} exported cats, {len(all_rows)} native pedigree lookups", flush=True)
        finally:
            simulation.close()


if __name__ == "__main__":
    main()
