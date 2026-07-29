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
  - `ClassChooser` is the ready business scene after the game-owned departure
    sign (component count stabilized from 79 to 81). Player live evidence on
    2026-07-29 corrected its meaning: it only displays cats already placed in
    the House expedition box and does not allow selecting or changing cats.
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

## Stage 08

- Status: complete; protection policy and strict read-only sidecar boundary.
- An independent, non-bypassable protection policy supports None, NoCull,
  NoMove, NoCullOrMove, and FullyUnmanaged with conservative permission
  intersection.
- Game-native lock, favorite, and special-state fields remain Unknown because
  the current save format/parser has not proved them. Unknown values fail
  closed and forbid both culling and movement.
- The MOD sidecar reader is schema-versioned and rejects missing/empty files,
  malformed JSON, old/future schemas, duplicate CatIds, invalid levels,
  invalid expiry values, unstable identity, and identity conflicts.
- Sidecar writing, backup recovery, save mutation, room planning, movement,
  and culling are not implemented.
- The classifier now requires a protection-policy decision for every cat.
  Whitelist/hard protection wins over blacklist preference; blacklist only
  reorders candidates after all Stage 07 safety and minimum-pool gates pass.
- Protection digest rechecks compare exact canonical entries, not only a hash,
  and return cancel/repreview when protection changes.
- Debug and Release `phase08_unit_tests` and `phase08_dll_load_smoke` pass.
- The current-save read-only probe returns eight conservatively protected cats,
  zero validation errors, a stable protection digest, zero preview culls, and
  zero executable culls.

## Stage gate

Stage 08 passed on 2026-07-29. Stage 09 may begin only when the player
explicitly requests it.

## Stage 09

- Status: complete; capacity-aware, protection-gated, read-only room planning.
- A separate room-planning domain distinguishes confirmed game hard capacity,
  MOD soft-layout preference, and Unknown room capability.
- Validation fails closed on snapshot/classification/protection mismatches,
  duplicate or missing cats/rooms/residents/capabilities, unknown IDs, and
  non-canonical current protection digests.
- ProtectionPolicy is a required independent input. NoMove, NoCullOrMove,
  FullyUnmanaged, fail-closed, adventure-box, and unconfirmed source-room
  states cannot produce moves.
- The abstract fully evidenced planner is deterministic, minimizes moves,
  never exceeds confirmed hard capacity, supports partial success, and emits
  only non-executable minimum capacity-relief suggestions from the Stage 07/08
  safe candidate order.
- Unknown hard capacity is never replaced by `default_soft_capacity`; soft
  capacity is only a layout preference after a target's hard safety boundary
  is confirmed.
- Current save/parser evidence still cannot prove hard capacities, room roles,
  special/locked/forced-resident states, or receive/release permissions.
  The real adapter therefore emits Unknown capabilities without inspecting
  room names or resident counts.
- Relationship and compatibility details remain unavailable. Breeding-core
  cats remain in place and no breeding pair or fictitious breeding layout is
  generated.
- Debug/Release `phase09_unit_tests` and `phase09_dll_load_smoke` pass.
- The current-save read-only probe returns eight conservatively protected
  cats, zero validation errors, stable IDs/ranking/protection/room plan, zero
  planned or executable moves, and zero executable culls.
- No UI, DLL deployment, game movement, culling, executor, or save write was
  added.

## Stage gate

Stage 09 passed on 2026-07-29. Stage 10 may begin only when the player
explicitly requests it.

## Stage 10

- Status: testable execution safety infrastructure complete; real execution
  remains blocked and Unsupported.
- Approved execution plans are independently sealed after snapshot-content,
  classification, RoomPlan, protection, scene generation, game day, save
  identity, and build identity checks.
- The Stage 07/08/09 read-only flags are unchanged. NoMove, NoCull,
  NoCullOrMove, FullyUnmanaged, fail-closed, adventure-box, duplicate,
  unknown, reordered, or changed inputs cannot bypass approval.
