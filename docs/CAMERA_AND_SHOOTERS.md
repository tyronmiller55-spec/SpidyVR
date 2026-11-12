# Camera & Stabilization, Web Shooters, Legs (Advanced)

Branch `feat/stabilization-wrist-lock` (from d0584e7). The user, Nov 11 22:59 PST: "Native Animation is moving
too much for players, and leveling steals all of the feeling ... my model forearm wrist shooters aren't working
at all ... There are too many wrist settings that constantly mess with each other." 23:02: "a setting for head
angle, from below to above the horizon". 23:07: "I can't find legs or camera settings in any of the menus."

**Headset status (Nov 12):** in the published build 0a593c0, tested in the user's 37-minute session from 03:49 PST with CAMERA MOTION at pitch 75%, roll 50%, bob 75%/75%, HEAD ANGLE LEVEL, LEG CONTROL with CONTROLLED and REALISTIC FLIPS. The legs are now engaged by holding **Y+X** (LEG HOLD, see [LEG_HOLD.md](LEG_HOLD.md)); there is no thumb rest control and no LEG TOGGLE setting.

## Where things are now

In game: Settings > SPIDY VR has three new tabs at the end. In the launcher: Options has three new pages with the
same names, and the Native Animation tab has a Camera & stabilization card.

| In game tab | Launcher page | Rows |
|---|---|---|
| VR CAMERA & STABILIZATION | Camera & Stabilization | CAMERA MOTION, PITCH, ROLL, BOB UP/DOWN, BOB SIDE, HEAD ANGLE, STABILIZATION and its 9 response rows (moved from VR DISPLAY) |
| VR WEB SHOOTERS | Web Shooters | SHOOTERS, LOCK STRENGTH, WEB ANGLE (moved from VR WEBS) |
| VR LEGS (ADVANCED) | Legs (Advanced) | LEG CONTROL (ADVANCED), LEG STIFFNESS, LEG DAMPING, LEG COUPLING (moved from VR FLIPS) |

LEGS rows are never hidden or greyed: they say what they need ("Needs CONTROLLED or REALISTIC FLIPS. ON here turns
CONTROLLED FLIPS on."). Since 23:35 LEG CONTROL works with REALISTIC FLIPS too: the legs' spread multiplies the arms'
pose inertia inside RealisticFlip (the spin is scaled by old/new inertia, so angular momentum is kept: a jackknife
spins faster, extending slows it), and the legs' swing turns the body back once per frame without adding spin (zero
net angular momentum), on the same axes as CONTROLLED FLIPS (`legTurnVector`). Handstands (future) need CONTROLLED
FLIPS.

## Camera motion (how much) and stabilization (how smoothly)

| Row | Choices | Default | spidy_vr.ini |
|---|---|---|---|
| CAMERA MOTION | FULL, LIGHT (75/50/75/75), HEAVY (50/25/50/50), COMFORT (25/0/25/25), CUSTOM | FULL | (none: fills the four below) |
| PITCH | 0% (STILL), 15, 25, 35, 50, 65, 75, 90, 100% (FULL) | 100% | `camera_pitch_pct` (0..100) |
| ROLL | same | 100% | `camera_roll_pct` |
| BOB UP/DOWN | same | 100% | `camera_bob_vertical_pct` |
| BOB SIDE | same | 100% | `camera_bob_side_pct` |
| HEAD ANGLE | DOWN 30, 20, 10, 5, LEVEL, UP 5, 10, 20, 30 | LEVEL | `camera_head_angle_deg` (-30..30) |

All personal (not in shared profiles), like the existing camera smoothing rows. The defaults are the identity:
the head matrix is returned bit for bit, so the current feel is unchanged until a player moves a row.

