"""Per-save unlock inputs and original native eligibility checks.

Only set membership/storage are adapted. The game's 232860/232920 decisions
run with the actual save lists and local locked_content resource.
"""

import struct
import sqlite3
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8

from breeding_native_cat import read_save_file
from breeding_resources import read_resources


class BreedingUnlocks:
    def __init__(self, native, resources, save, gpak, day, all_unlocked_mode=False):
        self.native = native
        self.resources = resources
        self.lists = native.unlock_lists(read_save_file(save, "unlocks"))
        locked = read_resources(gpak, ("data/locked_content.gon",))["data/locked_content.gon"]
        self.state = resources.set_day(day)
        native.uc.mem_write(self.state + 0x78D, bytes([all_unlocked_mode]))
        self.save_data = self.state + 0x38
        with sqlite3.connect(Path(save).resolve().as_uri() + "?mode=ro", uri=True) as db:
            self.properties = dict(db.execute("SELECT key, data FROM properties"))
        # 2322B0 reads the saved class vector here, preserving save order.
        classes = native.allocate(32 * len(self.lists[0]))
        for index, name in enumerate(self.lists[0]):
            resources.string(classes + 32 * index, name)
        end = classes + 32 * len(self.lists[0])
        native.uc.mem_write(self.save_data + 0xC0, struct.pack("<QQQ", classes, end, end))
        self.all_unlocked_mode = all_unlocked_mode
        self.unlocked = {"abilities": set(self.lists[1]), "passives": set(self.lists[2])}
        self.locked = {kind: set(locked[f"locked_{kind}"]) for kind in self.unlocked}
        self.sets = {}
        for kind, cache, restriction in (("abilities", 0x1A8, 0x330), ("passives", 0x1E8, 0x370)):
            self.sets[self.save_data + cache] = self.unlocked[kind]
            self.sets[self.save_data + restriction] = self.locked[kind]
            # A materialized membership adapter supplies these caches, including
            # empty lists, so the native lazy vector-to-set conversion is done.
            native.uc.mem_write(self.save_data + cache + 0x10, struct.pack("<Q", 1))
        self.hook = native.uc.hook_add(UC_HOOK_CODE, self._contains,
                                      begin=native.base + 0x1D8300, end=native.base + 0x1D8300)
        self.property_hook = native.uc.hook_add(UC_HOOK_CODE, self._property,
                                      begin=native.base + 0x22C5E0, end=native.base + 0x22C5E0)

    def close(self):
        self.native.uc.hook_del(self.hook)
        self.native.uc.hook_del(self.property_hook)

    def _property(self, uc, address, size, data):
        if uc.reg_read(UC_X86_REG_RCX) != self.save_data:
            raise ValueError("unexpected save-property owner")
        name = self.native.string(uc.reg_read(UC_X86_REG_RDX))
        value = self.properties.get(name, uc.reg_read(UC_X86_REG_R8))
        if not isinstance(value, int):
            raise ValueError(f"noninteger save property {name!r}")
        self.native._return(value & ((1 << 64) - 1))

    def _contains(self, uc, address, size, data):
        owner = uc.reg_read(UC_X86_REG_RCX)
        name = self.native.string(uc.reg_read(UC_X86_REG_RDX))
        if owner not in self.sets:
            raise ValueError(f"unmapped unlock membership set {owner:#x}")
        self.native._return(int(name in self.sets[owner]))

    def allowed(self, kind, name):
        return self.all_unlocked_mode or name in self.unlocked[kind] or name not in self.locked[kind]

    def native_allowed(self, kind, name):
        entry = {"abilities": 0x232860, "passives": 0x232920}[kind]
        mark = self.native.heap_next
        key = self.native.allocate(32)
        self.resources.string(key, name)
        result = bool(self.native.call(entry, self.save_data, key) & 255)
        self.native.rewind(mark)
        return result
