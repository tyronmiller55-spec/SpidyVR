#pragma once
// HERO CUE GUARD (beta polish A4, the owner's D6: the game's jump sounds on every point jump, never twice): the
// game's own jump set (its grunt dab33491 first) is already sounding when its grunt played on the hero within
// guardMs; a guarded cue of ours is then skipped. The sound hook (game_sound.cpp, 16d5e80) notes the grunt whether
// or not SOUND TRACE logs (D7). Pure.
#include <cstdint>

namespace spidy::sound_guard {

// docs/VALIDATION.md "Game jump sound set": the sound trace's jump signature over 434 game jumps.
constexpr uint32_t jumpGrunt = 0xdab33491; // the jump grunt (vocal), 91% of jumps at -9 ms; confirmed Nov 12
// The rest of the set: 84 / 73 / 58% of jumps at about -30 ms, jump-only (0-2% of wall jumps, 2-5% of landings).
// What each is (launch, whoosh, cloth) is not known.
constexpr uint32_t jumpSet[3] = {0xcba7688f, 0x2903bc21, 0xeedb2021};
// Already on the game's point jump (sound trace Nov 19, 214 point jumps): 23% / 20% within 80 ms, at +39 ms, and
// nearly absent from game and wall jumps. A point jump plays the grunt alone, so the rest of the set does not double
// them (docs/VALIDATION.md "Game jump sound set").
constexpr uint32_t pointJumpAlready[2] = {0xa442c255, 0x050553e7};
// WALL JUMP SOUND's candidates (sound trace Nov 19, 197 wall jumps): d321a6f9 14% within 80 ms at -7 ms (nearest to
// the jump 23 times), 110d202b 12% at 0 ms (19 times). Not 700c7633: 51% of landings, a landing sound. Not exclusive:
// the headset A/B decides. Index 1 and 2 of the setting (0: off).
constexpr uint32_t wallJumpCandidates[2] = {0xd321a6f9, 0x110d202b};
inline uint32_t wallJumpId(int choice) {
    return choice == 1 ? wallJumpCandidates[0] : choice == 2 ? wallJumpCandidates[1] : 0;
}
constexpr int64_t guardMs = 300;

// Whether a guarded cue should be skipped: the game's grunt played on the hero at `gruntQpc` (0 never), now is
// `nowQpc`, `frequency` ticks a second.
inline bool nativeSounding(int64_t gruntQpc, int64_t nowQpc, int64_t frequency) {
    if (gruntQpc <= 0 || frequency <= 0 || nowQpc < gruntQpc)
        return false;
    return (nowQpc - gruntQpc) * 1000 < guardMs * frequency;
}
// SpidySoundCue's argument: the id in the low 32 bits, bit 32 asks for the guard.
constexpr uint64_t guardBit = uint64_t{1} << 32;
inline uint64_t encode(uint32_t id, bool guarded) {
    return id | (guarded ? guardBit : 0);
}

} // namespace spidy::sound_guard
