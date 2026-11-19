// GROUND FLIPS (S2; the owner, Nov 18, t118u: "Flips can just be types of jumps that spin you at a specific rate from
// the ground. This should be tied into the flips system in general"; t119u: "let's do them all").
//
// On the ground under the animation, a FLICK of the right stick straight up or down (from under flickFrom to over
// flickTo within flickSeconds, within coneDegrees of straight up or down) followed by A within aSeconds is a flip
// jump: a backflip (down) or a front flip (up). The launch is the point jump's (the swing's SwingTakeoff: the game's
// own jump to unstick, then the launch velocity), and on his first airborne frame the spin goes to the flip mode that is
// on (the owner, Nov 18: "every flip mode", fights included): REALISTIC FLIPS (RealisticFlip::release, as the
// handstand's leap does) at ROTATION SPEED, with its falloff, arms, legs and A upright; CONTROLLED or basic FLIPS
// (FlipMotion::launch) one full turn at FLIP SPEED, uprighted on the landing. No second flip system, no canned rotation.
//
// Input rules (Trigger):
//  - A alone, or A after a slow push, is the game's A exactly as before (nothing holds A back).
//  - Sideways flicks are not flips: on the ground they are the snap turn's (a snap then a quick jump stays a jump).
//    Side flips wait for another trigger (plans/ground-moves-and-cancel.md, S2 note).
//  - B down, the legs engaged (Y+X), a blocked state or the cooldown: no flip; A goes on to the game (logged).
//  - The A that fires is consumed until it is let go of.
//  - Side flips: a directional A TAP with the left stick (TapTrigger, below); since Nov 18 1:09 PM ("Add forward and
//    back A-tap flips") forward and back taps too: a front flip and a backflip.
// Pure; no Windows.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::ground_flip {

enum class Kind { none, back, front, left, right };
inline const char* kindName(Kind k) {
    switch (k) {
    case Kind::back: return "backflip";
    case Kind::front: return "front flip";
    case Kind::left: return "side flip left";
    case Kind::right: return "side flip right";
    default: return "none";
    }
}
enum class Deny { none, blocked, legs, b, cooldown, moving };
inline const char* denyName(Deny d) {
    switch (d) {
    case Deny::blocked: return "blocked";
    case Deny::legs: return "the legs are engaged (Y+X)";
    case Deny::b: return "B held";
    case Deny::cooldown: return "cooldown";
    case Deny::moving: return "moving too fast (a running jump)";
    default: return "none";
    }
}

struct Params {
    float flickFrom = .3f, flickTo = .75f, flickSeconds = .15f, aSeconds = .15f;
    float coneDegrees = 35;
    float cooldownSeconds = .6f;
    // The launch, m/s: up, and along the look (back for a backflip, ahead for a front flip). About 1.3 s in the air at
    // the game's gravity: one turn at 270-360 deg/s.
    float upSpeed = 6.5f, backSpeed = 1.5f, frontSpeed = 2.5f, sideSpeed = 1.5f;
    float keepHorizontal = 8.f; // the run's speed kept, at most
};

class Trigger {
  public:
    struct Input {
        float seconds{};
        bool stickValid{};
        float stickX{}, stickY{}; // the right stick
        bool a{}, b{}, legs{};
        bool ground{};  // on the ground, the animation's
        bool blocked{}; // anything else that refuses (logged by the caller)
    };
    struct Out {
        Kind fire = Kind::none;
        bool a{}; // A to pass on
        Deny denied = Deny::none;
    };
    Out update(const Input& in, const Params& p) {
        Out out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        clock_ += dt;
        // The right stick's flick.
        if (!in.stickValid || !std::isfinite(in.stickX) || !std::isfinite(in.stickY)) {
            armed_ = false;
            restAt_ = -1;
        } else {
            const float tilt = std::hypot(in.stickX, in.stickY);
            if (tilt < p.flickFrom) {
                restAt_ = clock_;
                armed_ = true;
            } else if (armed_ && tilt > p.flickTo) {
                armed_ = false;
                if (restAt_ >= 0 && clock_ - restAt_ <= p.flickSeconds + 1e-5f) {
                    const float off = std::atan2(std::abs(in.stickX), std::abs(in.stickY)) * 57.29578f;
                    if (off <= p.coneDegrees) {
                        flick_ = in.stickY > 0 ? Kind::front : Kind::back;
                        flickAt_ = clock_;
                    }
                }
            }
        }
        const bool rising = in.a && !aWas_;
        aWas_ = in.a;
        if (consuming_) {
            if (!in.a)
                consuming_ = false;
            return out; // the firing A stays the flip's until let go of
        }
        out.a = in.a;
        const bool recent = flick_ != Kind::none && clock_ - flickAt_ <= p.aSeconds + 1e-5f;
        if (!rising || !recent || !in.ground)
            return out;
        const Kind kind = flick_;
        flick_ = Kind::none;
        if (in.blocked)
            out.denied = Deny::blocked;
        else if (in.legs)
            out.denied = Deny::legs;
        else if (in.b)
            out.denied = Deny::b;
        else if (lastFire_ >= 0 && clock_ - lastFire_ < p.cooldownSeconds)
            out.denied = Deny::cooldown;
        if (out.denied != Deny::none)
            return out;
        out.fire = kind;
        out.a = false;
        consuming_ = true;
        lastFire_ = clock_;
        return out;
    }
    // The swing refused the launch: the cooldown does not start.
    void refused() {
        lastFire_ = -1;
    }

