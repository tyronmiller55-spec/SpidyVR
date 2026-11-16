# PUNCH VIEW EASE (November 12)

The user, Nov 12 01:13 PST (t66u): "go for hold and ease". Built on bec50c8 (feat/leg-toggle). Not
headset-tested.

## Problem

At a punch's ANIM -> VR switch the position lock puts tracking over the actor's feet at once. In the
22:38 session the view jumped a median 0.69 m (max 1.31 m) at punch switches; the discarded anim-view
offset was a median 0.38 m, the rest the eyes' travel during the lunge. The existing 180 ms eye blend
crossfaded toward a moving target, which still read as a jump.

## What it does

- On a punch switch (`EnterVr::punch`) after `lockToFeet`, `GameTrackingRig::beginViewEase` takes
  offset = the eyes the last animated frame showed (`worldEyes`: the shown eyes carried with the player)
  minus where the lock now puts the tracked head. The rig adds that offset to the **placement** (like
  standOff / runViewOffset / headPush), so eyes, head, hands, webs and the body drawn on them move
  together: no glove/web mismatch. The lock (`rig_.origin` over the feet) is not changed.
- The first VR frame holds the shown eyes exactly; then the offset falls as `(1 - t)^3` (ease-out) over
  PUNCH VIEW EASE ms (default 250), never slower than a bleed of 0.10 m/s + 0.2 x the player's speed
  (EngineBot's motion-masked floor), so it ends sooner while he moves.
- Ended at once on: a teleport (feet over 5 m in one frame), a large turn (over 0.35 rad in a frame or
  0.8 rad in total), a recenter, a tracking reset, any other handoff (VR -> ANIM, or another ANIM -> VR),
  or anything not finite. Offsets under 1 cm or over 3 m are not eased.
- Log: `punch view ease: start offset X m, dur Y ms`, then `punch view ease: finished|cancelled (...)
  after Z ms`.
- Setting: PUNCH VIEW EASE on VR CAMERA & STABILIZATION and the launcher's Camera page (OFF (SNAP), 150,
  200, 250, 300, 400 ms); key `punch_view_ease_ms`; personal (not in profiles). 0 = the old snap.

## Scope and risks

- **Punch only.** Every other ANIM -> VR switch (web, grip-cancel, flip, anim end, point jump) runs the same
  code path and could use it with a one-line change, but they have not been checked against logs (a
  flip break mid-tilt, a point jump's ground exception), so they are left as they were. Flagged.
- Hand speed for punches is measured from physical controller motion, so the moving placement cannot fire
  a punch. Webs fired during the ease leave from where the hands are seen.
- Residual: the eyes' own travel during the lunge is still shown, now as continuous motion; the 180 ms
  pitch/roll settle is unchanged.

## Files

`include/spidy/view_ease.hpp`, `game_tracking.hpp/.cpp` (`beginViewEase`, `cancelViewEase`, `viewEase`,
placement step, kept across `reset`), `game_xr.cpp` (start at the punch switch, cancel on other switches,
logs), settings and launcher, tests `spidy_view_ease_tests`.
