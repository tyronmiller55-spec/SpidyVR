#pragma once
// EYE STALL WATCH (beta polish R1). The VR worker ends VR when no eye pair has been shown for 3 s. A module start on
// that same worker (native swinging took 3682 ms at 2:17 AM on Nov 19, under memory pressure in a cutscene) shows no
// pair while it runs, so the next check saw a 3.7 s gap and stopped a healthy session. A start that finished after the
// last shown pair restarts the count from its end, as long as the start itself stayed under the start limit (10 s);
// a start that ran longer still ends VR. Portable.
#include <cstdint>

namespace spidy::eye_stall {
inline constexpr uint64_t stallMs = 3000;
inline constexpr uint64_t startLimitMs = 10000;

enum class Verdict { fine, stalled, forgivenStart };

struct Watch {
    uint64_t startBeganMs{}, startEndedMs{};
    // A module start on the eye worker: call begin before it and end after.
    void begin(uint64_t now) {
        startBeganMs = now;
        startEndedMs = 0;
    }
    void end(uint64_t now) { startEndedMs = now; }
    bool starting() const { return startBeganMs && !startEndedMs; }
    uint64_t lastStartMs() const { return startEndedMs > startBeganMs ? startEndedMs - startBeganMs : 0; }
};

// lastPresentedMs 0: nothing shown yet (the first-image deadline covers that).
inline Verdict judge(uint64_t now, uint64_t lastPresentedMs, const Watch& w) {
    if (!lastPresentedMs || now < lastPresentedMs || now - lastPresentedMs < stallMs)
        return Verdict::fine;
    if (w.starting())
        return now - w.startBeganMs < startLimitMs ? Verdict::forgivenStart : Verdict::stalled;
    if (w.startEndedMs > lastPresentedMs && w.lastStartMs() < startLimitMs) {
        if (now - w.startEndedMs < stallMs)
            return Verdict::forgivenStart;
    }
    return Verdict::stalled;
}
} // namespace spidy::eye_stall
