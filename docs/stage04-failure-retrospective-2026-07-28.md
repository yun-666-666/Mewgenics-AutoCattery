# Stage 04 failed-attempt retrospective — 2026-07-28

## Final disposition

The Stage 04 attempts from commits `70c0e69` and `dbbffa1`, plus the later
uncommitted ClassChooser experiment, are rejected.

The repository and installed MOD must be returned to:

`3484f284e3d160f20da4ceded683b2c2656d9b3d`

`chore: integrate auto cattery Codex toolkit`

That is the player-requested post-Stage-03 baseline. This document is the only
intentional addition to that tree. Stage 04 must be restarted in a new session.

## Player-confirmed UI meaning

The player means the visible **new-day House screen containing the game-owned
`出发!` sign** when referring to the Stage 04 button surface. The supplied
screenshot is the visual source of truth.

Do not tell the player to look for a different screen merely because an
internal runtime context or implementation document uses the name
`ClassChooser`. Internal scene names and player-visible screen meanings are
not interchangeable. If the document and the player-confirmed screenshot
appear inconsistent, stop and map the transition with read-only evidence
before implementing.

## What went wrong

### 1. House-only nodes were attached in ClassChooser

The first Stage 04 implementation correctly observed the runtime context:

`EmbarkSelectionReady scene='ClassChooser'`

It then tried to find `test_button`, `test_text`, and `test_text_2`, which came
from the House-bound MIT example SWF. Those nodes did not exist in
ClassChooser. The log reported:

`AC4105 Embark marker button attach deferred: the embark button or safe demo text node is unavailable`

This should have been treated as a hard Stage 04 blocker. Instead, the next
attempt changed the requested screen interpretation.

### 2. The recommendation button was incorrectly moved into House

The second attempt interpreted the player's House screenshot as permission to
put both active MOD buttons into `HouseStatusUI`. It cloned the demo button and
text nodes and produced two House buttons. This contradicted the staged
product boundary and did not solve the unknown ClassChooser attachment seam.

### 3. `goto-and-play` was misused as visibility control

To hide the House controls in furniture placement mode, an empty MovieClip
frame was appended and repeatedly selected with MewUI's goto-and-play API.
That API does not stop on the requested frame. It continued advancing through
the demo timeline, causing flashing, non-interactable buttons, and exposed
fallback/demo content such as:

- `CLEAN UP!`
- `TEST`
- `TEST2`
- the example counters

Disabling a Button component was also incorrectly assumed to hide its artwork.
It only disables normal component updates and activation; visibility was not
proven.

### 4. A ClassChooser SWF linkage was guessed without live proof

The final uncommitted experiment mechanically renamed the MIT example root
linkage from `HouseStatusUI` to `ClassChooser`. Although the type string existed
in the local executable, that did not prove that a renamed demo SWF was a
valid or isolated ClassChooser extension. It was deployed before a live
rendering proof and again exposed demo artwork.

### 5. Automated tests were treated as stronger evidence than they were

The unit tests validated controller state and safe no-write behavior. DLL load
tests validated lifecycle and architecture. Reproducible SWF generation only
proved byte reproducibility. None of those checks proved:

- correct player-visible screen placement;
- absence of demo artwork in every MovieClip state;
- stable visibility in furniture mode;
- click hit-testing;
- absence of flicker;
- compatibility of a new SWF root linkage.

Reporting the implementation as fixed before those live facts were verified
was incorrect.

## Rules for the next Stage 04 session

1. Start from commit `3484f284e3d160f20da4ceded683b2c2656d9b3d`
   plus this retrospective only. Do not reuse either failed Stage 04
   implementation.
2. Treat the player's screenshot containing `出发!` as the expected visible
   surface unless the player explicitly changes that requirement.
3. First reproduce and record the Stage 03 baseline:
   - normal new-day House screen;
   - furniture placement screen;
   - the result of pressing the game-owned `出发!` sign.
4. Correlate screenshots, scene logs, scene-manager pointers, and component
   types. A scene/type name alone is not sufficient.
5. Before adding UI, use a read-only probe to identify a verified attachment
   seam and a verified way to control visibility. Do not invent node names,
   offsets, linkages, or visibility behavior.
6. Do not use `MewUI_PlayMovieClipFrame` as hide/show unless a stopping state
   and its lifecycle are proven in game.
7. Do not ship the raw MewUI example appearance. A candidate SWF must be
   mechanically checked for all unwanted placed instances and fallback
   strings, including every timeline state.
8. Keep the two defects separate:
   - missing Stage 04 recommendation button on the player-confirmed surface;
   - Stage 03 button incorrectly remaining visible in furniture mode.
9. If either seam remains unknown, stop with probes and logs. A partial
   read-only diagnostic build is preferable to another guessed UI build.
10. Do not create a completion commit until the player verifies all live gates.

## Required live gates before Stage 04 can be accepted

### Normal new-day House surface

- The game-owned `出发!` sign remains unchanged.
- Only the intended MOD controls are visible.
- No `CLEAN UP!`, `TEST`, `TEST2`, counters, navigation samples, or other demo
  content appears.
- The Stage 04 control is stable and clickable, with no flicker.

### Furniture placement

- No AutoCattery active button, label, placeholder, or demo marker is visible.
- Nothing flashes during entry, the whole furniture session, or exit.
- Returning to House does not duplicate or resurrect stale nodes.

### Interaction and safety

- Fifty recommendation-toggle clicks do not change team selection.
- Ten screen transitions create no duplicates or residual nodes.
- No cat, room, save, rest, day-advance, or embark-selection write occurs.
- The latest runtime log contains no repeating attachment error.

## Process lesson

After the first live contradiction, return to evidence and narrow the unknown
seam. Do not compensate for an unknown attachment point by changing the
meaning of the requested screen, cloning more demo UI, or adding animation
workarounds. Live UI evidence outranks a passing controller test.
