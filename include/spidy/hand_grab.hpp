#pragma once
// HAND GRAB (G2; the plan's Pillar 4; hand_grab, off until set, EXPERIMENTAL).
//
// With the palm closed on a prop or an enemy (the physical grip squeezed with the palm within 0.35 m of
// his capsule or the prop), the hand takes hold of him instead of shooting a web. Grip priority by
// distance: an enemy in reach, then a prop in reach, then a wall (G1), then a web.
//
// HOLD. The held body follows a point `ahead` of the palm along the aim (0.45 m for an enemy, so his chest
// is there; a prop's centre its radius and 0.15 m). The law is the critically damped spring
//     a = w^2 (x_hand - x) - 2 w (v - v_hand),   w = 18 rad/s,   F = m a,   |F| <= 1200 N per hand,
// carried by web_grab's Follow command (a rate-limited velocity servo: x_hand - x decays at w / 2, no
// overshoot, the same law the web tie uses, which also takes gravity out first, so the hand supports the
// weight). One hand lifts up to 90 kg (1200 N with a quarter of reserve for the motion); a heavier body
// lags and sags, and a body that outweighs the grip is dragged along the ground, not lifted. Two hands
// double the force. An enemy's mass comes from his capsule (a street thug 80 kg).
//
// HOLD COST. Holding an enemy tires the hand a little: the force fades from 100% toward 75% over
// 60 s x (80 kg / his mass)^2 / enemy_hold_cost. A standard enemy costs next to nothing (75% of 1200 N
// still lifts his 80 kg); a heavy one tires the grip fast, and a tired grip that no longer beats his weight
// lets him sag and slip away. 0 turns it off. Props cost nothing.
//
// PUNCH WHILE HELD. The free fist is the ordinary punch (kMelee, 2.2 to 7.5 m/s), which the game's punch
// already aims at a held enemy. The grip knocks him into his flight with a kinetic blow of 0.25 and again
// 0.8 s after the last blow (a punch's too), as the web tie does.
//
// THROW and SLAM. Letting go launches the body with the hand's velocity (the least-squares slope of the
// last five hand samples), times up to 1.6 (less for a body heavier than one hand lifts), at most 18 m/s;
// slower than 1.5 m/s it is only let go of. A hand moving down faster than 4 m/s lets go as a slam: at
// least 8 m/s along its way, so the landing is a strike (web_grab's StrikeConfig::landSpeed).
//
// Engine independent. WebGrab takes it (GrabConfig::hand); the game adapter (game_grab) supplies the
// bodies near the palm. No game address is used.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace spidy::hand_grab {
struct Config {
    bool enabled = false;
    // How near the palm must be to the body's surface (m), and where the held body's centre goes ahead
    // of it: an enemy's at `ahead`, a prop's at its radius + `propAhead`.
    float reach = .35f, ahead = .45f, propAhead = .15f;
    // One hand's force (N); what one hand lifts follows it (90 kg at 1200 N).
    float force = 1200.f;
    float liftPerNewton = .075f;
    // The hold's spring (rad/s), and when the hold breaks: the body this far (m) from his place.
    float omega = 18.f, shake = 1.5f;
    float gravity = 9.81f;
    // HOLD COST (enemy_hold_cost, 0 off): seconds to the floor for a standard enemy, the floor's share of
    // the force, and the standard enemy's mass (kg).
    float holdCost = 1.f, tireSeconds = 60.f, tiredFloor = .75f, standardMass = 80.f;
    // The kinetic blow that knocks the held enemy into the game's flight, and the wait before the next.
    float knockDamage = .25f, reknock = .8f;
    // Throw and slam.
    float throwGain = 1.6f, maxThrow = 18.f, minThrow = 1.5f, slamJerk = 4.f, slamStrike = 8.f;
    // The fastest the servo asks the held body to go (m/s).
    float maxSpeed = 25.f;
    // ENEMY CLASS RULES (hand_grab_class_rules, on; G2b, the user Nov 13 t17u). A heavy (a brute, or his
    // capsule's mass `heavyMass` kg and up) or a superpowered enemy (a boss) resists throws unless open for a
    // finisher: resistGain of the hand's speed, resistMax m/s at most, no slam. An armored (a shield guard)
    // or superpowered one tires the hand classCost times as fast (a heavy already does, by his mass). OPEN
    // for a finisher (the proxy: the game has him stunned, game_targets::isStunned, or webbed up): no cost, and
    // full manipulation - he weighs at most what one hand lifts and throws with the full gain.
    bool classRules = true;
    float heavyMass = 120.f, classCost = 3.f, resistGain = .25f, resistMax = 4.f;
};
inline bool valid(const Config& c) {
    const float positive[] = {c.reach,        c.ahead,       c.force,      c.liftPerNewton,
                              c.omega,        c.shake,       c.gravity,    c.tireSeconds,
                              c.standardMass, c.knockDamage, c.reknock,    c.throwGain,
                              c.maxThrow,     c.slamJerk,    c.slamStrike, c.maxSpeed};
    for (float v : positive)
        if (!std::isfinite(v) || v <= 0)
            return false;
    return std::isfinite(c.propAhead) && c.propAhead >= 0 && std::isfinite(c.holdCost) && c.holdCost >= 0 &&
           std::isfinite(c.tiredFloor) && c.tiredFloor > 0 && c.tiredFloor <= 1 &&
           std::isfinite(c.minThrow) && c.minThrow >= 0 && std::isfinite(c.heavyMass) && c.heavyMass > 0 &&
           std::isfinite(c.classCost) && c.classCost >= 1 && std::isfinite(c.resistGain) && c.resistGain > 0 &&
           c.resistGain <= c.throwGain && std::isfinite(c.resistMax) && c.resistMax > 0;
}
// The mass one hand lifts, kg, at the config's force.
inline float liftMass(const Config& c) {
    return c.force * c.liftPerNewton;
}
// How far the palm is from the surface of a sphere (m); 0 inside it.
inline float surfaceReach(Vec3 palm, Vec3 centre, float radius) {
    if (!finite(palm) || !finite(centre) || !std::isfinite(radius))
        return std::numeric_limits<float>::infinity();
    return std::max(0.f, length(palm - centre) - std::max(radius, 0.f));
}
// An enemy's mass (kg) from his capsule: a street thug's (0.4 to 1.6 m above his feet, radius 0.45) is 80,
// and it grows with his width squared and his height.
inline float enemyMass(float low, float high, float radius, const Config& c = {}) {
    if (!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(radius) || radius <= 0 || high < low)
        return c.standardMass;
    const float width = radius / .45f, height = (high - low + 2 * radius) / 2.1f;
    return std::clamp(c.standardMass * width * width * height, c.standardMass * .25f, c.standardMass * 6.f);
}
// Where the held body's centre goes: `ahead` of the palm along the aim for an enemy, its radius and
// propAhead for a prop.
inline float holdAhead(bool character, float radius, const Config& c = {}) {
    return character ? c.ahead : std::max(radius, 0.f) + c.propAhead;
}
// The share of the hand's force left after holding an enemy of `mass` kg for `seconds`.
inline float tiredness(float seconds, float mass, const Config& c = {}) {
    if (!(c.holdCost > 0) || !std::isfinite(seconds) || !std::isfinite(mass) || seconds <= 0)
        return 1.f;
    const float load = std::max(mass, 1.f) / c.standardMass;
    const float lost = c.holdCost * load * load * seconds / c.tireSeconds * (1.f - c.tiredFloor);
    return std::clamp(1.f - lost, c.tiredFloor, 1.f);
}
// The force the spring asks of the hand (N): F = m (w^2 e - 2 w v_rel), capped at `cap` newtons.
// `place` and `placeVelocity`: where the body should be and how that moves; gravity is not in it.
inline Vec3 springForce(Vec3 position, Vec3 velocity, Vec3 place, Vec3 placeVelocity, float mass, float cap,
                        const Config& c = {}) {
    const Vec3 f = ((place - position) * (c.omega * c.omega) - (velocity - placeVelocity) * (2 * c.omega)) *
                   std::max(mass, 1.f);
    return limited(f, cap);
}
// The velocity the hold asks of the body (web_grab's Follow): the hand's own, plus the error closing at
// w / 2 (no faster than it can stop in: the speed a brake of half of `acceleration` stops from over that
// distance, so a heavy body whose force is spent does not overshoot), and the gravity of one step taken
// out first (Follow adds it after the brake).
inline Vec3 holdVelocity(Vec3 position, Vec3 place, Vec3 placeVelocity, float dt, float acceleration,
                         const Config& c = {}) {
    const Vec3 error = place - position;
    const float distance = length(error);
    Vec3 closing{};
    if (distance > 1e-5f) {
        const float stop = std::sqrt(std::max(acceleration, 1.f) * distance);
        closing = error * (std::min(c.omega * .5f * distance, stop) / distance);
    }
    return limited(placeVelocity + closing, c.maxSpeed) + Vec3{0, c.gravity * std::max(dt, 0.f), 0};
}
// The most the hold changes the body's velocity per second: `hands` hands at `tired` of their force, over
// the mass.
inline float holdAcceleration(float mass, int hands, float tired, const Config& c = {}) {
    return std::max(hands, 1) * c.force * std::clamp(tired, 0.f, 1.f) / std::max(mass, 1.f);
}
// Whether `hands` hands at `tired` lift `mass` kg off the ground: the force beats the weight.
inline bool lifts(float mass, int hands, float tired, const Config& c = {}) {
    return holdAcceleration(mass, hands, tired, c) > c.gravity;
}

