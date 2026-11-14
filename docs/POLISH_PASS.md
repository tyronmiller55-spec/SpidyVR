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
