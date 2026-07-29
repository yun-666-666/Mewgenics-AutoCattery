"""Derive the Stage 03/04 House overlay from the pinned MewUI MIT example SWF.

The source SWF contains a test button, three text fields, navigation controls,
and a toggle. AutoCattery keeps the Stage 03 button, places one independently
named copy of its known-good artwork for the Stage 04 new-day control, retains
one independently named text field for the Stage 12 recommendation summary,
and removes the unused example fields. All other SWF definitions remain
byte-for-byte intact so the known-good artwork and its transitive dependencies
are preserved.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


DEFINE_SPRITE = 39
PLACE_OBJECT_2 = 26
END = 0
TARGET_MARKER = b"test_button\x00"
RECOMMENDATION_MARKER = b"recommend_button\x00"
SUMMARY_SOURCE_MARKER = b"test_text\x00"
SUMMARY_MARKER = b"recommend_summary\x00"
# Keep the cloned button beside the original button at depth 18.  Higher
# native House HUD layers must remain in front of both buttons' hanging ropes.
RECOMMENDATION_DEPTH = 19
REMOVED_NAMES = (
    b"test_nav_value\x00",
    b"test_nav_left\x00",
    b"test_nav_right\x00",
    b"test_toggle\x00",
    b"test_text_2\x00",
    b"test_text_3\x00",
)
RELOCATED_TRANSFORMS = {
    TARGET_MARKER: (1010.0, 85.0, 0.65),
}
RECOMMENDATION_TRANSFORM = (1175.0, 85.0, 0.65)
SUMMARY_TRANSFORM = (940.0, 145.0, 0.75)


class BitReader:
    def __init__(self, data: bytes, bit_position: int) -> None:
        self.data = data
        self.position = bit_position

    def unsigned(self, count: int) -> int:
        value = 0
        for _ in range(count):
            byte = self.data[self.position // 8]
            bit = 7 - (self.position % 8)
            value = (value << 1) | ((byte >> bit) & 1)
            self.position += 1
        return value

    def signed(self, count: int) -> int:
        value = self.unsigned(count)
        if count and value & (1 << (count - 1)):
            value -= 1 << count
        return value


class BitWriter:
    def __init__(self) -> None:
        self.bits: list[int] = []

    def unsigned(self, value: int, count: int) -> None:
        for shift in range(count - 1, -1, -1):
            self.bits.append((value >> shift) & 1)

    def signed(self, value: int, count: int) -> None:
        if value < 0:
            value += 1 << count
        self.unsigned(value, count)

    def bytes(self) -> bytes:
        while len(self.bits) % 8:
            self.bits.append(0)
        output = bytearray(len(self.bits) // 8)
        for index, bit in enumerate(self.bits):
            output[index // 8] |= bit << (7 - index % 8)
        return bytes(output)


def signed_bit_count(*values: int) -> int:
    return max(
        1,
        *(
            value.bit_length() + 1
            if value >= 0
            else (~value).bit_length() + 1
            for value in values
        ),
    )


def relocate_matrix(
    body: bytes,
    x: float,
    y: float,
    scale: float | None,
) -> bytes:
    flags = body[0]
    if not flags & 0x04:
        raise ValueError("named overlay object has no transform matrix")

    matrix_start = 3 + (2 if flags & 0x02 else 0)
    reader = BitReader(body, matrix_start * 8)
    has_scale = reader.unsigned(1)
    scale_values: tuple[int, int] | None = None
    scale_bits = 0
    if has_scale:
        scale_bits = reader.unsigned(5)
        scale_values = (
            reader.signed(scale_bits),
            reader.signed(scale_bits),
        )
    if scale is not None:
        has_scale = 1
        scale_raw = round(scale * 65536)
        scale_values = (scale_raw, scale_raw)
        scale_bits = signed_bit_count(*scale_values)

    has_rotate = reader.unsigned(1)
    rotate_values: tuple[int, int] | None = None
    rotate_bits = 0
    if has_rotate:
        rotate_bits = reader.unsigned(5)
        rotate_values = (
            reader.signed(rotate_bits),
            reader.signed(rotate_bits),
        )

    old_translate_bits = reader.unsigned(5)
    reader.signed(old_translate_bits)
    reader.signed(old_translate_bits)
    matrix_end = (reader.position + 7) // 8

    translate_x = round(x * 20)
    translate_y = round(y * 20)
    translate_bits = signed_bit_count(translate_x, translate_y)

    writer = BitWriter()
    writer.unsigned(has_scale, 1)
    if scale_values is not None:
        writer.unsigned(scale_bits, 5)
        writer.signed(scale_values[0], scale_bits)
        writer.signed(scale_values[1], scale_bits)
    writer.unsigned(has_rotate, 1)
    if rotate_values is not None:
        writer.unsigned(rotate_bits, 5)
        writer.signed(rotate_values[0], rotate_bits)
        writer.signed(rotate_values[1], rotate_bits)
    writer.unsigned(translate_bits, 5)
    writer.signed(translate_x, translate_bits)
    writer.signed(translate_y, translate_bits)

    return body[:matrix_start] + writer.bytes() + body[matrix_end:]


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


def filter_overlay_sprite(
    body: bytes,
) -> tuple[bytes, int, int, int, int]:
    sprite_header = body[:4]
    kept = bytearray(sprite_header)
    removed = 0
    relocated = 0
    cloned = 0
    summary = 0
    tags = list(read_tags(body, 4, len(body)))
    used_depths = {
        struct.unpack_from("<H", body, body_start + 1)[0]
        for code, _, body_start, tag_end in tags
        if code == PLACE_OBJECT_2 and tag_end - body_start >= 3
    }
    if RECOMMENDATION_DEPTH in used_depths:
        raise ValueError("recommendation depth is already occupied")

    for code, tag_start, body_start, tag_end in tags:
        raw_tag = body[tag_start:tag_end]
        if code == PLACE_OBJECT_2 and any(
            name in raw_tag for name in REMOVED_NAMES
        ):
            removed += 1
            continue
        if code == PLACE_OBJECT_2 and SUMMARY_SOURCE_MARKER in raw_tag:
            tag_body = body[body_start:tag_end].replace(
                SUMMARY_SOURCE_MARKER,
                SUMMARY_MARKER,
                1,
            )
            tag_body = relocate_matrix(tag_body, *SUMMARY_TRANSFORM)
            raw_tag = encode_tag(code, tag_body)
            summary += 1
        if code == PLACE_OBJECT_2:
            for name, transform in RELOCATED_TRANSFORMS.items():
                if name in raw_tag:
                    tag_body = relocate_matrix(
                        body[body_start:tag_end],
                        *transform,
                    )
                    raw_tag = encode_tag(code, tag_body)
                    relocated += 1
                    break
        kept.extend(raw_tag)
        if code == PLACE_OBJECT_2 and TARGET_MARKER in raw_tag:
            clone_body = bytearray(body[body_start:tag_end])
            struct.pack_into("<H", clone_body, 1, RECOMMENDATION_DEPTH)
            clone_body = bytearray(
                bytes(clone_body).replace(
                    TARGET_MARKER,
                    RECOMMENDATION_MARKER,
                    1,
                )
            )
            clone_body = bytearray(
                relocate_matrix(
                    bytes(clone_body),
                    *RECOMMENDATION_TRANSFORM,
                )
            )
            kept.extend(encode_tag(code, bytes(clone_body)))
            cloned += 1
    return bytes(kept), removed, relocated, cloned, summary


def build(source: Path, destination: Path) -> None:
    swf = source.read_bytes()
    start = tag_stream_start(swf)
    output = bytearray(swf[:start])
    found = False
    removed = 0
    relocated = 0
    cloned = 0
    summary = 0

    for code, tag_start, body_start, tag_end in read_tags(
        swf, start, len(swf)
    ):
        body = swf[body_start:tag_end]
        if code == DEFINE_SPRITE and TARGET_MARKER in body:
            if found:
                raise ValueError("more than one overlay sprite was found")
            body, removed, relocated, cloned, summary = (
                filter_overlay_sprite(body)
            )
            output.extend(encode_tag(code, body))
            found = True
        else:
            output.extend(swf[tag_start:tag_end])

    if (
        not found
        or removed != len(REMOVED_NAMES)
        or relocated != len(RELOCATED_TRANSFORMS)
        or cloned != 1
        or summary != 1
    ):
        raise ValueError(
            f"expected one overlay and {len(REMOVED_NAMES)} removals; "
            f"found={found} removed={removed} "
            f"relocated={relocated} cloned={cloned} summary={summary}"
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
