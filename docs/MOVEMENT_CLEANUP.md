# Movement cleanup (November 12) — ships as defaults

The user (November 12): "movement fixes will be defaults". The ten-item
list from EngineBot's "SpidyVR - Quick Sidestep & Ground Movement Cleanup"
page was re-ranked against 319e734 (feat/stable-view-yaw). Only items still
needed are in this change, and each is a **default**, not a preset.

| # | Item | Against 319e734 | Here |
|---|------|-----------------|------|
| 1 | RUN CURVE on | still needed (`Values::runCurve` was false) | **default on** — `include/spidy/vr_settings.hpp` (`runCurve = true`) |
| 1b | NATIVE RUN AT with the curve | new finding: with the curve on, 70% of the *sent* tilt is ~49% of the physical stick, so the sprint would start early | `run_update::curvedTilt` + `nativeRunThresholds(percent, curve)` in `include/spidy/run_update.hpp`; called in `src/game_xr.cpp` (NATIVE RUN AT). The sprint still starts at 70% / stops at 50% of the *physical* stick |
| 2 | EARLY JUMP QUICK TAP | still needed (`earlyJump` was 0) | **default 1** (`earlyJump = 1`) |
| 3 | Hybrid view tau 0.45 | obsolete: STABLE heading filters at `stable_yaw::stableTau` (0.1 s) and FULL lock replaced it; `hybrid_steer::viewTau` is only BODY (OLD) | — |
| 4 | ANIMATED HEAD Level on the ground | obsolete / conflicts: FULL head lock + ANIM YAW SWAY needs Full; Level turns `viewLockSteer` off (`src/game_xr.cpp` VIEW LOCK) | — |
| 5 | StickPush stricter | still needed (`game_pad::StickPush` unchanged) | `aside` .45 → .35, `holdSeconds` .15 → .2 in `include/spidy/game_pad.hpp` (class StickPush). The snap-turn guard is the existing `spent_` rule |
| 6 | One radial dead zone | still relevant, **deferred**: touches hybrid_steer, animated_model_yaw, run_update and stable_yaw; too wide for a small diff under feat/passive-legs | — |
| 7 | Punch 0.7 m jump | done: PUNCH VIEW EASE (694861c), generalised as AIR VIEW EASE (edeba41) | — |
| 8 | Left-hand steering frame | changed: not a default; optional later | — |
| 9 | RUN LOGS | changed: switched on by the tester in the headset script, not a default | — |
| 10 | NATIVE RUN AT 75 | changed: replaced by 1b (70%/50% of the physical stick) | — |

## Defaults for existing installs

`spidy_vr.ini` is written whole, so a saved `run_curve=0` / `early_jump=0`
would otherwise keep the old values forever. A one-time migration does it:

- New key `movement_defaults` (revision, `vr_settings::movementDefaultsRevision` = 1).
- `vr_settings::withMovementDefaults(values, fileText)` (src/vr_settings.cpp):
  a file without `movement_defaults=1` gets `run_curve=1` and
  `early_jump=max(early_jump,1)` (RUNNING stays RUNNING) once; the file is then
  written with `movement_defaults=1`, so a player who turns them off after
  that keeps their choice.
- Called where spidy_vr.ini is read: the game (`src/game_xr.cpp`) and the
  launcher (`apps/launcher/system.cpp`, loadTabOnly).
- `movement_defaults` is not a setting: it is left out of shared profiles.

## Tests

`tests/core_tests.cpp`: defaults (EARLY JUMP 1, RUN CURVE on, the defaults
text), the migration (old file, RUNNING kept, choice after migration kept,
no file), NATIVE RUN AT through the curve (sprint at 70% physical), StickPush
(.4 aside is a turn, a 0.19 s brush is nothing, .3 aside still heals).
`tests/launcher_tests.cpp`: profile defaults text, `movement_defaults` left
out of profiles.

Not headset-tested.
