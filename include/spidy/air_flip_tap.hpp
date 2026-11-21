// DOUBLE-TAP A: UPSIDE DOWN (air_flip_double_tap, 0.8.7-na-beta3b; the owner, Nov 21 t88u: "give me an option to
// simply flip Spidey perfectly upside down by double tapping A. I can hold on the double tap to stay inverted, this
// gives me options to lock myself upright or upside down"). Pure - no Windows, no game addresses.
//
// With REALISTIC FLIPS, A pressed in SpidyVR's air rights the player over A IN THE AIR (air_correct_ms) and, held,
// keeps him upright until it is let go of (RealisticFlip's brake). This watcher only tells which way that press
// turns: a press that starts in the air within `windowSeconds` of the last one that did (and was let go of between)
// is a double tap, and turns him upside down instead, the same smoothing, held the same way.
//
// No latency is added to a single A: the first press starts the upright at once, exactly as before; a second press
// inside the window is a new press of A, which the brake takes as a new turn from wherever the first one had got to,
// now toward upside down. With B down at the second press (A+B, the point jump's chord) it is not a double tap, and a
// press that began on the ground (a jump) never counts: both presses start in the air.
#pragma once
#include <algorithm>
#include <cmath>

namespace spidy::air_flip_tap {

struct Params {
    bool enabled = true;
    float windowSeconds = .3f; // air_flip_double_tap_ms
};
struct Input {
    bool a{};    // A held (the jump), as the brake reads it
    bool air{};  // SpidyVR's air, not on a surface: where A rights the player
    bool b{};    // B held: A+B is the point jump's chord, not a double tap
    float seconds{};
};
struct Output {
    bool press{};     // A went down in the air this frame
    bool doubleTap{}; // ... and it is the second of a double tap
    bool inverted{};  // the press in force (held or not) is a double tap's: the turn is toward upside down
    bool holding{};   // inverted and A still held: locked upside down until it is let go of
};

class Tap {
  public:
    Output update(const Input& in, const Params& p) {
        Output o;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        since_ = (std::min)(since_ + dt, 10.f);
        const bool rise = in.a && !held_;
        held_ = in.a;
        if (!in.air) {
            // On the ground (or a wall, a perch): nothing counts, and a press already down is not one from the air.
            armed_ = inverted_ = false;
            return o;
        }
        if (rise) {
            o.press = true;
            o.doubleTap = p.enabled && armed_ && since_ <= p.windowSeconds && !in.b;
            inverted_ = o.doubleTap;
            armed_ = !o.doubleTap; // a third tap starts over: upright, and the first of a new pair
            since_ = 0.f;
            ++presses_;
        }
        o.inverted = inverted_;
        o.holding = inverted_ && in.a;
        return o;
    }
    unsigned presses() const {
        return presses_;
    }
    void reset() {
        *this = {};
    }

  private:
    float since_ = 10.f;
    bool held_{}, armed_{}, inverted_{};
    unsigned presses_{};
};

} // namespace spidy::air_flip_tap
