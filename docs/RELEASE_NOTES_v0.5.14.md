# AutoCattery v0.5.14

## Large House move crash mitigation

- Native House moves are limited to eight per UI tick instead of executing a
  32- or 43-move plan synchronously in one callback.
- Each successful partial batch waits for another UI tick, refreshes the live
  House state, creates a fresh preview, and revalidates before continuing.
- Leaving House, changing scene generation, or losing the writable House
  context cancels the continuation before another batch runs.

## Breeding-room pair pool

- The primary recommended pair remains authoritative.
- Remaining breeding-room slots use disjoint eligible pair rankings based on
  seven-stat coverage, confirmed sexuality compatibility, and cached offspring
  COI instead of unrelated stable cat ordering.
- This improves which cats share the breeding room; it does not claim to force
  the game to choose a specific mating pair.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
