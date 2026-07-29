"""Derive the Stage 03/04 House overlay from the pinned MewUI MIT example SWF.

The source SWF contains a test button, three text fields, navigation controls,
and a toggle. AutoCattery keeps the Stage 03 button, places one independently
named copy of its known-good artwork for the Stage 04 new-day control, adds
four independently named Stage 12 recommendation rows, and removes the unused
example fields. Each row is a private two-frame static sign (hidden/shown) plus
an independent text field. It has no rope, button animation, or game Button
component. All other SWF definitions remain
byte-for-byte intact so the known-good artwork and its transitive dependencies
are preserved.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct


DEFINE_SPRITE = 39
DEFINE_EDIT_TEXT = 37
PLACE_OBJECT_2 = 26
SHOW_FRAME = 1
DO_ACTION = 12
END = 0
TARGET_MARKER = b"test_button\x00"
RECOMMENDATION_MARKER = b"recommend_button\x00"
# Keep the cloned button beside the original button at depth 18.  Higher
# native House HUD layers must remain in front of both buttons' hanging ropes.
RECOMMENDATION_DEPTH = 19
RECOMMENDATION_ROW_DEPTH = 40
RECOMMENDATION_TEXT_DEPTH = 50
RECOMMENDATION_ITEM_COUNT = 4
BUTTON_ROPE_DEPTH = 1
BUTTON_ICON_DEPTH = 6
BUTTON_LABEL_DEPTH = 8
DEFINITION_TAGS = frozenset({
    2, 6, 7, 10, 11, 20, 21, 22, 32, 33, 34, 35, 36, 37,
    39, 46, 48, 60, 73, 75, 83, 84, 87, 88, 90, 91,
})
REMOVED_NAMES = (
    b"test_nav_value\x00",
    b"test_nav_left\x00",
    b"test_nav_right\x00",
    b"test_toggle\x00",
    b"test_text\x00",
    b"test_text_2\x00",
    b"test_text_3\x00",
)
RELOCATED_TRANSFORMS = {
    TARGET_MARKER: (1010.0, 85.0, 0.65),
}
RECOMMENDATION_TRANSFORM = (1175.0, 85.0, 0.65)
RECOMMENDATION_ITEM_TRANSFORMS = tuple(
    (1110.0, 140.0 + index * 42.0, 0.42)
    for index in range(RECOMMENDATION_ITEM_COUNT)
)
RECOMMENDATION_TEXT_TRANSFORMS = tuple(
    (1015.0, 162.0 + index * 42.0, 0.32)
    for index in range(RECOMMENDATION_ITEM_COUNT)
)


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


def placed_character_id(body: bytes) -> int:
    if len(body) < 5 or not body[0] & 0x02:
        raise ValueError("button placement has no character id")
    return struct.unpack_from("<H", body, 3)[0]


def with_character_id(body: bytes, character_id: int) -> bytes:
    placed_character_id(body)
    output = bytearray(body)
    struct.pack_into("<H", output, 3, character_id)
    return bytes(output)


def make_recommendation_row_sprite(
    body: bytes,
    character_id: int,
) -> bytes:
    static_parts: list[bytes] = []
    rope_parts = 0
    icon_parts = 0
    label_parts = 0
    frame = 0
    for code, tag_start, body_start, tag_end in read_tags(
        body,
        4,
        len(body),
    ):
        if frame == 0 and code == PLACE_OBJECT_2:
            depth = struct.unpack_from("<H", body, body_start + 1)[0]
            if depth == BUTTON_ROPE_DEPTH:
                rope_parts += 1
            elif depth == BUTTON_ICON_DEPTH:
                icon_parts += 1
            elif depth == BUTTON_LABEL_DEPTH:
                label_parts += 1
            else:
                static_parts.append(body[tag_start:tag_end])
        if code == SHOW_FRAME:
            frame += 1
    if (
        rope_parts != 1
        or icon_parts != 1
        or label_parts != 1
        or len(static_parts) != 1
    ):
        raise ValueError(
            "unexpected source button first frame: "
            f"rope={rope_parts} icon={icon_parts} label={label_parts} "
            f"static={len(static_parts)}"
        )

    output = bytearray(struct.pack(
        "<HH",
        character_id,
        2,
    ))
    output.extend(encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(encode_tag(SHOW_FRAME, b""))
    for raw_tag in static_parts:
        output.extend(raw_tag)
    output.extend(encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(encode_tag(SHOW_FRAME, b""))
    output.extend(encode_tag(END, b""))
    return bytes(output)


def make_recommendation_text_definition(
    body: bytes,
    character_id: int,
) -> bytes:
    initial_text = b">Test</font>"
    if body.count(initial_text) != 1:
        raise ValueError("source recommendation text is not the expected Test")
    output = bytearray(body.replace(initial_text, b"></font>", 1))
    struct.pack_into("<H", output, 0, character_id)
    return bytes(output)


def filter_overlay_sprite(
    body: bytes,
    recommendation_row_character_id: int,
    recommendation_text_character_id: int,
) -> tuple[bytes, int, int, int]:
    sprite_header = body[:4]
    kept = bytearray(sprite_header)
    removed = 0
    relocated = 0
    cloned = 0
    tags = list(read_tags(body, 4, len(body)))
    source_button_body = None
    source_text_body = None
    for code, _, body_start, tag_end in tags:
        tag_body = body[body_start:tag_end]
        if code == PLACE_OBJECT_2 and TARGET_MARKER in tag_body:
            source_button_body = tag_body
        if code == PLACE_OBJECT_2 and b"test_text\x00" in tag_body:
            source_text_body = tag_body
    if source_button_body is None or source_text_body is None:
        raise ValueError("source row button/text placement is unavailable")

    used_depths = {
        struct.unpack_from("<H", body, body_start + 1)[0]
        for code, _, body_start, tag_end in tags
        if code == PLACE_OBJECT_2 and tag_end - body_start >= 3
    }
    reserved_depths = {
        RECOMMENDATION_DEPTH,
        *range(
            RECOMMENDATION_ROW_DEPTH,
            RECOMMENDATION_ROW_DEPTH + RECOMMENDATION_ITEM_COUNT,
        ),
        *range(
            RECOMMENDATION_TEXT_DEPTH,
            RECOMMENDATION_TEXT_DEPTH + RECOMMENDATION_ITEM_COUNT,
        ),
    }
    if used_depths & reserved_depths:
        raise ValueError("recommendation depth is already occupied")

    for code, tag_start, body_start, tag_end in tags:
        raw_tag = body[tag_start:tag_end]
        if code == PLACE_OBJECT_2 and any(
            name in raw_tag for name in REMOVED_NAMES
        ):
            removed += 1
            continue
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
            clone_body = bytearray(source_button_body)
            struct.pack_into("<H", clone_body, 1, RECOMMENDATION_DEPTH)
            clone_body = bytearray(bytes(clone_body).replace(
                    TARGET_MARKER,
                    RECOMMENDATION_MARKER,
                    1,
                ))
            clone_body = bytearray(relocate_matrix(
                    bytes(clone_body),
                    *RECOMMENDATION_TRANSFORM,
                ))
            kept.extend(encode_tag(code, bytes(clone_body)))
            cloned += 1
            for item_index, transform in enumerate(
                RECOMMENDATION_ITEM_TRANSFORMS
            ):
                item_body = bytearray(with_character_id(
                    source_button_body,
                    recommendation_row_character_id,
                ))
                struct.pack_into(
                    "<H",
                    item_body,
                    1,
                    RECOMMENDATION_ROW_DEPTH + item_index,
                )
                item_marker = (
                    f"recommend_row_{item_index + 1}".encode() + b"\x00"
                )
                item_body = bytearray(bytes(item_body).replace(
                    TARGET_MARKER,
                    item_marker,
                    1,
                ))
                item_body = bytearray(relocate_matrix(
                    bytes(item_body),
                    *transform,
                ))
                kept.extend(encode_tag(code, bytes(item_body)))
                cloned += 1
            for item_index, transform in enumerate(
                RECOMMENDATION_TEXT_TRANSFORMS
            ):
                text_body = bytearray(with_character_id(
                    source_text_body,
                    recommendation_text_character_id,
                ))
                struct.pack_into(
                    "<H",
                    text_body,
                    1,
                    RECOMMENDATION_TEXT_DEPTH + item_index,
                )
                text_marker = (
                    f"recommend_text_{item_index + 1}".encode() + b"\x00"
                )
                text_body = bytearray(bytes(text_body).replace(
                    b"test_text\x00",
                    text_marker,
                    1,
                ))
                text_body = bytearray(relocate_matrix(
                    bytes(text_body),
                    *transform,
                ))
                kept.extend(encode_tag(code, bytes(text_body)))
                cloned += 1
    return bytes(kept), removed, relocated, cloned


def build(source: Path, destination: Path) -> None:
    swf = source.read_bytes()
    start = tag_stream_start(swf)
    tags = list(read_tags(swf, start, len(swf)))
    overlay_button_character_id = None
    overlay_text_character_id = None
    existing_character_ids = set()
    for code, _, body_start, tag_end in tags:
        body = swf[body_start:tag_end]
        if code in DEFINITION_TAGS and len(body) >= 2:
            existing_character_ids.add(struct.unpack_from("<H", body, 0)[0])
        if code == DEFINE_SPRITE and TARGET_MARKER in body:
            for (
                child_code,
                _,
                child_body_start,
                child_tag_end,
            ) in read_tags(body, 4, len(body)):
                child = body[child_body_start:child_tag_end]
                if (
                    child_code == PLACE_OBJECT_2
                    and TARGET_MARKER in child
                ):
                    overlay_button_character_id = placed_character_id(child)
                if (
                    child_code == PLACE_OBJECT_2
                    and b"test_text\x00" in child
                ):
                    overlay_text_character_id = placed_character_id(child)
    if (
        overlay_button_character_id is None
        or overlay_text_character_id is None
        or not existing_character_ids
    ):
        raise ValueError("source button/text characters could not be resolved")
    recommendation_row_character_id = max(existing_character_ids) + 1
    recommendation_text_character_id = (
        recommendation_row_character_id + 1
    )
    if recommendation_text_character_id > 0xFFFF:
        raise ValueError("no SWF character id remains for recommendation rows")

    output = bytearray(swf[:start])
    found = False
    cloned_row_sprite = False
    cloned_text_definition = False
    removed = 0
    relocated = 0
    cloned = 0

    for code, tag_start, body_start, tag_end in tags:
        body = swf[body_start:tag_end]
        if (
            code == DEFINE_SPRITE
            and struct.unpack_from("<H", body, 0)[0] ==
                overlay_button_character_id
        ):
            output.extend(swf[tag_start:tag_end])
            output.extend(encode_tag(
                DEFINE_SPRITE,
                make_recommendation_row_sprite(
                    body,
                    recommendation_row_character_id,
                ),
            ))
            cloned_row_sprite = True
            continue
        if (
            code == DEFINE_EDIT_TEXT
            and struct.unpack_from("<H", body, 0)[0] ==
                overlay_text_character_id
        ):
            output.extend(swf[tag_start:tag_end])
            output.extend(encode_tag(
                DEFINE_EDIT_TEXT,
                make_recommendation_text_definition(
                    body,
                    recommendation_text_character_id,
                ),
            ))
            cloned_text_definition = True
            continue
        if code == DEFINE_SPRITE and TARGET_MARKER in body:
            if found:
                raise ValueError("more than one overlay sprite was found")
            body, removed, relocated, cloned = (
                filter_overlay_sprite(
                    body,
                    recommendation_row_character_id,
                    recommendation_text_character_id,
                )
            )
            output.extend(encode_tag(code, body))
            found = True
        else:
            output.extend(swf[tag_start:tag_end])

    if (
        not found
        or not cloned_row_sprite
        or not cloned_text_definition
        or removed != len(REMOVED_NAMES)
        or relocated != len(RELOCATED_TRANSFORMS)
        or cloned != 1 + (2 * RECOMMENDATION_ITEM_COUNT)
    ):
        raise ValueError(
            f"expected one overlay and {len(REMOVED_NAMES)} removals; "
            f"found={found} row_clone={cloned_row_sprite} "
            f"text_clone={cloned_text_definition} "
            f"removed={removed} "
            f"relocated={relocated} cloned={cloned}"
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
