# Changelog

## Unreleased

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
