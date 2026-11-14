// PUNCH AUTO-TARGET (the polish pass, slice 7). The user, t65u: "Add in an autotarget for punches that turns your view
// toward any enemy you're attacking", and when asked what counts: "I only want punches to target enemies that are
// highlighted and targeted by the player. Punches always do damage, but tracking only happens on targeted enemies. If the
// player punches toward a targeted enemy, everything should immediately realign in that direction." (t66u: the view
// turning is a switch, off by default.)
//
// A THROW is a free fist leaving rest: its speed (relative to the player) rising past PunchConfig::minSpeed after it was
// under rearmSpeed (or after rearmTime), the same thresholds a landed punch needs, so a throw is what becomes a punch. The
// verdict is taken ON THE THROW, not at the landing: the targeted enemy (game_punch's lock: the one the ring marks, the
// one the next punch and the auto dive go to), if the fist's direction is within the cone of the line from the head to his
// middle, and he is within the range. Nobody targeted, or a target out of the cone or the range: no tracking (the punch
// is as before and does the same damage). Pure.
#pragma once
#include "spidy/math.hpp"
#include <cmath>
#include <cstdint>

namespace spidy::punch_track {
inline constexpr float degrees = 3.14159265f / 180.f;
struct Config {
    bool on{};
    float coneDeg = 45.f, rangeM = 6.f;
};
enum class Why : uint8_t { tracked, off, noTarget, noDirection, tooFar, outsideCone };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::tracked: return "tracked";
    case Why::off: return "auto-target off";
    case Why::noTarget: return "nobody targeted";
    case Why::noDirection: return "no direction";
    case Why::tooFar: return "too far";
    default: return "outside the cone";
    }
}
struct Verdict {
    Why why = Why::off;
    float distance{}, angleDeg{};
    Vec3 toward{}; // unit, from the head to the enemy's middle (world); zero unless tracked
};
// `punch`: the fist's velocity relative to the player (world axes); `head`: the head; `target`: the targeted enemy's middle.
inline Verdict decide(const Config& c, bool haveTarget, Vec3 head, Vec3 punch, Vec3 target) {
    Verdict v;
    if (!c.on)
        return v;
    if (!haveTarget) {
        v.why = Why::noTarget;
        return v;
    }
    const Vec3 to = target - head;
    const float distance = length(to), speed = length(punch);
    if (!std::isfinite(distance) || !std::isfinite(speed) || distance < .05f || speed < .05f) {
        v.why = Why::noDirection;
        return v;
    }
    v.distance = distance;
    const float cosine = std::fmax(-1.f, std::fmin(1.f, dot(to, punch) / (distance * speed)));
    v.angleDeg = std::acos(cosine) / degrees;
    if (distance > c.rangeM) {
        v.why = Why::tooFar;
        return v;
    }
    if (v.angleDeg > c.coneDeg) {
        v.why = Why::outsideCone;
        return v;
    }
    v.why = Why::tracked;
    v.toward = to / distance;
    return v;
}
// One hand's throws. `speed`: the fist's speed relative to the player; `free`: the hand is not busy (a web, a grab, a
// wall). A throw once per swing: armed again under rearmSpeed or after rearmTime.
class Throws {
  public:
    static constexpr float minSpeed = 2.2f, rearmSpeed = 1.2f, rearmTime = .45f;
    bool update(float dt, float speed, bool free) {
        if (!std::isfinite(speed) || !std::isfinite(dt)) {
            return false;
        }
        if (!free) {
            armed_ = false;
            since_ = 0;
            return false;
        }
        if (speed < rearmSpeed) {
            armed_ = true;
            return false;
        }
        if (!armed_) {
            since_ += dt > 0 ? dt : 0;
            if (since_ >= rearmTime)
                armed_ = true;
        }
        if (armed_ && speed >= minSpeed) {
            armed_ = false;
            since_ = 0;
            return true;
        }
        return false;
    }
    void reset() {
        *this = Throws{};
    }

  private:
    bool armed_ = true;
    float since_{};
};
} // namespace spidy::punch_track
