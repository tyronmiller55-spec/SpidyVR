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
// worker (game_fp::want). Full honors pitch/roll from the anim head in every
// yield (ground and air); Level stays level only when the user picks Level.
// Outside yield the SpidyVR tracking path owns the view (no silent leveling).
//
// Web aim interrupt: freefall web-button intent stops the air pose yield so VR
// arms reclaim immediately; attached webs keep the swing anim (physics still
// mod-owned). Arms blend while yielding is a later slice.
//
// Handoff: when yieldsBody ends, resync actor/IK/tracking (animated_moves::Handoff
// + native_body::resyncAfterYield + GameTrackingRig::adoptFeet).
//
// ANIMATED ONLY (vr_settings::Values::animatedOnly, off by default): with MOVES
// and/or AIR on, the pose yield also covers wall, zip, parkour wall-run and
// combat, and freefall web intent no longer interrupts it — the SpidyVR body
// (bodyOn) never reclaims mid-gameplay, so there is no adoptFeet / head-follow
// off / tracking re-anchor every time the state flips to wall or combat (the
// 0.8.0 tracking-lock log showed 29 reclaims, up to 76 m "moved since yield",
// each one a location/head/torso/hand jump). Only a cutscene / menu / stale
// sample can end it, and only after StayLatch's grace (brief !fresh flicker
// keeps the yield and the head follow). Physics ownership is unchanged.
#pragma once
#include "spidy/head_probe.hpp"
#include "spidy/math.hpp"

namespace spidy::animated_moves {

// Which pose-yield gates are on (each setting off by default).
struct Gate {
    bool moves{}; // ANIMATED MOVES: ground + vault/mantle parkour
    bool air{};   // ANIMATED AIR: freefall / swing / web-hold air
    bool only{};  // ANIMATED ONLY: every gameplay state yields (no OG body reclaim)
};

// ANIMATED ONLY is live: the setting plus at least one of MOVES / AIR.
inline bool stays(Gate g) { return g.only && (g.moves || g.air); }

// True when a freefall web/aim button press should reclaim the VR body so the
// player can aim out of an air anim. Attached webs do not count — those keep
// the swing pose while the mod still owns physics. When ANIMATED ARMS is on,
// the overlay aims with the controllers — do not tear down pose yield (that
// caused enter/exit chatter and stale adoptFeet while still midair).
inline bool freefallWebInterrupt(bool webAimIntent, const head_probe::StateInputs& s,
                                 bool armsHandlesAim = false) {
    if (armsHandlesAim)
        return false;
    return webAimIntent && !s.webbed;
}

// True when the current state should let the game's animation drive the body.
// Wall-run under NATIVE RUN (parkour with wall != 0), zip, combat and cutscenes
// stay with the mod. Physics ownership is independent (swing owned).
// `armsHandlesAim`: ANIMATED ARMS on — freefall web intent keeps the yield.
inline bool yieldsBody(Gate g, const head_probe::StateInputs& s, bool webAimIntent = false,
                       bool armsHandlesAim = false) {
    if (!g.moves && !g.air)
        return false;
    const auto st = head_probe::classify(s);
    // ANIMATED ONLY: wall / zip / parkour / combat stay with the game anim too,
    // and web intent never tears the yield down (arms overlay or not).
    if (stays(g))
        return st != head_probe::State::cutscene;
    if (freefallWebInterrupt(webAimIntent, s, armsHandlesAim))
        return false;
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
// softlock is effective-only (not a settings row): midair torso-forward view
// frame (frameLevel / anchored 4) with free HMD — deliberate named policy, not
// a silent Full→Level rewrite.
enum class HeadFollow : int { off = 0, level = 1, full = 2, softlock = 3 };

inline HeadFollow headFollow(int setting) {
    if (setting == 2)
        return HeadFollow::full;
    if (setting == 1)
        return HeadFollow::level;
    return HeadFollow::off;
}

// Midair (same airborne set as AIR SPIN): softlock when HEAD ≠ Off. Ground /
// parkour / perch / wall keep the user's Full or Level headbone choice.
inline bool midairSoftlock(const head_probe::StateInputs& s) {
    if (s.grounded || s.wall != 0)
        return false;
    switch (head_probe::classify(s)) {
    case head_probe::State::air:
    case head_probe::State::swing:
    case head_probe::State::web:
    case head_probe::State::zip:
    case head_probe::State::parkour:
    case head_probe::State::combat:
        return true;
    default:
        return false;
    }
}

// Effective head-follow while yielding. Midair → softlock (torso-forward /
// perch-like level frame, free HMD). Ground keeps Full/Level as set.
inline HeadFollow headFollowForYield(int headSetting, const head_probe::StateInputs& s) {
    const HeadFollow choice = headFollow(headSetting);
    if (choice == HeadFollow::off)
        return HeadFollow::off;
    if (midairSoftlock(s))
        return HeadFollow::softlock;
    return choice;
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
    case HeadFollow::softlock:
        return "softlock";
    default:
        return "off";
    }
}

struct Handoff {
    bool yielding{};
    bool enter{};
    bool exit{};
    // Pending exit resync: wait out brief false exits (web interrupt / classify
    // flicker) before adoptFeet. Cleared if yield re-enters inside the window.
    bool pendingExit{};
    int64_t pendingExitNs{};
    Vec3 pendingExitFeet{};

    void update(bool now) {
        enter = now && !yielding;
        exit = !now && yielding;
        yielding = now;
    }

    // Call after update. Returns true once when a stable exit should resync.
    // debounceNs: ignore exits that re-enter sooner (200 ms default).
    bool consumeStableExit(int64_t nowNs, Vec3 feetNow, int64_t debounceNs = 200'000'000) {
        // Latch once on the exit edge; do not refresh the clock on later polls
        // while `exit` is still true (same frame / tests calling twice).
        if (exit && !pendingExit) {
            pendingExit = true;
            pendingExitNs = nowNs;
            pendingExitFeet = feetNow;
        }
        if (enter && pendingExit) {
            pendingExit = false; // brief interrupt — keep tracking origin
            return false;
        }
        if (!pendingExit)
            return false;
        if (nowNs - pendingExitNs < debounceNs)
            return false;
        pendingExit = false;
        return true;
    }
};

inline bool needsActorResync(const Handoff& h) { return h.exit || h.pendingExit; }

// ANIMATED ONLY grace: keep an active yield through a cutscene/stale classify
// for up to `graceNs` (the 0.8.0 log: "head follow off (cutscene)" then back
// 150 ms later). Longer losses (a real cutscene, the pause menu) end the yield
// normally so the mod resyncs once. Not used when ANIMATED ONLY is off.
constexpr int64_t stayGraceNs = 1'500'000'000;
struct StayLatch {
    int64_t lostNs{-1};
    bool held{}; // last update kept a yield the raw gate dropped
    // raw: yieldsBody this frame. wasYielding: Handoff::yielding before update.
    bool update(bool raw, bool wasYielding, int64_t nowNs, int64_t graceNs = stayGraceNs) {
        held = false;
        if (raw) {
            lostNs = -1;
            return true;
        }
        if (!wasYielding) {
            lostNs = -1;
            return false;
        }
        if (lostNs < 0)
            lostNs = nowNs;
        held = nowNs - lostNs < graceNs;
        if (!held)
            lostNs = -1;
        return held;
    }
};

} // namespace spidy::animated_moves
