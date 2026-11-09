// ANIMATED HYBRID (vr_settings::Values::animatedHybrid, off by default): the
// game's animation owns the body by default (ANIM) — running, fighting,
// parkour hops and vaults, jumps, game zips, the game's crawls, falls — and the
// headset stays on the hero's head the way finishers do. OG SpidyVR (VR) takes
// over only when the player asks for it: a web (attach, or trigger/grip web
// intent in the air), a mid-air rotation (R-stick held or a ~90° body turn),
// real speed in the air, Spidy's own wall run, or a real jump or leap the
// player asked for (Frame::jumpAsk), on the frame he leaves the ground: the
// game's takeoff plays, and SpidyVR picks the body up from that pose (jump
// adopt; native_body::beginJumpAdopt). Grip cancel is a consumed, no-shot
// switch to VR (the user, November 8 21:51: it supersedes "release restores
// animation"): VR then STAYS - full OG SpidyVR, webs on the ground included -
// until another native animation starts on its own (nativeStarts: the game's
// fight or dodge, its parkour, its crawl, or a game zip begins). That start
// hands back to ANIM; nothing lingers afterwards. Back to ANIM also once a web zip the player asked for (Y in the air or
// the game's zip to a point) actually begins: that zip is the game's animation
// until it ends. A press the game does not take never leaves VR. Pure domain
// FSM — no Windows.
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

// Why the owner went to VR. Only player intent (or the mod's own wall run).
enum class EnterVr : uint8_t {
    none,
    web,     // a web attached
    airWeb,  // trigger/grip web intent pressed while airborne, not fighting
    rotate,  // R-stick held or a fast body turn while airborne
    speed,   // airborne horizontal measured speed held above vrSpeed
    wallRun, // Spidy's own wall run (game_swing::Surface::wall): the mod drives it
    swing,   // a web zip the game did not take: the swing still carries him in the air
    jump,    // a real jump or leap the player asked for, on the frame he left the ground
    cancel,  // consumed grip: return to VR without a web or other action
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
    default:
        return "none";
    }
}

// Snapshot the live pose and keep the legs airborne: a real jump (even if the
// ground flickers on that frame), or any other takeover while the hero is not
// on the ground. A grounded web, wall run or swing still reclaims without a seed.
inline bool adoptsFromAir(EnterVr why, bool grounded) {
    return why == EnterVr::jump || !grounded;
}

// Why the owner went back to ANIM.
enum class EnterAnim : uint8_t {
    none,
    land, // legacy diagnostic value; current ground rule uses `ground`
    zip,  // the game began a web zip the player asked for (Frame::zipPress)
    // A jump taken over (EnterVr::jump) turned out to be a wall mount: the
    // game's crawl has him on the wall, and nothing else of the player's keeps
    // VR (no web). That climb is the game's animation, as any wall mount is.
    mount,
    ground, // eligible standing/walking/running/crawl: native immediately
    native, // after a grip cancel: another native animation started on its own
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
    default:
        return "none";
    }
}

// Whether a return to ANIM seeds the hero's facing toward where the player last
// looked in VR (the landing blend's short stick push). A web zip is the game's
// move once it begins: nothing of the player's is steered into it.
inline bool seedsFacing(EnterAnim why) {
    return why == EnterAnim::land;
}

inline const char* domainName(Domain d) {
    return d == Domain::vr ? "vr" : "anim";
}

// game_swing::Surface values as Frame::state.wall carries them (kept here so the
// FSM stays pure). Only `wall` (the swing's own wall run) is VR intent; the
// game's crawl (`game`) and the floor crawl flag (`floor`, lit by ordinary
// rooftop parkour) stay ANIM.
constexpr unsigned surfaceWall = 1, surfaceGame = 2, surfaceFloor = 3;

