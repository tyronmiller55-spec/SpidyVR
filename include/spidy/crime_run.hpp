#pragma once
// CRIME RUN (prototype; the owner, Nov 22 t103u): a roguelike random-crime mode. "Spiderman has to stop crimes to
// continue to progress, earning points and ranks for crimes stopped. For now, it can simply spawn crimes in a
// perimeter at regular intervals, with a bright indicator and negative points anytime Spidey misses one. You
// start at 20,000, and lose or gain until you hit zero, or stop a certain number of crimes ... something you start
// anywhere in the open world."
//
// Off by default (a mode: the CRIME RUN switch). With it on, the phone gets a CRIME RUN app; A on it starts a run
// wherever the player is (A twice within confirmSeconds quits one). Every spawnSeconds a crime appears, if fewer
// than maxSites are open:
//
//   - a REAL CRIME, when the game's own enemies are in play near the player (realCrimes on): the biggest group of
//     standing enemies between realMin and radiusMax away becomes the site. It is stopped once the player has
//     reached it and nobody of that group is left standing (knocked out, webbed up or gone) for clearSeconds.
//     The enemies come from the punches' own enemy list (game_targets::fighting, not cocooned): no new address.
//   - else a BEACON at a random point of the ring radiusMin..radiusMax round the player, its ground found by a
//     ray straight down (the swing's world queries). It is stopped by reaching it (reachRadius across, and
//     reachHeight up or down once its ground is known) and staying holdSeconds. Enemies who turn up standing
//     within thugRadius of a beacon make it a real crime: then they have to be stopped too.
//
// Every site has timerSeconds. A stop scores stopPoints, plus up to speedBonusPct more for the time left, plus
// thugPoints for each enemy the site had; a miss (the timer runs out) costs missPenalty. The score starts at
// startScore: the run is lost at 0 and won at winStops stops. Ranks go by stops (rankFor). The game itself is not
// changed: no enemy is spawned and no crime of the game's is started (no validated address for that).
//
// Engine independent: XR runs the Run, the swing module counts the enemies and casts the ground rays (Ask, Live).
#include "math.hpp"
#include "vertex.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spidy::crime_run {

struct Config {
    int startScore = 20000;
    float firstSpawnSeconds = 5;
    float spawnSeconds = 45;
    float radiusMin = 150, radiusMax = 400; // metres from the player, a beacon's ring
    float timerSeconds = 120;
    int stopPoints = 1000;
    int speedBonusPct = 100; // a stop at once scores this much more, one at the last moment none
    int thugPoints = 200;    // each enemy a real crime had
    int missPenalty = 2000;
    int winStops = 20;
    int maxSites = 2;
    float reachRadius = 15;  // metres across, a beacon
    float reachHeight = 25;  // metres up or down, once its ground is known
    float holdSeconds = 3;   // in a beacon's zone
    bool realCrimes = true;
    float thugRadius = 30;   // enemies within this of a real crime are its
    float realMin = 40;      // a real crime this near counts (they stream in close)
    float clearSeconds = 1.5f;
    // An enemy census older than this is none: a real crime then waits, and one that stays without a census for
    // staleSeconds becomes a beacon.
    float censusSeconds = 1.f, staleSeconds = 6.f;
};
// Everything in range (the launcher's sliders' ranges).
Config sanitized(Config c);

enum class Phase : uint8_t { idle, running, won, lost };
enum class Kind : uint8_t { beacon, thugs };
enum class Rank : uint8_t { rookie, streetLevel, friendlyNeighborhood, spectacular, sensational, amazing };
constexpr int rankCount = 6;
const char* rankName(Rank r);
// The rank `stops` earns in a run to `winStops`: Rookie, then Street-Level at 10% of the target, Friendly
// Neighborhood at 25%, Spectacular at 45%, Sensational at 70% and Amazing at the target (each at least one stop
// above the one before).
Rank rankFor(int stops, int winStops);
// The stops a rank takes in a run to `winStops`.
int stopsFor(Rank r, int winStops);

// A stop's points: stopPoints, plus speedBonusPct of it scaled by the share of the timer left, plus thugPoints for
// each enemy; to the nearest 10.
int pointsFor(const Config& c, float left, float total, uint32_t thugs);

