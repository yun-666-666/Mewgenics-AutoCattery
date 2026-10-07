"""Exercise physiological births from real cats; no whole-game seed replay claim."""

import argparse
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_native_cat import NativeCatReference, read_cat_blobs, read_save_file
from breeding_native_resources import NativeResources
from breeding_native_generation import NativeCatGeneration
from breeding_unlocks import BreedingUnlocks
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX


def check_strays(native, resources, generation, rng):
    mark = native.heap_next
    for appeal in (0, 20, 80, 160, 320):
        effects = resources.effect_vector({"Appeal": appeal})
        for _ in range(5):
            native.uc.mem_write(native.tls + 0x178,
                                struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
            cat = generation.initialize()
            before = struct.unpack("<7i", native.uc.mem_read(cat + 0x6F0, 28))
            generation.apply_stray_effects(cat, effects)
            after = struct.unpack("<7i", native.uc.mem_read(cat + 0x6F0, 28))
            assert all(old <= new <= 7 for old, new in zip(before, after))
            native.effective_stats(cat)
        native.rewind(mark)
    source = generation.visual.resource_files["data/special_strays.gon"] + 0x28
    special = resources.values[source]
    blocked = 0
    for name, definition in special.fields:
        # Force each resource entry to exercise every special-cat branch;
        # this branch-coverage check is not a probability or RNG-order test.
        def pick(uc, address, size, data):
            if uc.reg_read(UC_X86_REG_RCX) == source:
                native._return(resources.nodes[source][name])

        hook = native.uc.hook_add(UC_HOOK_CODE, pick,
                    begin=native.base + 0x94FE40, end=native.base + 0x94FE40)
        try:
            native.uc.mem_write(native.tls + 0x178,
                                struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
            effects = resources.effect_vector({"Appeal": 80, "SpecialStrayChance": 1})
            cat = generation.initialize()
            generation.apply_stray_effects(cat, effects)
            flags, = struct.unpack("<Q", native.uc.mem_read(cat + 0xBF8, 8))
            expected = definition.get("no_breed") is True
            assert bool(flags & 0x200000) == expected, (name, "special no_breed")
            blocked += expected
            native.effective_stats(cat)
            raw = native.serialize(cat)
            restored = native.load_cat(raw, generation.visual.frames)
            assert native.serialize(restored) == raw, (name, "special round trip")
        finally:
            native.uc.hook_del(hook)
            native.rewind(mark)
    return len(special.fields), blocked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--gpak", type=Path, required=True)
    parser.add_argument("--saves", type=Path, required=True)
    parser.add_argument("--snapshots", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=100)
    parser.add_argument("--strays", action="store_true")
    args = parser.parse_args()
    rng = random.Random(1921)
    total = 0
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        root = resources.load_cat_resources(args.gpak)
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text(encoding="utf-8-sig"))
        save = args.saves / f"steamcampaign0{slot}.sav"
        unlocks = BreedingUnlocks(native, resources, save, args.gpak, snapshot["day"])
        voice_progress = native.voice_progress(read_save_file(save, "npc_progress"))
        generation = NativeCatGeneration(native, resources, root, unlocks, args.gpak,
                                          npc_voice_progress=voice_progress)
        try:
            ids = {cat["id"] for cat in snapshot["cats"]}
            parents = []
            for key, raw in read_cat_blobs(save):
                if key in ids:
                    cat = native.load_cat(raw, generation.visual.frames)
                    native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                    parents.append(cat)
            mark = native.heap_next
            for sample in range(args.samples):
                state = [rng.getrandbits(64) for _ in range(4)]
                native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", *state))
                mother, father = rng.sample(parents, 2)
                # Also force defect branches with elevated COI; the actual
                # planner still uses the user's avoid-inbreeding settings.
                coi = (0.0, 0.25, 0.5)[sample % 3]
                child = generation.breed(mother, father, coi)
                stats = struct.unpack("<7i", native.uc.mem_read(child + 0x6F0, 28))
                parent_stats = [struct.unpack("<7i", native.uc.mem_read(p + 0x6F0, 28))
                                for p in (mother, father)]
                assert all(value in (parent_stats[0][i], parent_stats[1][i])
                           for i, value in enumerate(stats)), (slot, sample, "inheritance", stats)
                assert all(1 <= value <= 7 for value in stats), (slot, sample, stats)
                native.effective_stats(child)
                raw = native.serialize(child)
                reloaded = native.load_cat(raw, generation.visual.frames)
                assert native.serialize(reloaded) == raw, (slot, sample, "newborn round trip")
                native.rewind(mark)
                total += 1
                if (sample + 1) % 25 == 0:
                    print(f"slot {slot}: {sample + 1}/{args.samples} native births", flush=True)
            if args.strays:
                count, blocked = check_strays(native, resources, generation, rng)
                print(f"slot {slot}: 25 ordinary strays; all {count} special entries, "
                      f"{blocked} no-breed flags verified", flush=True)
        finally:
            generation.close()
            unlocks.close()
    print(f"PASS: {total} physiological native births with effective-attribute execution")


if __name__ == "__main__":
    main()
