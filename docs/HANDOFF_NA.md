# SpidyVR Native Animation handoff (as of 2026-11-26, Pacific time)

Written the morning of Thursday, November 26, 2026 (America/Los_Angeles), so a fresh assistant on another account can pick this up with no prior history. This supersedes the November 22 beta3g `docs/HANDOFF_NA.md`. Also read [NA_BETA3.md](NA_BETA3.md), [CRIME_RUN.md](CRIME_RUN.md), [HANDOFF_CRIME_RUN.md](HANDOFF_CRIME_RUN.md), [REFERENCE.md](REFERENCE.md), [VALIDATION.md](VALIDATION.md), [CHANGELOG.md](CHANGELOG.md), and [NA_HELP.md](NA_HELP.md).

**Urgent, open work:** under Meta Quest Link **Oculus 1.210.0**, the game dies about three seconds into VR with a stack overflow (`0xC00000FD`) in NVIDIA `nvwgf2umx.dll`. The last shipped attempt is **0.8.7-na-beta3l** (`3baaecf`). The owner reported on Nov 26 (t63u) that **beta3l still breaks**. **No beta3l logs or dumps were attached with that message.** Do not ship another guessed fix until you have a beta3l `spidy-vr-start.log` and, if it crashed, a `.mdmp`.

---

## 1. Project overview

