#pragma once
// LEVEL KEEPS HIM WITH YOU (0.8.7-na-beta3p, settings level_head_lock, revision 27; the owner, Nov 26 t151u:
// "Level ... felt great. The only problem was ... the body and the head had no cohesion, so you'd frequently be
// looking backwards after moves in animation. If we could lock it better, while still keeping it relatively
// level"; plan level-head-cohesion-1126.md Option A).
//
// HEAD: LEVEL used to freeze its heading from the head bone at the start of a yield and at each cut, and
// nothing turned the hero back under it. With this on (HYBRID, HEADSET FIRST), LEVEL joins the head lock FULL
// uses: the view's yaw is the published lock's (the right stick, a snap realign, a recenter) plus the headset -
// animation never turns it - and the game's own landing turn, facing check, turn-in-place and the stick basis
// bring the hero back onto the view after a dodge, roll or landing. The frame stays level: no pitch or roll
// from the bone. Where the lock is not live, the level latch stays, without its two frozen-anchor faults: no
// re-latch on the one-metre jump test (a fast dive or swing moves that far in a game step), and no latch onto
// the crown's yaw while the head looks steeply up or down (mid-flip, the plan's "crown behind" latch).
// Off: LEVEL as beta3o.
#include <cmath>
#include <cstdint>

namespace spidy::level_head_lock {

// LEVEL joins the head lock: the switch, HEAD LEVEL as the frame's mode, HYBRID movement and HEADSET FIRST
// (the lock's sway is then 0, so the bone never turns the view).
inline bool joins(bool option, bool levelMode, bool hybrid, bool headsetFirst) {
    return option && levelMode && hybrid && headsetFirst;
}

// The level frame for a heading (rig yaw, 0 facing -z): rows right, down, forward (game_fp's layout), no
// position.
inline void axes(float yaw, float out[16]) {
    const float ax = -std::sin(yaw), az = -std::cos(yaw);
    const float level[16] = {-az, 0, ax, 0, 0, -1, 0, 0, ax, 0, az, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; ++i)
        out[i] = level[i];
}

// The heading a level latch takes from a head pose (rows right, down, forward, position). `fixed` (the
// switch on): a head looking steeply up or down (its forward within about 17 degrees of vertical) gives none -
// the latch waits for a head that faces somewhere - instead of the crown's yaw, which mid-flip can face away
// from where he goes. Off: the crown's (as beta3o).
inline bool latchYaw(const float* head, bool fixed, float& yaw) {
    float ax = head[8], az = head[10];
    if (ax * ax + az * az < .09f) {
        if (fixed)
            return false;
        const float s = head[9] < 0 ? -1.f : 1.f;
        ax = s * head[4];
        az = s * head[6];
    }
    if (!(std::sqrt(ax * ax + az * az) > 1e-3f) || !std::isfinite(ax) || !std::isfinite(az))
        return false;
    yaw = std::atan2(-ax, -az);
    return true;
}

// Which cuts re-latch: all of them (off), or with the switch on all but the ones the one-metre jump test made
// alone (`moveCuts`, a fast dive or swing): another skeleton, a yield's start and a softlock edge still do.
inline uint32_t cutKey(uint32_t cuts, uint32_t moveCuts, bool fixed) {
    return fixed ? cuts - moveCuts : cuts;
}

} // namespace spidy::level_head_lock
