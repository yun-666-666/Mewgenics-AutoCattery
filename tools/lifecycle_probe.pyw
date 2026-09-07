"""Read-only, manually triggered before/after probe for dead-cat NPC delivery."""

import datetime
import json
import os
from pathlib import Path
import sqlite3
import struct
import tkinter as tk
from tkinter import messagebox, ttk


def decode_cat(blob):
    expected = struct.unpack_from("<I", blob)[0]
    if expected == 19:
        return blob
    if not 20 <= expected <= 65536:
        raise ValueError("Unsupported cat storage size")
    src, out, cursor = blob[4:], bytearray(), 0

    def length(value):
        nonlocal cursor
        if value == 15:
            while True:
                extra = src[cursor]
                cursor += 1
                value += extra
                if extra != 255:
                    break
        return value

    while cursor < len(src):
        token = src[cursor]
        cursor += 1
        size = length(token >> 4)
        if cursor + size > len(src) or len(out) + size > expected:
            raise ValueError("Invalid literal bounds")
        out.extend(src[cursor:cursor + size])
        cursor += size
        if cursor == len(src):
            break
        offset = struct.unpack_from("<H", src, cursor)[0]
        cursor += 2
        size = length(token & 15) + 4
        if not 0 < offset <= len(out) or len(out) + size > expected:
            raise ValueError("Invalid match bounds")
        for _ in range(size):
            out.append(out[-offset])
    if len(out) != expected:
        raise ValueError("Decompressed length mismatch")
    return bytes(out)


def capture(path):
    # One SQLite read transaction keeps cats and NPC progress consistent.
    connection = sqlite3.connect(path.as_uri() + "?mode=ro", uri=True, timeout=2)
    try:
        connection.execute("PRAGMA query_only=ON")
        connection.execute("BEGIN")
        cats = {}
        for cat_id, blob in connection.execute("SELECT key, data FROM cats ORDER BY key"):
            raw = decode_cat(blob)
            if len(raw) < 135 or struct.unpack_from("<I", raw)[0] != 19:
                raise ValueError("Unsupported cat format")
            birth, death = struct.unpack_from("<qq", raw, len(raw) - 103)
            old_state = struct.unpack_from("<i", raw, len(raw) - 79)[0]
            cats[str(cat_id)] = {
                "birth_day": birth, "death_day": death,
                "old_state": old_state,
                "dead": death >= 0, "senior": death < 0 and old_state >= 2,
            }
        files = dict(connection.execute(
            "SELECT key, data FROM files WHERE key IN ('house_state','npc_progress')"))
        day = connection.execute(
            "SELECT data FROM properties WHERE key='current_day'").fetchone()
        raw_day = day[0] if day else None
        if isinstance(raw_day, bytes):
            raw_day = int.from_bytes(raw_day, "little", signed=True)
        return {"game_day": raw_day, "cats": cats, "files": files}
    finally:
        connection.close()


def differences(before, after):
    changes = []
    limit = max(len(before), len(after))
    index = 0
    while index < limit:
        if before[index:index + 1] == after[index:index + 1]:
            index += 1
            continue
        start = index
        while index < limit and index - start < 32 and (
                before[index:index + 1] != after[index:index + 1]):
            index += 1
        changes.append({"offset": start, "before_hex": before[start:index].hex(),
                        "after_hex": after[start:index].hex()})
    return {"before_size": len(before), "after_size": len(after), "changes": changes}


