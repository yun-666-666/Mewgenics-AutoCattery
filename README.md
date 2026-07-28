# AutoCattery

Phase 01 engineering skeleton for a Windows x64 Mewgenics DLL mod.

Current behavior is intentionally non-destructive:

- resolves the installed Mewjector v3 API;
- initializes structured logging and layered JSON configuration;
- initializes a module registry;
- starts the MIT-licensed MewUI API 1.2.0 from source without creating a formal
  button during phase 01;
- never reads or writes cats, rooms, scenes, or saves.

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
