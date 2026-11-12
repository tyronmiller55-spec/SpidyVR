// LEG CONTROL's toggle (the user, November 12 00:44 and t64u: "The legs feel
// absolutely AMAZING, but I lose all of my other functionality, including the
// ability to rotate left and right ... let the player toggle legs in real
// time, on frame" with the left controller's thumb rest).
//
// LEG CONTROL stays the master switch. With LEG TOGGLE (leg_toggle_input):
//   0 THUMB REST TAP (default): a deliberate tap of the left thumb rest
//     (capacitive; Touch / Touch Plus /input/thumbrest/touch) toggles the
//     legs. A tap: the thumb rest untouched for at least idleSeconds, then
//     touched for tapMin..tapMax and let go, the left stick near its centre
//     and unclicked throughout. Resting the thumb there (a long touch), or
//     brushing it on the way to the stick, is no tap. It toggles on the frame
//     the thumb leaves the rest.
//   1 HOLD X+Y: X and Y held together 0.2 s toggles the legs (XYHold). The
//     left stick's click stays slow motion (t65u). Also where the runtime
//     took no thumb rest binding (not Touch, or refused): THUMB REST TAP
//     then reads X+Y (effective).
//   2 ALWAYS ON: the legs own the right stick in the air whenever LEG
//     CONTROL is on (as before November 12).
// Toggled off, the right stick is exactly what it was without LEG CONTROL
// (turn, zip up, heal down, basic flips' pitch), no double tap; the legs ease
// back to straight (the spring legs to their centre pose, easeSeconds, then
// the body's own blend out). The toggle starts OFF each session, so the right
// stick starts as it always was. Pure; no Windows.
#pragma once
#include <algorithm>
#include <cmath>

namespace spidy::leg_toggle {

enum class Input : int { thumbrest = 0, xyHold = 1, always = 2 };
// leg_toggle_input as chosen.
inline Input fromSetting(int value) {
    return value == 1 ? Input::xyHold : value == 2 ? Input::always : Input::thumbrest;
}
// What is read: THUMB REST TAP without a thumb rest binding is X+Y.
inline Input effective(Input chosen, bool thumbrestBound) {
    return chosen == Input::thumbrest && !thumbrestBound ? Input::xyHold : chosen;
}
inline const char* inputName(Input i) {
    switch (i) {
    case Input::thumbrest:
        return "thumb rest tap";
    case Input::xyHold:
        return "X+Y hold";
    default:
        return "always";
    }
}

// A deliberate tap of a capacitive thumb rest.
class ThumbrestTap {
  public:
    static constexpr float idleSeconds = .2f, tapMin = .03f, tapMax = .35f, stickRest = .3f;
    // seconds: since the last update; touched: the thumb rest; stick: the
    // left stick's tilt (0..1); click: the left stick pressed. True on the
    // frame a tap ends.
    bool update(float seconds, bool touched, float stick, bool click) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const bool still = std::isfinite(stick) && stick < stickRest && !click;
        bool tap = false;
        if (touched) {
            if (!touched_) {
                armed_ = idle_ >= idleSeconds && still;
                held_ = 0;
            } else {
                held_ += dt;
            }
            armed_ = armed_ && still && held_ <= tapMax;
            idle_ = 0;
        } else {
            if (touched_) {
                held_ += dt;
                tap = armed_ && still && held_ >= tapMin && held_ <= tapMax + dt;
            }
            armed_ = false;
            idle_ += dt;
        }
        touched_ = touched;
        return tap;
    }

  private:
    bool touched_{}, armed_{};
    float idle_ = 1.f, held_{};
};

// X and Y held together. From the frame both are down neither reaches the
// game or the gadget wheel (X held 0.3 s opens it) until both are let go of;
// held holdSeconds, it toggles once. A single X or Y is untouched (a Y
// pressed just before the X has already gone to the game).
class XYHold {
  public:
    static constexpr float holdSeconds = .2f;
    struct Out {
        bool tap{};   // the toggle, this frame
        bool mask{};  // X and Y are the toggle's now: keep them from the game
    };
    Out update(float seconds, bool x, bool y) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        Out out;
        if (x && y && !chord_) {
            chord_ = true;
            held_ = 0;
            fired_ = false;
        } else if (chord_) {
            held_ += dt;
        }
        if (chord_ && x && y && !fired_ && held_ >= holdSeconds) {
            fired_ = true;
            out.tap = true;
        }
        if (chord_ && !x && !y)
            chord_ = false;
        out.mask = chord_;
        return out;
    }

  private:
    bool chord_{}, fired_{};
    float held_{};
};

// The legs' state: toggled or not, and easing back after being turned off.
class Toggle {
  public:
    static constexpr float easeSeconds = .6f;
    // tapped: this frame's tap (ThumbrestTap or XYHold); master: LEG
    // CONTROL on (with the flips it needs, a body, VR). Returns whether the
    // legs are engaged (they may own the stick where they may move).
    bool update(float seconds, Input input, bool master, bool tapped) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        changed_ = false;
        if (!master) {
            on_ = false;
            ease_ = 0;
            return false;
        }
        if (input == Input::always)
            return on_ = true;
        if (tapped) {
            on_ = !on_;
            changed_ = true;
            ++toggles_;
            if (!on_)
                ease_ = easeSeconds;
        }
        if (on_)
            ease_ = 0;
        else
            ease_ = std::max(0.f, ease_ - dt);
        return on_;
    }
    bool on() const {
        return on_;
    }
    bool changed() const {
        return changed_;
    }
    // Turned off within easeSeconds: the legs still spring to straight.
    bool easing() const {
        return !on_ && ease_ > 0;
    }
    unsigned toggles() const {
        return toggles_;
    }
    void reset() {
        on_ = changed_ = false;
        ease_ = 0;
    }

  private:
    bool on_{}, changed_{};
    float ease_{};
    unsigned toggles_{};
};

} // namespace spidy::leg_toggle
