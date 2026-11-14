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
#include <cstdint>

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

inline bool ownsYaw(bool animLocked, bool modelYaw, bool crawling) {
    return animLocked && modelYaw && !crawling;
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

// Raw view-relative movement in world space. Shared by the ground-run input
// and pad heading; the pad may separately retain its existing run-curve magnitude.
inline Vec3 viewMove(float x, float y, float yaw, float zone = deadzone) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(yaw))
        return {};
    const float n = std::hypot(x, y);
    if (n < zone)
        return {};
    return (viewRight(yaw) * x + viewForward(yaw) * y) / std::max(n, 1.f);
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
    return animated_model_yaw::worldToStick(normalized(want), axisForward, axisRight, amount);
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

// After a SpidyVR ↔ animation handoff: CameraLook must not turn the walk
// camera under a held run (that orbits until a jump). Hold the look stick
// off through the blend, then until the left stick is released.
inline bool suppressLook(bool recheckMove, bool blendHold, float stickTilt, float zone = deadzone) {
    if (!recheckMove)
        return false;
    if (blendHold)
        return true;
    return std::isfinite(stickTilt) && stickTilt >= zone;
}

// The handoff is settled: the eye blend is done and the run stick is released.
inline bool doneRecheck(bool blendHold, float stickTilt, float zone = deadzone) {
    return !blendHold && (!std::isfinite(stickTilt) || stickTilt < zone);
}

// HEADSET FIRST (the polish pass, November 14; the user, t64u: "Pressing the stick diagonally left doesn't
// always do what you expect. Sometimes Spiderman starts running another direction because the model was in
// the process of turning ... Where the player looks takes full precedence"): the left stick's world BASIS.
// Resting, it is the view the player looks along, live. At the press it latches that view; while held it
// follows only what the PLAYER does - the headset's own turn since the last frame and the right stick's turn -
// never the hero's facing, the heading the body is brought onto, or a turn the animation made. An action edge
// (resync) takes the view again (it is the player's heading by then: slice "the view is yours").
// Yaw is the rig's convention (0 facing -z, + to the left), radians.
struct Basis {
    bool held = false;
    float yaw = 0, lastHmd = 0;
    bool set = false;
    uint32_t presses = 0, resyncs = 0;
    // `view`: where the player looks now (shown + hmd; NAN: unknown, the basis keeps its last value).
    // `hmd`: the headset's own turn off its latch. `playerTurn`: the right stick's turn this frame.
    float update(bool pressed, float view, float hmd, float playerTurn, bool resync = false) {
        const bool viewOk = std::isfinite(view);
        const bool hmdOk = std::isfinite(hmd);
        const float turn = std::isfinite(playerTurn) ? playerTurn : 0.f;
        if (!pressed) {
            if (viewOk) {
                yaw = std::remainder(view, 6.2831853f);
                set = true;
            }
            held = false;
        } else if (!held) {
            // The press: the view as it is now.
            if (viewOk) {
                yaw = std::remainder(view, 6.2831853f);
                set = true;
            }
            held = true;
            ++presses;
        } else if (resync && viewOk) {
            yaw = std::remainder(view, 6.2831853f);
            ++resyncs;
        } else if (set) {
            const float head = hmdOk ? std::remainder(hmd - lastHmd, 6.2831853f) : 0.f;
            yaw = std::remainder(yaw + head + turn, 6.2831853f);
        }
        if (hmdOk)
            lastHmd = hmd;
        return yaw;
    }
    void reset() {
        *this = Basis{};
    }
};

} // namespace spidy::hybrid_steer
