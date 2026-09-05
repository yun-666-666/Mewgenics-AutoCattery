# AutoCattery v0.5.28

- Fixes the repeatable current-beta crash immediately after the hidden F10
  panel attached (`AC18000`).
- Two independent full dumps terminated at `Mewgenics.exe+0x963041` with
  AutoCattery on the crashing thread. The native House-room enumerator was
  calling the legacy component-bucket preparation RVA `0x963040`; in the
  current beta the function begins at `0x963030`, so the old target entered the
  middle of an instruction.
- AutoCattery now accepts the verified stable or current-beta entry only when
  its invariant function signature matches. Unknown or ambiguous layouts
  disable native room enumeration instead of calling an unverified address.
- Preserves the v0.5.27 clean-image MewUI locator, all three installed MODs,
  F10, both normal House buttons, MoveOnly, configuration, protection and local
  data.

Automated verification covers stable/current-beta entry selection, fail-closed
signature rejection, current-beta offline resolution, Debug and Release builds,
the existing CTest suite and DLL loading. Real House entry, F10, both House
buttons and MoveOnly remain player-validation boundaries.
