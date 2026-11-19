// ANIMATED HYBRID (vr_settings::Values::animatedHybrid, off by default): who
// owns Spider-Man's body - the game's animation (ANIM) or OG SpidyVR (VR).
//
// OWNERSHIP UPDATE (the user, November 9 09:34; supersedes the November 8
// grip-cancel stay, ground grip cancel and jump-adopt rules):
//   Animation owns the ground, parkour, jumps and landings. SpidyVR owns the
//   air and wall runs / wall jumps.
//
//   where / what                     owner   switch
//   -------------------------------  ------  ---------------------------------
//   ground, game crawl, perch        ANIM    VR -> ANIM on landing (always)
//   ground + grip / trigger          ANIM    none: grip only arms SpidyVR
//                                            webs, balls, punches, markers
//   jump, vault, point launch, zip   ANIM    plays from the press / takeoff
//     (any midair native animation)          to the landing (November 11
//                                            23:39: no anim-end hand-off)
//   midair animation + a flip's      VR      flip-break, on the frame read
//     pitch or tilt (flipBreak)              (breakForFlip, before the frame
//                                            uses the owner)
//   midair animation + grip          VR      grip cancel (consumed, no shot)
//   midair animation + Spidy wall    VR      wall run
//   midair animation + SpidyVR web   VR      web (a web of SpidyVR's attached)
//   air after the animation ended    VR      (anim-end)
//   Y zip from VR in the air         ANIM    when the game begins the zip
//   Spidy wall run / wall jump       VR      -
//   wall run from a Native Run,      ANIM    run-release
//     stick released on the wall
//   FULL ARMS: a punch's web strike  VR      punch (its lunge / teleport), held
//     (ground or air)                        until the strike ends; then the
//                                            table (landing -> ANIM)
//   POINT JUMP fired (point_jump.hpp) VR     point-jump (the approved ground
//     (ground, perch, Spidy wall)            exception, November 11): on the
//                                            fire, the feet locked (the
//                                            position lock); held through the
//                                            takeoff (pointJumpHold); the air
//                                            is VR's; landing -> ANIM
//
// No ownership switch happens on the ground except a FULL ARMS punch's strike
// (the user, November 9 22:27: "punching can pull you out of the animation")
// and a POINT JUMP's launch (November 11: "point jump may take over from the
// ground animation"; lock the feet at the switch). "The animation ended" is
// not a signal the game gives the mod: it is taken as the move having
// stopped rising (vertical velocity at or below zero for fallHoldNs, after at
// least airAnimMinNs off the ground), no zip, fight or dodge still going on,
// or airAnimMaxNs at the most. A short hop or step down lands inside that
// and never switches. A web zip (Y) plays while its keys go out and then
// through what it leads into: its point launch ends at the launch's top (the
// same fall rule); a zip with no launch plays until he lands or perches, or
// zipMaxNs. The position lock (game_xr) runs on every switch.
// Pure domain FSM - no Windows.
//
// Diagnostics keep the old G->T / T->G spelling (G = anim, T = vr) so earlier
// log greps still work.
#pragma once
#include "spidy/animated_moves.hpp"
#include "spidy/head_probe.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hybrid_owner {

enum class Domain : uint8_t { anim, vr };

// Why the owner went to VR. The retired values stay for old logs and the
// session summary; the ownership update never produces them.
enum class EnterVr : uint8_t {
    none,
    web,     // a SpidyVR web attached in the air (or holds him as he leaves the ground)
    airWeb,  // retired: grip in the air is the cancel now
    rotate,  // retired: no mid-air rotate takeover
    speed,   // retired: no mid-air speed takeover
    wallRun, // Spidy's own wall run (game_swing::Surface::wall): the mod drives it
    swing,   // a web zip the game did not take: the swing still carries him in the air
    jump,    // retired: the jump is the game's animation until it ends (animEnd)
    cancel,  // consumed grip in a midair animation: VR without a web or other action
    animEnd, // a midair native animation ended with him still in the air
    punch,   // FULL ARMS: a SpidyVR punch opened a web strike (its lunge / teleport)
    flip,    // a flip's pitch or tilt input from either stick in a midair animation (breakForFlip)
    pointJump, // POINT JUMP fired off a surface (Frame::pointJump; the ground exception)
    timed,     // JUMP TO SPIDYVR (November 12): a midair animation's timer ran out (Frame::jumpHandoffNs)
    sidestep,  // QUICK SIDESTEP (November 12, sidestep.hpp): a ground burst (Frame::sidestep; a ground
               // exception)
    handstand, // HANDSTAND (November 13): the hands caught him in the landing's first frames
               // (Frame::handSupport)
    move,      // INTENT BREAK (Nov 18, breakForMove): a move's intent in a midair animation (the legs engaged, a kick)
};
// A landing's hand-off to the animation still takes a handstand catch back to SpidyVR this long
// after it (handstand::Params::touchdownGrace is the same 0.25 s): the catch is judged on the
// handoff's own frame, but the palms often come down a few frames later.
constexpr int64_t handstandLandNs = 250'000'000;

// JUMP TO SPIDYVR (the user, November 12 04:56 / 05:07 / 05:41: "All jumps
// should switch to Spidy VR after about a half of a second"; plain jumps, zip
// launches, vaults and wall launches): which midair animation the timer ended.
// What took a jump-timer handoff: the timer itself, a jump's vertical speed under takeoffDoneVel, or a plain fall.
enum class TimedWhy : uint8_t { timer, slowRise, fall, noLanding };
inline const char* timedWhyName(TimedWhy w) {
    switch (w) {
    case TimedWhy::slowRise: return "vertical speed low";
    case TimedWhy::fall: return "plain fall";
    case TimedWhy::noLanding: return "no landing came";
    default: return "timer";
    }
}
enum class JumpKind : uint8_t { none, jump, fall, zipLaunch, vault, wallLaunch };
inline const char* jumpKindName(JumpKind k) {
    switch (k) {
    case JumpKind::jump:
        return "jump";
    case JumpKind::fall:
        return "fall";
    case JumpKind::zipLaunch:
        return "zip launch";
    case JumpKind::vault:
        return "vault";
    case JumpKind::wallLaunch:
        return "wall launch";
    default:
        return "none";
    }
}
// A jump asked for (A) within this before the takeoff makes the air a jump.
constexpr int64_t jumpAskRecentNs = 400'000'000;
// Rising faster than this (m/s) during the air phase makes it a jump, not a fall.
constexpr float jumpRiseVel = 1.f;

