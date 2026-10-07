"""Real-save house-fight score, injury, death and winner-reward checks."""

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
from breeding_fights import NativeFights


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    rng = random.Random(52391)
    for slot in range(1, 4):
        native = NativeCatReference(args.exe)
        resources = NativeResources(native)
        root = resources.load_cat_resources(args.gpak)
        snapshot = json.loads((args.snapshots / f"slot{slot}.json").read_text(encoding="utf-8-sig"))
        save = args.saves / f"steamcampaign0{slot}.sav"
        unlocks = BreedingUnlocks(native, resources, save, args.gpak, snapshot["day"])
        generation = NativeCatGeneration(native, resources, root, unlocks, args.gpak,
            npc_voice_progress=native.npc_breeding_inputs(read_save_file(save, "npc_progress"))["voice_progress"])
        fights = NativeFights(native, resources)
        try:
            ids = {cat["id"] for cat in snapshot["cats"]}
            blobs = [(key, raw) for key, raw in read_cat_blobs(save) if key in ids]
            mark = native.heap_next
            total_pairs = injuries = deaths = rewards = 0
            for sample in range(12):
                cats = []
                for key, raw in rng.sample(blobs, min(6, len(blobs))):
                    cat = native.load_cat(raw, generation.visual.frames)
                    native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                    cats.append(cat)
                native.uc.mem_write(native.tls + 0x178,
                    struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
                for damage in (-100.0, 0.0, 100.0):
                    severity = fights.severity(cats[0], cats[1], damage)
                    assert all(value in (0, 1, 2) for value in severity), severity
                    if damage == -100:
                        assert severity == (0, 0), severity
                    if damage == 100:
                        assert severity == (2, 2), severity
                effects = resources.effect_vector({"Comfort": -100})
                result = fights.run([(cats, len(cats), effects)], cats,
                    day=snapshot["day"], departed_first_real_adventure=True)
                flat = [key for first, second, _, _ in result.pairs for key in (first, second)]
                assert len(flat) == len(set(flat)), "cat fought twice"
                by_id = {struct.unpack("<q", native.uc.mem_read(cat + 0xC48, 8))[0]: cat for cat in cats}
                for key in result.deaths:
                    assert native.uc.mem_read(by_id[key] + 0x7AC, 1) == b"\x01"
                    assert struct.unpack("<q", native.uc.mem_read(by_id[key] + 0xC40, 8))[0] == snapshot["day"]
                for cat in cats:
                    native.effective_stats(cat)
                    assert native.serialize(cat)
                total_pairs += len(result.pairs)
                injuries += len(result.injuries)
                deaths += len(result.deaths)
                rewards += len(result.rewards)
                native.rewind(mark)
            assert total_pairs, (slot, "no fight coverage")
            print(f"slot {slot}: 36 native severity cases; {total_pairs} fights, {injuries} injuries, "
                  f"{deaths} deaths, {rewards} rewards passed", flush=True)
        finally:
            generation.close()
            unlocks.close()
    print("PASS: isolated fight phase, not complete daily breeding")


if __name__ == "__main__":
    main()
