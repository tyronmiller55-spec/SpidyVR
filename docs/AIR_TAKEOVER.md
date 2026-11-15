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
