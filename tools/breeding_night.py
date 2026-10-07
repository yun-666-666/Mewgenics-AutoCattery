"""Room mating/birth phase for the forthcoming full daily population loop.

This phase does not advance the day or replace feeding, fights, illness,
aging, arrivals, or room planning. It does not silently prevent related cats
from mating: the planner must keep forbidden pairings out of the same room.
"""

from dataclasses import dataclass, field
import struct

from breeding_lab import GameRandom
from breeding_mating_cache import mating_stat_cache
from breeding_population import chance, litter_size, room_mating_factor


class NativeRandom:
    """Share the physiological RNG state with interleaved native calls."""

    def __init__(self, native):
        self.native = native

    def random(self):
        address = self.native.tls + 0x178
        rng = GameRandom(struct.unpack("<4Q", self.native.uc.mem_read(address, 32)))
        result = rng.random()
        self.native.uc.mem_write(address, struct.pack("<4Q", *rng.state))
        return result

    def coin(self):
        return bool(self.native.call(0x94F3D0, self.native.tls + 0x178) & 0xFF)


def shuffle_room(order, rng):
    """RVA 86F00, descending Fisher-Yates over the whole room."""
    for index in range(len(order) - 1, 0, -1):
        target = int(rng.random() * (index + 1))
        order[index], order[target] = order[target], order[index]


def weighted_partner(order, weights, rng):
    """RVA 1F21A0, including the zero-total first-entry behavior."""
    if not order:
        return None
    roll = rng.random() * sum(weights)
    for target, weight in zip(order, weights):
        roll -= weight
        if roll <= 0:
            return target
    return order[int(rng.random() * len(order))]


@dataclass
class NightResult:
    pairs: list = field(default_factory=list)
    rejected: list = field(default_factory=list)
    newborns: list = field(default_factory=list)
    newborn_rooms: dict = field(default_factory=dict)
    same_sex_pairs: int = 0
    same_sex_ids: list = field(default_factory=list)


class NativeBreedingNight:
    def __init__(self, native, generation, pedigree, next_id):
        self.native = native
        self.generation = generation
        self.pedigree = pedigree
        self.next_id = next_id
        self.rng = NativeRandom(native)

    def _value(self, cat, offset, kind):
        size = struct.calcsize(kind)
        return struct.unpack(kind, self.native.uc.mem_read(cat + offset, size))[0]

    def run(self, rooms, all_cats, day):
        """rooms: (cat pointers, effective Comfort, suppression, effect vector).

        Call with the living, at-home cats remaining after the feeding phase.
        Population callers retain returned native cats until serializing them.
        """
        native = self.native
        result = NightResult()
        successful = []
        shuffled = []
        # The game shuffles every room before evaluating any room's pairs.
        for room_index, (cats, comfort, suppression, effects) in enumerate(rooms):
            order = list(cats)
            shuffle_room(order, self.rng)
            shuffled.append((room_index, order, comfort, suppression, effects))
        with mating_stat_cache(native, (cat for _, order, _, _, _ in shuffled for cat in order)):
            for room_index, order, comfort, suppression, effects in shuffled:
                if suppression > 0.99:
                    continue
                used = set()
                factor = room_mating_factor(len(order), comfort)
                for actor in order:
                    if actor in used:
                        continue
                    weights = [0.0 if target in used else native.mating_weight(actor, target)
                               for target in order]
                    partner = weighted_partner(order, weights, self.rng)
                    if partner is None or partner == actor or partner in used:
                        continue
                    wants = chance(native.mating_weight(actor, partner) * factor, self.rng)
                    if not wants and day != 0:
                        continue
                    reciprocates = chance(native.mating_weight(partner, actor) * factor, self.rng)
                    if reciprocates or day == 0:
                        successful.append((actor, partner, effects, room_index))
                        used.update((actor, partner))
                    elif not native.call(0xD3130, actor) and not native.call(0xD3130, partner):
                        result.rejected.append((actor, partner))
        # All rooms finish selecting before relationship changes and births.
        for first, second, effects, room_index in successful:
            ids = (self._value(first, 0xC48, "<q"), self._value(second, 0xC48, "<q"))
            result.pairs.append(ids)
            native.call(0xD2BF0, first, second)
            native.call(0xD2BF0, second, first)
            for other in all_cats:
                if other in (first, second):
                    continue
                lover = self._value(other, 0xBC8, "<q")
                if lover == ids[0]:
                    native.call(0xD2CF0, other, second)
                if lover == ids[1]:
                    native.call(0xD2CF0, other, first)
            if self.rng.coin():
                first, second = second, first
            if self._value(first, 0x58, "<i") == 1:
                first, second = second, first
            if self._value(second, 0x58, "<i") == 0:
                first, second = second, first
            # D8580 registers these ordered IDs before A89A0 is called.
            ids = (self._value(first, 0xC48, "<q"), self._value(second, 0xC48, "<q"))
            sex = self._value(first, 0x58, "<i")
            other_sex = self._value(second, 0x58, "<i")
            if sex == other_sex and sex != 2:
                result.same_sex_pairs += 1
                result.same_sex_ids.extend(ids)
            count = litter_size(sex, other_sex, self._value(first, 0xBF0, "<d"),
                                self._value(second, 0xBF0, "<d"), self.rng)
            if day == 0:
                count = 1
            for _ in range(count):
                child_id = self.next_id
                self.next_id += 1
                self.pedigree.add(child_id, *ids)
                coi = self.pedigree.entries[child_id][2]
                child = self.generation.breed(first, second, coi, effects=effects)
                native.uc.mem_write(child + 0xC48, struct.pack("<q", child_id))
                # D8580's registered newborn has today's birth date; B7320
                # alone initializes an already-adult stray at day-2.
                native.uc.mem_write(child + 0xC38, struct.pack("<q", day))
                result.newborns.append((child_id, child))
                result.newborn_rooms[child_id] = room_index
        return result
