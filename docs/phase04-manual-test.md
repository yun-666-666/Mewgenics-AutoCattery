# Stage 04 player-operated acceptance

Codex may build, deploy, and perform a no-save launch smoke test. The player
selects saves and navigates gameplay. Back up the selected save before this
test even though Stage 04 contains no cat, room, selection, or persistence
adapter.

## Before testing

1. Build and deploy Release:

   ```powershell
   .\tools\build.ps1 -Configuration Release
   .\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   .\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   ```

2. Launch Mewgenics through Mewtator so the AutoCattery data-mod path is
   active.
3. Copy the save to a separate backup location. Record its timestamp and size.

## Acceptance

1. Enter and leave the `ClassChooser` embark-selection screen ten times.
   Exactly one `Mark Recommended Combat Cats` button must appear there and it
   must not remain visible in House or other screens.
2. Toggle the button fifty times. Odd clicks must show the text-only
   `* #1 DEMO` marker and `Visual demo only. No cat was selected.` Even clicks
   must restore the original empty overlay.
3. Select and deselect cats while the marker is visible. The selected cats,
   selection count, order, and all original controls must behave exactly as
   before.
4. Scroll or page if the screen offers those controls. The marker may remain
   in the MOD overlay, but it must not attach to or identify a real cat.
5. Leave the screen while the marker is visible. It must be cleared before the
   next `ClassChooser` generation, with no duplicate or residual nodes on
   return.
6. Check 1280x720, 1920x1080, 2560x1440, one ultrawide mode, and UI scales
   from 80% through 150%. The button and demo text must be readable and must
   not cover an original skill, state, cat-selection control, or navigation
   control.
7. Exit normally and compare the save timestamp, size, selected team, and cat
   state with the pre-test record. Stage 04 must not persist any marker or
   selection change.
8. Inspect the latest AutoCattery log. `AC4100` through `AC4103` should record
   attach, detach, show, and clear actions without per-frame spam. There must
   be no cat, save, room, automatic team-composition, rest, or embark-selection
   write.

Stop and keep Stage 04 unaccepted if the button duplicates, overlaps an
original control, survives outside `ClassChooser`, changes selection state,
identifies an arbitrary real cat as recommended, or changes the save.