inline const char* enterVrName(EnterVr e) {
    switch (e) {
    case EnterVr::cancel:
        return "grip-cancel";
    case EnterVr::web:
        return "web";
    case EnterVr::airWeb:
        return "air-web";
    case EnterVr::rotate:
        return "rotate";
    case EnterVr::speed:
        return "speed";
    case EnterVr::wallRun:
        return "wallrun";
    case EnterVr::swing:
        return "swing";
    case EnterVr::jump:
        return "jump";
    case EnterVr::animEnd:
        return "anim-end";
    case EnterVr::punch:
        return "punch";
    case EnterVr::flip:
        return "flip-break";
    case EnterVr::pointJump:
        return "point-jump";
    case EnterVr::timed:
        return "jump-timer";
    case EnterVr::sidestep:
        return "sidestep";
    case EnterVr::handstand:
        return "handstand";
    case EnterVr::move:
        return "move-break";
    default:
        return "none";
    }
}

// Snapshot the live pose and keep the legs airborne for any takeover while the
// hero is not on the ground. A grounded one reclaims without a seed.
inline bool adoptsFromAir(EnterVr why, bool grounded) {
    return why == EnterVr::jump || !grounded;
}

// Why the owner went back to ANIM.
enum class EnterAnim : uint8_t {
    none,
    land,       // VR touched the ground (or a perch): landings are the game's
    zip,        // the game began a web zip the player asked for (Frame::zipPress)
    mount,      // VR met the game's crawl (a wall mount is the game's animation)
    ground,     // retired spelling of land (old logs)
    native,     // retired: the grip-cancel stay is gone
    runRelease, // a wall run from a Native Run, the stick let go on the wall
    predicted,  // LANDING BLEND-IN (air_landing_release, unverified): the landing is about to happen; the game has the body before it
};

inline const char* enterAnimName(EnterAnim e) {
    switch (e) {
    case EnterAnim::land:
        return "land";
    case EnterAnim::zip:
        return "zip";
    case EnterAnim::mount:
        return "mount";
    case EnterAnim::ground:
        return "ground";
    case EnterAnim::native:
        return "native";
    case EnterAnim::runRelease:
        return "run-release";
    case EnterAnim::predicted:
        return "predicted-landing";
    default:
        return "none";
    }
}

inline const char* domainName(Domain d) {
    return d == Domain::vr ? "vr" : "anim";
}

// game_swing::Surface values as Frame::state.wall carries them (kept here so the
// FSM stays pure). Only `wall` (the swing's own wall run) is VR; the game's
// crawl (`game`) is supported ground for ownership; the floor crawl flag
// (`floor`, lit by ordinary rooftop parkour) is no surface.
constexpr unsigned surfaceWall = 1, surfaceGame = 2, surfaceFloor = 3;

constexpr int64_t combatClearNs = 300'000'000; // sticky combat ends after idle
// Retired thresholds of the old rotate / speed takeovers (settings still parse).
constexpr float vrSpeed = 15.f;
constexpr int64_t speedHoldNs = 200'000'000;
// A web zip's hold (Frame::zipPress). From VR while the swing still has him,
// ANIM starts only when the game begins the move (the swing yields). The game
// begins within game_swing::AirYield::askMs of the press or never does, and
// lets the swing have him back after AirYield::yieldMs at most.
constexpr int64_t zipAskNs = 400'000'000;
constexpr int64_t zipMaxNs = 3'000'000'000;
// The end of a midair native animation (see the top): at least this long off
// the ground, then falling (vertical velocity <= 0) for fallHoldNs; at most
// airAnimMaxNs. A point launch rises the whole way to its top, so it plays.
constexpr int64_t airAnimMinNs = 300'000'000;
constexpr int64_t fallHoldNs = 150'000'000;
constexpr int64_t airAnimMaxNs = 2'500'000'000;
// AIR TAKEOVER (Nov 14): a jump's vertical speed under this, after this long in the air, takes the air early.
constexpr float takeoffDoneVel = 4.f;
constexpr int64_t takeoffMinNs = 250'000'000;
// HANDOFF CHURN (the 2 AM log of Nov 18, approved 1:53 PM): a punch or a grip cancel that took him into SpidyVR
// just as he touched down handed him straight back to the animation on the same frame - 24 view eases ended
// "(landed)" under 50 ms, 20 of them at 0 ms, the hands jumping up to 2.4 m to the shown view and back. A
// landing hands back only once such a takeover has lasted this long.
constexpr int64_t landMinStintNs = 120'000'000;
// MINIMUM STINT (beta polish H1, the owner's D10; the Nov 18-19 logs: 10-13% of VR stints under 150 ms, almost all
// land and zip): every takeover but a handstand's or a swing's waits landMinStintNs before a landing hands back. A
// hard landing (falling faster than hardLandVel) still hands back at once, except after a punch or a grip cancel
// (the churn guard as it was).
constexpr float hardLandVel = -6.f;
// RISING TOUCHDOWN (F2, the Nov 19 log: a ground flip took off at 11 m/s and "touched down" 140 ms later still
// rising at 10.7 m/s, handed straight back to the animation): ground reported while rising faster than this, within
// risingTouchdownNs of the takeover, is no landing.
constexpr float risingTouchdownVel = 3.f;
constexpr int64_t risingTouchdownNs = 600'000'000;
// LANDING BLEND-IN's early hand-back (air_landing_release, unverified): no sooner than this after the owner changed (the owner's 0.25 s
// minimum dwell), the animation keeps the body while the impact is predicted within landKeepEta; if the prediction is lost for
// landLostNs, or no landing came in landHandBackMaxNs, SpidyVR takes the air again.
constexpr int64_t minDwellNs = 250'000'000, landLostNs = 200'000'000, landHandBackMaxNs = 800'000'000;
constexpr float landKeepEta = .45f;
// November 11 23:39 (the user: "Jumps are in animation until it's broken with
// a rotation, or grip cancel"): a midair animation no longer hands the air to
// SpidyVR when it ends (anim-end); it plays through the fall to the landing
// unless a flip's pitch or tilt (breakForFlip), the grip cancel, Spidy's wall,
// a SpidyVR web or a strike takes it. The proxy above still runs for the
// diagnostics (animEndSeen) and false here restores nothing else.
constexpr bool animEndHandsToVr = false;
// Stick noise, not a gesture: a flip's pitch or tilt past this breaks the
// animation (game_xr flipBreak).
constexpr float flipBreakDeadZone = .2f;
// After a zip, rising faster than this is its point launch.
constexpr float zipRiseVel = 1.f;
// Native Run's stick let go: below this tilt (NATIVE RUN's own off threshold
// is higher; a resting stick reads under it).
constexpr float runReleaseStick = .2f;
// A Native Run counts for the wall run it leads into within this of RT.
constexpr int64_t runRecentNs = 500'000'000;

