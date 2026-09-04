# AutoCattery v0.5.26

- Fixes the repeatable current-beta crash immediately after entering House and
  attaching the hidden F10 management panel (`AC18000`, followed by
  `Mewgenics.exe+0x963041`).
- Renames every MOD-owned management-panel artwork and text instance to a
  unique ASCII name no longer than 15 bytes. Native child lookup therefore
  stays inside the game's inline narrow-string representation instead of
  transferring heap-backed strings during initial panel attachment.
- Preserves the complete F10 settings, cat-protection and preview pages, Esc
  close behavior, both normal House buttons, MoveOnly, configuration and data.
- Adds a generated-asset contract test for all 145 panel instance names and
  verifies that the native lookup names match the SWF.

Automated verification covers House SWF generation, the panel-name contract,
the existing unit suite, Debug and Release builds, DLL loading, deployment and
installed-package checks. Real-game beta startup, House entry, F10 interaction,
the two normal House buttons and scene transitions remain player-validation
boundaries.
