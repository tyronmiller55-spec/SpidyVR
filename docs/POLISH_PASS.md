# Polish pass (Nov 14, branch `feat/polish-pass`, on 4d58169)

The principle (the user, t64u): **one per-frame VIEW HEADING, taken from the headset, is the only direction authority.** The left stick, landings, dodges, the hero's turn, the web shooters, the aim markers and the arms all answer to it. Animation, handoffs and the game's own facing never write it. Slice by slice:

1. The direction report (this section). 2. Stick from the headset. 3. Landings face the view. 4. The view is yours. 5. Re-sync on every action. 6. Runtime and lag. 7. Punch auto-target. 8. Soft pull on jumps. 9. Handstand balance.

## Reading `direction:` (slice 1)

```
direction: hmd +12 shown -48 view -36 basis -50 hero -52 bone -40 momentum -47 (6.1 m/s) action land 340 ms ago
direction audit: imposed view turn 0 deg in 0 frames (worst step 0 deg, 0 of 90 frames over 3 deg); worst skew eyes 0.0 hands 0.0 arms 0.0 stick 0.0 deg (frames off: 0/0/0/0); 2 actions; imposed since start 0 deg
```

- `view` = `shown` + `hmd`: where the player looks. Angles are degrees in the rig's convention (0 facing -z, + to the left). `-999` is unknown (no momentum under 1 m/s).
- `basis` is the world yaw the left stick is mapped from. The goal of slice 2 is `basis` = `view` at every press. `hero` is the actor's facing (the game's). `momentum` is the direction he travels.
- `imposed view turn` is the number to drive to 0: view movement that was not the head's own turn, the right stick's turn, a snap, a realign or a latch. Landings and dodges are where it was 21% and 30% of half-second steps before this pass.
- `worst skew` per consumer is the largest difference between the yaw it used and the yaw published, in the second. Slice 5 is about driving it to 0 on action frames.
- The `action` field and its age tell which edge last happened; a line is written on every edge.

The quiet log is `diag_log::Throttle` (`include/spidy/diag_log.hpp`): lines of the noisy kinds are kept when their shape (the line with its numbers taken out) changed, or after 2 s; in slow mode (slice 6) only on a change.

## Everything re-syncs on every action (slice 5)

`view_yaw_lock::publish` is a seqlock block (yaw, sway, hero, ride, limit, on, and the action serial), and `view_yaw_lock::live()` returns a whole block. `bumpAction()` is called on every action edge detected in the XR frame (see `Action` in `direction_report.hpp`), and the heading is published again on that frame. Consumers record the yaw and serial they used (`noteUse`): the eyes (`game_fp::frame`), the hand and marker map (`game_xr.cpp`, the arm anchor), and the arm IK (`native_body.cpp`, from the snapshot the XR worker hands the pose job, which carries `lockAction`). The audit prints the worst skew and how often a consumer was behind.

What reads the heading and where it is refreshed is unchanged from the table in the plan: the HUD markers and the web shooters take it through the hand/marker map, so they see the same block. Remaining skew sources are in the audit, and slice 4 removes the late writes (the facing check's view fix and the handoff's heading write) that cause the one that is left.

## The stick runs where you look when you press it (slice 2; `headset_first`)

`hybrid_steer::Basis` is the left stick's world basis. Resting: the live view (shown + hmd; it needs the view of the frame before, so it is `lastViewYaw` plus the headset's turn since). At the press: latched. Held: `yaw += the headset's turn this frame + the right stick's turn`; the hero's facing and the heading are never read. The run direction off the basis is `hybrid_movement::viewRunDirection`: 0 inside 15 degrees, a blend to 25, then the stick's own angle over the whole circle, so a diagonal is that angle off where you look and the back half runs backward (no rear snap: the game turns the body round, the view stays). The same basis feeds the swing module's ground move vector, the walk stick sent to the game, the air stick, and the diag's `direction:` line (`basis`). The head cone's old steering and the gate's settle logic are used only with Headset first off.

