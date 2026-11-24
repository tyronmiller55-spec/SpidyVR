#pragma once
// NA HELP (beta polish M5, the owner: "thorough help information about our mod, accessible in our NA menu"): the one
// source of the help pages. In the game they are the NA · HELP tab, a row per page (the game's help column holds
// 80 characters, so a topic runs over several rows); the launcher's Native Animation tab joins a topic's pages into
// a paragraph; docs/NA_HELP.md is generated from them (beta_polish_tests diffs it). Plain ASCII on purpose.
#include <cstddef>

namespace spidy::na_help {

struct Page {
    const char* topic; // the topic this page belongs to (the first page's row title)
    const char* title; // the row title in the game
    const char* text;  // at most pageChars
};
inline constexpr size_t pageChars = 80;

inline constexpr Page pages[] = {
    {"WHAT IS NATIVE ANIMATION", "WHAT IS NATIVE ANIMATION",
     "Insomniac's own animations play your body on the ground; SpidyVR owns the air."},
    {"WHAT IS NATIVE ANIMATION", "  (2) THE VIEW", "Your headset always leads the view: an animation never turns it."},
    {"WHAT IS NATIVE ANIMATION", "  (3) HAND-OFFS", "Hand-offs between the two glide; tune them on the NA HANDOFF tab."},

    {"TRY THE BETA", "TRY THE BETA",
     "This build has every current feature on so you can try them. Topics are below."},
    {"TRY THE BETA", "  (2) WHAT TO TRY",
     "Showtime (X+B), kicks (Y+X then a flick), hand grab, tethers, point jump (A+B)."},
    {"TRY THE BETA", "  (3) ALSO TRY",
     "Wall brake, grip fling, mid-air landings, air dodge (B), handstands, swing kick."},
    {"TRY THE BETA", "  (4) WHAT TO REPORT",
     "If something feels off, post the newest reports folder files on Discord."},

    {"JUMPS & POINT JUMP", "JUMPS & POINT JUMP", "A jumps: the game's own jump. Hold A to charge it."},
    {"JUMPS & POINT JUMP", "  (2) POINT JUMP", "A + B together (or hold B, then A) charges a POINT JUMP where you look."},
    {"JUMPS & POINT JUMP", "  (3) CHARGE", "Tap for a hop, hold up to 1 s for a leap. A grip cancels the charge."},
    {"JUMPS & POINT JUMP", "  (3) ON A WALL", "On a wall a grip no longer cancels it: A+B jumps you off the wall grab."},
    {"JUMPS & POINT JUMP", "  (4) AIR DODGE", "In SpidyVR's air, B alone dodges the way the left stick points (rest: back)."},

    {"FLIPS (A TAPS)", "FLIPS (A TAPS)", "Hold the left stick a way and tap A (under 180 ms): a flip that way."},
    {"FLIPS (A TAPS)", "  (2) FRONT AND BACK", "Right stick flicked down then A: backflip; flicked up then A: front flip."},
    {"FLIPS (A TAPS)", "  (3) WHERE", "Works in fights and with every flip mode (VR FLIPS tab)."},
    {"FLIPS (A TAPS)", "  (4) UPSIDE DOWN", "In the air, A rights you; tap A twice to turn upside down instead."},
    {"FLIPS (A TAPS)", "  (5) HOLD", "Hold the A: stay upright (one A) or upside down (two) until you let go."},

    {"FLIP BREAK & GRIP CANCEL", "FLIP BREAK & GRIP CANCEL",
     "In a game jump or move, tilt the left stick to break into SpidyVR's air."},
    {"FLIP BREAK & GRIP CANCEL", "  (2) GRIP", "Or squeeze a grip: the animation stops and the body is yours at once."},

    {"AIR KICKS", "AIR KICKS", "In the air hold Y + X to engage your legs, then flick the right stick fast."},
    {"AIR KICKS", "  (2) THE FLICK", "From under 0.35 to past 0.75 within 150 ms; a slower push poses the legs."},
    {"AIR KICKS", "  (3) DIRECTIONS", "Up: dive. Up-left/right: one-leg dive. Quarter circle to the top: roundhouse."},
    {"AIR KICKS", "  (4) AIM", "Other flicks: a quick kick. Kicks aim where you look (AIR KICK AIM, 40 deg)."},
    {"AIR KICKS", "  (5) KICK LUNGE", "An enemy up to 2.5 m past your leg: you fly at him and stop on the blow."},
    {"AIR KICKS", "  (6) KICK FEEL", "A kick taken ticks both controllers; a lunge thumps with the rush sound."},
    {"AIR KICKS", "  (7) TARGET RING", "KICK TARGET RING: legs engaged, a ring turns round who a kick would hit."},
    {"AIR KICKS", "  (8) KICK CHAMBER", "KICK CHAMBER: the leg winds up, snaps out through its line, folds back."},
    {"AIR KICKS", "  (9) LEAN, MOMENTUM", "KICK LEAN and KICK MOMENTUM: the chest leans in; the legs carry your travel."},
    {"AIR KICKS", "  (10) REBOUND", "KICK REBOUND: a kick that lands in the air springs you up and back off him."},
    {"AIR KICKS", "  (11) ROPE ASSIST", "KICK ROPE ASSIST: kick while on a web and the swing steers you at him."},

    {"GROUND KICKS", "GROUND KICKS", "Standing or walking (under 4.5 m/s), hold Y + X and flick the right stick:"},
    {"GROUND KICKS", "  (2) DIRECTIONS", "up a front kick, sideways a side kick, a half circle across the bottom a sweep."},
    {"GROUND KICKS", "  (3) GROUND LUNGE", "GROUND LUNGE: an enemy a step or two out of reach: you step in to kick."},

    {"SWING KICK", "SWING KICK", "While swinging, hold Y + X and flick the right stick up: a swing kick."},
    {"SWING KICK", "  (2) AUTO DIVE KICK", "AUTO DIVE KICK: a fast flight at the ringed enemy dives in feet first."},

    {"CRIME RUN", "CRIME RUN",
     "Off by default. Turn CRIME RUN on under Kicks & Combat, then open the phone."},
    {"CRIME RUN", "  (2) START",
     "The fifth app (a siren) starts a run here. A starts it; A twice in 3 s quits."},
    {"CRIME RUN", "  (3) THE RUN",
     "Pillars mark beacons to reach and hold, or the game's own thugs to take down."},
    {"CRIME RUN", "  (4) SCORE",
     "Start at 20,000. Stops add points; a miss costs 2,000. 0 loses; 20 wins."},

    {"MIDAIR POSES & LEGS", "MIDAIR POSES & LEGS", "Y + X with a slow push poses the legs: tuck, starfish, dive, superhero."},
    {"MIDAIR POSES & LEGS", "  (2) SWITCHES", "MIDAIR POSES and DIRECTIONAL FLIPS have their own switches (BODY & LEGS)."},

    {"PUNCHES", "PUNCHES", "Punch for real. PUNCH TRACK steers a blow onto the enemy in its cone (120 deg)."},
    {"PUNCHES", "  (2) THE VIEW", "A punch hands the body to SpidyVR briefly; the view eases back after."},
    {"PUNCHES", "  (3) FINISHERS", "Y + B when the game offers it is the game's finisher; SpidyVR waits it out."},

    {"X: THROW & INTERACT", "X: THROW & INTERACT", "X sends the game's LB + RB where the game shows it: throw, pick up, rip."},
    {"X: THROW & INTERACT", "  (2) INSTANT", "X and Y stay instant on the ground. Hold X for the gadget wheel elsewhere."},

    {"WEBS & WEB CANCEL", "WEBS & WEB CANCEL", "Shoot a web on the ground or a perch to cancel the animation into SpidyVR."},
    {"WEBS & WEB CANCEL", "  (2) NOT WHEN", "Not in fights, cutscenes, dodges or your own wall run. A zip cancels at once."},
    {"WEBS & WEB CANCEL", "  (3) SHOOTERS", "Y + X cuts a zip. SHOOTERS: LOCKED TO ARM fires from your wrist."},
    {"WEBS & WEB CANCEL", "  (4) SWING LINES", "FULL-LENGTH SWING LINES: webs keep their length, slack hangs; the reel shortens."},
    {"WEBS & WEB CANCEL", "  (5) WRAP SLACK", "WRAP SLACK: round edges a line keeps its slack, hung before the first edge."},
    {"WEBS & WEB CANCEL", "  (6) SECOND HAND", "SECOND HAND: the web in both hands keeps its line; climbing takes it in."},
    {"WEBS & WEB CANCEL", "  (7) UNTIL FADE", "PHYSICS UNTIL FADE: a let-go line swings free 1.2 s, then dissolves."},
    {"WEBS & WEB CANCEL", "  (8) CONNECT", "A stick-click web line or tie takes the hands' webs at once (CONNECT HANDOFF)."},
    {"WEBS & WEB CANCEL", "  (9) ROPE PHYSICS", "ROPE PHYSICS: hanging slack pulls you, coils fling, trailing lines drag. 100%."},
    {"WEBS & WEB CANCEL", "  (10) REEL OUT", "REEL OUT: double tap a web's reel trigger and hold: the line pays out, 4 m/s."},
    {"WEBS & WEB CANCEL", "  (11) AIR PHYSICS", "AIR PHYSICS WHILE SWINGING: arm spin, poses, air dodge, landing blend on a web."},
    {"WEBS & WEB CANCEL", "  (12) ROPE WEIGHT", "ROPE WEIGHT: a heavier line hangs and pulls harder. 100% (was 250%)."},
    {"WEBS & WEB CANCEL", "  (13) ROPE DRAG", "ROPE DRAG: less air on the line, less slack tug and fling; still swings. 100%."},
    {"WEBS & WEB CANCEL", "  (14) STEADY STEPS", "STEADY ROPE STEPS: the line steps at a fixed rate and settles, no jitter."},
    {"WEBS & WEB CANCEL", "  (15) TAUT UNDER LOAD", "TAUT UNDER LOAD: swing or hang on a web and its line pulls tight."},
    {"WEBS & WEB CANCEL", "  (16) WRAPS", "KEEP LINES THROUGH WRAPS: a wrapping web keeps its line, no new one."},

    {"HAND GRAB", "HAND GRAB", "HAND GRAB: a palm on an enemy or prop holds it."},
    {"HAND GRAB", "  (2) HEAVIES", "Heavies and bosses resist throws unless stunned or webbed up (open)."},
    {"HAND GRAB", "  (3) TIRING", "Shield guards and bosses tire your hand 3x faster; an open enemy is free."},
    {"HAND GRAB", "  (4) GRIP TO STICK", "GRIP TO STICK: walls no longer stick you; you slide down them with friction."},
    {"HAND GRAB", "  (5) STICK, CRAWL", "Squeeze a grip with your palm on the wall to stick, then crawl as usual. 50%."},

    {"HANDSTAND", "HANDSTAND", "Fall inverted onto both palms (REALISTIC FLIPS on): you balance on your hands."},
    {"HANDSTAND", "  (2) MOVING", "Lean to walk, push off to leap, let go or lean past FALL ANGLE to drop."},
    {"HANDSTAND", "  (3) TUNING", "BALANCE ASSIST steadies you; SWAY TIME is how long you may be off balance."},
    {"HANDSTAND", "  (4) STICKY", "STICKY HANDSTANDS: easier to catch (arms still up), short tracking drops held."},
    {"HANDSTAND", "  (5) ANGLES", "CATCH ANGLE and TIP-IN ANGLE: from 60 deg upside down now (100 and 120 before)."},

    {"SHOWTIME", "SHOWTIME", "SHOWTIME: hold X + B for 200 ms to step outside Spider-Man."},
    {"SHOWTIME", "  (2) ORBIT", "The right stick orbits; let go to return. WATCH ONLY keeps your hands still."},
    {"SHOWTIME", "  (3) WALLS", "SHOWTIME RAY FAN pulls the camera in before walls and trails less at speed."},
    {"SHOWTIME", "  (4) AUTO", "AUTO: FINISHERS / WEB STRIKES: out for a Y+B finisher or the game's web strike."},
    {"SHOWTIME", "  (5) HOW OFTEN", "Each auto kind (dodges too): EVERY N fires on every Nth, COOLDOWN waits."},
    {"SHOWTIME", "  (6) PUNCHES", "AUTO: PUNCHES (off by default): out for your own SpidyVR punch or dive."},

    {"CAMERA & STABILIZATION", "CAMERA & STABILIZATION", "How much the animation moves your view: CAMERA MOTION, PITCH, ROLL, BOB."},
    {"CAMERA & STABILIZATION", "  (2) SMOOTHING", "How smoothly: STABILIZATION and the LIGHT / HEAVY times. Lower = calmer."},

    {"HANDOFF TIMING", "HANDOFF TIMING", "LANDING BLEND, GLIDE SPEED and LONGEST, PLACE, ANGLE TIME, HEAD SEAT:"},
    {"HANDOFF TIMING", "  (2) WHAT THEY DO", "how view and body glide between animation and SpidyVR. Smaller = snappier."},
    {"HANDOFF TIMING", "  (3) HEAD FIRST", "HANDOFF HEAD FIRST: your body stays drawn until his head is read (2 frames)."},
    {"HANDOFF TIMING", "  (4) LANDING SNAP", "LANDING VIEW SNAP: a landing puts you within 15 cm of his head, then 120 ms."},
    {"HANDOFF TIMING", "  (5) NECK PULL", "HEAD SEAT PULL: past 5 cm off his neck your view eases back, up to 50 cm/s."},

    {"SETTINGS & RESET", "SETTINGS & RESET", "Y RESET ALL on an NA tab resets only that tab; launcher-only rows stay."},
    {"SETTINGS & RESET", "  (2) EVERY SETTING", "Every switch with its default: docs/NA_HELP.md and the launcher, NA marked."},

    {"OTHER MODS", "OTHER MODS", "Spidy chains its hooks in front of other mods (e.g. Manhattan Skies)."},
    {"OTHER MODS", "  (2) MISSING TABS", "If the SPIDY VR tabs or prompts go missing, the log's mods: line names the mod."},
    {"OTHER MODS", "  (3) TESTING", "Remove other DLL loaders (winmm, version, dinput8) to test. See MOD_COMPAT.md."},

    {"TROUBLESHOOTING", "TROUBLESHOOTING", "A 0xC0000409 at close is not a crash. A mid-play fault writes a .mdmp."},
    {"TROUBLESHOOTING", "  (2) LOGS", "Logs are in the reports folder: post the newest three files on Discord."},
};
inline constexpr size_t pageCount = sizeof(pages) / sizeof(pages[0]);

} // namespace spidy::na_help
