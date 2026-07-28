# Stage 04 read-only evidence capture

This run maps the player-visible states to read-only scene/component metadata.
It does not test a Stage 04 button yet and does not authorize Stage 05.

## Preparation

1. Exit Mewgenics before deployment.
2. Launch through Mewtator.
3. Use a backed-up save.
4. At every checkpoint below, take a screenshot and immediately press `F8`
   once. Wait one second before continuing.
5. Do not press `F8` repeatedly at the same checkpoint.

Each `F8` press creates a timestamped JSON file under:

`D:\steam\steam\steamapps\common\Mewgenics\Mods\AutoCattery\diagnostics`

The JSON contains only scene/component metadata, pointer identities, component
flags, and button roles/states. It excludes textures, scripts, game assets,
cats, rooms, saves, and persistence data.

## Required checkpoints

Capture these states in order:

1. `NEW_DAY_HOUSE`: the new-day House where the game-owned `出发!` sign is
   visible, before the day's first expedition.
2. `FURNITURE_ENTER`: historical evidence captured before the player removed
   furniture behavior from Stage 04 scope.
3. `FURNITURE_STAY`: historical evidence only; no longer an acceptance gate.
4. `FURNITURE_EXIT`: historical evidence only; no longer an acceptance gate.
5. `BEFORE_EMBARK_CLICK`: normal new-day House immediately before clicking the
   game-owned `出发!` sign.
6. `CLASS_CHOOSER`: the cat-selection screen reached from that sign.
7. `RETURNED_AFTER_BATTLE`: the House after completing the day's first battle.
8. `NEXT_NEW_DAY`: after ending the day and entering the next new-day House.

Record the screenshot filename beside each checkpoint. The screenshot time
and generated JSON filename must be within a few seconds of each other.

## Evidence gate

The capture is sufficient only if it lets us prove:

- which scene/context corresponds to each visible checkpoint;
- a stable read-only difference between `NEW_DAY_HOUSE`,
  `RETURNED_AFTER_BATTLE`, and `NEXT_NEW_DAY`;
- the lifecycle and generation changes around the game-owned `出发!` sign;
- a safe attachment seam and a visibility/removal mechanism can be tested
  without modifying the original sign or exposing demo UI.

If the metadata does not prove any item, Stage 04 remains partially complete
and the next probe must stay read-only.
