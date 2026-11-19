// SESSION DIAGNOSTICS: the feature lines (RUN LOGS' "run log:", WEB AIM's
// "forearm aim:", DODGE TUMBLE's "dodge tumble:", the state changes on the
// ground and in the air) where a player can see and send them. Until v4 they
// went only into the session report's timeline (game_prompts::note: 32 lines
// of 128 characters, "forearm aim:" every 2 s pushing out everything else),
// never into the launcher's console (the November 2 logs: not one such line in
// four sessions). Now each also goes into SpidyDiagData, which run_game_vr.py
// reads with the rest of a session, prints to the console and appends to
// reports/<session>-diag.log. The report's timeline is as before.
//
// Three rings, so the frequent lines cannot push out the rare ones: `main` for
// everything, `aim` for "forearm aim:" alone, `head` for HEAD PROBE's "head
// probe:" alone (about 10 a second while it is on; version 2). Each line is numbered by the
// ring's count of lines ever written, so a reader that falls behind knows how
// many it lost.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace spidy::diag_log {
inline constexpr unsigned mainLines = 128, aimLines = 16, headLines = 64, lineChars = 240, buildChars = 96;
enum class Channel : uint8_t { main, aim, head };
// Which ring a line belongs in: "forearm aim:" lines in their own.
inline Channel channelOf(std::string_view text) {
    if (text.substr(0, 12) == "forearm aim:")
        return Channel::aim;
    return text.substr(0, 11) == "head probe:" ? Channel::head : Channel::main;
}
template <unsigned N> struct Ring {
    uint64_t written{};
    char line[N][lineChars]{};
    void push(const char* text) {
        auto& slot = line[written % N];
        std::snprintf(slot, lineChars, "%s", text ? text : "");
        ++written;
    }
    // The newest `count` lines' text in writing order, at most N.
    const char* at(uint64_t index) const {
        return index < written && written - index <= N ? line[index % N] : nullptr;
    }
};
// SpidyDiagData (the stereo module's export, run_game_vr.py diag_snapshot):
// sequence odd while it is being written, the build's id, then the rings.
struct Data {
    uint32_t magic = 0x47414944 /* DIAG */, version = 2, bytes = sizeof(Data), reserved{};
    volatile int64_t sequence{};
    char build[buildChars]{};
    Ring<mainLines> main{};
    Ring<aimLines> aim{};
    Ring<headLines> head{}; // version 2
};
static_assert(offsetof(Data, sequence) == 16 && offsetof(Data, build) == 24 && offsetof(Data, main) == 120 &&
              offsetof(Data, aim) == 120 + 8 + mainLines * lineChars &&
              offsetof(Data, head) == 120 + 2 * 8 + (mainLines + aimLines) * lineChars &&
              sizeof(Data) == 120 + 3 * 8 + (mainLines + aimLines + headLines) * lineChars);
// The build's id: the version, and the git commit it was built from where the
// build knew it (CMake, at configure), else when it was built.
inline std::string buildId(const char* version, const char* commit, const char* stamp) {
    std::string out = "Spidy ";
    out += version && *version ? version : "?";
    if (commit && *commit)
        out += std::string(" (commit ") + commit + ")";
    else
        out += std::string(" (built ") + (stamp && *stamp ? stamp : "?") + ")";
    return out;
}
// WEB AIM's "forearm aim:" lines, at most when something a reader cares about
// changed: the fallback (why the controller's line was used), a turn moved by
// turnStep degrees or more, or heartbeat seconds since the last; and never
// more than once in quiet seconds.
class ChangeOnly {
  public:
    static constexpr float turnStep = 5, heartbeat = 10, quiet = .5f;
    bool update(float seconds, int reason, float turnDegrees) {
        since_ += seconds > 0 && seconds < 1 ? seconds : 0.f;
        const bool changed = !primed_ || reason != reason_ ||
                             (turnDegrees - turn_ >= turnStep || turn_ - turnDegrees >= turnStep);
        if (!primed_ || (changed && since_ >= quiet) || since_ >= heartbeat) {
            primed_ = true;
            reason_ = reason;
            turn_ = turnDegrees;
            since_ = 0;
            return true;
        }
        return false;
    }

