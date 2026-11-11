// 360 DEGREE SPHERICAL AIMING (the user, November 10 20:54: "When aiming high
// above/behind my head, or low near my legs, the web shooters break and the
// markers jump around. See if you can enable 360 degree spherical aiming").
//
// What broke. With WRIST SHOOTERS / MODEL FOREARM the webs leave along the
// body solve's forearm, elbow to wrist (web_aim::forearmWorldTurn). The solve
// guesses the elbow from one fixed pole - down, out and a little back
// (body_ik step 6) - so a hand over or behind the head, or down by the legs,
// gets an elbow on the wrong side: the solved forearm then points somewhere
// the real one does not (Nov 9 22:27: 227 of 4675 active "forearm aim:" lines
// were 90-180 degrees off the controller; Nov 10 18:40: 9 of 187). And
// whenever the solve could not say (wrist miss: out of the rig's reach, stale,
// not solved, the animation's arms), the web jumped from the forearm's line to
// the controller's line, on average 45 degrees away, over 0.15 s - the
// markers' jump. Nothing downstream is hemisphere-limited: the swing's shot,
// the grab's pick, the web balls' assist and the previews all work on a unit
// direction over the whole sphere.
//
// The fix, all on unit directions and quaternions (no Euler angles, no pitch
// clamp, no forward cone):
//  - ForearmModel learns where the player's forearm lies in the controller's
//    own aim frame from the solve while the solve is trustworthy (hand within
//    reach, forearm near the model), a slow average. Carried on the
//    controller's current aim it gives a forearm line that is good in ANY
//    direction: behind, above, below.
//  - trust() says how far to believe the solve now; Blend eases that over
//    trustSeconds, so a contortion fades the line onto the model, never cuts.
//  - When the solve cannot say at all, the model (once learned) keeps the web
//    on the same forearm line instead of jumping to the controller's.
//  - toward() moves a direction along the great circle, so a filter fed a
//    direction far from its state (a flick over the shoulder) never passes
//    through a near-zero vector.
// The wrist clamp (outward 0, inward 15 degrees) still applies on top, about
// the line chosen here. Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::spherical_aim {

constexpr float pi = 3.14159265f, degree = pi / 180;
// A wrist this near its target (metres) is fully trusted; at web_aim::forearmMiss
// (0.1 m) not at all.
constexpr float trustedMiss = .03f, untrustedMiss = .1f;
// The solved forearm this far off the learned model: trusted, then not.
constexpr float trustedOffDegrees = 35, untrustedOffDegrees = 70;
// Trust eases this fast either way (a contortion fades in, never cuts).
constexpr float trustSeconds = .1f;
// The model: how fast it learns, and how much learning makes it usable.
constexpr float learnSeconds = 1.5f, primeSeconds = .6f;
// Only samples this trusted teach the model; and none further than this
// from the controller's line (a solve that is plainly wrong).
constexpr float learnTrust = .9f, learnMaxOffControllerDegrees = 80;

