# Hitches: what the diag says and what the mod does about them

A hitch is a game step (or a VR frame) much longer than the rest. In the Grip1 playtest (Nov 14) the XR loop
ran at about 69 fps and the game thread stalled over 100 ms about 1,100 times in 20 minutes, with the scene
resolution at 67%. The first job is to tell the mod's cost from the game's own render load; the second is that
a stall must not break what the mod is doing.

## Reading the diag

| Line | Written | Says |
| --- | --- | --- |
| `perf:` | every 5 s | The VR loop's frames and the mod's own work in them (`frame_stats.hpp`). |
| `game frame: ... game steps ...` | every 10 s | The game thread's own cadence, from the swing sample's QPC stamp: steps, mean, longest, how many were over 50 ms. |
| `game frame: spike N ms ...` | each step over 100 ms | One long game step (a step of 50 to 100 ms is only counted in the summary). |
| `game thread cost: ...` | every 10 s | What the swing module's step cost the game thread: mean and longest in microseconds (the solver's and the kicks' share of the longest), how many steps ran over 1 ms and over 5 ms, and the hitch guards' fire counts. |
| `vr stabilization: ...` | on change | The tilt the rig wanted and what is shown. |

How to read the cost line: if the longest step is a few hundred microseconds while a `game frame: spike` of 100
ms or more sits in the same window, the game thread was stalled by something else (the render load: the G1
logs rose and fell with the scene resolution). A step over 5 ms is the mod's.
The lock wait counts in the cost: the swing module's step takes the game thread's turn from the start of
`visit()` (waiting for the `simulation` lock) to its return.

`hitch guard: N velocity samples held` is `hitch_guard::VelocityGuard::rejected()` for the window (a mover
velocity not believed because the step was long, stale, not finite or too fast, so the last believed one stood),
and `N over-limit frames ignored` is `hitch_guard::OverLimit::ignoredFrames()` (a frame over the speed limit
that did not count towards letting go of the webs).

## What the mod does about a stall

The swing sample's freshness (`swingFresh` in `game_xr.cpp`) used to be "stamped under 100 ms ago". Every gate
reads it, and `head_probe::classify` calls a state that is not fresh a cutscene, so a game-thread stall over 100
ms flipped the whole pipeline (the hybrid air-facing gate, ANIM HEAD RIDE, the hero turn, the steerable and
turn-in-place gates) for the length of the stall and back after it. `hitch_guard::FreshHold` gives it
hysteresis: a sample under 100 ms old enters live; it stays live until it is 350 ms old. A swing module that
stopped (status 0) or has no stamp is not live at once. A stall past 350 ms (a cutscene, a pause) goes stale as
before, 250 ms later. The one-shot actions (point jump, sidestep, ground kicks, mount) and the realistic-flip
flight keep the plain 100 ms rule: a stale press must not fire late, and the flight reads web tension.
`swing sample: N game stalls carried as live ...` (every 10 s, only when there were any) says how often it held.

## Where it lives

- `include/spidy/hitch_guard.hpp`: `VelocityGuard`, `OverLimit`, `GameFrameClock`, `CostWindow`, `FreshHold`.
- `src/game_swing.cpp`: `visit()` times itself (`VisitCost`, `CostTimer`) and publishes `SpidySwingCostLive`
  (`SwingCostLive`, `game_swing.hpp`) once a window; `src/game_xr.cpp` reads it and writes the line. No setting,
  no feel change; an older swing module has no `SpidySwingCostLive` and XR then writes no cost line.
- `include/spidy/vr_stabilize.hpp`: `LogGate`, the on-change rule of the `vr stabilization:` line.

Tests: `hitch:` checks in `tests/core_tests.cpp`; `tests/vr_stabilize_tests.cpp`.
