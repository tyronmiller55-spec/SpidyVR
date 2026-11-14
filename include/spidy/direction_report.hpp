#pragma once
// DIRECTION REPORT (the user, November 14 t64u: "there should be a direction report from the headset that
// everything is answering to every moment").
//
// One line, ten times a second and on every action edge, says which way the player looks and what every
// direction in the game was doing at that moment:
//   direction: hmd <headset's own turn off its latch> shown <the rig's turn> view <shown+hmd: where the
//   player looks> basis <the left stick's world basis> hero <the actor's facing> bone <the animated head
//   bone's yaw> momentum <the actor's travel direction> action <what happened last> <age> ms ago
// and once a second an audit: the degrees the view turned that the player did not make (the head's own
// turn, the right stick's turn, a snap or a realign are the player's; the rest is "imposed"), and for each
// consumer of the heading (eyes, the hand and marker map, the arm IK, the left stick) the worst skew
// between the yaw it used and the yaw published. All angles are degrees in the log, radians here, the rig's
// convention (0 facing -z, + to the left). Pure; no Windows, no game address.
#include "spidy/stable_yaw.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::direction_report {

using stable_yaw::degrees;
using stable_yaw::wrap;

enum class Action : uint8_t { none, turn, realign, jump, land, dodge, handoff, web, punch, count };
inline const char* actionName(Action a) {
    switch (a) {
    case Action::turn: return "turn";
    case Action::realign: return "realign";
    case Action::jump: return "jump";
    case Action::land: return "land";
    case Action::dodge: return "dodge";
    case Action::handoff: return "handoff";
    case Action::web: return "web fire";
    case Action::punch: return "punch";
    default: return "none";
    }
}

// Who reads the heading.
enum class Consumer : uint8_t { eyes, hands, arms, stick, count };
inline const char* consumerName(Consumer c) {
    switch (c) {
    case Consumer::eyes: return "eyes";
    case Consumer::hands: return "hands";
    case Consumer::arms: return "arms";
    default: return "stick";
    }
}

inline constexpr float lineSeconds = .1f, auditSeconds = 1.f;
inline constexpr float imposedFloor = 3.f * degrees; // a step under this is noise, not a turn
inline constexpr float skewFloor = .5f * degrees;

struct Frame {
    float seconds = 0;     // this frame's length
    bool ok = false;       // the view is known (the head lock is on)
    float hmd = 0;         // the headset's turn off its latch
    float shown = 0;       // the rig's turn (the lock)
    float view = 0;        // shown + hmd
    float basis = NAN;     // the left stick's world basis
    float hero = NAN;      // the actor's facing
    float bone = NAN;      // the animated head bone's yaw
    float momentum = NAN;  // the direction the actor travels (NAN under 1 m/s)
    float speed = 0;       // m/s
    float playerTurn = 0;  // the right stick's turn of the view this frame (radians)
    bool discontinuity = false; // a latch, a realign, a snap: the player's or a re-seat, not imposed
};

struct Audit {
    float imposedDeg = 0, worstStep = 0;
    uint32_t imposedFrames = 0, frames = 0, actions = 0;
    float skew[static_cast<int>(Consumer::count)]{};
    uint32_t skewFrames[static_cast<int>(Consumer::count)]{};
};

