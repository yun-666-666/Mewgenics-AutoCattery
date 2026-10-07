"""Local save input for the breeding web app (no snapshot-export executable).

The field layouts are the format-19/house-state-0 contracts already used by
src/snapshot. Opening a save is read-only; simulations use a SQLite copy.
"""

import math
from contextlib import closing
import os
from pathlib import Path
import sqlite3
import struct

from breeding_native_cat import decode_cat_blob
from breeding_names import NativeNames, save_language
from breeding_resources import read_resources


ROOMS = {"Default": "Floor1_Large", "SmallHouse_Attic": "Attic",
         "MediumHouse_SmallRoom": "Floor1_Small", "LargeHouse_Floor2Large": "Floor2_Large",
         "LargeHouse_Floor2Small": "Floor2_Small"}
ATTRIBUTES = ("Comfort", "Stimulation", "Health", "Evolution", "Appeal")


class Reader:
    def __init__(self, data, offset=0):
        self.data, self.offset = data, offset

    def read(self, fmt):
        value = struct.unpack_from("<" + fmt, self.data, self.offset)
        self.offset += struct.calcsize("<" + fmt)
        return value[0] if len(value) == 1 else value

    def string(self):
        size = self.read("Q")
        end = self.offset + size
        if end > len(self.data):
            raise ValueError("存档字符串不完整")
        value = self.data[self.offset:end].decode("utf-8")
        self.offset = end
        return value


def parse_house(data):
    r = Reader(data)
    version, count = r.read("II")
    if version != 0 or count > 10000:
        raise ValueError("不支持的 house_state 格式")
    entries = []
    for _ in range(count):
        key, room, position = r.read("q"), r.string(), r.read("3d")
        if key <= 0 or not all(map(math.isfinite, position)):
            raise ValueError("无效的猫位置")
        entries.append({"id": key, "room_id": room, "position": position})
    if r.offset != len(data):
        raise ValueError("house_state 包含未识别字段")
    return entries


def encode_house(entries):
    output = bytearray(struct.pack("<II", 0, len(entries)))
    for entry in entries:
        room = entry["room_id"].encode("utf-8")
        output += struct.pack("<qQ", entry["id"], len(room)) + room
        output += struct.pack("<3d", *entry["position"])
    parse_house(output)
    return bytes(output)


def read_table(data, offset, fmt):
    r = Reader(data, offset)
    first = r.read("Q")
    size = r.read("Q") if first >= 0xfffffffffffffff5 else first
    capacity = r.read("Q")
    if not 0 <= size <= capacity <= 1000000:
        raise ValueError("无效的谱系表")
    controls = data[r.offset:r.offset + capacity + 17]
    start = r.offset + capacity + 17
    width = struct.calcsize("<" + fmt)
    end = start + capacity * width + 8
    if end > len(data):
        raise ValueError("谱系表不完整")
    rows = [struct.unpack_from("<" + fmt, data, start + i * width)
            for i in range(capacity) if controls[i] <= 0x7f]
    if len(rows) != size:
        raise ValueError("谱系表计数不一致")
    return rows, end


def parse_pedigree(data):
    entries, cursor = read_table(data, 0, "qqqd")
    pairs, cursor = read_table(data, cursor, "qqd")
    _, cursor = read_table(data, cursor, "q")
    if cursor != len(data):
        raise ValueError("谱系尾部格式未知")
    return [[key, a if a > 0 else None, b if b > 0 else None, coi]
            for key, a, b, coi in entries], pairs


