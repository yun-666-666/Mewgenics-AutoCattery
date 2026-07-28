# Stage 03 player-operated acceptance

Codex may build, deploy, and perform a no-save launch smoke test. The player
selects saves and navigates gameplay.

## Before testing

1. Build and deploy Release:

   ```powershell
   .\tools\build.ps1 -Configuration Release
   .\tools\deploy.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   .\tools\verify_install.ps1 -GameRoot 'D:\steam\steam\steamapps\common\Mewgenics'
   ```

2. Start Mewtator and launch Mewgenics from it. Launching directly from Steam
   may omit the AutoCattery data-mod path.
3. Copy the save to a separate backup location before loading it.
4. Record the save timestamp and size.

## Acceptance

1. Enter and leave the House ten times. Exactly one
   `Auto-Organize Cattery` button must appear in the House each time and it
   must not remain visible outside the House.
2. Click the button twenty times, including rapid clicks. Each accepted click
   must show `Auto Cattery Manager` and
   `Button connected. This preview did not modify any cats.` No adjacent
   control may activate.
3. Open pause/save UI and transition scenes. The button must not accept input
   while the scene context is unsafe.
4. Check 1280x720, 1920x1080, 2560x1440, one ultrawide mode, and UI scales from
   80% through 150%. The button and placeholder text must remain readable and
   must not cover an original control.
5. Exit normally and compare the save timestamp, size, and in-game cat/room
   state with the pre-test record. Stage 03 must not change them.
6. Inspect the latest AutoCattery log. `AC3100`, `AC3101`, and `AC3102` should
   record attach, detach, and accepted clicks without per-frame spam.

Stop and do not begin Stage 04 if the button duplicates, overlaps original UI,
activates an adjacent control, survives outside House, or any save/cat state
changes.
