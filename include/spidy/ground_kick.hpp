#pragma once
// GROUND KICKS (K3; the plan's Pillar 5; ground_kicks, off until set, EXPERIMENTAL).
//
// Standing (and slow: the swing module refuses a kick above 2 m/s), with the legs engaged (Y + X, as in the
// air; leg_hold.hpp), the right stick kicks. Only a flick kicks; a slow push is nothing (the turn).
//
//   flick up (within 70 deg of up)          front kick: a freeform blow, the zone the gaze picks
//   flick sideways (70..110 deg)            waits up to 0.6 s for a sweep; otherwise a freeform side kick
//   half circle across the bottom           sweep: the foot swings round the pelvis ankle-high, from one side
//                                           through the front to the other, and takes the legs (a knock-down)
//   flick down                              nothing
//
// A sweep starts at a side (70..110 deg from up, at 0.6 or more of the stick) and, within 0.6 s, runs through
// the bottom (135 degrees or more from up, at 0.5 or more) to the other side (at 0.6 or more). It may begin
// with a flick (the side's flick then waits for it) or be drawn from a stick resting at the side (which is no
// kick if it is not completed). The leg that sweeps is the side the half circle began on.
//
// The kick itself is air_kick.hpp's (Kick: the same foot sphere against the fighting enemies' capsules, the
// blow through game_punch::kickHit); standing it has no lunge, the sweep is low and wide, and the flight is a
// knock-down (groundFlight). No torso lean and no hand plant: only the kick's foot is modelled, and the
// legs' ground pose is native_body's overlay (GroundKickAsk in the pose), which is off with the setting.
//
// Engine independent. XR detects the gesture (Gestures) and asks over the AIR KICKS side channel
// (game_swing::AirKickAsk, flags bit 2).
#include "air_kick.hpp"

namespace spidy::ground_kick {
using air_kick::deg;
using air_kick::Flick;
using air_kick::Gesture;

struct Config {
    // The flick: as the air's (the stick from under flickLow to over flickHigh within flickSeconds).
    float flickLow = .3f, flickHigh = .8f, flickSeconds = .08f;
    // Sideways, and the down that is nothing (degrees from up).
    float diveDegrees = 70.f, sideDegrees = 110.f;
    // A sweep: where each end is (size, and the side's band), how far the bottom is, the least size on the
    // way, and the longest it takes.
    float rimSize = .6f, bottomDegrees = 135.f, wayMin = .5f, sweepSeconds = .6f;
};

class Gestures {
  public:
    // x right, y up (the controller's). engaged: the legs are engaged and the setting is on.
    Flick update(float seconds, float x, float y, bool engaged, const Config& c = {}) {
        Flick none;
        if (!engaged || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(seconds)) {
            reset();
            return none;
        }
        const float dt = std::max(seconds, 0.f);
        const float size = std::hypot(x, y);
        const float angle = air_kick::stickAngle(x, y), a = std::abs(angle);
        if (pending_) {
            age_ += dt;
            // The way: the bottom reached, then the other side's rim.
            if (size >= c.wayMin && a >= c.bottomDegrees)
                bottom_ = true;
            if (bottom_ && size >= c.rimSize && a >= c.diveDegrees && a <= c.sideDegrees &&
                air_kick::sideOf(x) == -side_) {
                const int side = side_;
                end();
                armed_ = false;
                return {Gesture::sweep, side, angle, size};
            }
            if (size < c.wayMin || age_ > c.sweepSeconds) {
                // Lost the way (the stick came in, or too slow): a flick's side kick, a drawn start nothing.
                const bool flick = flick_;
                const Flick out{Gesture::freeform, side_, angle0_, size0_};
                end();
                armed_ = size < c.flickLow;
                return flick ? out : none;
            }
            return none;
        }
        if (size < c.flickLow) {
            armed_ = true;
            sinceLow_ = 0;
            return none;
        }
        sinceLow_ += dt;
        const bool side = a >= c.diveDegrees && a <= c.sideDegrees && size >= c.rimSize;
        const bool quick = armed_ && size > c.flickHigh && sinceLow_ <= c.flickSeconds + 1e-6f;
        if (size > c.flickHigh)
            armed_ = false;
        if (side && !(a > c.sideDegrees)) {
            // A side: a flick's (waits for a sweep), or a stick drawn there (the start of one).
            if (!quick && wasSide_) {
                return none;
            }
            wasSide_ = true;
            pending_ = true;
            flick_ = quick;
            side_ = air_kick::sideOf(x);
            age_ = 0;
            bottom_ = false;
            angle0_ = angle;
            size0_ = size;
            return none;
        }
        wasSide_ = false;
        if (!quick)
            return none;
        if (a > c.sideDegrees) // down: nothing
            return none;
        return {Gesture::freeform, air_kick::sideOf(x), angle, size};
    }
    void reset() {
        armed_ = pending_ = wasSide_ = false;
        sinceLow_ = 1e3f;
    }
    bool pending() const {
        return pending_;
    }

  private:
    void end() {
        pending_ = false;
    }
    bool armed_{}, pending_{}, flick_{}, bottom_{}, wasSide_{};
    float sinceLow_ = 1e3f, age_{}, angle0_{}, size0_{};
    int side_{};
};
} // namespace spidy::ground_kick
