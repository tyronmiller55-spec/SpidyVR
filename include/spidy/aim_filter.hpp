// WRIST MARKER SMOOTHING (the user, November 8 21:58): the hand's aim ray goes
// through a speed-adaptive low-pass (the "1 euro filter") BEFORE the swing
// works out the aim previews (the markers) and the shots from it, so the
// marker and where the web or ball goes stay the same line. Steady while the
// hand holds still; on a fast flick the cutoff rises with the aim's speed, so
// it follows within a frame or two. In the tracking space, so a snap turn or
// a flip's tilt is no motion. Both owners (animation and OG SpidyVR) send the
// same Command hands, so both get it. Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include "spidy/spherical_aim.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::aim_filter {

// Gentler than the render-only marker filter (AimMarkerMotion: 1 Hz floor):
// this one also steers the shot, so its floor is higher (less lag at rest).
constexpr float minCutoffHz = 2.5f, speedCutoff = 80, speedDeadband = .05f, speedCutoffHz = 4;
constexpr float maxSeconds = .1f; // a hitch or gap starts afresh

struct Direction {
    Vec3 steady{}, speed{};
    bool filtering{};
    // `direction` world unit, `turn` the tracking space's orientation in the
    // world, `seconds` since the last update. Returns the steadied world direction.
    Vec3 update(Vec3 direction, Quat turn, float seconds) {
        const float n = turn.x * turn.x + turn.y * turn.y + turn.z * turn.z + turn.w * turn.w;
        if (!finite(direction) || length(direction) < .5f || !std::isfinite(n) || std::abs(n - 1) > .01f) {
            filtering = false;
            return direction;
        }
        const Vec3 raw = turn.conjugate().rotate(normalized(direction));
        if (!filtering || !std::isfinite(seconds) || seconds <= 0 || seconds > maxSeconds) {
            steady = raw;
            speed = {};
            filtering = true;
            return turn.rotate(steady);
        }
        const auto share = [dt = seconds](float hz) { return dt / (dt + 1 / (2 * 3.14159265f * hz)); };
        speed = speed + ((raw - steady) / seconds - speed) * share(speedCutoffHz);
        const float cutoff = minCutoffHz + speedCutoff * std::max(length(speed) - speedDeadband, 0.f);
        // Along the great circle (spherical_aim::toward): a flick over the
        // shoulder never passes through a near-zero mean.
        steady = spherical_aim::toward(steady, raw, share(cutoff));
        return turn.rotate(steady);
    }
    void reset() { *this = {}; }
};
// The aim pose turned so its -Z runs along `steadied` (position kept).
inline Pose steer(Pose aim, Vec3 steadied) {
    const Vec3 raw = aim.orientation.rotate({0, 0, -1});
    if (!finite(steadied) || length(steadied) < .5f || !finite(raw) || length(raw) < .5f)
        return aim;
    aim.orientation = arc(normalized(raw), normalized(steadied)) * aim.orientation;
    return aim;
}

} // namespace spidy::aim_filter
