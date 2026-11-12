// CAMERA MOTION (the user, November 11 22:59: "Native Animation is moving too
// much for players, and leveling steals all of the feeling. We need to let
// the player dial that stuff in"; and 23:02: "Add in a setting for head
// angle, from below to above the horizon, as well").
//
// The camera motion an animation adds while its head carries the view
// (ANIMATED HEAD FULL / LEVEL, and the midair softlock) is, apart from the
// hero's travel itself:
//  - his heading (yaw): where he faces. Kept whole: turning is his.
//  - the head's tilt: PITCH (nodding, looking up and down with the clip) and
//    ROLL (leaning the horizon). HEAD LEVEL removes all of both - and keeps a
//    heading latched per cut - which is why it "steals all of the feeling".
//  - the head's bob relative to the feet: up/down (BOB UP/DOWN) and side and
//    fore-aft (BOB SIDE).
// and the existing CAMERA SMOOTHING (headset_smoothing.hpp) then eases what is
// left over time (response times, not amounts).
//
// Here each is an AMOUNT, percent of the animation's motion passed through:
//  - Tilt: the head's orientation is split about world up into its heading
//    (twist) and its tilt (swing); the tilt's pitch and roll parts are scaled
//    separately and HEAD ANGLE (degrees, + above the horizon) added to the
//    pitch. So 0% PITCH and ROLL is level with a live heading, 100% is the
//    animation exactly, anything between is partial leveling. A tilt past
//    flipStartDegrees (a flip, a hang upside down) fades the scaling out to
//    100% by flipFullDegrees, so a full flip turns smoothly instead of
//    popping where "up" turns over.
//  - Bob: the head's offset from the feet is split into a slow baseline
//    (bobBaselineSeconds: crouch, stand, a vault's rise) and the fast bob
//    about it; the bob is scaled (vertical and horizontal separately). The
//    eyes never move more than maxBobRemoved from the real head (the body or
//    a wall), and the feet - where he is - stay exact.
// It works on the animated head only: the headset's look and lean are put on
// top 1:1, so it never fights the player's head. All 100% and 0 degrees is
// the identity: the view exactly as before. The hands map onto the same
// stabilized head (game_xr: arm_frame's anchor), so they stay where the
// player's hands are. Pure; no Windows.
#pragma once
#include "animated_view.hpp"
#include "math.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>

