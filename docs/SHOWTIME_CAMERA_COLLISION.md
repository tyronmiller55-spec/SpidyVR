# Showtime camera collision: reverse-engineering note (S-d)

Read-only research for Showtime's camera (EngineBot, November 2026; Pillar 6 of
the Gymnastics & Combat plan). The user asked on November 13 (t23u): "the game
should already have a collision system for the camera to keep it from clipping
that we can repurpose". **No new game address is used or guessed here.** Every
address below is already in the code base, with the file that holds it.

> Ported Nov 19 from EngineBot's Showtime series onto this line's own Showtime (`showtime_rig.hpp`). This line's
> camera uses only the stock camera's pull-in (`game_fp::stockSeat`, SHOWTIME.md S1). The ray fan described below is
> **not** built here: the eight native ray slots are taken by the head, hands and roof (SHOWTIME.md), so it stays a
> plan, not code.

## What the mod already knows (no new RE)

| Fact | Where |
|---|---|
| The follow camera's mover update is shared by Follow, Look, LookGame, Turret, PerfTest and PhotomodeLook movers (`1e1cf80`). It ends in the commit (vtable slot 24 / `0xc0`, `1e1d600`), which the input bridge hooks. | `docs/VALIDATION.md` ~5685, ~6262; `src/game_bridge.cpp` (`commit`) |
| The bridge logs the player's camera transform before and after the commit (`Data::before`, `Data::after`) and the player's transform (`Data::player`) every frame. `game_xr` reads them as `camera` and `body`. | `include/spidy/game_bridge_protocol.hpp`; `src/game_xr.cpp` (`sampleBridge`) |
| The combat camera's update (`5c2630`) places its own camera with SetTranslation and never commits. It is hooked and observed only. | `src/game_bridge.cpp` (`combatUpdate`) |
| The camera vtables: follow `0x3871fd8`, combat `0x38720d0`. | `include/spidy/game_bridge_protocol.hpp` |
| The game's own position setter for the camera (`191c590`). It runs **after** the game's collision, so a position written there is not collision-checked. | `src/game_bridge.cpp` (`Mode::cameraOffset`) |
| The game's camera orbit input is the virtual pad's right stick (`walk.look`, `walk.lookUp` → `thumbRX/RY`). `game_pad::CameraLook` / `CameraPitch` already close a loop on the error between a wanted look and the camera's forward. It was measured at 170° → within 4° in 1.4 s. | `include/spidy/game_pad.hpp` (`CameraLook`), `src/game_xr.cpp` (`cameraLookTo`) |
| The stock camera pose reaches `game_fp::camera()` every render frame, in gameplay too. | `src/stereo_probe.cpp` (`activeView`) |
| Finishers and death use movers that place the camera from an animation (MeleeRelAnimCameraMover, RelativeAnimCameraMover, DeathCameraMover) and never commit. They read as a scene to the VR gate. | `docs/VALIDATION.md` ~5732, ~6265 |
| Native world rays (8 slots, Havok queries with the player's body filter). | `include/spidy/native_rays.hpp` |

## What Showtime does with it (implemented in S-e, no RE)

1. **Steer the game's camera, then read back its position.** While Showtime is out, `cameraLookTo` is the
   Showtime rig's orbit direction instead of the head's. `CameraLook`/`CameraPitch` turn the game's follow
   camera onto that bearing, and the game then resolves the camera against the world with its own
   collision. Each frame, `showtime_camera::CameraRig` gets the pivot (Spidey) and the game camera's position
   (`camera[12..14]`, the transform at the commit). It measures the game's distance along the pivot →
   camera ray and learns the open-space distance as a running maximum that decays over about 3 s. When the
   game's distance is shorter than that (a wall pulled it in), our distance is limited by the same ratio.
   This is used only while the game camera is within 20° of our bearing (`CameraRig::alignedDegrees`).
2. **Only the position is used.** The game camera's yaw, its auto-recentring and its shake never reach
   the view. The rig's yaw comes only from the player's orbit input, and its pitch and roll are always 0.
3. **Fallback: a ray fan.** Five native rays go from the pivot towards the rig (the centre plus a 0.25 m
   ring), tags 40 to 44, in the slots the head-clearance rays leave free while the camera is out (the head
   is not in the body then). The nearest hit, less 0.25 m, limits the distance. Pull-in uses ω = 20 rad/s
   and push-out ω = 3 rad/s. Below 0.8 m the view fades to black.
4. **Finishers.** Scene cameras ignore the stick and run no follow collision. FINISHER CAMERA: GAME'S
   CAMERA uses `sceneCamera(2)`, the director's camera as today. ORBIT uses the rig with the ray fan only.

## Unknowns (read-only RE, not done; no addresses guessed)

| Unknown | Why it would help | How to find it (read-only) |
|---|---|---|
| The follow mover's collision query inside `1e1cf80` (the sweep from the pivot to the wanted camera position), its radius and its filter word | Call it directly from the commit hook with our own pivot and target. That gives exact game collision in finishers too, with no steering lag | Static: follow `1e1cf80` to its calls into the Havok query context (cf. `native_rays`' QueryContext); look for a sphere or capsule cast whose result scales the boom. Dynamic: log its arguments in a debugger with the camera against a wall |
| FollowCameraMover's boom length and pivot height fields | Set DISTANCE and HEIGHT on the game's own camera so every result is collision-resolved | Diff the mover object while the game changes the camera distance (sprint, swing, combat zoom). Candidates are floats near the mover's +0x100..+0x300 that change smoothly with the camera distance |
| The vtables of MeleeRelAnimCameraMover / RelativeAnimCameraMover | A clean finisher classifier for AUTO ON FINISHERS instead of "a scene within 3 s of a fight, under 6 s" | Read `Data::mover`'s first word in a finisher (the bridge sees the mover only when it commits, so add a read-only sample of the active mover) |
| Which TimeScale channel a finisher's own slow-down uses | Blend Showtime's slow motion with the finisher's instead of taking the minimum | `game_time.hpp` hook: log the channels during a finisher |
| Whether B or X does anything in a finisher (skip, prompt) | The chord swallows them in scenes | Headset check S2 |

## Risks

- Steering may fight the game's auto-recentring at speed (swings, zips). If it does, the rig falls back to
  the ray fan (`Collide::rays`). The diag line `showtime: cam ... collide=<game|rays|none>` shows which one.
- The ray fan answers about 100 ms late, so it can clip briefly at speed. The minimum-distance fade
  covers it.
