# FULL head lock on the stable heading (rotation desync Step B)

Source: the user, Nov 12 05:56 PST. Built on edeba41 (feat/jump-handoff). Code: `include/spidy/view_yaw_lock.hpp`, `src/game_fp.cpp` (`frame` / `heroHead`), `src/native_body.cpp` (pose job anchor), `src/game_xr.cpp` (lock, turns, turn-in-place, logs). Tests: `tests/view_yaw_lock_tests.cpp`.

## What the eyes show
With HEAD LOCK **FULL** and VIEW YAW SOURCE **STABLE**: eye position, pitch and roll from the head bone; yaw from the lock's heading:

    shown yaw = lock + ANIM YAW SWAY x wrap(head bone yaw - hero facing)

- The lock is latched with the head lock (owner switch, recenter) so its first frame shows exactly the head bone; it is kept through the midair softlock. Only the right stick turns it (SMOOTH TURN deg/s scaled by the stick, or SNAP TURN flicks with the 0.7 / 0.4 hysteresis). The hero turning never turns it.
- **ANIM YAW SWAY** (VR CAMERA & STABILIZATION, launcher "Anim yaw sway", `anim_yaw_sway_pct`): 0% (STEADY) / 10 / 20 (default) / 30 / 40 / 50 / 60 / 80 / 100% (OLD FEEL). The sway is the head's yaw relative to the hero's facing. **100%** turns the lock off: the eyes ride the head bone exactly as before, with Step A steering.
- One turn, everywhere the Full anchor is resolved: the render thread's eyes (`game_fp::frame`), the XR thread's hand map (webs, balls, markers, shooter lock, `kin.head`), and the pose job's arm IK anchor (`animated_view::Targets::yawLock`, turned in model space). The stable heading reads the unturned head (`game_fp::heroHead`), so there is no loop.
- Steering: the heading is the lock + the headset's turn (no idle relatch to the head bone, no composeMove turn request).

## The hero's facing: TURN-IN-PLACE
There is no verified facing address, and none was guessed. The actor matrix is read only. The lever is the one already validated (InteractStep / MODEL YAW): the game's walk stick.
- Standing (left stick idle, grounded, the animation owns, no fight, dodge, zip, legs or crawl), once the heading is more than **TURN-IN-PLACE ANGLE** (`turn_in_place_deg`; OFF / 20 / 25 / 30 / 35 (default) / 45 / 60 / 90) from his facing (20 deg while the right stick turns), the walk stick points **at the heading** (tilt 0.35, 0.5 after 0.3 s without 5 deg). It stops within 12 deg; it gives up after 2.5 s (1 s cooldown).
- Closed loop on his measured facing: it cannot spin past or keep going the wrong way. The old standing turn pushed the stick 90 deg to his side, open loop, at 0.42 x (|x| - 0.2) / 0.8: small pushes didn't turn him (30% in the 03:49 session), full ones spun him (19% over 300 deg/s), and some went the wrong way (3).
- What it cannot do: set his turn speed (the game's), or rule out a small step forward at the end of a turn (the game reads a forward push as a walk; stopping at 12 deg keeps that small). The **view** turns at exactly the setting (120 deg/s, or 30 deg snaps); he follows.

## Logs
- `view yaw lock: latched at A deg (head bone B, hero H), sway S%, turn-in-place T deg, #n` / `view yaw lock: off (why)`
- `view yaw: lock .. bone .. hero .. shown .. hmd .. view .. view-hero .. shown-bone .. sway ..% full|softlock` (2 Hz)
- `view yaw divergence: N samples, view-hero mean/p95/max, over 45 %; view-headbone mean/p95/max; sway; turn in place counts` (every 10 s): the divergence metric.
- `view turn: smooth|snap, N ms, view X deg (R deg/s, k snaps), hero Y deg, #n` (each R-stick turn, after a 0.5 s follow-through)
- `turn in place: start (#n, R stick|look) gap .. deg ...` / `turn in place: done|gave up|interrupted after N ms, hero turned X of Y deg (R deg/s), left Z [WRONG WAY]; counts`
