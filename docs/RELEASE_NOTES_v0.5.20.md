# AutoCattery v0.5.20

## Stable House UI after native room updates

- Keeps the confirmed House context through temporary periods where no scene
  reports ready after native room moves.
- Continues to leave House immediately for explicit save, selection,
  expedition, ambiguous-scene, or replacement-House events.
- Prevents the House controls and hidden F10 panel nodes from being abandoned
  just because the game briefly rebuilds its House state.

The v0.5.19 player log showed a successful stable-House attach followed by 32
native room moves. Two seconds after the final batch, a no-ready-scene gap was
converted into `UnsafeTransition`, which removed the controls. That unmatched
gap is now ignored only while the current context is already House.

The v0.5.19 combat-time native sleep remains unchanged.
