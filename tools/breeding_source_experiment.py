"""Measure the recovered stray genetic stages for population optimization.

This is a source-distribution experiment, not the multi-day simulation. Body,
skill, special-cat eligibility and intervening full-generator RNG draws are
outside its scope. Each reported sample uses the verified native genetic rules.
"""

import argparse
from collections import Counter
import json
from pathlib import Path
import time

from breeding_lab import GameRandom
from breeding_population import initial_genetic_stats, mutate_stats, stray_appeal_increment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshots", nargs="+", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=100000)
    parser.add_argument("--appeal", nargs="+", type=float,
                        default=[0, 20, 40, 80, 120, 160, 240, 320])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.samples < 1:
        parser.error("samples must be positive")
    missing = {}
    for index, path in enumerate(args.snapshots, 1):
        cats = json.loads(path.read_text(encoding="utf-8"))["cats"]
        missing[str(index)] = [stat for stat in range(7)
                               if not any(cat["genetic"][stat] == 7 for cat in cats)]
    result = {
        "scope": "stray initial genetic stats plus full-house Appeal stat allocation",
        "limitations": ["not a daily or full newborn simulation",
                        "does not establish breeding eligibility of the generated source",
                        "does not reproduce intervening full-generator RNG consumption"],
        "missing_seven_stat_indices_by_snapshot": missing,
        "samples_per_appeal": args.samples,
        "rows": [],
    }
    started = time.monotonic()
    for appeal in args.appeal:
        rng = GameRandom([0x7847F3B9224EA68D, 0x173904ECC29651D3,
                          0xDB786198F48C2021, 0x72AE9428D50167B9])
        sevens = Counter()
        per_stat = [0] * 7
        supplies_missing = Counter()
        for _ in range(args.samples):
            genes = initial_genetic_stats(rng)
            increment = stray_appeal_increment(appeal, rng)
            genes = mutate_stats(genes, increment, 0, 3, 7, False, rng)
            sevens[sum(value == 7 for value in genes)] += 1
            for stat, value in enumerate(genes):
                per_stat[stat] += value == 7
            for slot, indices in missing.items():
                if indices and any(genes[stat] == 7 for stat in indices):
                    supplies_missing[slot] += 1
        row = {"full_house_appeal": appeal,
               "seven_count_histogram": dict(sorted(sevens.items())),
               "per_stat_seven_counts": per_stat,
               "supplies_any_missing_locus_counts": dict(supplies_missing),
               "all_seven_count": sevens[7]}
        result["rows"].append(row)
        result["elapsed_seconds"] = time.monotonic() - started
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(row), flush=True)
    print(f"COMPLETE: {len(result['rows'])} Appeal candidates, "
          f"{args.samples} source samples each; {args.output}", flush=True)


if __name__ == "__main__":
    main()
