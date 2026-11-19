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
     "Wall brake, grip fling, mid-air landings, a handstand and a swing kick."},
    {"TRY THE BETA", "  (4) WHAT TO REPORT",
     "If something feels off, post the newest reports folder files on Discord."},

    {"JUMPS & POINT JUMP", "JUMPS & POINT JUMP", "A jumps: the game's own jump. Hold A to charge it."},
    {"JUMPS & POINT JUMP", "  (2) POINT JUMP", "A + B together (or hold B, then A) charges a POINT JUMP where you look."},
    {"JUMPS & POINT JUMP", "  (3) CHARGE", "Tap for a hop, hold up to 1 s for a leap. A grip cancels the charge."},

    {"FLIPS (A TAPS)", "FLIPS (A TAPS)", "Hold the left stick a way and tap A (under 180 ms): a flip that way."},
    {"FLIPS (A TAPS)", "  (2) FRONT AND BACK", "Right stick flicked down then A: backflip; flicked up then A: front flip."},
    {"FLIPS (A TAPS)", "  (3) WHERE", "Works in fights and with every flip mode (VR FLIPS tab)."},

    {"FLIP BREAK & GRIP CANCEL", "FLIP BREAK & GRIP CANCEL",
     "In a game jump or move, tilt the left stick to break into SpidyVR's air."},
    {"FLIP BREAK & GRIP CANCEL", "  (2) GRIP", "Or squeeze a grip: the animation stops and the body is yours at once."},

    {"AIR KICKS", "AIR KICKS", "In the air hold Y + X to engage your legs, then flick the right stick fast."},
    {"AIR KICKS", "  (2) THE FLICK", "From under 0.35 to past 0.75 within 150 ms; a slower push poses the legs."},
    {"AIR KICKS", "  (3) DIRECTIONS", "Up: dive. Up-left/right: one-leg dive. Quarter circle to the top: roundhouse."},
    {"AIR KICKS", "  (4) AIM", "Other flicks: a quick kick. Kicks aim where you look (AIR KICK AIM, 40 deg)."},

    {"GROUND KICKS", "GROUND KICKS", "Standing or walking (under 3 m/s), hold Y + X and flick the right stick:"},
    {"GROUND KICKS", "  (2) DIRECTIONS", "up a front kick, sideways a side kick, a half circle across the bottom a sweep."},

    {"SWING KICK", "SWING KICK", "While swinging, hold Y + X and flick the right stick up: a swing kick."},
    {"SWING KICK", "  (2) AUTO DIVE KICK", "AUTO DIVE KICK: a fast flight at the ringed enemy dives in feet first."},

    {"MIDAIR POSES & LEGS", "MIDAIR POSES & LEGS", "Y + X with a slow push poses the legs: tuck, starfish, dive, superhero."},
    {"MIDAIR POSES & LEGS", "  (2) SWITCHES", "MIDAIR POSES and DIRECTIONAL FLIPS have their own switches (BODY & LEGS)."},

    {"PUNCHES", "PUNCHES", "Punch for real. PUNCH TRACK steers a blow onto the enemy in its cone (120 deg)."},
    {"PUNCHES", "  (2) THE VIEW", "A punch hands the body to SpidyVR briefly; the view eases back after."},

    {"X: THROW & INTERACT", "X: THROW & INTERACT", "X sends the game's LB + RB where the game shows it: throw, pick up, rip."},
    {"X: THROW & INTERACT", "  (2) INSTANT", "X and Y stay instant on the ground. Hold X for the gadget wheel elsewhere."},

    {"WEBS & WEB CANCEL", "WEBS & WEB CANCEL", "Shoot a web on the ground or a perch to cancel the animation into SpidyVR."},
    {"WEBS & WEB CANCEL", "  (2) NOT WHEN", "Not in fights, cutscenes, dodges or your own wall run. A zip cancels at once."},
    {"WEBS & WEB CANCEL", "  (3) SHOOTERS", "Y + X cuts a zip. SHOOTERS: LOCKED TO ARM fires from your wrist."},

    {"HAND GRAB", "HAND GRAB", "HAND GRAB: a palm on an enemy or prop holds it."},
    {"HAND GRAB", "  (2) HEAVIES", "Heavies and bosses resist throws unless stunned or webbed up (open)."},
    {"HAND GRAB", "  (3) TIRING", "Shield guards and bosses tire your hand 3x faster; an open enemy is free."},

    {"HANDSTAND", "HANDSTAND", "Fall inverted onto both palms (REALISTIC FLIPS on): you balance on your hands."},
    {"HANDSTAND", "  (2) MOVING", "Lean to walk, push off to leap, let go or lean past FALL ANGLE to drop."},
    {"HANDSTAND", "  (3) TUNING", "BALANCE ASSIST steadies you; SWAY TIME is how long you may be off balance."},

    {"SHOWTIME", "SHOWTIME", "SHOWTIME: hold X + B for 200 ms to step outside Spider-Man."},
    {"SHOWTIME", "  (2) ORBIT", "The right stick orbits; let go to return. WATCH ONLY keeps your hands still."},

    {"CAMERA & STABILIZATION", "CAMERA & STABILIZATION", "How much the animation moves your view: CAMERA MOTION, PITCH, ROLL, BOB."},
    {"CAMERA & STABILIZATION", "  (2) SMOOTHING", "How smoothly: STABILIZATION and the LIGHT / HEAVY times. Lower = calmer."},

    {"HANDOFF TIMING", "HANDOFF TIMING", "LANDING BLEND, GLIDE SPEED and LONGEST, PLACE, ANGLE TIME, HEAD SEAT:"},
    {"HANDOFF TIMING", "  (2) WHAT THEY DO", "how view and body glide between animation and SpidyVR. Smaller = snappier."},

    {"SETTINGS & RESET", "SETTINGS & RESET", "Y RESET ALL on an NA tab resets only that tab; launcher-only rows stay."},
    {"SETTINGS & RESET", "  (2) EVERY SETTING", "Every switch with its default: docs/NA_HELP.md and the launcher, NA marked."},

    {"OTHER MODS", "OTHER MODS", "Spidy chains its hooks in front of other mods (e.g. Manhattan Skies)."},
    {"OTHER MODS", "  (2) MISSING TABS", "If the SPIDY VR tabs or prompts go missing, the log's mods: line names the mod."},
    {"OTHER MODS", "  (3) TESTING", "Remove other DLL loaders (winmm, version, dinput8) to test. See MOD_COMPAT.md."},

    {"TROUBLESHOOTING", "TROUBLESHOOTING", "A crash code 0xC0000409 as the game closes is known and harmless."},
    {"TROUBLESHOOTING", "  (2) LOGS", "Logs are in the reports folder: post the newest three files on Discord."},
};
inline constexpr size_t pageCount = sizeof(pages) / sizeof(pages[0]);

} // namespace spidy::na_help