// What breaks a midair animation for a flip (the user, November 11 23:41:
// "Turning left or right on the right stick should keep you in animation. Any
// cancel or pitch/tilt will break animation on that exact frame"): the axes
// the flip system in force reads for pitch and tilt, past a noise dead zone.
//   right stick up/down (pitch): with basic FLIPS (the right stick flips) or
//     LEG CONTROL (the right stick poses the legs; their swing pitches him).
//     A diagonal counts when its up/down part is past the dead zone. Left /
//     right (a snap or smooth turn) never breaks.
//   left stick, any way (pitch and tilt): while CONTROLLED FLIPS is toggled
//     on in the air (A), or REALISTIC FLIPS steers with it (FLIPS on).
// The stick-up flick basic flips keep for a zip (zipPriority) is a zip.
struct FlipInput {
    bool rightValid{}, leftValid{};
    float rightX{}, rightY{}, leftX{}, leftY{};
    bool basicFlips{};        // FLIPS on, CONTROLLED and REALISTIC off
    bool legs{};              // LEG CONTROL in force (with CONTROLLED or REALISTIC FLIPS)
    bool controlledActive{};  // CONTROLLED FLIPS toggled on in the air this frame
    bool realisticSteering{}; // REALISTIC FLIPS with FLIPS on: the left stick steers the spin
    bool zipPriority{};       // the right stick's push is a zip
};
enum class FlipBreak : uint8_t { none, rightPitch, leftStick };
inline const char* flipBreakName(FlipBreak b) {
    return b == FlipBreak::rightPitch ? "right stick pitch" : b == FlipBreak::leftStick ? "left stick pitch/tilt" : "none";
}
inline FlipBreak flipBreak(const FlipInput& in, float deadZone = flipBreakDeadZone) {
    if (in.rightValid && (in.basicFlips || in.legs) && !in.zipPriority && std::isfinite(in.rightY) &&
        std::abs(in.rightY) > deadZone)
        return FlipBreak::rightPitch;
    if (in.leftValid && (in.controlledActive || in.realisticSteering) && std::isfinite(in.leftX) &&
        std::isfinite(in.leftY) && std::hypot(in.leftX, in.leftY) > deadZone)
        return FlipBreak::leftStick;
    return FlipBreak::none;
}

// FRESH PUSH (the 5061b4a playtest, November 12: 111 of the jumps broke to
// SpidyVR about 0.02 s after takeoff because the run stick was still held):
// the left stick's pitch/tilt break reads a stick that was already tilted at
// takeoff as the run's, not a flip's. It breaks only after the stick has come
// back inside the dead zone since takeoff, or has moved clearly (past
// clearAngleDeg) away from the direction it had at takeoff. The right stick
// is not gated. Fed from the frames outside a midair animation (ground()) and
// then the frames inside it (allows()). Pure.
class FreshPush {
  public:
    static constexpr float clearAngleDeg = 60.f;
    // Not in a midair animation: this is the stick he takes off with.
    void ground(bool valid, float x, float y, float deadZone = flipBreakDeadZone) {
        const float mag = valid && std::isfinite(x) && std::isfinite(y) ? std::hypot(x, y) : 0.f;
        armed_ = mag <= deadZone;
        x_ = armed_ ? 0.f : x;
        y_ = armed_ ? 0.f : y;
    }
    // In a midair animation: whether the left stick may break now.
    bool allows(bool valid, float x, float y, float deadZone = flipBreakDeadZone) {
        if (!valid || !std::isfinite(x) || !std::isfinite(y))
            return armed_ && valid;
        if (!armed_) {
            const float mag = std::hypot(x, y);
            if (mag <= deadZone) {
                armed_ = true; // let go since takeoff: the next push is a flip's
            } else {
                const float dot = (x * x_ + y * y_) / (mag * std::hypot(x_, y_));
                if (dot < std::cos(clearAngleDeg * 3.14159265f / 180.f))
                    armed_ = true; // moved clearly from the takeoff direction
            }
        }
        return armed_;
    }
    bool armed() const {
        return armed_;
    }

  private:
    bool armed_{true};
    float x_{}, y_{};
};

