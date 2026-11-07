// ANIMATED HEAD under HYBRID: keeps the head-locked view seated on the hero's
// neck. Pure - no Windows, no game addresses.
//
// While the animation owns the body the eyes are the headset's movement since
// a reference (latched when the lock began) put on the hero's head
// (animated_view::relative, then native_eyes::onHead). Turning the head is the
// player's to keep. But every step or lean in the room since that reference
// also moves the eyes off the head - and the lock can last through minutes of
// parkour, zips, crawls and fights without a new reference, so that offset
// grows until the view floats away from the neck (the player, November 6: "the
// head tracking consistently gets disconnected from the neck as I parkour and
// switch around").
//
// The check: on every switch (an owner edge, a change of what the animation is
// doing, a web zip, a HEAD mode change, a headset recenter, and as a backstop
// when the offset is larger than any lean) the reference moves to where the
// headset is now, so the eyes go back onto the head. It glides there over the
// same 180 ms smoothstep the owner handoff uses (hybrid_handoff), not a cut.
// Only the reference's place moves: its heading - where the player looks
// relative to the hero - stays, and so the hands' frame (the same reference)
// moves with the eyes and never apart from them.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::head_seat {

// The glide back onto the head: hybrid_handoff::handoffNs.
constexpr int64_t glideNs = 180'000'000;
// Smaller than this a switch leaves the view alone (no glide for nothing).
constexpr float minDrift = .02f;
// Farther than a standing lean from the head: reseated without waiting for a
// switch. 20 cm still leaves room to lean; 35 cm left a visibly disconnected head.
constexpr float maxDrift = .20f;
// While a glide is already heading there, chase a small target move without
// restarting the 180 ms clock (repeated action edges would hitch).
constexpr float followMeters = .08f;

enum class Why : uint8_t { none, owner, state, zip, mode, recenter, drift, action };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::owner:
        return "owner switch";
    case Why::state:
        return "move change";
    case Why::zip:
        return "web zip";
    case Why::mode:
        return "head mode";
    case Why::recenter:
        return "recenter";
    case Why::drift:
        return "drift";
    case Why::action:
        return "action";
    default:
        return "none";
    }
}

inline float alpha(int64_t elapsedNs) {
    if (elapsedNs <= 0)
        return 0.f;
    if (elapsedNs >= glideNs)
        return 1.f;
    const float t = static_cast<float>(elapsedNs) / static_cast<float>(glideNs);
    return t * t * (3.f - 2.f * t);
}

struct Glide {
    bool active{};
    Vec3 from{}, to{};
    int64_t startNs{};
};

// The reference's place now (a glide under way included) - what the eyes and the
// hands are relative to this frame. Ends the glide once it is there.
inline void step(Glide& g, Pose& reference, int64_t nowNs) {
    if (!g.active)
        return;
    const float a = alpha(nowNs - g.startNs);
    reference.position = g.from + (g.to - g.from) * a;
    if (a >= 1.f)
        g.active = false;
}

// The headset's offset from where the lock will have it once any glide is done.
inline Vec3 drift(const Glide& g, const Pose& reference, Vec3 head) {
    return head - (g.active ? g.to : reference.position);
}

// A switch: put the reference where the headset is now, gliding. False (nothing
// started) for a bad head or an offset under minDrift; a glide under way then
// keeps going to where it was going.
inline bool reseat(Glide& g, Pose& reference, Vec3 head, int64_t nowNs, float* moved = nullptr) {
    if (!finite(head) || !finite(reference.position))
        return false;
    const float d = length(drift(g, reference, head));
    if (moved)
        *moved = d;
    if (!std::isfinite(d) || d < minDrift)
        return false;
    if (g.active) {
        const float toOff = length(g.to - head);
        if (toOff < minDrift)
            return false;
        if (toOff < followMeters) {
            g.to = head;
            return false;
        }
    }
    step(g, reference, nowNs); // from exactly where the reference is this frame
    g.active = true;
    g.from = reference.position;
    g.to = head;
    g.startNs = nowNs;
    return true;
}

// The backstop: farther from the head than a standing lean, reseat without a switch.
inline bool overDrift(const Glide& g, const Pose& reference, Vec3 head) {
    const float d = length(drift(g, reference, head));
    return std::isfinite(d) && d > maxDrift;
}

} // namespace spidy::head_seat
