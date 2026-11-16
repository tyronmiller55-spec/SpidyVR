# LEG HOLD: Y+X engages the legs (November 12)

The user, Nov 12 01:33 PST (t67u): "Legs will use y+x, thumb rest didn't work. We can remove the setting
and leave it as y+x. Also, make sure that web zip and gadget menu are disabled while the player is holding
both buttons. Legs should stay active as long as the player is still manipulating the stick, even if they
let go of the buttons. Neutral on the right stick cancels legs." Built on 694861c (feat/punch-view-ease).
Replaces LEG TOGGLE (bec50c8). Not headset-tested.

## Behaviour (`include/spidy/leg_hold.hpp`)

- LEG CONTROL stays the master switch, with its gates: midair or swinging only, CONTROLLED or REALISTIC
  FLIPS, a body, VR, play; the legs' momentum coupling unchanged. Left stick click stays slow-mo.
- **Engage:** Y and X held together. The legs own the right stick that frame (where they may move).
- **Suppression:** from the chord until both buttons are let go of, neither reaches the game or the mod:
  no web strike / zip on Y, no gadget wheel on X, nothing else on them, on the press, during the hold or
  on the release (no release edge).
- **Chord window, 80 ms:** one pressed alone is held back up to 80 ms for the other. A deliberate
  two-button press (on Touch usually one thumb across both) lands its second button well inside 50 ms; a
  lone Y is late by at most 80 ms (6 frames at 72 Hz, under the ~100 ms where a press starts to feel late)
  and a lone X's 0.3 s wheel hold is unchanged in feel. Let go of inside the window, the button reaches the
  game as a 50 ms tap. Chords start only where the legs could move (last frame), with no prompt naming X
  or Y and the wheel shut; elsewhere X and Y pass at once, unchanged.
- **Stick persistence:** after letting go, the legs stay engaged while the right stick is deflected. Back
  under 0.18 for 80 ms disengages them and the stick is the turn's, zip's and heal's again on that frame;
  past 0.30 restarts the wait; between the two it neither counts nor restarts (noise never cancels).
  Neutral while Y+X are still held keeps the legs engaged. The stick alone never engages.
- **While engaged** the right stick poses the legs only: no turn, no zip, no heal (the November 10 double
  tap is gone; `leg_double_tap_ms` is still read but unused).
- **Disengaged** midair, the legs spring back to straight (critically damped, 0.6 s) before the leg layer
  blends out. Flip break: right-stick pitch breaks only while the legs are engaged; turns never.
- **Logs:** `legs: engaged (Y+X held)`, `legs: disengaged (right stick back to neutral | LEG CONTROL
  unavailable ...)`; a 0.2 haptic tick on engage.

## Removed

The thumb rest tap (its OpenXR action and the Touch "extras" binding: the Touch layout is exactly as
before bec50c8) and LEG TOGGLE / `leg_toggle_input` (launcher and in-game). An old `leg_toggle_input` line
in spidy_vr.ini is ignored and not written back.

## Tests

`spidy_leg_hold_tests`: chord together and staggered both ways inside 80 ms, nothing passing on press /
hold / release, lone Y and X passing after the window and staying held, a quick tap, not-allowed pass-
through, stick persistence, hysteresis, brief dips, neutral cancel and its reason, release with the stick
neutral, the gate (buttons stay held back until let go), easing, NaN input, flip break, the setting gone.
