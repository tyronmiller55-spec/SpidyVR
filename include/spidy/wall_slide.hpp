#pragma once
// GRIP TO STICK (0.8.7-na-beta3e, wall_grip_to_stick / wall_slide_friction; the owner, Nov 22 t94u: "I want
// to remove the auto wall stick so that I can slide on walls; in order to stick and crawl I need to grip the
// wall, then I can crawl as normal").
//
// SpidyVR's own walls (Swing's wall_, Surface::wall) were a hold the moment the body met one in the air
// (Swing::senseWall -> joinWall): no gravity along the wall, the stick crawling it at once. With this on,
// such a wall is a surface the body slides on: gravity's part along the wall pulls it down, friction (a share
// of gravity) slows the slide, and the speed it came in with carries on. A grip closed with a palm at the
// wall (hand climbing's hold, or with hand climbing off the same palm test) sticks the body there; from then
// on the wall is the crawl it always was, until the body leaves it. A mount (the walk from the game's own
// crawl, or the walked mount) sticks at once, and a wall the body was stuck to carries over a gap or a
// corner.
//
// The game's own wall crawl and wall run (Surface::game, the animation owning the body) are the game's: no
// validated hook stops the game attaching there (docs/REFERENCE.md has none), so this changes SpidyVR's walls
// only.
//
// Engine independent; no game address.
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::wall_slide {
// wall_slide_friction's default (%): half of gravity. At 100% a body at rest on a wall stays.
inline constexpr int defaultFrictionPct = 50;
// A body that was stuck and left its wall (not by a jump) sticks to the next one met within this long.
inline constexpr float carrySeconds = .6f;

// One step of a body sliding along a wall whose outward unit normal is `n`: `along` is its velocity in the
// wall's plane. Gravity's part in the plane is added, then friction (`friction` of `gravity`, 0 to 1) takes
// speed off against the slide, never past a stop.
inline Vec3 step(Vec3 along, Vec3 n, float gravity, float friction, float dt) {
    if (!finite(along) || !finite(n) || !(dt > 0) || !std::isfinite(gravity))
        return finite(along) ? along : Vec3{};
    const Vec3 g{0, -gravity, 0};
    along += (g - n * dot(g, n)) * dt;
    along -= n * dot(along, n);
    const float speed = length(along);
    const float brake = std::clamp(std::isfinite(friction) ? friction : 0.f, 0.f, 1.f) * gravity * dt;
    if (!(speed > brake))
        return {};
    return along * ((speed - brake) / speed);
}

// Whether a wall met now sticks for a body that left a stuck wall `sinceLeft` seconds ago, its last wall jump
// (or point jump, or web pull off) `sinceJump` ago: only a wall lost by itself (a gap, an edge) carries.
inline bool carries(bool wasStuck, float sinceLeft, float sinceJump) {
    return wasStuck && sinceLeft < carrySeconds && sinceJump > sinceLeft;
}
} // namespace spidy::wall_slide