def encode_id_table(rows, fmt):
    """Scalar-ID table layout and probing used by native 183E70/776350.

This is the game's container indexing operation, not a file integrity digest.
Tables are rebuilt without tombstones, using the persisted version-11 layout.
"""
    capacity = 31
    while len(rows) > (capacity + 1) * 7 // 8:
        capacity = (capacity + 1) * 2 - 1
    controls = bytearray([0x80] * (capacity + 17))
    controls[capacity] = 0xff
    width = struct.calcsize("<" + fmt)
    entries = bytearray(capacity * width)
    for row in rows:
        product = (row[0] & ((1 << 64) - 1)) * 0xde5fb9d2630458e9
        mixed = ((product >> 64) + product) & ((1 << 64) - 1)
        offset, step = (mixed >> 7) & capacity, 0
        while True:
            free = next((i for i in range(16) if controls[offset + i] == 0x80), None)
            if free is not None:
                index = (offset + free) & capacity
                break
            step += 16
            offset = (offset + step) & capacity
        entries[index * width:(index + 1) * width] = struct.pack("<" + fmt, *row)
        controls[index] = mixed & 0x7f
        controls[((index - 16) & capacity) + (capacity & 15) + 1] = mixed & 0x7f
    return (struct.pack("<QQQ", 0xfffffffffffffff5, len(rows), capacity) + controls + entries +
            struct.pack("<Q", (capacity + 1) * 7 // 8 - len(rows)))


def encode_pedigree(original, entries, new_ids):
    _, end = read_table(original, 0, "qqqd")
    _, pair_end = read_table(original, end, "qqd")
    accessible, _ = read_table(original, pair_end, "q")
    ids = sorted({row[0] for row in accessible} | set(new_ids))
    rows = [(key, a or -1, b or -1, coi) for key, (a, b, coi) in sorted(entries.items())]
    result = encode_id_table(rows, "qqqd") + original[end:pair_end] + encode_id_table([(key,) for key in ids], "q")
    parse_pedigree(result)
    return result


def encode_cat(raw):
    """A valid literal-only LZ4 block, with the game's uncompressed-size prefix."""
    size = len(raw)
    block = bytearray([min(size, 15) << 4])
    remaining = size - 15
    if remaining >= 0:
        while remaining >= 255:
            block.append(255)
            remaining -= 255
        block.append(remaining)
    result = struct.pack("<I", size) + block + raw
    if decode_cat_blob(result) != raw:
        raise ValueError("猫序列化回读失败")
    return bytes(result)


def copy_database(source, destination):
    with closing(sqlite3.connect(Path(source).resolve().as_uri() + "?mode=ro", uri=True)) as src:
        with closing(sqlite3.connect(destination)) as target, target:
            src.backup(target)


def export_population(simulation, destination, naming_source=None):
    """Merge trained new living cats into a copy; preserve campaign and originals.

Export returns to the campaign's original day, retaining each imported cat's
simulated age. Ancestors stay in pedigree even if they died in the simulation.
"""
    path = Path(destination)
    if path.exists() or path.resolve() == simulation.save.resolve():
        raise ValueError("导出必须使用新的文件，不能覆盖原存档")
    with sqlite3.connect(simulation.save.resolve().as_uri() + "?mode=ro", uri=True) as db:
        old_ids = {row[0] for row in db.execute("SELECT key FROM cats")}
        pedigree = db.execute("SELECT data FROM files WHERE key='pedigree'").fetchone()[0]
    imported = [(key, cat) for key, cat in simulation.cats.items()
                if key not in old_ids and not simulation._dead(cat) and simulation._all_seven(cat)]
    if not imported:
        raise ValueError("当前结果还没有可导出的全七后代")
    original_day = simulation.snapshot["day"]
    elapsed = simulation.day - original_day
    house = list(simulation.snapshot["house_entries"])
    positions = {entry["room_id"]: entry["position"] for entry in house}
    default_position = next(iter(positions.values()), None)
    if default_position is None:
        raise ValueError("没有可复用的已确认房屋位置")
    records = []
    for key, cat in imported:
        native = simulation.native
        birth = simulation._read(cat, 0xC38, "<q")
        rebased = birth - elapsed
        if rebased < 0:
            raise ValueError("当前战役天数不足以保留培养猫年龄，请选择更晚的存档")
        native.uc.mem_write(cat + 0xC38, struct.pack("<q", rebased))
        try:
            raw = native.serialize(cat)
        finally:
            native.uc.mem_write(cat + 0xC38, struct.pack("<q", birth))
        records.append((key, encode_cat(raw)))
        room = simulation.cat_rooms[key]
        house.append({"id": key, "room_id": room, "position": positions.get(room, default_position)})
    removed_ids = []
    limit = simulation.config.population_limit
    if len(house) > limit:
        # Evaluate the actual merged export, not just the simulated survivors:
        # originals retain their original data, while trained cats retain age.
        with sqlite3.connect(simulation.save.resolve().as_uri() + "?mode=ro", uri=True) as db:
            candidates = dict(db.execute("SELECT key,data FROM cats"))
        candidates.update(records)
        resident_ids = {entry["id"] for entry in house}
        quality = {}
        combat = {}
        native = simulation.native
        for key in sorted(resident_ids):
            mark = native.heap_next
            try:
                native.uc.mem_write(simulation.unlocks.state + 0x580, struct.pack("<q", original_day))
                cat = native.load_cat(decode_cat_blob(candidates[key]), simulation.generation.visual.frames)
                stats = simulation._stats(cat)
                alive = not simulation._dead(cat)
                combat[key] = (alive, sum(native.effective_stats(cat)), -key)
                quality[key] = (alive, all(x == 7 for x in stats), sum(stats),
                                sum(x == 7 for x in stats), combat[key][1], -key)
            finally:
                native.uc.mem_write(simulation.unlocks.state + 0x580, struct.pack("<q", simulation.day))
                native.rewind(mark)
        keep = set()
        for first, second in simulation.select_pairs(min(limit // 2, max(2, simulation.config.breeding_pairs))):
            if first not in resident_ids or second not in resident_ids or first in keep or second in keep:
                continue
            if len(keep) + 2 > min(limit, 2 * max(2, simulation.config.breeding_pairs)):
                break
            keep.update((first, second))
        for key in sorted(resident_ids, key=combat.get, reverse=True)[:min(8, limit)]:
            if len(keep) < limit:
                keep.add(key)
        for key in sorted(resident_ids, key=quality.get, reverse=True):
            if len(keep) >= limit:
                break
            keep.add(key)
        removed_ids = sorted(resident_ids - keep)
        house = [entry for entry in house if entry["id"] in keep]
        records = [(key, blob) for key, blob in records if key in keep]
        imported = [(key, cat) for key, cat in imported if key in keep]
    names = NativeNames(simulation.exe, simulation.gpak, simulation.save,
                        save_language(naming_source or simulation.save))
    named_records = []
    for key, blob in records:
        raw = decode_cat_blob(blob)
        if struct.unpack_from("<Q", raw, 12)[0] == 0:
            name = names.generate(cat_fields(key, blob)["native_sex"]).encode("utf-16-le")
            raw = raw[:12] + struct.pack("<Q", len(name) // 2) + name + raw[20:]
        named_records.append((key, encode_cat(raw)))
    records = named_records
    encoded_pedigree = encode_pedigree(pedigree, simulation.pedigree.entries,
                                      [key for key, _ in imported])
    copy_database(simulation.save, path)
    with sqlite3.connect(path) as db:
        db.executemany("DELETE FROM cats WHERE key=?", [(key,) for key in removed_ids])
        db.executemany("INSERT INTO cats(key,data) VALUES(?,?)", records)
        db.execute("UPDATE files SET data=? WHERE key='house_state'", (encode_house(house),))
        db.execute("UPDATE files SET data=? WHERE key='pedigree'", (encoded_pedigree,))
        db.execute("UPDATE files SET data=? WHERE key='name_gen_history_w'", (names.history(),))
        # D8C26 and 3A6D57 load catid_counter alongside existing registered IDs.
        db.execute("INSERT INTO properties(key,data) VALUES('catid_counter',?) "
                   "ON CONFLICT(key) DO UPDATE SET data=max(data,excluded.data)",
                   (simulation.next_id - 1,))
    return {"file": str(path), "imported_ids": [key for key, _ in imported],
            "campaign_day": original_day, "simulated_days": elapsed,
            "original_cats_preserved": not any(key in old_ids for key in removed_ids),
            "population_limit": limit, "removed_population_ids": removed_ids,
            "offspring_all_seven_assist": simulation.config.offspring_all_seven_assist}


def cat_fields(key, stored):
    data = decode_cat_blob(stored)
    count = struct.unpack_from("<Q", data, 12)[0]
    end = 20 + count * 2
    name = data[20:end].decode("utf-16-le")
    r = Reader(data, end)
    r.string()
    sex, presentation, flags = r.read("iiQ")
    breed = r.string()
    personality = r.offset
    libido, sexuality = struct.unpack_from("<dd", data, personality + 4)
    r.offset += 368
    r.string()  # voice
    stats_offset = r.offset + 8
    stats = struct.unpack_from("<7i", data, stats_offset)
    r.offset += 92
    r.string()  # stat type
    r.offset += 14
    abilities = [r.string() for _ in range(10)]
    for index in range(4):
        abilities[6 + index] = r.string()
        r.read("I")  # native passive/disorder level, not an inheritable upgrade
    definitions = (
        ("fur", "texture", 0), ("body", "body", 3), ("head", "head", 8),
        ("tail", "tail", 13), ("leg_L", "legs", 18), ("leg_R", "legs", 23),
        ("arm_L", "legs", 28), ("arm_R", "legs", 33), ("eye_L", "eyes", 38),
        ("eye_R", "eyes", 43), ("eyebrow_L", "eyebrows", 48),
        ("eyebrow_R", "eyebrows", 53), ("ear_L", "ears", 58),
        ("ear_R", "ears", 63), ("mouth", "mouth", 68))
    visual_parts = [{"slot": slot, "category": category,
                     "id": struct.unpack_from("<I", data, personality + 68 + index * 4)[0]}
                    for slot, category, index in definitions]
    return {"id": key, "name": name, "native_sex": sex,
            "sex_presentation": presentation, "no_breed": bool(flags & 0x200000),
            "breed": breed, "libido": libido, "sexuality": sexuality,
            "genetic": list(stats), "stats_offset": stats_offset,
            "ability_slots": abilities, "visual_parts": visual_parts}


def discover_saves():
    root = Path(os.environ.get("APPDATA", "")) / "Glaiel Games" / "Mewgenics"
    return sorted(root.glob("*/saves/steamcampaign0[123].sav"),
                  key=lambda path: path.stat().st_mtime_ns, reverse=True)


def read_snapshot(save, gpak):
    with closing(sqlite3.connect(Path(save).resolve().as_uri() + "?mode=ro", uri=True)) as db:
        db.execute("BEGIN")
        files = dict(db.execute("SELECT key,data FROM files WHERE key IN ('house_state','house_unlocks','pedigree')"))
        properties = dict(db.execute("SELECT key,data FROM properties"))
        blobs = dict(db.execute("SELECT key,data FROM cats"))
        furniture = list(db.execute("SELECT key,data FROM furniture"))
    house = parse_house(files["house_state"])
    pedigree, pairs = parse_pedigree(files["pedigree"])
    r = Reader(files["house_unlocks"])
    if r.read("I") != 1:
        raise ValueError("不支持的房屋解锁格式")
    r.string()
    upgrades = [r.string() for _ in range(r.read("Q"))]
    rooms = {ROOMS[u]: {} for u in upgrades if u in ROOMS}
    placed = []
    for identity, data in furniture:
        r = Reader(data, 4)
        item = r.string()
        r.offset += 8
        room_size = r.read("Q")
        room = data[r.offset:r.offset + room_size].decode("utf-8")
        if room not in ROOMS.values():
            continue
        rooms.setdefault(room, {})
        placed.append({"identity": identity, "item_id": item, "room_id": room})
    catalog = read_resources(gpak, ("data/furniture_effects.gon",))["data/furniture_effects.gon"]
    for item in placed:
        if item["item_id"] not in catalog:
            raise ValueError("游戏资源中找不到家具效果：" + item["item_id"])
        definition = catalog[item["item_id"]]
        for name, value in getattr(definition, "fields", definition.items()):
            if type(value) in (int, float):
                effects = rooms[item["room_id"]]
                effects[name] = effects.get(name, 0) + value
    cats = []
    for entry in house:
        if entry["id"] not in blobs:
            raise ValueError("房屋中的猫缺少存档数据")
        cat = cat_fields(entry["id"], blobs[entry["id"]])
        cat.update(entry)
        cats.append(cat)
    return {"day": int(properties["current_day"]), "food": int(properties.get("house_food", 0)),
            "cats": cats, "rooms": [{"id": key, "attributes": {a: value.get(a, 0) for a in ATTRIBUTES},
                                      "effects": value} for key, value in rooms.items()],
            "placed_furniture": placed, "pedigree": pedigree, "pair_coi": pairs,
            "house_entries": house}
