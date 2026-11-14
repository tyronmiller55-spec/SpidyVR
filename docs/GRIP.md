# Grip across the game: wall brake, grip fling (G1) and hand grab (G2)

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
- Enemy and prop grips take the grip before a surface does (Pillar 2's hand grab for props): that is G2, below.
- The open-palm push-off reuse of the handstand's push gain.

# Hand grab (G2, `hand_grab`)

**Experimental, off by default** (`hand_grab=0`; the launcher's Walls page, "Hand grab (experimental)"). The plan's Pillar 4: take an enemy or a prop by hand, hold it, punch it, throw it or slam it.

## Taking hold

Squeeze the physical grip (the controller's own grip, which is not the web button when the player swapped them) with the palm within **0.35 m** of an up enemy's capsule or a prop's surface. By distance, the grip goes to:

1. an enemy within 0.35 m (the nearer first),
2. a prop within 0.35 m,
3. a wall within 12 cm with the palm facing it (G1, wall brake and grip fling),
4. a web, as before.

A squeeze that begins elsewhere takes nothing later; let go and squeeze again. A hand that holds a body shoots no web, grips no wall, and the swing sees neither of its grips. Every enemy a hand can take passes the filter the webs use: only an enemy who is up and free to fight, not one the game holds (a car's seat, a cinematic, a takedown), movable and still registered (`game_targets::fighting`, `movable`, `live`). The game's own held-state flags are what gate it, so nothing in a scene is pulled out of it.

## Holding

The held body follows a point ahead of the palm along the hand's aim: **0.45 m** for an enemy (his chest there), a prop's radius and 15 cm. The law is the critically damped spring

    a = w^2 (x_hand - x) - 2 w (v - v_hand),   w = 18 rad/s,   F = m a,   |F| <= 1200 N per hand

carried by the web grab's Follow command, the law the web tie uses: a rate-limited velocity servo (the error closes at w / 2 per metre, no faster than the grip could stop it in, so there is no overshoot) with the gravity of the step taken out first, so the hand supports the weight. Where the force cannot beat the weight the body sags: the grip's real limit.

| Body | One hand | Two hands |
| --- | --- | --- |
| a prop (the game gives every prop 30 kg) | lifted, rides ahead of the palm | the same, firmer |
| a street thug (80 kg from his capsule) | lifted (1200 N / 80 kg = 15 m/s^2 against 9.8) | the same |
| a heavy (a wide, tall capsule: 180 kg and up) | cannot be lifted: dragged along the ground, sags | lifted |

A second hand squeezed on the same body joins the first and doubles the force; the pair holds him at the middle of both places. One hand letting go leaves him in the other; both letting go together throw him once.

An enemy is knocked into the game's own flight by a light kinetic blow (0.25, the web tie's) and the Follow law then writes the flight's velocity every step; once the game ends the flight he is knocked again **0.8 s after the last blow**, a punch's included. One the game will not fling (a heavy, a scripted scene) is steered on his mover, as the web tie's. An enemy who goes down in the hold (knocked out) is let go of after 1.5 s. A body carried more than 1.5 m from its place, or shaken off, is lost. While held he is not struck against what the hand holds him by (only a thrown body is).

Hold cost (`enemy_hold_cost`, 1; 0 never tires): holding an enemy tires the hand a little, the force fading from 100% toward 75% over 60 s x (80 kg / his mass)^2 / cost. A standard enemy costs next to nothing (75% of 1200 N still lifts his 80 kg); a heavy tires it fast, and a tired grip that no longer beats his weight lets him sag away. Props cost nothing. `hand_grab_force` (1200, 400 to 4000 N) sets one hand's force; what one hand lifts follows it (90 kg at 1200 N).

## Punching while held

The free fist is the ordinary punch (a blow of kMelee, 2.2 to 7.5 m/s); the game's punch already goes to a held enemy, and the holding hand's own fist does not punch. A punch resets the 0.8 s wait before the next knock.

## Throw and slam

Letting go of the squeeze launches the body at the hand's velocity, the **least-squares slope of the last five hand samples** (about 55 ms), times up to **1.6** (less for a body heavier than one hand lifts: 1 + 0.6 x lift mass / mass), at most **18 m/s**. Slower than 1.5 m/s it is only let go of and keeps what it has. A hand moving **down faster than 4 m/s** is a slam: the launch is at least **8 m/s** along its way (so the landing is a strike: `StrikeConfig::landSpeed`, 8), still capped at 18. A thrown enemy hurts what he strikes and lands as a thrown one does (the web grab's own strike rules).

## How it is wired

`include/spidy/hand_grab.hpp` is the portable math (reach, mass, hold cost, spring, hold velocity, least-squares samples, release, slam). `WebGrab` (`web_grab.cpp`) takes it: `claim` starts a hand grab before any web (`GrabConfig::hand`, off until enabled), `step` writes the Follow command (`TargetCommand::carried` for an enemy), `letGoHand` throws. `game_grab.cpp` supplies the bodies near the palm (`TargetQueries::within`) and carries the held enemy (`carry`). XR sends the settings (`SpidySwingHandGrab`, `game_swing::HandGrabAsk`, again every second) and logs and plays the feel from `SpidySwingHandGrabLive` (`game_swing::HandGrabLive`); an older swing module has neither export: nothing is taken by hand. `game_swing::Settings` (v40, 188 bytes) is unchanged. No game address is used: only the web grab's existing, verified ones (the kinetic damage request, BotStateFlung, `game_targets`).

Log lines (diag): `hand grab: settings sent`, `hand grab: <enemy|prop> mass=<kg> hold|throw|slam|drop v=<m/s> (taken ... throws ... slams ... let go ... lost ... knocks ...)`.

## Not in G2 (gaps)

- **Finishers and class rules.** The plan's "heavy and brute enemies are hard to throw unless open for a finisher; a finisher opens every enemy" and "superpowered or armored cost more" need the game's finisher-open state and enemy class (Brute, Heavy, armored, superpowered). No verified flag or address for them exists (docs/REFERENCE.md names the ACTIVE_FINISHER action only), so none is invented. Weight is the capsule's mass: a bigger capsule is heavier, harder to lift, throws with less of the gain, and tires the hand faster. Weapons, heads and limbs follow the game's existing rules because nothing here touches them.
- **Real prop mass.** The adapter gives every prop 30 kg (the web grab's flat figure); a 300 kg dumpster is not told apart. The hold law handles any mass (tested to 360 kg), so real prop masses need only a verified mass source.
- **A held enemy's pose.** The game's flight carries him; no grabbed animation is played (no verified animation id).
- **The look.** No web is drawn for a hand-held body (a held prop or enemy rides at the palm); no glove effects.
- Not tested in a headset. See the risks in the changelog.
