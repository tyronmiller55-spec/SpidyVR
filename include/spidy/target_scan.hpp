#pragma once
#include <algorithm>
#include <cstdint>

// TARGET SCAN OUTSIDE FIGHTS (the owner, Nov 16; vr_settings::Values::targetScanIdleMs, Settings v42): a target
// Watch (game_targets.hpp) rescans every fightScanMs while the player fights and for fightHoldMs after the last sign
// of it (Command asks bit 6, game_targets::sawFight), and every `idleMs` (250 to 2000, 500 by default) otherwise.
// Before: every 250 ms always. Portable.
namespace spidy::game_targets {
inline constexpr uint32_t fightScanMs = 250, fightHoldMs = 3000, minScanIdleMs = 250, maxScanIdleMs = 2000;
inline uint32_t scanIntervalMs(bool fighting, uint32_t idleMs) {
    return fighting ? fightScanMs : std::clamp(idleMs, minScanIdleMs, maxScanIdleMs);
}
} // namespace spidy::game_targets
