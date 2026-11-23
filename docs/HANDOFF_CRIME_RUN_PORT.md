# Crime Run port onto Native Animation beta3 (`feat/087-crime-run`)

Port date: November 23, 2026 (Pacific time). Crime Run was not played in a headset as part of this port.

## Tip used

- Work repo: https://github.com/tyronmiller55-spec/SpidyVR (remote `fork`). `origin` is IlyaMez/SpidyVR and was not pushed to.
- User's testing tip: `fork/feat/087-beta3` at `1bb2aa612e36b35e24119c39f75e594d832df96a` (beta3g `c488634` + the Native Animation handoff doc). It had not moved since the Crime Run prototype was written.
- Crime Run source of truth: https://github.com/tyronmiller55-spec/SpidyVR-CrimeRun branch `feat/crime-run` (tip `7966c68`, including the later handoff docs commit). A copy of the first three commits also exists on the fork as `feat/crime-run` (`343ad4c`); that branch was left untouched.

Crime Run's base equals the tip, so the "rebase" was a straight cherry-pick. No hand-port of glue was required.

## Branch

- **`feat/087-crime-run`** on `tyronmiller55-spec/SpidyVR`
- Packaged / CI tip: **`12367231e043a1b90ae0a65825773dbe2b159573`** (`1236723`)
- Existing branches were not rewritten: `feat/087-beta3`, `feat/crime-run` on the fork, and `feat/crime-run` on SpidyVR-CrimeRun are still at `1bb2aa6`, `343ad4c`, and `7966c68` respectively.
- No PR was opened. CURRENT on SpidyVR-NativeAnimation was not updated. The builds repo was not touched. CI was `publish=false` (tag, GitHub release, and Discord steps were skipped).

## Commits (`1bb2aa6..1236723`)

All four were cherry-picked from SpidyVR-CrimeRun with original authorship (`tyronmiller55-spec`) and original dates. Each new tree is byte-identical to its source commit.

| New sha | Subject | Source commit |
| --- | --- | --- |
| `3a83b3a` | Crime Run rules: a run from 20,000 points with timed crimes on a ring or at real fights, stops, misses, six ranks, a best score, pillar-of-light beacons and a score HUD (core and tests) | `9cb99c3` (core + tests + CMake) |
| `d5c3617` | Crime Run mode (off by default): start a run from the phone anywhere in the open world, bright pillars over each crime, real street fights counted as crimes, a score HUD, haptics and a saved best, with a launcher card | `6a09505` (glue) |
| `e37ddb3` | Docs: Crime Run - how to play, ranks, controls, settings, what the current hooks can and cannot do, and what to test | `343ad4c` (CRIME_RUN.md, CHANGELOG, README) |
| `1236723` | Docs: Crime Run handoff - how to port the mode into another Native Animation tree, the files and hooks it uses, and how to hand the port back | `7966c68` (HANDOFF_CRIME_RUN.md) |

## Conflicts and resolutions

**None.** Every cherry-pick applied cleanly. The tree at `1236723` matches `7966c68` on SpidyVR-CrimeRun.

### Settings revision

