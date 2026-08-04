# AutoCattery v0.5.6

## Level-up reroll setting

- Added `Level-up rerolls` to the F10 Settings page.
- Players can use the left/right controls or direct numeric input to select 0–99.
- The default remains 3; zero adds no extra rerolls.
- The selected value is written to all seven base and seven advanced player-class
  Mewtator data entries.
- Restart the game after changing the value because class data is merged at launch.

Do not enable the legacy standalone `SkillsPassivesFirstData` together with
AutoCattery; both data mods would modify the same class entries.

The player package remains DLL-only and includes no executable files.
