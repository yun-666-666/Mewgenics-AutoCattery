# AutoCattery v0.5.22

- Fixes both House buttons reverting to `Clean Up!` after ending a day by
  abandoning previous-generation button records and attaching to the new
  House scene instance.
- Resolves the F10 panel's overlay root through the short `test_button` marker
  before looking up the longer panel node names. This removes the observed
  initial-attachment path that searched unrelated House roots with
  `panel_background` and ended in heap-corruption detection.
- Keeps the v0.5.21 hidden-first-frame panel behavior and the v0.5.19 combat
  suspension behavior unchanged.

Automated builds and tests validate the lifecycle and asset contracts. Final
rendering and crash acceptance still require player testing in Mewgenics.
