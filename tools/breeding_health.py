"""Native mutation/disorder helpers and the house health/aging phases.

These phases are part of the offline daily model, not a complete day on their
own. Cats here must be at home; feeding and fights run before room health.
"""

import struct

from breeding_night import NativeRandom
from breeding_population import chance
from unicorn.x86_const import UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2


class NativeHealth:
    def __init__(self, native, resources, generation):
        self.native = native
        self.resources = resources
        self.generation = generation
        self.rng = NativeRandom(native)

    def _read(self, cat, offset, kind):
        return struct.unpack(kind, self.native.uc.mem_read(cat + offset, struct.calcsize(kind)))[0]

    def _float_call(self, address, *values):
        for register, value in zip((UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2), values):
            self.native.uc.reg_write(register, struct.unpack("<Q", struct.pack("<d", value))[0])
        self.native.call(address)
        return struct.unpack("<d", struct.pack("<Q", self.native.uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)))[0]

    def mutate(self, cat, unmutate):
        native = self.native
        key = native.allocate(32)
        self.resources.string(key, "none" if unmutate else "common")
        source = native.allocate(8)
        native.uc.mem_write(source, struct.pack("<Q", cat))
        callback = native.allocate(0x40)
        native.call(0x1F26B0, callback, source)
        return bool(native.call(0xCC3F0, cat, 1, key, callback) & 0xFF)

    def clear_disorder(self, cat, slot):
        native = self.native
        empty = native.allocate(0x28)
        native.call(0xD30D0, empty)
        native.call(0xD3100, cat + 0x910 + 0x28 * slot, empty)

    def gain_disorder(self, cat):
        native = self.native
        pool = self.generation.pools["passives", "low_hygeine_in_house_disorders"]

        def pick():
            node = native.call(0x94FE40, pool, native.tls + 0x178)
            name = native.allocate(32)
            native.call(0x940D30, node, name)
            return name

        name = pick()
        existing = {native.string(cat + 0x910 + 0x28 * slot) for slot in range(4)}
        for _ in range(25):
            if native.string(name) not in existing:
                break
            name = pick()
        native.uc.mem_write(native.stack + 0x20, bytes(8))
        return bool(native.call(0xC35C0, cat, name, 0, 0) & 0xFF)

    def room_health(self, cats, effects):
        """1EC7D0..1EDA2E, with presentation and text construction omitted."""
        value = lambda category, initial: self.resources.effect_value(effects, category, initial)
        cure_injury = value(0x1E, 0.0)
        cure_disorder = value(0x1F, -0.1)
        gain_disorder = value(0x20, 0.0)
        mutation = value(0x23, 0.0)
        unmutate = value(0x24, -0.25)
        events = []
        if mutation > 0:
            for cat in cats:
                if not self._read(cat, 0x7AC, "<B") and chance(mutation, self.rng):
                    if self.mutate(cat, chance(unmutate, self.rng)):
                        events.append((self._read(cat, 0xC48, "<q"), "mutation"))
        if cure_injury <= 0 and cure_disorder <= 0 and gain_disorder == 0:
            return events
        for cat in cats:
            if self._read(cat, 0x7AC, "<B"):
                continue
            key = self._read(cat, 0xC48, "<q")
            if self.native.call(0xCFF10, cat) and chance(cure_injury, self.rng):
                self.native.call(0xCFF90, cat)
                events.append((key, "cure_injury"))
                continue
            occupied = [slot for slot in (2, 3)
                        if self.native.string(cat + 0x910 + 0x28 * slot) != "None"]
            if occupied and chance(cure_disorder, self.rng):
                slot = occupied[int(self.rng.random() * len(occupied))]
                self.clear_disorder(cat, slot)
                events.append((key, "cure_disorder"))
            elif chance(gain_disorder, self.rng) and self.gain_disorder(cat):
                events.append((key, "gain_disorder"))
        return events

    def age(self, cats, effects):
        """1EDBA0..1EDF12. None denotes outside, not a zero-effect room."""
        age_threshold, risk = 18.0, 0.0
        if effects is not None:
            raw_risk = self.resources.effect_value(effects, 0x22, 0.0)
            raw_age = self.resources.effect_value(effects, 0x21, 18.0)
            risk = self._float_call(0x1F26E0, raw_risk, -0.05, 0.05)
            if risk > 0:
                risk *= 2
            age_threshold = self._float_call(0x1F2750, raw_age)
        events = []
        for cat in cats:
            if self._read(cat, 0x7AC, "<B") or self.native.call(0xD3130, cat) & 0xFF:
                continue
            stage = self._read(cat, 0xC34, "<i")
            key = self._read(cat, 0xC48, "<q")
            if stage == 0:
                age = self.native.call(0xD31F0, cat) & 0xFFFFFFFF
                if age >= age_threshold:
                    self.native.uc.mem_write(cat + 0xC34, struct.pack("<i", 1))
                    events.append((key, "aging"))
            elif stage == 1 and chance(0.11 + risk, self.rng):
                self.native.uc.mem_write(cat + 0xC34, struct.pack("<i", 2))
                events.append((key, "senior"))
            elif stage == 2 and chance(0.165 + risk, self.rng):
                self.native.call(0xD3420, cat)
                flags = self._read(cat, 0xBF8, "<Q") | 0x400
                self.native.uc.mem_write(cat + 0xBF8, struct.pack("<Q", flags))
                events.append((key, "death_old_age"))
        return events
