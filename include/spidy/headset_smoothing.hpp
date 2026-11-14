#pragma once
#include "math.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace spidy::headset_smoothing {
// Native Animation SMOOTHING: calm wobble on the animated head the view sits on.
// The headset's look and lean are left alone (no look delta). The head's travel
// (position) is copied through so the camera cannot trail through the body or a
// wall. Level 0 (Off) is identity.
//
// Idle jitter uses the response times below as a low cutoff. Faster motion
// raises the cutoff (one-euro style) so a vault, jump or real turn still comes
// through. A snap, recenter or cut jumps to the target.
constexpr float responseSec[] = {0.f, .05f, .10f, .18f, .30f};
constexpr float beta[] = {0.f, .55f, .50f, .45f, .40f};
constexpr int maxLevel = 4;
constexpr float snapRad = 45.f * 3.14159265f / 180.f;
// A teleport or a new shot this far away skips the ease, as spectator smoothing does.
constexpr float cutMeters = 3.f;
// The XR worker sets this each frame: the user's SMOOTHING level while Native
// Animation head-follow owns the view, otherwise 0. The render thread reads it
// in resolveScene so the eyes sit on the filtered parent.
inline std::atomic<int> liveLevel{0};

// CAMERA SMOOTHING and STABILIZATION (the user, November 10 20:54; vr_settings
// cameraStabilization and the CAMERA / LIGHT / HEAVY rows). The same filter
// with three response times instead of one level:
//  - VERTICAL and HORIZONTAL smooth the head's travel RELATIVE TO THE FEET
//    (`root`: the hero's position), so the animation's bob and sway calm down
//    while where the hero is - the feet, the position lock - stays exact and
//    a run or a swing never trails. The smoothed offset never lags more than
//    maxVerticalLag / maxHorizontalLag, so the eyes cannot sink through the
//    body or a wall. Without a root the travel is copied through.
//  - ANGLE smooths the head's turning, as SMOOTHING always did.
// Only the animated head the view sits on is filtered: the headset's own look
// and lean are applied on top of it 1:1, so smoothing never fights the
// player's head (no added rotation latency, no sickness). A snap, a cut, a
// teleport or a recenter jumps straight to the target.
struct Params {
    float verticalSec{}, horizontalSec{}, angleSec{};
    float angleBeta = .5f;    // per radian/second of turn (one-euro speed term)
    float snapRad = 45.f * 3.14159265f / 180.f; // a turn this far in one step is a cut, not eased
    float positionBeta = 1.f; // per metre/second of the offset's change
    bool any() const {
        return verticalSec > 0 || horizontalSec > 0 || angleSec > 0;
    }
};
// November 14 (STEADY): raised from .25 and .35 so the longer response times can hold the bob back.
constexpr float maxVerticalLag = .4f, maxHorizontalLag = .5f;
// Speed damping per bank: OFF's CAMERA rows, LIGHT, HEAVY (heavier holds on
// through faster motion before letting it through).
// STEADY (the fourth bank, November 14) holds on longest.
constexpr float bankAngleBeta[] = {.5f, .45f, .25f, .12f}, bankPositionBeta[] = {1.f, .6f, .25f, .12f};
constexpr int bankCount = 4;
constexpr int maxResponseMs = 1000;
// The old SMOOTHING level alone: angle only, as before.
inline Params fromLevel(int level) {
    level = std::clamp(level, 0, maxLevel);
    Params p;
    p.angleSec = responseSec[level];
    p.angleBeta = beta[level];
    return p;
}
// The settings in force: STABILIZATION's bank (0 the CAMERA rows, 1 LIGHT,
// 2 HEAVY), milliseconds. The SMOOTHING level stays a floor on the angle.
// `V` is vr_settings::Values (kept generic so this header stays pure).
template <class V>
inline Params fromValues(const V& v) {
    const int bank = std::clamp(v.cameraStabilization, 0, bankCount - 1);
    const int ms[bankCount][3] = {{v.cameraVerticalMs, v.cameraHorizontalMs, v.cameraAngleMs},
                                  {v.lightVerticalMs, v.lightHorizontalMs, v.lightAngleMs},
                                  {v.heavyVerticalMs, v.heavyHorizontalMs, v.heavyAngleMs},
                                  {v.steadyVerticalMs, v.steadyHorizontalMs, v.steadyAngleMs}};
    const auto sec = [](int m) { return static_cast<float>(std::clamp(m, 0, maxResponseMs)) * .001f; };
    Params p;
    p.verticalSec = sec(ms[bank][0]);
    p.horizontalSec = sec(ms[bank][1]);
    p.angleSec = sec(ms[bank][2]);
    p.angleBeta = bankAngleBeta[bank];
    p.positionBeta = bankPositionBeta[bank];
    const Params level = fromLevel(v.animatedHeadsetSmooth);
    if (level.angleSec > p.angleSec) {
        p.angleSec = level.angleSec;
        p.angleBeta = level.angleBeta;
    }
    return p;
}
// ANIM HEAD RIDE (camera_mode.hpp): while the view rides the animation's head its yaw is the bone's, so
// the angle holds on harder (a quarter of the speed term, a cut only past 90 degrees). The XR worker sets
// this; the render thread reads it with the params.
inline std::atomic<bool> liveRide{false};
inline Params withRide(Params p) {
    p.angleBeta *= .4f;
    p.snapRad = 90.f * 3.14159265f / 180.f;
    return p;
}
// The XR worker publishes the params in force (zero: none) beside liveLevel;
// the render thread reads them in resolveScene.
inline std::atomic<float> liveVertical{0}, liveHorizontal{0}, liveAngle{0}, liveAngleBeta{.5f},
    livePositionBeta{1.f};
