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

- Status: implementation and automated validation complete; live acceptance
  pending.
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

## Stage gate

Do not begin Stage 04 until the player completes
`docs/phase03-manual-test.md`, including repeated House entry/exit, rapid
clicks, save/pause/transition behavior, and visual checks at representative
resolutions.
