// LOW FPS (polish pass, slice 6). The 4d58169 playtest's game ran at 8.7 fps for 104 minutes ("Failed to
// evaluate DLSS" 22,894 times, Video Usage 6,449 MB over its 6,080 MB budget) and recovered when the audio
// device was re-initialised: the mod was under 1% of the game thread and wrote 50+ MB of identical lines.
// This watches the VR loop's frame times (the game paces them) in windows of `windowSeconds`:
//  - SLOW once every window for `slowSeconds` ran under `slowFps` (and the frames were still coming: a game
//    that stops altogether is the freeze watch's, game_freeze.py);
//  - back to NORMAL after `goodWindows` windows at `goodFps` or more.
// The XR worker turns SLOW into the diagnostics' quiet mode (diag_log::setSlow); tools/run_game_vr.py has the
// same rule on the frame counter to tell the launcher (WARNING line). Pure.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::low_fps {
inline constexpr float windowSeconds = 5.f, slowFps = 15.f, goodFps = 24.f;
inline constexpr float slowSeconds = 60.f;
inline constexpr unsigned goodWindows = 2;
enum class Event { none, slow, normal };
class Watch {
  public:
    // One frame (seconds since the last). The event when the state changes.
    Event frame(float seconds) {
        if (!std::isfinite(seconds) || seconds <= 0)
            return Event::none;
        // A frame past a minute is a pause or a stop: not a measurement.
        if (seconds > 60.f) {
            reset();
            return Event::none;
        }
        time_ += seconds;
        ++frames_;
        if (time_ < windowSeconds)
            return Event::none;
        fps_ = static_cast<float>(frames_) / time_;
        const float spent = time_;
        time_ = 0;
        frames_ = 0;
        if (fps_ < slowFps) {
            good_ = 0;
            slowFor_ += spent;
            if (!slow_ && slowFor_ >= slowSeconds) {
                slow_ = true;
                return Event::slow;
            }
        } else {
            slowFor_ = 0;
            if (slow_ && fps_ >= goodFps && ++good_ >= goodWindows) {
                slow_ = false;
                good_ = 0;
                return Event::normal;
            }
            if (fps_ < goodFps)
                good_ = 0;
        }
        return Event::none;
    }
    bool slow() const {
        return slow_;
    }
    // The last window's rate.
    float fps() const {
        return fps_;
    }
    float slowFor() const {
        return slowFor_;
    }
    void reset() {
        *this = Watch{};
    }

  private:
    bool slow_{};
    unsigned frames_{}, good_{};
    float time_{}, fps_{}, slowFor_{};
};
// The line for the diagnostics.
inline const char* line(Event e, char* out, unsigned size, float fps, float seconds) {
    if (e == Event::slow)
        std::snprintf(out, size,
                      "low fps: the game has run at about %.0f fps for %.0f s: quiet log mode on (only changes "
                      "are written); the mod's own cost is in the game thread cost lines",
                      fps, seconds);
    else
        std::snprintf(out, size, "low fps over: the game is back at about %.0f fps: normal log mode", fps);
    return out;
}
} // namespace spidy::low_fps
