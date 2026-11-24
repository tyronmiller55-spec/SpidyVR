#include "spidy/crime_run.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/soft_shapes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace spidy::crime_run {
namespace {
constexpr float pi = 3.14159265f;
float clampF(float v, float lo, float hi, float fallback) {
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}
float across(Vec3 a, Vec3 b) {
    const float x = a.x - b.x, z = a.z - b.z;
    return std::sqrt(x * x + z * z);
}
// 23450 as "23,450". Writes into out (at least 16 bytes) and returns it.
char* grouped(int n, char* out, size_t cap) {
    if (!out || cap < 2) {
        if (out && cap)
            out[0] = 0;
        return out;
    }
    const bool negative = n < 0;
    const unsigned long long v = negative ? static_cast<unsigned long long>(-static_cast<long long>(n))
                                          : static_cast<unsigned long long>(n);
    char digits[24];
    const int count = std::snprintf(digits, sizeof digits, "%llu", v);
    if (count <= 0) {
        out[0] = '0';
        out[1] = 0;
        return out;
    }
    size_t i = 0;
    if (negative && i + 1 < cap)
        out[i++] = '-';
    for (int d = 0; d < count && i + 1 < cap; ++d) {
        if (d && (count - d) % 3 == 0 && i + 1 < cap)
            out[i++] = ',';
        out[i++] = digits[d];
    }
    out[i] = 0;
    return out;
}
// "1:05". Writes into out (at least 8 bytes) and returns it.
char* clockTextBuf(float seconds, char* out, size_t cap) {
    const int s = std::isfinite(seconds) ? std::max(0, static_cast<int>(std::ceil(seconds - 1e-3f))) : 0;
    std::snprintf(out, cap, "%d:%02d", s / 60, s % 60);
    return out;
}
} // namespace

std::string clockText(float seconds) {
    char text[16];
    return clockTextBuf(seconds, text, sizeof text);
}

Config sanitized(Config c) {
    const Config d;
    c.startScore = std::clamp(c.startScore, 1000, 100000);
    c.firstSpawnSeconds = clampF(c.firstSpawnSeconds, 0, 60, d.firstSpawnSeconds);
    c.spawnSeconds = clampF(c.spawnSeconds, 10, 300, d.spawnSeconds);
    c.radiusMin = clampF(c.radiusMin, 30, 1000, d.radiusMin);
    c.radiusMax = clampF(c.radiusMax, c.radiusMin + 10, 1500, std::max(d.radiusMax, c.radiusMin + 10));
    c.timerSeconds = clampF(c.timerSeconds, 20, 600, d.timerSeconds);
    c.stopPoints = std::clamp(c.stopPoints, 0, 10000);
    c.speedBonusPct = std::clamp(c.speedBonusPct, 0, 300);
    c.thugPoints = std::clamp(c.thugPoints, 0, 2000);
    c.missPenalty = std::clamp(c.missPenalty, 0, 20000);
    c.winStops = std::clamp(c.winStops, 1, 200);
    c.maxSites = std::clamp(c.maxSites, 1, static_cast<int>(Run::mostSites));
    c.reachRadius = clampF(c.reachRadius, 3, 60, d.reachRadius);
    c.reachHeight = clampF(c.reachHeight, 3, 200, d.reachHeight);
    c.holdSeconds = clampF(c.holdSeconds, 0, 30, d.holdSeconds);
    c.thugRadius = clampF(c.thugRadius, 10, 80, d.thugRadius);
    c.realMin = clampF(c.realMin, 0, c.radiusMax, d.realMin);
    c.clearSeconds = clampF(c.clearSeconds, .2f, 10, d.clearSeconds);
    c.censusSeconds = clampF(c.censusSeconds, .2f, 5, d.censusSeconds);
    c.staleSeconds = clampF(c.staleSeconds, 1, 30, d.staleSeconds);
    return c;
}

