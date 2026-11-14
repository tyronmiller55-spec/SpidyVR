// WEBS SURVIVE HITCHES (the user, November 13 22:5x: "the game is hitching heavily every few
// minutes, which drops my web lines"). The 22:24 session's two worst stalls (diag t 44.5 s and
// 643-647 s) each came with a swing speed read from one long frame (268 m/s while the real body
// moved at 12): Swing::predictNativeStep let go of everything above twice its speed limit.
// Two guards, both pure:
//  - VelocityGuard: the game's measured velocity of a mover step (native_movement's
//    achievedVelocity, a position change over a step's dt) is only believed when the step was a
//    normal one: no longer than longestStep (50 ms), not older than that when read, finite and
//    under the plausible cap. Otherwise the last believed velocity is held, so the swing carries
//    on from where it was instead of from a hitch. Held for at most holdSeconds of game time:
//    a speed that really stays that high (a launch) is believed after that.
//  - OverLimit: the swing's own let-go on an over-limit speed needs several consecutive valid
//    frames (sustainedFrames or sustainedSeconds), never one, and never a frame over 50 ms.
// GameFrameClock: the game thread's own frame time, from the swing sample's QPC stamp, as diag
// "game frame:" lines: each spike (a step over 100 ms) with its duration, and a summary that counts
// every step over 50 ms.
// CostWindow: what the mod itself cost the game thread, per 10 s window (G1 playtest: the mod's share of
// a hitch could not be told from the render load), and how often the hitch guards fired.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::hitch_guard {
// A step this long or shorter is a normal one (the 50 ms the native step has always been judged by).
constexpr float longestStep = .05f;
// A sample read this long after its step stamped it is stale.
constexpr float staleSeconds = .05f;
// The velocity held through rejected steps, at most this long before a real one is believed.
constexpr float holdSeconds = .5f;
// Valid over-limit frames before a swing lets go: 3 frames or 50 ms.
constexpr unsigned sustainedFrames = 3;
constexpr float sustainedSeconds = .05f;

enum class Why : uint8_t { none, unseen, notFinite, longStep, stale, tooFast };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::unseen:
        return "first sample";
    case Why::notFinite:
        return "not finite";
    case Why::longStep:
        return "long step";
    case Why::stale:
        return "stale";
    case Why::tooFast:
        return "too fast";
    default:
        return "ok";
    }
}
class VelocityGuard {
  public:
    struct Result {
        Vec3 velocity{};
        bool rejected{};
        Why why{Why::none};
        float measuredSpeed{};
    };
    // One mover step's sample. `steps`: the step counter (a repeat of the last returns the last
    // answer). dt: that step's own length. idle: seconds since the step stamped it. cap: the
    // fastest believable speed (twice the swing's limit).
    Result sample(uint64_t steps, Vec3 achieved, float dt, float idle, float cap) {
        if (haveSteps_ && steps == steps_)
            return last_;
        Result r;
        r.measuredSpeed = finite(achieved) ? length(achieved) : 0.f;
        Why why = Why::none;
        if (!finite(achieved) || !std::isfinite(dt) || !std::isfinite(idle))
            why = Why::notFinite;
        else if (dt <= 0 || dt > longestStep)
            why = Why::longStep;
        else if (idle > staleSeconds)
            why = Why::stale;
        else if (r.measuredSpeed > cap)
            why = Why::tooFast;
        // Believed: a normal step; the first sample; or, once a velocity has been held for
        // holdSeconds, whatever the game says now (a slow game's every step is a long one, and a
        // speed that stays high is real). Never a velocity that is not finite.
        const bool believe = why == Why::none || !haveGood_ || heldSeconds_ >= holdSeconds;
        if (believe && finite(achieved)) {
            good_ = achieved;
            haveGood_ = true;
            heldSeconds_ = 0;
            r.velocity = good_;
            return remember(steps, r);
        }
        // A rejected step: the last believed velocity stands, for a while.
        heldSeconds_ += std::isfinite(dt) && dt > 0 ? std::min(dt, 1.f) : holdStep;
        r.velocity = good_;
        r.rejected = true;
        r.why = why == Why::none ? Why::notFinite : why;
        ++rejected_;
        return remember(steps, r);
    }
    uint32_t rejected() const {
        return rejected_;
    }
    void reset() {
        *this = {};
    }

