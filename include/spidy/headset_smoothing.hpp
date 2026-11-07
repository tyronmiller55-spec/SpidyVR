#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::headset_smoothing {
// Native Animation SMOOTHING: ease the view toward the headset's look.
// Translation is left alone so leaning and travel do not trail the body.
// Level 0 (Off) is identity. Set any level to 0 to disable.
constexpr float responseSec[] = {0.f, .05f, .10f, .18f, .30f};
constexpr int maxLevel = 4;
// A snap or recenter larger than this skips the ease, so snap-turn stays snap.
constexpr float snapRad = 45.f * 3.14159265f / 180.f;

struct Look {
    Quat filtered{};
    Quat delta{}; // filtered * raw.conjugate(); identity when Off or on a snap
};

inline void rotateBasis(Mat4& m, Quat delta) {
    if (std::abs(delta.x) + std::abs(delta.y) + std::abs(delta.z) < 1e-7f)
        return;
    for (int row = 0; row < 3; ++row) {
        const int at = row * 4;
        const Vec3 v = delta.rotate({m[at], m[at + 1], m[at + 2]});
        m[at] = v.x;
        m[at + 1] = v.y;
        m[at + 2] = v.z;
    }
}

class Filter {
    Quat previous_{};
    bool ready_{};

  public:
    void reset() {
        ready_ = false;
    }
    Look update(Quat target, float seconds, int level) {
        level = std::clamp(level, 0, maxLevel);
        Look out;
        out.filtered = target;
        if (level == 0 || !std::isfinite(seconds) || seconds < 0 || seconds > .25f) {
            previous_ = target;
            ready_ = level > 0;
            return out;
        }
        if (!ready_) {
            previous_ = target;
            ready_ = true;
            return out;
        }
        Quat delta = target * previous_.conjugate();
        if (delta.w < 0)
            delta = {-delta.x, -delta.y, -delta.z, -delta.w};
        const Vec3 axis{delta.x, delta.y, delta.z};
        const float angle = 2 * std::atan2(length(axis), std::max(0.f, delta.w));
        if (angle > snapRad) {
            previous_ = target;
            return out;
        }
        const float response = responseSec[level];
        const Quat step = Quat::around(axis, angle * (1 - std::exp(-seconds / response)));
        previous_ = step * previous_;
        const float n = std::sqrt(previous_.x * previous_.x + previous_.y * previous_.y +
                                  previous_.z * previous_.z + previous_.w * previous_.w);
        if (n > 1e-6f) {
            previous_.x /= n;
            previous_.y /= n;
            previous_.z /= n;
            previous_.w /= n;
        }
        out.filtered = previous_;
        out.delta = previous_ * target.conjugate();
        return out;
    }
};
} // namespace spidy::headset_smoothing
