// POINT JUMP BULLET (the user, Nov 14 t71u and t73s656: "point jump should orient the body vertically in the direction of
// the eyes on launch; it should feel like shooting off like a bullet. Let it take whatever angle the eyes had"; setting
// point_jump_bullet, default on). On a point jump's launch the body's long axis (feet to head) turns onto the RAW look
// direction, straight down included, eased over about 120 ms. The launch velocity keeps its own 10 degree out-of-surface
// fix (point_jump.hpp direction()): only the body takes the raw eye angle.
//
// The body only: the tilt is drawn on the native body (game_xr.cpp's body pose), about the eyes, like a flip's tilt, but it
// is not the rig's: it never moves the view, never writes a yaw (nothing here turns the heading: REFERENCE.md "camera
// leads", t72u), and never writes a velocity to the game. It is the same body quaternion convention as
// GameMotionFrame::bodyTilt, in world axes.
//
// It lets go (eases back to upright, 0.35 s; 0.12 s once he lands) when: he touches down, the speed falls under 6 m/s after
// 0.4 s, a web holds him, A is pressed (the A upright takes over), a hand holds him up (handstand), or 4 s have passed.
#pragma once

#include "math.hpp"

#include <algorithm>
#include <cmath>

namespace spidy::point_bullet {
struct Params {
    bool enabled = true;
    float easeIn = .12f;      // s: onto the look
    float easeOut = .35f;     // s: back to upright
    float easeLanded = .12f;  // s: back to upright once he lands
    float minSpeed = 6.f;     // m/s: slower than this after minHold lets go
    float minHold = .4f;      // s
    float maxHold = 4.f;      // s
};
struct Input {
    float seconds = 0;
    bool grounded{}, webbed{}, aPressed{}, handsHold{};
    float speed = 0; // m/s, the body's own
};
class Bullet {
  public:
    // The launch: `look` the raw eye direction (world), any angle; `heading` the way he faces (world, any length).
    void launch(Vec3 look, Vec3 heading, const Params& p) {
        if (!p.enabled || !(length(look) > .5f) || !std::isfinite(look.x + look.y + look.z))
            return;
        look = normalized(look);
        // Head to the look: the shortest turn of the body's up onto it. Straight down is a half turn about the heading's
        // right (the way a dive goes over, not a corkscrew).
        if (look.y < -.99999f) {
            Vec3 right = cross(Vec3{heading.x, 0, heading.z}, {0, 1, 0});
            right = length(right) > .1f ? normalized(right) : Vec3{1, 0, 0};
            target_ = Quat::around(right, 3.14159265f);
        } else
            target_ = arc({0, 1, 0}, look);
        age_ = 0;
        weight_ = 0;
        letGo_ = false;
        live_ = true;
        fade_ = p.easeOut;
        ++launches_;
    }
    // The body's turn this frame (identity when none).
    Quat update(const Input& in, const Params& p) {
        if (!live_)
            return {};
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        age_ += dt;
        if (!letGo_) {
            const bool slow = age_ > p.minHold && in.speed < p.minSpeed;
            if (!p.enabled || in.webbed || in.aPressed || in.handsHold || slow || age_ > p.maxHold ||
                (in.grounded && age_ > .1f)) {
                letGo_ = true;
                why_ = in.grounded && age_ > .1f ? "landed"
                       : in.webbed             ? "web"
                       : in.aPressed           ? "A"
                       : in.handsHold          ? "hands"
                       : !p.enabled            ? "off"
                       : slow                  ? "slow"
                                               : "time";
                fade_ = in.grounded ? p.easeLanded : p.easeOut;
                from_ = weight_;
                fadeAge_ = 0;
                ++letGoes_;
            }
        }
        if (letGo_) {
            fadeAge_ += dt;
            const float u = std::clamp(fadeAge_ / std::max(fade_, .02f), 0.f, 1.f);
            weight_ = from_ * (1 - smooth(u));
            if (u >= 1.f) {
                live_ = false;
                weight_ = 0;
                return {};
            }
        } else {
            weight_ = smooth(std::clamp(age_ / std::max(p.easeIn, .02f), 0.f, 1.f));
        }
        return turn(weight_);
    }
    bool live() const { return live_; }
    float weight() const { return weight_; }
    // The angle the body turns by at full weight, degrees (the log).
    float degrees() const {
        return 2 * std::atan2(length({target_.x, target_.y, target_.z}), std::abs(target_.w)) * 57.2957795f;
    }
    const char* why() const { return why_; }
    int launches() const { return launches_; }
    int letGoes() const { return letGoes_; }
    void reset() { *this = {}; }

  private:
    static float smooth(float u) { return u * u * u * (u * (6 * u - 15) + 10); }
    Quat turn(float w) const {
        const Vec3 v{target_.x, target_.y, target_.z};
        const float s = length(v);
        if (s < 1e-6f)
            return {};
        const float angle = 2 * std::atan2(s, target_.w);
        return Quat::around(v / s, angle * w);
    }
    Quat target_{};
    float age_{}, weight_{}, from_{}, fade_{.35f}, fadeAge_{};
    bool live_{}, letGo_{};
    const char* why_ = "";
    int launches_{}, letGoes_{};
};
} // namespace spidy::point_bullet
