// ANIMATED MOVES (vr_settings::Values::animatedMoves, off by default): step two
// of a head lock. When on, the game's own animation drives the hero's body in
// ground parkour, vaults and mantles unlocked by NATIVE RUN (RT). The pose
// writer clears bodyOn so the body blends back to the game's joints
// (native_body::Flags::bodyOn); NATIVE RUN's RT path is untouched.
//
// ANIMATED AIR (vr_settings::Values::animatedAir, off by default): when on, the
// same pose yield covers freefall, swing and web-hold air (head_probe air /
// swing / web). Physics ownership stays with the mod's swing / native movement
// — skeletal animation alone does not supply velocity. Zip, wall-run, combat
// and cutscenes stay with the mod's body. Point-jump / perch: perch is ground
// yield (ANIMATED MOVES); point-jump air uses this gate once ungrounded.
//
// ANIMATED HEAD (vr_settings::Values::animatedHead, Off|Level|Full, default Off):
// while yieldsBody, attach the VR view to the hero's gameplay head the way
// finishers do (anchored 2/4 → resolvedOnHead), without turning on the cutscene
// worker (game_fp::want). Air / swing / web yields clamp Full→Level for comfort
// until a separate Full-air opt-in exists.
//
// Web aim interrupt: freefall web-button intent stops the air pose yield so VR
// arms reclaim immediately; attached webs keep the swing anim (physics still
// mod-owned). Arms blend while yielding is a later slice.
//
// Handoff: when yieldsBody ends, resync actor/IK/tracking (animated_moves::Handoff
// + native_body::resyncAfterYield + GameTrackingRig::adoptFeet).
#pragma once
#include "spidy/head_probe.hpp"

namespace spidy::animated_moves {

// Which pose-yield gates are on (each setting off by default).
struct Gate {
    bool moves{}; // ANIMATED MOVES: ground + vault/mantle parkour
    bool air{};   // ANIMATED AIR: freefall / swing / web-hold air
};

// True when a freefall web/aim button press should reclaim the VR body so the
// player can aim out of an air anim. Attached webs do not count — those keep
// the swing pose while the mod still owns physics.
inline bool freefallWebInterrupt(bool webAimIntent, const head_probe::StateInputs& s) {
    return webAimIntent && !s.webbed;
}

// True when the current state should let the game's animation drive the body.
// Wall-run under NATIVE RUN (parkour with wall != 0), zip, combat and cutscenes
// stay with the mod. Physics ownership is independent (swing owned).
inline bool yieldsBody(Gate g, const head_probe::StateInputs& s, bool webAimIntent = false) {
    if (!g.moves && !g.air)
        return false;
    if (freefallWebInterrupt(webAimIntent, s))
        return false;
    const auto st = head_probe::classify(s);
    if (g.moves) {
        if (st == head_probe::State::ground)
            return true;
        if (st == head_probe::State::parkour && s.wall == 0)
            return true;
    }
    if (g.air) {
        if (st == head_probe::State::air || st == head_probe::State::swing ||
            st == head_probe::State::web)
            return true;
    }
    return false;
}

// Backward-compatible ground-only gate (tests / call sites without air).
inline bool yieldsBody(bool movesOn, const head_probe::StateInputs& s) {
    return yieldsBody(Gate{movesOn, false}, s, false);
}

inline const char* yieldName(const head_probe::StateInputs& s) {
    return head_probe::stateName(head_probe::classify(s));
}

// ANIMATED HEAD choices (vr_settings Int, swing Settings::animatedHead).
enum class HeadFollow : int { off = 0, level = 1, full = 2 };

inline HeadFollow headFollow(int setting) {
    if (setting == 2)
        return HeadFollow::full;
    if (setting == 1)
        return HeadFollow::level;
    return HeadFollow::off;
}

// Effective head-follow mode while yielding: air/swing/web never take Full
// (comfort), even if the setting is Full for ground parkour.
inline HeadFollow headFollowForYield(int headSetting, const head_probe::StateInputs& s) {
    const HeadFollow mode = headFollow(headSetting);
    if (mode == HeadFollow::off)
        return HeadFollow::off;
    const auto st = head_probe::classify(s);
    if (st == head_probe::State::air || st == head_probe::State::swing ||
        st == head_probe::State::web)
        return HeadFollow::level;
    return mode;
}

inline bool followsHead(int headSetting, bool yielding, const head_probe::StateInputs& s) {
    return yielding && headFollowForYield(headSetting, s) != HeadFollow::off;
}

// Legacy overload: no state → treat as ground (Full allowed). Prefer the
// StateInputs overload at call sites that know the yield state.
inline bool followsHead(int headSetting, bool yielding) {
    return yielding && headFollow(headSetting) != HeadFollow::off;
}

inline const char* headFollowName(HeadFollow mode) {
    switch (mode) {
    case HeadFollow::full:
        return "full";
    case HeadFollow::level:
        return "level";
    default:
        return "off";
    }
}

struct Handoff {
    bool yielding{};
    bool enter{};
    bool exit{};

    void update(bool now) {
        enter = now && !yielding;
        exit = !now && yielding;
        yielding = now;
    }
};

inline bool needsActorResync(const Handoff& h) { return h.exit; }

} // namespace spidy::animated_moves