struct Frame {
    head_probe::StateInputs state{};
    bool webRising{};   // a SpidyVR web attached this frame
    bool webIntent{};   // web-hold button held (grip by default; trigger if swapped)
    float speedH{};     // horizontal measured speed, m/s (diagnostics)
    float rStickX{};    // right stick x (diagnostics; no rotate takeover)
    float hmdYaw{};     // headset yaw (diagnostics)
    bool hmdYawValid{};
    bool cutscene{};    // !playing || !fresh - freeze domain
    // The player pressed a web zip this frame: the game's zip to a point or its
    // Y in the air (a web strike). Not Y on the ground or on Spidy's wall.
    bool zipPress{};
    // INSTANT WEB ZIP (web_zip_instant): a zip pressed from the swing or Spidy's wall goes to ANIM on the press
    // (the game's zip starts at once; a press it does not take hands back after zipAskNs). Off: VR waits for the
    // game to begin the move (zipFromHold).
    bool zipInstant{};
    bool jumpAsk{};     // a jump asked for (diagnostics; the jump is the game's)
    int64_t speedHoldNs = hybrid_owner::speedHoldNs; // retired setting
    // Consumed grip (animation_cancel::Gate, armed only in a midair animation)
    // and that grip still held.
    bool cancelGrip{}, cancelGripHeld{};
    bool groundAction{}; // actual dispatched takeoff, not a queue/phase cooldown
    // POINT JUMP (point_jump.hpp): fired this frame (VR at once, ground
    // included), and its takeoff still under way (VR stays on the ground
    // until he is off it, as groundAction).
    bool pointJump{}, pointJumpHold{};
    // QUICK SIDESTEP (sidestep.hpp; approved November 12): a burst began this
    // frame (VR at once, on the ground: the feet lock with the switch), and
    // the burst still under way (VR stays on the ground until it ends; then
    // the ground hands back as any landing, with the position lock).
    bool sidestep{}, sidestepHold{};
    // WEB LINE CANCEL (the owner, Nov 18, t119u: "for animation cancels, just make all web lines cancel into
    // SpidyVR"): a web fired while the animation has him on the ground or a perch (a safe state:
    // groundWebRefusal) takes the body this frame, and holds it while the line is held or attached; then the
    // ground hands back as any landing. In the air the grip cancel (cancelGrip) already does this.
    bool groundWeb{}, groundWebHold{};
    float parkourSeconds{}; // how long the game's parkour (head_probe's parkour state) has run; 0 when none
    // FULL ARMS (the user, Nov 9 22:27): a SpidyVR web strike is under way (its
    // flight or hang). It pulls him out of the animation, ground included,
    // and holds SpidyVR until it ends; then the table as usual.
    bool strike{};
    // The hero's vertical velocity, m/s (up +); NaN unknown.
    float velY = NAN;
    // The left (walk) stick's tilt 0..1; negative unknown.
    float moveStick = -1.f;
    // JUMP TO SPIDYVR: a midair animation (jump, fall, vault, wall launch; a
    // zip's point launch from its rise) hands to VR this long into the air;
    // 0 stays to the landing (the November 11 23:39 rule).
    int64_t jumpHandoffNs = 0;
    // AIR TAKEOVER: earlier takeovers inside the JUMP TO SPIDYVR timer (each 0 = off, as before): a jump whose vertical
    // speed is under takeoffDoneVel (m/s) once it has been takeoffMinNs in the air; a plain fall after fallHandoffNs.
    float takeoffDoneVel = 0;
    // LANDING BLEND-IN (landing_blend.hpp): the time to impact in SpidyVR's air (s, NaN unknown), and whether the release is on and
    // due (air_landing_release; false: the landing is the touchdown handoff, as always).
    float landEta = NAN;
    bool landHandBack{};
    int64_t takeoffMinNs = 0, fallHandoffNs = 0;
    // HANDSTAND (handstand.hpp, H1): the hands hold him on a floor (caught,
    // balancing, or just let go of): SpidyVR keeps the body, ground included;
    // the landing waits until this ends.
    bool handSupport{};
    // PERCH (camera_mode::PerchWatch; the user, November 13 15:42: perches
    // "should be animations with smooth entry and exits"): still off the
    // game's ground on a perch it holds him on (a pole, a zip's point) - no
    // web, no wall, the swing not driving. The animation's, as the ground is.
    bool perched{};
};

// The view ease (view_ease.hpp) a switch to VR takes, in ms (0: the snap):
// a punch's PUNCH VIEW EASE; the timed jump handoff, a flip break and the grip
// cancel AIR VIEW EASE (November 12 05:41: "the punch hold-and-ease view
// handoff ... to the timed jump handoff plus flip-break and grip-cancel").
// TAKEOVER VIEW EASE (takeover_view_ease; the owner, Nov 15: hitching during handoffs): `everyTakeover` gives every
// other takeover (a point jump, a web, a wall run, a handstand, a sidestep, a swing, an animation's end) AIR VIEW EASE too.
inline int viewEaseMs(EnterVr why, int punchMs, int airMs, bool everyTakeover = false) {
    switch (why) {
    case EnterVr::none:
        return 0;
    case EnterVr::punch:
        return std::max(punchMs, 0);
    case EnterVr::timed:
    case EnterVr::flip:
    case EnterVr::cancel:
    case EnterVr::move:
        return std::max(airMs, 0);
    default:
        return everyTakeover ? std::max(airMs, 0) : 0;
    }
}

// Airborne: off the ground and not on the game's own crawl. The floor-crawl
// flag counts as no surface (it lights during rooftop parkour).
inline bool airborne(const head_probe::StateInputs& s) {
    return !s.grounded && s.wall != surfaceGame;
}
// Supported ground for ownership: the ground, a perch, or the game's crawl.
inline bool supported(const head_probe::StateInputs& s) {
    return (s.grounded || s.wall == surfaceGame) && s.wall != surfaceWall;
}
// A midair native animation plays: the animation owns and he is in the air
// (not on Spidy's wall). Nothing of the mod's is steered into it (game_xr).
inline bool midairAnimation(bool animOwns, const head_probe::StateInputs& s) {
    return animOwns && airborne(s) && s.wall != surfaceWall;
}
// Grip cancel is armed only where it can switch: a midair animation. On the
// ground grip is the arms' SpidyVR web (no ownership change).
inline bool cancelArmed(bool animOwns, bool supportedNow) {
    return animOwns && !supportedNow;
}

// WEB LINE CANCEL on the ground: why a web fired in a native animation there may NOT hand to SpidyVR (nullptr:
// it may). Unsafe: a scene or a stale sample, a fight or a dodge (its finishers and counters are the game's), Spidy's
// own wall (already SpidyVR's), the game's vault, mantle or wall run (NATIVE RUN's
// parkour) in its first parkourCutSeconds, and the air (the grip cancel's, not this).
// The game's parkour may be cut once it has run parkourCutSeconds (the owner, Nov 18: "Allow web cancel during parkour after
// 150 ms"): its first frames carry the mantle's or vault's geometry (the body may be inside the ledge).
constexpr float parkourCutSeconds = .15f;
inline const char* groundWebRefusal(const head_probe::StateInputs& s, bool perched, float parkourSeconds = 0) {
    if (!s.playing || !s.fresh)
        return "cutscene";
    if (s.fighting || s.dodging)
        return "combat";
    // A zip is cut at once (the owner, Nov 18: "Zip cancel happens immediately"), its pull too.
    if (s.wall == surfaceWall)
        return "wall";
    if (!s.owned && s.runRt && s.wall != 0 && !(parkourSeconds >= parkourCutSeconds))
        return "parkour (its first 150 ms)";
    if (!supported(s) && !perched)
        return "air";
    return nullptr;
}

