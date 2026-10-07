"""Compare native deserialization with previously exported research snapshots."""

import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference, read_cat_blobs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--saves", type=Path, required=True)
    parser.add_argument("--snapshots", type=Path, required=True)
    args = parser.parse_args()
    native = NativeCatReference(args.exe)
    total = 0
    for slot in range(1, 4):
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text())
        records = dict(read_cat_blobs(args.saves / f"steamcampaign0{slot}.sav"))
        for expected in snapshot["cats"]:
            cat = native.deserialize(records[expected["id"]])

            def read(offset, fmt):
                return struct.unpack(fmt, native.uc.mem_read(cat + offset, struct.calcsize(fmt)))

            assert list(read(0x6F0, "<7i")) == expected["genetic"]
            for field, offset, fmt in (
                ("native_sex", 0x58, "<i"), ("sex_presentation", 0x5C, "<i"),
                ("libido", 0xBB8, "<d"), ("sexuality", 0xBC0, "<d"),
                ("lover_id", 0xBC8, "<q"), ("love", 0xBD0, "<d"),
                ("enemy_id", 0xBD8, "<q"), ("hate", 0xBE0, "<d"),
                ("aggression", 0xBE8, "<d"), ("fertility", 0xBF0, "<d"),
                ("old_state", 0xC34, "<i"),
            ):
                actual, = read(offset, fmt)
                assert actual == expected[field], (slot, expected["id"], field, actual, expected[field])
            assert native.string(cat + 0xC10) == expected["class_id"]
            assert [native.string(cat + 0x7D0 + 32 * i) for i in range(10)] == expected["abilities"]
            assert [[native.string(cat + 0x910 + 40 * i), read(0x930 + 40 * i, "<i")[0]]
                    for i in range(4)] == expected["passives"]
            total += 1
        print(f"slot {slot}: {len(snapshot['cats'])} complete native records matched", flush=True)
    print(f"PASS: {total} native records, complete byte consumption and breeding fields")


if __name__ == "__main__":
    main()
