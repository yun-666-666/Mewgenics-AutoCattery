"""Offline execution of the inspected build's complete CatData deserializer.

The EXE and saves stay local. Only allocation/free are host adapters; game
serialization and string/vector handling execute in Unicorn.
"""

from pathlib import Path
import sqlite3
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
    UC_X86_REG_RIP, UC_X86_REG_RSP, UC_X86_REG_R9, UC_X86_REG_XMM0,
)

from breeding_native_reference import NativeStatReference


def decode_cat_blob(stored):
    """The format-19/raw or length-prefixed LZ4 contract used by our adapter."""
    if len(stored) < 4:
        raise ValueError("truncated cat blob")
    expected, = struct.unpack_from("<I", stored)
    if expected == 19:
        return stored
    if not 20 <= expected <= 65536:
        raise ValueError("invalid cat blob size")
    cursor = 4
    output = bytearray()

    def length(value):
        nonlocal cursor
        if value == 15:
            while True:
                if cursor >= len(stored):
                    raise ValueError("truncated LZ4 length")
                extra = stored[cursor]
                cursor += 1
                value += extra
                if extra != 255:
                    break
        return value

    while cursor < len(stored):
        token = stored[cursor]
        cursor += 1
        literal = length(token >> 4)
        if cursor + literal > len(stored) or len(output) + literal > expected:
            raise ValueError("LZ4 literal exceeds bounds")
        output.extend(stored[cursor:cursor + literal])
        cursor += literal
        if cursor == len(stored):
            break
        if cursor + 2 > len(stored):
            raise ValueError("truncated LZ4 offset")
        offset, = struct.unpack_from("<H", stored, cursor)
        cursor += 2
        match = length(token & 15) + 4
        if not 0 < offset <= len(output) or len(output) + match > expected:
            raise ValueError("LZ4 match exceeds bounds")
        for _ in range(match):
            output.append(output[-offset])
    if len(output) != expected or output[:4] != b"\x13\0\0\0":
        raise ValueError("invalid decoded cat blob")
    return bytes(output)


def read_cat_blobs(save):
    with sqlite3.connect(Path(save).resolve().as_uri() + "?mode=ro", uri=True) as db:
        return [(key, decode_cat_blob(blob))
                for key, blob in db.execute("SELECT key, data FROM cats ORDER BY key")]


def read_save_file(save, key):
    with sqlite3.connect(Path(save).resolve().as_uri() + "?mode=ro", uri=True) as db:
        row = db.execute("SELECT data FROM files WHERE key=?", (key,)).fetchone()
        if row is None:
            raise ValueError(f"missing save file {key!r}")
        return row[0]


