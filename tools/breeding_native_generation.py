"""Native body/voice initialization with local resource and anatomy adapters.

This is a generation component, not yet the complete CatData initializer.
NPC voice progression is supplied from the native npc_progress reader.
"""

from contextlib import contextmanager
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_XMM3, UC_X86_REG_RSP,
)

from breeding_resources import read_resources
from breeding_visual import apply_head_visibility, head_placement_names
from breeding_native_pools import NativeBreedingPools


VOICE_FLAGS = (
    "mute_during_explicit_scenarios", "cat_speaks", "cat_swears",
    "cat_gimmick", "distinctly_male", "distinctly_female",
)


class NativeVisualGeneration:
    def __init__(self, native, resources, root, state, gpak, *, npc_voice_progress):
        self.native = native
        self.resources = resources
        self.root = root
        self.frames = head_placement_names(gpak)
        catgen = read_resources(gpak, ("data/catgen.gon",))["data/catgen.gon"]
        self.catgen = catgen
        self.resource_files = {}
        for name, value in read_resources(gpak, ("data/catgen.gon", "data/special_strays.gon",
                                                "data/injuries.gon")).items():
            pointer = native.allocate(0x28 + 0xB0)
            resources.node(value, address=pointer + 0x28)
            self.resource_files[name] = pointer
        resources.node(catgen, address=root + 0x12C8)
        progress = native.allocate(0x798)
        self.progress = progress
        native.uc.mem_write(progress + 0x5F0, struct.pack("<i", npc_voice_progress))
        native.uc.mem_write(progress + 0x790, struct.pack("<i", -1))
        native.uc.mem_write(state + 0x5A8, struct.pack("<Q", progress))
        names = set(catgen["voice_sets"]) | set(catgen["special_voice_sets"])
        voices = read_resources(gpak, tuple(f"audio/voices/{name}.gon" for name in sorted(names)))
        self.voice_meta = {}
        for name in names:
            meta = voices[f"audio/voices/{name}.gon"].get("Meta", {})
            entry = native.allocate(0x38)
            # 7A88F5..7A8B8F loads strict GON booleans in this order.
            flags = bytes(meta.get(key) is True for key in VOICE_FLAGS)
            native.uc.mem_write(entry + 0x30, flags)
            self.voice_meta[name] = entry
        self.hooks = []
        for offset, callback in ((0x611A0, self._resource), (0xAB840, self._voice_metadata),
                                 (0x7393E0, self._anatomy)):
            self.hooks.append(native.uc.hook_add(UC_HOOK_CODE, callback,
                              begin=native.base + offset, end=native.base + offset))

    def close(self):
        for hook in self.hooks:
            self.native.uc.hook_del(hook)
        self.hooks.clear()

    def _resource(self, uc, address, size, data):
        name = self.native.string(uc.reg_read(UC_X86_REG_RDX)).rstrip("\0")
        if name not in self.resource_files:
            raise ValueError(f"unmapped generation resource {name!r}")
        self.native._return(self.resource_files[name])

    def _voice_metadata(self, uc, address, size, data):
        if uc.reg_read(UC_X86_REG_RCX) != self.root + 0x1650:
            raise ValueError("unexpected voice metadata owner")
        name = self.native.string(uc.reg_read(UC_X86_REG_R8))
        output = uc.reg_read(UC_X86_REG_RDX)
        uc.mem_write(output, struct.pack("<QB", self.voice_meta[name], 0))
        self.native._return(output)

    def _anatomy(self, uc, address, size, data):
        apply_head_visibility(self.native, uc.reg_read(UC_X86_REG_RCX), self.frames)
        self.native._return()

    def initialize(self, cat, sex):
        if sex not in (0, 1, 2, 3):
            raise ValueError("invalid native generation sex")
        self.native.call(0x7374C0, cat + 0x60, sex)


