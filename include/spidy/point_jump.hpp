// POINT JUMP (milestone 1; the user, November 11, and EngineBot's plan
// "SpidyVR - Handstand & Point Jump Plan"): whenever the player touches a
// surface (the ground, a perch, Spidy's own wall) he can charge a jump that
// launches him the way he LOOKS. A tap is a quick jump; held up to CHARGE
// TIME a bigger leap. It reuses the charged leap's timing and haptics and the
// swing's takeoff (the game's own jump to unstick, then the launch velocity:
// game_swing's SwingTakeoff, as a point launch or a charged leap do), never
// the game's point launch and so never its smoke.
//
// The trigger (point_jump_trigger):
//  0  A+B together (default): the second button within chordSeconds of the
//     first. While one waits for the other it is held back from the game at
//     most chordSeconds; a button let go of inside that window still reaches
//     the game as a tap. A spider-sense warning sends B at once (the dodge).
//  1  hold B, then A: B pressed and held waits for A. B let go of with no A
//     is the dodge on the release; B held dodgeHoldSeconds with no A is the
//     dodge then. A spider-sense warning makes B the dodge at once. A alone
//     is never held back.
// Once both are down (the chord) neither reaches the game until both are let
// go of; letting go of either fires. A grip cancels the charge.
//
// Charge: held <= tapSeconds is a tap at minSpeed; then u = (held - tap) /
// (full - tap), c = 1 - (1 - u)^2 (ease-out; curve 1: c = u), speed = min +
// (max - min) c.
// Direction: d = normalize(look + normalBias n), then at least minOutDegrees
// out of the surface (n the surface's outward normal): d = n sin(a) + t cos(a)
// along d's own tangent t when d.n < sin(a). Never into the surface.
// Velocity: v = d speed + keep (v_old - d (d.v_old)), the kept part never
// into the surface.
// Gate (Controller): contact now or within coyoteSeconds; a release with no
// contact waits bufferSeconds for one; cooldownSeconds after a launch and a
// fresh contact (he must have left the surface) before the next. While the
// game's zip is asked for, until he lands from it, and zipGuardSeconds after
// that landing, a release waits (EngineBot's
// Q1: the takeoff's own jump press must not land in the game's point-launch
// window and become its launch, smoke and all).
// Pure; no Windows.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::point_jump {

struct Params {
    bool enabled = false;
    int trigger = 0; // 0 A+B together, 1 hold B then A
    float chordSeconds = .05f, tapSeconds = .15f, fullSeconds = 1.f;
    float minSpeed = 9, maxSpeed = 22;
    int curve = 0; // 0 ease-out, 1 linear
    float normalBias = .25f, minOutDegrees = 10, keep = .25f;
    float coyoteSeconds = .12f, bufferSeconds = .15f, cooldownSeconds = .25f;
    float zipGuardSeconds = .4f, dodgeHoldSeconds = .25f;
    bool inCombat = true;
};

// The Values (vr_settings) a Params is read from; generic so this header stays
// free of the settings catalog.
template <class V> Params paramsOf(const V& v) {
    Params p;
    p.enabled = v.pointJump;
    p.trigger = std::clamp(v.pointJumpTrigger, 0, 1);
    p.chordSeconds = std::clamp(v.pointJumpChordMs, 20, 150) / 1000.f;
    p.tapSeconds = std::clamp(v.pointJumpTapMs, 50, 400) / 1000.f;
    p.fullSeconds = std::max(p.tapSeconds + .1f, std::clamp(v.pointJumpFullMs, 300, 2000) / 1000.f);
    p.minSpeed = static_cast<float>(std::clamp(v.pointJumpMinSpeed, 3, 20));
    p.maxSpeed = std::max(p.minSpeed, static_cast<float>(std::clamp(v.pointJumpMaxSpeed, 5, 40)));
    p.curve = std::clamp(v.pointJumpCurve, 0, 1);
    p.normalBias = std::clamp(v.pointJumpNormalBias, 0, 100) / 100.f;
    p.minOutDegrees = static_cast<float>(std::clamp(v.pointJumpMinOutDegrees, 0, 45));
    p.keep = std::clamp(v.pointJumpKeep, 0, 100) / 100.f;
    p.coyoteSeconds = std::clamp(v.pointJumpCoyoteMs, 0, 500) / 1000.f;
    p.bufferSeconds = std::clamp(v.pointJumpBufferMs, 0, 500) / 1000.f;
    p.cooldownSeconds = std::clamp(v.pointJumpCooldownMs, 0, 2000) / 1000.f;
    p.zipGuardSeconds = std::clamp(v.pointJumpZipGuardMs, 0, 2000) / 1000.f;
    p.dodgeHoldSeconds = std::clamp(v.pointJumpDodgeHoldMs, 100, 600) / 1000.f;
    p.inCombat = v.pointJumpInCombat;
    return p;
}