def compare(before, after):
    old, new = before["cats"], after["cats"]
    removed = sorted(set(old) - set(new), key=int)
    return {
        "schema_version": 1,
        "purpose": "manual_dead_cat_delivery_observation_only",
        "game_day_before": before["game_day"], "game_day_after": after["game_day"],
        "dead_cat_ids_before": [key for key, cat in old.items() if cat["dead"]],
        "dead_cat_ids_after": [key for key, cat in new.items() if cat["dead"]],
        "removed_cat_records": [{"cat_id": key, **old[key]} for key in removed],
        "added_cat_ids": sorted(set(new) - set(old), key=int),
        "changed_cat_life_states": [
            {"cat_id": key, "before": old[key], "after": new[key]}
            for key in old.keys() & new.keys() if old[key] != new[key]],
        "file_differences": {
            key: differences(before["files"].get(key, b""), after["files"].get(key, b""))
            for key in ("house_state", "npc_progress")},
        "limitations": [
            "Only persisted state is observed; save normally before each capture.",
            "This is not a native-call trace and does not establish an invocation signature.",
            "Local file differences may include game text; no data is uploaded.",
        ],
    }


def main():
    root = tk.Tk()
    root.title("AutoCattery · 死亡猫交付只读探针")
    root.geometry("830x420")
    save_root = Path(os.environ["APPDATA"]) / "Glaiel Games" / "Mewgenics"
    saves = sorted((p for p in save_root.rglob("*.sav")
                    if not any(part.lower() in ("backup", "backups") for part in p.parts)),
                   key=lambda p: p.stat().st_mtime, reverse=True)
    status = tk.StringVar(value="尚未记录。此工具不会操作游戏或修改存档。")
    ttk.Label(root, text=(
        "1. 选择正在玩的存档；在游戏中正常保存，然后记录交付前。\n"
        "2. 手动把一只已死亡的猫交给红框的 Organ Grinder，记下猫名和人物计数。\n"
        "3. 再次正常保存，记录交付后。两次之间不要睡觉、换档或交付其他猫。\n"
        "探针只读取已保存数据；不会自动交付，不会推进游戏，也不会上传报告。"
    ), padding=15).pack(anchor="w")
    selector = ttk.Combobox(root, state="readonly", width=102,
                           values=[str(p.relative_to(save_root)) for p in saves])
    selector.pack(padx=15, pady=10)
    if saves:
        selector.current(0)
    recorded = None
    selected_path = None

    def record_before():
        nonlocal recorded, selected_path
        try:
            if selector.current() < 0:
                raise ValueError("没有选择存档")
            path = saves[selector.current()]
            value = capture(path)
            dead = [key for key, cat in value["cats"].items() if cat["dead"]]
            if not dead:
                raise ValueError("此存档未读取到已死亡猫。请核对存档并先正常保存。")
            recorded, selected_path = value, path
            status.set(f"交付前已记录：死亡猫 {len(dead)} 只。现在请手动交付一只并保存。")
        except Exception as error:
            messagebox.showerror("读取失败", str(error), parent=root)

    def record_after():
        nonlocal recorded
        try:
            if recorded is None or selected_path is None:
                raise ValueError("请先记录交付前")
            if selector.current() < 0 or saves[selector.current()] != selected_path:
                raise ValueError("两次记录必须使用同一存档")
            value = capture(selected_path)
            report = compare(recorded, value)
            if value["game_day"] != recorded["game_day"]:
                raise ValueError("游戏日期改变，请重新开始单次交付测试")
            if not any(x["changes"] for x in report["file_differences"].values()):
                raise ValueError("未读到房屋或NPC进度变化。请确认交付成功并正常保存后重试。")
            folder = Path(__file__).resolve().parent / "lifecycle-probe-results"
            folder.mkdir(exist_ok=True)
            path = folder / (datetime.datetime.now().strftime("delivery-%Y%m%d-%H%M%S-%f") + ".json")
            path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            status.set("报告已保存：" + str(path))
            recorded = None
            messagebox.showinfo("记录完成", "请把此报告和交付前后人物计数发给 Codex：\n" + str(path), parent=root)
        except Exception as error:
            messagebox.showerror("记录失败", str(error), parent=root)

    ttk.Button(root, text="记录交付前（只读）", command=record_before).pack(pady=8)
    ttk.Button(root, text="记录交付后并生成报告", command=record_after).pack(pady=8)
    ttk.Label(root, textvariable=status, wraplength=790, padding=15).pack(anchor="w")
    root.mainloop()


if __name__ == "__main__":
    main()
