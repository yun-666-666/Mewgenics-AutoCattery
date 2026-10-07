"""Check ordered native pool extraction against local resources and save unlocks."""

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference
from breeding_native_resources import NativeResources
from breeding_native_pools import NativeBreedingPools
from breeding_resources import GonObject
from breeding_unlocks import BreedingUnlocks


def entries(value):
    if isinstance(value, GonObject):
        return value.fields
    if isinstance(value, list):
        return [("", item) for item in value]
    if value is None:
        return []
    return [("", value)]


def expected_pool(resources, unlocks, root, kind, name):
    field = "ability_pool" if kind == "abilities" else "passive_pool"
    classes = resources.values[root + 0x508]
    pools = resources.values[root + (0x3A8 if kind == "abilities" else 0x458)]
    selected = []
    if name in ("Jester", "AnyUnlocked", "JesterMinusColorless"):
        ordered = [x for x in unlocks.lists[0] if x not in ("Jester", "Colorless")]
        ordered += [x for x in ("Jester", "Colorless") if x in unlocks.lists[0]
                    and (x != "Colorless" or name != "JesterMinusColorless")]
        if name != "JesterMinusColorless" or kind == "abilities":
            for class_name in ordered:
                selected.extend(entries(classes.get(class_name, {}).get(field)))
        if kind == "passives" and name in ("Jester", "AnyUnlocked"):
            return [(key, value) for key, value in selected if unlocks.allowed(kind, value)]
        if name == "JesterMinusColorless" and kind == "abilities":
            return [(key, value) for key, value in selected if unlocks.allowed(kind, value)]
    source = pools.get(name)
    for class_name, definition in classes.fields:
        if class_name == name:
            source = definition.get(field)
        if kind == "abilities" and name.startswith(class_name + "."):
            source = definition.get("ability_groups", {}).get(name[len(class_name) + 1:])
    selected.extend(entries(source))
    return [(key, value) for key, value in selected if unlocks.allowed(kind, value)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--gpak", type=Path, required=True)
    parser.add_argument("--saves", type=Path, required=True)
    args = parser.parse_args()
    total = 0
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        root = resources.load_cat_resources(args.gpak)
        unlocks = BreedingUnlocks(native, resources, args.saves / f"steamcampaign0{slot}.sav",
                                  args.gpak, day=100)
        pools = NativeBreedingPools(native, resources, unlocks, root, args.gpak)
        try:
            classes = resources.values[root + 0x508]
            for kind, offset in (("abilities", 0x3A8), ("passives", 0x458)):
                names = set(classes) | set(resources.values[root + offset])
                names.update(("Jester", "AnyUnlocked"))
                if kind == "abilities":
                    names.add("JesterMinusColorless")
                    for name, definition in classes.fields:
                        names.update(name + "." + group for group in definition.get("ability_groups", {}))
                for name in sorted(names):
                    actual = pools.extract(kind, name).fields
                    expected = expected_pool(resources, unlocks, root, kind, name)
                    assert actual == expected, (slot, kind, name, len(actual), len(expected))
                    total += 1
                print(f"slot {slot}: {len(names)} ordered {kind} pools matched", flush=True)
        finally:
            unlocks.close()
    print(f"PASS: {total} native ordered pools")


if __name__ == "__main__":
    main()
