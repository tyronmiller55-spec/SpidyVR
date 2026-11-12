# Leg control

LEG CONTROL (ADVANCED) is a setting in the launcher (Flip controls) and
the in-game VR FLIPS settings. It defaults off, needs CONTROLLED FLIPS or
REALISTIC FLIPS, and saves in spidy_vr.ini (`leg_control`) and shared
settings profiles. Off, nothing below runs and X, Y and the right stick
behave exactly as before.

In the air or swinging free (never on the ground, a wall, a crawl, a mount
or a web line), hold Y and X together: a short buzz says the legs are
engaged, and the right stick poses them.

- Forward: a jackknife (both legs forward).
- Back: a rearward splay (legs back and apart, knees a little bent).
- Left / right: both legs swung that way, the near leg wider.
- Centre: straight legs together. Diagonals blend their two neighbours.

Let go of Y and X with the stick still pushed and the legs stay engaged
while the stick moves; back to neutral for a moment ends them, and the
legs ease back to straight over 0.6 s. From the chord until both buttons
are let go of, neither Y nor X reaches the game or the mod (no web strike
or zip, no gadget wheel). One button pressed alone is held back at most
80 ms for the other and then passes as before. While the legs have the
right stick it does not turn, zip or heal.

The legs are loose: they swing with the body's motion (a spring set by
LEG STIFFNESS and LEG DAMPING), and their swing turns the body back by
LEG COUPLING (angular momentum kept; OFF turns nothing). Tucked they spin
a flip faster, stretched slower. With CONTROLLED FLIPS the rate of the
stick's flip is scaled; with REALISTIC FLIPS the spin is rescaled so its
angular momentum is kept.

| Setting | ini key | Values | Default |
| --- | --- | --- | --- |
| LEG CONTROL (ADVANCED) | `leg_control` | off / on | off |
| LEG STIFFNESS | `leg_stiffness_pct` | 25-200% | 100% |
| LEG DAMPING | `leg_damping_pct` | 25-200% | 100% |
| LEG COUPLING | `leg_coupling_pct` | OFF, 25-100% | 50% |

The legs are placed by `body_ik` over the game's own air pose (blending in
and out over 0.2 s); the game's animation and the swing are not changed.
The diagnostic log writes `legs: engaged/disengaged` and `leg control:`
lines.

`tests/leg_control_tests.cpp` (ctest `leg_control`) covers the chord and
its 80 ms window, the release and neutral hysteresis, the poses and their
blend, the spring's stability at any frame rate, the reaction and spin
scale, REALISTIC FLIPS' kept angular momentum, and the settings.
