"""Check private House clips before native UI attachment, including AS3 stop."""

from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_house_ui_asset import placed_character_id, read_tags, tag_stream_start
from swf_frame_scripts import Abc, Reader
from swf_panel_layout import (
    PANEL_BACKGROUND_NAME,
    PANEL_COMPACT_ELEMENTS,
    PANEL_PROTECTION_ELEMENTS,
    PANEL_SETTING_ELEMENTS,
    PANEL_TEXT_SUFFIX,
    PANEL_TEXTS,
)


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def check_button_setup_order(repo_root):
    for relative_path in (
            "src/ui/mew_ui_house_button_view.cpp",
            "src/ui/mew_ui_recommendation_marker_view.cpp"):
        source = (repo_root / relative_path).read_text(encoding="utf-8")
        show_frame = source.index(
            "HoldMewUiMovieClipFrame(button_node, 1)")
        native_setup = source.index("MewUI_SetupButtonFromNode")
        check(show_frame < native_setup,
              f"{relative_path} must materialize frame 1 before native setup")


def panel_instance_names():
    controls = tuple(name for name, *_ in
                     PANEL_PROTECTION_ELEMENTS + PANEL_COMPACT_ELEMENTS)
    text_nodes = tuple(f"{name}{PANEL_TEXT_SUFFIX}" for name in controls)
    return ((PANEL_BACKGROUND_NAME,) + controls + text_nodes +
            tuple(name for name, _ in PANEL_TEXTS))


def check_panel_lookup_names(repo_root):
    header = (repo_root / "src/ui/mew_ui_management_panel_view.hpp").read_text(
        encoding="utf-8")
    capacity = re.search(r"std::array<Element, (\d+)> setting_nodes_", header)
    check(capacity and int(capacity.group(1)) == len(PANEL_SETTING_ELEMENTS),
          "native setting capacity must match generated controls")
    source = (repo_root / "src/ui/mew_ui_management_panel_view.cpp").read_text(
        encoding="utf-8")
    expected_literals = (
        PANEL_BACKGROUND_NAME,
        PANEL_TEXT_SUFFIX,
        *(name for name, *_ in PANEL_COMPACT_ELEMENTS[:8]),
        *(name for name, _ in PANEL_TEXTS),
        "ac_prot_",
        "ac_group_",
        "ac_set_",
    )
    for name in expected_literals:
        check(f'"{name}"' in source,
              f"native panel lookup is missing generated name {name}")


