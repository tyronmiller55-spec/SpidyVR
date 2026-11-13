# MOVEMENT MODE: HYBRID

The user, Nov 12 (t75u): "the new movement frequently causes Spiderman to face the opposite direction of the headset. We need the headset to stay locked to Spiderman's head ... Since there is no backward run, we'll replace that with a snap turn anytime the left stick is pointed to the rear."

MOVEMENT MODE (NATIVE ANIMATION tab, launcher "Movement mode", `movement_mode`): **HYBRID** (default), **STABLE** (Step B, unchanged), **OLD** (head-bone steering, the former VIEW YAW SOURCE BODY). VIEW YAW SOURCE is superseded (launcher only, greyed). REAR SNAP TURN (CAMERA & STABILIZATION, `rear_snap_ms`): INSTANT (default) / 100 / 150 / 200 / 300 ms.

## HYBRID

- **View:** head lock on the head bone, yaw included. One heading (the right stick's, smooth or snap as set) is what the hero faces. While he faces it the view *is* the head bone (no divergence). While he is being brought onto it (rear snap, right-stick turn, a landing on an old game facing) the view holds the heading and the head's yaw sway fades out, then back in over 0.3 s once he arrives (within 12 deg). If he cannot get there in 2.5 s the heading rejoins him (logged, `GAVE UP`). With no hold the heading follows his facing every frame (parkour, combat, wall moves: the view on the head).
- **Left stick, front half (within 90 deg of forward):** he runs straight along the heading at the stick's forward share (tilt x cos angle); no body turn toward the stick. **The game cannot run off-facing:** it has no strafe or backpedal; its run always turns the hero toward where he runs, and head-locked that would turn the view. So a side push gives no sideways motion: 60 deg runs at half speed straight ahead, 90-100 deg does nothing. Turning is the right stick's.
- **Left stick, rear half:** past 100 deg, at half tilt or more, once the stick settles (or 60 ms): one snap of the heading (the view, instantly by default) to the stick's exact direction; he runs that way at the stick's tilt while it stays back; under 80 deg or neutral re-arms. The hero's body turns at the game's own pace under the held view (no facing write exists). Hands, webs, shooters and arm IK use the same published turn as the eyes (view_yaw_lock's state), and an instant snap starts a new hand-velocity epoch.
- **Standing:** a held heading brings him round with the forced TURN-IN-PLACE (closed loop on the walk stick, any angle).
- **Landings:** the vr->anim edge frame sets the heading to the view SpidyVR showed and holds it (before the animation's first frame turns him onto the game's stored facing); the facing check's hero correction keeps it while the walk stick turns him. Replays (tests): 31.70 s plain head lock swings 177 deg, HYBRID worst 10 deg; 408.44 s 128 deg vs 11 deg. The remaining ~10 deg is the settle as he arrives within the turn's 12 deg stop.
- **Air facing sync:** in SpidyVR's air (swing owns him, no web, no wall) the game gets a light walk stick (0.3) toward where you look while its facing is more than 20 deg off (off under 8), as SOFT LANDING already hands the game its stick in the air. Whether the game turns its stored facing from it in that state is unmeasured: `hybrid air facing sync: on/off` and `hybrid landing: view X, game facing Y (gap Z), air facing sync N ms` measure it in the next session. If it does not, the landing hold above still keeps the view; the hero then visibly turns after landing. **Retired Nov 12 (5061b4a playtest):** the stick no longer goes out (170 syncs: the game's facing did not move, median 0 deg); the gate still logs how long the gap lasted. `hybrid_movement::airSyncStick = true` restores it.

## Logs

`rear snap: stick +150 deg -> facing -150 (from 0, hero 0, view instant, #1)`, `hybrid heading: rear snap|turn|facing check hold arrived|GAVE UP after N ms`, `view yaw lock: HYBRID latched ...`, `hybrid landing: ...`, `hybrid air facing sync: ...`, the 10 s `view yaw divergence: ... mode HYBRID, rear snaps N, holds arrived N gave up N`, and the facing check lines.

## Limits

- No facing write: the hero turns at the game's pace (about 250 deg/s standing, a run start for snaps); only the view is instant.
- No strafe: a sideways push does not move him sideways.
- A landing straight into a held run keeps the game's facing until the run's stick turns him (the held view covers it).

## Landing view hold (Nov 12, playtest fixes)

A facing hold (the landing's, a takeover's, an anim re-entry's) lasts `landing_hold_ms` (default 600; 2500 is the old give-up), 150 ms in a fight, a dodge or with the left stick held. When it times out the heading eases onto the hero over `air_view_ease_ms` (smoothstep; 0 snaps) and the forced walk-stick turn stops. Hold arrived: unchanged. R-stick and rear-snap holds keep 2.5 s. Log: `hybrid heading: facing check hold timed out (view eases onto the head) after N ms`.
