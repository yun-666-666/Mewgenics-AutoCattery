# AutoCattery

Phase 03 house-button shell for a Windows x64 Mewgenics DLL mod.

Current behavior is intentionally non-destructive:

- resolves the installed Mewjector v3 API;
- initializes structured logging and layered JSON configuration;
- initializes a module registry;
- starts the MIT-licensed MewUI API 1.2.0 from source;
- recognizes the stable `House` scene, debounces unload/reload transitions, and
  publishes generation-tagged context events;
- recognizes the player-verified `ClassChooser` embark-selection scene;
- injects one `AutoCattery.House.AutoOrganizeButton` in the stable `House`
  scene and disables/detaches it when the context becomes unsafe;
- debounces clicks for 500 ms and shows a localized placeholder stating that
  no cats were modified;
- provides an opt-in F8 scene-summary export for diagnostics;
- never reads or writes cats, rooms, or saves.

Build:

```powershell
.\tools\build.ps1 -Configuration Release
```

Deploy after building:

```powershell
.\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
.\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
```

Mewjector scans only the immediate `mods` directory, so the DLL is deployed as
`mods\AutoCattery.dll`; configuration and logs live under
`mods\AutoCattery\`.

See `docs/phase03-manual-test.md` for the player-operated button validation
steps. Codex does not navigate saves or gameplay screens.
