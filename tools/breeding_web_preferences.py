"""Persist external workbench assist choices across browser and server restarts."""

import json
from pathlib import Path


DEFAULT_ASSISTS = {"assist": True, "food_assist": True}


def load_assists(path):
    path = Path(path)
    values = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    return {key: values.get(key, value) for key, value in DEFAULT_ASSISTS.items()}


def save_assists(path, values):
    selected = {key: values[key] for key in DEFAULT_ASSISTS}
    if any(type(value) is not bool for value in selected.values()):
        raise ValueError("辅助开关必须为布尔值")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(selected, ensure_ascii=False, indent=2), encoding="utf-8")
    return selected
