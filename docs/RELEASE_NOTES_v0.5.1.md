# AutoCattery v0.5.1

## What changed

- Clarified the difference between the developer `source.zip` and the installable
  `Windows-x64.zip` package.
- Added exact Mewjector and Mewtator installation paths, startup requirements, and
  the F10 preview/MoveOnly workflow to the Chinese and English READMEs.
- Added `Documentation/USER_GUIDE.md` to the Windows release package.
- Kept the MOD runtime behavior unchanged; this is a packaging and documentation
  update on top of v0.5.0.

## Installation reminder

Use `AutoCattery-v0.5.1-Windows-x64.zip` for installation. The
`AutoCattery-v0.5.1-source.zip` archive contains source code and development files;
it must not be copied directly into the game's `mods` directory.

After extracting the Windows package, copy the contents of `Mewjector/mods` to the
game's `mods` folder, copy `Mewtator/AutoCattery` to the Mewtator `mod_folder`, add
`AutoCattery` to `modlist.txt`, and launch through Mewtator. In House, press `F10`,
preview first, and click again only after reviewing the plan.