inline void publish(const Params& p) {
    liveVertical.store(p.verticalSec, std::memory_order_relaxed);
    liveHorizontal.store(p.horizontalSec, std::memory_order_relaxed);
    liveAngle.store(p.angleSec, std::memory_order_relaxed);
    liveAngleBeta.store(p.angleBeta, std::memory_order_relaxed);
    livePositionBeta.store(p.positionBeta, std::memory_order_relaxed);
}
inline Params live() {
    Params p;
    p.verticalSec = liveVertical.load(std::memory_order_relaxed);
    p.horizontalSec = liveHorizontal.load(std::memory_order_relaxed);
    p.angleSec = liveAngle.load(std::memory_order_relaxed);
    p.angleBeta = liveAngleBeta.load(std::memory_order_relaxed);
    p.positionBeta = livePositionBeta.load(std::memory_order_relaxed);
    return p;
}

class Filter {
    Mat4 previous_{};
    bool ready_{};
    Vec3 offset_{}, root_{}, want_{}; // want_: the last raw offset (its rate is the speed)
    bool offsetReady_{};

    static float turn(const Mat4& from, const Mat4& to, Vec3& axis) {
        const Vec3 oldX{from[0], from[1], from[2]}, oldZ{from[8], from[9], from[10]};
        const Vec3 nextX{to[0], to[1], to[2]}, nextZ{to[8], to[9], to[10]};
        Quat delta = arc(oldZ, nextZ);
        const Vec3 alignedX = delta.rotate(oldX);
        const float twist = std::atan2(dot(nextZ, cross(alignedX, nextX)), dot(alignedX, nextX));
        delta = Quat::around(nextZ, twist) * delta;
        if (delta.w < 0)
            delta = {-delta.x, -delta.y, -delta.z, -delta.w};
        axis = {delta.x, delta.y, delta.z};
        return 2 * std::atan2(length(axis), std::max(0.f, delta.w));
    }
    static Mat4 withPosition(Mat4 m, const Mat4& pos) {
        m[12] = pos[12];
        m[13] = pos[13];
        m[14] = pos[14];
        m[15] = pos[15];
        return m;
    }
    static Vec3 positionOf(const Mat4& m) {
        return {m[12], m[13], m[14]};
    }
    static float share(float responseSec, float betaPerSpeed, float speed, float seconds) {
        if (!(responseSec > 0))
            return 1.f;
        const float cutoff = 1.f / (6.2831853f * responseSec) + betaPerSpeed * speed;
        return std::clamp(1.f - std::exp(-6.2831853f * cutoff * seconds), 0.f, 1.f);
    }
    bool positionOn(const Params& p, const Vec3* root) const {
        return root && finite(*root) && (p.verticalSec > 0 || p.horizontalSec > 0);
    }
    void restart(const Mat4& target, const Vec3* root) {
        previous_ = target;
        ready_ = true;
        offsetReady_ = root && finite(*root);
        if (offsetReady_) {
            root_ = *root;
            offset_ = want_ = positionOf(target) - *root;
        }
    }
    // The travel: the feet exact, the head's offset from them smoothed.
    void placePosition(Mat4& out, const Mat4& target, float seconds, const Params& p, const Vec3* root) {
        if (!positionOn(p, root)) {
            out = withPosition(out, target);
            offsetReady_ = false;
            return;
        }
        const Vec3 want = positionOf(target) - *root;
        if (!offsetReady_ || !finite(offset_)) {
            offset_ = want_ = want;
            offsetReady_ = true;
        } else if (seconds > 0) {
            Vec3 d = want - offset_;
            // The speed term of the one-euro filter is the raw offset's own rate.
            const Vec3 rate = (want - want_) / seconds;
            want_ = want;
            // Vertical: one axis.
            const float vShare = share(p.verticalSec, p.positionBeta, std::abs(rate.y), seconds);
            offset_.y += d.y * vShare;
            const float lagV = want.y - offset_.y;
            if (std::abs(lagV) > maxVerticalLag)
                offset_.y = want.y - std::copysign(maxVerticalLag, lagV);
            // Horizontal: the plane, as one vector (no axis preference).
            const Vec3 dh{d.x, 0, d.z};
            const float hShare =
                share(p.horizontalSec, p.positionBeta, length(Vec3{rate.x, 0, rate.z}), seconds);
            offset_.x += dh.x * hShare;
            offset_.z += dh.z * hShare;
            const Vec3 lagH{want.x - offset_.x, 0, want.z - offset_.z};
            const float lagHLen = length(lagH);
            if (lagHLen > maxHorizontalLag) {
                const Vec3 keep = lagH * (maxHorizontalLag / lagHLen);
                offset_.x = want.x - keep.x;
                offset_.z = want.z - keep.z;
            }
        }
        root_ = *root;
        const Vec3 at = *root + offset_;
        out[12] = at.x;
        out[13] = at.y;
        out[14] = at.z;
        out[15] = target[15];
    }