// Historical constants retained for old diagnostics/tests; NOT restoration gates.
constexpr int64_t landHoldNs = 250'000'000;
constexpr int64_t vrMinDwellNs = 500'000'000;
// Active timings (nanoseconds) and thresholds, behind HYBRID only.
constexpr int64_t combatClearNs = 300'000'000; // sticky combat ends after idle
constexpr float vrSpeed = 15.f;                // m/s horizontal, measured
constexpr int64_t speedHoldNs = 200'000'000;   // default hold (ANIMATED HYBRID SPEED)
constexpr float rotateStick = .5f;             // |R-stick x|
constexpr int64_t rotateHoldNs = 150'000'000;  // R-stick held this long
constexpr float spinRadians = 1.5707963f;      // 90° body/HMD turn ...
constexpr int64_t spinWindowNs = 500'000'000;  // ... within this window
constexpr int64_t airWebAirNs = 80'000'000;    // airborne this long before air-web counts
// A web zip's hold (Frame::zipPress). From VR while the swing still has him,
// ANIM starts only when the game begins the move (the swing yields). The game
// begins within game_swing::AirYield::askMs of the press or never does, and
// lets the swing have him back after AirYield::yieldMs at most: the same
// 400 ms / 3 s, so no new feel number.
constexpr int64_t zipAskNs = 400'000'000;
constexpr int64_t zipMaxNs = 3'000'000'000;
// Jump adopt: a jump or leap asked for on the ground (Frame::jumpAsk) counts
// when he leaves the ground within this of the ask. The game's A goes out at
// most 0.2 s after the press (game_pad::JumpRelay), a charged leap within
// ChargeJump::leapSeconds of its release, SPIDY RUN JUMP's own jump within
// Swing::runJumpSeconds; the game's takeoff takes the rest.
constexpr int64_t jumpAskNs = 500'000'000;

struct Frame {
    head_probe::StateInputs state{};
    bool webRising{};   // a web attached this frame
    bool webIntent{};   // web-hold button held (grip by default; trigger if swapped)
    float speedH{};     // horizontal measured (position-delta) speed, m/s
    float rStickX{};    // right stick x, -1..1
    float hmdYaw{};     // headset yaw in tracking space, radians
    bool hmdYawValid{}; // hmdYaw is a tracked sample
    bool cutscene{};    // !playing || !fresh — freeze domain
    // The player pressed a web zip this frame: the game's zip to a point (its LT
    // + RT, the stick up) or its Y in the air (a web strike; a car's in a chase).
    // Not Y on the ground or on Spidy's wall (interact). game_xr sets it on the
    // press frame itself, before the swing or the game has moved. The owner
    // leaves VR only if the game then begins that zip.
    bool zipPress{};
    // The player asked for a real jump or leap this frame, from the ground:
    // A pressed (Command::jump), the game's A sent, a charged leap let go of
    // (Command::leap; ChargeJump, RUNNING LEAP), or SPIDY RUN JUMP's own jump
    // (Command::asks bit 3). Nothing the game does on its own - a parkour
    // vault or mantle, a hop, a combat launcher - sets it.
    bool jumpAsk{};
    // ANIMATED HYBRID SPEED: how long speed must hold before VR; 0 = speed never
    // forces a takeover (webs, mid-air turns and wall runs still do).
    int64_t speedHoldNs = hybrid_owner::speedHoldNs;
    bool cancelGrip{}, cancelGripHeld{}; // consumed no-action exit; hold VR until release
    bool groundAction{}; // actual dispatched takeoff, not a queue/phase cooldown
};

// Airborne for intent purposes: off the ground and not on the game's own crawl.
// The floor-crawl flag counts as no surface (it lights during rooftop parkour).
inline bool airborne(const head_probe::StateInputs& s) {
    return !s.grounded && s.wall != surfaceGame;
}

// Jump adopt: what keeps a jump ask from being the player's own real jump or
// leap - a fight (and its launcher hops), a web zip, any surface (Spidy's
// wall, the game's crawl, the floor crawl), the game's own parkour under
// NATIVE RUN (head_probe::State::parkour: its vault, mantle or wall run).
inline bool jumpBlocked(const head_probe::StateInputs& s, bool sticky) {
    return sticky || s.fighting || s.dodging || s.zipping || s.wall != 0 ||
           head_probe::classify(s) == head_probe::State::parkour;
}

// The native animations that end a grip cancel's VR stay, as bits: the game's
// fight (1), its dodge (2), its parkour under NATIVE RUN (4), its crawl (8).
// Only a bit that rises after the cancel counts (one already on at the cancel
// must end and start again). A game zip ends it through the zip path.
inline unsigned nativeAnimations(const head_probe::StateInputs& s) {
    return (s.fighting ? 1u : 0u) | (s.dodging ? 2u : 0u) |
           (head_probe::classify(s) == head_probe::State::parkour ? 4u : 0u) |
           (s.wall == 2u /* surfaceGame */ ? 8u : 0u);
}
inline unsigned nativeStarts(unsigned before, unsigned now) {
    return now & ~before;
}

