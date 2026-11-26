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
// WALL FLING ANY WAY: a grip fling off a wall leaves it along the whole fling with this much of it out of the
// wall (m/s); off, beta3m's 2 m/s (less only strokes along the wall).
inline constexpr float flingLeaveSpeed = .3f, flingLeaveSpeedOff = 2.f;
// The speed out of the wall a fling (`out`, m/s, + away from it) leaves the wall with.
inline bool flingLeaves(float out, bool anyWay) {
    return std::isfinite(out) && out >= (anyWay ? flingLeaveSpeed : flingLeaveSpeedOff);
}

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

// STICKY PALMS: sticky_palms_pct's default (0 to 200).
inline constexpr int defaultStickyPct = 100;
// At 100% each open palm on the wall acts at this rate (1/s; two palms add). Held still, it drags: the body's
// speed along the wall falls by this rate (a 6 m/s slide under 1 m/s in about half a second; never dead still,
// never a grip's hold). Swept, it strokes: the body gains `strokeGain` (the grip fling's 1.3) times the sweep the
// other way, at this rate, and the drag fades out as the sweep reaches `strokeFull` - a palm that moves on
// purpose is a stroke, not a brake.
inline constexpr float stickyRate = 4.f, strokeGain = 1.3f, strokeFull = 2.f;
// A palm's sweep counts up to the grip fling's most (m/s); faster is a tracking jump, not a stroke.
inline constexpr float sweepMost = 9.f, sweepGlitch = 15.f;
// How quickly the measured sweep follows the hand (s): it smooths tracking jitter, not a stroke.
inline constexpr float sweepSmoothing = .04f;
// An open palm: a grip squeezed past this is a grip (GRIP TO STICK, the grip fling), not a palm.
inline constexpr float openPalm = .35f;

// The body's velocity in the wall's plane (`along`, outward unit normal `n`) after `dt` with `count` open palms
// on the wall sweeping at `sweep` (their mean, m/s: the hands' motion against the head, turned into the world).
// A still palm slows the slide; one swept back along a run pushes it on; one swept down lifts; one swept the way
// the body goes brakes it. `strength`: sticky_palms_pct / 100. Never faster than `most`.
inline Vec3 palms(Vec3 along, Vec3 n, Vec3 sweep, unsigned count, float strength, float dt, float most) {
    if (!finite(along) || !finite(n) || !count || !(strength > 0) || !std::isfinite(strength) || !(dt > 0) ||
        !std::isfinite(dt))
        return along;
    if (!finite(sweep))
        sweep = {};
    Vec3 stroke = -sweep;
    stroke -= n * dot(stroke, n);
    stroke = limited(stroke, sweepMost);
    const float drag = std::clamp(1 - length(stroke) / strokeFull, 0.f, 1.f);
    const float rate = stickyRate * strength * static_cast<float>(std::min(count, 2u));
    Vec3 v = along + (stroke * strokeGain - along * drag) * (1 - std::exp(-rate * dt));
    v -= n * dot(v, n);
    return limited(v, std::max(0.f, std::isfinite(most) ? most : 0.f));
}

// A palm's measured sweep after a new reading `raw` (m/s) `dt` after the last.
inline Vec3 smoothSweep(Vec3 now, Vec3 raw, float dt) {
    if (!finite(raw) || length(raw) > sweepGlitch || !(dt > 0) || !std::isfinite(dt))
        return finite(now) ? now : Vec3{};
    if (!finite(now))
        now = {};
    return now + (raw - now) * (1 - std::exp(-dt / sweepSmoothing));
}
} // namespace spidy::wall_feel
