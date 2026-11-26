// The legs' layers in SpidyVR's air (the user, November 12 06:18): one spring
// simulation (leg_control::Simulation) driven toward whichever pose wins,
// highest first:
//   0. KICK - an air kick under way (air_kick.hpp, K2): its pose, the spin reaction at KICK SPIN.
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
//      FIRST MIDAIR POSES (the owner, Nov 18: "tuck, starfish, dive, superhero"), legs and (Nov 18 1:09 PM) torso
//      (the torso channel, J1, body_ik.hpp Targets::torso): TUCK (tumbling), DIVE (looking steeply
//      down while falling fast), SUPERHERO (one fist up and ahead, the other low, rising) and
//      STARFISH (arms spread wide, falling); MIDAIR POSES (air_poses) switches them. One at a time (that order wins), with hysteresis, a
//      minimum on-time, eased in and out; while one shows, the crouch and the reach give way.
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

enum class Layer : uint8_t { off, passive, autoPose, flail, active, easing, kick, wallSlide };
inline const char* layerName(Layer l) {
    switch (l) {
    case Layer::passive: return "passive";
    case Layer::autoPose: return "auto pose";
    case Layer::flail: return "flail";
    case Layer::active: return "active (right stick)";
    case Layer::easing: return "easing to straight";
    case Layer::kick: return "kick";
    case Layer::wallSlide: return "wall slide";
    default: return "off";
    }
}

// The authored poses (leg_control's angles: pitch forward +, side left +, knee bend).
// The midair crouch of the game's ground jump: thighs up toward the chest, knees well bent,
// a little apart.
inline constexpr Legs crouchPose{{{1.f, .14f, 1.15f}, {1.f, -.14f, 1.15f}}};
// Clearing a hand between the legs: spread, lifted forward, knees bent.
inline constexpr Legs reachPose{{{.7f, .45f, .85f}, {.7f, -.45f, .85f}}};

// FIRST MIDAIR POSES' legs. TUCK: thighs to the chest, knees fully bent, together (deeper than the
// crouch). DIVE: legs together, straight, trailing a little back. SUPERHERO: the left leg straight
// and trailing, the right knee bent forward (the flying pose). STARFISH: legs spread wide, nearly
// straight.
enum class AirPose : uint8_t { none, tuck, dive, superhero, starfish };
inline constexpr int airPoseCount = 5;
inline const char* airPoseName(AirPose p) {
    switch (p) {
    case AirPose::tuck: return "tuck";
    case AirPose::dive: return "dive";
    case AirPose::superhero: return "superhero";
    case AirPose::starfish: return "starfish";
    default: return "none";
    }
}
// F3 (beta polish): why a midair pose or the torso channel went off - the first that holds: a handstand catch, a
// touchdown, the animation taking him, a web attaching, else the pose's own end. Logged as
// "pose: <name> off (<reason>)".
inline const char* poseExitWhy(bool handstand, bool grounded, bool animTook, bool webbed) {
    return handstand  ? "handstand catch"
           : grounded ? "touchdown"
           : animTook ? "the animation took him"
           : webbed   ? "web attach"
                      : "the pose ended";
}
inline constexpr Legs tuckPose{{{1.6f, .08f, 1.2f}, {1.6f, -.08f, 1.2f}}};
inline constexpr Legs divePose{{{-.25f, .04f, .05f}, {-.25f, -.04f, .05f}}};
inline constexpr Legs superheroPose{{{-.2f, .05f, .05f}, {.45f, -.1f, 1.1f}}};
inline constexpr Legs starfishPose{{{.1f, .65f, .1f}, {.1f, -.65f, .1f}}};
// The poses' upper body on the TORSO CHANNEL (J1, body::Targets::torso; the owner, Nov 18 1:09 PM): radians, pitch
// curls the chest forward (+) or arches it back (-), roll bends it left (+), yaw twists it left (+). TUCK curls in
// (chest to the knees); DIVE stretches out with a slight arch, the body in line with the look; SUPERHERO arches
// toward the raised fist (the chest twists a little toward it: the caller gives the side); STARFISH opens the chest.
struct Torso {
    float pitch = 0, roll = 0, yaw = 0;
};
inline float torsoClamp(float a) {
    return std::isfinite(a) ? std::clamp(a, -.7f, .7f) : 0.f;
}
inline constexpr Torso tuckTorso{.55f, 0, 0}, diveTorso{-.25f, 0, 0}, superheroTorso{-.2f, 0, .15f},
                       starfishTorso{-.18f, 0, 0};
