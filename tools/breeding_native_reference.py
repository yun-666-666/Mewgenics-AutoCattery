"""Execute the local EXE's stat stage in Unicorn, without launching the game.

Offsets describe the executable inspected on 2026-09-13. Re-locate them before
using a different game build. No native machine code or game assets are bundled.
"""

from pathlib import Path
import struct

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_GS_BASE, UC_X86_REG_RSP, UC_X86_REG_RBP,
    UC_X86_REG_R12, UC_X86_REG_R13, UC_X86_REG_RDI,
    UC_X86_REG_XMM9, UC_X86_REG_XMM13, UC_X86_REG_XMM14,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9,
    UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2,
    UC_X86_REG_RAX, UC_X86_REG_RIP,
)


class NativeStatReference:
    def __init__(self, exe):
        # The emulator loads section bytes at ImageBase; it does not use PE
        # imports, unwind tables or relocation-directory parsing.
        pe = pefile.PE(str(Path(exe)), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(self.base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        self.uc.mem_write(self.base, pe.get_memory_mapped_image())
        pe.close()
        self.data = 0x20000000
        self.uc.mem_map(self.data, 0x20000)
        self.stack = self.data + 0x18000
        self.mother = self.data + 0x2000
        self.father = self.data + 0x4000
        self.child = self.data + 0x6000
        self.tls = self.data + 0x8000
        # gs:[0x58] -> TLS pointer table -> TLS block; RNG is TLS+0x178.
        self.uc.reg_write(UC_X86_REG_GS_BASE, self.data)
        self.uc.mem_write(self.data + 0x58, struct.pack("<Q", self.data + 0x1000))
        self.uc.mem_write(self.data + 0x1000, struct.pack("<Q", self.tls))

    def _call_random_helper(self, address, state):
        self.uc.mem_write(self.tls + 0x178, struct.pack("<4Q", *state))
        self.uc.reg_write(UC_X86_REG_RSP, self.stack - 8)
        stop = self.data + 0x1F000
        self.uc.mem_write(self.stack - 8, struct.pack("<Q", stop))
        self.uc.emu_start(self.base + address, stop, count=2000000)
        return list(struct.unpack("<4Q", self.uc.mem_read(self.tls + 0x178, 32)))

    def mutate_stats(self, stats, increments, decrements, lower, upper, retry, state):
        """Execute CatStats::mutate, including rejected draws and clamping."""
        uc = self.uc
        uc.mem_write(self.child, struct.pack("<7i", *stats))
        for register, value in (
            (UC_X86_REG_RCX, self.child), (UC_X86_REG_RDX, increments),
            (UC_X86_REG_R8, decrements), (UC_X86_REG_R9, lower),
        ):
            uc.reg_write(register, value)
        uc.mem_write(self.stack - 8 + 0x28, struct.pack("<i", upper))
        uc.mem_write(self.stack - 8 + 0x30, bytes([retry]))
        final = self._call_random_helper(0xB55B0, state)
        return list(struct.unpack("<7i", uc.mem_read(self.child, 28))), final

    def biased_random(self, bias, state):
        """Execute the original helper and its original CRT math implementation."""
        self.uc.reg_write(UC_X86_REG_RDX, self.tls + 0x178)
        self.uc.reg_write(UC_X86_REG_XMM0, int.from_bytes(struct.pack("<d", bias), "little"))
        final = self._call_random_helper(0x94F4C0, state)
        bits = self.uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)
        return struct.unpack("<d", bits.to_bytes(8, "little"))[0], final

    def initial_personality(self, state):
        self.uc.reg_write(UC_X86_REG_RCX, self.child)
        final = self._call_random_helper(0xB7110, state)
        values = tuple(struct.unpack("<d", self.uc.mem_read(self.child + offset, 8))[0]
                       for offset in (0x28, 0x30, 0x58, 0x60))
        return values, final

    def scalar_math(self, address, values):
        for register, value in zip(
            (UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2), values,
        ):
            self.uc.reg_write(register, int.from_bytes(struct.pack("<d", value), "little"))
        self._call_random_helper(address, [1, 2, 3, 4])
        bits = self.uc.reg_read(UC_X86_REG_XMM0)
        return struct.unpack("<2d", bits.to_bytes(16, "little"))

    def attraction(self, libido, sexuality, presentation, target_presentation,
                   target_charisma, lover_id, target_id, love):
        """Original mating-weight function with explicit adult/stat adapters.

        Only target adulthood and already-resolved effective stats are supplied;
        original trigonometry, sex branches and relationship arithmetic run.
        This does not validate the native effective-stat resolver itself.
        """
        uc = self.uc
        for cat in (self.mother, self.father):
            uc.mem_write(cat, bytes(0xC58))
        uc.mem_write(self.mother + 0xC48, struct.pack("<q", target_id + 1))
        uc.mem_write(self.father + 0xC48, struct.pack("<q", target_id))
        uc.mem_write(self.mother + 0x5C, struct.pack("<i", presentation))
        uc.mem_write(self.father + 0x5C, struct.pack("<i", target_presentation))
        uc.mem_write(self.mother + 0xBB8, struct.pack("<ddqd", libido, sexuality, lover_id, love))

        def resolved_input(uc, address, size, user_data):
            if address == self.base + 0xD3130:
                uc.reg_write(UC_X86_REG_RAX, 0)  # supplied adult target
            else:
                output = uc.reg_read(UC_X86_REG_RDX)
                uc.mem_write(output, struct.pack("<7i", 0, 0, 0, 0, 0, target_charisma, 0))
                uc.reg_write(UC_X86_REG_RAX, output)
            stack = uc.reg_read(UC_X86_REG_RSP)
            ret = struct.unpack("<Q", uc.mem_read(stack, 8))[0]
            uc.reg_write(UC_X86_REG_RSP, stack + 8)
            uc.reg_write(UC_X86_REG_RIP, ret)

        hooks = [uc.hook_add(UC_HOOK_CODE, resolved_input,
                             begin=self.base + offset, end=self.base + offset)
                 for offset in (0xD3130, 0xC1820)]
        try:
            uc.reg_write(UC_X86_REG_RCX, self.mother)
            uc.reg_write(UC_X86_REG_RDX, self.father)
            self._call_random_helper(0xD2850, [1, 2, 3, 4])
            bits = uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)
            return struct.unpack("<d", bits.to_bytes(8, "little"))[0]
        finally:
            for hook in hooks:
                uc.hook_del(hook)

    def inherit(self, mother, father, weights, state):
        uc = self.uc
        uc.mem_write(self.mother + 0x6F0, struct.pack("<7i", *mother))
        uc.mem_write(self.father + 0x6F0, struct.pack("<7i", *father))
        uc.mem_write(self.tls + 0x178, struct.pack("<4Q", *state))
        for register, value in (
            (UC_X86_REG_RSP, self.stack), (UC_X86_REG_R12, self.mother),
            (UC_X86_REG_R13, self.father), (UC_X86_REG_RDI, self.child),
        ):
            uc.reg_write(register, value)
        for register, weight in zip(
            (UC_X86_REG_XMM9, UC_X86_REG_XMM13, UC_X86_REG_XMM14),
            weights, strict=True,
        ):
            uc.reg_write(register, int.from_bytes(struct.pack("<d", weight), "little"))
        # The actual seven-call block from CatData::breed, including its helper
        # and native RNG. No Python RNG hook or replacement selector is used.
        uc.emu_start(self.base + 0xA8E65, self.base + 0xA8FA0, count=10000)
        child = list(struct.unpack("<7i", uc.mem_read(self.child + 0x6F0, 28)))
        final_state = list(struct.unpack("<4Q", uc.mem_read(self.tls + 0x178, 32)))
        return child, final_state

    def effect(self, name, value, kind, initial):
        """Run the original furniture-effect evaluator (including its memcmp)."""
        uc = self.uc
        obj = self.data + 0xA000
        encoded = name.encode("ascii")
        if len(encoded) > 15:
            uc.mem_write(obj, struct.pack("<Q", obj + 0x100) + bytes(8))
            uc.mem_write(obj + 0x100, encoded + b"\0")
        else:
            uc.mem_write(obj, encoded.ljust(16, b"\0"))
        uc.mem_write(obj + 16, struct.pack("<QQd", len(encoded), max(15, len(encoded)), value))
        output = obj + 0x200
        uc.mem_write(output, struct.pack("<d", initial))
        uc.reg_write(UC_X86_REG_RCX, obj)
        uc.reg_write(UC_X86_REG_RDX, kind)
        uc.reg_write(UC_X86_REG_R8, output)
        uc.reg_write(UC_X86_REG_RSP, self.stack - 8)
        uc.reg_write(UC_X86_REG_RBP, 0)
        stop = self.data + 0x1F000
        uc.mem_write(self.stack - 8, struct.pack("<Q", stop))
        uc.emu_start(self.base + 0x1B4930, stop, count=10000)
        return struct.unpack("<d", uc.mem_read(output, 8))[0]
