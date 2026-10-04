"""Run the inspected game's name generator on a separate cosmetic RNG stream.

The local EXE selects names and maintains its recent-name history. Adapters
provide local text resources, language settings and UTF-8/UTF-16 conversion.
No name lists or game machine code are distributed with the workbench.
"""

import csv
import io
from pathlib import Path
import secrets
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9

from breeding_native_cat import NativeCatReference, read_save_file
from breeding_native_resources import NativeResources
from breeding_resources import read_resource_payloads


def save_language(source):
    settings = Path(source).parent.parent / "settings.txt"
    if settings.is_file():
        for line in settings.read_text(encoding="utf-8-sig").splitlines():
            fields = line.split()
            if len(fields) == 2 and fields[0] == "current_language":
                return fields[1]
    return "en"


class NativeNames:
    def __init__(self, exe, gpak, save, language, seed=None):
        self.native = NativeCatReference(exe)
        self.resources = NativeResources(self.native)
        self.language = language
        # Discover actual text files; languages without cat-name files use the
        # original generator's English fallback.
        with Path(gpak).open("rb") as stream:
            count, = struct.unpack("<I", stream.read(4))
            names = []
            for _ in range(count):
                length, = struct.unpack("<H", stream.read(2))
                name = stream.read(length).decode("utf-8")
                stream.read(4)
                if name.startswith("data/catnames_") and name.endswith(".txt"):
                    names.append(name)
        payloads = read_resource_payloads(gpak, names + ["data/text/combined.csv"])
        rows = csv.DictReader(io.StringIO(payloads.pop("data/text/combined.csv").decode("utf-8-sig")))
        shippable = next(row for row in rows if row["KEY"] == "CURRENT_LANGUAGE_SHIPPABLE")
        self.languages = [key for key, value in shippable.items() if value == "yes"]
        self.texts = {key: value.decode("utf-8-sig") for key, value in payloads.items()}
        native = self.native
        self.owner = native.allocate(0x180)
        self.output = native.allocate(32)
        self._wide(self.output, "")
        root = native.allocate(0x2000)
        native.uc.mem_write(native.base + 0x13C79D0, struct.pack("<Q", root))
        # Preload the six inspected vector<string> caches. Native loading uses
        # newline getline, removes empty entries and trims C whitespace. Doing
        # that IO here keeps each original selection within the emulator limit.
        for category, ordinary, foreign in (("male", 0x1378, 0x1418),
                                            ("female", 0x1390, 0x1430),
                                            ("neutral", 0x13A8, 0x1448)):
            path = f"data/catnames_{category}_{language}.txt"
            if path not in self.texts:
                path = f"data/catnames_{category}_en.txt"
            self._vector(root + ordinary, self._lines(self.texts[path]))
            values = []
            for locale in self.languages:
                path = f"data/catnames_{category}_{locale}.txt"
                if path in self.texts:
                    values.extend(self._lines(self.texts[path]))
            self._vector(root + foreign, values)
        # A disabled platform-name context is the game's native empty state.
        platform = native.allocate(0x400)
        native.uc.mem_write(native.base + 0x13C4A30, struct.pack("<Q", platform))
        native.uc.mem_write(native.tls + 0x178, seed if seed is not None else secrets.token_bytes(32))
        history = read_save_file(save, "name_gen_history_w")
        count, = struct.unpack_from("<Q", history)
        cursor = 8
        values = []
        for _ in range(count):
            length, = struct.unpack_from("<Q", history, cursor)
            cursor += 8
            values.append(history[cursor:cursor + length * 2].decode("utf-16-le"))
            cursor += length * 2
        if cursor != len(history):
            raise ValueError("猫名历史格式不完整")
        self._vector(self.owner + 0x168, values, wide=True)
        native.uc.hook_add(UC_HOOK_CODE, self._convert,
                           begin=native.base + 0x2D8F00, end=native.base + 0x2D8F00)

    @staticmethod
    def _lines(text):
        return [line.strip(" \t\r\v\f") for line in text.split("\n") if line]

    def _wide(self, address, value):
        encoded = value.encode("utf-16-le")
        length = len(encoded) // 2
        capacity = max(7, length)
        if capacity > 7:
            storage = self.native.allocate(len(encoded) + 2)
            self.native.uc.mem_write(storage, encoded + b"\0\0")
            payload = struct.pack("<Q", storage) + bytes(8)
        else:
            payload = encoded.ljust(16, b"\0")
        self.native.uc.mem_write(address, payload + struct.pack("<QQ", length, capacity))

    def _read_wide(self, address):
        length, capacity = struct.unpack("<QQ", self.native.uc.mem_read(address + 16, 16))
        pointer = struct.unpack("<Q", self.native.uc.mem_read(address, 8))[0] if capacity > 7 else address
        return bytes(self.native.uc.mem_read(pointer, length * 2)).decode("utf-16-le")

    def _vector(self, address, values, wide=False):
        size = len(values) * 32
        begin = self.native.allocate(size + 39 if size >= 4096 else size) if values else 0
        if size >= 4096:
            allocation = begin
            begin = (allocation + 39) & ~31
            self.native.uc.mem_write(begin - 8, struct.pack("<Q", allocation))
        end = begin + len(values) * 32
        self.native.uc.mem_write(address, struct.pack("<QQQ", begin, end, end))
        for index, value in enumerate(values):
            (self._wide if wide else self.resources.string)(begin + index * 32, value)

    def _convert(self, uc, address, size, data):
        output = uc.reg_read(UC_X86_REG_RDX)
        start, end = uc.reg_read(UC_X86_REG_R8), uc.reg_read(UC_X86_REG_R9)
        self._wide(output, bytes(uc.mem_read(start, end - start)).decode("utf-8"))
        self.native._return(output)

    def generate(self, sex):
        # B7B46 uses the actual sex, allows neutral names for random-sex
        # generation, and disables the rare foreign-language override.
        self.native.uc.mem_write(self.native.stack + 0x20, b"\0")
        self.native.call(0xD8E20, self.owner, self.output, sex, 0)
        name = self._read_wide(self.output)
        if not name:
            raise ValueError("原游戏命名函数未返回猫名")
        return name

    def history(self):
        begin, end = struct.unpack("<QQ", self.native.uc.mem_read(self.owner + 0x168, 16))
        output = bytearray(struct.pack("<Q", (end - begin) // 32))
        for address in range(begin, end, 32):
            name = self._read_wide(address).encode("utf-16-le")
            output += struct.pack("<Q", len(name) // 2) + name
        return bytes(output)