- Tip and Crime Run both have `movementDefaultsRevision = 23` (beta3g's rope-weight rescale). Crime Run did **not** bump it, and this port did not either.
- That matches the Crime Run handoff (§1.4, §4.3): the mode is off by default, missing `crime_*` keys take struct defaults, and an older `spidy_vr.ini` without the keys behaves like the previous build. No `if (seen < N)` block is required for Crime Run itself.
- Owner's usual rule is "new features on by default, with a migration revision." Crime Run is an intentional exception (a mode, off by default, off == previous build). No conflict to resolve on this tip. If later on-by-default features land on the same line, they should increment past 23 in their own commit, not in these Crime Run commits.

### What did not change in the port

- Phone app index is still `phoneCrimeApp = 4` (fifth app; the original four stay at `phoneApps == 4` while the mode is off).
- Launcher card is still on the Kicks & Combat page, group title **Crime Run (prototype)**.
- No APIs were renamed or rebound. No new game addresses or offsets.
- The cherry-picked README paragraph still names the prototype branch `feat/crime-run`. On this work repo the port lives on `feat/087-crime-run`.

## Defaults

Crime Run itself: **`crime_run=0` (off)**. The other 17 keys take the struct defaults from the handoff:

| Key | Default |
| --- | --- |
| `crime_run` | 0 (off) |
| `crime_real_crimes` | 1 |
| `crime_hud` | 1 |
| `crime_start_score` | 20000 |
| `crime_win_stops` | 20 |
| `crime_spawn_s` | 45 |
| `crime_timer_s` | 120 |
| `crime_max_sites` | 2 |
| `crime_radius_min_m` | 150 |
| `crime_radius_max_m` | 400 |
| `crime_stop_points` | 1000 |
| `crime_speed_bonus_pct` | 100 |
| `crime_thug_points` | 200 |
| `crime_miss_penalty` | 2000 |
| `crime_reach_m` | 15 |
| `crime_hold_s` | 3 |
| `crime_best` / `crime_best_stops` | 0 (written by the game) |

Fixed `Config` fields (not ini keys): first spawn 5 s, reach height 25 m, thug radius 30 m, real-crime min 40 m, clear 1.5 s, census fresh 1 s, stale 6 s.

## Local tests

The other bot's box tooling (`/workspace/stubs`, `/workspace/spidy-tools`, `/workspace/zigcc`) was not on this machine. Recreated what was practical:

- Installed CMake 3.31.6 + Ninja. Used pip `ziglang` 0.17.0 as the C++ compiler (`/workspace/zigcc/cxx` wrapper around `zig c++`).
- Configured `/workspace/build-linux` with `SPIDY_BUILD_XR=OFF`, `-O0`. `spidy_core` built on Linux **without stubs** (the core sources do not include Windows headers).
- **`spidy_crime_run_tests`:** `2457/2457 crime run checks passed` (exit 0). The test file has 99 `check()` call sites; loops (ring samples, ranks at every target, etc.) make 2457 invocations. A failed check exits 1. This is the full suite, including the glue blocks (settings round-trip, fifth phone app only while on).
- **`spidy_launcher_tests`:** `44/44 launcher checks passed` (includes the `crime_` tab-only exemption).
- **`spidy_tests` / `core_tests.cpp`:** not built locally. It includes `hud_capture.hpp` → `<dxgiformat.h>`. Stubs (`/workspace/stubs/compat.h`) exist only on the other box, not in-repo. The defaults-text expectation was reviewed in source: the 18 `crime_*=` lines (including `crime_run=0`) sit immediately before `log_level=1` / `movement_defaults=23`.
- Windows-only syntax check of `game_xr.cpp` / `game_swing.cpp` / launcher (`syn.sh`) was skipped for the same missing stubs. CI compiled those sources.

## CI

- URL: https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45267015225
- Workflow: Release, ref `feat/087-crime-run`, **`publish=false`**
- Status: **success** in 5m20s (2026-11-23, about 1:11–1:16 PM Pacific time)
- CTest: **48/48 passed**, including `crime_run`, `core`, and `launcher`. Python protocol tests also passed.
- Publish, tag, and Discord announcement steps were skipped.

## Zip

- Path: `/workspace/animated-hybrid-build/Spidy-0.8.7-na-beta3g-crimerun-win64-1236723.zip`
- Inner folder: `Spidy-0.8.7-player-torso-1236723/` (re-zipped from the CI artifact, not the outer `Spidy-0.8.7-win64.zip` wrapper)
- Size: **18,624,819** bytes
- SHA-256: **`8c0e50bc1dfbd14f5acc40d2e6fd59b4b1d8bc1c53ac0fda7ba6503264ba94d3`**
- Built from commit `1236723`. The packaged `docs/` folder is only third-party licenses; `docs/CRIME_RUN.md` is on the branch, not inside the player zip (same packaging as other NA guides). How to play is in the launcher card (Kicks & Combat → Crime Run (prototype)) and at https://github.com/tyronmiller55-spec/SpidyVR/blob/feat/087-crime-run/docs/CRIME_RUN.md

## Headset checklist (condensed from the Crime Run handoff §5.2)

This is a prototype and has not been played in a headset yet. Off must leave beta3g unchanged.

1. **Off:** the phone has its four apps; nothing else differs from beta3g.
2. **On** (`crime_run=1` or the Crime Run card): the phone's fifth app **CRIME RUN** (siren) reads `START A RUN HERE`. A starts a run, the phone goes away, the HUD shows `CRIME RUN  20,000  STOPS 0/20  ROOKIE` and `NEXT CRIME IN 0:05`.
3. After 5 s a pillar appears 150–400 m away, with a label; the HUD arrow points at it as you turn; controllers tick.
4. Swing there; the ring appears near it; stand in it 3 s: `CRIME STOPPED +N`, a sound, a strong buzz, `STOPS 1/20`.
5. Let one run out: `CRIME MISSED -2000`, the score drops.
6. Near a street crime (thugs fighting): a red/blue pillar on them; take them all down: stopped with thug points. Webbed-up thugs count as down.
7. Quit: A, A on the app. To see win/lose quickly, set `crime_win_stops` to 2 or the start score to 2000; the best shows on the app and in the launcher after the session.
8. Diag log (`game-vr-*-diag.log`): `crime run:` lines for start (with the census source), every spawn (beacon or real), reached, stopped, missed, rank, and the summary line at the end.

Known limits (do not paper over with an unverified call): no enemy/crime spawning, no game crime objects or XP, no native map markers, no captured siren sound, far beacons may sit on a roof until collision streams in.

## Decisions (none blocking)

- No settings-revision bump was needed; say if you want one anyway for bookkeeping.
- README still mentions branch `feat/crime-run` (faithful cherry-pick). Can be pointed at `feat/087-crime-run` in a small docs commit if you want.
- Not published as CURRENT, and nothing was sent to Ilya.
