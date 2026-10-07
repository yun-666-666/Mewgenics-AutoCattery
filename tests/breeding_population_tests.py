"""Differential checks for newly recovered population-generation helpers."""

import argparse
import math
from pathlib import Path
import random
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_lab import GameRandom
from breeding_native_reference import NativeStatReference
from breeding_population import biased_random, mutate_stats, initial_personality, aging_parameters, attraction


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--personality-only", action="store_true")
    parser.add_argument("--aging-only", action="store_true")
    parser.add_argument("--attraction-only", action="store_true")
    args = parser.parse_args()
    native = NativeStatReference(args.exe)
    fixtures = random.Random(20260913)
    if args.attraction_only:
        comparisons = 0
        for sex in range(3):
            for target_sex in range(3):
                for sexuality in (0, .05, .5, .95, 1):
                    for lover in (-1, 10, 20):
                        values = (fixtures.random(), sexuality, sex, target_sex,
                                  fixtures.randint(1, 20), lover, 10, fixtures.random())
                        actual = attraction(*values)
                        expected = native.attraction(*values)
                        assert math.isclose(actual, expected, abs_tol=2e-15, rel_tol=2e-15), (
                            values, actual, expected)
                        comparisons += 1
        print(f"PASS: {comparisons} native attraction comparisons with explicit adult/stat inputs", flush=True)
        return
    if args.aging_only:
        for health in range(-100, 301):
            age, old_risk, death_risk = aging_parameters(health)
            raw_age = native.effect("Health", health, 33, 18.0)
            raw_risk = native.effect("Health", health, 34, 0.0)
            expected_age = native.scalar_math(0x1F2750, [raw_age])[0]
            risk = native.scalar_math(0x1F26E0, [raw_risk, -.05, .05])[0]
            if risk > 0:
                risk *= 2
            assert math.isclose(age, expected_age, abs_tol=1e-13)
            assert math.isclose(old_risk, .11 + risk, abs_tol=1e-15)
            assert math.isclose(death_risk, .165 + risk, abs_tol=1e-15)
        print("PASS: 401 native Health effect + aging math comparisons", flush=True)
        return
    if args.personality_only:
        for _ in range(500):
            state = [fixtures.getrandbits(64) for _ in range(4)]
            rng = GameRandom(state)
            expected, final = native.initial_personality(state)
            actual = initial_personality(rng)
            assert all(math.isclose(a, b, rel_tol=2e-15, abs_tol=2e-16)
                       for a, b in zip(actual, expected)), (actual, expected)
            assert rng.state == final, "Personality RNG mismatch"
        print("PASS: 500 native personality + final RNG comparisons", flush=True)
        return
    biased_count = mutation_count = effect_count = 0
    for bias in [-3.5, -1.0, -0.2, 0.0, 0.5, 1.0, 3.0, 10.2]:
        for _ in range(80):
            state = [fixtures.getrandbits(64) for _ in range(4)]
            rng = GameRandom(state)
            expected, final = native.biased_random(bias, state)
            actual = biased_random(bias, rng)
            assert math.isclose(actual, expected, rel_tol=2e-15, abs_tol=2e-16), (
                bias, actual, expected)
            assert rng.state == final, (bias, "RNG mismatch")
            biased_count += 1
    for increments, decrements, lower, upper, retry in [
        (7, 7, 4, 6, True), (1, 1, 3, 7, True),
        (0, 0, 3, 7, False), (3, 0, 3, 7, False),
        (20, 0, 3, 7, False), (-1, 0, 3, 7, False),
    ]:
        for _ in range(100):
            stats = [5] * 7 if lower == 4 else [fixtures.randint(3, 7) for _ in range(7)]
            state = [fixtures.getrandbits(64) for _ in range(4)]
            rng = GameRandom(state)
            expected, final = native.mutate_stats(
                stats, increments, decrements, lower, upper, retry, state)
            actual = mutate_stats(stats, increments, decrements, lower, upper, retry, rng)
            assert actual == expected, (stats, actual, expected)
            assert rng.state == final, "Mutation RNG mismatch"
            mutation_count += 1
    for kind, coefficient in [(18, .05), (19, .1), (20, .01), (21, .005)]:
        for appeal in [-20, -1, 0, 1, 10, 20, 100]:
            assert native.effect("Appeal", appeal, kind, 0) == coefficient * appeal
            effect_count += 1
    print(f"PASS: {biased_count} biased RNG + {mutation_count} stat mutation "
          f"native comparisons including RNG state; {effect_count} Appeal effects", flush=True)


if __name__ == "__main__":
    main()
