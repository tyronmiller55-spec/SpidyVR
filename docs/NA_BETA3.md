# 0.8.7-na-beta3: headset checklist

> **Build beta3b** (same version, 0.8.7-na-beta3; package `Spidy-0.8.7-na-beta3b-win64-<commit>.zip`): adds the
> [beta3b section](#beta3b-showtime-auto-kinds-and-double-tap-a) below - Showtime's automatic camera split per kind
> with how-often controls, and double-tap A to turn upside down. Settings revision 18.
>
> **Build beta3c** (same version; package `Spidy-0.8.7-na-beta3c-win64-<commit>.zip`): adds the
> [beta3c section](#beta3c-webs-across-the-game-and-rope-physics) below. Full-length swing lines now work round
> edges, in both hands and while climbing, swing free until they fade, and hand over to stick-click web lines. A
> new switch (on, 100%) makes the hanging line really pull on the swing. Settings revision 19.
>
> **Build beta3d** (same version; package `Spidy-0.8.7-na-beta3d-win64-<commit>.zip`): adds the
> [beta3d section](#beta3d-showtime-strikes-vs-punches-finishers-fixed-reel-out-air-physics-on-a-swing) below.
> Showtime steps out only for the game's own web strikes, Y+B finishers auto-Showtime correctly, a double tap and
> hold on the reel trigger pays a web out, and air systems work while swinging. Settings revision 20.
>
> **Build beta3e** (same version; package `Spidy-0.8.7-na-beta3e-win64-<commit>.zip`): adds the
> [beta3e section](#beta3e-point-jump-off-wall-grabs-grip-to-stick-rope-weight-and-drag) below. The point jump
> cancels wall grabs, walls no longer stick you until you grip them, and the swing line gets weight and drag
> sliders. Settings revision 21.
>
> **Build beta3f** (same version; package `Spidy-0.8.7-na-beta3f-win64-<commit>.zip`): adds the
> [beta3f section](#beta3f-kick-lunge-kick-feel-the-kick-s-outcome-in-the-log-point-jump-on-the-floor-crawl-flag)
> below. An air kick at an enemy a little out of reach flies you at him, a started kick ticks the controllers, and
> the point jump works in fights and on rooftops (refused on the floor-crawl flag only in a real duct). Settings
> revision 22.
>
> **Build beta3g** (same version; package `Spidy-0.8.7-na-beta3g-win64-<commit>.zip`): adds the
> [beta3g section](#beta3g-web-rope-physics-kick-animation-and-reach) below. The web rope takes fixed steps, pulls
> taut under your weight at the new 100% rope weight, and keeps its line through wraps. Kicks chamber, lean and
> carry your travel, with a target ring, a ground lunge, a rebound and a rope assist. Settings revision 23.
>
> **Build beta3h-polish** (same version; package `Spidy-0.8.7-na-beta3h-polish-win64-<commit>.zip`): adds the
> [beta3h-polish section](#beta3h-polish-runtime-saves-and-the-quit-confirm) below. Crime Run is in this package
> (off by default). A failed save of the SPIDY VR tab is tried again, Crime Run's quit confirm and swing asks
> freeze during a pause or load, and the overlay and Crime Run HUD no longer allocate every frame. No new setting
> and no revision bump: off still equals beta3g with Crime Run off.
>
> **Build beta3i** (same version; package `Spidy-0.8.7-na-beta3i-win64-<commit>.zip`): adds the
> [beta3i section](#beta3i-web-in-hand-thick-swing-line-grip-and-fists) below. A held web leaves from inside the
> closed hand, a thicker overlay follows the swing line, the animation can close a hand on its web or a punch, and
> two log wording fixes from the polish pass are in. Settings revision 24.

A **test build** (branch `feat/087-polish`, from Crime Run on `feat/087-crime-run`, itself from `feat/087-beta3` on
`feat/087-beta2` 1418992). The testers' **CURRENT** build stays
[0.8.7-na-beta2](NA_BETA2.md) until this one is played. What changed: [CHANGELOG.md](CHANGELOG.md).

Every feature stays on. Settings files from before take migration revision 17: HANDOFF HEAD FIRST, LANDING VIEW
SNAP, HEAD SEAT PULL and SHOWTIME AUTO: FINISHERS AND STRIKES come on once (a 0 chosen after it stays), and a saved
handstand tip-in of 120 (the old default) becomes 60. No other existing numeric default changed: HANDOFF PLACE
stays 125 cm, WALL JUMP SOUND stays SOUND A. No new game address: everything reuses hooks already in
[REFERENCE.md](REFERENCE.md) and [VALIDATION.md](VALIDATION.md).

## What the beta2 session showed (game-vr-20261121-080924, build d482497)

The owner (t86u): handoffs "periodically disconnecting the head from the body where frames of the body without
the neck can be seen". In that log 297 of the 300 `hybrid: vr->anim` lines read `headbone n/a`: on the switch's
own frame the hero's head sample was not on yet (it turned on with the head follow, later in the same frame), so
the landing glide started blind and the animation's body was drawn before there was a head to put the view on.
Landings then glided up to 1.25 m (HANDOFF PLACE) over up to 450 ms, and walking in the room re-seated the view
on the neck 0.20 m at a time.

## New settings (spidy_vr.ini keys; launcher Native Animation tabs; NA · HELP)

| Key | Default | What |
| --- | --- | --- |
| `handoff_head_first` | 1 | A switch into the animation reads his head first; SpidyVR's body stays drawn until it is (2 frames at most). |
| `landing_view_snap` | 1 | A landing, zip or mount puts the view within `landing_view_snap_cm` of his head at once, then glides. |
| `landing_view_snap_cm` | 15 | (0 to 100) how far from the head the snap leaves the view. |
| `landing_view_snap_ms` | 120 | (40 to 400) the glide over that last part (the beta2 glide could take 450 ms). |
| `head_seat_pull` | 1 | Past `head_seat_pull_cm` off his neck the view follows the headset back continuously. |
| `head_seat_pull_cm` | 5 | (0 to 20) how far the view may lean off the neck before the pull. |
| `head_seat_pull_cm_s` | 50 | (5 to 200) the pull at most; a faster step still re-seats at 20 cm. |
| `handstand_catch_deg` | 60 | (45 to 150) how far upside down a touchdown on the palms catches (100, hardcoded, before). |
| `handstand_tip_in_deg` | 60 | (45 to 180) the tip-in's angle (120 before; a saved 120 becomes 60). |
| `showtime_auto_finisher_strike` | 1 | (beta3 only; split in beta3b, below) Showtime's camera steps out for a Y+B finisher or a web strike. |

Launcher: **Handoff** tab, new card **Handoff fixes (beta3)**; **Handstand** card, **Catch angle** and **Tip-in
angle**; **Showtime** card, **Auto: finishers and strikes**.

## Takeovers (ANIM to SpidyVR)

The plan's H3 ("takeovers start the SpidyVR body at the current view") is already how a takeover works: TAKEOVER
VIEW EASE (`takeover_view_ease=1`, the beta1 default) holds the view where the animation showed it and moves the
whole SpidyVR placement with it (body, eyes and hands together, `GameTrackingRig::beginViewEase`), then eases onto
the feet. No new switch was added for it. The new head-gap log line measures takeovers too (`anim->vr`), so the
next session tells whether they still need work.

## Headset checklist

1. **Head gap per switch (diagnostics).** Every owner switch gets one line a second later:
   `handoff head gap: vr->anim <switch> worst X m at T ms, settled S m, over settled+5 cm for N frames (M ms),
   U of F frames unmeasured`. Small worst and few frames over is the goal. Post the log.
2. **No neckless frames.** Land, zip to a point and mount a perch many times. Do you still see his body without a
   head for a frame? Log: `handoff head first: ...` appears only when the head took a frame or two.
3. **Landing snap.** After a landing or zip the view should sit on his head almost at once, with a short settle,
   not a long slide. Log: `landing view snap: <switch> X m from the head, Y m snapped at once, the last Z m glide
   over 120 ms`. If the snap itself is jarring, try LANDING VIEW SNAP CM 30 or SNAP GLIDE 200 and say which.
4. **Neck pull.** Walk and lean about the room while the animation has the body (run, stand, perch). The view
   should stay on the neck with no 20 cm pop every few seconds. Log every 10 s: `head seat pull: X m over N frames
   ... ; R reseats this lock` (R should stay low).
5. **Handstands from 60 degrees.** Flip and put both palms down well before fully upside down: it should catch.
   Report any catch you did not want (on a slope, a roll, a low dive).
6. **Showtime on finishers and web strikes.** Y+B a stunned enemy: `finisher: ... Showtime's camera out for it`,
   `showtime: enter (finisher)`, and the normal `showtime: exit (release) after X s` when it ends. A web strike:
   `showtime: auto (web strike #n)`, `showtime: enter (web strike)`. Not headset-tested yet: Showtime inside the
   finisher's first-person scene. Report what you see (the outside camera, the scene's view, or a flicker).
7. Anything else odd: post the newest three files from the reports folder.

## beta3b: Showtime auto kinds and double-tap A

The owner, Nov 21 t88u: "separate toggles for dodges, finishers, and strikes, and different timings for how often it
fires. Also, give me an option to simply flip Spidey perfectly upside down by double tapping A. I can hold on the
double tap to stay inverted". Settings revision 18: beta3's `showtime_auto_finisher_strike=0`, if saved, turns both
new switches off; DOUBLE-TAP A comes on once (a 0 chosen after it stays).

| Key | Default | What |
| --- | --- | --- |
| `showtime_auto_finisher` | 1 | Showtime's camera out for a Y+B finisher (as beta3). |
| `showtime_auto_finisher_every` | 1 | (1 to 10) out on every Nth finisher; 1 every one. |
| `showtime_auto_finisher_cooldown_ms` | 0 | (0 to 60000) the least time between two automatic finisher cameras. |
| `showtime_auto_strike` | 1 | Showtime's camera out while a web strike flies (as beta3). |
| `showtime_auto_strike_every` | 1 | (1 to 10) every Nth web strike. |
| `showtime_auto_strike_cooldown_ms` | 0 | (0 to 60000) the least time between two. |
| `showtime_auto_dodge_every` | 1 | (1 to 10) every Nth dodge AUTO: DODGES counts (its Off / Perfect only / All row is unchanged). |
| `showtime_auto_dodge_cooldown_ms` | 0 | (0 to 60000) the least time between two. |
| `air_flip_double_tap` | 1 | Double-tap A in the air: upside down; hold the second A to stay there. |
| `air_flip_double_tap_ms` | 300 | (150 to 600) the window from the first A to the second. |

How often: every event of a kind counts; the Nth goes out if the cooldown has passed since that kind last took the
camera out. An event held back by the cooldown keeps the count full, so the first one after the wait goes out. A
held-back finisher is logged: `showtime: auto finisher held back (counting|cooling down: every N, cooldown M ms)`.
With AUTO: DODGES on All, a dodge counts once (its perfect moment only extends the camera it took out).

**Double-tap A vs a single A (no delay added).** With REALISTIC FLIPS, A in SpidyVR's air already rights him over A
IN THE AIR (1 s) and, held, keeps him upright until let go. A single A still starts that at once - nothing waits to
see whether a second tap comes. A second A within 300 ms of the first (both pressed in the air, let go in between)
is a new press of the same control: the turn restarts from wherever the upright had got to, now toward upside down,
with the same minimum-jerk smoothing and time (the shortest turn to his up straight down; from nearer upright a half
roll about his own forward, so he keeps facing the same way). The view's yaw is never written. Held, he stays exactly
upside down; let go, he is free (the flips turn him again), exactly as a held single A works upright. A third tap is a
single A again. B down at the second press is the point jump's A+B chord, not a double tap, and an A pressed on the
ground (a jump) never counts as the first tap.

### beta3b checklist

1. **Showtime kinds.** Turn AUTO: WEB STRIKES off: finishers still take the camera out, strikes do not. Try STRIKES:
   EVERY 3 and a COOLDOWN of 5000 ms in a fight and say whether it feels right.
2. **Double-tap A.** In the air (swinging, falling), tap A twice quickly: he turns upside down over about a second,
   facing the same way. Hold the second A: he stays upside down until you let go. One A still rights him at once.
   Log: `air flip: double tap A, turning upside down over up to 1000 ms, #n` and `air flip: A let go, free again`.
3. **Chords.** A+B point jumps and ground jumps feel exactly as before (no added delay).
4. Watch: righting from a perfect upside down with a single A may turn him through a pitch rather than a roll (the
   upright's turn is unchanged); say if he ends up facing backward.

## beta3c: webs across the game and rope physics

The owner, Nov 21 t90u: "Webs wrap around things and the web can be grabbed by your second hand to control the web or
climb ... polish our webs to work across all of those states, and let them keep the physics until they fade away ...
there's a feature that lets you click the sticks when the controllers are close to 'connect' two webs. Make sure our
webs work there too." t91u: "if it's just visual then fine, but see what's possible here because I like it." t92u:
"get it going".

**What beta3b was.** The full-length lines were looks only (`swing_line::Line` fed only the game rope's hand points;
the swing's rope, `SwingConfig::taut`, never saw them). The pull and fling you felt were the real, shorter swing
rope plus the drawing. Beta3c makes the pull real, behind its own switch. The analysis and plan are in the
beta3b playtest's ANALYSIS.md and WEB_POLISH_PLAN.md.

Settings revision 19: every new switch comes on once; a 0 chosen after it stays. No game address is new. The rope
pull uses a new side channel between SpidyVR's own modules (`SpidyRopePull`, the same pattern as `SpidyJumpPull`).

| Key | Default | What |
| --- | --- | --- |
| `web_wrap_slack` | 1 | Round edges the line keeps its length. The slack hangs between the hand and the first edge; the run edge to edge counts in its length. Off: taut through the edges, as before. |
| `web_shared_slack` | 1 | A web your other hand takes keeps its line (now kept per game rope, so it no longer restarts when it changes hands). Hand-over-hand climbing (BETTER CLIMBING, `WebState` bit 16) takes line in one-for-one, like the reel. Off: one web in both hands is drawn taut, as before. |
| `web_fade_physics` | 1 | A line you let go of swings free from where it sticks: its hand end falls with your hand's last velocity on the rope's length, and the rope sim lays the line. Then it dissolves as before. A new web on that rope ends it at once. |
| `web_fade_ms` | 1200 | (300 to 3000) how long the let-go line swings. |
| `web_connect_handoff` | 1 | A web line or web tie made with the stick click, or a zip to a line, ends any let-go line at once, so the game rope is free for the line. |
| `swing_rope_physics` | 1 | **Feel change.** With FULL-LENGTH SWING LINES, the hanging line pulls you along its first segment: half the slack's weight, plus the momentum of a coil whipping away from your hand. A line trailing a fast swing drags you back (the sim's still air). Only in SpidyVR's own air while a web is on; never taut (the swing rope already holds you), never on the ground. Off or 0%: the swing exactly as before. |
| `swing_rope_physics_pct` | 100 | (0 to 300) strength. At 100%, 30 m of slack hanging straight pulls about 1.5 m/s^2, at most 2.5 m/s^2 per hand. |

Launcher: Web shooters page, card "Webs across the game (beta3c)". NA HELP: WEBS & WEB CANCEL pages 5 to 9.

**Limits (no validated signal).**
- The game's own last dissolve is still its own straight fade. Its hand-points hook skips a released rope (+70c bit
  2), so the fade keeps the rope unreleased while it swings, then releases it.
- A web line's own rope takes no hand points, so the line itself stays taut. The hand webs hand over to it cleanly.
- In both hands the slack on the upper run can include the short span between your hands (the held bow is drawn
  separately, at most 0.5 m).

### beta3c checklist

1. **Rope pull.** Catch a far web (a long line) and swing in close: the slack's weight should tug you down and back,
   gently. Whip the hand: the coil should give a small yank. Try ROPE PULL STRENGTH 0, 100 and 200 and say which feels
   right. With the switch off, swings must feel exactly like beta3b.
2. **Wrap.** Swing round a corner so the web takes an edge: the slack should stay, hanging before the first edge, not
   snap taut. Log: `web wrap slack: L line round 1 edge(s) ...`.
3. **Second hand / climb.** Grab your web with the other hand: the line stays full. Climb hand over hand: the line
   takes in as you climb. Log: `web shared: ...` and `swing line: L climb drawn ...`.
4. **Until fade.** Let go of a long web: it should swing down from its anchor for about a second, then dissolve. Shoot
   again at once: the new web is not blocked. Log: `swing line: L let go: ... it swings free until it fades` and
   `web fade: L dissolves after 1200 ms (faded|a new web|...)`.
5. **Connect.** Hold webs on two surfaces, bring the hands together, click the right stick: the web line appears at
   once, with no leftover swinging line. Log: `web connect: a web line takes the webs ...` when a fade was cut.
6. Watch for any web drawn through a wall while fading, or a frozen line.

## beta3d: Showtime strikes vs punches, finishers fixed, reel out, air physics on a swing

The owner, Nov 21 t93u: "Showtime on punches is what's happening now, let's remove that. Let's only have it on
animation web strikes, not manual SpidyVR punches. Or, if we do, let's give it a toggle ... I'd like the ability to
double tap the trigger and hold to reel the line out, so we can have more control there. I also want air arm and leg
physics, and all mid air physics calculations to apply while swinging as well."

Settings revision 20: AUTO: PUNCHES is set off for everyone, and REEL OUT and AIR PHYSICS WHILE SWINGING come on
once. A value chosen after revision 20 stays. No game address is new. Reel out uses a new side channel between
SpidyVR's own modules (`SpidyReelOut`, the same pattern as `SpidyRopePull`).

| Key | Default | What |
| --- | --- | --- |
| `showtime_auto_strike` | 1 (kept) | **Changed meaning.** Now only the game's own animated web strike. That strike is `HeroStateWebRush` (REFERENCE.md), which runs as a dodge of kind 8 in the hero's dodge manager (game_dodge.hpp, "8 the web rush"). It is read from `SpidyDodgeLive`, the hook DODGE TUMBLE already reads. The beta3b log shows "the game's dodge began (kind 8)" at each strike. The camera stays out while the dodge is still kind 8, plus 0.3 s. |
| `showtime_auto_punch` | 0 | **New.** SpidyVR's own manual punch that flies you to an enemy, or a dive (`WebStrike`, `game_punch::Strikes::opened`). Until beta3d this is what AUTO: STRIKES fired on. |
| `showtime_auto_punch_every` | 1 | (1 to 10) out on every Nth punch. |
| `showtime_auto_punch_cooldown_ms` | 0 | (0 to 60000) least time between two punch cameras. |
| `swing_reel_out` | 1 | **Feel change.** While a hand's web is attached, press that hand's reel control (the trigger, or the grip with WEBS ON THE TRIGGER) a second time within the window and hold it. The line pays out until you let go, the web goes, or the line reaches the swing's longest (`SwingConfig::maxRange`). The first press still reels in, as always (a tap is a short reel). A single long press is the ordinary reel. Shooting and letting go of webs belong to the other control and are untouched. The taut take-in stands down for that hand while it pays out, so you fall or swing out onto the longer line. A full-length line grows one-for-one with it (`WebState` bit 32). |
| `swing_reel_out_tap_ms` | 300 | (150 to 600) the double tap's window, first press to second. |
| `swing_reel_out_speed` | 400 | (50 to 1500 cm/s) the pay-out speed. 4 m/s is half the reel-in's 8 m/s. |
| `swing_air_physics` | 1 | **Feel change.** The air systems below that stood down while a web held now run on a swing. Off: exactly beta3c (every changed gate is behind this switch). |

Launcher: the Showtime card has Auto: punches with its every and cooldown rows. The Web shooters page has a new card,
"Swinging (beta3d)". NA HELP: SHOWTIME (4) to (6), WEBS & WEB CANCEL (10) REEL OUT and (11) AIR PHYSICS.

### Finisher root cause (beta3b: detected, but the camera never went out)

The log fired the trigger at 1033.984 s and 1052.234 s ("Showtime's camera out for it (AUTO: FINISHERS)"), but no
"showtime: enter (finisher)" followed. A Y+B finisher is a first-person scene. In a scene the game's follow camera
is not committed, so `gameplay` is false and the tracking rig goes inactive (`GameTrackingRig` Inactive notPlaying,
"a menu, a scene, the game's own camera"). The head probe logged "combat -> cutscene" right across the finisher.

Showtime's gate let the finisher's own moment past `gameplay`, but it still required `motion.active`. So on the
scene's first frame `stOk` was false and `showtimeAuto.clear()` wiped the trigger it had just been given.

The fix has two parts:
- `showtime::OkInput` lets that moment past both checks.
- The first-person scene's eye path now places and publishes Showtime's outside camera (`showtimePlace`, shared
  with play). Before, only the play path did, so the scene's eyes would have stayed on his head. The native views
  read it through `game_fp::frameLevel`, as in play.

A new `showtime: auto (<kind>) held back: <reason>` line names whatever takes an auto camera away.

### Air systems on a swing (audit)

| System | beta3c with a web attached | beta3d (`swing_air_physics=1`) |
| --- | --- | --- |
| Momentum leg trail (`native_body::setAirMomentum`, legs in the air) | already on (`legsAirOnly` covers SpidyVR's swing) | unchanged |
| Tuck pose + pose coupling (`swing_coupling::scales` to the flips' inertia) | already on | unchanged |
| LEG CONTROL counter-turn and spin scale (REALISTIC FLIPS) | already on | unchanged |
| REALISTIC FLIPS: web alignment, release spin | already on | unchanged |
| REALISTIC FLIPS: arm swings (hand momentum) | **gated in effect**: added for one frame, then the web's alignment reset the momentum every frame | **on**: the arms' spin builds and falls off at the air falloff on top of the alignment (at most half the turn rate, so the web leads), and carries into the release (`RealisticFlipSettings::tetherSpin`) |
| Air correct (A rights him) and double-tap A upside down | already on (the leveling runs before the tether) | unchanged |
| Animated air spin | already on (head probe swing/web states) | unchanged |
| MIDAIR POSES: dive, superhero, starfish | **off** (`PoseInput::webbed`) | **on**. A one-hand-up rising swing is the superhero pose, and both hands spread falling is the starfish. |
| AIR DODGE (B) | **off** (`Why::web`, B went to the game) | **on**. The burst is the body's velocity, and the tension-only rope turns it into the arc. |
| LANDING BLEND-IN (upright before a touchdown) | **off** (a web held kept the tilt) | **on**: a swing down onto the ground lands like a fall |
| Air crouch (legs tucked at a hang) | off | **left off**: at a swing's low point he is upright and slow, and there the legs should trail the swing, not crouch |
| Dodge tumble (the game's dodge leaning the view) | off | **left off**: it is the game's own dodge, and on SpidyVR's swing the game is not dodging |
| Jump pull (nudge toward an aimed enemy) | off | **left off**: it would fight the rope (the swing kick steers on a web instead) |
| Soft landing | off with a web | **left off**: SMOOTH WEB LANDING is the web's own version |
| Point jump bullet | off | **left off**: an A-hold mechanic, not air physics |
| Ground flips, sidestep | off | **left off**: ground moves |

### beta3d checklist

1. **Showtime: strikes vs punches.**
   - Use the game's own web strike on an enemy: the camera should step out. Log: `showtime: the game's web strike
     began (its web rush, dodge kind 8, #N) -> camera out` and `showtime: enter (web strike)`.
   - Throw your own SpidyVR punches that fly you in: no camera (AUTO: PUNCHES is off). Log: none for them.
   - Turn AUTO: PUNCHES on: punches step out. Log: `showtime: SpidyVR's punch flew (manual web strike #N) -> camera
     out` and `showtime: enter (punch)`.
2. **Finishers.** Do a Y+B finisher: the camera should step outside for the scene and come back about 0.3 s after
   it ends. Log: `showtime: enter (finisher)`. If it doesn't, the `showtime: auto (finisher) held back: ...` line
   says why. Say whether the outside view in the scene looks right.
3. **Reel out.**
   - On a web, tap the trigger and then press and hold it: you should drop or swing out on a longer line. Let go and
     it stops. Log: `reel out: left hand paying out from X m` / `stopped (let go of), Y m paid out`.
   - A single hold still reels in, and the grip still lets go.
   - Try speeds 200, 400 and 800 cm/s.
   - With full-length lines, the drawn line should grow with it, with no snap.
4. **Air physics while swinging.**
   - Swing and swing your arms: the body should keep some of the spin you give it.
   - Raise one hand on a rising swing (superhero) and spread both on a fall (starfish).
   - Press B on a swing for the air dodge.
   - Swing down to the ground: the landing should upright you like a fall does.
   - With the switch off, the swing should feel exactly like beta3c.

## beta3e: point jump off wall grabs, grip to stick, rope weight and drag

The owner, Nov 22 t94u: "I want point jump to cancel wall grabs. I want to remove the auto wall stick so that I can
slide on walls; in order to stick and crawl I need to grip the wall, then I can crawl as normal. I want a weight
slider for the webs, right now they're so light I can't control the swing. A drag slider and a weight slider
alongside the pull strength. Reducing drag should reduce the effect of slack while swinging, but still allow for
the physics."

Settings revision 21: POINT JUMP CANCELS WALL GRABS and GRIP TO STICK come on once, and ROPE WEIGHT starts at
200%. A value chosen after revision 21 stays. No existing numeric default changed, and no game address is new.
Grip to stick uses a new side channel between SpidyVR's own modules (`SpidyWallStick` / `SpidyWallStickLive`, the
same pattern as `SpidyReelOut`). The point jump's side channel (`PointJumpAsk`) is now version 2, with one flag.

| Key | Default | What |
| --- | --- | --- |
| `point_jump_wall_cancel` | 1 | A+B on a wall fires the point jump off it. On SpidyVR's wall, a grip held (hand climbing's hold, the wall brake) no longer cancels the charge, so the jump detaches you from the grab. On the game's own wall crawl or wall run, the point jump is no longer refused as "game crawl": it launches out along the crawl's normal and the owner switch takes the body, as from the ground. It plays the point jump's sounds. Off: exactly beta3d. |
| `wall_grip_to_stick` | 1 | **Feel change.** Hitting one of SpidyVR's walls in the air no longer sticks you. You slide down and along it with friction, keeping your speed. Squeeze a grip with your palm on the wall to stick; then the stick crawls as before. Off: exactly beta3d (every wall holds at once). |
| `wall_slide_friction` | 50 | (0 to 100 %) the slide's friction as a share of gravity. 0 slides freely; at 50 you slide down a wall at half of gravity; 100 holds a body at rest (a moving one slows to a stop). |
| `swing_rope_weight_pct` | 200 | (0 to 300 %) the swing line's mass, for both the drawn rope sim and its pull on you. 100 is beta3d's line. |
| `swing_rope_drag_pct` | 100 | (0 to 300 %) the air drag on the swing line. 100 is beta3d's line. Lower drag means the slack tugs and flings you less, while the rope still swings and hangs. |

Launcher: the Web shooters page's "Webs across the game (beta3c)" card has Rope weight and Rope drag under Rope
pull strength. The Legs page has a new card, "Walls (beta3e)", under Grip and tethers. NA HELP: JUMPS & POINT JUMP
(3) ON A WALL, HAND GRAB (4) GRIP TO STICK and (5) STICK, CRAWL, WEBS & WEB CANCEL (12) ROPE WEIGHT and (13) ROPE
DRAG.

### How walls attach, and what grip to stick changes

There are two kinds of wall, as `game_swing::Surface` reports them:

- **SpidyVR's own wall** (`Surface::wall`, `Swing::wall_`). Until beta3e, `Swing::senseWall` joined any steep, fixed
  wall the body met in the air: head on, grazing, or pushed at with the stick (`joinWall`). From then on
  `wallMotion` held the body. Gravity did not pull along the wall, and the stick walked it at once. That join is
  the "auto wall stick". The hybrid owner's rule "midair animation + Spidy wall -> VR wall run" hands such a wall
  to SpidyVR.
- **The game's own wall crawl and wall run** (`Surface::game`, mover flag `0x80`). The game attaches the hero
  itself, while the animation owns the body or from its own crawl. The swing only reports it (`gameHoldsWall`,
  `supportSurface`) and takes no wall from inside it. The hybrid owner's rule "game crawl -> ANIM" (and
  `EnterAnim::mount`) is the earlier "a jump that becomes a wall crawl returns to ANIM" rule.
  [REFERENCE.md](REFERENCE.md) and [VALIDATION.md](VALIDATION.md) ("Stop on any wall and the game's own crawl takes
  you") have no hook that stops the game attaching, so **beta3e cannot stop the game's own wall crawl or wall run
  from taking you.** That stays game-native: a jump the animation owns can still end on the game's crawl. The
  point jump now gets you off it (above).

Grip to stick changes SpidyVR's own wall only (`include/spidy/wall_slide.hpp`):

- The join still happens, and its speed is kept as before (`wallCarry`). The wall is marked not stuck
  (`WallHold::stuck`).
- While not stuck, `wallMotion` slides the body. Each step adds gravity's part along the wall, and friction
  (`wall_slide_friction` of gravity) takes speed off against the slide, never past a stop. The stick does not
  crawl. The wall jump (A), webs and the point jump work as before.
- What sticks it:
  - Hand climbing's hold (`climbInputs`: a grip with the palm at the wall and pointing at it). The WALL BRAKE then
    slows the slide as before.
  - With hand climbing off, or the stick pushed, the same palm test with the grip squeezed past 0.65. This catches
    the slide at once, as hand climbing's hold does.
  - A mount, which is the walk onto SpidyVR's wall from the game's crawl or the walked mount. The player asked for
    the crawl.
  - A wall the body was stuck to and lost by itself (the next face of a shaft, past a gap, round a corner, within
    0.6 s with no jump between). A crawl keeps crawling.
- Once stuck, everything is beta3d: the stick crawls, and the body rests without input.
- The log shows `grip to stick: onto a wall sliding at X m/s, #N`, `grip to stick: stuck to the wall (Y m/s along
  it), #N` and `grip to stick: off the wall still sliding`.

Limits:

- The game's own crawl and wall run are untouched, as described above.
- In the WALLS mode "pressed" (`walls=4`), the swing presses the body into the wall so that the game grasps it. That
  grasp is the game's.
- The palm test needs a tracked hand at the wall (within 0.18 m of its face, pointing at it), the same as hand
  climbing.

### Point jump off a wall grab

- The charge's grip cancel ("A grip cancels a charge") stands down while the body is on a wall. Before, a hand
  holding the wall cancelled every chord: the beta3b log shows `point jump: charge cancelled (grip) after 0 ms` again
  and again.
- On SpidyVR's wall the swing already launched a point jump off it (`leaveWall`, then the wall-jump pause so the
  same wall does not take you back at once). The climbing hands let go with it.
- On the game's own wall crawl or run, XR now counts it as contact. The jump's normal is the crawl's
  (`SpidySwingData.wallNormal`), and the ask carries flag 1. The swing takes the launch there even where the body
  does not stand (`supportSurface`). The hybrid owner's point jump switch hands the body to SpidyVR, as for a jump
  off the ground.
- The log shows `point jump: cancels the wall grab (wall | game wall)` before the usual `point jump:` lines.
- Not validated in the headset: whether the game lets go of its crawl cleanly when SpidyVR takes the body there.
  Tell us if he sticks, snaps back or turns oddly.
- Found while checking: all 5 `point jump: denied game crawl` lines in the beta3b log came while the game's
  floor-crawl flag was on (`Surface::floor`, "wall 0->3" with "game ground 1->0" just before, lit by ordinary
  ground moves and fights), not on a wall. That refusal is unchanged in beta3e. Tell us if A+B on a rooftop or in
  a fight sometimes does nothing.

### Rope weight and drag

Both act on the drawn rope (`swing_line::Line`'s 24-particle sim) and on the pull it puts on you (the rope physics
`SpidyRopePull`) in the same way. 100% and 100% is exactly beta3d.

- **Weight** scales the line's mass (`massPerMetre`). Its hanging weight and the momentum its slack carries both
  scale with it, and so does the pull's cap past 100% (2.5 m/s^2 a hand at 100%, 5 at 200%). The sim's air
  damping is divided by it: a heavier line is slowed less by the air, so it hangs and keeps swinging with what it
  had. 200% by default because the owner found the line too light to steer by. At twice the mass the line pulls
  about twice as hard, and its momentum carries through the swing instead of dying in the air, so there is more
  to lean on. Your input doesn't change.
- **Drag** scales the sim's air damping and the momentum part of the pull: the tug and fling of a trailing or
  whipping slack. The hanging weight stays. At 0% the slack's tug is gone, and the rope still swings and hangs (a
  floor of damping keeps it stable).

Unit tests: drag 0 gives a smaller pull while the rope still moves, and keeps the hanging weight; 200% pulls much
harder under its own cap; a heavier line keeps swinging longer after the hand stops; 1 and 1 are the combat
rope's damping and mass.

### beta3e checklist

1. **Point jump off a wall.**
   - On a wall, hold a grip on it (hand climbing) and press A+B: the charge should build (no `charge cancelled
     (grip)`) and the jump should take you off the wall with its sounds.
   - On the game's own wall crawl, A+B should launch you out of it. Log: `point jump: cancels the wall grab (game
     wall)`. Say whether he leaves the wall cleanly.
   - With `point_jump_wall_cancel=0`, both should be as in beta3d.
2. **Grip to stick.**
   - Swing or jump into a wall: you should slide down and along it, keeping your speed. Log: `grip to stick: onto a
     wall sliding`.
   - Squeeze a grip with your palm on the wall: you should stop and stick. Then crawl with the stick as usual. Log:
     `grip to stick: stuck to the wall`.
   - Wall jump (A) and webs while sliding should work.
   - Try friction 20, 50 and 80.
   - With the switch off, walls should hold you at once, as in beta3d.
   - Note any time the game's own crawl still takes you: that is the game's.
3. **Rope weight and drag.**
   - Swing with weight 100, 200 and 300: heavier should hang lower and carry the swing more. Say which helps you
     control it.
   - Lower drag (50, 0): the slack should tug you less, and the line should still swing.
   - The log's `rope pull: on (strength S%, weight W%, drag D%, ...)` line shows the values.

## beta3f: kick lunge, kick feel, the kick's outcome in the log, point jump on the floor-crawl flag

The owner, Nov 22 t95u/t96u: "Why is there a floor crawl flag? Do we need it?" ... "Yep, I like it. Do that, and
let's start to work on kicks. I'll test them next build."

Settings revision 22: KICK LUNGE and KICK FEEL come on once. A value chosen after revision 22 stays. No existing
default changed, and no game address is new. The air kick's side channel (`AirKickAsk`) is now version 2 (flag
bit 5 and `lungeCm` in the old reserved word), and its account (`AirKickLive`) version 4.

| Key | Default | What |
| --- | --- | --- |
| `kick_lunge` | 1 | **Feel change (kicks only).** Off the ground, an air kick at an enemy up to `kick_lunge_reach_cm` past the leg's reach (the gaze cone, in front, or the punches' lock, as before) takes him as the target and carries you at him: your velocity toward him rises to what closes the gap within the kick's window, at most 12 m/s, and the rest of your motion is kept. The kick aims along the line to him. A blow off a lunge keeps 40% of your velocity, so you stop at him. SpidyVR's air only; standing kicks never lunge. Off: the leg's reach only, exactly beta3e. |
| `kick_lunge_reach_cm` | 250 | (0 to 400 cm) how far past the leg's reach (1.15 m + the air's 0.4 m, plus his 0.3 m radius) the lunge looks. |
| `kick_feel` | 1 | A kick that starts ticks both controllers (the legs have none); a lunge ticks harder, with the web strike's rush body haptic and its rush sound (BLOW SOUNDS OFF silences it). A landed kick keeps the strikes' blow feedback. Off: exactly beta3e. |

The point jump change has no switch: it is the refusal the owner asked to narrow.

Launcher: a new "Kicks (beta3f)" card on the Kicks & Combat page. NA HELP: AIR KICKS (5) KICK LUNGE and (6) KICK
FEEL. The full audit, the logs and the slices left are in the playtest folder's `KICKS_PLAN.md`.

### Why kicks needed this (the audit)

Air kicks (K2), ground kicks (K3) and the swing kick (K1) all hang off the legs chord (Y + X) and a fast flick of the
right stick ([AIR_KICKS.md](AIR_KICKS.md), [GROUND_KICKS.md](GROUND_KICKS.md), [SWING_KICK.md](SWING_KICK.md)). An
air kick's foot is modelled from the pelvis (1.15 m leg + 0.4 m lunge) and never moved the body, so an enemy had to
be within about 1.85 m when you flicked. In the beta3b and beta3c logs, 11 kicks were started and **none** had an
enemy in reach: 9 were outside a fight, one was refused on a landing (`moving too fast`), one started standing in a
fight with nobody close enough. The gestures registered every time; the reach did not.

### The kick's outcome in the log

- `kick: <gesture> start zone= target= aim=<gaze|nearest in front|punch lock|none (no enemy in reach)>`, then
  ` lunge 2.1 m at 8.1 m/s (lunges N)` when it lunges, and with no target `; 3 in the fight, nearest 4.2 m` or
  `; no enemy in a fight`.
- `kick: <gesture> miss (started, hits, misses; N in the fight, nearest X m)`.

### Point jump on the game's floor-crawl flag

The game's floor-crawl flag (mover flag `0x1`, `Surface::floor`) marks a vent crawl, but also lights when he ducks
under anything low, in fight poses and in rooftop parkour. The point jump refused it as "game crawl": 5 times in
the beta3b log and 15 in the beta3c log, none in a duct. Since beta3f it is refused there only when a duct's roof
has been seen over him (`FloorCrawlRoof::duct`, the headroom ray within 1.5 m of the feet), the check ground flips
use since Nov 18 (`floorCrawlRefuses`, game_tracking.hpp). `motion.ventCrawl` still refuses. The game's own wall
crawl keeps its rule (`point_jump_wall_cancel`). When a jump goes through that beta3e would have refused, the log
says `point jump: on the game's floor-crawl flag with no duct roof over him - allowed (beta3e refused it as "game
crawl")`. The quick sidestep's own floor-flag refusal is unchanged (not asked).

### beta3f checklist

1. **Kick lunge.**
   - In a fight, from 3 to 4 m off a thug in the air (jump, point jump or a swing), hold Y + X and flick: up
     (dive), straight up (double), the quarter circle (roundhouse), sideways (freeform). You should fly at him and
     stop on the blow. Log: `kick: dive start ... lunge 2.3 m at 8.8 m/s`, then `kick: dive hit ...`.
   - `kick_lunge_reach_cm` at 100, 250 and 400.
   - Kick while on a web: the rope bends the lunge. Say whether it feels right.
   - With `kick_lunge=0`, kicks should be beta3e's (no lunge, reach 1.85 m).
2. **Kick feel.** Every kick you start should tick both controllers; a lunge thumps harder with the rush sound.
   With it off, nothing on a start (as beta3e).
3. **Outcome log.** Out of a fight a kick logs `no enemy in a fight`; in one, how many and the nearest distance.
4. **Point jump.** A+B in fights, ducking under something low and in rooftop parkour should fire where it did
   nothing. In a vent or duct it should still be refused (`the game's crawl in a duct (a roof over him)`).

## beta3g: web rope physics, kick animation and reach

The owner, Nov 22 t97u: "they have a problem catching Spiderman's weight. They seem to build up kinetic energy and
become volatile, never settling ... the rope seems jittery and jumpy ... it feels like a step issue. Also, the webs
are cutting off and making new lines instead of wrapping and releasing ... Spiderman's weight should make it taut
when he swings against it ... I had to increase his weight to see an effect ... Kicks sound interesting, and your
inclusions all are perfect." Then t100u, on beta3f: "Kicks were okay, we need lots of animation work there, but
they seem to fire."

Settings revision 23: every switch below comes on once; KICK GROUND SPEED becomes 450 cm/s once; a saved ROPE
WEIGHT becomes the same mass on the new scale (beta3f's 200% default becomes 100%, 300% becomes 120%). A value
chosen after revision 23 stays. No game address is new. `AirKickAsk` is version 3 (80 bytes: `more` bits 0..3 and
`groundSpeedCms`), `AirKickLive` version 5 (the ring's pick, rebounds, ground lunges, rope assists).

| Key | Default | What |
| --- | --- | --- |
| `swing_rope_stable` | 1 | **Feel change.** The swing line's sim steps at a fixed 1/600 s out of the frames' time (beta3f: 8 steps a frame, each 1/8 of whatever the frame took), 8 constraint passes a step, damped toward moving with its ends (90%: a line carried along is no longer dragged back into the midair "stall"), no particle faster than 30 m/s about its ends, and the pull eased over 80 ms. Headless: a swung 18 m line at 90 Hz and at ragged 30..120 Hz frames ends 0.17 m apart (beta3f's steps: 4.4 m), and its energy settles to 0.2% of its peak within 2.5 s. Off: exactly beta3f. |
| `swing_rope_load_taut` | 1 | **Look change.** While the swing's own rope carries him (its pull past 2 m/s^2, all in by 6; hanging still is 9.8) the drawn line's spare length goes in over about 60 ms and it is drawn straight; the slack comes back over about 0.3 s when the load comes off (falling toward the anchor, thrown past it). The length it caught at is kept, and the pull follows the shown slack. Off: the slack always hangs. |
| `web_wrap_keep` | 1 | A web that wraps round an edge or comes off one keeps its line as it lies: the rope is cut at the new edge (or carried on to the new end) and its particles shared out again (`tether::Rope::reanchor`), instead of a fresh rope laid (what made the "new line" on every wrap). Log: `swing line: L kept through a wrap`. Off: laid anew. |
| `swing_rope_weight_pct` | 100 | **Rescaled.** The line's mass a metre is 2.5 times beta3f's at the same percentage (100% now = beta3f's 250%), and the pull's cap at full strength is 6 m/s^2 a hand (beta3f 2.5). |
| `kick_chamber` | 1 | **Look change (t100u).** An air kick's leg winds up (the knee drawn up, the thigh part way), snaps out through its line with a small overshoot held at full, and folds back as it recoils before easing home (`air_kick::shapedLegPose`); the kick layer comes in over 60 ms (0.2 s before: a 0.16 s kick was about 80% shown at its fullest). Off: the stiff leg, as beta3f. |
| `kick_lean` | 1 | An air kick leans the upper body on the torso channel: it curls over the chambered leg and turns toward it (the hips' turn, shown by the spine), then throws back as the leg snaps out (dives arch back 25 to 30 degrees, a roundhouse twists against the sweep, a side kick leans off it). Off: the legs only. |
| `kick_momentum` | 1 | An air kick's legs carry the body's travel (up to 0.35 rad at 10 m/s, less as the kick reaches full) and swing in over 70 ms rather than 40. Off: as beta3f. |
| `kick_target_ring` | 1 | With the legs engaged (Y + X), a turning amber ring round the enemy a kick would take now: the gaze's within reach (the lunge's on top), else the nearest in front, else the punches' lock (`air_kick::preview`, sent every frame then). |
| `kick_ground_lunge` | 1 | **Feel change.** A standing kick at an enemy past the leg's reach (up to KICK LUNGE's reach, at most 2 m) steps you in along the ground (the quick sidestep's drive: it stops at an edge or a wall), at most 7 m/s; the kick goes on while the burst runs. Never a sweep. |
| `kick_rebound` | 1 | **Feel change.** A kick that lands in the air springs you off him: after the blow's recoil, 3 m/s up and 2.5 m/s back along the kick (`Swing::kickImpulse`). |
| `kick_rope_assist` | 1 | An air kick at an enemy while a web holds you drives the swing kick's assist (the web's length toward his distance from the anchor, your velocity turned at him) while the leg goes out. |
| `kick_ground_speed_cms` | 450 | (100 to 1000) the fastest you may move and still start a standing kick (it may go on 1.5 m/s faster). 300 was the fixed value. |

Launcher: "Web rope (beta3g)" on the Shooters page and "Kicks (beta3g)" on the Kicks & Combat page. NA HELP: WEBS (14)
to (16), AIR KICKS (7) to (11), GROUND KICKS (3), ROPE WEIGHT and the ground kicks' speed updated.

### The logs behind it

**beta3e log (Nov 22 10:29, build 4272657, 46 min):** 860 swing-line let-gos. The most pull a line reached: median
0.74, p90 2.50 m/s^2 - 236 (27%) sat at the 2.5 cap (ROPE WEIGHT 200%, strength 50%). Most slack: median 3.2 m, p90
15.2 m, max 208 m; 311 let-gos over 5 m. Wraps 515, unwraps 68, obstruction cuts 16; 240 let-gos were "round
edges", and 81 re-catches on the same hand within 0.6 s. Only 16 of 860 catches had no web fired on that hand in the
0.6 s before: the game hardly ever makes a new web of its own - the "new line" seen on a wrap was our own rope being
laid afresh when the free run's length jumped (`tether::Rope::step`'s re-lay past a 50% change of its rest length).

**beta3f log (Nov 22 11:41, build 942f3ff, 7.6 min):** kicks fire - 32 air flicks, 26 kicks started (15 freeform,
6 dive, 5 double), 13 swing-kick flicks; 37 misses, 0 hits, 2 refused as busy, 1 on a wall, 9 swing kicks refused
"not swinging". 24 of the 26 starts had no enemy in a fight; one started with 4 in the fight and the nearest 1.1 m
(behind: no target), one lunged 0.5 m at 1.9 m/s. So beta3f's reach was barely tried in a fight; the owner's read
(t100u) is that kicks fire but look stiff - beta3g's kick work is mostly their animation. Rope: 200 let-gos, most
pull median 0.41, p90 2.50 (26 at the cap), most slack median 2.0 m, p90 9.4 m; wraps 103, unwraps 14, cuts 3.
Point jump: 33 jumps off a surface, 27 cancelling a wall grab, 4 refused "no contact" (A and B went on to the game),
and one on the game's floor-crawl flag with no duct roof that beta3e would have refused.

### What stays the game's (needs a new hook or is not ours)

- The swing's **obstruction cut**: a web whose line is blocked with no edge to bend round (or already 6 bends) is
  released after 0.15 s (`swing.cpp`, 16 in the beta3e log, 3 in beta3f's). Our rope keeps its line through every
  wrap and unwrap the swing makes; a cut is a real let-go.
- The game's own **web dissolve** at the end of a fade, and the **connected web line** (stick click), drawn taut by
  the game.
- The rope's hand points: the game draws a swing line through at most 6 points of ours; with many edges fewer are
  left for the slack (`6 - edges`).
- Kicks: no game kick animation or kick sound ids are captured, so the legs are ours (leg_layers, the torso
  channel) and the blow is the web strike's; the hips have no channel of their own (the spine's twist shows it).
- Legs mode without the Y + X chord (approved "if cheap"): not done - the right stick is the camera's and the
  gadget's without it, so a chord-free flick needs the input map reworked (backlog).

### beta3g checklist

1. **Steady rope.** Swing hard with slack out, then hang: the line should settle within a couple of seconds with no
   jitter or buzzing, and no midair "stall" when you give slack. Try at your usual frame rate and with ASW/reprojection
   off. `swing_rope_stable=0` brings beta3f's back for comparison.
2. **Taut under load.** Swing against the web at ROPE WEIGHT 100%: the line should snap straight as it takes your
   weight and the slack fall back in as you come over the top or fall toward the anchor.
3. **Wraps.** Swing round building corners: the line should bend round the edge and keep its shape - no new line.
   Log: `swing line: R kept through a wrap: 1 edge(s) now`. A blocked web with nowhere to bend still lets go.
4. **Rope weight.** 100% should now feel like beta3f's 250%; try 50% and 150%. The log's `rope pull:` line reports
   the steps, taut and wrap switches.
5. **Kick animation.** In the air, every flick: the leg should chamber, snap out, and fold back; the chest leans
   in and throws back; the legs come in with your travel. Compare `kick_chamber=0`, `kick_lean=0`, `kick_momentum=0`.
6. **Target ring.** Y + X near thugs: an amber ring on the one a kick would take; look between two to move it.
7. **Ground lunge.** Standing 2 to 3 m off a thug, kick: you should step in and land it. Walking at up to 4.5 m/s
   still kicks.
8. **Rebound and rope assist.** Land an air kick: you spring up and back. Kick while on a web: the swing steers at him.

## beta3h-polish: runtime, saves, and the quit confirm

The owner, Nov 24 t45u: start the polish pass on the Crime Run port. The Drive folder of logs from his webs and
kicks sessions could not be listed (it asks for a Google sign-in), and Crime Run itself has not been played in the
headset yet. This build is the safe half of that plan: docs, runtime, error handling, and two state bugs. No
play-feel change, no new setting, and no migration revision (still 23). Off still equals beta3g with Crime Run off.

| Key | Default | What |
| --- | --- | --- |
| (none) | — | No new `spidy_vr.ini` key. Crime Run stays off by default, as in the port. |

What changed:

- **A failed save is tried again.** If writing the SPIDY VR tab's own settings fails, the last good text is kept, so
  the next frame tries again instead of treating the failed write as saved.
- **Quit confirm during a pause.** Crime Run's 3 s "A twice to quit" window no longer runs down while the game is
  paused, loading, in a menu or on the flat screen. The second A still quits after you come back.
- **Swing asks while paused.** While a run is frozen, Crime Run does not ask the swing for enemies or ground.
- **Overlay lists reuse their buffers.** The aim marks and the translucent vertices (Crime Run's pillars and HUD go
  here) are kept on the session and cleared each eye, instead of a new heap allocation every frame.
- **Crime Run's HUD text.** Score, clock and event lines are formatted into stack buffers rather than building a
  `std::string` for every line, every eye.
- **Help and docs.** NA HELP has a Crime Run topic, and the close-code line now asks for a `.mdmp` if you have one.
  The missing beta3d, beta3f and beta3g banners are in this file. The work-fork landing page and the Native
  Animation repo description name CURRENT as 0.8.7-na-beta2 (text only; the CURRENT release itself is unchanged).
- **Tests.** Crime Run's pause-confirm contract, and a portable "off equals the previous build" check for revision
  23 and Crime Run's off default.

### What was left for later

Items that need Crime Run's own headset logs, a crash dump from a 0xC0000409 close, or a play-feel go from the
owner, are listed in `/workspace/polish-pass-notes.md` (and the plan). Nothing in that list is in this package.

### beta3h-polish checklist

1. **Same as beta3g when Crime Run is off.** Webs, kicks, point jump, Showtime and the rope should play as in the
   Crime Run zip (`1236723`) and beta3g. A saved off stays off.
2. **Settings save.** Change a SPIDY VR tab value, confirm it sticks after a restart. If a write ever fails, the
   "were not saved" line should appear and the next change should try again.
3. **Quit confirm (once you try Crime Run).** Start a run, open the phone, press A once to confirm quit, then pause
   the game for more than 3 s. After you resume, a second A should still quit. A pause mid-run should freeze the
   crime timers, as before.
4. **Close crash.** If the game still reports 0xC0000409 as it exits, keep the `.mdmp` if one is written (alongside
   the logs). It is known since beta3d; this build does not guess at a fix.

## beta3i: web in hand, thick swing line, grip and fists

The owner, Nov 25 t49u-t53u: prepare the next build (skip further beta3h testing). A held web was leaving from the
wrist shooter while the fist closed over empty air (t51u: "I swing from the wrist shooters"). That is not a beta3
regression: since October 18 every web leaves from the suit's nozzle (`webShooter`, the rig's webStart joint),
which is right for a shot and left a gap once the hand closed on a hold. The swing physics rope is the game's own
web, laid by `swing_line`; it looks thin in the headset (t52u). This build puts a held line in the fist, thickens
the visible line, and ships the wording, WEB GRIP and PUNCH FISTS work prepared locally after beta3h.

| Key | Default | What |
| --- | --- | --- |
| `animated_web_grip` | 1 | While the animation has the body, a hand on its web closes on the line and opens when it lets go. Off: the animation's fingers stay, as in beta3h. |
| `animated_punch_fists` | 1 | While the animation has the body, a punching hand closes into a fist. Off: the animation's fingers stay. The VR-owned body already fists a punch. |
| `web_hold_in_hand` | 1 | A held web leaves from inside the closed hand (the measured knuckles, 15.5 cm along the hand from the web shooter, [BODY.md](BODY.md)). A shot still leaves from the wrist shooter. Off: every line leaves from the shooter, as in beta3h. |
| `thick_swing_line` | 1 | A thicker, brighter overlay (2.4 cm) follows the swing physics rope so it is easier to see. Off: only the game's own web, as in beta3h. |

Launcher: **NA · WEB SHOOTERS** tab, after FULL-LENGTH SWING LINES: **WEB GRIP**, **PUNCH FISTS**, **WEB IN HAND**, **THICK SWING LINE**.
Settings files from before take migration revision 24: those four come on once (a 0 chosen after it stays).

What changed:

- **WEB IN HAND.** While a hand holds its web, the game's rope and the overlay start at the knuckles, not the
  wrist shooter. A shot, and a hand that is not holding, still leave from the shooter. The same rule applies
  whether VR or the animation has the body.
- **THICK SWING LINE.** The physics line is still the game's own web mesh. A 2.4 cm bright overlay now follows the
  same line so it stays readable against the sky.
- **WEB GRIP and PUNCH FISTS.** While the animation has the body, a hand on its web closes on the line, and a
  punching hand balls into a fist, using the fist measured from the game's own model (no verified grab/punch clip
  IDs). Off keeps the animation's fingers, as in beta3h. The VR-owned body already did both. Kicks do not close
  the hands.
- **Log wording (no play-feel change).** Closing the game window is no longer reported as a 0xC0000409 crash
  unless a new session `.mdmp` exists. A swing-kick flick that starts an air kick no longer looks like a missed
  kick in the log.
- **Still in from beta3h-polish.** Crime Run (off by default), the save retry, the quit-confirm and swing-ask
  freeze during a pause, and the overlay / HUD reuse.

AIR FISTS and a squeezed grip still close a fist as in beta3h; the empty-fist look while swinging was the line
leaving from the wrist, not those poses.

### beta3i checklist

1. **Held web in the hand.** Swing with VR driving the body: the line should run out of the closed fist, not from
   the wrist shooter. A new shot should still leave from the shooter, then move into the hand once it holds. Turn
   WEB IN HAND off: it should look like beta3h again (line from the shooter, fist over the line's start).
2. **Animation body.** With Native Animation on and WEB GRIP on, a hand should close on its held line and open
   when you let go. A punch should fist (PUNCH FISTS). Off: the animation's fingers stay.
3. **Thick swing line.** The swing line should be easier to see, including against the sky. Turn THICK SWING LINE
   off: only the game's own web, as in beta3h.
4. **Shots and punches.** Web balls, aim markers and punches still leave from / land at the shooter and knuckles
   as before. Kicks do not close the hands.
5. **Same as beta3h when the new switches are off.** Webs, kicks, point jump, Showtime and the rope should play as
   in the polish zip (`5c1c749`). Crime Run stays off unless you turn it on.
6. **Force close.** Closing the game window should not be reported as a 0xC0000409 crash. If a real crash writes a
   new `.mdmp`, keep it with the logs.


## beta3j-diag: flushed VR-start log for the Meta 1.210 crash

The owner, Nov 25 t54u–t57u: beta3i, then a rolled-back NVIDIA driver, then a Crime Run
port launch (`1236723`, not vanilla beta3g `c488634`) all died the same way after Meta
Quest Link updated from 1.208 to 1.210. Official 0.8.8 is not a fix for this and was
not ported. This build does not change how the game plays. It writes a flushed step
log through the first headset frame so the next crash names the last OpenXR call.

What the earlier sessions showed:

- The crash is a stack overflow (`0xC00000FD`) in NVIDIA's user-mode D3D driver,
  about 3 seconds after the headset session goes FOCUSED, after 56–59 VR-loop frames
  and **no** frame submitted to the headset. None of the new beta3i hand or line code
  had run (no web, no punch).
- It is the same on the Nov 20 NVIDIA driver and the Oct 27 driver after a rollback.
  The Nov 20 driver is not the cause; going back to it is fine.
- It is the same on the Crime Run port (`1236723`, Crime Run off, revision 23) as on
  beta3i (`3c7bb41`). That port does not change the OpenXR or D3D start path versus
  beta3g, so the later NA commits are not required for the crash.
- The last good headset sessions were beta3g on Oculus runtime **1.208**. From late
  morning on Nov 25 the machine offers **1.210** (37 extensions instead of 36). This
  console sometimes omits the "openxr: runtime Oculus …" line; the start log always
  writes it, and the full list of offered extensions.
- Official 0.8.8 (`cf0b279`) adds first-time VR setup and the comfort vignette. It
  does not mention this crash. The launcher's Update button is only a notice that
  Ilya released 0.8.8; **do not press it**, or this folder would be replaced by
  official 0.8.8.

What this build adds (no new settings, no new migration revision):

- **`spidy-vr-start.log`**, beside the stereo module in the Spidy folder (and also
  in the usual `-diag.log` once the launcher can poll). Each OpenXR wait, begin,
  acquire, copy and end is written and flushed to disk *before* the call and again
  after it succeeds, until the first headset frame is submitted. If the game dies
  in the first seconds, the last line is the last call that started.
- The 3-second "no eye images" stop is also written first, so it cannot be lost in
  a crash during unwind.

### beta3j-diag checklist

1. **Use this zip, not official 0.8.8.** Leave the launcher's Update button alone.
2. **If VR starts and a frame reaches the headset**, play as in beta3i. The new log
   should end with "first headset frame submitted". You can then look at Crime Run
   and the hand / line work as before.
3. **If it crashes again**, upload the usual logs *and* `spidy-vr-start.log` from the
   Spidy folder (next to the stereo module, the same place as this zip's DLLs), plus
   a new `.mdmp` if the game writes one.
4. **Other runtimes.** If you can, try Virtual Desktop or SteamVR as the OpenXR
   runtime. A good session there would confirm this is Meta 1.210's path. Rolling
   Meta Quest Link back to 1.208, or opting out of a Public Test Channel build if
   1.210 arrived that way, is also worth a try.


## beta3k: start the GPU bridge before the headset goes focused

The owner, Nov 25 t58u–t59u: beta3j-diag (`6fa2418`) crashed again on the current
NVIDIA driver, Meta runtime **1.210.0**. The flushed start log (also copied into
the console) names the last call.

What the 13:03 PM Pacific session showed:

- Build confirmed: **0.8.7-na-beta3j-diag**, commit `6fa24188a0`, folder
  `C:\Games\Spidy-0.8.7-player-torso-6fa2418`.
- Runtime: **Oculus 1.210.0**, 37 extensions offered; only `XR_KHR_D3D12_enable`
  was enabled (the new 37th extension is not used). The offered-name line was
  cut off in the log after `XR_KHR_convert_time`; this build writes the list in
  shorter lines.
- Frames 1–33: `shouldRender=0`, empty `xrEndFrame` (0 layers) all succeeded.
- Then the session went VISIBLE and FOCUSED. Frame 34: `xrWaitFrame` returned
  `shouldRender=1`, `xrBeginFrame` returned ok. **Headset active was never
  logged.** The crash is a stack overflow (`0xC00000FD`) in NVIDIA's user-mode
  driver `nvwgf2umx.dll+0x396e7e` (the Nov 20 driver, the current one). The
  faulting thread's stack was not in the dump, but the last Spidy step is
  certain: we died inside `native_gpu::start` (Present hooks and a throwaway
  swapchain) on the first focused frame.

This build starts that GPU bridge on an earlier unfocused frame, while the
compositor is still not rendering, and keeps the usual "Headset active" pause
for the first focused frame. How the game plays is unchanged. No new settings,
no new migration revision. The start log now also names each step inside the
GPU start (D3D lists, command hooks, present hook swapchain).

### beta3k checklist

1. **Use this zip, not official 0.8.8.** Leave the launcher's Update button alone.
   The current NVIDIA driver is fine.
2. **If VR starts and a frame reaches the headset**, play as in beta3i. The start
   log should show `GPU bridge started` *before* the session goes FOCUSED, then
   `first headset frame submitted`.
3. **If it crashes again**, upload the usual logs *and* `spidy-vr-start.log` from
   the Spidy folder (next to the stereo module, under `reports/stereo-modules\…\`,
   or as the console line `writing flushed steps to …` names it), plus a new
   `.mdmp`. The last line will say which GPU-start or OpenXR step it died on.
4. **Other runtimes.** Virtual Desktop or SteamVR as the OpenXR runtime, or
   rolling Meta Quest Link back to 1.208, remain useful if this still dies.

## beta3l: start the GPU bridge before the OpenXR session begins

The owner, Nov 25 t60u: beta3k (`990d384`) still crashed. The start log this time
names the exact step.

What the beta3k session showed:

- Build confirmed: **0.8.7-na-beta3k**, commit `990d38424c`, folder
  `C:\Games\Spidy-0.8.7-player-torso-990d384`.
- Runtime: **Oculus 1.210.0**, 37 extensions offered; only `XR_KHR_D3D12_enable`
  was enabled. The 37th offered extension is `XR_LOGITECH_mx_ink_stylus_interaction`
  (not enabled).
- Session create and begin both succeeded. Frame 1: `shouldRender=0`,
  `xrWaitFrame`, `xrBeginFrame` and `xrLocateViews` all returned ok, then
  `prepare` started the GPU bridge.
- **Last flushed line: `GPU creating D3D12 lists`.** There is no
  `GPU enabling command hooks`, no present-hook swapchain, no
  `GPU bridge started`, and the session never went FOCUSED. Moving the start
  to an earlier unfocused frame did not help, because that start still ran
  after `xrBeginFrame`, in the middle of a compositor frame.
- The overlay had already created D3D12 objects on the game's device
  successfully after `xrCreateSession` and before `xrBeginSession`. So the
  overflow is not "any command list after the session exists"; it is creating
  the GPU bridge's lists after `xrBeginFrame`.

This build starts the GPU bridge right after the overlay is created, before
`xrBeginSession` and before the first `xrWaitFrame`. The start log now names
each CreateCommandAllocator / CreateCommandList / Close / CreateFence step.
How the game plays is unchanged. No new settings, no new migration revision.
There is no playable fallback that skips the GPU bridge (the headset would
get no eye images). Virtual Desktop, SteamVR, or Meta Link 1.208 remain
workarounds if this still dies.

### beta3l checklist

1. **Use this zip, not official 0.8.8.** Leave the launcher's Update button alone.
   The current NVIDIA driver is fine.
2. **If VR starts and a frame reaches the headset**, play as in beta3i. The start
   log should show `GPU bridge started` *before* `begin session`, then later
   `first headset frame submitted`.
3. **If it crashes again**, upload the usual logs *and* `spidy-vr-start.log` from
   `reports\stereo-modules\…\`, plus a new `.mdmp`. The last line will now
   name the exact GPU-create or hook step, if it dies during start, or the
   OpenXR call if it dies later.
4. **Other runtimes.** Virtual Desktop or SteamVR as the OpenXR runtime, or
   rolling Meta Quest Link back to 1.208, remain useful if this still dies.

## beta3m: a bigger VR thread stack for the 0xC00000FD crash

The owner, Nov 26 (t133u, t135u, t138u): beta3l still crashed, on Meta Quest
Link 1.210 and on SteamVR alike. Its logs showed that the start itself was fixed.

What the beta3l sessions showed (Nov 26, 7:34 to 9:35 AM PT, Dev3 Drive folder):

- Build confirmed: **0.8.7-na-beta3l**, commit `3baaecff59`, folder
  `C:\Games\Spidy-0.8.7-player-torso-3baaecf`, on all six sessions.
- Runtimes: **Oculus 1.210.0** (37 extensions) and **SteamVR/OpenXR 2.18.4 in Meta
  compatibility mode** (42 extensions).
- The start log shows `GPU bridge started` *before* `begin session`, then
  `first headset frame submitted` and `running (300 frames submitted)`. No
  start step dies any more.
- Three sessions then died 20 to 30 s in, during the opening cutscene or
  gameplay: two on Link and one on SteamVR. Each left an `.mdmp` with
  `0xC00000FD` (stack overflow). The faulting function differed each time:
  ucrtbase, spidy_ray_bridge, ntdll. Two Link sessions closed normally.
- In all three dumps the overflowing stack is **the VR worker thread's**, the
  default **1 MB**, overflowing at about 1003 to 1008 KB. Its outermost function,
  `run()` in `src/game_xr.cpp`, reserves a fixed **867 KB** frame per the
  DLL's unwind data. Most of that is `NativeEyeHistory` (256 eye frames of
  about 3.2 KB each). That left about 150 KB for everything run() calls, so the
  OpenXR runtime, D3D11/D3D12 and the NVIDIA driver ran out of stack. The frame
  record grew on Nov 22 and 23 (kick target ring, Crime Run beacons and HUD).

This build:

1. Starts the VR worker with a **16 MB reserved stack**, committed only as used
   (`CreateThread` with `STACK_SIZE_PARAM_IS_A_RESERVATION`).
2. Moves the **eye-frame history to the heap**, so run()'s own frame is small again.
3. Writes the VR thread's stack bounds to `spidy-vr-start.log` once
   (`GetCurrentThreadStackLimits`).

How the game plays is unchanged. No new settings, no new migration revision.

### beta3m checklist

1. **Use this zip, not official 0.8.8.** Leave the launcher's Update button alone.
2. Right after its `writing flushed steps` line, the start log should read
   `vr start: VR thread stack 16384 KB reserved (...), N KB in use at start`,
   with N a few KB, not hundreds.
3. **Play past a minute on Link 1.210**, through the opening cutscene and into
   gameplay. Then do one **SteamVR** run.
4. **If it crashes**, upload the usual logs, `spidy-vr-start.log` and the new
   `.mdmp`.

## beta3p: Level keeps him with you, HEAD LEVEL by default, the smoothing clock fix

The owner, Nov 26 (t151u), after trying the HEAD choices: FULL's choppiness goes with LEVEL, and "Level was the
one that felt like something that should be default, felt great. The only problem was it didn't track all
animation moves, so the dodges didn't roll, and the body and the head had no cohesion, so you'd frequently be
looking backwards after moves in animation. If we could lock it better, while still keeping it relatively level,
it would be an improvement". His answer to the plan (`plans/level-head-cohesion-1126.md`): "Build A and make
LEVEL the default, Build the smoothing clock fix". Option B (a dodge tilt) and C (reprojecting the animated
rotation) are not built.

**Why LEVEL lost him.** LEVEL froze its heading from the head bone at the start of each yield and at every cut,
and nothing turned the hero back under that frozen heading - FULL's head lock (the HYBRID heading, the landing
turn, the facing check, turn-in-place, the HEADSET FIRST stick basis) was FULL-only. After a dodge, a roll, a
turn-around or a landing on the game's stored facing he faced one way and the frozen view another. The latch
could also jump mid-move: the one-metre cut test fires at a fast dive's speed and re-latched onto wherever the
head bone faced, and a latch while the head looked steeply up took the crown's yaw.

This build:

1. **LEVEL KEEPS HIM WITH YOU** (`level_head_lock=1`; `level_head_lock.hpp`). With HEAD LEVEL, HYBRID movement and
   HEADSET FIRST (all three are the defaults), LEVEL joins FULL's head lock: the view's turn is the lock's - your
   headset, the right stick (smooth or snap turn), snap realign, a recenter - and the animation never turns it.
   The game's own turns bring Spider-Man back under the view after a dodge, roll or landing (the landing turn,
   the facing check, turn-in-place, and the stick taking the view again), as in FULL. The view stays level: no
   pitch or roll from the bone, so it is as smooth as LEVEL was. The eyes and the hands now read the one published
   heading (the eyes' level frame is built from it every frame), so they can no longer disagree after a latch.
   Where the lock is not live (another movement mode, HEADSET FIRST off), LEVEL keeps its latch but no longer
   re-latches on the one-metre test during a fast dive or swing, and never latches onto the crown's yaw while
   the head looks steeply up or down (it waits for a head that faces somewhere). A first-person scene's level
   frame is unchanged. Off: LEVEL as beta3o. Dodges still don't roll the view - that was Option B, not built.
2. **HEAD LEVEL is the default** (`animated_head=1`). Settings revision 27 turns a saved FULL (the old default)
   into LEVEL once; a saved OFF stays, and FULL chosen after this build stays. NATIVE ANIMATION on now applies
   LEVEL. The game's HEAD row, the launcher's Head row and NA HELP say so. Going back: HEAD FULL.
3. **SMOOTHING CLOCK FIX** (a bug fix, no setting). The animated view's smoothing (CAMERA SMOOTHING and CAMERA
   MOTION's bob) took its time step from a clock that moves in 15.6 ms steps, so at a 20 ms game frame each step
   read 15.6 or 31.2 ms and the glide was uneven; and the head is resolved twice a frame, which could step the
   filter twice. It now uses the performance counter and steps once a frame, on the newest head. The level
   latch and the cut test, shared by the eyes' and the hands' threads, are now under one lock.

No view yaw is written by the animation; the headset heading stays the only direction authority. Settings
revision **27**. Level keeps him with you is in the launcher's **Native Animation** group, after Head. Not
verified in a headset yet.

### beta3p checklist

1. **The default.** Start with your saved settings: HEAD reads LEVEL (game NA tab and launcher). The log has
   `view yaw lock: HYBRID latched ...` on the first yield, and `view yaw: ... level` lines every half second.
2. **Cohesion.** HEAD LEVEL: dodge, roll, do a 180 turn-around, land from swings, run a combat chain. You should
   never be left facing his back or side; after each move he turns back under you within the game's turn time
   (about a quarter second standing). Stick and hands match what you see.
3. **Turning.** Right stick (smooth or snap) and the snap realign taps turn the view as in FULL; a recenter puts
   the view where you look.
4. **Fast moves.** A fast dive and a fast swing in LEVEL: no sudden yaw pop. Backflips and wall runs up: no
   frozen view behind him afterwards.
5. **Still smooth.** LEVEL should feel as smooth as beta3o's LEVEL. With SMOOTHING on (Low to Max) the glide
   should be even, with no frame-to-frame unevenness.
6. **FULL against LEVEL.** Switch HEAD to FULL and back, for the default decision. FULL plays like beta3o
   (apart from the smoother smoothing).
7. **Off check.** Level keeps him with you off (launcher, Native Animation): LEVEL plays like beta3o.
8. **Logs to send.** A short LEVEL session's log: the direction audit's imposed view turn should sit near 0, and
   `landing rear turn` / facing-check lines should show the hero turning onto the view.

## beta3o: grip locks the wall jump, a wall slide pose, the held line inside the fist, a rope grip

The owner, Nov 26: "Only point jumps should work through the grip. Normal jumps should stay locked. Also, we need
an animation for wall sliding. Maybe we can pose his legs like the image whenever he's sliding, similar to the old
classic Spiderman 2 Playstation game" (t147u); "I want an animation so that Spiderman actually holds the line while
swinging. Currently, it just floats outside his fist" (t148u), and yes to options A and B of the web-in-hand study
(t149u: the fist-centre line and the rope grip; C, the wrist and elbow along the rope, is held).

**Why the line floated.** WEB IN HAND (beta3i) started a held line at the punch point, 15.5 cm along the hand from
the web shooter: 5 cm past the grip pose's point, the front of the fist. The game draws its web where we say
(beta3m's `web_hand_gap_mean_m` 0.0098), so the start point was the whole of it.

This build:

1. **WALL SLIDE POSE** (`wall_slide_pose=1`; visual only, `leg_layers::wallSlidePose`). While SpidyVR's own wall
   slides you (GRIP TO STICK, before a grip sticks you) mostly down or slowly, his legs take a braced crouch:
   the left thigh 1.2 rad up and 0.55 out with the knee bent 1.2, the right 0.7 up and 0.5 out with the knee 1.1
   (within leg_control's limits), eased in and out by the legs' spring, turning nothing on the body. The wall
   crawl's planted legs and the wall run's stride stand down while it shows. A slide the stick runs along or up
   the wall keeps the wall run (it starts while the speed across is under the speed down plus 0.3 m/s, and stays
   while under 1.5 times it plus 0.6). **An approximation:** the owner's reference picture could not be read by
   the build, so the pose is the classic wall-slide crouch as described; the angles are one table in
   `leg_layers.hpp` to match the picture. Off: the crawl's or the run's legs (beta3n).
2. **GRIP LOCKS THE WALL JUMP** (`wall_grip_jump_lock=1`; `grip_jump.hpp`). On one of SpidyVR's walls with a grip
   squeezed past 0.65 (Grip to stick's squeeze) on a hand holding no web, a press of A is no jump - not the swing's
   wall jump, not the game's A - until it is let go of. Letting go of the grip mid-press does not jump; a press
   begun before the grip goes on. A+B, the point jump, is read before the lock and fires through the grip (as
   POINT JUMP CANCELS WALL GRABS has it). The handstand's grip has held A since beta3n. The game's own wall crawl
   has no hook and is unchanged. Off: A wall jumps with a grip held (beta3n).
3. **WEB IN HAND: the line from the fist's channel** (a fix inside `web_hold_in_hand`, no new key). Each body solve
   measures the closed fist's loop from the drawn hand's own finger bones (`body::solvedFistChannel`: for the
   middle and ring fingers, the midpoints of the bone in the palm and of the bone after the knuckle, averaged; no
   constant) and publishes it in the solved grip's frame. A held line starts there on the drawn glove - both
   hands, the one-web-in-both-hands strand, the XR loop and the eye-frame history alike. With no solved hand it
   starts at the OpenXR grip pose's own point (the centre of the grasp), not the knuckles. A channel more than 15
   cm from the grip point is refused. Visual only: the swing, aim and punches don't read it. Off (web in hand
   off): the shooter, as before. On the test rig's hero hand the channel sits 3.0 cm from the grip point, where
   the old point sat 5.1 cm out past it.
4. **ROPE GRIP** (`web_rope_grip=1`; fingers only, `hand_pose.hpp`, `body_ik` poseFingers). A hand on a held web
   closes fully - with the animation owning the body too, where the squeeze is zeroed and the fist followed the
   web button's travel - and the fist's thumb tucks onto the index finger's middle bone (its target moves from the
   first two fingers' middle bones to the index's alone, at the fist's own thumb rest), a ring the line comes out
   of. It eases out with the fist on release. The measured fist bends are unchanged; no wrist, forearm or elbow
   change. Off: the beta3n fist.

No view yaw is written; nothing else changes how the game plays. Settings revision **26** turns the three new
switches on once; a 0 chosen after it stays. Wall slide pose and Grip locks the wall jump are in the launcher's
**Walls** group (Legs page); Rope grip is in the Combat group after Punch fists. Not verified in a headset yet;
the fist channel and the thumb tuck want one look against the game's own hand (`probe_game_body.py --fists 1`).

### beta3o checklist

1. **Slide pose.** Jump at one of SpidyVR's walls and slide down it without gripping: his legs brace in the
   crouch, knees up and wide. Look down at them and in a mirror view or recording. Grip to stick: the crawl's legs
   come back. Push the stick at the wall to wall run: the run's legs. The log has `legs: wall slide pose on`.
   Tell me how far the pose is from your picture (which leg higher, how wide, how bent).
2. **Jump lock.** Grip the wall (palm on it, squeeze) and press A: nothing. Keep A held and let go of the grip:
   still nothing. Release A and press it again: you wall jump. The log has `grip jump lock: A held back`.
3. **Point jump through the grip.** Gripping the wall, A+B: the point jump fires off the wall.
4. **Held line.** Swing and look at each fist, with the anchor overhead and out in front: the line should come out
   of the closed fist, not float in front of the knuckles. Two-hand hold: both ends sit in the fists. The log has
   `web in hand: held lines from the fist's channel - left bones ..., right bones ...`.
5. **Rope grip.** On a held web the fist closes fully and the thumb sits on the first finger, while the animation
   has the body too (a swing begun from the ground). Let go: the hand opens as before.
6. **Off checks.** Turn each switch off in turn (and Web in hand off): that piece plays like beta3n.

## beta3n: wall running back, wall fling any way, sticky palms, grip-held handstands; the thick swing line gone

The owner, Nov 26 (t141u): beta3m ran with no crash. He asked to remove the extra white line along the swing
web, to leave a rule for bots (nothing without his signoff), and to bring wall running back with his wall feel.

**Why wall running broke.** GRIP TO STICK (beta3e, `6657b76`) returns from `Swing::wallMotion` with the slide
step for every wall the body is not stuck to. That return sits before the stick's wall run. A wall met in the air
slides until a grip sticks it, so the stick never ran it, and gravity plus the 50% friction took the run's
speed. Only after a grip stuck you did the stick crawl the wall again.

This build:

1. **THICK SWING LINE is removed** (`thick_swing_line`, beta3i). Swings show only the game's own web, as before
   beta3i. An old file's key is ignored. The web is not made more visible: no documented native web width or
   brightness parameter exists (docs/REFERENCE.md, docs/VALIDATION.md, `native_webs.hpp`), and no new line or
   overlay is added.
2. **No additions without the owner's signoff**: [AGENTS.md](../AGENTS.md), [HANDOFF_NA.md](HANDOFF_NA.md) §2,
   [CONTRIBUTING.md](../CONTRIBUTING.md).
3. **WALL RUN PUSH** (`wall_run_push=1`, `wall_run_blend_ms=250`). Near a wall you slide, as in beta3e to beta3m.
   Push the stick toward the wall and the slide blends into the wall run in the stick's direction over the
   blend time: straight at the wall runs up it, angled runs up and along it, and a faster run keeps its speed.
   Let go of the stick, or point it only along the wall, and it blends back into the slide. Off: the stick does
   nothing on a slide (beta3m).
4. **WALL FLING ANY WAY** (`wall_fling_any_way=1`). Holding a wall with a grip, a grip fling leaves the wall along
   the whole fling once any of it (0.3 m/s) points out of the wall: up, down, sideways or out. A fling into the
   wall still strokes along it. Needs GRIP FLING (on by default). Off: only a push of 2 m/s or more straight out
   leaves (beta3m).
5. **STICKY PALMS** (`sticky_palms=1`, `sticky_palms_pct=100`). An open palm (grip not squeezed) touching the wall
   you slide or run on holds softly, only while it touches. Held still it slows the slide: at 100%, a 6 m/s slide
   drops under 1 m/s in about half a second. It never holds you like a grip. Swept, it strokes: you gain 1.3
   times the sweep the other way, and the drag fades out as the sweep reaches 2 m/s. Sweep it back along a wall
   run to go faster, down to lift, or the way you are going to brake. Off: a palm does nothing until it grips
   (beta3m).

6. **GRIP HOLDS THE HANDSTAND** (`handstand_grip_hold=1`; the owner, Nov 26 t146u: "I want handstands to lock
   easily, and grip to be used to 'hold' the stance"). Upside down near the floor (inverted at least the catch
   angle, 60 degrees, spinning under 5 rad/s), a squeezed grip is the intent: the hands-over-head test and its
   hold wait are skipped, and the palms count from 2.0 m over the floor (the tip-in's reach) instead of 1.6 m.
   While a grip is held the handstand is locked: no fall (the centre of mass is kept inside the palms'
   support), no step-down or miss, no push, no leap; the grip shoots no web and A is no jump. Let go of the grip
   and everything is as before. A push or a stick still held as you let go fires at once: letting go is the
   launch, as with the grip fling. A lost tracking still ends it. Off: a squeeze after the catch ends the
   handstand (beta3m). On walls the same rule already holds: a squeezed grip sticks you, an open palm is the
   soft sticky palm, and letting go of a grip is the fling.
7. **Launcher text.** A stalled headset picture no longer blames Wi-Fi. That line came from the PC's own network
   adapter, not from the headset's connection (the owner is on a Link cable).

The wall pieces are on SpidyVR's own walls only. The game's own wall crawl and wall run, and the ground, have no
validated hook (docs/REFERENCE.md), so sticky palms do nothing there. The wall jump and its sound are unchanged.
No view yaw is written. Settings revision **25** turns the four switches on once; a 0 chosen after it stays.
The wall tunables are in the launcher's **Walls** group (Legs page), next to Grip to stick; Grip holds the
handstand is in its **Handstand** group.

### beta3n checklist

1. **The web.** Swing: only the game's own web shows, with no second white line.
2. **Slide.** Jump at a wall: you slide down and along it, as in beta3m.
3. **Wall run.** While sliding, push the stick toward the wall. Within about a quarter second the slide turns
   into a run up the wall, with no snap. Angle the stick to run up and along it. Let go and you slide again.
   Try a fast swing into a wall with the stick angled along it: the run should keep its speed.
4. **Blend.** Set **Wall run push: blend** to 0 ms (instant) and to 1000 ms, and feel the difference.
5. **Fling any way.** Grip the wall (palm on it, squeeze), pull, let go. Pull down and you go up and off. Pull
   sideways and you go sideways. Pull toward the wall and you go out. Pull away from the wall and you stay on it,
   carried along.
6. **Sticky palms.** Sliding, lay an open hand on the wall and hold it still: the slide slows but does not stop
   dead. Wall running, sweep the open hand backward along the wall: you speed up. Try **Sticky palms: strength**
   at 50% and 200%.
7. **Grip handstand.** Flip upside down toward the floor with a grip squeezed and your hands anywhere: the
   handstand should catch. Keep squeezing: lean, lift your palms, press A, push the stick - it stays. Let go and
   push into the floor: you launch. Let go and lift the palms: you step down. Grip again after the catch: it
   holds, it does not drop you.
8. **Off checks.** Turn each switch off in turn: that piece plays like beta3m.
9. **Logs.** The swing log line `grip to stick: ...; wall run push: on (250 ms); wall fling any way: on; sticky
   palms: on (100%)` confirms the settings reached the swing.

