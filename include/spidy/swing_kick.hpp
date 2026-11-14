#pragma once
// MANUAL SWING KICK (K1; the plan's Pillar 4; swing_kick_manual, off until set, EXPERIMENTAL).
//
// While swinging on a web with the legs engaged (Y + X, leg_hold.hpp), a flick of the right stick up
// (under 0.3 to over 0.8 within 80 ms) sends the legs out at the enemy the ring marks, if he is 3 to 14 m
// away, the swing is at least 9 m/s and he sits within 30 degrees of the swing's VELOCITY (not the view:
// it cannot pull the player sideways or backwards). A slow push stays the jackknife pose.
//
// What the kick does (all of it small, none of it adds speed):
//  - Rope assist: each web's length moves toward |anchor - enemy| by at most 1.2 m in all and 2 m/s
//    (length_new = length + clamp(|A - P| - length, +-1.2 m)).
//  - Side steering: the velocity turns toward him at 2.5 m/s^2 at most, as the swing's air acceleration
//    does; the turn keeps |v|, so the assist never raises the speed.
//  - Impact: the foot's swept 10 cm sphere meets his capsule: a kinetic blow, his flight 70 % of the
//    player's velocity, the player keeps 60 % and swings on.
//  - A miss (he is passed, gone, the web let go, 1.6 s): the assist fades over 0.35 s, the legs return,
//    nothing snaps.
// Only the ringed enemy counts, as with the auto dive (WebStrike::dive), which stays its own setting
// (swing_kick_auto); the two can both be on, and the dive holds back while a manual kick is under way.
//
// Engine independent. The XR worker detects the flick (Flick) and asks (game_swing::SwingKickAsk); the
// swing module arms, steers and lands it (Kick, Swing::kickAssist, game_punch::kickBlow). No game address
// is used: the target and the blow are the ones WebStrike and game_punch already use.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::swing_kick {
struct Config {
    float coneDegrees = 30.f;
    float minDistance = 3.f, maxDistance = 14.f;
    float minSpeed = 9.f;
    // The flick: the stick's size from under flickLow to over flickHigh within flickSeconds, its push
    // within upDegrees of straight up.
    float flickLow = .3f, flickHigh = .8f, flickSeconds = .08f, upDegrees = 50.f;
    // The rope assist: at most ropeMetres in all, ropeRate m/s; the steering at most steer m/s^2.
    float ropeMetres = 1.2f, ropeRate = 2.f, steer = 2.5f;
    // A miss's fade, and the longest a kick stays armed.
    float fade = .35f, longest = 1.6f;
    // The foot: a sphere of footRadius on the leg reaching legReach from the pelvis (hipHeight over the
    // feet) toward him.
    float footRadius = .1f, legReach = .95f, hipHeight = .9f;
    // What the blow gives his flight, and what the player keeps.
    float transfer = .7f, keep = .6f;
};
inline constexpr float pi = 3.14159265f;

enum class Refusal : std::uint8_t {
    none,
    off,         // swing_kick_manual is off
    legs,        // the legs are not engaged
    notSwinging, // no web holds, on the ground or a wall
    noTarget,    // nobody ringed
    tooClose,
    tooFar,
    tooSlow,
    outsideCone, // not within the cone of the velocity
};
inline const char* refusalName(Refusal r) {
    switch (r) {
    case Refusal::none:
        return "none";
    case Refusal::off:
        return "off";
    case Refusal::legs:
        return "legs not engaged";
    case Refusal::notSwinging:
        return "not swinging";
    case Refusal::noTarget:
        return "nobody ringed";
    case Refusal::tooClose:
        return "too close";
    case Refusal::tooFar:
        return "too far";
    case Refusal::tooSlow:
        return "too slow";
    case Refusal::outsideCone:
        return "outside the cone";
    }
    return "?";
}

