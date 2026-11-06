// ANIMATED HYBRID parkour steering (with HYBRID on, while the animation owns the
// body and the view is locked to the hero's head): the left stick is aimed
// relative to what the player actually sees — the head-locked view — then put
// into the game camera's walk axes, so the game's own camera-relative
// locomotion turns the hero toward the push the way the vanilla game does.
// ANIMATED STEERING (actor-relative, which throws the stick's direction away)
// is bypassed here; it stays for non-HYBRID play. Pure — no Windows.
#pragma once
#include "spidy/animated_model_yaw.hpp"
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::hybrid_steer {

// Stick tilt below this is released (same floor as ANIMATED STEERING).
constexpr float deadzone = .15f;
// View-yaw low-pass, seconds: holding a side arcs instead of spinning with the
// view that turns with the hero.
constexpr float viewTau = .3f;

using Stick = animated_model_yaw::Stick;

// View-relative steering is on: HYBRID, the animation owns, and the view is
// head-locked (HEAD not Off, not the flat screen).
inline bool usesView(bool hybridOn, bool animOwns, bool headLocked) {
    return hybridOn && animOwns && headLocked;
}

// ANIMATED STEERING applies only when view steering does not.
inline bool steeringApplies(bool steeringOn, bool yielding, bool viewSteer) {
    return steeringOn && yielding && !viewSteer;
}

// While MODEL YAW owns the R-stick under view steering: free CameraLook only on
// frames where the stick actually turns; otherwise the game camera follows the
// view (its parkour assists see a sensible camera).
inline bool clearsLook(bool modelYawOwns, float rStickX, float zone = animated_model_yaw::deadzone) {
    return modelYawOwns && std::isfinite(rStickX) && std::abs(rStickX) >= zone;
}

// Level forward / right of a view yaw (0 = facing -Z, as hybrid_handoff::lookYaw).
inline Vec3 viewForward(float viewYaw) {
    return Quat::yaw(viewYaw).rotate({0, 0, -1});
}
inline Vec3 viewRight(float viewYaw) {
    return Quat::yaw(viewYaw).rotate({1, 0, 0});
}

// Stick (x right, y forward) relative to the view yaw → walk stick in the game
// camera's axes (axisForward / axisRight: walkForwardAxis / walkRightAxis), same
// convention as animated_model_yaw::worldToStick. magnitude < 0: the stick's own
// tilt (capped at 1); otherwise that magnitude (e.g. the vanilla walk's, which
// already has the run curve applied).
inline Stick viewRelativeStick(float stickX, float stickY, float viewYaw, Vec3 axisForward, Vec3 axisRight,
                               float magnitude = -1.f, float zone = deadzone) {
    Stick out;
    if (!std::isfinite(stickX) || !std::isfinite(stickY) || !std::isfinite(viewYaw))
        return out;
    const float tilt = std::hypot(stickX, stickY);
    if (tilt < zone)
        return out;
    const float amount = magnitude < 0.f ? std::min(tilt, 1.f) : std::clamp(magnitude, 0.f, 1.f);
    const Vec3 want = viewRight(viewYaw) * stickX + viewForward(viewYaw) * stickY;
    return animated_model_yaw::worldToStick(want, axisForward, axisRight, amount);
}

// Yaw the head-locked view shows: the hero's head yaw plus the headset's turn
// since the view latched onto it (tracked yaw − reference yaw) plus the R-stick
// view turn (Level), all radians, wrapped to ±π.
inline float viewYaw(float headYaw, float trackedYaw, float refYaw, float headTurn) {
    return std::remainder(headYaw + (trackedYaw - refYaw) + headTurn, 6.2831853f);
}

// First-order low-pass on an angle (wraps through ±π).
struct YawFilter {
    float yaw{};
    bool set{};
    float update(float target, float dt, float tau = viewTau) {
        if (!std::isfinite(target))
            return yaw;
        if (!set || !std::isfinite(dt) || tau <= 0.f) {
            yaw = target;
            set = true;
            return yaw;
        }
        const float a = std::clamp(dt / tau, 0.f, 1.f);
        yaw = std::remainder(yaw + std::remainder(target - yaw, 6.2831853f) * a, 6.2831853f);
        return yaw;
    }
    void reset() {
        set = false;
    }
};

} // namespace spidy::hybrid_steer
