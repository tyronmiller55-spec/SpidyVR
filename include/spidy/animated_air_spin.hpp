// ANIMATED AIR SPIN (vr_settings::Values::animatedAirSpin, Off|Flips|Flips +
// momentum, default Off): while a pose yield is active and the hero is
// airborne (freefall / swing / web / zip / airborne parkour or combat), turn
// the *animated* character the way SpidyVR mode turns its own body:
//
//  Flips: the RealisticFlip / AutomaticHandMotion / stick flip tilt
//   (GameMotionFrame::bodyTilt, the same quaternion the OG body takes as
//   native_body::Command::tilt) — full pitch, roll and yaw, not MODEL YAW's
//   yaw-only walk-facing request. Rotation momentum (swing release carry,
//   hand momentum, angular drag) is already integrated by RealisticFlip.
//  Flips + momentum: also lean the body along the mod's travel (dive nose
//   toward the velocity, climb nose-up) and bank into turns from the
//   lateral acceleration of the swing/native velocity.
//
// Visual + view only: the whole animated skeleton is turned about its head in
// the pose job (native_body::setYieldTilt → body::Targets::yieldTilt); the
// actor transform, physics and collision are untouched (no facing RVA). With
// ANIMATED HEAD Full the view follows the turned head joint (pitch/roll kept,
// never silently leveled). Grounded / wall states take the identity.
#pragma once
#include "spidy/head_probe.hpp"
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::animated_air_spin {

enum class Mode : int { off = 0, flips = 1, momentum = 2 };
inline Mode mode(int setting) {
    return setting >= 2 ? Mode::momentum : setting == 1 ? Mode::flips : Mode::off;
}
inline const char* modeName(Mode m) {
    return m == Mode::momentum ? "flips+momentum" : m == Mode::flips ? "flips" : "off";
}

constexpr float degree = .0174532925f;
// Provisional comfort limits for the momentum lean (not headset-tuned).
constexpr float maxDive = 60 * degree;  // nose-down toward a falling velocity
constexpr float maxClimb = 35 * degree; // nose-up toward a rising velocity
constexpr float maxBank = 35 * degree;  // into a turn
constexpr float leanSpeedLow = 4.f, leanSpeedHigh = 14.f; // m/s ramp
constexpr float gravity = 9.81f;

// Airborne states that turn the animated body. Wall (standing on it) and the
// ground keep the game's upright anim.
inline bool airborne(const head_probe::StateInputs& s) {
    if (s.grounded || s.wall != 0)
        return false;
    switch (head_probe::classify(s)) {
    case head_probe::State::air:
    case head_probe::State::swing:
    case head_probe::State::web:
    case head_probe::State::zip:
    case head_probe::State::parkour:
    case head_probe::State::combat:
        return true;
    default:
        return false;
    }
}

inline bool active(int setting, bool yielding, const head_probe::StateInputs& s) {
    return mode(setting) != Mode::off && yielding && airborne(s);
}

inline Quat unitQ(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!(n > 1e-6f) || !std::isfinite(n))
        return {};
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
inline float angleOf(Quat q) {
    q = unitQ(q);
    return 2 * std::acos(std::clamp(std::abs(q.w), 0.f, 1.f));
}
// Normalised lerp along the short way (smoothing only; small steps).
inline Quat nlerp(Quat a, Quat b, float t) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0)
        b = {-b.x, -b.y, -b.z, -b.w};
    t = std::clamp(t, 0.f, 1.f);
    return unitQ({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t});
}
inline bool finiteQ(Quat q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}

// The flip tilt in world axes, as game_xr hands the OG body (pose.tilt).
inline Quat worldFlip(Quat trackingTilt, float trackingYaw) {
    if (!finiteQ(trackingTilt) || !std::isfinite(trackingYaw))
        return {};
    const Quat yaw = Quat::yaw(trackingYaw);
    return unitQ(yaw * unitQ(trackingTilt) * yaw.conjugate());
}

