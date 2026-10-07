"""Native arrival-effect differential and three-save social-source checks."""

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
from breeding_population import Pedigree
from breeding_arrivals import ArrivalInputs, NativeArrivals, arrival_effects
from unicorn.x86_const import UC_X86_REG_RBP, UC_X86_REG_RSP, UC_X86_REG_R12, UC_X86_REG_RIP


def check_effects(native, resources, generation):
    mark = native.heap_next
    progress = generation.visual.progress
    state = generation.state
    old_day = bytes(native.uc.mem_read(state + 0x580, 8))
    old_progress = bytes(native.uc.mem_read(progress + 0x780, 24))
    checked = 0
    try:
        for day in (0, 10, 11, 340, 1470, 2000):
            for special, social in ((-1, 0), (0, 2), (8, 4)):
                inputs = ArrivalInputs(1, special, list(range(social)))
                base_effects = {"Appeal": 80, "SpecialStrayChance": 0.03, "GayStrayChance": 0.2}
                expected = arrival_effects(base_effects, day, inputs)
                vector = resources.effect_vector(base_effects)
                frame = native.stack - 0x80
                native.uc.mem_write(frame - 0x71, bytes(native.uc.mem_read(vector, 24)))
                native.uc.mem_write(state + 0x580, struct.pack("<q", day))
                native.uc.mem_write(progress + 0x784, struct.pack("<i", social))
                native.uc.mem_write(progress + 0x790, struct.pack("<i", special))
                native.uc.reg_write(UC_X86_REG_RBP, frame)
                native.uc.reg_write(UC_X86_REG_RSP, native.stack - 0x200)
                native.uc.reg_write(UC_X86_REG_R12, 0)
                native.uc.emu_start(native.base + 0x1EEC63, native.base + 0x1EEE85, count=2000000)
                assert native.uc.reg_read(UC_X86_REG_RIP) == native.base + 0x1EEE85
                begin, end = struct.unpack("<QQ", native.uc.mem_read(frame - 0x71, 16))
                actual = {}
                for pointer in range(begin, end, 0x28):
                    name = native.string(pointer)
                    value, = struct.unpack("<d", native.uc.mem_read(pointer + 0x20, 8))
                    actual[name] = actual.get(name, 0) + value
                assert actual == expected, (day, special, social, actual, expected)
                checked += 1
                native.rewind(mark)
    finally:
        native.uc.mem_write(state + 0x580, old_day)
        native.uc.mem_write(progress + 0x780, old_progress)
    print(f"PASS: {checked} native arrival-effect comparisons", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    rng = random.Random(523)
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        root = resources.load_cat_resources(args.gpak)
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text(encoding="utf-8-sig"))
        save = args.saves / f"steamcampaign0{slot}.sav"
        unlocks = BreedingUnlocks(native, resources, save, args.gpak, snapshot["day"])
        npc = native.npc_breeding_inputs(read_save_file(save, "npc_progress"))
        generation = NativeCatGeneration(native, resources, root, unlocks, args.gpak,
            npc_voice_progress=npc["voice_progress"])
        try:
            if slot == 1:
                check_effects(native, resources, generation)
            ids = {cat["id"] for cat in snapshot["cats"]}
            cats = {}
            for key, raw in read_cat_blobs(save):
                if key in ids:
                    cat = native.load_cat(raw, generation.visual.frames)
                    native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                    cats[key] = cat
            pedigree = Pedigree(snapshot["pedigree"])
            inputs = ArrivalInputs(unlocks.properties.get("min_strays_tomorrow", 1),
                                   npc["special_stray_counter"], npc["same_sex_ids"])
            arrivals = NativeArrivals(native, resources, generation, pedigree, inputs)
            total = 0
            for sample in range(3):
                # First uses saved inputs; the other two explicitly cover
                # social-source selection and the low-population mercy path.
                if sample == 1:
                    inputs.same_sex_ids = list(cats)[:4]
                minimum = inputs.min_strays_tomorrow
                native.uc.mem_write(native.tls + 0x178,
                    struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
                result = arrivals.run(day=snapshot["day"], next_id=max(pedigree.entries) + 1,
                    cats=cats, eligible_adults=2 if sample == 2 else len(cats),
                    mercy_eligible=sample == 2, house_effects={"Appeal": 80})
                assert len(result) == max(minimum, 2 if sample == 2 else 1)
                for key, cat in result:
                    assert pedigree.entries[key] == (None, None, 0)
                    native.effective_stats(cat)
                    assert native.serialize(cat)
                assert inputs.min_strays_tomorrow == 1 and inputs.special_stray_counter == -1
                assert not inputs.same_sex_ids
                total += len(result)
            print(f"slot {slot}: full NPC read, saved/social/mercy arrivals ({total} cats) passed", flush=True)
        finally:
            generation.close()
            unlocks.close()
    print("PASS: arrival phase only, not full daily/multigeneration validation")


if __name__ == "__main__":
    main()
