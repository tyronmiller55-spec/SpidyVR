// MOVE HAPTICS (the user, November 12 04:56/05:07, t70u: "haptic feedback for
// all jumps and landings as well, scaled accordingly"; point jump: "I'd like
// haptic feedback when it happens"). Controller pulses, both hands:
//   - a takeoff (supported -> airborne): a game jump (up 2.5 m/s or more), a
//     wall jump (off a wall at 4 m/s or more) or a zip launch (a zip pressed in
//     the last 3 s), strength 0.35 + 0.65 x speed/25 m/s;
//   - a zip launch in the air: up-speed rising 6 m/s within 0.25 s (to 5 m/s or
//     more) after a zip press, no web held, once per air phase;
//   - a point jump: a distinct double thump (1.0 for 70 ms, then 110 ms later
//     0.85 for 100 ms); takeoffs within 0.4 s of it are its own;
//   - a landing after 0.25 s or more in the air coming down at 3 m/s or more
//     (step-downs skipped): 0.2 + 0.8 x (fall - 4)/21 m/s, 0.7x on a wall.
// Its own strength (MOVE HAPTICS 0-200%), NOT scaled by CONTROLLER VIBRATION:
// the caller plays these with XrRuntime::hapticUnscaled. Above 100% the
// amplitude caps at 1 and the pulse lengthens (up to 1.5x). Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::move_haptics {

enum class Kind : uint8_t { none, jump, wallJump, zipLaunch, pointJump, landing, wallLanding };
inline const char* kindName(Kind k) {
    switch (k) {
    case Kind::jump:
        return "jump";
    case Kind::wallJump:
        return "wall jump";
    case Kind::zipLaunch:
        return "zip launch";
    case Kind::pointJump:
        return "point jump";
    case Kind::landing:
        return "landing";
    case Kind::wallLanding:
        return "wall landing";
    default:
        return "none";
    }
}

inline constexpr float jumpUp = 2.5f, wallSpeed = 4.f, zipRecentSeconds = 3.f;
inline constexpr float airZipRise = 6.f, airZipWindow = .25f, airZipUp = 5.f;
inline constexpr float pointJumpQuiet = .4f, takeoffQuiet = .3f, takeoffOpenSeconds = .15f;
inline constexpr float landingAir = .25f, landingFall = 3.f, fallWindow = .3f, wallLandingShare = .7f;

inline float launchStrength(float speed) {
    return .35f + .65f * std::clamp((std::isfinite(speed) ? speed : 0.f) / 25.f, 0.f, 1.f);
}
inline float launchSeconds(float strength) {
    return .06f + .08f * std::clamp(strength, 0.f, 1.f);
}
inline float landingStrength(float fall) {
    return .2f + .8f * std::clamp(((std::isfinite(fall) ? fall : 0.f) - 4.f) / 21.f, 0.f, 1.f);
}
inline float landingSeconds(float strength) {
    return .04f + .12f * std::clamp(strength, 0.f, 1.f);
}

// One controller pulse: hand 0 left, 1 right, -1 both.
struct Pulse {
    int hand = -1;
    float amplitude = 0;
    float seconds = 0;
};

// The setting's scale (MOVE HAPTICS % / 100) on a pulse: amplitude capped at
// 1, the rest as length (up to 1.5x); 0 or less: no pulse.
inline Pulse scaled(Pulse p, float scale) {
    if (!(scale > 0) || !std::isfinite(scale) || !(p.amplitude > 0)) {
        p.amplitude = 0;
        return p;
    }
    const float a = p.amplitude * scale;
    if (a > 1.f) {
        p.seconds *= std::min(a, 1.5f);
        p.amplitude = 1.f;
    } else
        p.amplitude = a;
    p.seconds = std::clamp(p.seconds, .005f, .5f);
    return p;
}

// Pulses due later (the point jump's second thump).
struct Scheduler {
    struct Item {
        float at;
        Pulse pulse;
    };
    std::array<Item, 8> items{};
    size_t count = 0;
    float clock = 0;
    void add(float delay, Pulse p) {
        if (count < items.size())
            items[count++] = {clock + std::max(delay, 0.f), p};
    }
    // Advance by dt; calls play(Pulse) for each pulse now due.
    template <class Play> void update(float dt, Play&& play) {
        clock += std::isfinite(dt) ? std::max(dt, 0.f) : 0.f;
        for (size_t k = 0; k < count;) {
            if (items[k].at <= clock + 1e-5f) {
                play(items[k].pulse);
                items[k] = items[--count];
            } else
                ++k;
        }
    }
    void clear() {
        count = 0;
    }
};

// The point jump's double thump: {first now, second after `gap`}.
inline constexpr float pointGap = .11f;
inline Pulse pointFirst() {
    return {-1, 1.f, .07f};
}
inline Pulse pointSecond() {
    return {-1, .85f, .10f};
}

struct Input {
    float dt = 0;
    bool live = false;     // VR play (immersive)
    bool fresh = false;    // the swing data is current
    bool airborne = false; // game_swing::airborne
    bool onWall = false;   // swingState.wall != 0
    bool webbed = false;   // a web attached
    bool zipPressed = false; // a zip press began this frame
    Vec3 velocity{};       // world m/s
};

