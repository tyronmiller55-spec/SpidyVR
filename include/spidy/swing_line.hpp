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
    };
    Out update(const Input& in, Vec3 wrist, Vec3 anchor, float seconds) {
        Out o;
        o.distance = finite(wrist) && finite(anchor) ? length(anchor - wrist) : -1.f;
        Input given = in;
        given.distance = o.distance;
        o.drawn = length_.update(given, &o.change);
        if (!in.enabled || !in.attached || o.distance < 0) {
            rope_.reset();
            return o;
        }
        if (o.change.attached)
            rope_.reset();
        const float slack = o.drawn - o.distance;
        // Past maxSlack the rope is drawn maxSlack longer than the line (the rest is not drawn).
        const float laid = o.distance + std::min(slack, maxSlack);
        if (!rope_.started())
            rope_.configure(config());
        tether::Rope::Frame f;
        f.dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        f.hand = wrist;
        f.end = anchor;
        f.length = laid;
        const auto r = rope_.step(f);
        if (!(slack > minSlack))
            return o; // taut: the straight line, as before (the rope keeps running for the next slack)
        if (r.active || r.asleep) {
            o.count = rope_.draw(o.points, tether::maxDrawn);
            o.simulated = o.count > 0;
        }
        if (!o.simulated)
            o.count = sagPoints(wrist, anchor, o.drawn, tether::maxDrawn, o.points);
        return o;
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
};
} // namespace spidy::swing_line
