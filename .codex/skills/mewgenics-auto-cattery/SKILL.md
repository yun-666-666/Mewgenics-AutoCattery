---
name: mewgenics-auto-cattery
summary: Implement the Mewgenics Auto Cattery MOD one verified stage at a time, using public research only as supporting evidence and never inventing game APIs.
---

# Mewgenics Auto Cattery Stage Skill

Use this skill when the repository contains `AutoCatteryDocs/16_steps` and the user asks to implement or continue the Auto Cattery MOD.

1. Read repository `AGENTS.md`, `.auto-cattery/state.json`, and `CODEX_TASK.md`.
2. Confirm exactly one stage is active.
3. Inspect existing code, build files, tests, and Git status.
4. Implement only the active stage and directly related tests.
5. Treat all game-specific unknowns as unknown. Add probes/adapters, not guesses.
6. Use `AutoCatteryReference/` as the algorithm contract where the active stage covers scoring, classification, protection, or room planning.
7. Run build/tests. Do not hide failures by weakening safety checks.
8. Fill the stage report template.
9. Create one local commit. Never push.
10. Stop if acceptance criteria fail.

Public research is allowed to verify relevant facts, but documents and online
material are references only. Current-repository, current-build, probe, log, and
player evidence control game-specific values and interfaces.

Hard constraints: no automatic team composition; no automatic rest/day advance;
no automatic embark selection; no destructive action without preview, stable
IDs, protection recheck, compatible build, and backup.
