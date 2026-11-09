// LAUNCH FACING (the user, November 8 21:51: "point launches orient Spider-Man
// in inconsistent directions; he should always face his momentum"). A jump
// the player asked for (A: a point launch, a leap) that leaves the ground with
// real speed across it turns the hero toward that speed for the first moments
// of the flight, through the game's own walk-facing turn (the stick the game
// reads, as MODEL YAW and the landing seed already do: animated_model_yaw::
// worldToStick). No facing write, no new physics. Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include <cmath>
#include <cstdint>

namespace spidy::launch_facing {

constexpr int64_t askNs = 500'000'000;    // the ask counts when he leaves within this
constexpr int64_t windowNs = 600'000'000; // facing applies this long after leaving
constexpr float minSpeed = 4.f;           // m/s across the ground; slower has no "momentum"

struct Tracker {
    int64_t askAtNs{-1}, leftAtNs{-1};
    bool groundedWas{}, groundedWasSet{};
    // One update a frame. True while the facing applies, with `way` the unit
    // horizontal direction of `velocity`.
    bool update(int64_t nowNs, bool jumpAsk, bool grounded, Vec3 velocity, Vec3& way) {
        if (jumpAsk)
            askAtNs = nowNs;
        if (groundedWasSet && groundedWas && !grounded && askAtNs >= 0 && nowNs - askAtNs <= askNs) {
            leftAtNs = nowNs;
            askAtNs = -1;
        }
        if (grounded && !jumpAsk)
            leftAtNs = -1;
        groundedWas = grounded;
        groundedWasSet = true;
        if (leftAtNs < 0 || nowNs - leftAtNs > windowNs)
            return false;
        const Vec3 across{velocity.x, 0, velocity.z};
        if (!finite(across) || length(across) < minSpeed)
            return false;
        way = normalized(across);
        return true;
    }
    void reset() { *this = {}; }
};

} // namespace spidy::launch_facing
