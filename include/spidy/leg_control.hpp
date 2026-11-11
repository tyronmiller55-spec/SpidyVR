#pragma once
// RIGHT-STICK LEG CONTROL (the user, November 10: "We're gonna put the
// procedural leg control on the right stick, but only when Controlled Flips is
// on ... smoothly blend between straight leg positions, jackknife for forward,
// left and right leg splays, and a rearward leg splay ... loosen the legs more
// so that momentum plays on them ... and we want momentum on them so that they
// affect rotation"; 21:03: "procedural legs are only in midair or during
// swinging").
//
// Pure and testable. The caller decides when the control is active (LEG
// CONTROL on, CONTROLLED FLIPS on, the body airborne or swinging free: never
// the ground, a wall, a crawl or parkour) and feeds the right stick and the
// body's motion; this gives each leg's hip angles and knee bend in the body's
// own frame, what that motion did to the body's turn (the reaction of the legs'
// swing, angular momentum kept), and how the legs' spread changes a flip's spin
// (the moment of inertia). body_ik places the feet from the angles; the
// tracking rig's FlipMotion turns the player by the reaction and scales the
// flip's rate.
//
// Angles, radians, per leg in the body's frame (forward, left, up):
//   pitch: the leg swung forward (+, toward a jackknife) or back (-);
//   side:  the leg swung toward the body's left (+) or right (-);
//   knee:  0 straight, up to kneeMax bent.
// Stick: x right, y forward (the controller's up). Centre: straight legs
// together. Forward: jackknife. Back: rearward splay (legs back, apart, knees
// a little bent). Left / right: both legs swung that way, the near leg wider.
// The blend is linear in the stick in each half-axis, so it is continuous and
// a diagonal mixes its two neighbours.
#include "spidy/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace spidy::leg_control {

struct Angles {
    float pitch{}, side{}, knee{};
};
using Legs = std::array<Angles, 2>; // 0 left, 1 right

// The five authored poses.
inline constexpr Legs centrePose{{{0.f, .03f, .06f}, {0.f, -.03f, .06f}}};
inline constexpr Legs forwardPose{{{1.45f, .05f, 0.f}, {1.45f, -.05f, 0.f}}};
inline constexpr Legs backPose{{{-.6f, .35f, .5f}, {-.6f, -.35f, .5f}}};
inline constexpr Legs leftPose{{{.15f, .75f, .1f}, {.15f, .3f, .1f}}};
inline constexpr Legs rightPose{{{.15f, -.3f, .1f}, {.15f, -.75f, .1f}}};

inline constexpr float pitchMin = -.95f, pitchMax = 1.75f, sideMax = 1.f, kneeMax = 1.2f,
                       rateMax = 14.f, stickDeadZone = .12f;

inline float finiteOr(float v, float fallback) {
    return std::isfinite(v) ? v : fallback;
}
inline Angles lerp(const Angles& a, const Angles& b, float t) {
    return {a.pitch + (b.pitch - a.pitch) * t, a.side + (b.side - a.side) * t, a.knee + (b.knee - a.knee) * t};
}
inline Angles plus(const Angles& base, const Angles& from, const Angles& to, float t) {
    return {base.pitch + (to.pitch - from.pitch) * t, base.side + (to.side - from.side) * t,
            base.knee + (to.knee - from.knee) * t};
}
// The stick's pose: inside the unit circle, past a small dead zone, each
// half-axis adds its pose's difference from centre by how far it is pushed.
inline Legs blend(float x, float y) {
    x = finiteOr(x, 0), y = finiteOr(y, 0);
    const float r = std::hypot(x, y);
    if (r > 1) {
        x /= r;
        y /= r;
    }
    const float m = std::hypot(x, y);
    const float k = m <= stickDeadZone ? 0.f : (m - stickDeadZone) / (1 - stickDeadZone) / std::max(m, 1e-6f);
    x *= k;
    y *= k;
    Legs out = centrePose;
    for (int i = 0; i < 2; ++i) {
        out[i] = plus(out[i], centrePose[i], forwardPose[i], std::max(y, 0.f));
        out[i] = plus(out[i], centrePose[i], backPose[i], std::max(-y, 0.f));
        out[i] = plus(out[i], centrePose[i], leftPose[i], std::max(-x, 0.f));
        out[i] = plus(out[i], centrePose[i], rightPose[i], std::max(x, 0.f));
        out[i].pitch = std::clamp(out[i].pitch, pitchMin, pitchMax);
        out[i].side = std::clamp(out[i].side, -sideMax, sideMax);
        out[i].knee = std::clamp(out[i].knee, 0.f, kneeMax);
    }
    return out;
}