const char* rankName(Rank r) {
    switch (r) {
    case Rank::rookie: return "ROOKIE";
    case Rank::streetLevel: return "STREET-LEVEL";
    case Rank::friendlyNeighborhood: return "FRIENDLY NEIGHBORHOOD";
    case Rank::spectacular: return "SPECTACULAR";
    case Rank::sensational: return "SENSATIONAL";
    case Rank::amazing: return "AMAZING";
    }
    return "ROOKIE";
}
int stopsFor(Rank r, int winStops) {
    static constexpr float shares[rankCount] = {0, .10f, .25f, .45f, .70f, 1.f};
    const int win = std::max(1, winStops);
    int stops = 0;
    for (int i = 1; i <= static_cast<int>(r) && i < rankCount; ++i) {
        const int share = static_cast<int>(std::ceil(shares[i] * static_cast<float>(win) - 1e-4f));
        stops = std::min(std::max(stops + 1, share), win);
    }
    return static_cast<int>(r) >= rankCount - 1 ? win : stops;
}
Rank rankFor(int stops, int winStops) {
    Rank best = Rank::rookie;
    for (int i = 1; i < rankCount; ++i)
        if (stops >= stopsFor(static_cast<Rank>(i), winStops))
            best = static_cast<Rank>(i);
    return best;
}
int pointsFor(const Config& c, float left, float total, uint32_t thugs) {
    const float share = total > 0 && std::isfinite(left) ? std::clamp(left / total, 0.f, 1.f) : 0.f;
    const float points = static_cast<float>(c.stopPoints) * (1 + static_cast<float>(c.speedBonusPct) / 100 * share) +
                         static_cast<float>(c.thugPoints) * static_cast<float>(thugs);
    return static_cast<int>(std::lround(points / 10)) * 10;
}
Vec3 ringPoint(Vec3 centre, float rMin, float rMax, float u, float v) {
    const float a = 2 * pi * std::clamp(u, 0.f, 1.f);
    const float lo = std::max(rMin, 0.f), hi = std::max(rMax, lo);
    const float r = std::sqrt(lo * lo + std::clamp(v, 0.f, 1.f) * (hi * hi - lo * lo));
    return {centre.x + std::cos(a) * r, centre.y, centre.z + std::sin(a) * r};
}
const char* whatName(What w) {
    switch (w) {
    case What::started: return "started";
    case What::spawned: return "spawned";
    case What::reached: return "reached";
    case What::stopped: return "stopped";
    case What::missed: return "missed";
    case What::ranked: return "ranked";
    case What::won: return "won";
    case What::lost: return "lost";
    case What::abandoned: return "abandoned";
    }
    return "?";
}

