# SHOWTIME (EXPERIMENTAL, on by default since revision 14)

> The plan's Pillar 6 (the user, Nov 13 3:50 PM PT): hold X and B and the view steps outside Spidey. You still look and lean with the headset, one to one. Let go and you are back on his head.

Built in stages, one commit each (S0 the chord and the cut; S1 the camera; S2 the automatic moments; S3 the body). See the CHANGELOG for each. Every row sits under **Showtime** in the SPIDY VR tab and the launcher, It is on by default (the owner, Nov 19: leave all features on for further testing; movement_defaults revision 14 turns an old off once). Turn it off with SHOWTIME in the NA · SHOWTIME tab or `showtime=0`.

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
| Camera | Eye command 4 (`anchored` level frame) as a level scene uses, resolved on the render thread from `game_fp::showtimeFrame` (the XR thread publishes the frame every update; it expires after 250 ms, so a stalled thread gives the eyes back to the head within a quarter second). The eyes are the headset's own (`toRef`: the head's position and heading at the cut). S0: fixed 2.5 m behind him along the look heading; S1 below. |
| Hands | **WATCH ONLY (default):** nothing new starts. No web, ball, punch, strike, walk, jump or dodge; a hand holding a web keeps its squeeze and trigger, every other is let go of; sticks, A, B, X and Y are not the game's. **KEEP CONTROL (S1):** the left stick walks the way the camera faces, A jumps, B dodges (B is the chord's while X is held, so the dodge is for LATCH); webs, punches and flips stay off. |
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

## S1: the camera

`showtime_rig.hpp` (`OrbitRig`): a level frame (never a roll or a pitch) at Spidey's pivot (his pelvis, 1 m above his feet) plus an offset.

