# AutoCattery

[中文](README.md)

AutoCattery is a Windows x64 Mewgenics cattery-management mod. It was made by
**OpenAI GPT-5.6** from wordy's requirements, code review, and hands-on player
validation. It is not an official game component and does not modify original
game files, Steam Cloud data, or player save databases.

## Current capabilities

- Adds an **Auto-Organize Cattery** button in the House scene.
- The first click creates a preview. The second click uses the native House
  room path that is available in the current runtime to move cats.
- Refreshes live cat and room state before entering House and before every new
  preview, then selects a save by the live cat count (including 25 and 79 cat
  saves).
- Treats cats whose live room pointer is null as unassigned sources; only
  verified ordinary rooms are movement targets.
- Supports the verified ordinary rooms `Attic`, `Floor1_Large`,
  `Floor1_Small`, and `Floor2_Large`, with 2-, 3-, and 4-room balancing.
- Reads furniture-derived room attributes and uses actual cat sex, potential,
  breeding stage, protection rules, and unlocked data when planning.
- Keeps protection rules fail-closed. `No Cull`, `No Move`, `No Cull or Move`,
  `Fully Unmanaged`, and fixed-room rules require an explicit player action.
- Provides next-day combat recommendations, cat details, an external settings
  editor, backups, and offline restore tools.
- Press `F10` in House to open or close the management panel. `Esc` closes it.
  The panel has Settings, Cat Protection, Full Preview, and Close pages.
- Full Preview is read-only and shows each planned cat's source room, target
  room, sex, potential, reason, and before/after room totals and sex ratios.
- Chinese and English are selectable at the bottom of Settings; Chinese is the
  default. Because no verified game-language API is available, the choice is
  explicit and persisted.
- Optional local cat-data collection is off by default. When enabled, it writes
  technical planning snapshots to `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData`.
  It excludes cat names, save names and paths, OS usernames, machine IDs, and
  account IDs, and never uploads automatically.

For 25- and 79-cat saves, the F10 panel resolves native MewUI text nodes once
when attached and updates only changed text and frames. It no longer scans the
entire House scene once per row and refresh.

## Game-version compatibility

The mod no longer refuses to enable because `Mewgenics.exe` has a different
file size or SHA-256. Startup only confirms that the game directory contains a
regular `Mewgenics.exe`. Native adapters still validate pointers, components,
and call results at runtime. If a future game changes its internal layout, a
native move or probe can fail safely and log the reason while the panel,
read-only preview, and external editor remain available. After a game update,
preview first and do not repeat movement until the result and log look correct.

## F10 management panel

See the bilingual [button-by-button user guide](docs/USER_GUIDE.md) for every
control and its effect.

## Optional cat data and issue feedback

The feature is off by default. When enabled, each new preview writes a
deduplicated JSON snapshot containing technical planning data such as CatId,
room, sex, life stage, base stats, skill/passive/mutation IDs, classification,
protection result, planned moves, and room attributes. It does not contain
names, save paths, usernames, machine IDs, or account IDs.

To provide data for performance or planning analysis:

1. Open **Collect cat data (off by default)** in F10 Settings.
2. Reproduce the lag, incorrect preview, failed movement, or post-update issue.
3. Exit the game and compress the entire `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData`
   folder into a ZIP. Upload only that ZIP; do not include saves, the whole game
   directory, or unrelated personal files.
4. Open the repository's [GitHub Issues](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues),
   describe the game version, cat count, steps, expected result, and actual
   result, then attach the ZIP to the new issue. Turn collection off afterward.

Inspect the archive yourself before uploading and remove anything unexpected.
The archive is intended to help GPT-5.6 analyze planning and performance paths;
the mod does not send it automatically.

## Protection behavior

Run `Mewgenics\\Mods\\AutoCattery\\AutoCatterySettings.exe` and choose **Manage Cat
Protection**, or use the F10 Cat Protection page. Selecting a save only chooses
which cats to list. Select a cat, choose a level or fixed room, and press
**Apply**. **Remove** deletes only that cat's player rule. Rules are stored in
`Mewgenics\\Mods\\AutoCattery\\config\\protection.json`; the default records list
is empty and no player save or CatId is pre-protected.

## Build and deploy

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
.\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
```

Mewjector scans the game's `Mods`/`mods` directories, so the runtime DLL is
installed at `Mewgenics\\Mods\\AutoCattery.dll`. Configuration and logs are in
`Mewgenics\\Mods\\AutoCattery\\`; the button SWF and text patch are supplied by
the enabled Mewtator data mod.

## Acknowledgements and references

Thanks to the Mewgenics modding community and the following projects. They are
references for interfaces, structure, compatibility, or planning ideas only;
AutoCattery does not copy their closed binaries, SWFs, FLAs, game assets, or
personal data:

- [Mewjector](https://github.com/githubuser508/mewjector): runtime DLL loading
  and module registration; AutoCattery enters the game through it.
- [MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api): House scene
  discovery, MewUI lifecycle, node lookup, direct text, and input interception.
- [JSON for Modern C++](https://github.com/nlohmann/json): parsing and writing
  configuration, protection rules, and local diagnostic JSON.
- **AutoCattery Codex Toolkit** (the user-provided MIT reference toolkit):
  deterministic scoring, retained-pool classification, protection permission
  intersection, and preview/execution boundaries; its game fields and capacity
  examples were not copied directly.
- **Push To Meow**: reference for mod directory layout, loader compatibility,
  and release shape.
- **Quick-Cleanup**: reference for room-organization user flow and safety
  messaging.

The code and documentation were made by **OpenAI GPT-5.6** under wordy's
product direction and hands-on validation. See
[`ACKNOWLEDGEMENTS.md`](ACKNOWLEDGEMENTS.md) and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for license and revision
records. The complete button guide is [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).
