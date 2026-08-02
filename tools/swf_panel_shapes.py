"""Small SWF DefineShape3 helpers for the AutoCattery panel skin."""

from __future__ import annotations

import struct


DEFINE_SHAPE_3 = 32
PLACE_OBJECT_2 = 26
SHOW_FRAME = 1
DO_ACTION = 12
END = 0


class Bits:
    def __init__(self) -> None:
        self.values: list[int] = []

    def unsigned(self, value: int, count: int) -> None:
        for shift in range(count - 1, -1, -1):
            self.values.append((value >> shift) & 1)

    def signed(self, value: int, count: int) -> None:
        if value < 0:
            value += 1 << count
        self.unsigned(value, count)

    def bytes(self) -> bytes:
        while len(self.values) % 8:
            self.values.append(0)
        output = bytearray(len(self.values) // 8)
        for index, value in enumerate(self.values):
            output[index // 8] |= value << (7 - index % 8)
        return bytes(output)


def _signed_bits(*values: int) -> int:
    return max(1, *(value.bit_length() + 1 if value >= 0
                    else (~value).bit_length() + 1 for value in values))


def _rect(width: int, height: int) -> bytes:
    count = _signed_bits(0, width, height)
    bits = Bits()
    bits.unsigned(count, 5)
    for value in (0, width, 0, height):
        bits.signed(value, count)
    return bits.bytes()


def _edge(bits: Bits, delta: int, vertical: bool) -> None:
    count = max(2, _signed_bits(delta))
    bits.unsigned(1, 1)
    bits.unsigned(1, 1)
    bits.unsigned(count - 2, 4)
    bits.unsigned(0, 1)
    bits.unsigned(1 if vertical else 0, 1)
    bits.signed(delta, count)


def rectangle_shape(
    character_id: int,
    width_px: int,
    height_px: int,
    fill: tuple[int, int, int, int],
    line: tuple[int, int, int, int],
    line_width_px: int = 3,
) -> bytes:
    width = width_px * 20
    height = height_px * 20
    output = bytearray(struct.pack("<H", character_id))
    output.extend(_rect(width, height))
    output.extend(bytes((1, 0, *fill)))
    output.extend(bytes((1,)))
    output.extend(struct.pack("<H", line_width_px * 20))
    output.extend(bytes(line))
    output.append(0x11)

    shape = Bits()
    shape.unsigned(0, 1)
    shape.unsigned(0b01101, 5)
    shape.unsigned(1, 5)
    shape.signed(0, 1)
    shape.signed(0, 1)
    shape.unsigned(1, 1)
    shape.unsigned(1, 1)
    _edge(shape, width, False)
    _edge(shape, height, True)
    _edge(shape, -width, False)
    _edge(shape, -height, True)
    shape.unsigned(0, 6)
    output.extend(shape.bytes())
    return bytes(output)


def _encode_tag(code: int, body: bytes) -> bytes:
    if len(body) < 0x3F:
        return struct.pack("<H", (code << 6) | len(body)) + body
    return (struct.pack("<H", (code << 6) | 0x3F) +
            struct.pack("<I", len(body)) + body)


def _identity_matrix() -> bytes:
    bits = Bits()
    bits.unsigned(0, 1)
    bits.unsigned(0, 1)
    bits.unsigned(1, 5)
    bits.signed(0, 1)
    bits.signed(0, 1)
    return bits.bytes()


def _place(character_id: int) -> bytes:
    return (bytes((0x06,)) + struct.pack("<HH", 1, character_id) +
            _identity_matrix())


def three_frame_sprite(
    character_id: int,
    normal_shape_id: int,
    pressed_shape_id: int,
) -> bytes:
    output = bytearray(struct.pack("<HH", character_id, 3))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(PLACE_OBJECT_2, _place(normal_shape_id)))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(PLACE_OBJECT_2, _place(pressed_shape_id)))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(END, b""))
    return bytes(output)


def hidden_default_frame_sprite(
    character_id: int,
    normal_shape_id: int,
    pressed_shape_id: int,
) -> bytes:
    """Build a panel sprite whose initial timeline frame is empty."""
    # MewUI instantiates injected clips on timeline frame index 2. Keep that
    # frame hidden as well; visible artwork starts at index 3 after F10.
    output = bytearray(struct.pack("<HH", character_id, 5))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(PLACE_OBJECT_2, _place(normal_shape_id)))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(PLACE_OBJECT_2, _place(pressed_shape_id)))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(END, b""))
    return bytes(output)


def panel_root_sprite(character_id: int, child_tags: list[bytes]) -> bytes:
    """Build a root panel clip with no children on its initial frames."""
    output = bytearray(struct.pack("<HH", character_id, 5))
    for _ in range(3):
        output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
        output.extend(_encode_tag(SHOW_FRAME, b""))
    for tag in child_tags:
        output.extend(tag)
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(DO_ACTION, b"\x07\x00"))
    output.extend(_encode_tag(SHOW_FRAME, b""))
    output.extend(_encode_tag(END, b""))
    return bytes(output)