| Piece | What it does |
|---|---|
| Place | DISTANCE behind along the orbit heading, ORBIT ANGLE off his back (he appears that many degrees to one side of the view's middle), HEIGHT above the pivot. Defaults 2.5 m, 35 degrees, +0.3 m. The first frame is the headset's look heading, so the cut turns nothing. |
| Follow | A critically damped spring on the pivot in every action, swings and zips included: horizontal omega SOFT 4 / NORMAL 6 / TIGHT 9 rad/s, vertical 3.5; omega rises toward 12 as the lag goes from 3 to 6 m (never loses him, never a cut); a 0.12 s look-ahead of his velocity, low-passed at 2 Hz. The orbit itself is not smoothed: it is the player's own input. |
| Orbit | The right stick turns the camera round him: smooth at your SMOOTH TURN speed (120 deg/s when the launcher has snap turn), eased in and out; or ORBIT TURN snap: 30 degrees a flick under a 100 ms blink (the yaw moves at the blink's black). Up and down: the height, -0.5 to +1.5 m in all. The right stick never turns the play heading, zips or heals while out. |
| Vignette | From the rig's own speed (starts 3 m/s, full 12) or acceleration (6 to 20 m/s2), low-passed; LIGHT 25 %, STRONG 45 %, OFF. Never from head motion. Drawn by the grade pass. |
| Nearer than 0.8 m | The picture fades to black. |
| Collision | **Only the game's own camera as a pull-in**, read-only (`game_fp::stockSeat`): when it sits within 30 degrees of the way we would sit and nearer than the distance it reached in the last 3 s, our distance is limited to it (in at 20 rad/s, out at 3). The plan's primary (steering the game's camera by its stick), the follow mover's collision query and boom fields (needs RE) and the ray fan are **not in this build**: the eight native ray slots are all used by the head, hands and roof, and taking them while out would starve head clearance and hand contact. Without a stock camera near our side the camera has no collision: it can sit in a wall. |
| Slow motion | SLOW-MO IN SHOWTIME (on): L3 starts it while out, drains the same meter. Off: L3 does nothing out there. R3 is never the game's out there (so no pause pair). |
| LATCH | Double press: a chord entered again within 0.8 s of the last one's end keeps the camera out after the let-go; the next chord press takes the latch off and its let-go leaves. Any forced exit clears it. |

Log line each second out: `showtime: cam d=<m> lag=<m> v=<m/s> w=<rad/s> vignette=<0-1> collide=stock|none pull=<m> fade=close|none`.

## S2: the automatic moments

| Row | Default | What it does |
|---|---|---|
| CHORD | HOLD X+B | AUTO ONLY: the chord is not read at all (X and B are the game's, the gadget wheel and the dodge as always). |
| AUTO: DODGES | OFF | PERFECT ONLY: the camera goes out when the game slows its time for a dodge (the `Moment::dodge` start). ALL: at the start of any dodge. It ends 0.3 s after the dodge input does, unless X+B is held; a dodge never seen live within 0.5 s is dropped; nothing keeps it out past 4 s (`showtime::Auto`). An auto dodge keeps its own A and B while out (WATCH ONLY would otherwise cut them). |
| AUTO: FINISHERS (LOG ONLY) | OFF | **Gated.** The plan has finishers go out by themselves with ON as the default. Finishers read as a first-person scene to the mod (`fpScene`) and nothing yet tells one from a cutscene, or says which owner animates him. Since beta2 a scene that starts in a fight within 1.5 s of Y+B (the game's own finisher input, which the game runs) logs `finisher: the game's own finisher on Y+B (...)` with `; AUTO: FINISHERS is log only` while this row is on, and any other scene `finisher: not a finisher (...)` (`finisher_handoff.hpp`). Nothing moves the camera. |

## S3: the body from outside

| Piece | What it does |
|---|---|
| Head restored | While out (fade to fade), his head is shown. In an animation the head shrink is a write in the game's own pose job (`game_fp::shrinkHead`); `game_fp::showtimeHead` (a 250 ms lease the XR thread refreshes each frame) makes it stand down, and since nothing persists, the next job shrinks it again at the cut back, or within a quarter second if the thread stalls. In SpidyVR (the native body) the solver's `hideHead` pose flag is dropped while out. The second is by the flag's own definition (`native_body.hpp`: "the head shrinks away"), not measured on a headset. |
| Hands, webs, marks | Not drawn over the outside view (scene mode), as in S0. |
| BODY (ADVANCED) | GAME POSE, HEAD SHOWN (default) / AS IN PLAY (NO HEAD): the second keeps the head hidden, to compare if the head restore misbehaves. NATURAL ARMS (Nov 19, from EngineBot's S0, `showtime_body.hpp`): the head shown; SpidyVR's body at the hero's own arm length (`pose.armLength` 0, body_calibration's arm scale 1) and the arm overlay paused under the game's animation, so the game's arms show. GAME SWING POSE (EXPERIMENTAL): as NATURAL ARMS, and while SpidyVR owns a swing the game animates (ANIMATION MOVES on, `swingState.owned`) the VR body's `bodyOn` is dropped so the game's pose is drawn (physics stays VR); `native_body::beginReclaim` picks the body back up. Log: `showtime: body <mode> owner=<anim|vr|frozen>`. |
| Who drives the body | Unchanged. Showtime never changes the owner (ANIM or VR) and never yields or re-adopts the pose: the body is what drives him now. |

**Gated, not built:** the plan's CORRECTED and RAW (DEBUG) body modes and the hybrid's pose corrections (they need a body forced to the VR pose or a corrected animation pose outside, which would flip the owner or edit the game's pose mid-animation; nothing here was verified on the game), the pose-yield handling that puts the VR body back with the position lock, and the plan's "finisher ends while the chord is held: carry on" (finishers are not classified yet). The camera, its collision (the ray fan, the game's own collision query and the boom fields) and AUTO: FINISHERS are as described above.

## Verification

`tests/showtime_rig_tests.cpp` (the spring has no overshoot and is exact for any step, placement, the yaw only from the stick, the snap and its blink, the height range, follow and lag, the vignette curve, the pull-in, the latch). `tests/showtime_tests.cpp` (chord order, Y block, B hold-back, fade at the cut, the exit reasons, the level frame's layout and yaw, the stick rotation). The camera, fade, and chord feel need the headset: nothing here was run on a headset.

## EngineBot's Showtime series (Nov 19 port)

EngineBot built a parallel Showtime on c6a8279 (`/workspace/spidyvr-patches/showtime/0001-0008`). This line already had
its own Showtime (eca9a0a, 798a767, 4bb497e, 331a70b) covering the same chord, fade, follow spring, orbit (smooth and snap),
vignette, close fade, latch, auto dodges and head restore, with the same `showtime_*` ini keys. Applying the series as is
would have made a second SHOWTIME tab, a second X+B chord and clashing ini keys, so only what this line lacked was ported:
the BODY modes (above) and the camera collision RE note (`SHOWTIME_CAMERA_COLLISION.md`). Not ported, with reasons:
the ray fan (no free native ray slots), automatic finishers with the finisher camera (finishers are still not told apart
from cutscenes; AUTO: FINISHERS stays log-only), and the exit realign to the heading (this line's exit keeps the exact
heading the view left, by the headset-leads rule).