// 0 for a tap (held <= tapSeconds), up to 1 at fullSeconds.
inline float charge(float held, const Params& p) {
    if (!std::isfinite(held) || held <= p.tapSeconds)
        return 0.f;
    const float span = std::max(p.fullSeconds - p.tapSeconds, 1e-3f);
    const float u = std::clamp((held - p.tapSeconds) / span, 0.f, 1.f);
    return p.curve == 1 ? u : 1.f - (1.f - u) * (1.f - u);
}
inline float speed(float held, const Params& p) {
    return p.minSpeed + (p.maxSpeed - p.minSpeed) * charge(held, p);
}

struct Direction {
    Vec3 way{0, 1, 0};
    float outFixDegrees = 0; // how far it was turned out of the surface (0: not)
};
// The launch's way: the look, a little out of the surface, never into it.
inline Direction direction(Vec3 look, Vec3 normal, const Params& p) {
    const Vec3 n = finite(normal) && length(normal) > .5f ? normalized(normal) : Vec3{0, 1, 0};
    const Vec3 l = finite(look) && length(look) > .5f ? normalized(look) : n;
    Direction out;
    Vec3 d = l + n * p.normalBias;
    d = length(d) > 1e-4f ? normalized(d) : n;
    const float radians = std::clamp(p.minOutDegrees, 0.f, 89.f) * 3.14159265f / 180.f;
    const float s = std::sin(radians);
    const float along = dot(d, n);
    if (along < s) {
        Vec3 t = d - n * along;
        if (length(t) < 1e-4f) {
            // Straight into the surface: straight out of it.
            out.way = n;
            out.outFixDegrees = std::acos(std::clamp(along, -1.f, 1.f)) * 180.f / 3.14159265f;
            return out;
        }
        t = normalized(t);
        const Vec3 fixed = n * s + t * std::cos(radians);
        out.outFixDegrees = std::acos(std::clamp(dot(fixed, d), -1.f, 1.f)) * 180.f / 3.14159265f;
        d = normalized(fixed);
    }
    out.way = d;
    return out;
}
// The launch velocity: along d at `speed`, with `keep` of what the body had
// across d (never into the surface).
inline Vec3 launchVelocity(Vec3 d, float speedNow, Vec3 before, Vec3 normal, const Params& p) {
    const Vec3 n = finite(normal) && length(normal) > .5f ? normalized(normal) : Vec3{0, 1, 0};
    Vec3 kept = finite(before) ? before - d * dot(d, before) : Vec3{};
    if (dot(kept, n) < 0)
        kept = kept - n * dot(kept, n);
    return d * speedNow + kept * std::clamp(p.keep, 0.f, 1.f);
}

// The A+B (or B-then-A) chord and what of A and B reaches the game.
class Chord {
  public:
    struct Input {
        float seconds{};
        bool a{}, b{};        // the controllers' A and B
        bool allowed{};       // a chord may start now (Controller::allowed)
        bool warning{};       // a spider-sense warning stands: B is the dodge at once
        int mode{};           // Params::trigger
        float window = .05f;  // Params::chordSeconds
        float dodgeHold = .25f; // Params::dodgeHoldSeconds
    };
    struct Output {
        bool a{}, b{};         // what reaches the game (and the rest of the mod)
        bool began{};          // the chord closed this frame
        bool released{};       // ... and was let go of this frame: fire
        bool cancelled{};      // a charge cancelled this frame (cancel())
        float held{};          // the chord's time held (began..released)
        float gap{};           // the time between the two presses
        bool aFirst{};         // which came first (mode 0)
    };
    enum class Phase { idle, waitA, waitB, chord, spent };

