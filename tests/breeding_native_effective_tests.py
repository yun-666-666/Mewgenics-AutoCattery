"""Native effective-stat fixtures and integrated three-save mating weights."""

import argparse
import json
import math
from pathlib import Path
import struct
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference, read_cat_blobs
from breeding_native_resources import NativeResources
from breeding_population import attraction
from breeding_visual import head_placement_names


def fixtures(native, resources):
    resources.set_day(100)
    cat = native.allocate(0xC58)
    native.call(0x5D6B0, cat)
    assert native.effective_stats(cat) == [5] * 7
    native.uc.mem_write(cat + 0x6F0, struct.pack("<7i", 1, 2, 3, 4, 5, 6, 7))
    resources.string(cat + 0x910, "BareMinimum")
    native.uc.mem_write(cat + 0x930, struct.pack("<i", 1))
    assert native.effective_stats(cat) == [5, 5, 5, 5, 5, 6, 7]
    native.uc.mem_write(cat + 0x930, struct.pack("<i", 2))
    assert native.effective_stats(cat) == [7] * 7
    resources.string(cat + 0x910, "EternalYouth")
    native.uc.mem_write(cat + 0x6F0, struct.pack("<7i", *([5] * 7)))
    assert native.call(0xD3130, cat) & 255 == 1
    assert native.effective_stats(cat) == [3] * 7
    resources.string(cat + 0x910, "")
    native.uc.mem_write(cat + 0xC38, struct.pack("<q", 99))
    assert native.call(0xD3130, cat) & 255 == 1
    native.uc.mem_write(cat + 0xC38, struct.pack("<q", 98))
    assert native.call(0xD3130, cat) & 255 == 0
    print("PASS: neutral stats, passive levels, permanent kitten, age 1/2 boundary", flush=True)


def pairs(native, resources, saves, snapshots, output):
    start = time.monotonic()
    results = []
    baseline = native.heap_next
    for slot in range(1, 4):
        snapshot = json.loads((snapshots / f"slot{slot}.json").read_text())
        resources.set_day(snapshot["day"])
        records = dict(read_cat_blobs(saves / f"steamcampaign0{slot}.sav"))
        cats = []
        for expected in snapshot["cats"]:
            pointer = native.load_cat(records[expected["id"]], resources.head_frames)
            native.uc.mem_write(pointer + 0xC48, struct.pack("<q", expected["id"]))
            mark = native.heap_next
            stats = native.effective_stats(pointer)
            kitten = bool(native.call(0xD3130, pointer) & 255)
            native.rewind(mark)
            cats.append((expected, pointer, stats, kitten))
        mark = native.heap_next
        checked = 0
        for source, pointer, _, _ in cats:
            for target, other, stats, kitten in cats:
                expected = 0.0
                if source["id"] != target["id"] and not kitten and not (
                    source["no_breed"] or target["no_breed"]
                ):
                    expected = attraction(
                        source["libido"], source["sexuality"], source["sex_presentation"],
                        target["sex_presentation"], stats[5], source["lover_id"],
                        target["id"], source["love"],
                    )
                actual = native.mating_weight(pointer, other)
                native.rewind(mark)
                if not math.isclose(expected, actual, rel_tol=1e-12, abs_tol=1e-12):
                    raise AssertionError((slot, source["id"], target["id"], expected, actual))
                checked += 1
            if checked % (len(cats) * 20) == 0:
                print(f"slot {slot}: {checked}/{len(cats)**2} native pair weights", flush=True)
        results.append({"slot": slot, "cats": len(cats), "pairs": checked,
                        "effective_charisma_differs": sum(c[0]["genetic"][5] != c[2][5] for c in cats),
                        "native_kittens": sum(c[3] for c in cats)})
        print(f"PASS slot {slot}: {checked} native pair weights", flush=True)
        native.rewind(baseline)
    report = {"scope": "native effective attributes and mating weight; not daily simulation",
              "slots": results, "elapsed_seconds": time.monotonic() - start}
    if output:
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--gpak", type=Path, required=True)
    parser.add_argument("--saves", type=Path)
    parser.add_argument("--snapshots", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    native = NativeCatReference(args.exe)
    resources = NativeResources(native)
    resources.load_cat_resources(args.gpak)
    resources.head_frames = head_placement_names(args.gpak)
    fixtures(native, resources)
    if args.saves and args.snapshots:
        pairs(native, resources, args.saves, args.snapshots, args.output)


if __name__ == "__main__":
    main()
