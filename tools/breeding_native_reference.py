"""Execute the local EXE's stat stage in Unicorn, without launching the game.

Offsets describe the executable inspected on 2026-09-13. Re-locate them before
using a different game build. No native machine code or game assets are bundled.
"""

from pathlib import Path
import struct

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import (
    UC_X86_REG_GS_BASE, UC_X86_REG_RSP, UC_X86_REG_RBP,
    UC_X86_REG_R12, UC_X86_REG_R13, UC_X86_REG_RDI,
    UC_X86_REG_XMM9, UC_X86_REG_XMM13, UC_X86_REG_XMM14,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
)


class NativeStatReference:
    def __init__(self, exe):
        pe = pefile.PE(str(Path(exe)))
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
