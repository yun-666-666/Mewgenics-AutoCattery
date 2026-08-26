# Changelog

## Unreleased

- Kept expedition sleep latched across transient `Battle` and `Map` readiness
  gaps. AutoCattery now wakes only for save/class selection or after `House`
  remains continuously ready for three seconds, preventing combat poll churn
  and premature House UI attachment.
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
