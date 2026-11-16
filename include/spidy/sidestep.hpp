// QUICK SIDESTEP (the user, November 12: "something quicker and upright with
// no flipping, for fast lateral movement. The standard dodge is too long";
// approved Nov 12 with SpidyVR owning the ground for the burst). EngineBot's
// "SpidyVR - Quick Sidestep & Ground Movement Cleanup" page.
//
// On the ground, under the animation, a FLICK of the left stick (from under
// flickFrom to over flickTo within flickSeconds) followed by A within
// aSeconds is a short upright burst the way the stick was flicked: about
// `distance` metres in `seconds`, at a flat speed, then a hard stop. No flip,
// no roll, no camera rotation. B stays the game's own dodge.
//
// Input rules (Trigger):
//  - A alone, or A with a stick that was not FLICKED in the last aSeconds
//    (a running jump: the stick held steady, then A), is the game's A exactly
//    as before: nothing here holds A back, so a jump gets no added delay.
//  - The flick must come first (or on the same frame as A). A flick after A
//    is a turn, not a sidestep.
//  - Flicks inside forwardConeDegrees of straight ahead are not sidesteps
//    unless `forward` is on (default off): a run started and jumped within
//    120 ms stays a jump.
//  - B down (A+B point jump, hold B then A, the game's dodge) or a point jump
//    chord under way: no sidestep; the point jump's chord gate has A first.
//  - Y+X leg hold engaged: no sidestep.
//  - The A that fires a sidestep is consumed until it is let go of (no jump,
//    no charged leap). A new press during the burst is held until the burst
//    ends and then reaches the game as a fresh press (a buffered jump).
//  - Cooldown cooldownSeconds after a burst ends; chain bursts within
//    chainWindowSeconds of each other, then lockoutSeconds before the next.
//
// Pure; no Windows.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::sidestep {

struct Params {
    bool enabled = true;
    float distance = 2.f;     // metres
    float seconds = .18f;     // the whole burst, the hard stop included
    float stopSeconds = .04f; // the hard stop at its end (zero speed across)
    float cooldownSeconds = .3f;
    int chain = 3; // bursts in a row before the lockout
    float chainWindowSeconds = .5f, lockoutSeconds = .8f;
    float flickFrom = .3f, flickTo = .8f, flickSeconds = .08f, aSeconds = .12f;
    bool forward = false; // flicks straight ahead count too
    float forwardConeDegrees = 35;
    // HYBRID v2 (Nov 13, with EngineBot): only SIDEWAYS flicks are sidesteps - the flick's
    // way between sidewaysMinDegrees and sidewaysMaxDegrees off straight ahead, either side.
    // A diagonal (under 55) is the free front-half run starting, and the rear (over 100,
    // HYBRID's snapAngle) is the rear snap's: a flick there turns the view, and a sidestep
    // read off the turned heading would go the wrong way. EngineBot proposed 55-125; the
    // top is held at the snap line so the two never overlap. `forward` lifts both limits.
    bool sidewaysOnly = true;
    float sidewaysMinDegrees = 55, sidewaysMaxDegrees = 100;
    bool iframes = true; // the game's dodge (game_dodge, the web dodge's hook) answers warned blows
};

// The stick's dead zone the flick's start must clear nothing of: the flick
// starts at rest or near it, under flickFrom, and flickFrom is kept above the
// steering dead zone (0.15) so stick noise is never a flick's start.
constexpr float stickDeadZone = .15f;

// The Values (vr_settings) a Params is read from; generic so this header stays
// free of the settings catalog.
template <class V> Params paramsOf(const V& v) {
    Params p;
    p.enabled = v.sidestep;
    p.distance = std::clamp(v.sidestepDistanceCm, 100, 300) / 100.f;
    p.seconds = std::clamp(v.sidestepMs, 120, 300) / 1000.f;
    p.cooldownSeconds = std::clamp(v.sidestepCooldownMs, 0, 2000) / 1000.f;
    p.chain = std::clamp(v.sidestepChain, 1, 10);
    p.lockoutSeconds = std::clamp(v.sidestepLockoutMs, 0, 3000) / 1000.f;
    p.flickFrom = std::clamp(v.sidestepFlickFrom, 20, 60) / 100.f;
    p.flickTo = std::max(p.flickFrom + .2f, std::clamp(v.sidestepFlickTo, 50, 95) / 100.f);
    p.flickSeconds = std::clamp(v.sidestepFlickMs, 30, 200) / 1000.f;
    p.aSeconds = std::clamp(v.sidestepAMs, 40, 250) / 1000.f;
    p.forward = v.sidestepForward;
    p.iframes = v.sidestepIframes;
    return p;
}

