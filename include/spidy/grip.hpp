#pragma once
// GRIP ACROSS THE GAME (G1; the plan's Pillar 2; wall_brake and grip_fling, both off until set,
// EXPERIMENTAL).
//
// WALL BRAKE. On Spidy's wall, closing the physical grip with the palm at the wall (the hand climb's own
// contact test) brakes the sliding body as friction does, instead of stopping it at once:
//     a = min(aMax, mu * (g + k * pressed)),     mu 0.9, k 40 m/s^2 per m, aMax 14 m/s^2 per hand
// (pressed: how far the tracked palm is past the wall's face, m). One hand stops 10 m/s in about 1.2 s, two
// in about 0.6 s. Under 0.5 m/s the brake becomes the hand-climb hold as it was. The feel is grain pulses at
// 30 to 160 Hz by the slide speed, strong by the braking force.
//
// GRIP FLING. With the physical grip closed on a surface (a wall, the ground, anything the palm meets
// within 12 cm and faces), pulling the hand and then letting go launches the body the other way:
//     v_out = min(vMax, gain * |mean hand velocity|)   (mean over the last 60 ms, relative to the head,
//                                                       so to the surface the body is held against)
// opposite to that velocity; hands slower than 0.5 m/s fling nothing. The history clears on a snap turn, a
// tracking loss, a long frame and every new grip, as in GorillaLocomotion. The launch is a velocity at
// most vMax (9 m/s): no hand speed gives more, and nothing here adds to a speed that is already over it.
// It works everywhere, the ground included (an approved ownership exception, docs/OWNERSHIP.md, as the
// point jump's).
//
// Engine independent. The swing module (Swing::gripInputs) feeds it; XR only sends the settings
// (game_swing::GripAsk) and plays the feel from game_swing::GripLive. No game address is used.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::grip {
struct Config {
    // Brake.
    float mu = .9f, k = 40.f, aMax = 14.f, gravity = 9.81f, stopSpeed = .5f, maxPress = .15f;
    // Fling.
    float gain = 1.3f, vMax = 9.f, minHandSpeed = .5f, window = .06f;
    // The most a hand moves in a tracked frame that is believed (m/s); more is a tracking glitch.
    float glitchSpeed = 15.f;
    // The hop a launch from the ground carries, so it leaves the floor (m/s up, at least).
    float groundHop = 2.5f;
    // A palm counts as at a surface within this (m), pointing at it.
    float contactReach = .12f;
};

// The deceleration of one braking hand (m/s^2) for a press of `pressed` metres.
inline float brakeDecel(float pressed, const Config& c = {}) {
    const float p = std::isfinite(pressed) ? std::clamp(pressed, 0.f, c.maxPress) : 0.f;
    return std::min(c.aMax, std::max(0.f, c.mu) * (c.gravity + c.k * p));
}
// The speed after dt of the hands' summed deceleration; never below zero, never above `speed`.
inline float brakeSpeed(float speed, float decel, float dt) {
    if (!(speed > 0) || !std::isfinite(speed))
        return 0;
    if (!std::isfinite(decel) || !std::isfinite(dt) || decel <= 0 || dt <= 0)
        return speed;
    return std::max(0.f, speed - decel * dt);
}
// Whether a hand's brake goes on (the body slides at least stopSpeed when it presses) and when it ends
// (the slide drops under stopSpeed: the hold).
inline bool brakeBegins(float slideSpeed, const Config& c = {}) {
    return std::isfinite(slideSpeed) && slideSpeed >= c.stopSpeed;
}
inline bool brakeEnds(float slideSpeed, const Config& c = {}) {
    return !(slideSpeed >= c.stopSpeed);
}
// The grain of the brake's feel: pulses per second (30 slow to 160 fast, by the slide speed up to 12
// m/s) and the amplitude (by the braking force against aMax, 0.15 to 0.9).
inline float grainHz(float slideSpeed) {
    const float s = std::isfinite(slideSpeed) ? std::clamp(slideSpeed / 12.f, 0.f, 1.f) : 0.f;
    return 30.f + 130.f * s;
}
inline float grainAmplitude(float decel, const Config& c = {}) {
    const float f = std::isfinite(decel) ? std::clamp(decel / std::max(c.aMax, 1e-3f), 0.f, 1.f) : 0.f;
    return .15f + .75f * f;
}

