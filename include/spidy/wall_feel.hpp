#pragma once
// WALL FEEL (0.8.7-na-beta3n; wall_run_push / wall_run_blend_ms, wall_fling_any_way, sticky_palms /
// sticky_palms_pct; each on by default, off: beta3m's wall). The owner, Nov 26 t141u: "when adding wall grip
// and slide, wall running broke. Add that back ... it's a slide anytime you're near the wall, and pushing the
// stick towards the wall smoothly initiates the wallrun in that direction. Gripping the wall, I want to be
// able to fling myself in any direction via grip fling, and I want a softer "sticky" palm whenever my palms
// touch a surface. This active-when-in-contact soft friction can be tied to the momentum, and can use it to
// slow slides, or gain momentum off of wallruns or jumps."
//
// Why the wall run broke (beta3e, 6657b76 "Grip to stick"): Swing::wallMotion returned with the slide step
// for every wall the body was not stuck to, before the stick's wall run. Met in the air, a wall slides until a
// grip sticks it, so the stick never ran it: gravity and friction took the run's speed instead.
//
// SpidyVR's own walls only (Swing's wall_): the game's own wall crawl and wall run, and the ground, are the
// game's (no validated hook in docs/REFERENCE.md moves the body there).
//
// Engine independent; no game address.
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::wall_feel {
// wall_run_blend_ms's default: the slide becomes the run (and back) over a quarter second.
inline constexpr int defaultBlendMs = 250;
// How much of the stick must point at the wall (0..1) to start the run: a stick along the wall only does not.
inline constexpr float pushInto = .25f;
// The run's blend after `dt`: toward 1 while the stick pushes at the wall (`into`, 0..1, of the stick toward
// it), toward 0 otherwise, over `seconds` (0: at once).
inline float blend(float now, float into, float seconds, float dt) {
    if (!std::isfinite(now))
        now = 0;
    const float target = std::isfinite(into) && into > pushInto ? 1.f : 0.f;
    if (!(seconds > 0) || !std::isfinite(seconds))
        return target;
    const float step = std::isfinite(dt) && dt > 0 ? dt / seconds : 0.f;
    return now < target ? std::min(target, now + step) : std::max(target, now - step);
}

// The slide and the run mixed: `t` of the way from one to the other.
inline Vec3 mix(Vec3 slide, Vec3 run, float t) {
    t = std::isfinite(t) ? std::clamp(t, 0.f, 1.f) : 0.f;
    return slide + (run - slide) * t;
}

} // namespace spidy::wall_feel
