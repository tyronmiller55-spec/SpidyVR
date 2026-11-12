// The legs' layers in SpidyVR's air (the user, November 12 06:18): one spring
// simulation (leg_control::Simulation) driven toward whichever pose wins,
// highest first:
//   1. ACTIVE - LEG CONTROL's right stick (Y+X held, or the stick still moved
//      after); the stick's pose, full LEG COUPLING.
//   2. FLAIL - in the air, Y+X physically held and B held: a run / flail
//      cycle on the legs instead of the tuck, for as long as B is held. The
//      right stick pushed past flailStickZone takes over (1 above 2).
//   3. AUTO POSES - AIR CROUCH (upright with hang time or near the apex, low
//      rotation: the legs fold into the ground jump's midair crouch) and REACH
//      SPREAD (a hand below the pelvis and between the thighs: the legs spread
//      and lift forward with bent knees, clearing the arm). Each eased in and
//      out on its own weight.
//   4. PASSIVE - PASSIVE LEGS: the centre pose with no stick at all, so only
//      the body's motion (swing, spin, deceleration) moves them; shown at
//      PASSIVE LEGS percent, its turn on the body at PASSIVE COUPLING percent
//      of LEG COUPLING.
// The pose the spring chases changes; its state never resets between layers,
// so nothing pops; the shown amount (body_ik's legControlWeight) eases too.
// Pure; no Windows.
#pragma once
#include "spidy/leg_control.hpp"
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::leg_layers {

using leg_control::Angles;
using leg_control::Legs;

enum class Layer : uint8_t { off, passive, autoPose, flail, active, easing };
inline const char* layerName(Layer l) {
    switch (l) {
    case Layer::passive: return "passive";
    case Layer::autoPose: return "auto pose";
    case Layer::flail: return "flail";
    case Layer::active: return "active (right stick)";
    case Layer::easing: return "easing to straight";
    default: return "off";
    }
}

// The authored poses (leg_control's angles: pitch forward +, side left +, knee bend).
// The midair crouch of the game's ground jump: thighs up toward the chest, knees well bent,
// a little apart.
inline constexpr Legs crouchPose{{{1.f, .14f, 1.15f}, {1.f, -.14f, 1.15f}}};
// Clearing a hand between the legs: spread, lifted forward, knees bent.
inline constexpr Legs reachPose{{{.7f, .45f, .85f}, {.7f, -.45f, .85f}}};

// AIR CROUCH thresholds.
//  - upright: the body's up within 30 degrees of the world's (the user's ~30),
//    out past 40 so it doesn't flicker at the edge;
//  - hang time: 0.45 s airborne (a jump's rise to a ~1 m apex takes about that
//    long: a hop or a step off a kerb stays straight), or near the apex
//    (vertical speed within 2 m/s - about +-0.2 s around it - after 0.15 s up);
//  - low rotation: under 1.5 rad/s (86 deg/s); a flip spins at 4-8 rad/s, a
//    swing's own turn and a drift stay under it; out past 2.5;
//  - no web attached: at a swing's low point he is upright and slow-turning
//    too, and there the legs should trail the swing, not crouch;
//  - not falling fast: under -7 m/s (about 2.5 m of fall) the legs come down
//    for the landing.
inline constexpr float uprightIn = .866f /* cos 30 */, uprightOut = .766f /* cos 40 */;
inline constexpr float hangSeconds = .45f, apexSpeed = 2.f, apexAfter = .15f;
inline constexpr float spinIn = 1.5f, spinOut = 2.5f, fallOut = -7.f;
inline constexpr float crouchInSeconds = .25f, crouchOutSeconds = .2f;
struct CrouchInput {
    float upDot = 0;     // dot(body up, world up)
    float airSeconds = 0;
    float vy = 0;        // vertical speed, m/s
    float spin = 0;      // |angular velocity|, rad/s
    bool webbed = false;
};
inline bool crouchWanted(const CrouchInput& in, bool was) {
    if (in.webbed || !std::isfinite(in.upDot) || !std::isfinite(in.vy) || !std::isfinite(in.spin) || in.vy < fallOut)
        return false;
    const bool upright = in.upDot >= (was ? uprightOut : uprightIn);
    const bool calm = in.spin < (was ? spinOut : spinIn);
    const bool hang = in.airSeconds >= hangSeconds || (in.airSeconds >= apexAfter && std::abs(in.vy) < apexSpeed);
    return upright && calm && (hang || was);
}

// REACH SPREAD: a hand relative to the player's pelvis, in the body's level
// frame (forward, left, up), metres. Below the pelvis (2 cm, out above +4) and
// between the thighs (within 16 cm of the middle, out past 22; forward of
// 20 cm behind and within 40 cm ahead).
inline constexpr float reachBelowIn = -.02f, reachBelowOut = .04f, reachSideIn = .16f, reachSideOut = .22f,
                       reachBack = -.2f, reachAhead = .4f;
inline constexpr float reachInSeconds = .25f, reachOutSeconds = .3f;
inline bool handBetween(Vec3 rel, bool was) {
    if (!finite(rel))
        return false;
    const float f = rel.x, side = rel.y, u = rel.z;
    if (f < reachBack || f > reachAhead)
        return false;
    return was ? (u < reachBelowOut && std::abs(side) < reachSideOut)
               : (u < reachBelowIn && std::abs(side) < reachSideIn);
}
// The pelvis from the head: 44% of the standing eye height below it (hips at
// about 0.53 of stature, eyes at 0.94); a seated player's sits about as far.
inline float pelvisDrop(float standingEye) {
    return std::clamp(.44f * (std::isfinite(standingEye) ? standingEye : 1.6f), .55f, .9f);
}

// FLAIL: a running cycle, legs in opposition, the back leg's knee bent most.
inline constexpr float flailHz = 2.2f, flailStickZone = .3f;
inline Legs flailPose(float phase) {
    const float s = std::sin(phase);
    Legs out;
    for (int i = 0; i < 2; ++i) {
        const float k = i == 0 ? s : -s;
        out[i].pitch = .35f + .8f * k;
        out[i].side = (i == 0 ? 1.f : -1.f) * (.08f + .04f * k);
        out[i].knee = .35f + .7f * std::max(0.f, -k) + .2f * std::max(0.f, k);
    }
    return out;
}

inline Legs mix(const Legs& base, const Legs& pose, float w) {
    Legs out = base;
    for (int i = 0; i < 2; ++i)
        out[i] = leg_control::plus(out[i], leg_control::centrePose[i], pose[i], std::clamp(w, 0.f, 1.f));
    return out;
}

struct Settings {
    int passivePct = 60;         // PASSIVE LEGS: 0 off, else the shown share
    int passiveCouplingPct = 15; // of LEG COUPLING
    bool crouch = true, reach = true;
};
struct Input {
    float dt = 0;
    bool air = false;          // legs may move in SpidyVR's air (legsAirOnly)
    bool active = false;       // LEG CONTROL engaged (the stick's)
    bool easing = false;       // LEG CONTROL just let go of
    float stickX = 0, stickY = 0;
    bool flail = false;        // in the air, Y+X and B physically held (the caller's gate)
    CrouchInput crouch{};
    bool handBetween[2]{};     // already with hysteresis (handBetween)
};
struct Output {
    Layer layer = Layer::off;
    bool run = false;          // the simulation runs
    bool useStick = false;     // toward the stick's pose (else `pose`)
    Legs pose = leg_control::centrePose;
    float amount = 0;          // the shown share (0..1), eased
    float couplingScale = 0;   // times LEG COUPLING
    bool easing = false;       // leg_control's stiff ease to straight
    float crouchWeight = 0, reachWeight = 0;
    bool layerChanged = false, crouchChanged = false, reachChanged = false;
    bool crouchOn = false, reachOn = false;
};

class Mixer {
  public:
    Output update(const Input& in, const Settings& s) {
        Output out;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        const bool passiveOn = s.passivePct > 0;
        const float stick = std::hypot(leg_control::finiteOr(in.stickX, 0), leg_control::finiteOr(in.stickY, 0));
        // Auto poses: wanted (hysteresis), then eased weights.
        airSeconds_ = in.air ? airSeconds_ + dt : 0.f;
        CrouchInput c = in.crouch;
        c.airSeconds = airSeconds_;
        const bool crouch = in.air && s.crouch && crouchWanted(c, crouchOn_);
        const bool reach = in.air && s.reach && (in.handBetween[0] || in.handBetween[1]);
        out.crouchChanged = crouch != crouchOn_;
        out.reachChanged = reach != reachOn_;
        crouchOn_ = crouch;
        reachOn_ = reach;
        crouchW_ = approach(crouchW_, crouch ? 1.f : 0.f, dt / (crouch ? crouchInSeconds : crouchOutSeconds));
        reachW_ = approach(reachW_, reach ? 1.f : 0.f, dt / (reach ? reachInSeconds : reachOutSeconds));
        if (!in.air) {
            crouchW_ = reachW_ = 0;
            flailPhase_ = 0;
        }
        out.crouchOn = crouch;
        out.reachOn = reach;
        out.crouchWeight = crouchW_;
        out.reachWeight = reachW_;
        // The layer, highest first.
        Layer layer = Layer::off;
        if (in.air) {
            if (in.active && !(in.flail && stick < flailStickZone))
                layer = Layer::active;
            else if (in.flail)
                layer = Layer::flail;
            else if (in.easing)
                layer = Layer::easing;
            else if (crouchW_ > .001f || reachW_ > .001f)
                layer = Layer::autoPose;
            else if (passiveOn)
                layer = Layer::passive;
        }
        out.layer = layer;
        out.layerChanged = layer != layer_;
        layer_ = layer;
        out.run = layer != Layer::off;
        float want = 0;
        switch (layer) {
        case Layer::active:
            out.useStick = true;
            out.couplingScale = 1;
            want = 1;
            break;
        case Layer::flail:
            flailPhase_ = std::fmod(flailPhase_ + 2 * 3.14159265f * flailHz * dt, 2 * 3.14159265f);
            out.pose = flailPose(flailPhase_);
            out.couplingScale = 1;
            want = 1;
            break;
        case Layer::easing:
            out.useStick = true; // leg_control's ease: the stick centred, stiff
            out.easing = true;
            out.couplingScale = 1;
            want = 1;
            break;
        case Layer::autoPose: {
            out.pose = mix(mix(leg_control::centrePose, crouchPose, crouchW_), reachPose, reachW_);
            out.couplingScale = passiveCoupling(s);
            const float passive = passiveOn ? s.passivePct * .01f : 0.f;
            want = passive + (1 - passive) * std::max(crouchW_, reachW_);
            break;
        }
        case Layer::passive:
            out.couplingScale = passiveCoupling(s);
            want = std::clamp(s.passivePct, 0, 100) * .01f;
            break;
        default:
            break;
        }
        if (layer != Layer::flail)
            flailPhase_ = 0;
        // The shown amount eases at body_ik's airLegBlendSeconds pace (0.2 s full scale).
        amount_ = approach(amount_, want, dt / .2f);
        out.amount = amount_;
        return out;
    }
    void reset() {
        *this = Mixer{};
    }
    Layer layer() const {
        return layer_;
    }
    float airSeconds() const {
        return airSeconds_;
    }

  private:
    static float approach(float v, float to, float step) {
        return v < to ? std::min(to, v + step) : std::max(to, v - step);
    }
    static float passiveCoupling(const Settings& s) {
        return std::clamp(s.passiveCouplingPct, 0, 100) * .01f;
    }
    Layer layer_ = Layer::off;
    float airSeconds_ = 0, crouchW_ = 0, reachW_ = 0, amount_ = 0, flailPhase_ = 0;
    bool crouchOn_ = false, reachOn_ = false;
};

// FLAIL's B: once B is held in the air with Y+X physically held, B is the
// legs' until it is let go of - never the dodge, the tuck or POINT JUMP's A+B
// - even if Y+X or the air end first (a B held before Y+X is taken over when
// they come). It flails only while all of them hold.
struct FlailGate {
    bool owned = false;
    struct Out {
        bool swallowB = false; // keep B from the game and the mod this frame
        bool flail = false;    // play the cycle
        bool started = false, ended = false;
    };
    Out update(bool allowed, bool air, bool x, bool y, bool b) {
        Out out;
        if (!b) {
            out.ended = owned;
            owned = false;
            return out;
        }
        if (!owned && allowed && air && x && y)
            owned = out.started = true;
        out.swallowB = owned;
        out.flail = owned && allowed && air && x && y;
        return out;
    }
};

} // namespace spidy::leg_layers
