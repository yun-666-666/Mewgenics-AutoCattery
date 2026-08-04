# AutoCattery v0.5.8

## F10 panel text and flicker fix

- Fixes the blank management panel where only frames and boxes were visible.
- Removes the incorrect v0.5.7 assumption that MOD UI assets belong to the
  game `HouseTest` root.
- Finds the MOD asset root only while attaching and stops at the first match.
- Avoids reading all 4,249 component type names.
- Limits a failed F10 panel attachment to one retry per 500 ms instead of one
  retry every UI frame.

Recommendation rendering remains idempotent and uses cached row text nodes.
No save data, protection rules, reroll settings, or MoveOnly behavior changed.
