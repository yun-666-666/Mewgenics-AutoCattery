# AutoCattery

[中文](README.md)

AutoCattery is a Windows x64 Mewgenics mod for deterministic, safety-gated
cattery organization.

## Main features

- Adds an Auto-Organize Cattery button to the House scene. The first click
  creates a preview; the second click applies verified native room moves for
  the supported game build.
- Balances 2–4 available rooms by population, sex mix, cat potential, breeding
  stage, and live furniture-derived room attributes.
- Press `F10` in the House to open the management panel. Full Preview is a
  top-level page beside Settings, Cat Protection, and Close.
- Full Preview shows every planned cat move, its source, destination, sex,
  potential, and reason, plus each room's before/after population and
  female-to-male ratio.
- Chinese and English are available. Chinese is the default. Because no
  verified game-language API is available for the supported build, language is
  selected explicitly at the bottom of the F10 Settings page and persisted.
- Cat protection supports no-cull, no-move, fully unmanaged, combined
  protection, and fixed-room rules using stable cat identities.
- Optional local cat-data collection is disabled by default. When enabled at
  the bottom of Settings, technical planning snapshots are written to
  `Mods\AutoCattery\AutoCatteryData`. Cat names, save names and paths, OS user
  names, machine IDs, and account IDs are excluded. Nothing is uploaded.

The F10 panel caches resolved native MewUI text nodes and updates only changed
text and frames. This removes the former repeated full-scene scans that caused
visible stalls on 25-cat and 79-cat saves.

## Installation

The Windows release archive has two payloads:

1. Copy the contents of `Mewjector\mods` to the game's `mods` folder.
2. Copy `Mewtator\AutoCattery` into the Mewtator mod folder and enable
   `AutoCattery` in `modlist.txt`.
3. Launch the game through Mewtator. Mewjector and a compatible data-mod loader
   are prerequisites.

The currently supported `Mewgenics.exe` is 21,981,184 bytes with SHA-256
`C3A41E436A93FA58CD386EC46DAD5C2A6F21A583D33C3A57A15A2604C726439E`.
Unsupported builds do not enable the native move entry point.

## Build

```powershell
.\tools\build.ps1 -Configuration Debug
.\tools\build.ps1 -Configuration Release
```

See [Game value reference](docs/GAME_VALUE_REFERENCE.md),
[third-party notices](THIRD_PARTY_NOTICES.md), and
[acknowledgements](ACKNOWLEDGEMENTS.md).