// Left-stick flick detector: the stick from under `from` (and inside the
// steering dead zone's neighbourhood) to over `to` within `window` seconds.
// Its way is the stick's at the frame it crossed `to`.
class Flick {
  public:
    struct Out {
        bool now{};     // a flick completed this frame
        float x{}, y{}; // its way (unit), stick axes (x right, y forward)
    };
    Out update(float seconds, bool valid, float x, float y, const Params& p) {
        Out out;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        clock_ += dt;
        sinceFlick_ += dt;
        if (!valid || !std::isfinite(x) || !std::isfinite(y)) {
            restAt_ = -1;
            armed_ = false;
            return out;
        }
        const float tilt = std::hypot(x, y);
        const float from = std::max(p.flickFrom, stickDeadZone + .05f);
        if (tilt < from) {
            restAt_ = clock_; // the last moment it was low
            armed_ = true;
            return out;
        }
        if (armed_ && tilt > p.flickTo) {
            armed_ = false; // one flick per return under `from`
            if (restAt_ >= 0 && clock_ - restAt_ <= p.flickSeconds + 1e-5f) {
                out.now = true;
                out.x = x / tilt;
                out.y = y / tilt;
                lastX_ = out.x;
                lastY_ = out.y;
                sinceFlick_ = 0;
            }
        }
        return out;
    }
    // Seconds since the last completed flick (large: none yet).
    float since() const {
        return sinceFlick_;
    }
    float x() const {
        return lastX_;
    }
    float y() const {
        return lastY_;
    }
    void reset() {
        *this = {};
    }

  private:
    float clock_{}, restAt_ = -1, sinceFlick_ = 1e9f, lastX_{}, lastY_{};
    bool armed_{};
};

// Why a sidestep did not happen (diagnostics).
enum class Deny : uint8_t {
    none,
    off,
    notGround,
    owner,
    blocked,
    pointJump,
    legs,
    chord,
    cooldown,
    lockout,
    forward,
    notSideways
};
inline const char* denyName(Deny d) {
    switch (d) {
    case Deny::off:
        return "off";
    case Deny::notGround:
        return "not on the ground";
    case Deny::owner:
        return "not the animation's ground";
    case Deny::blocked:
        return "blocked";
    case Deny::pointJump:
        return "point jump";
    case Deny::legs:
        return "leg hold";
    case Deny::chord:
        return "B held";
    case Deny::cooldown:
        return "cooldown";
    case Deny::lockout:
        return "chain lockout";
    case Deny::forward:
        return "forward flick (a running jump)";
    case Deny::notSideways:
        return "not sideways (55-100 deg off ahead)";
    default:
        return "none";
    }
}

// The trigger, the A it consumes, the burst's clock, the cooldown and chain.
class Trigger {
  public:
    struct Input {
        float seconds{};
        bool stickValid{};
        float stickX{}, stickY{}; // the left stick
        bool a{};                 // A as the point jump's chord gate passed it on
        bool b{};                 // B held (raw, before any chord)
        bool groundAnim{};        // grounded and the animation owns him (hybrid)
        bool blocked{};           // out of play, a prompt, a vent, a line, a crawl, a takeoff ...
        bool pointJump{};         // the point jump's chord is pending or charging
        bool legs{};              // Y+X leg hold engaged
    };
    struct Out {
        bool fire{};    // start a burst this frame
        float x{}, y{}; // its way, stick axes (unit)
        bool a{};       // A as it goes on to the game and the rest of the mod
        bool active{};  // a burst under way (its move or its stop)
        bool ended{};   // the burst's time ran out this frame
        Deny denied{};  // A pressed after a flick but no sidestep: why
    };
    Out update(const Input& in, const Params& p) {
        Out out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const auto flick = flick_.update(dt, in.stickValid, in.stickX, in.stickY, p);
        (void)flick;
        sinceEnd_ += dt;
        sinceStart_ += dt;
        if (active_) {
            burst_ += dt;
            if (burst_ >= p.seconds) {
                active_ = false;
                out.ended = true;
                sinceEnd_ = 0;
            }
        }
        const bool pressed = in.a && !aWas_;
        aWas_ = in.a;
        if (!in.a)
            eatA_ = false;
        if (pressed && !active_ && !eatA_) {
            const bool flicked = flick_.since() <= p.aSeconds + 1e-5f;
            if (flicked) {
                const Deny why = deny(in, p);
                if (why == Deny::none) {
                    out.fire = true;
                    out.x = flick_.x();
                    out.y = flick_.y();
                    active_ = true;
                    burst_ = 0;
                    eatA_ = true;
                    chain_ = sinceStart_ <= p.chainWindowSeconds + p.seconds ? chain_ + 1 : 1;
                    sinceStart_ = 0;
                    ++fired_;
                    flick_.reset(); // one A per flick
                } else {
                    out.denied = why;
                }
            }
        }
        out.active = active_;
        // The firing press, and any A while the burst lasts, do not reach the game.
        out.a = in.a && !eatA_ && !active_;
        return out;
    }
    bool active() const {
        return active_;
    }
    float burstSeconds() const {
        return burst_;
    }
    uint32_t fired() const {
        return fired_;
    }
    int chain() const {
        return chain_;
    }
    // A burst the swing could not take (refused, or ended early by an edge or
    // a wall): its clock stops; cooldown from now.
    void stop() {
        if (active_) {
            active_ = false;
            sinceEnd_ = 0;
        }
    }
    void reset() {
        *this = {};
    }

