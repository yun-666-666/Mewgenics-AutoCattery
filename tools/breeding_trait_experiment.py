"""Matched-seed, multi-day native inheritance comparison on an isolated save copy.

The daily simulator uses its two-pair room planner, not the MOD's six-cat group
planner. Birth physiology executes the installed EXE; cosmetic RNG is omitted.
Utility scores are resource-derived heuristics. Raw inherited identities are
also recorded so a higher selection proxy alone cannot pass this experiment.
"""

import argparse
from collections import Counter
from dataclasses import asdict
import json
import math
import pickle
import shutil
from pathlib import Path
import statistics
import struct
import subprocess
import sys
import traceback

from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_checkpoint import capture_simulation, restore_simulation
from breeding_save import cat_fields, copy_database, discover_saves, read_snapshot


class AttributeFirstSimulation(NativeDailySimulation):
    """Control arm: legacy genetic priorities for both pairing and retention."""

    def _candidate_pairs(self):
        pairs = []
        for score, first, second in super()._candidate_pairs():
            stable = (self._all_seven(self.cats[first]) and
                      self._all_seven(self.cats[second]) and
                      self.pedigree.coi(first, second) == 0)
            pairs.append(((int(stable), *score[2:]), first, second))
        return sorted(pairs, reverse=True)

    def trim_population(self):
        # The base retention switches to trait-first when assistance is enabled.
        # Only this selection phase is changed; birth assistance stays identical.
        assisted = self.config.offspring_all_seven_assist
        self.config.offspring_all_seven_assist = False
        try:
            return super().trim_population()
        finally:
            self.config.offspring_all_seven_assist = assisted


def seed_state(seed):
    """Four SplitMix64 words, explicitly recorded; never an all-zero RNG state."""
    words = []
    mask = (1 << 64) - 1
    value = seed & mask
    for _ in range(4):
        value = (value + 0x9E3779B97F4A7C15) & mask
        word = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & mask
        word = ((word ^ (word >> 27)) * 0x94D049BB133111EB) & mask
        words.append(word ^ (word >> 31))
    return struct.pack("<4Q", *words)


def trait_inventory(cat, profile):
    slots = cat["ability_slots"]
    inventory = {
        "active": Counter(name for name in slots[2:6] if name and name != "None"),
        "passive": Counter(name for name in slots[6:8] if name and name != "None"),
        "disorder": Counter(name for name in slots[8:10] if name and name != "None"),
        "mutation": Counter(),
        "defect": Counter(),
    }
    for part in cat["visual_parts"]:
        key = f"{part['category']}:{part['id']}"
        if key in profile["birth_defect_overrides"]:
            inventory["defect"][key] += 1
        elif key in profile["mutation_overrides"]:
            inventory["mutation"][key] += 1
    return inventory


PROFILE_KEYS = {
    "active": "active_ability_overrides", "passive": "passive_overrides",
    "disorder": "disorder_overrides", "mutation": "mutation_overrides",
    "defect": "birth_defect_overrides",
}


def observed_metrics(inventory, profile):
    result = {}
    for category, counts in inventory.items():
        weights = profile[PROFILE_KEYS[category]]
        result[category + "_utility"] = sum(weights.get(name, 0) * count
                                               for name, count in counts.items())
        result[category + "_count"] = sum(counts.values())
    mutations = inventory["mutation"]
    weights = profile["mutation_overrides"]
    result["positive_mutations"] = sum(n for key, n in mutations.items() if weights[key] > 0)
    result["negative_mutations"] = sum(n for key, n in mutations.items() if weights[key] < 0)
    return result


