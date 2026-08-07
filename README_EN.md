# AutoCattery

[中文](README.md)

AutoCattery is a Windows x64 Mewgenics cattery-management MOD made by
**OpenAI GPT-5.6** from project requirements, code review, and hands-on player
feedback. It is not an official game component and does not modify original
game files, Steam Cloud data, or player save databases.

## Current capabilities

- Adds an **Auto-Organize Cattery** button in the stable `House` scene.
- The first click creates a preview. The second click uses the native House
  room path available in the current runtime to move cats. Large plans submit
  at most eight native moves per UI tick, refresh and revalidate after each
  batch, and cancel remaining batches when House is left.
- The preview overlays the current in-game CatId-to-room mapping over older
  unsaved save distribution. A manual move after preview invalidates it and no
  movement is executed.
- Refreshes runtime data whenever entering House and before every preview, then
  selects the matching save by current cat count so a previous save is not reused.
- Cats currently outside an ordinary room but still belonging to the House are
  treated as unassigned sources and can be moved to verified ordinary rooms.
- Supports the verified 2-, 3-, and 4-room layouts:
  - `Attic`
  - `Floor1_Large`
  - `Floor1_Small`
  - `Floor2_Large`
- Balances actual room counts and reads furniture-derived comfort, stimulation,
  health, mutation, and attraction attributes. Cats are assigned by room purpose,
  not by a fixed room order.
- Only a confirmed female/male breeding pair enables sex balancing. The separate
  breeding target room receives the best achievable basic female/male mix after
  fixed and immovable residents are counted. Combat staging, nursery, and
  ordinary rooms ignore sex and never replace higher-potential cats for a ratio.
  Combat staging prefers health and comfort instead of reserving the highest-
  stimulation room. With kitten separation enabled, an additional room becomes
  a health/comfort-oriented nursery. Repeating organization restores the room
  goal after a manual same-count swap.
- Seven base stats are read when present in early saves. Sexuality and kinship are
  read only after the corresponding `tink_sexuality`, `tink_inbreeding`, and
  `tink_relationships` progress is actually unlocked; locked dimensions do not
  affect scoring.
- After complete breeding data is unlocked, known female/male adult pairs use seven-stat gaps,
  cached game COI, and sexuality. Skill and mutation weights stay off before
  stable all-seven breeding, then abilities, passives, disorders, mutations, and
  birth defects are considered. Breeding-room selection rejects comfort at or
  below the confirmed `-10` automatic-failure boundary when a viable room exists,
  balances comfort with stimulation, and uses Mutation as a stable-stage
  tie-breaker. Remaining breeding-room slots use disjoint eligible pair rankings
  instead of unrelated stable ordering; this improves the pool without claiming
  to lock the game's actual mating choice.
- The F10 Cat Protection page lists local saves and cats for protection
  management. Only an explicit **Apply Protection** writes `NoCull`, `NoMove`,
  `NoCullOrMove`, `FullyUnmanaged`, or `fixed_room`; rules use stable cat
  fingerprints rather than save-file identity.
- Protection rules are read before preview and execution. A change during the
  operation cancels it and requires a new preview.
- Combat history, profession, injury, or whether a cat has fought are not used as
  automatic exclusion rules.
- Provides next-day combat recommendations, cat details, in-game settings and
  protection, backups, and offline restore source tools.
- Press `F10` in House to open or close the management panel; `Esc` closes it.
  The panel contains Settings, Cat Protection, Full Preview, and Close.
- Full Preview is read-only and shows each planned cat's source, target, sex,
  potential, reason, and before/after room counts and sex ratios.
- F10 Settings can set `0`–`99` level-up rerolls for every base and advanced
  player class; the default is `3` and a game restart is required. When the
  verified sibling `SkillsPassivesFirstData` patch is installed, AutoCattery
  dynamically mirrors the current control-panel value into both data mods, so
  an old fixed value of 3 cannot override later choices.
- Chinese and English can be selected at the bottom of Settings; Chinese is the
  default. Since no verified game-language API is available, the choice is
  explicit and persisted.