namespace spidy::camera_motion {
constexpr float pi = 3.14159265f, degree = pi / 180;
constexpr float flipStartDegrees = 100, flipFullDegrees = 170;
constexpr float bobBaselineSeconds = .5f;
// The most the bob scaling may move the eyes from the real head, metres.
constexpr float maxBobRemovedVertical = .15f, maxBobRemovedSide = .2f;
// A jump of the head this far from its baseline is a cut, not a bob.
constexpr float cutMeters = 1.5f;
constexpr int maxHeadAngle = 30;

struct Amounts {
    float pitch = 1, roll = 1, bobVertical = 1, bobSide = 1; // shares 0..1
    float headAngle = 0;                                     // radians, + above the horizon
    bool tiltIdentity() const {
        return pitch >= .9995f && roll >= .9995f && std::abs(headAngle) < 1e-6f;
    }
    bool bobIdentity() const {
        return bobVertical >= .9995f && bobSide >= .9995f;
    }
    bool identity() const {
        return tiltIdentity() && bobIdentity();
    }
};
// vr_settings::Values (generic, so this header stays free of it).
template <class V> inline Amounts fromValues(const V& v) {
    const auto share = [](int pct) { return static_cast<float>(std::clamp(pct, 0, 100)) / 100; };
    Amounts a;
    a.pitch = share(v.cameraPitchPct);
    a.roll = share(v.cameraRollPct);
    a.bobVertical = share(v.cameraBobVerticalPct);
    a.bobSide = share(v.cameraBobSidePct);
    a.headAngle = static_cast<float>(std::clamp(v.cameraHeadAngleDeg, -maxHeadAngle, maxHeadAngle)) * degree;
    return a;
}

// CAMERA MOTION's presets: they fill the four percentages, which stay
// editable; any other mix shows CUSTOM.
struct Preset {
    const char* name;
    int pitch, roll, bobVertical, bobSide;
};
inline constexpr Preset presets[] = {
    {"FULL", 100, 100, 100, 100}, // the animation as it is (default; as before)
    {"LIGHT", 75, 50, 75, 75},
    {"HEAVY", 50, 25, 50, 50},
    {"COMFORT", 25, 0, 25, 25}, // nearly level, a little life left
};
inline constexpr int presetCount = static_cast<int>(std::size(presets));
inline constexpr int customPreset = presetCount; // the row's last choice
template <class V> inline int presetOf(const V& v) {
    for (int i = 0; i < presetCount; ++i)
        if (v.cameraPitchPct == presets[i].pitch && v.cameraRollPct == presets[i].roll &&
            v.cameraBobVerticalPct == presets[i].bobVertical && v.cameraBobSidePct == presets[i].bobSide)
            return i;
    return customPreset;
}
// False for CUSTOM (nothing to fill) or a preset already in force.
template <class V> inline bool applyPreset(V& v, int index) {
    if (index < 0 || index >= presetCount || presetOf(v) == index)
        return false;
    v.cameraPitchPct = presets[index].pitch;
    v.cameraRollPct = presets[index].roll;
    v.cameraBobVerticalPct = presets[index].bobVertical;
    v.cameraBobSidePct = presets[index].bobSide;
    return true;
}

inline float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
inline Quat unit(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!std::isfinite(n) || n < 1e-6f)
        return {};
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
// The heading part of an orientation (-Z forward, +Y up): its turn about world
// up. Upside down exactly it has none: the identity.
inline Quat heading(Quat q) {
    q = unit(q);
    const float n = std::sqrt(q.y * q.y + q.w * q.w);
    if (n < 1e-5f)
        return {};
    return {0, q.y / n, 0, q.w / n};
}
// How far an orientation is tilted off upright, radians (0 to pi).
inline float tiltAngle(Quat q) {
    const Vec3 up = unit(q).rotate({0, 1, 0});
    return std::acos(std::clamp(up.y, -1.f, 1.f));
}
// The orientation with its tilt scaled (pitch and roll apart) and HEAD ANGLE
// added to its pitch; its heading untouched.
inline Quat tilted(Quat q, const Amounts& a) {
    q = unit(q);
    if (a.tiltIdentity())
        return q;
    const Quat twist = heading(q);
    Quat swing = unit(q * twist.conjugate());
    if (swing.w < 0)
        swing = {-swing.x, -swing.y, -swing.z, -swing.w};
    const Vec3 v{swing.x, swing.y, swing.z};
    const float s = length(v);
    const float angle = 2 * std::atan2(s, swing.w);
    const Vec3 rotation = s > 1e-7f ? v * (angle / s) : Vec3{};
    const Vec3 right = twist.rotate({1, 0, 0}), back = twist.rotate({0, 0, 1});
    const float pitch = dot(rotation, right), roll = dot(rotation, back);
    // A flip: the scaling fades out (see the header).
    const float flip = smoothstep(flipStartDegrees * degree, flipFullDegrees * degree, angle);
    const float kp = a.pitch + (1 - std::clamp(a.pitch, 0.f, 1.f)) * flip;
    const float kr = a.roll + (1 - std::clamp(a.roll, 0.f, 1.f)) * flip;
    const Vec3 scaled = right * (pitch * kp + a.headAngle) + back * (roll * kr);
    const float turn = length(scaled);
    const Quat tilt = turn > 1e-7f ? Quat::around(scaled * (1 / turn), turn) : Quat{};
    Quat out = unit(tilt * twist);
    // Where you look stays where the animation looks. Untilting the up line
    // the shortest way turns the forward line a few degrees when the head is
    // both pitched and rolled (3 at 20 and 10), a wobble with every roll.
    // Turn it back about straight up; let go where forward is near vertical,
    // where heading means nothing and the shortest way stays continuous.
    const Vec3 fq = q.rotate({0, 0, -1}), fo = out.rotate({0, 0, -1});
    const float flat = std::min(std::sqrt(fq.x * fq.x + fq.z * fq.z), std::sqrt(fo.x * fo.x + fo.z * fo.z));
    // Upside down the animation's forward reads half a turn round: let go there too.
    const float hold = smoothstep(.3f, .6f, flat) * smoothstep(0.f, .3f, q.rotate({0, 1, 0}).y) * (1 - flip);
    if (hold > 0) {
        float delta = std::atan2(-fq.x, -fq.z) - std::atan2(-fo.x, -fo.z);
        while (delta > 3.14159265f)
            delta -= 2 * 3.14159265f;
        while (delta < -3.14159265f)
            delta += 2 * 3.14159265f;
        out = unit(Quat::yaw(delta * hold) * out);
    }
    return out;
}

// The bob about a slow baseline, scaled. One per thread that shows the head.
class Bob {
    Vec3 base_{};
    bool ready_{};

