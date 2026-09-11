import re
import struct
import sys
from pathlib import Path


ENTRY = re.compile(
    r'\{(MEW_RVA_[A-Z0-9_]+), 0, "[^"]+", "([0-9A-F? ]+)", '
    r'(?:MEW_DIRECT_RVA|(\d+)U, (\d+)U)\}'
)
COMPONENT_BUCKET_PREPARE_PATTERN = (
    "40 56 41 56 41 57 48 83 EC 40 48 8B 41 20 48 8B F1 4C 63 FA "
    "4D 8B C7 4D 8B F7 49 C1 E0 04 42 80 7C 00 08 00"
)
HOUSE_DETAIL_LAYOUTS = (
    (0xEBEF0, 0xEFCB0, 0x1A93F0, 0x1FE082, 0x1FE262, 0x1FE2D5),
    (0xEC7B0, 0xF0570, 0x1A9E10, 0x1FEAF2, 0x1FECD2, 0x1FED45),
)
HOUSE_DETAIL_OPEN_PATTERN = "40 53 48 83 EC 40 48 8B D9 48 85 D2"
HOUSE_DETAIL_TARGET_PATTERN = (
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 71 18"
)
HOUSE_DETAIL_DRAWER_PATTERN = (
    "48 89 5C 24 08 57 48 83 EC 20 48 8B 79 18 BA D5 01 00 00 "
    "48 8B 5F 08 48"
)
HOUSE_DETAIL_CAT_CALL_PATTERN = "49 8B CF E8 ?? ?? ?? ?? 4C 8B F8"
HOUSE_DETAIL_DRAWER_CALL_PATTERN = (
    "49 8B CE E8 ?? ?? ?? ?? 48 8B F8 49 83 7E 38 00"
)
HOUSE_DETAIL_OPEN_CALL_PATTERN = (
    "41 B0 01 49 8B D7 48 8B CF E8 ?? ?? ?? ??"
)
NATIVE_MOVE_CANDIDATES = (
    (0x2E7DB0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20"),
    (0x2E88D0,
     "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F9 "
     "48 8B F2 48 8B 49 70 4C 8B C2 8B 47 6C"),
)


def text_section(image: bytes) -> tuple[int, bytes]:
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    section_count = struct.unpack_from("<H", image, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe_offset + 20)[0]
    section_offset = pe_offset + 24 + optional_size
    for index in range(section_count):
        entry = section_offset + index * 40
        name = image[entry:entry + 8].rstrip(b"\0")
        virtual_address = struct.unpack_from("<I", image, entry + 12)[0]
        raw_size = struct.unpack_from("<I", image, entry + 16)[0]
        raw_offset = struct.unpack_from("<I", image, entry + 20)[0]
        if name == b".text":
            return virtual_address, image[raw_offset:raw_offset + raw_size]
    raise ValueError("PE image has no .text section")


def locate_unique(data: bytes, pattern: str) -> int:
    tokens = pattern.split()
    required = [(token != "??", int(token, 16) if token != "??" else 0)
                for token in tokens]
    matches = []
    first_required = next(index for index, value in enumerate(required)
                          if value[0])
    marker = bytes([required[first_required][1]])
    start = 0
    while True:
        marker_offset = data.find(marker, start)
        if marker_offset < 0:
            break
        candidate = marker_offset - first_required
        start = marker_offset + 1
        if candidate < 0 or candidate + len(required) > len(data):
            continue
        if all(not needed or data[candidate + index] == value
               for index, (needed, value) in enumerate(required)):
            matches.append(candidate)
            if len(matches) > 1:
                break
    if len(matches) != 1:
        raise AssertionError(
            f"pattern resolved {len(matches)} matches instead of one")
    return matches[0]


def matches_at(data: bytes, offset: int, pattern: str) -> bool:
    tokens = pattern.split()
    if offset < 0 or offset + len(tokens) > len(data):
        return False
    return all(token == "??" or data[offset + index] == int(token, 16)
               for index, token in enumerate(tokens))


def relative_call_target(data: bytes, offset: int) -> int | None:
    if offset < 0 or offset + 5 > len(data) or data[offset] != 0xE8:
        return None
    return offset + 5 + struct.unpack_from("<i", data, offset + 1)[0]


def detail_layout_matches(
        text: bytes, text_rva: int, layout: tuple[int, ...]) -> bool:
    open_rva, target_rva, drawer_rva, cat_call, drawer_call, open_call = layout
    offsets = [value - text_rva for value in layout]
    open_offset, target_offset, drawer_offset = offsets[:3]
    cat_call_offset, drawer_call_offset, open_call_offset = offsets[3:]
    return (
        matches_at(text, open_offset, HOUSE_DETAIL_OPEN_PATTERN) and
        matches_at(text, target_offset, HOUSE_DETAIL_TARGET_PATTERN) and
        matches_at(text, drawer_offset, HOUSE_DETAIL_DRAWER_PATTERN) and
        matches_at(text, cat_call_offset, HOUSE_DETAIL_CAT_CALL_PATTERN) and
        relative_call_target(text, cat_call_offset + 3) == target_offset and
        matches_at(text, drawer_call_offset,
                   HOUSE_DETAIL_DRAWER_CALL_PATTERN) and
        relative_call_target(text, drawer_call_offset + 3) == drawer_offset and
        matches_at(text, open_call_offset, HOUSE_DETAIL_OPEN_CALL_PATTERN) and
        relative_call_target(text, open_call_offset + 9) == open_offset)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: mew_ui_runtime_locator_tests.py <mew_ui_api.c> <Mewgenics.exe>")
        return 2
    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    for token in (
            "GetModuleFileNameW",
            "CreateFileMappingW",
            "SEC_IMAGE",
            "MapViewOfFile",
            "MewUI_OpenCleanGameImage(&view)"):
        if token not in source:
            raise AssertionError(
                f"runtime locator must scan a clean executable image: {token}")
    image = Path(sys.argv[2]).read_bytes()
    text_rva, text = text_section(image)
    entries = ENTRY.findall(source)
    if len(entries) < 25:
        raise AssertionError(f"only {len(entries)} runtime locator entries found")
    resolved = {}
    match_offsets = {}
    patterns = {}
    for name, pattern, displacement_offset, instruction_end_offset in entries:
        try:
            match_offset = locate_unique(text, pattern)
            match_rva = text_rva + match_offset
        except AssertionError as error:
            raise AssertionError(f"{name}: {error}") from error
        match_offsets[name] = match_offset
        patterns[name] = pattern
        if displacement_offset:
            offset = match_rva - text_rva + int(displacement_offset)
            displacement = struct.unpack_from("<i", text, offset)[0]
            match_rva += int(instruction_end_offset) + displacement
        resolved[name] = match_rva
    scene_name = "MEW_RVA_SCENE_READY_UPDATE"
    scene_required = [token != "??" for token in patterns[scene_name].split()]
    first_required = next(index for index, needed in enumerate(scene_required)
                          if needed)
    hooked_text = bytearray(text)
    patch_offset = match_offsets[scene_name] + first_required
    hooked_text[patch_offset] ^= 0xFF
    try:
        locate_unique(bytes(hooked_text), patterns[scene_name])
    except AssertionError:
        pass
    else:
        raise AssertionError(
            "the test hook mutation must invalidate the live scene-ready signature")
    scene_ready = resolved["MEW_RVA_SCENE_READY_UPDATE"]
    component_candidates = (0x96B470,)
    component_matches = [
        rva for rva in component_candidates
        if matches_at(
            text, rva - text_rva, COMPONENT_BUCKET_PREPARE_PATTERN)
    ]
    if len(component_matches) != 1:
        raise AssertionError(
            "component bucket prepare candidate resolved "
            f"{len(component_matches)} matches instead of one")
    component_bucket_prepare = component_matches[0]
    detail_matches = [
        layout for layout in HOUSE_DETAIL_LAYOUTS
        if detail_layout_matches(text, text_rva, layout)
    ]
    if len(detail_matches) != 1:
        raise AssertionError(
            f"House detail layout resolved {len(detail_matches)} matches "
            "instead of one")
    detail_layout = detail_matches[0]
    native_move_matches = [
        candidate_rva for candidate_rva, pattern in NATIVE_MOVE_CANDIDATES
        if matches_at(text, candidate_rva - text_rva, pattern)
    ]
    if len(native_move_matches) != 1:
        raise AssertionError(
            f"native House move resolved {len(native_move_matches)} matches "
            "instead of one")
    native_move = native_move_matches[0]
    print(f"PASS: resolved {len(resolved)} unique runtime UI addresses")
    print(f"scene-ready update RVA: 0x{scene_ready:X}")
    print(f"button activate RVA: 0x{resolved['MEW_RVA_BUTTON_ACTIVATE']:X}")
    print(f"button can-activate RVA: 0x{resolved['MEW_RVA_BUTTON_CAN_ACTIVATE']:X}")
    print(f"component bucket prepare RVA: 0x{component_bucket_prepare:X}")
    print(f"House detail open RVA: 0x{detail_layout[0]:X}")
    print(f"House detail target RVA: 0x{detail_layout[1]:X}")
    print(f"House drawer resolver RVA: 0x{detail_layout[2]:X}")
    print(f"native House move RVA: 0x{native_move:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
