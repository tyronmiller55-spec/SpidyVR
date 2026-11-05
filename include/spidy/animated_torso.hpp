// ANIMATED TORSO (vr_settings::Values::animatedTorso, off by default): while
// ANIMATED ARMS overlays controllers on a pose yield, bound the upper spine
// (waist→chest) and clavicles toward reachable hand targets so extreme midair
// aim can contort the torso. Legs, actor root/feet, and the gameplay head's
// world pose stay owned by the game anim (head orientation is compensated
// after spine turns). Does not replace full-body IK / bodyOn.
//
// Limits below are PROVISIONAL — not headset-validated.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::animated_torso {

// Provisional (unvalidated) spine limits, radians.
constexpr float yawMax = .6981317f;   // ~40°
constexpr float pitchMax = .4363323f; // ~25°
// Soft reach: demand starts when wrist is this far past the arm's length (m).
constexpr float reachSlack = .02f;
// How fast smoothed torso angles chase the demand (rad/s).
constexpr float blendSpeed = 8.f;

struct Demand {
    float yaw{};   // + turn toward the character's left
    float pitch{}; // + lean / reach upward along look
};

inline float clampAngle(float a, float limit) {
    if (!std::isfinite(a) || !std::isfinite(limit) || limit <= 0)
        return 0.f;
    return std::clamp(a, -limit, limit);
}

// Per-arm yaw/pitch of the wrist from the shoulder, in the chest frame
// (forward/left/up). Weight scales the contribution; zero weight ignored.
inline Demand reachDemand(Vec3 /*chest*/, Vec3 up, Vec3 forward, Vec3 left,
                          const Vec3 shoulders[2], const Vec3 wrists[2], const float weights[2],
                          const float armLen[2], float maxYaw = yawMax, float maxPitch = pitchMax) {
    Demand d{};
    float wSum = 0.f;
    if (!finite(up) || !finite(forward) || !finite(left) || length(up) < .5f || length(forward) < .5f ||
        length(left) < .5f)
        return d;
    up = normalized(up);
    forward = normalized(forward);
    left = normalized(left);
    for (int i = 0; i < 2; ++i) {
        const float w = std::isfinite(weights[i]) ? std::clamp(weights[i], 0.f, 1.f) : 0.f;
        if (w <= .001f || !finite(shoulders[i]) || !finite(wrists[i]))
            continue;
        const Vec3 reach = wrists[i] - shoulders[i];
        const float len = length(reach);
        if (len < 1e-4f)
            continue;
        const float soft = std::max(.05f, (std::isfinite(armLen[i]) ? armLen[i] : len) + reachSlack);
        // Demand grows as the wrist sits past a comfortable reach, and from
        // the off-axis angle even inside the sphere (extreme aim directions).
        const float over = std::clamp((len - soft * .75f) / (soft * .35f), 0.f, 1.f);
        const Vec3 dir = reach * (1.f / len);
        const float yaw = std::atan2(dot(dir, left), std::max(1e-4f, dot(dir, forward)));
        const float pitch = std::asin(std::clamp(dot(dir, up), -1.f, 1.f));
        // Oppositional reaches: signed averages cancel pure left/right splits;
        // over-reach still pulls pitch. Weight by activity and over-reach.
        const float use = w * (.35f + .65f * over);
        d.yaw += yaw * use;
        d.pitch += pitch * use;
        wSum += use;
    }
    if (wSum > 1e-4f) {
        d.yaw /= wSum;
        d.pitch /= wSum;
    } else {
        d = {};
    }
    d.yaw = clampAngle(d.yaw, maxYaw);
    d.pitch = clampAngle(d.pitch, maxPitch);
    return d;
}

inline float approach(float now, float want, float rate, float dt) {
    if (!std::isfinite(now) || !std::isfinite(want))
        return 0.f;
    if (!std::isfinite(rate) || !std::isfinite(dt) || rate <= 0 || dt <= 0)
        return want;
    const float step = rate * dt;
    if (now < want)
        return std::min(want, now + step);
    return std::max(want, now - step);
}

// Spine joint share: lower (waist) participates; upper (chest) takes more of
// the twist so shoulders move farther for the same total angle.
inline float spineShare(size_t index, size_t count) {
    if (count == 0)
        return 0.f;
    // Weights 1,2,...,n normalized.
    const float w = static_cast<float>(index + 1);
    const float sum = static_cast<float>(count * (count + 1)) * .5f;
    return w / sum;
}

} // namespace spidy::animated_torso
