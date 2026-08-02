# AutoCattery v0.5.3

## What changed

- Fixed F10 management-panel click alignment on smaller screens and mixed-DPI
  Windows setups. Panel clicks now use the client coordinates delivered with the
  mouse message before mapping them to the SWF's 1280x720 virtual stage.
- Added automated coverage for widescreen, 4:3, and boundary coordinate mapping.
- Existing F10 pages, MoveOnly planning, protection rules, and the external
  settings editor are unchanged.

## Installation reminder

Use `AutoCattery-v0.5.3-Windows-x64.zip` for installation. Install
[Mewjector](https://github.com/githubuser508/mewjector) and
[Mewtator](https://github.com/dancomstock/mewtator) first, then copy the package's
`Mewjector/mods` contents to the game's `mods` folder and copy
`Mewtator/AutoCattery` into `Mewtator/mods`, producing
`Mewtator/mods/AutoCattery`. Add `AutoCattery` to `Mewtator/mods/modlist.txt` and
launch the game through Mewtator.

If you previously saw a vertical offset when clicking settings on a small or
scaled display, replace both the runtime DLL and the Mewtator data-mod files
with this release before testing again.
