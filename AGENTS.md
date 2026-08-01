# AutoCattery Codex Repository Rules

## Mission
Implement the Mewgenics Auto Cattery MOD one stage at a time, using
`AutoCatteryDocs/16_steps/` as scope and design guidance rather than a source
of authoritative game values. Validate technical details against the current
repository, current game build, runtime evidence, and reliable public sources.

## Feature-first priority
- Implement the player-visible MOD behavior explicitly requested by the user
  before expanding validation, safety hardening, reports, or research.
- Keep only the minimum build/test cycle needed to prove the current feature;
  do not delay core functionality with repeated audits or safety work.
- For an explicitly requested test-save edit, make one recoverable backup, then
  return immediately to the MOD feature being tested.

## Non-negotiable behavior
- Read `CODEX_TASK.md` before modifying files.
- Implement only the current stage. Never pre-implement later stages.
- Web research is allowed when it helps verify relevant facts or locate public
  technical information. Treat documentation and online material as references
  only: never copy unverified game values, APIs, offsets, scene/UI names, save
  fields, code, or assets, and do not change the fixed technical route without
  explicit user approval.
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
