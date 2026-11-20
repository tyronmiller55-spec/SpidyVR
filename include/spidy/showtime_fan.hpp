// SHOWTIME RAY FAN (beta2; the owner, Nov 19 t79u: "ray fan for Showtime is a good call"). Pure; no Windows.
//
// While SHOWTIME's camera is out, the eyes are the outside camera's, so the head's clearance rays (tags 2 to
// 6, game_tracking.cpp appendTrackingRays) have nothing to keep clear. Their slots of the eight native world
// rays (native_rays.hpp: capacity 8, the game's own Havok queries with the player's body filter,
// Command::bodyRays) carry a fan instead: from his pelvis to where the camera wants to sit, the centre and up
// to four rays to a 0.25 m ring around it (tags 40 to 44; docs/SHOWTIME_CAMERA_COLLISION.md, "a ray fan").
// The nearest hit, less a 0.25 m margin, limits the camera's distance by the same ratio (showtime_rig.hpp:
// fast in, slow out). No new game address: the same query and the same slots the head already uses.
#pragma once
#include "math.hpp"
#include "native_rays.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace spidy::showtime_fan {

inline constexpr unsigned maxRays = 5;
inline constexpr uint32_t firstTag = 40;
inline constexpr float ring = .25f;   // m around the camera's place
inline constexpr float margin = .25f; // m kept short of a hit
inline constexpr float freshSeconds = .25f;

// How many fan rays fit: the slots the aim, the hands' contact and the roof leave (at most five, at least
// none).
inline unsigned room(unsigned aim, unsigned hands, bool roof) {
    const unsigned used = aim + hands + (roof ? 1u : 0u);
    return used >= native_rays::capacity ? 0u : std::min(maxRays, native_rays::capacity - used);
}

// The fan from `from` (his pelvis) to `to` (the camera's wanted place): the centre first, then the ring's
// right, left, up and down, as many as `count`. None when the two are too close or not finite.
inline unsigned rays(Vec3 from, Vec3 to, unsigned count, std::array<native_rays::Ray, maxRays>& out) {
    count = std::min(count, maxRays);
    if (!finite(from) || !finite(to) || !count)
        return 0;
    const Vec3 d = to - from;
    const float len = length(d);
    if (!(len > .3f) || len > 50.f)
        return 0;
    const Vec3 ahead = d / len;
    Vec3 right = cross(ahead, Vec3{0, 1, 0});
    if (length(right) < .1f)
        right = {1, 0, 0};
    right = normalized(right);
    const Vec3 up = normalized(cross(right, ahead));
    const Vec3 targets[maxRays] = {to, to + right * ring, to - right * ring, to + up * ring, to - up * ring};
    unsigned n = 0;
    for (unsigned i = 0; i < count; ++i) {
        const Vec3 t = targets[i] - from;
        const float l = length(t);
        if (!(l > .01f))
            continue;
        out[n++] = {from, l, t / l, firstTag + i};
    }
    return n;
}

struct Limit {
    bool ok{};       // a fan batch was read (fresh, from the fan's own rays)
    unsigned rays{}; // its rays seen
    unsigned hits{}; // those that met the world
    float ratio =
        1; // the camera's distance may be this much of what it wants: the nearest hit less the margin
    uint32_t tag{}; // the nearest hit's ray
};

inline bool isFan(uint32_t tag) {
    return tag >= firstTag && tag < firstTag + maxRays;
}

// The nearest of a batch's fan hits. A ray that met nothing leaves the ratio at 1; one that met the world
// limits it to (hit - margin) / length, never below 0.
inline Limit select(const native_rays::Hit* hits, unsigned count) {
    Limit l;
    for (unsigned i = 0; hits && i < count && i < native_rays::capacity; ++i) {
        const auto& h = hits[i];
        if (!isFan(h.ray.tag) || !(h.ray.distance > .01f))
            continue;
        ++l.rays;
        if (!h.count || !std::isfinite(h.fraction) || h.fraction >= 1.f)
            continue;
        ++l.hits;
        const float ratio =
            std::clamp((std::max(h.fraction, 0.f) * h.ray.distance - margin) / h.ray.distance, 0.f, 1.f);
        if (ratio < l.ratio) {
            l.ratio = ratio;
            l.tag = h.ray.tag;
        }
    }
    l.ok = l.rays > 0;
    return l;
}

// SPEED LEAD: the share of his own velocity the pivot is carried by each step before its spring, from 8 m/s
// (none) to 25 m/s (all of `lead`), so a critically damped follow's lag (2 v / omega) shrinks at speed and is
// as before slow.
inline float speedLead(float speed, float lead) {
    if (!std::isfinite(speed) || !std::isfinite(lead))
        return 0.f;
    return std::clamp(lead, 0.f, 1.f) * std::clamp((speed - 8.f) / 17.f, 0.f, 1.f);
}

} // namespace spidy::showtime_fan