struct HybridOwner {
    Domain domain = Domain::anim;
    int64_t sinceNs{};
    int64_t groundedNs{-1};
    int64_t airNs{-1};
    int64_t speedNs{-1};
    int64_t rotateNs{-1};
    bool combatSticky{};
    int64_t combatIdleNs{-1};
    bool webIntentWas{};
    bool airWebArmed{};
    // Unwrapped HMD yaw samples while airborne (tracking space).
    static constexpr unsigned spinSamples = 128;
    int64_t spinT[spinSamples]{};
    float spinYaw[spinSamples]{};
    unsigned spinHead{}, spinCount{};
    float yawUnwrapped{}, yawLast{};
    bool yawLastSet{};
    float lastSpin{}; // largest yaw change (radians) in the window, this frame
    EnterVr lastEnterVr = EnterVr::none;
    EnterAnim lastEnterAnim = EnterAnim::none;
    // Web zip hold: the last press (or zip key still going out), -1 none.
    // zipFromHold: the swing or Spidy's wall had him when he asked, so VR
    // waits for the game to take the zip instead of switching on the press.
    int64_t zipNs{-1};
    int64_t zipPressNs{-1};
    bool zipFromHold{};
    // How long the last VR stint lasted (set on each VR -> ANIM edge, for diag).
    int64_t lastVrNs{};
    // Why VR has the body now (kept for the stint; a web on the way makes it the web's).
    EnterVr vrBy = EnterVr::none;
    // Jump adopt: when the player last asked for a real jump from the ground (-1 none),
    // and whether he stood on the ground last update (the leave-ground frame).
    int64_t jumpAskAtNs{-1};
    bool groundedWas{}, groundedWasSet{};
    bool enterVr{};
    bool enterAnim{};
    // Grip cancel's VR stay (see the top): set on EnterVr::cancel, cleared on
    // any return to ANIM. nativeWas: nativeAnimations() last update.
    bool cancelSticky{};
    unsigned nativeWas{};
    unsigned lastNativeStart{}; // the bits that ended the stay (diagnostics)