  private:
    float clock_{}, restAt_ = -1, flickAt_ = -1, lastFire_ = -1;
    bool armed_{}, aWas_{}, consuming_{};
    Kind flick_ = Kind::none;
};

// SIDE FLIPS BY A DIRECTIONAL A TAP (the owner, Nov 18: "Just make it directional A taps"). Standing (slower than
// maxSpeed, so a running or strafing jump is never one), the LEFT stick held to a side (at least `deflect`, between
// sideMinDegrees and sideMaxDegrees off straight ahead, the sidestep's side band) for steadySeconds, then a quick A TAP
// (let go of within tapSeconds) is a side flip that way. Held longer, the A goes on to the game as a jump (late by
// tapSeconds: only in this standing, stick-held case). A fresh flick then A is QUICK SIDESTEP's (it runs first and
// takes the A).
// FORWARD AND BACK TAPS (the owner, Nov 18 1:09 PM): the same rules, the stick held within straightDegrees of straight
// ahead (a front flip) or straight back (a backflip), slower than straightMaxSpeed (2 m/s: a running jump - the stick
// held ahead a while - is past it, and a stick only just pushed has not held steadySeconds: both stay plain jumps).
// HYBRID's rear snap: a back stick at the snap's tilt or more (0.5) turns the view within 60 ms, so where the rear snap
// is live (`rearSnap`) the back tap counts only a held back stick UNDER the snap's tilt (backSoftMin..backSoftMax: there
// HYBRID neither snaps nor moves him), and never while the snap holds (`snapHeld`: he runs on the snapped heading).
// Elsewhere (STABLE, OLD, HEADSET FIRST) the back stick at `deflect` counts.
struct TapParams {
    float deflect = .8f, steadySeconds = .15f, tapSeconds = .18f, maxSpeed = 3.f;
    float sideMinDegrees = 55, sideMaxDegrees = 100;
    float cooldownSeconds = .6f;
    float straightDegrees = 25, straightMaxSpeed = 2.f;
    float backSoftMin = .3f, backSoftMax = .5f; // HYBRID's rear snap fires at 0.5 (hybrid_movement::snapTilt)
};
class TapTrigger {
  public:
    struct Input {
        float seconds{};
        bool stickValid{};
        float stickX{}, stickY{}; // the left stick (x right, y ahead)
        bool a{}, b{}, legs{};
        bool ground{}, blocked{};
        float speed{}; // horizontal, m/s (NaN: refused)
        bool sides = true, straight = true; // DIRECTIONAL FLIPS: side taps; forward and back taps
        bool rearSnap{}, snapHeld{};        // HYBRID's rear snap live; it has snapped and holds (see above)
    };
    struct Out {
        Kind fire = Kind::none;
        bool a{};       // A to pass on
        bool late{};    // this A is a held-back press let go on to the game (held past tapSeconds)
        bool waiting{}; // an A is held back to see whether it is a tap
        Deny denied = Deny::none;
        float speed{};
        float limit{}; // F4: the speed limit this tap's side had (m/s)
    };
    Out update(const Input& in, const TapParams& p) {
        Out out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        clock_ += dt;
        Kind side = Kind::none; // the held direction (any of the four)
        if (in.stickValid && std::isfinite(in.stickX) && std::isfinite(in.stickY)) {
            const float tilt = std::hypot(in.stickX, in.stickY);
            const float off = std::atan2(std::abs(in.stickX), in.stickY) * 57.29578f; // 0 ahead, 180 behind
            if (in.sides && tilt >= p.deflect && off >= p.sideMinDegrees && off <= p.sideMaxDegrees)
                side = in.stickX > 0 ? Kind::right : Kind::left;
            else if (in.straight && tilt >= p.deflect && off <= p.straightDegrees)
                side = Kind::front;
            else if (in.straight && off >= 180 - p.straightDegrees && !in.snapHeld &&
                     (in.rearSnap ? tilt >= p.backSoftMin && tilt < p.backSoftMax : tilt >= p.deflect))
                side = Kind::back;
        }
        steady_ = side != Kind::none && side == side_ ? steady_ + dt : 0.f;
        side_ = side;
        const bool rising = in.a && !aWas_;
        aWas_ = in.a;
        if (consuming_) {
            if (!in.a)
                consuming_ = false;
            return out;
        }
        if (pending_ != Kind::none) {
            pendingAge_ += dt;
            if (!in.a) { // let go of in time: the tap
                out.fire = pending_;
                pending_ = Kind::none;
                lastFire_ = clock_;
                return out;
            }
            if (pendingAge_ > p.tapSeconds + 1e-5f) { // held: the game's jump after all
                pending_ = Kind::none;
                out.a = out.late = true;
                return out;
            }
            out.waiting = true;
            return out;
        }
        out.a = in.a;
        if (!rising || !in.ground || side == Kind::none || steady_ < p.steadySeconds - 1e-5f)
            return out;
        out.speed = in.speed;
        out.limit = side == Kind::front || side == Kind::back ? p.straightMaxSpeed : p.maxSpeed;
        if (in.blocked)
            out.denied = Deny::blocked;
        else if (in.legs)
            out.denied = Deny::legs;
        else if (in.b)
            out.denied = Deny::b;
        else if (!(in.speed <= out.limit))
            out.denied = Deny::moving;
        else if (lastFire_ >= 0 && clock_ - lastFire_ < p.cooldownSeconds)
            out.denied = Deny::cooldown;
        if (out.denied != Deny::none)
            return out;
        pending_ = side;
        pendingAge_ = 0;
        out.a = false;
        out.waiting = true;
        return out;
    }
    void refused() {
        lastFire_ = -1;
    }