// The tunables (the menu's LEG STIFFNESS, LEG DAMPING and LEG COUPLING, in
// percent of these defaults; LEG COUPLING 0..100 % of couplingFull).
struct Params {
    float frequencyHz = 1.6f;  // the legs' spring: lower is looser
    float dampingRatio = .35f; // under 1 overshoots and swings
    float coupling = .5f;      // 0..1: how much the legs turn the body
    float stickSeconds = .08f; // the stick's own smoothing
    float looseness = 1.f;     // how hard the body's motion throws the legs
};
inline constexpr float baseFrequencyHz = 1.6f, baseDampingRatio = .35f;
// The reaction share at full coupling: the legs are about a third of the
// body's mass and their swing turns the rest back by about this much.
inline constexpr float couplingFull = .3f;
// The flip's spin scale stays within these.
inline constexpr float spinScaleMin = .75f, spinScaleMax = 1.5f;
// The body's turn the legs may cause, radians a second at most.
inline constexpr float reactionRateMax = 2.f;
// From percent settings (100 = the defaults above).
inline Params fromPercent(int stiffnessPct, int dampingPct, int couplingPct) {
    Params p;
    p.frequencyHz = baseFrequencyHz * std::clamp(stiffnessPct, 10, 400) * .01f;
    p.dampingRatio = std::clamp(baseDampingRatio * std::clamp(dampingPct, 10, 400) * .01f, .05f, 1.5f);
    p.coupling = std::clamp(couplingPct, 0, 100) * .01f;
    return p;
}

// The body's moment of inertia about its pitch axis relative to straight
// legs: a jackknife or bent knees bring the legs' mass in, a splay takes it
// out a little. A flip's spin goes by its inverse, by `coupling`.
inline float relativeInertia(const Legs& legs) {
    float sum = 0;
    for (const auto& a : legs) {
        const float fold = (1 - std::cos(std::clamp(a.pitch, pitchMin, pitchMax))) * .5f;
        sum += 1 - .45f * fold - .15f * std::clamp(a.knee, 0.f, kneeMax) + .1f * std::min(std::abs(a.side), 1.f);
    }
    return std::clamp(sum * .5f, .5f, 1.3f);
}
inline float spinScale(const Legs& legs, float coupling) {
    const float c = std::clamp(finiteOr(coupling, 0), 0.f, 1.f);
    return std::clamp(1 + c * (1 / relativeInertia(legs) - 1), spinScaleMin, spinScaleMax);
}

// The body's motion this frame, in the body's own frame (forward, left, up
// components): the specific force (acceleration minus gravity, m/s^2; zero in
// a free fall, +9.8 up hanging still) and the angular velocity (rad/s about
// the body's forward, left and up axes: x = about forward, y = about left,
// z = about up).
struct Motion {
    Vec3 specificForce{};
    Vec3 angularVelocity{};
};

struct Output {
    Legs legs{};
    // The body's turn this frame from the legs' swing (radians), the legs'
    // own turn reversed by the coupling share: about the body's right axis
    // (a jackknife's legs turn about +right, so the body goes nose down,
    // about -right) and about its forward axis (legs swung left turn about
    // +forward; the body rolls back about -forward).
    float reactionPitch{}, reactionRoll{};
    // A flip's rate multiplied by this (tighter legs spin faster).
    float spinScale = 1;
    bool active{};
};