void Run::start(const Config& c, Vec3 player, uint64_t seed, std::vector<Event>& events) {
    (void)player;
    config_ = sanitized(c);
    phase_ = Phase::running;
    rng_ = Rng(seed);
    sites_ = {};
    count_ = 0;
    score_ = config_.startScore;
    stops_ = misses_ = streak_ = bestStreak_ = won_ = lost_ = 0;
    elapsed_ = 0;
    nextSpawn_ = config_.firstSpawnSeconds;
    nextId_ = 1;
    spawned_ = real_ = 0;
    abandoned_ = false;
    events.push_back({What::started, 0, Kind::beacon, 0, score_, Rank::rookie, 0});
}
void Run::abandon(std::vector<Event>& events) {
    if (phase_ != Phase::running)
        return;
    phase_ = Phase::idle;
    abandoned_ = true;
    count_ = 0;
    events.push_back({What::abandoned, 0, Kind::beacon, 0, score_, rank(), 0});
}
void Run::clear() {
    if (phase_ != Phase::running) {
        phase_ = Phase::idle;
        count_ = 0;
    }
}
void Run::ground(uint32_t id, float y) {
    if (!std::isfinite(y))
        return;
    for (size_t i = 0; i < count_; ++i)
        if (sites_[i].id == id && !sites_[i].grounded) {
            sites_[i].point.y = y;
            sites_[i].grounded = true;
        }
}
bool Run::taken(Vec3 p, float within) const {
    for (size_t i = 0; i < count_; ++i)
        if (across(sites_[i].point, p) < within)
            return true;
    return false;
}
const Site* Run::mostUrgent() const {
    const Site* best = nullptr;
    for (size_t i = 0; i < count_; ++i)
        if (!best || sites_[i].left < best->left)
            best = &sites_[i];
    return best;
}
const Site* Run::nearest(Vec3 player) const {
    const Site* best = nullptr;
    for (size_t i = 0; i < count_; ++i)
        if (!best || across(sites_[i].point, player) < across(best->point, player))
            best = &sites_[i];
    return best;
}
void Run::spawn(Vec3 player, const Census& census, std::vector<Event>& events) {
    if (count_ >= static_cast<size_t>(config_.maxSites) || count_ >= mostSites)
        return;
    Site s;
    s.id = nextId_++;
    s.total = s.left = config_.timerSeconds;
    bool placed = false;
    // A real crime: the biggest group of standing enemies within reach (ties: the nearer).
    if (config_.realCrimes && census.fresh && census.feet) {
        size_t bestCount = 0;
        float bestDistance = 0;
        Vec3 bestCentre{};
        for (size_t i = 0; i < census.count; ++i) {
            const Vec3 f = census.feet[i];
            const float d = across(f, player);
            if (!finite(f) || d < config_.realMin || d > config_.radiusMax || taken(f, config_.thugRadius * 1.5f))
                continue;
            size_t n = 0;
            Vec3 sum{};
            for (size_t j = 0; j < census.count; ++j)
                if (finite(census.feet[j]) && across(census.feet[j], f) <= config_.thugRadius) {
                    ++n;
                    sum += census.feet[j];
                }
            if (n > bestCount || (n == bestCount && d < bestDistance)) {
                bestCount = n;
                bestDistance = d;
                bestCentre = sum / static_cast<float>(n);
            }
        }
        if (bestCount > 0) {
            s.kind = Kind::thugs;
            s.point = bestCentre;
            s.grounded = true;
            s.thugs = s.thugsMost = static_cast<uint32_t>(bestCount);
            placed = true;
            ++real_;
        }
    }
    // A beacon on the ring, away from the open ones.
    for (int tries = 0; !placed && tries < 8; ++tries) {
        const float u = rng_.unit(), v = rng_.unit();
        const Vec3 p = ringPoint(player, config_.radiusMin, config_.radiusMax, u, v);
        if (tries < 7 && taken(p, config_.reachRadius * 4))
            continue;
        s.kind = Kind::beacon;
        s.point = p;
        s.grounded = false;
        placed = true;
    }
    sites_[count_++] = s;
    ++spawned_;
    events.push_back({What::spawned, s.id, s.kind, 0, score_, rank(), across(s.point, player)});
}
void Run::close(size_t i, bool stopped, std::vector<Event>& events) {
    const Site s = sites_[i];
    for (size_t j = i + 1; j < count_; ++j)
        sites_[j - 1] = sites_[j];
    --count_;
    if (stopped) {
        const Rank before = rank();
        const int points = pointsFor(config_, s.left, s.total, s.thugsMost);
        score_ += points;
        won_ += points;
        ++stops_;
        bestStreak_ = std::max(bestStreak_, ++streak_);
        events.push_back({What::stopped, s.id, s.kind, points, score_, rank(), 0});
        if (rank() != before)
            events.push_back({What::ranked, s.id, s.kind, 0, score_, rank(), 0});
    } else {
        const int points = std::min(config_.missPenalty, score_);
        score_ -= points;
        lost_ += points;
        ++misses_;
        streak_ = 0;
        events.push_back({What::missed, s.id, s.kind, -points, score_, rank(), 0});
    }
}
void Run::update(float dt, Vec3 player, const Census& census, std::vector<Event>& events) {
    if (phase_ != Phase::running || !std::isfinite(dt) || dt <= 0 || !finite(player))
        return;
    dt = std::min(dt, .25f);
    elapsed_ += dt;
    const bool counted = census.fresh && (census.feet || census.count == 0);
    for (size_t i = count_; i-- > 0;) {
        Site& s = sites_[i];
        s.left -= dt;
        const float flat = across(s.point, player), rise = std::abs(player.y - s.point.y);
        if (s.kind == Kind::beacon && config_.realCrimes && counted) {
            // Enemies standing at a beacon make it a real crime.
            uint32_t n = 0;
            Vec3 sum{};
            for (size_t j = 0; j < census.count; ++j)
                if (finite(census.feet[j]) && across(census.feet[j], s.point) <= config_.thugRadius) {
                    ++n;
                    sum += census.feet[j];
                }
            if (n) {
                s.kind = Kind::thugs;
                s.thugs = n;
                s.thugsMost = std::max(s.thugsMost, n);
                s.clearFor = 0;
                if (!s.grounded) {
                    s.point.y = sum.y / static_cast<float>(n);
                    s.grounded = true;
                }
            }
        }
        if (s.kind == Kind::beacon) {
            const bool inside = flat <= config_.reachRadius && (!s.grounded || rise <= config_.reachHeight);
            if (inside) {
                if (!s.reached) {
                    s.reached = true;
                    events.push_back({What::reached, s.id, s.kind, 0, score_, rank(), flat});
                }
                s.hold += dt;
                if (s.hold >= config_.holdSeconds) {
                    close(i, true, events);
                    continue;
                }
            } else {
                s.hold = std::max(0.f, s.hold - 2 * dt);
            }
        } else {
            if (counted) {
                s.staleFor = 0;
                uint32_t n = 0;
                Vec3 sum{};
                for (size_t j = 0; j < census.count; ++j)
                    if (finite(census.feet[j]) && across(census.feet[j], s.point) <= config_.thugRadius) {
                        ++n;
                        sum += census.feet[j];
                    }
                s.thugs = n;
                s.thugsMost = std::max(s.thugsMost, n);
                if (n) {
                    // The site follows its group, slowly (a fight moves about).
                    const Vec3 centre = sum / static_cast<float>(n);
                    const float follow = std::min(1.f, dt * .5f);
                    s.point.x += (centre.x - s.point.x) * follow;
                    s.point.z += (centre.z - s.point.z) * follow;
                    s.point.y = centre.y;
                    s.grounded = true;
                    s.clearFor = 0;
                } else {
                    s.clearFor += dt;
                }
            } else {
                s.staleFor += dt;
            }
            const float rose = std::abs(player.y - s.point.y);
            if (!s.reached && flat <= config_.thugRadius && rose <= config_.reachHeight) {
                s.reached = true;
                events.push_back({What::reached, s.id, s.kind, 0, score_, rank(), flat});
            }
            if (s.thugs == 0 && s.clearFor >= config_.clearSeconds) {
                if (s.reached) {
                    close(i, true, events);
                    continue;
                }
                // Gone before the player got there (streamed out, or someone else's doing): a beacon now.
                s.kind = Kind::beacon;
                s.hold = 0;
                s.clearFor = 0;
            } else if (s.staleFor >= config_.staleSeconds) {
                // No census for a while (the punches are off, the swing module restarted): a beacon now.
                s.kind = Kind::beacon;
                s.hold = 0;
                s.staleFor = 0;
            }
        }
        if (s.left <= 0)
            close(i, false, events);
    }
    if (score_ <= 0) {
        score_ = 0;
        phase_ = Phase::lost;
        count_ = 0;
        events.push_back({What::lost, 0, Kind::beacon, 0, score_, rank(), 0});
        return;
    }
    if (stops_ >= config_.winStops) {
        phase_ = Phase::won;
        count_ = 0;
        events.push_back({What::won, 0, Kind::beacon, 0, score_, rank(), 0});
        return;
    }
    nextSpawn_ -= dt;
    if (nextSpawn_ <= 0) {
        nextSpawn_ = std::max(nextSpawn_ + config_.spawnSeconds, .0f);
        if (nextSpawn_ <= 0)
            nextSpawn_ = config_.spawnSeconds;
        spawn(player, census.fresh ? census : Census{}, events);
    }
}
std::string Run::summary() const {
    const char* outcome = phase_ == Phase::won    ? "WON"
                          : phase_ == Phase::lost ? "LOST"
                          : abandoned_            ? "QUIT"
                          : phase_ == Phase::running ? "UNDER WAY"
                                                     : "NOT STARTED";
    char line[200], score[16], time[16], won[16], lost[16];
    std::snprintf(line, sizeof line,
                  "CRIME RUN %s: score %s, stops %d/%d, misses %d, rank %s, time %s, best streak %d, crimes %u "
                  "(real %u), +%s -%s",
                  outcome, grouped(score_, score, sizeof score), stops_, config_.winStops, misses_, rankName(rank()),
                  clockTextBuf(elapsed_, time, sizeof time), bestStreak_, spawned_, real_, grouped(won_, won, sizeof won),
                  grouped(lost_, lost, sizeof lost));
    return line;
}
bool better(int score, int stops, bool won, int bestScore, int bestStops) {
    return (won && score > bestScore) || stops > bestStops;
}

