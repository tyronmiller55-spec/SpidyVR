# Build 1 (Nov 12): MOVE HAPTICS, VIEW YAW SOURCE: STABLE, SOUND TRACE

The user, Nov 12 04:56/05:07 PST (t70u). Branch `feat/haptics-stable-yaw` from `feat/legs-yx-hold` @ 0a593c0.
Build 2 (jumps hand over to SpidyVR after ~0.5 s) is not part of this.

## MOVE HAPTICS (`move_haptics.hpp`)

| Event | Detected | Strength (before the setting) | Length |
|---|---|---|---|
| Jump | supported -> airborne, up >= 2.5 m/s (within the first 0.15 s) | 0.35 + 0.65 x clamp(speed/25) | 60 + 80 x s ms |
| Wall jump | off a wall, speed >= 4 m/s | same | same |
| Zip launch | takeoff with a zip pressed in the last 3 s; or in the air: up-speed +6 m/s within 0.25 s to >= 5 m/s, no web, once per air phase | same | same |
| Point jump | swingPointJump accepted | 1.0, then 0.85 at +110 ms | 70 ms, 100 ms |
| Landing | >= 0.25 s in the air, fastest fall over the last 0.3 s >= 3 m/s | 0.2 + 0.8 x clamp((fall-4)/21); x0.7 onto a wall | 40 + 120 x s ms |

- No jump within 0.4 s of a point jump (its own) or 0.3 s of another takeoff; a web's yank is no jump.
- Scale = MOVE HAPTICS / 100 (OFF, 25-200%). Played with `XrRuntime::hapticUnscaled`: CONTROLLER VIBRATION does not scale them. Above 1 the amplitude caps and the pulse lengthens up to 1.5x.
- The point jump's charge rumble moved onto MOVE HAPTICS too.
- Log: `move haptics: <kind> <speed> m/s -> <amplitude> <ms> ms (move haptics N%)`.

## VIEW YAW SOURCE: STABLE (`stable_yaw.hpp`) - rotation desync Step A

Root cause: steering target = head-bone yaw + HMD delta + Level turn; the head bone turns with the hero, so a held side push chased itself (03:49 session: 26% of steer samples > 45 deg apart, chase episodes up to 106 s).

STABLE heading = base + (trackedYaw - refYaw) + animHeadTurn + own R turn:
- base latched from the head bone at the handoff; follows it while the left stick has been idle 0.2 s; frozen while the left stick is held.
- own R turn (smooth deg/s or snap, with 0.7/0.4 flick hysteresis) only while running and outside Level (Level's animHeadTurn already turns).
- filtered at tau 0.1 s (was 0.3); the game camera (CameraLook) follows the same heading; composeMove gets R = 0 while running (left stick idle: the bounded facing request stays).
- BODY (OLD) keeps the old formula.

Closed-loop test: with the old source a held right push sweeps > 720 deg in 5 s; STABLE turns 90 deg and runs straight.

Logs:
- `yaw sync: hero H view V steer S gap G L l R r src stable|body` at 10 Hz while steering in animation.
- `yaw sync: gap over 45 (#n) ...` / `yaw sync: gap back under 40 after N ms`.
- `yaw sync: RUNAWAY TURN #n hero swept X deg in N ms, own turn Y, stick (x,y) R r src ...`: the hero sweeps >= 90 deg one way within 1 s while the stick holds one direction (1.3 s), R is idle and the player's own turn (HMD + R) moves < 30 deg. Rearms after 1 s.

ANIM YAW SWAY is deferred to Step B: Step A does not change the eyes.

## SOUND TRACE (`sound_trace.hpp`, `game_sound.cpp`)

- MinHook detour on the game's sound call (16d5e80, entry bytes verified as before). 16 integer parameters forward the 13 real ones (floats live in stack slots, copied bit for bit). Nothing is changed.
- Each call -> `SpidySoundTrace` ring (2048 entries, lock-free, per-entry sequence): QPC, id, handle, flags (hero: handle == the player's from game_swing; spidy: our own play()).
- Per takeoff/landing/point jump, after +1.0 s: `sound trace: <event> | N sounds | hero: id@+ms ... | near: id/hhandle@+ms ...` (hero ids nearest the event, at most 12; others within 150 ms, at most 4).
- SOUND TRACE setting (NATIVE ANIMATION), `sound_trace=0` disables the hook. Errors: 9801 another build, 9810+/9830+/9850+ MinHook.