    Domain update(const Frame& f, int64_t nowNs) {
        enterVr = enterAnim = false;
        lastEnterVr = EnterVr::none;
        lastEnterAnim = EnterAnim::none;
        lastSpin = 0;

        if (f.cutscene) {
            // Freeze: keep domain, clear pending clocks so a long cutscene does
            // not fire a stale edge on return.
            resetClocks();
            clearZip();
            jumpAskAtNs = -1;
            groundedWasSet = false;
            webIntentWas = f.webIntent;
            return domain;
        }
        // Explicit cancel wins over a stale zip/jump/web edge, including combat
        // and grounded animation. Existing pose adoption/blend handles continuity.
        const unsigned nativeNow = nativeAnimations(f.state);
        const unsigned started = nativeStarts(nativeWas, nativeNow);
        nativeWas = nativeNow;
        if (f.cancelGrip && domain == Domain::anim) {
            clearZip();
            webIntentWas = false;
            goVr(EnterVr::cancel, nowNs);
            cancelSticky = true;
            return domain;
        }
        if (domain == Domain::vr && f.cancelGripHeld) {
            resetClocks();
            clearZip();
            jumpAskAtNs = -1;
            webIntentWas = false;
            return domain;
        }
        if (domain == Domain::vr && cancelSticky && started) {
            lastNativeStart = started;
            goAnim(nowNs, EnterAnim::native);
            return domain;
        }
        // The frame he leaves the ground (the swing's sample of the game's contact).
        const bool leftGround = groundedWasSet && groundedWas && !f.state.grounded;
        groundedWas = f.state.grounded;
        groundedWasSet = true;

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

        const bool air = airborne(f.state);
        const bool pressed = f.webIntent && !webIntentWas;
        webIntentWas = f.webIntent;
        if (air) {
            if (airNs < 0)
                airNs = nowNs;
        } else {
            airNs = -1;
        }

        // Definitive ground contract: grip-held cancellation is the explicit
        // exception above. Ground arm actions and passive attached webs do not
        // require the OG body. Only an actually dispatched takeoff may defer.
        const bool supported = f.state.grounded || f.state.wall == surfaceGame;
        if (supported && f.state.wall != surfaceWall && !f.groundAction && !cancelSticky) {
            if (domain == Domain::vr) goAnim(nowNs, EnterAnim::ground);
            if (f.zipPress || f.state.zipping) {
                jumpAskAtNs = -1;
                zipNs = nowNs;
                if (f.zipPress || zipPressNs < 0) zipPressNs = nowNs;
                zipFromHold = false;
                holdForZip();
            } else {
                clearZip();
                const bool blocked = jumpBlocked(f.state, combatSticky || fighting);
                if (f.jumpAsk) jumpAskAtNs = blocked ? -1 : nowNs;
                if (jumpAskAtNs >= 0 && (blocked || nowNs-jumpAskAtNs > jumpAskNs)) jumpAskAtNs = -1;
            }
            return domain;
        }
        // A web zip: remember the press. From animation, hold ANIM for that zip
        // (no land confirm). From VR while the swing or Spidy's wall still has
        // him, stay in VR until the game begins the move (the swing yields). A
        // press the game refuses then never leaves VR. From VR with no swing (a
        // speed or rotate flight) there is no yield to watch, so ANIM on the
        // press still gets out of the way. A fresh grip is the explicit way out
        // of an ANIM zip hold.
        if (f.zipPress) {
            jumpAskAtNs = -1; // a zip is the game's move, never a jump to adopt
            zipNs = zipPressNs = nowNs;
            zipFromHold = f.state.owned || f.state.wall == surfaceWall;
            if (domain == Domain::vr && zipFromHold)
                return domain;
            if (domain == Domain::vr)
                goAnim(nowNs, EnterAnim::zip);
            holdForZip();
            return domain;
        }
        if (zipNs >= 0) {
            jumpAskAtNs = -1;
            if (f.state.zipping)
                zipNs = nowNs; // the zip's keys still go out
            const bool asked = nowNs - zipPressNs < zipAskNs || f.state.zipping;
            if (domain == Domain::vr && zipFromHold) {
                // Waiting in VR for the game to take a zip the swing still has.
                const bool gameTook = !f.state.owned && f.state.wall != surfaceWall;
                if (gameTook) {
                    goAnim(nowNs, EnterAnim::zip);
                    holdForZip();
                    return domain;
                }
                if (pressed || !asked)
                    clearZip();
                else
                    return domain;
            } else {
                // Done: landed, the swing has him again (a press from animation
                // the game refused, or its move over and a web of the swing's
                // carrying him), or the longest the game keeps him.
                const bool done = nowNs - zipNs >= zipMaxNs || (!asked && (!air || f.state.owned));
                if (!done) {
                    if (pressed) {
                        clearZip();
                        goVr(EnterVr::airWeb, nowNs);
                        return domain;
                    }
                    holdForZip();
                    return domain;
                }
                clearZip();
                // The swing still carrying him in the air (the game never took
                // an ANIM press): the swing's owner again. Spidy's wall is its
                // own wall run, below.
                if (domain == Domain::anim && air && f.state.owned && f.state.wall != surfaceWall) {
                    goVr(f.state.webbed ? EnterVr::web : EnterVr::swing, nowNs);
                    return domain;
                }
            }
        }

        if (domain == Domain::anim) {
            // Immediate: a web attached, or Spidy's own wall run (mod drives it).
            if (f.webRising) {
                goVr(EnterVr::web, nowNs);
                return domain;
            }
            if (f.state.wall == surfaceWall) {
                goVr(EnterVr::wallRun, nowNs);
                return domain;
            }
            // Jump adopt: only a real jump or leap the player asked for on the ground,
            // on the frame he leaves it. Vaults, mantles, hops and launchers ask nothing;
            // a fight, a zip, a wall or a crawl, or the game's parkour block the ask.
            const bool jumpSticky = combatSticky || fighting;
            if (f.jumpAsk && f.state.grounded)
                jumpAskAtNs = jumpBlocked(f.state, jumpSticky) ? -1 : nowNs;
            if (jumpAskAtNs >= 0 && (jumpBlocked(f.state, jumpSticky) || nowNs - jumpAskAtNs > jumpAskNs))
                jumpAskAtNs = -1;
            if (leftGround && air && jumpAskAtNs >= 0) {
                goVr(EnterVr::jump, nowNs);
                return domain;
            }
            if (air)
                jumpAskAtNs = -1; // off the ground some other way (a hop, a fall): spent
            // Grip (the web-hold button) leaves ANIM on that press: on the
            // ground, in the air, or in a fight. Web balls never set webIntent.
            if (pressed) {
                goVr(EnterVr::airWeb, nowNs);
                return domain;
            }
            if (!air) {
                // Hops and crawls stay ANIM; a jump waits for leave-ground.
                airWebArmed = false;
                speedNs = rotateNs = -1;
                clearSpin();
                return domain;
            }
            const bool sticky = combatSticky || fighting;
            // Mid-air rotation: R-stick held, or a fast physical turn.
            if (std::isfinite(f.rStickX) && std::abs(f.rStickX) >= rotateStick) {
                if (rotateNs < 0)
                    rotateNs = nowNs;
            } else {
                rotateNs = -1;
            }
            lastSpin = sampleSpin(f, nowNs);
            if (!sticky && ((rotateNs >= 0 && nowNs - rotateNs >= rotateHoldNs) || lastSpin >= spinRadians)) {
                goVr(EnterVr::rotate, nowNs);
                return domain;
            }
            // Speed: measured, held (a takeoff spike does not count).
            if (std::isfinite(f.speedH) && f.speedH >= vrSpeed) {
                if (speedNs < 0)
                    speedNs = nowNs;
            } else {
                speedNs = -1;
            }
            if (!sticky && f.speedHoldNs > 0 && speedNs >= 0 && nowNs - speedNs >= f.speedHoldNs) {
                goVr(EnterVr::speed, nowNs);
                return domain;
            }
            return domain;
        }

        // A web on the way makes an adopted jump's stint the web's.
        if (vrBy == EnterVr::jump && (f.webRising || f.state.webbed))
            vrBy = EnterVr::web;
        // An adopted jump the game's crawl took onto a wall: a wall mount from a
        // jump, which is the game's animation (no land confirm or VR minimum).
        if (vrBy == EnterVr::jump && f.state.wall == surfaceGame) {
            goAnim(nowNs, EnterAnim::mount);
            return domain;
        }
        // Eligible ground returned above on this very update. No landing
        // confirm, VR minimum, run edge, neutral/repress or queue cooldown.
        return domain;
    }

