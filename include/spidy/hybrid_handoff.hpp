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

// ONE HANDOFF BLEND (handoff_one_blend): the same smoothstep over `durationNs` (handoffNs when not positive).
inline float blendAlphaFor(int64_t elapsedNs, int64_t durationNs) {
    const int64_t d = durationNs > 0 ? durationNs : handoffNs;
    if (elapsedNs <= 0)
        return 0.f;
    if (elapsedNs >= d)
        return 1.f;
    const float t = static_cast<float>(elapsedNs) / static_cast<float>(d);
    return t * t * (3.f - 2.f * t);
}
inline bool blendDoneFor(int64_t elapsedNs, int64_t durationNs) {
    return elapsedNs >= (durationNs > 0 ? durationNs : handoffNs);
}
// Its length: AIR VIEW EASE's (150-400 ms) with the setting on, else handoffNs (also when AIR VIEW EASE is off).
inline int64_t oneBlendNs(bool on, int airViewEaseMs) {
    if (!on || airViewEaseMs <= 0)
        return handoffNs;
    return static_cast<int64_t>(std::clamp(airViewEaseMs, 150, 400)) * 1'000'000;
}

// HANDOFF GLIDE SPEED (handoff_glide_cm_s, handoff_glide_max_ms): a glide's length so the camera's peak speed stays
// under `speedCmS` - `peak` is the curve's peak over its average speed (1.5 smoothstep, 1.875 smootherstep, 3 the
// cubic ease-out). Never shorter than `baseMs`, never longer than `maxMs` (or baseMs, if larger). speedCmS <= 0: baseMs.
inline int glideMs(float metres, int baseMs, int speedCmS, int maxMs, float peak) {
    if (speedCmS <= 0 || !std::isfinite(metres) || !(metres > 0) || !std::isfinite(peak) || peak <= 0)
        return baseMs;
    const float need = peak * metres / (static_cast<float>(speedCmS) * .01f) * 1000.f;
    const int top = (std::max)(baseMs, maxMs);
    return std::clamp(static_cast<int>(std::ceil(need)), baseMs, top);
}
inline constexpr float peakSmoothstep = 1.5f, peakSmootherstep = 1.875f, peakEaseOut = 3.f;

// HANDOFF PLACE (handoff_place_cm; the owner, Nov 17): what a landing's blend onto the head does with the distance the
// view has to go - glide (stretched by HANDOFF GLIDE SPEED, glideMs) or, farther than `placeCm`, place the view on the
// head at once (the position only: the angle blends as before, so the headset still leads and no view yaw is
// written). b318cb1: glides of 5 to 47 m squeezed into 700 ms were a slide at up to 67 m/s. placeCm <= 0: always glide.
// HANDOFF SOFT PLACE (the 2 AM log of Nov 18, approved 1:53 PM): 39 places of 2.0 to 43.6 m arrived in one frame.
// Now only the far part is placed at once (`placed` metres), and the last placeCm glide onto the head over `ms`
// (glideMs of that remainder), so the view settles onto him instead of landing in one frame.
struct LandingGlide {
    bool place{};
    int ms{};
    float placed{}; // metres placed at once (0: none); the rest glides
};
inline LandingGlide landingGlide(float metres, int baseMs, int speedCmS, int maxMs, int placeCm, float peak) {
    LandingGlide g;
    g.ms = baseMs;
    if (!std::isfinite(metres) || metres <= 0.f)
        return g;
    if (placeCm > 0 && metres * 100.f > static_cast<float>(placeCm)) {
        g.place = true;
        const float rest = static_cast<float>(placeCm) * .01f;
        g.placed = metres - rest;
        g.ms = glideMs(rest, baseMs, speedCmS, maxMs, peak);
        return g;
    }
    g.ms = glideMs(metres, baseMs, speedCmS, maxMs, peak);
    return g;
}

