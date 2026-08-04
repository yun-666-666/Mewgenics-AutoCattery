# AutoCattery v0.5.4

## What changed

- Hardened `AutoCatterySettings.exe` to reduce heuristic antivirus false
  positives. The cat protection manager now opens inside the existing process
  instead of starting a second copy of the executable.
- Added standard Windows product name, description, original filename, and
  `0.5.4.0` file/product version metadata.
- Embedded an application manifest that explicitly requests normal-user
  (`asInvoker`) privileges and declares Per-Monitor-V2 DPI awareness.
- Added release build gates that reject the settings executable if it is not
  x64, lacks product metadata, or imports self-launch/remote-process APIs.

No F10 panel, MoveOnly planning, settings, or cat protection behavior was
removed. The external editor remains available as a fallback.

## Antivirus note

The v0.5.3 settings executable was flagged by 2 of 70 VirusTotal engines with
generic machine-learning labels while 68 engines reported it clean. The v0.5.4
executable removes an unnecessary self-launch behavior and adds normal Windows
identity metadata. The Release executable also passes a local Microsoft
Defender custom scan with current signatures.

AutoCattery binaries are still unsigned because this project does not currently
have a trusted code-signing certificate. Antivirus cloud reputation can take
time to update, so no release can guarantee that every heuristic engine will
immediately report an unsigned new binary as clean. The matching source archive
is provided for inspection and reproducible review.

## Installation reminder

Use `AutoCattery-v0.5.4-Windows-x64.zip` for installation. Install
[Mewjector](https://github.com/githubuser508/mewjector) and
[Mewtator](https://github.com/dancomstock/mewtator) first, then copy the
package's `Mewjector/mods` contents to the game's `mods` folder and copy
`Mewtator/AutoCattery` into `Mewtator/mods`. Add `AutoCattery` to
`Mewtator/mods/modlist.txt` and launch the game through Mewtator.
