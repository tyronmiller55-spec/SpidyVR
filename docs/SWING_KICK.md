# Manual swing kick (K1)

Off by default, EXPERIMENTAL, like the handstand. `swing_kick_manual=1` in `spidy_vr.ini` (or **Swing kick (experimental)** on the launcher's Legs page) turns it on. `swing_kick_auto` (on, as before) is the existing dive (`WebStrike::dive`) as its own setting; both can be on, and the dive holds back while a hand kick is under way.

## Controls

1. Swing on a web (VR owner, at least one web holding, off the ground and any wall) at 9 m/s or faster.
2. Engage the legs (hold Y + X: `leg_hold.hpp`, which needs LEG CONTROL and a flip mode as today).
3. The ring marks an enemy 3 to 14 m away who sits within 30 degrees of the swing's **velocity** (not the view).
4. Flick the right stick up: from under 0.3 to over 0.8 within 80 ms, within 50 degrees of straight up. A slow push is still the jackknife pose; down and sideways are not kicks.

No new buttons. While the kick is armed (and the fade after it) the legs keep the right stick, so its push up is not the game's zip.

## What it does

- **Rope assist:** each ready web's length moves toward the enemy's distance from its anchor: `length_new = length + clamp(|A - P| - length, +-1.2 m)`, at most 2 m/s, scaled by the assist's weight. The taut-rope clamp is off while the assist runs (otherwise the rope would be pulled straight back).
- **Side steering:** the velocity turns toward him at 2.5 m/s^2 at most (the swing's own air acceleration). The turn keeps |v|, so the assist never raises the speed.
- **Impact:** the foot's swept 10 cm sphere meets the ringed enemy's capsule. The blow goes through `game_punch` as a kinetic blow (`kickBlow`, the dive's strength), his flight takes 70% of the player's velocity (`BotStateFlung` through `native_bodies::fling`, as every kinetic blow), the player keeps 60% and goes on swinging.
- **Miss:** he is passed, gone, the web lets go, or 1.6 s pass: the assist fades over 0.35 s, the legs return (250 ms after), nothing snaps.
- Only the ringed enemy counts (the lock's mark, the same one the dive uses).

## Where it lives

| Piece | File |
| --- | --- |
| Gate, cone, flick, clamps, foot sweep, the kick's states | `include/spidy/swing_kick.hpp` |
| Rope and steering in the swing's step, recoil | `Swing::kickAssist`, `Swing::kickRecoil` (`swing.cpp`) |
| Arm, update, blow, the XR channel | `game_swing.cpp` (`SpidySwingKick`, `SpidySwingKickLive`; wire structs `SwingKickAsk` 32 bytes, `SwingKickLive` 104 bytes in `game_swing.hpp`; `game_swing::Settings` is untouched) |
| The ringed target, the blow, the dive's hold-back | `game_punch::kickTarget`, `game_punch::kickBlow`, `Player::noDive` |
| Flick, settings sent, log, legs | `game_xr.cpp` |
| Tests | `tests/swing_kick_tests.cpp` |

No game address is used: the target and the blow are the ones the web strike and the punches already use (REFERENCE.md and VALIDATION.md list nothing new for K1; the plan's table says RE: none).

## Log lines

`swing kick: flick (stick x, y)`; `swing kick: armed target=<handle> cone=<deg> speed=<m/s> distance=<m>`; `swing kick: hit|miss assist=<m> steer=<m/s2> (...)`; `swing kick: refused (<why>) ...`.

## Unverified (needs a headset)

- The foot is modelled (the pelvis 0.9 m over the feet, the leg reaching 0.95 m toward him), because no foot world position is read back from the body. The legs' pose (`body::Targets::kick`) may reach a little more or less.
- The rope and steering numbers are the plan's; how they feel at 9 to 25 m/s is not known.
- Heavies and scripted fights may ignore the flight (the plan's risk: "Fighting AI overrides flight requests on some enemies").