// The cosine between the swing's velocity and the line to him; 0 if either is nothing.
inline float coneDot(Vec3 velocity, Vec3 toTarget) {
    const Vec3 v = normalized(velocity), p = normalized(toTarget);
    return finite(v) && finite(p) ? dot(v, p) : 0.f;
}
inline float coneDegrees(Vec3 velocity, Vec3 toTarget) {
    return std::acos(std::clamp(coneDot(velocity, toTarget), -1.f, 1.f)) * (180.f / pi);
}
inline bool inCone(Vec3 velocity, Vec3 toTarget, float degrees) {
    return coneDot(velocity, toTarget) >= std::cos(std::clamp(degrees, 1.f, 89.f) * (pi / 180.f)) - 1e-6f;
}

struct ArmInput {
    bool enabled{}, legs{}, swinging{}, target{};
    float speed{};    // the player's, real m/s
    float distance{}; // to the enemy, metres
    float cone{};     // coneDot()
};
// Why the kick may not arm, or none.
inline Refusal armCheck(const ArmInput& in, const Config& c = {}) {
    if (!in.enabled)
        return Refusal::off;
    if (!in.legs)
        return Refusal::legs;
    if (!in.swinging)
        return Refusal::notSwinging;
    if (!in.target)
        return Refusal::noTarget;
    if (!(in.distance >= c.minDistance))
        return Refusal::tooClose;
    if (!(in.distance <= c.maxDistance))
        return Refusal::tooFar;
    if (!(in.speed >= c.minSpeed))
        return Refusal::tooSlow;
    if (!(in.cone >= std::cos(c.coneDegrees * (pi / 180.f)) - 1e-6f))
        return Refusal::outsideCone;
    return Refusal::none;
}

// The right stick's flick (XR side). Feed every sample; true once on the sample that completes one.
class Flick {
  public:
    // x right, y up (the controller's). engaged: the legs are engaged and the setting is on.
    bool update(float seconds, float x, float y, bool engaged, const Config& c = {}) {
        if (!engaged || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(seconds)) {
            armed_ = false;
            sinceLow_ = 1e3f;
            return false;
        }
        const float size = std::hypot(x, y);
        if (size < c.flickLow) {
            armed_ = true;
            sinceLow_ = 0;
            return false;
        }
        sinceLow_ += std::max(seconds, 0.f);
        if (size <= c.flickHigh || !armed_)
            return false;
        // Over the high mark: a flick if it came quickly and pushes up, otherwise a push (the legs' pose).
        armed_ = false;
        if (sinceLow_ > c.flickSeconds + 1e-6f)
            return false;
        const float fromUp = std::atan2(std::abs(x), y) * (180.f / pi);
        return y > 0 && fromUp <= c.upDegrees;
    }
    void reset() {
        armed_ = false;
        sinceLow_ = 1e3f;
    }

  private:
    bool armed_{};
    float sinceLow_ = 1e3f;
};

// The rope's change this step toward |A - P| (the enemy's distance from the web's anchor), within
// +-ropeMetres of the length it had when the kick armed (`start`), at ropeRate m/s, scaled by the assist's
// weight. `length` is the rope's now; the result is its new length (never under minRope).
inline float ropeLength(float start, float length, float anchorToTarget, float weight, float seconds,
                        float minRope, const Config& c = {}) {
    if (!std::isfinite(anchorToTarget) || !std::isfinite(length) || !std::isfinite(start) || weight <= 0)
        return length;
    const float wanted = start + std::clamp(anchorToTarget - start, -c.ropeMetres, c.ropeMetres);
    const float step = c.ropeRate * std::clamp(weight, 0.f, 1.f) * std::max(seconds, 0.f);
    return std::max(std::min(minRope, length), length + std::clamp(wanted - length, -step, step));
}

