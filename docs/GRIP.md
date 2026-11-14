# Grip across the game: wall brake and grip fling (G1)

Both are EXPERIMENTAL and off by default. Turn them on in `spidy_vr.ini` (`wall_brake=1`, `grip_fling=1`) or with **Wall brake (experimental)** and **Grip fling (experimental)** on the launcher's Walls page (VR MOVEMENT in the game's own menu). They use the controller's **physical grip** (not the web button mapping), the same squeeze the hand climb uses: closed past 0.65, held until it opens under 0.35.

## Wall brake (`wall_brake`)

On Spidy's wall with HAND WALL CLIMBING on, squeezing a grip with your palm at the wall (the hand climb's own contact test: palm within 18 cm of the face and a ray-confirmed wall, pointing at it) while you slide along it brakes the slide as friction would, instead of stopping you at once.

    a = min(a_max, mu * (g + k * pressed))     mu 0.9, k 40 m/s^2 per m, a_max 14 m/s^2 per hand

`pressed` is how far the tracked palm is past the wall's face (0 to 15 cm): pushing the hand into the wall grips harder. One hand stops 10 m/s in about 1.2 s, two hands in about 0.6 s. Under 0.5 m/s the brake ends and it is the hand-climb hold as before. The brake only starts when the body is sliding at 0.5 m/s or faster as the first hand closes; with the other hand already holding, the body moves at the hand's own speed and nothing brakes it.

Feel: grain pulses in the braking hands, 30 Hz (slow) to 160 Hz (12 m/s and over) by the slide speed, strong by the braking force; MOVE HAPTICS scales it.

`wall_brake_mu_pct` (90, 30 to 150) is the friction, in `spidy_vr.ini` only.

## Grip fling (`grip_fling`)

Close a grip with the palm at a surface, pull your hand, and let go: you are launched the other way.

    v_out = min(v_max, gain * |mean hand velocity|)     gain 1.3, v_max 9 m/s

- The hand velocity is the hand's movement relative to your head (tracking space turned into the world), averaged over the last 60 ms before the release. The launch goes the opposite way.
- A hand under 0.5 m/s flings nothing.
- The history clears on a snap turn (a view turn over 7 degrees in a frame), a tracking loss, a frame over 100 ms, a hand speed over 15 m/s (a tracking jump) and every new grip.
- A grip counts when the palm is within 12 cm of a fixed surface it faces (a ray from the palm along the hand's aim) as the grip closes: a wall, the ground, a ledge, anything. The surface is not needed afterwards.
- With the other hand still holding on (on the wall or any surface), letting go is a stroke, not a throw: no fling (hand over hand on a wall).
- A palm on a surface does not shoot a web or start a grab with that grip (the same rule as the hand climb's).
- **No energy from nothing:** the launch is at most `v_max`. In the air it adds to your velocity, but the result never goes past the larger of your speed before and `v_max`. The velocity limit of the swing still applies. A fling velocity feeds the dive kick's 9 m/s gate like any other speed.

Where it works:

| Where | What the fling does |
| --- | --- |
| Spidy's wall | the part along the wall is the launch (a stroke coasts on and the wall's own brake slows it); a push out of the wall of 2 m/s or more leaves the wall |
| The ground | launch away with at least a 2.5 m/s hop so you leave the floor (a hand pulled up cannot push you into it); the game's jump takes off as a point jump does |
| The air | the velocity added; with a web held in the other hand the rope still limits you |

`grip_fling_gain_pct` (130, 50 to 300) and `grip_fling_max` (9 m/s, 3 to 20) are in `spidy_vr.ini` only.

From the ground this is an approved ownership exception (docs/OWNERSHIP.md), like the point jump's: the launch is the swing's, and VR keeps the body through the takeoff (`Frame::pointJumpHold`, at most 400 ms).

## How it is wired

`include/spidy/grip.hpp` is the portable math (brake, history, fling, the grip state machine). `Swing::gripInputs` feeds it each input sample, `Swing::wallMotion` applies the brake, and the step applies a queued fling next to the point jump's. XR only sends the settings (`SpidySwingGrip`, `game_swing::GripAsk`, again every second) and plays the feel from `SpidySwingGripLive` (`game_swing::GripLive`). An older swing module has neither export: the grip is as it was. `game_swing::Settings` is unchanged. No game address is used.

Log lines (diag): `grip: settings sent`, `grip: brake hands= v= a=`, `grip: fling v= (x, y, z) from the ground|wall|air (flings ... slow pulls ... refused ...)`.

## Not in G1

- Brake in free air (no wall hold): the brake needs Spidy's wall; a body falling past a wall that has not taken it is not slowed. Needs headset time to tune the wall's take-over first.
- The palm pinned to the wall and the glove's short web smear (feedback visuals): they need the arm solver's hand pose and are not done.
- Enemy and prop grips take the grip before a surface does (Pillar 2's hand grab for props): that is G2.
- The open-palm push-off reuse of the handstand's push gain.
