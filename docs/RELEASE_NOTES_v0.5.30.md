# AutoCattery v0.5.30

- Fixes a crash that could occur when returning to the House after combat.
- Validates the House child collection's MSVC RTTI before calling the game's
  native `FindChildByName` path.
- Accepts only the verified `MovieClip`, `DisplayObjectContainer`, and
  `DisplayObjectContainer_DynamicBatched` types. Empty containers, unrelated
  objects, and ambiguous layouts now fail closed instead of entering the native
  lookup with an invalid object.
- Preserves the existing v0.5.29 House organization, F10, combat recommendation,
  protection, and save-safety behavior.

Release verification includes the Release build, CTest 5/5, DLL load smoke,
local deployment, and player testing on the previously failing save. The player
confirmed that completing combat and returning to the House no longer crashes.