// The velocity turned toward the enemy by the side steering: at most `steer` m/s^2 across its direction,
// weighted, and its length kept (the assist never raises the speed). accel: what was used, m/s^2.
inline Vec3 steered(Vec3 velocity, Vec3 toTarget, float weight, float seconds, const Config& c,
                    float* accel) {
    if (accel)
        *accel = 0;
    const float speed = length(velocity);
    if (!(speed > .5f) || !finite(toTarget) || weight <= 0 || !(seconds > 0))
        return velocity;
    const Vec3 along = velocity / speed;
    Vec3 across = toTarget - along * dot(toTarget, along);
    const float off = length(across);
    if (off < 1e-4f)
        return velocity;
    across = across / off;
    const float a = c.steer * std::clamp(weight, 0.f, 1.f);
    // Not past the line to him: a turn that large would overshoot it.
    const float need = off / std::max(length(toTarget), 1e-3f) * speed / std::max(seconds, 1e-3f);
    const float used = std::min(a, need);
    Vec3 turned = velocity + across * (used * seconds);
    const float n = length(turned);
    if (n < 1e-4f)
        return velocity;
    turned = turned * (speed / n); // |v| kept
    if (accel)
        *accel = length(turned - velocity) / seconds;
    return turned;
}

// Segment-to-segment distance (the closest approach of two lines of a limited length).
inline float segmentDistance(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    const Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    float s{}, t{};
    constexpr float eps = 1e-9f;
    if (a <= eps && e <= eps)
        return length(r);
    if (a <= eps) {
        t = std::clamp(f / e, 0.f, 1.f);
    } else {
        const float cc = dot(d1, r);
        if (e <= eps) {
            s = std::clamp(-cc / a, 0.f, 1.f);
        } else {
            const float b = dot(d1, d2), denom = a * e - b * b;
            s = denom > eps ? std::clamp((b * f - cc * e) / denom, 0.f, 1.f) : 0.f;
            t = (b * s + f) / e;
            if (t < 0) {
                t = 0;
                s = std::clamp(-cc / a, 0.f, 1.f);
            } else if (t > 1) {
                t = 1;
                s = std::clamp((b - cc) / a, 0.f, 1.f);
            }
        }
    }
    return length((p1 + d1 * s) - (p2 + d2 * t));
}
// The foot's swept sphere (from `from` to `to`) meets the enemy's capsule (feet, height, radius).
inline bool footHits(Vec3 from, Vec3 to, float footRadius, Vec3 feet, float height, float radius) {
    if (!finite(from) || !finite(to) || !finite(feet))
        return false;
    const float r = std::max(radius, .05f);
    const Vec3 bottom = feet + Vec3{0, std::min(r, height * .5f), 0};
    const Vec3 top = feet + Vec3{0, std::max(height - r, std::min(r, height * .5f)), 0};
    return segmentDistance(from, to, bottom, top) <= footRadius + r;
}
// Where the foot is: the leg's reach from the pelvis toward the aim.
inline Vec3 footPoint(Vec3 feet, Vec3 aim, const Config& c = {}) {
    const Vec3 pelvis = feet + Vec3{0, c.hipHeight, 0};
    const Vec3 way = normalized(aim - pelvis);
    return pelvis + (length(way) > .5f ? way : Vec3{0, 0, -1}) * c.legReach;
}
inline Vec3 enemyVelocity(Vec3 playerVelocity, const Config& c = {}) {
    return playerVelocity * c.transfer;
}
inline Vec3 playerKeeps(Vec3 playerVelocity, const Config& c = {}) {
    return playerVelocity * c.keep;
}

