#pragma once
#include "animated_view.hpp"
#include <cstdint>

namespace spidy::upper_frame {
// Full player-rig mode must not mix 45% authored arm motion into a tracked
// idle hand. Lost tracking may retain only the existing short visual fade;
// XR/raw tracking still withholds actions and clears physical punch history.
inline float armWeight(bool playerRig, bool tracked, bool valid, float fade) {
    return !valid ? 0.f : playerRig && tracked ? 1.f : std::clamp(fade,0.f,1.f);
}
// Original authoring anchor, NOT the player-driven head. Captured before any
// pose mutation and composed with the CURRENT actor transform on read.
struct Anchor {
    uintptr_t instance{};
    uint64_t serial{}, deadline{};
    Mat4 model{};
    bool active{};
    int read(uintptr_t actor, uint64_t now, const Mat4& root, Mat4& out) const {
        if (!active || actor != instance) return 0;
        if (!serial || now >= deadline || !native_view::validPose(root)) return -1;
        Mat4 world{};
        for (unsigned r=0;r<4;++r)
            for (unsigned c=0;c<4;++c)
                for (unsigned k=0;k<4;++k) world[r*4+c] += model[r*4+k]*root[k*4+c];
        if (!native_view::validPose(world)) return -1;
        out=world;
        return 1;
    }
};
// A job publishes directions/positions rotated into its actor's world axes.
// Rebase onto a more recent actual actor turn; translation is added ONLY by
// the caller using its freshly read root. No second tracking/view transform.
inline Pose rebase(Pose value, Quat turn) {
    return {turn.rotate(value.position), body::unit(turn*value.orientation)};
}
} // namespace spidy::upper_frame
