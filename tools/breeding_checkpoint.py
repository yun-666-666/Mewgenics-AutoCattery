"""Day-boundary checkpoints for the native daily simulator's local experiments."""

from dataclasses import asdict
import struct

from breeding_arrivals import ArrivalInputs
from breeding_population import Pedigree


def capture_simulation(sim):
    return {"day": sim.day, "food": sim.food, "next_id": sim.next_id,
            "furniture": list(sim.furniture), "rooms": dict(sim.cat_rooms),
            "cats": {key: sim.native.serialize(cat) for key, cat in sim.cats.items()},
            "pedigree": [(key, *parents) for key, parents in sim.pedigree.entries.items()],
            "arrival_inputs": asdict(sim.arrival_inputs),
            "bonus_items": list(sim.generation.bonus_items),
            "item_counter": bytes(sim.native.uc.mem_read(sim.native.base + 0x13C6C38, 8)),
            "rng": bytes(sim.native.uc.mem_read(sim.native.tls + 0x178, 32))}


def restore_simulation(sim, state):
    sim.native.rewind(sim.population_mark)
    sim.cats = {}
    for key, raw in state["cats"].items():
        cat = sim.native.load_cat(raw, sim.generation.visual.frames)
        sim.native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
        sim.cats[key] = cat
    sim.day, sim.food, sim.next_id = state["day"], state["food"], state["next_id"]
    sim.furniture, sim.cat_rooms = state["furniture"], state["rooms"]
    sim.pedigree = Pedigree(state["pedigree"])
    sim.arrival_inputs = ArrivalInputs(**state["arrival_inputs"])
    sim.generation.bonus_items = list(state.get("bonus_items", []))
    if "item_counter" in state:
        sim.native.uc.mem_write(sim.native.base + 0x13C6C38, state["item_counter"])
    sim.native.uc.mem_write(sim.unlocks.state + 0x580, struct.pack("<q", sim.day))
    sim.unlocks.properties["current_day"] = sim.day
    sim.unlocks.properties["house_food"] = sim.food
    sim.native.uc.mem_write(sim.native.tls + 0x178, state["rng"])
