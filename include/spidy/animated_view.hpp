#pragma once
#include "body_ik.hpp"
#include "native_view.hpp"

// The head-locked eye chain, also used by HYBRID's arm-only overlay. No game
// addresses: axes/eye offsets come from the discovered rig (REFERENCE.md, BODY).
namespace spidy::animated_view {
enum class Frame { none, full, level, world, missing };
struct Targets {
    Frame frame = Frame::none;
    Pose head{}, grips[2]{}, aims[2]{}; // relative to the displayed anchor
    bool valid[2]{};
    Quat levelWorld{};
    Pose worldAnchor{}; // ABSOLUTE world eye anchor, also for head-off tracking view
    // HAND PREDICTION (arm_frame::ahead): each grip's velocity in this same
    // relative frame (m/s) and when it was sampled (QueryPerformanceCounter
    // ticks; 0 none). The pose job moves the glove on by its wait.
    Vec3 gripVelocity[2]{};
    int64_t sampledTicks{}; float leadSeconds{};
};
inline Pose relative(Pose raw, Pose reference, float turn) {
    return compose({{}, Quat::yaw(turn)}, compose(inverse(reference), raw));
}
inline Pose fromNative(const Mat4& m) {
    return {{m[12], m[13], m[14]},
            body::fromAxes({m[0], m[1], m[2]}, {-m[4], -m[5], -m[6]}, {-m[8], -m[9], -m[10]})};
}
inline Mat4 toNative(Pose p) {
    const Mat4 axes = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
    return native_view::relativePose(axes, p);
}
inline Pose resolve(const Mat4& anchor, Pose relativePose) {
    return fromNative(native_view::relativePose(anchor, relativePose));
}
// Shared by the production pose job and real-pose regressions. `authored`
// contains the ORIGINAL head eye frame in model space, never a solved head.
inline bool modelAnchor(const Targets& view, Quat modelToWorld, Mat4 authored, Mat4& out, Vec3 actorRoot = {}) {
    const Quat toModel = modelToWorld.conjugate();
    if (view.frame == Frame::level) {
        auto at = fromNative(authored);
        at.orientation = toModel * view.levelWorld;
        authored = toNative(at);
    } else if (view.frame == Frame::world) {
        authored = toNative({toModel.rotate(view.worldAnchor.position - actorRoot),
                              toModel * view.worldAnchor.orientation});
    }
    if (view.frame == Frame::none || view.frame == Frame::missing || !native_view::validPose(authored))
        return false;
    out=authored;
    return true;
}
// Same right/down/forward convention as game_fp::frame. eyeOffset is in
// discovered (forward, up, left) metres, not a new hard-coded bone offset.
inline bool headFrame(Vec3 head, Vec3 forward, Vec3 up, Vec3 eyeOffset, Mat4& out) {
    if (!finite(head) || !finite(forward) || !finite(up) || !finite(eyeOffset) || length(forward) < 1e-6f ||
        length(up) < 1e-6f)
        return false;
    forward = normalized(forward);
    up = normalized(up);
    Vec3 left = cross(up, forward);
    if (length(left) < 1e-6f)
        return false;
    left = normalized(left);
    up = cross(forward, left);
    const Vec3 eye = head + forward * eyeOffset.x + up * eyeOffset.y + left * eyeOffset.z;
    out = {-left.x,   -left.y,   -left.z,   0, -up.x, -up.y, -up.z, 0,
           forward.x, forward.y, forward.z, 0, eye.x, eye.y, eye.z, 1};
    return true;
}
} // namespace spidy::animated_view
