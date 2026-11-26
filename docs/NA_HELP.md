# Native Animation help

Generated from `include/spidy/na_help.hpp` and the settings catalog (`src/vr_settings.cpp`); in the game these pages are the NA · HELP tab. Do not edit by hand: `spidy_beta_polish_tests` checks it.

## WHAT IS NATIVE ANIMATION

Insomniac's own animations play your body on the ground; SpidyVR owns the air. Your headset always leads the view: an animation never turns it. Hand-offs between the two glide; tune them on the NA HANDOFF tab.

## TRY THE BETA

This build has every current feature on so you can try them. Topics are below. Showtime (X+B), kicks (Y+X then a flick), hand grab, tethers, point jump (A+B). Wall brake, grip fling, mid-air landings, air dodge (B), handstands, swing kick. If something feels off, post the newest reports folder files on Discord.

## JUMPS & POINT JUMP

A jumps: the game's own jump. Hold A to charge it. A + B together (or hold B, then A) charges a POINT JUMP where you look. Tap for a hop, hold up to 1 s for a leap. A grip cancels the charge. On a wall a grip no longer cancels it: A+B jumps you off the wall grab. In SpidyVR's air, B alone dodges the way the left stick points (rest: back).

## FLIPS (A TAPS)

Hold the left stick a way and tap A (under 180 ms): a flip that way. Right stick flicked down then A: backflip; flicked up then A: front flip. Works in fights and with every flip mode (VR FLIPS tab). In the air, A rights you; tap A twice to turn upside down instead. Hold the A: stay upright (one A) or upside down (two) until you let go.

## FLIP BREAK & GRIP CANCEL

In a game jump or move, tilt the left stick to break into SpidyVR's air. Or squeeze a grip: the animation stops and the body is yours at once.

## AIR KICKS

In the air hold Y + X to engage your legs, then flick the right stick fast. From under 0.35 to past 0.75 within 150 ms; a slower push poses the legs. Up: dive. Up-left/right: one-leg dive. Quarter circle to the top: roundhouse. Other flicks: a quick kick. Kicks aim where you look (AIR KICK AIM, 40 deg). An enemy up to 2.5 m past your leg: you fly at him and stop on the blow. A kick taken ticks both controllers; a lunge thumps with the rush sound. KICK TARGET RING: legs engaged, a ring turns round who a kick would hit. KICK CHAMBER: the leg winds up, snaps out through its line, folds back. KICK LEAN and KICK MOMENTUM: the chest leans in; the legs carry your travel. KICK REBOUND: a kick that lands in the air springs you up and back off him. KICK ROPE ASSIST: kick while on a web and the swing steers you at him.

## GROUND KICKS

Standing or walking (under 4.5 m/s), hold Y + X and flick the right stick: up a front kick, sideways a side kick, a half circle across the bottom a sweep. GROUND LUNGE: an enemy a step or two out of reach: you step in to kick.

## SWING KICK

While swinging, hold Y + X and flick the right stick up: a swing kick. AUTO DIVE KICK: a fast flight at the ringed enemy dives in feet first.

## CRIME RUN

Off by default. Turn CRIME RUN on under Kicks & Combat, then open the phone. The fifth app (a siren) starts a run here. A starts it; A twice in 3 s quits. Pillars mark beacons to reach and hold, or the game's own thugs to take down. Start at 20,000. Stops add points; a miss costs 2,000. 0 loses; 20 wins.

## MIDAIR POSES & LEGS

Y + X with a slow push poses the legs: tuck, starfish, dive, superhero. MIDAIR POSES and DIRECTIONAL FLIPS have their own switches (BODY & LEGS).

## PUNCHES

Punch for real. PUNCH TRACK steers a blow onto the enemy in its cone (120 deg). A punch hands the body to SpidyVR briefly; the view eases back after. Y + B when the game offers it is the game's finisher; SpidyVR waits it out.

## X: THROW & INTERACT

X sends the game's LB + RB where the game shows it: throw, pick up, rip. X and Y stay instant on the ground. Hold X for the gadget wheel elsewhere.

## WEBS & WEB CANCEL