Toggle::Ask Toggle::update(float dt, bool pressed, bool running) {
    if (std::isfinite(dt) && dt > 0)
        confirm_ = std::max(0.f, confirm_ - dt);
    if (!running)
        confirm_ = 0;
    if (!pressed)
        return Ask::none;
    if (!running)
        return Ask::start;
    if (confirm_ > 0) {
        confirm_ = 0;
        return Ask::quit;
    }
    confirm_ = confirmSeconds;
    return Ask::confirm;
}


// ---- What the eyes show ----
namespace {
// Each vertex from `from` on pulled toward the viewer by `scale` (a far mark drawn nearer at its own angular size:
// every part of it scaled alike about the eyes looks the same to them, but for the two eyes' parallax).
void pullIn(std::vector<Vertex>& out, size_t from, Vec3 viewer, float scale) {
    if (scale >= 1)
        return;
    for (size_t i = from; i < out.size(); ++i)
        out[i].position = viewer + (out[i].position - viewer) * scale;
}
Vec3 pulse(const BeaconView& v) {
    const bool urgent = v.total > 0 && v.left < v.total * .25f;
    if (v.kind == Kind::thugs) // a police light: red and blue
        return std::fmod(v.clock * (urgent ? 4.f : 2.f), 1.f) < .5f ? Vec3{1, .08f, .08f} : Vec3{.15f, .35f, 1};
    if (urgent)
        return std::fmod(v.clock * 3, 1.f) < .5f ? Vec3{1, .1f, .05f} : Vec3{1, .85f, .1f};
    return {1, .25f, .05f}; // a flare's orange-red
}
} // namespace

