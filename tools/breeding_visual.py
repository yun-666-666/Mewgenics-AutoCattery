"""Read anatomy visibility from the local CatHeadPlacements timeline.

These booleans affect mutation collection. Drawing transforms are not part of
this projection. No SWF data is bundled or written by this module.
"""

import struct
import zlib

from build_house_ui_asset import BitReader, matrix_end, read_tags, tag_stream_start
from breeding_resources import read_resource_payloads


# CatVisualData offsets written by native 7393E0, including copied brow flags.
PLACEMENT_FLAGS = {
    0x290: "leye", 0x2E4: "reye", 0x338: "leye", 0x38C: "reye",
    0x3E0: "lear", 0x434: "rear", 0x488: "mouth", 0x4DC: "ahead",
    0x530: "aneck", 0x584: "aface",
}


def head_placement_names(gpak):
    raw = read_resource_payloads(gpak, ("swfs/catparts.swf",))["swfs/catparts.swf"]
    if raw[:3] == b"CWS":
        raw = b"FWS" + raw[3:8] + zlib.decompress(raw[8:])
    if len(raw) != struct.unpack_from("<I", raw, 4)[0]:
        raise ValueError("invalid SWF length")
    tags = list(read_tags(raw, tag_stream_start(raw), len(raw)))
    symbol_id = None
    for code, _, start, end in tags:
        if code != 76:
            continue
        count, = struct.unpack_from("<H", raw, start)
        cursor = start + 2
        for _ in range(count):
            character, = struct.unpack_from("<H", raw, cursor)
            cursor += 2
            stop = raw.index(0, cursor, end)
            if raw[cursor:stop] == b"CatHeadPlacements":
                symbol_id = character
            cursor = stop + 1
    if symbol_id is None:
        raise ValueError("CatHeadPlacements symbol is missing")
    for code, _, start, end in tags:
        if code == 39 and struct.unpack_from("<H", raw, start)[0] == symbol_id:
            return _timeline_names(raw[start + 4:end], struct.unpack_from("<H", raw, start + 2)[0])
    raise ValueError("CatHeadPlacements sprite is missing")


def _timeline_names(raw, expected_frames):
    depths = {}
    frames = []
    for code, _, start, end in read_tags(raw, 0, len(raw)):
        if code == 0:
            break
        if code == 1:
            frames.append(frozenset(name for name in depths.values() if name))
            continue
        if code == 28:
            depth, = struct.unpack_from("<H", raw, start)
            depths.pop(depth, None)
            continue
        if code != 26:
            raise ValueError(f"unsupported anatomy timeline tag {code}")
        flags = raw[start]
        depth, = struct.unpack_from("<H", raw, start + 1)
        cursor = start + 3
        name = depths.get(depth, "") if flags & 1 else ""
        if flags & 2:
            cursor += 2
        if flags & 4:
            cursor = matrix_end(raw, cursor)
        if flags & 8:
            bits = BitReader(raw, cursor * 8)
            add, multiply, width = bits.unsigned(1), bits.unsigned(1), bits.unsigned(4)
            for _ in range(4 * (add + multiply)):
                bits.signed(width)
            cursor = (bits.position + 7) // 8
        if flags & 16:
            cursor += 2
        if flags & 32:
            stop = raw.index(0, cursor, end)
            name = raw[cursor:stop].decode("utf-8")
            cursor = stop + 1
        if flags & 64:
            cursor += 2
        if flags & 128 or cursor != end:
            raise ValueError("unsupported anatomy placement payload")
        depths[depth] = name
    if len(frames) != expected_frames:
        raise ValueError("anatomy timeline frame count differs")
    return frames


def apply_head_visibility(native, visual, frames):
    head, = struct.unpack("<i", native.uc.mem_read(visual + 0x84, 4))
    if not 1 <= head <= len(frames):
        raise ValueError(f"unsupported head placement frame {head}")
    names = frames[head - 1]
    for offset, name in PLACEMENT_FLAGS.items():
        native.uc.mem_write(visual + offset, bytes([name in names]))
