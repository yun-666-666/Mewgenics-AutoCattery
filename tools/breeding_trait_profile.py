"""从当前游戏资源生成通用特性价值；不读取猫 ID，不修改游戏资源或存档。

这是可调整的综合效果评分，不是所有职业/装备组合的绝对强度榜。
遗传以一级特性计价；升级等级、装备、后天属性不当成遗传收益。
"""

import argparse
import ast
import json
import math
from pathlib import Path

from breeding_resources import read_resources

STATS = ("str", "dex", "con", "int", "spd", "cha", "lck")
# Scoring preferences, not native probabilities or undocumented game constants.
EFFECT_VALUES = {
    "TakeExtraTurn": 8, "DoubleCastWeapons": 6, "DoubleCastSpells": 6,
    "AllStatsUp": 7, "AddMovement": 2, "AddActPoints": 4,
    "AddMana": 1, "HealthRegenUp": 2, "ManaRegenUp": 2,
    "DodgeChance": 6, "CritChanceUp": 4, "AddCritMultiplier": 2,
    "BoostHeals": 2, "AddBonusRange": 2, "Thorns": 1,
    "Brace": 1, "WaterWalk": 1, "Flying": 3,
    "Stun": 3, "Freeze": 3, "Fear": 2, "Confusion": 2,
    "Poison": 1, "Bleed": 1, "Burn": 1, "Weakness": 1,
    "Blind": 2, "Heal": 1, "Shield": 1,
    "Bruise": -2, "MissChance": -4, "PermanentKitten": -8,
}


def number(value):
    """Only evaluate arithmetic with a level-one, unequipped reference.

    Unknown expressions remain unvalued; never execute resource code.
    """
    if type(value) in (int, float):
        return float(value) if math.isfinite(value) else None
    if not isinstance(value, str):
        return None
    try:
        tree = ast.parse(value, mode="eval")

        def evaluate(node):
            if isinstance(node, ast.Constant) and type(node.value) in (int, float):
                return float(node.value)
            if isinstance(node, ast.Name):
                if node.id == "level":
                    return 1.0
                if node.id.startswith("bonus_"):
                    return 0.0
            if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
                return (-1 if isinstance(node.op, ast.USub) else 1) * evaluate(node.operand)
            if isinstance(node, ast.BinOp):
                a, b = evaluate(node.left), evaluate(node.right)
                if isinstance(node.op, ast.Add): return a + b
                if isinstance(node.op, ast.Sub): return a - b
                if isinstance(node.op, ast.Mult): return a * b
                if isinstance(node.op, ast.Div): return a / b
            raise ValueError("unvalued expression")

        result = evaluate(tree.body)
        return result if math.isfinite(result) else None
    except (ValueError, SyntaxError, ZeroDivisionError, OverflowError, TypeError):
        return None


def effects_value(effects):
    value = 0.0
    for key, item in effects.items():
        if key in EFFECT_VALUES:
            if isinstance(item, list) and len(item) == 2:
                amount, probability = number(item[0]), number(item[1])
                amount = None if amount is None or probability is None else amount * probability
            else:
                amount = number(item)
            if amount is not None:
                value += EFFECT_VALUES[key] * amount
        # Structured/conditional effects need a build-specific interpretation.
        # They are not counted again as if always active.
    return value


def passive_value(definition):
    body = definition.get("1", definition)
    return sum(number(body.get("stats", {}).get(stat, 0)) or 0 for stat in STATS) + \
        effects_value(body.get("passives", {}))


def merge(base, override):
    result = dict(base) if isinstance(base, dict) else {}
    for key, value in override.items():
        result[key] = merge(result.get(key, {}), value) if isinstance(value, dict) else value
    return result


