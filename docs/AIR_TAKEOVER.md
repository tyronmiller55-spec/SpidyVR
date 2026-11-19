# Air takeover (Nov 14, branch `feat/air-takeover`, on 331a70b)

The user's words (t71u, t72u, t73u and the answers to t73s656) and the analysis of the b9b2196 session
(`game-vr-20261114-164317`) are in [CHANGELOG.md](CHANGELOG.md); numbers for each check in [VALIDATION.md](VALIDATION.md);
the game's facts this builds on (the capsule, the landing, the swing's takeoff) in [REFERENCE.md](REFERENCE.md). The rule all of it
answers to: the headset leads the view (HEADSET FIRST); nothing here writes the view's yaw, and no step writes a velocity to the game.

## 2. View turns that were fought over
With Headset first on, the head ride (`anim_head_ride`) and the shoulder limit (`shoulder_limit`) are ignored whatever the
settings file holds: they turn the view and the view guard turns it back, every frame. `direction audit:` now ends with
`of the imposed turn the head ride had N deg and the shoulder limit N deg; body tilt worst N deg, N frames past 100`.

## 3. The rear landing
`facing_check::rearTurn` (120 deg): a calm landing (no fight, no dodge) with the view further than that behind him does not force
the hero round. The turn itself cannot be eased: there is no facing write, and the game does not turn its actor in SpidyVR's air
(hybrid_movement.hpp `airSyncStick`), so the 142 deg/s walk-turn is the game's. Fights and gaps under 120 deg are as before.

## 5. A in the air
`air_correct_ms` (1000; 400-2000 in the launcher, 300-2500 in the ini): REALISTIC FLIPS' A press turns the body from its tilt to level about the heading as one minimum-jerk turn (`RealisticFlipSettings::levelMs`). A half turn takes the whole time, a smaller tilt its share (floor 30%, 0.2 s). The old rule chased the error at up to `speed` deg/s. The turn is the body tilt only; it never writes the view, and the heading it takes (`takeHeading`) is unchanged.

## 6. Point jump bullet
`point_jump_bullet` (on): the body's axis takes the raw look at the launch, eased 120 ms, any angle (straight down: a half turn about the heading's right). Body only (`pose.tilt` in `game_xr.cpp`'s native body), so the view and the heading are untouched; the velocity keeps the 10 deg out-of-surface fix. Lets go on touchdown, web, A (a fresh press: the chord's own A is not one), hands holding, under 6 m/s after 0.4 s, or 4 s.

## 7. The air is SpidyVR's
The takeoff animation plays; the air is VR's at the first of `jump_handoff_ms` (300), a jump's vertical speed under 4 m/s after the 0.25 s minimum dwell, or a plain fall after `fall_handoff_ms` (150). Web, grip, flip stick, Spidy's wall and a point jump still take it at once. Fights, dodges and zips keep the animation; the landing is the animation's. A fall has no takeoff animation, so the 0.25 s dwell applies to jumps only.

## 8. Landing blend-in
`air_landing_blend` (ON): a ground ray (native tag 8) takes the roof ray's slot while he is in SpidyVR's air; `landing_blend::timeToImpact` with the vertical speed gives the time to impact; the body comes upright over the last 0.3 s (body only, never the view or the heading); the touchdown handoff and the 180 ms eye blend are the fallback and are unchanged. One `landing blend:` log line per landing says how good the prediction was.

`air_landing_release` (on by default since revision 14 for further testing; UNVERIFIED): at 0.2 s to impact the owner hands the animation the body (`EnterAnim::predicted`, "predicted-landing"). It stays there to the landing, and comes back to VR if the prediction is lost for 0.2 s or no landing came in 0.8 s. Never in a fight, a zip, a hold, a handstand or inside the owner's 0.25 s minimum dwell. No velocity is written.

### The probe (what has to be true before the release is turned on)
1. The game keeps his velocity when the body is let go in the air. A swing-owned body released with `air_landing_release` on should show `native swing` speed within a few percent across the `hybrid: vr->anim (predicted-landing)` line.
2. The game's own landing clip is the normal one, not the hard landing, for a fall of 5-12 m/s down (look for `landing hero` lines and the animation's landing name in the sound/animation trace around the touchdown).
3. The `landing blend:` error stays within 40 ms over at least 50 landings, so the release point is the right one.
It is on by default since revision 14 so testers can try it; `air_landing_release=0` turns it off. If probes 1 or 2 fail in a session, turn it off and report the log. (Releasing a swing-owned body in mid-air is what `game_swing::AirYield` does for the game's own zip and strike, which suggests 1 holds, but that path only runs while the game has begun a move.)

## 9. Airborne requests
Web strike (Y), web zip and now any other game move asked for in flight (a prompt's button, a finisher's) hand the body to the animation only when the game takes it, and the swing has him back at the end of the move. After a request or a fall's start the timers do not take the air back while the time to impact is under 0.45 s (`Frame::landEta`): the animation keeps it to the landing. Not wired: an air dodge (B) and finishers with no button. A 0.25 s owner-wide minimum dwell is kept only where it is new (the takeoff and the landing release); the punch's 0.19 s VR stint is the strike hold's and is unchanged.