- Offline backup uses SHA-256 and size verification, temporary files, atomic
  publication, containment checks, and no-overwrite behavior. A running save,
  WAL/SHM sidecars, path traversal, or reparse-point escape is rejected.
- Journal and recovery metadata are atomically published and contain operation
  indices instead of cat names or CatIds. Automatic live restore is forbidden.
- The injected transaction executor stops at every failure point, verifies
  each move/cull through an independent reader, rechecks protection/scene/save
  state at boundaries, and records RolledBack or ManualRecoveryRequired.
- Local SDK/source inspection found no verified move, cull, or restore API.
  The read-only SQLite wrapper has no backup API, and copying a live main save
  without WAL consistency is unproved. The real adapter and configuration
  therefore remain Unsupported/disabled.
- The existing House button still requests preview only. No DLL was deployed
  and no real save was written.
- Debug and Release `phase10_unit_tests` and `phase10_dll_load_smoke` are the
  required final validation targets.

## Stage gate

Stage 10 safety infrastructure is complete, but Stage 11 remains blocked until
a separately verified real write and restore mechanism exists. No player game
test is requested for the Unsupported adapter.

## Stage 11

- Status: PreviewOnly complete; real execution blocked by Unsupported Stage 10
  adapter.
- The House button is the only trigger for immutable snapshot capture, combat
  and breeding ranking, classification, protection evaluation, conservative
  room planning, and an anonymous preview summary.
- The explicit workflow state machine fails closed. Applying/Verifying cannot
  be entered with PreviewOnly capability, and concurrent preview/execution
  attempts are rejected.
- Preview authorization is short-lived and binds HouseReady generation, game
  day, snapshot/classification/protection/RoomPlan/config/candidate-order
  digests, anonymous save identity, and build identity. Cancelled, expired,
  changed, duplicate, or consumed previews cannot be claimed.
- Preview output reports anonymous counts, Complete/Partial/Invalid
  disposition, Unknown capability limitations, and that no game data changed.
  It does not log cat names, CatIds, save names, or personal paths.
- Unsupported execution returns NotAvailable before backup, journal, recovery,
  or write-adapter access. Synthetic MoveOnly tests forbid cull routing and
  prove that move failure does not fall back to culling.
- The recommendation sidecar boundary uses a temporary file, checksum, and
  atomic replacement under the MOD data directory. It is reachable only after
  a synthetic committed outcome; PreviewOnly, Cancelled, Failed, RolledBack,
  and ManualRecoveryRequired-equivalent outcomes do not write.
- House preview work runs off the MewUI callback and completion is applied by
  the existing UI tick. No background, House-entry, timed, rest, day-advance,
  team-composition, or embark-selection automation was added.
- Toolkit 1.0.0 was used only for the MIT-licensed workflow/order and
  deterministic contract. Its integer RoomId, six-stat model, room metadata,
  game fields, and write API assumptions were rejected.
- No web research, DLL deployment, player-save read/write, or manual game test
  was performed.

## Stage gate

Stage 11 PreviewOnly is complete. Stage 12 remains blocked until the player
explicitly requests it; real organize execution remains independently blocked
by the Unsupported Stage 10 adapter.

## Stage 12

- Status: Player validation required.
- The existing Stage 4 button now makes an on-demand Stage 12 request. It
  reads only the MOD-owned `state/recommendations.json` boundary and displays
  `Probe Required`; it never shows a fabricated cat recommendation.
- The schema-1 reader validates the exact Stage 11 payload and checksum.
  Missing sidecars are normal; malformed, old/future, incomplete, duplicate,
  or non-finite data is rejected.
- Historical use fails closed because schema 1 lacks build and save identity.
  Unknown/current-day mismatch, config changes, algorithm changes, and
  unverified current candidates also require a current recomputation.
- Player live evidence corrected the active surface: cats are chosen and put
  into the expedition box in House; ClassChooser only displays the boxed
  subset and cannot change it.
