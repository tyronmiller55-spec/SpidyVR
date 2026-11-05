// ANIMATED ARMS (vr_settings::Values::animatedArms, off by default): while a
// pose yield is active (ANIMATED MOVES / AIR), overlay controller-driven arms
// on top of the game's torso/legs/root/head animation via native_body's
// armsOverlay path (body::Targets::armsOnly). Full bodyOn is NOT used — that
// would replace the whole figure.
//
// Per-arm weights with hysteresis: controller motion (relative to the actor
// root, so travel/swing alone is not "active"), fist/shoot, or web-button
// intent raises weight immediately; idle decays after a hold. Tracking loss
// clears the held latch and returns that arm to the game anim.
//
// Thresholds below are PROVISIONAL scaffolding — not headset-validated.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::animated_arms {

// Provisional (unvalidated) motion / blend tuning.
constexpr float speedOn = .35f;      // m/s of grip vs root → active
constexpr float speedOff = .12f;     // m/s → may release after hold
constexpr float holdSeconds = .35f;  // keep active after motion drops
constexpr float blendIn = 12.f;      // weight shares per second (interrupt)
constexpr float blendOut = 6.f;      // return to anim
constexpr float webBoost = 1.f;      // fist/shoot/web button → full weight

struct Arm {
    float weight{};
    float hold{};
    Vec3 lastRel{};
    bool lastRelSet{};
};

struct State {
    Arm arms[2]{};
    Vec3 lastRoot{};
    bool rootSet{};
};

struct Sample {
    Vec3 grip{};       // world grip point
    bool tracked{};
    float fist{};      // 0..1
    float shoot{};     // 0..1
    bool webButton{};  // grip/trigger web intent this frame
};

inline float approach(float now, float want, float rate, float dt) {
    if (!std::isfinite(now) || !std::isfinite(want) || !std::isfinite(rate) || !std::isfinite(dt))
        return want;
    const float step = std::max(0.f, rate) * std::max(0.f, dt);
    if (now < want)
        return std::min(want, now + step);
    return std::max(want, now - step);
}

// Update per-arm overlay weights. root = actor feet / tracking anchor (world).
// Subtracts root travel from grip motion so falling/swinging the body is not
// counted as hand activity.
inline void update(State& s, float dt, bool enabled, Vec3 root, const Sample hands[2]) {
    dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
    if (!enabled) {
        for (auto& a : s.arms)
            a = {};
        s.rootSet = false;
        return;
    }
    Vec3 rootDelta{};
    if (s.rootSet && finite(root) && finite(s.lastRoot))
        rootDelta = root - s.lastRoot;
    if (finite(root)) {
        s.lastRoot = root;
        s.rootSet = true;
    }
    for (int i = 0; i < 2; ++i) {
        auto& a = s.arms[i];
        const auto& h = hands[i];
        if (!h.tracked || !finite(h.grip) || !finite(root)) {
            a.lastRelSet = false;
            a.hold = 0;
            a.weight = approach(a.weight, 0.f, blendOut, dt);
            continue;
        }
        const Vec3 rel = h.grip - root;
        float speed = 0.f;
        if (a.lastRelSet && dt > 1e-4f) {
            // Relative motion already excludes rootDelta if lastRel was
            // previous (grip-root); use grip delta minus root delta equivalently.
            const Vec3 dRel = rel - a.lastRel;
            speed = length(dRel) / dt;
        }
        a.lastRel = rel;
        a.lastRelSet = true;
        const bool intent = h.webButton || h.fist > .2f || h.shoot > .05f;
        const bool moving = speed >= speedOn;
        const bool still = speed <= speedOff;
        if (intent || moving) {
            a.hold = holdSeconds;
        } else if (still) {
            a.hold = std::max(0.f, a.hold - dt);
        }
        const float want = (intent || a.hold > 0.f) ? (intent ? webBoost : 1.f) : 0.f;
        a.weight = approach(a.weight, want, want > a.weight ? blendIn : blendOut, dt);
    }
}

inline bool anyActive(const State& s) {
    return s.arms[0].weight > .001f || s.arms[1].weight > .001f;
}

} // namespace spidy::animated_arms