struct Event {
    Kind kind = Kind::none;
    float speed = 0; // launch speed or fall speed, m/s
    Pulse pulse{};   // unscaled
};

class Detector {
  public:
    // A point jump fired now (game_xr's swingPointJump success).
    void pointJump() {
        sincePoint_ = 0;
    }
    Event update(const Input& in) {
        Event e;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .25f) : 0.f;
        clock_ += dt;
        sinceZip_ = in.zipPressed ? 0.f : sinceZip_ + dt;
        sincePoint_ += dt;
        sinceTakeoff_ += dt;
        if (!in.live || !in.fresh || !finite(in.velocity)) {
            // Unknown: start over from the next known frame (no edges across a gap).
            known_ = false;
            history_ = 0;
            return e;
        }
        const bool inAir = in.airborne && !in.onWall;
        const bool supported = !inAir;
        const Vec3 v = in.velocity;
        push(v.y);
        const bool zipRecent = sinceZip_ <= zipRecentSeconds;
        // Classify a takeoff (on the edge, or while its up-speed builds over
        // the first takeoffOpenSeconds of the air phase).
        const auto takeoff = [&]() {
            const float speed = length(v);
            Kind kind = Kind::none;
            if (sincePoint_ <= pointJumpQuiet || sinceTakeoff_ < takeoffQuiet || in.webbed)
                return false;
            if (zipRecent && v.y >= jumpUp)
                kind = Kind::zipLaunch;
            else if (fromWall_ && speed >= wallSpeed)
                kind = Kind::wallJump;
            else if (v.y >= jumpUp)
                kind = Kind::jump;
            if (kind == Kind::none)
                return false;
            const float s = launchStrength(speed);
            e = {kind, speed, {-1, s, launchSeconds(s)}};
            sinceTakeoff_ = 0;
            airZipDone_ = kind == Kind::zipLaunch;
            return true;
        };
        if (known_) {
            if (wasSupported_ && inAir) {
                airSeconds_ = 0;
                airZipDone_ = false;
                fromWall_ = wasOnWall_;
                takeoffOpen_ = !takeoff();
            } else if (wasInAir_ && supported) {
                // Landing (or onto a wall).
                const float fall = fastestFall();
                if (airSeconds_ >= landingAir && fall >= landingFall) {
                    float s = landingStrength(fall);
                    const bool wall = in.onWall;
                    if (wall)
                        s *= wallLandingShare;
                    e = {wall ? Kind::wallLanding : Kind::landing, fall, {-1, s, landingSeconds(s)}};
                }
                airSeconds_ = 0;
                takeoffOpen_ = false;
            } else if (inAir) {
                airSeconds_ += dt;
                if (takeoffOpen_ && airSeconds_ <= takeoffOpenSeconds) {
                    if (takeoff())
                        takeoffOpen_ = false;
                } else {
                    takeoffOpen_ = false;
                    // A zip launch in the air: up-speed rising fast after a zip press.
                    if (!airZipDone_ && zipRecent && !in.webbed && v.y >= airZipUp &&
                        v.y - lowestUp() >= airZipRise && sincePoint_ > pointJumpQuiet) {
                        airZipDone_ = true;
                        const float speed = length(v);
                        const float s = launchStrength(speed);
                        e = {Kind::zipLaunch, speed, {-1, s, launchSeconds(s)}};
                        sinceTakeoff_ = 0;
                    }
                }
            }
        }
        known_ = true;
        wasSupported_ = supported;
        wasInAir_ = inAir;
        wasOnWall_ = in.onWall;
        return e;
    }
    float airSeconds() const {
        return airSeconds_;
    }

  private:
    struct Sample {
        float t, up;
    };
    void push(float up) {
        hist_[histHead_] = {clock_, up};
        histHead_ = (histHead_ + 1) % hist_.size();
        history_ = std::min(history_ + 1, hist_.size());
    }
    template <class F> void recent(float window, F&& f) const {
        for (size_t k = 1; k <= history_; ++k) {
            const Sample& s = hist_[(histHead_ + hist_.size() - k) % hist_.size()];
            if (clock_ - s.t > window + 1e-4f)
                break;
            f(s);
        }
    }
    // The fastest coming down over the last fallWindow (m/s, positive).
    float fastestFall() const {
        float most = 0;
        recent(fallWindow, [&](const Sample& s) { most = std::max(most, -s.up); });
        return most;
    }
    float lowestUp() const {
        float least = 1e9f;
        recent(airZipWindow, [&](const Sample& s) { least = std::min(least, s.up); });
        return least;
    }
    std::array<Sample, 128> hist_{};
    size_t histHead_ = 0, history_ = 0;
    float clock_ = 0, sinceZip_ = 1e9f, sincePoint_ = 1e9f, sinceTakeoff_ = 1e9f, airSeconds_ = 0;
    bool known_ = false, takeoffOpen_ = false, fromWall_ = false, wasSupported_ = true, wasInAir_ = false, wasOnWall_ = false, airZipDone_ = false;
};

} // namespace spidy::move_haptics
