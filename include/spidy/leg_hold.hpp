// LEG CONTROL's hold (the user, November 12 01:33, t67u: "Legs will use y+x,
// thumb rest didn't work ... make sure that web zip and gadget menu are
// disabled while the player is holding both buttons. Legs should stay active
// as long as the player is still manipulating the stick, even if they let go
// of the buttons. Neutral on the right stick cancels legs.").
//
// LEG CONTROL stays the master switch; the legs are a hold-to-engage modifier:
//   - Y and X held together engage the legs (the right stick poses them where
//     legs may move: in the air or swinging). From the chord until BOTH are
//     let go of, neither button reaches the game or the mod: no web strike /
//     zip on Y, no gadget wheel on X, on the press, during the hold or on the
//     release.
//   - One pressed alone is held back up to chordSeconds for the other. 80 ms:
//     a deliberate two-button press (on Touch usually one thumb across both)
//     lands its second button well inside 50 ms, so 80 ms catches nearly all
//     of them with margin, while a lone Y (the web strike / zip) is late by at
//     most 80 ms - 6 frames at 72 Hz, under the ~100 ms where a press starts
//     to feel late - and a lone X's 0.3 s gadget-wheel hold is unaffected in
//     feel. Let go of inside the window, it reaches the game as a tap
//     (tapSeconds long, so the game's poll sees it). Only while a chord may
//     start (allowed: the legs could move); otherwise X and Y pass at once.
//   - Let go of the buttons with the right stick deflected, the legs stay
//     engaged while the stick is moved: back to neutral (under neutralLow)
//     for neutralSeconds disengages them, and the right stick is the turn's,
//     zip's and heal's again on that frame. Hysteresis: pushed past
//     neutralHigh restarts the neutral wait; between the two it neither
//     counts nor restarts. Neutral while both buttons are still held keeps
//     them engaged.
//   - Disengaged, the legs ease back to straight over easeSeconds.
// Pure; no Windows.
#pragma once
#include <algorithm>
#include <cmath>

namespace spidy::leg_hold {

inline constexpr float chordSeconds = .08f, tapSeconds = .05f;
inline constexpr float neutralLow = .18f, neutralHigh = .30f, neutralSeconds = .08f;
inline constexpr float easeSeconds = .6f;

enum class Why : unsigned char { none, chord, neutral, gate };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::chord:
        return "Y+X held";
    case Why::neutral:
        return "right stick back to neutral";
    case Why::gate:
        return "LEG CONTROL unavailable (off, out of play)";
    default:
        return "none";
    }
}

class Hold {
  public:
    struct Input {
        float seconds{};
        bool x{}, y{};               // the controllers' X and Y as pressed
        float stickX{}, stickY{};    // the right stick
        bool master{};               // LEG CONTROL usable in play (its flips, a body, VR, playing)
        bool allowed{};              // a chord may start now (master, and the legs could move)
    };
    struct Output {
        bool x{}, y{};               // what reaches the game and the rest of the mod
        bool engaged{};              // the legs own the right stick (where they may move)
        bool engagedNow{}, disengagedNow{};
        Why why = Why::none;         // why, on those frames
        bool easing{};               // disengaged within easeSeconds
        bool chord{};                // both buttons are the legs' (held back) now
    };
    enum class Phase : unsigned char { idle, waitX, waitY, chord };

    Output update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        const bool xPressed = in.x && !xWas_, yPressed = in.y && !yWas_;
        xWas_ = in.x;
        yWas_ = in.y;
        Output out;
        xPass_ = xPass_ && in.x;
        yPass_ = yPass_ && in.y;
        tapX_ = std::max(0.f, tapX_ - dt);
        tapY_ = std::max(0.f, tapY_ - dt);
        switch (phase_) {
        case Phase::chord:
            if (!in.x && !in.y)
                phase_ = Phase::idle;
            break;
        case Phase::idle:
            if (in.allowed && !xPass_ && !yPass_) {
                if (xPressed && yPressed)
                    begin(out);
                else if (xPressed)
                    phase_ = Phase::waitX, wait_ = 0;
                else if (yPressed)
                    phase_ = Phase::waitY, wait_ = 0;
            } else {
                // A button already the game's: the other goes to the game too.
                xPass_ = xPass_ || xPressed;
                yPass_ = yPass_ || yPressed;
            }
            break;
        case Phase::waitX:
        case Phase::waitY: {
            wait_ += dt;
            const bool first = phase_ == Phase::waitX ? in.x : in.y;
            const bool other = phase_ == Phase::waitX ? yPressed || in.y : xPressed || in.x;
            if (other && in.allowed && first) {
                begin(out);
            } else if (!first) {
                // Let go of inside the window: the game's tap, late.
                // (The other pressed on this very frame goes to the game.)
                (phase_ == Phase::waitX ? tapX_ : tapY_) = tapSeconds;
                xPass_ = xPass_ || in.x;
                yPass_ = yPass_ || in.y;
                phase_ = Phase::idle;
            } else if (!in.allowed || wait_ >= chordSeconds) {
                // Alone past the window (or the legs no longer possible): the game's, late.
                xPass_ = xPass_ || in.x;
                yPass_ = yPass_ || in.y;
                phase_ = Phase::idle;
            }
            break;
        }
        }
        // Engaged: kept while both are held; after, by the stick; the gate ends it.
        if (engaged_) {
            const bool both = phase_ == Phase::chord && in.x && in.y;
            const float sx = std::isfinite(in.stickX) ? in.stickX : 0.f;
            const float sy = std::isfinite(in.stickY) ? in.stickY : 0.f;
            const float tilt = std::hypot(sx, sy);
            if (!in.master) {
                end(out, Why::gate);
            } else if (both || tilt > neutralHigh) {
                neutral_ = 0;
            } else if (tilt < neutralLow) {
                neutral_ += dt;
                if (neutral_ >= neutralSeconds)
                    end(out, Why::neutral);
            }
        }
        if (!engaged_)
            ease_ = std::max(0.f, ease_ - (out.disengagedNow ? 0.f : dt));
        out.engaged = engaged_;
        out.easing = !engaged_ && ease_ > 0;
        out.chord = phase_ == Phase::chord || phase_ == Phase::waitX || phase_ == Phase::waitY;
        // Held back while the legs have them (the chord) or one waits for the other.
        out.x = phase_ != Phase::chord && phase_ != Phase::waitX && (xPass_ || tapX_ > 0);
        out.y = phase_ != Phase::chord && phase_ != Phase::waitY && (yPass_ || tapY_ > 0);
        return out;
    }
    bool engaged() const {
        return engaged_;
    }
    Phase phase() const {
        return phase_;
    }
    unsigned engagements() const {
        return engagements_;
    }
    void reset() {
        *this = {};
    }

  private:
    void begin(Output& out) {
        phase_ = Phase::chord;
        tapX_ = tapY_ = 0;
        xPass_ = yPass_ = false;
        neutral_ = 0;
        if (!engaged_) {
            engaged_ = true;
            out.engagedNow = true;
            out.why = Why::chord;
            ++engagements_;
        }
        ease_ = 0;
    }
    void end(Output& out, Why why) {
        engaged_ = false;
        neutral_ = 0;
        ease_ = easeSeconds;
        out.disengagedNow = true;
        out.why = why;
    }
    Phase phase_ = Phase::idle;
    bool xWas_{}, yWas_{}, xPass_{}, yPass_{}, engaged_{};
    float wait_{}, tapX_{}, tapY_{}, neutral_{}, ease_{};
    unsigned engagements_{};
};

} // namespace spidy::leg_hold
