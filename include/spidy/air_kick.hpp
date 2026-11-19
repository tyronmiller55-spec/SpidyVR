#pragma once
// AIR KICKS (K2; the plan's Pillar 5; kicks, off until set, EXPERIMENTAL).
//
// Legs engaged (Y + X, leg_hold.hpp) and off the ground, a fast flick of the right stick kicks. A slow push
// is still the legs' pose (jackknife, splay): only a flick (the stick from under 0.3 to over 0.8 within
// 80 ms) kicks.
//
//   flick straight up (within 20 deg)       two-leg dive (drop kick), at the chest
//   flick up-left / up-right (20..70 deg)   one-leg dive kick with that side's leg, chest or head
//   quarter circle, a side to up            spinning roundhouse (the foot sweeps in from that side), head
//   flick sideways (70..110) or down        freeform blow: a short, lighter kick, the nearest zone
//
// A sideways flick waits up to 0.25 s (or until the stick comes back) to see whether it is the start of a
// roundhouse's quarter circle; every other flick kicks at once.
//
// Kicks work like punches: a fast foot that meets an enemy lands a blow. The foot is modelled (the pelvis
// 0.9 m over the feet, the foot on a leg reaching 1.15 m plus a 0.4 m lunge in the air, no foot position is
// read back from the body), swept as a 10 cm sphere against every fighting enemy's capsule. Its speed (its
// own and the player's motion) gives the kick's strength: nothing under 3 m/s, full at 10 m/s. The gaze
// picks the zone point (head over 80 % of the capsule, legs under 35 %, chest between) of an enemy within
// 25 degrees of it and within reach, and bends the foot toward it by w = smoothstep(cos 25, cos 8, gaze . to
// zone), at most 0.25 m or 15 degrees. Standing is ground_kick.hpp's (K3): the same kick with no lunge.
//
// The kick's legs are leg_layers' top layer (a pose of leg_control's angles); its spin reaction goes through
// the legs' coupling scaled by KICK SPIN (30 % by default). No game address is used: the blow is the web
// strike's (game_punch), the targets are the ones the punches use.
//
// Engine independent. XR detects the gesture (Gestures) and asks (game_swing::AirKickAsk); the swing module
// runs the kick (Kick) and lands it; XR poses the legs from its account (legPose).
#include "enemy_joints.hpp"
#include "leg_control.hpp"
#include "math.hpp"
#include "swing_kick.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace spidy::air_kick {
inline constexpr float pi = 3.14159265f;
inline constexpr float deg = pi / 180.f;

enum class Gesture : std::uint8_t { none, dive, doubleDive, roundhouse, freeform, sweep };
inline const char* gestureName(Gesture g) {
    switch (g) {
    case Gesture::dive:
        return "dive";
    case Gesture::doubleDive:
        return "double";
    case Gesture::roundhouse:
        return "roundhouse";
    case Gesture::freeform:
        return "free";
    case Gesture::sweep:
        return "sweep";
    default:
        return "none";
    }
}
// arms: only with ENEMY LIMB ZONES (R1), from an enemy's forearm joints; never from the capsule.
enum class Zone : std::uint8_t { head, chest, legs, arms };
inline const char* zoneName(Zone z) {
    return z == Zone::head ? "head" : z == Zone::legs ? "legs" : z == Zone::arms ? "arms" : "chest";
}

