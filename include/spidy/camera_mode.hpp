// CAMERA MODES (the user, November 13 15:42: "lock the headset to Spidey's
// face during animations ... walking and running around are its own system
// ... limit the player's head by Spidey's shoulders ... snap turn should be its
// own system too, and work in 360 degrees ... a reset that realigns
// everything"). Pure; no Windows. game_xr runs them on the view lock
// (view_yaw_lock.hpp), so every head anchor (the render thread's eyes, the
// hands' map, the arms' IK) takes them alike.
//
//   Mode::locomotion    Hybrid v2 (the heading the view holds, the gaze cone,
//                       the rear snap): walking, running, the air.
//   Mode::animHeadRide  a game animation (a landing, a perch, standing still):
//                       the view's yaw rides Spider-Man's head bone (its
//                       position, pitch and roll were the head's already),
//                       the headset's own turn on top to look around. Entry
//                       and exit are eased blends (HeadRide), never a pop.
//
// Hooks kept for camera modes stacked on top later (EngineBot's planned
// cinematic camera; nothing of it is built here): the mode is one explicit
// state (HeadRide::mode), the shoulder limit has a global switch
// (viewLimitActive), the snap/realign has a callable request
// (realignToHeading) and the right stick can be taken from it
// (snapInputYielded).
#pragma once
#include "spidy/stable_yaw.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace spidy::camera_mode {

using stable_yaw::degrees;
using stable_yaw::wrap;

enum class Mode : uint8_t { locomotion, animHeadRide };
inline const char* modeName(Mode m) {
    return m == Mode::animHeadRide ? "ANIM HEAD RIDE" : "LOCOMOTION";
}

// ---- Global hooks (read by game_xr every frame) ----
// The shoulder limit applies (another mode with the camera off the body sets
// it false while it is out). Default true.
inline std::atomic<bool> viewLimitActive{true};
// Another system has the right stick: the snap/realign reads no taps.
inline std::atomic<bool> snapInputYielded{false};
// A realign asked for from outside the snap (consumed by game_xr on its next
// frame, as a snap tap's): the heading, the model and the headset all
// re-synced to `yaw` (rig yaw, radians); NaN: to where the player looks now.
inline std::atomic<bool> realignAsked{false};
inline std::atomic<float> realignYaw{std::numeric_limits<float>::quiet_NaN()};
inline void realignToHeading(float yaw = std::numeric_limits<float>::quiet_NaN()) {
    realignYaw.store(yaw, std::memory_order_relaxed);
    realignAsked.store(true, std::memory_order_release);
}
// game_xr: takes a pending request (true once per request).
inline bool takeRealign(float& yaw) {
    if (!realignAsked.exchange(false, std::memory_order_acq_rel))
        return false;
    yaw = realignYaw.load(std::memory_order_relaxed);
    return true;
}

// ---- ANIM HEAD RIDE ----
// When the head ride is wanted (game_xr's facts, decided here so it is tested).
struct RideIn {
    bool enabled = true;    // ANIM HEAD RIDE (anim_head_ride)
    bool lockOn = false;    // the view lock runs (HYBRID, the head followed) and the head bone was read
    bool animOwns = false;  // the animation has the body (hybrid_owner's domain)
    bool fighting = false;  // a fight or a dodge: its own camera rules stay
    bool onWall = false;    // a wall or a crawl: the climb is locomotion
    bool turning = false;   // the right stick turns the heading
    bool legs = false;      // LEG CONTROL has the right stick
    bool snapHold = false;  // a snap/realign is bringing him round
    bool perched = false;   // still off the ground on a perch (game_xr's PerchWatch)
    bool landing = false;   // the animation began with a landing or a zip's arrival
    float sinceAnim = 1e9f; // seconds since the animation took the body
    float moveStick = 0;    // the walk stick's tilt, 0..1
    float speed = 0;        // horizontal, m/s
};
// A landing's own animation: ridden this long whatever the stick says, then
// the stick may take him into the run.
inline constexpr float landingSeconds = .6f, landingMinSeconds = .3f;
inline constexpr float stillStick = .2f, runStick = .3f, stillSpeed = 1.f;
enum class RideWhy : uint8_t { none, landing, perch, still };
inline const char* rideWhyName(RideWhy w) {
    switch (w) {
    case RideWhy::landing: return "landing";
    case RideWhy::perch: return "perch";
    case RideWhy::still: return "standing still";
    default: return "none";
    }
}
inline RideWhy rideWanted(const RideIn& in) {
    if (!in.enabled || !in.lockOn || !in.animOwns || in.fighting || in.onWall || in.turning || in.legs ||
        in.snapHold)
        return RideWhy::none;
    const bool inLanding = in.landing && in.sinceAnim < landingSeconds;
    const bool locked = in.landing && in.sinceAnim < landingMinSeconds;
    const float stick = std::isfinite(in.moveStick) ? in.moveStick : 0.f;
    if (stick >= runStick && !locked)
        return RideWhy::none; // the walk stick drives: locomotion
    if (inLanding)
        return RideWhy::landing;
    if (in.perched)
        return RideWhy::perch;
    if (stick < stillStick && std::isfinite(in.speed) && in.speed < stillSpeed)
        return RideWhy::still;
    return RideWhy::none;
}