  private:
    static constexpr float holdStep = .0167f;
    Result remember(uint64_t steps, Result r) {
        steps_ = steps;
        haveSteps_ = true;
        last_ = r;
        return r;
    }
    uint64_t steps_{};
    bool haveSteps_{}, haveGood_{};
    Vec3 good_{};
    float heldSeconds_{};
    uint32_t rejected_{};
    Result last_{};
};

// The swing's let-go on a speed over its limit: only a sustained run of valid frames.
class OverLimit {
  public:
    // One frame. over: its speed is over the limit. dt: the frame's length. Returns true when
    // the swing should let go: sustainedFrames valid over-limit frames in a row, or
    // sustainedSeconds of them. A frame over longestStep neither counts nor clears the run.
    // `ignored` (out) is true for a frame whose speed is not to be used (over the limit but not yet
    // sustained, or a long one): the caller carries on from its last velocity.
    bool update(bool over, float dt, bool& ignored) {
        ignored = false;
        if (!over) {
            frames_ = 0;
            seconds_ = 0;
            return false;
        }
        if (!std::isfinite(dt) || dt <= 0 || dt > longestStep) {
            ignored = true;
            ++longIgnored_;
            return false;
        }
        ++frames_;
        seconds_ += dt;
        if (frames_ >= sustainedFrames || seconds_ >= sustainedSeconds) {
            return true;
        }
        ignored = true;
        ++ignoredFrames_;
        return false;
    }
    void reset() {
        frames_ = 0;
        seconds_ = 0;
    }
    unsigned frames() const {
        return frames_;
    }
    uint32_t ignoredFrames() const {
        return ignoredFrames_ + longIgnored_;
    }

  private:
    unsigned frames_{};
    float seconds_{};
    uint32_t ignoredFrames_{}, longIgnored_{};
};

// The game thread's frame time from the swing sample's QPC stamp (one new stamp per game step).
class GameFrameClock {
  public:
    // A step over spikeSeconds is counted in the summary; one over lineSeconds also gets a line of its own
    // (the G1 playtest wrote 1,100 lines in 20 minutes at 50 ms: the summary now carries the rest).
    static constexpr float spikeSeconds = .05f, lineSeconds = .1f, pauseSeconds = 2.f, summaryEvery = 10.f;
    // qpc: the sample's stamp (0: none). Writes lines into `out` (count of lines returned):
    // "game frame: spike ..." for a step over lineSeconds, and a summary every summaryEvery of
    // game time. wall: seconds since the last call (the reader's own clock), to pace summaries.
    struct Lines {
        char text[2][200]{};
        unsigned count{};
    };
    void update(uint64_t qpc, int64_t frequency, float wall, Lines& out) {
        out.count = 0;
        if (!qpc || frequency <= 0)
            return;
        if (std::isfinite(wall) && wall > 0 && wall < 1)
            sinceSummary_ += wall;
        if (qpc != last_) {
            if (last_ && qpc > last_) {
                const float gap =
                    static_cast<float>(static_cast<double>(qpc - last_) / static_cast<double>(frequency));
                if (gap < pauseSeconds) {
                    ++steps_;
                    total_ += gap;
                    longest_ = std::max(longest_, gap);
                    if (gap > spikeSeconds) {
                        ++spikes_;
                        ++spikesTotal_;
                        worstTotal_ = std::max(worstTotal_, gap);
                        if (gap > lineSeconds)
                            std::snprintf(out.text[out.count++], sizeof out.text[0],
                                          "game frame: spike %d ms between game steps (spike %u, steps %llu; "
                                          "the swing sample, the game thread's own cadence)",
                                          static_cast<int>(gap * 1000 + .5f), spikesTotal_,
                                          static_cast<unsigned long long>(steps_));
                    }
                }
            }
            last_ = qpc;
        }
        if (sinceSummary_ >= summaryEvery && steps_ > 0 && out.count < 2) {
            std::snprintf(out.text[out.count++], sizeof out.text[0],
                          "game frame: %.1f s, %u game steps (%.1f a second), mean %.1f ms, longest %d ms, "
                          "%u over 50 ms (%u this session, worst %d ms)",
                          total_, steps_ > 0 ? static_cast<unsigned>(steps_ - summarisedSteps_) : 0u,
                          total_ > 0 ? static_cast<float>(steps_ - summarisedSteps_) / total_ : 0.f,
                          steps_ > summarisedSteps_
                              ? total_ * 1000 / static_cast<float>(steps_ - summarisedSteps_)
                              : 0.f,
                          static_cast<int>(longest_ * 1000 + .5f), spikes_, spikesTotal_,
                          static_cast<int>(worstTotal_ * 1000 + .5f));
            summarisedSteps_ = steps_;
            sinceSummary_ = 0;
            total_ = longest_ = 0;
            spikes_ = 0;
        }
    }
    unsigned spikesTotal() const {
        return spikesTotal_;
    }