def build_profile(gpak):
    resources = read_resources(gpak, prefixes=("data/abilities/", "data/passives/",
                                               "data/mutations/", "data/ability_templates/"))
    abilities, passives, templates = {}, {}, {}
    for filename, definitions in resources.items():
        target = (abilities if filename.startswith("data/abilities/") else
                  passives if filename.startswith("data/passives/") else
                  templates if filename.startswith("data/ability_templates/") else None)
        if target is not None:
            target.update(definitions)

    def resolve(name, visited=()):
        if name in visited or name not in abilities:
            return {}
        definition = abilities[name]
        base = resolve(definition.get("variant_of"), (*visited, name))
        template = templates.get(definition.get("template"), {})
        return merge(merge(template, base), definition)

    active = {}
    for name in abilities:
        body = resolve(name)
        damage = body.get("damage_instance", {})
        target = body.get("target", {})
        cost = body.get("cost", {})
        base_damage = number(damage.get("damage", 0)) or 0
        # Explicit friendly healing and enemy damage have positive utility;
        # self harm is subtracted. Unknown scripted effects keep a baseline.
        effect = effects_value(damage.get("effects", {}))
        aoe = number(target.get("max_aoe", 0)) or 0
        breadth = 1.0 + min(3.0, max(0.0, aoe)) * 0.25
        chance = number(target.get("aoe_chance", 1))
        if chance is not None:
            breadth *= max(0.0, min(1.0, chance))
        reach = number(target.get("max_range", 1)) or 1
        mana = max(0.0, number(cost.get("mana", 0)) or 0)
        self_harm = max(0.0, number(body.get("self_damage", {}).get("damage", 0)) or 0)
        value = (1 + (abs(base_damage) + effect) * breadth +
                 min(6.0, max(0.0, reach - 1)) * 0.15 - self_harm * 0.5) / (1 + mana * 0.08)
        active[name] = round(max(-20.0, min(30.0, value)), 6)
    active["None"] = 0.0
    passive = {name: round(1 + passive_value(body), 6) for name, body in passives.items()
               if isinstance(body, dict) and body.get("class") != "Disorder"}
    # SkillShare is noncopyable; native checks its level-two partner transfer
    # separately. Do not promise ordinary passive inheritance for it.
    passive["SkillShare"] = 0.0
    disorders = {name: round(max(0.0, 1 - passive_value(body)), 6)
                 for name, body in passives.items() if body.get("class") == "Disorder"}
    mutations, defects = {}, {}
    for filename, groups in resources.items():
        if not filename.startswith("data/mutations/"):
            continue
        for category, parts in groups.items():
            if not isinstance(parts, dict):
                continue
            for identity, definition in parts.items():
                try:
                    part_id = int(identity)
                except ValueError:
                    continue
                if not isinstance(definition, dict):
                    continue
                value = sum(number(definition.get(stat, 0)) or 0 for stat in STATS)
                value += effects_value(definition.get("passives", {}))
                key = f"{category}:{part_id & 0xffffffff}"
                if definition.get("tag") == "birth_defect" or part_id == -2:
                    defects[key] = round(max(0.0, -value), 6)
                elif part_id >= 300:
                    mutations[key] = round(value, 6)
    return {"active_ability_overrides": active, "passive_overrides": passive,
            "disorder_overrides": disorders, "mutation_overrides": mutations,
            "birth_defect_overrides": defects}


def pair_trait_score(a, b, profile, stimulation=0):
    """Same slot means/group inheritance proxy as the MOD pair trait scorer."""
    def mean(cat, start, end, key, fallback, exclude_share=False):
        values = [profile[key].get(name, fallback)
                  for name in cat["ability_slots"][start:end]
                  if name and name != "None" and not (exclude_share and name == "SkillShare")]
        return sum(values) / len(values) if values else 0.0

    def chance(base, scale):
        return max(0.0, min(1.0, base + scale * max(0.0, stimulation)))

    score = .5 * (mean(a, 2, 6, "active_ability_overrides", 1) +
                   mean(b, 2, 6, "active_ability_overrides", 1)) * \
        (chance(.2, .025) + chance(.02, .005))
    score += .5 * (mean(a, 6, 8, "passive_overrides", 1, True) +
                    mean(b, 6, 8, "passive_overrides", 1, True)) * chance(.05, .01)
    score -= .15 * (mean(a, 8, 10, "disorder_overrides", 1) +
                     mean(b, 8, 10, "disorder_overrides", 1))
    preferred = (1 + .01 * max(0.0, stimulation)) / (2 + .01 * max(0.0, stimulation))

    def groups(cat):
        result = {}
        for part in cat.get("visual_parts", []):
            key = f"{part['category']}:{part['id']}"
            if key in profile["birth_defect_overrides"]:
                value = -profile["birth_defect_overrides"][key]
            elif key in profile["mutation_overrides"]:
                value = profile["mutation_overrides"][key]
            else:
                continue
            slot = part["slot"]
            group = {"leg": "legs", "arm": "arms", "eye": "eyes",
                     "eyebrow": "eyebrows", "ear": "ears"}.get(slot.split("_")[0], slot)
            result.setdefault(group, []).append(value)
        return {key: sum(values) / len(values) for key, values in result.items()}

    av, bv = groups(a), groups(b)
    for group in av.keys() | bv.keys():
        score += (.5 * (av[group] + bv[group]) if group in av and group in bv else
                  preferred * (av.get(group, 0) + bv.get(group, 0)))
    return score


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True,
                        help="输出到此默认配置；勿指定 user_config.json")
    args = parser.parse_args()
    if args.config.name == "user_config.json":
        parser.error("请生成到 default_config.json，保留用户权重覆盖")
    config = json.loads(args.config.read_text(encoding="utf-8-sig"))
    profile = build_profile(args.game / "resources.gpak")
    config["breeding_scoring"].update(profile)
    for key in ("active_ability_overrides", "passive_overrides", "disorder_overrides"):
        config["combat_scoring"][key] = profile[key]
    args.config.write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Generated effect-based trait profile:", {k: len(v) for k, v in profile.items()})


if __name__ == "__main__":
    main()