void appendBeacon(std::vector<Vertex>& out, const BeaconView& view, Vec3 viewer, float pixelAngle) {
    if (!finite(view.point) || !finite(viewer) || !std::isfinite(pixelAngle) || pixelAngle <= 0)
        return;
    const size_t first = out.size();
    const Vec3 up{0, 1, 0};
    const float distance = std::max(length(view.point - viewer), .5f);
    const float flat = across(view.point, viewer);
    const Vec3 color = pulse(view);
    // The pillar: a billboard turned round the upright to the viewer, 150 m tall, at least 10 pixels wide, a
    // bright core and a glow fading out to the sides and the top.
    Vec3 toViewer = viewer - view.point;
    toViewer.y = 0;
    const Vec3 right = length(toViewer) > .01f ? normalized(cross(up, normalized(toViewer))) : Vec3{1, 0, 0};
    const float core = std::max(.6f, 5 * pixelAngle * distance), glow = core * 3.5f;
    const float bottom = view.point.y - 2, top = view.point.y + 150;
    const auto strip = [&](float inner, float outer, float alphaIn, float alphaOut, Vec3 c) {
        for (const float side : {-1.f, 1.f}) {
            const Vec3 a = view.point + right * (side * inner), b = view.point + right * (side * outer);
            const Vertex ab{{a.x, bottom, a.z}, c, alphaIn}, bb{{b.x, bottom, b.z}, c, alphaOut},
                at{{a.x, top, a.z}, c, 0}, bt{{b.x, top, b.z}, c, 0};
            soft::quad(out, ab, bb, bt, at);
        }
    };
    const Vec3 white{1, 1, 1};
    const Vec3 hot = color * .5f + white * .5f;
    // The core, whitish at its middle.
    {
        const Vertex l{view.point - right * core + Vec3{0, bottom - view.point.y, 0}, color, .9f},
            m{view.point + Vec3{0, bottom - view.point.y, 0}, hot, .95f},
            r{view.point + right * core + Vec3{0, bottom - view.point.y, 0}, color, .9f};
        const Vertex lt{l.position + Vec3{0, top - bottom, 0}, color, 0}, mt{m.position + Vec3{0, top - bottom, 0}, hot, 0},
            rt{r.position + Vec3{0, top - bottom, 0}, color, 0};
        soft::quad(out, l, m, mt, lt);
        soft::quad(out, m, r, rt, mt);
    }
    strip(core, glow, .35f, 0, color);
    // The zone on the ground, and a beacon's hold filling round it, while near enough to matter.
    if (view.grounded && flat < 300) {
        const float width = std::max(.35f, 3 * pixelAngle * distance);
        const Vec3 c = view.point + Vec3{0, .3f, 0};
        soft::softArc(out, c, {1, 0, 0}, {0, 0, 1}, view.zone - width, view.zone, 0, 2 * pi, color, .7f,
                      width * .5f, 48);
        if (view.kind == Kind::beacon && view.hold > 0)
            soft::softArc(out, c + Vec3{0, .05f, 0}, {1, 0, 0}, {0, 0, 1}, view.zone - width * 3, view.zone - width,
                          0, 2 * pi * std::clamp(view.hold, 0.f, 1.f), {.2f, 1, .3f}, .8f, width * .5f, 48);
    }
    // The label, at the viewer's height on the pillar (so it is on the horizon from afar), 22 pixels tall.
    {
        Vec3 anchor{view.point.x, std::clamp(viewer.y + 2, view.point.y + 3, top - 5), view.point.z};
        const float at = std::max(length(anchor - viewer), .5f);
        char text[48];
        const int metres = static_cast<int>(std::lround(flat));
        char left[16];
        if (view.kind == Kind::thugs)
            std::snprintf(text, sizeof text, "CRIME %u  %s  %d M", view.thugs, clockTextBuf(view.left, left, sizeof left),
                          metres);
        else
            std::snprintf(text, sizeof text, "CRIME  %s  %d M", clockTextBuf(view.left, left, sizeof left), metres);
        TextStyle style;
        style.height = 22 * pixelAngle * at;
        style.weight = .16f;
        style.color = view.total > 0 && view.left < view.total * .25f ? Vec3{1, .9f, .2f} : white;
        style.round = true;
        style.soft = style.height * .05f;
        const Vec3 face = normalized(viewer - anchor);
        Vec3 r = cross(up, face);
        r = length(r) > .01f ? normalized(r) : right;
        const Vec3 u = normalized(cross(face, r));
        const float w = textWidth(text, style.height);
        appendText(out, text, anchor - r * (w / 2), r, u, style);
    }
    float farthest = 0;
    for (size_t i = first; i < out.size(); ++i)
        farthest = std::max(farthest, length(out[i].position - viewer));
    pullIn(out, first, viewer, farthest > drawReach ? drawReach / farthest : 1.f);
}

