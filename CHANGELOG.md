# Changelog

## 0.5.35 — 2026-10-07

- Adds a MOD-only download without the external workbench/runtime. Both in-game F10
  offspring all-seven and food assists are included; the workbench is an optional separate download.
- Publishes the complete current MOD and external workbench sources since v0.5.34,
  including the previously uncommitted population, assistance and native simulator modules.
- Reads saved breeding fields independently of Tink display unlocks; refreshes native room
  identities and supports five-room houses and unassigned HousePipe cats.
- Adds configurable population management with a cancellable 10-second surplus preview,
  protected-cat retention, sequential trash delivery and NPC recipient priorities.
- Exposes optional in-game all-seven offspring and food assists. Balances inherited ability,
  passive and positive mutation categories while retaining disorder/defect penalties.
- Selects breeding rooms dynamically by comfort, stimulation, health and mutation,
  excluding breeding suppression and including rare/merged-furniture effect multipliers.
- Ships a standalone external breeding workbench with Python, native simulation, confirmed
  save application and recovery copy, native random names, actual/target pair display,
  and persistent assist preferences. Includes a separate Workbench download.
- Supports checkpoint resume and bounded parallel experiment arms; preserves completed
  native-equivalence and long-run comparison evidence without treating it as game acceptance.
- Updates Chinese/English READMEs, user instructions and implementation status. The player
  confirmed normal actual play on 2026-10-07, promoting v0.5.35 to the formal Latest Release.
  The known aggregate fixed-room test failures remain documented in the release notes.

## 0.5.34 — 2026-09-10

- Clear House poop after confirmed organization, including zero-move plans; preserve normal furniture and the confirmation label.
- Keep both native House buttons updating during temporary input blocking so they remain usable afterward.
- Player reported multiple successful play sessions with the final version.

## Unreleased

- Records player acceptance of v0.5.30: completing combat and returning to the
  House no longer crashes on the previously failing save.
- Validates the House child collection's MSVC RTTI before calling the game's
  native child lookup, accepting only the verified display-container types and
  rejecting unrelated objects even when their virtual slots are executable.
- Records player acceptance of v0.5.29 on both the current beta and official
  game versions, including the full organization preview, second-click native
  movement, and click-to-open combat recommendation details.
- Restores current-beta MoveOnly previews by validating live `HouseCat`
  components by their engine type instead of the stable build's fixed vtable
  address. When the native room bucket is unavailable, the runtime snapshot
  now derives the safe available-room count from distinct occupied room
  pointers; unknown or incomplete room identity still fails closed.
- Resolves the current-beta native House move entry at `0x2E88D0` while
  preserving the verified stable `0x2E7DB0` entry. Each candidate must match
  its expected machine-code signature before a move can be invoked.
- Restores click-to-open behavior for the combat recommendation list by
  selecting a complete stable/current-beta House-detail layout. The target
  resolver, drawer resolver, open-details function and all three relative
  call sites must agree before any native detail call is allowed.
- Fixes the current beta House-entry crash after `AC18000`. Two independent
  full dumps showed AutoCattery calling the legacy component-bucket preparation
  RVA `0x963040`, which is ten bytes past the current beta function entry and
  lands inside an instruction; both terminated at `Mewgenics.exe+0x963041`.
- Selects the verified stable (`0x963040`) or current-beta (`0x963030`)
  component-bucket preparation entry only when its invariant machine-code
  signature matches. Unknown or ambiguous layouts now leave native room
  enumeration unavailable instead of calling an unverified address.
- Resolves all runtime MewUI addresses from a clean `SEC_IMAGE` mapping of the
  executable that launched the current process. Another MOD may therefore hook
  scene-ready first without erasing the signature AutoCattery still needs to
  join Mewjector's existing hook chain.
- Adds a regression check showing that a simulated scene-ready hook invalidates
  the old live-memory signature while the unmodified executable remains
  uniquely resolvable. Successful startup now logs whether the clean executable
  image or the live fallback supplied the addresses.
- Replaces every MOD-owned F10 panel instance name with an ASCII name of at
  most 15 bytes, keeping current beta House attachment on the native inline
  string path. Later full-dump analysis established that the repeated
  `Mewgenics.exe+0x963041` crash was caused by the separate fixed native-room
  RVA, not by these lookup strings.
- Adds an asset/runtime contract test covering all 145 panel artwork and text
  instances, including uniqueness, the 15-byte limit, complete SWF placement,
  and agreement with the C++ lookup names.
- Replaces fixed MewUI function RVAs with runtime discovery against the game
  image that is actually running. Public and beta builds can resolve their own
  UI addresses without an executable hash, timestamp gate, or single-version
  address table; ambiguous or unavailable functions leave the UI disabled
  instead of installing a hook at the wrong address.
- Logs the resolved scene-ready and Button hook RVAs before installation so a
  player startup report identifies the active game layout directly.
- Recorded player acceptance of the current v0.5.24 House buttons, F10 panel,
  two/three/four-room MoveOnly flows, stale-preview cancellation, unassigned-cat
  handling, save/reload persistence, and repeated organization.
