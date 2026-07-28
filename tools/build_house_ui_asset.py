"""Derive the Stage 03 House overlay from the pinned MewUI MIT example SWF.

The source SWF contains a test button, three text fields, navigation controls,
and a toggle. AutoCattery keeps only the button and two text fields. All other
SWF definitions remain byte-for-byte intact so the known-good button artwork
and its transitive symbol dependencies are preserved.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


DEFINE_SPRITE = 39
PLACE_OBJECT_2 = 26
END = 0
TARGET_MARKER = b"test_button\x00"
REMOVED_NAMES = (
    b"test_nav_value\x00",
    b"test_nav_left\x00",
    b"test_nav_right\x00",
    b"test_toggle\x00",
    b"test_text_3\x00",
)


def tag_stream_start(swf: bytes) -> int:
    if swf[:3] != b"FWS":
        raise ValueError("the pinned source must be an uncompressed FWS file")
    rect_bits = 5 + 4 * (swf[8] >> 3)
    return 8 + (rect_bits + 7) // 8 + 4


def read_tags(data: bytes, start: int, end: int):
    position = start
    while position + 2 <= end:
        tag_start = position
        header = struct.unpack_from("<H", data, position)[0]
        position += 2
        code = header >> 6
        length = header & 0x3F
        if length == 0x3F:
            length = struct.unpack_from("<I", data, position)[0]
            position += 4
        body_start = position
        position += length
        if position > end:
            raise ValueError("truncated SWF tag")
        yield code, tag_start, body_start, position
        if code == END:
            break


def encode_tag(code: int, body: bytes) -> bytes:
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return (
        struct.pack("<H", (code << 6) | 0x3F)
        + struct.pack("<I", len(body))
        + body
    )


def filter_overlay_sprite(body: bytes) -> tuple[bytes, int]:
    sprite_header = body[:4]
    kept = bytearray(sprite_header)
    removed = 0
    for code, tag_start, _, tag_end in read_tags(body, 4, len(body)):
        raw_tag = body[tag_start:tag_end]
        if code == PLACE_OBJECT_2 and any(
            name in raw_tag for name in REMOVED_NAMES
        ):
            removed += 1
            continue
        kept.extend(raw_tag)
    return bytes(kept), removed


def build(source: Path, destination: Path) -> None:
    swf = source.read_bytes()
    start = tag_stream_start(swf)
    output = bytearray(swf[:start])
    found = False
    removed = 0

    for code, tag_start, body_start, tag_end in read_tags(
        swf, start, len(swf)
    ):
        body = swf[body_start:tag_end]
        if code == DEFINE_SPRITE and TARGET_MARKER in body:
            if found:
                raise ValueError("more than one overlay sprite was found")
            body, removed = filter_overlay_sprite(body)
            output.extend(encode_tag(code, body))
            found = True
        else:
            output.extend(swf[tag_start:tag_end])

    if not found or removed != len(REMOVED_NAMES):
        raise ValueError(
            f"expected one overlay and {len(REMOVED_NAMES)} removals; "
            f"found={found} removed={removed}"
        )

    struct.pack_into("<I", output, 4, len(output))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    build(args.source, args.destination)


if __name__ == "__main__":
    main()
