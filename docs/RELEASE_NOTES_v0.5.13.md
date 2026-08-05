# AutoCattery v0.5.13

## 8-cat combat recommendation button

- Save selection now starts a fresh recommendation lifecycle for the selected
  save.
- A stale ready `Battle` or `Map` scene left in the runtime scene list no
  longer disables the recommendation button before the 8-cat House opens.

## Dynamic level-up rerolls

- The F10 value remains fully configurable from `0` to `99`; 11 is only the
  player's current saved value, not a hard-coded value.
- AutoCattery continues to generate the same native `AddLevelUpRerolls N`
  class patches used by the verified `SkillsPassivesFirstData` project.
- When that sibling data MOD is installed, saving or deploying AutoCattery
  mirrors the current value into both base and advanced class patches, so its
  previous fixed value of 3 cannot win a path-order conflict.