Look for: `stick basis: press #N latched at the view V (heading H, hero R, stick S deg)` at each press, and in `direction:` that `basis` equals `view` at rest and moves only with `hmd` and your own turns while held. `yaw sync: gap over 45` episodes should fall to the game's own turn lag.

## Landings face where you look (slice 3; with `headset_first`)

`hybrid_movement::Heading::In::view` carries where the player looks (shown + hmd, `hsViewNow` in `game_xr.cpp`). A facing hold (a landing's) now arrives when the hero faces the view, and the forced turn (`view_yaw_lock::TurnInPlace` `forceYaw`) is retargeted every frame onto the live view, from the landing frame, any angle. The fight tail does not hold a landing's turn off. A stick held through the landing hands the turn to the stick (log line above); its basis is the view.

Look for: after `hybrid: vr->anim (land ...)`, `facing check: landing hero turn start ...` on the landing frame, `hero turn done` with `left` under 12 deg, and in `direction:` that `hero` approaches `view` (not `momentum`) within a few hundred ms of `action land`.

## The view is yours (slice 4; with `headset_first`)

`shown = lock + hmd` where `lock` (= `hybridHeading.yaw`) changes only from: the right stick (`viewLock.turn`), the snap realign, a handoff's continuity write (`hybrid landing: ...` sets the heading to the view SpidyVR showed so the head-locked view starts on it), the relatch absorb (below), the view guard's correction, and the punch snap. What was removed, per the options table in the plan: sway weight (0), the heading following the hero (`viewOwn`), the facing check's `viewFix` (kept: logged only), the rear snap (the stick runs backward instead), and the defaults of head ride and shoulder limit (off, still rows in the launcher).

**Relatch absorb.** An owner switch or a recenter re-references the head (the headset's turn off the reference becomes zero). The heading absorbs the old turn (`hsRelatchHmd`) so the view does not move; a realign or a handoff has already written the heading that frame, so no absorb then.

**Pitch and roll.** Not touched: they follow the animated head as before, with `camera_pitch_pct` 50 and `camera_roll_pct` 25 (the user: "Keep animation pitch/roll tilt on the view, just no yaw turning").

Look for: `imposed view turn` at or near 0 in `direction audit:` outside your own turns (it was about 880 deg a minute); `view guard:` lines rare (each is a hole to find: it names the last action); `facing check: ... view ... off, kept (the view is yours)` instead of `corrected`; in `direction:` `view` equal to `shown + hmd` and `hmd`, `shown` moving only when you turn.

## Runtime, lag and loading (slice 6)

**Where the longest visit goes.** `game thread sections:` follows `game thread cost:` every 10 s: `longest visit N us = lock waits A, crawl B, wall tests C, grab and tethers D, grip settings E, punch scan F, kicks G, solver H, untimed R; section peaks us: ...`. The playtest had 5 visits a second over 5 ms and one a window over 20-57 ms with the solver and kicks at 0.7% of it. If `untimed` is the whole of the longest visit, the time is outside `visit()` (the game thread was preempted, or the game's own call). Look for: which section carries the 20 ms visit. (`SpidySwingCostSections`, 88 bytes, `hitch_guard::SectionWindow`; the sections are timed with QPC around code that already ran.)

**First-use starts.** `module start: controller world rays took N ms`, `native swinging`, `swing settings`: any start over 20 ms. The 750 ms `prepare` stall at t=15 s (and G1's 763 and 827 ms) is not named by the logs we have; this names it next time.

**Low fps.** `low_fps.hpp`: windows of 5 s, SLOW after 60 s under 15 fps, normal again after two windows at 24 or more. In the diagnostics: `low fps: ...` then quiet mode, `low fps over: ...`. In the console and launcher: `WARNING: the game has run slowly, about N fps for N s ...` (one per spell), `The game is back up to speed: about N fps.` A stopped game (no frames) is not this: the freeze watch has it.

**Defaults.** Scene resolution 50% (was 67%), STEADY 200/200/200 ms (was 350/300/450) with speed terms .2/.2 (were .12/.12). The 4d58169 sessions all ran the scene at 67% (1120x1128); the 50% advice had not been applied. The reset path: movement-defaults revision 2 and the launcher's `upscale_default=2`.


## Punch auto-target (slice 7)

`punch_track.hpp` (pure): `Throws` (one per hand: a free fist's speed past 2.2 m/s after it was under 1.2, or after 0.45 s) and `decide` (the targeted enemy only; the cone is the angle between the fist's velocity and head -> his middle; the range is the distance). `game_punch.cpp` judges each throw on its sample against `lock.target()` among the fighting enemies and publishes `SpidyPunchTrackLive` (`events` last); `game_xr.cpp` reads it before the realign block, so the same frame carries the turn: `facingHeroPending` + `Event::punchTrack` (forced hero turn, 0.6 s hold, not held off by the fight tail), the punch action bit (the heading's consumers re-publish), and `camera_mode::realignToHeading(yaw)` only with PUNCH VIEW SNAP. Cites: enemy positions from `game_targets::position` (C0_FINDINGS section 2.1; REFERENCE.md lines 126-127 for the bots whose state something else moves, which the watch already skips); no velocity of an enemy is read and nothing new is offset-based.

Look for: `punch track: ... tracked the targeted enemy` on a throw at a targeted enemy, followed in the same or next frame by `facing check: punch track hero turn start` and a `direction:` line with `action punch` where `hero` approaches the line to the enemy while `view` stays at your gaze (or snaps with the switch); `punch track: ... not tracked (outside the cone ...)` for a throw wide of him. A punch with nobody targeted logs nothing here.


## Jump pull (slice 8)
A jump toward an enemy is pulled toward him a little (`jump_pull`, on; launcher Movement page > Combat, rows JUMP PULL, JUMP PULL GAZE 20 deg, JUMP PULL STICK 25 deg, JUMP PULL RANGE 12 m, JUMP PULL LIMIT 1.5 m/s). `jump_pull.hpp` picks the enemy (the gaze's cone, or the held stick's, the nearer in angle; range; not inside 1.5 m) and gives the acceleration (4 m/s^2 at most, faded at the edges, halved vertically, not in the first 0.15 s, capped at the limit per jump). XR sends the view's gaze (the headset-first view heading + the head's pitch) and the stick's world direction every frame through `SpidyJumpPull`; the swing module picks and `Swing::step` applies it in its own air only. `SpidyJumpPullLive` reports each pick and how much a jump had added; the `jump pull:` lines show `added 0.00 m/s` when the jump was the game's own (not pulled by design: its air velocity is never written). Tests: `tests/jump_pull_tests.cpp` (pick, fade, cap, the swing's air, defaults).

## Handstand balance and leap (slice 9)
Hard to fall out of, easy to leap out of (t67u). Before -> now: support radius 12 -> 18 cm; fall region = support -> support + 6 cm; off-balance time before a fall 120 -> 500 ms; fall grace after the catch 150 -> 600 ms; lean cap 35 -> 50 deg; assist gain 300% -> 500%; damping none -> 250% (2.5 /s); push-off threshold 0.6 -> 0.45 m/s; palm re-placements carried in both states (was catching only); new leap: A press / hard stick 0.15 s / push -> 5.5 m/s up + 4 m/s forward along the view. Launcher: Legs page rows BALANCE SUPPORT, SWAY TIME, SWAY DAMPING, HANDSTAND LEAP (fall angle and assist strength have new defaults). Read the `handstand exit:` line: `fell` with a small `off balance` and lean means the support/grace is still too tight; `other (missed ...)` is the palms leaving the floor in the catch; `leap` shows how long it held and the launch.
