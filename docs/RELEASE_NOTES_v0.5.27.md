# AutoCattery v0.5.27

- Fixes AutoCattery becoming completely unavailable when another MOD installs
  the shared scene-ready Hook before MewUI's delayed initialization.
- The observed current-BETA startup loaded CombineDuplicateFurniture v0.6.19,
  which hooked scene-ready RVA `0x96AC50`; AutoCattery then repeatedly failed to
  find the already-overwritten signature and never reached `AC1202` or attached
  its F10 and House controls.
- Runtime UI RVAs are now resolved from a clean `SEC_IMAGE` mapping of the exact
  executable backing the current game process. Hooked process memory is used
  only as a fallback if that clean mapping cannot be opened.
- The clean image is checked against the loaded image's PE timestamp and image
  size before use. This is an identity comparison, not a game-version whitelist
  or fixed-address gate.
- After resolving the clean RVA, AutoCattery installs through Mewjector normally
  and can join an existing shared Hook chain without requiring either MOD to be
  disabled or renamed.
- Preserves the v0.5.26 short management-panel names, complete F10 behavior, both
  normal House buttons, MoveOnly, configuration, protection and local data.

Automated verification covers all 30 current-BETA UI signatures, a simulated
live scene-ready Hook mutation, Debug and Release builds, the existing CTest
suite and DLL loading. Real-game coexistence with CombineDuplicateFurniture,
House entry, F10 and both House buttons remain player-validation boundaries.