    Output update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const bool aPressed = in.a && !aWas_, bPressed = in.b && !bWas_;
        aWas_ = in.a;
        bWas_ = in.b;
        Output out;
        if (cancelNow_) {
            out.cancelled = phase_ == Phase::chord;
            if (phase_ == Phase::chord || phase_ == Phase::waitA || phase_ == Phase::waitB)
                phase_ = Phase::spent;
            cancelNow_ = false;
        }
        aPass_ = aPass_ && in.a;
        bPass_ = bPass_ && in.b;
        bool tapA{}, tapB{};
        switch (phase_) {
        case Phase::chord:
            held_ += dt;
            out.held = held_;
            if (!in.a || !in.b) {
                out.released = true;
                phase_ = Phase::spent;
            }
            break;
        case Phase::spent:
            if (!in.a && !in.b)
                phase_ = Phase::idle;
            break;
        case Phase::idle:
            if (in.allowed && !aPass_ && !bPass_) {
                if (aPressed && bPressed) {
                    begin(out, 0, in.mode == 0);
                } else if (aPressed && in.mode == 0) {
                    phase_ = Phase::waitA;
                    wait_ = 0;
                } else if (aPressed) {
                    aPass_ = true; // hold B then A: A alone is never held back
                } else if (bPressed) {
                    if (in.warning)
                        bPass_ = true;
                    else
                        phase_ = Phase::waitB, wait_ = 0;
                }
            } else {
                aPass_ = aPass_ || aPressed;
                bPass_ = bPass_ || bPressed;
            }
            break;
        case Phase::waitA:
            wait_ += dt;
            if (bPressed && in.allowed) {
                begin(out, wait_, true);
            } else if (!in.a) {
                tapA = true; // let go of inside the window: the game's tap, late
                phase_ = Phase::idle;
                bPass_ = bPass_ || bPressed;
            } else if (!in.allowed || wait_ >= in.window) {
                aPass_ = true;
                bPass_ = bPass_ || bPressed;
                phase_ = Phase::idle;
            }
            break;
        case Phase::waitB:
            wait_ += dt;
            if (aPressed && in.allowed) {
                begin(out, wait_, false);
            } else if (!in.b) {
                tapB = true; // the dodge, on the release
                phase_ = Phase::idle;
                aPass_ = aPass_ || aPressed;
            } else if (in.warning || !in.allowed || wait_ >= (in.mode == 1 ? in.dodgeHold : in.window)) {
                bPass_ = true;
                aPass_ = aPass_ || aPressed;
                phase_ = Phase::idle;
            }
            break;
        }
        if (phase_ == Phase::chord || phase_ == Phase::spent) {
            out.a = out.b = false;
        } else {
            out.a = tapA || (aPass_ && in.a);
            out.b = tapB || (bPass_ && in.b);
        }
        return out;
    }
    // A grip (or a lost surface past the buffer): the charge ends unfired;
    // the buttons stay swallowed until both are let go of.
    void cancel() { cancelNow_ = true; }
    void reset() { *this = Chord{}; }
    Phase phase() const { return phase_; }
    bool charging() const { return phase_ == Phase::chord; }
    float held() const { return held_; }

  private:
    void begin(Output& out, float gap, bool aFirst) {
        phase_ = Phase::chord;
        held_ = 0;
        out.began = true;
        out.gap = gap;
        out.aFirst = aFirst;
    }
    Phase phase_ = Phase::idle;
    bool aWas_{}, bWas_{}, aPass_{}, bPass_{}, cancelNow_{};
    float wait_{}, held_{};
};

// Why a chord may not start, or a release did not launch.
enum class Deny { none, disabled, blocked, combat, cooldown, noContact, fresh, zipGuard, expired };
inline const char* denyName(Deny d) {
    switch (d) {
    case Deny::disabled:
        return "disabled";
    case Deny::blocked:
        return "blocked";
    case Deny::combat:
        return "combat";
    case Deny::cooldown:
        return "cooldown";
    case Deny::noContact:
        return "no contact";
    case Deny::fresh:
        return "no fresh contact since the last";
    case Deny::zipGuard:
        return "zip guard";
    case Deny::expired:
        return "buffer expired";
    default:
        return "none";
    }
}

