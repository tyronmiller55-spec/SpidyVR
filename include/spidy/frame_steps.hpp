#pragma once
// FRAME STEPS: where the VR worker's time goes, step by step (November 17). The freezes the watchdog named
// ("the VR worker has not come round") said nothing of where it was stuck. Each step the worker can wait in is
// marked as it starts and ends: OpenXR's wait frame, begin frame, the eye images' swapchain waits, the GPU copy
// of the eye images, OpenXR's end frame, and the calls into the swing module. The rest is the worker's own code.
// The worker writes the longest time per step every 5 s (a "steps:" line), and the watchdog names the step the
// worker is stuck in and for how long. Times in microseconds of a steady clock; marked by the worker only, read
// by the watchdog from one atomic word.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace spidy::frame_steps {
enum Step : int { worker, waitFrame, beginFrame, eyeImages, gpuCopy, endFrame, swing, count };
// The watchdog's name for a step.
inline const char* name(int step) {
    switch (step) {
    case waitFrame: return "xrWaitFrame";
    case beginFrame: return "xrBeginFrame";
    case eyeImages: return "the eye images' swapchain wait";
    case gpuCopy: return "the GPU copy of the eye images";
    case endFrame: return "xrEndFrame";
    case swing: return "a call into the swing module";
    default: return "its own code (no OpenXR, GPU or swing call)";
    }
}
// The "steps:" line's short name.
inline const char* shortName(int step) {
    switch (step) {
    case waitFrame: return "wait";
    case beginFrame: return "begin";
    case eyeImages: return "images";
    case gpuCopy: return "copy";
    case endFrame: return "end";
    case swing: return "swing";
    default: return "worker";
    }
}
inline uint64_t nowUs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}
class Steps {
  public:
    static constexpr uint64_t windowUs = 5000000;
    static constexpr uint64_t timeMask = (uint64_t{1} << 56) - 1;
    // A step begins (the one before it ends, its time kept).
    void mark(int step, uint64_t now) {
        const uint64_t was = where_.load(std::memory_order_relaxed);
        const int previous = static_cast<int>(was >> 56);
        const uint64_t since = was & timeMask;
        if (since && now >= since && previous >= 0 && previous < count && now - since > longest_[previous])
            longest_[previous] = now - since;
        if (step == waitFrame)
            ++frames_;
        where_.store(static_cast<uint64_t>(step) << 56 | (now & timeMask), std::memory_order_release);
    }
    // The step for a scope: marked on entry, the worker's own code again on exit (an exception too).
    class Scope {
      public:
        Scope(Steps* steps, int step) : steps_(steps) {
            if (steps_)
                steps_->mark(step, nowUs());
        }
        ~Scope() {
            if (steps_)
                steps_->mark(worker, nowUs());
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

      private:
        Steps* steps_;
    };
    // Where the worker is and since when (any thread).
    int current() const {
        return static_cast<int>(where_.load(std::memory_order_acquire) >> 56);
    }
    uint64_t since() const {
        return where_.load(std::memory_order_acquire) & timeMask;
    }
    // Every 5 s, the longest time in each step in that window, into out (the worker's; under 240 characters):
    // "steps: longest in 5.0 s, ms: wait 11.9, begin 0.2, images 0.1, copy 3.4, end 1.9, swing 0.6, worker 4.1;
    // 450 frames". The window starts over. False when not due.
    bool peaks(uint64_t now, char* out, std::size_t size) {
        if (!windowStart_) {
            windowStart_ = now;
            return false;
        }
        if (!out || !size || now < windowStart_ || now - windowStart_ < windowUs)
            return false;
        // The step running now counts up to now.
        const int at = current();
        const uint64_t from = since();
        uint64_t longest[count];
        for (int i = 0; i < count; ++i)
            longest[i] = longest_[i];
        if (at >= 0 && at < count && from && now >= from && now - from > longest[at])
            longest[at] = now - from;
        const auto ms = [&](int step) { return clampMs(longest[step]); };
        std::snprintf(out, size,
                      "steps: longest in %.1f s, ms: wait %.1f, begin %.1f, images %.1f, copy %.1f, end %.1f, "
                      "swing %.1f, worker %.1f; %llu frames",
                      (now - windowStart_) / 1e6, ms(waitFrame), ms(beginFrame), ms(eyeImages), ms(gpuCopy),
                      ms(endFrame), ms(swing), ms(worker), static_cast<unsigned long long>(frames_));
        for (auto& l : longest_)
            l = 0;
        frames_ = 0;
        windowStart_ = now;
        return true;
    }
    // The watchdog's line: the step the worker is in and for how long, when it has been there at least
    // `atLeastUs`. "watchdog: the VR worker is stuck in xrWaitFrame for 6.2 s (free commit 5120 MB)".
    // Again each `repeatUs` at most, whichever watchdog thread asks.
    bool stuck(uint64_t now, uint64_t atLeastUs, uint64_t repeatUs, unsigned long long freeCommitMb, char* out,
               std::size_t size) {
        const uint64_t from = since();
        if (!out || !size || !from || now < from || now - from < atLeastUs)
            return false;
        uint64_t warned = warned_.load();
        if ((warned && now >= warned && now - warned < repeatUs) || !warned_.compare_exchange_strong(warned, now))
            return false;
        std::snprintf(out, size, "watchdog: the VR worker is stuck in %s for %.1f s (free commit %llu MB)",
                      name(current()), (now - from) / 1e6, freeCommitMb);
        return true;
    }
    // Once a session: whether the watchdog takes the worker's stack now.
    bool claimStack() {
        return !stackTaken_.exchange(true);
    }
    // The frame loop is over (the worker stops VR, or failed): no step for the watchdog to name.
    void idle() {
        where_ = 0;
    }
    // A new session (any thread, before the worker starts).
    void reset() {
        where_ = 0;
        for (auto& l : longest_)
            l = 0;
        frames_ = 0;
        windowStart_ = 0;
        stackTaken_ = false;
        warned_ = 0;
    }

  private:
    static double clampMs(uint64_t us) {
        return us > 99999900 ? 99999.9 : us / 1000.0;
    }
    std::atomic<uint64_t> where_{};   // the step (top 8 bits) and since when
    uint64_t longest_[count]{};       // the worker's own
    uint64_t frames_{}, windowStart_{}; // the worker's own
    std::atomic<bool> stackTaken_{};
    std::atomic<uint64_t> warned_{};
};
} // namespace spidy::frame_steps
