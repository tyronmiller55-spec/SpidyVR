#pragma once
// FINISHERS ON Y+B (beta2; the owner, Nov 19 t79u: "I've been executing finishers with Y+B"). Pure; no
// Windows.
//
// Y+B is the game's own finisher input, and the game runs it: in the 0.8.7-na-beta1 session
// (game-vr-20261119-130907) every one of the 11 combat scenes the log called a "finisher candidate" came
// 0.4-1.3 s after B and then Y (the B often sent as the prompt's own Y+B, 0xa000), and the game's finisher
// scene started about 0.1 s after the Y. SpidyVR builds no finisher button of its own. This watch only tells
// a finisher scene from any other scene for the log, and says while one plays, so nothing of SpidyVR's takes
// the body back in the middle of it (the grip cancel did, once, 3.8 s into a 4.0 s finisher at t=971.7 s).
//
// A finisher: a scene that starts in a fight within leadSeconds of a Y+B ask - Y and B each pressed within
// pairSeconds of the other, or the game's prompt naming Y+B sent (game_pad::NamedPress) - and lasts until the
// scene ends.
#include <algorithm>
#include <cmath>

namespace spidy::finisher_handoff {

inline constexpr float pairSeconds = .6f;  // Y and B pressed at most this far apart
inline constexpr float leadSeconds = 1.5f; // the scene starts at most this long after the ask
inline constexpr float never = 1e9f;

class Watch {
  public:
    struct Scene {
        bool finisher{}; // the game's own finisher on Y+B
        float since{};   // seconds from the ask to the scene's start (never: no ask)
        float gap{};     // seconds between the Y and the B (0 for a named Y+B)
        bool named{};    // the ask was the prompt's Y+B
    };
    // Each frame: the controllers' Y and B (before anything of SpidyVR's takes them) and whether the game's
    // prompt naming Y+B is being sent this frame.
    void update(float seconds, bool y, bool b, bool namedYB) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        sinceY_ += dt;
        sinceB_ += dt;
        sinceAsk_ += dt;
        if (y && !y_)
            sinceY_ = 0;
        if (b && !b_)
            sinceB_ = 0;
        y_ = y;
        b_ = b;
        const bool fresh = sinceY_ == 0 || sinceB_ == 0;
        if (fresh && sinceY_ <= pairSeconds && sinceB_ <= pairSeconds) {
            sinceAsk_ = 0;
            gap_ = std::abs(sinceY_ - sinceB_);
            named_ = false;
        }
        if (namedYB) {
            sinceAsk_ = 0;
            gap_ = 0;
            named_ = true;
        }
    }
    // A scene started this frame (the first-person cutscene gate): whether it is a finisher. While it is,
    // active().
    Scene started(bool fighting) {
        Scene s;
        s.since = sinceAsk_ < never * .5f ? sinceAsk_ : never;
        s.gap = gap_;
        s.named = named_;
        s.finisher = fighting && sinceAsk_ <= leadSeconds;
        active_ = s.finisher;
        startedAt_ = 0;
        return s;
    }
    void tick(float seconds) {
        if (active_)
            startedAt_ += std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
    }
    // The scene ended: how long the finisher played (0 for none).
    float ended() {
        const float played = active_ ? startedAt_ : 0.f;
        active_ = false;
        startedAt_ = 0;
        return played;
    }
    bool active() const {
        return active_;
    }
    float sinceAsk() const {
        return sinceAsk_;
    }

  private:
    bool y_{}, b_{}, named_{}, active_{};
    float sinceY_ = never, sinceB_ = never, sinceAsk_ = never, gap_{}, startedAt_{};
};

} // namespace spidy::finisher_handoff