class Reporter {
  public:
    // An action edge: a line goes out on this frame.
    void action(Action a) {
        last_ = a;
        age_ = 0;
        edge_ = true;
        ++audit_.actions;
        ++total_[static_cast<int>(a)];
    }
    // Skew of a consumer against the published heading (radians).
    void skew(Consumer c, float used, float published) {
        if (!std::isfinite(used) || !std::isfinite(published))
            return;
        const float d = std::abs(wrap(used - published));
        const int i = static_cast<int>(c);
        audit_.skew[i] = std::max(audit_.skew[i], d);
        if (d > skewFloor)
            ++audit_.skewFrames[i];
    }
    struct Out {
        bool line = false;
        bool audit = false;
    };
    Out step(const Frame& f) {
        Out out;
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        age_ += dt;
        lineClock_ += dt;
        auditClock_ += dt;
        if (f.ok) {
            if (prevOk_ && !f.discontinuity) {
                const float moved = wrap(f.view - prevView_);
                const float own = wrap(f.hmd - prevHmd_) + f.playerTurn;
                const float imposed = std::abs(wrap(moved - own));
                ++audit_.frames;
                if (imposed >= imposedFloor) {
                    audit_.imposedDeg += imposed / degrees;
                    audit_.worstStep = std::max(audit_.worstStep, imposed);
                    ++audit_.imposedFrames;
                    imposedTotal_ += imposed / degrees;
                }
            }
            prevView_ = f.view;
            prevHmd_ = f.hmd;
            prevOk_ = true;
        } else {
            prevOk_ = false;
        }
        if (edge_ || lineClock_ >= lineSeconds) {
            out.line = f.ok;
            lineClock_ = 0;
            edge_ = false;
        }
        if (auditClock_ >= auditSeconds) {
            out.audit = true;
            auditClock_ = 0;
        }
        return out;
    }
    // "direction: ..." into out.
    int format(char* out, std::size_t size, const Frame& f) const {
        const auto deg = [](float v) { return std::isfinite(v) ? v / degrees : -999.f; };
        return std::snprintf(out, size,
                             "direction: hmd %+.0f shown %+.0f view %+.0f basis %+.0f hero %+.0f bone %+.0f "
                             "momentum %+.0f (%.1f m/s) action %s %d ms ago",
                             deg(f.hmd), deg(f.shown), deg(f.view), deg(f.basis), deg(f.hero), deg(f.bone),
                             deg(f.momentum), f.speed, actionName(last_), static_cast<int>(age_ * 1000.f + .5f));
    }
    // "direction audit: ..." into out; clears the window.
    int formatAudit(char* out, std::size_t size) {
        const int n = std::snprintf(
            out, size,
            "direction audit: imposed view turn %.0f deg in %u frames (worst step %.0f deg, %u of %u frames "
            "over %.0f deg); worst skew eyes %.1f hands %.1f arms %.1f stick %.1f deg (frames off: %u/%u/%u/%u); "
            "%u actions; imposed since start %.0f deg",
            audit_.imposedDeg, audit_.imposedFrames, audit_.worstStep / degrees, audit_.imposedFrames,
            audit_.frames, imposedFloor / degrees, audit_.skew[0] / degrees, audit_.skew[1] / degrees,
            audit_.skew[2] / degrees, audit_.skew[3] / degrees, audit_.skewFrames[0], audit_.skewFrames[1],
            audit_.skewFrames[2], audit_.skewFrames[3], audit_.actions, imposedTotal_);
        audit_ = {};
        return n;
    }
    const Audit& window() const {
        return audit_;
    }
    float imposedTotal() const {
        return imposedTotal_;
    }
    Action last() const {
        return last_;
    }
    float age() const {
        return age_;
    }
    uint32_t count(Action a) const {
        return total_[static_cast<int>(a)];
    }
    void reset() {
        prevOk_ = false;
    }

  private:
    Action last_ = Action::none;
    float age_ = 0, lineClock_ = 0, auditClock_ = 0, prevView_ = 0, prevHmd_ = 0, imposedTotal_ = 0;
    bool prevOk_ = false, edge_ = false;
    Audit audit_{};
    uint32_t total_[static_cast<int>(Action::count)]{};
};

// The actor's travel direction from its position change (radians in the rig's convention), NAN under
// minSpeed m/s or on a bad step.
inline float momentumYaw(float dx, float dz, float dt, float minSpeed = 1.f, float* speedOut = nullptr) {
    if (!std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(dt) || dt <= 1e-4f) {
        if (speedOut)
            *speedOut = 0;
        return NAN;
    }
    const float speed = std::hypot(dx, dz) / dt;
    if (speedOut)
        *speedOut = speed;
    if (speed < minSpeed || speed > 120.f)
        return NAN;
    return std::atan2(-dx, -dz);
}

} // namespace spidy::direction_report