- **What:** the Native Animation (NA) line of SpidyVR (GPL-3.0), Ilya Mezerowsky's fan VR mod for Marvel's Spider-Man Remastered (PC). Insomniac's own animations drive the body on the ground; SpidyVR owns the air. Keep the Spidy launcher's identity and the credits to Ilya and upstream contributors.
- **Tester:** the owner ("SpidyVR Modder") on a **Meta Quest 3S** over **Meta Quest Link**.
- **Work repo:** [tyronmiller55-spec/SpidyVR](https://github.com/tyronmiller55-spec/SpidyVR). Current work branch: **`feat/087-next`**, tip **`3baaecff595b30116e2c44243d7e1328de776363`** (`3baaecf`).
- **Remotes on the work box:** `fork` = `tyronmiller55-spec/SpidyVR` (push here only); `origin` = `IlyaMez/SpidyVR` (**fetch only, never push, never open a PR unless the owner explicitly asks**); `crimerun` = `tyronmiller55-spec/SpidyVR-CrimeRun` (reference only). There is no `SpidyVR-builds` remote in this checkout; **never add or push to `tyronmiller55-spec/SpidyVR-builds`**.
- **Release repo:** [tyronmiller55-spec/SpidyVR-NativeAnimation](https://github.com/tyronmiller55-spec/SpidyVR-NativeAnimation). **CURRENT** is still **0.8.7-na-beta2** at **`1418992a8fda6a2f6044c75abafb52940814000e`** (`1418992`, Nov 21). Tag `CURRENT-NativeAnimationv0.8.7-beta2`. Update CURRENT **only when the owner asks**. All beta3 builds are test zips sent as chat attachments.
- **Upstream official:** Ilya released **0.8.8** on Nov 25 at 12:49 AM Pacific time, commit **`cf0b2791bc335a49d17f999c999d61a6a11cf2c2`** (`cf0b279`, tag `v0.8.8`): first-time VR setup and a comfort vignette. It is **not** a known fix for the 1.210 crash and was **not** ported. The launcher's **Update** button is only a notice that 0.8.8 exists (our `kVersion` is still `0.8.7-na-beta3`). **Tell the owner not to press Update** — it would replace the NA folder with official 0.8.8.
- **`gh` on the box** is signed in as `tyronmiller55-spec`. The GitHub login lacks the `workflow` scope, so **never edit workflow files**.

---

## 2. Standing rules

Follow these unless the owner explicitly overrides one.

- **No additions without the owner's signoff (Nov 26, t141u).** Do not add any feature, visual, overlay, setting or behavior the owner did not explicitly ask for. Propose it and get the owner's signoff first. Example: the owner said the webs were hard to see, and a bot added THICK SWING LINE, a second bright white line along the swing web that he never asked for and found garish. It was removed in beta3n. See also [AGENTS.md](../AGENTS.md).
- Never push to upstream IlyaMez or open PRs there without an explicit ask.
- Never send Discord or other messages as the user. Draft them only.
- Never force-push or rewrite existing branches.
- Never change workflow files (the GitHub login lacks the `workflow` scope).
- Always run CI with `publish=false`.
- Push only to the fork (`tyronmiller55-spec/SpidyVR`). Never push to `SpidyVR-builds`.
- Update CURRENT only when the owner asks.
- Leave fork PRs **#1** ("Keep Native Animation preferences with 0.8.5 presets and pages", branch `native-animation-085-compat`) and **#2** ("Diagnostic: distinguish web-ball dispatch from projectile observations", branch `diagnostic/web-ball-outcomes`) alone.
- Ask before play-feel changes. Say what would change and that it is a feel change; the owner has been approving those freely when told.
- New features get their own toggle, **ON by default**, and **OFF must equal the previous build**. Every new default goes through a settings migration revision (`movementDefaultsRevision`). **Crime Run is the documented exception: it ships OFF by default**, per its own handoff. Ask the owner if they want that changed.
- The headset heading is the only direction authority; **never write view yaw**.
- The air belongs to SpidyVR.
- Leave the wall jump and its sound as they are.
- Never invent offsets or addresses; cite `REFERENCE.md` and `VALIDATION.md`.
- Never modify the game folder; reversible hooks only.
- One feature per commit, with player-facing subjects, and a docs commit per build.
- Zips go to the owner as chat attachments.
- Do-not-start until the owner asks: **R2** (enemy limb zones), **trigger-squeeze cancel**, **tumble-roll**, any **IlyaMez PR**, touching fork PRs #1 and #2, and updating CURRENT.
- Tell the user **not to press the launcher's Update button**.

---

## 3. Branches and tips

All of these are on `tyronmiller55-spec/SpidyVR`. Do not rewrite them.

| Branch | Tip | What it is |
| --- | --- | --- |
| **`feat/087-next`** (work branch) | `3baaecff595b30116e2c44243d7e1328de776363` (`3baaecf`) | Crime Run + polish + beta3i features + start-log diagnostics + GPU-start workarounds through beta3l |
| `feat/087-polish` | `5c1c74949644926864c44db51b16420ca9acff64` (`5c1c749`) | beta3h-polish. Never headset-tested before the Nov 25 crash wave |
| `feat/087-crime-run` | `70f3f8a3f7b0e3c6643e8a4c5c4f90df5ac71502` (`70f3f8a`) | Crime Run port onto beta3g. Packaged zip is from `1236723` (docs tip is one commit later) |
| `feat/087-beta3` | `1bb2aa612e36b35e24119c39f75e594d832df96a` (`1bb2aa6`) | Official 0.8.7 + NA beta3 through **beta3g** (`c488634`) plus the Nov 22 handoff docs |
| `community-build` | `fe981278c1ce52dfcb0e1ca3cf4532d30f707496` (`fe98127`) | Player-facing README. The Nov 24 docs fix names CURRENT as 0.8.7-na-beta2, not 0.8.6 |

**Last confirmed good headset play:** **beta3g** `c488634d48ca8e7b55cafb789298f5106591e758` on **Oculus 1.208.0** (36 extensions), before the morning of Nov 25.

### 3.1 Commits on `feat/087-crime-run` (`1bb2aa6..70f3f8a`)

| Sha | Subject |
| --- | --- |
| `3a83b3a` | Crime Run rules (core and tests) |
| `d5c3617` | Crime Run mode (off by default), phone app, pillars, HUD, launcher card |
| `e37ddb3` | Docs: how to play Crime Run |
| `1236723` | Docs: Crime Run port handoff |
| `70f3f8a` | Docs: port notes, CI, test zip |

### 3.2 Commits on `feat/087-polish` (`70f3f8a..5c1c749`)

| Sha | Subject |
| --- | --- |
| `d484a78` | A failed save of the SPIDY VR tab is tried again, not treated as saved |
| `5fdd221` | Crime Run's quit confirm no longer lapses during a pause or load |
| `bf041b3` | Crime Run does not ask the swing for enemies or ground while the game is paused |
| `85c8fc5` | The overlay keeps its aim marks and translucent vertices from frame to frame |
| `1c38d36` | Crime Run's score HUD is drawn without a heap allocation each eye |
| `a377618` | Tests: Crime Run stays off and a later 0 stays, matching the previous build |
| `5c1c749` | Docs: 0.8.7-na-beta3h-polish, missing beta3 banners, and Crime Run help |

### 3.3 Commits on `feat/087-next` after polish (`5c1c749..3baaecf`)

| Sha | Subject |
| --- | --- |
| `9198c87` | Closing the game window is no longer reported as a 0xC0000409 crash |
| `016f983` | A swing-kick flick that starts an air kick no longer looks like a missed kick in the log |
| `c302bf1` | A hand on its web closes on the line while the animation has the body |
| `095c6f5` | A punching hand closes into a fist while the animation has the body |
| `965ab55` | A held web leaves from inside the closed hand |
| `ba3ef8a` | A thicker swing line is easier to see in the headset |
| `02bd1e2` | Docs: 0.8.7-na-beta3i, web in hand, thick swing line, grip and fists |
| plus catalog/tab-layout commits `1181e45`, `3f27fe3`, `3c7bb41` | WEB GRIP / line rows on the in-game tab |
| `ef0696d` | VR start leaves a flushed step log if the headset session crashes |
| `e512fb7` | Docs: 0.8.7-na-beta3j-diag |
| `6fa2418` | The start log looks up its folder from a unique function address (MSVC fix) |
| `9967f31` | The GPU bridge starts before the headset goes focused (beta3k) |
| `990d384` | Docs: 0.8.7-na-beta3k |
| `c8e4fda` | The GPU bridge starts before the OpenXR session begins (beta3l) |
| `3baaecf` | Docs: 0.8.7-na-beta3l |
| `f141fab` | The VR thread gets a 16 MB stack (beta3m, the 0xC00000FD fix) |
| `9790ed0` | The eye-frame history lives on the heap (beta3m) |
| `41c8b27` | The VR start log names the VR thread's stack size (beta3m) |
| `19bebec` | Docs: 0.8.7-na-beta3m |
| `bbf763d` | The extra white line along the swing web is gone (beta3n) |
| `912988a` | Docs: no feature, visual or setting without the owner's signoff |
| `e11f17b` | Wall running is back: the stick at the wall runs a slide (beta3n) |
| `52a3c72` | A grip fling throws you off a wall in any direction (beta3n) |
| `c544f24` | Sticky palms (beta3n) |
| `51915aa` | Docs: 0.8.7-na-beta3n |
| `6250345` | Grip holds the handstand (beta3n) |
| `bd2065b` | Launcher: a stalled picture no longer blames Wi-Fi |
| `61f3c80` | Docs: beta3n's handstand and launcher text |
| `46d3589` | Wall slide pose (beta3o) |
| `29996b0` | Grip locks the wall jump; point jumps go through the grip (beta3o) |
| `bb9796e` | Web in hand: the held line starts in the fist's channel (beta3o) |
| `193ea31` | Rope grip (beta3o) |
| `7a68938` | Docs: 0.8.7-na-beta3o |
| `2a8dd95` | Smoothing clock fix (beta3p) |
| `186038c` | Level keeps him with you (beta3p) |
| `bbbac4e` | HEAD LEVEL is the default (beta3p) |
| (next) | Docs: 0.8.7-na-beta3p |

---

## 4. Settings revision and toggles

`movementDefaultsRevision` is **27** (set at beta3p). It was 26 at beta3o, 25 at beta3n, 24 at beta3i, 23 at beta3g / Crime Run / polish.

beta3p (revision 27): `level_head_lock` — LEVEL KEEPS HIM WITH YOU (`level_head_lock.hpp`; game_xr's lockWanted /
fullLockGate / levelTurnGate, the HYBRID steering and HEADSET FIRST basis, `game_fp::levelHeadLock` and frameLevel;
launcher Native Animation group), **ON by default**, OFF = beta3o's LEVEL. `animated_head` defaults to 1 (LEVEL);
revision 27 turns a saved 2 (FULL) into 1 once. The smoothing clock fix (stereo_probe resolveScene: QPC, one step a
frame; game_fp's latch under an SRW lock) has no setting.

beta3o's switches, **ON by default** (revision 26 turns them on once). OFF plays like beta3n:

- `wall_slide_pose` — WALL SLIDE POSE (`leg_layers::wallSlidePose`, `Layer::wallSlide`; launcher Walls group). Visual. The pose is an approximation of the owner's picture (unreadable to the build): tune the one table.
- `wall_grip_jump_lock` — GRIP LOCKS THE WALL JUMP (`grip_jump.hpp`, game_xr before the controller frame; launcher Walls group). Point jumps go through (their chord is read first).
- `web_rope_grip` — ROPE GRIP (`HandPoseInput::ropeGrip`, `Targets::Hand::rope`, `native_body::setRopeGrip`; launcher Combat group). Fingers only.
- WEB IN HAND's held line now starts at `body::solvedFistChannel` (published as `OverlayTracking::holdLocal`, carried in `NativeEyeFrame::fistHold`), else the grip point; `webHoldPoint` (the knuckles) is no longer used for held lines. Option C (wrist/elbow along the rope) is held by the owner.

beta3n's switches, **ON by default** (revision 25 turns them on once), in the launcher's **Walls** group (Legs
page). OFF plays like beta3m. SpidyVR's own walls only (`include/spidy/wall_feel.hpp`):

- `wall_run_push` / `wall_run_blend_ms` (250) — WALL RUN PUSH. On a slide, the stick toward the wall blends into the stick's wall run. Root cause of the beta3e break: GRIP TO STICK's slide returned before the run in `Swing::wallMotion`.
- `wall_fling_any_way` — a grip fling off a wall leaves along the whole fling with 0.3 m/s out of the wall (was 2 m/s).
- `sticky_palms` / `sticky_palms_pct` (100) — open palms on the wall drag a slide, or stroke the body the other way of their sweep (1.3x).
- `handstand_grip_hold` — GRIP HOLDS THE HANDSTAND (`handstand.hpp` Params::gripHold, launcher Handstand group). A grip is the catch's intent (no arms-over-head test, tip-in reach 2.0 m) and locks the stance while held; release to push, leap or step out.
- The wall pieces reach the swing on `WallStickAsk` version 2 (flags bits 1-3, `runBlendMs`, `stickyPct`); version 1 is still taken.

beta3i's switches, **ON by default**, on the **NA · WEB SHOOTERS** tab and the Movement tab. OFF plays like beta3h / pre-beta3i:

- `animated_web_grip` — WEB GRIP. A hand closes on its attached web while the game's animation drives the body.
- `animated_punch_fists` — PUNCH FISTS. A punching hand balls into a fist (pose measured from the game's own model; no verified clip IDs).
- `web_hold_in_hand` — WEB IN HAND. A held web leaves from the closed fist (~15.5 cm ahead of `webShooter` / `webStart`, `fistAhead` in `game_punch.cpp`). A new shot still fires from the wrist shooter.
- `thick_swing_line` — THICK SWING LINE. **Removed in beta3n** (the owner: unasked, garish). Do not bring it back.

**Crime Run** stays **off by default**. Turn it on under Kicks & Combat; start a run from the fifth app on the phone (the siren). With it off, play should match the parent build.

AIR FISTS is an older setting and was not changed for the empty-fist / wrist-shooter issue.

---

## 5. Build, CI, and packaging

Work tree on the current box: **`/workspace/SpidyVR`** (not the older `/workspace/SpidyVR-NativeRun` path from the Nov 22 handoff; that tree is not the one in use here). Linux test binaries live in `/workspace/build-linux` and `/tmp/spidy-build`. Zig compilers are under `/workspace/zigcc`. CMake may need `-DCMAKE_MAKE_PROGRAM=/tmp/buildvenv/bin/ninja`. `zip` may be missing; package with Python `zipfile`.

```sh
# Always publish=false. Always -R the fork. Never the IlyaMez remote.
git push fork feat/087-next
gh workflow run Release --ref feat/087-next -f publish=false -R tyronmiller55-spec/SpidyVR
gh run list --workflow=Release --branch=feat/087-next -R tyronmiller55-spec/SpidyVR --limit 3
# Download the artifact, then re-zip the inner player folder:
#   Spidy-0.8.7-win64.zip/Spidy-0.8.7-player-torso-<sha7>/
# to /workspace/animated-hybrid-build/Spidy-0.8.7-na-<build>-win64-<sha7>.zip
# Archive root must be Spidy-0.8.7-player-torso-<sha7>/ (includes Spidy Launcher.exe).
```

CMake label: `SPIDY_VERSION_LABEL` in `CMakeLists.txt` (currently `${PROJECT_VERSION}-na-beta3l`).

One feature per commit, then a docs commit for the build (usually a section in `docs/NA_BETA3.md`). Attach the zip in chat. Do not tag, publish, or announce on Discord (`publish=false` skips those steps).

---

## 6. Test zips (not CURRENT)

All under `/workspace/animated-hybrid-build/`. Send as chat attachments. Sizes are bytes.

| Zip | Commit | Bytes | SHA-256 | CI (`publish=false`) |
| --- | --- | --- | --- | --- |
| `Spidy-0.8.7-na-beta3g-crimerun-win64-1236723.zip` | `12367231e043a1b90ae0a65825773dbe2b159573` | 18,624,819 | `8c0e50bc1dfbd14f5acc40d2e6fd59b4b1d8bc1c53ac0fda7ba6503264ba94d3` | [45267015225](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45267015225) |
| `Spidy-0.8.7-na-beta3h-polish-win64-5c1c749.zip` | `5c1c74949644926864c44db51b16420ca9acff64` | 18,623,413 | `576c6d805bc8238326f2fe177874fb44b903d2fe8a8be7d79b0c2e4c6f7027f1` | [45363229372](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45363229372) |
| `Spidy-0.8.7-na-beta3i-win64-3c7bb41.zip` | `3c7bb41a359420c56a48c45ce0410df9bc9a23f7` | 18,568,121 | `7a060079b7f69409cc6b010bee83e5a695d8575099103427accfdb52799d4e2a` | [45667725953](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45667725953) |
| `Spidy-0.8.7-na-beta3j-diag-win64-6fa2418.zip` | `6fa24188a08dcb001a1ba5b4908356e3cdf21cf0` | 18,571,153 | `56b42969e7d6b34eaa0eb65376df6a518e6f86375a701882491879da6f0ddc17` | [45692331969](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45692331969) (earlier run [45691545896](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45691545896) failed MSVC: `durableBegin` overload; fixed in `6fa2418`) |
| `Spidy-0.8.7-na-beta3k-win64-990d384.zip` | `990d38424ced73e7cbb8f9fbe8f052a4f1c548aa` | 18,630,515 | `8739baddfcd852e494bb30e3b89cfccd0bd57c22a3d5a43b8945d8a91b544b0f` | [45702108786](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45702108786) |
| `Spidy-0.8.7-na-beta3l-win64-3baaecf.zip` | `3baaecff595b30116e2c44243d7e1328de776363` | 18,630,535 | `ed932f2c5cbc899e2760504f53366facbe12dbc96edbc4d8d657dcf43cd8c554` | [45716813398](https://github.com/tyronmiller55-spec/SpidyVR/actions/runs/45716813398) |

Install by extracting to a **new folder**. The player folder name is `Spidy-0.8.7-player-torso-<sha7>/`. Confirm the console line `build Spidy 0.8.7-na-beta3l (commit 3baaecf…)` and that path, not an older `…-990d384` folder.

Packaging notes also live at `/workspace/beta3i-notes.md` through `/workspace/beta3l-notes.md` and `/workspace/crime-run-port-notes.md`.

---

## 7. Key files and architecture

C++20 OpenXR / D3D12 mod, DLL injection, MinHook hooks. Do not invent addresses.

| Area | Where |
| --- | --- |
| VR session / frame loop / GPU-bridge start | `src/game_xr.cpp` (`run()`, `prepare()`), `src/xr_session.cpp` |
| Native GPU bridge (eye copies, Present hooks, throwaway 64px DXGI swapchain, command-list hooks) | `src/native_eye_gpu.cpp`, `include/spidy/native_eye_gpu.hpp` |
| Flushed VR-start log | `include/spidy/diag_log.hpp` (and its `.cpp`). Writes `spidy-vr-start.log` beside the stereo module: `reports\stereo-modules\<hash>\spidy-vr-start.log` |
| Hand poses (VR-owned body). Fist bend 83/95/63 from the game's own fist (`docs/VALIDATION.md`, `probe_game_body.py --fists`). No verified native grab/punch clip IDs | `include/spidy/hand_pose.hpp` |
| Punch / `fistAhead` (~15.5 cm) | `src/game_punch.cpp` |
| Swing rope layout | `include/spidy/swing_line.hpp` |
| Drawing the game's web mesh | `include/spidy/native_webs.hpp` |
| Walls: slide (GRIP TO STICK), wall run push, fling any way, sticky palms | `include/spidy/wall_slide.hpp`, `include/spidy/wall_feel.hpp`, `src/swing.cpp` (`wallMotion`, `wallRun`, `palmInputs`), `tests/wall_feel_tests.cpp` |
| Settings, revision, catalog | `include/spidy/vr_settings.hpp`, `src/vr_settings.cpp` |
| Launcher, exit-code wording, Update banner | `tools/run_game_vr.py` (around `crash_advice`; force-close `0xC0000409` is a non-bug, fixed in `9198c87`) |
| Minidump inspector (candidates, not an unwound stack) | `tools/inspect_minidump.py` |
| Crime Run | `docs/CRIME_RUN.md`, `docs/HANDOFF_CRIME_RUN.md` |

`native_gpu::start()` creates a command allocator, command list, Close, fence, then MinHook on Barrier / Reset / ExecuteCommandLists / EndQuery, then a throwaway swapchain to find DXGI Present/Present1. `wantScreen` stays gated on viewing. `submitCommands` calls `originalExecute` directly (not the hook). Present hooks have a `thread_local presenting` guard. `CreateThread` calls use stack size 0 (Windows default). The `submission` mutex is not recursive.

beta3k started this from `prepare()` after the first `xrBeginFrame` (even on `shouldRender=0`). That overflowed during **CreateCommandAllocator / CreateCommandList / Close / CreateFence**, before hooks were enabled. beta3l moved `native_gpu::start()` to just after `overlay.initialize()`, **before `xrBeginSession` and the first `xrWaitFrame`**. The start log now names each create call. Play is unchanged if the session starts. There is **no playable fallback that skips the GPU bridge** (the headset would get no eye images). A separate D3D12 device cannot copy the game's eye textures.

---

## 8. VR-start crash investigation (Nov 25–26)

All times below are **Pacific time (America/Los_Angeles)** on **Wednesday, November 25, 2026**, unless noted.

### 8.1 What changed in the environment

- **Meta Quest Link OpenXR** updated from **Oculus 1.208.0** (36 extensions) to **Oculus 1.210.0** (37 extensions) on the morning of Nov 25, between the last good session and the first crash. Runtime JSON: `C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json`. We enable only `XR_KHR_D3D12_enable`. The 37th *offered* extension is `XR_LOGITECH_mx_ink_stylus_interaction` (not enabled, not treated as the cause). `openxr-oculus-compatibility.dll` from Virtual Desktop Streamer is often loaded; we are not using VD as the runtime in these sessions.
- **NVIDIA** `nvwgf2umx.dll`: current driver dated **Nov 20, 2026, 4:26:03 PM PT**, size 92,958,720, checksum `0x5881b94`, `nvdmi.inf_amd64_7453e343317ef197`. A rollback to the previous driver (the 11:38 session) **still crashed**. The owner is back on the current Nov 20 driver; that is fine.
- Official Spidy **0.8.8** appeared overnight. The Update button is a notice only.

### 8.2 The crash family

- Exception **`0xC00000FD`** (`STATUS_STACK_OVERFLOW`) in **`nvwgf2umx.dll`**.
- Fault offsets seen on the same driver binary: **`+0x315331`** (morning sessions and the 2:41 / 2:43 PM beta3k dumps) and **`+0x396e7e`** (1:03 PM beta3j-diag dump). RIP/base vary with ASLR; e.g. afternoon beta3k dumps: RIP `0x7ffbac175331`, base `0x7ffbabe60000`.
- Typically ~3 seconds in, **0 headset submits**. RSP is often off the captured stack, so `inspect_minidump.py` returns **0 walk candidates** — no Spidy, OpenXR, or LibOVR frames. Do not invent a stack.
- A different older dump (Nov 21, beta3b, 22 minutes in) was a null-pointer read in the game, not this family. Force-close `0xC0000409` at exit is a **non-bug** (launcher wording).

### 8.3 Timeline of tests and builds

1. **beta3i** (`3c7bb41`) and then earlier NA builds all died the same way once 1.210 was in. First crashes ~10:57–11:02 AM, dumps in Drive **Dev3**. Not the new hand/line code: no web was out, no punch thrown, death before the first submitted frame.
2. NVIDIA rollback: **still crashed** (11:38 AM, same overflow, older driver).
3. A "beta3g" check was actually the **Crime Run port** `1236723` (same OpenXR start path as `c488634` with Crime Run off). Same death. That **clears later NA commits** as the unique cause.
4. **beta3j-diag** (`6fa2418`): same features as beta3i, plus a flushed start log. 1:03 PM session: frames 1–33 empty `xrEndFrame` ok; FOCUSED; frame 34 `xrBeginFrame` ok on first `shouldRender=1`; **no "Headset active"**; died in `native_gpu::start()` during `prepare()`. Dump: `+0x396e7e`.
5. **beta3k** (`990d384`): move GPU start to an earlier unfocused frame. **Failed.** Start log last line: **`GPU creating D3D12 lists`** after `xrBeginFrame` on frame 1 (`shouldRender=0`). No hook enable, no throwaway swapchain, no `GPU bridge started`, no FOCUSED. Sessions at 2:41:38 and 2:43:07 PM (folder `C:\Games\Spidy-0.8.7-player-torso-990d384`, stereo hash `838fc174…`). Dumps 2:41:56 and 2:43:22 PM, both `+0x315331`, threads 37628 and 35036. Consoles end "Game closed" with no exit-code line. The owner later attached these thinking they might be beta3l; they are not — they predate the beta3l zip (~3:00 PM).
6. **beta3l** (`3baaecf`): start the GPU bridge after the overlay is created, **before `xrBeginSession`**. Start log should show `GPU bridge started` *before* `begin session`. Shipped Nov 25 ~3:00 PM. On Nov 26 the owner said **"beta3l still breaks."** **No beta3l start log, session logs, or `.mdmp` have been received.** Confirm the install path is `…-player-torso-3baaecf`, not `…-990d384`, before treating that report as a failed workaround.

Public search did not turn up a matching Meta 1.210 / OpenXR / D3D12 / NVIDIA stack-overflow note.

### 8.4 Hypotheses not yet tried (or not yet evidenced)

- **beta3l actually failed after the pre-session move** — unknown until a start log names the last step (a later create call, hook enable, throwaway swapchain, first `xrBeginFrame`, first submit, etc.).
- Hook re-entrancy or a small `CreateThread` stack — **ruled out for the beta3k death** (hooks never enabled). Could matter if beta3l dies later.
- Mid-frame `CreateCommandList` was the whole story — beta3l tests that; we do not have the result on disk.
- Official 0.8.7 / 0.8.8 also crash on Link 1.210 (our GPU bridge vs Ilya's start path).
- Link 1.210 only; VD or SteamVR as the OpenXR runtime would work.
- A Meta PTC / 1.208 rollback would restore play without a code change.

---

## 9. Ranked next steps

**Status, Nov 26 (evening):** beta3p builds the owner's t151u answer: Option A of
`plans/level-head-cohesion-1126.md` (Level keeps him with you), HEAD LEVEL as the default and the smoothing clock
fix, as a test zip (CI publish=false). Options B (dodge tilt) and C (animated-rotation reprojection) are not built.

**Status, Nov 26 (later):** beta3o adds the owner's t147u-t149u (above): the wall slide pose, the grip jump lock,
the held line in the fist and the rope grip, built as a test zip (CI publish=false).

**Status, Nov 26:** beta3m fixed the crash (the owner: "No MDMP, game ran fine"). beta3n adds the owner's t141u and
t146u requests (above) and is built as a test zip (CI publish=false). The list below is the older crash plan, kept
for history.

1. **Get beta3l evidence first.** Ask the owner to install `Spidy-0.8.7-na-beta3l-win64-3baaecf.zip`, launch the **`3baaecf`** folder, not press Update, and if it dies, send the usual logs, `spidy-vr-start.log` from `reports\stereo-modules\<hash>\`, and a new `.mdmp`. Confirm the console build line. The last start-log line decides the next code change. Do not guess a beta3m without that.
2. **Bisect the runtime vs our code.** Ask the owner to run **official 0.8.7** and/or **official 0.8.8** once on the same Link 1.210 + current NVIDIA driver (in a separate folder; do not use Update to overwrite NA). If official also dies, this is a Meta 1.210 / NVIDIA interop bug and a workaround or a runtime rollback is the path. If official runs, diff Ilya's start path (`cf0b279`) against ours and match the safe parts.
3. **Try Virtual Desktop or SteamVR** as the OpenXR runtime (leave Link installed). If those play, that confirms Link 1.210 as the trigger and gives the owner a way to play NA in the meantime.
4. **Meta Link 1.208 rollback or PTC opt-out**, if the owner is willing. Last known-good combination was beta3g + 1.208.
5. **Only after a beta3l start log names the dying call**, consider code changes such as:
   - creating the command lists on a dedicated thread with a large stack;
   - deferring remaining start steps (hook enable, throwaway swapchain) until after `xrEndFrame` of N empty frames, never mid-`xrBeginFrame`;
   - skipping only the throwaway Present-hook swapchain if that is the last line (headset copies do not require it);
   - matching any safer official 0.8.8 start-order difference.
   Creating lists on a **separate D3D12 device** is probably not viable for eye copies (wrong device). There is no playable "skip the whole GPU bridge" mode.

---

## 10. Pending, deferred, and Drive

### Pending

- **Audio:** 11 ElevenLabs jump sounds, owner-made on a paid account, to ship with the mod. Waiting on the owner to trim, tag, and say where each goes. Then design playback (bundled files), with a toggle ON by default and a new migration revision.
- **beta3l crash evidence**, as above.
- **Crime Run** has not been played in a headset yet. A checklist is in `docs/CRIME_RUN.md`. Hold Crime Run play-feel and death/mission/load policy until that happens.

### Feel proposals (ask first; several were held for kick-on-thug logs)

- **S4.** Wider kick lock-on cone when the target is behind the player. Held: Dev2 had 8 kicks, 0 hits, no enemy in range.
- **S5.** Kick animation pass 2. Held for the same reason.
- **S6.** Double-tap A while upside down: 11 worked in Dev2, a few looked off. Needs a headset report.

Airborne kick flicks while not swinging already start an air kick; the old "refused" log line was wording only (`016f983`). No play-feel change required there.

### Google Drive (connector is connected)

The owner uploads logs here. Listing/download may need the Drive tools; some folders needed sign-in until the owner connected Drive.

| Folder | Id | What |
| --- | --- | --- |
| Owner log drop (handoff listing URL) | `1nSJCrB69eluLQ5opnSrFoiua5KSBwWkp` | Usual `game-vr-<date>-<time>-{diag,game,console}.log` and summaries |
| Dev2 | `1u9SaL0KgE734pYvyMiDgXppe-kpfsaqw` | Nov 23–24 beta3g webs/kicks (not Crime Run). Healthy 71–72 fps, mod time << 0.1 ms/frame |
| Dev3 | `1mbsV8vQkAPf2R80pZolysPyB1MJXvmLA` | Nov 25 morning crash logs and dumps (beta3i / Crime Run port). Afternoon sessions were often attached in chat instead |

Attachments on this box sometimes appear late; `Path.exists()` on the exact hash path can succeed before `iterdir()` lists them. Copy into `/workspace` and parse. Chat dumps: `/home/box/agent-data/agents/<agent-id>/attachments/`.

### Notes for the new assistant

- Confirm every headset test's **folder sha7 and version label** before attributing a crash to a build. The 2:41–2:43 PM "How about now?" batch was still beta3k `990d384`.
- Do not push to `origin` (IlyaMez). Do not force-push. Do not touch CURRENT. Do not edit workflows.
- Discord: draft only.
- This document's box copy is `/workspace/SPIDYVR_NA_HANDOFF.md`. The in-repo copy is `docs/HANDOFF_NA.md` on `feat/087-next`.