// FULL ARMS strike hold (EngineBot review M3/M4, Nov 9 23:01): what tells the
// owner a punch's web strike is under way. The strike sample must be FRESH
// (its sequence advanced within freshNs: a stalled game or punch module
// cannot keep SpidyVR forever), a hold lasts capNs at most (then the strike
// must go idle before another may hold), and animated_full_arms is frozen
// for the length of a hold (a menu or ini change lands after it).
class StrikeHold {
  public:
    static constexpr int64_t freshNs = 100'000'000, capNs = 4'000'000'000;
    enum class Event : uint8_t { none, enter, ended, stale, capped };
    static const char* eventName(Event e) {
        switch (e) {
        case Event::enter: return "enter";
        case Event::ended: return "ended";
        case Event::stale: return "stale";
        case Event::capped: return "capped";
        default: return "none";
        }
    }
    struct Out {
        bool hold{};
        Event event{};
        int64_t heldNs{}; // on a leave: how long it held
    };
    // The setting in force: frozen while a hold lasts.
    bool fullArms(bool setting) const { return holding_ ? latched_ : setting; }
    bool holding() const { return holding_; }
    Out update(bool read, bool underWay, int64_t sequence, bool setting, int64_t nowNs) {
        Out out;
        if (read && (seqNs_ < 0 || sequence != seq_)) {
            seq_ = sequence;
            seqNs_ = nowNs;
        }
        const bool fresh = read && seqNs_ >= 0 && nowNs - seqNs_ <= freshNs;
        const bool active = fresh && underWay;
        if (!underWay || !read)
            blocked_ = false; // idle again (or gone): a new strike may hold
        if (holding_) {
            Event leave = Event::none;
            if (!fresh)
                leave = Event::stale;
            else if (!underWay)
                leave = Event::ended;
            else if (nowNs - startNs_ >= capNs) {
                leave = Event::capped;
                blocked_ = true;
            }
            if (leave != Event::none) {
                holding_ = false;
                out.event = leave;
                out.heldNs = nowNs - startNs_;
            }
        } else if (setting && active && !blocked_) {
            holding_ = true;
            latched_ = setting;
            startNs_ = nowNs;
            out.event = Event::enter;
        }
        out.hold = holding_;
        return out;
    }

  private:
    bool holding_{}, latched_{true}, blocked_{};
    int64_t seq_{}, seqNs_{-1}, startNs_{};
};

struct HybridOwner {
    Domain domain = Domain::anim;
    int64_t sinceNs{};
    // MINIMUM STINT / RISING TOUCHDOWN: landings held back (at most one a stint), the last one's reason (1 the
    // minimum stint, 2 a rising touchdown) and how far into the stint it came.
    uint32_t landingsHeld{};
    uint8_t landHeldWhy{};
    int64_t landHeldAtNs{};
    // When the last landing handed him to the animation (EnterAnim::land), until he is VR again (-1: none);
    // lastEnterAnim is only set on the switch's own frame.
    int64_t landNs = -1;
    bool combatSticky{};
    int64_t combatIdleNs{-1};
    bool webIntentWas{};
    float lastSpin{}; // retired (always 0), kept for the diag line
    EnterVr lastEnterVr = EnterVr::none;
    EnterAnim lastEnterAnim = EnterAnim::none;
    // Web zip hold: the last press (or zip key still going out), -1 none.
    // zipFromHold: the swing or Spidy's wall had him when he asked, so VR
    // waits for the game to take the zip instead of switching on the press.
    int64_t zipNs{-1};
    int64_t zipPressNs{-1};
    bool zipFromHold{};
    bool zipRose{}; // the move after the zip rose (a point launch)
    // The midair animation now playing: since when (-1 none), and since when
    // it has been falling (-1 not falling).
    int64_t airAnimNs{-1};
    int64_t fallNs{-1};
    // Native Run: RT last held (-1 never), whether this VR stint is a wall run
    // that came out of it, and the wall it was released on (no wall run again
    // until that wall contact ends).
    int64_t runRtNs{-1};
    bool runStint{};
    bool runReleaseLatch{};
    // How long the last VR stint lasted (set on each VR -> ANIM edge, for diag).
    int64_t lastVrNs{};
    EnterVr vrBy = EnterVr::none;
    bool enterVr{};
    bool enterAnim{};
    // Retired (always false / 0): kept so old diagnostics compile unchanged.
    bool cancelSticky{};
    unsigned lastNativeStart{};
    // A midair animation's end as the proxy reads it, now only seen (not a
    // switch; animEndHandsToVr): set on the frame it is first read.
    bool animEndSeen{};
    // JUMP TO SPIDYVR: what the timed handoff ended (on the frame of a
    // `timed` switch) and how long he had been in that air (ms).
    // LANDING BLEND-IN: the animation has the body before the touchdown (air_landing_release).
    bool handedBack() const { return handedBack_; }
    JumpKind lastJumpKind = JumpKind::none;
    int64_t lastJumpAirNs{};
    TimedWhy lastTimedWhy = TimedWhy::timer; // which rule took the last jump-timer handoff

    // A FLIP BREAK (the user, November 11 23:39 / 23:41: "Any cancel or
    // pitch/tilt will break animation on that exact frame"; turning left or
    // right on the right stick keeps the animation). Called by game_xr as soon
    // as the input is read, before the frame's tracking and hands use the
    // owner, so the frame it is read is SpidyVR's (review M1, for this path):
    // the update that follows this frame keeps enterVr / lastEnterVr ==
    // flip for the handoff (position lock on the feet, seed from the air
    // pose, momentum kept). Only in a midair animation: on the ground nothing
    // switches (the jump then breaks on its first airborne frame).
    bool breakForFlip(const head_probe::StateInputs& s, bool cutscene, int64_t nowNs) {
        return breakNow(EnterVr::flip, s, cutscene, nowNs);
    }
    // INTENT BREAK (the owner, Nov 18, t116u: "every move available out of animation jumping"): a move's intent (the
    // legs engaged with Y+X, a kick finished in the animation) breaks a midair animation on the frame it is read, as a
    // flip's input does. Only in a midair animation.
    bool breakForMove(const head_probe::StateInputs& s, bool cutscene, int64_t nowNs) {
        return breakNow(EnterVr::move, s, cutscene, nowNs);
    }
    // The grip cancel, the same way: on the frame the gate consumes the grip.
    bool breakForCancel(const head_probe::StateInputs& s, bool cutscene, int64_t nowNs) {
        return breakNow(EnterVr::cancel, s, cutscene, nowNs);
    }

    Domain update(const Frame& f, int64_t nowNs) {
        const Domain d = step(f, nowNs);
        if (!f.cutscene) {
            prevSupported_ = supported(f.state);
            prevWall_ = f.state.wall;
            prevRunRt_ = f.state.runRt && !f.state.owned;
            if (f.jumpAsk)
                jumpAskNs_ = nowNs;
        }
        return d;
    }
    JumpKind airKind() const {
        return airKind_;
    }