def independent_pairs(simulation, pairs):
    within = all(simulation.pedigree.coi(a, b) == 0 for a, b in pairs)
    cross = all(simulation.pedigree.coi(a, b) == 0
                for index, pair in enumerate(pairs) for other in pairs[index + 1:]
                for a in pair for b in other)
    disjoint = len({cat for pair in pairs for cat in pair}) == 2 * len(pairs)
    return within and cross and disjoint


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def worker(args):
    config = SimulationConfig(**json.loads((args.output / "conditions.json").read_text())["config"])
    save = args.output / "input.sav"
    snapshot = read_snapshot(save, args.game / "resources.gpak")
    cls = AttributeFirstSimulation if args.arm == "attribute" else NativeDailySimulation
    sim = cls(args.game / "Mewgenics.exe", args.game / "resources.gpak", save, snapshot, config)
    rows = []
    birth_totals = Counter()
    frequencies = {key: Counter() for key in PROFILE_KEYS}
    births_today = []
    original_breed = sim.generation.breed

    def capture(mother, father, coi, *, effects=0):
        parents = [trait_inventory(cat_fields(0, sim.native.serialize(cat)), sim.trait_profile)
                   for cat in (mother, father)]
        child = original_breed(mother, father, coi, effects=effects)
        fields = cat_fields(0, sim.native.serialize(child))
        inventory = trait_inventory(fields, sim.trait_profile)
        metrics = observed_metrics(inventory, sim.trait_profile)
        metrics["births"] = 1
        metrics["native_all_seven_births"] = int(fields["genetic"] == [7] * 7)
        metrics["assisted_all_seven_births"] = int(config.offspring_all_seven_assist or
                                                   fields["genetic"] == [7] * 7)
        for category in ("active", "passive", "mutation"):
            weights = sim.trait_profile[PROFILE_KEYS[category]]
            threshold = 0 if category == "mutation" else 1
            desirable = {name for parent in parents for name in parent[category]
                         if weights.get(name, 0) > threshold}
            metrics[category + "_donor_births"] = int(bool(desirable))
            metrics[category + "_transmitted_births"] = int(bool(desirable & inventory[category].keys()))
        births_today.append(metrics)
        for key, counts in inventory.items():
            frequencies[key].update(counts)
        return child

    sim.generation.breed = capture
    log_path = args.output / f"seed-{args.seed}-{args.arm}.jsonl"
    checkpoint_path = args.output / f"seed-{args.seed}-{args.arm}.checkpoint"
    old_rows = ([json.loads(line) for line in log_path.read_text().splitlines()]
                if args.resume and log_path.exists() else [])
    first_day = 1
    try:
        sim.native.uc.mem_write(sim.native.tls + 0x178, seed_state(args.seed))
        if args.resume and checkpoint_path.exists():
            with checkpoint_path.open("rb") as saved:
                checkpoint = pickle.load(saved)
            restore_simulation(sim, checkpoint["simulation"])
            rows = checkpoint["rows"]
            birth_totals, frequencies = checkpoint["totals"], checkpoint["frequencies"]
            first_day = len(rows) + 1
            if json.loads(json.dumps(rows)) != old_rows:
                raise AssertionError("checkpoint and daily observations differ")
            print(f"RESUME seed={args.seed} arm={args.arm} from day={first_day}", flush=True)
        with log_path.open("a" if args.resume else "w", encoding="utf-8") as log:
            for elapsed in range(first_day, args.days + 1):
                births_today.clear()
                result = sim.step()
                if len(births_today) != len(result.births):
                    raise AssertionError("native birth capture differs from day result")
                if not independent_pairs(sim, result.selected_pairs):
                    raise AssertionError("selected breeding families lost COI-zero independence")
                totals = Counter()
                for metrics in births_today:
                    totals.update(metrics)
                birth_totals.update(totals)
                population = Counter()
                for key, cat in sim.cats.items():
                    population.update(observed_metrics(
                        trait_inventory(cat_fields(key, sim.native.serialize(cat)), sim.trait_profile),
                        sim.trait_profile))
                replacements = sim.select_pairs(2)
                if not independent_pairs(sim, replacements):
                    raise AssertionError("replacement families lost COI-zero independence")
                row = {"elapsed": elapsed, "day": result.day, "births": dict(totals),
                       "population": len(sim.cats), "population_traits": dict(population),
                       "all_seven_population": sum(sim._all_seven(cat) for cat in sim.cats.values()),
                       "independent_replacement_pairs": len(replacements),
                       "selected_pairs": result.selected_pairs,
                       "bonus_items": len(sim.generation.bonus_items),
                       "removed_population": len(result.removed_population)}
                rows.append(row)
                if elapsed <= len(old_rows):
                    if json.loads(json.dumps(row)) != old_rows[elapsed - 1]:
                        raise AssertionError(f"recovered day {elapsed} differs from preserved observations")
                else:
                    log.write(json.dumps(row, ensure_ascii=False) + "\n")
                    log.flush()
                if elapsed >= len(old_rows):
                    with checkpoint_path.open("wb") as saved:
                        pickle.dump({"simulation": capture_simulation(sim), "rows": rows,
                                     "totals": birth_totals, "frequencies": frequencies}, saved)
                print(f"seed={args.seed} arm={args.arm} +{elapsed}/{args.days} "
                      f"births={len(result.births)} cats={len(sim.cats)} "
                      f"independent_pairs={len(replacements)}", flush=True)
        result = {"seed": args.seed, "arm": args.arm, "days": args.days,
                  "birth_totals": dict(birth_totals), "frequencies": frequencies,
                  "bonus_items": sim.generation.bonus_items,
                  "final": rows[-1],
                  "minimum_independent_pairs": min(row["independent_replacement_pairs"] for row in rows)}
        write_json(args.output / f"seed-{args.seed}-{args.arm}.json", result)
    except Exception:
        (args.output / f"seed-{args.seed}-{args.arm}.error.txt").write_text(
            traceback.format_exc(), encoding="utf-8")
        raise
    finally:
        sim.close()


