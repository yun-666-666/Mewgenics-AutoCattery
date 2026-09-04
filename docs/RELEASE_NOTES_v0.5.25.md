# AutoCattery v0.5.25

- Fixes the post-update startup crash caused by installing MewUI hooks at the
  previous game's fixed addresses.
- Locates the required UI functions from the currently running game image, so
  the public and beta executables can use their own runtime addresses without
  an EXE hash, timestamp check, or one-version RVA table.
- Refuses only the unavailable UI path when a required function cannot be
  located uniquely; it does not install a hook at an unrelated address.
- Logs the resolved scene-ready and Button hook RVAs before installation.
- Preserves F10, the two House buttons, MoveOnly, protection, configuration,
  data collection, and the existing Mewtator package layout.

Automated verification covers runtime address discovery on the currently
installed executable, the existing unit suite, both Debug and Release builds,
and DLL loading. Real-game startup, F10, House buttons, scene transitions, and
the beta executable remain player-validation boundaries.
