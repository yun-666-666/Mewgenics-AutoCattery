# AutoCattery v0.5.23

- Removes the fixed three-second wait after returning from combat. The first
  once-per-second wake probe that detects a ready House or selection scene
  resumes AutoCattery UI work immediately, without an additional timer.
- Keeps the two normal House buttons on an empty, stopped first frame before
  their native controllers attach.
- Shows the buttons only after their native controllers have installed the
  localized Auto-Organize and recommendation labels, preventing the source
  asset's `Clean Up!` text from flashing after combat.
- Preserves the v0.5.22 generation-aware reattachment, the v0.5.21 hidden F10
  panel, and the v0.5.19 combat suspension behavior.

Automated asset checks validate the hidden-first-frame and explicit-show
contract. Final rendering still requires player testing in Mewgenics.
