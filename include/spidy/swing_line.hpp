#pragma once
// FULL-LENGTH SWING LINES (beta2; swing_line_full_length; the owner, Nov 20 t80u/t81u: "Right now, they
// shorten as you get closer to the swing point. I'd like for some way for lines to keep their same length,
// maybe the rope physics we use for combat webs might work" - "Yes, reel should shorten one-for-one, the line
// should stay full. Let's add this as a toggle option." Plan:
// /workspace/playtest-na-beta2/SWING_LINE_PLAN.md, Option 1).
//
// Visual only. The swing's rope (swing.cpp, SwingConfig::taut) is still taken in as the body comes nearer its
// anchor; what is drawn keeps the length the web had when it caught, and the length it does not need hangs as
// slack, laid by the combat webs' rope (tether::Rope: 24 particles, 8 substeps, gravity, no world collision)
// through the game rope's hand points (native_webs::Bent, at most 6), or a sag curve when that cannot run.
//   - The drawn length is the anchor-to-wrist distance on the frame the web is first drawn attached.
//   - It stays while the player closes on the anchor (it does not shrink by itself).
//   - The trigger's reel shortens it one-for-one: by the swing rope's own shortening on frames the hand
//   reels.
//   - On the ground the swing pays the rope out as the game walks him away (payOut): it grows by as much.
//   - Never shorter than the line: past it (taut) it is the distance.
//   - A new web (another rope key) starts it again; off: the distance (no slack, the straight line as
//   before).
//
// Engine independent; no game address.
#include "math.hpp"
#include "tether.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::swing_line {
// Slack under this (metres) is drawn straight; past maxSlack the rest is not drawn (a 200 m web caught far
// off would otherwise hang a 100 m loop through the street: the rope has no world collision here).
inline constexpr float minSlack = .05f, maxSlack = 30.f;

struct Input {
    bool enabled{}, attached{};
    int64_t key{};      // the rope (native_webs' attachedAt): another value is another web
    float distance{};   // anchor to wrist now, metres
    float ropeLength{}; // the swing's rope (game_swing::WebState::length)
    bool reeling{};     // that hand reels it in now (WebState::attached bit 8)
    bool grounded{};    // the swing pays out on the ground (Swing::payOut)
    // WRAP (beta3c, web_wrap_slack): the web's run past the free part, metres (wrist to the first edge is the
    // free part; the edges to where it sticks are this). It counts in the line's length; the slack hangs on
    // the free part. 0: a straight web.
    float beyond{};
    // How many points the free part may use (the game rope's hand points left after the edges).
    int most = tether::maxDrawn;
    // ROPE PHYSICS ON THE SWING (beta3c, swing_rope_physics): the pull's strength, 1 = moderate; 0 = none.
    float pullStrength{};
};
struct Change {
    bool attached{}; // this update caught a new web
    bool reeled{};   // the reel took some in this update
};

class DrawnLength {
  public:
    // The length to draw this frame.
    float update(const Input& in, Change* change = nullptr) {
        Change c;
        const bool sound = std::isfinite(in.distance) && in.distance >= 0 && std::isfinite(in.ropeLength);
        if (!in.enabled || !in.attached || !sound) {
            started_ = false;
            key_ = 0;
            drawn_ = sound ? in.distance : 0.f;
            if (change)
                *change = c;
            return drawn_;
        }
        if (!started_ || in.key != key_) {
            started_ = true;
            key_ = in.key;
            drawn_ = in.distance;
            rope_ = in.ropeLength;
            reeled_ = 0;
            c.attached = true;
        } else {
            const float d = in.ropeLength - rope_;
            if (in.reeling && d < 0) {
                drawn_ += d; // one-for-one with the reel
                reeled_ -= d;
                c.reeled = true;
            } else if (in.grounded && d > 0) {
                drawn_ += d; // the ground's payout
            }
            rope_ = in.ropeLength;
        }
        drawn_ = std::max(drawn_, in.distance);
        if (change)
            *change = c;
        return drawn_;
    }
    float drawn() const {
        return drawn_;
    }
    float reeledIn() const {
        return reeled_;
    }
    bool started() const {
        return started_;
    }

  private:
    bool started_{};
    int64_t key_{};
    float drawn_{}, rope_{}, reeled_{};
};

