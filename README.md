# AutoCattery

Phase 02 scene-context foundation for a Windows x64 Mewgenics DLL mod.

Current behavior is intentionally non-destructive:

- resolves the installed Mewjector v3 API;
- initializes structured logging and layered JSON configuration;
- initializes a module registry;
- starts the MIT-licensed MewUI API 1.2.0 from source;
- recognizes the stable `House` scene, debounces unload/reload transitions, and
  publishes generation-tagged context events;
- keeps embark selection disabled until its real scene signature is captured;
- provides an opt-in F8 scene-summary export for diagnostics;
- never creates a formal button and never reads or writes cats, rooms, or saves.

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

See `docs/phase02-manual-test.md` for the player-operated scene validation
steps. Codex does not navigate saves or gameplay screens.
