"""Persistent native-backed house-day simulation for breeding experiments.

This module reads saves and local game resources but never writes either. It
connects the already validated physiological phases in their House::EndDay
order and serializes/reloads every cat after each simulated day, so mutations,
relationships, hunger, injuries, age, births, and arrivals survive into the
next planning pass.

Cosmetic name generation remains intentionally omitted by
``NativeCatGeneration``. The physiological random stream is therefore a
deterministic experiment stream, not a claim that a particular game save will
produce the same named cat from the same persisted seed.
"""

from dataclasses import dataclass, field
import json
from pathlib import Path
import sqlite3
import struct

from breeding_arrivals import ArrivalInputs, NativeArrivals
from breeding_feeding import NativeFeeding
from breeding_fights import NativeFights
from breeding_health import NativeHealth
from breeding_native_cat import NativeCatReference, read_cat_blobs, read_save_file
from breeding_native_generation import NativeCatGeneration
from breeding_native_resources import NativeResources
from breeding_night import NativeBreedingNight, NativeRandom, shuffle_room
from breeding_population import Pedigree, attraction, select_lineage_pairs
from breeding_resources import read_resources
from breeding_unlocks import BreedingUnlocks
from breeding_trait_profile import build_profile, pair_trait_score
from breeding_save import cat_fields


STAT_OFFSET = 0x6F0
DEAD_OFFSET = 0x7AC
FLAGS_OFFSET = 0xBF8
ID_OFFSET = 0xC48
NO_BREED = 0x200000


@dataclass
class SimulationConfig:
    breeding_pairs: int = 2
    avoid_inbreeding: bool = True
    max_coi: float = 0.0
    suppress_nonbreeding_rooms: bool = False
    nonbreeding_suppression: float = 1.0
    room_overrides: dict = field(default_factory=dict)
    house_effect_overrides: dict = field(default_factory=dict)
    breeding_room_overrides: dict = field(default_factory=dict)
    nonbreeding_room_overrides: dict = field(default_factory=dict)
    daily_food_refill: int | None = None
    remove_dead_after_day: bool = True
    enable_fights: bool = True
    enable_health: bool = True
    enable_arrivals: bool = True
    offspring_all_seven_assist: bool = False
    food_supply_assist: bool = False
    population_limit: int = 150
    trait_overrides: dict = field(default_factory=dict)


@dataclass
class DayResult:
    day: int
    selected_pairs: list = field(default_factory=list)
    selected_coverages: list = field(default_factory=list)
    births: list = field(default_factory=list)
    all_seven_births: int = 0
    arrivals: list = field(default_factory=list)
    feeding_deaths: list = field(default_factory=list)
    fight_deaths: list = field(default_factory=list)
    health_events: list = field(default_factory=list)
    removed_dead: list = field(default_factory=list)
    food_remaining: int = 0
    assisted_births: int = 0
    removed_population: list = field(default_factory=list)


def _saved_properties(save):
    with sqlite3.connect(Path(save).resolve().as_uri() + "?mode=ro", uri=True) as db:
        return dict(db.execute("SELECT key, data FROM properties"))


def _numeric_effects(definition):
    if definition is None:
        return {}
    result = {}
    fields = getattr(definition, "fields", definition.items())
    for name, value in fields:
        if type(value) in (int, float):
            result[name] = result.get(name, 0.0) + value
    return result


def _add_effects(target, source):
    for name, value in source.items():
        target[name] = target.get(name, 0.0) + value


def _is_edible(item_id):
    # The inspected saves use the game's food prop families below. The native
    # CatData food preference and 25% corpse removal still execute in Unicorn.
    return item_id.startswith(("small_food_", "object_food_"))


