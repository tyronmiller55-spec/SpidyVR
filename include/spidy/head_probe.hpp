// HEAD PROBE (vr_settings::Values::headProbe, off by default): step one of a
// head lock - the game's own parkour and swing animations playing in VR with
// the view on Spider-Man's head. Before any of that, this measures. It is
// READ-ONLY: it never writes the game's pose, camera or movement.
//
// native_body (the pose writer's hook, after the game's animation wrote the
// hero's joints and before the body's solve changes them) fills a Sample with
// the head bone as the game's animation left it, the head as the body showed
// it, the headset the body was given, and the hero's root. The VR worker turns
// the samples into "head probe:" lines (their own diag_log ring, so they cannot
// push out the others): about 10 a second, state changes at once, a summary of
// the current state every 2 s and one per state when VR stops.
//
// Every line is made by a formatter with named fields (the V6 crash was a
// printf argument count that did not match its format): one function per
// line, its format checked by the compiler where it can (formatLine).
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spidy::head_probe {
// The diagnostics' line (diag_log::lineChars), less its "[%8.3f] " stamp.
inline constexpr std::size_t lineChars = 240, textChars = lineChars - 12;
// A printf-style write with the format checked where the compiler can.
#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
inline int formatLine(char* out, std::size_t size, const char* format, ...) {
    if (!out || !size)
        return 0;
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(out, size, format, args);
    va_end(args);
    if (n < 0)
        out[0] = 0;
    return n;
}

// Names the probe looks for in a rig, beside the head the body uses: when no
// head is found the line lists which of these the rig has (only hashes are in
// the rig, so names come from this list).
inline constexpr const char* candidateNames[] = {"head",      "neck",    "neck_A", "neck_B",  "neck_C",
                                                 "neck_01",   "neck1",   "Head",   "Neck",    "head_end",
                                                 "c_head",    "jaw",     "chest",  "a_body",  "pelvis",
                                                 "spine_neck"};
inline constexpr unsigned candidateCount = sizeof(candidateNames) / sizeof(candidateNames[0]);
static_assert(candidateCount <= 32);

// What the pose writer saw at one hero pose job (native_body::headSample).
struct Sample {
    uint64_t job{};          // counts the samples; 0 before any
    uint32_t joints{};       // the rig's
    uint32_t problem{};      // native_body's for the rig (0 none)
    int32_t head = -1;       // the head bone's index, -1 not found
    int32_t neck = -1;       // the neck's (by name, else the head's parent), -1 none
    uint32_t chain{};        // joints from the head up to the rig's root
    uint32_t candidates{};   // a bit per candidateNames entry the rig has
    uint32_t bodyOn{};       // the VR worker asked for the body (Command::bodyOn, live)
    uint32_t solved{};       // the body's solve changed this job's joints (the mod owns the pose)
    uint32_t headset{};      // eyes and facing are the headset's (a live command)
    uint32_t turned{};       // the instance's axes were a rotation (toWorld valid)
    Quat toWorld{};          // the hero's model axes in world
    Vec3 root{};             // the hero instance's position in world
    Vec3 gameHead{}, gameForward{}, gameUp{};   // model space, as the game's animation wrote it
    Vec3 shownHead{};                           // model space, after the body's solve (gameHead if none)
    Vec3 eyes{};             // the headset between the eyes, world axes, relative to the feet
    Quat facing{};           // the headset, world axes (OpenXR: -z ahead)
};

// The states, as the mod already tells them apart (ground_state::Sample,
// game_pad's gate): cutscene is any time the mod has no play to sample (the
// game's camera, a menu, a scene).
enum class State : uint8_t { ground, air, swing, web, zip, wall, parkour, combat, cutscene, count };
inline constexpr unsigned stateCount = static_cast<unsigned>(State::count);
inline const char* stateName(State s) {
    switch (s) {
    case State::ground: return "ground";
    case State::air: return "air";
    case State::swing: return "swing";
    case State::web: return "web";
    case State::zip: return "zip";
    case State::wall: return "wall";
    case State::parkour: return "parkour";
    case State::combat: return "combat";
    case State::cutscene: return "cutscene";
    default: return "?";
    }
}
struct StateInputs {
    bool playing{};   // gameplay with the VR view (not a scene, menu or the game's camera)
    bool fresh{};     // the swing's sample is recent
    bool grounded{};  // the game's ground contact
    bool owned{};     // the swing drives the body's movement
    bool webbed{};    // a web holds
    bool zipping{};   // a zip asked for
    bool fighting{};  // a fight
    bool dodging{};   // a dodge asked for
    bool runRt{};     // NATIVE RUN holds the game's RT (its sprint, vaults, mantles, wall runs)
    unsigned wall{};  // game_swing::Surface (0 none)
};
inline State classify(const StateInputs& s) {
    if (!s.playing || !s.fresh)
        return State::cutscene;
    if (s.fighting || s.dodging)
        return State::combat;
    // The game's own parkour under NATIVE RUN: its vault, mantle or wall run.
    if (!s.owned && s.runRt && (!s.grounded || s.wall != 0))
        return State::parkour;
    if (s.zipping)
        return State::zip;
    if (s.webbed && s.owned && !s.grounded)
        return State::swing;
    if (s.webbed)
        return State::web;
    if (s.wall != 0)
        return State::wall;
    return s.grounded ? State::ground : State::air;
}

// Yaw (about the world's up, 0 along +z), pitch (up +) and roll, degrees, of
// a frame looking along `forward` with `up` as its up.
struct Angles {
    float yaw{}, pitch{}, roll{};
};
inline Angles anglesOf(Vec3 forward, Vec3 up) {
    constexpr float deg = 57.2957795f;
    Angles a;
    if (!finite(forward) || !finite(up) || length(forward) < 1e-6f)
        return {NAN, NAN, NAN};
    const Vec3 f = normalized(forward);
    a.yaw = std::atan2(f.x, f.z) * deg;
    a.pitch = std::asin(std::clamp(f.y, -1.f, 1.f)) * deg;
    Vec3 right = cross(Vec3{0, 1, 0}, f);
    if (length(right) < 1e-4f)
        return a; // straight up or down: no roll to tell
    right = normalized(right);
    const Vec3 levelUp = cross(f, right);
    a.roll = std::atan2(-dot(up, right), dot(up, levelUp)) * deg;
    return a;
}
// The angle between two frames given as forward and up, degrees: the larger
// of the two axes' turns (the turn itself is at least this, at most ~1.4x).
inline float frameAngle(Vec3 f0, Vec3 u0, Vec3 f1, Vec3 u1) {
    const auto between = [](Vec3 a, Vec3 b) {
        const float n = length(a) * length(b);
        return n > 1e-9f ? std::acos(std::clamp(dot(a, b) / n, -1.f, 1.f)) * 57.2957795f : 0.f;
    };
    return std::max(between(f0, f1), between(u0, u1));
}

// A fixed histogram (no allocation): quantiles to a bin, the max exact.
template <unsigned Bins> struct Histogram {
    float top{};
    uint32_t bins[Bins]{};
    uint32_t total{};
    float max{};
    explicit Histogram(float top_ = 1) : top(top_) {}
    void add(float v) {
        if (!std::isfinite(v) || v < 0)
            return;
        const unsigned b = std::min<unsigned>(Bins - 1, static_cast<unsigned>(v / top * Bins));
        ++bins[b];
        ++total;
        max = std::max(max, v);
    }
    // The upper edge of the bin holding the q-th quantile (the max if lower).
    float quantile(float q) const {
        if (!total)
            return 0;
        const double want = std::clamp(q, 0.f, 1.f) * total;
        uint64_t seen = 0;
        for (unsigned b = 0; b < Bins; ++b) {
            seen += bins[b];
            if (seen >= want && seen)
                return std::min(max, top * (b + 1) / Bins);
        }
        return max;
    }
};

// One state's numbers over the session.
struct StateStats {
    uint32_t samples{}, bad{}, jumps{}, modOwned{}, animYes{}, animNo{}, animUnknown{};
    Histogram<160> speed{16};       // head, world, m/s (0.1 m/s bins)
    Histogram<160> local{8};        // head against the root, m/s (0.05 m/s bins)
    Histogram<180> angular{1440};   // head, world, deg/s (8 deg/s bins)
    float maxRoll{}, maxPitch{};    // |roll|, |pitch| of the head bone, degrees
};

// ---- The lines, each from its named fields. ----
struct SampleLine {
    const char* state = "";
    bool modBody{}, swingMoves{};
    Vec3 head{};          // world
    Angles headAngles{};
    bool headset{};
    Angles headsetAngles{};
    Vec3 offset{};        // headset - head, world axes
    float offsetAngle{};  // degrees between the head's frame and the headset's
    float speed{}, localSpeed{}, angularSpeed{}; // m/s, m/s, deg/s
    float rollMax{}, pitchMax{};                 // |degrees| in the last ~1 s
    unsigned chain{};
    const char* anim = "unknown";
};
inline int formatSampleLine(char* out, std::size_t size, const SampleLine& l) {
    char hmd[48];
    if (l.headset)
        formatLine(hmd, sizeof hmd, "%.0f/%.0f/%.0f", static_cast<double>(l.headsetAngles.yaw),
                   static_cast<double>(l.headsetAngles.pitch), static_cast<double>(l.headsetAngles.roll));
    else
        formatLine(hmd, sizeof hmd, "%s", "n/a");
    return formatLine(out, size,
                      "head probe: %s body %s move %s head %.2f %.2f %.2f ypr %.0f/%.0f/%.0f hmd ypr %s off %.2f "
                      "%.2f %.2f (%.2f m %.0f deg) v %.2f vl %.2f w %.0f r1s %.0f p1s %.0f chain %u anim %s",
                      l.state ? l.state : "?", l.modBody ? "mod" : "game", l.swingMoves ? "swing" : "game",
                      static_cast<double>(l.head.x), static_cast<double>(l.head.y), static_cast<double>(l.head.z),
                      static_cast<double>(l.headAngles.yaw), static_cast<double>(l.headAngles.pitch),
                      static_cast<double>(l.headAngles.roll), hmd, static_cast<double>(l.offset.x),
                      static_cast<double>(l.offset.y), static_cast<double>(l.offset.z),
                      static_cast<double>(length(l.offset)), static_cast<double>(l.offsetAngle),
                      static_cast<double>(l.speed), static_cast<double>(l.localSpeed),
                      static_cast<double>(l.angularSpeed), static_cast<double>(l.rollMax),
                      static_cast<double>(l.pitchMax), l.chain, l.anim ? l.anim : "?");
}
struct EdgeLine {
    const char* from = "", *to = "";
    bool modBody{}, swingMoves{};
};
inline int formatEdgeLine(char* out, std::size_t size, const EdgeLine& l) {
    return formatLine(out, size, "head probe: state %s -> %s (body %s, move %s)", l.from ? l.from : "?",
                      l.to ? l.to : "?", l.modBody ? "mod" : "game", l.swingMoves ? "swing" : "game");
}
struct OwnerLine {
    const char* state = "";
    bool modBody{}, swingMoves{};
};
inline int formatOwnerLine(char* out, std::size_t size, const OwnerLine& l) {
    return formatLine(out, size, "head probe: owner in %s: body %s, move %s", l.state ? l.state : "?",
                      l.modBody ? "mod" : "game", l.swingMoves ? "swing" : "game");
}
struct BoneLine {
    bool found{};
    int head = -1, neck = -1;
    unsigned chain{}, joints{}, problem{};
    uint32_t candidates{};
};
inline int formatBoneLine(char* out, std::size_t size, const BoneLine& l) {
    char names[160]{};
    std::size_t at = 0;
    for (unsigned i = 0; i < candidateCount; ++i)
        if (l.candidates & (1u << i)) {
            const int n = formatLine(names + at, sizeof(names) - at, "%s%s", at ? "," : "", candidateNames[i]);
            if (n < 0 || at + n >= sizeof(names))
                break;
            at += static_cast<std::size_t>(n);
        }
    if (l.found)
        return formatLine(out, size, "head probe: head bone %d, neck %d, parent chain %u, rig %u joints (names: %s)",
                          l.head, l.neck, l.chain, l.joints, at ? names : "none");
    return formatLine(out, size, "head probe: head bone: not found (rig %u joints, problem %u; candidates found: %s)",
                      l.joints, l.problem, at ? names : "none");
}
struct AnimLine {
    const char* state = "";
    const char* anim = "unknown";
    bool modBody{};
    float gameMoved{}, gameTurned{}, shownMoved{}, rootMoved{}; // m, deg, m, m over the window
};
inline int formatAnimLine(char* out, std::size_t size, const AnimLine& l) {
    return formatLine(out, size,
                      "head probe: anim live: %s in %s (body %s): game head %.3f m %.1f deg in model space, shown "
                      "head %.3f m, body root %.2f m in 1 s",
                      l.anim ? l.anim : "?", l.state ? l.state : "?", l.modBody ? "mod" : "game",
                      static_cast<double>(l.gameMoved), static_cast<double>(l.gameTurned),
                      static_cast<double>(l.shownMoved), static_cast<double>(l.rootMoved));
}
struct SummaryLine {
    const char* kind = "summary"; // "summary" every 2 s, "session" when VR stops
    const char* state = "";
    const StateStats* stats{};
};
inline int formatSummaryLine(char* out, std::size_t size, const SummaryLine& l) {
    static const StateStats none{};
    const StateStats& s = l.stats ? *l.stats : none;
    const unsigned anim = s.animYes + s.animNo + s.animUnknown;
    const auto pct = [&](uint32_t n) { return anim ? 100.0 * n / anim : 0.0; };
    const bool stable = s.samples && !s.bad && s.jumps * 100 <= s.samples;
    return formatLine(out, size,
                      "head probe: %s %s n %u v %.1f/%.1f/%.1f vl %.2f/%.2f w %.0f/%.0f/%.0f r %.0f p %.0f "
                      "stable %s (bad %u jumps %u) anim y%.0f n%.0f ?%.0f%% mod %.0f%%",
                      l.kind ? l.kind : "summary", l.state ? l.state : "?", s.samples,
                      static_cast<double>(s.speed.quantile(.5f)), static_cast<double>(s.speed.quantile(.95f)),
                      static_cast<double>(s.speed.max), static_cast<double>(s.local.quantile(.5f)),
                      static_cast<double>(s.local.quantile(.95f)), static_cast<double>(s.angular.quantile(.5f)),
                      static_cast<double>(s.angular.quantile(.95f)), static_cast<double>(s.angular.max),
                      static_cast<double>(s.maxRoll), static_cast<double>(s.maxPitch), stable ? "yes" : "no", s.bad,
                      s.jumps, pct(s.animYes), pct(s.animNo), pct(s.animUnknown),
                      s.samples ? 100.0 * s.modOwned / s.samples : 0.0);
}

// The probe: samples in, lines out. No allocation, no locks; the VR worker's.
class Probe {
  public:
    static constexpr float sampleEvery = .1f, summaryEvery = 2, window = 1;
    // A jump: the game's head this far in model space from one sample to the next.
    static constexpr float jumpMetres = .5f;
    // anim live: yes when the game's head moved or turned this much in model
    // space in a window, no when less than the still limits.
    static constexpr float liveMetres = .01f, liveDegrees = 1, stillMetres = .002f, stillDegrees = .2f;
    static constexpr unsigned maxLines = 6;
    struct Lines {
        char text[maxLines][lineChars]{};
        unsigned count{};
        char* next() {
            return count < maxLines ? text[count++] : nullptr;
        }
    };
    void reset() {
        *this = Probe{};
    }
    const StateStats& stats(State s) const {
        return stats_[static_cast<unsigned>(s) % stateCount];
    }
    const char* anim() const {
        return animName(anim_);
    }
    // One frame: seconds since the last call, the state, who moves the body, the latest sample.
    void update(float seconds, State state, bool swingMoves, const Sample& s, Lines& out) {
        out.count = 0;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
        sinceSample_ += dt;
        sinceLine_ += dt;
        sinceSummary_ += dt;
        sinceJob_ += dt;
        const bool modBody = s.solved != 0;
        // The rig's head, once per rig (again when it changes).
        if (s.job && (!boneSaid_ || s.joints != boneJoints_ || s.head != boneHead_)) {
            boneSaid_ = true;
            boneJoints_ = s.joints;
            boneHead_ = s.head;
            if (char* t = out.next()) {
                BoneLine b;
                b.found = s.head >= 0;
                b.head = s.head;
                b.neck = s.neck;
                b.chain = s.chain;
                b.joints = s.joints;
                b.problem = s.problem;
                b.candidates = s.candidates;
                formatBoneLine(t, lineChars, b);
            }
        }
        // Edges at once.
        const bool stateChanged = primed_ && state != state_;
        const bool ownerChanged = primed_ && !stateChanged && (modBody != modBody_ || swingMoves != swingMoves_);
        if (stateChanged) {
            if (char* t = out.next())
                formatEdgeLine(t, lineChars, {stateName(state_), stateName(state), modBody, swingMoves});
        } else if (ownerChanged) {
            if (char* t = out.next())
                formatOwnerLine(t, lineChars, {stateName(state), modBody, swingMoves});
        }
        primed_ = true;
        state_ = state;
        modBody_ = modBody;
        swingMoves_ = swingMoves;
        if (stateChanged)
            window_ = {}; // anim live is per state
        // A new sample: the numbers.
        const bool fresh = s.job && s.job != job_;
        if (fresh && s.head >= 0) {
            measure(state, s, sinceJob_, out);
            sinceJob_ = 0;
        }
        if (fresh)
            job_ = s.job;
        // ~10 Hz sample lines, and edges with one at once.
        if (haveLine_ && (sinceLine_ >= sampleEvery || stateChanged || ownerChanged)) {
            if (char* t = out.next()) {
                line_.state = stateName(state);
                line_.modBody = modBody;
                line_.swingMoves = swingMoves;
                line_.anim = animName(anim_);
                formatSampleLine(t, lineChars, line_);
            }
            sinceLine_ = 0;
        }
        if (sinceSummary_ >= summaryEvery) {
            sinceSummary_ = 0;
            if (stats(state).samples)
                if (char* t = out.next())
                    formatSummaryLine(t, lineChars, {"summary", stateName(state), &stats(state)});
        }
    }
    // The session's lines, one per state that had samples; returns how many.
    unsigned session(char (*out)[lineChars], unsigned max) const {
        unsigned n = 0;
        for (unsigned i = 0; i < stateCount && n < max; ++i)
            if (stats_[i].samples)
                formatSummaryLine(out[n++], lineChars, {"session", stateName(static_cast<State>(i)), &stats_[i]});
        return n;
    }

  private:
    enum Anim : uint8_t { unknown, yes, no };
    static const char* animName(uint8_t a) {
        return a == yes ? "yes" : a == no ? "no" : "unknown";
    }
    struct Window {
        float seconds{};
        bool primed{};
        Vec3 gameHead{}, gameForward{}, gameUp{}, shownHead{}, root{};
        float gameMoved{}, gameTurned{}, shownMoved{}, rootMoved{};
        float roll{}, pitch{};
    };
    void measure(State state, const Sample& s, float dt, Lines& out) {
        auto& st = stats_[static_cast<unsigned>(state) % stateCount];
        ++st.samples;
        st.modOwned += s.solved != 0;
        const Quat w = s.turned ? s.toWorld : Quat{};
        const Vec3 headLocal = w.rotate(s.gameHead);
        const Vec3 head = s.root + headLocal;
        const Vec3 forward = w.rotate(s.gameForward), up = w.rotate(s.gameUp);
        const bool ok = finite(head) && finite(forward) && finite(up) && length(forward) > .1f && length(up) > .1f &&
                        finite(s.shownHead);
        if (!ok) {
            ++st.bad;
            havePrev_ = false;
            return;
        }
        const Angles a = anglesOf(forward, up);
        float speed = 0, localSpeed = 0, angular = 0;
        if (havePrev_ && dt > 1e-4f) {
            speed = length(head - prevHead_) / dt;
            localSpeed = length(headLocal - prevLocal_) / dt;
            angular = frameAngle(prevForward_, prevUp_, forward, up) / dt;
            if (length(s.gameHead - prevModel_) > jumpMetres)
                ++st.jumps;
            st.speed.add(speed);
            st.local.add(localSpeed);
            st.angular.add(angular);
        }
        st.maxRoll = std::max(st.maxRoll, std::abs(a.roll));
        st.maxPitch = std::max(st.maxPitch, std::abs(a.pitch));
        havePrev_ = true;
        prevHead_ = head;
        prevLocal_ = headLocal;
        prevModel_ = s.gameHead;
        prevForward_ = forward;
        prevUp_ = up;
        // The last ~1 s: this window and the one before.
        auto& win = window_;
        if (!win.primed) {
            win = {};
            win.primed = true;
            win.gameHead = s.gameHead;
            win.gameForward = s.gameForward;
            win.gameUp = s.gameUp;
            win.shownHead = s.shownHead;
            win.root = s.root;
        }
        win.seconds += dt;
        win.gameMoved = std::max(win.gameMoved, length(s.gameHead - win.gameHead));
        win.gameTurned = std::max(win.gameTurned, frameAngle(win.gameForward, win.gameUp, s.gameForward, s.gameUp));
        win.shownMoved = std::max(win.shownMoved, length(s.shownHead - win.shownHead));
        win.rootMoved = std::max(win.rootMoved, length(s.root - win.root));
        win.roll = std::max(win.roll, std::abs(a.roll));
        win.pitch = std::max(win.pitch, std::abs(a.pitch));
        if (win.seconds >= window) {
            const uint8_t was = anim_;
            anim_ = win.gameMoved >= liveMetres || win.gameTurned >= liveDegrees     ? yes
                    : win.gameMoved < stillMetres && win.gameTurned < stillDegrees ? no
                                                                                   : unknown;
            (anim_ == yes ? st.animYes : anim_ == no ? st.animNo : st.animUnknown) += 1;
            if (anim_ != was || !animSaid_)
                if (char* t = out.next()) {
                    AnimLine l;
                    l.state = stateName(state);
                    l.anim = animName(anim_);
                    l.modBody = s.solved != 0;
                    l.gameMoved = win.gameMoved;
                    l.gameTurned = win.gameTurned;
                    l.shownMoved = win.shownMoved;
                    l.rootMoved = win.rootMoved;
                    formatAnimLine(t, lineChars, l);
                }
            animSaid_ = true;
            lastRoll_ = win.roll;
            lastPitch_ = win.pitch;
            win = {};
        }
        // The line's fields, written at the next ~10 Hz line.
        line_.head = head;
        line_.headAngles = a;
        line_.headset = s.headset != 0 && finite(s.eyes);
        Vec3 hf{}, hu{};
        if (line_.headset) {
            hf = s.facing.rotate({0, 0, -1});
            hu = s.facing.rotate({0, 1, 0});
            line_.headsetAngles = anglesOf(hf, hu);
            line_.offset = s.eyes - headLocal;
            line_.offsetAngle = frameAngle(forward, up, hf, hu);
        } else {
            line_.headsetAngles = {};
            line_.offset = {};
            line_.offsetAngle = 0;
        }
        line_.speed = speed;
        line_.localSpeed = localSpeed;
        line_.angularSpeed = angular;
        line_.rollMax = std::max(lastRoll_, win.roll);
        line_.pitchMax = std::max(lastPitch_, win.pitch);
        line_.chain = s.chain;
        haveLine_ = true;
    }
    StateStats stats_[stateCount]{};
    State state_{};
    bool primed_{}, modBody_{}, swingMoves_{}, boneSaid_{}, haveLine_{}, havePrev_{}, animSaid_{};
    uint32_t boneJoints_{};
    int32_t boneHead_ = -2;
    uint64_t job_{};
    float sinceSample_{}, sinceLine_{}, sinceSummary_{}, sinceJob_{};
    Vec3 prevHead_{}, prevLocal_{}, prevModel_{}, prevForward_{}, prevUp_{};
    Window window_{};
    uint8_t anim_ = unknown;
    float lastRoll_{}, lastPitch_{};
    SampleLine line_{};
};
} // namespace spidy::head_probe
