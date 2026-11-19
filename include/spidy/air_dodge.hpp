#pragma once
// AIR DODGE ON B (beta2; the owner, Nov 19 t79u: "Air dodge is fine on B"). Pure; no Windows.
//
// In SpidyVR's air (VR owns him, no web held, off the ground and off a wall), B is a short burst the way the
// left stick points, relative to the view's heading (the headset's: animation never turns it); a stick at
// rest is a burst backward, away from where he looks. The burst is a velocity change for the swing's next
// step (SpidySwingAirDodge), with the game's own dodge answering a warned blow during it (game_dodge's carry,
// as a web or a sidestep has it).
//
// Living with the other B's:
//   - A+B (the point jump's chord) and Y+B (the game's finisher), X+B (Showtime's chord): B waits
//   chordSeconds (the
//     point jump's own window, 50 ms) for A, X or Y. One of them inside the window: no dodge, and B goes on
//     to the game with it, as it always did. B let go of inside the window: the dodge (a tap).
//   - On the ground, on a wall, on a web, in the animation's air, during a prompt naming B, the phone, the
//   pad layer or
//     a calibration: B is the game's at once, exactly as before (the ground's sidestep and dodge are
//     untouched).
//   - A spider-sense warning standing: B is the game's at once (its perfect dodge keeps its timing).
// While a dodge has B, the game gets none of it until it is let go of.
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::air_dodge {

struct Params {
    bool enabled = true;
    float chordSeconds = .05f; // B waits this long for A, X or Y (air_dodge_chord_ms; the point jump's 50 ms)
    float speed = 9.f;         // m/s along the dodge's way (air_dodge_speed)
    float keep = .4f;          // share of the velocity across the way he keeps
    float lift = 1.5f;         // m/s up, on top of what he keeps of a rise (a fall is stalled)
    float cooldownSeconds = .5f; // after a dodge (air_dodge_cooldown_ms)
    int perAir = 2;              // dodges before he touches something again
    float carrySeconds = .3f;    // the game's dodge answers warned blows this long (its i-frames)
    float stickDeadZone = .3f;   // a stick under this is at rest: backward
};
template <class V> Params paramsOf(const V& v) {
    Params p;
    p.enabled = v.airDodge;
    p.chordSeconds = std::clamp(v.airDodgeChordMs, 0, 200) / 1000.f;
    p.speed = static_cast<float>(std::clamp(v.airDodgeSpeed, 3, 20));
    p.cooldownSeconds = std::clamp(v.airDodgeCooldownMs, 100, 3000) / 1000.f;
    return p;
}

enum class Why : uint8_t { none, ground, owner, web, blocked, warning, chord, cooldown, spent, disabled };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::ground:
        return "not in SpidyVR's free air (the ground or a wall)";
    case Why::owner:
        return "the animation has him";
    case Why::web:
        return "a web holds him";
    case Why::blocked:
        return "B is taken (a prompt, the phone, the pad layer, a calibration)";
    case Why::warning:
        return "a spider-sense warning: the game's perfect dodge";
    case Why::chord:
        return "A, X or Y came with it (the point jump, Showtime or a finisher)";
    case Why::cooldown:
        return "cooling down";
    case Why::spent:
        return "both dodges of this air used";
    case Why::disabled:
        return "AIR DODGE is off";
    default:
        return "none";
    }
}

