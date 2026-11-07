#pragma once
#include "math.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace spidy::headset_smoothing {
// Native Animation SMOOTHING: calm wobble on the animated head the view sits on.
// The headset's look and lean are left alone (no look delta). The head's travel
// (position) is copied through so the camera cannot trail through the body or a
// wall. Level 0 (Off) is identity.
//
// Idle jitter uses the response times below as a low cutoff. Faster motion
// raises the cutoff (one-euro style) so a vault, jump or real turn still comes
// through. A snap, recenter or cut jumps to the target.
constexpr float responseSec[] = {0.f, .05f, .10f, .18f, .30f};
constexpr float beta[] = {0.f, .55f, .50f, .45f, .40f};
constexpr int maxLevel = 4;
constexpr float snapRad = 45.f * 3.14159265f / 180.f;
// A teleport or a new shot this far away skips the ease, as spectator smoothing does.
constexpr float cutMeters = 3.f;
// The XR worker sets this each frame: the user's SMOOTHING level while Native
// Animation head-follow owns the view, otherwise 0. The render thread reads it
// in resolveScene so the eyes sit on the filtered parent.
inline std::atomic<int> liveLevel{0};

class Filter {
    Mat4 previous_{};
    bool ready_{};

    static float turn(const Mat4& from, const Mat4& to, Vec3& axis) {
        const Vec3 oldX{from[0], from[1], from[2]}, oldZ{from[8], from[9], from[10]};
        const Vec3 nextX{to[0], to[1], to[2]}, nextZ{to[8], to[9], to[10]};
        Quat delta = arc(oldZ, nextZ);
        const Vec3 alignedX = delta.rotate(oldX);
        const float twist = std::atan2(dot(nextZ, cross(alignedX, nextX)), dot(alignedX, nextX));
        delta = Quat::around(nextZ, twist) * delta;
        if (delta.w < 0)
            delta = {-delta.x, -delta.y, -delta.z, -delta.w};
        axis = {delta.x, delta.y, delta.z};
        return 2 * std::atan2(length(axis), std::max(0.f, delta.w));
    }
    static Mat4 withPosition(Mat4 m, const Mat4& pos) {
        m[12] = pos[12];
        m[13] = pos[13];
        m[14] = pos[14];
        m[15] = pos[15];
        return m;
    }

  public:
    void reset() {
        ready_ = false;
    }
    Mat4 update(Mat4 target, float seconds, int level) {
        level = std::clamp(level, 0, maxLevel);
        if (level == 0 || !std::isfinite(seconds) || seconds < 0 || seconds > .25f) {
            previous_ = target;
            ready_ = level > 0;
            return target;
        }
        if (!ready_) {
            previous_ = target;
            ready_ = true;
            return target;
        }
        if (seconds == 0)
            return previous_ = withPosition(previous_, target);
        const Vec3 jump{target[12] - previous_[12], target[13] - previous_[13],
                        target[14] - previous_[14]};
        if (length(jump) > cutMeters) {
            previous_ = target;
            return target;
        }
        Vec3 axis{};
        const float angle = turn(previous_, target, axis);
        if (angle > snapRad) {
            previous_ = target;
            return target;
        }
        const float speed = angle / seconds;
        const float minCutoff = 1.f / (6.2831853f * responseSec[level]);
        const float cutoff = minCutoff + beta[level] * speed;
        const float alpha = 1.f - std::exp(-6.2831853f * cutoff * seconds);
        const Quat step = Quat::around(axis, angle * std::clamp(alpha, 0.f, 1.f));
        for (int row = 0; row < 3; ++row) {
            const int at = row * 4;
            const Vec3 v = step.rotate({previous_[at], previous_[at + 1], previous_[at + 2]});
            previous_[at] = v.x;
            previous_[at + 1] = v.y;
            previous_[at + 2] = v.z;
        }
        previous_ = withPosition(previous_, target);
        return previous_;
    }
};
} // namespace spidy::headset_smoothing
