# HANDSTAND, phase H1 (November 12)

Experimental, **off by default** (`handstand=1` in spidy_vr.ini). REALISTIC
FLIPS only (`paramsOf` needs `realistic_flips=1`). Leg control (Y+X) stays usable.

## What it does

In SpidyVR's air (hybrid: VR owns him), past 90° inverted the palms probe
for a floor (HandCollision). At ≥100° inverted with **both palms on a floor**
and spin ≤3 rad/s, the hands **catch**. Then the body **balances** on them:
a linear inverted pendulum about the palms, with the DCM steered back into
the support (palm segment widened by 0.06 + 0.06 m). The view holds that lean
through RealisticFlip's support override. Exits:

- **push**: the tracked palms driven into the floor (arms extended) at over
  0.6 m/s, averaged over 50 ms. Launch = push speed × 2.2, capped 7 m/s
  along the normal and 4 m/s across. It goes through `SpidySwingPointJump`
  (the point jump's own event, **no smoke**) and the point jump's takeoff hold.
- **step-down**: palms lifted without a push. RealisticFlip rights him as on landing.
- **fall**: the DCM outside the support for 0.12 s, or a lean over 35°.
  RealisticFlip goes on with the tip's spin.
- **grip**, **tracking lost**, **disabled**, **missed** (palms off before the catch held).

A new catch waits for the palms to leave the floor first.

**Inverted eye drop (needed for any catch):** with the current placement
the eyes stay about 1.6 m above the capsule's feet, so inverted palms never
reach a flat floor. With handstand on, the tracking space comes down by up
to 0.9 m (`handstand_eye_drop_cm`), from 100° to 160° of inversion (smoothstep),
eased at 2 m/s or less. It only applies in VR's air or while the hands hold him.

## H1 checklist (the Handstand page) — status

| # | Item | Status |
|---|---|---|
| 1 | `include/spidy/handstand.hpp` (pure) | **done**: Params, State {off, catching, balance, leaving}, Exit, Support, Balance (DCM, substeps of 1/120 s or less), Push, launch, supportTilt, Handstand |
| 2 | RealisticFlight::handSupport / supportTilt / supportAngular, RealisticFlip::release | **done** |
| 3 | HandCollision floorContacts | **done**: normal within 40° of up, the existing −2..+12 mm gap; floor patches kept 1.0 s within 0.5 m while probing |
| 4 | game_tracking: probe when inverted >90° in VR air or supported; support override; release | **done**: uses last frame's balance; plus the eye drop |
| 5 | GameTrackingRig members / setters / resets | **done**: `handstandSettings`, `handstandVrAir`; reset with tracking loss and recenter (push history) |
| 6–7 | swing Input handSupport + Command version bump | **not done, by choice**: the capsule stays the game's on the floor. The push uses the existing point-jump side channel, so there's no Command bump that could collide with feat/passive-legs |
| 8 | Owner: `Frame::handSupport`, landing waits | **done** as an early return before GROUND (the landing line is unchanged). No `EnterVr::handstand` (already VR); log `owner: kept vr (handSupport)` |
| 9 | Settings | **done** (ini-only): handstand 0, handstand_assist 1, handstand_push_gain 220, handstand_push_max 7, handstand_fall_degrees 35, plus handstand_height_cm 95, handstand_gain_pct 300, handstand_eye_drop_cm 90. Launcher rows (Legs page, "Handstand (experimental)"; each says it needs REALISTIC FLIPS) added Nov 14; no in-game menu row |
| 10 | body_ik palm pin | **deferred (TODO)** |
| 11 | Logs | **done**: `handstand: catch`, `handstand: balance ...` (2 Hz), `handstand: exit <why>`, `handstand: push-off (x, y, z) m/s`, `owner: kept vr (handSupport)` |
| 12 | Tests | **done**: tests/handstand_tests.cpp (catch gates, balance converges, assist off falls, fall/push/step-down/grip/tracking/missed/disabled, caps, re-arm, supportTilt, RealisticFlip override + release, owner hold, settings); core_tests defaults text |

Not yet: the "palms moving toward the floor" catch condition, A's hand hop (H2),
legs shifting ξref (H2), handsprings (H2).

## Known risks (headset needed)

- The capsule is not held up: if the palms catch while the capsule is still
  falling, the body keeps falling until its feet land. HandCollision's push
  (up to 0.45 m) keeps the eyes off the floor meanwhile.
- The lean pivots the view about the head, not the palms, so the hands can
  appear to slide until the body_ik pin (step 10) lands.
- A fall on a flat floor: the capsule is already grounded, so ANIM can take
  him back right after the 0.3 s leaving hold (no visible tumble), possibly
  with a camera snap.
- The eye drop also applies to inverted flips in VR air with handstand on.
- A pressed while supported still reaches the game (H1 does nothing with it).

## The landing catch, second pass (Nov 13 22:24 playtest)

28 touchdowns judged of 63 SpidyVR landings (18 "not inverted enough", 8 "a grip held", 1 palms high, 1 untracked); the one catch, 161 degrees inverted, fell after 63 ms (lean 43.7 degrees) because the balance started from the landing's 9.3 m/s. Changes (handstand.hpp, hybrid_owner.hpp, game_xr.cpp; no offsets involved):

- The balance starts from the landing's speed and spin damped (`entryKeep` .12, `entrySpinKeep` .2, at most `entryMaxSpeed` .5 m/s: the palms' support is ~0.15 m wide, the pendulum's divergence 3.2/s).
- A grip squeezed at the touchdown no longer refuses a catch with both palms down, and is carried: it lets go only once released and squeezed again.
- `touchdownReach` 1.0 to 1.6 m (the eyes' drop .9 plus an arm .7): the playtest's refusal had the palms 1.54 m over the floor at 121 degrees.
- The landing's hand-off to the animation leaves the catch its first 0.25 s (`hybrid_owner::handstandLandNs`): the touchdown is judged in the animation's domain too, and a catch there takes SpidyVR back (`EnterVr::handstand`).
- diag: every failing gate on each refusal ("every failing gate: ..."), and a line for a landing handed over with no touchdown judged, with the inputs that kept it out.

## TODO

- `TODO(handstand sound)`: the push-off sound and grunt; no sound IDs yet (SOUND TRACE, 1008510).
- body_ik palm pin (step 10). Menu row.
