# 0.8.7-na-beta2: headset checklist

A test build for the owner (branch `feat/087-beta2`, from `feat/087-beta1` 9e8d7cb). It is **not** the testers'
CURRENT: that stays [0.8.7-na-beta1](NA_BETA1.md) until the owner asks. What changed: [CHANGELOG.md](CHANGELOG.md).
Every feature stays on; settings files from before take migration revision 15 (AIR DODGE, SHOWTIME RAY FAN and
STICKY HANDSTANDS come on once; a 0 chosen after it stays). No existing numeric default changed; WALL JUMP SOUND
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

## New settings

| Setting | Tab | Default | ini |
|---|---|---|---|
| AIR DODGE | NA · KICKS & COMBAT | ON | `air_dodge=1`, `air_dodge_speed=9` (3-20 m/s), `air_dodge_chord_ms=50` (0-200), `air_dodge_cooldown_ms=500` (100-3000) |
| SHOWTIME RAY FAN | NA · SHOWTIME | ON | `showtime_ray_fan=1`, `showtime_speed_lead_pct=60` (0-100) |
| STICKY HANDSTANDS | NA · BODY & LEGS | ON | `handstand_sticky=1`, `handstand_sticky_grace_ms=400` (250-1000) |

## Known limits

- The ray fan uses the head-clearance ray slots while Showtime is out; on the way back the head's rays return
  about 100 ms late, under the fade. With both hands in contact there is room for 3 fan rays, otherwise 5.
- Air dodge holds B back for up to 50 ms in SpidyVR's air while it waits for A, X or Y.
- Whether the point jump's sounds are audible in the headset is not proven by the log; only that the game's
  sound call returned a sound.