struct Config {
    // The gaze: zone points within gazeCone of it, full weight inside gazeFull.
    float gazeCone = 25.f, gazeFull = 8.f; // KICK GAZE CONE sets it (40 by default since the 2 AM log)
    // KICK TARGET FALLBACK (the 2 AM log, approved Nov 18 1:53 PM): with no zone point inside the gaze cone, the
    // nearest enemy in front (within frontDegrees of the gaze, level) and within the foot's reach is the target:
    // the kick goes his way and his zone is the gesture's own.
    float frontDegrees = 75.f, frontSlack = 0.f;
    // The leg's reach from the pelvis, and the air's lunge on top; the foot's sphere; the pelvis' height
    // over the feet.
    float reach = 1.15f, lunge = .4f, footRadius = .1f, hipHeight = .9f;
    // The foot bends toward the zone point at most this far and this many degrees.
    float bendMetres = .25f, bendDegrees = 15.f;
    // The foot's speed (m/s) for no blow and for the full one.
    float minFoot = 3.f, fullFoot = 10.f;
    // A blow this strong or more flies the enemy (a kinetic blow, as every dive).
    float flyStrength = .7f;
    // The flick: the stick's size from under flickLow to over flickHigh within flickSeconds.
    float flickLow = .35f, flickHigh = .75f, flickSeconds = .15f; // .3, .8, 80 ms until Nov 18 (the owner: kicks unreliable)
    // Straight up within straightDegrees (double dive), a side's dive within diveDegrees, else freeform.
    float straightDegrees = 20.f, diveDegrees = 70.f, sideDegrees = 110.f;
    // A roundhouse: the side (size and share of the stick's width), the longest the quarter circle takes,
    // where it must end (degrees from up) and the least size on the way.
    float rimSide = .6f, rimSeconds = .25f, rimEnd = 35.f, rimSize = .5f;
    // The kick's timeline (seconds): the foot's way out, held, and back. The roundhouse's sweep takes longer;
    // a freeform blow is slower and shorter.
    float extend = .16f, roundhouseExtend = .3f, freeformExtend = .22f, hold = .1f, recover = .25f;
    float freeformReach = .7f;   // the share of the reach a freeform blow has
    float roundhouseArc = 100.f; // degrees the foot sweeps in from the side
    // The least time between kicks.
    float cooldown = .15f;
    // The flight a blow gives (share of the foot's speed, the most, and a little up).
    float flightShare = .8f, flightMax = 14.f, flightUp = 1.5f;
    // GROUND KICKS (K3, ground_kick.hpp): standing, the player may not be faster than groundSpeed (m/s) when
    // a kick starts (or groundSpeed + groundSlack while it runs); the leg has no lunge. A sweep (the stick's
    // half circle across the bottom) swings the foot round the pelvis at sweepReach, sweepHeight over the
    // feet, through sweepArc degrees in sweepExtend seconds. A ground blow knocks down rather than throws:
    // the enemy's flight is groundFlightShare of the foot's speed, at most groundFlightMax m/s, groundFlightUp
    // up.
    float groundSpeed = 3.f, groundSlack = 1.5f; // 2 m/s until Nov 18 (ground kicks while walking: faster, more reliable)
    float sweepReach = .9f, sweepHeight = .12f, sweepArc = 180.f, sweepExtend = .35f;
    float groundFlightShare = .5f, groundFlightMax = 6.f, groundFlightUp = .5f;
    // ENEMY LIMB ZONES (R1, enemy_joints.hpp): the legs and the arms from an enemy's shin and forearm joints
    // (a Target's joints); off, the legs are the capsule's band and there is no arms zone.
    bool limbZones = false;
};
// The same, standing: no lunge.
inline Config standingConfig(Config c) {
    c.lunge = 0;
    return c;
}

