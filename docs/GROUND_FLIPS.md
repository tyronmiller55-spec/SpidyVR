# Ground flips

Flips from the ground are jump types (the owner, Nov 18): the point jump's launch, then the spin goes to the flip mode
that is on (REALISTIC FLIPS at ROTATION SPEED; CONTROLLED or basic FLIPS one turn at FLIP SPEED), in fights too.
Code: `include/spidy/ground_flip.hpp`, wired in `src/game_xr.cpp` (GROUND FLIPS block). Tests: `tests/ground_moves_tests.cpp`.

## Triggers

| Trigger | Flip |
|---|---|
| Right stick flicked straight down, then A within 150 ms | backflip |
| Right stick flicked straight up, then A within 150 ms | front flip |
| **Directional A tap:** left stick held, then A pressed and let go within 180 ms | that way (below) |

### Directional A taps

Common rules: on the ground under the animation, the stick held steady in its band for at least **150 ms** (a stick
only just pushed is QUICK SIDESTEP's or a plain jump), A let go of within **180 ms** (held longer, it goes on to the
game as its normal jump, 180 ms late), 0.6 s between flips. B down, the legs chord (Y+X) or a blocked state: no flip,
the A goes to the game at once. Plain A with the stick centred is never held back.

| Direction | Stick | Speed limit | Flip |
|---|---|---|---|
| Side | at least 0.8, 55-100 deg off straight ahead | 3 m/s | side flip (cartwheel) that way |
| Forward (Nov 18 1:09 PM) | at least 0.8, within 25 deg of straight ahead | 2 m/s | front flip |
| Back, STABLE / OLD / HEADSET FIRST (Nov 18 1:09 PM) | at least 0.8, within 25 deg of straight back | 2 m/s | backflip |
| Back, HYBRID | **0.3 to under 0.5**, within 25 deg of straight back, and the rear snap not holding | 2 m/s | backflip |

- **Running jumps stay jumps:** moving faster than the limit (2 m/s ahead or back) denies the flip and the A goes to the
  game at once; so does a stick that has not held 150 ms.
- **HYBRID's rear snap** fires at a back tilt of 0.5 within about 60 ms (`hybrid_movement::snapTilt`, `settleSeconds`)
  and turns the view. A back tap therefore only counts a held back stick under that tilt, where HYBRID neither snaps
  nor moves him, and never once the snap holds (he runs the snapped way).
- Logs: `ground flip: <kind>, launch ..., by tap`, `tap flip: denied <why> (stick x, y, speed)`,
  `tap flip: A held past 180 ms, the game's jump`.