- Public source packaging now reads the version from `CMakeLists.txt`, rejects a
  mismatched explicit version, excludes internal stage/handoff/task state, and
  stops if a personal absolute path enters the source archive.
- Consolidated current-status documentation and removed obsolete handoff and
  duplicate rollback notes. Historical failures are no longer listed as current
  issues without new evidence on the current release.
- Restores mouse input for both normal House buttons. Their AS3-stopped first
  frame is intentionally empty, so v0.5.23 created the native Button while its
  state nodes and hit area were absent. The view now materializes the stopped
  visible frame before native setup, while setup still replaces the source
  label in the same UI tick.
- Keeps both normal House buttons on an empty, stopped first frame until their
  native controllers have replaced the source `Clean Up!` labels, preventing
  the default artwork from appearing before native attachment.
- Removes the fixed three-second House wake delay. The first wake probe that
  sees a ready House or selection scene resumes UI work without another timer;
  expedition suspension and its once-per-second probe remain unchanged.
- Reattaches both House buttons when the game replaces the House scene during
  a day transition, instead of retaining previous-generation UI pointers and
  exposing the source asset's `Clean Up!` labels.
- Resolves the F10 panel through the short MOD button marker before looking up
  panel nodes, avoiding long-name native searches against unrelated House
  roots during initial attachment.
- Rejects stale deployment packages and verifies that the installed DLL
  contains the same version declared by the installed data mod.
- Gives private House panel clips an AS3 first-frame stop copied from the
  pinned source SWF. The game ignores their former AVM1 DoAction stops, so
  blank controls could animate before the post-expedition UI wake completed.
- Generates the House SWF during builds instead of packaging the stale asset.
- Keeps an already-confirmed House context through temporary no-ready-scene
  gaps caused by native room updates. Explicit save, selection, expedition,
  ambiguous-scene, and House-instance changes still replace the context.
- Suspends MewUI's own tracked-button maintenance and AutoCattery button-hook
  record scans while expedition sleep is latched. The once-per-second scene
  callback remains active only to detect a stable return home.
- Kept expedition sleep latched across transient `Battle` and `Map` readiness
  gaps to prevent combat poll churn. The initial three-second House stability
  delay introduced with this change was removed in v0.5.23.
- Suspended AutoCattery's UI, workflow, and configuration polling while a ready
  `Battle` or `Map` scene is present. A minimal once-per-second scene probe
  wakes the MOD after the expedition clears; House responsiveness is otherwise
  unchanged. Runtime configuration file timestamps are checked at most twice
  per second outside expeditions instead of once per UI frame.
- Breeding-room selection now optimizes the whole mixed-sex cohort, not only
  separate high-ranked pairs. Every newly preferred cat is evaluated against
  all already selected opposite-sex residents, maximizing the weakest
  cross-pair score and then minimizing the worst cached offspring COI.
- Analysis of the opt-in main-save history from game days 257 through 264
  confirmed that the game selected cross-pairs inside the shared breeding
  room. The first day after the v0.5.14 pair pool improved newborn average
  seven-stat gap from 2.93 to 2.60 and average offspring COI from 0.220 to
  0.168, while also showing why independent-pair ranking was insufficient.
- Split large native House move plans into batches of at most eight moves,
  separated by UI ticks and fresh runtime previews. Leaving House cancels the
  continuation before another batch can run.
- Breeding-room filler cats now come from disjoint eligible pair rankings that
  already combine seven-stat coverage, sexuality, and cached offspring COI,
  instead of being selected by unrelated stable ordering after the primary
  pair is placed.
- Reset next-day combat recommendation availability when the save-selection
  screen is observed. A stale ready `Battle`/`Map` scene can no longer disable
  the recommendation button in a newly selected 8-cat save.
- F10 level-up reroll saves now dynamically mirror the selected `0`–`99` value
  into an installed sibling `SkillsPassivesFirstData` patch. The known working
  companion data MOD and AutoCattery therefore agree instead of letting its
  old fixed value override the control-panel setting.
- Room purposes now follow the confirmed House mechanics: breeding selection
  requires viable comfort and balances comfort with stimulation; combat
  staging prefers health and comfort instead of reserving the highest-
  stimulation room.
- The existing "separate kittens when possible" setting is now active in
  MoveOnly. When an occupied room remains after breeding and combat staging
  targets are selected, movable kittens receive a health/comfort-oriented
  nursery target and adults avoid those slots when possible.
- Stable-all-7 breeding still uses Mutation as an additional tie-breaker, but
  no room with comfort at or below the confirmed automatic-failure boundary
  wins merely because it has very high stimulation.
- Balances only the confirmed breeding target room toward the best achievable
  female/male mix. Recommended pair members, fixed-room cats, and immovable
  residents are counted first; an infeasible 1:1 mix degrades safely instead
  of failing the preview.
- Removed the remaining combat-potential mixed-sex replacement. Combat,
  training, and ordinary rooms now ignore sex completely, so a lower-ranked
  cat is never selected merely to change a displayed ratio.
