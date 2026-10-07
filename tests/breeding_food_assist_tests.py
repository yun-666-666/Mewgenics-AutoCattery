"""Food assistance on/off against a real cat and native hunger state."""
import argparse
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_save import read_snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--save", type=Path, required=True)
    args = parser.parse_args()
    snapshot = read_snapshot(args.save, args.game / "resources.gpak")
    sim = NativeDailySimulation(args.game / "Mewgenics.exe", args.game / "resources.gpak",
                                args.save, snapshot, SimulationConfig())
    try:
        assert not sim.config.food_supply_assist
        native = sim.native
        cat = next(cat for cat in sim.cats.values()
                   if not sim._dead(cat) and sim.feeding.requirement(cat) > 0)
        empty = sim.resources.effect_vector({})
        room_effects = {"room": empty}

        def hunger():
            return struct.unpack("<Q", native.uc.mem_read(cat + 0xBF8, 8))[0]

        def run(enabled, room="room", food=0):
            native.uc.mem_write(cat + 0xBF8, struct.pack("<Q", hunger() & ~4))
            return sim.feeding.run([cat], {cat: room}, room_effects,
                food=food, storage_limit=100, edible_furniture=[], food_supply_assist=enabled)

        off = run(False)
        assert off.hungry and not off.deaths and hunger() & 4
        on = run(True)
        assert not on.hungry and not on.deaths and not hunger() & 4
        assert on.food_remaining == 0
        stocked = run(True, food=17)
        assert stocked.food_remaining == 17
        assert sim.resources.effect_value(empty, 0x1D, 0.0) == 0
        outside = run(True, room=None)
        assert outside.hungry and hunger() & 4
        disabled_again = run(False)
        assert disabled_again.hungry and hunger() & 4
        print("PASS food assist: on feeds indoors; off restores native hunger; "
              "outside unaffected; stock and furniture effect preserved", flush=True)
    finally:
        sim.close()


if __name__ == "__main__":
    main()
