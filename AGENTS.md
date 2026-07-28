# AutoCattery Codex Repository Rules

## Mission
Implement the Mewgenics Auto Cattery MOD strictly from `AutoCatteryDocs/16_steps/`. This repository uses a staged delivery process. Do not redesign the product and do not perform web research.

## Non-negotiable behavior
- Read `CODEX_TASK.md` before modifying files.
- Implement only the current stage. Never pre-implement later stages.
- Do not browse the web, search for new mods, or change the fixed technical route.
- Inspect the current repository and reuse existing code.
- Never invent game function names, offsets, scene names, UI node names, IDs, save fields, or API signatures.
- Unknown game details must be isolated behind probes/adapters and safe failure paths.
- Do not modify original game files or commit binaries, saves, decompiled output, closed assets, secrets, or personal paths.
- Default to read-only when compatibility or data identity is uncertain.
- Any move/cull/write operation must be previewable, cancellable, audited, protected, and recoverable.
- Never implement automatic team composition. Combat cats are independently scored and highlighted only.
- Never implement automatic rest/day advance or automatic embark selection.
- Build and test before completion.
- Create one local Git commit per completed stage. Never push.
- If the stage acceptance criteria fail, stop and report the blocker.

## Architecture boundaries
`UI -> Application Services -> Domain/Scoring/Planning -> Game Adapters/Persistence`, with Safety/Journal cross-cutting.

- UI must not write game state directly.
- Scorers operate only on immutable snapshots.
- Planners never call write adapters.
- Executors never decide scoring rules.
- Game-specific offsets/signatures must be contained in build-specific adapters.

## Required final stage report
Fill `.auto-cattery/reports/stage-XX.md` with actual files, commands, build/test results, game validation status, risks, omitted later work, local commit hash, and `是否 push：否`.
