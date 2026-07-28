# Phase 02 manual game test

Codex may launch the game only for a load/no-crash smoke test. The player
performs all save selection and gameplay navigation.

## Preparation

1. Exit Mewgenics before replacing `AutoCattery.dll`.
2. Deploy the Release build.
3. To capture an unknown scene, create or merge
   `Mods/AutoCattery/config/user_config.json` with:

   ```json
   {
     "ui": {
       "show_debug_overlay": true
     }
   }
   ```

   Despite the compatibility setting name, Phase 02 writes diagnostics logs
   only and does not draw an overlay.

## House lifecycle

1. Start the game normally and enter the house shown in the supplied
   screenshots.
2. Wait two seconds.
3. End/rest for the day and return to the house.
4. Repeat the leave/return sequence five times if practical.
5. Close the game normally.
6. In `Mods/AutoCattery/logs/auto_cattery.log`, confirm each cycle contains:
   - `Context=HouseReady scene='House'`;
   - `Context=UnsafeTransition` while the house unloads;
   - a later `HouseReady` with a larger `generation`.

Cat count, position, animation, and rest state must not create a context event
unless the game actually unloads/reloads the `House` scene.

## Embark-selection regression test

1. Navigate to the screen where cats can be selected for the next expedition.
2. Wait two seconds.
3. Return to the house and close the game normally.
4. Confirm the log contains
   `Context=EmbarkSelectionReady scene='ClassChooser'`, followed by an
   `UnsafeTransition` or `HouseReady` event when leaving.

The player capture on 2026-07-28 identified `ClassChooser` as the real
embark-selection scene. F8 export is only needed again after a game update or
if the context event stops appearing.
