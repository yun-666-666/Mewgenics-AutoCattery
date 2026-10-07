"""Read only the local GON resources needed by the breeding experiment.

No game assets are bundled. Object iteration preserves repeated fields, while
named lookup selects the last field (the published GON library's representation,
also reflected in the current executable's child-array/index-map lookup).
"""

import json
from pathlib import Path
import re
import struct


TOKEN = re.compile(r'\s+|//[^\n]*|\#[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}\[\],=:]|(?:(?!//|/\*)[^\s{}\[\],=:#])+')


class GonObject(dict):
    def __init__(self):
        super().__init__()
        self.fields = []

    def append(self, key, value):
        self.fields.append((key, value))
        self[key] = value


def parse_gon(text):
    tokens = [match.group() for match in TOKEN.finditer(text)
              if not match.group().isspace()
              and not match.group().startswith(("//", "/*", "#"))
              and match.group() not in (",", "=", ":")]
    cursor = 0

    def value():
        nonlocal cursor
        token = tokens[cursor]
        cursor += 1
        if token == "{":
            return mapping("}")
        if token == "[":
            result = []
            while cursor < len(tokens) and tokens[cursor] != "]":
                if tokens[cursor] == ",":
                    cursor += 1
                else:
                    result.append(value())
            if cursor == len(tokens):
                raise ValueError("Unclosed GON array")
            cursor += 1
            return result
        if token in ("}", "]", ","):
            raise ValueError(f"Unexpected GON token {token}")
        if token.startswith('"'):
            return json.loads(token)
        if token in ("true", "false"):
            return token == "true"
        try:
            return int(token)
        except ValueError:
            try:
                return float(token.rstrip("%")) / (100 if token.endswith("%") else 1)
            except ValueError:
                return token

    def mapping(end=None):
        nonlocal cursor
        result = GonObject()
        while cursor < len(tokens):
            if tokens[cursor] == end:
                cursor += 1
                return result
            if tokens[cursor] == ",":
                cursor += 1
                continue
            key = tokens[cursor]
            cursor += 1
            if key.startswith('"'):
                key = json.loads(key)
            if cursor == len(tokens):
                raise ValueError(f"Missing GON value for {key}")
            result.append(key, value())
        if end:
            raise ValueError("Unclosed GON object")
        return result

    return mapping()


def read_resource_payloads(gpak, names=(), prefixes=()):
    wanted = set(names)
    result = {}
    with Path(gpak).open("rb") as stream:
        count, = struct.unpack("<I", stream.read(4))
        entries = []
        for _ in range(count):
            length, = struct.unpack("<H", stream.read(2))
            name = stream.read(length).decode("utf-8")
            size, = struct.unpack("<I", stream.read(4))
            entries.append((name, size))
        for name, size in entries:
            if name in wanted or (name.startswith(tuple(prefixes)) and name.endswith(".gon")):
                payload = stream.read(size)
                if len(payload) != size:
                    raise ValueError(f"Truncated game resource {name}")
                result[name] = payload
            else:
                stream.seek(size, 1)
    if wanted - result.keys():
        raise ValueError(f"Missing game resources: {sorted(wanted - result.keys())}")
    return result


def read_resources(gpak, names=(), prefixes=()):
    return {name: parse_gon(payload.decode("utf-8-sig"))
            for name, payload in read_resource_payloads(gpak, names, prefixes).items()}
