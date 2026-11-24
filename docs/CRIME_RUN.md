# Crime Run (prototype)

A roguelike random-crime mode for SpidyVR, on the Native Animation line (the owner, Nov 22, t103u):

> a roguelike random crime mode, where Spiderman has to stop crimes to continue to progress, earning points and
> ranks for crimes stopped. For now, it can simply spawn crimes in a perimeter at regular intervals, with a bright
> indicator and negative points anytime Spidey misses one. You start at 20,000, and lose or gain until you hit
> zero, or stop a certain number of crimes ... something you start anywhere in the open world.

Status: **prototype, not yet played in a headset.** Off by default. Source: `include/spidy/crime_run.hpp`,
`src/crime_run.cpp` (the rules and the drawing, engine independent), `tests/crime_run_tests.cpp`, the swing
module's census and ground rays (`game_punch::crimeCensus`, `crimeStep` / `SpidyCrimeAsk` / `SpidyCrimeLive` in
`src/game_swing.cpp`) and the glue in `src/game_xr.cpp` (search `CRIME RUN`).

## How to play

1. Launcher: **Kicks & Combat** page, card **Crime Run (prototype)**, turn **Crime Run** on (or `crime_run=1` in
   `%APPDATA%\Spidy\spidy_vr.ini`). Nothing else changes while it is off.
2. In the open world, anywhere: pull the phone from your back pocket (grip behind a hip, bring the hand up). The
   home screen now has a fifth app, **CRIME RUN** (a siren). Pick it with the stick and press **A**. The phone puts
   itself away and the run starts where you are.
3. A crime appears 5 s in, then one every **45 s** (if fewer than **2** are open). Each is a **pillar of light**
   150 m tall with a label (`CRIME 1:42 230 M`) and, close up, a ring on the ground:
   - **orange-red** pillar: a **beacon**. Reach it (within **15 m** across, and within 25 m up or down once its
     ground is known) and **stay in its ring 3 s** (a green arc fills). Leaving drains the hold.
   - **red/blue flashing** pillar: a **real crime** - the game's own enemies are standing there. Get within 30 m and
     **take every one of them down** (knocked out, webbed up or gone) and keep it clear 1.5 s.
   - The pillar flashes yellow (or faster) in the last quarter of its **120 s** timer.
4. The **HUD** under your gaze shows the score, stops, rank, the most urgent crime's distance and time with an arrow
   pointing to it, and the latest event (`CRIME STOPPED +1,450`, `CRIME MISSED -2000`, `RANK UP - SPECTACULAR`).
5. Score: you start at **20,000**. A stop scores **1,000** plus up to **100%** more for speed (all of it at once,
   none at the last second) plus **200 per enemy** a real crime had. A miss costs **2,000**. At **0** the run is
   **lost**; at **20 stops** it is **won**. The best (the highest final score of a won run, and the most stops in any
   run) is saved to `spidy_vr.ini` (`crime_best`, `crime_best_stops`) and shown in the launcher and on the app.
6. Quit: phone, CRIME RUN, **A twice** within 3 s. Turning the mode off in the launcher or tab also ends a run.
   A pause, a load or a menu does not run that 3 s window down: the second A still quits after you come back.