How it works (`include/spidy/camera_motion.hpp`): only while the animated head carries the view (HEAD FULL or
LEVEL). The animated head's orientation is split into heading (kept, so you still turn with Spider-Man) and tilt;
the tilt's pitch and roll are scaled separately, HEAD ANGLE is added to the pitch, and the heading is held where
the animation looks. Past 100 degrees of tilt (a flip, hanging upside down) the scaling fades to full by 170 so a
flip turns over smoothly. The bob is the head's offset from the feet around a 0.5 s baseline, scaled per axis
(at most 15 cm up/down and 20 cm sideways removed; a jump of 1.5 m is a cut). Applied before the existing
smoothing, on the render thread (`stereo_probe` resolveScene) and the XR thread (animated head, hands' anchor).
Your headset moves the view 1:1 on top, always. The levelling and HEAD ANGLE work about a supplied up (`Amounts::up`, world
up today), so a future inverted view (handstand) supplies its own up and the same code holds; HEAD ANGLE is added
once per head (render thread and XR thread each stabilize their own head, never one on top of the other). HEAD LEVEL's heading latch and the midair soft lock are as they
were; at 0% PITCH and ROLL with HEAD FULL you get a level view that still follows the animation's heading.

## Web shooters: why model forearm "did not work at all"

Evidence: d0584e7 session log, Nov 11 22:38 PST, with `wrist_shooters=1 web_aim_source=1 forearm_blend=100
forearm_aim_limit=35 palm_shooters=0`, Native Animation with ARMS IN ANIMATION. 2,972 of 5,768 `spherical aim:`
lines were `arms overlay` with trust 0.00: in Native Animation the code refused the visible arm
(`else if (armView.valid) why = Fallback::armsOverlay`), and learning was off while the arm was shown, so the web
used a learned forearm from before (median 32 degrees off the controller, 22 degrees off the arm you saw) or, in
175, the controller. On top, the line was clamped to 15 degrees inward (`forearm aim: ... limit 15`, active turns
median 33.5 degrees).

Fix: LOCKED TO ARM uses the arm you see (SpidyVR's arm, or the arm drawn on the animation once its overlay is
in), learns in animation too, falls back to the learned forearm when the arm cannot be read (the controller only
before one is learned), and has no inward cap. A log line every second says which: `shooter lock: LOCKED TO ARM visible arm, ...,
strength 100%, line off controller N deg`.

## Old wrist settings and how they fought

| Old row (key) | What it did | Conflict |
|---|---|---|
| WRIST SHOOTERS (`wrist_shooters`) | forearm line at 100% | ignored BLEND; marked redundant when WEB AIM was MODEL FOREARM, but still on |
| WEB AIM (`web_aim_source`) | CONTROLLER or MODEL FOREARM | MODEL FOREARM used BLEND, WRIST SHOOTERS didn't |
| FOREARM BLEND (`forearm_blend`) | 25..100% of the forearm | only with WEB AIM = MODEL FOREARM |
| FOREARM AIM LIMIT (`forearm_aim_limit`) | meant as a cap | not used: the code passed 180 and a fixed 15 degree inward clamp applied instead |
| PALM SHOOTERS (`palm_shooters`) | quarter turn out of the palm | turned on top of either forearm mode |

## Old to new (no file changes needed)

SHOOTERS and LOCK STRENGTH are views over the old keys; nothing is migrated and an older build reads the same file.

| Old keys | SHOOTERS | LOCK STRENGTH |
|---|---|---|
| palm_shooters=1 (anything else) | PALM | - |
| wrist_shooters=1, web_aim_source=0 | LOCKED TO ARM | 100% |
| web_aim_source=1 (wrist either) | LOCKED TO ARM | forearm_blend |
| all off | CONTROLLER | - |

Choosing writes: CONTROLLER `wrist_shooters=0 web_aim_source=0 palm_shooters=0`; LOCKED TO ARM `wrist_shooters=1
web_aim_source=1 palm_shooters=0`; PALM `palm_shooters=1`, the others 0. LOCK STRENGTH writes `forearm_blend`.
`forearm_aim_limit` is kept and ignored. PALM no longer stacks on the arm lock. The old rows are off the in-game
tab and the launcher (titled "(OLD)" in the settings table).