    // True while the game's animation owns the body (and the head follows it).
    // No per-frame classify: only an owner edge changes this, so one ungrounded
    // frame cannot drop head-follow without a blend.
    bool animOwns(bool movesOn, const head_probe::StateInputs& s) const {
        return movesOn && domain == Domain::anim && s.playing && s.fresh;
    }

    // Older name, same answer as animOwns (web/aim intent is an owner edge now).
    bool yielding(bool movesOn, const head_probe::StateInputs& s, bool /*webAimIntent*/ = false,
                  bool /*armsHandlesAim*/ = false) const {
        return animOwns(movesOn, s);
    }

  private:
    void resetClocks() {
        groundedNs = airNs = speedNs = rotateNs = -1;
        airWebArmed = false;
        clearSpin();
    }
    void clearZip() {
        zipNs = zipPressNs = -1;
        zipFromHold = false;
    }
    // While a zip holds ANIM: intent clocks start over when it ends.
    void holdForZip() {
        speedNs = rotateNs = groundedNs = -1;
        airWebArmed = false;
        clearSpin();
    }
    void clearSpin() {
        spinHead = spinCount = 0;
        yawLastSet = false;
    }
    // Largest |yaw change| between now and any airborne sample in the window.
    float sampleSpin(const Frame& f, int64_t nowNs) {
        if (!f.hmdYawValid || !std::isfinite(f.hmdYaw))
            return 0.f;
        if (!yawLastSet) {
            yawUnwrapped = f.hmdYaw;
            yawLastSet = true;
        } else {
            yawUnwrapped += std::remainder(f.hmdYaw - yawLast, 6.2831853f);
        }
        yawLast = f.hmdYaw;
        spinT[spinHead] = nowNs;
        spinYaw[spinHead] = yawUnwrapped;
        spinHead = (spinHead + 1) % spinSamples;
        if (spinCount < spinSamples)
            ++spinCount;
        float most = 0.f;
        for (unsigned i = 0; i < spinCount; ++i) {
            const unsigned k = (spinHead + spinSamples - 1 - i) % spinSamples;
            if (nowNs - spinT[k] > spinWindowNs)
                break;
            most = std::max(most, std::abs(yawUnwrapped - spinYaw[k]));
        }
        return most;
    }
    void goVr(EnterVr why, int64_t nowNs) {
        domain = Domain::vr;
        sinceNs = nowNs;
        enterVr = true;
        lastEnterVr = why;
        vrBy = why;
        jumpAskAtNs = -1;
        resetClocks();
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
        jumpAskAtNs = -1;
        resetClocks();
        combatSticky = false;
        combatIdleNs = -1;
        cancelSticky = false;
    }
};

} // namespace spidy::hybrid_owner