// The last few hand samples, for the release velocity: the slope of a least-squares line through each
// axis against time. Five samples (about 55 ms at 90 Hz) smooth a tracker's jitter and keep a flick.
class Samples {
  public:
    static constexpr int count = 5;
    void clear() {
        n_ = 0;
        next_ = 0;
    }
    // One sample, `seconds` after the one before (a new run starts with the first).
    void push(float seconds, Vec3 position) {
        if (!finite(position) || !std::isfinite(seconds) || seconds < 0 || seconds > .1f) {
            clear();
            return;
        }
        clock_ = n_ ? clock_ + seconds : 0.f;
        p_[next_] = position;
        t_[next_] = clock_;
        next_ = (next_ + 1) % count;
        n_ = std::min(n_ + 1, count);
    }
    int size() const {
        return n_;
    }
    // Metres per second; zero with fewer than two samples or a span under a millisecond.
    Vec3 velocity() const {
        if (n_ < 2)
            return {};
        float tm = 0;
        Vec3 pm{};
        for (int i = 0; i < n_; ++i) {
            tm += t_[i];
            pm += p_[i];
        }
        tm /= static_cast<float>(n_);
        pm = pm / static_cast<float>(n_);
        float den = 0;
        Vec3 num{};
        for (int i = 0; i < n_; ++i) {
            const float dt = t_[i] - tm;
            den += dt * dt;
            num += (p_[i] - pm) * dt;
        }
        return den > 1e-8f ? num / den : Vec3{};
    }