- Player live evidence `AC12105` established a complete stable bijection
  between all eight read-only snapshot CatIds and all eight rooted HouseCat
  components for the current build.
- An explicitly armed, read-only House mapping probe records only generation,
  anonymous component/type/Button counts, and anonymous type/role digests.
  Repeated clicks allow before/after box-state comparison without cat names,
  CatIds, pointers, save names, or paths.
- Player validation found the plain text summary too difficult to use with
  many cats. The first eight-button replacement also failed live validation:
  every cloned sign retained a full rope whose visual/input area covered
  earlier rows, disabled rows remained visible, and the column obstructed the
  native depart control.
- The second four-Button replacement also failed live validation: the game
  showed and animated four `Clean Up!` signs before Mark, Clear removed only
  their text, and neither row clicks nor hover-gated wheel input arrived
  reliably.
- The final replacement uses exactly four compact non-Button rows listing
  rank, display name, score, and `?` for unconfirmed eligibility. Rank remains
  numeric, but the preceding `#`/star-like glyph is removed.
- Each row is a private three-frame SWF paper sign plus independent text:
  frame 0 is empty, frame 1 is normal paper, and frame 2 is the source
  button's pressed-size paper. The generator masks the paper from the pinned
  MIT texture and enlarges it to the old board bounds. The wooden board,
  source trash icon, label, full-rope placement, and autonomous button
  timeline are not copied.
- The first static-row live test proved wheel scrolling and rank hit testing
  reached the MOD, but also proved that `MewUI_PlayMovieClipFrame` kept the
  two-frame row playing, so it flashed before Mark and after Clear. The current
  build follows the current EXE's native goto-and-stop sequence by clearing
  MovieClip state bit `+0x09 & ~0x02` immediately after the frame jump.
- A House-thread Windows message observer hit-tests the visible row rectangles
  directly. Mouse wheel scrolls through all eligible results one row at a time,
  and mouse release maps the physical row to the current real rank. Messages
  are never swallowed or rewritten. Identity remains CatId-only; names are
  display labels, never match keys.
- Mouse down stops the row on its pressed frame. Mouse up holds that frame for
  about 90 ms, restores the normal frame, and then invokes the already
  validated details callback, so the visual feedback remains visible before
  the native drawer opens.
- Clicking a recommendation row opens that exact HouseCat in the game's
  native details drawer and green focus outline. The build-specific adapter
  reuses the locally disassembled HouseCatClickManager path only after current
  EXE instruction signatures, scene generation, component ownership/type, and
  unique HouseDrawerUI validation pass.
- Live `AC12109` evidence showed the old adapter received every clicked rank
  but threw inside the native call (`signature=scene=house=cat=1`,
  `opened=0`). Reinspection of the native call site proved its first argument
  is the HouseDrawerUI returned by the click manager, not the House component.
  The next live run verified the unique drawer but still returned
  `drawer=cat=1 opened=0`. Following the native path farther back proved it
  converts HouseCat through RVA `0xEFCB0` before passing the result as the
  details function's second argument. The adapter now reproduces that exact
  conversion and validates its function/call-site signatures and result type.
- The latest live log then proved ranks 1-8 all reached the adapter with
  `signature=scene=drawer=cat=target=1` but still returned `opened=0`.
  Reproducing the native path instruction-for-instruction identified the
  remaining mismatch: the game obtains its first argument by calling RVA
  `0x1A93F0` on the unique `HouseCatClickManager`; scene enumeration of a
  same-typed `HouseDrawerUI` does not prove object identity. The adapter now
  calls that exact getter and validates the returned `HouseDrawerUI`.
- `AC12109` now reports click-manager validation, whether the getter result
  matches the scene-enumerated drawer, and SEH failure stage/code/module RVA.
  It records no object address or player identity and makes any remaining
  native failure instruction-level rather than ambiguous.