  private:
    uint64_t last_{}, steps_{}, summarisedSteps_{};
    float total_{}, longest_{}, worstTotal_{}, sinceSummary_{};
    unsigned spikes_{}, spikesTotal_{};
};

// What the mod's game-thread step cost, in microseconds, over a window of game time (QPC). `add` takes one
// visit's cost, the solver's share of it, and the kick blocks' share, plus the hitch guards' cumulative
// counters (a counter that went down was reset: the new value counts whole). Returns true when a window
// closed; `done()` is then that window's account.
class CostWindow {
  public:
    static constexpr double windowSeconds = 10.;
    struct Window {
        double seconds{};
        uint32_t visits{}, over1ms{}, over5ms{}, rejected{}, ignored{};
        float meanUs{}, maxUs{}, solverMaxUs{}, kickMaxUs{};
    };
    bool add(uint64_t qpc, int64_t frequency, double visitUs, double solverUs, double kickUs, uint32_t rejected,
             uint32_t ignored) {
        if (!qpc || frequency <= 0 || !std::isfinite(visitUs))
            return false;
        if (!started_) {
            started_ = true;
            start_ = qpc;
            rejectedAt_ = rejected;
            ignoredAt_ = ignored;
        }
        ++visits_;
        total_ += visitUs;
        max_ = std::max(max_, visitUs);
        solverMax_ = std::max(solverMax_, std::isfinite(solverUs) ? solverUs : 0.);
        kickMax_ = std::max(kickMax_, std::isfinite(kickUs) ? kickUs : 0.);
        over1ms_ += visitUs > 1000.;
        over5ms_ += visitUs > 5000.;
        const double seconds = qpc > start_ ? static_cast<double>(qpc - start_) / static_cast<double>(frequency) : 0.;
        if (seconds < windowSeconds)
            return false;
        done_.seconds = seconds;
        done_.visits = visits_;
        done_.over1ms = over1ms_;
        done_.over5ms = over5ms_;
        done_.rejected = rejected >= rejectedAt_ ? rejected - rejectedAt_ : rejected;
        done_.ignored = ignored >= ignoredAt_ ? ignored - ignoredAt_ : ignored;
        done_.meanUs = visits_ ? static_cast<float>(total_ / visits_) : 0.f;
        done_.maxUs = static_cast<float>(max_);
        done_.solverMaxUs = static_cast<float>(solverMax_);
        done_.kickMaxUs = static_cast<float>(kickMax_);
        started_ = false;
        visits_ = over1ms_ = over5ms_ = 0;
        total_ = max_ = solverMax_ = kickMax_ = 0;
        return true;
    }
    const Window& done() const {
        return done_;
    }

  private:
    bool started_{};
    uint64_t start_{};
    uint32_t visits_{}, over1ms_{}, over5ms_{}, rejectedAt_{}, ignoredAt_{};
    double total_{}, max_{}, solverMax_{}, kickMax_{};
    Window done_{};
};
// The account as the diag's line (one per window).
inline void costLine(const CostWindow::Window& w, char* out, size_t size) {
    std::snprintf(out, size,
                  "game thread cost: %.1f s, %u mod steps, mean %.0f us, longest %.0f us (solver %.0f us, kicks %.0f us), "
                  "%u over 1 ms, %u over 5 ms; hitch guard: %u velocity samples held, %u over-limit frames ignored",
                  w.seconds, w.visits, w.meanUs, w.maxUs, w.solverMaxUs, w.kickMaxUs, w.over1ms, w.over5ms,
                  w.rejected, w.ignored);
}
} // namespace spidy::hitch_guard
