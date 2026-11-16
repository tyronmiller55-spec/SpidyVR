#pragma once
// LANDING BODY TO VIEW (landing_body_to_view; the owner, Nov 15: "ease the body to my view"). After a landing the
// game's body faces the momentum and the walk stick then brings him round onto where the player looks (the facing
// hold); until he arrives the DRAWN body is turned about the head onto the view (native_body::setYieldTilt, the pose
// job only: no write to the game, the camera's place is the head's and does not move). The turn eases in, holds
// while the hero comes round (the offset shrinks with the gap), then eases out. Portable, pure.
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::landing_body {

inline constexpr float pi = 3.14159265f;
inline constexpr float skipAngle = 12.f * pi / 180.f;  // the facing hold's arrive angle: nothing to show below it
inline constexpr float rearAngle = 120.f * pi / 180.f; // facing_check::rearTurn (LANDING REAR TURN off: kept)
inline constexpr float graceSeconds = .6f;  // after easing in, how long to wait for the hero's turn to start
inline constexpr float holdMaxSeconds = 2.5f; // hybrid_movement::Heading::giveUpSeconds

inline float wrapPi(float a) {
    if (!std::isfinite(a))
        return 0.f;
    a = std::fmod(a + pi, 2.f * pi);
    if (a < 0)
        a += 2.f * pi;
    return a - pi;
}
inline float smooth01(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

enum class Phase : uint8_t { idle, in, hold, out };
enum class End : uint8_t { none, arrived, skippedSmall, skippedRear, driving, timedOut, cancelled, noTurn };
inline const char* endName(End e) {
    switch (e) {
    case End::arrived: return "he came round onto the view";
    case End::skippedSmall: return "skipped: he already faces the view";
    case End::skippedRear: return "skipped: the view is behind him (LANDING REAR TURN off)";
    case End::driving: return "the left stick runs him";
    case End::timedOut: return "timed out";
    case End::cancelled: return "cancelled";
    case End::noTurn: return "his turn never started";
    default: return "none";
    }
}

struct Ease {
    Phase phase = Phase::idle;
    float easeSeconds = .5f;
    float t = 0, held = 0, w = 0, wOut = 0;
    float offset = 0;     // radians, the drawn body's turn this frame (unwrapped against the last)
    float startGap = 0;   // the gap on the first frame
    bool first = false, sawTurn = false, rearAllowed = true;
    // LANDING BODY HOLDS THE TURN (landing_body_hold_turn): in the hold, "arrived" only once the gap is inside
    // skipAngle (a pause in the walk stick's turn is not the end); false: the turn's end as before.
    bool holdToArrival = false;
    uint32_t begun = 0;

    bool active() const {
        return phase != Phase::idle;
    }
    void begin(float easeS, bool rearTurn) {
        phase = Phase::in;
        easeSeconds = std::isfinite(easeS) ? std::clamp(easeS, .05f, 1.5f) : .5f;
        t = held = w = wOut = offset = startGap = lastGap = 0;
        outWhy_ = End::none;
        first = true;
        sawTurn = false;
        rearAllowed = rearTurn;
        ++begun;
    }
    struct Out {
        float yaw = 0;          // the drawn body's turn (radians, + the rig's yaw convention)
        bool started = false;   // the first frame: startGap is set
        End ended = End::none;  // it ended this frame (yaw is 0 then)
        End easingOut = End::none; // it began easing out this frame, and why
    };
    // `gap`: the view minus the hero's facing; `turning`: the walk stick is bringing him round (the facing hold);
    // `driving`: the player's left stick runs him; `cancel`: a takeover, the air spin, or the setting off.
    Out update(float dt, float gap, bool turning, bool driving, bool cancel) {
        Out o;
        if (phase == Phase::idle)
            return o;
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        auto stop = [&](End e) {
            phase = Phase::idle;
            w = offset = 0;
            o.yaw = 0;
            o.ended = e;
            return o;
        };
        if (cancel || !std::isfinite(gap))
            return stop(End::cancelled);
        float g = wrapPi(gap);
        if (first) {
            first = false;
            startGap = g;
            o.started = true;
            if (std::abs(g) < skipAngle)
                return stop(End::skippedSmall);
            if (!rearAllowed && std::abs(g) > rearAngle)
                return stop(End::skippedRear);
        } else if (std::abs(g - lastGap) > pi) {
            // the gap crossed +-180 (a rear landing): keep the same way round
            g += g < 0 ? 2.f * pi : -2.f * pi;
        }
        lastGap = g;
        sawTurn = sawTurn || turning;
        auto toOut = [&](End why) {
            phase = Phase::out;
            wOut = w;
            t = 0;
            outWhy_ = why;
            o.easingOut = why;
        };
        End outWhy = End::none;
        switch (phase) {
        case Phase::in:
            t += d;
            w = smooth01(t / easeSeconds);
            if (driving) {
                outWhy = End::driving;
                toOut(outWhy);
            } else if (t >= easeSeconds) {
                phase = Phase::hold;
                held = 0;
            }
            break;
        case Phase::hold:
            w = 1;
            held += d;
            if (driving)
                outWhy = End::driving;
            else if (holdToArrival ? sawTurn && std::abs(g) < skipAngle : sawTurn && !turning)
                outWhy = End::arrived;
            else if (!sawTurn && held > graceSeconds)
                outWhy = End::noTurn;
            else if (held > holdMaxSeconds)
                outWhy = End::timedOut;
            if (outWhy != End::none)
                toOut(outWhy);
            break;
        default:
            break;
        }
        if (phase == Phase::out) {
            t += d;
            w = wOut * (1.f - smooth01(t / easeSeconds));
            if (t >= easeSeconds)
                return stop(outWhy_);
        }
        offset = g * w;
        o.yaw = offset;
        return o;
    }
    End outWhy() const {
        return outWhy_;
    }

  private:
    End outWhy_ = End::none;
    float lastGap = 0;
};

// The drawn body's turn: about +Y, the rig's yaw convention (lookYaw(Quat::yaw(a).rotate(f)) = lookYaw(f) + a).
inline Quat turnOf(float yaw) {
    return std::isfinite(yaw) && yaw != 0.f ? Quat::yaw(yaw) : Quat{};
}

} // namespace spidy::landing_body
