# Point jump

POINT JUMP (EXPERIMENTAL) is a setting in the launcher (Jumping and landing)
and the in-game VR SWINGING settings. It defaults off and saves in
spidy_vr.ini (`point_jump`) and shared settings profiles. Off, nothing below
runs and A and B behave exactly as before.

Whenever the player touches a surface (the ground, a perch, Spidy's own
wall), he can charge a jump that launches him **the way he is looking**. A
tap is a quick jump; holding up to CHARGE TIME gives a bigger leap, with the
charged leap's rumble. The launch runs in Spidy's own swing physics
(`Swing::pointJump`, `EventKind::PointJump`) and leaves the surface the way a
charged leap does (the game's own jump to unstick, then the launch
velocity). It never asks for the game's point launch, so there is no smoke.

## Trigger

| POINT JUMP TRIGGER | How | B alone | A alone |
|---|---|---|---|
| A+B TOGETHER (default) | both within 50 ms of each other | the dodge, at most 50 ms late; at once while a spider-sense warning stands | the game's A, at most 50 ms late; a tap inside the window still reaches the game |
| HOLD B, THEN A | hold B, press A | let go with no A: the dodge on the release; held 0.25 s with no A: the dodge then; a warning: at once | never held back |

Once both are down neither reaches the game until both are let go of;
letting go of either fires. A grip cancels the charge. The chord can start
only while point jump is allowed (below); otherwise A and B pass at once.
`ChargeJump::consume()` ends whatever A was doing when the chord closes, so
a chord never becomes a charged leap or a late A.

## Charge, direction, velocity

- A tap (held up to 0.15 s) launches at TAP SPEED. Longer, the charge eases
  out (`c = 1 - (1 - u)^2`) up to MAX SPEED at CHARGE TIME.
- Direction: the look plus SURFACE PUSH of the surface's normal (up on the
  ground, out of Spidy's wall), turned to at least 10 degrees out of the
  surface. Never into it.
- 25% of the speed across the jump is kept, never into the surface.

## When it is allowed

- On the surface now, or within 120 ms of leaving it. A release with no
  contact waits 150 ms for one, then is dropped.
- 250 ms after a launch, and only from a fresh contact.
- Not out of play, with the phone or gadget wheel open, in the pad layer,
  during the calibration, with a prompt naming A or B, in a vent, on a web
  line, in the game's own crawl, or during a takeoff.
- After the game's zip lands, the release waits 400 ms, so the takeoff's
  own jump press is never read by the game as its point launch.

## Settings

| Row | ini key | Default | Choices |
|---|---|---|---|
| POINT JUMP (EXPERIMENTAL) | `point_jump` | OFF | OFF, ON |
| POINT JUMP TRIGGER | `point_jump_trigger` | A+B TOGETHER (0) | A+B TOGETHER, HOLD B, THEN A |
| TAP SPEED | `point_jump_min_speed` | 9 m/s | 6, 7, 8, 9, 10, 12, 14 |
| MAX SPEED | `point_jump_max_speed` | 22 m/s | 14, 16, 18, 20, 22, 26, 30 |
| CHARGE TIME | `point_jump_full_ms` | 1 s | 0.5, 0.75, 1, 1.25, 1.5 s |
| SURFACE PUSH | `point_jump_normal_bias` | 25% | 0, 10, 25, 40, 60% |

The other timings (chord window, coyote time, buffer, cooldown, zip guard)
are `point_jump::Params` defaults.

## Diagnostics (`point jump:`)

- `point jump: chord A-first 18 ms (trigger 0, ground)`
- `point jump: charge 0.62 -> 17.9 m/s dir (...) n (...) out-fix 0 deg, held 640 ms, launch (...), from the ground`
- `point jump: denied <why>`, `point jump: charge cancelled (grip) after N ms`,
  `point jump: the swing refused the launch (N)`

`tests/point_jump_tests.cpp` (ctest `point_jump`) covers both triggers, the
charge curve, the direction, the gate, the zip guard, `ChargeJump::consume`,
the swing bridge and the settings; `spidy_tests` checks the swing's launch.
