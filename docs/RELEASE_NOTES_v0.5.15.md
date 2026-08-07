# AutoCattery v0.5.15

## Cross-compatible breeding-room cohort

- Opt-in main-save snapshots from game days 257 through 264 were analyzed as a
  timeline rather than as one latest-day sample.
- The first day after the v0.5.14 pair-pool change produced five observed
  newborns from four different cross-pairs inside the breeding room. Newborn
  average seven-stat gap improved from 2.93 across the prior six observed days
  to 2.60, and average offspring COI improved from 0.220 to 0.168.
- Because the game selected cross-pairs rather than the primary recommended
  pair, v0.5.15 ranks the whole room cohort. Every preferred cat must pair
  eligibly with all already selected opposite-sex residents; selection then
  maximizes the weakest cross-pair score, minimizes worst COI, and maximizes
  average pair score.

The existing opt-in collector remains local-only and already retains distinct
preview snapshots by game day and content digest. No save, cat name, save path,
account identifier, or collected JSON is included in the release.

Real culling, automatic day advance, automatic expedition selection, and save
database writes remain disabled.
