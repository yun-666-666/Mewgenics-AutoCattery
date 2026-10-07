"""Execute native pool selection up to its cache commit, without game I/O.

The original code selects classes/groups, orders entries and checks unlocks.
Only GON insertion is collected by the host; the native cache commit and
temporary-container destruction are outside this bounded extraction call.
"""

import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
    UC_X86_REG_RIP, UC_X86_REG_RSP,
)

from breeding_resources import GonObject, read_resources


class NativeBreedingPools:
    def __init__(self, native, resources, unlocks, root, gpak):
        self.native = native
        self.resources = resources
        self.unlocks = unlocks
        self.root = root
        sources = read_resources(gpak, ("data/ability_pools.gon", "data/passive_pools.gon"))
        for kind, offset in (("ability", 0x3A8), ("passive", 0x458)):
            resources.node(sources[f"data/{kind}_pools.gon"], address=root + offset)

    def extract(self, kind, name):
        entry, finish, cache = {
            "abilities": (0x7B9F00, 0x7BA957, 0x1538),
            "passives": (0x7BAA30, 0x7BAEE8, 0x1578),
        }[kind]
        native = self.native
        uc = native.uc
        mark = native.heap_next
        result = GonObject()

        def collect(uc, address, size, data):
            owner = uc.reg_read(UC_X86_REG_RCX)
            key = native.string(uc.reg_read(UC_X86_REG_RDX))
            source = uc.reg_read(UC_X86_REG_R8)
            result.append(key, self.resources.values[source])
            native._return(owner)

        hook = uc.hook_add(UC_HOOK_CODE, collect,
                          begin=native.base + 0x945210, end=native.base + 0x945210)
        self.unlocks.sets[self.root + cache] = set()
        try:
            key = native.allocate(32)
            self.resources.string(key, name)
            uc.reg_write(UC_X86_REG_RCX, self.root)
            uc.reg_write(UC_X86_REG_RDX, key)
            uc.reg_write(UC_X86_REG_RSP, native.stack - 8)
            uc.mem_write(native.stack - 8, struct.pack("<Q", native.data + 0x1F000))
            uc.emu_start(native.base + entry, native.base + finish, count=2000000)
            if uc.reg_read(UC_X86_REG_RIP) != native.base + finish:
                raise RuntimeError("native pool extraction did not reach cache commit")
            return result
        except Exception as error:
            rva = uc.reg_read(UC_X86_REG_RIP) - native.base
            raise RuntimeError(f"native {kind} pool {name!r} failed at {rva:#x}") from error
        finally:
            uc.hook_del(hook)
            del self.unlocks.sets[self.root + cache]
            native.rewind(mark)
