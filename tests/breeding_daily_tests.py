"""Focused three-day persistence test using the smallest real save."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    config = SimulationConfig(
        breeding_pairs=2,
        daily_food_refill=1000,
        remove_dead_after_day=False,
        room_overrides={"*": {"Comfort": 60, "Health": 20}},
    )
    simulation = NativeDailySimulation(
        args.exe, args.gpak, args.saves / "steamcampaign03.sav",
        args.snapshots / "slot3_house.json", config)
    try:
        initial_day = simulation.day
        initial_ids = set(simulation.cats)
        for offset in range(3):
            result = simulation.step()
            assert result.day == initial_day + offset
            assert simulation.day == initial_day + offset + 1
            assert result.food_remaining >= 0
            assert simulation.next_id > max(initial_ids)
            assert initial_ids <= set(simulation.pedigree.entries)
            assert simulation.native.heap_next > simulation.population_mark
            for cat in simulation.cats.values():
                assert simulation.native.serialize(cat)
            print(
                f"PASS slot 3 day {result.day}: pairs={len(result.selected_pairs)} "
                f"births={len(result.births)} arrivals={len(result.arrivals)} "
                f"living_records={len(simulation.cats)} food={result.food_remaining}",
                flush=True)
    finally:
        simulation.close()


if __name__ == "__main__":
    main()
