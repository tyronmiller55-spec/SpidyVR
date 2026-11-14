# Physical web tethers (W1)

Off by default, EXPERIMENTAL. `physical_tethers=1` in `spidy_vr.ini` (or **Web tethers (experimental)** on the launcher's Walls page, with **Tether points** and **Tether steps** under it) turns it on. It is the plan's Pillar 3, milestone W1 (XPBD tether core, the game's rope drawing, a throw on a tether, whip and spin). It builds on the web grab (`web_grab.hpp`, G1/G2): a web on a prop or an enemy, per hand. It changes nothing while off.

## Controls

| You do | What happens |
| --- | --- |
| Shoot a web at a prop or an enemy (grip, as always) | The line is a simulated rope: it sags and drapes, and is drawn through the game's own web. |
| **Whip**: with a taut web, snap the hand back along the line (about 4 m/s or more, relative to you) | The wave takes the slack up and jerks the target toward you, at the wave's speed times 0.9, at most 14 m/s, less for a heavy one (the web's force over its mass, for 0.15 s). 0.4 s between whips. A slow pull does nothing; the pull gesture still yanks. |
| **Spin**: circle the hand (3 rad/s or more, over 1 m/s, for a third of a second) | The target orbits you about the circle's axis at the hand's own rate, built up over half a second, at most 18 m/s, held at its distance (the web's length at most). |
| **Sling**: let go of the grip while it spins | It leaves by the web grab's throw (its velocity relative to you, times the throw multiplier, at most 25 m/s, aim-assisted at a character in the way). |
| **Throw on a tether**: let go of the grip with the trigger held (0.1 or more) after you hold it at your hand (reeled in and caught) | It is thrown as before, but the line is kept and pays out after it. The trigger is the brake: light, it flies nearly free; deeper, a friction holds it back; at 0.95 or more the length locks and it swings or yo-yos on the line. |
| Let go of the trigger (under 0.1) | The line is cut. |
| Pull the hand sharply (the yank), or squeeze the grip again | The kept line yanks the prop back (and ends when it is caught); a new grip press lets the old line go and shoots a new web. |

The grip is the web button and the trigger reels, as before; a kept line has no reel (the trigger is its brake). Both hands have their own line.

## How it works

- **The rope** (`include/spidy/tether.hpp`): 24 particles, 8 substeps a frame (Small Steps XPBD). A tension-only distance constraint per segment (a rope pulls, it does not push), compliance 1e-7 m/N, a long-range attachment to the hand and the target so it never stretches like rubber, the hand end pinned to the tracked wrist and the far end to the target each substep. It sleeps after half a second of stillness and wakes on a move of 1 cm.
- **Collision** is cheap: up to 8 rays a step (the segments and each particle's travel in turn) against the world's fixed surfaces cache planes (12, 0.6 s), which the particles stay off by the line's thickness (3 cm); an enemy near the line is a capsule it goes round. The plane faces the end of the line nearer the hit, so a particle that slipped through is brought back.
- **The whip's wave** is the line's mean velocity along the chord toward the hand, relative to the target, over the last 0.15 s, counted when the line goes taut; it needs the hand's own speed (relative to you) over 4 m/s in the same window, so swinging along with a prop on a web never whips.
- **The spin** is the signed turn of the hand's velocity direction (relative to you) over time; it keeps its axis (within 60 degrees) for 0.3 s, and lets go 0.15 s after the turn stops.
- **Drawing.** The game's own rope can only be laid with up to 8 points (more crashed it, C0's findings), and its drape is canned (a straight line), so the mod draws the simulated rope through it: 24 particles are reduced to at most 6 bends by greedy Douglas-Peucker (the points that bend it most, within 2 cm), laid as the game rope's hand points (`native_webs::Bent`, the verified hook the swing's edges already use). Where the hook is missing (`native_webs::drawsHeld()` false) the line stays the straight web it was; the simulation, whip, spin and throw still work.
- **Side channel** (as G1/K1/G2/K2): XR sends `physical_tethers`, `tether_particles` and `tether_substeps` to the swing module (`SpidyTether`, a 32-byte `TetherAsk`) whenever they change and every second; the swing fills `SpidyTetherLive` (`TetherLive`, 240 bytes: each hand's bends, end and tension, and the account). `game_swing::Settings` stays v40, 188 bytes; no new game address.

## Where it lives

| Piece | File |
| --- | --- |
| The rope, drawn points, whip, spin, payout | `include/spidy/tether.hpp` |
| Per-hand ropes, the spin and whip laws, the throw that keeps its line | `web_grab.cpp` (`WebGrab::tether`, `tetherView`, `tetherReport`) |
| The swing's settings and account | `game_grab.cpp` (`tetherSettings`, `tetherData`), `game_swing.cpp` (`SpidyTether`, `SpidyTetherLive`) |
| Settings sent, log, drawing, haptics | `game_xr.cpp` |
| Settings | `physical_tethers`, `tether_particles` (24), `tether_substeps` (8) in `vr_settings`; rows on the launcher's Walls page |
| Tests | `tests/tether_tests.cpp`, a settings and ini check in `tests/core_tests.cpp` |

## Log lines

`tethers: settings sent (on|off, N points, N steps)`; `tethers: <whip|spin|sling|throw on a tether|cut> v=<m/s> (whips, spins, slings, kept throws, cuts)`; the hands thump by the speed of a whip, spin, sling or kept throw.

## Not built (needs a verified structure or a headset first)

- **The bola** (two props on one line) and **whips through a crowd**: the line is one web on one target. Enemy capsules only push the line; they are not struck by it.
- **Game-side rope physics**: the game's rope is drawn, not driven. No new address or RE; the plan's "more laid points" is bounded by the 8-point crash (6 bends past the two ends).
- **A strike from a thrown prop on a tether.** The kept throw is a Launch that is not a "thrown" command, so the game does not count the prop's flight as a strike until the line ends (the same as a yank).
- **Webs round corners.** The web grab still cuts a web a wall stays between for 0.3 s; the rope drapes over an edge, it does not hold the web through a wall.

## Unverified (needs a headset)

- Whether whip and spin feel right: the thresholds (4 m/s, 3 rad/s) and gains are authored from the plan, not measured.
- The rope updates at the game's physics rate (about 30 Hz), so a fast hand shows a stepped line; the collision lags up to 100 ms (the cached planes).
- Whether the game's rope shows 6 bends cleanly with its material, and whether it flickers when the number of bends changes.
- The targets' masses are G2's guesses, so a whip on a heavy prop may be weaker or stronger than it feels.