// The fallback: `count` points strictly between `from` and `to` on a parabola as long as `drawn` (s = c + 8
// d^2 / 3c), sagging straight down (sideways a little when the line stands straight up). Returns how many (0:
// no slack).
inline int sagPoints(Vec3 from, Vec3 to, float drawn, int count, Vec3* out) {
    if (!out || count <= 0 || !finite(from) || !finite(to) || !std::isfinite(drawn))
        return 0;
    const Vec3 chord = to - from;
    const float c = length(chord);
    const float extra = std::min(drawn - c, maxSlack);
    if (!(extra > minSlack))
        return 0;
    const float sag = std::min(drawn * .5f, c > .01f ? std::sqrt(3.f * c * extra / 8.f) : extra * .5f);
    const Vec3 down{0, -1, 0};
    Vec3 bow = down;
    if (c > .01f) {
        const Vec3 along = chord / c, side{1, 0, 0};
        bow = (down - along * dot(down, along)) + (side - along * dot(side, along)) * .05f;
        bow = length(bow) > 1e-4f ? normalized(bow) : down;
    }
    for (int i = 0; i < count; ++i) {
        const float s = static_cast<float>(i + 1) / static_cast<float>(count + 1);
        out[i] = from + chord * s + bow * (4.f * sag * s * (1.f - s));
    }
    return count;
}

