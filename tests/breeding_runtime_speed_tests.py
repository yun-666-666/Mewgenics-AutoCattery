"""Measure native runtime adapters against their previous implementations.

The slow adapter functions are kept here as the behavior/performance baseline;
the same saved cats and RNG seed drive both runs, including native births.
"""

import argparse
from dataclasses import asdict
import json
from pathlib import Path
import struct
import sys
import time
from unittest.mock import patch

import pefile
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RIP, UC_X86_REG_RSP

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_checkpoint import capture_simulation
from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_native_cat import NativeCatReference
from breeding_save import read_snapshot
from breeding_trait_experiment import seed_state


def previous_return(self, value=0):
    stack = self.uc.reg_read(UC_X86_REG_RSP)
    ret, = struct.unpack("<Q", self.uc.mem_read(stack, 8))
    self.uc.reg_write(UC_X86_REG_RAX, value)
    self.uc.reg_write(UC_X86_REG_RSP, stack + 8)
    self.uc.reg_write(UC_X86_REG_RIP, ret)


def previous_string(self, address):
    length, capacity = struct.unpack("<QQ", self.uc.mem_read(address + 16, 16))
    if capacity > 15:
        address, = struct.unpack("<Q", self.uc.mem_read(address, 8))
    return bytes(self.uc.mem_read(address, length)).decode("utf-8")


def run(args, snapshot, previous):
    start = time.perf_counter()
    sim = NativeDailySimulation(args.game / "Mewgenics.exe", args.game / "resources.gpak",
                               args.save, snapshot,
                               SimulationConfig(offspring_all_seven_assist=True, food_supply_assist=True))
    init_seconds = time.perf_counter() - start
    sim.native.fight_stat_cache_enabled = not previous
    births = []
    native_breed = sim.generation.breed

    def record_birth(*parents, **kwargs):
        cat = native_breed(*parents, **kwargs)
        births.append(sim.native.serialize(cat))
        return cat

    sim.generation.breed = record_birth
    rows = []
    start = time.perf_counter()
    try:
        sim.native.uc.mem_write(sim.native.tls + 0x178, seed_state(7123))
        for day in range(args.days):
            result = sim.step()
            rows.append((asdict(result), capture_simulation(sim), list(births)))
            print(f"previous={previous} day={day + 1} complete", flush=True)
        return rows, init_seconds, time.perf_counter() - start
    finally:
        sim.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path, required=True)
    parser.add_argument("--days", type=int, default=2)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    exe = args.game / "Mewgenics.exe"
    full = pefile.PE(str(exe), fast_load=False)
    fast = pefile.PE(str(exe), fast_load=True)
    assert full.get_memory_mapped_image() == fast.get_memory_mapped_image()
    full.close()
    fast.close()
    snapshot = read_snapshot(args.save, args.game / "resources.gpak")
    real_pe = pefile.PE

    class PreviousPE(real_pe):
        def __init__(self, *params, **kwargs):
            kwargs["fast_load"] = False
            super().__init__(*params, **kwargs)

    with patch.object(NativeCatReference, "_return", previous_return), \
            patch.object(NativeCatReference, "string", previous_string), \
            patch.object(pefile, "PE", PreviousPE):
        before, old_init, old_days = run(args, snapshot, True)
    after, new_init, new_days = run(args, snapshot, False)
    assert before == after, "native days, births, cat bytes, RNG or logical state changed"
    report = {"days": args.days, "mapped_exe_bytes_equal": True,
              "native_events_birth_bytes_and_day_states_equal": True,
              "previous_init_seconds": old_init, "optimized_init_seconds": new_init,
              "previous_day_seconds": old_days, "optimized_day_seconds": new_days,
              "day_speedup": old_days / new_days,
              "total_speedup": (old_init + old_days) / (new_init + new_days)}
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
