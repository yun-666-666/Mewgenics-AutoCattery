# AutoCattery implementation status

## Stage 01

- Status: complete.
- Game build:
  - Local `Mewgenics.exe` has no embedded file/product version.
  - Size: `21,981,184` bytes.
  - Last modified UTC: `2026-05-23 06:12:17`.
  - SHA-256: `C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`.
- Mewjector:
  - Runtime log reports API v3.
  - Source pinned at `ccdd6813cef0f51342eb74c0cecb47654f7dbeef` (`v3.4` tag).
- JSON parser: nlohmann/json `v3.12.0`, source dependency.
- MewUI API:
  - Version `1.2.0`.
  - Source pinned at `0b3415ab5fa8617edcecc7eb206c165cb8b6e991`.
  - MIT-licensed source is compiled into AutoCattery.
  - `MewUiBridge` starts/stops the bootstrap and performs no formal UI work in
    Stage 01.
- References:
  - Push To Meow revision `0ef06c146be341af0eba9d5062081c952902850d`
    confirmed the current Mewjector hook pattern but contains no button UI.
  - Quick-Cleanup revision `c1e522bb05a202c8f7045efe0d2e0a74bcabed88`
    and MewUI API are the actual MIT-licensed button/UI references.
- Safety:
  - Only MewUI lifecycle hooks are installed.
  - No scene, cat, room, or save data is accessed.
  - No formal UI is injected.
- Local tests:
  - Release and Debug unit tests pass.
  - Release and Debug DLL load smoke tests pass three consecutive
    load/initialize/shutdown/unload cycles.
  - Both DLLs expose `AutoCattery_Initialize` and `AutoCattery_Shutdown` and
    have an x64 PE machine type.
- Live game test:
  - Release DLL deployed to `mods/AutoCattery.dll`; SHA-256 matched build output.
  - Three controlled launch attempts were performed without opening a save.
  - Steam's launcher handoff created more than one game process on later
    attempts; each process logged one AutoCattery initialization.
  - Mewjector loaded AutoCattery and the existing SkillsPassivesFirst mod.
  - AutoCattery resolved the Mewjector v3 API once per process.
  - Configuration, degraded UI bridge, and initialization-complete events were
    logged.
  - All test processes were closed and no Mewgenics process remains.

## Stage 02

- Status: complete.
- Implemented:
  - `SceneContextService` with stable-frame entry debounce and ten-frame loss
    debounce.
  - Generation-tagged `HouseReady`, `EmbarkSelectionReady`, and
    `UnsafeTransition` snapshots.
  - Exception-isolated subscriptions on the MewUI game/UI callback thread.
  - Versioned `config/scene_signatures.json`; empty signatures fail closed.
  - Debug/opt-in scene summary logging and F8 JSON export without textures,
    scripts, assets, or save contents.
  - Explicit save-scene downgrade and no formal buttons or cat access.
- Automated tests:
  - Release and Debug `phase02_unit_tests` pass.
  - Release and Debug `phase02_dll_load_smoke` pass.
  - Tests cover house/embark entry and exit, duplicate callbacks, transient
    node/layout changes, ten-frame loss, rapid switches, saving, and subscriber
    exceptions.
- Player-assisted evidence on 2026-07-28:
  - Actual house scene is `House`.
  - `HouseReady generation=1` was observed.
  - Resting unloaded the scene and emitted `UnsafeTransition generation=2`.
  - The reloaded house emitted `HouseReady generation=3`.
  - Supplied before/after screenshots show the same house UI with dynamic cat
    positions; detection does not depend on cat layout.
  - Five player-operated diagnostic captures distinguish `House` from the
    embark-selection scene.
  - `ClassChooser` is the only ready business scene introduced on the
    embark-selection screen (component count stabilized from 79 to 81).
  - House lifecycle generations progressed through 18 across repeated
    leave/return cycles without duplicate ready events or errors.
- Operational rule:
  - Codex may perform launch/no-crash smoke tests only.
  - The player performs save selection and gameplay navigation, following
    `docs/phase02-manual-test.md`.

## Stage 03

- Status: complete. Automated validation and player-operated live acceptance
  passed on 2026-07-28.
- Implemented:
  - One role-identified `AutoCattery.House.AutoOrganizeButton`, created from a
    dedicated House SWF node.
  - House-only attach, unsafe-context detach/disable, and idempotent reuse.
  - 500 ms click debounce and running-state duplicate suppression.
  - A placeholder workflow that returns `NotImplemented` and never opens cat,
    room, save, or persistence data.
  - Placeholder title/body text that explicitly says no cats were modified.
  - Chinese and English built-in strings. Runtime uses English until CJK glyph
    coverage is verified for the pinned SWF font.
  - Build, deployment, and install verification for the SWF, localization, and
    append files.