// A hand's recent velocities (relative to the head, world axes), each held for the time it covered.
class History {
  public:
    static constexpr std::size_t capacity = 16;
    void clear() {
        count_ = 0;
    }
    std::size_t size() const {
        return count_;
    }
    void add(Vec3 velocity, float dt) {
        if (!finite(velocity) || !std::isfinite(dt) || dt <= 0)
            return;
        if (count_ == capacity) {
            for (std::size_t i = 1; i < count_; ++i)
                samples_[i - 1] = samples_[i];
            --count_;
        }
        samples_[count_++] = {velocity, dt};
    }
    // The mean velocity over the newest `window` seconds, as long as at least a third of the window is
    // covered (a first frame is not a mean).
    Vec3 mean(float window = .06f) const {
        float covered = 0;
        Vec3 sum{};
        for (std::size_t i = count_; i > 0 && covered < window; --i) {
            const auto& s = samples_[i - 1];
            const float take = std::min(s.dt, window - covered);
            sum += s.velocity * take;
            covered += take;
        }
        if (covered < window / 3)
            return {};
        return sum / covered;
    }

  private:
    struct Sample {
        Vec3 velocity{};
        float dt{};
    };
    std::array<Sample, capacity> samples_{};
    std::size_t count_{};
};

// The launch of a pull let go of: `handMean` is the hand's mean velocity (m/s); the body goes the other
// way at min(vMax, gain * speed), and nothing at all under minHandSpeed.
inline Vec3 flingVelocity(Vec3 handMean, const Config& c = {}) {
    if (!finite(handMean))
        return {};
    const float speed = length(handMean);
    if (!(speed >= c.minHandSpeed) || speed < 1e-4f)
        return {};
    const float out = std::min(std::max(c.vMax, 0.f), std::max(c.gain, 0.f) * speed);
    return handMean * (-out / speed);
}

// One hand's grip: what it grips, its history, and when it lets go.
class Hand {
  public:
    enum class Kind : std::uint8_t { none, wall, surface };
    struct Frame {
        float dt{};
        bool valid{};    // tracked, focused, the settings on
        float squeeze{}; // the physical grip, 0..1
        bool contact{};  // the palm is at a surface it points at (a press only)
        bool wall{};     // the hand climb holds this hand on the wall
        Vec3 velocity{}; // the hand's velocity relative to the head, world axes (m/s)
        bool turned{};   // a snap turn or a tracking jump this frame
    };
    struct Out {
        bool pressed{}, released{};
        Vec3 launch{}; // with `released`: the fling, m/s (zero: none)
        float handSpeed{};
    };
    Out update(const Frame& f, const Config& c = {}) {
        Out out;
        const bool believable = f.valid && std::isfinite(f.dt) && f.dt > 0 && f.dt <= .1f && !f.turned;
        if (!f.valid || !std::isfinite(f.squeeze)) {
            *this = {};
            return out;
        }
        const bool held = f.squeeze > (held_ ? .35f : .65f);
        const bool press = held && !held_;
        if (press) {
            // A new grip: the history starts again.
            history_.clear();
            kind_ = f.wall ? Kind::wall : f.contact ? Kind::surface : Kind::none;
            out.pressed = kind_ != Kind::none;
        } else if (held && kind_ == Kind::none && f.wall) {
            kind_ = Kind::wall;
            history_.clear();
        }
        if (held && kind_ != Kind::none) {
            if (!believable || length(f.velocity) > c.glitchSpeed)
                history_.clear();
            else
                history_.add(f.velocity, f.dt);
        }
        if (!held && held_ && kind_ != Kind::none && believable) {
            const Vec3 mean = history_.mean(c.window);
            out.handSpeed = length(mean);
            out.launch = flingVelocity(mean, c);
            out.released = true;
        }
        if (!held)
            kind_ = Kind::none;
        if (!held)
            history_.clear();
        held_ = held;
        return out;
    }
    void reset() {
        *this = {};
    }
    bool gripping() const {
        return held_ && kind_ != Kind::none;
    }
    Kind kind() const {
        return kind_;
    }

  private:
    History history_;
    Kind kind_{};
    bool held_{};
};
} // namespace spidy::grip
