# AutoCattery v0.5.29

- Fixes current-beta automatic House organization remaining unavailable after
  the v0.5.28 House-entry crash repair. The live log showed 41 mapped cats but
  zero native room components; the stable-only HouseCat vtable check was
  discarding every current room pointer before preview construction.
- HouseCat validation now uses the already-proven engine type name. If the
  native room bucket is unavailable, distinct occupied room pointers provide
  the safe room-count fallback while incomplete or ambiguous room identity
  still rejects preview or execution.
- Adds the verified current-beta native House move entry at `0x2E88D0` while
  preserving the stable `0x2E7DB0` entry. Unknown or ambiguous signatures do
  not invoke a move.
- Fixes combat recommendation names no longer opening the selected cat. The
  current beta now resolves the detail opener `0xEC7B0`, cat target resolver
  `0xF0570`, drawer resolver `0x1A9E10`, and their matching call sites as one
  fail-closed layout.

Automated verification covers stable/current-beta native layout selection,
room-count fallback, fail-closed rejection, current-beta offline EXE
resolution, Debug and Release builds, CTest 5/5 and DLL loading. Player testing
passed on both the current beta and official game versions: the full preview,
second-click movement, and click-to-open combat-cat detail flows all worked.