  private:
    Domain step(const Frame& f, int64_t nowNs) {
        enterVr = enterAnim = false;
        lastEnterVr = EnterVr::none;
        lastEnterAnim = EnterAnim::none;
        animEndSeen = false;
        // A flip break earlier this frame: its switch is this frame's.
        if (carry_ != EnterVr::none) {
            enterVr = true;
            lastEnterVr = carry_;
            carry_ = EnterVr::none;
        }

        if (f.cutscene) {
            clearZip();
            airAnimNs = fallNs = -1;
            webIntentWas = f.webIntent;
            return domain;
        }
        const bool pressed = f.webIntent && !webIntentWas;
        webIntentWas = f.webIntent;
        const bool fighting = f.state.fighting || f.state.dodging;
        if (fighting) {
            if (domain == Domain::anim)
                combatSticky = true;
            combatIdleNs = -1;
        } else if (combatSticky) {
            if (combatIdleNs < 0)
                combatIdleNs = nowNs;
            if (nowNs - combatIdleNs >= combatClearNs)
                combatSticky = false;
        } else {
            combatIdleNs = -1;
        }
        if (f.state.runRt)
            runRtNs = nowNs;
        if (f.state.wall != surfaceWall)
            runReleaseLatch = false;

        // The cancel (a consumed grip, no shot) wins over everything in a
        // midair animation, stale zip or jump asks included.
        if (f.cancelGrip && domain == Domain::anim && !supported(f.state)) {
            clearZip();
            goVr(EnterVr::cancel, nowNs);
            return domain;
        }

        // FULL ARMS: a punch's web strike takes the body wherever he is, and
        // keeps it until the strike ends (landing then hands back as always).
        if (f.strike) {
            clearZip();
            airAnimNs = fallNs = -1;
            if (domain == Domain::anim)
                goVr(EnterVr::punch, nowNs);
            return domain;
        }

        // POINT JUMP: the launch takes the body wherever he is (the ground
        // exception); the feet lock with the switch (game_xr's position lock).
        if (f.pointJump) {
            clearZip();
            airAnimNs = fallNs = -1;
            if (domain == Domain::anim)
                goVr(EnterVr::pointJump, nowNs);
            return domain;
        }

        // QUICK SIDESTEP: the burst takes the body on the ground (the
        // approved ground exception, November 12) and holds it while the
        // burst lasts there; off the ground the air's rules take over.
        if (f.sidestep || (f.sidestepHold && domain == Domain::vr && supported(f.state))) {
            clearZip();
            airAnimNs = fallNs = -1;
            if (f.sidestep && domain == Domain::anim)
                goVr(EnterVr::sidestep, nowNs);
            return domain;
        }

        // WEB LINE CANCEL: a web fired on the ground or a perch takes the body there (the ground exception, as a
        // sidestep) and holds it while the line lasts; off the ground the air's rules take over. Never out of a fight
        // the game still holds (combatSticky).
        const bool onGroundNow = supported(f.state) || (f.perched && f.state.wall != surfaceWall);
        const bool groundWebGo = f.groundWeb && domain == Domain::anim && !combatSticky &&
                                 groundWebRefusal(f.state, f.perched, f.parkourSeconds) == nullptr;
        if (groundWebGo || (f.groundWebHold && domain == Domain::vr && onGroundNow)) {
            clearZip();
            airAnimNs = fallNs = -1;
            if (groundWebGo)
                goVr(EnterVr::cancel, nowNs);
            return domain;
        }

        // While the cancelling grip is still held in the air, nothing else
        // (a stale zip ask) takes the body back; landing still does, below.
        if (f.cancelGripHeld && domain == Domain::vr && !supported(f.state))
            clearZip();
        const bool cancelHeldAir = f.cancelGripHeld && domain == Domain::vr && !supported(f.state);

        // HANDSTAND, the landing's first frames: the hands caught him after the landing handed him to the
        // animation (EnterAnim::land): SpidyVR takes him back, as the catch on the handoff's own frame keeps
        // him.
        if (f.handSupport && domain == Domain::anim && landNs >= 0 && supported(f.state) &&
            nowNs - landNs <= handstandLandNs) {
            airAnimNs = fallNs = -1;
            clearZip();
            goVr(EnterVr::handstand, nowNs);
            return domain;
        }
        // HANDSTAND: the hands hold him on the ground; the landing waits.
        if (f.handSupport && domain == Domain::vr && supported(f.state)) {
            airAnimNs = fallNs = -1;
            clearZip();
            return domain;
        }

        // GROUND: the animation's, always. Landing comes back at once; nothing
        // on the ground (grip, trigger, a web, a jump press) switches. Only a
        // takeoff the swing actually dispatched leaves VR there to leave.
        if (supported(f.state) || (f.perched && f.state.wall != surfaceWall)) {
            airAnimNs = fallNs = -1;
            handedBack_ = false; // the landing came
            const bool churnKind = vrBy == EnterVr::punch || vrBy == EnterVr::cancel;
            const bool guarded = vrBy != EnterVr::handstand && vrBy != EnterVr::swing && vrBy != EnterVr::none;
            const bool hard = std::isfinite(f.velY) && f.velY < hardLandVel;
            const bool churn = guarded && (churnKind || !hard) && nowNs - sinceNs < landMinStintNs;
            const bool rising = f.state.grounded && std::isfinite(f.velY) && f.velY > risingTouchdownVel &&
                                nowNs - sinceNs < risingTouchdownNs;
            if (domain == Domain::vr && !f.groundAction && !f.pointJumpHold && (churn || rising) && !landHeldThisStint_) {
                landHeldThisStint_ = true;
                ++landingsHeld;
                landHeldWhy = rising ? 2 : 1;
                landHeldAtNs = nowNs - sinceNs;
            }
            if (domain == Domain::vr && !f.groundAction && !f.pointJumpHold && !churn && !rising)
                goAnim(nowNs, f.state.grounded || f.perched ? EnterAnim::land : EnterAnim::mount);
            if (f.zipPress || f.state.zipping) {
                zipNs = nowNs;
                if (f.zipPress || zipPressNs < 0)
                    zipPressNs = nowNs;
                zipFromHold = false;
            } else {
                clearZip();
            }
            return domain;
        }

        // Spidy's own wall run / wall jump: SpidyVR's. A wall run that came
        // out of a Native Run hands back to the animation when the stick is
        // let go on the wall (and stays the animation's on that wall).
        if (f.state.wall == surfaceWall) {
            airAnimNs = fallNs = -1;
            if (domain == Domain::vr && runStint && f.moveStick >= 0.f && f.moveStick < runReleaseStick) {
                clearZip();
                runReleaseLatch = true;
                goAnim(nowNs, EnterAnim::runRelease);
                return domain;
            }
            if (domain == Domain::anim && !runReleaseLatch && !f.zipPress && zipNs < 0) {
                const bool fromRun = runRtNs >= 0 && nowNs - runRtNs <= runRecentNs;
                goVr(EnterVr::wallRun, nowNs);
                runStint = fromRun;
                return domain;
            }
            if (domain == Domain::vr && !f.zipPress && zipNs < 0)
                return domain;
            if (domain == Domain::anim && runReleaseLatch && !f.zipPress && zipNs < 0)
                return domain;
        }

        if (cancelHeldAir)
            return domain;
        // A web zip: from VR while the swing or Spidy's wall still has him, VR
        // waits until the game begins the move; otherwise ANIM at once. The zip
        // is the game's until it ends; then it is a midair animation like any.
        if (f.zipPress) {
            zipNs = zipPressNs = nowNs;
            zipFromHold = f.state.owned || f.state.wall == surfaceWall;
            if (domain == Domain::vr && zipFromHold && !f.zipInstant)
                return domain;
            if (domain == Domain::vr)
                goAnim(nowNs, EnterAnim::zip);
            airAnimNs = nowNs;
            fallNs = -1;
            return domain;
        }
        if (zipNs >= 0) {
            if (f.state.zipping)
                zipNs = nowNs; // the zip's keys still go out
            const bool asked = nowNs - zipPressNs < zipAskNs || f.state.zipping;
            if (domain == Domain::vr && zipFromHold) {
                const bool gameTook = !f.state.owned && f.state.wall != surfaceWall;
                if (gameTook) {
                    goAnim(nowNs, EnterAnim::zip);
                    airAnimNs = nowNs;
                    fallNs = -1;
                    return domain;
                }
                if (pressed || !asked)
                    clearZip();
                else
                    return domain;
            } else if (domain == Domain::anim) {
                if (f.cancelGrip) {
                    clearZip();
                    goVr(EnterVr::cancel, nowNs);
                    return domain;
                }
                // The swing still carrying him (the game never took it): VR.
                if (!asked && f.state.owned) {
                    clearZip();
                    goVr(f.state.webbed ? EnterVr::web : EnterVr::swing, nowNs);
                    return domain;
                }
                // The zip plays, and what it leads into: a point launch rises
                // off the point and ends at its top (the fall held); a zip
                // with no launch after it plays until he lands or perches, or
                // the game's longest zip.
                if (!asked && std::isfinite(f.velY) && f.velY > zipRiseVel && !zipRose) {
                    zipRose = true;
                    zipLaunchNs_ = nowNs;
                }
                // JUMP TO SPIDYVR: the zip's point launch hands over its timer
                // after the launch began (not while the zip still pulls him).
                if (zipRose && f.jumpHandoffNs > 0 && !asked && !fighting && !combatSticky &&
                    nowNs - zipLaunchNs_ >= f.jumpHandoffNs) {
                    const int64_t air = nowNs - zipLaunchNs_;
                    clearZip();
                    goTimed(JumpKind::zipLaunch, air, nowNs);
                    return domain;
                }
                if (zipRose && std::isfinite(f.velY) && f.velY <= 0.f) {
                    if (fallNs < 0)
                        fallNs = nowNs;
                } else {
                    fallNs = -1;
                }
                const bool launchOver = zipRose && fallNs >= 0 && nowNs - fallNs >= fallHoldNs;
                if (asked || (nowNs - zipNs < zipMaxNs && !launchOver))
                    return domain;
                animEndSeen = true;
                const bool rose = zipRose;
                clearZip();
                if (animEndHandsToVr)
                    goVr(EnterVr::animEnd, nowNs);
                else {
                    // Plays to the landing, or (JUMP TO SPIDYVR) on into the air's timer.
                    // A launch's air counts from its rise (its timer goes on).
                    airAnimNs = rose && zipLaunchNs_ >= 0 ? zipLaunchNs_ : nowNs;
                    fallNs = -1, animEndLatched_ = true;
                    airKind_ = rose ? JumpKind::zipLaunch : JumpKind::fall;
                    airRose_ = rose;
                }
                return domain;
            } else {
                clearZip();
            }
        }

        if (domain == Domain::vr) {
            // LANDING BLEND-IN: the impact is about to happen: the game has the body for its landing (never in a fight, a zip, a
            // hold or a handstand).
            if (f.landHandBack && !fighting && zipNs < 0 && !cancelHeldAir && !f.handSupport && !f.pointJumpHold &&
                !f.strike && nowNs - sinceNs >= minDwellNs) {
                handedBack_ = true;
                lostNs_ = -1;
                goAnim(nowNs, EnterAnim::predicted);
            }
            return domain; // the air is SpidyVR's until he lands
        }

        // A MIDAIR ANIMATION: the game's jump, launch, vault or fall plays
        // uninterrupted. The cancel, Spidy's wall (above) or a SpidyVR web
        // takes it at once; otherwise VR only once it has ended in the air.
        if (airAnimNs < 0) {
            airAnimNs = nowNs;
            fallNs = -1;
            animEndLatched_ = false;
            // What this air is, from the frame before the takeoff.
            airRose_ = false;
            if (prevWall_ == surfaceGame || prevWall_ == surfaceWall)
                airKind_ = JumpKind::wallLaunch;
            else if (prevRunRt_ || (f.state.runRt && !f.state.owned))
                airKind_ = JumpKind::vault;
            else if (jumpAskNs_ >= 0 && nowNs - jumpAskNs_ <= jumpAskRecentNs)
                airKind_ = JumpKind::jump;
            else
                airKind_ = JumpKind::fall;
        }
        if (std::isfinite(f.velY) && f.velY > jumpRiseVel)
            airRose_ = true;
        if (f.cancelGrip) {
            goVr(EnterVr::cancel, nowNs);
            return domain;
        }
        if (f.webRising || (f.state.webbed && f.state.owned)) {
            goVr(EnterVr::web, nowNs);
            return domain;
        }
        // LANDING BLEND-IN: handed the body before the touchdown, the animation keeps it to the landing; the breaks above (a web, a
        // grip cancel, a flip, a strike, a point jump) took it at once. Nothing came of the impact: the air is SpidyVR's again.
        if (handedBack_) {
            if (std::isfinite(f.landEta) && f.landEta <= landKeepEta)
                lostNs_ = -1;
            else if (lostNs_ < 0)
                lostNs_ = nowNs;
            const int64_t held = nowNs - sinceNs;
            if ((lostNs_ >= 0 && nowNs - lostNs_ >= landLostNs && held >= minDwellNs) || held >= landHandBackMaxNs) {
                lastTimedWhy = TimedWhy::noLanding;
                goTimed(JumpKind::fall, held, nowNs);
            }
            return domain;
        }
        if (std::isfinite(f.velY) && f.velY <= 0.f) {
            if (fallNs < 0)
                fallNs = nowNs;
        } else {
            fallNs = -1;
        }
        const bool busy = f.state.zipping || fighting || combatSticky;
        const int64_t inAir = nowNs - airAnimNs;
        const bool fell = fallNs >= 0 && nowNs - fallNs >= fallHoldNs && inAir >= airAnimMinNs;
        if (!busy && (fell || inAir >= airAnimMaxNs) && !animEndLatched_) {
            animEndSeen = animEndLatched_ = true;
            if (animEndHandsToVr) {
                goVr(EnterVr::animEnd, nowNs);
                return domain;
            }
        }
        // JUMP TO SPIDYVR: the timer ends the midair animation (never during a
        // fight, a dodge or a zip; landing first keeps it the animation's).
        // AIR TAKEOVER (the user, Nov 14 t71u, t73u, t73s656: "give the air to SpidyVR completely", "take over 0.3 s after
        // leaving the ground; vertical speed under 4 m/s, or an input that needs SpidyVR, can take over earlier"): the
        // takeoff animation plays, and the air is SpidyVR's at the first of
        //   the timer (jumpHandoffNs, 300 ms by default),
        //   a jump's vertical speed under takeoffDoneVel once it has had takeoffMinNs (a hop never reaches the timer),
        //   a fall with no press (no jump ask, never rose: a ledge, a stumble) after fallHandoffNs, there being no takeoff
        //   animation to wait for,
        // and the inputs that need SpidyVR (web, grip, flip, wall, point jump) took it above, at once. The landing is the
        // animation's: only the air is VR's (VR -> ANIM on touchdown).
        // AIR TAKEOVER (slice 9): the air goes back to SpidyVR after a request's move (a web strike, a finisher, a dive) or a
        // fall's start only if no landing is near: with the ground less than landKeepEta ahead the animation keeps the body to
        // its landing (a hop, a short drop and the end of a strike near the street never switch twice).
        const bool landSoon = std::isfinite(f.landEta) && f.landEta <= landKeepEta;
        if (f.jumpHandoffNs > 0 && !busy && !landSoon) {
            JumpKind kind = airKind_;
            if (kind == JumpKind::fall && airRose_)
                kind = JumpKind::jump;
            TimedWhy why = TimedWhy::timer;
            bool due = inAir >= f.jumpHandoffNs;
            if (!due && kind == JumpKind::jump && f.takeoffDoneVel > 0 && inAir >= f.takeoffMinNs &&
                std::isfinite(f.velY) && f.velY < f.takeoffDoneVel) {
                due = true;
                why = TimedWhy::slowRise;
            }
            if (!due && kind == JumpKind::fall && f.fallHandoffNs > 0 && inAir >= f.fallHandoffNs) {
                due = true;
                why = TimedWhy::fall;
            }
            if (due) {
                lastTimedWhy = why;
                goTimed(kind, inAir, nowNs);
                return domain;
            }
        }
        return domain;
    }