// A point of the ring between rMin and rMax round `centre` (level with it), from two numbers 0..1: the angle
// and the radius (even over the ring's area).
Vec3 ringPoint(Vec3 centre, float rMin, float rMax, float u, float v);

// xorshift64*: the same seed, the same run.
class Rng {
  public:
    explicit Rng(uint64_t seed = 0x9e3779b97f4a7c15ull) : s_(seed ? seed : 0x9e3779b97f4a7c15ull) {}
    uint64_t next() {
        s_ ^= s_ >> 12;
        s_ ^= s_ << 25;
        s_ ^= s_ >> 27;
        return s_ * 0x2545f4914f6cdd1dull;
    }
    float unit() { // [0, 1)
        return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24);
    }

  private:
    uint64_t s_;
};

struct Site {
    uint32_t id{};
    Kind kind = Kind::beacon;
    Vec3 point{};
    bool grounded{};     // point.y is the ground's (a ray, or the enemies' feet)
    float left{}, total{}; // seconds
    float hold{};        // seconds in a beacon's zone
    float clearFor{};    // seconds with nobody of a real crime standing
    float staleFor{};    // seconds a real crime has had no census
    bool reached{};
    uint32_t thugs{}, thugsMost{};
};

// The enemies standing now (feet, world metres), as the swing module counted them, and whether that count is
// recent (fresh: within censusSeconds).
struct Census {
    const Vec3* feet{};
    size_t count{};
    bool fresh{};
};

enum class What : uint8_t { started, spawned, reached, stopped, missed, ranked, won, lost, abandoned };
const char* whatName(What w);
struct Event {
    What what{};
    uint32_t site{};
    Kind kind{};
    int points{}; // + for a stop, - for a miss
    int score{};  // after it
    Rank rank{};
    float distance{}; // metres from the player at a spawn
};

class Run {
  public:
    static constexpr size_t mostSites = 4;
    // Starts a run where the player is (feet), the score at startScore.
    void start(const Config& c, Vec3 player, uint64_t seed, std::vector<Event>& events);
    // Ends a run under way, scoring nothing more.
    void abandon(std::vector<Event>& events);
    // Back to idle after a run has ended (won, lost or abandoned).
    void clear();
    // One frame: dt real seconds (0 or less, or a paused game: nothing moves), the player's feet, the enemies.
    void update(float dt, Vec3 player, const Census& census, std::vector<Event>& events);
    // A ray found site `id`'s ground at height y (world metres).
    void ground(uint32_t id, float y);

    Phase phase() const { return phase_; }
    bool running() const { return phase_ == Phase::running; }
    int score() const { return score_; }
    int stops() const { return stops_; }
    int misses() const { return misses_; }
    int bestStreak() const { return bestStreak_; }
    int streak() const { return streak_; }
    int pointsWon() const { return won_; }
    int pointsLost() const { return lost_; }
    Rank rank() const { return rankFor(stops_, config_.winStops); }
    float elapsed() const { return elapsed_; }
    float nextSpawn() const { return nextSpawn_; }
    uint32_t spawned() const { return spawned_; }
    uint32_t realSpawned() const { return real_; }
    const Config& config() const { return config_; }
    const std::array<Site, mostSites>& sites() const { return sites_; }
    size_t siteCount() const { return count_; }
    // The open site whose timer runs out first (nullptr for none).
    const Site* mostUrgent() const;
    // The open site nearest the player across (nullptr for none).
    const Site* nearest(Vec3 player) const;
    // One line for the log and the HUD: the outcome, the score, stops, misses, rank, time and streak.
    std::string summary() const;

  private:
    void spawn(Vec3 player, const Census& census, std::vector<Event>& events);
    void close(size_t i, bool stopped, std::vector<Event>& events);
    bool taken(Vec3 p, float within) const;
    Config config_;
    Phase phase_ = Phase::idle;
    Rng rng_;
    std::array<Site, mostSites> sites_{};
    size_t count_{};
    int score_{}, stops_{}, misses_{}, streak_{}, bestStreak_{}, won_{}, lost_{};
    float elapsed_{}, nextSpawn_{};
    uint32_t nextId_ = 1, spawned_{}, real_{};
    bool abandoned_{};
};