- Optional local cat-data collection is off by default. When enabled, it writes
  technical planning data under `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData`.
  It excludes cat names, save names and paths, OS usernames, machine IDs, and
  account IDs, and never uploads automatically. See the issue-feedback section.

The current live execution capability is `MoveOnly`. Room attributes, first-stage
breeding pairing, default base-stat reads, room identity caching, and native
movement of unassigned cats are implemented. Real culling is not enabled. See
[`docs/pre-completion-functional-roadmap.md`](docs/pre-completion-functional-roadmap.md).

House UI text nodes are cached and updated only when content changes. The
recommendation list also writes through cached text nodes, and unavailable
recommendations are synchronized only when the state changes instead of being
cleared every frame. Root lookup runs only during attachment, deduplicates
component roots, stops at the first root containing the MOD assets, and avoids
reading every component type. Failed attachment is retried at most twice per
second rather than every UI frame.

### F10 management panel

Press `F10` in House to open or close the panel; `Esc` closes it:

- **Settings** changes planning parameters, interface language, and optional data
  collection.
- **Cat Protection** assigns `NoCull`, `NoMove`, `NoCullOrMove`, `FullyUnmanaged`,
  or a fixed room to a selected cat.
- **Full Preview** shows room totals and each planned move after one
  Auto-Organize click; it is read-only and never moves cats by itself.

For large saves, the panel resolves native MewUI text nodes once when attached and
updates only changed text and frames instead of rescanning the entire House scene
for every row and refresh.

## Installation

Download `AutoCattery-vX.Y.Z-Windows-x64.zip` from GitHub Releases.
`AutoCattery-vX.Y.Z-source.zip` contains the source code so that the MOD can be
modified.

### Install prerequisites

