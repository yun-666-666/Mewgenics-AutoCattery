"""Native differential room draws and real-save mating/birth phase checks."""

import argparse
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_lab import GameRandom
from breeding_native_cat import NativeCatReference, read_cat_blobs, read_save_file
from breeding_native_resources import NativeResources
from breeding_native_generation import NativeCatGeneration
from breeding_unlocks import BreedingUnlocks
from breeding_population import Pedigree
from breeding_night import NativeBreedingNight, shuffle_room, weighted_partner
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_XMM0


def check_draws(native):
    mark = native.heap_next
    rng = random.Random(238)
    vector = native.allocate(16)
    storage = native.allocate(64)
    used = native.allocate(16)
    actor = native.allocate(8)
    context = native.allocate(16)
    native.uc.mem_write(context, struct.pack("<QQ", used, actor))
    handles = [native.allocate(0x90) for _ in range(8)]
    cats = [native.allocate(0xC58) for _ in range(8)]
    for index, (handle, cat) in enumerate(zip(handles, cats)):
        native.uc.mem_write(handle + 0x80, struct.pack("<Q", index))
        native.uc.mem_write(cat + 0xC48, struct.pack("<Q", index))
    weights = {}

    def lookup(uc, address, size, data):
        native._return(cats[uc.reg_read(UC_X86_REG_RDX)])

    def weight(uc, address, size, data):
        value = weights[uc.reg_read(UC_X86_REG_RDX)]
        uc.reg_write(UC_X86_REG_XMM0, struct.unpack("<Q", struct.pack("<d", value))[0])
        native._return(0)

    hooks = [native.uc.hook_add(UC_HOOK_CODE, callback,
                begin=native.base + address, end=native.base + address)
             for callback, address in ((lookup, 0xD7220), (weight, 0xD2850))]
    try:
        for sample in range(128):
            count = sample % 9
            state = tuple(rng.getrandbits(64) for _ in range(4))
            native.uc.mem_write(vector, struct.pack("<IIQ", count, count, storage))
            native.uc.mem_write(storage, struct.pack("<8Q", *handles))
            native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", *state))
            expected = handles[:count]
            reference = GameRandom(state)
            shuffle_room(expected, reference)
            native.call(0x86F00, vector, native.tls + 0x178)
            actual = list(struct.unpack(f"<{count}Q", native.uc.mem_read(storage, count * 8))) if count else []
            assert actual == expected, (sample, "shuffle")
            assert bytes(native.uc.mem_read(native.tls + 0x178, 32)) == struct.pack("<4Q", *reference.state)
            values = [0.0 if sample % 3 == 0 else rng.random() * 4 for _ in range(count)]
            weights = dict(zip(cats, values))
            native.uc.mem_write(storage, struct.pack("<8Q", *handles))
            native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", *state))
            reference = GameRandom(state)
            expected = weighted_partner(handles[:count], values, reference)
            actual = native.call(0x1F21A0, vector, context, 0, native.tls + 0x178)
            assert actual == (expected or 0), (sample, "weighted selection")
            assert bytes(native.uc.mem_read(native.tls + 0x178, 32)) == struct.pack("<4Q", *reference.state)
    finally:
        for hook in hooks:
            native.uc.hook_del(hook)
        native.rewind(mark)
    print("PASS: 128 native shuffle and weighted-partner cases, including empty/zero weights", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    rng = random.Random(723)
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        root = resources.load_cat_resources(args.gpak)
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text(encoding="utf-8-sig"))
        save = args.saves / f"steamcampaign0{slot}.sav"
        unlocks = BreedingUnlocks(native, resources, save, args.gpak, snapshot["day"])
        generation = NativeCatGeneration(native, resources, root, unlocks, args.gpak,
            npc_voice_progress=native.voice_progress(read_save_file(save, "npc_progress")))
        try:
            if slot == 1:
                check_draws(native)
            ids = {cat["id"] for cat in snapshot["cats"]}
            raw_cats = [(key, raw) for key, raw in read_cat_blobs(save) if key in ids]
            mark = native.heap_next
            births = pairs = 0
            for sample in range(12):
                pointers = {}
                for key, raw in rng.sample(raw_cats, min(6, len(raw_cats))):
                    cat = native.load_cat(raw, generation.visual.frames)
                    native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                    pointers[key] = cat
                pedigree = Pedigree(snapshot["pedigree"])
                first_id = max(pedigree.entries) + 1
                night = NativeBreedingNight(native, generation, pedigree, first_id)
                native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
                result = night.run([(list(pointers.values()), 40, 0, 0)], list(pointers.values()), snapshot["day"])
                flat = [key for pair in result.pairs for key in pair]
                assert len(flat) == len(set(flat)), "cat mated twice in one night"
                for key, cat in result.newborns:
                    mother, father, coefficient = pedigree.entries[key]
                    assert mother in pointers and father in pointers
                    assert coefficient == pedigree.coi(mother, father)
                    assert struct.unpack("<q", native.uc.mem_read(cat + 0xC38, 8))[0] == snapshot["day"]
                    stats = struct.unpack("<7i", native.uc.mem_read(cat + 0x6F0, 28))
                    inherited = [struct.unpack("<7i", native.uc.mem_read(pointers[parent] + 0x6F0, 28))
                                 for parent in (mother, father)]
                    assert all(value in (inherited[0][i], inherited[1][i]) for i, value in enumerate(stats))
                    native.effective_stats(cat)
                    assert native.serialize(cat)
                pairs += len(result.pairs)
                births += len(result.newborns)
                native.rewind(mark)
            assert pairs and births, (slot, "no mating/birth coverage")
            print(f"slot {slot}: 12 independent six-cat nights; {pairs} pairs, {births} births passed", flush=True)
        finally:
            generation.close()
            unlocks.close()
    print("PASS: room phase only; not a full daily or multi-generation validation")


if __name__ == "__main__":
    main()