inline Torso airPoseTorso(AirPose p, int heroSide = 0) {
    switch (p) {
    case AirPose::tuck: return tuckTorso;
    case AirPose::dive: return diveTorso;
    case AirPose::superhero: {
        // heroSide: +1 the left fist up, -1 the right (the chest twists toward it), 0 neither known.
        Torso t = superheroTorso;
        t.yaw *= static_cast<float>(heroSide > 0 ? 1 : heroSide < 0 ? -1 : 0);
        return t;
    }
    case AirPose::starfish: return starfishTorso;
    default: return {};
    }
}
inline const Legs& airPoseLegs(AirPose p) {
    switch (p) {
    case AirPose::tuck: return tuckPose;
    case AirPose::dive: return divePose;
    case AirPose::superhero: return superheroPose;
    case AirPose::starfish: return starfishPose;
    default: return leg_control::centrePose;
    }
}
// The poses' thresholds (the second number of each pair: to stay on):
//  - TUCK: tumbling - the body's pitch and roll turn (not its yaw) over 3 rad/s, out under 2 (a
//    ground flip spins at 2pi; a swing's own turn stays under 2);
//  - DIVE: the head looks over 45 degrees down (out above 35) and he falls faster than 4 m/s (out
//    slower than 2.5);
//  - SUPERHERO: one hand above the head (out 10 cm below it) and 25 cm ahead of it (out 15), the
//    other 45 cm below the head (out 35) - while rising (vy over 1 m/s, out under -0.5);
//  - STARFISH: both hands over 1.1 m apart (out under 0.9) while falling (vy under 0, out over 1);
//  - every one but TUCK with no web attached and 0.15 s airborne (a swing's own arms are its);
//  - on, it stays poseMinOnSeconds, and leaves when its reason has been gone poseExitSeconds.
inline constexpr float tumbleIn = 3.f, tumbleOut = 2.f;
inline constexpr float diveLookIn = -45.f, diveLookOut = -35.f, diveFallIn = -4.f, diveFallOut = -2.5f;
inline constexpr float heroUpIn = 0.f, heroUpOut = -.1f, heroAheadIn = .25f, heroAheadOut = .15f,
                       heroLowIn = -.45f, heroLowOut = -.35f, heroRiseIn = 1.f, heroRiseOut = -.5f;
