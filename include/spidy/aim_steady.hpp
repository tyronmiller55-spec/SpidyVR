// AIM STEADY (the user, Nov 13, on 99b8154: "The markers are somehow connected
// to the camera motion instead of being their own indicators").
//
// While the animation owns the body, the hands are mapped onto the head the
// eyes ride (arm_frame::ViewMap: the animated head bone's place, its pitch and
// roll at CAMERA PITCH / ROLL, the bob's shift, the sway's yaw), so a hand held
// still in the room points wherever Spidey's head bobs, nods and sways: the
// webs' line - and the marker drawn on it - swing with the camera, metres at
// range. AnchorSteady keeps a slow copy of that anchor's orientation and turns
// the aim line from the moving anchor onto it:
//   steady <- slerp(steady, anchor, 1 - exp(-dt / seconds)), at most maxLag behind
//   aim'   =  (steady * anchor^-1) * aim
// so the anchor's quick motion (the bob's nod, a stride's roll, a few degrees
// at 1-3 Hz) no longer moves the line, while what lasts (a wall's tilt, a
// turn, a slope) comes through within `seconds`, never more than maxLag
// behind. A jump of over jumpDegrees in one frame (a snap turn, a frame
// switch) is taken at once. Only the line's direction turns: where the web
// leaves from (the glove) does not move. Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include "spidy/web_aim.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::aim_steady {

// The angle of the turn between two orientations, radians.
inline float turnAngle(Quat a, Quat b) {
    const Quat d = a * b.conjugate();
    const float w = std::clamp(std::abs(d.w), 0.f, 1.f);
    return 2 * std::acos(w);
}

class AnchorSteady {
  public:
    static constexpr float seconds = .2f;
    static constexpr float maxLagDegrees = 5.f;
    static constexpr float jumpDegrees = 25.f;
    // The turn to put on an aim taken from `anchor` this frame (dt seconds
    // since the last): identity the first frame, after a jump or a bad input.
    Quat update(Quat anchor, float dt) {
        const float n = anchor.x * anchor.x + anchor.y * anchor.y + anchor.z * anchor.z + anchor.w * anchor.w;
        if (!std::isfinite(n) || n < .5f) {
            primed_ = false;
            return {};
        }
        anchor = web_aim::unitOr(anchor);
        constexpr float toRad = 3.14159265f / 180;
        if (!primed_ || !std::isfinite(dt) || dt <= 0 || dt > .25f ||
            turnAngle(anchor, last_) > jumpDegrees * toRad) {
            steady_ = last_ = anchor;
            primed_ = true;
            return {};
        }
        last_ = anchor;
        steady_ = web_aim::slerp(steady_, anchor, 1 - std::exp(-dt / seconds));
        const float lag = turnAngle(steady_, anchor);
        const float maxLag = maxLagDegrees * toRad;
        if (lag > maxLag)
            steady_ = web_aim::slerp(anchor, steady_, maxLag / lag);
        return web_aim::unitOr(steady_ * anchor.conjugate());
    }
    void reset() {
        primed_ = false;
    }
    bool primed() const {
        return primed_;
    }

  private:
    Quat steady_{}, last_{};
    bool primed_{};
};

} // namespace spidy::aim_steady