class NativeDailySimulation:
    def __init__(self, exe, gpak, save, snapshot_path, config=None):
        self.exe = Path(exe)
        self.gpak = Path(gpak)
        self.save = Path(save)
        self.snapshot = (snapshot_path if isinstance(snapshot_path, dict) else
                         json.loads(Path(snapshot_path).read_text(encoding="utf-8-sig")))
        self.config = config or SimulationConfig()
        self.trait_profile = build_profile(self.gpak)
        for key, overrides in self.config.trait_overrides.items():
            self.trait_profile[key].update(overrides)
        self.properties = _saved_properties(self.save)
        self.day = int(self.snapshot["day"])

        self.native = NativeCatReference(self.exe)
        self.resources = NativeResources(self.native)
        self.root = self.resources.load_cat_resources(self.gpak)
        self.unlocks = BreedingUnlocks(
            self.native, self.resources, self.save, self.gpak, self.day)
        npc = self.native.npc_breeding_inputs(read_save_file(self.save, "npc_progress"))
        self.generation = NativeCatGeneration(
            self.native, self.resources, self.root, self.unlocks, self.gpak,
            npc_voice_progress=npc["voice_progress"])
        self.pedigree = Pedigree(self.snapshot["pedigree"])
        self.arrival_inputs = ArrivalInputs(
            int(self.properties.get("min_strays_tomorrow", 1)),
            npc["special_stray_counter"], list(npc["same_sex_ids"]))
        self.next_id = max(self.pedigree.entries, default=0) + 1
        seed = self.properties.get("random_seed")
        if not isinstance(seed, bytes) or len(seed) != 32:
            raise ValueError("save random_seed is not the inspected 32-byte state")
        self.native.uc.mem_write(self.native.tls + 0x178, seed)

        catalog = read_resources(
            self.gpak, ("data/furniture_effects.gon",))["data/furniture_effects.gon"]
        self.furniture_catalog = catalog
        self.furniture = [
            {"identity": item.get("identity", index), "item_id": item["item_id"], "room_id": item["room_id"]}
            for index, item in enumerate(self.snapshot.get("placed_furniture", []))
        ]
        self.room_ids = [room["id"] for room in self.snapshot["rooms"]]
        if not self.room_ids:
            raise ValueError("snapshot has no unlocked house rooms")

        self.population_mark = self.native.heap_next
        snapshot_cats = {cat["id"]: cat for cat in self.snapshot["cats"]}
        self.cats = {}
        for key, raw in read_cat_blobs(self.save):
            if key not in snapshot_cats:
                continue
            cat = self.native.load_cat(raw, self.generation.visual.frames)
            self.native.uc.mem_write(cat + ID_OFFSET, struct.pack("<q", key))
            self.cats[key] = cat
        if self.cats.keys() != snapshot_cats.keys():
            missing = sorted(snapshot_cats.keys() - self.cats.keys())
            raise ValueError(f"snapshot cats are missing from save storage: {missing}")
        self.cat_rooms = {cat["id"]: cat.get("room_id") for cat in self.snapshot["cats"]}
        self.food = int(self.properties.get("house_food", 0))
        self.feeding = NativeFeeding(self.native, self.resources)
        self.fights = NativeFights(self.native, self.resources)
        self.health = NativeHealth(self.native, self.resources, self.generation)

    def close(self):
        self.generation.close()
        self.unlocks.close()

    def _read(self, cat, offset, kind):
        return struct.unpack(
            kind, self.native.uc.mem_read(cat + offset, struct.calcsize(kind)))[0]

    def _dead(self, cat):
        return bool(self._read(cat, DEAD_OFFSET, "<B"))

    def _adult(self, cat):
        return not self._dead(cat) and not (self.native.call(0xD3130, cat) & 0xFF)

    def _breedable(self, cat):
        return self._adult(cat) and not (self._read(cat, FLAGS_OFFSET, "<Q") & NO_BREED)

    def _stats(self, cat):
        return struct.unpack("<7i", self.native.uc.mem_read(cat + STAT_OFFSET, 28))

    def _all_seven(self, cat):
        return all(value == 7 for value in self._stats(cat))

    def _sex(self, cat):
        return self._read(cat, 0x58, "<i")

    def _compatible(self, first, second):
        return self._sex(first) != self._sex(second) or 2 in (self._sex(first), self._sex(second))

    def _room_effects(self):
        effects = {room: {} for room in self.room_ids}
        total = {}
        for item in self.furniture:
            definition = self.furniture_catalog.get(item["item_id"])
            values = _numeric_effects(definition)
            if item["room_id"] in effects:
                _add_effects(effects[item["room_id"]], values)
            _add_effects(total, values)
        for room, overrides in self.config.room_overrides.items():
            if room == "*":
                for values in effects.values():
                    values.update(overrides)
            elif room in effects:
                effects[room].update(overrides)
        total = {}
        for values in effects.values():
            _add_effects(total, values)
        return effects, total

    def _candidate_pairs(self):
        adults = [(key, cat) for key, cat in self.cats.items() if self._breedable(cat)]
        traits = {key: cat_fields(key, self.native.serialize(cat)) for key, cat in adults}
        effects, _ = self._room_effects()
        viable = [values for values in effects.values() if values.get("BreedSuppression", 0) <= .99]
        preferred = max(viable, key=lambda v: (v.get("Comfort", 0), v.get("Stimulation", 0),
                                             v.get("Health", 0)), default={})
        stimulation = self.config.breeding_room_overrides.get("Stimulation", preferred.get("Stimulation", 0))
        charisma = {key: self.native.effective_stats(cat)[5] for key, cat in adults}

        def weight(actor_id, actor, target_id, target):
            return attraction(
                self._read(actor, 0xBB8, "<d"), self._read(actor, 0xBC0, "<d"),
                self._read(actor, 0x5C, "<i"), self._read(target, 0x5C, "<i"),
                charisma[target_id], self._read(actor, 0xBC8, "<q"), target_id,
                self._read(actor, 0xBD0, "<d"))

        pairs = []
        for index, (first_id, first) in enumerate(adults):
            for second_id, second in adults[index + 1:]:
                if not self._compatible(first, second):
                    continue
                coi = self.pedigree.coi(first_id, second_id)
                if self.config.avoid_inbreeding and coi > self.config.max_coi:
                    continue
                first_weight = weight(first_id, first, second_id, second)
                second_weight = weight(second_id, second, first_id, first)
                if first_weight <= 0 or second_weight <= 0:
                    continue
                first_stats, second_stats = self._stats(first), self._stats(second)
                coverage = sum(max(a, b) == 7 for a, b in zip(first_stats, second_stats))
                shared = sum(a == b == 7 for a, b in zip(first_stats, second_stats))
                stable = self._all_seven(first) and self._all_seven(second) and coi == 0
                assisted = self.config.offspring_all_seven_assist
                trait_quality = pair_trait_score(traits[first_id], traits[second_id],
                                                 self.trait_profile, stimulation)
                score = (
                    int(stable) if not assisted else 1,
                    trait_quality if assisted or stable else 0,
                    coverage, shared, sum(max(a, b) for a, b in zip(first_stats, second_stats)),
                    min(first_weight, second_weight), -coi, -first_id, -second_id,
                )
                pairs.append((score, first_id, second_id))
        pairs.sort(reverse=True)
        return pairs

    def plan(self, room_effects):
        ranked_rooms = sorted(
            self.room_ids,
            key=lambda room: (room_effects[room].get("Comfort", 0), room),
            reverse=True)
        available = [room for room in ranked_rooms
                     if room_effects[room].get("BreedSuppression", 0) <= .99]
        limit = max(0, self.config.breeding_pairs) if available else 0
        selected = self.select_pairs(limit)

        assignments = {}
        breeding_rooms = available[:min(len(selected), max(1, len(ranked_rooms) - 1))]
        for index, pair in enumerate(selected):
            room = breeding_rooms[index % len(breeding_rooms)]
            for key in pair:
                assignments[key] = room
        other_rooms = [room for room in ranked_rooms if room not in breeding_rooms]
        if not other_rooms:
            other_rooms = ranked_rooms[-1:]
        for room in breeding_rooms:
            room_effects[room].update(self.config.breeding_room_overrides)
        for room in other_rooms:
            room_effects[room].update(self.config.nonbreeding_room_overrides)
        nursery = max(other_rooms, key=lambda room: room_effects[room].get("Health", 0))
        cursor = 0
        for key, cat in self.cats.items():
            if key in assignments:
                continue
            if self._dead(cat):
                assignments[key] = self.cat_rooms.get(key) or nursery
            elif not self._adult(cat):
                assignments[key] = nursery
            else:
                assignments[key] = other_rooms[cursor % len(other_rooms)]
                cursor += 1
        if self.config.suppress_nonbreeding_rooms:
            for room in other_rooms:
                room_effects[room]["BreedSuppression"] = self.config.nonbreeding_suppression
        self.cat_rooms = assignments
        return selected

    def select_pairs(self, count):
        return select_lineage_pairs(self._candidate_pairs(), count, self.pedigree.coi,
                                    self.config.avoid_inbreeding, self.config.max_coi)

    def _room_vectors(self, room_effects):
        return {room: self.resources.effect_vector(values)
                for room, values in room_effects.items()}

    def _room_cats(self):
        rooms = {room: [] for room in self.room_ids}
        for key, cat in self.cats.items():
            room = self.cat_rooms.get(key)
            if room in rooms:
                rooms[room].append(cat)
        return rooms

    def _remove_ids(self, ids):
        for key in ids:
            self.cats.pop(key, None)
            self.cat_rooms.pop(key, None)

    def trim_population(self):
        """Keep compatible breeding pairs, strong combat cats, then genetic quality.

        Removing a resident never removes its pedigree entry. Offspring keep
        their ancestry even when a parent is no longer in the house.
        """
        limit = self.config.population_limit
        if limit < 4:
            raise ValueError("猫群数量上限至少为4，才能保留两对种猫")
        living = {key: cat for key, cat in self.cats.items() if not self._dead(cat)}
        if len(living) <= limit:
            return []
        keep = set()
        for first, second in self.select_pairs(min(limit // 2, max(2, self.config.breeding_pairs))):
            keep.update((first, second))
        combat = {key: sum(self.native.effective_stats(cat)) for key, cat in living.items()}
        fighters = sorted(living, key=lambda key: (combat[key], -key), reverse=True)
        for key in fighters[:min(8, limit)]:
            if len(keep) < limit:
                keep.add(key)

        def quality(key):
            cat = living[key]
            stats = self._stats(cat)
            age = self.day - self._read(cat, 0xC38, "<q")
            fields = cat_fields(key, self.native.serialize(cat))
            traits = pair_trait_score(fields, fields, self.trait_profile)
            return (traits if self.config.offspring_all_seven_assist else 0,
                    self._all_seven(cat), sum(stats), sum(x == 7 for x in stats), traits,
                    self._breedable(cat), combat[key], -age, -key)

        for key in sorted(living, key=quality, reverse=True):
            if len(keep) >= limit:
                break
            keep.add(key)
        removed = sorted(set(living) - keep)
        self._remove_ids(removed)
        return removed

    def _compact_population(self):
        stored = {key: self.native.serialize(cat) for key, cat in self.cats.items()}
        rooms = dict(self.cat_rooms)
        self.native.rewind(self.population_mark)
        self.cats = {}
        for key, raw in stored.items():
            cat = self.native.load_cat(raw, self.generation.visual.frames)
            self.native.uc.mem_write(cat + ID_OFFSET, struct.pack("<q", key))
            self.cats[key] = cat
        self.cat_rooms = rooms

    def replacement_pairs(self):
        adults = [(key, cat) for key, cat in self.cats.items()
                  if self._breedable(cat) and self._all_seven(cat)]
        edges = []
        for index, (first_id, first) in enumerate(adults):
            for second_id, second in adults[index + 1:]:
                if not self._compatible(first, second):
                    continue
                if (self.config.avoid_inbreeding and
                        self.pedigree.coi(first_id, second_id) > self.config.max_coi):
                    continue
                if (self.native.mating_weight(first, second) <= 0 or
                        self.native.mating_weight(second, first) <= 0):
                    continue
                edge = {first_id, second_id}
                if any(not edge & earlier for earlier in edges):
                    return 2
                edges.append(edge)
        return int(bool(edges))

    def step(self):
        current_day = self.day
        removed_population = self.trim_population()
        room_effects, house_effects = self._room_effects()
        selected = self.plan(room_effects)
        selected_coverages = [
            sum(max(a, b) == 7 for a, b in
                zip(self._stats(self.cats[first]), self._stats(self.cats[second])))
            for first, second in selected]
        house_effects = {}
        for values in room_effects.values():
            _add_effects(house_effects, values)
        house_effects.update(self.config.house_effect_overrides)
        vectors = self._room_vectors(room_effects)
        rooms = self._room_cats()

        storage_limit = 100 + int(house_effects.get("FoodStorage", 0))
        if self.config.daily_food_refill is not None:
            self.food = min(storage_limit, self.food + self.config.daily_food_refill)
        feeding = self.feeding.run(
            list(self.cats.values()),
            {cat: self.cat_rooms[key] for key, cat in self.cats.items()},
            vectors, food=self.food, storage_limit=storage_limit,
            food_supply_assist=self.config.food_supply_assist,
            edible_furniture=[(item["identity"], item["room_id"])
                               for item in self.furniture if _is_edible(item["item_id"])])
        self.food = feeding.food_remaining
        consumed = set(feeding.consumed_furniture)
        if consumed:
            self.furniture = [item for item in self.furniture
                              if item["identity"] not in consumed]
        self._remove_ids(feeding.removed_corpses)

        rooms = self._room_cats()
        night_rooms = []
        ordered_rooms = []
        for room in self.room_ids:
            living = [cat for cat in rooms[room] if not self._dead(cat)]
            effects = room_effects[room]
            night_rooms.append((living, effects.get("Comfort", 0),
                                effects.get("BreedSuppression", 0), vectors[room]))
            ordered_rooms.append(room)
        night = NativeBreedingNight(
            self.native, self.generation, self.pedigree, self.next_id)
        night_result = night.run(night_rooms, list(self.cats.values()), current_day)
        self.next_id = night.next_id
        assisted_births = 0
        for key, cat in night_result.newborns:
            if self.config.offspring_all_seven_assist and not self._all_seven(cat):
                self.native.uc.mem_write(cat + STAT_OFFSET, struct.pack("<7i", *([7] * 7)))
                assisted_births += 1
            self.cats[key] = cat
            self.cat_rooms[key] = ordered_rooms[night_result.newborn_rooms[key]]

        fight_result = None
        if self.config.enable_fights:
            rooms = self._room_cats()
            fight_result = self.fights.run(
                [(rooms[room], len(rooms[room]), vectors[room]) for room in self.room_ids],
                list(self.cats.values()), day=current_day,
                departed_first_real_adventure=bool(
                    self.properties.get("departed_first_real_adventure", 0)))

        health_events = []
        if self.config.enable_health:
            # 1EC69E shuffles the house list; 1EC730 then shuffles each room
            # in the room-name map. All room health finishes before 1EDBA0 aging.
            order = list(self.cats)
            rng = NativeRandom(self.native)
            shuffle_room(order, rng)
            health_rooms = {room: [] for room in sorted(self.room_ids)}
            for key in order:
                room = self.cat_rooms[key]
                if room in health_rooms:
                    health_rooms[room].append(self.cats[key])
            for cats in health_rooms.values():
                shuffle_room(cats, rng)
            for room, cats in health_rooms.items():
                health_events.extend(self.health.room_health(cats, vectors[room]))
            for key, cat in self.cats.items():
                health_events.extend(self.health.age([cat], vectors.get(self.cat_rooms[key])))

        arrivals = []
        self.arrival_inputs.same_sex_ids.extend(night_result.same_sex_ids)
        if self.config.enable_arrivals:
            eligible = sum(self._adult(cat) for cat in self.cats.values())
            arrival_phase = NativeArrivals(
                self.native, self.resources, self.generation,
                self.pedigree, self.arrival_inputs)
            arrivals = arrival_phase.run(
                day=current_day, next_id=self.next_id, cats=self.cats,
                eligible_adults=eligible,
                mercy_eligible=not bool(self.properties.get("generated_mercy_strays_today", 0)),
                house_effects=house_effects)
            self.next_id += len(arrivals)
            arrival_room = max(
                self.room_ids, key=lambda room: room_effects[room].get("Appeal", 0))
            for key, cat in arrivals:
                self.cats[key] = cat
                self.cat_rooms[key] = arrival_room

        removed_dead = []
        if self.config.remove_dead_after_day:
            removed_dead = [key for key, cat in self.cats.items() if self._dead(cat)]
            self._remove_ids(removed_dead)

        birth_ids = [key for key, _ in night_result.newborns]
        all_seven = sum(self._all_seven(self.cats[key]) for key in birth_ids if key in self.cats)
        removed_population.extend(self.trim_population())
        self.day += 1
        self.native.uc.mem_write(self.unlocks.state + 0x580, struct.pack("<q", self.day))
        self.unlocks.properties["current_day"] = self.day
        self.unlocks.properties["house_food"] = self.food
        self._compact_population()
        return DayResult(
            day=current_day,
            selected_pairs=selected,
            selected_coverages=selected_coverages,
            births=birth_ids,
            all_seven_births=all_seven,
            arrivals=[key for key, _ in arrivals],
            feeding_deaths=list(feeding.deaths),
            fight_deaths=[] if fight_result is None else list(fight_result.deaths),
            health_events=health_events,
            removed_dead=removed_dead,
            food_remaining=self.food,
            assisted_births=assisted_births,
            removed_population=removed_population,
        )


def run_days(simulation, days):
    return [simulation.step() for _ in range(days)]