inline constexpr float starSpanIn = 1.1f, starSpanOut = .9f, starFallIn = 0.f, starFallOut = 1.f;
inline constexpr float poseAirAfter = .15f, poseMinOnSeconds = .3f, poseExitSeconds = .12f;
inline constexpr float poseInSeconds = .2f, poseOutSeconds = .25f;
struct PoseInput {
    float tumble = 0;        // the body's pitch + roll turn, rad/s
    float lookPitch = 0;     // the head's look, degrees (down -)
    float vy = 0;            // m/s
    bool webbed = false;
    bool hands = false;      // both hands tracked
    Vec3 hand[2]{};          // each hand from the head, the level frame he faces (forward, left, up), m
};
inline bool poseWanted(AirPose p, const PoseInput& in, float airSeconds, bool was) {
    if (!std::isfinite(in.tumble) || !std::isfinite(in.lookPitch) || !std::isfinite(in.vy))
        return false;
    if (p == AirPose::tuck)
        return in.tumble > (was ? tumbleOut : tumbleIn);
    if (in.webbed || airSeconds < poseAirAfter)
        return false;
    switch (p) {
    case AirPose::dive:
        return in.lookPitch < (was ? diveLookOut : diveLookIn) && in.vy < (was ? diveFallOut : diveFallIn);
    case AirPose::superhero: {
        if (!in.hands || !finite(in.hand[0]) || !finite(in.hand[1]) || !(in.vy > (was ? heroRiseOut : heroRiseIn)))
            return false;
        for (int i = 0; i < 2; ++i) {
            const Vec3 up = in.hand[i], low = in.hand[1 - i];
            if (up.z > (was ? heroUpOut : heroUpIn) && up.x > (was ? heroAheadOut : heroAheadIn) &&
                low.z < (was ? heroLowOut : heroLowIn))
                return true;
        }
        return false;
    }
    case AirPose::starfish:
        return in.hands && finite(in.hand[0]) && finite(in.hand[1]) &&
               length(in.hand[0] - in.hand[1]) > (was ? starSpanOut : starSpanIn) &&
               in.vy < (was ? starFallOut : starFallIn);
    default:
        return false;
    }
}