Shoot a web on the ground or a perch to cancel the animation into SpidyVR. Not in fights, cutscenes, dodges or your own wall run. A zip cancels at once. Y + X cuts a zip. SHOOTERS: LOCKED TO ARM fires from your wrist. FULL-LENGTH SWING LINES: webs keep their length, slack hangs; the reel shortens. WRAP SLACK: round edges a line keeps its slack, hung before the first edge. SECOND HAND: the web in both hands keeps its line; climbing takes it in. PHYSICS UNTIL FADE: a let-go line swings free 1.2 s, then dissolves. A stick-click web line or tie takes the hands' webs at once (CONNECT HANDOFF). ROPE PHYSICS: hanging slack pulls you, coils fling, trailing lines drag. 100%. REEL OUT: double tap a web's reel trigger and hold: the line pays out, 4 m/s. AIR PHYSICS WHILE SWINGING: arm spin, poses, air dodge, landing blend on a web. ROPE WEIGHT: a heavier line hangs and pulls harder. 100% (was 250%). ROPE DRAG: less air on the line, less slack tug and fling; still swings. 100%. STEADY ROPE STEPS: the line steps at a fixed rate and settles, no jitter. TAUT UNDER LOAD: swing or hang on a web and its line pulls tight. KEEP LINES THROUGH WRAPS: a wrapping web keeps its line, no new one.

## HAND GRAB

HAND GRAB: a palm on an enemy or prop holds it. Heavies and bosses resist throws unless stunned or webbed up (open). Shield guards and bosses tire your hand 3x faster; an open enemy is free. GRIP TO STICK: walls no longer stick you; you slide down them with friction. Squeeze a grip with your palm on the wall to stick, then crawl as usual. 50%.

## HANDSTAND

Fall inverted onto both palms (REALISTIC FLIPS on): you balance on your hands. Lean to walk, push off to leap, let go or lean past FALL ANGLE to drop. BALANCE ASSIST steadies you; SWAY TIME is how long you may be off balance. STICKY HANDSTANDS: easier to catch (arms still up), short tracking drops held. CATCH ANGLE and TIP-IN ANGLE: from 60 deg upside down now (100 and 120 before).

## SHOWTIME

SHOWTIME: hold X + B for 200 ms to step outside Spider-Man. The right stick orbits; let go to return. WATCH ONLY keeps your hands still. SHOWTIME RAY FAN pulls the camera in before walls and trails less at speed. AUTO: FINISHERS / WEB STRIKES: out for a Y+B finisher or the game's web strike. Each auto kind (dodges too): EVERY N fires on every Nth, COOLDOWN waits. AUTO: PUNCHES (off by default): out for your own SpidyVR punch or dive.

## CAMERA & STABILIZATION

How much the animation moves your view: CAMERA MOTION, PITCH, ROLL, BOB. How smoothly: STABILIZATION and the LIGHT / HEAVY times. Lower = calmer.

## HANDOFF TIMING

LANDING BLEND, GLIDE SPEED and LONGEST, PLACE, ANGLE TIME, HEAD SEAT: how view and body glide between animation and SpidyVR. Smaller = snappier. HANDOFF HEAD FIRST: your body stays drawn until his head is read (2 frames). LANDING VIEW SNAP: a landing puts you within 15 cm of his head, then 120 ms. HEAD SEAT PULL: past 5 cm off his neck your view eases back, up to 50 cm/s.

## SETTINGS & RESET

Y RESET ALL on an NA tab resets only that tab; launcher-only rows stay. Every switch with its default: docs/NA_HELP.md and the launcher, NA marked.

## OTHER MODS

Spidy chains its hooks in front of other mods (e.g. Manhattan Skies). If the SPIDY VR tabs or prompts go missing, the log's mods: line names the mod. Remove other DLL loaders (winmm, version, dinput8) to test. See MOD_COMPAT.md.

## TROUBLESHOOTING

A 0xC0000409 at close is not a crash. A mid-play fault writes a .mdmp. Logs are in the reports folder: post the newest three files on Discord.

## Every NA setting and its default

### NA · CORE

| Setting | Default | Where |
|---|---|---|
| NATIVE ANIMATION | ON | game and launcher |
| HEAD | LEVEL | game and launcher |
| SMOOTHING | OFF | game and launcher |
| ARMS | ON | game and launcher |
| TORSO | ON | game and launcher |
| MODEL YAW | ON | game and launcher |
| HYBRID SPEED | 200 MS | game and launcher |
| MOVEMENT MODE | HYBRID | game and launcher |
| JUMP TO SPIDYVR | 250 MS (DEFAULT) | game and launcher |
| RUN SPEED | 100% | game and launcher |
| SLOW-MO LENGTH | 12 S | game and launcher |
| SOFT LOCK | 15 M | game and launcher |
| MOVE HAPTICS | 100% | game and launcher |
| ANIMATION MOVES (ADVANCED) | ON | game and launcher |
| HYBRID OWNER (ADVANCED) | ON | game and launcher |
| HEAD PROBE | OFF | game and launcher |
| SOUND TRACE | OFF | game and launcher |
| TORSO TWIST | 80 DEG | launcher |
| FALL TO SPIDYVR | 150 MS (DEFAULT) | launcher |
| AIR LANDING BLEND | ON | launcher |
| AIR LANDING RELEASE (EXPERIMENTAL) | ON | launcher |
| VIEW YAW SOURCE | STABLE | launcher |
| AIR (ADVANCED) | OFF | launcher |
| ONLY (ADVANCED) | OFF | launcher |
| STEERING (ADVANCED) | OFF | launcher |
| AIR SPIN (ADVANCED) | OFF | launcher |