  private:
    bool primed_{};
    int reason_{};
    float turn_{}, since_{};
};
// QUIET LOG (the 4d58169 playtest: six lines made up about 40% of 2.6 million - "head probe: cutscene" 36k,
// "forearm sign:" 62k, "shooter lock:" 51k, "spherical aim:" 36k, "animated hand:" 42k, "tracking lock:" 22k -
// and a game stuck at 8.7 fps for 104 minutes wrote 50+ MB). A line of one of those kinds is kept when its
// SHAPE (the line with its numbers taken out) changed, or `heartbeat` seconds passed since the last kept
// one of its kind; while the game is not stepping (slow mode), those kinds are kept only on a change and
// the rest of the lines at most once per slowHeartbeat seconds per shape. Pure; the clock is the caller's.
class Throttle {
  public:
    static constexpr double heartbeat = 2.0, slowHeartbeat = 10.0;
    // LOG LEVEL (log_level, beta polish G4; diag was 13.2 MB for 27 min, the head probe and the aim family over
    // 60% of it): 2 VERBOSE keeps the 12 kinds below at the 2 s heartbeat, as before; 1 (the default) adds the head
    // probe's samples, the forearm aim and the player torso and repeats each kind's unchanged shape once per 5 s;
    // 0 QUIET once per 30 s. A changed shape (a new state or reason) is always kept at once.
    static constexpr unsigned kinds = 15, baseKinds = 12, shapeChars = 48;
    static constexpr double levelHeartbeat[3] = {30.0, 5.0, heartbeat};
    // Polish pass: and the repeats of the 4d58169 playtest (point jump: denied 296, handstand: no catch 673,
    // sidestep: denied 54, hybrid: adopt never seeded 27): a new REASON is a new shape and is kept at once.
    static constexpr const char* noisy[kinds] = {"head probe: cutscene", "forearm sign:",  "shooter lock:",
                                                 "spherical aim:",       "animated hand:", "animated hand points:",
                                                 "tracking lock:",       "vr stabilization: tilt",
                                                 "point jump: denied",   "sidestep: denied",
                                                 "handstand: no catch",  "hybrid: adopt never seeded",
                                                 "head probe:",          "forearm aim:",
                                                 "player torso:"};
    // Is `text` (without the time stamp) one of the noisy kinds? Its index, or -1. VERBOSE (level 2) leaves the
    // three added at the beta out.
    static int kindOf(std::string_view text, int level = 1) {
        const unsigned count = level >= 2 ? baseKinds : kinds;
        for (unsigned i = 0; i < count; ++i) {
            const std::string_view k = noisy[i];
            if (text.substr(0, k.size()) == k)
                return static_cast<int>(i);
        }
        return -1;
    }
    // The line with digits and number signs taken out (first shapeChars characters).
    static void shapeOf(std::string_view text, char (&out)[shapeChars]) {
        unsigned n = 0;
        for (const char c : text) {
            if ((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-')
                continue;
            if (n + 1 >= shapeChars)
                break;
            out[n++] = c;
        }
        out[n] = 0;
    }
    // The number of lines kept and dropped, per everything.
    uint64_t kept = 0, dropped = 0;
    void slow(bool on) {
        slow_ = on;
    }
    bool isSlow() const {
        return slow_;
    }
    void level(int l) {
        level_ = l < 0 ? 0 : l > 2 ? 2 : l;
    }
    int level() const {
        return level_;
    }
    bool allow(std::string_view text, double seconds) {
        const int kind = kindOf(text, level_);
        char shape[shapeChars];
        shapeOf(text, shape);
        if (kind >= 0) {
            auto& e = entry_[kind];
            const bool same = e.set && std::strncmp(e.shape, shape, shapeChars) == 0;
            const bool due = !e.set || seconds - e.at >= levelHeartbeat[level_] || seconds < e.at;
            if (same && (slow_ || !due)) {
                ++dropped;
                return false;
            }
            std::snprintf(e.shape, shapeChars, "%s", shape);
            e.set = true;
            e.at = seconds;
            ++kept;
            return true;
        }
        if (!slow_) {
            ++kept;
            return true;
        }
        // Slow mode: the other lines once per shape per slowHeartbeat seconds (a small table, oldest replaced).
        for (auto& e : other_)
            if (e.set && std::strncmp(e.shape, shape, shapeChars) == 0) {
                if (seconds - e.at < slowHeartbeat && seconds >= e.at) {
                    ++dropped;
                    return false;
                }
                e.at = seconds;
                ++kept;
                return true;
            }
        auto* oldest = &other_[0];
        for (auto& e : other_)
            if (!e.set || e.at < oldest->at)
                oldest = &e;
        std::snprintf(oldest->shape, shapeChars, "%s", shape);
        oldest->set = true;
        oldest->at = seconds;
        ++kept;
        return true;
    }

  private:
    struct Entry {
        bool set = false;
        double at = 0;
        char shape[shapeChars]{};
    };
    Entry entry_[kinds]{};
    Entry other_[32]{};
    bool slow_ = false;
    int level_ = 1;
};
// STARTUP STAGES and the VR worker's WATCHDOG. The November 3 sessions froze
// the headset at start, and the diagnostics said nothing of how far VR had got:
// now each stage is a "stage:" line (headset active, the eye views started, the
// first frame OpenXR accepted, 300 frames), and a watchdog thread writes a
// "watchdog:" line when no frame was submitted 10 s after the headset went
// active, or when the worker stopped coming round (a hang in the frame loop),
// with what it knew. Times in GetTickCount64 milliseconds; any thread.
class Stages {
  public:
    static constexpr uint64_t firstFrameMs = 10000, silentMs = 5000, repeatMs = 10000;
    enum Stage : int { none, headset, views, firstFrame, running };
    static const char* name(int stage) {
        switch (stage) {
        case headset: return "headset active, the eye views not started";
        case views: return "eye views started, waiting for the first eye images";
        case firstFrame: return "frames reaching the headset";
        case running: return "running (300 frames submitted)";
        default: return "starting OpenXR";
        }
    }
    int stage() const {
        return stage_.load();
    }
    uint64_t frames() const {
        return frames_.load();
    }
    // The stage reached, once each (a later stage never goes back); true when new.
    bool mark(int stage, uint64_t now) {
        int was = stage_.load();
        while (was < stage)
            if (stage_.compare_exchange_weak(was, stage)) {
                if (stage == headset)
                    headsetAt_ = now;
                return true;
            }
        return false;
    }
    void beat(uint64_t now) {
        beat_ = now;
    }
    // A new session: nothing reached yet (SpidyXrStart; the game may start VR
    // again in the same process).
    void reset() {
        stage_ = none;
        beat_ = headsetAt_ = lastFrame_ = frames_ = 0;
        warned_ = 0;
    }
    // A frame OpenXR accepted; the stage for its count, or none.
    int submitted(uint64_t now) {
        lastFrame_ = now;
        const uint64_t n = ++frames_;
        if (n == 1 && mark(firstFrame, now))
            return firstFrame;
        if (n == 300 && mark(running, now))
            return running;
        return none;
    }
    // A watchdog line into out when one is due, else false.
    bool check(uint64_t now, char* out, std::size_t size) {
        if (!out || !size || !beat_.load() || (warned_ && now - warned_ < repeatMs))
            return false;
        const uint64_t beat = beat_.load(), headsetAt = headsetAt_.load(), last = lastFrame_.load();
        const uint64_t frames = frames_.load();
        const auto ago = [&](uint64_t then) { return then && now >= then ? (now - then) / 1000.0 : -1.0; };
        if (now >= beat && now - beat >= silentMs) {
            std::snprintf(out, size,
                          "watchdog: the VR worker has not come round for %.1f s (stage: %s; %llu frames "
                          "submitted, the last %.1f s ago): the frame loop is stuck",
                          ago(beat), name(stage()), static_cast<unsigned long long>(frames), ago(last));
        } else if (headsetAt && !frames && now - headsetAt >= firstFrameMs) {
            std::snprintf(out, size,
                          "watchdog: no frame reached the headset %.1f s after it went active (stage: %s; "
                          "the worker came round %.1f s ago)",
                          ago(headsetAt), name(stage()), ago(beat));
        } else {
            return false;
        }
        warned_ = now;
        return true;
    }

  private:
    std::atomic<int> stage_{};
    std::atomic<uint64_t> beat_{}, headsetAt_{}, lastFrame_{}, frames_{};
    uint64_t warned_{}; // the watchdog thread's own
};
// Whether lines are kept (RUN LOGS; the VR worker sets it with the settings).
// Off by default: write() keeps nothing.
void enable(bool on);
bool enabled();
// The game is not stepping (stuck under about 15 fps): the diagnostics write only what changed (Throttle).
void setSlow(bool on);
// LOG LEVEL (log_level: 0 quiet, 1 default, 2 verbose; Throttle::level).
void setLevel(int level);
// Lines the Throttle kept and dropped since the session began.
uint64_t droppedLines();
// A line for the diagnostics (and only there: the report's timeline is
// game_prompts::note's, which calls this too), while enabled. Stamped with the
// seconds since the session's first line. Thread-safe. In the stereo module
// (diag_log.cpp).
void write(const char* text);
// Formatted, as printf.
void writef(const char* format, ...);
// The rings, copied into SpidyDiagData by the VR worker.
void copyOut(Data& out);
// This build's id (buildId with what CMake gave diag_log.cpp).
const char* build();
} // namespace spidy::diag_log