- Automated tests:
  - Debug and Release `phase03_unit_tests` pass.
  - Debug and Release `phase03_dll_load_smoke` pass.
  - Controller tests cover unsafe attach rejection, idempotent attach, click
    debounce, placeholder-only execution, and idempotent detach.
  - A regression test confirms the always-loaded `PauseMenu` scene is not a
    blocking overlay; only a ready save scene forces the save safety path.
- Asset provenance:
  - `auto_cattery_house.swf` is mechanically derived from the pinned MIT MewUI
    example SWF by removing its navigation, toggle, and unused third text node.
  - `tools/build_house_ui_asset.py` reproduces the derived asset without
    downloading or copying any new third-party material.

## Stage 04

- Status: complete; player-operated live acceptance passed on 2026-07-28.
- Completion commit: `d83629c`.
- The recommendation marker lifecycle and final compact layout were accepted.

## Stage 05

- Status: complete; player-operated House click acceptance passed on
  2026-07-29.
- A read-only save adapter now produces immutable cat and room snapshots.
- Real local evidence replaced inaccurate reference-document assumptions:
  stable IDs are 64-bit SQLite keys, room IDs are strings, and the game has
  seven stats including Charisma.
- Unverified class, age, relationship, room-capacity, and missing-room fields
  remain unavailable.
- Debug/Release unit tests and DLL load smoke pass.
- The selected save contains 30 raw `cats` table records, but only eight IDs
  occur in its current `house_state`. These are records from one save, not a
  merge across save slots.
- After the player identified the UI count mismatch, snapshots were narrowed
  to reliably room-assigned house cats. The corrected current-save probe
  returns eight house cats, two represented rooms, zero validation errors, and
  stable IDs across two captures.
- The accepted live session produced ten identical sanitized `AC5100`
  snapshots and five recommendation-marker toggles, with zero warnings, zero
  errors, one attach per control, and clean detach on leaving House.

## Stage gate

## Stage 06

- Status: complete; pure read-only scoring/ranking stage.
- The user-provided AutoCattery Toolkit 1.0.0 deterministic single-cat
  selector flow was adapted to the verified snapshot domain.
- The old six-stat reference assumption was rejected. Ranking uses all seven
  real stats, including Charisma, and totals genetic, heredity bonus, and
  equipment bonus values.
- Core save ability parsing was corrected from nine to ten slots: movement,
  basic attack, four active abilities, two passives, and two disorders.
- Subjective ability/disorder values default to zero and require explicit
  configuration overrides.
- Known dead, kitten, unavailable, and injured states are filterable. The
  current save adapter still reports these states as unknown, so results carry
  explicit limitations and fail the default confirmed-eligibility gate.
- Stable ranking, configurable threshold/top-N selection, finite-value
  validation, explanations, and deterministic caching are implemented.
- Debug and Release unit/DLL smoke tests pass. Tests cover 1000 cats and 100
  identical repeats.
- The current-save probe returns eight ranked house cats, zero default
  recommendations until eligibility is confirmed, zero validation errors,
  stable IDs, and stable ranking
  across two captures.
- No UI marking, team composition, movement, culling, or persistence write was
  added.

## Stage gate

Stage 06 passed on 2026-07-29. Stage 07 may begin only when the player
explicitly requests it.

## Stage 07

- Status: complete; read-only breeding scoring and safe preview
  classification stage.
- The user-provided AutoCattery Toolkit 1.0.0 flow was adapted, while its
  six-stat, mutation, breeding-eligibility, protection, and compatibility
  assumptions were rejected where the current adapter cannot verify them.
- Breeding score uses all seven confirmed game stats from genetic plus
  heredity bonus values. Equipment bonus is explicitly excluded.
- Ability, passive, and disorder values default to zero and require explicit
  saved-ID overrides.
- Stable classification produces combat recommendation, breeding core/reserve,
  general reserve, protected, ineligible, and preview cull roles.
- Minimum combat, breeding, and general pools are enforced. Unknown
  protection, special state, identity, breeding eligibility, relationships,
  life stage, injury state, or insufficient confidence blocks candidates.
- Quality candidates and the potential capacity-relief pool are separate.
  Room capacity is not evaluated before Stage 09.
- No candidate permits destructive execution; no UI, movement, culling,
  executor, or save write was added.
- Debug and Release unit/DLL smoke tests pass.
- The current-save probe returns eight breeding ranking rows, zero validation
  errors, stable IDs/ranking, zero preview culls, and zero executable culls.

## Stage gate

Stage 07 passed on 2026-07-29. Stage 08 may begin only when the player
explicitly requests it.
