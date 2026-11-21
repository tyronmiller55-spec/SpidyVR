# 0.8.7-na-beta2: headset checklist

The testers' **CURRENT** Native Animation build since Nov 21 (branch `feat/087-beta2`, from `feat/087-beta1`
9e8d7cb), after the owner's Nov 21 session; it replaces [0.8.7-na-beta1](NA_BETA1.md). What changed: [CHANGELOG.md](CHANGELOG.md).
Every feature stays on; settings files from before take migration revision 15 (AIR DODGE, SHOWTIME RAY FAN and
STICKY HANDSTANDS come on once; a 0 chosen after it stays) and 16 (FULL-LENGTH SWING LINES, Nov 20). No existing numeric default changed; WALL JUMP SOUND
stays SOUND A. No new game address: everything below reuses hooks already in [REFERENCE.md](REFERENCE.md) and
[VALIDATION.md](VALIDATION.md).

## What the beta1 session showed (game-vr-20261119-130907, build 81fc830)

- **Finishers on Y+B are the game's own.** All 11 combat scenes the log called a finisher candidate came 0.4 to
  1.3 s after a B (often sent as the game's prompt, `the player's press of a button the game names -> 0xa000`)
  and a Y, the scene starting 0.08 to 0.1 s after the Y. The only interference: a grip cancel (`anim->vr`) at
  t=971.72 s inside the 3.97 s finisher that began at 967.875 s. So no SpidyVR finisher was built; beta2 names
  each one in the log and holds the grip cancel until it ends.
- **Point jump audio.** 97 `jump sounds: 1 of 1 queued for the point jump` lines and sound traces such as
  `point jump | ... dab33491@+1ms(spidy) a442c255@+32ms 050553e7@+32ms`: the grunt (`dab33491`) was ours on
  almost every point jump, but a grunt is a voice line, not a launch sound, and the game added its own
  `a442c255`/`050553e7` on only about a fifth to a third of them. Beta2 plays the whole jump set (as a ground
  flip already did) and logs whether the game played them.

## Headset checklist

1. **Finishers.** Y+B on a stunned enemy: `finisher: the game's own finisher on Y+B (...)`, and at the end
   `finisher: the game's finisher ended after X s; the grip cancel is back`. A grip squeezed during it does not
   cancel it.
2. **Point jump sounds.** Do you hear a launch on every point jump? Log: `jump sounds: point jump result: N of M
   played (...), K skipped (...), L lost`. Report if M played but nothing was heard.
3. **Air dodge (NA · KICKS & COMBAT, AIR DODGE).** In SpidyVR's air with no web held, tap B: a dodge the way the
   left stick points from your view (stick at rest: backward), a pulse on both hands, `air dodge: sent #N ...`
   then `air dodge: the swing took #N`. A+B still point jumps, X+B still Showtime, Y+B still the finisher
   (`air dodge: B went on to the game (chord)`); B on the ground is unchanged. Two an air, 0.5 s apart.
4. **Showtime ray fan (NA · SHOWTIME, SHOWTIME RAY FAN).** In Showtime near a wall or under a roof the camera
   pulls in instead of going through (`showtime: cam ... collide=rays+stock ... rays=N m (h of r hit ...)`); at
   swing speed it trails less (`lag=` lower than beta1's ~3.5 m, `lead=` up to 0.60).
5. **Sticky handstands (NA · BODY & LEGS, STICKY HANDSTANDS).** Catches come a little easier with the arms up;
   a short tracking drop no longer ends one (`handstand: sticky: the palms moved with the head ...`), and lifting
   the palms for a moment does not step down (`handstand: sticky: palms off the floor, held for the off grace`).
   The arms still have to be up.

6. **Wrist shooters (SHOOTERS: LOCKED TO ARM, Nov 20).** Tilt the hand toward the palm: the web line leaves the
   forearm toward the palm (up to 45 deg). Tilt it toward the back of the hand, or hold it straight: the line
   stays on the forearm. Both hands. Log: `forearm sign: L/R +N deg (positive toward palm: the line follows it;
   toward the back: on the forearm ...)`.
   **To settle the direction**, stand still, hold each arm straight out in front with the palm facing inward,
   and for about 3 s each: (a) keep the wrist straight, (b) bend the wrist toward the palm, (c) bend it toward
   the back of the hand. Do the left hand, then the right, and note in chat which tilts looked wrong. Each half second
   the log writes `shooter check: L palm-side +N, offset N, web vs arm N, vs controller N deg; arm palm on aim +X
   +0.NN (L expects +)`. On (b), palm-side should read about +30 to +60 and the web should follow (web vs arm ≈ the
   offset). On (c), palm-side should read negative with offset 0 and web vs arm about 0. If (b) reads negative,
   or the arm palm value has the wrong sign for that hand, the palm axis is flipped in the game's frame for that
   hand.
7. **Full-length swing lines (NA · WEB SHOOTERS, FULL-LENGTH SWING LINES, Nov 20).** Swing in toward the anchor:
   the web keeps its length and the extra hangs and swings as rope instead of the line shrinking. Reel in with the
   trigger: it shortens by what you reel. The swing itself should feel exactly as before. Log on each catch and
   (once a second) while reeling: `swing line: L caught drawn X m, line Y m (slack S m), swing rope R m, ...`.
   Check that the slack never pokes through the hand or flickers, and how a very long web looks (past 30 m of
   slack the rest is not drawn).

## What the beta2 sessions showed (game-vr-20261119-170012 and -171657, build 4b13b66)

- Point jump sounds: 151 of 155 point jumps played 4 of 4 of the game's jump sounds; 4 stood down for the game's
  own. Air dodge: never fired (every air B came with a chord, a held web or a spider-sense dodge). Ray fan: pulled
  the camera to 2.0 and 2.7 m at a wall; lag at most 3.6 m at 21 m/s. Handstands: no catch - every window failed
  "not inverted enough" (most touchdowns under 15 deg; catchDegrees 100). Finisher: one Y+B finisher held to the
  end. Forearm sign: as the owner said (fixed above). The second session ended in a game access violation with
  1.0 GB RAM and 2.4 GB page file free (free commit as low as 238 MB). Full notes: the playtest's ANALYSIS.md.

## New settings

| Setting | Tab | Default | ini |
|---|---|---|---|
| AIR DODGE | NA · KICKS & COMBAT | ON | `air_dodge=1`, `air_dodge_speed=9` (3-20 m/s), `air_dodge_chord_ms=50` (0-200), `air_dodge_cooldown_ms=500` (100-3000) |
| SHOWTIME RAY FAN | NA · SHOWTIME | ON | `showtime_ray_fan=1`, `showtime_speed_lead_pct=60` (0-100) |
| STICKY HANDSTANDS | NA · BODY & LEGS | ON | `handstand_sticky=1`, `handstand_sticky_grace_ms=400` (250-1000) |
| FULL-LENGTH SWING LINES | NA · WEB SHOOTERS | ON | `swing_line_full_length=1` |

## Known limits

- The ray fan uses the head-clearance ray slots while Showtime is out; on the way back the head's rays return
  about 100 ms late, under the fade. With both hands in contact there is room for 3 fan rays, otherwise 5.
- Air dodge holds B back for up to 50 ms in SpidyVR's air while it waits for A, X or Y.
- Whether the point jump's sounds are audible in the headset is not proven by the log; only that the game's
  sound call returned a sound.

## Zip checklist

- Built by Python (`shutil.make_archive`) from the CI artifact of the commit, never the zip CLI.
- Name `Spidy-0.8.7-na-beta2-win64-<sha7>.zip`; top folder `Spidy-0.8.7-player-torso-<sha7>/`.
- The launcher header reads 0.8.7-na-beta2; the diag log's build line names the commit.
- Defaults: every beta2 feature on (revisions 15 and 16; a saved off stays off); LOG LEVEL 1; X and Y instant;
  wall jump sound unchanged (SOUND A).