// The mode and its blend: weight 0 (locomotion) .. 1 (riding the head),
// linear in time, shown eased (smoothstep).
struct HeadRide {
    static constexpr float enterSeconds = .25f, exitSeconds = .2f;
    // EASED BY GAP (G1 playtest: 37-52% of the landing rides started over 45 degrees between the view and the
    // head bone, 23-37% over 90: a 250 ms slew of up to ~130 degrees). The blend takes as long as the slew needs
    // at easeRate degrees a second of the smoothstep's peak (1.5 x the gap / the rate), never under the usual
    // 250 ms (a gap under 40 degrees: as it was) and never over easeMaxSeconds.
    static constexpr float easeRate = 240.f * (3.14159265f / 180.f), easeMaxSeconds = 1.2f;
    static float enterSecondsFor(float gap) {
        if (!std::isfinite(gap))
            return enterSeconds;
        return std::clamp(1.5f * std::abs(gap) / easeRate, enterSeconds, easeMaxSeconds);
    }
    Mode mode = Mode::locomotion;
    float weight = 0;
    float age = 0; // seconds in the current mode
    float enterTime = enterSeconds; // this ride's blend (enterSecondsFor the gap at its start)
    uint32_t enters = 0, exits = 0;
    struct Step {
        bool entered = false, exited = false;
    };
    // gap: view minus head bone, radians (any wrap), read when the ride starts.
    Step update(float dt, bool wanted, float gap = 0.f) {
        Step s;
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        age += d;
        if (wanted && mode != Mode::animHeadRide) {
            mode = Mode::animHeadRide;
            age = 0;
            enterTime = enterSecondsFor(wrap(gap));
            ++enters;
            s.entered = true;
        } else if (!wanted && mode == Mode::animHeadRide) {
            mode = Mode::locomotion;
            age = 0;
            ++exits;
            s.exited = true;
        }
        if (mode == Mode::animHeadRide)
            weight = std::min(1.f, weight + d / enterTime);
        else
            weight = std::max(0.f, weight - d / exitSeconds);
        return s;
    }
    float eased() const {
        const float w = std::clamp(weight, 0.f, 1.f);
        return w * w * (3 - 2 * w);
    }
    bool active() const {
        return mode == Mode::animHeadRide || weight > 0.f;
    }
    // At once to locomotion, no blend (a snap/realign: the frame jumps anyway).
    void reset() {
        if (mode == Mode::animHeadRide)
            ++exits;
        mode = Mode::locomotion;
        weight = 0;
        age = 0;
    }
};

// The shown yaw `base` (locomotion's) taken toward the head bone by `ride`
// (0..1), the short way round.
inline float rideYaw(float base, float bone, float ride) {
    if (!std::isfinite(base))
        return bone;
    if (!std::isfinite(bone) || !(ride > 0.f))
        return base;
    const float k = std::min(ride, 1.f);
    return wrap(base + k * wrap(bone - base));
}

// ---- SHOULDER LIMIT ----
// The view (shown yaw + the headset's turn) never turns past Spider-Man's
// shoulders: within 90 degrees of the way his chest faces. One clamp on this
// frame's published yaw, nothing fed back. The gap is unwrapped against last
// frame's SHOWN gap (always within the limit), so the side it holds on changes
// only once the head has really come round to the other shoulder - never the
// old rule's runaway (it unwrapped the raw gap: held up to 270 degrees off).
struct ShoulderLimit {
    static constexpr float limitDegrees = 90.f;
    float lastOut = 0;
    bool primed = false;
    unsigned held = 0;   // frames held (for the log)
    float worst = 0;     // the largest excess held, radians (for the log)
    unsigned chest = 0, hero = 0; // frames the shoulders came from the chest bone / the hero's facing
    unsigned streak = 0; // frames held in a row
    float streakWorst = 0;
    float correction(float viewYaw, float shouldersYaw) {
        if (!std::isfinite(viewYaw) || !std::isfinite(shouldersYaw)) {
            primed = false;
            streak = 0;
            return 0.f;
        }
        const float raw = wrap(viewYaw - shouldersYaw);
        const float d = primed ? lastOut + wrap(raw - lastOut) : raw;
        const float limit = limitDegrees * degrees;
        const float out = std::clamp(d, -limit, limit);
        lastOut = out;
        primed = true;
        const float c = out - d;
        if (c != 0.f) {
            ++held;
            ++streak;
            worst = std::max(worst, std::abs(c));
            streakWorst = std::max(streakWorst, std::abs(c));
        } else {
            streak = 0;
            streakWorst = 0;
        }
        return c;
    }
    void reset() {
        primed = false;
        lastOut = 0;
        streak = 0;
        streakWorst = 0;
    }
};
// Where the shoulders face: the animation's chest bone when it was read and
// is a body's twist from his facing (within 100 degrees), else his facing.
inline constexpr float chestTrustDegrees = 100.f;
inline float shouldersYaw(bool chestOk, float chestYaw, float heroYaw, bool* usedChest = nullptr) {
    const bool ok = chestOk && std::isfinite(chestYaw) &&
                    (!std::isfinite(heroYaw) || std::abs(wrap(chestYaw - heroYaw)) <= chestTrustDegrees * degrees);
    if (usedChest)
        *usedChest = ok;
    return ok ? chestYaw : heroYaw;
}

