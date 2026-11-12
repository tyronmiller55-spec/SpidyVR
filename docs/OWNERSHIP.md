# Ownership: who drives Spider-Man's body (ANIMATED HYBRID)

The single ownership table. It **replaces** every earlier ownership rule:
- the Nov 8 08:50 ground-release rule;
- the Nov 8 21:51 "grip cancel stays in SpidyVR until a native animation starts" rule;
- jump adopt (SpidyVR taking the body on the takeoff frame);
- the mid-air rotate and speed takeovers;
- the ground grip cancel;
- launch facing.

Source: the user, Nov 9 09:34 PST. Code: `include/spidy/hybrid_owner.hpp`, the `HybridOwner::update` function.

> Animation owns the ground, parkour, landings and jumps. SpidyVR owns the air and wall running / wall jumping.

| Where / what | Owner | Switch |
|---|---|---|
| Ground, perch, the game's crawl, parkour | ANIM | VR → ANIM the moment he lands (`land`, or `mount` onto the game's crawl) |
| Ground + grip or trigger | ANIM | **None.** Grip only enables SpidyVR's arm features on the animated arms (webs, web balls, punches, markers). The one exception is a punch's web strike under FULL ARMS (next rows). |
| Jump, vault, point launch, Y zip, fall: any midair native animation | ANIM, then VR on the timer | **Nov 12 05:41 (JUMP TO SPIDYVR, `jump_handoff_ms`, default 500):** a plain jump, a fall, a zip launch, a vault or a wall launch plays animated for the timer's time in the air, then hands to VR (`jump-timer`; diag `jump handoff: <kind> after X ms`). A zip launch's timer counts from its rise; the zip's pull itself stays ANIM. The timer waits while he fights, dodges or zips. Landing before the timer: no switch. **STAY TO LANDING** (0) keeps the Nov 11 23:39 rule: the animation plays through the fall to the landing. |
| Midair animation + a flip's pitch or tilt (either stick, see below) | VR | `flip-break`, **on the frame the input is read** (`HybridOwner::breakForFlip`, before that frame's tracking, hands and body). |
| Midair animation + grip | VR | Grip cancel (`grip-cancel`). It is consumed and fires no web or ball. **Nov 11:** on the frame the gate consumes it (`breakForCancel`). |
| Midair animation + Spidy's wall | VR | `wallrun` |
| Midair animation + a SpidyVR web attaching | VR | `web` |
| Air after a break (flip, grip, web, wall) or the jump timer | VR | It stays VR until landing; landing on the feet always returns to ANIM (`land`). |
| Y zip pressed from VR in the air | ANIM | `zip`, once the game begins the zip. A zip the game refuses never leaves VR. |
| Spidy wall run / wall jump | VR | none |
| Wall run that came out of a Native Run, stick let go on the wall | ANIM | `run-release`. It stays ANIM on that wall. |
| FULL ARMS (`animated_full_arms`, on): a punch opens a web strike in an animation | VR | `punch`, for the strike's flight and hang, even on the ground. The hold needs a fresh strike sample (within 100 ms), lasts 4 s at most, and freezes the setting while it lasts (`full arms strike:` diag). It lands back in ANIM. |
| POINT JUMP (`point_jump`, off by default) fired from the ground, a perch or Spidy's wall | VR | `point-jump`, **the approved ground exception** (the user, Nov 11: point jump may take over from the ground animation; lock the feet at the switch). On the frame the launch is sent (`Frame::pointJump`); the position lock puts the origin on the actor's feet; VR is held on the ground until he is off it (`Frame::pointJumpHold`, at most 400 ms). The air is VR's; landing on the feet returns to ANIM (`land`). See [POINT_JUMP.md](POINT_JUMP.md). |
| Cutscene / stale sample | frozen | none |