  private:
    Deny deny(const Input& in, const Params& p) const {
        if (!p.enabled)
            return Deny::off;
        if (in.blocked)
            return Deny::blocked;
        if (in.pointJump)
            return Deny::pointJump;
        if (in.b)
            return Deny::chord;
        if (in.legs)
            return Deny::legs;
        if (!in.groundAnim)
            return Deny::owner;
        if (chain_ >= p.chain && sinceStart_ <= p.chainWindowSeconds + p.seconds + p.lockoutSeconds &&
            sinceEnd_ < p.lockoutSeconds)
            return Deny::lockout;
        if (sinceEnd_ < p.cooldownSeconds)
            return Deny::cooldown;
        if (!p.forward && std::isfinite(flick_.y()) && flick_.y() > 0 &&
            std::atan2(std::abs(flick_.x()), flick_.y()) < p.forwardConeDegrees * 3.14159265f / 180.f)
            return Deny::forward;
        if (!p.forward && p.sidewaysOnly && std::isfinite(flick_.x()) && std::isfinite(flick_.y())) {
            const float off = std::atan2(std::abs(flick_.x()), flick_.y()) * 180.f / 3.14159265f;
            if (off < p.sidewaysMinDegrees || off > p.sidewaysMaxDegrees)
                return Deny::notSideways;
        }
        return Deny::none;
    }
    Flick flick_{};
    bool active_{}, aWas_{}, eatA_{};
    float burst_{}, sinceEnd_ = 1e9f, sinceStart_ = 1e9f;
    int chain_{};
    uint32_t fired_{};
};

// The burst's plan: a flat speed for (seconds - stopSeconds), then zero speed
// across for stopSeconds (the hard stop). `way` is level and unit.
struct Burst {
    Vec3 velocity{}; // across the ground, m/s
    float moveSeconds{}, stopSeconds{};
};
inline Burst plan(Vec3 way, const Params& p) {
    Burst b;
    Vec3 level{way.x, 0, way.z};
    if (!finite(level) || length(level) < 1e-3f)
        return b;
    level = normalized(level);
    b.stopSeconds = std::clamp(p.stopSeconds, 0.f, p.seconds * .5f);
    b.moveSeconds = std::max(p.seconds - b.stopSeconds, .05f);
    b.velocity = level * (p.distance / b.moveSeconds);
    return b;
}
// The stick's way (x right, y forward) in the world, level, from a view yaw
// (0 = facing -Z, as hybrid_steer::viewForward / viewRight).
inline Vec3 worldWay(float x, float y, float viewYaw) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(viewYaw))
        return {};
    const Vec3 forward = Quat::yaw(viewYaw).rotate({0, 0, -1});
    const Vec3 right = Quat::yaw(viewYaw).rotate({1, 0, 0});
    const Vec3 w = right * x + forward * y;
    return length(w) > 1e-3f ? normalized(Vec3{w.x, 0, w.z}) : Vec3{};
}

// Swing side: where the burst must stop early. `floorAhead`: the ground
// found under the feet `probe` m ahead within `maxDrop` below them (an edge
// when not); `wallAhead`: something solid across the way within reach.
enum class Stop : uint8_t { none, time, edge, wall, air, refused, cancelled };
inline const char* stopName(Stop s) {
    switch (s) {
    case Stop::time:
        return "time";
    case Stop::edge:
        return "edge";
    case Stop::wall:
        return "wall";
    case Stop::air:
        return "left the ground";
    case Stop::refused:
        return "refused";
    case Stop::cancelled:
        return "cancelled";
    default:
        return "none";
    }
}
constexpr float edgeProbe = .35f, edgeMaxDrop = .5f, wallReach = .75f;

} // namespace spidy::sidestep