// The kick's state in the swing module.
class Kick {
  public:
    enum class Phase : std::uint8_t { idle, active, fading };
    enum class Event : std::uint8_t { none, armed, hit, miss };
    struct Target {
        bool valid{};
        std::uint64_t id{};
        Vec3 feet{};
        float height = 1.8f, radius = .3f;
        Vec3 middle() const {
            return feet + Vec3{0, height * .6f, 0};
        }
    };
    struct Frame {
        float seconds{}; // real
        bool focused = true, swinging{}, legs = true, enabled = true;
        Vec3 feet{};     // the player's
        Vec3 velocity{}; // real m/s
        Target target{};
    };
    struct Output {
        Event event{};
        Refusal refusal{};
        // The assist this step: steer and shorten/lengthen toward `aim` at `weight` (0..1).
        bool assist{};
        Vec3 aim{};
        float weight{};
        // On a hit: the blow's flight for the enemy, and what the player keeps.
        Vec3 enemy{}, keep{};
        std::uint64_t target{};
        float cone{}, speed{}, distance{};
        bool lost{}; // a miss because the target or the web went
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
    bool active() const {
        return phase_ == Phase::active;
    }
    // The assist is still running (armed or fading).
    bool busy() const {
        return phase_ != Phase::idle;
    }
    std::uint64_t target() const {
        return target_;
    }
    Vec3 aim() const {
        return aim_;
    }
    // A flick asked: arm if the swing allows it.
    Output ask(const Frame& f) {
        Output out;
        ArmInput in;
        in.enabled = f.enabled;
        in.legs = f.legs;
        in.swinging = f.focused && f.swinging;
        in.target = f.target.valid;
        in.speed = length(f.velocity);
        const Vec3 pelvis = f.feet + Vec3{0, config_.hipHeight, 0};
        const Vec3 toTarget = f.target.middle() - pelvis;
        in.distance = f.target.valid ? length(toTarget) : 0.f;
        in.cone = coneDot(f.velocity, toTarget);
        out.refusal = armCheck(in, config_);
        out.target = f.target.id;
        out.cone = coneDegrees(f.velocity, toTarget);
        out.speed = in.speed;
        out.distance = in.distance;
        if (out.refusal != Refusal::none)
            return out;
        phase_ = Phase::active;
        target_ = f.target.id;
        aim_ = f.target.middle();
        age_ = 0;
        weight_ = 1;
        footSet_ = false;
        out.event = Event::armed;
        return out;
    }
    // One step of the swing.
    Output update(const Frame& f) {
        Output out;
        out.target = target_;
        const float dt = std::clamp(std::isfinite(f.seconds) ? f.seconds : 0.f, 0.f, .1f);
        if (phase_ == Phase::idle)
            return out;
        if (phase_ == Phase::fading) {
            weight_ -= dt / std::max(config_.fade, 1e-3f);
            if (weight_ <= 0 || !f.focused || !f.swinging) {
                phase_ = Phase::idle;
                weight_ = 0;
                return out;
            }
            out.assist = true;
            out.aim = aim_;
            out.weight = weight_;
            return out;
        }
        age_ += dt;
        const bool here = f.target.valid && f.target.id == target_;
        const Vec3 pelvis = f.feet + Vec3{0, config_.hipHeight, 0};
        if (here)
            aim_ = f.target.middle();
        const Vec3 toTarget = aim_ - pelvis;
        out.speed = length(f.velocity);
        out.distance = length(toTarget);
        out.cone = coneDegrees(f.velocity, toTarget);
        // The foot's sweep this step: from where it was (nowhere the first step: its own point).
        const Vec3 foot = footPoint(f.feet, aim_, config_);
        const Vec3 from = footSet_ ? lastFoot_ : foot;
        lastFoot_ = foot;
        footSet_ = true;
        if (here && f.focused && f.swinging &&
            footHits(from, foot, config_.footRadius, f.target.feet, f.target.height, f.target.radius)) {
            out.event = Event::hit;
            out.enemy = enemyVelocity(f.velocity, config_);
            out.keep = playerKeeps(f.velocity, config_);
            out.assist = true;
            out.aim = aim_;
            out.weight = 1;
            phase_ = Phase::fading;
            return out;
        }
        const bool passed = dot(f.velocity, toTarget) < 0 && out.distance > 1.f;
        if (!here || !f.focused || !f.swinging || passed || age_ > config_.longest) {
            out.event = Event::miss;
            out.lost = !here || !f.focused || !f.swinging;
            out.assist = f.focused && f.swinging;
            out.aim = aim_;
            out.weight = 1;
            phase_ = out.assist ? Phase::fading : Phase::idle;
            return out;
        }
        out.assist = true;
        out.aim = aim_;
        out.weight = 1;
        return out;
    }
    void reset() {
        phase_ = Phase::idle;
        weight_ = 0;
        target_ = 0;
    }

  private:
    Config config_;
    Phase phase_{};
    std::uint64_t target_{};
    Vec3 aim_{}, lastFoot_{};
    float age_{}, weight_{};
    bool footSet_{};
};
} // namespace spidy::swing_kick