Timers run only while you play in the headset (gameplay camera, Spider-Man, the swing module's account fresh): a
pause, a menu, a cutscene or the flat screen freezes them.

### Ranks

By stops, as shares of the target (20 by default): **Rookie** 0, **Street-Level** 10% (2), **Friendly
Neighborhood** 25% (5), **Spectacular** 45% (9), **Sensational** 70% (14), **Amazing** 100% (20).

### Controls

| What | How |
| --- | --- |
| Start a run | Phone (back-pocket draw) > CRIME RUN > **A** |
| Quit a run | Phone > CRIME RUN > **A**, then **A** again within 3 s |
| Stop a beacon | Stand in its ring 3 s |
| Stop a real crime | Reach it and take every standing enemy there down |

No new button chord: every button combination in VR play already does something (A+B point jump, Y+B finisher,
X+B Showtime, Y+X legs, both sticks the pad layer). The phone's A is the phone's own while it is out.

## Settings (`spidy_vr.ini`, launcher card)

| Key | Default | Range | What |
| --- | --- | --- | --- |
| `crime_run` | 0 | 0/1 | the mode (the phone app) |
| `crime_real_crimes` | 1 | 0/1 | the game's enemies in play become crimes |
| `crime_hud` | 1 | 0/1 | the HUD below the gaze |
| `crime_start_score` | 20000 | 1000..100000 | the score a run starts at |
| `crime_win_stops` | 20 | 1..200 | stops that win |
| `crime_spawn_s` | 45 | 10..300 | seconds between crimes |
| `crime_timer_s` | 120 | 20..600 | each crime's timer |
| `crime_max_sites` | 2 | 1..4 | crimes open at once |
| `crime_radius_min_m` / `crime_radius_max_m` | 150 / 400 | 30..1000 / +10..1500 | the beacons' ring; the max also bounds real crimes |
| `crime_stop_points` | 1000 | 0..10000 | a stop's base points |
| `crime_speed_bonus_pct` | 100 | 0..300 | the bonus for a stop at once |
| `crime_thug_points` | 200 | 0..2000 | per enemy of a real crime |
| `crime_miss_penalty` | 2000 | 0..20000 | a miss |
| `crime_reach_m` | 15 | 3..60 | a beacon's zone across |
| `crime_hold_s` | 3 | 0..30 | the stay in a beacon's zone |
| `crime_best`, `crime_best_stops` | 0 | | written by the game |

Fixed for now (`crime_run::Config`): reach height 25 m, a real crime's radius 30 m, real crimes from 40 m away,
clear 1.5 s, a census older than 1 s counts as none and a real crime with no census for 6 s becomes a beacon.

## Research: what the mod can and cannot do here (Nov 22)

Checked against `docs/REFERENCE.md`, `docs/VALIDATION.md` and the `game_*` adapters. **No new address or offset
is used**: everything below is an existing, validated reader or call.

**Works with validated hooks (used):**

- **Enemies near the player, and whether each is still standing.** `game_targets` scans the game's component
  registry for bots (RTTI: `BotMoverManager`-derived movers, `ThugBot`, `CivilianBot`, `AllyBot` ...; REFERENCE.md
  "Thug, civilian, breakable components", registry scans "at the crime"). `game_targets::fighting` = an enemy (not
  a civilian, ally or neutral), **up** (in play, hit points left, not in `BotStateDeathAnim`/`Ragdoll`/`WallStuck`)
  and **not held** (no car seat, cinematic ...); `cocooned` = webbed up (`BotStateWebStruggle`). This is exactly the
  list the punches and the kicks' "N in the fight" use. The census (`game_punch::crimeCensus`) walks the punches'
  own list every 250 ms while a run asks, with no distance limit; up to 16 nearest go to XR.
- **Defeated enemies:** an enemy who stops being `fighting` or becomes `cocooned` is down; a real crime is stopped
  when none of its group is left standing (after the player reached it, so a crime the game cleans up on its own
  far away does not count: it becomes a beacon instead).
- **The game's own street crimes, approximately:** in free roam the game spawns thug groups with its street crimes
  (VALIDATION.md: "a fresh free-roam session has none until crimes spawn"; "a street crime of 7 ThugBots"). Standing
  enemies in play in the open world are that, so REAL CRIMES puts the site on the biggest group 40..400 m away. No
  readout of the game's crime *objects* (type, state, timer) is validated, so this is a heuristic: a fight you start
  yourself, or a mission's enemies, count too.
- **Ground height at a point:** the swing's Havok ray queries (`WorldQueries::raycast`, native_rays) - a ray straight
  down from 300 m above the higher of the player and the site, 700 m long, cast from the swing module's visit (after
  the aim previews, so a ray never faults the swing). A miss (the city's collision not streamed in that far) is cast
  again every second; until then a beacon's reach is judged across only.
- **World-space drawing:** the overlay's vertex lists that draw the aim markers, web lines and knockout lettering
  (`translucent`, world metres, swing world = overlay world as the kick ring uses it). Pillars far off are drawn at
  90 m at their own angular size so no far plane cuts them.
- **HUD text:** the overlay's stroke font (`appendText`), on a panel held below the gaze from the rendered head pose
  (as Spider-Sense's sightline marks use it).
- **Haptics:** `runtime.haptic` (OpenXR). **Sound:** the mod's own impact sounds (`play(ImpactSound::Finisher)` on a
  stop); the game's sound ids (FNV-1, `game_sound`) work, but no crime/siren sound id has been captured.
- **Start anywhere:** the phone (`phone.hpp`) - a back-pocket draw, A on an app - needs no button chord.

**Needs new hooks or captures (not done; listed for a later pass):**

1. **True spawning** of enemies or of the game's own crimes/activities. Nothing validated spawns a bot or starts a
   crime: REFERENCE.md's only spawn paths are the web shooter's shots (`SpawnShot 0x2150830`). Needed: the game's
   crime/activity director (what starts a street crime, its types and spawn points) found by RTTI and disassembly,
   then traced in the running game, and a reversible call into it.
2. **The game's crime objects**: a readout of active crimes (type, location, stage, the game's own "crime stopped"
   event) so sites are the game's crimes exactly and a stop is the game's own completion (and XP), not our census.
3. **Native map/HUD markers**: the game's world markers go through `AddPOI(id, category, subtype)` /
   `ClearPOI(id)` (REFERENCE.md row "The world markers", only *logged* through the UI's call wrapper). Calling them
   for our sites (POI_OBJECTIVE) would put real waypoints in the game's HUD and map; that needs the call validated
   (thread, lifetime, ids that cannot collide with the game's).
4. **Crime audio**: the game's siren/police-radio sound ids (a capture session; `game_sound` can already play an id).
5. **Ground everywhere**: rays only hit loaded collision; a nav-mesh or street-level query would put far beacons on
   the street instead of a roof (a beacon over a building is on its roof).

## What to test (headset)

1. Crime Run off: the phone has its four apps; nothing else differs from beta3g.
2. On: the phone's fifth app CRIME RUN (siren icon) reads `START A RUN HERE`; A starts a run, the phone goes away,
   the HUD shows `CRIME RUN 20,000 STOPS 0/20 ROOKIE` and `NEXT CRIME IN 0:05`.
3. After 5 s a pillar appears 150..400 m away, with a label; the HUD arrow points at it as you turn; controllers tick.
4. Swing there; the ring appears near it; stand in it 3 s: `CRIME STOPPED +N`, a sound, a strong buzz, STOPS 1/20.
5. Let one run out: `CRIME MISSED -2000`, the score drops.
6. Near a street crime (thugs fighting): a red/blue pillar on them; take them all down: stopped with thug points.
   Webbed-up thugs count as down.
7. Quit: A, A on the app. Win/lose: set `crime_win_stops` low (2) or the start score low (2000) to see both ends;
   the best shows on the app and in the launcher after the session.
8. The log (`game-vr-*-diag.log`): `crime run:` lines for start (with `enemy census from the swing module`), every
   spawn (beacon or real), reached, stopped, missed, rank and the summary line at the end.

Known limits: the beacons ignore walls (seen through buildings, as a waypoint); a beacon's ground may be a roof; real
crimes include any enemies in play (missions too); the score is the mod's, the game's own XP and crime count are
untouched.
