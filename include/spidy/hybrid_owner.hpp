// ANIMATED HYBRID (vr_settings::Values::animatedHybrid, off by default): keep
// the game's animation on the ground (Native Run / Spidy Run / fight / perch /
// vault) and hand the body back to OG SpidyVR for every traversal state (jump,
// freefall, swing, web, zip, wall crawl/run). Pure domain FSM — no Windows.
// Slice 1: ownership gate only. Clean eye/IK handoff is a later slice.
#pragma once
#include "spidy/animated_moves.hpp"
#include "spidy/head_probe.hpp"
#include <cstdint>

namespace spidy::hybrid_owner {

enum class Domain : uint8_t { ground, traversal };

enum class EnterT : uint8_t {
    none,
    web,
    zip,
    wall,
    jump,
    swingTook,
};

inline const char* enterTName(EnterT e) {
    switch (e) {
    case EnterT::web: return "web";
    case EnterT::zip: return "zip";
    case EnterT::wall: return "wall";
    case EnterT::jump: return "jump";
    case EnterT::swingTook: return "swing-took";
    default: return "none";
    }
}

inline const char* domainName(Domain d) {
    return d == Domain::traversal ? "traversal" : "ground";
}

// Leave-ground / land timings (nanoseconds).
constexpr int64_t jumpHoldNs = 120'000'000;     // grounded off before jump counts
constexpr int64_t landHoldNs = 250'000'000;     // grounded on before land confirms
constexpr int64_t jumpRearmNs = 150'000'000;    // after land, jump needs this in G
constexpr int64_t combatClearNs = 300'000'000;  // sticky combat ends after idle
constexpr float jumpVz = 1.5f;                  // m/s up
constexpr float jumpHeight = 0.4f;              // m above takeoff feet

struct Frame {
    head_probe::StateInputs state{};
    bool webRising{};   // a web attached this frame
    bool zipRising{};   // zip started this frame
    bool wallRising{};  // wall != 0 rose this frame (not ground parkour)
    bool swingTook{};   // owned rose while !grounded
    float velocityY{};  // world m/s
    float feetY{};      // world m (actor feet)
    bool cutscene{};    // !playing || !fresh — freeze domain
};

struct HybridOwner {
    Domain domain = Domain::ground;
    int64_t sinceNs{};
    int64_t ungroundedNs{-1};
    int64_t groundedNs{-1};
    int64_t landConfirmNs{-1};
    float takeoffY{};
    bool takeoffYSet{};
    bool combatSticky{};
    int64_t combatIdleNs{-1};
    EnterT lastEnterT = EnterT::none;
    bool enterT{};
    bool enterG{};
    bool bounceSuppressed{};
    const char* bounceWhy{};

    Domain update(const Frame& f, int64_t nowNs) {
        enterT = enterG = bounceSuppressed = false;
        lastEnterT = EnterT::none;
        bounceWhy = nullptr;

        if (f.cutscene) {
            // Freeze: keep domain, clear pending jump/land clocks so a long
            // cutscene does not fire a stale edge on return.
            ungroundedNs = groundedNs = landConfirmNs = -1;
            takeoffYSet = false;
            return domain;
        }

        const bool fighting = f.state.fighting || f.state.dodging;
        if (fighting) {
            if (domain == Domain::ground)
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

        if (domain == Domain::ground) {
            // Immediate traversal events win over combat sticky.
            if (f.webRising) {
                goT(EnterT::web, nowNs);
                return domain;
            }
            if (f.zipRising) {
                goT(EnterT::zip, nowNs);
                return domain;
            }
            if (f.wallRising) {
                goT(EnterT::wall, nowNs);
                return domain;
            }
            if (f.swingTook) {
                goT(EnterT::swingTook, nowNs);
                return domain;
            }

            if (!f.state.grounded) {
                if (ungroundedNs < 0) {
                    ungroundedNs = nowNs;
                    takeoffY = f.feetY;
                    takeoffYSet = true;
                }
                const bool held = nowNs - ungroundedNs >= jumpHoldNs;
                const float rise = takeoffYSet ? (f.feetY - takeoffY) : 0.f;
                const bool rose = f.velocityY > jumpVz || rise > jumpHeight;
                if (held && rose) {
                    if (combatSticky) {
                        // Sticky combat: stay G through a launcher hop unless a
                        // web/zip/wall/swing event already fired above.
                        return domain;
                    }
                    if (nowNs - sinceNs < jumpRearmNs && sinceNs > 0) {
                        bounceSuppressed = true;
                        bounceWhy = "jump-rearm";
                        return domain;
                    }
                    goT(EnterT::jump, nowNs);
                    return domain;
                }
            } else {
                ungroundedNs = -1;
                takeoffYSet = false;
            }
            return domain;
        }

        // Traversal → ground: slow confirm.
        const bool clear =
            f.state.grounded && !f.state.webbed && !f.state.zipping && f.state.wall == 0;
        if (clear) {
            if (groundedNs < 0)
                groundedNs = nowNs;
            if (nowNs - groundedNs >= landHoldNs) {
                goG(nowNs);
                return domain;
            }
        } else {
            groundedNs = -1;
        }
        return domain;
    }

    bool yielding(bool movesOn, const head_probe::StateInputs& s, bool webAimIntent,
                  bool armsHandlesAim) const {
        if (!movesOn || domain != Domain::ground)
            return false;
        // Ground domain: MOVES ground / vault / sticky combat. Air forced off.
        if (!s.playing || !s.fresh)
            return false;
        if (animated_moves::freefallWebInterrupt(webAimIntent, s, armsHandlesAim))
            return false;
        const auto st = head_probe::classify(s);
        if (st == head_probe::State::ground)
            return true;
        if (st == head_probe::State::parkour && s.wall == 0)
            return true;
        if (st == head_probe::State::combat && combatSticky)
            return true;
        return false;
    }

private:
    void goT(EnterT why, int64_t nowNs) {
        domain = Domain::traversal;
        sinceNs = nowNs;
        enterT = true;
        lastEnterT = why;
        ungroundedNs = groundedNs = -1;
        takeoffYSet = false;
        // Leaving via web/zip/wall/swing clears sticky; jump can keep it until
        // the hop ends, but we already blocked sticky jumps above.
        if (why != EnterT::jump)
            combatSticky = false;
    }
    void goG(int64_t nowNs) {
        domain = Domain::ground;
        sinceNs = nowNs;
        enterG = true;
        ungroundedNs = groundedNs = -1;
        takeoffYSet = false;
        combatSticky = false;
        combatIdleNs = -1;
    }
};

} // namespace spidy::hybrid_owner