- Same-sex and unknown-sex pairs are no longer eligible kitten-breeding pairs;
  the current adapter requires a known female/male combination.
- Fixed F10 level-up reroll values being overwritten by a later Mewtator data
  mod. Deployment and in-game reroll saves now deduplicate `AutoCattery` and
  keep it last in `modlist.txt`, so the configured value wins class-data
  conflicts without adding another mod's rerolls.
- Opening F10 now suppresses the House organize and recommendation controls
  without detaching them. Closing F10 restores the existing registrations
  instead of scanning and registering against a potentially stale UI root.
- Removed the global one-female/one-male requirement from every managed room.
  Sex constraints apply only inside the confirmed breeding target room.
- Separates the breeding-pair target room from the best combat-development
  room when more than one room is available. Combat and ordinary rooms no
  longer move cats merely to improve their displayed sex ratio.
- Fixed the v0.5.7 blank/flickering F10 panel regression. The MOD assets do
  not belong to the game `HouseTest` root, so forcing every lookup through that
  owner found incomplete nodes and repeatedly failed panel attachment.
- House UI root discovery now runs only while attaching, deduplicates roots,
  stops at the first matching MOD asset root, and avoids querying all 4,249
  component type names. Failed F10 attachment is throttled to one attempt per
  500 ms instead of retrying every UI frame.
- Fixed the remaining post-battle House exception storm. When combat
  recommendations were unavailable for the rest of the day, the controller
  repeatedly cleared four recommendation rows every UI tick; each clear used
  a full-scene node scan. Availability updates are now idempotent, row text is
  written through cached nodes, and House UI discovery is kept out of the
  steady-state render path.
- Removed the repeated invalid lookups that grew `chainloader.log` to about
  1.2 GB and matched the end-day heap-corruption crash window.
- Fixed severe post-battle House lag caused by the UI compatibility lookup
  calling the game's exception-heavy root-owned child routine across thousands
  of House components. AutoCattery now validates and deduplicates component
  roots, uses the direct child lookup, caches the matched SWF root, and fails
  closed instead of falling back to a scene-wide scan.
- Added a player-selectable `0`–`99` level-up reroll count to the F10 Settings
  page. The default remains 3; changes rewrite all 14 player-class data patches
  and take effect after restarting the game.
- Fixed the F10 Settings hit test so hidden bottom navigation no longer blocks
  the combat Luck weight row and all setting rows use their full vertical slot.
- Consumed `Esc` while the F10 panel is open so it closes or cancels MOD input
  without also opening the game's settings overlay.
- Invalidated every cached House UI pointer at a scene-generation boundary and
  kept the four recommendation rows stopped and hidden after adventure return.
  This addresses the observed row flicker and the heap-corruption crash window.
- Removed `AutoCatterySettings.exe` from source targets, build, deployment,
  install verification, and release packages. Current player configuration and
  protection are available only through the F10 panel.

## 0.5.4 - 2026-08-04

- Reduced false-positive antivirus signals in `AutoCatterySettings.exe` by
  opening cat protection management in the existing process instead of
  launching a second copy of the executable.
- Added standard Windows product/version metadata and an embedded `asInvoker`,
  Per-Monitor-V2 DPI-aware application manifest.
- Added build checks that reject x86 output, missing product metadata, and
  unexpected self-launch or remote-process APIs in the settings executable.
- Kept the F10 panel, MoveOnly behavior, settings, and cat protection features
  unchanged.

## 0.5.3 - 2026-08-02

- Fixed F10 management-panel clicks on smaller screens and mixed-DPI Windows
  setups by using the mouse message's client coordinates consistently.
- Added virtual-viewport coordinate regression coverage for widescreen, 4:3,
  and boundary inputs.
- Kept the existing F10 pages, MoveOnly workflow, protection model, and external
  settings editor unchanged.

## 0.5.2 - 2026-08-02

- Removed the trash-can artwork from the House Auto-Organize and combat-marker
  buttons while keeping their text and click behavior.
- Centered the button labels after removing the icon space.
- Kept the prerequisite installation links and Mewtator `mods` path explicit in
  the bilingual installation documentation.

## 0.5.1 - 2026-08-02

- Clarified that `source.zip` is for developers and that `Windows-x64.zip` is the
  installable release package.
- Added exact Mewjector/Mewtator copy paths and F10 usage steps to both READMEs.
- Included the bilingual User Guide and version-matched release notes in binary
  release documentation.
- Kept runtime behavior unchanged; this release only improves packaging and user
  guidance.

## 0.5.0 - 2026-08-02

- Added a top-level Full Preview page to the F10 management panel.
- Added per-cat move source, destination, sex, potential, and reason.
- Added room before/after population and female/male ratio summaries.
- Removed repeated full House-scene text lookups by caching MewUI nodes and
  skipping unchanged text/frame updates.
- Added persistent Chinese/English selection, defaulting to Chinese.
- Added opt-in, local-only, privacy-minimized cat planning data collection.
- Added bilingual user documentation, a current-build game-value reference,
  acknowledgements, and release packaging.
