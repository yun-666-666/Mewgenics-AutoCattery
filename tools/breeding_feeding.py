"""Offline house food consumption and starvation, using native cat effects."""

from dataclasses import dataclass, field
import math
import struct

from breeding_night import NativeRandom, shuffle_room
from breeding_population import chance


@dataclass
class FeedingResult:
    food_remaining: int
    hungry: list = field(default_factory=list)
    deaths: list = field(default_factory=list)
    consumed_furniture: list = field(default_factory=list)
    removed_corpses: list = field(default_factory=list)


class NativeFeeding:
    def __init__(self, native, resources):
        self.native = native
        self.resources = resources
        self.rng = NativeRandom(native)

    def _read(self, cat, offset, kind):
        return struct.unpack(kind, self.native.uc.mem_read(cat + offset, struct.calcsize(kind)))[0]

    def requirement(self, cat):
        """1E9045..1E90BC: ceil(product of native passive multipliers)."""
        native = self.native
        mark = native.heap_next
        try:
            key = native.allocate(32)
            self.resources.string(key, "HouseFoodRequirementMultiplier")
            vector = native.allocate(16)
            native.call(0xBE200, cat, vector, key)
            count = self._read(vector, 4, "<I")
            pointer = self._read(vector, 8, "<Q")
            requirement = 1.0
            for index in range(count):
                node = self._read(pointer, index * 8, "<Q")
                if self._read(node, 0xA8, "<i") == 2:
                    requirement *= self._read(node, 0x58, "<d")
            return math.ceil(requirement)
        finally:
            native.rewind(mark)

    def run(self, cats, cat_rooms, room_effects, *, food, storage_limit, edible_furniture,
            food_supply_assist=False):
        """Consume actual stock; no implicit adventure income or free refills.

        cats includes bodies currently in the house. cat_rooms maps native cat
        pointers to room keys; None denotes outside. edible_furniture contains
        (furniture identity, room key), filtered by the actual edible rule by
        the caller. Consumed identities must be removed before later phases.
        """
        native = self.native
        result = FeedingResult(min(food, storage_limit))
        order = list(cats)
        shuffle_room(order, self.rng)
        edible = list(edible_furniture)
        shuffle_room(edible, self.rng)
        corpses = [cat for cat in order if self._read(cat, 0x7AC, "<B")]
        shuffle_room(corpses, self.rng)
        automatic = {room: math.ceil(self.resources.effect_value(effects, 0x1D, 0.0))
                     for room, effects in room_effects.items()}
        if food_supply_assist:
            automatic = {room: max(amount, 1000000) for room, amount in automatic.items()}
        for cat in order:
            if self._read(cat, 0x7AC, "<B"):
                continue
            key = self._read(cat, 0xC48, "<q")
            room = cat_rooms[cat]
            need = self.requirement(cat)
            if need == 0 or (room is not None and
                            (result.food_remaining >= need or automatic[room] >= need)):
                for _ in range(max(0, need)):
                    if room is not None and automatic[room] > 0:
                        automatic[room] -= 1
                    else:
                        result.food_remaining -= 1
                flags = self._read(cat, 0xBF8, "<Q") & ~4
                native.uc.mem_write(cat + 0xBF8, struct.pack("<Q", flags))
                continue
            if native.call(0xBE1F0, cat, 4) & 0xFF:
                furniture = next((item for item in edible if item[1] == room), None)
                if furniture is not None:
                    edible.remove(furniture)
                    result.consumed_furniture.append(furniture[0])
                    continue
                corpse = next((body for body in corpses if cat_rooms[body] == room), None)
                if corpse is not None:
                    if chance(0.25, self.rng):
                        corpses.remove(corpse)
                        result.removed_corpses.append(self._read(corpse, 0xC48, "<q"))
                    continue
            native.call(0xD33C0, cat)
            if self._read(cat, 0x7AC, "<B"):
                result.deaths.append(key)
                corpses.append(cat)
            else:
                result.hungry.append(key)
        return result