// One hand's drawn swing line: the length, and the combat webs' rope laying the slack.
class Line {
  public:
    struct Out {
        float drawn{}, distance{};
        int count{};                     // points past the ends (0: straight)
        Vec3 points[tether::maxDrawn]{}; // from the wrist toward the anchor
        bool simulated{};                // laid by the rope (else the sag curve, or straight)
        Change change{};
        float slack{}; // drawn past the line (metres)
        Vec3 pull{};   // ROPE PHYSICS ON THE SWING: the line's pull on the player (m/s^2), 0 when not asked
    };
    Out update(const Input& in, Vec3 wrist, Vec3 anchor, float seconds) {
        Out o;
        fading_ = false;
        o.distance = finite(wrist) && finite(anchor) ? length(anchor - wrist) : -1.f;
        const float beyond = std::isfinite(in.beyond) ? std::max(in.beyond, 0.f) : 0.f;
        Input given = in;
        given.distance = o.distance < 0 ? o.distance : o.distance + beyond;
        o.drawn = length_.update(given, &o.change);
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!in.enabled || !in.attached || o.distance < 0) {
            rope_.reset();
            age_ = 0;
            laid_ = 0;
            return o;
        }
        if (o.change.attached) {
            rope_.reset();
            age_ = 0;
            lastWrist_ = wrist;
            handVelocity_ = {};
        }
        if (dt > 0 && finite(lastWrist_)) {
            const Vec3 v = (wrist - lastWrist_) * (1.f / dt);
            handVelocity_ = finite(v) && length(v) < 60.f ? v : Vec3{};
        }
        lastWrist_ = wrist;
        age_ += dt;
        const float slack = o.drawn - (o.distance + beyond);
        o.slack = std::max(slack, 0.f);
        // Past maxSlack the rope is drawn maxSlack longer than the line (the rest is not drawn).
        const float laid = o.distance + std::clamp(slack, 0.f, maxSlack);
        laid_ = laid;
        if (!rope_.started())
            rope_.configure(config());
        tether::Rope::Frame f;
        f.dt = dt;
        f.hand = wrist;
        f.end = anchor;
        f.length = laid;
        const auto r = rope_.step(f);
        if (!(slack > minSlack))
            return o; // taut: the straight line, as before (the rope keeps running for the next slack)
        const int most = std::clamp(in.most, 0, tether::maxDrawn);
        if (most > 0 && (r.active || r.asleep)) {
            o.count = rope_.draw(o.points, most);
            o.simulated = o.count > 0;
        }
        if (!o.simulated && most > 0)
            o.count = sagPoints(wrist, anchor, o.drawn - beyond, most, o.points);
        if (in.pullStrength > 0 && !in.grounded && (r.active || r.asleep))
            o.pull = pull(slack, in.pullStrength);
        return o;
    }
    // PHYSICS UNTIL FADE (beta3c, web_fade_physics): the web was let go of. Its wrist end comes free with the
    // hand's last velocity and swings on the rope from `anchor` (gravity, still air), the rope laying the
    // line between, until the caller lets it dissolve. Call release() once on the let-go frame, then fade()
    // each frame.
    bool release() {
        if (!rope_.started() || !(laid_ > 0) || !finite(lastWrist_))
            return false;
        fading_ = true;
        freeEnd_ = lastWrist_;
        freeVelocity_ = handVelocity_;
        fadeAge_ = 0;
        return true;
    }
    bool fading() const {
        return fading_;
    }
    float fadeAge() const {
        return fadeAge_;
    }
    Out fade(Vec3 anchor, float seconds, int most = tether::maxDrawn) {
        Out o;
        if (!fading_ || !finite(anchor)) {
            stop();
            return o;
        }
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        fadeAge_ += dt;
        // The free end: a particle on the rope's length from the anchor.
        freeVelocity_ += Vec3{0, -config().gravity, 0} * dt;
        freeVelocity_ = freeVelocity_ * std::exp(-config().damping * dt);
        freeEnd_ += freeVelocity_ * dt;
        const Vec3 out = freeEnd_ - anchor;
        const float reach = length(out);
        if (reach > laid_ && reach > 1e-4f) {
            const Vec3 dir = out / reach;
            freeEnd_ = anchor + dir * laid_;
            const float away = dot(freeVelocity_, dir);
            if (away > 0)
                freeVelocity_ -= dir * away;
        }
        if (!finite(freeEnd_) || !finite(freeVelocity_)) {
            stop();
            return o;
        }
        tether::Rope::Frame f;
        f.dt = dt;
        f.hand = freeEnd_;
        f.handVelocity = freeVelocity_;
        f.end = anchor;
        f.length = laid_;
        const auto r = rope_.step(f);
        o.distance = length(anchor - freeEnd_);
        o.drawn = laid_;
        o.slack = std::max(laid_ - o.distance, 0.f);
        const int n = std::clamp(most, 0, tether::maxDrawn);
        if (n > 0 && laid_ - o.distance > minSlack && (r.active || r.asleep)) {
            o.count = rope_.draw(o.points, n);
            o.simulated = o.count > 0;
        }
        return o;
    }
    // Where the let-go web's wrist end is now (fade()).
    Vec3 freeEnd() const {
        return freeEnd_;
    }
    void stop() {
        fading_ = false;
        rope_.reset();
        laid_ = 0;
        age_ = 0;
    }
    // ROPE PHYSICS ON THE SWING: the pull (m/s^2 on the player) the hanging line puts on the hand, along the
    // rope's first segment there (where a hanging rope's tension pulls): half the laid slack's weight, and
    // the momentum the slack carries away from the hand handed over in flingSeconds. The line's mass per
    // metre is massPerMetre of the player's; capped at pullCap (both times the strength), ramped in over
    // pullRamp after a catch. The sim's damping is world-space (still air), so a line trailing a fast swing
    // pulls back: the drag.
    static constexpr float massPerMetre = .0102f, flingSeconds = .5f, pullCap = 2.5f, pullRamp = .3f;
    Vec3 pull(float slack, float strength) const {
        const auto x = rope_.points();
        const auto v = rope_.velocities();
        const size_t n = x.size();
        if (n < 3 || v.size() != n || !(strength > 0) || !(slack > minSlack))
            return {};
        const Vec3 first = x[1] - x[0];
        const float len = length(first);
        if (!(len > 1e-5f))
            return {};
        const Vec3 dir = first / len;
        const float mass = massPerMetre * std::min(slack, maxSlack);
        float along = 0;
        for (size_t i = 1; i + 1 < n; ++i)
            along += dot(v[i] - handVelocity_, dir);
        along /= static_cast<float>(n - 2);
        const float weight = .5f * mass * config().gravity;
        const float fling = mass * std::max(along, 0.f) / flingSeconds;
        Vec3 a = dir * ((weight + fling) * strength);
        const float cap = pullCap * strength, m = length(a);
        if (m > cap && m > 0)
            a = a * (cap / m);
        a = a * std::clamp(age_ / pullRamp, 0.f, 1.f);
        return finite(a) ? a : Vec3{};
    }
    const DrawnLength& drawnLength() const {
        return length_;
    }
    static tether::Config config() {
        tether::Config c; // the combat webs' rope: 24 particles, 8 substeps, gravity, its damping
        c.enabled = true;
        c.raysPerStep = 0; // no world here
        return c;
    }

  private:
    DrawnLength length_;
    tether::Rope rope_;
    bool fading_{};
    float age_{}, laid_{}, fadeAge_{};
    Vec3 lastWrist_{}, handVelocity_{}, freeEnd_{}, freeVelocity_{};
};
} // namespace spidy::swing_line
