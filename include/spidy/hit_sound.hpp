#pragma once
// LOUDER HITS (beta polish A1/A3, the owner's D5): how hard the game's own melee impact sounds a blow of ours (the
// Enemy_Impact_Intensity switch, game_sound.hpp: 1 Light, 2 Medium, 3 Heavy, 4 Extreme). Before the beta every plain
// jab was Light. Now a jab is Medium, a full swing (strength 0.6 or more) Heavy, a kick at least Heavy and a kick that
// sends him flying Extreme, a finisher Extreme. The hit sound itself stays the game's default one (#0) until the
// capture session finds the ones its own punches and kicks use. Pure.
#include <cstdint>

namespace spidy::hit_sound {

constexpr float heavyStrength = .6f;

struct Blow {
    bool finisher{};  // BlowKind::Finisher
    bool special{};   // not a plain hit (a launcher, a slam...) or a called blow
    bool kick{};      // a foot's blow (air, dive or ground kick: WebStrike::diveHand)
    bool flies{};     // the kick sends him flying
    float strength{}; // 0..1
};

inline uint8_t intensity(const Blow& b) {
    if (b.finisher || (b.kick && b.flies))
        return 4;
    if (b.kick || b.special || b.strength >= heavyStrength)
        return 3;
    return 2;
}

// HERO ATTACK SOUND (beta polish A2): at most one a hand this often (a punch's hand, or the legs for a kick).
constexpr int64_t attackGapMs = 250;
inline bool attackDue(int64_t lastQpc, int64_t nowQpc, int64_t frequency) {
    return frequency > 0 && (lastQpc <= 0 || (nowQpc - lastQpc) * 1000 >= attackGapMs * frequency);
}

} // namespace spidy::hit_sound
