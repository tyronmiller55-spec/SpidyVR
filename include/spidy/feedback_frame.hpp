#pragma once
#include "flip_reprojection.hpp"
#include "animated_view.hpp"

namespace spidy::feedback_frame {
// Transport a head-relative physical gaze/widget into the actual display
// frame. Never apply this to already solved hands, world anchors or IK input.
inline Pose transport(Pose sourceHead, Pose displayedHead, Pose value) {
    return compose(displayedHead, compose(inverse(sourceHead), value));
}
// A render camera alone does not prove that either hand has a current solved
// action pose. Missing/stale ownership frames hide hand-bound feedback, never
// silently present the unrelated raw rig. Original VR presentation is unchanged.
inline void hands(GameMotionFrame& frame, bool native, unsigned mask) {
    if (native) for (unsigned i=0;i<2;++i)
        if (!(mask & (1u<<i))) frame.swing.hands[i].tracked = false;
}
// Install the captured native camera BEFORE held reprojection chooses its
// pivot. Caller has validated the render generation + serial pair and removed
// render travel once. Actual hands remain in their own authoritative world frame.
inline void camera(GameMotionFrame& frame, const std::array<Mat4,2>& eyes) {
    Pose head = animated_view::fromNative(eyes[0]);
    head.position = {(eyes[0][12]+eyes[1][12])*.5f,
                     (eyes[0][13]+eyes[1][13])*.5f,
                     (eyes[0][14]+eyes[1][14])*.5f};
    frame.gazePose = transport(frame.headPose, head, frame.gazePose);
    frame.headPose = head;
    frame.eyes = eyes;
}
} // namespace spidy::feedback_frame
