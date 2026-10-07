"""Native food requirement differential and successive-night starvation."""

import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference, read_cat_blobs
from breeding_native_resources import NativeResources
from breeding_feeding import NativeFeeding
from breeding_visual import head_placement_names
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RBP, UC_X86_REG_RSP, UC_X86_REG_RSI, UC_X86_REG_XMM9, UC_X86_REG_EDX, UC_X86_REG_RIP


def native_requirement(native, cat):
    mark = native.heap_next
    hook = native.uc.hook_add(UC_HOOK_CODE, lambda *_: native._return(cat),
        begin=native.base + 0x1FF1E0, end=native.base + 0x1FF1E0)
    try:
        frame = native.stack - 0x4000
        native.uc.reg_write(UC_X86_REG_RBP, frame)
        native.uc.reg_write(UC_X86_REG_RSP, frame - 0x100)
        native.uc.reg_write(UC_X86_REG_RSI, cat)
        native.uc.reg_write(UC_X86_REG_XMM9, struct.unpack("<Q", struct.pack("<d", 1.0))[0])
        stop = native.base + 0x1E90C0
        native.uc.emu_start(native.base + 0x1E9045, stop, count=2000000)
        assert native.uc.reg_read(UC_X86_REG_RIP) == stop
        return struct.unpack("<i", struct.pack("<I", native.uc.reg_read(UC_X86_REG_EDX)))[0]
    finally:
        native.uc.hook_del(hook)
        native.rewind(mark)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    frames = head_placement_names(args.gpak)
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        resources.load_cat_resources(args.gpak)
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text(encoding="utf-8-sig"))
        state = resources.set_day(snapshot["day"])
        ids = {cat["id"] for cat in snapshot["cats"]}
        feeding = NativeFeeding(native, resources)
        cats = []
        for key, raw in read_cat_blobs(args.saves / f"steamcampaign0{slot}.sav"):
            if key in ids:
                cat = native.load_cat(raw, frames)
                native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                assert feeding.requirement(cat) == native_requirement(native, cat)
                cats.append(cat)
        candidate = next(cat for cat in cats if feeding.requirement(cat) > 0
                         and native.uc.mem_read(cat + 0x7AC, 1) == b"\0")
        key, = struct.unpack("<q", native.uc.mem_read(candidate + 0xC48, 8))
        native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", 112, 223, 334, 445))
        effects = resources.effect_vector({"AutoFeedCat": 20})
        # First feed clears any hunger already present in the real snapshot.
        result = feeding.run([candidate], {candidate: 0}, {0: effects},
            food=0, storage_limit=100, edible_furniture=[])
        assert not result.hungry and not result.deaths
        assert result.food_remaining == 0
        empty = resources.effect_vector({})
        result = feeding.run([candidate], {candidate: 0}, {0: empty},
            food=0, storage_limit=100, edible_furniture=[])
        assert result.hungry == [key] and not result.deaths
        native.uc.mem_write(state + 0x580, struct.pack("<q", snapshot["day"] + 1))
        result = feeding.run([candidate], {candidate: 0}, {0: empty},
            food=0, storage_limit=100, edible_furniture=[])
        assert result.deaths == [key]
        assert struct.unpack("<q", native.uc.mem_read(candidate + 0xC40, 8))[0] == snapshot["day"] + 1
        print(f"slot {slot}: {len(cats)} native food requirements; autofeed and consecutive hunger death passed", flush=True)
    print("PASS: feeding phase, not full daily/multigeneration acceptance")


if __name__ == "__main__":
    main()
