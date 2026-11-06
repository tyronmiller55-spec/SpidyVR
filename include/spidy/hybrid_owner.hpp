// ANIMATED HYBRID (vr_settings::Values::animatedHybrid, off by default): the
// game's animation owns the body by default (ANIM) — running, fighting,
// parkour hops and vaults, jumps, game zips, the game's crawls, falls — and the
// headset stays on the hero's head the way finishers do. OG SpidyVR (VR) takes
// over only when the player asks for it: a web (attach, or trigger/grip web
// intent in the air), a mid-air rotation (R-stick held or a ~90° body turn),
// real speed in the air, or Spidy's own wall run. Back to ANIM after a
// confirmed landing. Pure domain FSM — no Windows.
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
};

inline const char* enterVrName(EnterVr e) {
    switch (e) {
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
    default:
        return "none";
    }
}

inline const char* domainName(Domain d) {
    return d == Domain::vr ? "vr" : "anim";
}

// game_swing::Surface values as Frame::state.wall carries them (kept here so the
// FSM stays pure). Only `wall` (the swing's own wall run) is VR intent; the
// game's crawl (`game`) and the floor crawl flag (`floor`, lit by ordinary
// rooftop parkour) stay ANIM.
constexpr unsigned surfaceWall = 1, surfaceGame = 2, surfaceFloor = 3;

// Timings (nanoseconds) and thresholds. Provisional, behind HYBRID only.
constexpr int64_t landHoldNs = 250'000'000;    // grounded + clear before VR -> ANIM
constexpr int64_t vrMinDwellNs = 500'000'000;  // VR at least this long (no ping-pong)
constexpr int64_t combatClearNs = 300'000'000; // sticky combat ends after idle
constexpr float vrSpeed = 15.f;                // m/s horizontal, measured
constexpr int64_t speedHoldNs = 200'000'000;   // default hold (ANIMATED HYBRID SPEED)
constexpr float rotateStick = .5f;             // |R-stick x|
constexpr int64_t rotateHoldNs = 150'000'000;  // R-stick held this long
constexpr float spinRadians = 1.5707963f;      // 90° body/HMD turn ...
constexpr int64_t spinWindowNs = 500'000'000;  // ... within this window
constexpr int64_t airWebAirNs = 80'000'000;    // airborne this long before air-web counts

struct Frame {
    head_probe::StateInputs state{};
    bool webRising{};   // a web attached this frame
    bool webIntent{};   // trigger/grip web button held (ground: a fist)
    float speedH{};     // horizontal measured (position-delta) speed, m/s
    float rStickX{};    // right stick x, -1..1
    float hmdYaw{};     // headset yaw in tracking space, radians
    bool hmdYawValid{}; // hmdYaw is a tracked sample
    bool cutscene{};    // !playing || !fresh — freeze domain
    // ANIMATED HYBRID SPEED: how long speed must hold before VR; 0 = speed never
    // forces a takeover (webs, mid-air turns and wall runs still do).
    int64_t speedHoldNs = hybrid_owner::speedHoldNs;
};

// Airborne for intent purposes: off the ground and not on the game's own crawl.
// The floor-crawl flag counts as no surface (it lights during rooftop parkour).
inline bool airborne(const head_probe::StateInputs& s) {
    return !s.grounded && s.wall != surfaceGame;
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
    bool enterVr{};
    bool enterAnim{};

    Domain update(const Frame& f, int64_t nowNs) {
        enterVr = enterAnim = false;
        lastEnterVr = EnterVr::none;
        lastSpin = 0;

        if (f.cutscene) {
            // Freeze: keep domain, clear pending clocks so a long cutscene does
            // not fire a stale edge on return.
            resetClocks();
            webIntentWas = f.webIntent;
            return domain;
        }

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
            if (!air) {
                // On the ground, grip is a fist; jumps, hops, zips, crawls stay ANIM.
                airWebArmed = false;
                speedNs = rotateNs = -1;
                clearSpin();
                return domain;
            }
            const bool sticky = combatSticky || fighting;
            // Air-web: the press has to start in the air (a ground fist held
            // through a hop is not a web), then hold past a brief air time.
            if (pressed && !sticky)
                airWebArmed = true;
            if (!f.webIntent || sticky)
                airWebArmed = false;
            if (airWebArmed && nowNs - airNs >= airWebAirNs) {
                goVr(EnterVr::airWeb, nowNs);
                return domain;
            }
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

        // VR -> ANIM: grounded and clear for landHoldNs, after the VR minimum.
        // A zip while in VR stays VR until it lands (zipping is not clear). Only
        // Spidy's own wall keeps VR; the game's crawl / floor flag do not.
        const bool clear =
            f.state.grounded && !f.state.webbed && !f.state.zipping && f.state.wall != surfaceWall;
        if (clear) {
            if (groundedNs < 0)
                groundedNs = nowNs;
            if (nowNs - groundedNs >= landHoldNs && nowNs - sinceNs >= vrMinDwellNs) {
                goAnim(nowNs);
                return domain;
            }
        } else {
            groundedNs = -1;
        }
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
        resetClocks();
        combatSticky = false;
        combatIdleNs = -1;
    }
    void goAnim(int64_t nowNs) {
        domain = Domain::anim;
        sinceNs = nowNs;
        enterAnim = true;
        resetClocks();
        combatSticky = false;
        combatIdleNs = -1;
    }
};

} // namespace spidy::hybrid_owner