  public:

    // True while the game's animation owns the body (and the head follows it).
    bool animOwns(bool movesOn, const head_probe::StateInputs& s) const {
        return movesOn && domain == Domain::anim && s.playing && s.fresh;
    }

    // Older name, same answer as animOwns.
    bool yielding(bool movesOn, const head_probe::StateInputs& s, bool /*webAimIntent*/ = false,
                  bool /*armsHandlesAim*/ = false) const {
        return animOwns(movesOn, s);
    }

  private:
    void goTimed(JumpKind kind, int64_t airNs, int64_t nowNs) {
        lastJumpKind = kind == JumpKind::none ? JumpKind::fall : kind;
        lastJumpAirNs = airNs;
        goVr(EnterVr::timed, nowNs);
    }
    JumpKind airKind_ = JumpKind::none;
    bool handedBack_{};
    bool landHeldThisStint_{};
    int64_t lostNs_{-1};
    bool airRose_{}, prevSupported_{true}, prevRunRt_{};
    unsigned prevWall_{};
    int64_t jumpAskNs_{-1}, zipLaunchNs_{-1};
    EnterVr carry_ = EnterVr::none; // an early break this frame (breakNow)
    bool breakNow(EnterVr why, const head_probe::StateInputs& s, bool cutscene, int64_t nowNs) {
        if (cutscene || domain != Domain::anim || supported(s) || s.wall == surfaceWall)
            return false;
        clearZip();
        goVr(why, nowNs);
        carry_ = why;
        return true;
    }
    bool animEndLatched_{}; // the end read once per midair animation
    void clearZip() {
        zipNs = zipPressNs = -1;
        zipFromHold = false;
        zipRose = false;
    }
    void goVr(EnterVr why, int64_t nowNs) {
        domain = Domain::vr;
        handedBack_ = false;
        landHeldThisStint_ = false;
        sinceNs = nowNs;
        enterVr = true;
        lastEnterVr = why;
        landNs = -1;
        vrBy = why;
        airAnimNs = fallNs = -1;
        runStint = false;
        combatSticky = false;
        combatIdleNs = -1;
    }
    void goAnim(int64_t nowNs, EnterAnim why) {
        lastVrNs = nowNs - sinceNs;
        domain = Domain::anim;
        sinceNs = nowNs;
        enterAnim = true;
        lastEnterAnim = why;
        landNs = why == EnterAnim::land ? nowNs : -1;
        vrBy = EnterVr::none;
        airAnimNs = fallNs = -1;
        runStint = false;
        combatSticky = false;
        combatIdleNs = -1;
    }
};

} // namespace spidy::hybrid_owner
