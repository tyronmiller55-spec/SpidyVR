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

// POSITION LOCK (November 8): the one authoritative place is the game actor's
// feet (the physics body both owners move). Every ANIM -> VR switch puts the
// tracking origin over those feet - the tracked head's floor point exactly on
// them, the yaw continuing the look the player had - instead of under the
// animated head-bone view, whose offset from the feet (a lean, a roll, root
// motion: 0.7 m median, up to 2 m in the 19:16 session) adoptEyesOrigin used
// to bake into the whole VR stint. The 180 ms eye blend covers the change.
inline Vec3 lockedOrigin(Vec3 feet, float rigYaw, Vec3 trackedHead) {
    if (!finite(feet) || !finite(trackedHead) || !std::isfinite(rigYaw))
        return {};
    return feet - Quat::yaw(rigYaw).rotate({trackedHead.x, 0, trackedHead.z});
}
// The tracked head's floor point minus the feet, across the ground: what the
// lock leaves (0 at the switch; later physical steps/leans by the player).
inline float residualAcross(Vec3 origin, float rigYaw, Vec3 trackedHead, Vec3 feet) {
    if (!finite(origin) || !finite(trackedHead) || !finite(feet) || !std::isfinite(rigYaw))
        return -1.f;
    const Vec3 d = origin + Quat::yaw(rigYaw).rotate({trackedHead.x, 0, trackedHead.z}) - feet;
    return std::hypot(d.x, d.z);
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

// Blend endpoints. The source is the view the headset last showed (both eyes in
// the world, whichever owner placed them: tracking, the head lock, or a blend
// still under way), so an edge - even one during another blend - starts exactly
// where the player was looking. It travels with the player from the frame it was
// shown (native_eyes::Command::anchor), so at swing speed the view does not lag
// behind the hero and catch up. The destination is what the new owner will show
// once the blend is over. Same 180 ms smoothstep; nothing lengthened.
struct Shown {
    Pose eyes[2]{};
    Vec3 anchor{}; // the player position the eyes were placed from
    int64_t ns{};  // its frame's display time
    bool valid{};
};
// A shown view older than this (a menu, a scene, a pause between) is no source.
constexpr int64_t shownMaxAgeNs = 100'000'000;

// native_eyes::reanchorOffset's teleport limit: farther is a respawn or a bad read.
constexpr float carryLimit = 20.f;

// The player's travel from `from` to `now`; none for a teleport or a bad read.
inline Vec3 travel(Vec3 from, Vec3 now) {
    if (!finite(from) || !finite(now))
        return {};
    const Vec3 d = now - from;
    return length(d) <= carryLimit ? d : Vec3{};
}

inline bool validShown(const Shown& s, int64_t nowNs) {
    if (!s.valid || !finite(s.anchor) || nowNs < s.ns || nowNs - s.ns > shownMaxAgeNs)
        return false;
    for (const auto& e : s.eyes) {
        const Quat& q = e.orientation;
        const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (!finite(e.position) || !std::isfinite(n) || std::abs(n - 1.f) > .01f)
            return false;
    }
    return true;
}

inline Vec3 shownCentre(const Shown& s) {
    return (s.eyes[0].position + s.eyes[1].position) * .5f;
}

// The shown view's look yaw (tracking-rig convention), false when it looks too
// nearly straight up or down to have one.
inline bool shownYaw(const Shown& s, float& yaw) {
    const Vec3 f = s.eyes[0].orientation.rotate({0, 0, -1}) + s.eyes[1].orientation.rotate({0, 0, -1});
    if (!finite(f) || std::hypot(f.x, f.z) < .4f) // under ~78 degrees of pitch (both eyes summed)
        return false;
    yaw = lookYaw(f);
    return true;
}

// One eye of the blend: the shown eye carried by the player's travel since it was
// shown, toward the new owner's eye. a = blendAlpha: 0 is exactly the source, 1
// exactly the destination.
inline Pose blendEye(Pose from, Vec3 carry, Pose to, float a) {
    return {lerpVec(from.position + carry, to.position, a), slerpQuat(from.orientation, to.orientation, a)};
}

} // namespace spidy::hybrid_handoff