def comparison(results):
    pairs = {}
    for result in results:
        pairs.setdefault(result["seed"], {})[result["arm"]] = result
    metrics = [key for key in results[0]["birth_totals"]
               if key.endswith("_utility") or key in ("positive_mutations", "negative_mutations")]
    summary = {}
    for metric in metrics:
        differences = []
        means = {arm: [] for arm in ("attribute", "trait")}
        for pair in pairs.values():
            for arm in means:
                totals = pair[arm]["birth_totals"]
                means[arm].append(totals.get(metric, 0) / max(1, totals.get("births", 0)))
            differences.append(means["trait"][-1] - means["attribute"][-1])
        summary[metric] = {"attribute_per_birth": statistics.mean(means["attribute"]),
                           "trait_per_birth": statistics.mean(means["trait"]),
                           "matched_seed_differences": differences,
                           "difference_mean": statistics.mean(differences),
                           "difference_standard_error": statistics.stdev(differences) /
                               math.sqrt(len(differences)) if len(differences) > 1 else None}
    return {"completed_seed_pairs": len(pairs), "metrics": summary, "runs": results}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--days", type=int, default=60)
    parser.add_argument("--seeds", type=int, nargs="+", default=[101, 202, 303, 404, 505, 606])
    parser.add_argument("--arm", choices=("attribute", "trait"))
    parser.add_argument("--seed", type=int)
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--reuse-baseline", type=Path,
                        help="reuse completed attribute controls when no population trimming occurred")
    args = parser.parse_args()
    args.game = args.game.resolve()
    args.output = args.output.resolve()
    if args.days < 1:
        parser.error("days must be positive")
    if args.arm:
        worker(args)
        return
    if not args.resume:
        args.output.mkdir(parents=True, exist_ok=False)
        source = args.save or (args.reuse_baseline / "input.sav" if args.reuse_baseline else discover_saves()[0])
        copy_database(source, args.output / "input.sav")
        snapshot = read_snapshot(args.output / "input.sav", args.game / "resources.gpak")
        config = SimulationConfig(offspring_all_seven_assist=True, food_supply_assist=True)
        write_json(args.output / "conditions.json", {
        "source_slot": source.name, "source_day": snapshot["day"],
        "source_population": len(snapshot["cats"]), "days_per_arm": args.days,
        "seeds": args.seeds, "rng_words": {seed: list(struct.unpack("<4Q", seed_state(seed)))
                                             for seed in args.seeds},
        "config": asdict(config), "planner": "simulator two-pair rooms",
        "native_birth": "installed EXE A89A0; cosmetic name RNG omitted",
            "utility": "resource-derived heuristic; raw identities and transmissions recorded"})
        if args.reuse_baseline:
            prior = json.loads((args.reuse_baseline / "conditions.json").read_text())
            if prior["config"] != asdict(config) or prior["seeds"] != args.seeds or prior["days_per_arm"] != args.days:
                raise ValueError("reused controls must have identical settings, seeds and duration")
            if prior["source_day"] != snapshot["day"] or prior["source_population"] != len(snapshot["cats"]):
                raise ValueError("reused controls must use the same experiment input")
            # The attribute arm discards trait scores for pairing. Retention can
            # use a trait tiebreaker, so controls are reusable only without it.
            for seed in args.seeds:
                name = f"seed-{seed}-attribute"
                observations = [json.loads(line) for line in
                                (args.reuse_baseline / (name + ".jsonl")).read_text().splitlines()]
                if len(observations) != args.days or any(row["removed_population"] for row in observations):
                    raise ValueError("controls with population trimming require a fresh comparison")
                for extension in (".json", ".jsonl"):
                    shutil.copyfile(args.reuse_baseline / (name + extension), args.output / (name + extension))
    else:
        conditions = json.loads((args.output / "conditions.json").read_text())
        args.seeds, args.days = conditions["seeds"], conditions["days_per_arm"]
    results = []
    for seed in args.seeds:
        for arm in ("attribute", "trait"):
            completed = args.output / f"seed-{seed}-{arm}.json"
            if not ((args.resume or args.reuse_baseline) and completed.exists()):
                subprocess.run([sys.executable, "-u", str(Path(__file__).resolve()),
                            "--game", str(args.game), "--output", str(args.output),
                                "--days", str(args.days), "--arm", arm, "--seed", str(seed),
                                *(["--resume"] if args.resume else [])], check=True)
            results.append(json.loads(completed.read_text()))
        write_json(args.output / "results.json", comparison(results))
    print("COMPLETED matched-seed native inheritance comparison", flush=True)


if __name__ == "__main__":
    main()
