"""Run the persistent native-backed breeding experiment on all three saves."""

import argparse
from collections import deque
from dataclasses import asdict
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from breeding_daily import NativeDailySimulation, SimulationConfig


def run_slot(args, slot, config):
    simulation = NativeDailySimulation(
        args.exe, args.gpak, args.saves / f"steamcampaign0{slot}.sav",
        args.snapshots / f"slot{slot}_house.json", config)
    window = deque(maxlen=args.stable_days)
    first_all_seven_day = None
    stable = None
    history = []
    try:
        for elapsed in range(1, args.max_days + 1):
            result = simulation.step()
            full_population = sum(simulation._all_seven(cat) for cat in simulation.cats.values()
                                  if not simulation._dead(cat))
            replacements = simulation.replacement_pairs()
            if first_all_seven_day is None and full_population:
                first_all_seven_day = simulation.day
            if replacements >= args.replacement_pairs:
                window.append((len(result.births), result.all_seven_births))
            else:
                window.clear()
            births = sum(day_births for day_births, _ in window)
            all_seven_births = sum(count for _, count in window)
            ratio = all_seven_births / births if births else 0.0
            history.append({
                "day": result.day,
                "population": len(simulation.cats),
                "all_seven_population": full_population,
                "replacement_pairs": replacements,
                "selected_pairs": len(result.selected_pairs),
                "selected_coverages": result.selected_coverages,
                "births": len(result.births),
                "all_seven_births": result.all_seven_births,
                "arrivals": len(result.arrivals),
                "food": result.food_remaining,
            })
            if elapsed == 1 or elapsed % args.progress_every == 0:
                print(
                    f"slot {slot} +{elapsed}: day={simulation.day} cats={len(simulation.cats)} "
                    f"all7={full_population} replacements={replacements} "
                    f"window={len(window)}/{args.stable_days} births={births} ratio={ratio:.4f}",
                    flush=True)
            if len(window) == args.stable_days and births and ratio >= args.threshold:
                stable = {
                    "start_day": simulation.day - args.stable_days,
                    "end_day": simulation.day - 1,
                    "births": births,
                    "all_seven_births": all_seven_births,
                    "ratio": ratio,
                    "replacement_pairs": replacements,
                }
                break
        return {
            "slot": slot,
            "initial_day": simulation.snapshot["day"],
            "final_day": simulation.day,
            "first_all_seven_day": first_all_seven_day,
            "stable": stable,
            "history": history,
        }
    finally:
        simulation.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--slots", nargs="+", type=int, choices=(1, 2, 3), default=(1, 2, 3))
    parser.add_argument("--max-days", type=int, default=365)
    parser.add_argument("--stable-days", type=int, default=30)
    parser.add_argument("--threshold", type=float, default=0.95)
    parser.add_argument("--replacement-pairs", type=int, default=2)
    parser.add_argument("--breeding-pairs", type=int, default=2)
    parser.add_argument("--food-refill", type=int, default=0)
    parser.add_argument("--comfort", type=float)
    parser.add_argument("--stimulation", type=float)
    parser.add_argument("--health", type=float)
    parser.add_argument("--evolution", type=float)
    parser.add_argument("--appeal", type=float)
    parser.add_argument("--assist", action="store_true", help="explicitly set newborn genetic stats to seven")
    parser.add_argument("--food-assist", action="store_true", help="temporary daily room food at least 1000000")
    parser.add_argument("--population-limit", type=int, default=150)
    parser.add_argument("--progress-every", type=int, default=10)
    args = parser.parse_args()
    if args.max_days <= 0 or args.stable_days <= 0 or args.progress_every <= 0:
        parser.error("day counts and progress interval must be positive")
    if not 0 <= args.threshold <= 1:
        parser.error("threshold must be in [0, 1]")

    breeding_effects = {
        "Comfort": args.comfort,
        "Stimulation": args.stimulation,
        "Health": args.health,
        "Evolution": args.evolution,
    }
    breeding_effects = {key: value for key, value in breeding_effects.items() if value is not None}
    config = SimulationConfig(
        breeding_pairs=args.breeding_pairs,
        daily_food_refill=args.food_refill or None,
        remove_dead_after_day=True,
        breeding_room_overrides=breeding_effects,
        nonbreeding_room_overrides={} if args.health is None else {"Health": args.health},
        house_effect_overrides={} if args.appeal is None else {"Appeal": args.appeal},
        offspring_all_seven_assist=args.assist,
        food_supply_assist=args.food_assist,
        population_limit=args.population_limit,
    )
    results = [run_slot(args, slot, config) for slot in args.slots]
    output = {
        "criterion": {
            "replacement_pairs": args.replacement_pairs,
            "stable_days": args.stable_days,
            "all_seven_birth_threshold": args.threshold,
            "avoid_inbreeding": config.avoid_inbreeding,
        },
        "config": asdict(config),
        "slots": results,
        "passed": all(result["stable"] is not None for result in results),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({
        "passed": output["passed"],
        "slots": [{"slot": item["slot"], "final_day": item["final_day"],
                   "first_all_seven_day": item["first_all_seven_day"],
                   "stable": item["stable"]} for item in results],
    }, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
