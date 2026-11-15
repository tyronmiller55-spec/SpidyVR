# SHOWTIME (EXPERIMENTAL, off by default)

> The plan's Pillar 6 (the user, Nov 13 3:50 PM PT): hold X and B and the view steps outside Spidey. You still look and lean with the headset, one to one. Let go and you are back on his head.

Built in stages, one commit each (S0 here; S1 to S3 are listed in the CHANGELOG as they land). Every row sits under **Showtime** in the SPIDY VR tab and the launcher, and none of it runs unless SHOWTIME is turned on.

## How it keeps "animation never turns the view yaw"

The user's standing rule: the headset's direction is the single per-frame view-heading authority, and animation never turns the player's view yaw in normal play. Showtime is an explicit camera mode the player chooses, and it keeps the rule where it matters:

- **It never writes the heading authority.** HybridHeading, the view yaw lock, the stick basis and the facing paths are not touched while the camera is out, nor at the exit. The exit puts the eyes back on his head with exactly the heading they left (the eye command is not touched; `game_fp::showtimeFrame` is withdrawn and `frame()` returns to `heroHead`).
- **The rig yaw is the camera's own.** While out it is set once, at the cut, to the heading the player was looking along (`hsViewNow`, headset first), so the cut turns nothing. Afterwards it changes only from the player's right stick (the orbit, S1). A dodge, a flip, a landing, a handoff, an owner switch, an enemy or a finisher never turns it: if he spins, he spins in the picture.
- **The right stick belongs to the orbit** while out: no snap or smooth turn of the play heading.
- **Own marks, webs and hands are not drawn** over the outside view (the same `scene` mode a cutscene uses).

## S0 (this build): the chord and the cut

| Piece | What it does |
|---|---|
| Chord (`showtime::Chord`) | X and B held for 200 ms. **X first:** B never reaches the game (no dodge). **B first:** B passes at once (the dodge keeps its perfect-dodge timing), then X and B held 200 ms enter. **Y held blocks it** (Y+X+B is the passive legs' flail, [PASSIVE_LEGS.md](PASSIVE_LEGS.md)). A+B and hold-B-then-A (the point jump, [POINT_JUMP.md](POINT_JUMP.md)) have no X, so they are not a chord. From the moment both are down X is Showtime's (no gadget wheel, no Back); once entered, B is too. It is read before FLAIL, POINT JUMP, the dodge and the wheel see X or B. |
| Cut (`showtime::Machine`) | Fade out and in about the cut: FADE ms (OFF / 100 / 150 / 250 / 400, default 150), the cut at black. OFF cuts instantly. |
| Camera | Eye command 4 (`anchored` level frame) as a level scene uses, resolved on the render thread from `game_fp::showtimeFrame` (the XR thread publishes the frame every update; it expires after 250 ms, so a stalled thread gives the eyes back to the head within a quarter second). The eyes are the headset's own (`toRef`: the head's position and heading at the cut). S0: fixed 2.5 m behind him along the look heading, 2.0 m above his feet. |
| Hands | **WATCH ONLY (default):** nothing new starts. No web, ball, punch, strike, walk, jump or dodge; a hand holding a web keeps its squeeze and trigger, every other is let go of; sticks, A, B, X and Y are not the game's. **KEEP CONTROL:** WHILE row is stored (the left stick walks relative to the camera, A jumps, B dodges) and is wired in S1. |
| Exits | Release, SHOWTIME turned off, the Menu button (`menu`), a feet jump over 30 m in a frame (`teleport`), or the game taking the view (a scene, a pause, a lost character: `lost control`, no fade over it, the published frame is withdrawn at once). |
| Look | `D3D12Renderer::Grade` gained `vignette` and `fade` (the slow-motion recolouring pass, root constants 12 to 16). The grade now also runs over a bare (scene) view when either is on, so the fade covers the cut both ways. |

Log lines: `showtime: chord lead=X|B (X+B held N ms)`, `showtime: enter (chord) yaw N deg, seat ...`, `showtime: exit (release|menu|teleport|disabled|lost control) after N s`.

### Settings (S0)

| Row | Default | Notes |
|---|---|---|
| SHOWTIME | OFF | EXPERIMENTAL. `game_swing::Settings` is untouched (v40): the rows travel in `spidy_vr.ini` (`showtime`, `showtime_while`, `showtime_fade_ms`). |
| WHILE | WATCH ONLY | WATCH ONLY / KEEP CONTROL |
| FADE | 150 ms | |

Needs the body on (`values.body`), Spider-Man as the player, and the headset tracking. None of these rows is in shared profiles.

## Verification

`tests/showtime_tests.cpp` (chord order, Y block, B hold-back, fade at the cut, the exit reasons, the level frame's layout and yaw, the stick rotation). The camera, fade, and chord feel need the headset: nothing here was run on a headset.