// SUPERHERO's raised fist: +1 left, -1 right, 0 none (poseWanted's rule, `was` thresholds).
inline int heroSide(const PoseInput& in) {
    for (int i = 0; i < 2; ++i) {
        const Vec3 up = in.hand[i], low = in.hand[1 - i];
        if (finite(up) && finite(low) && up.z > heroUpOut && up.x > heroAheadOut && low.z < heroLowOut)
            return i == 0 ? 1 : -1;
    }
    return 0;
}

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
//    for the landing; one starts only above -5 (hysteresis: a fall hovering at -7 no longer
//    flickers it on and off);
//  - once on, it stays crouchMinOnSeconds at least (the 5061b4a playtest, Nov 12: 41% of 523
//    crouches ended inside 0.35 s, a third inside 0.3 s - falls past -7, spins past 2.5, webs) and
//    leaves only when the reason to leave has lasted crouchExitSeconds (a one-frame spike of spin
//    or a web's attach does not drop it). Leaving the air always ends it at once.
inline constexpr float uprightIn = .866f /* cos 30 */, uprightOut = .766f /* cos 40 */;
inline constexpr float hangSeconds = .45f, apexSpeed = 2.f, apexAfter = .15f;
inline constexpr float spinIn = 1.5f, spinOut = 2.5f, fallIn = -5.f, fallOut = -7.f;
inline constexpr float crouchMinOnSeconds = .35f, crouchExitSeconds = .12f;
inline constexpr float crouchInSeconds = .25f, crouchOutSeconds = .2f;
struct CrouchInput {
    float upDot = 0;     // dot(body up, world up)
    float airSeconds = 0;
    float vy = 0;        // vertical speed, m/s
    float spin = 0;      // |angular velocity|, rad/s
    bool webbed = false;
};
inline bool crouchWanted(const CrouchInput& in, bool was) {
    if (in.webbed || !std::isfinite(in.upDot) || !std::isfinite(in.vy) || !std::isfinite(in.spin) ||
        in.vy < (was ? fallOut : fallIn))
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

// WALL SLIDE POSE (beta3o, wall_slide_pose; the owner, Nov 26 t147u: "we need an animation for wall sliding.
// Maybe we can pose his legs like the image whenever he's sliding, similar to the old classic Spiderman 2
// Playstation game"): while SpidyVR's own wall slides him (GRIP TO STICK, not stuck), the legs take a braced
// crouch - knees up and wide, the left foot higher than the right, as the classic game's slide down a wall.
// An approximation: the owner's picture was not readable here. Angles as leg_control's (pitch forward +,
// side left +, knee bend, within its limits).
inline constexpr Legs wallSlidePose{{{1.2f, .55f, 1.2f}, {.7f, -.5f, 1.1f}}};
// Whether the slide shows the pose: sliding (not stuck), and mostly going down or slow - a slide the stick
// runs along or up the wall (WALL RUN PUSH) keeps the wall run's own legs. `down` is the speed down the wall
// (m/s, + down), `across` the speed along it the other ways (m/s). Hysteresis: on, it stays while across is
// under 1.5x down plus 0.6 m/s; off, it starts under 1x down plus 0.3 m/s.
inline constexpr float slideAcrossIn = 1.f, slideAcrossOut = 1.5f, slideSlackIn = .3f, slideSlackOut = .6f;
inline bool wallSlideWanted(bool sliding, float down, float across, bool was) {
    if (!sliding || !std::isfinite(down) || !std::isfinite(across))
        return false;
    const float d = std::max(down, 0.f), a = std::abs(across);
    return was ? a < slideAcrossOut * d + slideSlackOut : a < slideAcrossIn * d + slideSlackIn;
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
    bool poses = true; // MIDAIR POSES (air_poses)
    bool wallSlide = false; // WALL SLIDE POSE (wall_slide_pose; the caller's values)
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
    PoseInput pose{};
    // AIR KICKS (air_kick.hpp): a kick is under way; its pose, and the share of the legs' spin reaction
    // it keeps (KICK SPIN).
    bool kick = false;
    Legs kickPose = leg_control::centrePose;
    float kickSpin = .3f;
    // KICK LEAN (beta3g, kick_lean; air_kick::kickLean): the kick's upper body on the torso channel while the
    // kick layer runs. Off: the torso stays the solve's during a kick, as before.
    bool kickLeanOn = false;
    Torso kickTorso{};
    // KICK CHAMBER (beta3g, kick_chamber): the kick layer comes in over 60 ms (0.2 s before: a 0.16 s kick was
    // about 80% shown at its fullest) - the pose itself carries the wind-up and the recoil.
    bool kickQuick = false;
    // WALL SLIDE POSE (beta3o): SpidyVR's wall slides him and the pose is wanted (wallSlideWanted, the caller's
    // hysteresis). Off the air only: the air's layers take the legs in the air.
    bool wallSlide = false;
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
    AirPose airPose = AirPose::none, airPoseWas = AirPose::none;
    bool airPoseChanged = false;
    float airPoseWeight = 0; // the strongest pose's eased weight
    // TORSO CHANNEL (J1): the poses' upper body, weighted by each pose's eased weight (0 with none), and whether it
    // runs (a pose shows and the layer is the auto poses': the stick, a kick, the flail keep the torso the solve's).
    Torso torso{};
    bool torsoOn = false;
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
        bool crouch = in.air && s.crouch && crouchWanted(c, crouchOn_);
        if (crouchOn_ && in.air && s.crouch) {
            // On already: the minimum on-time, then an exit that has to last.
            crouchOnFor_ += dt;
            exitFor_ = crouch ? 0.f : exitFor_ + dt;
            crouch = crouch || crouchOnFor_ < crouchMinOnSeconds || exitFor_ < crouchExitSeconds;
        } else {
            crouchOnFor_ = exitFor_ = 0.f;
        }
        const bool reach = in.air && s.reach && (in.handBetween[0] || in.handBetween[1]);
        // FIRST MIDAIR POSES: the first wanted in priority order; the one on keeps its minimum on-time
        // (only TUCK cuts in) and leaves when its reason has lasted gone poseExitSeconds.
        AirPose pose = AirPose::none;
        if (in.air && s.poses) {
            for (AirPose p : {AirPose::tuck, AirPose::dive, AirPose::superhero, AirPose::starfish})
                if (poseWanted(p, in.pose, airSeconds_, p == pose_)) {
                    pose = p;
                    break;
                }
            if (pose_ != AirPose::none) {
                poseOnFor_ += dt;
                const bool still = poseWanted(pose_, in.pose, airSeconds_, true);
                poseExitFor_ = still ? 0.f : poseExitFor_ + dt;
                const bool hold = poseOnFor_ < poseMinOnSeconds || poseExitFor_ < poseExitSeconds;
                if (pose != pose_ && hold && pose != AirPose::tuck)
                    pose = pose_;
            }
        }
        out.airPoseWas = pose_;
        out.airPoseChanged = pose != pose_;
        if (out.airPoseChanged)
            poseOnFor_ = poseExitFor_ = 0.f;
        pose_ = pose;
        float poseMax = 0;
        for (int i = 1; i < airPoseCount; ++i) {
            const bool on = static_cast<int>(pose) == i;
            poseW_[i] = in.air ? approach(poseW_[i], on ? 1.f : 0.f, dt / (on ? poseInSeconds : poseOutSeconds)) : 0.f;
            poseMax = std::max(poseMax, poseW_[i]);
        }
        out.airPose = pose;
        out.airPoseWeight = poseMax;
        const int side = heroSide(in.pose);
        for (int i = 1; i < airPoseCount; ++i) {
            const Torso t = airPoseTorso(static_cast<AirPose>(i), side);
            out.torso.pitch += t.pitch * poseW_[i];
            out.torso.roll += t.roll * poseW_[i];
            out.torso.yaw += t.yaw * poseW_[i];
        }
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
            if (in.kick)
                layer = Layer::kick;
            else if (in.active && !(in.flail && stick < flailStickZone))
                layer = Layer::active;
            else if (in.flail)
                layer = Layer::flail;
            else if (in.easing)
                layer = Layer::easing;
            else if (crouchW_ > .001f || reachW_ > .001f || poseMax > .001f)
                layer = Layer::autoPose;
            else if (passiveOn)
                layer = Layer::passive;
        } else if (in.wallSlide && s.wallSlide) {
            layer = Layer::wallSlide;
        }
        out.torsoOn = layer == Layer::autoPose && poseMax > .001f;
        if (!out.torsoOn)
            out.torso = {};
        if (layer == Layer::kick && in.kickLeanOn) {
            out.torsoOn = true;
            out.torso = {torsoClamp(in.kickTorso.pitch), torsoClamp(in.kickTorso.roll), torsoClamp(in.kickTorso.yaw)};
        }
        out.layer = layer;
        out.layerChanged = layer != layer_;
        layer_ = layer;
        out.run = layer != Layer::off;
        float want = 0;
        switch (layer) {
        case Layer::kick:
            out.pose = in.kickPose;
            out.couplingScale = std::clamp(leg_control::finiteOr(in.kickSpin, .3f), 0.f, 1.f);
            want = 1;
            break;
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
            // A pose shows over the crouch and the reach: they give way as it eases in.
            const float giveWay = 1 - poseMax;
            out.pose = mix(mix(leg_control::centrePose, crouchPose, crouchW_ * giveWay), reachPose, reachW_ * giveWay);
            for (int i = 1; i < airPoseCount; ++i)
                if (poseW_[i] > .001f)
                    out.pose = mix(out.pose, airPoseLegs(static_cast<AirPose>(i)), poseW_[i]);
            out.couplingScale = passiveCoupling(s);
            const float passive = passiveOn ? s.passivePct * .01f : 0.f;
            want = passive + (1 - passive) * std::max({crouchW_, reachW_, poseMax});
            break;
        }
        case Layer::wallSlide:
            // The wall holds him: the pose turns nothing on the body.
            out.pose = wallSlidePose;
            out.couplingScale = 0;
            want = 1;
            break;
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
        amount_ = approach(amount_, want, dt / (layer == Layer::kick && in.kickQuick ? .06f : .2f));
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
    float crouchOnFor_ = 0, exitFor_ = 0;
    AirPose pose_ = AirPose::none;
    float poseW_[airPoseCount]{};
    float poseOnFor_ = 0, poseExitFor_ = 0;
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
