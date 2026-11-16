#pragma once
// PHYSICAL WEB TETHERS (W1; the plan's Pillar 3; physical_tethers, off until set, EXPERIMENTAL).
//
// The web grab already behaves like one straight rope segment (web_grab.hpp). A tether is a simulated rope
// laid along it, one per arm (two in all), so a held line sags, drapes over edges, whips and wraps, and the
// hand's motion can do three things the straight rope could not:
//   Whip   - snap the hand and the wave runs down the line and jerks the target with the speed it arrives with.
//   Spin   - circle the hand and the target orbits, gaining tangential speed from the hand (at most 18 m/s);
//            let go to sling it (the throw is the grab's own: the target's velocity relative to the player).
//   Throw on a tether - let go of the grip with the trigger held and the line pays out after the prop: the
//            trigger's depth is the brake, a full trigger locks the length, letting go of the trigger cuts it.
//
// The rope: 24 particles, 8 substeps a frame (Small Steps XPBD): a unilateral distance constraint per
// segment (a rope pulls, it does not push) with compliance about 1e-7, a long-range attachment to the hand so
// it never stretches like rubber, the hand end pinned to the tracked wrist and the far end to the target each
// substep. Collision: spheres against planes cached from rays and against enemy capsules; bends are saved as
// the drawn points. It sleeps when still. The game's own rope is a canned straight line (C0 findings), so it
// has no drape to reuse; the mod draws this one through the game's rope by laying its bends as the hand
// points the game rope takes (native_webs::Bent: at most 6, a rope past 8 points crashed the game).
//
// Engine independent; no game address. The web grab runs it (WebGrab::step); XR draws it (game_swing's
// TetherLive).
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace spidy::tether {
inline constexpr int maxParticles = 32;
// The points a rope is drawn through, past its two ends (native_webs::maxRopeBends).
inline constexpr int maxDrawn = 6;

struct Config {
    bool enabled = false;
    int particles = 24, substeps = 8;
    // The distance constraint's compliance (XPBD alpha), m/N: about 1e-7.
    float compliance = 1e-7f;
    float gravity = 9.81f;
    // The rope's own velocity damping, per second, and a particle's mass (kg: only the ratio to the pinned ends
    // matters, which are heavy).
    float damping = 2.f, particleMass = .02f;
    // Collision: the sphere's radius (the line's thickness), how long a cached plane lives, how many rays a
    // step may cast (round robin over the segments and the particles), how many planes are kept.
    float radius = .03f, planeSeconds = .6f;
    int raysPerStep = 8, maxPlanes = 12;
    // Asleep once the ends and every particle move under sleepSpeed for sleepSeconds; woken by a move of
    // wakeDistance of an end.
    float sleepSpeed = .05f, sleepSeconds = .5f, wakeDistance = .01f;
    // Whip: the hand's speed within whipWindow, the wave's speed arriving at the target, the share of it the
    // jerk gives, the most, over how long a force takes it, and the pause between whips.
    float whipHandSpeed = 4.f, whipArrival = 3.f, whipShare = .9f, whipMax = 14.f, whipWindow = .15f,
          whipImpulseSeconds = .15f, whipCooldown = .4f;
    // Spin: the hand's circling rate (rad/s) it takes, the hand's least speed, how long it has to last, how fast
    // the orbit builds, and the most the target's speed goes to.
    float spinRate = 3.f, spinHandSpeed = 1.f, spinSeconds = .3f, spinRamp = .5f, spinMax = 18.f;
    // Throw on a tether: the trigger depth where the line starts to pay out (less cuts it) and where it locks.
    float payoutTrigger = .1f, lockTrigger = .95f;
};
inline bool valid(const Config& c) {
    return c.particles >= 8 && c.particles <= maxParticles && c.substeps >= 2 && c.substeps <= 16 &&
           std::isfinite(c.compliance) && c.compliance >= 0 && c.compliance <= 1e-3f && std::isfinite(c.gravity) &&
           std::isfinite(c.damping) && c.damping >= 0 && std::isfinite(c.particleMass) && c.particleMass > 1e-4f &&
           std::isfinite(c.radius) && c.radius >= 0 && c.radius < .5f && c.planeSeconds > 0 && c.raysPerStep >= 0 &&
           c.raysPerStep <= 64 && c.maxPlanes >= 1 && c.maxPlanes <= 64 && c.sleepSeconds > 0 && c.whipMax > 0 &&
           c.spinMax > 0 && c.payoutTrigger >= 0 && c.lockTrigger > c.payoutTrigger && c.lockTrigger <= 1.f;
}
inline float smoothstep(float a, float b, float x) {
    if (!(b > a))
        return x >= b ? 1.f : 0.f;
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// What a rope's collision asks of the world: the first thing a line from `from` to `to` meets.
class Collider {
  public:
    virtual ~Collider() = default;
    virtual bool ray(Vec3 from, Vec3 to, Vec3& point, Vec3& normal) const = 0;
};
// An enemy (or any capsule): feet, height, radius.
struct Capsule {
    Vec3 feet{};
    float height = 1.8f, radius = .3f;
};
// A cached plane a rope's particles are kept off.
struct Plane {
    Vec3 point{}, normal{};
    float age{};
};

// The points to draw a rope through: at most `most` of them past the ends, the ones that bend it most (greedy
// Douglas-Peucker, not under `tolerance` metres off the line between their neighbours). `out` gets them in
// order; returns how many.
inline int simplify(std::span<const Vec3> points, int most, float tolerance, Vec3* out) {
    const int n = static_cast<int>(points.size());
    if (n < 3 || most <= 0 || !out)
        return 0;
    std::array<int, maxParticles> chosen{};
    int count = 2;
    chosen[0] = 0;
    chosen[1] = n - 1;
    int interior = 0;
    while (interior < most) {
        float best = tolerance;
        int at = -1, where = 0;
        for (int s = 0; s + 1 < count; ++s) {
            const Vec3 a = points[chosen[s]], b = points[chosen[s + 1]];
            const Vec3 ab = b - a;
            const float len2 = dot(ab, ab);
            for (int i = chosen[s] + 1; i < chosen[s + 1]; ++i) {
                const float t = len2 > 1e-12f ? std::clamp(dot(points[i] - a, ab) / len2, 0.f, 1.f) : 0.f;
                const float d = length(points[i] - (a + ab * t));
                if (d > best) {
                    best = d;
                    at = i;
                    where = s + 1;
                }
            }
        }
        if (at < 0)
            break;
        for (int k = count; k > where; --k)
            chosen[k] = chosen[k - 1];
        chosen[where] = at;
        ++count;
        ++interior;
    }
    for (int s = 1; s + 1 < count; ++s)
        out[s - 1] = points[chosen[s]];
    return count - 2;
}

class Rope {
  public:
    struct Frame {
        float dt{};
        Vec3 hand{}, handVelocity{}, end{}, endVelocity{};
        // The hand's velocity relative to the player (a whip is the hand's snap, not the swing's travel); the
        // hand's own when not given.
        Vec3 handRelative{};
        bool hasRelative{};
        // The web's length (the rope's), metres.
        float length{};
        const Collider* world{};
        std::span<const Capsule> capsules{};
    };
    struct Result {
        bool active{}, asleep{};
        // How taut the line is, 0 (slack) to 1 (the ends are 10% or more past its length).
        float tension{};
        bool taut{};
        // The line's snap: the speed (m/s) its body carries along the chord away from the target, as the hand's
        // pull takes the slack up (the best of the last whipWindow), counted only when it is taut; `away` is that
        // direction (toward the hand); and a whip's jerk when the hand snapped and the line went taut with it.
        float arrival{};
        Vec3 away{};
        bool whip{};
        float whipSpeed{};
    };
    void configure(const Config& c) {
        if (!valid(c))
            return;
        const bool rebuild = c.particles != config_.particles;
        config_ = c;
        if (rebuild)
            reset();
    }
    const Config& config() const {
        return config_;
    }
    void reset() {
        started_ = false;
        asleep_ = false;
        planes_.clear();
        rays_ = 0;
        whipCool_ = 0;
        handSpeeds_.clear();
        still_ = 0;
    }
    bool started() const {
        return started_;
    }
    bool asleep() const {
        return asleep_;
    }
    std::span<const Vec3> points() const {
        return {x_.data(), started_ ? static_cast<std::size_t>(config_.particles) : 0u};
    }
    const std::vector<Plane>& planes() const {
        return planes_;
    }
    // The points to draw through (at most `most`, 6), the line's bends and sag.
    int draw(Vec3* out, int most = maxDrawn) const {
        return started_ ? simplify(points(), std::min(most, maxDrawn), .02f, out) : 0;
    }
    Result step(const Frame& f) {
        Result out;
        const int n = config_.particles;
        const float dt = std::isfinite(f.dt) ? std::clamp(f.dt, 0.f, .1f) : 0.f;
        if (!finite(f.hand) || !finite(f.end) || !finite(f.handVelocity) || !finite(f.endVelocity) ||
            !std::isfinite(f.length) || f.length < .05f || !(dt > 0)) {
            return out;
        }
        out.active = true;
        const float distance = length(f.end - f.hand);
        const float rest = f.length / static_cast<float>(n - 1);
        if (!started_ || std::abs(rest - rest_) > rest_ * .5f + .05f || distance > f.length * 4.f + 2.f) {
            lay(f.hand, f.end, f.length);
        }
        // A rope grows or shortens by the web's length (a reel, a payout): the rest length follows it, and
        // the particles keep their places (the constraint takes the slack up, or lets it pay out).
        rest_ = rest;
        out.tension = std::clamp((distance - f.length) / std::max(f.length * .1f, .05f), 0.f, 1.f);
        out.taut = distance >= f.length * .98f;
        // The hand's speed and the line's along the chord, over the whip's window (the along of this step is
        // put in below).
        handSpeeds_.push_back({dt, length(f.hasRelative && finite(f.handRelative) ? f.handRelative : f.handVelocity), 0});
        float window = 0;
        for (std::size_t i = handSpeeds_.size(); i-- > 0;) {
            window += handSpeeds_[i].dt;
            if (window > config_.whipWindow) {
                handSpeeds_.erase(handSpeeds_.begin(), handSpeeds_.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        whipCool_ = std::max(0.f, whipCool_ - dt);
        // Asleep: nothing moves.
        const float handMove = length(f.hand - lastHand_), endMove = length(f.end - lastEnd_);
        const bool moved = handMove > config_.wakeDistance || endMove > config_.wakeDistance;
        lastHand_ = f.hand;
        lastEnd_ = f.end;
        if (asleep_ && !moved && std::abs(f.length - lastLength_) < 1e-3f) {
            out.asleep = true;
            return out;
        }
        asleep_ = false;
        lastLength_ = f.length;
        // The step.
        const int substeps = config_.substeps;
        const float h = dt / static_cast<float>(substeps);
        const float alpha = config_.compliance / (h * h);
        const float inverseMass = 1.f / config_.particleMass;
        const Vec3 gravity{0, -config_.gravity, 0};
        const float damp = std::exp(-config_.damping * h);
        capsules_.assign(f.capsules.begin(), f.capsules.end());
        const Vec3 hand0 = pinHand_, end0 = pinEnd_;
        const bool longRange = distance <= f.length * 1.001f;
        for (int s = 0; s < substeps; ++s) {
            const float t = static_cast<float>(s + 1) / static_cast<float>(substeps);
            const Vec3 hand = hand0 + (f.hand - hand0) * t, end = end0 + (f.end - end0) * t;
            for (int i = 1; i + 1 < n; ++i) {
                v_[i] = (v_[i] + gravity * h) * damp;
                p_[i] = x_[i];
                x_[i] += v_[i] * h;
            }
            p_[0] = x_[0];
            p_[n - 1] = x_[n - 1];
            x_[0] = hand;
            x_[n - 1] = end;
            std::fill(lambda_.begin(), lambda_.begin() + (n - 1), 0.f);
            // A rope pulls, it does not push: a segment is only constrained once it is longer than its rest.
            for (int pass = 0; pass < 4; ++pass) {
                for (int i = 0; i + 1 < n; ++i) {
                    const Vec3 d = x_[i] - x_[i + 1];
                    const float len = length(d);
                    const float c = len - rest;
                    if (c <= 0 || len < 1e-6f)
                        continue;
                    const float wi = i == 0 ? 0.f : inverseMass, wj = i + 1 == n - 1 ? 0.f : inverseMass;
                    if (wi + wj <= 0)
                        continue;
                    const Vec3 dir = d / len;
                    const float dl = (-c - alpha * lambda_[i]) / (wi + wj + alpha);
                    lambda_[i] += dl;
                    x_[i] += dir * (wi * dl);
                    x_[i + 1] -= dir * (wj * dl);
                }
            }
            // The long-range attachment: no particle further from an end than the rope between them.
            if (longRange)
                for (int i = 1; i + 1 < n; ++i) {
                    const float fromHand = rest * static_cast<float>(i), fromEnd = rest * static_cast<float>(n - 1 - i);
                    Vec3 d = x_[i] - hand;
                    float len = length(d);
                    if (len > fromHand && len > 1e-6f)
                        x_[i] = hand + d * (fromHand / len);
                    d = x_[i] - end;
                    len = length(d);
                    if (len > fromEnd && len > 1e-6f)
                        x_[i] = end + d * (fromEnd / len);
                }
            collide();
            for (int i = 1; i + 1 < n; ++i)
                v_[i] = (x_[i] - p_[i]) / h;
        }
        pinHand_ = f.hand;
        pinEnd_ = f.end;
        // The planes: age them, then cast this step's rays.
        for (auto& pl : planes_)
            pl.age += dt;
        planes_.erase(std::remove_if(planes_.begin(), planes_.end(),
                                     [&](const Plane& pl) { return pl.age > config_.planeSeconds; }),
                      planes_.end());
        if (f.world)
            cast(*f.world);
        // The line's snap: its mean velocity along the chord toward the hand, relative to the target.
        const Vec3 chord = f.hand - f.end;
        const float chordLength = length(chord);
        float recentHand = 0;
        if (chordLength > 1e-4f) {
            out.away = chord / chordLength;
            float along = 0;
            for (int i = 1; i + 1 < n; ++i)
                along += dot(v_[i] - f.endVelocity, out.away);
            handSpeeds_.back().along = along / static_cast<float>(n - 2);
            float best = 0;
            for (const auto& s : handSpeeds_) {
                best = std::max(best, s.along);
                recentHand = std::max(recentHand, s.speed);
            }
            if (out.taut)
                out.arrival = best;
        } else {
            for (const auto& s : handSpeeds_)
                recentHand = std::max(recentHand, s.speed);
        }
        // Still: nothing moved for a while.
        float fastest = 0;
        for (int i = 1; i + 1 < n; ++i)
            fastest = std::max(fastest, length(v_[i]));
        const bool calm = fastest < config_.sleepSpeed && !moved;
        still_ = calm ? still_ + dt : 0.f;
        if (still_ >= config_.sleepSeconds) {
            asleep_ = true;
            for (int i = 1; i + 1 < n; ++i)
                v_[i] = {};
        }
        // A whip: the hand snapped and the wave arrived.
        if (out.arrival >= config_.whipArrival && recentHand >= config_.whipHandSpeed && whipCool_ <= 0 &&
            out.taut) {
            out.whip = true;
            out.whipSpeed = std::min(config_.whipMax, out.arrival * config_.whipShare);
            whipCool_ = config_.whipCooldown;
        }
        return out;
    }

  private:
    struct Sample {
        float dt, speed, along;
    };
    // Laid along the straight line with the sag a rope of this length has between the ends.
    void lay(Vec3 hand, Vec3 end, float len) {
        const int n = config_.particles;
        const Vec3 chord = end - hand;
        const float c = length(chord);
        const float extra = std::max(len - c, 0.f);
        const float sag = std::min(len * .5f, c > .01f ? std::sqrt(3.f * std::max(c, .3f) * extra / 8.f) : extra * .5f);
        for (int i = 0; i < n; ++i) {
            const float s = static_cast<float>(i) / static_cast<float>(n - 1);
            x_[i] = hand + chord * s + Vec3{0, -1, 0} * (4.f * sag * s * (1.f - s));
            p_[i] = x_[i];
            v_[i] = {};
        }
        pinHand_ = lastHand_ = hand;
        pinEnd_ = lastEnd_ = end;
        rest_ = len / static_cast<float>(n - 1);
        lastLength_ = len;
        started_ = true;
        asleep_ = false;
        still_ = 0;
        planes_.clear();
        handSpeeds_.clear();
    }
    // The particles out of the cached planes and the capsules.
    void collide() {
        const int n = config_.particles;
        const float r = config_.radius;
        for (int i = 1; i + 1 < n; ++i) {
            for (const auto& pl : planes_) {
                const float d = dot(x_[i] - pl.point, pl.normal) - r;
                if (d < 0)
                    x_[i] -= pl.normal * d;
            }
            for (const auto& cap : capsules_) {
                const float lowY = cap.feet.y + std::min(cap.radius, cap.height * .5f);
                const float highY = std::max(cap.feet.y + cap.height - cap.radius, lowY);
                const Vec3 axis{cap.feet.x, std::clamp(x_[i].y, lowY, highY), cap.feet.z};
                Vec3 d = x_[i] - axis;
                const float len = length(d);
                const float reach = cap.radius + r;
                if (len < reach) {
                    d = len > 1e-5f ? d / len : Vec3{1, 0, 0};
                    x_[i] = axis + d * reach;
                }
            }
        }
    }
    void cast(const Collider& world) {
        const int n = config_.particles;
        for (int k = 0; k < config_.raysPerStep; ++k) {
            // The segments in turn, and between them each particle's travel since the step before.
            Vec3 a, b;
            if (k % 2 == 0) {
                const int s = (rays_ / 2) % (n - 1);
                a = x_[s];
                b = x_[s + 1];
            } else {
                const int i = 1 + (rays_ / 2) % (n - 2);
                a = prevStep_[i];
                b = x_[i];
            }
            ++rays_;
            if (length(b - a) < 1e-4f)
                continue;
            Vec3 point, normal;
            if (!world.ray(a, b, point, normal) || !finite(point) || !finite(normal) || length(normal) < .5f)
                continue;
            // The plane faces the end of the line nearer the hit (the hand's or the target's): both ends are in
            // the open, so a particle that has slipped through is brought back, not held on the wrong side.
            normal = normalized(normal);
            const Vec3 side = length(point - pinHand_) <= length(point - pinEnd_) ? pinHand_ : pinEnd_;
            if (dot(normal, side - point) < 0)
                normal = -normal;
            planes_.push_back({point, normal, 0});
            if (static_cast<int>(planes_.size()) > config_.maxPlanes)
                planes_.erase(planes_.begin());
        }
        for (int i = 0; i < n; ++i)
            prevStep_[i] = x_[i];
    }
    Config config_;
    bool started_{}, asleep_{};
    std::array<Vec3, maxParticles> x_{}, p_{}, v_{}, prevStep_{};
    std::array<float, maxParticles> lambda_{};
    std::vector<Plane> planes_;
    std::vector<Capsule> capsules_;
    std::vector<Sample> handSpeeds_;
    Vec3 pinHand_{}, pinEnd_{}, lastHand_{}, lastEnd_{};
    float rest_{}, lastLength_{}, whipCool_{}, still_{};
    int rays_{};
};

// ---- spin ----
// The hand circling (its velocity relative to the player turning steadily about an axis): when it has gone
// round at spinRate or more for spinSeconds, the target orbits; the orbit builds over spinRamp.
class Spinner {
  public:
    struct State {
        bool active{};
        Vec3 axis{};
        float rate{}, gain{};
    };
    State update(float seconds, Vec3 handVelocity, const Config& c) {
        State out;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!finite(handVelocity) || !(dt > 0)) {
            return state_(out);
        }
        const float speed = length(handVelocity);
        if (speed >= c.spinHandSpeed) {
            const Vec3 u = handVelocity / speed;
            if (haveLast_) {
                // The turn of its direction this step: its axis and rate.
                const Vec3 c = cross(last_, u);
                const float sine = length(c);
                const float angle = std::atan2(sine, dot(last_, u));
                const Vec3 w = sine > 1e-6f ? c * (angle / (sine * dt)) : Vec3{};
                const float follow = 1 - std::exp(-dt / .15f);
                omega_ = omega_ + (w - omega_) * follow;
            }
            last_ = u;
            haveLast_ = true;
        } else {
            haveLast_ = false;
            omega_ = omega_ * std::exp(-dt / .1f);
        }
        const float rate = length(omega_);
        const Vec3 axis = rate > 1e-3f ? omega_ / rate : Vec3{};
        const float want = active_ ? c.spinRate * .6f : c.spinRate;
        const bool turning = rate >= want && speed >= c.spinHandSpeed * (active_ ? .5f : 1.f) &&
                             (!hasAxis_ || dot(axis, axis_) > .5f);
        if (turning) {
            hold_ += dt;
            lost_ = 0;
            axis_ = axis;
            hasAxis_ = true;
        } else {
            lost_ += dt;
            if (!active_ || lost_ > .15f)
                hold_ = 0;
            if (lost_ > .15f)
                hasAxis_ = false;
        }
        const bool on = hold_ >= c.spinSeconds || (active_ && lost_ <= .15f);
        active_ = on;
        gain_ = on ? std::min(1.f, gain_ + dt / std::max(c.spinRamp, 1e-3f)) : 0.f;
        out.active = on;
        out.axis = axis_;
        out.rate = rate;
        out.gain = gain_;
        return out;
    }
    void reset() {
        *this = Spinner{};
    }
    bool active() const {
        return active_;
    }

  private:
    State state_(State s) {
        reset();
        return s;
    }
    Vec3 last_{}, omega_{}, axis_{};
    bool haveLast_{}, active_{}, hasAxis_{};
    float hold_{}, lost_{}, gain_{};
};
// The velocity an orbiting target is steered to: the hand's own, plus the turn about `axis` at `rate` through
// the target's offset from the hand (its speed at most spinMax), built up by `gain`, and the line's length kept.
inline Vec3 orbitVelocity(Vec3 hand, Vec3 handVelocity, Vec3 at, float length_, Vec3 axis, float rate, float gain,
                          const Config& c = {}) {
    const Vec3 r = at - hand;
    const float len = length(r);
    if (!finite(r) || len < 1e-3f || !finite(axis))
        return handVelocity;
    Vec3 tangent = cross(axis, r / len);
    const float speed = std::min(c.spinMax, rate * len);
    Vec3 v = handVelocity + tangent * (speed * std::clamp(gain, 0.f, 1.f));
    // Keep the line's length: toward it at most 6 m/s.
    v += (r / len) * std::clamp((length_ - len) * 6.f, -6.f, 6.f) * std::clamp(gain, 0.f, 1.f);
    return v;
}

// ---- throw on a tether ----
struct Payout {
    bool cut{};
    float length{};
    float brake{}; // 0 free to 1 locked
    float grip{};  // the share of the web's force the line holds with while it slips (1: locked)
};
// The line after a throw on a tether: the trigger's depth is the brake, like a hand on a rope running out. Under
// payoutTrigger it is cut. At lockTrigger or more the length is locked and the web holds with all its force.
// Between, the line runs out as far as the target goes, held back by friction: the web's force times the brake
// squared times a third, so a light trigger lets it fly nearly free and a deep one stops it in a metre or two.
inline Payout payout(float trigger, float length, float distance, const Config& c = {}) {
    Payout out;
    out.length = length;
    if (!std::isfinite(trigger) || trigger < c.payoutTrigger) {
        out.cut = true;
        return out;
    }
    out.brake = smoothstep(c.payoutTrigger, c.lockTrigger, trigger);
    if (trigger >= c.lockTrigger) {
        out.grip = 1.f;
        return out;
    }
    out.length = std::max(length, distance);
    out.grip = out.brake * out.brake * .35f;
    return out;
}
// A whip's jerk on a target: along `away` at `speed`, no more than the line's force over his mass over the
// impulse's time (so a heavy one gets less).
inline Vec3 whipJerk(Vec3 away, float speed, float mass, float webForce, const Config& c = {}) {
    if (!finite(away) || length(away) < 1e-4f || !(speed > 0))
        return {};
    const float m = std::max(mass, 1.f);
    const float most = webForce / m * c.whipImpulseSeconds;
    return normalized(away) * std::max(std::min({speed, c.whipMax, most}), 0.f);
}
} // namespace spidy::tether
