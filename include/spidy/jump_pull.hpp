// JUMP PULL (the polish pass, slice 8). The user, t65u: "a soft pull that gently pulls you toward enemies you jump
// toward." In SpidyVR's own air (never the animation's: the game owns that velocity), with no web out, a nudge toward the
// enemy the jump is aimed at, never a steer you could feel as a hand on the stick:
//  - WHO: an enemy in the fight within `rangeM` of you, whose line from the head is within `gazeDeg` of where you look, or,
//    with the left stick held, within `stickDeg` of where the stick points (the nearer in angle wins);
//  - HOW MUCH: an acceleration of `accelMax` m/s^2 that fades to nothing at the cone's edge and at the range's, and off
//    inside `nearM` (no pulling you into him), until the jump has added `capMps` m/s in all (the cap is per jump: it resets
//    on the ground or a wall); the vertical share is halved (a pull is not a lift);
//  - WHEN: in the air only, and not for the first `settleSeconds` of the jump (the jump's own impulse and the animation's
//    takeoff are left alone).
// The player's stick and web still win: the pull is a few m/s^2 and the cap is 1.5 m/s. Pure.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::jump_pull {
inline constexpr float degrees = 3.14159265f / 180.f;
struct Config {
    bool on{};
    float gazeDeg = 20.f, stickDeg = 25.f, rangeM = 12.f, capMps = 1.5f;
    float accelMax = 4.f, nearM = 1.5f, settleSeconds = .15f;
};
struct Candidate {
    uint64_t id{};
    Vec3 middle{};
};
struct Pick {
    bool valid{};
    uint64_t id{};
    Vec3 aim{};        // his middle
    float weight{};    // 0..1: the fade
    float angleDeg{};  // off the line that picked him
    bool byStick{};
};
inline float fade(float x, float from, float to) { // 1 at `from`, 0 at `to`
    if (!(to > from))
        return 0.f;
    const float t = std::clamp((x - from) / (to - from), 0.f, 1.f);
    return 1.f - t * t * (3.f - 2.f * t);
}
// The enemy the pull goes to. `head`: the eyes; `gaze`: the view's direction; `stick`: the walk stick's world direction
// (zero when it is not held, any length). The nearest in angle among those in a cone and the range.
inline Pick choose(const Config& c, Vec3 head, Vec3 gaze, Vec3 stick, const Candidate* list, size_t count) {
    Pick best;
    if (!c.on)
        return best;
    const float gazeLen = length(gaze);
    const Vec3 stickFlat{stick.x, 0.f, stick.z};
    const float stickLen = length(stickFlat);
    const bool haveGaze = std::isfinite(gazeLen) && gazeLen > .5f;
    const bool haveStick = std::isfinite(stickLen) && stickLen > .3f;
    for (size_t i = 0; i < count; ++i) {
        const Vec3 to = list[i].middle - head;
        const float distance = length(to);
        if (!std::isfinite(distance) || distance < c.nearM || distance > c.rangeM)
            continue;
        const Vec3 toFlat{to.x, 0.f, to.z};
        const float flat = length(toFlat);
        float angle = 1e9f, cone = 1.f;
        bool byStick = false;
        if (haveGaze) {
            const float cosine = std::clamp(dot(to, gaze) / (distance * gazeLen), -1.f, 1.f);
            const float a = std::acos(cosine) / degrees;
            if (a <= c.gazeDeg) {
                angle = a;
                cone = c.gazeDeg;
            }
        }
        if (haveStick && flat > .05f) {
            const float cosine = std::clamp(dot(toFlat, stickFlat) / (flat * stickLen), -1.f, 1.f);
            const float a = std::acos(cosine) / degrees;
            if (a <= c.stickDeg && a < angle) {
                angle = a;
                cone = c.stickDeg;
                byStick = true;
            }
        }
        if (angle > 1e8f)
            continue;
        const float weight = fade(angle, cone * .4f, cone) * fade(distance, c.rangeM * .6f, c.rangeM) *
                             (1.f - fade(distance, c.nearM, c.nearM * 2.f));
        if (weight <= 0.f)
            continue;
        if (!best.valid || angle < best.angleDeg) {
            best = {true, list[i].id, list[i].middle, weight, angle, byStick};
        }
    }
    return best;
}
// One step of the pull: the velocity change (m/s) toward `aim` from `position`, given what this jump has added.
// `airSeconds`: how long this jump has been in the air.
struct Step {
    Vec3 dv{};
    float added{};
};
inline Step step(const Config& c, Vec3 position, Vec3 aim, float weight, float dt, float airSeconds, float pulled) {
    Step s;
    if (!c.on || !(weight > 0.f) || !(dt > 0.f) || !std::isfinite(weight) || !std::isfinite(dt) || airSeconds < c.settleSeconds ||
        pulled >= c.capMps)
        return s;
    Vec3 to = aim - position;
    to.y *= .5f;
    const float distance = length(to);
    if (!std::isfinite(distance) || distance < .1f)
        return s;
    const float room = c.capMps - pulled;
    const float add = std::min(c.accelMax * std::clamp(weight, 0.f, 1.f) * dt, room);
    s.dv = to * (add / distance);
    s.added = add;
    return s;
}
} // namespace spidy::jump_pull
