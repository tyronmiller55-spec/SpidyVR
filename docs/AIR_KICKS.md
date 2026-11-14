# Air kicks (K2)

Off by default, EXPERIMENTAL, like the handstand. `kicks=1` in `spidy_vr.ini` (or **Air kicks (experimental)** on the launcher's Legs page, with **Air kick aim** and **Air kick spin** under it) turns it on. It builds on the leg control (Y + X, `leg_hold.hpp`, which needs LEG CONTROL and a flip mode as today) and the swing kick (K1). It is the plan's Pillar 5 (milestone K2), air only: the ground is K3's.

## Controls

Off the ground with the legs engaged (hold Y + X), flick the right stick: the stick from under 0.3 to over 0.8 within 80 ms. A slow push is still the legs' pose (jackknife, splay), as before.

| Gesture | Kick | Zone it goes for |
| --- | --- | --- |
| Flick straight up (within 20 degrees) | Two-leg dive (drop kick), both legs out together | Chest |
| Flick up-left or up-right (20 to 70 degrees) | One-leg dive with that side's leg (the other bent) | Chest or head, the nearer to your gaze |
| Quarter circle from a side up to the top, along the rim, within 0.25 s | Spinning roundhouse: the foot sweeps in from that side (the leg of that side) over 100 degrees to your gaze | Head |
| Flick sideways (70 to 110 degrees) or down | Freeform blow: a lighter, shorter kick (the side's leg out, or both back for down) | The zone you look at |

A sideways flick waits up to 0.25 s, or until the stick comes back in, to see whether it is the start of a roundhouse's quarter circle; every other flick kicks at once, so a sideways freeform blow has that small delay. The stick has to come back under 0.3 before the next flick. The roundhouse's "far leg" in the plan is read here as the leg of the side the circle begins on (the foot comes in from that side); this is a guess to confirm on a headset.

No new buttons. The swing kick (K1) keeps the flick it arms on while swinging (up, with a ringed enemy in its cone); any flick it does not take (no one ringed, too slow, too close) is an air kick. The auto dive (`swing_kick_auto`) holds back while either is under way.

## What it does

- **The foot** is modelled: the pelvis 0.9 m over the feet, the foot on a leg reaching 1.15 m plus a 0.4 m lunge in the air (a freeform blow 70% of it), going out over 0.16 s (roundhouse 0.30 s, freeform 0.22 s), held 0.1 s, back over 0.25 s. No foot position is read back from the body, so the legs' pose (`leg_layers`) and the foot the blow uses are two models of one kick.
- **Aim.** The gaze picks the enemy and his zone point (head over 80% of the capsule, legs under 35%, chest between) within 25 degrees of it (`kick_gaze_cone_deg`) and within reach. The foot bends toward the point by `w = smoothstep(cos 25, cos 8, gaze . to point)`, at most 0.25 m or 15 degrees. The gaze is the headset's at the flick, turned to the world's axes by last frame's rig.
- **Blow.** The foot's swept 10 cm sphere against **every** fighting enemy's capsule (nearest 12), not only the ringed one. Its speed (its own and the player's motion, per step) gives the strength: nothing under 3 m/s, full at 10 m/s. A blow of 0.7 or more flies him (a kinetic blow at 80% of the foot's speed along its way, a little up, at most 14 m/s, as every dive does); a softer one is a plain hit. One blow per kick. The player's own velocity is not changed (no recoil, no speed added).
- **Legs.** A kick is the legs' top layer (`leg_layers::Layer::kick`): the pose is leg_control's angles, the spring is the same, quicker for a kick. Its spin reaction on the body goes through the legs' coupling, scaled by `kick_spin_pct` (30% by default: a roundhouse turns you a little).
- A kick ends where it is (the legs ease back) if the player lands, touches a wall or the setting goes off.

## Where it lives

| Piece | File |
| --- | --- |
| Gestures, zone, gaze weight, foot bend, strength, timeline, leg poses, the kick | `include/spidy/air_kick.hpp` |
| The kick layer | `include/spidy/leg_layers.hpp` |
| Enemies in the fight, the blow | `game_punch::kickCandidates`, `game_punch::kickHit` |
| Arm, update, blow, the channel | `game_swing.cpp` (`SpidyAirKick`, `SpidyAirKickLive`; wire structs `AirKickAsk` 64 bytes, `AirKickLive` in `game_swing.hpp`; `game_swing::Settings` is untouched) |
| Gestures, settings sent, log, legs | `game_xr.cpp` |
| Settings | `kicks`, `kick_gaze_cone_deg` (25), `kick_spin_pct` (30) in `vr_settings` |
| Tests | `tests/air_kick_tests.cpp`, a settings check in `tests/core_tests.cpp` |

No game address is used: the targets and the blow are the web strike's and the punches' (REFERENCE.md and VALIDATION.md list nothing new for K2; the plan's table says RE: none).

## Log lines

`air kick: gesture <dive|double|roundhouse|free> side= angle= size=`; `kick: <gesture> start zone= target=`; `kick: <gesture> hit foot=<m/s> zone= strength= target= (started, hits, flies, misses)`; `kick: <gesture> miss`; `kick: refused (<why>)`.

## Not built (needs a verified structure or a headset first)

- **The torso** (pitch back up to 25 degrees for the dives, pelvis yaw 0.8 x the sweep and a counter-yaw of the chest for the roundhouse) and `kick_torso_pct`: body_ik has no torso channel for this and the body command's layout is the official one; a new field is a protocol change. Without it the kick moves the legs only.
- **The spin vignette** (above 60 degrees a second): the mod has no vignette to drive.
- **Kicking off an enemy** (the small rebound of the plan's extra ideas) and **ground sweeps** (K3).
- **Enemy classes** (a heavy shrugging a kick off): no verified flag, as in G2.

## Unverified (needs a headset)

- The foot is modelled, so a kick that looks like it lands on the legs may miss by the legs' real reach, and one that does not may land.
- Whether the flick feels distinct from the legs' pose pushes, and whether the 0.25 s wait for a roundhouse is too long or the quarter circle too hard to draw.
- The leg poses (angles authored here), the reaction's size at 30%, and how the layer's pose joins the spring.
- Whether the game's flight follows a 14 m/s kick on every enemy (heavies and scripted fights may ignore it, as K1 notes).
