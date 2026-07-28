#!/usr/bin/env python3
"""Deterministic AutoCattery reference planner.

This is an algorithm oracle and fixture runner. It never reads game memory,
modifies a save, moves a real cat, or performs culling.
"""
from __future__ import annotations
import argparse, hashlib, json, math, sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

STATS = ("strength", "dexterity", "constitution", "intelligence", "speed", "luck")
ALGORITHM_VERSION = "autocattery-reference-1.0.0"
PROTECTION_ORDER = {"none":0,"no_cull":1,"no_move":2,"no_cull_or_move":3,"fully_unmanaged":4}

class InputError(ValueError): pass

def finite_number(value: Any, field: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(float(value)):
        raise InputError(f"{field} must be a finite number")
    return float(value)

def tri(value: Any) -> bool | None:
    return value if isinstance(value, bool) else None

def stable_hash(obj: Any) -> str:
    raw = json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()

def validate_house(house: dict[str, Any]) -> None:
    ids = [c.get("id") for c in house.get("cats", [])]
    room_ids = [r.get("id") for r in house.get("rooms", [])]
    if len(ids) != len(set(ids)) or any(not isinstance(x, int) or x <= 0 for x in ids):
        raise InputError("cat IDs must be unique positive integers")
    if len(room_ids) != len(set(room_ids)) or any(not isinstance(x, int) or x <= 0 for x in room_ids):
        raise InputError("room IDs must be unique positive integers")
    known = set(ids)
    seen: set[int] = set()
    for room in house.get("rooms", []):
        for cid in room.get("residents", []):
            if cid not in known: raise InputError(f"room {room['id']} references missing cat {cid}")
            if cid in seen: raise InputError(f"cat {cid} appears in multiple rooms")
            seen.add(cid)

def effective_protection(cat: dict[str, Any]) -> str:
    configured = cat.get("protection", "none")
    if configured not in PROTECTION_ORDER:
        return "fully_unmanaged"
    if configured == "fully_unmanaged":
        return configured
    no_cull = configured in ("no_cull", "no_cull_or_move")
    no_move = configured in ("no_move", "no_cull_or_move")
    if tri(cat.get("game_locked")) is True:
        no_cull = True
        no_move = True
    elif tri(cat.get("player_favorite")) is True:
        no_cull = True
    if no_cull and no_move:
        return "no_cull_or_move"
    if no_cull:
        return "no_cull"
    if no_move:
        return "no_move"
    return "none"

def score_stats(cat: dict[str, Any], weights: dict[str, Any], missing_penalty: float):
    total=0.0; missing=0; components=[]
    stats=cat.get("base_stats") or {}
    for key in STATS:
        weight=finite_number(weights.get(key,1.0), f"stat_weights.{key}")
        value=stats.get(key)
        if value is None:
            missing += 1; contribution=-missing_penalty
            components.append({"key":key,"raw_value":None,"weight":weight,"contribution":contribution,"explanation":"missing field penalty"})
        else:
            raw=finite_number(value, f"cat[{cat.get('id')}].base_stats.{key}")
            contribution=raw*weight
            components.append({"key":key,"raw_value":raw,"weight":weight,"contribution":contribution,"explanation":"base stat"})
        total += contribution
    return total, missing, components

def combat_score(cat: dict[str, Any], cfg: dict[str, Any]) -> dict[str, Any]:
    reasons=[]
    life=cat.get("life_stage","unknown")
    if life == "dead": reasons.append("dead")
    if cfg.get("exclude_kittens",True) and life == "kitten": reasons.append("kitten")
    if tri(cat.get("available_for_combat")) is False: reasons.append("not available for combat")
    if cfg.get("exclude_injured",False) and tri(cat.get("injured")) is True: reasons.append("injured")
    miss_pen=finite_number(cfg.get("missing_field_penalty",0.25),"combat.missing_field_penalty")
    total,missing,parts=score_stats(cat,cfg.get("stat_weights",{}),miss_pen)
    def add_items(field, default_key, overrides_key, sign=1.0):
        nonlocal total
        default=finite_number(cfg.get(default_key,0.0), f"combat.{default_key}")
        overrides=cfg.get(overrides_key,{}) or {}
        for item in cat.get(field,[]) or []:
            weight=finite_number(overrides.get(item,default),f"combat.{overrides_key}.{item}")*sign
            total += weight
            parts.append({"key":f"{field}:{item}","raw_value":1,"weight":weight,"contribution":weight,"explanation":field})
    add_items("abilities","ability_count_weight","ability_overrides")
    add_items("passives","passive_count_weight","passive_overrides")
    add_items("mutations","mutation_default_weight","mutation_overrides")
    add_items("disorders","disorder_default_penalty","disorder_overrides",-1.0)
    if tri(cat.get("injured")) is True:
        p=finite_number(cfg.get("injury_penalty",5.0),"combat.injury_penalty"); total-=p
        parts.append({"key":"injury","raw_value":1,"weight":-p,"contribution":-p,"explanation":"injury penalty"})
    observed=6-missing
    confidence=max(0.0,min(1.0,observed/6.0))
    eligible=not reasons and total >= finite_number(cfg.get("minimum_score",0.0),"combat.minimum_score")
    return {"cat_id":cat["id"],"eligible":eligible,"score":round(total,6),"confidence":round(confidence,6),"exclusion_reasons":reasons,"components":parts}

def breeding_score(cat: dict[str, Any], cfg: dict[str, Any]) -> dict[str, Any]:
    reasons=[]
    if cat.get("life_stage") in ("dead","kitten"): reasons.append(cat.get("life_stage"))
    if tri(cat.get("available_for_breeding")) is False: reasons.append("not available for breeding")
    miss_pen=finite_number(cfg.get("missing_field_penalty",0.25),"breeding.missing_field_penalty")
    total,missing,parts=score_stats(cat,cfg.get("stat_weights",{}),miss_pen)
    ability_default=finite_number(cfg.get("ability_default_weight",1.0),"breeding.ability_default_weight")
    mutation_default=finite_number(cfg.get("mutation_default_weight",1.0),"breeding.mutation_default_weight")
    disorder_default=finite_number(cfg.get("disorder_default_penalty",3.0),"breeding.disorder_default_penalty")
    rare=cfg.get("rare_mutation_weights",{}) or {}
    for item in cat.get("abilities",[]) or []:
        total += ability_default; parts.append({"key":f"ability:{item}","raw_value":1,"weight":ability_default,"contribution":ability_default,"explanation":"inheritance value"})
    for item in cat.get("mutations",[]) or []:
        w=finite_number(rare.get(item,mutation_default),f"breeding.rare_mutation_weights.{item}")
        total += w; parts.append({"key":f"mutation:{item}","raw_value":1,"weight":w,"contribution":w,"explanation":"gene value"})
    for item in cat.get("disorders",[]) or []:
        total -= disorder_default; parts.append({"key":f"disorder:{item}","raw_value":1,"weight":-disorder_default,"contribution":-disorder_default,"explanation":"disorder risk"})
    confidence=max(0.0,min(1.0,(6-missing)/6.0))
    return {"cat_id":cat["id"],"eligible":not reasons,"score":round(total,6),"confidence":round(confidence,6),"exclusion_reasons":reasons,"components":parts}

def rank(results: list[dict[str,Any]], cats_by_id: dict[int,dict[str,Any]]) -> list[dict[str,Any]]:
    def stat_sum(cid:int):
        s=cats_by_id[cid].get("base_stats") or {}; return sum(v for v in (s.get(k) for k in STATS) if isinstance(v,(int,float)) and not isinstance(v,bool))
    return sorted(results,key=lambda r:(not r["eligible"],-r["score"],-r["confidence"],-stat_sum(r["cat_id"]),r["cat_id"]))

def classify(house:dict[str,Any], cfg:dict[str,Any]):
    cats_by_id={c["id"]:c for c in house["cats"]}
    combat=rank([combat_score(c,cfg["combat_scoring"]) for c in house["cats"]],cats_by_id)
    breed=rank([breeding_score(c,cfg["breeding_scoring"]) for c in house["cats"]],cats_by_id)
    ccfg=cfg["classification"]
    combat_eligible=[r for r in combat if r["eligible"]]
    breed_eligible=[r for r in breed if r["eligible"]]
    recommended_count=int(cfg["combat_scoring"].get("recommended_count",8))
    combat_recommended={r["cat_id"] for r in combat_eligible[:recommended_count]}
    combat_pool={r["cat_id"] for r in combat_eligible[:max(recommended_count,int(ccfg.get("minimum_combat_pool",0)))]}
    core_n=int(cfg["breeding_scoring"].get("core_breeders",cfg["breeding_scoring"].get("core_breeders_per_sex_or_role",4)))
    reserve_n=int(cfg["breeding_scoring"].get("reserve_breeders",4))
    breeding_core={r["cat_id"] for r in breed_eligible[:core_n]}
    breeding_reserve={r["cat_id"] for r in breed_eligible[core_n:core_n+reserve_n]}
    breeding_pool={r["cat_id"] for r in breed_eligible[:max(core_n+reserve_n,int(ccfg.get("minimum_breeding_pool",0)))]}
    cs={r["cat_id"]:r for r in combat}; bs={r["cat_id"]:r for r in breed}
    decisions=[]; undecided=[]
    for c in house["cats"]:
        cid=c["id"]; prot=effective_protection(c)
        if prot == "fully_unmanaged": role="protected_unmanaged"
        elif not cs[cid]["eligible"] and not bs[cid]["eligible"]: role="ineligible"
        elif cid in combat_recommended and cid in breeding_core:
            role="combat_recommended" if ccfg.get("combat_priority_over_breeding",False) else "breeding_core"
        elif cid in breeding_core: role="breeding_core"
        elif cid in combat_recommended: role="combat_recommended"
        elif cid in breeding_reserve: role="breeding_reserve"
        else: role="unassigned"; undecided.append(cid)
        decisions.append({"cat_id":cid,"primary_role":role,"combat_score":cs[cid]["score"],"breeding_score":bs[cid]["score"],"confidence":round(min(cs[cid]["confidence"],bs[cid]["confidence"]),6),"protection":prot,"combat_recommended":cid in combat_recommended,"combat_pool_protected":cid in combat_pool,"breeding_core":cid in breeding_core,"breeding_reserve":cid in breeding_reserve,"breeding_pool_protected":cid in breeding_pool,"destructive_action_allowed":False,"reasons":[]})
    byid={d["cat_id"]:d for d in decisions}
    # Keep best remaining general reserves, deterministic.
    remaining=sorted(undecided,key=lambda cid:(-max(cs[cid]["score"],bs[cid]["score"]),cid))
    reserve=set(remaining[:int(ccfg.get("minimum_general_reserve",4))])
    threshold=finite_number(ccfg.get("never_cull_if_data_confidence_below",0.85),"classification.confidence")
    for cid in remaining:
        d=byid[cid]
        c=cats_by_id[cid]
        hard_protected=d["protection"] in ("no_cull","no_cull_or_move","fully_unmanaged") or d["combat_pool_protected"] or d["breeding_pool_protected"] or c.get("life_stage")=="kitten"
        if cid in reserve or hard_protected or d["confidence"] < threshold:
            d["primary_role"]="general_reserve"
            d["reasons"].append("minimum pool, protection, kitten, or confidence guard")
        else:
            d["primary_role"]="cull_candidate"; d["destructive_action_allowed"]=True; d["reasons"].append("outside protected combat/breeding/general pools")
    return combat,breed,decisions

def room_plan(house:dict[str,Any], decisions:list[dict[str,Any]], cfg:dict[str,Any]):
    rooms={r["id"]:dict(r) for r in house["rooms"]}; cats={c["id"]:c for c in house["cats"]}; dec={d["cat_id"]:d for d in decisions}
    rcfg=cfg["room_planning"]
    occupancy={rid:[] for rid in rooms}
    for c in house["cats"]:
        if c.get("room_id") in occupancy: occupancy[c["room_id"]].append(c["id"])
    def capacity(r):
        if isinstance(r.get("hard_capacity"),int): return r["hard_capacity"]
        soft=int(r.get("soft_capacity") or rcfg.get("default_soft_capacity",4))
        return soft + (int(rcfg.get("max_soft_overflow_per_room",0)) if rcfg.get("allow_soft_overflow",True) else 0)
    def can_move(cid): return dec[cid]["protection"] not in ("no_move","no_cull_or_move","fully_unmanaged")
    def role_penalty(cid,r):
        role=dec[cid]["primary_role"]; cat=cats[cid]
        if r.get("is_special_room") or r.get("player_locked"): return 10000
        if role=="breeding_core": return 0 if r.get("allows_breeding") else 30
        if dec[cid]["combat_recommended"]: return 0 if r.get("room_type")=="combat_staging" else 10
        if cat.get("life_stage")=="kitten": return 0 if r.get("allows_kittens",True) else 10000
        return 0 if r.get("room_type") in ("general","combat_staging") else 5
    # Unknown breeding compatibility must preserve an existing breeding-room placement.
    def preserve_unknown_breeder(cid: int) -> bool:
        cat = cats[cid]
        current = rooms.get(cat.get("room_id"))
        return (
            dec[cid]["primary_role"] == "breeding_core"
            and current is not None
            and current.get("allows_breeding")
            and not (cat.get("compatible_breeding_ids") or [])
        )
    movable=[
        c["id"] for c in house["cats"]
        if can_move(c["id"])
        and dec[c["id"]]["primary_role"] not in ("cull_candidate", "protected_unmanaged", "ineligible")
        and not preserve_unknown_breeder(c["id"])
    ]
    priority=lambda cid:({"breeding_core":0,"combat_recommended":1,"breeding_reserve":3,"general_reserve":4,"ineligible":5}.get(dec[cid]["primary_role"],4),cid)
    target={cid:cats[cid].get("room_id",0) for cid in cats}
    # Remove movable cats from occupancy so assignment is globally consistent.
    for cid in movable:
        old=target[cid]
        if old in occupancy and cid in occupancy[old]: occupancy[old].remove(cid)
    unplaced=[]
    for cid in sorted(movable,key=priority):
        old=target[cid]; options=[]
        for rid,r in rooms.items():
            if len(occupancy[rid]) >= capacity(r): continue
            rp=role_penalty(cid,r)
            if rp>=10000: continue
            if dec[cid]["primary_role"] == "breeding_core" and r.get("allows_breeding"):
                existing_breeders = [x for x in occupancy[rid] if dec.get(x, {}).get("breeding_core")]
                if existing_breeders:
                    compatible = set(cats[cid].get("compatible_breeding_ids") or [])
                    if not compatible or any(x not in compatible for x in existing_breeders):
                        continue
            move_pen=0 if rid==old else 1
            soft=int(r.get("soft_capacity") or rcfg.get("default_soft_capacity",4))
            overflow=max(0,len(occupancy[rid])+1-soft)
            options.append((rp,overflow,move_pen,len(occupancy[rid]),rid))
        if not options:
            unplaced.append(cid)
            if old in occupancy and len(occupancy[old]) < capacity(rooms[old]): occupancy[old].append(cid)
            continue
        rid=min(options)[-1]; target[cid]=rid; occupancy[rid].append(cid)
    moves=[]
    for cid,new in sorted(target.items()):
        old=cats[cid].get("room_id",0)
        if new!=old and cid not in unplaced:
            moves.append({"cat_id":cid,"from_room":old,"to_room":new,"reason":dec[cid]["primary_role"],"priority":priority(cid)[0]})
    culls=[d["cat_id"] for d in decisions if d["primary_role"]=="cull_candidate" and d["destructive_action_allowed"]]
    culls.sort(key=lambda cid:(max(dec[cid]["combat_score"],dec[cid]["breeding_score"]),cid))
    # If movable non-cull cats are unplaced, suggest the smallest number of culls needed.
    relief=culls[:len(unplaced)]
    warnings=[]
    if unplaced: warnings.append(f"capacity insufficient: {len(unplaced)} cat(s) remain unplaced")
    return moves,culls,relief,unplaced,warnings

def build_plan(house:dict[str,Any], cfg:dict[str,Any]) -> dict[str,Any]:
    validate_house(house)
    combat,breed,decisions=classify(house,cfg)
    moves,quality,relief,unplaced,warnings=room_plan(house,decisions,cfg)
    return {"schema_version":1,"source_snapshot_id":house["snapshot_id"],"algorithm_version":ALGORITHM_VERSION,"input_hash":stable_hash({"house":house,"config":cfg}),"combat_ranking":combat,"breeding_ranking":breed,"decisions":decisions,"moves":moves,"quality_cull_candidates":quality,"capacity_relief_candidates":relief,"unplaced_cats":unplaced,"warnings":warnings,"fully_satisfied":not unplaced}

def load_json(path:Path):
    try: return json.loads(path.read_text(encoding="utf-8"))
    except (OSError,json.JSONDecodeError) as e: raise InputError(f"cannot load {path}: {e}") from e

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest="cmd",required=True)
    q=sub.add_parser("plan",help="build a read-only organize plan")
    q.add_argument("--house",type=Path,required=True); q.add_argument("--config",type=Path,required=True); q.add_argument("--output",type=Path)
    q=sub.add_parser("validate",help="validate a fixture and print summary")
    q.add_argument("--house",type=Path,required=True); q.add_argument("--config",type=Path,required=True)
    a=p.parse_args(argv)
    try:
        plan=build_plan(load_json(a.house),load_json(a.config))
        if a.cmd=="validate":
            print(json.dumps({"ok":True,"source_snapshot_id":plan["source_snapshot_id"],"decisions":len(plan["decisions"]),"moves":len(plan["moves"]),"quality_culls":len(plan["quality_cull_candidates"]),"unplaced":len(plan["unplaced_cats"])},ensure_ascii=False,indent=2)); return 0
        raw=json.dumps(plan,ensure_ascii=False,indent=2)
        if a.output: a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(raw+"\n",encoding="utf-8")
        else: print(raw)
        return 0
    except InputError as e:
        print(f"ERROR: {e}",file=sys.stderr); return 2
if __name__=="__main__": raise SystemExit(main())
