"""Check all local class/trait candidates against native unlock decisions."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference
from breeding_native_resources import NativeResources
from breeding_resources import read_resources
from breeding_unlocks import BreedingUnlocks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--gpak", type=Path, required=True)
    parser.add_argument("--saves", type=Path, required=True)
    args = parser.parse_args()
    native = NativeCatReference(args.exe)
    resources = NativeResources(native)
    sources = read_resources(args.gpak, prefixes=("data/classes/", "data/passives/"))
    abilities = set()
    passives = set()
    for path, source in sources.items():
        if path.startswith("data/passives/"):
            passives.update(source)
        else:
            for definition in source.values():
                if isinstance(definition, dict):
                    abilities.update(definition.get("ability_pool", []))
                    passives.update(definition.get("passive_pool", []))
    total = 0
    for slot in range(1, 4):
        save = args.saves / f"steamcampaign0{slot}.sav"
        unlocks = BreedingUnlocks(native, resources, save, args.gpak, day=100)
        try:
            checked = 0
            for kind, names in (("abilities", abilities), ("passives", passives)):
                for name in sorted(names | unlocks.locked[kind] | unlocks.unlocked[kind]):
                    expected = unlocks.allowed(kind, name)
                    assert unlocks.native_allowed(kind, name) == expected, (slot, kind, name)
                    checked += 1
            total += checked
            print(f"slot {slot}: native unlock lists {[len(x) for x in unlocks.lists]}, {checked} decisions matched", flush=True)
        finally:
            unlocks.close()
    print(f"PASS: {total} native unlock decisions")


if __name__ == "__main__":
    main()
