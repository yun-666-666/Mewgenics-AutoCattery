"""Native-derived population helpers; the complete daily simulation is pending.

These functions model isolated game stages, not a complete generated stray or
an end-of-day transition. In particular, special cats and body effects remain
part of the caller's generation pipeline.
"""

import math

from breeding_lab import MASK64


def chance(probability, rng):
    return probability >= 1.0 or (probability > 0.0 and probability > rng.random())


def biased_random(bias, rng):
    """RVA 0x94F4C0; retain draws even for an integral or zero bias."""
    first = rng.random()
    magnitude = abs(bias)
    integer = math.floor(magnitude)
    extra = chance(magnitude - integer, rng)
    result = max(rng.random() ** (1.0 / (integer + 1.0)), first * extra)
    return 1.0 - result if bias < 0.0 else result


def mutate_stats(stats, increments, decrements, lower, upper, retry, rng):
    """RVA 0xB55B0: retry the entire allocation or clamp individual values."""
    for _ in range(1000):
        candidate = list(stats)
        for _ in range(max(0, increments)):
            candidate[int(rng.random() * 7)] += 1
        for _ in range(max(0, decrements)):
            candidate[int(rng.random() * 7)] -= 1
        if not retry:
            return [min(upper, max(lower, value)) for value in candidate]
        if all(lower <= value <= upper for value in candidate):
            return candidate
    return list(stats)


def initial_genetic_stats(rng):
    stats = mutate_stats([5] * 7, 7, 7, 4, 6, True, rng)
    return mutate_stats(stats, 1, 1, 3, 7, True, rng)


def initial_personality(rng):
    """RVA 0xB7110: libido, sexuality, aggression, and fertility."""
    libido = biased_random(3.0, rng) * 0.5
    raw = (rng.state[0] + rng.state[3]) & MASK64
    rng.random()
    if 0 < raw < (1 << 63):
        libido = 1.0 - libido
    gay = chance(0.1, rng)
    mixed = chance(0.1, rng)
    sexuality = rng.random()
    if gay:
        sexuality = 1.0 - sexuality * 0.1
    elif not mixed:
        sexuality *= 0.1
    aggression = rng.random()
    fertility = 1.0 + biased_random(-1.0, rng) * 0.25
    return libido, sexuality, aggression, fertility


def stray_appeal_increment(appeal, rng):
    """RVA 0xAAA6E: full-house effective Appeal, before stat allocation.

    Other draws intervene before the allocation in the full native generator.
    Do not use this helper to claim full-generator RNG-state equivalence.
    """
    low = 0.05 * appeal
    high = 1.0 + 0.1 * appeal
    return math.floor(low + rng.random() * (high - low))


def attraction(libido, sexuality, presentation, target_presentation,
               target_charisma, lover_id, target_id, love):
    """RVA 0xD28AB, for a distinct, breedable adult target.

    target_charisma must be the effective native stat, including body/passive
    effects. Genetic charisma alone is not a substitute for that input.
    """
    angle = sexuality * (math.pi / 2)
    same = math.sin(angle) * libido
    different = math.cos(angle) * libido
    if presentation == 2 or target_presentation == 2:
        desire = math.sqrt(same * same + different * different)
    else:
        desire = same if presentation == target_presentation else different
    relationship = 1.0 if lover_id == -1 else (
        1.0 + love if lover_id == target_id else 1.0 - love)
    return target_charisma * desire * 0.15 * relationship


def room_mating_factor(population, comfort):
    """RVA 0x2EA8C0; negative native square-root input produces NaN."""
    factor = 1.0 - 0.1 * max(population - 4, 0) + 0.1 * comfort
    return math.sqrt(factor) if factor >= 0.0 else math.nan


def aging_parameters(health):
    """House EndDay with a room: RVAs 0x1EDBE3 through 0x1EDC69."""
    raw_age = 18.0 + 0.5 * health
    age = 20.0 + 15.0 * math.tanh(raw_age * (1.0 / 15.0) - 4.0 / 3.0)
    low, high = -0.05, 0.05
    raw_risk = -0.005 * health
    risk = (math.tanh((2.0 * raw_risk - (low + high)) / (high - low))
            * (high - low) + high + low) * 0.5
    if risk > 0.0:
        risk *= 2.0
    return age, 0.11 + risk, 0.165 + risk


def litter_size(sex, target_sex, fertility, target_fertility, rng):
    """RVA 0x1EA05F, ordinary days; retain draws for infertile sex pairings."""
    product = fertility * target_fertility
    count = int(chance(product, rng))
    if product > 1.0:
        count += chance(product - 1.0, rng)
    return 0 if sex == target_sex and sex != 2 else count


def select_lineage_pairs(ranked, count, coi, avoid_inbreeding=True, max_coi=0.0):
    """Prefer two unrelated families before filling remaining disjoint pairs.

    A pair's own COI is insufficient: its children must also be unrelated to
    the other selected family's children. All four cross-parent links matter.
    The caller supplies eligible pairs in descending genetic-quality order.
    """
    if count <= 0:
        return []
    selected = []
    if avoid_inbreeding and count >= 2:
        for index, (_, a, b) in enumerate(ranked):
            for _, c, d in ranked[index + 1:]:
                if len({a, b, c, d}) != 4:
                    continue
                if all(coi(x, y) <= max_coi for x in (a, b) for y in (c, d)):
                    selected = [(a, b), (c, d)]
                    break
            if selected:
                break
    used = {cat for pair in selected for cat in pair}
    for _, a, b in ranked:
        if len(selected) >= count:
            break
        if a not in used and b not in used:
            selected.append((a, b))
            used.update((a, b))
    return selected


class Pedigree:
    """Native kinship recurrence at RVA 0x772C00, including founder behavior.

    IDs increase when cats are created. Preserve the entire exported pedigree,
    including dead/removed ancestors; dropping them would erase relatedness.
    """

    def __init__(self, entries):
        self.entries = {cat: (mother, father, coefficient)
                        for cat, mother, father, coefficient in entries}
        self.cache = {}

    def coi(self, first, second):
        if first is None or second is None or first <= 0 or second <= 0:
            return 0.0
        a, b = sorted((first, second))
        key = (a, b)
        if key in self.cache:
            return self.cache[key]
        if a == b:
            result = 0.5 * (1.0 + self.entries.get(a, (None, None, 0.0))[2])
        elif a not in self.entries or b not in self.entries:
            result = 0.0
        else:
            mother, father, _ = self.entries[b]
            result = 0.5 * (self.coi(a, mother) + self.coi(a, father))
        self.cache[key] = result
        return result

    def add(self, cat, mother=None, father=None):
        if cat in self.entries or any(parent is not None and parent >= cat
                                      for parent in (mother, father)):
            raise ValueError("New pedigree IDs must be distinct and later than parents")
        self.entries[cat] = (mother, father, self.coi(mother, father))