### NA · CAMERA

| Setting | Default | Where |
|---|---|---|
| CAMERA MOTION | FULL | game and launcher |
| PITCH | 100% (FULL) | game and launcher |
| ROLL | 100% (FULL) | game and launcher |
| BOB UP/DOWN | 100% (FULL) | game and launcher |
| BOB SIDE | 100% (FULL) | game and launcher |
| HEAD ANGLE | LEVEL | game and launcher |
| STABILIZATION | OFF | game and launcher |
| PUNCH VIEW EASE | 250 MS (DEFAULT) | game and launcher |
| AIR VIEW EASE | 200 MS (DEFAULT) | game and launcher |
| REAR SNAP TURN | 200 MS | game and launcher |
| HEADSET FIRST | ON | launcher |
| ANIM HEAD RIDE | OFF | launcher |
| SHOULDER LIMIT | OFF | launcher |
| ANIM YAW SWAY | 20% | game and launcher |
| TURN-IN-PLACE ANGLE | 35 DEG | launcher |
| CAMERA VERTICAL | OFF | game and launcher |
| CAMERA HORIZONTAL | OFF | game and launcher |
| CAMERA ANGLE | OFF | game and launcher |
| LIGHT VERTICAL | 75 MS | game and launcher |
| LIGHT HORIZONTAL | 50 MS | game and launcher |
| LIGHT ANGLE | 75 MS | game and launcher |
| HEAVY VERTICAL | 200 MS | game and launcher |
| HEAVY HORIZONTAL | 150 MS | game and launcher |
| HEAVY ANGLE | 200 MS | game and launcher |
| STEADY VERTICAL | 200 MS | launcher |
| STEADY HORIZONTAL | 200 MS | launcher |
| STEADY ANGLE | 200 MS | launcher |
| HORIZON LEVEL | OFF | launcher |
| VR STABILIZATION | ON | launcher |

### NA · HANDOFF

| Setting | Default | Where |
|---|---|---|
| LANDING BLEND | 200 MS (DEFAULT) | game and launcher |
| HANDOFF GLIDE SPEED | 350 CM/S (DEFAULT) | game and launcher |
| HANDOFF GLIDE LONGEST | 450 MS (DEFAULT) | game and launcher |
| HANDOFF PLACE | 125 CM (DEFAULT) | game and launcher |
| HANDOFF ANGLE TIME | 200 MS (DEFAULT) | game and launcher |
| HEAD SEAT GLIDE | 165 MS (DEFAULT) | game and launcher |
| LANDING BODY EASE | 500 MS (DEFAULT) | game and launcher |

### NA · BODY & LEGS

| Setting | Default | Where |
|---|---|---|
| LEG CONTROL (ADVANCED) | OFF | game and launcher |
| LEG STIFFNESS | 100% | game and launcher |
| LEG DAMPING | 100% | game and launcher |
| LEG COUPLING | 50% | game and launcher |
| PASSIVE LEGS | 60% | game and launcher |
| PASSIVE COUPLING | 15% | game and launcher |
| AIR CROUCH | ON | game and launcher |
| REACH SPREAD | ON | game and launcher |
| MIDAIR POSES | ON | game and launcher |
| DIRECTIONAL FLIPS | ON | game and launcher |
| AIR UPRIGHT TIME | 1 S (DEFAULT) | launcher |
| HANDSTAND (EXPERIMENTAL) | ON | game and launcher |
| HANDSTAND BALANCE ASSIST | ON | game and launcher |
| HANDSTAND SWAY TIME | 500 MS (DEFAULT) | game and launcher |
| HANDSTAND FALL ANGLE | 50 DEG (DEFAULT) | launcher |
| HANDSTAND SUPPORT | 18 CM (DEFAULT) | launcher |
| HANDSTAND DAMPING | 250% (DEFAULT) | launcher |
| HANDSTAND LEAP | ON | launcher |
| HANDSTAND PUSH-OFF | 220% | launcher |
| HANDSTAND PUSH-OFF MAX | 7 M/S | launcher |
| HANDSTAND BALANCE HEIGHT | 95 CM | launcher |
| HANDSTAND BALANCE GAIN | 500% | launcher |
| HANDSTAND VIEW DROP | 90 CM | launcher |
| STICKY HANDSTANDS | ON | game and launcher |
| WALL BRAKE (EXPERIMENTAL) | ON | launcher |
| GRIP FLING (EXPERIMENTAL) | ON | launcher |
| WEB TETHERS (EXPERIMENTAL) | ON | launcher |
| TETHER POINTS | 24 (DEFAULT) | launcher |
| TETHER STEPS | 8 (DEFAULT) | launcher |