// The stick's way: degrees from straight up, + to the right (0..180).
inline float stickAngle(float x, float y) {
    return std::atan2(x, y) * (180.f / pi);
}
inline float smoothstep(float a, float b, float x) {
    if (!(b > a))
        return x >= b ? 1.f : 0.f;
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
// What one flick is: the gesture, the side (-1 left, +1 right; a roundhouse's: the side the quarter circle
// began on), the angle and size it had.
struct Flick {
    Gesture gesture{};
    int side{};
    float angle{}, size{};
};
// Where a flick at `angle` (degrees from up, + right) falls: not a roundhouse's, that is a path.
inline Gesture classify(float angle, const Config& c = {}) {
    const float a = std::abs(angle);
    if (a <= c.straightDegrees)
        return Gesture::doubleDive;
    if (a <= c.diveDegrees)
        return Gesture::dive;
    return Gesture::freeform;
}
inline int sideOf(float x) {
    return x < 0 ? -1 : 1;
}

// The right stick's gestures (XR side). Feed every sample; the returned Flick has a gesture only on the
// sample that completes one.
class Gestures {
  public:
    // x right, y up (the controller's). engaged: the legs are engaged and the setting is on.
    Flick update(float seconds, float x, float y, bool engaged, const Config& c = {}) {
        Flick none;
        if (!engaged || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(seconds)) {
            reset();
            return none;
        }
        const float dt = std::max(seconds, 0.f);
        const float size = std::hypot(x, y);
        if (pending_) {
            // A sideways flick waiting for its quarter circle: up and over -> roundhouse; the stick back
            // in, or too slow -> freeform.
            pendingAge_ += dt;
            const float angle = stickAngle(x, y);
            if (size > c.flickHigh && y > 0 && std::abs(angle) <= c.rimEnd && minSize_ >= c.rimSize) {
                pending_ = false;
                armed_ = false;
                return {Gesture::roundhouse, pendingSide_, angle, size};
            }
            minSize_ = std::min(minSize_, size);
            if (size < c.rimSize || pendingAge_ > c.rimSeconds) {
                pending_ = false;
                armed_ = size < c.flickLow;
                return {Gesture::freeform, pendingSide_, pendingAngle_, pendingSize_};
            }
            return none;
        }
        if (size < c.flickLow) {
            armed_ = true;
            sinceLow_ = 0;
            return none;
        }
        sinceLow_ += dt;
        if (size <= c.flickHigh || !armed_)
            return none;
        // Over the high mark: a flick if it came quickly, otherwise a push (the legs' pose).
        armed_ = false;
        if (sinceLow_ > c.flickSeconds + 1e-6f)
            return none;
        const float angle = stickAngle(x, y);
        const float a = std::abs(angle);
        if (a >= c.diveDegrees && a <= c.sideDegrees) {
            pending_ = true;
            pendingAge_ = 0;
            pendingSide_ = sideOf(x);
            pendingAngle_ = angle;
            pendingSize_ = size;
            minSize_ = size;
            return none;
        }
        return {classify(angle, c), sideOf(x), angle, size};
    }
    void reset() {
        armed_ = pending_ = false;
        sinceLow_ = 1e3f;
    }
    bool pending() const {
        return pending_;
    }

  private:
    bool armed_{}, pending_{};
    float sinceLow_ = 1e3f, pendingAge_{}, minSize_{};
    int pendingSide_{};
    float pendingAngle_{}, pendingSize_{};
};

// KICK FLICK MISSES (the 2 AM log, approved Nov 18 1:53 PM: 7 Y+X holds there had no kick and the log said not
// why). Watches the same stick as Gestures while the legs are engaged: each push out of the low mark that no
// gesture came of, once the stick is back under it (or the legs let go), is a miss - its peak size, the time from
// the low mark to the peak and to the high mark (if it got there), and why: too small (never past flickHigh) or
// too slow (past it after flickSeconds), not armed (past it in time, but the stick was already out when the legs
// engaged) or not taken (in time and armed: a cooldown or a kick under way). Pushes that never pass `watchSize` are
// resting thumbs, not tries.
struct FlickMiss {
    const char* why{}; // nullptr: none
    float peak{}, peakSeconds{}, highSeconds = -1.f, angle{};
};
class FlickWatch {
  public:
    static constexpr float watchSize = .5f;
    // fired: a gesture came of this sample (or is pending, the roundhouse's wait).
    FlickMiss update(float seconds, float x, float y, bool engaged, bool fired, const Config& c = {}) {
        FlickMiss miss;
        const float size = std::isfinite(x) && std::isfinite(y) ? std::hypot(x, y) : 0.f;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (fired)
            out_ = false, done_ = true;
        if (!engaged) {
            sawLow_ = false;
        } else if (size < c.flickLow) {
            sawLow_ = true;
        }
        if (!engaged || size < c.flickLow) {
            if (out_ && !done_ && peak_ >= watchSize) {
                // Past the high mark in time is no slow flick (the 2:23 AM log: "too slow ... past 0.75 at 14 ms"):
                // the stick was already out when the legs engaged (no low mark to start from), or a kick was busy.
                miss.why = peak_ <= c.flickHigh                         ? "too small"
                           : highAt_ < 0 || highAt_ > c.flickSeconds + 1e-6f ? "too slow"
                           : !armedStart_                               ? "not armed: the stick was out when the legs engaged"
                                                                        : "not taken: cooldown or a kick under way";
                miss.peak = peak_;
                miss.peakSeconds = peakAt_;
                miss.highSeconds = highAt_;
                miss.angle = angle_;
            }
            out_ = done_ = false;
            age_ = peak_ = peakAt_ = 0;
            highAt_ = -1.f;
            return miss;
        }
        if (!out_ && !done_) {
            out_ = true;
            armedStart_ = sawLow_;
            age_ = peak_ = peakAt_ = 0;
            highAt_ = -1.f;
        }
        if (!out_)
            return miss;
        age_ += dt;
        if (size > peak_) {
            peak_ = size;
            peakAt_ = age_;
            angle_ = stickAngle(x, y);
        }
        if (highAt_ < 0 && size > c.flickHigh)
            highAt_ = age_;
        return miss;
    }

  private:
    bool out_{}, done_{}, sawLow_{}, armedStart_{};
    float age_{}, peak_{}, peakAt_{}, highAt_ = -1.f, angle_{};
};

// ---- the zone ----
inline Zone zoneAt(float heightShare) {
    return heightShare > .8f ? Zone::head : heightShare < .35f ? Zone::legs : Zone::chest;
}
inline Vec3 zonePoint(Vec3 feet, float height, Zone z) {
    const float share = z == Zone::head ? .9f : z == Zone::legs ? .2f : .65f;
    return feet + Vec3{0, height * share, 0};
}
// The same for an enemy whose joints were read (R1): the joint's own place where it has one (the head and chest
// points; with the limb zones the nearer shin or forearm to `ref`), else the capsule's band. `byJoints` says
// which.
inline Vec3 zonePoint(Vec3 feet, float height, Zone z, const enemy_joints::Joints& joints, bool limbs, Vec3 ref,
                      bool* byJoints = nullptr) {
    Vec3 on;
    if (joints.valid && enemy_joints::partPoint(joints, static_cast<int>(z), limbs, ref, on)) {
        if (byJoints)
            *byJoints = true;
        return on;
    }
    if (byJoints)
        *byJoints = false;
    return zonePoint(feet, height, z == Zone::arms ? Zone::chest : z);
}
// The zone a hit at `point` met on an enemy: from his joints when they give a head and a chest (the nearest
// of head, chest and, with the limb zones, forearms and shins; without them the band under 35 % is the legs),
// else the capsule's bands (zoneAt of the height share).
inline Zone zoneOfHit(Vec3 feet, float height, Vec3 point, const enemy_joints::Joints& joints, bool limbs,
                      bool* byJoints = nullptr) {
    const float share = std::clamp((point.y - feet.y) / std::max(height, .5f), 0.f, 1.f);
    if (byJoints)
        *byJoints = false;
    if (joints.valid) {
        const int part = enemy_joints::partOfHit(joints, point, limbs);
        if (part >= 0) {
            if (byJoints)
                *byJoints = true;
            // The legs without the limb zones: the band (a shin joint is not read for them).
            if (!limbs && part != enemy_joints::partHead && share < .35f)
                return Zone::legs;
            return static_cast<Zone>(part);
        }
    }
    return zoneAt(share);
}
// The zones a gesture goes for, nearest to the gaze among them: the dives the chest, a one-leg dive the
// chest or the head, the roundhouse the head, a freeform blow any.
inline std::uint32_t zonesFor(Gesture g) {
    switch (g) {
    case Gesture::sweep:
        return 1u << static_cast<int>(Zone::legs);
    case Gesture::doubleDive:
        return 1u << static_cast<int>(Zone::chest);
    case Gesture::dive:
        return (1u << static_cast<int>(Zone::chest)) | (1u << static_cast<int>(Zone::head));
    case Gesture::roundhouse:
        return 1u << static_cast<int>(Zone::head);
    default:
        return 7u; // (a freeform blow may also go for the arms, 8, with the limb zones: Kick::ask)
    }
}
// The gaze's weight on a zone point: smoothstep(cos 25, cos 8, gaze . (point - eye)^).
inline float gazeWeight(Vec3 gaze, Vec3 eye, Vec3 point, const Config& c = {}) {
    const Vec3 to = point - eye;
    if (!finite(gaze) || !finite(to) || length(to) < 1e-4f || length(gaze) < 1e-4f)
        return 0;
    return smoothstep(std::cos(c.gazeCone * deg), std::cos(c.gazeFull * deg),
                      dot(normalized(gaze), normalized(to)));
}
// The foot at rest `rest` bent toward `point`, by weight w, at most bendMetres and bendDegrees (seen from the
// pelvis).
inline Vec3 bentFoot(Vec3 pelvis, Vec3 rest, Vec3 point, float w, const Config& c = {}) {
    const Vec3 a = rest - pelvis, b = point - pelvis;
    const float la = length(a), lb = length(b);
    if (!(w > 0) || la < 1e-4f || lb < 1e-4f || !finite(rest) || !finite(point))
        return rest;
    const Vec3 ua = a / la, ub = b / lb;
    const float angle = std::acos(std::clamp(dot(ua, ub), -1.f, 1.f));
    // At most bendDegrees, and the chord the foot moves at most bendMetres (a leg la long turned by t moves
    // 2 la sin(t/2)).
    const float limit =
        std::min(c.bendDegrees * deg, 2.f * std::asin(std::clamp(c.bendMetres / (2.f * la), 0.f, 1.f)));
    const float turn = std::min(angle, limit) * std::clamp(w, 0.f, 1.f);
    Vec3 across = ub - ua * dot(ub, ua);
    const float off = length(across);
    if (off > 1e-5f && angle > 1e-5f)
        return pelvis + normalized(ua * std::cos(turn) + (across / off) * std::sin(turn)) * la;
    return rest;
}

// ---- the kick's strength and flight ----
inline float strengthOf(float footSpeed, const Config& c = {}) {
    if (!std::isfinite(footSpeed) || footSpeed < c.minFoot)
        return 0;
    return std::clamp((footSpeed - c.minFoot) / std::max(c.fullFoot - c.minFoot, 1e-3f), 0.f, 1.f);
}
inline bool flies(float strength, const Config& c = {}) {
    return strength >= c.flyStrength;
}
// A blow on the ground knocks down: a shorter, flatter flight.
inline Vec3 groundFlight(Vec3 way, float footSpeed, const Config& c = {}) {
    if (!finite(way) || length(way) < 1e-4f)
        return {};
    return normalized(way) * std::min(c.groundFlightMax, std::max(footSpeed, 0.f) * c.groundFlightShare) +
           Vec3{0, c.groundFlightUp, 0};
}
inline Vec3 enemyFlight(Vec3 way, float footSpeed, const Config& c = {}) {
    if (!finite(way) || length(way) < 1e-4f)
        return {};
    return normalized(way) * std::min(c.flightMax, std::max(footSpeed, 0.f) * c.flightShare) +
           Vec3{0, c.flightUp, 0};
}

// ---- the timeline ----
inline float extendOf(Gesture g, const Config& c = {}) {
    return g == Gesture::roundhouse ? c.roundhouseExtend
           : g == Gesture::freeform ? c.freeformExtend
           : g == Gesture::sweep    ? c.sweepExtend
                                    : c.extend;
}
// The part of the kick the foot can land a blow in, and all of it.
inline float windowOf(Gesture g, const Config& c = {}) {
    return extendOf(g, c) + c.hold;
}
inline float totalOf(Gesture g, const Config& c = {}) {
    return windowOf(g, c) + c.recover;
}
// How far out the kick is at `t` seconds: 0 to 1 on the way out, held, back to 0.
inline float amountAt(Gesture g, float t, const Config& c = {}) {
    if (!(t > 0))
        return 0;
    const float out = extendOf(g, c), window = windowOf(g, c);
    if (t <= window)
        return smoothstep(0, out, t);
    return 1.f - smoothstep(0, c.recover, t - window);
}
// The reach the foot has at `t` (a freeform blow's is shorter).
inline float reachAt(Gesture g, float t, const Config& c = {}) {
    if (g == Gesture::sweep)
        return c.sweepReach; // the foot is out from the first step: the sweep is round the pelvis
    const float full = (c.reach + c.lunge) * (g == Gesture::freeform ? c.freeformReach : 1.f);
    return g == Gesture::roundhouse ? full : full * smoothstep(0, extendOf(g, c), t);
}
// A roundhouse's foot comes in from the side (the yaw off the gaze, radians, signed as `side`).
inline float sweepYaw(Gesture g, int side, float t, const Config& c = {}) {
    if (g == Gesture::sweep) // a sweep: from one side through the front to the other
        return static_cast<float>(side) * c.sweepArc * .5f * deg * (1.f - 2.f * smoothstep(0, extendOf(g, c), t));
    if (g != Gesture::roundhouse)
        return 0;
    return static_cast<float>(side) * c.roundhouseArc * deg * (1.f - smoothstep(0, extendOf(g, c), t));
}

// ---- the legs' pose ----
// leg_control's angles: 0 the left leg, 1 the right; pitch forward +, side toward the body's left +, knee
// bend. `side`: -1 left, +1 right. `angle`: a freeform flick's (degrees from up).
inline leg_control::Legs legPose(Gesture g, int side, float angle, float t, const Config& c = {}) {
    leg_control::Legs out = leg_control::centrePose;
    const float a = amountAt(g, t, c);
    if (g == Gesture::none || g == Gesture::sweep || !(a > 0)) // a sweep is the ground's: no air pose
        return out;
    const int kick = side >= 0 ? 1 : 0, other = 1 - kick;
    const float outward = kick == 1 ? -1.f : 1.f; // a leg's side angle away from the body's middle
    switch (g) {
    case Gesture::doubleDive:
        for (int i = 0; i < 2; ++i)
            out[i] = {1.35f * a, (i == 1 ? -1.f : 1.f) * .12f * a, .12f * a};
        break;
    case Gesture::dive:
        out[kick] = {1.45f * a, outward * .1f * a, .1f * a};
        out[other] = {.6f * a, -outward * .08f * a, 1.0f * a};
        break;
    case Gesture::roundhouse: {
        // The leg on the side the circle began from sweeps in from there; the other tucks.
        const float sweep = 1.f - smoothstep(0, extendOf(g, c), t);
        out[kick] = {.95f * a, outward * .9f * sweep * a, .25f * a};
        out[other] = {.3f * a, -outward * .1f * a, .6f * a};
        break;
    }
    default:
        if (std::abs(angle) > c.sideDegrees) { // down: both legs back
            out[0] = {-.7f * a, .1f * a, .6f * a};
            out[1] = {-.7f * a, -.1f * a, .6f * a};
        } else { // sideways: the leg of that side kicks out to it
            out[kick] = {.35f * a, outward * .95f * a, .2f * a};
            out[other] = {.1f * a, 0.f, .3f * a};
        }
        break;
    }
    for (auto& leg : out) {
        leg.pitch = std::clamp(leg.pitch, leg_control::pitchMin, leg_control::pitchMax);
        leg.side = std::clamp(leg.side, -leg_control::sideMax, leg_control::sideMax);
        leg.knee = std::clamp(leg.knee, 0.f, leg_control::kneeMax);
    }
    return out;
}

// ---- the kick in the swing module ----
enum class Refusal : std::uint8_t { none, off, legs, grounded, wall, busy, noGesture, moving, groundOnly };
inline const char* refusalName(Refusal r) {
    switch (r) {
    case Refusal::off:
        return "off";
    case Refusal::legs:
        return "legs not engaged";
    case Refusal::grounded:
        return "on the ground";
    case Refusal::wall:
        return "on a wall";
    case Refusal::busy:
        return "a kick is under way";
    case Refusal::noGesture:
        return "no gesture";
    case Refusal::moving:
        return "moving too fast";
    case Refusal::groundOnly:
        return "a sweep is for the ground";
    default:
        return "none";
    }
}
class Kick {
  public:
    enum class Phase : std::uint8_t { idle, kicking, recovering };
    enum class Event : std::uint8_t { none, started, hit, miss, refused };
    struct Target {
        std::uint64_t id{};
        Vec3 feet{};
        float height = 1.8f, radius = .3f;
        // ENEMY JOINTS (R1): his joints in the world when they were read (valid is 0 otherwise: the bands).
        enemy_joints::Joints joints{};
    };
    struct Frame {
        float seconds{}; // real
        // enabled: AIR KICKS is on; ground: GROUND KICKS is on (a kick off the ground needs the first, on it the
        // second); speed: the player's, m/s.
        bool focused = true, enabled = true, legs = true, grounded{}, wall{}, ground{};
        float speed{};
        Vec3 feet{};                // the player's
        Vec3 eyeOffset{0, 1.6f, 0}; // eyes from the feet, world axes
        Vec3 gaze{0, 0, -1};
        std::span<const Target> targets{};
        // KICK LOCK TARGET (Nov 19 log: an enemy at 1.9 m, the punches locked on him, the air kick target=0): the enemy
        // the punches are locked on (game_punch::kickTarget, as the swing kick's), 0 none.
        std::uint64_t locked{};
    };
    struct Output {
        Event event{};
        Refusal refusal{};
        Gesture gesture{};
        int side{};
        // On a hit: the one hit, the zone, the foot's speed and the blow's strength, where, which way, his
        // flight.
        std::uint64_t target{};
        Zone zone = Zone::chest;
        float foot{}, strength{};
        bool fly{}, lost{};
        bool byJoints{}; // the zone (the hit's, or the one asked for) came from his joints, not the bands
        Vec3 point{}, way{}, enemy{};
    };
    explicit Kick(Config c = {}) : config_(c) {}
    const Config& config() const {
        return config_;
    }
    void configure(const Config& c) {
        config_ = c;
    }
    Phase phase() const {
        return phase_;
    }
    bool busy() const {
        return phase_ != Phase::idle;
    }
    Gesture gesture() const {
        return gesture_;
    }
    int side() const {
        return side_;
    }
    // The running kick began standing (GROUND KICKS).
    bool onGround() const {
        return startedGrounded_;
    }
    float angle() const {
        return angle_;
    }
    float elapsed() const {
        return t_;
    }
    // 0..1 through the whole kick.
    float progress() const {
        return gesture_ == Gesture::none ? 0.f : std::clamp(t_ / totalOf(gesture_, config_), 0.f, 1.f);
    }
    Zone zone() const {
        return zone_;
    }
    std::uint64_t target() const {
        return target_;
    }
    // KICK TARGET FALLBACK: the target is the nearest in front, none being in the gaze cone.
    bool fallback() const {
        return fallback_;
    }
    // KICK LOCK TARGET: the fallback's target is the punches' lock.
    bool lockAimed() const {
        return lockAimed_;
    }
    Vec3 foot() const {
        return lastFoot_;
    }
    // A gesture: the kick starts (event started) or is refused.
    Output ask(const Frame& f, Flick flick) {
        Output out;
        out.gesture = flick.gesture;
        out.side = flick.side;
        const auto refuse = [&](Refusal r) {
            out.event = Event::refused;
            out.refusal = r;
            return out;
        };
        // Off the ground AIR KICKS' setting rules, on it GROUND KICKS'.
        if (f.grounded ? !f.ground : !f.enabled)
            return refuse(f.grounded ? Refusal::grounded : Refusal::off);
        if (flick.gesture == Gesture::none)
            return refuse(Refusal::noGesture);
        if (!f.legs)
            return refuse(Refusal::legs);
        if (!f.grounded && flick.gesture == Gesture::sweep)
            return refuse(Refusal::groundOnly);
        if (f.grounded && !(f.speed <= config_.groundSpeed)) // (also a speed that is not a number)
            return refuse(Refusal::moving);
        if (f.wall)
            return refuse(Refusal::wall);
        if (busy() || since_ < config_.cooldown)
            return refuse(Refusal::busy);
        const Config c = f.grounded ? standingConfig(config_) : config_;
        startedGrounded_ = f.grounded;
        gesture_ = flick.gesture;
        side_ = flick.side >= 0 ? 1 : -1;
        angle_ = flick.angle;
        t_ = 0;
        footSet_ = false;
        hit_ = false;
        phase_ = Phase::kicking;
        gaze_ = finite(f.gaze) && length(f.gaze) > .5f ? normalized(f.gaze) : Vec3{0, 0, -1};
        eye_ = f.feet + f.eyeOffset;
        // The gaze picks the enemy and his zone.
        target_ = 0;
        fallback_ = false;
        zone_ = Zone::chest;
        float best = 0;
        const Vec3 pelvis = f.feet + Vec3{0, c.hipHeight, 0};
        std::uint32_t zones = zonesFor(gesture_);
        if (c.limbZones && zones == 7u)
            zones |= 8u;
        bool askedByJoints = false;
        for (const auto& t : f.targets) {
            for (int z = 0; z < (c.limbZones ? 4 : 3); ++z) {
                if (!(zones & (1u << z)))
                    continue;
                // The arms are a joint zone only: an enemy whose forearms were not read has none.
                if (z == static_cast<int>(Zone::arms) && !enemy_joints::hasPart(t.joints, enemy_joints::partArms, true))
                    continue;
                bool byJoints = false;
                const Vec3 p = zonePoint(t.feet, t.height, static_cast<Zone>(z), t.joints, c.limbZones, gazeRef(t), &byJoints);
                if (length(p - pelvis) > (gesture_ == Gesture::sweep ? c.sweepReach + c.footRadius : c.reach + c.lunge) +
                                             t.radius)
                    continue;
                float w = gazeWeight(gaze_, eye_, p, c);
                if (gesture_ == Gesture::sweep) {
                    // A sweep is low: the gaze is not on the legs. The nearest in the half in front.
                    Vec3 ahead{gaze_.x, 0, gaze_.z}, to{p.x - pelvis.x, 0, p.z - pelvis.z};
                    const float la = length(ahead), lt = length(to);
                    w = la > 1e-3f && (lt < 1e-3f || dot(ahead / la, to / lt) >= 0)
                            ? std::max(.01f, 1.f - length(p - pelvis) / (c.sweepReach + c.footRadius + t.radius + 1.f))
                            : 0.f;
                }
                if (w > best) {
                    best = w;
                    target_ = t.id;
                    zone_ = static_cast<Zone>(z);
                    askedByJoints = byJoints;
                }
            }
        }
        // KICK TARGET FALLBACK: none in the gaze cone - the nearest in front within reach (and the slack).
        if (target_ == 0 && gesture_ != Gesture::sweep) {
            Vec3 aim = gaze_;
            const Vec3 ahead{gaze_.x, 0, gaze_.z};
            const float la = length(ahead);
            float nearest = 1e9f;
            for (const auto& t : f.targets) {
                const Zone z = zonesFor(gesture_) & (1u << static_cast<int>(Zone::chest)) ? Zone::chest
                               : zonesFor(gesture_) & (1u << static_cast<int>(Zone::head)) ? Zone::head
                                                                                          : Zone::legs;
                bool byJoints = false;
                const Vec3 p = zonePoint(t.feet, t.height, z, t.joints, c.limbZones, gazeRef(t), &byJoints);
                const float d = length(p - pelvis);
                if (!(d <= c.reach + c.lunge + t.radius + c.frontSlack) || d >= nearest)
                    continue;
                const Vec3 to{p.x - pelvis.x, 0, p.z - pelvis.z};
                const float lt = length(to);
                if (la > 1e-3f && lt > 1e-3f && dot(ahead / la, to / lt) < std::cos(c.frontDegrees * deg))
                    continue;
                nearest = d;
                target_ = t.id;
                zone_ = z;
                askedByJoints = byJoints;
                fallback_ = true;
                // The kick goes his way: its direction is the line to him (the gaze's height kept out).
                const Vec3 line = p - eye_;
                if (finite(line) && length(line) > 1e-3f)
                    aim = normalized(line);
            }
            if (fallback_)
                gaze_ = aim;
        }
        // KICK LOCK TARGET: none in the gaze or in front - the enemy the punches are locked on, any side, if the leg
        // reaches him (the lunge and the slack on top). The kick goes his way.
        lockAimed_ = false;
        if (target_ == 0 && f.locked != 0 && gesture_ != Gesture::sweep) {
            for (const auto& t : f.targets) {
                if (t.id != f.locked)
                    continue;
                const Zone z = zonesFor(gesture_) & (1u << static_cast<int>(Zone::chest)) ? Zone::chest
                               : zonesFor(gesture_) & (1u << static_cast<int>(Zone::head)) ? Zone::head
                                                                                          : Zone::legs;
                bool byJoints = false;
                const Vec3 p = zonePoint(t.feet, t.height, z, t.joints, c.limbZones, gazeRef(t), &byJoints);
                if (!(length(p - pelvis) <= c.reach + c.lunge + t.radius + c.frontSlack))
                    break;
                target_ = t.id;
                zone_ = z;
                askedByJoints = byJoints;
                fallback_ = lockAimed_ = true;
                const Vec3 line = p - eye_;
                if (finite(line) && length(line) > 1e-3f)
                    gaze_ = normalized(line);
                break;
            }
        }
        out.event = Event::started;
        out.target = target_;
        out.zone = zone_;
        out.byJoints = askedByJoints && target_ != 0;
        return out;
    }
    // One step.
    Output update(const Frame& f) {
        Output out;
        out.gesture = gesture_;
        out.side = side_;
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        if (phase_ == Phase::idle) {
            since_ += dt;
            return out;
        }
        t_ += dt;
        if (!(f.grounded ? f.ground : f.enabled) || !f.focused || f.grounded != startedGrounded_ || f.wall ||
            (f.grounded && f.speed > config_.groundSpeed + config_.groundSlack)) {
            // The air, the ground or the setting is gone (or a standing kick's player runs): the kick ends
            // where it is, nothing snaps (the legs ease back).
            out.event = hit_ ? Event::none : Event::miss;
            out.lost = true;
            finish();
            return out;
        }
        const Config c = startedGrounded_ ? standingConfig(config_) : config_;
        const float window = windowOf(gesture_, c), total = totalOf(gesture_, c);
        const Vec3 pelvis = f.feet + Vec3{0, c.hipHeight, 0};
        if (phase_ == Phase::kicking) {
            const Target* chosen{};
            for (const auto& t : f.targets)
                if (t.id == target_)
                    chosen = &t;
            const Vec3 yawed = turned(gaze_, sweepYaw(gesture_, side_, t_, c));
            Vec3 foot = pelvis + yawed * reachAt(gesture_, t_, c);
            if (gesture_ == Gesture::sweep)
                foot = Vec3{foot.x, f.feet.y + c.sweepHeight, foot.z}; // low: the sweep takes the legs
            if (chosen && gesture_ != Gesture::roundhouse && gesture_ != Gesture::sweep) {
                const Vec3 p = zonePoint(chosen->feet, chosen->height, zone_, chosen->joints, c.limbZones, gazeRef(*chosen));
                foot = bentFoot(pelvis, foot, p, gazeWeight(gaze_, eye_, p, c), c);
            }
            const Vec3 from = footSet_ ? lastFoot_ : foot;
            lastFoot_ = foot;
            footSet_ = true;
            const float speed = dt > 1e-4f ? length(foot - from) / dt : 0.f;
            out.foot = speed;
            if (!hit_ && speed >= c.minFoot) {
                const Target* met{};
                for (const auto& t : f.targets)
                    if (swing_kick::footHits(from, foot, config_.footRadius, t.feet, t.height, t.radius) &&
                        (!met || t.id == target_))
                        met = &t;
                if (met) {
                    hit_ = true;
                    out.event = Event::hit;
                    out.target = met->id;
                    const Vec3 way = length(foot - from) > 1e-4f ? normalized(foot - from) : gaze_;
                    out.way = way;
                    out.point = foot;
                    out.zone = zoneOfHit(met->feet, met->height, foot, met->joints, c.limbZones, &out.byJoints);
                    out.strength = strengthOf(speed, c);
                    out.fly = flies(out.strength, c);
                    out.enemy = !out.fly ? Vec3{}
                                : startedGrounded_ ? groundFlight(way, speed, c)
                                                   : enemyFlight(way, speed, c);
                    t_ = std::max(t_, window); // a landed kick holds, then comes back
                }
            }
            if (t_ >= window) {
                phase_ = Phase::recovering;
                if (!hit_ && out.event == Event::none)
                    out.event = Event::miss;
            }
        } else if (t_ >= total) {
            finish();
        }
        return out;
    }
    void reset() {
        *this = Kick(config_);
    }

  private:
    // Where the gaze passes his middle: which of his two arms or legs a limb zone means (the nearer to it).
    Vec3 gazeRef(const Target& t) const {
        const Vec3 mid = t.feet + Vec3{0, t.height * .5f, 0};
        return eye_ + gaze_ * std::max(0.f, dot(mid - eye_, gaze_));
    }
    static Vec3 turned(Vec3 v, float yaw) {
        const float c = std::cos(yaw), s = std::sin(yaw);
        return {v.x * c - v.z * s, v.y, v.x * s + v.z * c}; // + yaw turns toward +x, the right
    }
    void finish() {
        phase_ = Phase::idle;
        since_ = 0;
        t_ = 0;
        gesture_ = Gesture::none;
    }
    Config config_;
    Phase phase_{};
    Gesture gesture_{};
    int side_{};
    float angle_{}, t_{}, since_ = 1e3f;
    Zone zone_ = Zone::chest;
    std::uint64_t target_{};
    Vec3 gaze_{0, 0, -1}, eye_{}, lastFoot_{};
    bool footSet_{}, hit_{}, startedGrounded_{}, fallback_{}, lockAimed_{};
};
} // namespace spidy::air_kick
