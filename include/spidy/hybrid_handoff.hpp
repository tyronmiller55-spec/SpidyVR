// ANIMATED HYBRID handoff helpers (Slices 2–3): re-anchor / blend math used when
// ownership moves between animated ground (G) and OG SpidyVR traversal (T).
// Pure — no Windows. Tunable window matches body_ik blendSpeed≈4 (~250 ms
// full ramp); 180 ms is the eye/yaw crossfade the plan calls for.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hybrid_handoff {

// Eye + yaw blend window (nanoseconds / seconds).
constexpr int64_t handoffNs = 180'000'000;
constexpr float handoffSec = .18f;

// 0 at the edge → 1 after handoffNs. Smoothstep so the first/last frames ease.
inline float blendAlpha(int64_t elapsedNs) {
    if (elapsedNs <= 0)
        return 0.f;
    if (elapsedNs >= handoffNs)
        return 1.f;
    const float t = static_cast<float>(elapsedNs) / static_cast<float>(handoffNs);
    return t * t * (3.f - 2.f * t);
}

inline bool blendDone(int64_t elapsedNs) { return elapsedNs >= handoffNs; }

// World look yaw from a level forward (0 = facing −Z, same as the tracking rig).
inline float lookYaw(Vec3 forward) {
    if (!finite(forward))
        return 0.f;
    if (std::hypot(forward.x, forward.z) < .05f)
        return 0.f;
    return std::atan2(-forward.x, -forward.z);
}

// Actor / player-matrix row-2 forward → the same look-yaw the tracking rig uses
// (so MODEL YAW carry into rig_.yaw does not pick up a π flip).
inline float actorLookYaw(Vec3 actorForward) {
    return lookYaw(actorForward);
}

inline float wrapPi(float a) {
    return std::remainder(a, 6.2831853f);
}

inline float lerpAngle(float from, float to, float a) {
    a = std::clamp(a, 0.f, 1.f);
    return from + wrapPi(to - from) * a;
}

inline Vec3 lerpVec(Vec3 a, Vec3 b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return a + (b - a) * t;
}

// Origin that places tracking-owned eyes on `worldEyes` at `rigYaw`, keeping
// the current stand-off / head-drop / run-view offsets (no rested snap).
// `trackedHead` is the OpenXR head position in tracking space.
// Pure form of GameTrackingRig::adoptEyes — unit-tested here.
inline Vec3 adoptEyesOrigin(Vec3 worldEyes, float /*worldLookYaw*/, float rigYaw, Vec3 trackedHead,
                            Vec3 standOff, Vec3 headDrop, Vec3 runViewOffset = {}) {
    if (!finite(worldEyes) || !finite(trackedHead) || !std::isfinite(rigYaw))
        return {};
    if (!finite(standOff))
        standOff = {};
    if (!finite(headDrop))
        headDrop = {};
    if (!finite(runViewOffset))
        runViewOffset = {};
    // placed.origin = rig.origin + standOff + headDrop (+ runViewOffset later).
    // placed.toWorld(head).position ≈ origin_placed + yaw.rotate(trackedHead)
    // (tilt discarded — level settle is the blend's job).
    return worldEyes - standOff - headDrop - runViewOffset - Quat::yaw(rigYaw).rotate(trackedHead);
}

// Rig yaw so a tracked head looking `trackedLook` faces `worldLookYaw` in world.
inline float adoptEyesRigYaw(float worldLookYaw, Vec3 trackedLook) {
    if (!std::isfinite(worldLookYaw))
        return 0.f;
    const float tracked = lookYaw(trackedLook);
    return wrapPi(worldLookYaw - tracked);
}

// Predicted world eye position after adoptEyesOrigin (for the unit test).
inline Vec3 predictedEyes(Vec3 origin, float rigYaw, Vec3 trackedHead, Vec3 standOff, Vec3 headDrop,
                          Vec3 runViewOffset = {}) {
    const Vec3 placed = origin + standOff + headDrop + runViewOffset;
    return placed + Quat::yaw(rigYaw).rotate(trackedHead);
}


// Tiny Quat slerp (same convention as body_ik / web_aim) for eye-mat blends.
inline Quat slerpQuat(Quat a, Quat b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    Quat b2 = b;
    if (dot < 0.f) {
        b2 = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    if (dot > .9995f) {
        Quat r{a.x + (b2.x - a.x) * t, a.y + (b2.y - a.y) * t, a.z + (b2.z - a.z) * t,
               a.w + (b2.w - a.w) * t};
        const float n = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
        return n > 1e-8f ? Quat{r.x / n, r.y / n, r.z / n, r.w / n} : a;
    }
    const float theta = std::acos(std::clamp(dot, -1.f, 1.f));
    const float s = std::sin(theta);
    const float w1 = std::sin((1.f - t) * theta) / s;
    const float w2 = std::sin(t * theta) / s;
    return {a.x * w1 + b2.x * w2, a.y * w1 + b2.y * w2, a.z * w1 + b2.z * w2, a.w * w1 + b2.w * w2};
}

// Same layout as game_tracking::worldPose (right, down, forward−Z, translation).
inline Mat4 eyeMatFromPose(Vec3 pos, Quat q) {
    const Vec3 x = q.rotate({1, 0, 0}), y = q.rotate({0, -1, 0}), z = q.rotate({0, 0, -1});
    return {x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, pos.x, pos.y, pos.z, 1};
}

inline Mat4 blendEyeMat(Vec3 fromPos, Quat fromOri, Vec3 toPos, Quat toOri, float a) {
    return eyeMatFromPose(lerpVec(fromPos, toPos, a), slerpQuat(fromOri, toOri, a));
}

} // namespace spidy::hybrid_handoff
