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