class NativeCatGeneration:
    """Physiological initializer; cosmetic names are intentionally excluded.

    D8E20 returns a wide name and updates name history, with no cat-attribute
    output. Removing that stream segment makes this a stochastic breeding
    model, not an exact replay of the game's complete RNG state/seed.
    """

    def __init__(self, native, resources, root, unlocks, gpak, *, npc_voice_progress):
        self.native = native
        self.root = root
        self.state = unlocks.state
        self.bonus_items = []
        self.pools = {}
        extractor = NativeBreedingPools(native, resources, unlocks, root, gpak)
        classes = resources.values[root + 0x508]
        for kind, offset in (("abilities", 0x3A8), ("passives", 0x458)):
            names = set(classes) | set(resources.values[root + offset]) | {"AnyUnlocked", "Jester"}
            if kind == "abilities":
                names.add("JesterMinusColorless")
                for name, definition in classes.fields:
                    names.update(name + "." + group for group in definition.get("ability_groups", {}))
            for name in sorted(names):
                self.pools[kind, name] = resources.node(extractor.extract(kind, name))
        self.visual = NativeVisualGeneration(native, resources, root, unlocks.state, gpak,
                                             npc_voice_progress=npc_voice_progress)
        self.hooks = []
        for offset, callback in ((0x7B9E60, self._abilities), (0x7B9EB0, self._passives),
                                 (0x7B9F00, self._abilities), (0x7BAA30, self._passives),
                                 (0xD8E20, self._cosmetic_name), (0x962430, self._localized_name),
                                 (0x2E13F0, self._store_bonus_item)):
            self.hooks.append(native.uc.hook_add(UC_HOOK_CODE, callback,
                              begin=native.base + offset, end=native.base + offset))

    def close(self):
        for hook in self.hooks:
            self.native.uc.hook_del(hook)
        self.hooks.clear()
        self.visual.close()

    def _store_bonus_item(self, uc, address, size, data):
        # C4231 grants the resource's bonus_items after native item construction
        # (including modifiers/RNG). The remaining call only stores the item.
        caller, = struct.unpack("<Q", uc.mem_read(uc.reg_read(UC_X86_REG_RSP), 8))
        if caller != self.native.base + 0xC4236:
            raise ValueError("unexpected inventory write outside native bonus_items grant")
        item = uc.reg_read(UC_X86_REG_RDX)
        self.bonus_items.append({
            "native_id": struct.unpack("<Q", uc.mem_read(item, 8))[0],
            "item_id": self.native.string(item + 8),
            "modifier": self.native.string(item + 0x28),
            "native_tail": bytes(uc.mem_read(item + 0x48, 24)).hex(),
        })
        self.native._return()

    def _pool(self, kind, uc):
        if uc.reg_read(UC_X86_REG_RCX) != self.root:
            raise ValueError("unexpected generation pool owner")
        name = self.native.string(uc.reg_read(UC_X86_REG_RDX))
        self.native._return(self.pools[kind, name])

    def _abilities(self, uc, address, size, data):
        self._pool("abilities", uc)

    def _passives(self, uc, address, size, data):
        self._pool("passives", uc)

    def _cosmetic_name(self, uc, address, size, data):
        output = uc.reg_read(UC_X86_REG_RDX)
        uc.mem_write(output, bytes(24) + struct.pack("<Q", 7))
        self.native._return(output)

    def _localized_name(self, uc, address, size, data):
        caller, = struct.unpack("<Q", uc.mem_read(uc.reg_read(UC_X86_REG_RSP), 8))
        if caller != self.native.base + 0xAA9FB:
            raise ValueError("unexpected non-name localization call")
        self._cosmetic_name(uc, address, size, data)

    def initialize(self, sex=3):
        if sex not in (0, 1, 2, 3):
            raise ValueError("invalid native generation sex")
        cat = self.native.allocate(0xC58)
        self.native.call(0x5D6B0, cat)
        self.native.call(0xB7320, cat, 0, sex, 1)
        return cat

    def breed(self, mother, father, coi, *, effects=0):
        """Run A89A0, including native trait/anatomy inheritance and defects.

        Parent identities/pedigree and the night's fertility/partner rolls
        belong to the population caller. effects is a native effect vector;
        zero represents the game's null-vector (no furniture effects) path.
        """
        if not 0 <= coi <= 1:
            raise ValueError("COI must be in [0, 1]")
        native = self.native
        cat = native.allocate(0xC58)
        native.call(0x5D6B0, cat)
        native.uc.reg_write(UC_X86_REG_XMM3, struct.unpack("<Q", struct.pack("<d", coi))[0])
        native.uc.mem_write(native.stack + 0x20, struct.pack("<Q", effects))
        native.call(0xA89A0, cat, mother, father)
        return cat

    def apply_stray_effects(self, cat, effects):
        self.native.call(0xAA1E0, cat, effects)

    @contextmanager
    def social_sources(self, ids, cats):
        """Supply actual same-sex mating IDs and ID lookup for AA1E0.

        All cats, including parents that died later that night, must remain
        in the in-memory registry until arrivals have been generated.
        """
        native = self.native
        vector = self.visual.progress + 0x780
        old = bytes(native.uc.mem_read(vector, 16))
        pointers = native.allocate(max(8, len(ids) * 8))
        if ids:
            native.uc.mem_write(pointers, struct.pack(f"<{len(ids)}q", *ids))
        native.uc.mem_write(vector, struct.pack("<IIQ", len(ids), len(ids), pointers))
        owner, = struct.unpack("<Q", native.uc.mem_read(self.state + 0x598, 8))

        def lookup(uc, address, size, data):
            if uc.reg_read(UC_X86_REG_RCX) != owner:
                raise ValueError("unexpected social-source registry")
            key = uc.reg_read(UC_X86_REG_RDX)
            if key not in cats:
                raise ValueError(f"missing social-source cat {key}")
            native._return(cats[key])

        hook = native.uc.hook_add(UC_HOOK_CODE, lookup,
            begin=native.base + 0xD7220, end=native.base + 0xD7220)
        try:
            yield
        finally:
            native.uc.hook_del(hook)
            native.uc.mem_write(vector, old)
