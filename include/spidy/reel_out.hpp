#pragma once
// REEL OUT (0.8.7-na-beta3d, swing_reel_out; the owner, Nov 21 t93u: "I'd like the ability to double tap the
// trigger and hold to reel the line out, so we can have more control there"). With a web attached, the hand's
// reel control (the trigger; the grip with WEBS ON THE TRIGGER) pressed a second time within tapSeconds of
// the first press and held pays that hand's line out at `speed` m/s until it is let go of, the web dropped,
// or the line at the swing's longest (SwingConfig::maxRange). The first press reels in as it always did (a
// tap is a short reel); only the second, held, pays out. A first press held past the window is an ordinary
// reel and starts nothing. Shooting and letting go of webs are the other control's and never change. Engine
// independent; the swing module runs it each step (Swing::reelOut) and pays the line out (the taut take-in
// stands down for that hand meanwhile, so the body falls or swings out onto the longer line).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::reel_out {

struct Params {
    bool enabled = true;
    float tapSeconds = .3f; // from the first press to the second (swing_reel_out_tap_ms, 150 to 600)
    float speed = 4.f;      // m/s while held (swing_reel_out_speed, cm/s in the file)
};

// The reel control's press with hysteresis, as the swing's reel reads it: down past 0.65, up under 0.35.
inline bool pressed(float control, bool wasDown) {
    if (!std::isfinite(control))
        return false;
    return wasDown ? control > .35f : control > .65f;
}

// One hand's double tap and hold.
class Tap {
  public:
    enum class Phase : uint8_t { idle, first, gap, holding, out };
    // seconds: this step; down: the reel control pressed (pressed()); attached: that hand's web holds
    // (ready). True while the line pays out.
    bool update(float seconds, bool down, bool attached, const Params& p) {
        began_ = false;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float window = std::clamp(std::isfinite(p.tapSeconds) ? p.tapSeconds : .3f, .15f, .6f);
        if (!p.enabled || !attached) {
            // A press already down when the web goes on is no first tap: wait for it to be let go of.
            phase_ = down ? Phase::holding : Phase::idle;
            t_ = 0;
            return false;
        }
        switch (phase_) {
        case Phase::idle:
            if (down) {
                phase_ = Phase::first;
                t_ = 0;
            }
            break;
        case Phase::first:
            t_ += dt;
            if (!down)
                phase_ = t_ <= window ? Phase::gap : Phase::idle;
            else if (t_ > window)
                phase_ = Phase::holding; // an ordinary reel
            break;
        case Phase::gap:
            t_ += dt;
            if (down) {
                if (t_ <= window) {
                    phase_ = Phase::out;
                    began_ = true;
                    ++count_;
                } else {
                    phase_ = Phase::first;
                    t_ = 0;
                }
            } else if (t_ > window) {
                phase_ = Phase::idle;
            }
            break;
        case Phase::holding:
            if (!down)
                phase_ = Phase::idle;
            break;
        case Phase::out:
            if (!down)
                phase_ = Phase::idle;
            break;
        }
        return phase_ == Phase::out;
    }
    bool out() const {
        return phase_ == Phase::out;
    }
    // This update began paying out.
    bool began() const {
        return began_;
    }
    Phase phase() const {
        return phase_;
    }
    uint32_t count() const {
        return count_;
    }
    void reset() {
        phase_ = Phase::idle;
        t_ = 0;
        began_ = false;
    }

  private:
    Phase phase_ = Phase::idle;
    float t_{};
    bool began_{};
    uint32_t count_{};
};

// The line paid out over one step: `speed` m/s, up to `longest`; never shorter than it was.
inline float paidOut(float length, float speed, float seconds, float longest) {
    if (!std::isfinite(length) || !std::isfinite(speed) || !std::isfinite(seconds) || seconds <= 0 ||
        speed <= 0)
        return length;
    const float cap = std::isfinite(longest) ? longest : length;
    return std::max(length, std::min(length + speed * seconds, cap));
}

} // namespace spidy::reel_out
