# 0.8.7-na-beta3: headset checklist

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
| `showtime_auto_finisher_strike` | 1 | Showtime's camera steps out for a Y+B finisher or a web strike, back 0.3 s after it ends. |

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
