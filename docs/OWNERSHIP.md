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
| Ground + grip or trigger | ANIM | **None.** Grip only enables SpidyVR's arm features on the animated arms (webs, web balls, punches, markers). |
| Jump, vault, point launch, Y zip, fall: any midair native animation | ANIM | Plays from the press or takeoff until the animation ends, then hands to VR **only if he is still in the air** (`anim-end`). |
| Midair animation + grip | VR | Grip cancel (`grip-cancel`). It is consumed and fires no web or ball. |
| Midair animation + Spidy's wall | VR | `wallrun` |
| Midair animation + a SpidyVR web attaching | VR | `web` |
| Air after the animation ended | VR | It stays VR until landing. |
| Y zip pressed from VR in the air | ANIM | `zip`, once the game begins the zip. A zip the game refuses never leaves VR. |
| Spidy wall run / wall jump | VR | none |
| Wall run that came out of a Native Run, stick let go on the wall | ANIM | `run-release`. It stays ANIM on that wall. |
| Cutscene / stale sample | frozen | none |

**Rules:**
- No ownership switch happens on the ground.
- Switching happens only in midair, on wall runs, and at Native Run's release.
- The position lock (`position lock:` diag) runs on every switch.

## When "the animation ended"

The game does not tell the mod when a jump animation ends, so the mod uses a proxy (`hybrid_owner.hpp` constants). The animation counts as ended when all of these hold:
- he has been off the ground at least **0.3 s**;
- he has been falling (vertical velocity ≤ 0) for **0.15 s**;
- no zip, fight or dodge is still going on.

It always ends at **2.5 s** at the most.

The proxy has these consequences:
- A short hop or step down lands inside 0.3 s and never switches.
- A point launch rises the whole way to its top, so it plays in full.
- A Y zip plays while its keys go out, and then through what it leads into:
  - its point launch ends at the launch's top, by the same fall rule;
  - a zip with no launch plays until he lands or perches, or 3 s.

## Nothing of the mod's is steered into the game's animation

The following were removed: launch facing, the facing seed after a handoff, and the model-yaw R-stick hero turn during a midair animation.

In the air, the R stick turns the camera as in the main game. The player's own L stick is still mapped to the view, as on the ground.

## Defaults chosen (flag if wrong)

- **Air cancel input** = grip, the web-hold button. If WEB BUTTON is swapped, it is the trigger. Web balls never cancel.
- **Native Run release** applies to a wall run entered within 0.5 s of Native Run's RT. "Released" = left stick tilt under 0.2. In open air the release changes nothing, because SpidyVR owns the air and landing hands back anyway.
- **ANIMATED HYBRID SPEED** setting is still parsed but no longer used, because the speed takeover was removed.
