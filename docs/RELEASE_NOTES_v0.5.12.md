# AutoCattery v0.5.12

This release corrects MoveOnly room-purpose planning.

- Breeding rooms now require viable comfort and balance comfort with
  stimulation. A room at or below the confirmed `-10` comfort automatic-
  failure boundary no longer wins only because its stimulation is high.
- Combat staging now prefers health and comfort. It ignores sex and does not
  reserve the best breeding environment.
- "Separate kittens when possible" now affects live MoveOnly planning. An
  additional occupied room becomes a health/comfort-oriented nursery after
  breeding and combat targets are assigned.
- Stable-all-7 planning continues to consider room Mutation as an additional
  breeding tie-breaker. Automatic low-comfort fighting remains disabled due
  to injury and death risk.

No automatic team composition, rest/day advance, embark selection, or real
culling is added.
