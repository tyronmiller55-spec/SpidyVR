# FACING CHECK

The user, Nov 12 07:14 PST: "Take a look at landings and point launches again. There are times where it turns me 180 on landing or launch. Maybe we need a check on each event."

## What the 06:37 log showed (build 319e734)

| event | n | view >90 deg within 0.35 s | hero/body >90 deg off expected |
|---|---|---|---|
| landing | 166 | 56 * | 52 |
| flip break | 98 | 3 | 20 |
| point launch | 61 | 0 | 21 (13 >135; 48 of 61 were wall launches, often backward) |
| jump handoff (timer) | 28 | 0 | 9 |
| web takeover | 23 | 1 | 7 |

\* Landing view figures compare the head probe's headset yaw before the landing against the shown view on the animation's side afterwards; the probe's headset frame is approximate, so treat the column as an upper bound. Reliable numbers: at 52 of 199 yaw-lock latches the head bone and the actor's facing were more than 135 deg apart, and after landing the animation turned the hero onto the actor's facing within ~0.3 s.

Cause: **the game's landing animation plays on the actor's own facing**, which SpidyVR's air does not drive (it stays put or follows the game's motion), not on the player's view. Not the handoff latching a wrong heading (the latch takes the SpidyVR body's head on the first animation frame, which is the view), not the view ease (position only), not a velocity-facing push (removed in b84b3a7). A latch a frame late, on the game's already-turned head, would show the hero's way: the check covers that too. The 20% sway added a ~35 deg slide on each such landing.

## The check

`include/spidy/facing_check.hpp`, wired in `src/game_xr.cpp`.

- Armed at: the vr->anim edge (landing / anim re-entry; expected = the view SpidyVR showed last), the anim->vr edge (jump handoff / takeover; expected = the shown view the rig adopts), the point launch (expected = the launch direction if within 90 deg of the view, else the view).
- View: the first sample after the event (animation side: lock + sway + headset; SpidyVR: the rig's head, from the frame after the edge). More than 45 deg off the view before: turned back now (`viewLock.yaw` or `rig.adoptEyes` about the eyes). Without the lock (head follow not Full, sway 100%) it is reported, not turned.
- Hero (animation side only; SpidyVR's body is built under the view): more than 45 deg off expected (90 until the G1 playtest, Nov 14: 46-50% of landings were over 45 off and half of those were logged "ok") starts a forced TURN-IN-PLACE to it: any angle, no cooldown. **The turn stays pending until a legal standing frame** (grounded, left stick idle, no fight/dodge/zip/leg stick), up to 2.5 s (it waited 0.4 s before, and 65 to 100% of the landings' turns were dropped for want of a frame in that time). The player's own left stick wins: held for 0.25 s in a row it drops the turn (he is walking where he wants; a tap does not). It rides out gaps up to 0.25 s, survives a lock relatch, and gives up after 2.5 s on the turn itself.
- Dropped turns say why: `facing check: landing hero turn dropped after 2500 ms (blocked by fighting, 2300 ms of it)` or `... after 270 ms (the player's left stick won: the left stick held, 270 ms of it)`. The reason is the first failing gate (the animation does not own him yet, no hero facing, stale swing sample, not grounded, the left stick held, fighting, dodging, zipping, the leg stick); the one named is the one that held longest.
- Logs: `facing check: landing view expected 97 got 97 (ok)`, `facing check: landing hero expected 97 got -80 (corrected), walk stick turns him`, `facing check: landing hero turn done after 640 ms, turned +170 of +177, left +7 deg`.

## Limits

No direct facing write exists (no address is guessed): the hero's correction is the walk stick, so he visibly turns (about 250 deg/s) after a landing, and a landing straight into a run (left stick held) keeps the game's facing until you stop.