// A run's best: kept in the settings (crime_best, crime_best_stops). Whether `run` beat them: the higher score of a
// won run, else more stops.
bool better(int score, int stops, bool won, int bestScore, int bestStops);

// A start on the phone, and A twice within confirmSeconds to quit one under way.
class Toggle {
  public:
    static constexpr float confirmSeconds = 3.f;
    enum class Ask : uint8_t { none, start, confirm, quit };
    // `pressed`: A on the app this frame; `running`: a run is under way.
    Ask update(float dt, bool pressed, bool running);
    bool confirming() const { return confirm_ > 0; }

  private:
    float confirm_{};
};

// ---- What the eyes show ----
// A site as the overlay draws it: a pillar of light up from its point, a ring on its ground and its label.
struct BeaconView {
    Vec3 point{};
    Kind kind = Kind::beacon;
    bool grounded{};
    float left{}, total{};  // seconds
    float zone = 15;        // metres: the ring's radius
    float hold{};           // 0..1 of holdSeconds
    uint32_t thugs{};
    float clock{};          // seconds, for the pulse
};
// Pillar beacons draw at most this far (metres): one whose farthest vertex is further is drawn nearer at its own
// angular size (so no far plane cuts it), scaled about the eyes.
constexpr float drawReach = 90;
void appendBeacon(std::vector<Vertex>& out, const BeaconView& view, Vec3 viewer, float pixelAngle);
// The run's HUD: a panel held below the gaze, the score, stops and rank, the next crime's distance and time with
// an arrow to it, and the latest event's line.
struct HudView {
    bool shown{};
    Phase phase = Phase::idle;
    int score{}, stops{}, winStops{}, misses{};
    Rank rank{};
    bool target{};        // a site to point at
    Vec3 targetPoint{};
    float targetLeft{};   // seconds
    uint32_t targetThugs{};
    Kind targetKind{};
    char flash[64]{};     // the latest event's line (empty: none)
    float flashAge = -1;  // seconds since it
    bool good{};          // the flash is good news (green) or bad (red)
    bool confirming{};    // quit asked once
    float nextSpawn{};
};
constexpr float flashSeconds = 4.f;
// head: the eyes' pose in the world (x right, y up, -z ahead).
void appendHud(std::vector<Vertex>& out, const HudView& view, Pose head);
// The view of the run as it stands (flash and flashAge are the caller's).
HudView hudOf(const Run& run, Vec3 player);
// "1:05" for 65 seconds.
std::string clockText(float seconds);

// ---- The bridge: XR's ask to the swing module, and what it found ----
// SpidyCrimeAsk: the sites whose ground is still unknown (up to 4: id, x, z, and the height a ray down starts
// from), and that the census is wanted (sent every 250 ms while a run is under way; none for 2 s: no census).
struct Ask {
    uint32_t magic = 0x53435241, version = 1, bytes = sizeof(Ask), count{};
    uint32_t serial{}, pad{};
    uint32_t ids[4]{};
    float x[4]{}, z[4]{}, top[4]{};
};
// SpidyCrimeLive: the standing enemies (feet, nearest the player first, up to 16 of them), the census' serial
// (counts up with each new count), and the grounds of the latest ask's sites (hit 0: the ray found nothing - the
// city may not be loaded there yet). `sequence` is odd while it is written.
struct Live {
    uint32_t magic = 0x5343524c, version = 1, bytes = sizeof(Live), count{};
    int64_t sequence{};
    uint64_t census{};
    uint32_t askSerial{}, groundCount{};
    struct Thug {
        float x{}, y{}, z{}, pad{};
    } thugs[16]{};
    struct Ground {
        uint32_t id{}, hit{};
        float y{}, pad{};
    } grounds[4]{};
};
static_assert(sizeof(Ask) == 88 && sizeof(Live) == 360);
bool valid(const Ask& a);
bool valid(const Live& l);
// How far down a ground ray looks from `top`, metres.
constexpr float groundRay = 700;

} // namespace spidy::crime_run
