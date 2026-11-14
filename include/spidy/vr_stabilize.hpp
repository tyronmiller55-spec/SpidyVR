// VR STABILIZATION (the user, Nov 13 22:24 playtest: "stronger stabilization and leveling across the board,
// I'm not seeing much effect there with the settings"). CAMERA & STABILIZATION worked only while Native
// Animation carried the view (the animated head, camera_motion.hpp and headset_smoothing.hpp); swinging,
// falling, fighting and wall crawling - SpidyVR's own view - never saw them.
//
// SpidyVR's view is the player's headset on a tracking space the rig places (game_tracking: origin, yaw and
// a TILT: a flip's, the swing's lean, a wall's, a momentum run's bank). Here the same settings act on that
// tilt, and on nothing else:
//  - PITCH, ROLL and HORIZON LEVEL (camera_motion::Amounts) scale the tilt's roll and pitch about the
//  heading,
//    as they scale the animated head's tilt; a tilt past camera_motion::flipStartDegrees (a flip) fades back
//    to the whole tilt by flipFullDegrees, so flips turn fully.
//  - The bank's ANGLE response (headset_smoothing, STABILIZATION's CAMERA/LIGHT/HEAVY/STEADY row) eases the
//    tilt toward what is wanted, with the one-euro speed term, so a quick lean through the swing's catch is
//    calmed and a real turn comes through; it lags at most maxLagDegrees, and a flip is not eased.
// The tilt is turned about the player's head (game_tracking keeps the head where it was), so the headset's
// own look and lean stay 1:1 with no latency at all: nothing here touches the headset's rotation, the yaw
// (the stick's turn and the facing) or the position (where the hero is). On a surface (a wall, a vent) the
// tilt is deliberate: it is eased but never levelled. All amounts at 100% and the angle at 0 is the identity.
// Pure; no Windows.
#pragma once
#include "camera_motion.hpp"
#include "headset_smoothing.hpp"
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::vr_stabilize {
// The tilt never trails what is wanted by more than this.
constexpr float maxLagDegrees = 25.f;
// A tilt that jumps this far in a frame (a recenter, a teleport) is not eased.
constexpr float snapDegrees = 90.f;

struct Settings {
    bool on = false;
    float pitch = 1, roll = 1; // camera_motion amounts, HORIZON LEVEL folded in
    float angleSec = 0, angleBeta = .5f;
    bool levels() const {
        return pitch < .9995f || roll < .9995f;
    }
    bool any() const {
        return on && (levels() || angleSec > 0);
    }
};
// `V` is vr_settings::Values. The old SMOOTHING level is the animated head's alone.
template <class V> inline Settings fromValues(const V& v) {
    Settings s;
    s.on = v.cameraVrStabilization != 0;
    const camera_motion::Amounts a = camera_motion::fromValues(v);
    s.pitch = a.pitch;
    s.roll = a.roll;
    V bank = v;
    bank.animatedHeadsetSmooth = 0;
    const headset_smoothing::Params p = headset_smoothing::fromValues(bank);
    s.angleSec = p.angleSec;
    s.angleBeta = p.angleBeta;
    return s;
}

class TiltFilter {
  public:
    struct Result {
        Quat tilt{};
        bool changed = false;
        float rawDegrees = 0, shownDegrees = 0; // the tilt off upright, wanted and shown
    };
    void reset() {
        ready_ = false;
    }
    // `yaw` the rig's heading, `tilt` the rig's tilt (the heading's frame), `surface` a deliberate surface
    // tilt (never levelled, only eased).
    Result apply(Quat yaw, Quat tilt, float seconds, const Settings& s, bool surface) {
        Result r;
        const Quat wanted = camera_motion::unit(tilt);
        r.tilt = wanted;
        r.rawDegrees = camera_motion::tiltAngle(wanted) / camera_motion::degree;
        r.shownDegrees = r.rawDegrees;
        if (!s.any()) {
            shown_ = wanted;
            ready_ = true;
            return r;
        }
        Quat target = wanted;
        if (!surface && s.levels()) {
            camera_motion::Amounts a;
            a.pitch = std::clamp(s.pitch, 0.f, 1.f);
            a.roll = std::clamp(s.roll, 0.f, 1.f);
            const Quat q = camera_motion::unit(yaw) * wanted;
            target = camera_motion::unit(camera_motion::unit(yaw).conjugate() * camera_motion::tilted(q, a));
        }
        Quat out = target;
        if (s.angleSec > 0 && ready_ && std::isfinite(seconds) && seconds > 0 && seconds <= .25f) {
            Vec3 axis{};
            const float angle = turn(shown_, target, axis);
            if (angle > snapDegrees * camera_motion::degree) {
                out = target;
            } else {
                const float tiltNow = camera_motion::tiltAngle(target);
                const float flip = camera_motion::smoothstep(
                    camera_motion::flipStartDegrees * camera_motion::degree,
                    camera_motion::flipFullDegrees * camera_motion::degree, tiltNow);
                float alpha = share(s.angleSec, s.angleBeta, angle / seconds, seconds);
                alpha = alpha + (1 - alpha) * flip;
                // Never trailing by more than the lag cap.
                const float lag = angle * (1 - alpha);
                const float cap = maxLagDegrees * camera_motion::degree;
                if (lag > cap)
                    alpha = 1.f - cap / angle;
                out = camera_motion::unit(Quat::around(axis, angle * alpha) * shown_);
            }
        }
        shown_ = out;
        ready_ = true;
        r.tilt = out;
        r.changed = true;
        r.shownDegrees = camera_motion::tiltAngle(out) / camera_motion::degree;
        return r;
    }

  private:
    // The turn from `from` to `to`: its angle (0 to pi) and axis.
    static float turn(Quat from, Quat to, Vec3& axis) {
        Quat d = camera_motion::unit(to * from.conjugate());
        if (d.w < 0)
            d = {-d.x, -d.y, -d.z, -d.w};
        axis = {d.x, d.y, d.z};
        const float s = length(axis);
        if (s > 1e-7f)
            axis = axis * (1 / s);
        else
            axis = {0, 1, 0};
        return 2 * std::atan2(s, d.w);
    }
    static float share(float responseSec, float betaPerSpeed, float speed, float seconds) {
        if (!(responseSec > 0))
            return 1.f;
        const float cutoff = 1.f / (6.2831853f * responseSec) + betaPerSpeed * speed;
        return std::clamp(1.f - std::exp(-6.2831853f * cutoff * seconds), 0.f, 1.f);
    }
    Quat shown_{};
    bool ready_{};
};
} // namespace spidy::vr_stabilize
