"""House fight selection and native injury/death resolution, offline only."""

from dataclasses import dataclass, field
import struct

from breeding_night import NativeRandom, shuffle_room, weighted_partner
from breeding_mating_cache import mating_stat_cache
from unicorn.x86_const import (
    UC_X86_REG_RBP, UC_X86_REG_RSP, UC_X86_REG_R13, UC_X86_REG_R14,
    UC_X86_REG_R15, UC_X86_REG_ESI, UC_X86_REG_RIP, UC_X86_REG_XMM0,
    UC_X86_REG_XMM2, UC_X86_REG_XMM9, UC_X86_REG_XMM12, UC_X86_REG_XMM13,
    UC_X86_REG_XMM14, UC_X86_REG_XMM15,
)


@dataclass
class FightResult:
    pairs: list = field(default_factory=list)
    injuries: list = field(default_factory=list)
    deaths: list = field(default_factory=list)
    rewards: list = field(default_factory=list)


class NativeFights:
    def __init__(self, native, resources):
        self.native = native
        self.resources = resources
        self.rng = NativeRandom(native)

    def _read(self, cat, offset, kind):
        return struct.unpack(kind, self.native.uc.mem_read(cat + offset, struct.calcsize(kind)))[0]

    def _weight(self, actor, target):
        self.native.call(0xD29E0, actor, target)
        return struct.unpack("<d", struct.pack("<Q", self.native.uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)))[0]

    def severity(self, first, second, damage):
        """Run the original four luck/STR/CON rolls through severity 0/1/2."""
        native = self.native
        mark = native.heap_next
        try:
            # All four severity rolls and C1C00 use the same native all-minus-one
            # stat context, passives=1 and final flag=0. No cat writes occur here.
            callers = (0x1EB4D5, 0x1EB58E, 0x1EB63A, 0x1EB6DA, 0xC1C3A)
            if not getattr(native, "fight_stat_cache_enabled", True):
                callers = ()
            with mating_stat_cache(native, (first, second), callers):
                frame = native.stack - 0x4000
                native.uc.reg_write(UC_X86_REG_RBP, frame)
                native.uc.reg_write(UC_X86_REG_RSP, frame - 0x100)
                native.uc.reg_write(UC_X86_REG_R13, first)
                native.uc.reg_write(UC_X86_REG_R15, second)
                native.uc.reg_write(UC_X86_REG_R14, native.data + 0x1000)
                for register, value in ((UC_X86_REG_XMM9, damage), (UC_X86_REG_XMM12, 3.0),
                                        (UC_X86_REG_XMM13, 5.0)):
                    native.uc.reg_write(register, struct.unpack("<Q", struct.pack("<d", value))[0])
                native.uc.reg_write(UC_X86_REG_XMM14,
                    int.from_bytes(native.uc.mem_read(native.base + 0x113FB98, 8), "little"))
                native.uc.reg_write(UC_X86_REG_XMM15, (1 << 128) - 1)
                stop = native.base + 0x1EB767
                native.uc.emu_start(native.base + 0x1EB457, stop, count=2000000)
                if native.uc.reg_read(UC_X86_REG_RIP) != stop:
                    raise RuntimeError("native fight score did not reach severity output")
                return (native.uc.reg_read(UC_X86_REG_R14), native.uc.reg_read(UC_X86_REG_ESI))
        finally:
            native.rewind(mark)

    def run(self, rooms, all_cats, *, day, departed_first_real_adventure):
        """rooms: (cat pointers, total room population, native effect vector)."""
        result = FightResult()
        if day == 0 or not departed_first_real_adventure:
            return result
        native = self.native
        shuffled = []
        for cats, population, effects in rooms:
            order = [cat for cat in cats if not self._read(cat, 0x7AC, "<B")
                     and not native.call(0xD3130, cat) & 0xFF]
            shuffle_room(order, self.rng)
            shuffled.append((order, population, effects))
        selected = []
        with mating_stat_cache(native, (cat for order, _, _ in shuffled for cat in order)):
            for order, population, effects in shuffled:
                used = set()
                base = 1.0 - 0.1 * max(population - 4, 0)
                offset = 1.0 - self.resources.effect_value(effects, 0x1C, base)
                for actor in order:
                    if actor in used:
                        continue
                    weights = [0.0 if target in used else self._weight(actor, target) for target in order]
                    partner = weighted_partner(order, weights, self.rng)
                    if partner is None or partner == actor or partner in used:
                        continue
                    native.uc.reg_write(UC_X86_REG_XMM2, struct.unpack("<Q", struct.pack("<d", offset))[0])
                    if native.call(0xD2B20, actor, partner) & 0xFF:
                        selected.append((actor, partner, effects))
                        used.update((actor, partner))
        killed = []
        for first, second, effects in selected:
            first_id, second_id = (self._read(cat, 0xC48, "<q") for cat in (first, second))
            native.call(0xD2C70, first, second)
            native.call(0xD2C70, second, first)
            reward = int(self.resources.effect_value(effects, 0x25, 1.0))
            damage = self.resources.effect_value(effects, 0x26, 0.0)
            first_severity, second_severity = self.severity(first, second, damage)
            result.pairs.append((first_id, second_id, first_severity, second_severity))
            if first_severity != second_severity:
                winner = first if first_severity < second_severity else second
                # Native list order is STR, DEX, CON, INT, CHA, SPD, LCK.
                index = (0, 1, 2, 3, 5, 4, 6)[int(self.rng.random() * 7)]
                increment = [0] * 7
                increment[index] = reward
                pointer = native.allocate(28)
                native.uc.mem_write(pointer, struct.pack("<7i", *increment))
                native.call(0xC2DA0, winner, pointer)
                result.rewards.append((self._read(winner, 0xC48, "<q"), index, reward))
            for cat, opponent, severity in ((first, second, first_severity), (second, first, second_severity)):
                key = self._read(cat, 0xC48, "<q")
                if severity == 1:
                    node = native.call(0xD0690, cat)
                    if self._read(node, 0xA8, "<i"):
                        result.injuries.append(key)
                elif severity == 2:
                    native.call(0xD3420, cat)
                    result.deaths.append(key)
                    killed.append((cat, opponent))
        for victim, killer in killed:
            if self._read(killer, 0x7AC, "<B"):
                continue
            key = self._read(victim, 0xC48, "<q")
            for cat in all_cats:
                if cat not in (victim, killer) and self._read(cat, 0xBC8, "<q") == key:
                    native.call(0xD2CF0, cat, killer)
        return result
