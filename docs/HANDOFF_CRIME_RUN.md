# Handoff: porting Crime Run into another Native Animation tree

Written November 23, 2026 (Pacific time) so another SpideyVR bot, working on its own (unknown, possibly
diverged) Native Animation tree, can bring Crime Run across accurately. Player-facing rules and the headset
checklist live in [CRIME_RUN.md](CRIME_RUN.md). This document is the integration map: what to copy as-is, what
to hand-port, the real APIs it hangs off, and how to hand the port back.

You do not need this tree to match beta3g. The core is engine-independent C++. The glue is a small set of
hooks into systems you almost certainly still have under some name (the punches' enemy list, the swing
module's world rays, the phone, the overlay, `spidy_vr.ini`). Find the equivalents; do not invent offsets.

## 0. Source of truth

- **Canonical repo:** [tyronmiller55-spec/SpidyVR-CrimeRun](https://github.com/tyronmiller55-spec/SpidyVR-CrimeRun)
  (`origin`). `main` is the Native Animation **beta3g** base, commit `1bb2aa6` ("Docs: handoff for the Native
  Animation beta3 line…"). The prototype is branch **`feat/crime-run`**.
- **The three Crime Run commits** (parent `1bb2aa6`; `git diff 1bb2aa6..343ad4c` is 19 files, +2130 / −9).
  This handoff is a later docs commit on the same branch.

  - `9cb99c3` (`9cb99c33e1719f5dff6b723fd67bcd3a744b8269`) — portable core, its tests, and CMake.
    Subject: *Crime Run rules: a run from 20,000 points with timed crimes on a ring or at real fights,
    stops, misses, six ranks, a best score, pillar-of-light beacons and a score HUD (core and tests)*.
  - `6a09505` (`6a09505aa5815a6e1ccfa2c9ce9c32db745d5954`) — all glue.
    Subject: *Crime Run mode (off by default): start a run from the phone anywhere in the open world,
    bright pillars over each crime, real street fights counted as crimes, a score HUD, haptics and a
    saved best, with a launcher card*.
  - `343ad4c` (`343ad4c3f1816dbc243f4c715cf8e0a283682456`) — `docs/CRIME_RUN.md`, CHANGELOG, README.
    Subject: *Docs: Crime Run - how to play, ranks, controls, settings, what the current hooks can and
    cannot do, and what to test*.
- A copy of the same three commits also landed on `tyronmiller55-spec/SpidyVR` (`feat/crime-run`). Prefer
  **SpidyVR-CrimeRun** as the source.
- Status: **prototype, not yet played in a headset.** Off by default. No new game address or offset is used.
- Search the tree for the marker `CRIME RUN` (and `crime_run`) to find every touch.

## 1. What Crime Run is

A roguelike random-crime mode for SpidyVR. With the mode on, the player starts a run from the phone anywhere
in the open world. Crimes appear on a timer as pillars of light: either a **beacon** to reach and hold, or a
**real crime** on the game's own standing enemies. Stops score points, misses cost points. The score starts at
20,000; the run is lost at 0 and won at 20 stops (both configurable). Ranks go by share of the win target.
The game itself is not changed: **no enemy is spawned and no crime of the game's is started** (nothing
validated does that; see §6).

### 1.1 How to play

1. Launcher: **Kicks & Combat** page, card **Crime Run (prototype)**, turn **Crime Run** on (or `crime_run=1`
   in `%APPDATA%\Spidy\spidy_vr.ini`). Nothing else changes while it is off: the phone still has its four
   apps, and there is no HUD and no pillar.
2. In the open world, anywhere: pull the phone from the back pocket (grip behind a hip, bring the hand up).
   The home screen has a fifth app, **CRIME RUN** (a siren). Pick it with the stick and press **A**. The phone
   puts itself away and the run starts where the player is.
3. A crime appears **5 s** in (`Config::firstSpawnSeconds`, not a setting), then one every **45 s** if fewer
   than **2** are open. Each is a **pillar of light 150 m tall** with a label, and close up a ring on the
   ground:
   - **orange-red** pillar: a **beacon**. Reach it (within **15 m** across, and within **25 m** up or down
     once its ground is known) and **stay in its ring 3 s** (a green arc fills). Leaving drains the hold at
     twice the fill rate.
   - **red/blue flashing** pillar: a **real crime** — the game's own enemies are standing there. Get within
     **30 m** (and 25 m up or down) and **take every one of them down** (knocked out, webbed up, or gone) and
     keep it clear **1.5 s**. Knocking them out from afar, without first reaching the site, does **not**
     count: the site becomes a beacon instead.
   - The pillar flashes yellow (beacon) or faster (real crime) in the last quarter of its **120 s** timer.
4. The **HUD** under the gaze shows the score, stops, rank, the most urgent crime's distance and time with an
   arrow pointing to it, and the latest event. An ended run's HUD stays **12 s**, then the run is cleared.
5. Quit: phone, CRIME RUN, **A twice** within **3 s** (`crime_run::Toggle::confirmSeconds`). Turning the mode
   off in the launcher or tab also abandons a run under way.
6. Timers run only while the player is in the headset in gameplay (gameplay camera, Spider-Man, the swing
   module's account less than 250 ms old). A pause, a menu, a cutscene or the flat screen freezes them
   (`update` is not called; `dt <= 0` also no-ops). `update` itself caps `dt` at 0.25 s.

No new button chord. Every combination in VR play already does something (A+B point jump, Y+B finisher, X+B
Showtime, Y+X legs, both sticks the pad layer). The phone's A is the phone's own while it is out.

### 1.2 Scoring

- Start score: **20,000**.
- A stop scores `pointsFor`: **1,000** (`stopPoints`) plus up to **100%** more for the share of the timer
  left (all of the bonus at once, none at the last second), plus **200** (`thugPoints`) for each enemy the
  site ever had (`thugsMost`), rounded to the nearest 10.

  ```
  points = stopPoints * (1 + speedBonusPct/100 * left/total) + thugPoints * thugsMost
  return lround(points / 10) * 10
  ```

  Examples at the defaults: a beacon stopped at once = **2,000**; at the last moment = **1,000**; half way =
  **1,500**; half way with three enemies = **2,100**.
- A miss costs **2,000**, but never more than the score left (`min(missPenalty, score)`).
- Lost when the score reaches **0**; won at **20** stops. Win is checked after a stop, before the next spawn.
- Best, written to `spidy_vr.ini` when a run ends: `crime_run::better` is true when `(won && score > bestScore)
  || stops > bestStops`. A won run may raise `crime_best` (the final score) and `crime_best_stops`. An
  abandoned or lost run may raise `crime_best_stops` only.

### 1.3 Ranks

By stops, as shares of `winStops` (default 20). Each rank is at least one stop above the one before;
**Amazing** is always the target. Names are uppercase, as the HUD prints them:

| Rank | Share | Stops at a target of 20 | `rankName` |
| --- | --- | --- | --- |
| Rookie | 0 | 0 | `ROOKIE` |
| Street-Level | 10% | 2 | `STREET-LEVEL` |
| Friendly Neighborhood | 25% | 5 | `FRIENDLY NEIGHBORHOOD` |
| Spectacular | 45% | 9 | `SPECTACULAR` |
| Sensational | 70% | 14 | `SENSATIONAL` |
| Amazing | 100% | 20 | `AMAZING` |

Implemented by `stopsFor` / `rankFor` in `src/crime_run.cpp` (ceil of the share, then at least previous + 1).

### 1.4 Settings (`spidy_vr.ini`, launcher card)

18 keys, all under the `crime_` prefix, listed in `vr_settings.cpp`'s `tabOnly[]` and clamped in
`vr_settings::sanitized` (and again in `crime_run::sanitized` for the run's own `Config`). Defaults match
`crime_run::Config` and `Values`.

| Key | `Values` field | Default | Range (sanitized) | What |
| --- | --- | --- | --- | --- |
| `crime_run` | `crimeRun` | 0 | 0/1 | the mode (the phone app). **Off by default.** |
| `crime_real_crimes` | `crimeRealCrimes` | 1 | 0/1 | the game's enemies in play become crimes |
| `crime_hud` | `crimeHud` | 1 | 0/1 | the HUD below the gaze (beacons always show) |
| `crime_start_score` | `crimeStartScore` | 20000 | 1000..100000 | the score a run starts at |
| `crime_win_stops` | `crimeWinStops` | 20 | 1..200 | stops that win |
| `crime_spawn_s` | `crimeSpawnS` | 45 | 10..300 | seconds between crimes after the first |
| `crime_timer_s` | `crimeTimerS` | 120 | 20..600 | each crime's timer |
| `crime_max_sites` | `crimeMaxSites` | 2 | 1..4 | crimes open at once (`Run::mostSites` is 4) |
| `crime_radius_min_m` | `crimeRadiusMinM` | 150 | 30..1000 | inner radius of the beacon ring, metres |
| `crime_radius_max_m` | `crimeRadiusMaxM` | 400 | min+10..1500 | outer radius; also the farthest real crime |
| `crime_stop_points` | `crimeStopPoints` | 1000 | 0..10000 | a stop's base points |
| `crime_speed_bonus_pct` | `crimeSpeedBonusPct` | 100 | 0..300 | bonus for a stop at once, percent |
| `crime_thug_points` | `crimeThugPoints` | 200 | 0..2000 | per enemy a real crime had |
| `crime_miss_penalty` | `crimeMissPenalty` | 2000 | 0..20000 | a miss |
| `crime_reach_m` | `crimeReachM` | 15 | 3..60 | a beacon's zone across, metres |
| `crime_hold_s` | `crimeHoldS` | 3 | 0..30 | stay in a beacon's zone, seconds |
| `crime_best` | `crimeBest` | 0 | 0..10000000 | written by the game: highest final score of a won run |
| `crime_best_stops` | `crimeBestStops` | 0 | 0..1000 | written by the game: most stops in any run |

**Not settings** (fixed `crime_run::Config` defaults; `crimeConfig()` in `game_xr.cpp` does not copy them, so
they stay at these values unless you add keys):

| `Config` field | Default | Meaning |
| --- | --- | --- |
| `firstSpawnSeconds` | 5 | first crime after start |
| `reachHeight` | 25 m | up or down once the site's ground is known |
| `thugRadius` | 30 m | enemies within this of a site are its |
| `realMin` | 40 m | a real crime this near (or nearer) is ignored at spawn |
| `clearSeconds` | 1.5 | a reached real crime must stay empty this long |
| `censusSeconds` | 1 | a census older than this is not `fresh` |
| `staleSeconds` | 6 | a real crime with no fresh census this long becomes a beacon |

**Settings migration:** Crime Run did **not** bump `movementDefaultsRevision`. In this tree that constant is
still **23** (beta3g's rope-weight rescale). The new keys take the struct defaults; `crime_run` is off, so an
older `spidy_vr.ini` without the keys behaves like the previous build. There is no `if (seen < N)` block for
Crime Run, and none is required for Crime Run itself. See §4.3 for how to treat *your* revision number.

XR builds a `crime_run::Config` from the values (`crimeConfig` in `src/game_xr.cpp`) and passes it through
`crime_run::sanitized` at `Run::start`.

## 2. File inventory (`git diff 1bb2aa6..343ad4c`)

19 files: 5 added, 14 modified.

### 2.1 Portable core — copy / cherry-pick as-is

These have no Windows, no game addresses, and no headset. They belong in `spidy_core`. Cherry-pick
`9cb99c3` first; it should apply on a tree that still has the same CMake `spidy_core` / `air_kick` test
block, and is otherwise self-contained.

- **`include/spidy/crime_run.hpp`** — added, 277 lines. `Config`, `Run`, `Toggle`, `Event`, ranks,
  `pointsFor`, `Rng`, `Ask` / `Live` (the swing-module bridge: `sizeof(Ask) == 88`, `sizeof(Live) == 360`,
  magics `0x53435241` / `0x5343524c`, version 1), `appendBeacon`, `appendHud`, `hudOf`. Depends only on
  `math.hpp` and `vertex.hpp`.
- **`src/crime_run.cpp`** — added, 623 lines. The rules, scoring, ranks, beacons, HUD. Drawing uses
  `overlay_text.hpp` (`appendText`, `textWidth`, `fittedHeight`, `TextStyle`) and `soft_shapes.hpp`
  (`soft::quad`, `soft::softArc`).
- **`tests/crime_run_tests.cpp`** — added in `9cb99c3` (413 lines of portable rule tests); extended in
  `6a09505` with settings-round-trip and phone-app tests (468 lines total, 99 checks, prints
  `N/N crime run checks passed`). After the glue commit it also includes `phone.hpp` and
  `vr_settings.hpp`. If you cherry-pick only the core first, the 413-line file compiles against
  `spidy_core` alone. Bring the two extra test blocks over with the glue.
- **`CMakeLists.txt`** — modified. (1) Add `src/crime_run.cpp` to the `spidy_core` source list (after
  `src/tech_marks.cpp`). (2) Add the test target next to the air-kick tests:
  `add_executable(spidy_crime_run_tests tests/crime_run_tests.cpp)`, link `spidy_core`,
  `add_test(NAME crime_run COMMAND spidy_crime_run_tests)`.

`Run` is the whole mode: `start(config, player, seed, events)`, `update(dt, player, census, events)`,
`ground(id, y)`, `abandon`, `clear`. The glue only feeds it, draws what it returns, and writes the best.

### 2.2 Glue — hand-port; do not assume a clean cherry-pick of `6a09505`

Every touch is marked `CRIME RUN`. Off must leave the previous build's behaviour.

- **`include/spidy/vr_settings.hpp`** — new fields on `vr_settings::Values`, placed after
  `// BETA3 SETTINGS END` and before the HERO ATTACK SOUND comment: `crimeRun = false` and the 17
  companion ints/bools listed in §1.4. Default `movementDefaults` is unchanged (23 in this tree).
- **`src/vr_settings.cpp`** — clamps in `sanitized(Values)` (max radius is `crimeRadiusMinM + 10` ..
  1500). 18 entries in `tabOnly[]` immediately after `kick_chamber` and before `log_level` /
  `movement_defaults`. No `if (seen < N)` migration block.
- **`apps/launcher/ui.cpp`** — a `group(kicksPage, 1, "Crime Run (prototype)", …)` card on the Kicks
  & Combat page (`Category::kicks`), immediately after `// BETA3G LAUNCHER END` and before the NA HELP
  topics. Toggles, sliders matching the ranges above, and a read-only **Best** line
  (`ImGui::Text("%d  (%d stops)", …)`). The best is written by the game, not the launcher.
- **`include/spidy/phone.hpp`** — `phoneCrimeApp = 4` (fifth app after CAMERA). `PhoneAction::crimeRun`
  added at the end of the enum. `PhoneUi::setCrimeApp(bool)` / `apps()`; the wrap uses `apps_` instead
  of the old hardcoded `phoneApps`. `Phone::setCrimeApp` / `putAway()`. `PhoneView::crimeApp` and
  `crimeNote[32]`. `phoneApps` itself stays **4** (the four original apps).
- **`src/phone.cpp`** — home screen: names/notes arrays sized `phoneApps + 1`; when `view.crimeApp`,
  five cards sit a little closer (`first` 0.047, `pitch` 0.0205, `card` 0.0175, else the old 0.045 /
  0.0235 / 0.0195). `sirenIcon` for `i == phoneCrimeApp`. `PhoneUi::update`: wrap with `apps_`; A on
  `phoneCrimeApp` while the fifth app is present returns `PhoneAction::crimeRun`. MUSIC / MENU / GEAR /
  CAMERA are unchanged.
- **`include/spidy/game_punch.hpp`** — `constexpr uint32_t crimeCensusMs = 250;`
  `void wantCrimeCensus();` `size_t crimeCensus(Vec3 from, Vec3* out, size_t max, uint64_t& serial);`
  — standing enemies anywhere the punches' list sees them, nearest `from` first, no new address.
- **`src/game_punch.cpp`** — in `game_punch::update`, every 250 ms while `wantCrimeCensus` was called
  within the last 2 s: walk `watch.current(bots)` and keep the feet of each bot for which
  `game_targets::fighting(base, b) && !game_targets::cocooned(base, b) && game_targets::position(b, feet)
  && finite(feet)`. Stored under `crimeLock`. `wantCrimeCensus` stamps `GetTickCount64()`.
  `crimeCensus` copies nearest-first under a shared lock and returns the serial.
- **`src/game_swing.cpp`** — `#include "spidy/crime_run.hpp"`. Exported `crime_run::Live SpidyCrimeLive`
  (same pattern as `SpidyAirKickLive`). `crimeStep(const WorldQueries& world, Vec3 feet)`: if an ask
  arrived within 2 s, call `game_punch::wantCrimeCensus()`; every 250 ms write `SpidyCrimeLive` (census
  via `crimeCensus(feet, found, 16, serial)`, plus grounds). Grounds: when `ask.serial` is new, for each
  asked site `world.raycast({x, top, z}, {0, -1, 0}, crime_run::groundRay)` with `groundRay = 700`.
  Sequence is odd while written (`InterlockedIncrement64` before and after `memcpy`, under the existing
  `output` lock). Called from `visit` **after** `previewAims`, and only when `enabled && !world.error()`,
  so a ray never faults the swing. Export `SpidyCrimeAsk(void* input)`: 2001 if not a valid `Ask`, 1004
  if the swing has not started, 0 if taken.
- **`include/spidy/native_eye_history.hpp`** — on `NativeEyeFrame`: `#include "crime_run.hpp"`,
  `std::array<crime_run::BeaconView, 4> crimeBeacons{}`, `uint32_t crimeBeaconCount{}`,
  `crime_run::HudView crimeHud{}`.
- **`src/game_xr.cpp`** — the mode tick. See §3.9 for the call sites and signatures it uses. Blocks:
  include; `crimeAskCall` / `crimeLive` at module scope; `crimeConfig()`; locals in `run()`;
  `phone.setCrimeApp(values.crimeRun)` and `case PhoneAction::crimeRun`; the large `CRIME RUN` tick
  after `native_hud::follow` (start/quit, census, ask, events, best, 12 s end); fill
  `saved.crimeBeacons` / `saved.crimeHud` and the phone's `crimeNote`; draw `appendBeacon` / `appendHud`
  on the overlay's `translucent` list; `GetProcAddress` for `SpidyCrimeAsk` / `SpidyCrimeLive` in
  `SpidyXrStart` (an older swing module leaves them null: beacons still spawn, their ground stays
  unknown, no census).
- **`tests/core_tests.cpp`** — the defaults-text expectation gains the 18 `crime_*=` lines (including
  `crime_run=0`) immediately before `log_level=1` / `movement_defaults=23`. If your tree's revision is
  not 23, keep *your* revision in that string and still insert the `crime_*` lines.
- **`tests/launcher_tests.cpp`** — in the loop that flags tab-only keys without a launcher row (or the
  equivalent exemption list), skip Crime Run with `if (key.rfind("crime_", 0) == 0) continue;`.
- **`tests/crime_run_tests.cpp`** — the two extra blocks described above (settings survive
  `tabOnlyText` / `withTabOnly`; fifth app only while the mode is on; `appendPhoneScreen` draws more
  vertices with the app on).

### 2.3 Docs (commit `343ad4c`, plus this file)

- **`docs/CRIME_RUN.md`** — added. How to play, ranks, settings, what the hooks can and cannot do,
  headset checklist. Bring this file over with the port.
- **`docs/CHANGELOG.md`** — a **Crime Run prototype** entry at the top (November 22), still on beta3g,
  pointing at `CRIME_RUN.md`.
- **`README.md`** — a short **Crime Run (prototype, branch `feat/crime-run`)** paragraph after the fork
  intro.
- **`docs/HANDOFF_CRIME_RUN.md`** — this file.

## 3. Existing systems and APIs it depends on

If a name has moved in your tree, find the equivalent. Do not invent a new offset or a spawn path to "make
it work." Every hook below already existed and is validated; Crime Run only reads them.

### 3.1 Enemy list (real crimes)

From `include/spidy/game_targets.hpp` / the punches' watch, the same list punches and kicks use (`REFERENCE.md`
"Thug, civilian, breakable components"; registry scans "at the crime"):

```cpp
struct Candidate { uint64_t component{}, record{}; uint32_t handle{}; /* … */ };
inline bool enemy(const Candidate& c);          // bot, not civilian / ally / neutral
bool inPlay(const Candidate&);
bool up(uintptr_t base, const Candidate&);      // in play, hit points left, not DeathAnim / Ragdoll / WallStuck
bool held(uintptr_t base, const Candidate&);    // car seat, cinematic, …
inline bool fighting(uintptr_t base, const Candidate& c); // enemy(c) && up(base, c) && !held(base, c)
bool cocooned(uintptr_t base, const Candidate&); // BotStateWebStruggle
bool position(const Candidate&, Vec3& out);     // actor transform translation

class Watch {
    void start(uintptr_t base, uint32_t kinds);
    uint64_t current(std::vector<Candidate>& out) const;
};
```

Crime Run's definition of "standing" is **`fighting && !cocooned`**. A webbed-up thug is down. There is no
readout of the game's crime *objects* (type, state, timer); a group of standing enemies in the open world is
the heuristic, so a fight the player starts, or a mission's enemies, count too.

The census API added on the punches (so the walk stays on the thread that already samples the list):

```cpp
// include/spidy/game_punch.hpp
constexpr uint32_t crimeCensusMs = 250;
void wantCrimeCensus();
size_t crimeCensus(Vec3 from, Vec3* out, size_t max, uint64_t& serial);
// game_punch::update already does: watch.current(bots);
Opened update(float seconds, const Input&, const Player&, const WorldQueries&);
```

If your punches live elsewhere, keep the rule: stamp "wanted" from the swing side, walk the existing list on
the punches' update, return nearest-first feet and a serial. No second registry scan, no new address.

### 3.2 Ground rays (beacon height)

From `include/spidy/swing.hpp` (the swing module's visit already has a `WorldQueries`):

```cpp
struct RayHit { Vec3 point{}, normal{}; std::uint64_t surface{}; bool fixed{true}; };
class WorldQueries {
    virtual std::optional<RayHit> raycast(Vec3 origin, Vec3 direction, float distance) const = 0;
};
constexpr float maxRayDistance = 1000; // the game's cap; Crime Run asks 700, under it
```

Crime Run casts straight down from `top = max(playerFeet.y, site.y) + 300`, distance `crime_run::groundRay`
(700). A miss is asked again every 1 s (the ask's `serial` increments; `crimeStep` recasts when `serial`
changes). Until a hit, a beacon is judged across only (`reachHeight` is applied only once `grounded`).

### 3.3 Swing-module bridge (`Ask` / `Live`)

Same export pattern as air kicks (`SpidyAirKick` / `SpidyAirKickLive`):

```cpp
// game_swing.cpp
__declspec(dllexport) crime_run::Live SpidyCrimeLive;
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCrimeAsk(void* input);
// 2001: not a valid Ask; 1004: swing has not started; 0: taken.

// game_xr.cpp, SpidyXrStart (GetProcAddress on the rays / swing module):
using BridgeCall = DWORD(WINAPI*)(void*);
crimeAskCall = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyCrimeAsk"));
crimeLive    = reinterpret_cast<const crime_run::Live*>(GetProcAddress(rays, "SpidyCrimeLive"));
if (crimeLive && !crime_run::valid(*crimeLive)) crimeLive = nullptr;
```

XR reads `Live` only when `sequence` is even and unchanged across a `memcpy` (odd = mid-write). The swing
module's existing `copy(void* destination, const void* source, size_t bytes)` (SEH-safe `memcpy`, used by
`SpidyAirKick` and friends) is what `SpidyCrimeAsk` uses to take the `Ask`.

Ask payload: up to 4 ungrounded sites (`ids`, `x`, `z`, `top`). Live payload: up to 16 thug feet (nearest
first), a census serial, and up to 4 grounds (`id`, `hit`, `y`).

### 3.4 Overlay (pillars and HUD)

World-space vertices, same list as the aim markers, kick ring, and knockout lettering (`translucent`, world
metres). Drawn only when `feedback && !saved->flatScreen`.

```cpp
// overlay_text.hpp
void appendText(std::vector<Vertex>& out, std::string_view text, Vec3 origin, Vec3 right, Vec3 up,
                const TextStyle& style = {});
float textWidth(std::string_view text, float height);
float fittedHeight(std::string_view text, float height, float width);

// soft_shapes.hpp
namespace soft {
inline void quad(std::vector<Vertex>& out, const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d);
inline void softArc(std::vector<Vertex>& out, Vec3 c, Vec3 a, Vec3 b, float inner, float outer, float t0,
                    float t1, Vec3 color, float alpha, float soft, int segments, float innerSoft = -1);
}

// crime_run.hpp — what XR calls
constexpr float drawReach = 90; // farther pillars are pulled in, at their own angular size
void appendBeacon(std::vector<Vertex>& out, const BeaconView& view, Vec3 viewer, float pixelAngle);
void appendHud(std::vector<Vertex>& out, const HudView& view, Pose head);
// head: the eyes in the world (x right, y up, -z ahead).
HudView hudOf(const Run& run, Vec3 player);
```

`NativeEyeFrame` (in this tree, `include/spidy/native_eye_history.hpp`) carries the four `BeaconView`s and
the `HudView` from the tick to the eye path, the same way the kick target ring does. If your frame struct
lives elsewhere, add the same three fields.

HUD placement: a 0.8 × 0.2 m dark-glass panel 1.4 m ahead of the head and 0.45 m down. Top line:
`CRIME RUN  <grouped score>  STOPS <n>/<win>  <RANK>`. Flash lines are written by XR (4 s,
`flashSeconds`), not by `hudOf`. Runtime flash formats (no thousands separators):

- start: `CRIME RUN - STOP %d CRIMES`
- spawn: `NEW CRIME %d M`
- stop: `CRIME STOPPED +%d`
- miss: `CRIME MISSED %d` (`points` is already negative, so `CRIME MISSED -2000`)
- rank: `RANK UP - %s`
- quit: `RUN QUIT - %d STOPS`
- won / lost: `AMAZING! FINAL %d` / `THE CITY FELL - %d STOPS`

Beacon labels: `CRIME  <clock>  <m> M`, or `CRIME <thugs>  <clock>  <m> M` for a real crime.

### 3.5 Phone

```cpp
constexpr int phoneApps = 4, phoneMenuApp = 1, phoneCameraApp = 3;
constexpr int phoneCrimeApp = 4; // only while setCrimeApp(true)
enum class PhoneAction : int { none, close, previous, play, next, autoplay, camera, menu, crimeRun };

void PhoneUi::setCrimeApp(bool on); // apps_ = on ? phoneApps + 1 : phoneApps; clamps app_
int  PhoneUi::apps() const;
PhoneAction PhoneUi::update(const PhoneControls::Event& e, int gearRows);

void Phone::setCrimeApp(bool on);
void Phone::putAway();              // open_ = live_ = false
Use  Phone::take(XrFrame& frame, int gearRows); // Use { Event controls; PhoneAction action; unsigned stick; }
void appendPhoneScreen(std::vector<Vertex>& out, const PhoneView& view);
```

The note under the app (`PhoneView::crimeNote`): empty → the screen prints `START A RUN HERE`; while
confirming, XR sets `A AGAIN TO QUIT`; in a run, `%d - %d/%d - A TO QUIT`; otherwise `BEST %d - %d STOPS`.

### 3.6 Settings I/O

```cpp
std::string vr_settings::tabOnlyText(const Values&);
// game_xr.cpp, already used for other live-written keys:
bool writeTabOnly(const std::string& text);
```

A new best is saved only when `tabOnlyText(values)` differs from the last written text. Failure logs
`crime run: the best could not be saved to spidy_vr.ini`.

### 3.7 Haptics and sound

```cpp
// xr_session.hpp — OpenXR controller pulse; seconds default 0.025, up to 0.5
void haptic(int hand, float strength, float seconds = .025f); // hand 0 left, 1 right

// game_xr.cpp, local to run() (already used by kicks / punches), around line 2302 in this tree:
auto play = [&](ImpactSound sound) { /* voice_pool for blocks, else PlaySoundW of impactWave */ };
// A stop plays ImpactSound::Finisher.
enum class ImpactSound : std::uint8_t { Light, Firm, Heavy, Finisher, Rush, /* blocks… */ };
```

If your `play` lambda was renamed or moved out of `run()`, call the equivalent ImpactSound helper. No game
siren id is used (none has been captured).

Pulses (strength, seconds): start 0.5 / 0.1 both hands; spawn 0.4 / 0.08 both; reached 0.3 / 0.04 right;
stop 0.8 / 0.12 both; miss 0.25 / 0.3 both; quit-confirm 0.3 / 0.05 right; won 0.9 / 0.25 both; lost or
quit 0.4 / 0.25 both.

### 3.8 Logging

`diag_log::writef(const char* format, ...)` (`include/spidy/diag_log.hpp`). Every line starts with
`crime run:`. Start includes `enemy census from the swing module` or `none (an older swing module)`. The
end-of-run line is `crime run: %s (asks refused %u)` with `Run::summary()`.

### 3.9 XR tick — when to call what

Place this in the same per-frame path that already has `values`, `phone`, `swingState`, `runtime`, and the
gameplay / viewing / flat-screen / Spider-Man flags (in this tree: after `native_hud::follow` inside
`run()`). Logic, not a paste you should force into a moved file:

- `phone.setCrimeApp(values.crimeRun)` just before `phone.take(...)`.
- On `PhoneAction::crimeRun`, set a one-frame `crimePressed` flag; the tick consumes it.
- Player feet: `swingState.position - Vec3{0, 1, 0}`.
- Swing fresh: `swingState.status && swingState.qpc` and the qpc is within a quarter second.
- Playable: `gameplay && viewing && !flatScreen && spiderMan && swingFresh`.
- If the mode is turned off while a run is under way, `abandon`.
- `Toggle::update(dt, crimePressed && values.crimeRun, crimeRun.running())` → `start` / `confirm` / `quit` /
  `none`. Start only when the swing is fresh and the feet are finite; otherwise log
  `crime run: not started - no position from the swing module yet`. Start and quit both `phone.putAway()`.
  Seed: `uint64_t(now.QuadPart) ^ (GetTickCount64() << 20)`.
- While running, snapshot a valid `Live`, `ground()` each hit, build a `Census` (`fresh` if the serial was
  seen within `censusSeconds`), then `if (crimePlay) crimeRun.update(dt, feet, census, events)`.
- Every 250 ms while running, send an `Ask` of ungrounded sites; bump `serial` at most once a second when
  any site is still ungrounded (so a missed ray is retried as the city streams in).
- Walk `events` for log / haptic / `play` / flash / best. Then, after 12 s in a non-idle, non-running
  phase, `clear()`.
- Fill `NativeEyeFrame` beacons and HUD (`hudOf`, then override `shown` with `values.crimeRun && values.crimeHud`,
  and copy the flash / confirming flag).

## 4. Recommended integration procedure

Work on a branch of *your* tip. Do not reset your tree onto this repo's `main`.

### 4.1 Add the repo and take the core

```sh
git remote add crimerun https://github.com/tyronmiller55-spec/SpidyVR-CrimeRun.git
git fetch crimerun
git checkout -b feat/crime-run   # or feat/crime-run-port, if you already have work on another name
git cherry-pick 9cb99c3
```

If `CMakeLists.txt` conflicts, keep your source list and test layout and apply the two edits in §2.1 by
hand. `crime_run.hpp` / `crime_run.cpp` / the 413-line tests should not conflict. Build `spidy_core` and run
the crime_run tests (see §5). Fix compile breaks in the core only if your `math.hpp` / `vertex.hpp` /
`overlay_text.hpp` / `soft_shapes.hpp` moved; do not change the rules to match a renamed type if an alias
will do.

### 4.2 Hand-port the glue (`6a09505`), then the docs

Do not expect `git cherry-pick 6a09505` to apply cleanly if your `game_xr.cpp`, `game_swing.cpp`,
`vr_settings.*`, phone, or launcher have diverged (they likely have). Use the commit as a map:

1. Settings fields + `tabOnly[]` + `sanitized` clamps (§1.4, §2.2, §4.3).
2. Phone enum / fifth app / siren / `setCrimeApp` / `putAway` / `crimeNote`. If you **already have a fifth
   home-screen app**, do not reuse index 4. Give Crime Run the next free slot, set `phoneCrimeApp` to that
   index, and keep wrap math on `apps_` (or the total you pass in). The four original apps must stay
   `phoneApps == 4` while the mode is off.
3. `game_punch::wantCrimeCensus` / `crimeCensus` on the punches' existing update / watch.
4. `SpidyCrimeAsk` / `SpidyCrimeLive` / `crimeStep` in the swing module, after the aim-preview rays.
5. `NativeEyeFrame` fields.
6. XR: `crimeConfig`, the tick, overlay draw, `GetProcAddress` in start, phone action, best save.
7. Launcher card: on your Kicks & Combat page if you still have one; otherwise the combat / modes page that
   is closest. Keep the group title **Crime Run (prototype)** and the same ini keys.
8. Tests: core_tests defaults text, launcher_tests `crime_` exemption, the two extra crime_run_tests blocks.
9. Docs: cherry-pick or copy `343ad4c` (`docs/CRIME_RUN.md`, CHANGELOG, README). Add a short port note (see
   §7).

Buttons: do **not** invent a new chord if A on the phone is still the phone's own. If you have already bound
A on a new phone app, use that app's confirm pattern, but keep start = one press and quit = two presses
within 3 s.

### 4.3 Settings revision — likely conflict, and how to resolve it

- In *this* tree, `inline constexpr int movementDefaultsRevision = 23;` and `Values::movementDefaults = 23`.
  Crime Run did not increment it.
- **Do not write 23 over a newer revision on your tip.** Keep your current `movementDefaultsRevision`.
- Crime Run needs no `if (seen < N)` block: it is off by default, and missing keys get the struct defaults,
  which is "the previous build" for this mode.
- If you are *also* landing other features that are on by default, those need their own increment, per the
  project rule (a switch comes on once; a value chosen after the revision stays). Do that in a separate
  commit from Crime Run if you can.
- Update whatever defaults-text test you have so it lists the 18 `crime_*` lines and **your**
  `movement_defaults=<your revision>`.

### 4.4 Project rules (unchanged; they apply to the port too)

- **Off by default**, and **off matches the previous build.** (Crime Run is a mode, so it is an intentional
  exception to the usual "new features on by default" rule.)
- One feature per commit, with a player-facing subject. Suggested split if you hand-port instead of
  cherry-picking: (1) core + tests, (2) mode glue (phone, census, rays, HUD, launcher, settings), (3) docs.
- Never modify the game folder. Reversible hooks only.
- Never invent offsets or addresses. Cite `docs/REFERENCE.md` and `docs/VALIDATION.md`. If you do not have a
  validated spawn or `AddPOI` call, leave those for later (they are listed in §6).
- The headset heading is the only direction authority; never write view yaw. The air belongs to SpidyVR.
  Leave the wall jump alone. Ask before play-feel changes.
- Keep the Spidy launcher identity and the credits to Ilya Mezerowsky and the upstream contributors.
- Push only to **your / the owner's `origin`**. **Never push to a `builds` remote.**
- Run CI with **`publish=false`**. Do not publish a release, tag, or Discord announcement as part of this
  port.
- **No PR to `IlyaMez/SpidyVR`** unless the owner asks. Leave any existing fork PRs (#1 and #2 on
  `tyronmiller55-spec/SpidyVR`) alone.
- Draft Discord posts for the owner if asked; never post them.

### 4.5 Build and CI

On the box this was developed on (adapt paths to yours):

```sh
cmake --build /workspace/build-linux --target spidy_core
# crime_run tests (use -O0; always relink against a freshly built libspidy_core)
# then the rest of your usual core / air-kick / beta-polish suite
```

CTest name: `crime_run` (`spidy_crime_run_tests`). After a successful local run:

```sh
git push -q origin <your-branch>
gh workflow run Release --ref <your-branch> -f publish=false
gh run watch <id> --exit-status --interval 30
```

The workflow must be started as the repository owner. Package the player zip the way your tree already does
(on the beta3 line: download the run artifact and re-zip the player folder). Do not use `publish=true`.

## 5. Verification checklist

### 5.1 Headless

- [ ] `spidy_crime_run_tests` prints `99/99 crime run checks passed` once the glue tests are included
      (fewer, but all passing, after the core-only cherry-pick).
- [ ] Your core test binary still passes, including the defaults-text check with the new `crime_*` keys and
      your `movement_defaults` revision.
- [ ] Launcher tests still pass (the `crime_` exemption, or every `crime_*` key has a card row).
- [ ] Windows-only sources that you touched (`game_xr.cpp`, `game_swing.cpp`, `game_punch.cpp`, launcher)
      still syntax-check / compile in your usual way.
- [ ] CI on your branch, `publish=false`, green. Keep the run URL for the hand-back.

The portable tests already cover: defaults and sanitizing; ranks at a target of 20 and at 1; `pointsFor`
(at once / last moment / half way / with thugs); the ring and the RNG; a beacon spawn, ground, miss (−2000);
reach + hold + drain; a real crime at a group of three, not counting a wipe from afar, then a reached clear
(+ thug points); a stale census turning a real crime into a beacon; enemies arriving at a beacon; REAL
CRIMES off; spawn interval and `maxSites`; lose at 0; win and rank-up; abandon and paused (`dt <= 0`); the
phone toggle (A starts, A twice quits); `Ask` / `Live` `valid`; drawing; settings round-trip; fifth app only
while on.

### 5.2 In the headset (from CRIME_RUN.md)

1. Crime Run off: the phone has its four apps; nothing else differs from the build you started from.
2. On: the phone's fifth app CRIME RUN (siren) reads `START A RUN HERE`; A starts a run, the phone goes
   away, the HUD shows `CRIME RUN  20,000  STOPS 0/20  ROOKIE` and `NEXT CRIME IN 0:05`.
3. After 5 s a pillar appears 150..400 m away, with a label; the HUD arrow points at it as you turn;
   controllers tick.
4. Swing there; the ring appears near it; stand in it 3 s: `CRIME STOPPED +N`, a sound, a strong buzz,
   `STOPS 1/20`.
5. Let one run out: `CRIME MISSED -2000`, the score drops.
6. Near a street crime (thugs fighting): a red/blue pillar on them; take them all down: stopped with thug
   points. Webbed-up thugs count as down.
7. Quit: A, A on the app. Win/lose: set `crime_win_stops` low (2) or the start score low (2000) to see both
   ends; the best shows on the app and in the launcher after the session.
8. The log (`game-vr-*-diag.log`): `crime run:` lines for start (with the census source), every spawn
   (beacon or real), reached, stopped, missed, rank, and the summary line at the end.

## 6. Known limits and future hooks

These are still true after a faithful port. Do not paper over them with an unverified call.

**Works with validated hooks (already used):** enemy census (`fighting`, not `cocooned`); defeated = no
longer fighting, or cocooned; street crimes approximated as standing enemy groups 40..400 m away; Havok
`WorldQueries::raycast` for ground; overlay vertices and stroke font; OpenXR haptics; the mod's
`ImpactSound::Finisher`; start-anywhere via the phone.

**Not done (needs new, validated hooks; listed in CRIME_RUN.md for a later pass):**

1. **True spawning** of enemies or of the game's own crimes/activities. REFERENCE.md's only spawn paths are
   the web shooter's shots (`SpawnShot 0x2150830`). There is no validated call that starts a street crime.
2. **The game's crime objects**: type, location, stage, the game's own "crime stopped" event (and XP).
3. **Native map / HUD markers**: `AddPOI(id, category, subtype)` / `ClearPOI(id)` are only *logged* through
   the UI's call wrapper in REFERENCE.md ("The world markers"). Calling them needs thread, lifetime, and id
   validation so they cannot collide with the game's.
4. **Crime audio**: no siren / police-radio sound id has been captured. `game_sound` can play an id once one
   exists.
5. **Ground everywhere**: rays only hit loaded collision. A far beacon may sit on a roof; a miss is retried
   each second once the city streams in. A nav-mesh or street-level query would do better; none is validated
   for this.

**Gameplay limits of the prototype:** beacons are visible through walls (a waypoint, not a line of sight);
real crimes include any enemies in play (missions too); a crime the game cleans up on its own before the
player reaches it becomes a beacon, not a stop; the score is the mod's — the game's own XP and crime count
are untouched.

## 7. How to hand the port back

The owner will bring the tree back to the Native Animation / Crime Run bot. Leave a trail a cold reader can
follow. Do not push onto `feat/crime-run` on SpidyVR-CrimeRun in a way that rewrites the prototype commits;
put the port on **your** origin branch (suggested name: `feat/crime-run` if that name is free on your repo,
otherwise `feat/crime-run-port`).

Include all of the following in a short `docs/HANDOFF_CRIME_RUN_PORT.md` (or a section at the bottom of this
file) and in the message you give the owner:

1. **Repo and branch** (clone URL + branch name).
2. **Commit list** (sha and player-facing subject), and which of `9cb99c3` / `6a09505` / `343ad4c` you
   cherry-picked vs. re-applied by hand.
3. **What changed in the port** — be specific: settings revision you used (`movementDefaultsRevision = ?`);
   launcher page the card landed on; phone app index if it was not 4; any renamed API you bound to (punches
   watch, `WorldQueries::raycast`, overlay list, `PhoneAction`, `NativeEyeFrame`, `play(ImpactSound)`,
   `writeTabOnly`); conflicts you resolved; anything you could not hook and why (missing census, older swing
   module, no phone, …).
4. **Tests:** crime_run `N/N`, core tests `N/N`, any other suite you ran.
5. **CI:** run URL, `publish=false`, pass/fail.
6. **Zip** (if you packaged one): path, size in bytes, sha256, and the commit it was built from. Zips go to
   the owner as chat attachments.

Once that is in their hands, the work comes back here. Thank you for keeping the mode off by default, the
hooks reversible, and the offsets honest.
