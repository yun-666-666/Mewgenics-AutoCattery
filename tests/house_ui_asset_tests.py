"""Check private House clips before native UI attachment, including AS3 stop."""

from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_house_ui_asset import read_tags, tag_stream_start
from swf_frame_scripts import Abc, Reader


def check(condition, message):
    if not condition:
        raise AssertionError(message)


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
    overlay = next(body for body in sprites.values()
                   if b"panel_background\0" in body)
    private_ids = set()
    panel_nodes = 0
    for code, _, start, end in read_tags(overlay, 4, len(overlay)):
        body = overlay[start:end]
        if code == 26 and any(prefix in body for prefix in
                              (b"panel_", b"recommend_row_")):
            character_id = struct.unpack_from("<H", body, 3)[0]
            if character_id in sprites:
                private_ids.add(character_id)
                panel_nodes += int(b"panel_" in body)
    check(panel_nodes == 72, "all 72 panel artwork placements remain discoverable")
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

    print("House UI asset: 72 panel nodes, 3 unique AS3 first-frame stops; "
          "pre-attach hidden timeline and show/hide contract passed.")


if __name__ == "__main__":
    check_asset(sys.argv[1])
