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
constexpr float trustSeconds = .25f; // v2: .1 let the marker jump (t86u)
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
// How far the solved forearm is off the model ACROSS the wrist's bend (degrees; negative before the model is
// ready). The model is the forearm in the controller's frame at the wrist's usual angle, so a wrist bent
// toward the palm or the back of the hand turns the controller about its own Y (the thumb-to-little-finger
// axis, square to the palm's +-X) and puts the solve that far off the model although the solve is right.
// Beta2's session 031344 (d482497; the owner, Nov 20 t83u: "web shooters were still off"): the full angle cut
// trust to a median 0.23 at a 30-45 deg palm-side bend on the left (0.47 at 45-60 on the right, 0.37 at 60-75
// back), so the web took the model's line, which turns with the hand - the palm-side rule then read no offset
// (L median 0), and a bend toward the back of the hand moved the line with it. Only what a bend about that
// axis cannot do counts here: the difference of the two lines' Y components in the aim frame.
inline float offModelAcrossBendDegrees(Vec3 solvedWorld, const ForearmModel& model, Quat aimAtSolve) {
    if (!model.ready() || !unitish(solvedWorld))
        return -1.f;
    const Vec3 local = normalized(aimAtSolve.conjugate().rotate(normalized(solvedWorld)));
    if (!unitish(local))
        return -1.f;
    const float a = std::asin(std::clamp(local.y, -1.f, 1.f)),
                b = std::asin(std::clamp(model.inAim.y, -1.f, 1.f));
    return std::abs(a - b) / degree;
}
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

// W2/W3 (beta polish D11; the logs: the visible-arm line median 35 deg off the controller, p90 56, and 88-95 deg
// with an arm behind the back or overhead). The web line keeps leaving along the arm, turned by the player's own
// wrist angle as learned (ForearmModel::inAim, the forearm in the controller's frame) so it points where the
// controller does when the arm and controller agree: up to offsetCapDegrees, eased in over offsetEaseSeconds once
// the model is ready. Past fallbackDegrees between that line and the controller (a reach-around) the direction is
// the controller's (the origin stays the wrist) until back under fallbackLeaveDegrees.
constexpr float offsetCapDegrees = 45, offsetEaseSeconds = .5f, fallbackDegrees = 60, fallbackLeaveDegrees = 50;
// The hand's palm axis (aim +X for the left hand, -X for the right) across `line`: unit, or zero.
inline Vec3 palmAxis(Vec3 line, Quat aimNow, int hand) {
    const Vec3 palm = aimNow.rotate({hand == 0 ? 1.f : -1.f, 0, 0});
    const Vec3 across = palm - line * dot(palm, line);
    return length(across) > .1f ? normalized(across) : Vec3{};
}
// How far the controller's line points from `line` toward the palm (radians, + palm side, - the back of the
// hand), in the plane of the line and the palm axis (as web_aim::signedPalmAngle).
inline float palmSideRadians(Vec3 line, Quat aimNow, int hand) {
    const Vec3 c = aimNow.rotate({0, 0, -1}), out = palmAxis(line, aimNow, hand);
    if (!unitish(out) || !unitish(c))
        return 0.f;
    return std::atan2(dot(c, out), dot(c, line));
}
struct ArmLine {
    float offsetWeight{};
    bool reachAround{};
    float offsetDegrees{}; // the turn applied last (degrees)
    float offDegrees{};    // the corrected line against the controller (degrees, before W3)
    // `line` the forearm line (unit, or zero: none), `model` the learned forearm, `aimNow` the controller,
    // `hand` 0 left / 1 right for LOCKED TO ARM's palm-side rule below (-1: the learned offset, as in beta1).
    Vec3 update(Vec3 line, const ForearmModel& model, Quat aimNow, float seconds, int hand = -1) {
        seconds = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
        if (!unitish(line)) {
            reachAround = false;
            return line;
        }
        line = normalized(line);
        const Vec3 controller = normalized(aimNow.rotate({0, 0, -1}));
        const bool palmRule = hand == 0 || hand == 1;
        offsetWeight =
            model.ready() || palmRule ? std::min(1.f, offsetWeight + seconds / offsetEaseSeconds) : 0.f;
        offsetDegrees = 0;
        float side = 0;
        if (palmRule) {
            // PALM SIDE (beta2; the owner, Nov 20 t80u: "with lock to forearm, tilting the hand palm-side
            // offsets the line, whereas tilting it towards the back of the hand brings it in line. Right now,
            // it works the opposite way"). Beta1's learned offset (below) turned the arm's line by the
            // wrist's USUAL angle - in the logs the controller rests 25-35 deg toward the back of the hand
            // (forearm sign: L median about -30) - so the line sat that far toward the back of the hand at
            // rest, stayed there tilted back, and a palm-side tilt brought the controller onto it: the
            // owner's inverted feel. Now the line is the forearm's while the controller points toward the
            // back of the hand (or along the arm), and follows the controller's tilt toward the palm (the
            // hand's palm axis: left +X, right -X of the aim) up to offsetCapDegrees.
            side = palmSideRadians(line, aimNow, hand);
            if (side > 0) {
                const float turn =
                    std::min(side, offsetCapDegrees * degree) * offsetWeight * inwardShare(side);
                const Vec3 palm = palmAxis(line, aimNow, hand);
                if (unitish(palm)) {
                    line = normalized(line * std::cos(turn) + palm * std::sin(turn));
                    offsetDegrees = turn / degree;
                }
            }
        } else if (offsetWeight > 0) {
            const Vec3 forearm = model.world(aimNow);
            const float learned = angleBetween(forearm, controller);
            if (learned > 1e-4f && learned < pi - 1e-3f) {
                const Vec3 axis = normalized(cross(forearm, controller));
                const float turn = std::min(learned, offsetCapDegrees * degree) * offsetWeight;
                line = normalized(Quat::around(axis, turn).rotate(line));
                offsetDegrees = turn / degree;
            }
        }
        offDegrees = angleBetween(line, controller) / degree;
        // The reach-around is for an arm behind the back or overhead, not a wrist bent back: toward the back
        // of the hand (palm rule) only what the bend cannot make counts - the line off the aim's palm plane
        // (its Y; offModelAcrossBendDegrees). Session 031344 (d482497): bent back 60-75 deg past the forearm
        // the left hand's web went to the controller (reach-around) in 50 of 75 samples - the owner's "still
        // off".
        float reach = offDegrees;
        if (palmRule && side <= 0) {
            const Vec3 across = aimNow.rotate({0, 1, 0});
            reach = unitish(across)
                        ? std::abs(std::asin(std::clamp(dot(line, normalized(across)), -1.f, 1.f))) / degree
                        : offDegrees;
        }
        reachAround = reachAround ? reach > fallbackLeaveDegrees : reach > fallbackDegrees;
        return reachAround ? controller : line;
    }
    void reset() {
        *this = {};
    }
};

} // namespace spidy::spherical_aim
