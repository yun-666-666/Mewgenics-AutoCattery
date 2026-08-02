"""Derive the Stage 03/04 House overlay from the pinned MewUI MIT example SWF.

The source SWF contains a test button, three text fields, navigation controls,
and a toggle. AutoCattery keeps the Stage 03 button, places one independently
named copy of its known-good artwork for the Stage 04 new-day control, adds
four independently named Stage 12 recommendation rows, and removes the unused
example fields. Each row is a private three-frame paper sign
(hidden/normal/pressed) plus an independent text field. The paper is isolated
from the source texture and enlarged to the original board bounds. It has no
wooden board, rope, game Button component, or autonomous timeline animation.
All other SWF definitions remain
byte-for-byte intact so the known-good artwork and its transitive dependencies
are preserved.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import zlib

from swf_panel_shapes import (
    hidden_default_frame_sprite,
    rectangle_shape,
)
from swf_panel_layout import (
    PANEL_BACKGROUND_TRANSFORM,
    PANEL_COMPACT_ELEMENTS,
    PANEL_PROTECTION_ELEMENTS,
    PANEL_TEXTS,
)


DEFINE_SPRITE = 39
DEFINE_EDIT_TEXT = 37
DEFINE_SHAPE = 2
DEFINE_SHAPE_3 = 32
DEFINE_BITS_LOSSLESS_2 = 36
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
BUTTON_BACKGROUND_DEPTH = 5
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
RECOMMENDATION_ITEM_TRANSFORMS = (
    (1025.0, 175.0, 0.42),
    (1160.0, 175.0, 0.42),
    (1025.0, 235.0, 0.42),
    (1160.0, 235.0, 0.42),
)
RECOMMENDATION_TEXT_TRANSFORMS = (
    (986.0, 187.0, 0.25),
    (1121.0, 187.0, 0.25),
    (986.0, 247.0, 0.25),
    (1121.0, 247.0, 0.25),
)
# Pixel coordinates in the pinned source bitmap. The polygon follows the
# jagged white-paper silhouette and excludes the surrounding wooden board.
PAPER_POLYGON = (
    (52, 130), (102, 115), (119, 132), (129, 115),
    (244, 112), (257, 133), (268, 111), (378, 104),
    (381, 214), (365, 217), (353, 232), (338, 217),
    (324, 232), (302, 224), (277, 223), (263, 236),
    (251, 212), (238, 238), (227, 224), (153, 228),
    (143, 219), (133, 226), (119, 213), (106, 226),
    (83, 219), (66, 219), (54, 210),
)
PAPER_CROP = (52, 99, 382, 239)


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


def read_rect(data: bytes, byte_offset: int) -> tuple[
    tuple[int, int, int, int],
    int,
]:
    reader = BitReader(data, byte_offset * 8)
    count = reader.unsigned(5)
    bounds = (
        reader.signed(count),
        reader.signed(count),
        reader.signed(count),
        reader.signed(count),
    )
    return bounds, (reader.position + 7) // 8


def encode_rect(bounds: tuple[int, int, int, int]) -> bytes:
    count = signed_bit_count(*bounds)
    writer = BitWriter()
    writer.unsigned(count, 5)
    for value in bounds:
        writer.signed(value, count)
    return writer.bytes()


def matrix_end(data: bytes, byte_offset: int) -> int:
    reader = BitReader(data, byte_offset * 8)
    if reader.unsigned(1):
        count = reader.unsigned(5)
        reader.signed(count)
        reader.signed(count)
    if reader.unsigned(1):
        count = reader.unsigned(5)
        reader.signed(count)
        reader.signed(count)
    count = reader.unsigned(5)
    reader.signed(count)
    reader.signed(count)
    return (reader.position + 7) // 8


def encode_matrix(
    scale_x: float,
    scale_y: float,
    translate_x: int,
    translate_y: int,
) -> bytes:
    scale_values = (
        round(scale_x * 65536),
        round(scale_y * 65536),
    )
    scale_bits = signed_bit_count(*scale_values)
    translate_bits = signed_bit_count(translate_x, translate_y)
    writer = BitWriter()
    writer.unsigned(1, 1)
    writer.unsigned(scale_bits, 5)
    writer.signed(scale_values[0], scale_bits)
    writer.signed(scale_values[1], scale_bits)
    writer.unsigned(0, 1)
    writer.unsigned(translate_bits, 5)
    writer.signed(translate_x, translate_bits)
    writer.signed(translate_y, translate_bits)
    return writer.bytes()


def point_in_polygon(x: float, y: float) -> bool:
    inside = False
    previous = PAPER_POLYGON[-1]
    for current in PAPER_POLYGON:
        x1, y1 = previous
        x2, y2 = current
        if (y1 > y) != (y2 > y):
            crossing = (x2 - x1) * (y - y1) / (y2 - y1) + x1
            if x < crossing:
                inside = not inside
        previous = current
    return inside


def make_paper_bitmap(
    body: bytes,
    character_id: int,
) -> bytes:
    if len(body) < 7 or body[2] != 5:
        raise ValueError("source button texture is not lossless ARGB")
    width, height = struct.unpack_from("<HH", body, 3)
    pixels = zlib.decompress(body[7:])
    if len(pixels) != width * height * 4:
        raise ValueError("source button texture has unexpected dimensions")

    left, top, right, bottom = PAPER_CROP
    if right > width or bottom > height:
        raise ValueError("paper crop exceeds the source button texture")
    crop_width = right - left
    crop_height = bottom - top
    cropped = bytearray(crop_width * crop_height * 4)
    for crop_y, source_y in enumerate(range(top, bottom)):
        for crop_x, source_x in enumerate(range(left, right)):
            if not point_in_polygon(source_x + 0.5, source_y + 0.5):
                continue
            source_offset = (source_y * width + source_x) * 4
            target_offset = (
                (crop_y * crop_width + crop_x) * 4
            )
            cropped[target_offset:target_offset + 4] = (
                pixels[source_offset:source_offset + 4]
            )

    return (
        struct.pack(
            "<HBHH",
            character_id,
            5,
            crop_width,
            crop_height,
        )
        + zlib.compress(bytes(cropped), 9)
    )


def bitmap_id_from_shape(body: bytes) -> int:
    _, position = read_rect(body, 2)
    if body[position] != 1 or body[position + 1] != 0x41:
        raise ValueError("button background shape is not one clipped bitmap")
    return struct.unpack_from("<H", body, position + 2)[0]


def make_paper_shape(
    body: bytes,
    character_id: int,
    bitmap_character_id: int,
) -> bytes:
    bounds, position = read_rect(body, 2)
    if body[position] != 1 or body[position + 1] != 0x41:
        raise ValueError("button background shape is not one clipped bitmap")
    matrix_start = position + 4
    old_matrix_end = matrix_end(body, matrix_start)
    left, top, right, bottom = PAPER_CROP
    crop_width = right - left
    crop_height = bottom - top
    xmin, xmax, ymin, ymax = bounds
    matrix = encode_matrix(
        (xmax - xmin) / crop_width,
        (ymax - ymin) / crop_height,
        xmin,
        ymin,
    )
    output = bytearray(body[:matrix_start])
    struct.pack_into("<H", output, 0, character_id)
    struct.pack_into(
        "<H",
        output,
        position + 2,
        bitmap_character_id,
    )
    output.extend(matrix)
    output.extend(body[old_matrix_end:])
    return bytes(output)


def relocate_matrix(
    body: bytes,
    x: float,
    y: float,
    scale: float | tuple[float, float] | None,
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
        scale_pair = scale if isinstance(scale, tuple) else (scale, scale)
        scale_values = tuple(round(value * 65536) for value in scale_pair)
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
    normal_shape_character_id: int,
    pressed_shape_character_id: int,
) -> bytes:
    normal_background: bytes | None = None
    pressed_background: bytes | None = None
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
            elif depth == BUTTON_BACKGROUND_DEPTH:
                normal_background = with_character_id(
                    body[body_start:tag_end],
                    normal_shape_character_id,
                )
        if (
            frame == 39
            and code == PLACE_OBJECT_2
            and struct.unpack_from("<H", body, body_start + 1)[0] ==
                BUTTON_BACKGROUND_DEPTH
        ):
            pressed_background = with_character_id(
                body[body_start:tag_end],
                pressed_shape_character_id,
            )
        if code == SHOW_FRAME:
            frame += 1
    if (
        rope_parts != 1
        or icon_parts != 1
        or label_parts != 1
        or normal_background is None
        or pressed_background is None
    ):
        raise ValueError(
            "unexpected source button first frame: "
            f"rope={rope_parts} icon={icon_parts} label={label_parts} "
            f"normal={normal_background is not None} "
            f"pressed={pressed_background is not None}"
        )

    output = bytearray(struct.pack(
        "<HH",
        character_id,
        3,
    ))
    output.extend(encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(encode_tag(SHOW_FRAME, b""))
    output.extend(encode_tag(PLACE_OBJECT_2, normal_background))
    output.extend(encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(encode_tag(SHOW_FRAME, b""))
    output.extend(encode_tag(PLACE_OBJECT_2, pressed_background))
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


def make_panel_text_definition(
    body: bytes,
    character_id: int,
    width_twips: int = 60000,
) -> bytes:
    output = make_recommendation_text_definition(body, character_id)
    _, bounds_end = read_rect(output, 2)
    wide_bounds = encode_rect((-40, width_twips, -40, 2800))
    return output[:2] + wide_bounds + output[bounds_end:]


def button_background_ids(body: bytes) -> tuple[int, int]:
    frame = 0
    normal = None
    pressed = None
    for code, _, body_start, _ in read_tags(body, 4, len(body)):
        if code == PLACE_OBJECT_2:
            depth = struct.unpack_from("<H", body, body_start + 1)[0]
            if depth == BUTTON_BACKGROUND_DEPTH and frame == 0:
                normal = placed_character_id(body[body_start:])
            elif depth == BUTTON_BACKGROUND_DEPTH and frame == 39:
                pressed = placed_character_id(body[body_start:])
        if code == SHOW_FRAME:
            frame += 1
    if normal is None or pressed is None:
        raise ValueError("source button normal/down backgrounds are unavailable")
    return normal, pressed


def filter_overlay_sprite(
    body: bytes,
    recommendation_row_character_id: int,
    recommendation_text_character_id: int,
    panel_background_character_id: int,
    panel_control_character_id: int,
    panel_text_character_id: int,
    panel_compact_text_character_id: int,
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
        60,
        *range(61, 61 + len(PANEL_PROTECTION_ELEMENTS)),
        *range(90, 90 + len(PANEL_PROTECTION_ELEMENTS)),
        *range(200, 200 + len(PANEL_COMPACT_ELEMENTS)),
        *range(300, 300 + len(PANEL_COMPACT_ELEMENTS)),
        *range(400, 400 + len(PANEL_TEXTS)),
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
            background = bytearray(with_character_id(
                source_button_body,
                panel_background_character_id,
            ))
            struct.pack_into("<H", background, 1, 60)
            background = bytearray(bytes(background).replace(
                TARGET_MARKER, b"panel_background\x00", 1))
            background = bytearray(relocate_matrix(
                bytes(background), *PANEL_BACKGROUND_TRANSFORM))
            kept.extend(encode_tag(code, bytes(background)))
            cloned += 1
            for index, (name, row_transform, text_transform) in enumerate(
                PANEL_PROTECTION_ELEMENTS
            ):
                panel_row = bytearray(with_character_id(
                    source_button_body,
                    panel_control_character_id,
                ))
                struct.pack_into("<H", panel_row, 1, 61 + index)
                panel_row = bytearray(bytes(panel_row).replace(
                    TARGET_MARKER, name.encode() + b"\x00", 1))
                panel_row = bytearray(relocate_matrix(
                    bytes(panel_row), *row_transform))
                kept.extend(encode_tag(code, bytes(panel_row)))

                panel_text = bytearray(with_character_id(
                    source_text_body,
                    panel_text_character_id,
                ))
                struct.pack_into("<H", panel_text, 1, 90 + index)
                panel_text = bytearray(bytes(panel_text).replace(
                    b"test_text\x00",
                    f"{name}_text".encode() + b"\x00", 1))
                panel_text = bytearray(relocate_matrix(
                    bytes(panel_text), *text_transform))
                kept.extend(encode_tag(code, bytes(panel_text)))
                cloned += 2
            for index, (name, row_transform, text_transform) in enumerate(
                PANEL_COMPACT_ELEMENTS
            ):
                panel_row = bytearray(with_character_id(
                    source_button_body,
                    panel_control_character_id,
                ))
                struct.pack_into("<H", panel_row, 1, 200 + index)
                panel_row = bytearray(bytes(panel_row).replace(
                    TARGET_MARKER, name.encode() + b"\x00", 1))
                panel_row = bytearray(relocate_matrix(
                    bytes(panel_row), *row_transform))
                kept.extend(encode_tag(code, bytes(panel_row)))

                panel_text = bytearray(with_character_id(
                    source_text_body,
                    panel_compact_text_character_id,
                ))
                struct.pack_into("<H", panel_text, 1, 300 + index)
                panel_text = bytearray(bytes(panel_text).replace(
                    b"test_text\x00",
                    f"{name}_text".encode() + b"\x00", 1))
                panel_text = bytearray(relocate_matrix(
                    bytes(panel_text), *text_transform))
                kept.extend(encode_tag(code, bytes(panel_text)))
                cloned += 2
            for index, (name, transform) in enumerate(PANEL_TEXTS):
                panel_text = bytearray(with_character_id(
                    source_text_body,
                    panel_text_character_id,
                ))
                struct.pack_into(
                    "<H", panel_text, 1, 400 + index)
                panel_text = bytearray(bytes(panel_text).replace(
                    b"test_text\x00", name.encode() + b"\x00", 1))
                panel_text = bytearray(relocate_matrix(
                    bytes(panel_text), *transform))
                kept.extend(encode_tag(code, bytes(panel_text)))
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

    button_body = None
    definition_bodies: dict[int, tuple[int, bytes]] = {}
    for code, _, body_start, tag_end in tags:
        body = swf[body_start:tag_end]
        if code in DEFINITION_TAGS and len(body) >= 2:
            definition_bodies[struct.unpack_from("<H", body, 0)[0]] = (
                code,
                body,
            )
        if (
            code == DEFINE_SPRITE
            and len(body) >= 2
            and struct.unpack_from("<H", body, 0)[0] ==
                overlay_button_character_id
        ):
            button_body = body
    if button_body is None:
        raise ValueError("source button definition is unavailable")
    normal_source_id, pressed_source_id = button_background_ids(button_body)
    normal_definition = definition_bodies.get(normal_source_id)
    pressed_definition = definition_bodies.get(pressed_source_id)
    if (
        normal_definition is None
        or normal_definition[0] != DEFINE_SHAPE
        or pressed_definition is None
        or pressed_definition[0] != DEFINE_SHAPE
    ):
        raise ValueError("source button background shapes are unavailable")
    source_bitmap_id = bitmap_id_from_shape(normal_definition[1])
    if bitmap_id_from_shape(pressed_definition[1]) != source_bitmap_id:
        raise ValueError("source button states do not share one texture")
    bitmap_definition = definition_bodies.get(source_bitmap_id)
    if (
        bitmap_definition is None
        or bitmap_definition[0] != DEFINE_BITS_LOSSLESS_2
    ):
        raise ValueError("source button texture is unavailable")

    paper_bitmap_character_id = max(existing_character_ids) + 1
    normal_paper_shape_character_id = paper_bitmap_character_id + 1
    pressed_paper_shape_character_id = paper_bitmap_character_id + 2
    recommendation_row_character_id = paper_bitmap_character_id + 3
    recommendation_text_character_id = paper_bitmap_character_id + 4
    panel_background_shape_id = paper_bitmap_character_id + 5
    panel_background_character_id = paper_bitmap_character_id + 6
    panel_control_normal_shape_id = paper_bitmap_character_id + 7
    panel_control_pressed_shape_id = paper_bitmap_character_id + 8
    panel_control_character_id = paper_bitmap_character_id + 9
    panel_text_character_id = paper_bitmap_character_id + 10
    panel_compact_text_character_id = paper_bitmap_character_id + 11
    if panel_compact_text_character_id > 0xFFFF:
        raise ValueError("no SWF character id remains for recommendation rows")

    output = bytearray(swf[:start])
    found = False
    cloned_row_sprite = False
    cloned_text_definition = False
    cloned_panel_definitions = False
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
                DEFINE_BITS_LOSSLESS_2,
                make_paper_bitmap(
                    bitmap_definition[1],
                    paper_bitmap_character_id,
                ),
            ))
            output.extend(encode_tag(
                DEFINE_SHAPE,
                make_paper_shape(
                    normal_definition[1],
                    normal_paper_shape_character_id,
                    paper_bitmap_character_id,
                ),
            ))
            output.extend(encode_tag(
                DEFINE_SHAPE,
                make_paper_shape(
                    pressed_definition[1],
                    pressed_paper_shape_character_id,
                    paper_bitmap_character_id,
                ),
            ))
            output.extend(encode_tag(
                DEFINE_SPRITE,
                make_recommendation_row_sprite(
                    body,
                    recommendation_row_character_id,
                    normal_paper_shape_character_id,
                    pressed_paper_shape_character_id,
                ),
            ))
            output.extend(encode_tag(
                DEFINE_SHAPE_3,
                rectangle_shape(
                    panel_background_shape_id, 1000, 660,
                    (224, 216, 195, 248), (39, 37, 32, 255), 4),
            ))
            output.extend(encode_tag(
                DEFINE_SPRITE,
                hidden_default_frame_sprite(
                    panel_background_character_id,
                    panel_background_shape_id,
                    panel_background_shape_id),
            ))
            output.extend(encode_tag(
                DEFINE_SHAPE_3,
                rectangle_shape(
                    panel_control_normal_shape_id, 1000, 100,
                    (247, 243, 232, 255), (49, 46, 39, 255), 3),
            ))
            output.extend(encode_tag(
                DEFINE_SHAPE_3,
                rectangle_shape(
                    panel_control_pressed_shape_id, 1000, 100,
                    (205, 181, 126, 255), (49, 46, 39, 255), 4),
            ))
            output.extend(encode_tag(
                DEFINE_SPRITE,
                hidden_default_frame_sprite(
                    panel_control_character_id,
                    panel_control_normal_shape_id,
                    panel_control_pressed_shape_id),
            ))
            cloned_row_sprite = True
            cloned_panel_definitions = True
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
            output.extend(encode_tag(
                DEFINE_EDIT_TEXT,
                make_panel_text_definition(body, panel_text_character_id),
            ))
            output.extend(encode_tag(
                DEFINE_EDIT_TEXT,
                make_panel_text_definition(
                    body, panel_compact_text_character_id, 16000),
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
                    panel_background_character_id,
                    panel_control_character_id,
                    panel_text_character_id,
                    panel_compact_text_character_id,
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
        or not cloned_panel_definitions
        or removed != len(REMOVED_NAMES)
        or relocated != len(RELOCATED_TRANSFORMS)
        or cloned != (
            2 + (2 * RECOMMENDATION_ITEM_COUNT) +
            (2 * len(PANEL_PROTECTION_ELEMENTS)) +
            (2 * len(PANEL_COMPACT_ELEMENTS)) + len(PANEL_TEXTS)
        )
    ):
        raise ValueError(
            f"expected one overlay and {len(REMOVED_NAMES)} removals; "
            f"found={found} row_clone={cloned_row_sprite} "
            f"text_clone={cloned_text_definition} "
            f"panel_defs={cloned_panel_definitions} "
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
