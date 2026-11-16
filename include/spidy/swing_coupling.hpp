#pragma once
#include <algorithm>
#include <cmath>

namespace spidy::swing_coupling {
// Slice P: feed the visual tuck into the swing's existing spin drag and air
// steering. Gains are conservative; set either to 0 to disable that half.
// An untucked pose (tuckWeight 0) is identity, so today's swing is unchanged.
constexpr float tuckInertia = 0.22f; // full tuck: I *= 0.78 (spin speeds up)
constexpr float tuckAngDrag = 0.25f; // full tuck: angular drag *= 0.75

struct Scales {
    float inertia = 1; // multiply rotational inertia (lower = faster spin)
    float angDrag = 1; // multiply angular drag / divide falloff time
    float steer = 1;   // multiply air / free-hand steering (1 / inertia)
};

inline Scales scales(float tuckWeight) {
    const float t = std::clamp(tuckWeight, 0.f, 1.f);
    Scales s;
    s.inertia = std::clamp(1.f - tuckInertia * t, .55f, 1.f);
    s.angDrag = std::clamp(1.f - tuckAngDrag * t, .55f, 1.f);
    s.steer = 1.f / s.inertia;
    return s;
}
} // namespace spidy::swing_coupling