  public:
    void reset() {
        ready_ = false;
    }
    // `offset` the head's position less the feet's; the offset to use.
    Vec3 update(Vec3 offset, float seconds, const Amounts& a) {
        if (!finite(offset))
            return offset;
        if (a.bobIdentity()) {
            // Kept current, so turning it on later starts from here.
            base_ = offset;
            ready_ = true;
            return offset;
        }
        if (!ready_ || !std::isfinite(seconds) || seconds <= 0 || seconds > .25f ||
            length(offset - base_) > cutMeters) {
            base_ = offset;
            ready_ = true;
            return offset;
        }
        base_ = base_ + (offset - base_) * (1 - std::exp(-seconds / bobBaselineSeconds));
        const Vec3 bob = offset - base_;
        // What is taken away, within reach of the real head.
        float removedY = bob.y * (1 - std::clamp(a.bobVertical, 0.f, 1.f));
        removedY = std::clamp(removedY, -maxBobRemovedVertical, maxBobRemovedVertical);
        Vec3 removedSide{bob.x * (1 - std::clamp(a.bobSide, 0.f, 1.f)), 0,
                         bob.z * (1 - std::clamp(a.bobSide, 0.f, 1.f))};
        if (const float l = length(removedSide); l > maxBobRemovedSide)
            removedSide = removedSide * (maxBobRemovedSide / l);
        return {offset.x - removedSide.x, offset.y - removedY, offset.z - removedSide.z};
    }
};

// A native head matrix (game_fp::frame's rows: right, down, forward, eye)
// stabilized: the tilt always, the bob when a filter and the feet are given.
inline Mat4 stabilize(const Mat4& head, const Amounts& a, Bob* bob = nullptr, const Vec3* feet = nullptr,
                      float seconds = 0) {
    const bool useBob = bob && feet && finite(*feet);
    if (a.tiltIdentity() && (!useBob || a.bobIdentity())) {
        if (useBob)
            bob->update(Vec3{head[12], head[13], head[14]} - *feet, seconds, a);
        return head;
    }
    Pose p = animated_view::fromNative(head);
    if (!finite(p.position))
        return head;
    p.orientation = tilted(p.orientation, a);
    if (useBob)
        p.position = *feet + bob->update(p.position - *feet, seconds, a);
    Mat4 out = animated_view::toNative(p);
    for (float f : out)
        if (!std::isfinite(f))
            return head;
    return out;
}

// The XR worker publishes the amounts in force (the identity while the
// animated head does not carry the view); the render thread reads them where
// it puts the eyes on the head (stereo_probe resolveScene).
inline std::atomic<float> livePitch{1}, liveRoll{1}, liveBobVertical{1}, liveBobSide{1}, liveHeadAngle{0};
inline void publish(const Amounts& a) {
    livePitch.store(a.pitch, std::memory_order_relaxed);
    liveRoll.store(a.roll, std::memory_order_relaxed);
    liveBobVertical.store(a.bobVertical, std::memory_order_relaxed);
    liveBobSide.store(a.bobSide, std::memory_order_relaxed);
    liveHeadAngle.store(a.headAngle, std::memory_order_relaxed);
}
inline Amounts live() {
    Amounts a;
    a.pitch = livePitch.load(std::memory_order_relaxed);
    a.roll = liveRoll.load(std::memory_order_relaxed);
    a.bobVertical = liveBobVertical.load(std::memory_order_relaxed);
    a.bobSide = liveBobSide.load(std::memory_order_relaxed);
    a.headAngle = liveHeadAngle.load(std::memory_order_relaxed);
    return a;
}
} // namespace spidy::camera_motion
