"""Preview and apply a cultivated save while Mewgenics is closed."""

import ctypes
from contextlib import closing
from ctypes import wintypes
import json
import os
from pathlib import Path
import sqlite3
import tempfile
import time

from breeding_save import copy_database, read_snapshot


def game_is_running():
    class ProcessEntry(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260),
        ]

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
    kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.CreateToolhelp32Snapshot(2, 0)
    if handle == ctypes.c_void_p(-1).value:
        raise OSError(ctypes.get_last_error(), "无法检查游戏进程，请稍后重试")
    try:
        entry = ProcessEntry()
        entry.dwSize = ctypes.sizeof(entry)
        available = kernel.Process32FirstW(handle, ctypes.byref(entry))
        if not available:
            raise OSError(ctypes.get_last_error(), "无法读取游戏进程列表")
        while available:
            if entry.szExeFile.casefold() == "mewgenics.exe":
                return True
            available = kernel.Process32NextW(handle, ctypes.byref(entry))
        return False
    finally:
        kernel.CloseHandle(handle)


def require_game_closed():
    if game_is_running():
        raise ValueError("请先保存并完全退出 Mewgenics，再进行模拟或确认使用结果")


def save_stamp(path):
    info = Path(path).stat()
    return [info.st_size, info.st_mtime_ns]


def result_preview(source, original, exported, game):
    trained = read_snapshot(exported["file"], Path(game) / "resources.gpak")
    added = set(exported["imported_ids"])
    removed = set(exported["removed_population_ids"])
    return {
        "target": str(Path(source).resolve()),
        "campaign_day": exported["campaign_day"],
        "before_population": len(original["cats"]),
        "after_population": len(trained["cats"]),
        "added": [cat for cat in trained["cats"] if cat["id"] in added],
        "removed": [cat for cat in original["cats"] if cat["id"] in removed],
    }


def apply_result(job):
    require_game_closed()
    if job.get("applied"):
        return job["applied"]
    if job.get("status") not in ("completed", "cancelled") or not job.get("export"):
        raise ValueError("请先完成培养并查看可使用的结果")
    if not job.get("preview"):
        raise ValueError("结果预览未完成，不能写回存档")
    source = Path(job["source"])
    if save_stamp(source) != job["source_stamp"]:
        raise ValueError("原存档已在模拟后发生变化，请重新读取并培养，避免覆盖新的进度")
    trained = Path(job["export"]["file"])
    with closing(sqlite3.connect(trained.resolve().as_uri() + "?mode=ro", uri=True)) as db:
        # Read the actual game save tables before touching the target.
        db.execute("SELECT key FROM cats LIMIT 1").fetchall()
        db.execute("SELECT data FROM files WHERE key='house_state'").fetchone()
    folder = Path(job["report"]).parent
    backup = folder / "before-apply.sav"
    if not backup.exists():
        copy_database(source, backup)
    application = {
        "target": str(source), "backup": str(backup),
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "imported_ids": job["export"]["imported_ids"],
        "removed_population_ids": job["export"]["removed_population_ids"],
    }
    journal = folder / "application.json"
    journal.write_text(json.dumps({**application, "status": "prepared"},
                                 ensure_ascii=False, indent=2), encoding="utf-8")
    descriptor, temporary = tempfile.mkstemp(prefix=".autocattery-", suffix=".tmp", dir=source.parent)
    os.close(descriptor)
    try:
        copy_database(trained, temporary)
        require_game_closed()
        if save_stamp(source) != job["source_stamp"]:
            raise ValueError("原存档已变化，本次未写回；请重新培养")
        os.replace(temporary, source)
    finally:
        Path(temporary).unlink(missing_ok=True)
    application["status"] = "applied"
    # Preserve the successful write state even if saving the audit file fails.
    job["applied"] = application
    try:
        journal.write_text(json.dumps(application, ensure_ascii=False, indent=2), encoding="utf-8")
        report = Path(job["report"])
        values = json.loads(report.read_text(encoding="utf-8"))
        values["application"] = application
        report.write_text(json.dumps(values, ensure_ascii=False, indent=2), encoding="utf-8")
    except OSError as error:
        application["audit_error"] = str(error)
    return application