// Contact, coyote time, the buffer, the cooldown and the zip guard.
class Controller {
  public:
    static constexpr float zipArmSeconds = 2.f;
    struct Input {
        float seconds{};
        bool contact{};  // on the ground, a perch or Spidy's wall now
        bool blocked{};  // a prompt naming A/B, the pad layer, calibration, a vent, a web line...
        bool fighting{};
        bool zipping{};  // the game's zip keys go out now (a zip asked for)
    };
    // A zip asked for arms the guard until he has been off the surface and
    // touched down again (or zipArmSeconds), then zipGuardSeconds more.
    // Each frame, before Chord::update. Returns whether a chord may start.
    bool update(const Input& in, const Params& p) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        if (in.contact) {
            if (!contactWas_)
                fresh_ = true; // touched down again since the last launch
            sinceContact_ = 0;
            contactFor_ += dt;
            // A launch that never left the surface (the game held him): after
            // half a second of contact it counts as fresh again.
            if (contactFor_ >= .5f)
                fresh_ = true;
        } else {
            sinceContact_ += dt;
            contactFor_ = 0;
        }
        contactWas_ = in.contact;
        cooldown_ = std::max(0.f, cooldown_ - dt);
        sinceZip_ += dt;
        if (in.zipping) {
            zipArmed_ = true;
            zipAir_ = false;
            zipArmAge_ = 0;
        } else if (zipArmed_) {
            zipArmAge_ += dt;
            if (!in.contact)
                zipAir_ = true;
            if (zipAir_ && in.contact) {
                zipArmed_ = false; // landed from the zip: the guard runs from here
                sinceZip_ = 0;
            } else if (zipArmAge_ >= zipArmSeconds) {
                zipArmed_ = false;
            }
        }
        in_ = in;
        deny_ = denyNow(p);
        // A release waiting for contact or for the zip guard.
        if (pending_) {
            if (deny_ != Deny::zipGuard)
                pendingAge_ += dt;
            pendingGuard_ += dt;
        }
        return deny_ == Deny::none || deny_ == Deny::zipGuard;
    }
    Deny deny() const { return deny_; }
    // A chord let go of: fire now, or wait (the buffer, the zip guard).
    void release(float held) {
        pending_ = true;
        pendingHeld_ = held;
        pendingAge_ = pendingGuard_ = 0;
    }
    struct Fire {
        bool now{};
        bool dropped{};
        Deny why{};
        float held{};
        float waited{}; // seconds the release waited (buffer or zip guard)
    };
    // After update and release: whether the pending release launches now.
    Fire take(const Params& p) {
        Fire f;
        if (!pending_)
            return f;
        f.held = pendingHeld_;
        f.waited = pendingGuard_;
        if (deny_ == Deny::none) {
            f.now = true;
            pending_ = false;
            cooldown_ = p.cooldownSeconds;
            fresh_ = false;
            contactFor_ = 0;
            return f;
        }
        const bool waits = (deny_ == Deny::noContact && pendingAge_ < p.bufferSeconds) ||
                           (deny_ == Deny::zipGuard && pendingGuard_ < p.zipGuardSeconds + p.bufferSeconds + .5f);
        if (!waits) {
            f.dropped = true;
            f.why = deny_ == Deny::noContact ? Deny::expired : deny_;
            pending_ = false;
        }
        return f;
    }
    bool pending() const { return pending_; }
    void cancel() { pending_ = false; }

  private:
    Deny denyNow(const Params& p) const {
        if (!p.enabled)
            return Deny::disabled;
        if (in_.blocked)
            return Deny::blocked;
        if (in_.fighting && !p.inCombat)
            return Deny::combat;
        if (!in_.contact && sinceContact_ > p.coyoteSeconds)
            return Deny::noContact;
        if (cooldown_ > 0)
            return Deny::cooldown;
        if (!fresh_)
            return Deny::fresh;
        if (in_.zipping || zipArmed_ || sinceZip_ < p.zipGuardSeconds)
            return Deny::zipGuard;
        return Deny::none;
    }
    Input in_{};
    Deny deny_ = Deny::disabled;
    bool contactWas_{}, fresh_ = true, pending_{};
    float sinceContact_ = 100, contactFor_{}, cooldown_{}, sinceZip_ = 100;
    float pendingHeld_{}, pendingAge_{}, pendingGuard_{};
    bool zipArmed_{}, zipAir_{};
    float zipArmAge_{};
};

} // namespace spidy::point_jump
