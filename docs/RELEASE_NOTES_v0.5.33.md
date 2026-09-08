# AutoCattery v0.5.33 (Pre-release)

This pre-release adds the Stage 38 House-management tools for the current
Mewgenics build.

## Added

- An F10 **Senior Cats** page.  It lists only living cats that the game itself
  marks as old; it does not substitute a MOD-defined age threshold.  The page
  supports saved-state refresh, score/age ordering, paging, and opening the
  game's native cat details.
- A separate, preview-first **Dead Cat Delivery** flow.  It lists eligible
  dead House cats in stable ID order, excludes MOD-protected and fixed-room
  cats, and sends each confirmed cat through the game's native pipe and Organ
  Grinder route.
- Per-cat runtime verification, cancellation, and audit logging for the
  delivery queue.  The queue waits for the game's own NPC/drawer cleanup
  before it starts another cat; it never saves, advances a day, chooses an
  expedition, or automatically composes a combat team.

## Fixed

- Dead-cat parsing now handles the current variable-length metadata after a
  cat's death state, so valid post-combat saves can populate the delivery
  preview.
- Delivery cancellation consumes its Esc/F10 input while native NPC UI is
  transitioning, preventing that same keypress from being passed on to the
  game.  The queue still stops only future deliveries; a cat already handed to
  the native flow may complete.
- Delivery sequencing waits for native drawer cleanup between cats rather than
  reopening details during the previous NPC flow's teardown.
- Breeding pair ranking now honors the configured stat weights.

## Verification and current limit

Release builds and all six CTest targets passed.  Player testing confirmed the
Senior Cats page, one dead-cat delivery, and delivery of two consecutive cats.
The final in-game validation still required before this becomes a stable
release is cancelling during a cat-detail delivery step with Esc, confirming
that the queue stops and Mewgenics does not crash.  This is therefore a
pre-release, not a claim of full player acceptance.

## Install and use

Extract the Windows x64 package as described in `Documentation/README.md`.
Before confirming a dead-cat delivery, save normally.  To undo an unsaved
delivery, reload that pre-delivery save before the game saves again.
