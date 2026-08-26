# AutoCattery v0.5.18

## Sticky expedition sleep

- Keeps AutoCattery asleep after the first ready `Battle` or `Map` scene,
  including temporary readiness gaps between encounters and animations.
- Wakes immediately for save or class selection, or after a ready `House`
  scene remains stable for three continuous seconds.
- Prevents repeated combat-time suspend/resume cycles and avoids attaching the
  House panel to short-lived return scenes.

The 2026-08-26 follow-up log showed dozens of `AC1203`/`AC1204` pairs during a
single expedition because `Battle` and `Map` temporarily stopped reporting as
ready. Those gaps no longer resume configuration, controller, workflow, or UI
polling.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