def check_asset(path):
    data = Path(path).read_bytes()
    tags = [(code, data[start:end]) for code, _, start, end in
            read_tags(data, tag_stream_start(data), len(data))]
    scripts = [body for code, body in tags if code == 82]
    check(len(scripts) == 1, "the game supports exactly one DoABC block")
    body = scripts[0]
    abc = Abc(body[body.index(b"\0", 4) + 1:])
    classes = {}
    for class_id, instance in enumerate(abc.instances):
        qname = abc.multinames[instance[0]]
        package = abc.strings[abc.namespaces[qname[1]][1]]
        classes[package + b"." + abc.name(instance[0])] = class_id, instance

    bindings = {}
    for code, body in tags:
        if code != 76:
            continue
        count = struct.unpack_from("<H", body)[0]
        pos = 2
        for _ in range(count):
            character_id = struct.unpack_from("<H", body, pos)[0]
            end = body.index(b"\0", pos + 2)
            name = body[pos + 2:end]
            check(name not in bindings.values(), "class bindings must be unique")
            bindings[character_id] = name
            pos = end + 1
        check(pos == len(body), "symbol table length")

    sprites = {struct.unpack_from("<H", body)[0]: body
               for code, body in tags if code == 39}
    panel_names = panel_instance_names()
    check(len(panel_names) == len(set(panel_names)),
          "panel instance names must be unique")
    for name in panel_names:
        check(len(name.encode("ascii")) <= 15,
              f"panel instance name exceeds inline-string capacity: {name}")

    background_marker = PANEL_BACKGROUND_NAME.encode() + b"\0"
    overlay = next(body for body in sprites.values()
                   if background_marker in body)
    check(b"test_button\0" in overlay and b"recommend_button\0" in overlay,
          "buttons and panel must share the MOD-owned overlay root")
    button_ids = {placed_character_id(overlay[start:end])
                  for code, _, start, end in read_tags(overlay, 4, len(overlay))
                  if code == 26 and (b"test_button\0" in overlay[start:end] or
                                     b"recommend_button\0" in overlay[start:end])}
    check(len(button_ids) == 1, "both normal buttons must share one hidden asset")
    button_id = next(iter(button_ids))
    check(bindings.get(button_id) == b"house_fla.AutoCatteryHouseButton",
          "normal buttons need their own AS3 first-frame stop")
    button_frames = [[]]
    for tag, _, start, end in read_tags(sprites[button_id], 4,
                                       len(sprites[button_id])):
        if tag == 1:
            button_frames.append([])
        elif tag != 0:
            button_frames[-1].append((tag, sprites[button_id][start:end]))
    check(not button_frames[0] and button_frames[1],
          "normal buttons must stay hidden until native attachment")
    private_ids = set()
    found_panel_names = set()
    panel_markers = {name: name.encode() + b"\0" for name in panel_names}
    for code, _, start, end in read_tags(overlay, 4, len(overlay)):
        body = overlay[start:end]
        if code != 26:
            continue
        placed_panel_names = {name for name, marker in panel_markers.items()
                              if marker in body}
        if placed_panel_names:
            found_panel_names.update(placed_panel_names)
        if placed_panel_names or b"recommend_row_" in body:
            character_id = struct.unpack_from("<H", body, 3)[0]
            if character_id in sprites:
                private_ids.add(character_id)
    check(found_panel_names == set(panel_names),
          "all panel artwork and text placements must remain discoverable")
    check({"ac_set_49", "ac_set_49_t"} <= found_panel_names,
          "breeding population needs both a visible control and text node")
    check(len(private_ids) == 3, "panel background, controls and recommendation rows")

    for character_id in private_ids:
        check(character_id in bindings,
              f"sprite {character_id} has no AS3 stop binding (AVM1 is ignored)")
        class_id, instance = classes[bindings[character_id]]
        name, _, initializer, traits, *_ = instance
        registration = next((init for init, t in abc.scripts
                             if t == [(name, 4, 0, class_id)]), None)
        check(registration is not None, "private class must be registered")
        code = Reader(abc.bodies[registration][1])
        registered = []
        while code.pos < len(code.data):
            op = code.byte()
            if op in (0x5D, 0x60, 0x58, 0x68):
                operand = code.uint()
                if op != 0x60:
                    registered.append((op, operand))
            else:
                check(op in (0xD0, 0x30, 0x1D, 0x47), "registration opcode")
        check(registered == [(0x5D, name), (0x58, class_id), (0x68, name)],
              "registration must construct its own class, not the source clip")
        check(len(traits) == 1 and abc.name(traits[0][0]) == b"frame1",
              "no inherited UI fields or unrelated frame scripts")
        code = Reader(abc.bodies[initializer][1])
        check(code.take(6) == b"\xd0\x30\xd0\x49\x00\x5d", "constructor prefix")
        add_script = code.uint()
        check(abc.strings[abc.multinames[add_script][1]] == b"addFrameScript",
              "constructor must install the frame script")
        check(code.take(4) == b"\x24\x00\xd0\x66", "register on initial frame 0")
        frame_method = code.uint()
        check(abc.strings[abc.multinames[frame_method][1]] == b"frame1",
              "constructor must reference the bound frame1 method")
        check(code.byte() == 0x4F and code.uint() == add_script and
              code.take(2) == b"\x02\x47", "register exactly one frame callback")

        code = Reader(abc.bodies[traits[0][3]][1])
        check(code.take(3) == b"\xd0\x30\x5d", "frame script prefix")
        stop = code.uint()
        check(abc.strings[abc.multinames[stop][1]] == b"stop", "frame1 calls stop")
        check(code.byte() == 0x4F and code.uint() == stop and
              code.take(2) == b"\x00\x47", "frame1 only stops playback")

        sprite = sprites[character_id]
        frames = [[]]
        for tag, _, start, end in read_tags(sprite, 4, len(sprite)):
            if tag == 1:
                frames.append([])
            elif tag != 0:
                frames[-1].append((tag, sprite[start:end]))
        check(len(frames) == 4 and not frames[0], "initial frame must be empty")
        check(all(any(tag == 26 for tag, _ in frame) for frame in frames[1:3]),
              "normal and selected artwork frames must remain available")
        # Structural contract only; actual game rendering is player-verified.
        for requested in (1, 2, 0):
            check(bool(frames[requested]) == (requested != 0), "F10 show/hide frames")

    repo_root = Path(__file__).resolve().parents[1]
    check_button_setup_order(repo_root)
    check_panel_lookup_names(repo_root)
    print(f"House UI asset: {len(panel_names)} short panel instances, 4 "
          "unique AS3 first-frame stops; buttons stay hidden, then "
          "materialize before native setup.")


if __name__ == "__main__":
    check_asset(sys.argv[1])
