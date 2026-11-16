# Ground kicks (K3)

Off by default, EXPERIMENTAL, like the air kicks. `ground_kicks=1` in `spidy_vr.ini` (or **Ground kicks (experimental)** on the launcher's Legs page, next to Air kicks; **Air kick aim** aims these too) turns it on. It is the plan's Pillar 5, milestone K3: the same foot, sphere and blow as the air kicks ([AIR_KICKS.md](AIR_KICKS.md)), standing. The air kicks are a separate switch (`kicks`); either, both or neither may be on.

## Controls

Standing (on the ground, slower than 2 m/s) hold **Y + X**, as for the legs in the air, and flick the right stick: the stick from under 0.3 to over 0.8 within 80 ms. A slow push is nothing (it is the turn's; while the chord is held the stick does not turn you). Held or released, the legs stay engaged while the right stick is moved and end when it is neutral, as in the air.

| Gesture | Kick | Zone |
| --- | --- | --- |
| Flick up, up-left or up-right (within 70 degrees of up) | Front kick: a freeform blow with that side's leg (straight up: the right) | The zone you look at (head, chest or legs) |
| Flick sideways (70 to 110 degrees) | Waits up to 0.6 s for a sweep; the stick coming back in, or the wait ending, makes it a side kick (freeform) | The zone you look at |
| Half circle across the bottom: from a side (70 to 110 degrees, at 0.6 or more of the stick) through the bottom (135 degrees or more, at 0.5 or more) to the other side (at 0.6 or more), within 0.6 s | **Sweep**: the foot swings round the pelvis at 0.9 m, 12 cm over the feet, 180 degrees from the side you began on through the front to the other side in 0.35 s | The legs, of the nearest enemy in front |
| Flick down | Nothing | |

The sweep may begin with a flick to the side (which then waits for it) or be drawn from a stick resting at the side. A drawn start that is not completed is no kick at all, so holding the stick at a side to turn never kicks. The leg that sweeps is the side the half circle began on (a guess to confirm on a headset).

**The Y + X chord.** With ground kicks on, standing and slow, the chord is the legs' engage on the ground too (it needs neither LEG CONTROL nor a flip mode): neither button reaches the game while it is held, so no web strike or zip with both held, and no gadget wheel (one button alone waits up to 80 ms for the other, as in the air). It is not taken where a prompt names X or Y or the wheel is open. Off by default, so nothing changes until you turn it on.

## What it does

- **The same kick as the air's** (`air_kick::Kick`), standing: the pelvis 0.9 m over the feet, the leg 1.15 m (a front or side kick 70% of it) with **no lunge**; the foot's 10 cm swept sphere against every fighting enemy's capsule; the foot's speed gives the strength (nothing under 3 m/s). The gaze aims it as in the air, within `kick_gaze_cone_deg`.
- **A knock-down, not a throw.** A ground blow of 0.7 strength or more flies him at half the foot's speed along its way, at most 6 m/s, 0.5 m/s up (the air's: 80%, 14 m/s, 1.5 up). The sweep takes the legs: its zone is the legs; its foot goes about 8 m/s round the pelvis, so a normal one is a firm hit and a knock-down.
- **Refused or ended:** faster than 2 m/s when it starts (or not a number), the legs not engaged, a wall, another kick, the cooldown. A standing kick ends where it is if the player runs faster than 3.5 m/s, leaves the ground or the switch goes off; an air kick that lands ends as before.
- **The legs' pose (headset-untested).** While the animation owns the body (ANIMATED moves), one leg is solved onto the game's pose: the hip, knee and ankle reach the foot the swing modelled (`body_ik::Targets::groundKick`, through `native_body::setGroundKick`), eased in and out with the kick (`air_kick::amountAt`), the pelvis, the other leg and the torso stay on the animation. In the VR-owned body (no ANIMATED yield) the body has no ground leg, so the blow lands but no leg is drawn.

## Where it lives

| Piece | File |
| --- | --- |
| Stick gestures (flick, side wait, half circle) | `include/spidy/ground_kick.hpp` |
| The kick standing (the `ground` and `speed` frame, the sweep's foot, no lunge, the knock-down's flight) | `include/spidy/air_kick.hpp` |
| Arm, speed, blow, the channel | `game_swing.cpp` (`SpidyAirKick` flags bit 2 and gesture 5 = sweep; `AirKickLive` version 2 adds the foot and the speed; `game_swing::Settings` is untouched) |
| Gestures, chord, stick, log, the leg's overlay | `game_xr.cpp` |
| One leg onto the foot | `body_ik.cpp` (the armsOnly path), `native_body.cpp` (`setGroundKick`) |
| Settings | `ground_kicks` in `vr_settings` |
| Tests | `tests/ground_kick_tests.cpp`, a settings check in `tests/core_tests.cpp`, the launcher exemption in `tests/launcher_tests.cpp` |

No game address is used: the targets and the blow are the web strike's and the punches' (no new RE; the plan's table says: none).

## Log lines

The air kicks': `air kick: gesture <dive|double|roundhouse|free|sweep> ...` (gesture `sweep` and `free` standing too), `kick: ...` start, hit, miss and `kick: refused (<why>)`, where the new reasons are `moving too fast` and `a sweep is for the ground`.

## Not built (needs a verified structure or a headset first)

- **The torso**: the pelvis drop (0.45 m), the lean (25 degrees) and the hand plant for a sweep (`kick_torso_pct`). The pose job has no verified pelvis or torso writer for the ground, and in ANIMATED moves the camera follows the animated head, so a lean needs a head counter-rotation that cannot be judged without a headset. The kick moves the legs only.
- **A ground leg in the VR-owned body**: body_ik plants the feet there; the dive branch is the air's.
- **Foot orientation and the support leg's weight shift** in the ground pose.
- **Knock-down by state:** the blow is the web strike's kinetic one (the game's own fall); there is no verified "knocked down" state to set.

## Unverified (needs a headset)

- The leg overlay: whether the animation's model space has its origin at the feet (the foot target is the swing's, from the player's feet, in world axes turned into the model's), how the knee looks, and whether the leg pops when it ends.
- Whether the chord is comfortable on the ground, and the 0.6 s side wait; whether a drawn half circle is easy against the stick's gate.
- The sweep's side, its reach and arc against enemies at different distances, and the knock-down on heavies and scripted fights.
