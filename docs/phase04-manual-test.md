# Stage 04 player-operated acceptance

This checklist reflects the player-confirmed placement correction from
2026-07-28: the recommendation marker button belongs on the normal House
departure surface shown beside the existing AutoCattery button and the
game-owned `出发!` sign. It must not appear in furniture placement mode.

Codex may build and deploy. The player selects saves and performs gameplay
navigation.

## Before testing

1. Build, deploy, and verify Release:

   ```powershell
   .\tools\build.ps1 -Configuration Release
   .\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   .\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   ```

2. Launch through Mewtator so the AutoCattery data-mod path is active.
3. Back up the selected save and record its timestamp and size.

## Acceptance

1. Enter the normal House screen. Exactly two AutoCattery controls must be
   visible:

   - `Auto-Organize Cattery`
   - `Mark Recommended Combat Cats`

   The game-owned `出发!` control must remain unchanged.

2. Toggle `Mark Recommended Combat Cats` fifty times. Odd clicks show the
   text-only `* #1 DEMO` marker and
   `Visual demo only. No cat was selected.` Even clicks remove both lines and
   restore the mark-button label.

3. Enter furniture placement mode while either AutoCattery button has
   placeholder text visible. Within the scene-loss debounce, both AutoCattery
   buttons and all AutoCattery placeholder/marker text must disappear. The
   controls must remain absent for the entire furniture session.

4. Exit furniture placement mode. The normal House screen must show one clean
   copy of each AutoCattery button. Old `Preview Complete`, manager text, and
   recommendation marker text must not return.

5. Repeat normal House -> furniture mode -> normal House ten times. There must
   be no duplicate buttons, stale text, residual nodes, crash, or increasing
   delay.

6. Enter the game-owned departure/class-selection flow. The House overlay must
   disappear with the House scene and must not cover the class/party selection
   controls.

7. Check the normal House and furniture mode at the active resolution and UI
   scale. For full layout acceptance, also check 1280x720, 1920x1080,
   2560x1440, one ultrawide mode, and UI scales from 80% through 150%.

8. Exit normally and compare the save and cat state with the pre-test record.
   Neither AutoCattery button may select a cat, modify a room, rename a cat,
   write a marker, rest, advance the day, or start an expedition.

9. Inspect the latest AutoCattery log:

   - `AC3100` / `AC3101`: House organize button attach/detach.
   - `AC4100` / `AC4101`: recommendation button attach/detach.
   - `AC4102` / `AC4103`: demo marker show/clear.
   - No repeating `AC4105` should remain on the corrected House surface.
   - Entering furniture mode must produce an unsafe/unknown transition and
     detach both controls.

Stop and keep Stage 04 unaccepted if either button survives furniture mode,
the recommendation button is missing from the normal House screen, any button
duplicates, stale text remains, a game-owned control is obstructed, cat/team
selection changes, or any save/cat/room write occurs.