- The next live run consistently reported `manager=1 drawer=0 failure=0`,
  while the same scene's `AC12103` reported exactly one `HouseDrawerUI`.
  The getter had succeeded; its return is the native interface/subobject
  pointer used by the call site, not a scene-component base suitable for
  `GetObjectTypeSTR`. The adapter no longer rejects that valid pointer through
  the component-type vtable. It instead validates the exact `+0x38` and
  `+0x60` fields consumed by the signed native detail routine.
- The latest screenshot also proved the centered HTML text field itself was
  placed left of the sign. Its generated transform is now calculated from the
  real SWF bounds: sign/text centers are 1124.35/1124.16 and text width 107.33
  fits inside sign width 114.89.
- This player-triggered action changes only the House detail focus. It has no
  adventure-box, expedition-team, confirmation, or save-write API.
- The first click recomputes with the Stage 6 independent-cat scorer after
  snapshot/generation/bijection validation. The second click clears the list.
  Leaving House also clears it. No selection, box, party, confirmation, or
  game-save write API exists in this path.
- Cross-save live logs exposed two lifecycle defects after the earlier
  acceptance. Requests 2-9 in House generation 4 completed immediately with
  `snapshot_valid=0`; generation 6 likewise failed until request 13, after the
  selected save finally became the newest modified `.sav`. Mark is now one
  pending operation with one-second read-only retries for at most 30 seconds,
  so the player does not need to click repeatedly while the selected save is
  settling.
- Save capture now parses every CatId in the current `house_state`, including
  ordinary rooms, `AdventureBox`, and empty room IDs. An empty room means only
  that no room assignment is known; it no longer removes the cat. Historical
  rows present only in the `cats` table remain excluded.
- The 25-cat save has 16 `Colorless` cats, but one has a persisted death day;
  only 15 are combat-available. That dead cat and all 9 cats with an assigned
  combat class are excluded. Recommendation output includes every available cat,
  without the old limit of eight; the four physical rows remain a scrollable
  window over the full result set.
- The current main save has 79 House cats: 74 combat-available, 1 persisted
  dead, and 5 with an assigned combat class. Its variable-length pre-breed
  stat-affinity descriptor exposed a fixed-width parser error, while the live
  HouseCat, anonymous mapping, and detail probes separately rejected scenes
  above a fixed 4096-component limit. The reader now follows the length prefix
  and reads persisted birth/death days; scene component access validates the
  vector bounds and readable pointer range instead of a business-count cap.
- Capture reads all save candidates in the active Steam profile and selects
  the candidate with a complete stable CatId-to-rooted-HouseCat mapping. This
  selects the current 8-, 25-, or 79-cat save immediately and rejects a stale
  snapshot from another slot.
- Recommendation row labels use direct UTF-8 text instead of a localization
  numeric placeholder, so clearing a row cannot render `0` or `.`. The two
  upper MOD buttons are reused by role and have their callbacks refreshed,
  preventing duplicate button components across furniture/House reattachment.
- At the 23:54:33 House exit, chainloader recorded repeated access violations
  in the same millisecond as UI detach. Both House controls now verify that the
  stored scene is still the current ready House before touching components or
  MovieClips. An unloaded scene only causes hook removal and local-handle
  invalidation; a new generation re-resolves all UI nodes and stops the four
  recommendation clips on the hidden frame.
- The four recommendation items are now a two-column, two-row grid ordered
  top-left, top-right, bottom-left, bottom-right. Separate hit rectangles leave
  gaps between items and keep the grid away from the two upper MOD controls
  and the native depart sign.
- Debug/Release `phase12_unit_tests` and `phase12_dll_load_smoke` are the
  required validation targets and pass. The Release DLL and Mewtator UI data
  MOD are deployed for player-visible validation.

## Stage gate

Stage 12 is reopened for player-visible save-switch validation. The native cat
detail path remains player-confirmed, but death filtering, dynamic large-save
mapping, and the retry/lifecycle/grid repair must pass in one process across the
8-, 25-, and 79-cat saves. Stage 13 remains blocked.