inline float smoothstep(float a, float b, float x) {
    if (!(b > a))
        return x >= b ? 1.f : 0.f;
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
inline bool unitish(Vec3 v) {
    return finite(v) && length(v) > .5f;
}
inline float angleBetween(Vec3 a, Vec3 b) {
    if (!unitish(a) || !unitish(b))
        return pi;
    a = normalized(a);
    b = normalized(b);
    // atan2 of |a x b| and a.b: accurate near 0 and near 180 degrees alike.
    return std::atan2(length(cross(a, b)), dot(a, b));
}
// Unit `a` moved toward unit `b` by share `t` (0 a, 1 b) of the angle between
// them, along the great circle. Opposite directions turn about any axis square
// to `a`: the result stays unit, never the near-zero sum a lerp gives.
inline Vec3 toward(Vec3 a, Vec3 b, float t) {
    if (!unitish(a))
        return unitish(b) ? normalized(b) : a;
    a = normalized(a);
    if (!unitish(b) || !std::isfinite(t))
        return a;
    b = normalized(b);
    t = std::clamp(t, 0.f, 1.f);
    const float angle = angleBetween(a, b);
    if (angle < 1e-5f)
        return b;
    Vec3 axis = cross(a, b);
    if (length(axis) < 1e-6f)
        axis = cross(a, std::abs(a.x) < .9f ? Vec3{1, 0, 0} : Vec3{0, 0, 1});
    return normalized(Quat::around(normalized(axis), angle * t).rotate(a));
}
// How far to believe the body solve's forearm now (0 none, 1 all): the
// wrist's miss (metres; negative unknown) and, once the model is learned,
// how far the solved forearm is off it.
inline float trust(float handError, float offModelDegrees, bool modelLearned) {
    if (!(handError >= 0) || !std::isfinite(handError))
        return 0;
    float t = 1 - smoothstep(trustedMiss, untrustedMiss, handError);
    if (modelLearned)
        t = std::min(t, 1 - smoothstep(trustedOffDegrees, untrustedOffDegrees,
                                      std::isfinite(offModelDegrees) ? offModelDegrees : 180.f));
    return t;
}
// The player's forearm in the controller's aim frame (-Z the aim line),
// learned from the trusted solve.
struct ForearmModel {
    Vec3 inAim{};       // unit once any sample came
    float learned{};    // seconds of trusted samples, up to primeSeconds
    bool ready() const {
        return learned >= primeSeconds && unitish(inAim);
    }
    // One trusted sample: the solved forearm (world) and the aim pose's
    // orientation it was solved against.
    void learn(Vec3 forearmWorld, Quat aimAtSolve, float seconds) {
        if (!unitish(forearmWorld) || !std::isfinite(seconds) || seconds <= 0)
            return;
        const Vec3 local = normalized(aimAtSolve.conjugate().rotate(normalized(forearmWorld)));
        if (!unitish(local) || angleBetween(local, {0, 0, -1}) > learnMaxOffControllerDegrees * degree)
            return;
        seconds = std::min(seconds, .1f);
        if (!unitish(inAim) || learned <= 0) {
            inAim = local;
        } else {
            // Faster while priming, then the slow average.
            const float tau = ready() ? learnSeconds : primeSeconds * .5f;
            inAim = toward(inAim, local, 1 - std::exp(-seconds / tau));
        }
        learned = std::min(primeSeconds, learned + seconds);
    }
    // The model's forearm in the world for the aim pose now; zero before ready.
    Vec3 world(Quat aimNow) const {
        return ready() ? normalized(aimNow.rotate(inAim)) : Vec3{};
    }
    void reset() {
        *this = {};
    }
};
// The forearm line the webs take: the solve's where it is trusted, the
// model's where it is not, eased (trustSeconds) along the great circle.
struct Blend {
    float trust{};
    bool primed{};
    // `solved` (zero: none), `model` (zero: not learned), `target` the trust
    // now (trust()). Returns the line, or zero when there is neither.
    Vec3 update(Vec3 solved, Vec3 model, float target, float seconds) {
        const bool haveSolved = unitish(solved), haveModel = unitish(model);
        if (!haveSolved && !haveModel) {
            primed = false;
            trust = 0;
            return {};
        }
        if (!haveModel)
            target = 1; // nothing else to go to: as before
        if (!haveSolved)
            target = 0;
        target = std::clamp(std::isfinite(target) ? target : 0.f, 0.f, 1.f);
        if (!primed || !std::isfinite(seconds) || seconds < 0 || seconds > .25f) {
            trust = target;
            primed = true;
        } else {
            trust += (target - trust) * (1 - std::exp(-seconds / trustSeconds));
        }
        if (!haveModel)
            return normalized(solved);
        if (!haveSolved)
            return normalized(model);
        return toward(normalized(model), normalized(solved), trust);
    }
    void reset() {
        *this = {};
    }
};
// The inward cap (web_aim::forearmInwardDegrees) over the signed palm angle
// `signedRadians` (web_aim::signedPalmAngle): as before up to 90 degrees off,
// then fading to none at 120, so the wrap of that angle at 180 degrees (a
// controller pointing back along the forearm) cannot flip the web by the cap.
inline float inwardShare(float signedRadians) {
    const float off = std::abs(std::isfinite(signedRadians) ? signedRadians : 0.f);
    return 1 - smoothstep(90 * degree, 120 * degree, off);
}

} // namespace spidy::spherical_aim
