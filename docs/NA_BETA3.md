# 0.8.7-na-beta3: headset checklist

> **Build beta3b** (same version, 0.8.7-na-beta3; package `Spidy-0.8.7-na-beta3b-win64-<commit>.zip`): adds the
> [beta3b section](#beta3b-showtime-auto-kinds-and-double-tap-a) below - Showtime's automatic camera split per kind
> with how-often controls, and double-tap A to turn upside down. Settings revision 18.
>
> **Build beta3c** (same version; package `Spidy-0.8.7-na-beta3c-win64-<commit>.zip`): adds the
> [beta3c section](#beta3c-webs-across-the-game-and-rope-physics) below. Full-length swing lines now work round
> edges, in both hands and while climbing, swing free until they fade, and hand over to stick-click web lines. A
> new switch (on, 100%) makes the hanging line really pull on the swing. Settings revision 19.

A **test build** (branch `feat/087-beta3`, from `feat/087-beta2` 1418992). The testers' **CURRENT** build stays
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
