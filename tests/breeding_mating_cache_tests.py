"""Compare full native days with/without the partner-selection stat cache."""

import argparse
from dataclasses import asdict
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_save import read_snapshot
from breeding_trait_experiment import seed_state


def run(game, save, days, enabled):
    sim = NativeDailySimulation(game / "Mewgenics.exe", game / "resources.gpak", save,
                               read_snapshot(save, game / "resources.gpak"),
                               SimulationConfig(offspring_all_seven_assist=True,
                                                food_supply_assist=True))
    sim.native.mating_cache_enabled = enabled
    sim.native.uc.mem_write(sim.native.tls + 0x178, seed_state(7123))
    rows = []
    start = time.perf_counter()
    try:
        for elapsed in range(days):
            result = sim.step()
            rows.append({"result": asdict(result),
                         "cats": {key: sim.native.serialize(cat) for key, cat in sim.cats.items()},
                         "rng": bytes(sim.native.uc.mem_read(sim.native.tls + 0x178, 32)),
                         "rooms": dict(sim.cat_rooms), "food": sim.food,
                         "furniture": list(sim.furniture),
                         "pedigree": dict(sim.pedigree.entries)})
            print(f"cache={enabled} day={elapsed + 1} complete", flush=True)
        return rows, time.perf_counter() - start
    finally:
        sim.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path, required=True)
    parser.add_argument("--days", type=int, default=2)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    old, old_seconds = run(args.game, args.save, args.days, False)
    new, new_seconds = run(args.game, args.save, args.days, True)
    for index, (before, after) in enumerate(zip(old, new), 1):
        for field in before:
            assert before[field] == after[field], (index, field)
    report = {"days": args.days, "native_day_results_equal": True,
              "all_cat_bytes_equal": True, "rng_state_equal": True,
              "original_seconds": old_seconds, "cached_seconds": new_seconds,
              "speedup": old_seconds / new_seconds}
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