  private:
    std::array<Vec3, count> p_{};
    std::array<float, count> t_{};
    int n_{}, next_{};
    float clock_{};
};

struct Throw {
    Vec3 velocity{};
    bool slam{};
    bool thrown{}; // faster than minThrow: otherwise it is only let go of, at the hand's velocity
};
// The launch of a body let go of by a hand moving at `hand` (m/s): the hand's velocity times a gain of
// up to throwGain, less for a body heavier than one hand lifts, at most maxThrow; a slam when the hand
// moves down faster than slamJerk, at least slamStrike along its way.
inline Throw release(Vec3 hand, float mass, const Config& c = {}) {
    Throw out;
    if (!finite(hand))
        return out;
    const float speed = length(hand);
    if (speed < c.minThrow) {
        out.velocity = hand;
        return out;
    }
    out.thrown = true;
    const float gain = 1.f + (c.throwGain - 1.f) * std::clamp(liftMass(c) / std::max(mass, 1.f), 0.f, 1.f);
    out.velocity = limited(hand * gain, c.maxThrow);
    if (hand.y < -c.slamJerk) {
        out.slam = true;
        if (length(out.velocity) < c.slamStrike)
            out.velocity = normalized(hand) * std::min(c.slamStrike, c.maxThrow);
    }
    return out;
}

