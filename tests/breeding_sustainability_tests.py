"""Exercise the workbench's sustained-breeding criterion on explicit save copies."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_web import Workbench


class ObservedWorkbench(Workbench):
    def update(self, **values):
        super().update(**values)
        if "latest" in values:
            day = values["latest"]
            print(f"+{day['elapsed']}: cats={day['population']} "
                  f"all7={day['all_seven_population']} food={day['food_remaining']} "
                  f"replacements={day['replacement_pairs']} "
                  f"window={day['window_days']}/30 ratio={day['window_ratio']:.3f}",
                  flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--saves", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--days", type=int, default=90)
    parser.add_argument("--food-assist", action="store_true")
    parser.add_argument("--no-genetic-assist", action="store_true",
                        help="keep native offspring genetics without setting stats to seven")
    parser.add_argument("--population-limit", type=int, default=150)
    parser.add_argument("--full-duration", action="store_true",
                        help="continue through all requested days even after reaching stability")
    args = parser.parse_args()
    # An explicit new output directory prevents replacing previous evidence.
    args.output.mkdir(parents=True, exist_ok=False)
    results = []
    for index, source in enumerate(args.saves, 1):
        assist = not args.no_genetic_assist
        print(f"START case {index}: {source.name}; actual rooms, food_assist={args.food_assist}, genetic_assist={assist}",
              flush=True)
        app = ObservedWorkbench(args.game, args.output / f"case{index}")
        app.run(source.resolve(), args.days, 2, 0, assist, args.food_assist,
                stop_when_stable=not args.full_duration, population_limit=args.population_limit)
        result = app.state()["job"]
        result["first_stable_elapsed"] = next((day["elapsed"] for day in result.get("history", [])
            if day["window_days"] == 30 and day["window_births"] > 0
            and day["window_ratio"] >= .95), None)
        results.append(result)
        (args.output / "results.json").write_text(
            json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"RESULT case {index}: status={result['status']} "
              f"sustained={result.get('passed', False)} first_stable_day={result['first_stable_elapsed']} "
              f"{result.get('message', '')}",
              flush=True)
        if result["status"] == "failed":
            return 2
    return 0 if all(result.get("passed") for result in results) else 1


if __name__ == "__main__":
    sys.exit(main())
