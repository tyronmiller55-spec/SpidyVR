# LEG TOGGLE (November 12)

The user, Nov 12 00:44 PST: "The legs feel absolutely AMAZING, but I lose all of my other functionality,
including the ability to rotate left and right ... let the player toggle legs in real time, on frame";
t64u: the left thumb rest, a deliberate tap, the default; t65u: never the left stick click (slow-mo),
fallback HOLD X+Y. Built on c128662 (feat/point-jump). Not headset-tested.

## Behaviour

- **LEG CONTROL** stays the master switch. **LEG TOGGLE** (`leg_toggle_input`): `0` THUMB REST TAP
  (default), `1` HOLD X+Y, `2` ALWAYS ON (the old behaviour: legs own the right stick in the air).
- Legs start **OFF** every session. OFF: the right stick is exactly what it is without LEG CONTROL, in
  every situation including midair (snap/smooth turn, up zip, down heal, basic flips' pitch); no double tap.
- ON: in the air / swinging the right stick poses the legs as before (double tap up zips, down heals).
- The toggle takes effect on the frame it is read: the flip break, the rig's turn and the zip/heal see the
  new state that frame (also turning ON midair: the legs take the stick that frame).
- Turning OFF midair: for 0.6 s the leg springs drive to straight, critically damped at 2.5 Hz with the
  stick centred and no body throw (worst residual under 0.05 rad in the test), then body_ik blends the
  leg layer out as on landing. No snap.
- Each toggle: a 0.25 haptic tick on the left controller and a `leg toggle: ON|OFF (input)` log line;
  the chosen input and whether the thumb rest is bound are logged once (`leg toggle: input ...`).
- Flip break: unchanged rules. Turns never break. With legs ON the right stick's pitch breaks (as 07447b8);
  with legs OFF the right stick's up/down is zip/heal and does not break (as without LEG CONTROL); basic
  FLIPS' right-stick pitch still breaks; the left stick under CONTROLLED/REALISTIC flips still breaks.

## Inputs

- **Thumb rest tap.** `/user/hand/left/input/thumbrest/touch` on `/interaction_profiles/oculus/touch_controller`
  (core OpenXR 1.0 for that profile; Quest 3 / 3S Touch Plus keep the capacitive thumb rest). Suggested as
  a Touch *extra*: the Touch layout is suggested with it first and, if the runtime refuses, again without
  it, so the Touch layout can never be lost. A tap = untouched at least 0.2 s, touched 30-350 ms, released,
  with the left stick under 0.3 tilt and unclicked throughout. A resting thumb, a brush straight after
  another touch, a one-frame flicker, or a touch while steering/clicking (slow-mo) is no tap.
- **HOLD X+Y.** Both held 0.2 s toggles once. From the frame both are down, neither reaches the game or
  the gadget wheel (which opens on X held 0.3 s) until both are released; X or Y alone is untouched. A Y
  pressed a few frames before X has already gone to the game (in the air: a web strike) - press X first
  or both together. Blocked while a prompt names X or Y, the phone or wheel is open, the pad layer is
  latched or in calibration. X+Y was not otherwise used.
- **Fallback:** where the runtime took no thumb rest binding (not Touch, Index, Steam Frame, or refused),
  THUMB REST TAP reads HOLD X+Y instead (logged). Whether Quest Link / Virtual Desktop / SteamVR actually
  deliver thumb rest touch on a Quest 3S must be confirmed in the headset: check the log for
  `Controller layout Touch: extras taken (left thumb rest touch)`, and that a tap logs `leg toggle: ON`.
  If taps never register, choose HOLD X+Y.

## Further option (not built)

Keep turning with the legs ON: the legs use only the right stick's vertical axis and its horizontal stays
the turn (legs lose the side splays), or the left stick's horizontal turns while legs are ON (conflicts
with CONTROLLED/REALISTIC flips' left-stick steering). The toggle covers the request; either could be a
later LEG TOGGLE choice.

## Files

`include/spidy/leg_toggle.hpp` (ThumbrestTap, XYHold, Toggle), `controller_layout.hpp` (Touch extras),
`xr_session.*` (thumb rest action, two-step suggest, `thumbrestBound()`), `tracking.hpp`
(`XrFrame::thumbrest`), `game_xr.cpp` (toggle block before X/Y/right-stick readers, legMode/stick
ownership/flip break/easing), settings (row LEG TOGGLE on the LEGS tab after LEG CONTROL, launcher Legs
page, `leg_toggle_input`, personal), tests `spidy_leg_toggle_tests`.
