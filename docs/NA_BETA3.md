# 0.8.7-na-beta3: headset checklist

> **Build beta3b** (same version, 0.8.7-na-beta3; package `Spidy-0.8.7-na-beta3b-win64-<commit>.zip`): adds the
> [beta3b section](#beta3b-showtime-auto-kinds-and-double-tap-a) below - Showtime's automatic camera split per kind
> with how-often controls, and double-tap A to turn upside down. Settings revision 18.

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
