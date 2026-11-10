// ARMS IN ANIMATION (the user, November 9 22:27): while the game's animation
// owns the body, the player's hands are mapped into the game EVERY XR FRAME
// through the very map the eyes use - the controller relative to the latched
// headset reference, turned by the view's own R-stick turn, placed on the
// animated head's eye anchor as the actor stands NOW. Webs, web balls, markers
// and punches aim from that tracked pose (the SpidyVR hands), never from the
// animated bones: a contorted animation pose no longer bends the web line, and
// a turn of the actor (R-stick model yaw) turns the webs with the view.
// Pure: no Windows, no game addresses.
#pragma once
#include "animated_view.hpp"
#include "hybrid_handoff.hpp"
#include "tracking.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::arm_frame {

// The eyes' tracking -> game map this frame.
struct ViewMap {
    bool valid{};
    Mat4 anchor{};   // the displayed anchor in WORLD (native axes), absolute position
    Pose reference{}; // the latched headset reference (tracking space)
    float turn{};    // the view's own turn (Level R-stick), radians
};

// The anchor a frame mode displays, in world: FULL is the animated head's eye
// frame; LEVEL keeps its place with the scene's level heading; WORLD is the
// blend's absolute eye centre. Mirrors animated_view::modelAnchor in world.
inline bool worldAnchor(animated_view::Frame frame, const Mat4& headWorld, Quat levelWorld,
                        Pose worldEyes, Mat4& out) {
    using animated_view::Frame;
    if (frame == Frame::full) {
        out = headWorld;
    } else if (frame == Frame::level) {
        auto at = animated_view::fromNative(headWorld);
        at.orientation = levelWorld;
        out = animated_view::toNative(at);
    } else if (frame == Frame::world) {
        out = animated_view::toNative(worldEyes);
    } else {
        return false;
    }
    return native_view::validPose(out);
}

// A tracked (OpenXR tracking-space) pose in the game, as the eyes show it.
inline Pose toGame(const ViewMap& m, Pose tracked) {
    return animated_view::resolve(m.anchor, animated_view::relative(tracked, m.reference, m.turn));
}

// The swing's tracking yaw that turns tracking-space offsets the way this map
// does (its heading part): the mapped look's yaw less the tracked look's.
inline float trackingYaw(const ViewMap& m, Pose trackedHead) {
    const Vec3 tracked = trackedHead.orientation.rotate({0, 0, -1});
    const Vec3 mapped = toGame(m, trackedHead).orientation.rotate({0, 0, -1});
    return hybrid_handoff::wrapPi(hybrid_handoff::lookYaw(mapped) - hybrid_handoff::lookYaw(tracked));
}

// The web shooter on the tracked hand in the game: what webs, balls and the
// marker aim from while the animation owns.
inline Pose shooter(const ViewMap& m, Pose trackedGrip, unsigned hand) {
    return webShooter(toGame(m, trackedGrip), hand);
}

// HAND PREDICTION for the animated gloves: the pose job runs on the game's
// frame (about 41 a second in the Nov 9 22:27 session against the headset's
// 72+), after the command was sent. The glove's target moves on by the
// hand's own velocity for that wait, at most maxAhead.
constexpr float maxAhead = .025f;
inline Pose ahead(Pose relative, Vec3 velocity, float seconds) {
    if (!finite(velocity) || !std::isfinite(seconds) || seconds <= 0)
        return relative;
    relative.position = relative.position + velocity * std::min(seconds, maxAhead);
    return relative;
}

} // namespace spidy::arm_frame
