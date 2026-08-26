# AutoCattery v0.5.17

## Expedition sleep mode

- Suspends AutoCattery's UI, workflow, recommendation, MoveOnly, and runtime
  configuration polling while a ready `Battle` or `Map` scene is present.
- Keeps only a minimal once-per-second scene probe so the MOD can detect the
  end of the expedition and wake automatically.
- Restores the existing responsive lifecycle after the expedition clears,
  including normal House UI reattachment.
- Checks runtime configuration file timestamps at most twice per second outside
  expeditions instead of once per UI frame.

This directly addresses the sustained combat stutter observed in the
2026-08-26 player log. The same run contained 204 caught C++ exceptions during
combat; Mewjector logging remains available for diagnosis, but AutoCattery now
does no business, UI, or filesystem polling during the expedition.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
