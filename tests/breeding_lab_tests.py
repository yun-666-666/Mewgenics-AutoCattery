"""Focused native differential tests; requires a local game EXE, never runs it."""

import argparse
from pathlib import Path
import random
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_lab import GameRandom, inheritance_weights, inherit_stats, experiment
from breeding_native_reference import NativeStatReference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    native = NativeStatReference(args.exe)
    fixture_rng = random.Random(92341)
    weights_to_check = [
        (1.0, 1.0, 0.0), (1.0, 1.0, 0.2), (1.0, 1.0, -0.4),
        (0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 0.0),
        (-1.0, 2.0, 0.0), (3.0, -1.0, 0.0), (2.0, 1.5, 0.8),
    ]
    comparisons = 0
    for weights in weights_to_check:
        for _ in range(120):
            mother = [fixture_rng.randint(1, 7) for _ in range(7)]
            father = [fixture_rng.randint(1, 7) for _ in range(7)]
            state = [fixture_rng.getrandbits(64) for _ in range(4)]
            python_rng = GameRandom(state)
            expected, final_state = native.inherit(mother, father, weights, state)
            actual = inherit_stats(mother, father, weights, python_rng)
            assert actual == expected, (mother, father, weights, actual, expected)
            assert python_rng.state == final_state, "Native RNG consumption differs"
            comparisons += 1
    effects_checked = 0
    for name, kind, initial, coefficient in [
        ("InheritStatFavorMom", 6, 1.0, 0.25),
        ("InheritStatFavorDad", 7, 1.0, 0.25),
        ("InheritStatFavorBest", 8, 0.0, 0.1),
        ("Stimulation", 8, 0.0, 0.01),
    ]:
        for value in [-10.0, -1.0, 0.0, 1.0, 12.5, 100.0]:
            actual = native.effect(name, value, kind, initial)
            assert actual == initial + coefficient * value, (name, value, actual)
            effects_checked += 1
    assert inheritance_weights({"Stimulation": 20.0}) == (1.0, 1.0, 0.2)
    mother = [7, 1, 7, 1, 7, 1, 7]
    father = [1, 7, 1, 7, 1, 7, 1]
    output = experiment(mother, father, {}, [1, 2, 3, 4], 100, 7)
    assert output["all_seven_probability_under_uniform_independent_draws"] == 1 / 128
    assert experiment([7] * 7, [7] * 7, {}, [1, 2, 3, 4], 100, 7)[
        "observed_all_seven_count"
    ] == 100
    assert experiment([6] * 7, [6] * 7, {}, [1, 2, 3, 4], 100, 7)[
        "observed_all_seven_count"
    ] == 0
    print(f"PASS: {comparisons} native seven-stat + final RNG comparisons; "
          f"{effects_checked} native effect checks; conditional distribution checks")


if __name__ == "__main__":
    main()
