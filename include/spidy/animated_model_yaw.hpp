// ANIMATED MODEL YAW (vr_settings::Values::animatedModelYaw, off by default):
// while a pose yield is active, right-stick X asks the *game* to turn the hero
// through the same left-stick walk-facing path InteractStep already uses
// (docs/VALIDATION.md: left stick turns the player matrix row-2 forward;
// InteractStep / DEVELOPMENT.md: 0.15 s left stick toward look turns the hero).
//
// This is NOT an actor-transform / facing RVA write, and it is NOT CameraLook
// view yaw (that turns the follow camera / would fake model turn). HMD free
// look stays on OpenXR tracking; Full head follow is unchanged.
//
// native_movement::Command stays velocity-only. Midair with ANIMATED AIR SPIN
// on: AIR SPIN owns the right stick for 6DOF (RealisticFlip / worldFlip); this
// path stays idle so walk-facing yaw does not fight spherical tilt. Ground /
// wall / perched yields keep MODEL YAW when the setting is on.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::animated_model_yaw {

// Stick X below this is ignored (separate from walk deadzone).
constexpr float deadzone = .20f;
// Peak left-stick tilt used to ask for a turn (VALIDATION: ~0.4 turns hard on
// ground; keep a bit lower as a provisional comfort default).
constexpr float turnTilt = .42f;

struct Stick {
    float right{}, forward{};
};

inline bool active(bool settingOn, bool yielding) { return settingOn && yielding; }

// True when MODEL YAW should drive the pad this frame. airSpinOwnsStick is set
// when ANIMATED AIR SPIN is active midair (see animated_air_spin::ownsRightStick).
inline bool ownsStick(bool settingOn, bool yielding, bool airSpinOwnsStick) {
    return active(settingOn, yielding) && !airSpinOwnsStick;
}

// World direction → walk stick in the pad's axis frame (same convention as
// animated_steering::actorRelativeForward).
inline Stick worldToStick(Vec3 worldDir, Vec3 axisForward, Vec3 axisRight, float magnitude) {
    Stick out;
    if (!finite(worldDir) || !finite(axisForward) || !finite(axisRight) || !std::isfinite(magnitude))
        return out;
    Vec3 want{worldDir.x, 0, worldDir.z};
    Vec3 af{axisForward.x, 0, axisForward.z};
    Vec3 ar{axisRight.x, 0, axisRight.z};
    if (length(want) < .5f || length(af) < .5f || length(ar) < .5f)
        return out;
    want = normalized(want);
    af = normalized(af);
    ar = normalized(ar);
    const float right = dot(want, ar), forward = dot(want, af);
    const float len = std::hypot(right, forward);
    if (len < 1e-4f)
        return out;
    const float scale = std::clamp(magnitude, 0.f, 1.f) / len;
    out.right = right * scale;
    out.forward = forward * scale;
    return out;
}

// HYBRID: compose once in world space, then project for both native run and
// the pad. The turn request changes heading, NEVER the active L-stick speed.
inline Vec3 composeMove(Vec3 walk, float rStickX, Vec3 actorForward, float zone = deadzone,
                        float tilt = turnTilt) {
    if (!finite(walk))
        return {};
    if (!std::isfinite(rStickX) || std::abs(rStickX) <= zone || !finite(actorForward))
        return walk;
    Vec3 ahead{actorForward.x, 0, actorForward.z};
    if (length(ahead) < .5f)
        return walk;
    ahead = normalized(ahead);
    const float response = std::clamp((std::abs(rStickX) - zone) / (1.f - zone), 0.f, 1.f);
    const float sign = std::copysign(1.f, rStickX);
    const Vec3 turn{-ahead.z * sign * tilt * response, 0, ahead.x * sign * tilt * response};
    const float magnitude = length(walk);
    if (magnitude < zone)
        return turn; // idle L: bounded walk-facing request only (not native run)
    const Vec3 mixed = walk * .45f + turn * .55f;
    const float n = length(mixed);
    // Opposing vectors can cancel: retain the player's L heading, never zero/NaN.
    return n <= 1e-4f * magnitude ? walk : mixed * (magnitude / n);
}

// Compose left-stick locomotion with a right-stick model-yaw request.
// walk*: current left stick (already actor-relative if ANIMATED STEERING ran).
// rStickX: raw right-stick X (-1..1). actorForward: game.player matrix row 2.
// Returns the left stick to send the game; caller must also clear Walk::look
// (CameraLook) so the same thumb does not also yaw the follow camera.
inline Stick apply(float walkRight, float walkForward, float rStickX, Vec3 actorForward,
                   Vec3 axisForward, Vec3 axisRight, float zone = deadzone, float tilt = turnTilt) {
    Stick out;
    out.right = std::isfinite(walkRight) ? walkRight : 0.f;
    out.forward = std::isfinite(walkForward) ? walkForward : 0.f;
    if (!std::isfinite(rStickX) || std::abs(rStickX) < zone)
        return out;
    if (!finite(actorForward))
        return out;
    Vec3 ahead{actorForward.x, 0, actorForward.z};
    if (length(ahead) < .5f)
        return out;
    ahead = normalized(ahead);
    // Point the virtual left stick toward a yaw-offset of the actor facing so
    // the game's walk-facing turns him (same family as InteractStep).
    // Actor forward is OpenXR/game ahead (−Z). +R-stick must ask toward the
    // actor's right (+X when facing −Z): +90° yaw. The earlier −stickX match to
    // SpidyVR *view* turn inverted walk-facing on headset (known bug).
    const float yaw = std::copysign(1.5707963f, rStickX);
    const float c = std::cos(yaw), s = std::sin(yaw);
    const Vec3 want{ahead.x * c - ahead.z * s, 0, ahead.x * s + ahead.z * c};
    const float amount = tilt * std::min(1.f, std::abs(rStickX));
    const Stick turn = worldToStick(want, axisForward, axisRight, amount);
    const float walkMag = std::hypot(out.right, out.forward);
    if (walkMag < zone) {
        // Standing turn-in-place request (may still creep slightly — VALIDATION).
        return turn;
    }
    // Keep locomotion magnitude; blend in the turn direction's lateral so the
    // hero faces into the strafe/forward the player is asking for.
    const float tw = .55f; // provisional blend of turn into active walk
    out.right = out.right * (1.f - tw) + turn.right * tw;
    out.forward = out.forward * (1.f - tw) + turn.forward * tw;
    const float mag = std::hypot(out.right, out.forward);
    if (mag > 1.f) {
        out.right /= mag;
        out.forward /= mag;
    }
    return out;
}

} // namespace spidy::animated_model_yaw