AutoCattery requires [Mewjector](https://github.com/githubuser508/mewjector) and
[Mewtator](https://github.com/dancomstock/mewtator) to be installed and enabled first:

1. Follow the [Mewjector](https://github.com/githubuser508/mewjector) release
   instructions to install it in the Mewgenics game root. Confirm that `version.dll`
   is next to `Mewgenics.exe` and keep the game's `mods\\` folder.
2. Follow the [Mewtator](https://github.com/dancomstock/mewtator) release instructions
   and run it once. Confirm that
   `Mewtator\\config.json` exists in the game root. Set `mod_folder` to Mewtator's
   `mods\\` folder (usually `Mewtator\\mods`) and use that directory's
   `modlist.txt` to manage data MODs.
3. After both prerequisites work, install AutoCattery as described below. Launch
   Mewgenics through Mewtator; launching directly from Steam does not load the
   Mewtator data MOD.

For a Windows release archive:

1. Extract `Windows-x64.zip`; do not place the archive's outer folder as an extra
   nested level inside `mods`.
2. Copy `Mewjector\\mods\\AutoCattery.dll` and
   `Mewjector\\mods\\AutoCattery\\` from the archive into the game's `mods\\`
   folder (or the same directory scanned by Mewjector).
3. Copy `Mewtator\\AutoCattery\\` into the Mewtator `mods\\` folder so the result
   is `Mewtator\\mods\\AutoCattery\\`, then add one line containing `AutoCattery`
   to that folder's `modlist.txt`.
4. Launch the game through Mewtator. Mewjector and a compatible data-mod loader
   are prerequisites; launching only from Steam does not load the Mewtator data MOD.

In `House`, press `F10` to open the panel. The first **Auto-Organize Cattery** click
creates a preview; review the room and cat source/target details, then click again to
run MoveOnly. Press `Esc` or **Close** to leave the panel. The Settings, Cat Protection,
Full Preview, and every setting's effect are described in
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).

The runtime DLL is installed at `Mewgenics\\Mods\\AutoCattery.dll`. Configuration
and logs are in `Mewgenics\\Mods\\AutoCattery\\`. The button SWF and text patch
are supplied by the enabled Mewtator data mod.

## Optional cat data and issue feedback

Collection is off by default. When enabled, each successful new preview writes a
deduplicated JSON snapshot containing CatId, room, sex, life stage, base stats,
skill/passive/mutation IDs, classification, protection result, planned moves, and
room attributes. It does not contain names, save paths, usernames, machine IDs, or
account IDs. Existing files are not deleted when collection is turned off.

To provide data for performance or planning analysis:

1. Open **Collect cat data (off by default)** in F10 Settings.
2. Re-enter House and reproduce lag, an incorrect preview, failed movement, or a
   post-update compatibility problem.
3. Exit the game and compress the entire
   `Mewgenics\\Mods\\AutoCattery\\AutoCatteryData` folder into a ZIP. Upload only
   that ZIP; do not include saves, `user_config.json`, unrelated personal files,
   account screenshots, or the whole game directory.
4. Open the repository's [GitHub Issues](https://github.com/yun-666-666/Mewgenics-AutoCattery/issues),
   describe the game version, cat count, reproduction steps, expected result, and
   actual result, then attach the ZIP. Turn collection off afterward.

Inspect the archive yourself before uploading and keep only the JSON technical
snapshots. The archive is intended to help GPT-5.6 analyze planning and performance
paths; the MOD never sends it automatically.

## Protection behavior

Enter House, press `F10`, and select **Cat Protection**. Selecting a save only
chooses which cats to list. Select a cat, choose a level or fixed room, and press
**Apply**. **Remove** deletes only that cat's player rule. The project no longer
builds or ships a helper executable. Rules are stored in
`Mewgenics\\Mods\\AutoCattery\\config\\protection.json`; the default records list
is empty and no save or CatId is pre-protected.

Manual format example:

```json
{
  "schema_version": 1,
  "records": [
    {
      "cat_id": 123,
      "level": "NoMove",
      "identity_token": "generated by the protection manager"
    },
    {
      "cat_id": 456,
      "level": "NoCull",
      "identity_token": "generated by the protection manager",
      "fixed_room": "Attic"
    }
  ],
  "blacklist": []
}
```

Use the protection manager to generate a real `identity_token`; do not copy the
example value. `fixed_room` must be an ordinary room present in the current save.
Corrupt files or missing target rooms stop the current operation safely.

## Build

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

## Deploy

Replace `<GAME_ROOT>` with the local Mewgenics installation directory:

```powershell
.\tools\deploy.ps1 -GameRoot '<GAME_ROOT>'
.\tools\verify_install.ps1 -GameRoot '<GAME_ROOT>'
```

Mewjector scans the game's immediate `Mods`/`mods` directories, so the runtime DLL
is installed at `Mewgenics\\Mods\\AutoCattery.dll`. Configuration and logs are in
`Mewgenics\\Mods\\AutoCattery\\`; the button SWF and text patch are supplied by
the enabled Mewtator data mod.

## Acknowledgements and references

Thanks to the Mewgenics modding community and the following projects. They are
references for interfaces, structure, compatibility, or planning ideas only;
AutoCattery does not copy their closed binaries, SWFs, FLAs, game assets, or
personal data:

- [Mewjector](https://github.com/githubuser508/mewjector): runtime DLL loading and
  module registration; AutoCattery enters the game through it.
- [MewUI API](https://github.com/Pseudonym-Tim/mewgenics-ui-api): House scene
  discovery, MewUI lifecycle, node lookup, direct text, and input interception.
- [JSON for Modern C++](https://github.com/nlohmann/json): parsing and writing
  configuration, protection rules, and local diagnostic JSON.
- **AutoCattery Codex Toolkit** (the user-provided MIT reference toolkit):
  deterministic scoring, retained-pool classification, protection permission
  intersection, and preview/execution boundaries. Its game fields and capacity
  examples were not copied directly.
- **Push To Meow**: reference for MOD directory layout, loader compatibility, and
  release shape.
- **Quick-Cleanup**: reference for room-organization user flow and safety messaging.

The code and documentation were made by **OpenAI GPT-5.6** under the project's
direction and hands-on validation. See [`ACKNOWLEDGEMENTS.md`](ACKNOWLEDGEMENTS.md)
and [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for license and revision
records. The complete button guide is [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md).