// ---- SNAP / REALIGN (its own system) ----
// The right stick's tap left or right: one snap of the heading (SNAP TURN
// degrees, 45 when that is 0), any number of times round the full 360, and
// every tap a realign (game_xr: heading, model and headset re-synced). With
// SMOOTH TURN on, a tap is out past 0.7 and back under 0.4 within 0.25 s (a
// longer push is the smooth turn, as before); without it, the flick fires on
// the way out (the old snap's feel). `turn`: radians, + left (the rig's yaw).
struct SnapRealign {
    static constexpr float on = .7f, off = .4f, tapSeconds = .25f;
    bool out = false;
    bool fired = false; // this push already fired (snap mode)
    float outFor = 0;
    float sign = 0;
    uint32_t taps = 0;
    struct Out {
        bool began = false; // a push went out (smooth mode: the view to come back to)
        bool fire = false;
        float turn = 0;
    };
    static float stepRadians(int snapDeg) {
        return static_cast<float>(snapDeg > 0 ? std::clamp(snapDeg, 5, 180) : 45) * degrees;
    }
    Out update(float dt, float x, bool smooth, int snapDeg) {
        Out o;
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const float v = std::isfinite(x) ? x : 0.f;
        if (!out) {
            if (std::abs(v) > on) {
                out = true;
                fired = false;
                outFor = 0;
                sign = v > 0 ? 1.f : -1.f;
                o.began = true;
                if (!smooth) {
                    fired = true;
                    o.fire = true;
                    o.turn = -sign * stepRadians(snapDeg);
                    ++taps;
                }
            }
            return o;
        }
        outFor += d;
        if (std::abs(v) < off) {
            out = false;
            if (smooth && !fired && outFor <= tapSeconds) {
                o.fire = true;
                o.turn = -sign * stepRadians(snapDeg);
                ++taps;
            }
        }
        return o;
    }
    void reset() {
        out = fired = false;
        outFor = 0;
        sign = 0;
    }
};

// ---- PERCH WATCH ----
// A perch the game holds him on (a pole, a ledge's edge after a zip): off its
// ground by its own flag yet still - no web, no wall, no swing driving. Still
// for `enterSeconds` is a perch; moving off it ends it at once.
struct PerchWatch {
    static constexpr float enterSeconds = .25f, stillSpeed = .4f, stillUp = .4f, leaveSpeed = 1.f,
                           leaveUp = 1.f;
    bool perched = false;
    float still = 0;
    uint32_t perches = 0;
    struct In {
        float dt = 0;
        bool airborne = false; // not on the game's ground
        bool free = false;     // no web, no wall, the swing not driving, no zip, no fight
        float speed = 0;       // horizontal, m/s
        float up = 0;          // vertical, m/s
    };
    struct Out {
        bool began = false, ended = false;
    };
    Out update(const In& in) {
        Out o;
        const float d = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        const bool known = std::isfinite(in.speed) && std::isfinite(in.up);
        const bool candidate = in.airborne && in.free && known;
        if (perched) {
            if (!candidate || in.speed > leaveSpeed || std::abs(in.up) > leaveUp) {
                perched = false;
                still = 0;
                o.ended = true;
            }
            return o;
        }
        if (candidate && in.speed < stillSpeed && std::abs(in.up) < stillUp) {
            still += d;
            if (still >= enterSeconds) {
                perched = true;
                ++perches;
                o.began = true;
            }
        } else {
            still = 0;
        }
        return o;
    }
    void reset() {
        perched = false;
        still = 0;
    }
};

} // namespace spidy::camera_mode