### NA · KICKS & COMBAT

| Setting | Default | Where |
|---|---|---|
| AIR KICKS (EXPERIMENTAL) | ON | game and launcher |
| GROUND KICKS | ON | game and launcher |
| AIR KICK AIM | 40 DEG (DEFAULT) | game and launcher |
| SWING KICK (EXPERIMENTAL) | ON | launcher |
| AUTO DIVE KICK | ON | launcher |
| AIR KICK SPIN | 30% (DEFAULT) | launcher |
| ENEMY JOINTS (EXPERIMENTAL) | OFF | launcher |
| ENEMY LIMB ZONES (EXPERIMENTAL) | OFF | launcher |
| PUNCH TRACK CONE | 120 DEG (DEFAULT) | game and launcher |
| PUNCH TRACK RANGE | 10 M (DEFAULT) | game and launcher |
| JUMP PULL | ON | game and launcher |
| PUNCH AUTO-TARGET | ON | launcher |
| PUNCH VIEW SNAP | OFF | launcher |
| JUMP PULL GAZE | 20 DEG (DEFAULT) | launcher |
| JUMP PULL STICK | 25 DEG (DEFAULT) | launcher |
| JUMP PULL RANGE | 12 M (DEFAULT) | launcher |
| JUMP PULL LIMIT | 1.5 M/S (DEFAULT) | launcher |
| HAND GRAB (EXPERIMENTAL) | ON | launcher |
| HAND GRAB STRENGTH | 1200 N | launcher |
| HAND GRAB HOLD COST | 1 (DEFAULT) | launcher |
| HAND GRAB CLASS RULES | ON | launcher |
| AIR DODGE | ON | game and launcher |

### NA · WEB SHOOTERS

| Setting | Default | Where |
|---|---|---|
| SHOOTERS | CONTROLLER | game and launcher |
| LOCK STRENGTH | 100% | game and launcher |
| WEB ANGLE | 0 | game and launcher |
| WEB AIM (OLD) | CONTROLLER | launcher |
| FOREARM BLEND (OLD) | 100% | launcher |
| FOREARM AIM LIMIT (RETIRED) | NO LIMIT | launcher |
| WRIST SHOOTERS (OLD) | OFF | launcher |
| PALM SHOOTERS (OLD) | OFF | launcher |
| ARM AIM FALLBACK | 60 DEG | launcher |
| FULL-LENGTH SWING LINES | ON | game and launcher |
| WEB GRIP | ON | game and launcher |
| PUNCH FISTS | ON | game and launcher |
| WEB IN HAND | ON | game and launcher |

### NA · POINT JUMP

| Setting | Default | Where |
|---|---|---|
| POINT JUMP (EXPERIMENTAL) | ON | game and launcher |
| POINT JUMP TRIGGER | A+B TOGETHER | game and launcher |
| TAP SPEED | 9 M/S | game and launcher |
| MAX SPEED | 22 M/S | game and launcher |
| CHARGE TIME | 1 S | game and launcher |
| SURFACE PUSH | 25% | game and launcher |
| POINT JUMP BULLET | ON | launcher |
| JUMP SOUNDS | ON | game and launcher |
| WALL JUMP SOUND | SOUND A (DEFAULT) | game and launcher |

### NA · SHOWTIME

| Setting | Default | Where |
|---|---|---|
| SHOWTIME (EXPERIMENTAL) | ON | game and launcher |
| WHILE OUT | WATCH ONLY (DEFAULT) | game and launcher |
| FADE TIME | 150 MS (DEFAULT) | game and launcher |
| BODY (ADVANCED) | GAME POSE, HEAD SHOWN (DEFAULT) | game and launcher |
| CHORD | HOLD X+B (DEFAULT) | game and launcher |
| AUTO: DODGES | PERFECT ONLY (DEFAULT) | game and launcher |
| AUTO: FINISHERS (LOG ONLY) | ON | game and launcher |
| DISTANCE | 2.5 M (DEFAULT) | game and launcher |
| HEIGHT | +0.3 M (DEFAULT) | game and launcher |
| ORBIT ANGLE | 35 DEGREES (DEFAULT) | game and launcher |
| ORBIT TURN | SMOOTH (DEFAULT) | game and launcher |
| FOLLOW | NORMAL (DEFAULT) | game and launcher |
| VIGNETTE | LIGHT (DEFAULT) | game and launcher |
| SLOW-MO IN SHOWTIME | ON | game and launcher |
| LATCH | OFF (DEFAULT) | game and launcher |
| SHOWTIME RAY FAN | ON | game and launcher |
