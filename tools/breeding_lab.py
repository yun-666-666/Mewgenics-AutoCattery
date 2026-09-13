"""Offline experiment for the seven inherited genetic stats in CatData::breed.

This is the confirmed stat-inheritance stage, not a complete newborn generator.
See tools/breeding_lab.md for the native evidence and experimental boundary.
Only the optional native verification needs pefile and unicorn.
"""

import argparse
from collections import Counter
import json
import math


MASK64 = (1 << 64) - 1


class GameRandom:
    """xoshiro256+ state transition and double conversion at EXE RVA 0x1595A0."""

    def __init__(self, state):
        self.state = list(state)
        if len(self.state) != 4 or not any(self.state) or any(
            not isinstance(word, int) or not 0 <= word <= MASK64
            for word in self.state
        ):
            raise ValueError("RNG state requires four uint64 words, not all zero")

    def random(self):
        a, b, c, d = self.state
        result = (a + d) & MASK64
        shifted = (b << 17) & MASK64
        c ^= a
        d ^= b
        b ^= c
        a ^= d
        c ^= shifted
        d = ((d << 45) | (d >> 19)) & MASK64
        self.state[:] = a, b, c, d
        return (result >> 11) * (2.0 ** -53)


def inheritance_weights(effects):
    """Effect magnitudes, already evaluated by the game, not furniture counts."""
    supported = {
        "InheritStatFavorMom", "InheritStatFavorDad",
        "InheritStatFavorBest", "Stimulation",
    }
    unknown = effects.keys() - supported
    if unknown:
        raise ValueError(f"Unmodeled effects: {', '.join(sorted(unknown))}")
    if any(not math.isfinite(value) for value in effects.values()):
        raise ValueError("Effect magnitudes must be finite")
    return (
        1.0 + 0.25 * effects.get("InheritStatFavorMom", 0.0),
        1.0 + 0.25 * effects.get("InheritStatFavorDad", 0.0),
        0.1 * effects.get("InheritStatFavorBest", 0.0)
        + 0.01 * effects.get("Stimulation", 0.0),
    )


def mother_probability(mother, father, weights):
    """Branch-for-branch arithmetic from native stat selector RVA 0xA7A00."""
    mom_weight, dad_weight, best = weights
    if mother > father:
        if best > 0.0:
            mom_weight += best
        else:
            dad_weight -= best
    elif father > mother:
        if best > 0.0:
            dad_weight += best
        else:
            mom_weight -= best
    denominator = mom_weight + dad_weight
    probability = mom_weight / denominator if denominator else math.nan
    if not 0.0 <= probability <= 1.0:
        return 0.5
    return probability


def inherit_stats(mother, father, weights, rng):
    result = []
    for mom, dad in zip(mother, father, strict=True):
        probability = mother_probability(mom, dad, weights)
        # Native code skips RNG at 0/1, but draws even when parent values match.
        choose_mother = probability >= 1.0 or (
            probability > 0.0 and probability > rng.random()
        )
        result.append(mom if choose_mother else dad)
    return result


def validate_stats(stats):
    if len(stats) != 7 or any(
        not isinstance(value, int) or not -(1 << 31) <= value < (1 << 31)
        for value in stats
    ):
        raise ValueError("Supply seven signed int32 genetic stats for each parent")


def experiment(mother, father, effects, state, samples, threshold):
    validate_stats(mother)
    validate_stats(father)
    if samples < 1:
        raise ValueError("samples must be positive")
    weights = inheritance_weights(effects)
    rng = GameRandom(state)
    probabilities = [
        mother_probability(mom, dad, weights)
        for mom, dad in zip(mother, father, strict=True)
    ]
    distributions = []
    for mom, dad, probability in zip(mother, father, probabilities, strict=True):
        distribution = Counter()
        distribution[mom] += probability
        distribution[dad] += 1.0 - probability
        distributions.append(dict(sorted(distribution.items())))
    histograms = [Counter() for _ in range(7)]
    successes = 0
    first_offspring = []
    for _ in range(samples):
        child = inherit_stats(mother, father, weights, rng)
        if len(first_offspring) < 5:
            first_offspring.append(child)
        successes += all(value >= threshold for value in child)
        for histogram, value in zip(histograms, child, strict=True):
            histogram[value] += 1
    return {
        "scope": "seven genetic stats at the inheritance stage of CatData::breed",
        "not_modeled": [
            "mating success, partner choice, litter size, or births per day",
            "skills, passives, disorders, body mutations, and displayed stat bonuses",
            "full-birth RNG consumption before and after the stat stage",
            "conversion of placed furniture into effective effect magnitudes",
        ],
        "mother": mother, "father": father, "effects": effects,
        "weights_mother_father_best": weights,
        "rng_initial_state": state, "rng_final_state": rng.state,
        "samples": samples, "threshold_all_seven_at_least": threshold,
        "per_stat_distribution": distributions,
        "expected_genetic_stats": [
            sum(value * probability for value, probability in dist.items())
            for dist in distributions
        ],
        "all_seven_probability_under_uniform_independent_draws": math.prod(
            sum(prob for value, prob in dist.items() if value >= threshold)
            for dist in distributions
        ),
        "observed_all_seven_count": successes,
        "observed_all_seven_fraction": successes / samples,
        "observed_stat_counts": [dict(sorted(hist.items())) for hist in histograms],
        "first_five_offspring_genetic_stats": first_offspring,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mother", nargs=7, type=int, required=True)
    parser.add_argument("--father", nargs=7, type=int, required=True)
    parser.add_argument("--samples", type=int, default=10000)
    parser.add_argument("--threshold", type=int, default=7)
    parser.add_argument(
        "--state", nargs=4, type=lambda value: int(value, 0),
        default=[0x123456789ABCDEF0, 0xFEDCBA9876543210,
                 0xF1E2D3C4B5A6978, 0x8877665544332211],
        help="four RNG words at the START of the stat stage; not a save seed",
    )
    parser.add_argument(
        "--effects", type=json.loads, default={},
        help='JSON of effective values, e.g. {"Stimulation": 20}',
    )
    args = parser.parse_args()
    print(json.dumps(experiment(
        args.mother, args.father, args.effects, args.state,
        args.samples, args.threshold,
    ), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
