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
//     (any midair native animation)          until the animation ends; then
//                                            VR only if still in the air
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
//
// No ownership switch happens on the ground except a FULL ARMS punch's strike
// (the user, November 9 22:27: "punching can pull you out of the animation"). "The animation ended" is
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
};

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
// After a zip, rising faster than this is its point launch.
constexpr float zipRiseVel = 1.f;
// Native Run's stick let go: below this tilt (NATIVE RUN's own off threshold
// is higher; a resting stick reads under it).
constexpr float runReleaseStick = .2f;
// A Native Run counts for the wall run it leads into within this of RT.
constexpr int64_t runRecentNs = 500'000'000;

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
    bool jumpAsk{};     // a jump asked for (diagnostics; the jump is the game's)
    int64_t speedHoldNs = hybrid_owner::speedHoldNs; // retired setting
    // Consumed grip (animation_cancel::Gate, armed only in a midair animation)
    // and that grip still held.
    bool cancelGrip{}, cancelGripHeld{};
    bool groundAction{}; // actual dispatched takeoff, not a queue/phase cooldown
    // FULL ARMS (the user, Nov 9 22:27): a SpidyVR web strike is under way (its
    // flight or hang). It pulls him out of the animation, ground included,
    // and holds SpidyVR until it ends; then the table as usual.
    bool strike{};
    // The hero's vertical velocity, m/s (up +); NaN unknown.
    float velY = NAN;
    // The left (walk) stick's tilt 0..1; negative unknown.
    float moveStick = -1.f;
};

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

struct HybridOwner {
    Domain domain = Domain::anim;
    int64_t sinceNs{};
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

    Domain update(const Frame& f, int64_t nowNs) {
        enterVr = enterAnim = false;
        lastEnterVr = EnterVr::none;
        lastEnterAnim = EnterAnim::none;

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

        // While the cancelling grip is still held in the air, nothing else
        // (a stale zip ask) takes the body back; landing still does, below.
        if (f.cancelGripHeld && domain == Domain::vr && !supported(f.state))
            clearZip();
        const bool cancelHeldAir = f.cancelGripHeld && domain == Domain::vr && !supported(f.state);

        // GROUND: the animation's, always. Landing comes back at once; nothing
        // on the ground (grip, trigger, a web, a jump press) switches. Only a
        // takeoff the swing actually dispatched leaves VR there to leave.
        if (supported(f.state)) {
            airAnimNs = fallNs = -1;
            if (domain == Domain::vr && !f.groundAction)
                goAnim(nowNs, f.state.grounded ? EnterAnim::land : EnterAnim::mount);
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
            if (domain == Domain::vr && zipFromHold)
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
                if (!asked && std::isfinite(f.velY) && f.velY > zipRiseVel)
                    zipRose = true;
                if (zipRose && std::isfinite(f.velY) && f.velY <= 0.f) {
                    if (fallNs < 0)
                        fallNs = nowNs;
                } else {
                    fallNs = -1;
                }
                const bool launchOver = zipRose && fallNs >= 0 && nowNs - fallNs >= fallHoldNs;
                if (asked || (nowNs - zipNs < zipMaxNs && !launchOver))
                    return domain;
                clearZip();
                goVr(EnterVr::animEnd, nowNs);
                return domain;
            } else {
                clearZip();
            }
        }

        if (domain == Domain::vr)
            return domain; // the air is SpidyVR's until he lands

        // A MIDAIR ANIMATION: the game's jump, launch, vault or fall plays
        // uninterrupted. The cancel, Spidy's wall (above) or a SpidyVR web
        // takes it at once; otherwise VR only once it has ended in the air.
        if (airAnimNs < 0) {
            airAnimNs = nowNs;
            fallNs = -1;
        }
        if (f.cancelGrip) {
            goVr(EnterVr::cancel, nowNs);
            return domain;
        }
        if (f.webRising || (f.state.webbed && f.state.owned)) {
            goVr(EnterVr::web, nowNs);
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
        if (!busy && (fell || inAir >= airAnimMaxNs)) {
            goVr(EnterVr::animEnd, nowNs);
            return domain;
        }
        return domain;
    }

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
    void clearZip() {
        zipNs = zipPressNs = -1;
        zipFromHold = false;
        zipRose = false;
    }
    void goVr(EnterVr why, int64_t nowNs) {
        domain = Domain::vr;
        sinceNs = nowNs;
        enterVr = true;
        lastEnterVr = why;
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
        vrBy = EnterVr::none;
        airAnimNs = fallNs = -1;
        runStint = false;
        combatSticky = false;
        combatIdleNs = -1;
    }
};

} // namespace spidy::hybrid_owner
