# AutoCattery v0.5.5

## Fixes

- Fixed the F10 settings hit targets, including both Luck weight fields.
- `Esc` is consumed by the open F10 panel instead of also opening game settings.
- House UI references are discarded at scene-generation boundaries to prevent stale
  native-node access during settings, adventure, and House transitions.
- Recommendation rows are attached in a disabled hidden state after adventure return,
  preventing their SWF timeline from flickering until the next day.
- Debug hit-test regressions now fail normally without dereferencing an empty
  `std::optional` or opening a Visual C++ Debug Assertion dialog.

## Distribution

- The player package contains the AutoCattery DLL and data assets only.
- The external settings executable is no longer built, deployed, or packaged.
- Development-only test and save tools remain source/build targets and are not included
  in the player package.

## Validation

- Debug CTest: 4/4 passed.
- Release build, CTest, DLL export, and x64 architecture checks passed.
- Release deployment and installed DLL SHA-256 verification passed.

The heap-corruption crash window is addressed by the House UI lifecycle repair, but
long-session player validation is still required because the crash dump did not identify
the exact heap write site.
