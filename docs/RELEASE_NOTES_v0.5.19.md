# AutoCattery v0.5.19

## Full native UI sleep during expeditions

- Keeps the lightweight once-per-second scene callback used to detect the end
  of an expedition.
- Stops MewUI's tracked-button maintenance loop while expedition sleep is
  latched.
- Makes both global native button hooks pass directly through to the game
  without scanning AutoCattery button records during combat.
- Restores normal House UI work only after save/class selection or a ready
  `House` scene remains stable for three continuous seconds.

The v0.5.18 player log confirmed that upper-level AutoCattery work stayed
asleep for the entire expedition, but MewUI's lower-level button work still ran
before the callback. v0.5.19 closes that remaining combat-time path.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
