// ANIMATED MOVES (vr_settings::Values::animatedMoves, off by default): step two
// of a head lock. When on, the game's own animation drives the hero's body in
// ground parkour, vaults and mantles unlocked by NATIVE RUN (RT). Swing, web,
// zip and wall-run stay with the mod's physics this step (hybrid later). The
// pose writer clears bodyOn so the body blends back to the game's joints
// (native_body::Flags::bodyOn); NATIVE RUN's RT path is untouched.
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

} // namespace spidy::animated_moves
