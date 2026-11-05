// ANIMATED STEERING (vr_settings::Values::animatedSteering, off by default):
// while ANIMATED MOVES / ANIMATED AIR yields the body pose to the game, remap
// the left stick so its magnitude runs the hero along his current facing
// (read from the actor transform), instead of camera-relative every way.
//
// Right-stick whole-model yaw is NOT a facing RVA write. When the separate
// ANIMATED MODEL YAW setting is on, yaw is requested through the game's
// left-stick walk-facing path (animated_model_yaw.hpp; VALIDATION /
// InteractStep). CameraLook view yaw must not be used as a substitute.
// Midair spherical aiming remains out of scope.
//
// Outside pose yield the sticks are untouched (no double-apply with normal
// camera-relative walk).
#pragma once
#include "spidy/math.hpp"
#include <cmath>

namespace spidy::animated_steering {

// Stick tilt below this is treated as released (matches common pad floors).
constexpr float deadzone = .15f;

struct Stick {
    float right{}, forward{};
};

inline bool active(bool settingOn, bool yielding) { return settingOn && yielding; }

// camera-relative stick → stick that asks the game to move along actorForward.
// actorForward / axisForward / axisRight: world vectors (axis* = the same
// walkForwardAxis / walkRightAxis the pad already uses). Horizontalised.
inline Stick actorRelativeForward(float stickRight, float stickForward, Vec3 actorForward,
                                  Vec3 axisForward, Vec3 axisRight, float zone = deadzone) {
    Stick out;
    if (!std::isfinite(stickRight) || !std::isfinite(stickForward))
        return out;
    const float mag = std::hypot(stickRight, stickForward);
    if (mag < zone)
        return out;
    if (!finite(actorForward) || !finite(axisForward) || !finite(axisRight))
        return out;
    Vec3 ahead{actorForward.x, 0, actorForward.z};
    Vec3 af{axisForward.x, 0, axisForward.z};
    Vec3 ar{axisRight.x, 0, axisRight.z};
    if (length(ahead) < .5f || length(af) < .5f || length(ar) < .5f)
        return out;
    ahead = normalized(ahead);
    af = normalized(af);
    ar = normalized(ar);
    const float right = dot(ahead, ar), forward = dot(ahead, af);
    const float len = std::hypot(right, forward);
    if (len < 1e-4f)
        return out;
    const float scale = std::min(mag, 1.f) / len;
    out.right = right * scale;
    out.forward = forward * scale;
    return out;
}

} // namespace spidy::animated_steering
