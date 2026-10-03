"""Effect values, passive/cache separation and MOD/simulator score agreement."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_trait_profile import build_profile, pair_trait_score, number
from breeding_save import cat_fields
from breeding_native_cat import NativeCatReference, read_cat_blobs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path, required=True)
    args = parser.parse_args()
    assert number("__import__('os').system('cmd')") is None
    profile = build_profile(args.game / "resources.gpak")
    assert profile["passive_overrides"]["DualWield"] > 1
    assert profile["active_ability_overrides"]["MeteorStorm"] > profile["active_ability_overrides"]["Spur"]
    assert profile["mutation_overrides"]["body:300"] > 0
    assert min(profile["mutation_overrides"].values()) < 0
    empty = {"ability_slots": ["None"] * 10, "visual_parts": []}
    good = {"ability_slots": ["None"] * 6 + ["DualWield", "None", "None", "None"], "visual_parts": []}
    assert pair_trait_score(empty, good, profile) > pair_trait_score(empty, empty, profile)
    native = NativeCatReference(args.game / "Mewgenics.exe")
    records = read_cat_blobs(args.save)
    # Read a bounded sample with varied names/records through the actual EXE.
    selected = records[:2] + records[-2:]
    for key, raw in selected:
        fields = cat_fields(key, raw)
        cat = native.deserialize(raw)
        expected = [native.string(cat + 0x7D0 + 32 * index) for index in range(6)] + \
            [native.string(cat + 0x910 + 40 * index) for index in range(4)]
        assert fields["ability_slots"] == expected, (key, fields["ability_slots"], expected)
    print("PASS effect values and", len(selected), "native passive/disorder records")


if __name__ == "__main__":
    main()