class Trigger {
  public:
    struct Input {
        float seconds{};
        bool b{};       // B as it would reach the game now (after the point jump's and Showtime's chords)
        bool others{};  // A, X or Y held (the chords B makes with them)
        bool freeAir{}; // off the ground and off a wall, in SpidyVR's own air
        bool vrOwns{};  // SpidyVR has the body
        bool webbed{};  // a web holds him
        bool blocked{}; // a prompt naming B, the phone, the pad layer, a calibration
        bool warning{}; // a spider-sense warning stands
        bool contact{}; // touching the ground or a wall: the air's dodges come back
    };
    struct Output {
        bool b{};       // what of B reaches the game
        bool fire{};    // dodge now
        bool waiting{}; // B held back, waiting for the chord window
        Why passed{}; // B pressed and passed on to the game: why it was not a dodge (none: not a press, or a
                      // dodge)
    };
    Output update(const Input& in, const Params& p) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        cooldown_ = std::max(0.f, cooldown_ - dt);
        if (in.contact)
            used_ = 0;
        const bool pressed = in.b && !bWas_;
        bWas_ = in.b;
        Output out;
        switch (phase_) {
        case Phase::idle:
            if (pressed) {
                const Why why = refusal(in, p);
                if (why == Why::none) {
                    phase_ = Phase::waiting;
                    wait_ = 0;
                } else {
                    phase_ = Phase::passing;
                    out.passed = why;
                }
            }
            break;
        case Phase::waiting:
            wait_ += dt;
            if (in.others || in.warning || in.blocked || !in.freeAir || !in.vrOwns || in.webbed) {
                phase_ = in.b ? Phase::passing : Phase::idle;
                out.passed = in.others    ? Why::chord
                             : in.warning ? Why::warning
                             : in.blocked ? Why::blocked
                             : in.webbed  ? Why::web
                             : !in.vrOwns ? Why::owner
                                          : Why::ground;
                // A tap let go of inside the window that then lost the air: the game's tap, late.
                out.b = true;
            } else if (!in.b || wait_ >= p.chordSeconds) {
                out.fire = true;
                ++used_;
                cooldown_ = p.cooldownSeconds;
                phase_ = in.b ? Phase::taken : Phase::idle;
            }
            break;
        case Phase::taken:
            if (!in.b)
                phase_ = Phase::idle;
            break;
        case Phase::passing:
            if (!in.b)
                phase_ = Phase::idle;
            break;
        }
        out.waiting = phase_ == Phase::waiting;
        if (phase_ == Phase::passing)
            out.b = in.b;
        else if (phase_ == Phase::idle && !out.b)
            out.b = in.b && !out.fire;
        return out;
    }
    int used() const {
        return used_;
    }
    void reset() {
        phase_ = Phase::idle;
    }

  private:
    enum class Phase : uint8_t { idle, waiting, taken, passing };
    Why refusal(const Input& in, const Params& p) const {
        if (!p.enabled)
            return Why::disabled;
        if (in.blocked)
            return Why::blocked;
        if (!in.vrOwns)
            return Why::owner;
        if (!in.freeAir)
            return Why::ground;
        if (in.webbed)
            return Why::web;
        if (in.warning)
            return Why::warning;
        if (in.others)
            return Why::chord;
        if (used_ >= p.perAir)
            return Why::spent;
        if (cooldown_ > 0)
            return Why::cooldown;
        return Why::none;
    }
    Phase phase_ = Phase::idle;
    bool bWas_{};
    float wait_{}, cooldown_{};
    int used_{};
};

// The dodge's way, level: the left stick turned by the view's heading (look: the headset's forward), backward
// (away from the look) with the stick at rest. `fromStick`: whether the stick chose it.
inline Vec3 way(Vec3 look, float stickX, float stickY, const Params& p, bool* fromStick = nullptr) {
    Vec3 forward{look.x, 0, look.z};
    if (!finite(forward) || length(forward) < 1e-3f)
        forward = {0, 0, -1};
    forward = normalized(forward);
    const Vec3 right{-forward.z, 0, forward.x};
    const float x = std::isfinite(stickX) ? stickX : 0.f, y = std::isfinite(stickY) ? stickY : 0.f;
    const bool stick = std::hypot(x, y) >= p.stickDeadZone;
    if (fromStick)
        *fromStick = stick;
    if (!stick)
        return forward * -1.f;
    return normalized(right * x + forward * y);
}
// The velocity the dodge leaves him with: `speed` along the way, `keep` of what he had across it, and the
// lift on top of what he keeps of a rise (a fall is stalled, not kept).
inline Vec3 velocity(Vec3 dodgeWay, Vec3 before, const Params& p) {
    const Vec3 v = finite(before) ? before : Vec3{};
    Vec3 level{v.x, 0, v.z};
    level = level - dodgeWay * dot(level, dodgeWay);
    Vec3 out = dodgeWay * p.speed + level * p.keep;
    out.y = std::max(v.y, 0.f) * p.keep + p.lift;
    return out;
}

} // namespace spidy::air_dodge
