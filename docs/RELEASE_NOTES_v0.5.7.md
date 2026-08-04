# AutoCattery v0.5.7

## Post-battle House stability fix

- Stops the unavailable combat-recommendation state from clearing its four
  rows every UI frame.
- Writes recommendation text through cached MewUI nodes instead of scanning
  every House component.
- Restricts House UI root discovery to the single observed `HouseTest` owner
  for the supported game build and fails closed if that boundary changes.
- Addresses the exception storm that grew `chainloader.log` to about 1.2 GB,
  caused severe post-battle lag, and overlapped the end-day heap-corruption
  crash window.

No save data, protection rules, room planning, or MoveOnly behavior changed.