// A blend stretched after it began (the landing's, once the head's first frame tells the distance; 14-28 ms in, up to
// 278 ms in b318cb1): the same smoothstep at the same progress t, so the view does not step back. Keeping the start
// and changing only the length moved the view back by s(t_old) - s(t_new) of the distance in one frame (a 1.5 m glide
// stretched 250 -> 700 ms at 28 ms: 4.5 cm back; at 100 ms: 0.45 m back), and a blend already finished started over.
// Returns the clock the place runs on now: its start (moved earlier) and its new length. A blend already done, or a
// length not longer, keeps its own clock.
struct BlendClock {
    int64_t startNs{}, durNs{};
};
inline BlendClock stretchKeepingProgress(int64_t nowNs, int64_t startNs, int64_t durNs, int64_t newDurNs) {
    const int64_t d = durNs > 0 ? durNs : handoffNs;
    const int64_t elapsed = nowNs - startNs;
    if (newDurNs <= d || elapsed >= d)
        return {startNs, d};
    if (elapsed <= 0)
        return {startNs, newDurNs};
    const double t = static_cast<double>(elapsed) / static_cast<double>(d);
    return {nowNs - static_cast<int64_t>(t * static_cast<double>(newDurNs)), newDurNs};
}

// LANDING VIEW SNAP (landing_view_snap, beta3 H1; the owner, Nov 21 t86u: "Handoffs have to be cleaned up so
// that jump isn't visible to players"): a landing, zip or mount puts the view within `snapCm` of the head at
// once and glides the rest over a short `snapMs`, instead of gliding up to handoff_place_cm over up to 450 ms
// with his body drawn away from the view. The position only; the angle still blends.
inline LandingGlide landingSnap(float metres, int snapCm, int snapMs) {
    LandingGlide g;
    g.ms = (std::max)(snapMs, 1);
    if (!std::isfinite(metres) || metres <= 0.f)
        return g;
    const float keep = static_cast<float>((std::max)(snapCm, 0)) * .01f;
    if (metres > keep) {
        g.place = true;
        g.placed = metres - keep;
    }
    return g;
}
// stretchKeepingProgress for any new length, shorter too (LANDING VIEW SNAP shortens the glide): the same
// progress t on the new clock, so the view does not step. A blend already done, or no new length, keeps its
// own clock.
inline BlendClock retimeKeepingProgress(int64_t nowNs, int64_t startNs, int64_t durNs, int64_t newDurNs) {
    const int64_t d = durNs > 0 ? durNs : handoffNs;
    const int64_t elapsed = nowNs - startNs;
    if (newDurNs <= 0 || newDurNs == d || elapsed >= d)
        return {startNs, d};
    if (elapsed <= 0)
        return {startNs, newDurNs};
    const double t = static_cast<double>(elapsed) / static_cast<double>(d);
    return {nowNs - static_cast<int64_t>(t * static_cast<double>(newDurNs)), newDurNs};
}

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

// Where the blend's source view is now: the centre of the eyes shown at the switch, carried with the player's travel
// since (travel; none past carryLimit). The distance from it to the head is the distance the view actually glides -
// not the tracked eyes' (b318cb1: the landing's glide measured from the tracked eyes read 5 to 47 m where the
// shown view was at the body).
inline Vec3 shownSource(const Vec3 (&eyePos)[2], Vec3 fromAnchor, Vec3 anchorNow) {
    return (eyePos[0] + eyePos[1]) * .5f + travel(fromAnchor, anchorNow);
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
// ONE HANDOFF BLEND: the place and the angle on their own weights (the view ease carries the place: `aPos` 1).
inline Pose blendEyeSplit(Pose from, Vec3 carry, Pose to, float aPos, float aOri) {
    return {lerpVec(from.position + carry, to.position, aPos), slerpQuat(from.orientation, to.orientation, aOri)};
}
// HANDOFF HEAD LEADS (handoff_head_leads): the angle between the two views at the switch (from = offset * to), fading
// out on its own weight `a` while `to` (the live view) passes every head turn through 1:1.
inline Quat oriOffsetOf(Quat from, Quat to) {
    return from * to.conjugate();
}
inline Quat headLeadOri(Quat offset, Quat to, float a) {
    return slerpQuat(offset, Quat{}, a) * to;
}
inline Pose blendEye(Pose from, Vec3 carry, Pose to, float a) {
    return {lerpVec(from.position + carry, to.position, a), slerpQuat(from.orientation, to.orientation, a)};
}

} // namespace spidy::hybrid_handoff
