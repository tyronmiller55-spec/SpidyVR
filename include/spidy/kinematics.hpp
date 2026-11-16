// BODY KINEMATICS (the user, November 9 22:27: "we need to know where head,
// hands, arms, body is at all times, along with rotation and velocity ...
// checks at every state change, but not bog down runtime"). One snapshot a
// frame - head, both hands (grip and web aim), forearms, feet, actor yaw and
// the view's turn - with velocities from the one before. Validated on every
// owner or state change; logged only then, on a new mismatch, or on a slow
// heartbeat. A few vector operations a frame. Pure: no Windows.
#pragma once
#include "math.hpp"
#include <cmath>
#include <cstdint>

namespace spidy::kinematics {

struct Snapshot {
    int64_t ns{-1};
    uint32_t owner{};  // 0 animation, 1 SpidyVR
    uint32_t state{};  // the caller's state code (head_probe::State)
    uint32_t epoch{};  // the source's: a new one (snap turn, re-latch, recenter, reseat) zeroes velocities
    bool headValid{}, feetValid{};
    bool handValid[2]{}, forearmValid[2]{};
    Pose head{}, grip[2]{}, aim[2]{};
    Vec3 elbow[2]{}, wrist[2]{};
    Vec3 feet{};
    float actorYaw{}, viewYaw{};
    float handResidual[2]{-1, -1}; // the rendered glove's miss of the tracked hand, m (-1 unknown)
    // Derived (Tracker::derive): m/s and rad/s.
    Vec3 headVelocity{}, feetVelocity{}, handVelocity[2]{};
    float headSpin{}, handSpin[2]{};
};

// Plausibility limits: what a real body can do, wide enough for flips.
struct Limits {
    float headAcross = 1.2f;   // head off the feet, across the ground
    float headBelow = -.4f;    // head below the feet (a hang may invert him)
    float headAbove = 2.6f;
    float reach = 1.3f;        // a hand from the head
    float residual = .12f;     // rendered glove from the tracked hand
    float speed = 80.f;        // any point, m/s (a teleport is a jump, not speed)
    float forearmAim = 2.4f;   // forearm against the web aim, radians (behind the arm)
};

enum Issue : uint32_t {
    nonFinite = 1,
    headOffFeet = 2,
    leftReach = 4,
    rightReach = 8,
    leftResidual = 16,
    rightResidual = 32,
    tooFast = 64,
    leftBackward = 128,
    rightBackward = 256,
};

inline bool finiteQuat(Quat q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}
inline uint32_t validate(const Snapshot& s, const Limits& k = {}) {
    uint32_t out{};
    // Any NaN or infinity anywhere a value is claimed (positions, turns,
    // velocities, yaws) is its own issue, never a silent pass of a check.
    if (s.headValid && (!finite(s.head.position) || !finiteQuat(s.head.orientation)))
        out |= nonFinite;
    if ((s.feetValid && !finite(s.feet)) || !finite(s.headVelocity) || !finite(s.feetVelocity) ||
        !std::isfinite(s.headSpin) || !std::isfinite(s.actorYaw) || !std::isfinite(s.viewYaw))
        out |= nonFinite;
    if (s.headValid && s.feetValid && finite(s.feet) && finite(s.head.position)) {
        const Vec3 d = s.head.position - s.feet;
        if (std::hypot(d.x, d.z) > k.headAcross || d.y < k.headBelow || d.y > k.headAbove)
            out |= headOffFeet;
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (!s.handValid[i])
            continue;
        if (!finite(s.grip[i].position) || !finite(s.aim[i].position) || !finiteQuat(s.grip[i].orientation) ||
            !finiteQuat(s.aim[i].orientation) || !finite(s.handVelocity[i]) || !std::isfinite(s.handSpin[i]) ||
            (s.forearmValid[i] && (!finite(s.elbow[i]) || !finite(s.wrist[i])))) {
            out |= nonFinite;
            continue;
        }
        if (s.headValid && length(s.grip[i].position - s.head.position) > k.reach)
            out |= i ? rightReach : leftReach;
        if (s.handResidual[i] > k.residual)
            out |= i ? rightResidual : leftResidual;
        if (length(s.handVelocity[i]) > k.speed)
            out |= tooFast;
        if (s.forearmValid[i]) {
            const Vec3 arm = s.wrist[i] - s.elbow[i];
            const Vec3 aim = s.aim[i].orientation.rotate({0, 0, -1});
            if (length(arm) > 1e-3f && length(aim) > .5f &&
                std::acos(std::fmax(-1.f, std::fmin(1.f, dot(normalized(arm), normalized(aim))))) > k.forearmAim)
                out |= i ? rightBackward : leftBackward;
        }
    }
    if (length(s.headVelocity) > k.speed || length(s.feetVelocity) > k.speed)
        out |= tooFast;
    return out;
}

inline float angleBetween(Quat a, Quat b) {
    const Quat d = a.conjugate() * b;
    const float w = std::fmin(1.f, std::fabs(d.w));
    return 2.f * std::acos(w);
}

class Tracker {
  public:
    // Velocities from the last snapshot; a gap (or a first frame) gives zero,
    // never a spike. Low-passed by `smooth` (0 none, toward 1 heavy).
    void derive(Snapshot& s, float smooth = .5f) {
        const float dt = last_.ns >= 0 && s.ns > last_.ns ? static_cast<float>(s.ns - last_.ns) * 1e-9f : 0.f;
        const bool ok = dt > 1e-4f && dt < .1f && s.epoch == last_.epoch;
        const auto blend = [&](Vec3 before, Vec3 now) { return before * smooth + now * (1 - smooth); };
        const auto bf = [&](float before, float now) { return before * smooth + now * (1 - smooth); };
        s.headVelocity = s.feetVelocity = s.handVelocity[0] = s.handVelocity[1] = {};
        s.headSpin = s.handSpin[0] = s.handSpin[1] = 0;
        if (ok && s.headValid && last_.headValid) {
            s.headVelocity = blend(last_.headVelocity, (s.head.position - last_.head.position) / dt);
            s.headSpin = bf(last_.headSpin, angleBetween(last_.head.orientation, s.head.orientation) / dt);
        }
        if (ok && s.feetValid && last_.feetValid)
            s.feetVelocity = blend(last_.feetVelocity, (s.feet - last_.feet) / dt);
        for (unsigned i = 0; i < 2; ++i)
            if (ok && s.handValid[i] && last_.handValid[i] && s.owner == last_.owner) {
                s.handVelocity[i] = blend(last_.handVelocity[i], (s.grip[i].position - last_.grip[i].position) / dt);
                s.handSpin[i] =
                    bf(last_.handSpin[i], angleBetween(last_.grip[i].orientation, s.grip[i].orientation) / dt);
            }
        last_ = s;
    }
    // Whether this frame's snapshot is worth a line. Always on an owner or
    // state change. Issues pass a HYSTERESIS first: a bit counts once it has
    // held raiseFrames frames in a row and stops counting after clearFrames
    // clean frames, so one flickering check cannot write a line a frame.
    // Then a newly counted issue is logged (at most one line per
    // minIntervalNs for issues), and while any lasts, a heartbeat.
    static constexpr int raiseFrames = 3, clearFrames = 15;
    static constexpr int64_t minIntervalNs = 250'000'000;
    bool shouldLog(const Snapshot& s, uint32_t issues, int64_t heartbeatNs = 5'000'000'000) {
        for (unsigned b = 0; b < bits; ++b) {
            const uint32_t bit = 1u << b;
            if (issues & bit) {
                clean_[b] = 0;
                if (held_[b] < raiseFrames && ++held_[b] >= raiseFrames)
                    active_ |= bit;
            } else {
                held_[b] = 0;
                if ((active_ & bit) && ++clean_[b] >= clearFrames)
                    active_ &= ~bit;
            }
        }
        const bool changed = s.owner != loggedOwner_ || s.state != loggedState_;
        const bool spaced = loggedNs_ < 0 || s.ns - loggedNs_ >= minIntervalNs;
        const bool fresh = (active_ & ~loggedIssues_) != 0 && spaced;
        const bool beat = active_ && (loggedNs_ < 0 || s.ns - loggedNs_ >= heartbeatNs);
        if (!(changed || fresh || beat)) {
            loggedIssues_ &= active_; // a cleared issue may be news again
            return false;
        }
        loggedIssues_ = active_;
        loggedOwner_ = s.owner;
        loggedState_ = s.state;
        loggedNs_ = s.ns;
        return true;
    }
    // The issues that passed the hysteresis (what a line reports).
    uint32_t active() const { return active_; }
    const Snapshot& last() const { return last_; }

  private:
    Snapshot last_{};
    uint32_t loggedOwner_ = ~0u, loggedState_ = ~0u, loggedIssues_{};
    int64_t loggedNs_{-1};
    static constexpr unsigned bits = 9;
    int held_[bits]{}, clean_[bits]{};
    uint32_t active_{};
};

} // namespace spidy::kinematics
