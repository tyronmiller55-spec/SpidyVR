# Handoff: SpidyVR Native Animation (0.8.7-na-beta3 line)

Written Nov 22, 2026 (Pacific time) at build beta3g, so another SpideyVR bot can pick this project up cold. Read this,
then [NA_BETA3.md](NA_BETA3.md) (every beta3 build's settings, logs and checklist), [CHANGELOG.md](CHANGELOG.md) and
[NA_HELP.md](NA_HELP.md).

## 1. The project

- **What:** the Native Animation fork of SpidyVR (GPL-3.0), Ilya Mezerowsky's fan VR mod for Marvel's Spider-Man
  Remastered (PC). Insomniac's own animations play the body on the ground; SpidyVR owns the air. Keep the Spidy
  launcher's identity and the credits to Ilya and the upstream contributors.
- **Repo (work):** `tyronmiller55-spec/SpidyVR`. Branch in use: `feat/087-polish` (from `feat/087-crime-run`, itself
  from `feat/087-beta3`). Push only to the fork; never to IlyaMez or `SpidyVR-builds`.
- **Remote `builds`** (`tyronmiller55-spec/SpidyVR-builds`): **never push to it.**
- **Release repo:** `tyronmiller55-spec/SpidyVR-NativeAnimation`. Its **CURRENT** release is still
  **0.8.7-na-beta2 at 1418992** (Nov 21). Update CURRENT **only when the owner asks**; the beta3 builds are test zips.
- **Upstream:** `IlyaMez/SpidyVR`. **No PR to IlyaMez unless the owner asks.** Fork PRs #1 ("Keep Native Animation
  preferences with 0.8.5 presets and pages") and #2 ("Diagnostic: distinguish web-ball dispatch from projectile
  observations") on origin are open: leave them alone.
- `gh` on the box is signed in as `tyronmiller55-spec`. Cursor cloud agents are not on this plan: work locally.

## 2. Local paths (the box)

| Path | What |
| --- | --- |
| `/workspace/SpidyVR-NativeRun` | the checkout (branch `feat/087-beta3`) |
| `/workspace/build-linux` | CMake build of `spidy_core` (headless core library) and `core_tests_linux` |
| `/workspace/zigcc/cxx` | the C++ compiler (zig) for tests and the Windows syntax check |
| `/workspace/stubs` | Windows stubs for Linux test builds (`compat.h`) |
| `/workspace/spidy-tools/` | `syn.sh` (Windows-target syntax check), `mkmini.py` (build a subset of core tests), `cf.yaml` (clang-format style), `hunks.py` (apply chosen `-U0` hunks, for splitting commits). Copies also at `/workspace/syn.sh`, `/tmp/*` (may vanish). |
| `/workspace/animated-hybrid-build/` | packaged zips `Spidy-0.8.7-na-<build>-win64-<sha7>.zip` |
| `/workspace/playtest-na-beta3b/` | beta3b logs + `ANALYSIS.md`, `KICKS_PLAN.md`, `WEB_POLISH_PLAN.md` |
| `/workspace/playtest-na-beta3c/`, `/workspace/playtest-na-beta3d-or-later/` (beta3e log), `/workspace/playtest-na-beta3f/` | the owner's logs per build |
| `/workspace/beta3g-wip/` | beta3g's final-tree snapshot and the stage script used to split its commits |

## 3. Build, test, CI, package

```sh
cd /workspace/SpidyVR-NativeRun
# core library + core tests (run-core.sh stops silently: run the steps by hand; a full run takes about 4 min)
cmake --build /workspace/build-linux --target spidy_core
/workspace/zigcc/cxx -std=c++20 -O0 -Iinclude -I/workspace/stubs -include /workspace/stubs/compat.h \
  tests/core_tests.cpp /workspace/build-linux/libspidy_core.a -o /workspace/build-linux/core_tests_linux -lpthread
/workspace/build-linux/core_tests_linux > /workspace/core-full.log 2>&1   # "N/N tests passed"
# a subset: python3 /workspace/spidy-tools/mkmini.py "<test name prefix>" ... -> /tmp/mini.cpp, compile the same way
# air kicks: same compile of tests/air_kick_tests.cpp; beta polish + NA_HELP.md:
#   compile tests/beta_polish_tests.cpp with -DSPIDY_SOURCE_DIR=\"$PWD\", run with --write-help to regenerate docs/NA_HELP.md
# Windows-only sources (game_xr, game_swing, launcher): sh /workspace/spidy-tools/syn.sh src/game_xr.cpp  (no output = OK)
```

- Use **-O0** for test builds (-O1 pulls in ubsan symbols that do not link). Always rebuild `spidy_core` before
  linking tests; a stale library gives garbage (a wrong row table, "a valid place" failures).
- clang-format (`git clang-format --binary /home/box/.local/bin/clang-format --style=file:/workspace/spidy-tools/cf.yaml`)
  only on `src/*.cpp` and settings headers - **never** on `tests/core_tests.cpp`, `include/spidy/na_help.hpp` or
  `apps/launcher/ui.cpp` (it reflows huge blocks). CI does not check formatting.
- **CI:** `git push -q origin feat/087-beta3`, then `gh workflow run Release --ref feat/087-beta3 -f publish=false`,
  `gh run watch <id> --exit-status --interval 30`. **Always publish=false.**
- **Package:** `gh run download <id> -R tyronmiller55-spec/SpidyVR -D /tmp/art<build>`; inside is
  `Spidy-0.8.7-win64.zip/Spidy-0.8.7-player-torso-<sha7>/`; re-zip that folder with python
  `shutil.make_archive(base, 'zip', '.', 'Spidy-0.8.7-player-torso-<sha7>')` to
  `/workspace/animated-hybrid-build/Spidy-0.8.7-na-<build>-win64-<sha7>.zip`, and report size and sha256.
- **Zips go to the owner as chat attachments.**

## 4. The owner's rules and design principles

- Never modify the game folder; reversible hooks only.
- Never invent offsets or addresses; cite [REFERENCE.md](REFERENCE.md) and [VALIDATION.md](VALIDATION.md).
- One feature per commit, player-facing commit subjects; a docs commit per build.
- Ask before play-feel changes (the owner has been approving them freely: say what changes and that it is a feel change).
- New features: their own toggle, **on by default**; **off must equal the previous build**. Every new default goes
  through a settings migration revision (`movementDefaultsRevision`, now **23**): a switch comes on once, a value
  chosen after the revision stays. Pattern: field before `// BETA3 SETTINGS END` (vr_settings.hpp), clamp +
  catalog entry (vr_settings.cpp), `if (seen < N)` block, launcher card (`apps/launcher/ui.cpp`), the key in
  `tests/launcher_tests.cpp`'s exemption list, core_tests' defaults text / off values / migrated / kept checks, NA
  help lines (at most 80 chars, titles upper case) and regenerated NA_HELP.md.
- **The headset heading is the only direction authority; never write view yaw.**
- **The air belongs to SpidyVR.**
- **Leave the wall jump (and its sound) as it is.**
- No IlyaMez PR unless asked. Push only to origin. CI publish=false. CURRENT only when asked.
- **Discord posts: draft them for the owner, never post.**

## 5. The owner's testing flow and logs

The owner tests each build while asking for the next ("work on these while I test this build"), then uploads logs
to Google Drive folder **`1nSJCrB69eluLQ5opnSrFoiua5KSBwWkp`** (`game-vr-<date>-<time>-{diag,game,console}.log` and
`-summary.json`). List: `curl "https://drive.google.com/embeddedfolderview?id=1nSJCrB69eluLQ5opnSrFoiua5KSBwWkp&cb=<ts>"`;
download: `https://drive.usercontent.google.com/download?id=<id>&export=download&confirm=t`. The diag log's first
line names the build commit. Report findings with numbers (counts, medians, p90s).

## 6. Builds (0.8.7-na-beta3 line)

| Build | Commit | CI run (publish=false) | Core tests | Zip (`/workspace/animated-hybrid-build/`) | sha256 (first 16) |
| --- | --- | --- | --- | --- | --- |
| beta3 | df2dd9a | 44941175026 | pass | Spidy-0.8.7-na-beta3-win64-df2dd9a.zip | 4bee9f57222ceeea |
| beta3b | 1bd3967 | 44943770713 | pass | Spidy-0.8.7-na-beta3b-win64-1bd3967.zip | c1cb8d7b893ebb69 |
| beta3c | 0a6b65a | 44951004305 | pass | Spidy-0.8.7-na-beta3c-win64-0a6b65a.zip | 72da8419672e4c61 |
| beta3d | b306834 | 44986162624 | pass | Spidy-0.8.7-na-beta3d-win64-b306834.zip | b67e811a9654fe5b |
| beta3e | 4272657 | 45033337280 | pass | Spidy-0.8.7-na-beta3e-win64-4272657.zip | 507b391dc487183e |
| beta3f | 942f3ff | 45047830952 | 988/988 | Spidy-0.8.7-na-beta3f-win64-942f3ff.zip (18,580,839 B) | c8c3bfbcd0a37b8e |
| beta3g | c488634 | 45081781847 | 994/994 | Spidy-0.8.7-na-beta3g-win64-c488634.zip (18,596,713 B) | 93eafa8e229da238 |
| crime-run port | 1236723 (docs 70f3f8a) | 45267015225 | 48/48 CI; 2457 crime-run | Spidy-0.8.7-na-beta3g-crimerun-win64-1236723.zip (18,624,819 B) | 8c0e50bc1dfbd14f |
| beta3h-polish | (this branch `feat/087-polish`) | CI after the docs commit | see notes | Spidy-0.8.7-na-beta3h-polish-win64-<sha7>.zip | in polish-pass-notes.md |

beta3g commits: 4182819 web rope (steady steps, taut under load, wrap keep, weight rescale); d67e992 kick chamber,
lean, momentum; 6c6600c kick target ring, ground lunge, rebound, rope assist, ground kick speed; c488634 docs.

## 7. System map

- **Handoff / head gap (beta3):** HANDOFF HEAD FIRST (the SpidyVR body drawn at most 2 frames while the head is read),
  LANDING VIEW SNAP (15 cm, 120 ms), HEAD SEAT PULL (past 5 cm at 50 cm/s; the 20 cm reseat stays a backstop);
  `takeover_view_ease` already places the body at the view. Log `handoff head gap:`. Docs: JUMP_HANDOFF.md,
  AIR_TAKEOVER.md, OWNERSHIP.md.
- **Showtime triggers (beta3b/d):** per-kind toggles (finishers, strikes, dodges, punches) with every-Nth and
  cooldown. The game's web strike is `HeroStateWebRush` (dodge kind 8 via `SpidyDodgeLive`); SpidyVR punches are
  `game_punch::Strikes::opened` (AUTO: PUNCHES, off). Finisher fix: `showtime::OkInput` + `showtimePlace` also in the
  first-person scene's eye path. Docs: SHOWTIME.md.
- **Double-tap A flip (beta3b):** in the air double-tap A turns him upside down over 1 s; hold the second tap to stay;
  a single A still starts the upright at once, a second tap within 300 ms redirects it. CONTROLLED_FLIPS.md.
- **Rope physics:** drawn swing line `swing_line::Line` (include/spidy/swing_line.hpp) on `tether::Rope`
  (tether.hpp: 24 particles, XPBD, long-range attachment). Its pull goes to the swing through **`SpidyRopePull`**
  (game_xr ~8490 -> `Swing::ropePull`); REEL OUT through **`SpidyReelOut`** (reel_out.hpp). The swing's own rope is
  separate (`Swing`, `SwingConfig::taut`, `WebState{length, tension}`); wraps are `Swing::wrap/unwrap` (swing.cpp,
  max 6 bends), XR gets them as `SpidySwingBends`. beta3g: `stableConfig` (fixed 1/600 s, 8 passes, carry .9,
  30 m/s relative limit), TAUT UNDER LOAD (smoothstep 2..6 m/s^2 of `web.tension`), `Rope::reanchor` on a wrap,
  massPerMetre .0255, pullCap 6. Docs: WEB-LINE.md, NA_BETA3.md beta3c..beta3g.
- **Grip to stick (beta3e):** SpidyVR's own walls slide with friction (wall_slide.hpp) until a palm grip sticks; the
  game's own crawl and wall run still grab (no validated hook). Point jump cancels wall grabs. WALL_CRAWLING.md, GRIP.md.
- **Floor-crawl roof check (beta3f):** the game's mover flag 0x1 refuses the point jump only with a duct roof seen
  (`FloorCrawlRoof`, `floorCrawlRefuses`, game_tracking.hpp); the sidestep's floor-flag refusal unchanged. POINT_JUMP.md.
- **Kicks:** legs chord Y + X then a right-stick flick (air_kick.hpp `Gestures`, `FlickWatch`). Air kick `Kick`
  (air_kick.hpp) runs in the swing module (game_swing.cpp "AIR KICKS" block) from `SpidyAirKick` (`AirKickAsk` v3)
  and reports `SpidyAirKickLive` (`AirKickLive` v5); ground kicks are the same class standing (ground_kick.hpp);
  the swing kick K1 is swing_kick.hpp (`solver.kickAssist`). The legs are leg_layers' kick layer (game_xr ~12230:
  `shapedLegPose`, `momentumPose`, `kickLean` on the torso channel `native_body::setTorsoControl`). Blows are the web
  strike's (`game_punch::kickHit`). beta3f: KICK LUNGE (`Swing::kickLunge`), KICK FEEL. beta3g: ring
  (`air_kick::preview`, drawn as an AimMark::target in game_xr's aim markers), ground lunge (sidestepRun burst),
  rebound (`Swing::kickImpulse`), rope assist, ground speed. Docs: AIR_KICKS.md, GROUND_KICKS.md, SWING_KICK.md,
  `/workspace/playtest-na-beta3b/KICKS_PLAN.md`.

## 8. Known limits (need new hooks / captures)

- The swing's obstruction cut (no edge to bend round, or 6 bends) is a real let-go; the game's final web dissolve;
  the connected (stick-click) web line is drawn taut by the game; the game rope takes at most 6 points of ours.
- The game's own wall crawl / wall run grab: no validated hook to stop them.
- Kicks: no game kick animations or kick sound ids captured (hero attack sound `attack_sound` is 0 until a capture
  session); no pelvis/hip channel (the spine shows the turn); the body command layout (v16, 304 bytes) is official -
  a new field is a protocol change.
- 0xC0000409 crash at game close (known, every session since beta3d).

## 9. Open issues

- A single A from fully inverted may pitch him forward instead of rolling (double-tap A flip).
- beta3f log: 9 swing kicks refused "not swinging" (flicks while airborne off a web): consider routing them to the
  air kick. One air kick with 4 enemies in the fight and the nearest at 1.1 m had no target (he was behind: the
  front cone is 75 degrees).
- Kicks barely tried in fights yet (24 of 26 beta3f kicks had no enemy in a fight); the owner wants **lots of kick
  animation work** (t100u) - beta3g's chamber/lean/momentum is the first pass.

## 10. Backlog, in priority order

1. Read the owner's beta3g logs: `swing line: ... kept through a wrap` counts, let-go `most pull` vs the new 6 cap,
   slack medians, any jitter reports; kick starts with targets, ground lunges, rebounds, rope assists.
2. Kick animation pass 2 (from his beta3g feedback): per-gesture timing curves, a proper roundhouse hip turn, a
   landing-to-kick blend, arms counter-swing, maybe a kick-specific body haptic.
3. Kicks: route airborne "not swinging" swing-kick flicks to the air kick; a wider front cone when the punches' lock
   is behind.
4. Legs mode without the Y + X chord (approved "if cheap"; needs the right stick's input map reworked).
5. Rope: optional coil/curl shaping on a slack line (the owner: "weight and coil and curl").
6. Ask the owner before updating CURRENT to a beta3 build.

**Standing do-not-start items:** R2 (enemy limb zones work) until after the C0 skeleton capture; trigger-squeeze
cancel; tumble-roll; any upstream (IlyaMez) PR; touching fork PRs #1 and #2; updating CURRENT - each only when the
owner asks.