// ENEMY CLASS RULES (G2b; Config::classRules). What an enemy is, for the hold's cost and the throw only:
// never his own attributes (those are the game's and other modders'), never his damage.
enum class Foe : std::uint8_t { standard, heavy, armored, super };
inline const char* foeName(Foe f) {
    switch (f) {
    case Foe::standard: return "standard";
    case Foe::heavy: return "heavy";
    case Foe::armored: return "armored";
    case Foe::super: return "superpowered";
    }
    return "?";
}
// From what game_targets already reads (no new class name, no address): boss, a BossHealth (trait boss);
// brute, the guard's Brawler kind (a brute's guard); armored, the guard's Shield kind (a shield or armour
// that blocks); heavy, a brute or his capsule's mass (enemyMass) at heavyMass or more.
inline Foe classify(float mass, bool armored, bool boss, bool brute = false, const Config& c = {}) {
    if (boss)
        return Foe::super;
    if (brute || (std::isfinite(mass) && mass >= c.heavyMass))
        return Foe::heavy;
    if (armored)
        return Foe::armored;
    return Foe::standard;
}
// A heavy or a superpowered enemy not open for a finisher resists the throw.
inline bool resists(Foe f, bool open, const Config& c = {}) {
    return c.classRules && !open && (f == Foe::heavy || f == Foe::super);
}
// How much faster than a standard enemy he tires the hand (0: open, no cost).
inline float costScale(Foe f, bool open, const Config& c = {}) {
    if (!c.classRules)
        return 1.f;
    if (open)
        return 0.f;
    return f == Foe::armored || f == Foe::super ? c.classCost : 1.f;
}
// The mass the hold and the throw work with: open for a finisher, at most what one hand lifts.
inline float handledMass(float mass, bool open, const Config& c = {}) {
    return c.classRules && open ? std::min(mass, liftMass(c)) : mass;
}
inline float tirednessOf(float seconds, float mass, Foe f, bool open, const Config& c = {}) {
    Config k = c;
    k.holdCost = c.holdCost * costScale(f, open, c);
    return tiredness(seconds, handledMass(mass, open, c), k);
}
struct ClassThrow : Throw {
    bool resisted{};
};
inline ClassThrow releaseOf(Vec3 hand, float mass, Foe f, bool open, const Config& c = {}) {
    ClassThrow out;
    if (!resists(f, open, c)) {
        static_cast<Throw&>(out) = release(hand, handledMass(mass, open, c), c);
        return out;
    }
    out.resisted = true;
    if (!finite(hand))
        return out;
    const float speed = length(hand);
    if (speed < c.minThrow) {
        out.velocity = hand;
        return out;
    }
    out.thrown = true;
    out.velocity = limited(hand * c.resistGain, std::min(c.resistMax, c.maxThrow));
    return out;
}
// HandGrabLive::lastKind: bits 0-7 what (1 enemy, 2 prop), 8-11 his Foe, bit 12 open, bit 13 resisted.
inline std::uint32_t kindBits(bool character, Foe f, bool open, bool resisted) {
    if (!character)
        return 2u;
    return 1u | static_cast<std::uint32_t>(f) << 8 | (open ? 1u << 12 : 0u) | (resisted ? 1u << 13 : 0u);
}
} // namespace spidy::hand_grab