**Rules:**
- No ownership switch happens on the ground, except a FULL ARMS punch's web strike (the user, Nov 9 22:27: "Punching can pull you out of the animation and teleport you") and a POINT JUMP's launch (Nov 11, approved).
- Otherwise, switching happens only in midair, on wall runs, and at Native Run's release.
- The position lock (`position lock:` diag) runs on every switch.
- **View handoff (Nov 12 05:41):** a punch, the jump timer, a flip break and a grip cancel hold the view where it was and ease it onto the new eyes (one code path, `view_ease.hpp` / `beginViewEase`; the rig placement moves, so eyes, hands, webs and body move together). Punch: PUNCH VIEW EASE; the others: AIR VIEW EASE (`air_view_ease_ms`, 250 by default; OFF = the old snap). Landing cancels an ease (`cancelled (landed)`). Diag: `<why> view ease: start offset X m, dur N ms` and `<why> view ease: <end> after N ms`.
- **Legs across the timer:** Y+X may engage the legs during a midair animation (it is not zipped); an engaged hold carries into VR, and the air legs blend in over 0.2 s.
- Same-frame breaks (flip, grip cancel, web, Spidy's wall) still come first, before the timer; right-stick turning never breaks; point jump and swing releases are instant.

## Flip break (the user, Nov 11 23:39 and 23:41)

> "Flips with the left or right stick cancel animations on that frame ... landing on your feet always snaps back to animation. Jumps are in animation until it's broken with a rotation, or grip cancel." "Turning left or right on the right stick should keep you in animation. Any cancel or pitch/tilt will break animation on that exact frame."

What counts (`hybrid_owner::flipBreak`; dead zone 0.2, noise only):

| Input | Breaks when |
|---|---|
| Right stick left / right | **Never** (the snap or smooth turn). |
| Right stick up / down (pitch), diagonals by their up/down part | Basic FLIPS (the right stick flips), or LEG CONTROL in force (with CONTROLLED or REALISTIC FLIPS; the legs' swing pitches him). Not FLIPS' zip flick (`zipPriority`). With no flips on the right stick, up / down stays the zip / heal push and breaks nothing. |
| Left stick, any way (pitch and tilt) | CONTROLLED FLIPS toggled on in the air (A), or REALISTIC FLIPS with FLIPS on (its left-stick steering). Otherwise the left stick steers the game's jump and breaks nothing. |
| Grip cancel | Always, in a midair animation (consumed, no shot). |

How (game_xr, `flip break:` and `grip cancel: anim->vr this frame` diag lines with the display time):
- The right stick is read before the legs take it and before `rig.update`; the left stick right after `rig.update` (it reports the controlled flip's toggle). Either calls `breakForFlip`, which switches the owner at once; the owner's update later in the same frame reports `enterVr` with `flip-break`, so the usual handoff runs that frame: position lock on the actor's feet, the pose seeded from the air (`adoptsFromAir`), the swing's velocity kept.
- The tracking rig reads the same stick that frame, so a basic flip or a controlled / realistic flip turns from the first frame (a basic flip's stick must have been at rest once in the air, as before).
- With LEG CONTROL the push that broke goes to the legs from that frame (`legStickOwns`): a double tap's first push counts, the zip / heal fires on the second tap as in VR air. In the air with LEG CONTROL a single up / down push is the legs', not a zip.
- On the ground nothing switches; a jump breaks on its first airborne frame if the stick is held.
- Review M1 (ownership decided a frame late) is fixed for these two paths only; the other switches still decide in the owner's update.

## When "the animation ended"

The game does not tell the mod when a jump animation ends, so the mod uses a proxy (`hybrid_owner.hpp` constants). The animation counts as ended when all of these hold:
- he has been off the ground at least **0.3 s**;
- he has been falling (vertical velocity ≤ 0) for **0.15 s**;
- no zip, fight or dodge is still going on.

It always ends at **2.5 s** at the most. Since Nov 11 this is only read (diagnostics); the animation keeps the air until a break or the landing.

The proxy has these consequences:
- A short hop or step down lands inside 0.3 s and never switches.
- A point launch rises the whole way to its top, so it plays in full.
- A Y zip plays while its keys go out, and then through what it leads into:
  - its point launch ends at the launch's top, by the same fall rule;
  - a zip with no launch plays until he lands or perches, or 3 s.

## Nothing of the mod's is steered into the game's animation

The following were removed: launch facing, the facing seed after a handoff, and the model-yaw R-stick hero turn during a midair animation.

In the air, the R stick turns the camera as in the main game. The player's own L stick is still mapped to the view, as on the ground.

## Facing check on every event (Nov 12 07:14)

Every ownership event (landing, point launch, jump handoff, anim re-entry, takeover) checks the facing within 0.3 s against the view shown before it: a view more than 45 deg off is turned back (yaw lock or rig), a hero more than 90 deg off on the animation's side is turned to it by the walk stick (forced TURN-IN-PLACE). Details: [FACING_CHECK.md](FACING_CHECK.md).

## MOVEMENT MODE HYBRID (Nov 12, t75u)

In HYBRID the walk stick is the mod's on the ground (front half: along the right-stick heading; rear half: one snap) and, in SpidyVR's air, a light stick toward the view keeps the game's facing with it; a landing holds the heading on the shown view while the walk stick turns him. STABLE and OLD keep the rules above. Details: [HYBRID_MOVEMENT.md](HYBRID_MOVEMENT.md).

## Defaults chosen (flag if wrong)

- **Air cancel input** = grip, the web-hold button. If WEB BUTTON is swapped, it is the trigger. Web balls never cancel.
- **Native Run release** applies to a wall run entered within 0.5 s of Native Run's RT. "Released" = left stick tilt under 0.2. In open air the release changes nothing, because SpidyVR owns the air and landing hands back anyway.
- **ANIMATED HYBRID SPEED** setting is still parsed but no longer used, because the speed takeover was removed.

## Arms in an animation (Nov 9 22:27)

While the animation owns, the arms are the player's own SpidyVR arms (`include/spidy/arm_frame.hpp`). The controller is mapped into the game every headset frame through the same map the eyes use:
- the headset reference;
- the view's turn;
- the animated head's eye anchor, as the actor stands now.

Webs, web balls, markers and punches aim from that pose, and the glove is solved onto it. When the player torso's anchor is missing, the old pose-job mapping is used.