inline float ramp(float speed) {
    if (!std::isfinite(speed))
        return 0;
    const float t = std::clamp((speed - leanSpeedLow) / (leanSpeedHigh - leanSpeedLow), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

struct Lean {
    Quat turn{};
    float pitch{}; // rad, + nose up
    float bank{};  // rad, magnitude
};
// The momentum lean for a velocity (m/s, world) and a horizontal lateral
// acceleration (m/s², world, already smoothed). actorForward: fallback travel
// direction while falling straight down.
inline Lean leanFor(Vec3 velocity, Vec3 lateralAccel, Vec3 actorForward) {
    Lean out;
    if (!finite(velocity))
        return out;
    const Vec3 up{0, 1, 0};
    const float speed = length(velocity);
    const float r = ramp(speed);
    if (r <= 0)
        return out;
    Vec3 horizontal{velocity.x, 0, velocity.z};
    const float h = length(horizontal);
    Vec3 dir = h > 1.f ? horizontal / h : Vec3{actorForward.x, 0, actorForward.z};
    if (!finite(dir) || length(dir) < .5f)
        return out;
    dir = normalized(dir);
    const float pitch = std::clamp(std::atan2(velocity.y, std::max(h, .01f)), -maxDive, maxClimb) * r;
    out.pitch = pitch;
    const Quat pitchTurn = arc(dir, normalized(dir * std::cos(pitch) + up * std::sin(pitch)));
    Quat bankTurn{};
    if (finite(lateralAccel)) {
        Vec3 lat{lateralAccel.x, 0, lateralAccel.z};
        lat = lat - dir * dot(lat, dir);
        const float a = length(lat);
        if (a > .5f) {
            const float bank = std::min(std::atan(a / gravity), maxBank) * r;
            out.bank = bank;
            const Vec3 side = lat / a;
            bankTurn = arc(up, normalized(up * std::cos(bank) + side * std::sin(bank)));
        }
    }
    out.turn = unitQ(bankTurn * pitchTurn);
    return out;
}

struct Inputs {
    bool active{};    // active(): setting on, yielding, airborne
    bool momentum{};  // Mode::momentum
    Quat flipWorld{}; // worldFlip(GameMotionFrame::bodyTilt(), trackingYaw)
    Vec3 velocity{};  // the swing's / native velocity, world m/s
    bool velocityFresh{};
    Vec3 actorForward{};
    float dt{};
};
struct Output {
    Quat body{};  // whole animated body, world axes, about the anim head
    Quat hands{}; // the rig's flip on the grips: native_body un-tilts the arm targets by it
    float flipDeg{}, leanDeg{}, bodyDeg{}, pitchDeg{}, bankDeg{};
};

// Smooths entry/exit so a yield or landing never snaps the skeleton.
class Tracker {
    Quat flip_{}, lean_{};
    Vec3 prevVelocity_{}, lateral_{};
    bool prevSet_{};
    float sinceSample_{};

  public:
    void reset() { *this = {}; }
    Output update(const Inputs& in) {
        Output out;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        const auto follow = [dt](Quat& value, Quat target, float tau) {
            const float k = dt > 0 ? 1 - std::exp(-dt / tau) : 0.f;
            value = nlerp(value, target, k);
        };
        // Lateral acceleration from velocity samples that actually advanced.
        sinceSample_ += dt;
        if (in.active && in.momentum && in.velocityFresh && finite(in.velocity)) {
            if (prevSet_ && sinceSample_ > 1e-3f &&
                length(in.velocity - prevVelocity_) > 1e-4f) {
                const Vec3 accel = (in.velocity - prevVelocity_) / sinceSample_;
                const Vec3 a = finite(accel) ? limited(Vec3{accel.x, 0, accel.z}, 4 * gravity) : Vec3{};
                const float k = 1 - std::exp(-sinceSample_ / .2f);
                lateral_ = lateral_ + (a - lateral_) * k;
                prevVelocity_ = in.velocity;
                sinceSample_ = 0;
            } else if (!prevSet_) {
                prevVelocity_ = in.velocity;
                prevSet_ = true;
                sinceSample_ = 0;
            }
        } else {
            prevSet_ = false;
            lateral_ = {};
        }
        Quat flipTarget{}, leanTarget{};
        if (in.active) {
            flipTarget = finiteQ(in.flipWorld) ? unitQ(in.flipWorld) : Quat{};
            if (in.momentum) {
                const Lean l = leanFor(in.velocity, lateral_, in.actorForward);
                leanTarget = l.turn;
                out.pitchDeg = l.pitch / degree;
                out.bankDeg = l.bank / degree;
            }
            out.hands = flipTarget;
        }
        follow(flip_, flipTarget, in.active ? .06f : .15f);
        follow(lean_, leanTarget, .25f);
        out.body = unitQ(lean_ * flip_);
        out.flipDeg = angleOf(flip_) / degree;
        out.leanDeg = angleOf(lean_) / degree;
        out.bodyDeg = angleOf(out.body) / degree;
        return out;
    }
    bool resting() const { return angleOf(flip_) < 1e-3f && angleOf(lean_) < 1e-3f; }
};

} // namespace spidy::animated_air_spin
