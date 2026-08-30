# AutoCattery v0.5.24

- Restores click input for both normal House buttons.
- Keeps the v0.5.23 hidden first frame, but moves the stopped visible-frame
  transition before native Button setup so the game can resolve the button
  state nodes and mouse hit area during installation.
- Keeps localized label replacement in the same UI tick, preserving the fix
  for the source asset's temporary `Clean Up!` text.
- Preserves generation-aware House reattachment, F10 suppression, expedition
  suspension, MoveOnly planning, protection, and save-safety behavior.

The latest v0.5.23 log showed both roles attaching without either click
callback. Earlier v0.5.22 logs showed the same controls producing callbacks;
the hidden-first-frame setup order was the only intervening input-path change.
Automated builds validated the asset and lifecycle contracts. The player later
confirmed the current v0.5.24 House buttons, F10 panel, MoveOnly flow, scene
transitions, and repeated use without reporting a new current-version issue.
