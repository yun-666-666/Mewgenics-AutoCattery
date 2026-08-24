# AutoCattery v0.5.16

## House-entry crash fix

- Fixes a crash that could occur immediately after entering the House while
  AutoCattery was attaching its management panel and House controls.
- Rejects invalid scene-component roots before calling the game's native UI
  child lookup, preventing the repeated invalid virtual calls that led to
  `STATUS_HEAP_CORRUPTION`.
- Adds a focused regression test for valid and invalid UI child-collection
  virtual tables.

The fix was validated in the previously crashing save: entering the House,
opening the F10 panel, and re-entering the House no longer crashes.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