class NativeCatReference(NativeStatReference):
    def __init__(self, exe):
        super().__init__(exe)
        self.return_stub = self.data + 0x1F010
        self.uc.mem_write(self.return_stub, b"\xc3")
        self.heap = 0x30000000
        # Full ability definitions alone take ~14 MiB with the existing cat
        # resources. Leave room for catgen, real populations and newborns.
        self.heap_size = 0x4000000
        self.heap_next = self.heap
        self.uc.mem_map(self.heap, self.heap_size)
        self.allocations = {}
        # Keep 52A40 native: allocations >= 4096 use a 32-byte aligned
        # pointer with the original allocation stored at pointer-8.
        for address in (0xD3E0A4,):
            self.uc.hook_add(UC_HOOK_CODE, self._allocate_hook,
                             begin=self.base + address, end=self.base + address)
        for address in (0xD3E10C, 0xD4CB24):
            self.uc.hook_add(UC_HOOK_CODE, self._free_hook,
                             begin=self.base + address, end=self.base + address)
        self.uc.hook_add(UC_HOOK_CODE, self._reallocate_hook,
                         begin=self.base + 0xD4CB38, end=self.base + 0xD4CB38)
        self.uc.hook_add(UC_HOOK_CODE, self._linear_allocate_hook,
                         begin=self.base + 0x969930, end=self.base + 0x969930)
        self.uc.hook_add(UC_HOOK_CODE, self._pointer_vector_reserve_hook,
                         begin=self.base + 0x48D90, end=self.base + 0x48D90)
        self.uc.hook_add(UC_HOOK_CODE, self._int_vector_reserve_hook,
                         begin=self.base + 0xABB40, end=self.base + 0xABB40)

    def allocate(self, size):
        address = self.heap_next
        end = address + ((max(size, 1) + 15) & ~15)
        if end > self.heap + self.heap_size:
            raise MemoryError("native reference heap exhausted")
        self.heap_next = end
        self.allocations[address] = size
        self.uc.mem_write(address, bytes(max(size, 1)))
        return address

    def rewind(self, mark):
        """Release transient allocations after a completed, read-only call."""
        if not self.heap <= mark <= self.heap_next:
            raise ValueError("invalid native heap checkpoint")
        while self.allocations and next(reversed(self.allocations)) >= mark:
            self.allocations.popitem()
        self.heap_next = mark

    def _return(self, value=0):
        self.uc.reg_write(UC_X86_REG_RAX, value)
        # Let x86 RET consume the stack instead of round-tripping RSP and the
        # return address through Python for every resource/allocation callback.
        self.uc.reg_write(UC_X86_REG_RIP, self.return_stub)

    def _allocate_hook(self, uc, address, size, data):
        self._return(self.allocate(uc.reg_read(UC_X86_REG_RCX)))

    def _linear_allocate_hook(self, uc, address, size, data):
        self._return(self.allocate(uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF))

    def _pointer_vector_reserve_hook(self, uc, address, size, data):
        self._vector_reserve(uc, 8)

    def _int_vector_reserve_hook(self, uc, address, size, data):
        self._vector_reserve(uc, 4)

    def _vector_reserve(self, uc, item_size):
        vector = uc.reg_read(UC_X86_REG_RCX)
        capacity = uc.reg_read(UC_X86_REG_RDX) & 0xFFFFFFFF
        old_capacity, count, pointer = struct.unpack("<IIQ", uc.mem_read(vector, 16))
        if not capacity:
            uc.mem_write(vector, bytes(16))
        elif not pointer or capacity > old_capacity:
            result = self.allocate(capacity * item_size)
            if pointer:
                uc.mem_write(result, bytes(uc.mem_read(pointer, count * item_size)))
            uc.mem_write(vector, struct.pack("<IIQ", capacity, min(count, capacity), result))
        elif count > capacity:
            uc.mem_write(vector + 4, struct.pack("<I", capacity))
        self._return()

    def _free_hook(self, uc, address, size, data):
        pointer = uc.reg_read(UC_X86_REG_RCX)
        if pointer and pointer not in self.allocations:
            raise ValueError(f"native free of unknown pointer {pointer:#x}")
        self._return()

    def _reallocate_hook(self, uc, address, size, data):
        previous = uc.reg_read(UC_X86_REG_RCX)
        size = uc.reg_read(UC_X86_REG_RDX)
        result = self.allocate(size)
        if previous:
            count = min(size, self.allocations[previous])
            self.uc.mem_write(result, bytes(self.uc.mem_read(previous, count)))
        self._return(result)

    def call(self, address, *arguments):
        for register, value in zip(
            (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), arguments,
        ):
            self.uc.reg_write(register, value)
        stop = self.data + 0x1F000
        self.uc.mem_write(self.stack - 8, struct.pack("<Q", stop))
        self.uc.reg_write(UC_X86_REG_RSP, self.stack - 8)
        try:
            self.uc.emu_start(self.base + address, stop, count=2000000)
        except Exception as error:
            rva = self.uc.reg_read(UC_X86_REG_RIP) - self.base
            raise RuntimeError(f"native call {address:#x} failed at RVA {rva:#x}") from error
        if self.uc.reg_read(UC_X86_REG_RIP) != stop:
            raise RuntimeError(f"native call {address:#x} exceeded instruction limit")
        return self.uc.reg_read(UC_X86_REG_RAX)

    def deserialize(self, raw):
        if raw[:4] != b"\x13\0\0\0":
            raise ValueError("only the inspected format 19 is supported")
        cat = self.allocate(0xC58)
        self.call(0x5D6B0, cat)
        stream = self.read_stream(raw)
        self.call(0x22F410, cat, stream, 0)
        consumed, = struct.unpack("<I", self.uc.mem_read(stream + 0x28, 4))
        if consumed != len(raw):
            raise ValueError(f"native deserializer consumed {consumed}/{len(raw)} bytes")
        return cat

    def load_cat(self, raw, head_frames):
        """Breeding inputs after the loader's post-deserialization anatomy step.

        Native 230172 calls 22F410, then 23017B calls 7393E0. The latter's
        drawing coordinates are unnecessary here; its mutation-relevant
        visibility flags are projected from the local SWF timeline.
        """
        from breeding_visual import apply_head_visibility

        cat = self.deserialize(raw)
        apply_head_visibility(self, cat + 0x60, head_frames)
        return cat

    def serialize(self, cat):
        """Native in-memory record for the next simulated day; no save writes."""
        mark = self.heap_next
        try:
            stream = self.allocate(0x150)
            self.uc.mem_write(stream, struct.pack("<I", 1))
            self.call(0x22F410, cat, stream, 0)
            pointer, = struct.unpack("<Q", self.uc.mem_read(stream + 0x10, 8))
            length, = struct.unpack("<I", self.uc.mem_read(stream + 0x2C, 4))
            return bytes(self.uc.mem_read(pointer, length))
        finally:
            self.rewind(mark)

    def read_stream(self, raw, cursor=0):
        stream = self.allocate(0x150)
        source = self.allocate(len(raw))
        self.uc.mem_write(source, raw)
        # ByteStream mode 0 is READ; modes 1/2 are memory/file WRITE.
        self.uc.mem_write(stream + 0x18, struct.pack("<Q", source))
        self.uc.mem_write(stream + 0x24, struct.pack("<II", len(raw), cursor))
        return stream

    def unlock_lists(self, raw):
        """Seven version-3 vectors in the order read by 22B5E0.

        First three are classes, abilities, passives. Keep later vectors in
        serialized order rather than assigning unverified meanings.
        """
        if len(raw) < 4 or struct.unpack_from("<I", raw)[0] != 3:
            raise ValueError("only inspected unlock format 3 is supported")
        stream = self.read_stream(raw, 4)
        result = []
        for _ in range(7):
            vector = self.allocate(24)
            self.call(0x1DA440, stream, vector)
            begin, end = struct.unpack("<QQ", self.uc.mem_read(vector, 16))
            result.append([self.string(address) for address in range(begin, end, 32)])
        consumed, = struct.unpack("<I", self.uc.mem_read(stream + 0x28, 4))
        if consumed != len(raw):
            raise ValueError(f"native unlock reader consumed {consumed}/{len(raw)} bytes")
        return result

    def voice_progress(self, raw):
        """Read NPC progress through the native version-38 field at +5F0.

        This is the prefix of 1D3B90, stopped immediately after that field;
        it does not claim to deserialize the remaining NPC state.
        """
        if len(raw) < 4 or struct.unpack_from("<i", raw)[0] != 38:
            raise ValueError("only inspected NPC progress format 38 is supported")
        mark = self.heap_next
        try:
            progress = self.allocate(0x1000)
            stream = self.read_stream(raw)
            self.uc.reg_write(UC_X86_REG_RCX, progress)
            self.uc.reg_write(UC_X86_REG_RDX, stream)
            self.uc.reg_write(UC_X86_REG_RSP, self.stack - 8)
            stop = self.base + 0x1D4636
            self.uc.emu_start(self.base + 0x1D3B90, stop, count=2000000)
            if self.uc.reg_read(UC_X86_REG_RIP) != stop:
                raise RuntimeError("native NPC reader did not reach voice progression field")
            consumed, = struct.unpack("<I", self.uc.mem_read(stream + 0x28, 4))
            if consumed > len(raw):
                raise ValueError("native NPC reader exceeded input length")
            return struct.unpack("<i", self.uc.mem_read(progress + 0x5F0, 4))[0]
        finally:
            self.rewind(mark)

    def npc_breeding_inputs(self, raw):
        """Execute the complete format-38 reader; retain arrival inputs only."""
        if len(raw) < 4 or struct.unpack_from("<i", raw)[0] != 38:
            raise ValueError("only inspected NPC progress format 38 is supported")
        mark = self.heap_next
        try:
            progress = self.allocate(0x1000)
            stream = self.read_stream(raw)
            self.call(0x1D3B90, progress, stream)
            consumed, = struct.unpack("<I", self.uc.mem_read(stream + 0x28, 4))
            if consumed != len(raw):
                raise ValueError("native NPC reader did not consume the complete input")
            count, pointer = struct.unpack("<IQ", self.uc.mem_read(progress + 0x784, 12))
            return {
                "voice_progress": struct.unpack("<i", self.uc.mem_read(progress + 0x5F0, 4))[0],
                "same_sex_ids": list(struct.unpack(f"<{count}q", self.uc.mem_read(pointer, count * 8))) if count else [],
                "special_stray_counter": struct.unpack("<i", self.uc.mem_read(progress + 0x790, 4))[0],
                "mercy_npc_active": bool(struct.unpack("<Q", self.uc.mem_read(progress + 0x468, 8))[0])
                    and struct.unpack("<i", self.uc.mem_read(progress + 0x478, 4))[0] == 0,
            }
        finally:
            self.rewind(mark)

    def string(self, address):
        raw = self.uc.mem_read(address, 32)
        length, capacity = struct.unpack_from("<QQ", raw, 16)
        if capacity > 15:
            pointer, = struct.unpack_from("<Q", raw)
            return bytes(self.uc.mem_read(pointer, length)).decode("utf-8")
        return bytes(raw[:length]).decode("utf-8")

    def effective_stats(self, cat):
        output = self.allocate(28)
        context = self.allocate(28)
        self.uc.mem_write(context, struct.pack("<7i", *([-1] * 7)))
        self.uc.mem_write(self.stack - 8 + 0x28, bytes(8))
        self.call(0xC1820, cat, output, context, 1)
        return list(struct.unpack("<7i", self.uc.mem_read(output, 28)))

    def mating_weight(self, cat, target):
        """Entire D2850, including native adulthood and effective stats."""
        self.call(0xD2850, cat, target)
        bits = self.uc.reg_read(UC_X86_REG_XMM0) & ((1 << 64) - 1)
        return struct.unpack("<d", bits.to_bytes(8, "little"))[0]
