"""Check anatomy projection against native 7393E0 for every local head frame.

The graphics adapter supplies resolved timeline child names and identity draw
transforms. Native code decides all anatomy flags. This validates the breeding
projection, not rendering coordinates or the whole game's SWF parser.
"""

import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference
from breeding_visual import PLACEMENT_FLAGS, apply_head_visibility, head_placement_names
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX


class ResolvedHeadClip:
    def __init__(self, native, frames):
        self.native = native
        self.frames = frames
        self.clip = native.allocate(0x200)
        vtable = native.allocate(32)
        noop = native.data + 0x1F100
        native.uc.mem_write(noop, b"\xc3")
        native.uc.mem_write(vtable, struct.pack("<4Q", noop, noop, noop, noop))
        native.uc.mem_write(self.clip, struct.pack("<Q", vtable))
        self.children = {}
        for name in sorted(set().union(*frames)):
            node = native.allocate(0xA0)
            encoded = name.encode() + b"\0"
            text = native.allocate(len(encoded))
            native.uc.mem_write(text, encoded)
            native.uc.mem_write(node + 0x48, struct.pack("<Q", text))
            native.uc.mem_write(node + 0x60, struct.pack("<6f", 1, 0, 0, 1, 0, 0))
            native.uc.mem_write(node + 0x78, struct.pack("<8f", 1, 1, 1, 1, 0, 0, 0, 0))
            self.children[name] = node
        self.pointers = native.allocate(8 * len(self.children))
        native.uc.hook_add(UC_HOOK_CODE, self.factory,
                          begin=native.base + 0x9BB890, end=native.base + 0x9BB890)
        native.uc.hook_add(UC_HOOK_CODE, self.goto,
                          begin=native.base + 0x9A88A0, end=native.base + 0x9A88A0)

    def factory(self, uc, address, size, data):
        assert self.native.string(uc.reg_read(UC_X86_REG_RDX)) == "CatHeadPlacements"
        self.native._return(self.clip)

    def goto(self, uc, address, size, data):
        assert uc.reg_read(UC_X86_REG_RCX) == self.clip
        index = uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF
        assert index < len(self.frames)
        pointers = [self.children[name] for name in sorted(self.frames[index])]
        count = len(pointers)
        uc.mem_write(self.clip + 0xA8, struct.pack("<II", count, count))
        destination = self.pointers if count > 4 else self.clip + 0xB0
        if count > 4:
            uc.mem_write(self.clip + 0xB0, struct.pack("<Q", destination))
        if pointers:
            uc.mem_write(destination, struct.pack(f"<{count}Q", *pointers))
        self.native._return()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--gpak", type=Path, required=True)
    args = parser.parse_args()
    frames = head_placement_names(args.gpak)
    native = NativeCatReference(args.exe)
    ResolvedHeadClip(native, frames)
    original = native.allocate(0x690)
    projected = native.allocate(0x690)
    mark = native.heap_next
    for head in range(1, len(frames) + 1):
        for pointer in (original, projected):
            native.uc.mem_write(pointer + 0x84, struct.pack("<i", head))
        native.call(0x7393E0, original)
        apply_head_visibility(native, projected, frames)
        for offset in PLACEMENT_FLAGS:
            assert native.uc.mem_read(original + offset, 1) == native.uc.mem_read(projected + offset, 1), (head, offset)
        native.rewind(mark)
    print(f"PASS: {len(frames)} head frames, {len(frames)*len(PLACEMENT_FLAGS)} native anatomy flags")


if __name__ == "__main__":
    main()
