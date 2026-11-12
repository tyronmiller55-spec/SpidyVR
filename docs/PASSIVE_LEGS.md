# PASSIVE LEGS, AUTO POSES, FLAIL (the legs build of the Nov 12 plan)

Source: the user, Nov 12 06:18 PST. Built on 319e734 (feat/stable-view-yaw). Code: `include/spidy/leg_layers.hpp` (layers, thresholds, flail gate), `include/spidy/leg_control.hpp` (`Simulation::updateTarget`), `src/game_xr.cpp`, `src/body_ik.cpp` / `src/native_body.cpp` (`legAmount`). Tests: `tests/passive_legs_tests.cpp`.

## One spring, four layers (highest first)
1. **Active**: LEG CONTROL's right stick (Y+X engaged). Full LEG COUPLING.
2. **Flail**: in the air, Y+X physically held and B held: a 2.2 Hz run cycle, legs in opposition. The right stick pushed past 0.3 takes over (1 beats 2).
3. **Auto poses** (each eased in and out by its own weight; they can combine):
   - **AIR CROUCH** (`leg_air_crouch`, on): the ground jump's midair crouch (thighs 1.0 rad up, knees 1.15, slightly apart).
   - **REACH SPREAD** (`leg_reach_spread`, on): spread 0.45 rad, lifted 0.7, knees 0.85.
4. **Passive**: **PASSIVE LEGS** (`passive_legs_pct`; OFF / 20 / 40 / 60 (default) / 80 / 100% shown). The centre pose with no stick, so only swing, spin and deceleration move the legs. Their turn on the body is **PASSIVE COUPLING** (`passive_coupling_pct`; default 15%) of LEG COUPLING. Auto poses use the same coupling.

The spring's state never resets between layers, only its target changes (eased over 0.08 s). The shown share (body_ik's `legControlWeight`) eases at the 0.2 s air-leg pace. Tested: no leg angle moves more than 0.16 rad in one frame across six layer changes. The B tuck still overlays on top as before. Landing blends everything out (0.2 s), as before.

## Gates
- PASSIVE LEGS and the auto poses need no LEG CONTROL. They run wherever the legs may move in SpidyVR's air (`legsAirOnly`: SpidyVR-owned air, or the hybrid leap air after a jump handoff), so they carry across the jump handoff. The stable view lock does not touch them.
- **Flip modes:** the passive legs are **seen** with any flip mode, or none. Their **turn on the body** is applied only with CONTROLLED or REALISTIC FLIPS, because those momentum models (FlipMotion, RealisticFlip) are what take `rig.legCoupling`; with neither, there is nothing that integrates the turn consistently.
- **FLAIL needs LEG CONTROL** (with its flip mode): Y+X only belong to the legs through LEG CONTROL's hold. Without it, Y would zip and X would open the gadget wheel.

## AIR CROUCH thresholds (and why)
- Upright: body up within **30 deg** of world up (as asked); exits past 40 deg (hysteresis).
- Hang time: **0.45 s** airborne. A rise to a ~1 m apex takes about that long, so hops and kerb step-offs stay straight. Or **near the apex**: vertical speed within **2 m/s** (about +-0.2 s around it), after 0.15 s.
- Low rotation: under **1.5 rad/s** (86 deg/s); exits past 2.5. Flips spin at 4-8 rad/s.
- Not on a web: the bottom of a swing is upright and slow-turning too, and the legs should trail there.
- Falling faster than **7 m/s** (about 2.5 m of fall): the legs come down for the landing.
- Eased in over 0.25 s, out over 0.2 s.

## REACH SPREAD detection
The pelvis is estimated as 44% of the standing eye height below the head (hips at about 0.53 of stature, eyes at 0.94; a seated player's is about as far). A hand is tested in the player's own level frame in tracking space:
- below the pelvis by 2 cm (exits above +4 cm);
- within 16 cm of the midline (exits past 22 cm);
- between 20 cm behind and 40 cm ahead.

## FLAIL's B
Once B is held in the air with Y+X physically held, B is taken **before** POINT JUMP's A+B, the dodge and the tuck read it, and it stays taken until B is let go of (even if Y+X or the air end first). A B held before Y+X is taken over when they come. B alone is the normal tuck.

## Logs
- `legs: layer <passive|auto pose|flail|active (right stick)|easing to straight|off> (shown N%, coupling N% of LEG COUPLING)`
- `legs: air crouch on|off (up D deg, air S s, vy V m/s, spin W rad/s[, webbed])`
- `legs: reach spread on|off (left hand|right hand|both hands|no hand)`
- `legs: flail B taken (...)` and `legs: flail B let go of`
