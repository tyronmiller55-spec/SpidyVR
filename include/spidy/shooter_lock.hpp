// WEB SHOOTERS: LOCKED TO ARM (the user, November 11 22:59: "my model forearm
// wrist shooters aren't working at all. I need clear settings in the menu to
// lock that to my arm. There are too many wrist settings that constantly mess
// with each other").
//
// Before: five rows - WEB AIM (CONTROLLER / MODEL FOREARM), FOREARM BLEND,
// FOREARM AIM LIMIT, WRIST SHOOTERS, PALM SHOOTERS - plus WEB ANGLE, all
// stacking on one another (WRIST SHOOTERS took 100% of the forearm unless WEB
// AIM was MODEL FOREARM, then FOREARM BLEND did; FOREARM AIM LIMIT had not
// been read since the spherical aim; PALM SHOOTERS turned on top of either).
// And in Native Animation, whatever they said, the webs were refused the arm
// the player sees: game_xr marked every sample "arms overlay" and used a
// forearm offset learned only while SpidyVR owned the body (never in
// animation), or the controller's line until it had learned one.
//
// Now one choice, SHOOTERS, and one strength, LOCK STRENGTH:
//  - CONTROLLER: along the controller, as before.
//  - LOCKED TO ARM: along the forearm of the arm the player sees, elbow to
//    wrist - the solve's arm, the SpidyVR arms drawn over the animation too -
//    exactly (no inward/outward wrist clamp). Where that arm cannot be
//    trusted (out of reach, not solved, fading in) the forearm learned on the
//    controller (spherical_aim::ForearmModel) holds the line, never a jump to
//    the controller's.
//  - PALM: the old palm-side quarter turn (PALM SHOOTERS).
// WEB ANGLE still tilts the line up or down on top. Both rows are views over
// the old spidy_vr.ini keys (wrist_shooters, web_aim_source, palm_shooters,
// forearm_blend): nothing to migrate, an older build reads the same file, and
// a profile from one plays the same here. forearm_aim_limit is retired (kept
// in the file, read by nothing). Pure; no Windows.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::shooter_lock {
enum class Mode : int { controller = 0, arm = 1, palm = 2 };
inline const char* modeName(Mode m) {
    switch (m) {
    case Mode::controller:
        return "CONTROLLER";
    case Mode::arm:
        return "LOCKED TO ARM";
    case Mode::palm:
        return "PALM";
    }
    return "?";
}
// SHOOTERS as the old keys say it. PALM first: it was a turn on top of
// either, and on its own it never followed the arm.
template <class V> inline Mode mode(const V& v) {
    if (v.palmShooters)
        return Mode::palm;
    if (v.wristShooters || v.webAimSource == 1)
        return Mode::arm;
    return Mode::controller;
}
// SHOOTERS chosen: the old keys set so that mode() reads it back, every
// older reader agreeing (game_swing's palm turn, game_xr's forearm turns).
template <class V> inline void setMode(V& v, Mode m) {
    v.palmShooters = m == Mode::palm;
    v.wristShooters = m == Mode::arm;
    v.webAimSource = m == Mode::arm ? 1 : 0;
}
// LOCK STRENGTH, percent of the arm's line (0 the controller's): FOREARM
// BLEND where WEB AIM said MODEL FOREARM; WRIST SHOOTERS alone took all of it.
template <class V> inline int strength(const V& v) {
    return v.webAimSource == 1 ? std::clamp(v.forearmBlend, 0, 100) : 100;
}
template <class V> inline void setStrength(V& v, int percent) {
    v.forearmBlend = std::clamp(percent, 0, 100);
    if (mode(v) == Mode::arm)
        v.webAimSource = 1;
}

// Which arm the lock follows this frame (the log's "lock:" source).
enum class Source : int { none, visibleArm, learnedArm };
inline const char* sourceName(Source s) {
    return s == Source::visibleArm ? "visible arm" : s == Source::learnedArm ? "learned arm" : "controller";
}
// The arm drawn this frame, elbow and wrist in world axes, and how far its
// wrist missed the hand (metres; negative unknown).
struct Arm {
    bool valid{};
    Vec3 elbow{}, wrist{};
    float handError = -1;
};
// An arm whose forearm the webs may take: a real length and the wrist on the
// hand (web_aim::forearmMiss, 0.1 m).
inline bool trusted(const Arm& a, float maxMiss = .1f) {
    if (!a.valid || !finite(a.elbow) || !finite(a.wrist))
        return false;
    if (length(a.wrist - a.elbow) < .05f)
        return false;
    return a.handError >= 0 && a.handError <= maxMiss;
}
inline Vec3 forearm(const Arm& a) {
    return normalized(a.wrist - a.elbow);
}
// In Native Animation the drawn arm is the SpidyVR arm solved onto the
// tracked hand once the overlay has faded in; below this weight it is still
// partly the animation's.
constexpr float overlayInWeight = .85f;
// LOCKED TO ARM refuses the visible arm only where it is not on the hand:
// before the overlay has faded in (Native Animation), out of reach, or a
// solve that is not new. Never just because the animation owns the body.
inline bool visibleUsable(const Arm& a, bool inAnimation, float overlayWeight, bool fresh) {
    if (!fresh || !trusted(a))
        return false;
    return !inAnimation || overlayWeight > overlayInWeight;
}
} // namespace spidy::shooter_lock
