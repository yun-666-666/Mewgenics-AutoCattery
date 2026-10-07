"""Physiological new-arrival phase from the inspected house end-day code.

The daily caller supplies actual eligible-adult counts and the global/NPC
mercy condition; this module does not invent progression or advance time.
"""

from dataclasses import dataclass, field
import struct


@dataclass
class ArrivalInputs:
    min_strays_tomorrow: int
    special_stray_counter: int
    same_sex_ids: list = field(default_factory=list)


def arrival_count(eligible_adults, mercy_eligible, minimum):
    # 1EEA71..1EEB23: the extra mercy cats lose furniture effects after
    # the first arrival; count is still bounded below by the save property.
    mercy = eligible_adults + 1 < 4 and mercy_eligible
    return max(4 - eligible_adults if mercy else 1, minimum), mercy


def arrival_effects(house_effects, day, inputs):
    effects = dict(house_effects)
    if inputs.same_sex_ids:
        effects["GayStrayChance"] = effects.get("GayStrayChance", 0) + len(inputs.same_sex_ids)
    if inputs.special_stray_counter >= 0:
        effects["SpecialStrayChance"] = (effects.get("SpecialStrayChance", 0)
            + (inputs.special_stray_counter + 1) * 0.01)
    if day > 10:
        effects["SpecialStrayChance"] = (effects.get("SpecialStrayChance", 0)
            + min((day - 10) * (1.0 / 1460), 1.0) * 0.001)
    return effects


class NativeArrivals:
    def __init__(self, native, resources, generation, pedigree, inputs):
        self.native = native
        self.resources = resources
        self.generation = generation
        self.pedigree = pedigree
        self.inputs = inputs

    def run(self, *, day, next_id, cats, eligible_adults, mercy_eligible, house_effects):
        count, mercy = arrival_count(eligible_adults, mercy_eligible, self.inputs.min_strays_tomorrow)
        effects = self.resources.effect_vector(arrival_effects(house_effects, day, self.inputs))
        empty = self.resources.effect_vector({})
        result = []
        with self.generation.social_sources(self.inputs.same_sex_ids, cats):
            for index in range(count):
                cat = self.generation.initialize()
                self.generation.apply_stray_effects(cat, empty if mercy and index else effects)
                key = next_id + index
                self.native.uc.mem_write(cat + 0xC48, struct.pack("<q", key))
                self.pedigree.add(key)
                result.append((key, cat))
        self.inputs.min_strays_tomorrow = 1
        self.inputs.special_stray_counter = -1
        self.inputs.same_sex_ids.clear()
        return result