// The legs' state: a spring-damper per hip angle toward the stick's pose,
// driven by the body's specific force (the legs hang and swing with it, as a
// pendulum does) and its angular acceleration (they lag a turn), with the
// body's yaw rate throwing them apart. Semi-implicit Euler in substeps of at
// most 1/240 s, velocities and angles clamped: stable for every setting.
class Simulation {
  public:
    static constexpr float legLength = .9f, substep = 1.f / 240;
    void reset() {
        *this = Simulation{};
    }
    const Legs& legs() const {
        return legs_;
    }
    const Legs& target() const {
        return target_;
    }
    // seconds since the last update; active: the control owns the legs now
    // (when false the state resets and nothing turns); x, y: the stick for
    // the legs (StickDoubleTap's legX/legY).
    Output update(float seconds, bool active, float x, float y, const Motion& motion, const Params& p) {
        Output out;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!active) {
            reset();
            out.legs = centrePose;
            return out;
        }
        out.active = true;
        x = finiteOr(x, 0), y = finiteOr(y, 0);
        if (!started_) {
            started_ = true;
            legs_ = centrePose;
            vel_ = {};
            stickX_ = x;
            stickY_ = y;
            prevW_ = finite(motion.angularVelocity) ? motion.angularVelocity : Vec3{};
        }
        const float follow = p.stickSeconds > 1e-4f ? 1 - std::exp(-dt / p.stickSeconds) : 1.f;
        stickX_ += (x - stickX_) * follow;
        stickY_ += (y - stickY_) * follow;
        target_ = blend(stickX_, stickY_);
        const Vec3 f = finite(motion.specificForce) ? limited(motion.specificForce, 60.f) : Vec3{};
        const Vec3 w = finite(motion.angularVelocity) ? limited(motion.angularVelocity, 20.f) : Vec3{};
        const Vec3 alpha = dt > 1e-4f ? limited((w - prevW_) / dt, 80.f) : Vec3{};
        prevW_ = w;
        const float omega = 2 * 3.14159265f * std::clamp(finiteOr(p.frequencyHz, baseFrequencyHz), .2f, 8.f);
        const float zeta = std::clamp(finiteOr(p.dampingRatio, baseDampingRatio), .05f, 1.5f);
        const float loose = std::clamp(finiteOr(p.looseness, 1.f), 0.f, 2.f);
        const float pendulum = 1.5f / legLength * loose; // rod about its end
        const Legs before = legs_;
        float left = dt;
        while (left > 1e-6f) {
            const float h = std::min(left, substep);
            left -= h;
            for (int i = 0; i < 2; ++i) {
                Angles& a = legs_[i];
                Angles& v = vel_[i];
                const float sign = i == 0 ? 1.f : -1.f;
                // The "gravity" the legs feel is minus the specific force:
                // its pull along each angle's direction of travel.
                const Vec3 g{-f.x, -f.y, -f.z};
                const float cp = std::cos(a.pitch), sp = std::sin(a.pitch);
                const float cs = std::cos(a.side), ss = std::sin(a.side);
                // d(dir)/d(pitch) = cos p * forward + sin p * up;
                // d(dir)/d(side) = cos s * left + sin s * up.
                const float pullPitch = pendulum * (g.x * cp + g.z * sp);
                const float pullSide = pendulum * (g.y * cs + g.z * ss);
                // A body turn the legs lag: forward pitch is about +right
                // (-left), a left swing about +forward.
                const float lagPitch = alpha.y * loose;
                const float lagSide = -alpha.x * loose;
                // A yaw spin throws the legs out from its axis.
                const float spread = sign * .35f * w.z * w.z * loose * cs;
                const float accPitch = omega * omega * (target_[i].pitch - a.pitch) - 2 * zeta * omega * v.pitch +
                                       pullPitch + lagPitch;
                const float accSide = omega * omega * (target_[i].side - a.side) - 2 * zeta * omega * v.side +
                                      pullSide + lagSide + spread;
                v.pitch = std::clamp(v.pitch + accPitch * h, -rateMax, rateMax);
                v.side = std::clamp(v.side + accSide * h, -rateMax, rateMax);
                a.pitch += v.pitch * h;
                a.side += v.side * h;
                if (a.pitch < pitchMin || a.pitch > pitchMax) {
                    a.pitch = std::clamp(a.pitch, pitchMin, pitchMax);
                    v.pitch = 0;
                }
                if (std::abs(a.side) > sideMax) {
                    a.side = std::clamp(a.side, -sideMax, sideMax);
                    v.side = 0;
                }
                // The knee follows its target, critically damped-ish.
                const float kf = 1 - std::exp(-h * omega * .8f);
                a.knee = std::clamp(a.knee + (target_[i].knee - a.knee) * kf, 0.f, kneeMax);
            }
        }
        out.legs = legs_;
        // Angular momentum kept: the legs' swing turns the body back by a
        // share of it. Symmetric splays cancel; both legs one way roll it.
        const float share = couplingFull * std::clamp(finiteOr(p.coupling, 0), 0.f, 1.f);
        const float dPitch = (legs_[0].pitch - before[0].pitch + legs_[1].pitch - before[1].pitch) * .5f;
        const float dSide = (legs_[0].side - before[0].side + legs_[1].side - before[1].side) * .5f;
        const float cap = reactionRateMax * dt;
        out.reactionPitch = std::clamp(-share * dPitch, -cap, cap);
        out.reactionRoll = std::clamp(-share * dSide, -cap, cap);
        out.spinScale = spinScale(legs_, p.coupling);
        return out;
    }

  private:
    Legs legs_ = centrePose, target_ = centrePose;
    std::array<Angles, 2> vel_{};
    float stickX_{}, stickY_{};
    Vec3 prevW_{};
    bool started_{};
};

// Where a leg's foot goes: from the hip, along the angles in the body's frame,
// as far as the knee lets it (reach: thigh + shin).
inline Vec3 footDirection(const Angles& a, Vec3 forward, Vec3 left, Vec3 up) {
    const float cp = std::cos(a.pitch), sp = std::sin(a.pitch);
    const float cs = std::cos(a.side), ss = std::sin(a.side);
    return normalized(forward * (sp * cs) + left * ss - up * (cp * cs));
}
inline float footReach(const Angles& a, float reach) {
    return reach * std::clamp(.98f - .32f * std::clamp(a.knee, 0.f, kneeMax), .5f, .98f);
}
// The knee's pole: ahead of the leg, turning with its pitch.
inline Vec3 kneePole(const Angles& a, Vec3 forward, Vec3 up) {
    return normalized(forward * std::cos(a.pitch) + up * std::sin(a.pitch));
}

} // namespace spidy::leg_control
