// ANIMATED MOVES (vr_settings::Values::animatedMoves, off by default): step two
// of a head lock. When on, the game's own animation drives the hero's body in
// ground parkour, vaults and mantles unlocked by NATIVE RUN (RT). Swing, web,
// zip and wall-run stay with the mod's physics this step (hybrid later). The
// pose writer clears bodyOn so the body blends back to the game's joints
// (native_body::Flags::bodyOn); NATIVE RUN's RT path is untouched.
//
// ANIMATED HEAD (vr_settings::Values::animatedHead, Off|Level|Full, default Off):
// while yieldsBody, attach the VR view to the hero's gameplay head the way
// finishers do (anchored 2/4 → resolvedOnHead), without turning on the cutscene
// worker (game_fp::want). Only active when the body is yielding.
//
// Reuses head_probe::classify / StateInputs so the gated states match HEAD
// PROBE's (and the headset logs that said ground anim is live).
#pragma once
#include "spidy/head_probe.hpp"

namespace spidy::animated_moves {

// True when the setting is on and the current state should let the game's
// animation drive the body. Wall-run under NATIVE RUN (parkour with wall != 0)
// stays with the mod; vaults and mantles are parkour with no wall.
inline bool yieldsBody(bool settingOn, const head_probe::StateInputs& s) {
    if (!settingOn)
        return false;
    const auto st = head_probe::classify(s);
    if (st == head_probe::State::ground)
        return true;
    if (st == head_probe::State::parkour && s.wall == 0)
        return true;
    return false;
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

// View follows the hero's gameplay head while the body is yielding.
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

} // namespace spidy::animated_moves