  private:
    float clock_{}, steady_{}, pendingAge_{}, lastFire_ = -1;
    Kind side_ = Kind::none, pending_ = Kind::none;
    bool aWas_{}, consuming_{};
};

// The look laid level (world axes; -Z when it points straight up or down).
inline Vec3 levelForward(Vec3 look) {
    Vec3 f{look.x, 0, look.z};
    if (!finite(f) || length(f) < .3f)
        return {0, 0, -1};
    return normalized(f);
}
// The flip's spin, world axes, rad/s, at the flip system's ROTATION SPEED (degrees a second): about the level right
// axis, nose up for a backflip and down for a front flip (the convention of the realistic flips' stick turn).
inline Vec3 spin(Kind k, Vec3 look, float degreesPerSecond) {
    if (k == Kind::none || !std::isfinite(degreesPerSecond))
        return {};
    const Vec3 f = levelForward(look);
    const Vec3 right{-f.z, 0, f.x};
    const float rate = std::clamp(degreesPerSecond, 0.f, 720.f) * .017453293f;
    // A side flip (cartwheel) turns about the level forward axis, toward its side (the stick turn's convention: the
    // stick right is +forward).
    if (k == Kind::right)
        return f * rate;
    if (k == Kind::left)
        return f * -rate;
    return right * (k == Kind::back ? rate : -rate);
}
// The launch velocity, world axes: up, along the look (back or ahead), the run's horizontal speed kept (capped).
inline Vec3 launch(Kind k, Vec3 look, Vec3 before, const Params& p) {
    const Vec3 f = levelForward(look);
    Vec3 h = finite(before) ? Vec3{before.x, 0, before.z} : Vec3{};
    if (length(h) > p.keepHorizontal)
        h = normalized(h) * p.keepHorizontal;
    const float along = k == Kind::back ? -p.backSpeed : k == Kind::front ? p.frontSpeed : 0.f;
    const Vec3 right{-f.z, 0, f.x};
    const float aside = k == Kind::right ? p.sideSpeed : k == Kind::left ? -p.sideSpeed : 0.f;
    return h + f * along + right * aside + Vec3{0, p.upSpeed, 0};
}
// A turn's worth of air: how long a launch of upSpeed stays up (s) at gravity g.
inline float airSeconds(float upSpeed, float g = 9.8f) {
    return upSpeed > 0 && g > 0 ? 2 * upSpeed / g : 0.f;
}

} // namespace spidy::ground_flip
