// SOUND TRACE (the user, November 12 05:07, t70u: "yes to the sound trace").
// Diagnostics only, for finding the game's own jump / landing / "umpf" sounds
// before anything plays them: every call of the game's sound call (16d5e80,
// game_sound.hpp: the system, the event's id, the emitter's handle, ...) is
// recorded - its id, the handle it plays on, when (QPC), whether that handle
// is the hero's and whether Spidy itself asked for it - into a lock-free ring
// exported by the ray module (SpidySoundTrace). Nothing is changed: the call
// goes on to the game with every argument as it came.
//
// The headset module reads the ring around its own events (takeoffs, landings,
// point jumps; move_haptics.hpp) from 0.5 s before to 1.0 s after, and logs one
// line per event: the hero's sound ids first with their offsets in ms, then the
// other handles' ids close to the event (within 150 ms), capped. Off with the
// SOUND TRACE setting (ini sound_trace=0): the hook is disabled.
//
// No address here is new: the hook sits on the sound call game_sound already
// uses (its entry bytes checked first) and the hero's handle is made as
// game_sound makes it. Pure (portable) apart from the ray module's hook.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>

namespace spidy::sound_trace {

inline constexpr uint32_t flagHero = 1, flagOurs = 2;

struct Entry {
    std::atomic<uint32_t> seq{}; // index + 1 once written; 0 while being written
    std::atomic<uint32_t> id{}, handle{}, flags{};
    std::atomic<int64_t> qpc{};
};

// SpidySoundTrace, exported by the ray module.
struct Ring {
    static constexpr uint32_t capacity = 2048;
    uint32_t magic = 0x53535443, version = 1, bytes = sizeof(Ring); // "SSTC"
    std::atomic<uint32_t> status{}; // 0 off, 1 tracing, 3 failed (error)
    std::atomic<uint32_t> error{};  // 9801 another build; 9810+ MH_Initialize, 9830+ MH_CreateHook, 9850+ MH_EnableHook
    std::atomic<uint32_t> heroHandle{};
    std::atomic<uint64_t> written{};
    Entry entries[capacity];
};

// The writer (any thread, the game's sound call): lock-free.
inline void record(Ring& r, int64_t qpc, uint32_t id, uint32_t handle, uint32_t flags) {
    const uint64_t n = r.written.fetch_add(1, std::memory_order_relaxed);
    Entry& e = r.entries[n % Ring::capacity];
    e.seq.store(0, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    e.id.store(id, std::memory_order_relaxed);
    e.handle.store(handle, std::memory_order_relaxed);
    e.flags.store(flags, std::memory_order_relaxed);
    e.qpc.store(qpc, std::memory_order_relaxed);
    e.seq.store(static_cast<uint32_t>(n + 1), std::memory_order_release);
}

struct Hit {
    int64_t qpc{};
    uint32_t id{}, handle{}, flags{};
};

// The ring's entries with from <= qpc <= to that `keep`, oldest first; up to
// N. Torn entries (written over while read) are skipped. `seen` counts every
// entry in the window, kept or not.
template <size_t N, class Keep>
size_t collect(const Ring& r, int64_t from, int64_t to, Hit (&out)[N], Keep&& keep, size_t* seen = nullptr) {
    const uint64_t written = r.written.load(std::memory_order_acquire);
    const uint64_t first = written > Ring::capacity ? written - Ring::capacity : 0;
    size_t count = 0;
    for (uint64_t n = first; n < written && count < N; ++n) {
        const Entry& e = r.entries[n % Ring::capacity];
        const uint32_t want = static_cast<uint32_t>(n + 1);
        if (e.seq.load(std::memory_order_acquire) != want)
            continue;
        Hit h{e.qpc.load(std::memory_order_relaxed), e.id.load(std::memory_order_relaxed),
              e.handle.load(std::memory_order_relaxed), e.flags.load(std::memory_order_relaxed)};
        std::atomic_thread_fence(std::memory_order_acquire);
        if (e.seq.load(std::memory_order_relaxed) != want)
            continue;
        if (h.qpc < from || h.qpc > to)
            continue;
        if (seen)
            ++*seen;
        if (keep(h))
            out[count++] = h;
    }
    std::sort(out, out + count, [](const Hit& a, const Hit& b) { return a.qpc < b.qpc; });
    return count;
}
template <size_t N> size_t collect(const Ring& r, int64_t from, int64_t to, Hit (&out)[N]) {
    return collect(r, from, to, out, [](const Hit&) { return true; });
}

inline constexpr float before = .5f, after = 1.f, nearOthers = .15f;
inline constexpr size_t heroMost = 12, othersMost = 4;

// One event waiting for its window to close.
struct Pending {
    int64_t qpc{};
    char what[48]{};
};

// Events waiting for their +1.0 s; a few at most.
struct Watch {
    Pending items[8]{};
    size_t count = 0;
    uint64_t dropped = 0, lines = 0;
    void add(int64_t qpc, const char* what) {
        if (count >= std::size(items)) {
            ++dropped;
            return;
        }
        Pending& p = items[count++];
        p.qpc = qpc;
        std::snprintf(p.what, sizeof(p.what), "%s", what ? what : "event");
    }
    // Lines for every event whose window has closed by `now`.
    template <class Line> void update(const Ring* ring, int64_t now, int64_t frequency, Line&& line) {
        if (frequency <= 0)
            return;
        for (size_t k = 0; k < count;) {
            const Pending p = items[k];
            if (now < p.qpc + static_cast<int64_t>(after * frequency)) {
                ++k;
                continue;
            }
            items[k] = items[--count];
            if (!ring)
                continue;
            ++lines;
            line(format(*ring, p, frequency));
        }
    }
    void clear() {
        count = 0;
    }

    static std::string format(const Ring& ring, const Pending& p, int64_t frequency) {
        Hit heroHits[64], nearHits[othersMost + 1];
        const int64_t from = p.qpc - static_cast<int64_t>(before * frequency),
                      to = p.qpc + static_cast<int64_t>(after * frequency);
        const int64_t nearTicks = static_cast<int64_t>(nearOthers * frequency);
        size_t seen = 0, seenAgain = 0;
        const size_t heroes =
            collect(ring, from, to, heroHits, [](const Hit& h) { return (h.flags & flagHero) != 0; }, &seen);
        const size_t nears = collect(
            ring, p.qpc - nearTicks, p.qpc + nearTicks, nearHits,
            [](const Hit& h) { return (h.flags & flagHero) == 0; }, &seenAgain);
        const auto ms = [&](const Hit& h) {
            return static_cast<int>((h.qpc - p.qpc) * 1000 / frequency);
        };
        // The hero's nearest the event, shown in time order.
        size_t shown = std::min(heroes, heroMost);
        if (heroes > heroMost) {
            std::partial_sort(heroHits, heroHits + heroMost, heroHits + heroes, [&](const Hit& a, const Hit& b) {
                return std::llabs(a.qpc - p.qpc) < std::llabs(b.qpc - p.qpc);
            });
            std::sort(heroHits, heroHits + heroMost, [](const Hit& a, const Hit& b) { return a.qpc < b.qpc; });
        }
        std::string text = "sound trace: ";
        text += p.what;
        char buf[64];
        std::snprintf(buf, sizeof(buf), " | %zu sounds | hero:", seen);
        text += buf;
        for (size_t i = 0; i < shown; ++i) {
            const Hit& h = heroHits[i];
            std::snprintf(buf, sizeof(buf), " %08x@%+dms%s", h.id, ms(h), h.flags & flagOurs ? "(spidy)" : "");
            text += buf;
        }
        if (!heroes)
            text += " none";
        if (heroes > heroMost) {
            std::snprintf(buf, sizeof(buf), " (+%zu%s)", heroes - heroMost, heroes >= std::size(heroHits) ? "+" : "");
            text += buf;
        }
        text += " | near:";
        for (size_t i = 0; i < std::min(nears, othersMost); ++i) {
            const Hit& h = nearHits[i];
            std::snprintf(buf, sizeof(buf), " %08x/h%x@%+dms%s", h.id, h.handle, ms(h),
                          h.flags & flagOurs ? "(spidy)" : "");
            text += buf;
        }
        if (!nears)
            text += " none";
        if (nears > othersMost)
            text += " ...";
        return text;
    }
};

} // namespace spidy::sound_trace