  public:
    void reset() {
        ready_ = false;
        offsetReady_ = false;
    }
    Mat4 update(Mat4 target, float seconds, int level) {
        level = std::clamp(level, 0, maxLevel);
        if (level == 0)
            return update(target, seconds, Params{}, nullptr);
        return update(target, seconds, fromLevel(level), nullptr);
    }
    // `root` the hero's position (the feet) in the same world, or null: then
    // the travel is copied through as before.
    Mat4 update(Mat4 target, float seconds, const Params& p, const Vec3* root) {
        if (!p.any() || !std::isfinite(seconds) || seconds < 0 || seconds > .25f) {
            previous_ = target;
            ready_ = p.any();
            offsetReady_ = false;
            if (ready_ && root && finite(*root)) {
                offset_ = want_ = positionOf(target) - *root;
                root_ = *root;
                offsetReady_ = true;
            }
            return target;
        }
        if (!ready_) {
            restart(target, root);
            return target;
        }
        const Vec3 jump = positionOf(target) - positionOf(previous_);
        const bool rootJump = root && finite(*root) && offsetReady_ && length(*root - root_) > cutMeters;
        if ((!positionOn(p, root) && length(jump) > cutMeters) || rootJump ||
            (positionOn(p, root) && offsetReady_ &&
             length((positionOf(target) - *root) - offset_) > cutMeters)) {
            restart(target, root);
            return target;
        }
        if (seconds == 0) {
            Mat4 out = previous_;
            placePosition(out, target, 0, p, root);
            return previous_ = out;
        }
        Vec3 axis{};
        const float angle = turn(previous_, target, axis);
        if (angle > p.snapRad) {
            restart(target, root);
            return target;
        }
        Mat4 out = target;
        if (p.angleSec > 0) {
            const float alpha = share(p.angleSec, p.angleBeta, angle / seconds, seconds);
            const Quat step = Quat::around(axis, angle * alpha);
            out = previous_;
            for (int row = 0; row < 3; ++row) {
                const int at = row * 4;
                const Vec3 v = step.rotate({previous_[at], previous_[at + 1], previous_[at + 2]});
                out[at] = v.x;
                out[at + 1] = v.y;
                out[at + 2] = v.z;
            }
        }
        placePosition(out, target, seconds, p, root);
        previous_ = out;
        return out;
    }
};
} // namespace spidy::headset_smoothing
