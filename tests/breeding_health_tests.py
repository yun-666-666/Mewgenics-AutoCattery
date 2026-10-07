"""Exercise actual saved-cat mutation, disease, and aging transitions offline."""

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
from breeding_health import NativeHealth


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe", "gpak", "saves", "snapshots"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    rng = random.Random(5210)
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
        health = NativeHealth(native, resources, generation)
        try:
            ids = {cat["id"] for cat in snapshot["cats"]}
            blobs = [(key, raw) for key, raw in read_cat_blobs(save) if key in ids]
            events = {}
            mark = native.heap_next
            for key, raw in rng.sample(blobs, min(6, len(blobs))):
                cat = native.load_cat(raw, generation.visual.frames)
                native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                native.uc.mem_write(native.tls + 0x178, struct.pack("<4Q", *(rng.getrandbits(64) for _ in range(4))))
                health.mutate(cat, False)
                native.effective_stats(cat)
                health.mutate(cat, True)
                health.gain_disorder(cat)
                health.clear_disorder(cat, 2)
                assert native.string(cat + 0x960) == "None"
                effects = resources.effect_vector({"Health": 80, "Stimulation": 80})
                health.room_health([cat], effects)
                # Exercise state transitions over time, without claiming this
                # isolated health loop simulates complete breeding days.
                for day in range(snapshot["day"], snapshot["day"] + 180):
                    native.uc.mem_write(unlocks.state + 0x580, struct.pack("<q", day))
                    for _, event in health.age([cat], effects):
                        events[event] = events.get(event, 0) + 1
                native.effective_stats(cat)
                assert native.serialize(cat)
                native.rewind(mark)
                native.uc.mem_write(unlocks.state + 0x580, struct.pack("<q", snapshot["day"]))
            assert events.get("senior") and events.get("death_old_age"), (slot, events)
            print(f"slot {slot}: six saved cats mutation/disorder helpers and health transitions passed {events}", flush=True)
        finally:
            generation.close()
            unlocks.close()
    print("PASS: isolated health transitions; daily fights/feeding/planning still required")


if __name__ == "__main__":
    main()