HudView hudOf(const Run& run, Vec3 player) {
    HudView v;
    v.shown = run.phase() != Phase::idle;
    v.phase = run.phase();
    v.score = run.score();
    v.stops = run.stops();
    v.winStops = run.config().winStops;
    v.misses = run.misses();
    v.rank = run.rank();
    v.nextSpawn = run.nextSpawn();
    if (const Site* s = run.mostUrgent()) {
        v.target = true;
        v.targetPoint = s->point;
        v.targetLeft = s->left;
        v.targetThugs = s->thugs;
        v.targetKind = s->kind;
    }
    (void)player;
    return v;
}

void appendHud(std::vector<Vertex>& out, const HudView& view, Pose head) {
    if (!view.shown || !finite(head.position))
        return;
    const Vec3 up{0, 1, 0};
    Vec3 ahead = head.orientation.rotate({0, 0, -1});
    ahead.y = 0;
    if (length(ahead) < .1f) { // looking straight down or up: the top of the head is ahead
        ahead = head.orientation.rotate({0, 1, 0}) * -1.f;
        ahead.y = 0;
    }
    if (length(ahead) < .1f)
        ahead = {0, 0, -1};
    ahead = normalized(ahead);
    const Vec3 centre = head.position + ahead * 1.4f + Vec3{0, -.45f, 0};
    const Vec3 right = normalized(cross(ahead, up));
    const Vec3 face = normalized(head.position - centre);
    const Vec3 u = normalized(cross(face, right));
    const auto at = [&](float x, float y) { return centre + right * x + u * y; };
    // The panel: dark glass.
    {
        const float w = .4f, h = .1f;
        const Vec3 dark{.02f, .02f, .04f};
        soft::quad(out, {at(-w, -h), dark, .5f}, {at(w, -h), dark, .5f}, {at(w, h), dark, .5f},
                   {at(-w, h), dark, .5f});
        const Vec3 red{.85f, .05f, .05f};
        soft::quad(out, {at(-w, h), red, .9f}, {at(w, h), red, .9f}, {at(w, h + .008f), red, .9f},
                   {at(-w, h + .008f), red, .9f});
    }
    TextStyle style;
    style.weight = .14f;
    style.round = true;
    const auto line = [&](std::string_view text, float y, float height, Vec3 color, float x = -.38f) {
        style.height = std::min(height, fittedHeight(text, height, .76f));
        style.color = color;
        style.soft = style.height * .05f;
        appendText(out, text, at(x, y), right, u, style);
    };
    const Vec3 white{1, 1, 1}, gold{1, .82f, .2f}, green{.3f, 1, .4f}, red{1, .25f, .2f};
    char top[96], score[16], left[16];
    std::snprintf(top, sizeof top, "CRIME RUN  %s  STOPS %d/%d  %s", grouped(view.score, score, sizeof score), view.stops,
                  view.winStops, rankName(view.rank));
    line(top, .055f, .026f, gold);
    char middle[96];
    if (view.phase == Phase::won)
        std::snprintf(middle, sizeof middle, "RUN WON - FINAL %s", grouped(view.score, score, sizeof score));
    else if (view.phase == Phase::lost)
        std::snprintf(middle, sizeof middle, "RUN LOST - THE CITY FELL - %d STOPS", view.stops);
    else if (view.target) {
        const float metres = across(view.targetPoint, head.position);
        if (view.targetKind == Kind::thugs)
            std::snprintf(middle, sizeof middle, "CRIME %d M  %s  %u STANDING", static_cast<int>(std::lround(metres)),
                          clockTextBuf(view.targetLeft, left, sizeof left), view.targetThugs);
        else
            std::snprintf(middle, sizeof middle, "CRIME %d M  %s", static_cast<int>(std::lround(metres)),
                          clockTextBuf(view.targetLeft, left, sizeof left));
    } else
        std::snprintf(middle, sizeof middle, "NEXT CRIME IN %s", clockTextBuf(view.nextSpawn, left, sizeof left));
    line(middle, .01f, .03f, view.target && view.targetLeft < 20 ? red : white, -.3f);
    // The arrow to the crime, turned as it lies from the way the head faces.
    if (view.target && view.phase == Phase::running) {
        Vec3 to = view.targetPoint - head.position;
        to.y = 0;
        if (length(to) > .5f) {
            to = normalized(to);
            const float angle = std::atan2(dot(cross(ahead, to), up), dot(ahead, to));
            const Vec3 tip = u * std::cos(angle) - right * std::sin(angle);
            const Vec3 side = cross(face, tip);
            const Vec3 c = at(-.345f, .022f);
            const float s = .028f;
            const Vec3 color = view.targetKind == Kind::thugs ? Vec3{.3f, .5f, 1} : Vec3{1, .3f, .05f};
            out.insert(out.end(), {Vertex{c + tip * s, color, 1}, Vertex{c - tip * (s * .6f) + side * (s * .7f), color, 1},
                                   Vertex{c - tip * (s * .3f), color, 1}});
            out.insert(out.end(), {Vertex{c + tip * s, color, 1}, Vertex{c - tip * (s * .3f), color, 1},
                                   Vertex{c - tip * (s * .6f) - side * (s * .7f), color, 1}});
        }
    }
    if (view.confirming)
        line("PRESS A AGAIN TO QUIT THE RUN", -.04f, .022f, red);
    else if (view.flash[0] && view.flashAge >= 0 && view.flashAge < flashSeconds)
        line(view.flash, -.04f, .026f, view.good ? green : red);
    else if (view.misses)
        line("MISSES " + std::to_string(view.misses), -.04f, .018f, Vec3{.7f, .7f, .7f});
}

bool valid(const Ask& a) {
    if (a.magic != 0x53435241 || a.version != 1 || a.bytes != sizeof(Ask) || a.count > 4)
        return false;
    for (uint32_t i = 0; i < a.count; ++i)
        if (!std::isfinite(a.x[i]) || !std::isfinite(a.z[i]) || !std::isfinite(a.top[i]) || !a.ids[i] ||
            std::abs(a.x[i]) > 1e5f || std::abs(a.z[i]) > 1e5f || std::abs(a.top[i]) > 1e5f)
            return false;
    return true;
}
bool valid(const Live& l) {
    return l.magic == 0x5343524c && l.version == 1 && l.bytes == sizeof(Live) && l.count <= 16 && l.groundCount <= 4;
}

} // namespace spidy::crime_run
