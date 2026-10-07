"""GON data adapter for native, offline cat attribute evaluation.

Only named lookup is adapted. Iteration, scalar getters and game calculations
use the original executable. This is not a native GON-parser equivalence claim.
"""

import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8

from breeding_resources import GonObject, read_resources


class NativeResources:
    def __init__(self, native):
        self.native = native
        self.nodes = {}
        self.values = {}
        self.null = self.node(None)
        self.node(None, address=native.base + 0x13C4D30)
        self.node(None, address=native.base + 0x13C4DE0)
        for offset in (0x942CA0, 0x942DA0):
            native.uc.hook_add(UC_HOOK_CODE, self._lookup,
                              begin=native.base + offset, end=native.base + offset)
        native.uc.hook_add(UC_HOOK_CODE, self._find,
                          begin=native.base + 0x124DF0, end=native.base + 0x124DF0)

    def string(self, address, value):
        encoded = str(value).encode("utf-8")
        capacity = max(15, len(encoded))
        if capacity > 15:
            storage = self.native.allocate(len(encoded) + 1)
            self.native.uc.mem_write(storage, encoded + b"\0")
            payload = struct.pack("<Q", storage) + bytes(8)
        else:
            payload = encoded.ljust(16, b"\0")
        self.native.uc.mem_write(address, payload + struct.pack("<QQ", len(encoded), capacity))

    def node(self, value, name="", address=None):
        address = self.native.allocate(0xB0) if address is None else address
        uc = self.native.uc
        self.string(address + 0x68, value if isinstance(value, str) else "")
        self.string(address + 0x88, name)
        kind = 0
        self.nodes[address] = {}
        self.values[address] = value
        if isinstance(value, (dict, list)):
            kind = 4 if isinstance(value, list) else 3
            fields = (list(enumerate(value)) if kind == 4 else
                      value.fields if isinstance(value, GonObject) else list(value.items()))
            children = self.native.allocate(len(fields) * 0xB0) if fields else 0
            uc.mem_write(address + 0x38, struct.pack("<QQQ", children,
                         children + len(fields) * 0xB0, children + len(fields) * 0xB0))
            for index, (key, child) in enumerate(fields):
                ptr = self.node(child, "" if kind == 4 else key, children + index * 0xB0)
                self.nodes[address][str(key)] = ptr
        elif isinstance(value, bool):
            kind = 5
            uc.mem_write(address + 0x60, bytes([value]))
        elif isinstance(value, (int, float)):
            kind = 2
            integer = int(value)
            # The native numeric node stores both an int32 and a double.
            integer = (integer + 2**31) % 2**32 - 2**31
            uc.mem_write(address + 0x50, struct.pack("<i", integer))
            uc.mem_write(address + 0x58, struct.pack("<d", value))
        elif isinstance(value, str):
            kind = 1
        elif value is not None:
            raise TypeError(type(value))
        uc.mem_write(address + 0xA8, struct.pack("<i", kind))
        return address

    def _lookup(self, uc, address, size, data):
        owner = uc.reg_read(UC_X86_REG_RCX)
        key = self.native.string(uc.reg_read(UC_X86_REG_RDX))
        if owner not in self.nodes:
            raise ValueError(f"unmapped GON owner {owner:#x}, key {key!r}")
        self.native._return(self.nodes[owner].get(key, self.null))

    def _find(self, uc, address, size, data):
        owner = uc.reg_read(UC_X86_REG_RCX)
        output = uc.reg_read(UC_X86_REG_RDX)
        key = self.native.string(uc.reg_read(UC_X86_REG_R8))
        if owner not in self.nodes:
            raise ValueError(f"unmapped GON map {owner:#x}, key {key!r}")
        child = self.nodes[owner].get(key)
        if child is None:
            uc.mem_write(output, bytes(16))
        else:
            children, = struct.unpack("<Q", uc.mem_read(owner + 0x38, 8))
            entry = self.native.allocate(40)
            uc.mem_write(entry + 32, struct.pack("<i", (child - children) // 0xB0))
            uc.mem_write(output, struct.pack("<QQ", entry, entry))
        self.native._return(output)

    def load_cat_resources(self, gpak):
        sources = read_resources(gpak, prefixes=("data/classes/", "data/passives/",
                                                 "data/mutations/", "data/items/", "data/abilities/"))
        root = self.native.allocate(0x2000)
        for offset, prefix in ((0x508, "data/classes/"), (0x878, "data/passives/"),
                               (0x9D8, "data/mutations/"), (0x5B8, "data/items/"),
                               (0x198, "data/abilities/")):
            merged = GonObject()
            for name, source in sources.items():
                if name.startswith(prefix) and name != "data/items/modifiers.gon":
                    for key, value in source.fields:
                        merged.append(key, value)
            self.node(merged, address=root + offset)
        self.node(sources["data/items/modifiers.gon"], address=root + 0x668)
        custom = read_resources(gpak, ("data/custom_cats.gon",))["data/custom_cats.gon"]
        self.node(custom, address=root + 0x7C8)
        self.native.uc.mem_write(self.native.base + 0x13C79D0, struct.pack("<Q", root))
        return root

    def set_day(self, day):
        # The weak game-state pointer and matching generation used by D3130.
        storage = self.native.allocate(0x1000)
        state = storage + 8
        self.native.uc.mem_write(state + 0x580, struct.pack("<i", day))
        self.native.uc.mem_write(self.native.base + 0x13DAC30, struct.pack("<QQ", state, 0))
        return state

    def effect_vector(self, effects):
        """Materialize the inspected vector of name/double effect records."""
        vector = self.native.allocate(24)
        begin = self.native.allocate(len(effects) * 0x28) if effects else 0
        end = begin + len(effects) * 0x28
        self.native.uc.mem_write(vector, struct.pack("<QQQ", begin, end, end))
        for index, (name, value) in enumerate(effects.items()):
            address = begin + index * 0x28
            self.string(address, name)
            self.native.uc.mem_write(address + 0x20, struct.pack("<d", value))
        return vector

    def effect_value(self, vector, category, initial):
        """Apply native furniture effects to one end-day category."""
        mark = self.native.heap_next
        try:
            value = self.native.allocate(8)
            self.native.uc.mem_write(value, struct.pack("<d", initial))
            self.native.call(0x1B5910, vector, category, value)
            return struct.unpack("<d", self.native.uc.mem_read(value, 8))[0]
        finally:
            self.native.rewind(mark)
