// SOUND TRACE (sound_trace.hpp, November 12): the lock-free ring (written,
// read back in a window, torn and overwritten entries skipped), the event
// lines (hero ids first with ms offsets, nearby others, caps), the wait for a
// window to close, and the setting. Portable.
#include "spidy/sound_trace.hpp"
#include "spidy/vr_settings.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace spidy;
using namespace spidy::sound_trace;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr int64_t hz = 10'000'000; // QPC ticks a second
int64_t ms(int v) {
    return static_cast<int64_t>(v) * hz / 1000;
}
bool has(const std::string& s, const char* part) {
    return s.find(part) != std::string::npos;
}
} // namespace

int main() {
    // The ring: written in order, collected in a window, oldest first.
    {
        auto ring = std::make_unique<Ring>();
        check(ring->magic == 0x53535452 && ring->version == 1 && ring->bytes == sizeof(Ring), "header");
        for (int i = 0; i < 10; ++i)
            record(*ring, ms(100 * i), 0x1000 + i, 7, 0);
        Hit hits[16];
        const size_t n = collect(*ring, ms(250), ms(650), hits);
        check(n == 4 && hits[0].id == 0x1003 && hits[3].id == 0x1006, "window 250..650 ms: 4 entries in order");
        Hit few[2];
        check(collect(*ring, 0, ms(1000), few) == 2, "capped by the buffer");
        // A torn entry (being written) is skipped.
        ring->entries[5].seq.store(0);
        check(collect(*ring, ms(250), ms(650), hits) == 3, "torn entry skipped");
    }
    // Wrap-around: only the newest `capacity` are there.
    {
        auto ring = std::make_unique<Ring>();
        const uint32_t total = Ring::capacity + 100;
        for (uint32_t i = 0; i < total; ++i)
            record(*ring, i, i, 0, 0);
        static Hit hits[Ring::capacity + 10];
        const size_t n = collect(*ring, 0, total, hits);
        check(n == Ring::capacity && hits[0].id == 100 && hits[n - 1].id == total - 1, "the newest capacity entries");
    }
    // An event's line: hero ids first with offsets, others near the event.
    {
        auto ring = std::make_unique<Ring>();
        ring->heroHandle = 42;
        const int64_t t = ms(10'000);
        record(*ring, t - ms(800), 0xdead0001, 42, flagHero);            // before the window
        record(*ring, t - ms(120), 0xaaaa0001, 42, flagHero);            // footstep before
        record(*ring, t + ms(40), 0xbbbb0002, 42, flagHero);             // the jump's
        record(*ring, t + ms(60), 0xcccc0003, 42, flagHero | flagOurs);  // Spidy's own
        record(*ring, t + ms(10), 0x11110001, 99, 0);                   // someone else, near
        record(*ring, t + ms(500), 0x22220002, 99, 0);                  // someone else, far
        record(*ring, t + ms(1200), 0xdead0002, 42, flagHero);          // after the window
        Pending p{t, "jump 6.0 m/s"};
        const std::string line = Watch::format(*ring, p, hz);
        check(has(line, "sound trace: jump 6.0 m/s"), "names the event");
        check(has(line, "aaaa0001@-120ms") && has(line, "bbbb0002@+40ms") && has(line, "cccc0003@+60ms(spidy)"),
              "hero ids with offsets, Spidy's own marked");
        check(!has(line, "dead0001") && !has(line, "dead0002"), "outside -0.5..+1.0 s: left out");
        check(has(line, "11110001/h63@+10ms") && !has(line, "22220002"), "others only within 150 ms");
        check(line.find("aaaa0001") < line.find("11110001"), "hero first");
        check(has(line, "| 5 sounds |"), "how many sounds in the window");
    }
    // Caps: at most 12 hero ids, the nearest the event.
    {
        auto ring = std::make_unique<Ring>();
        const int64_t t = ms(10'000);
        for (int i = 0; i < 30; ++i)
            record(*ring, t - ms(400) + ms(40 * i), 0x5000 + i, 1, flagHero);
        const std::string line = Watch::format(*ring, {t, "landing"}, hz);
        check(has(line, "(+18)"), "the rest counted");
        check(has(line, "0000500a@+0ms") && !has(line, "00005000@"), "the nearest kept");
        const std::string none = Watch::format(*ring, {ms(100'000), "jump"}, hz);
        check(has(none, "hero: none") && has(none, "near: none"), "nothing heard: none");
    }
    // The watch: a line only once the +1.0 s has passed; none without a ring.
    {
        auto ring = std::make_unique<Ring>();
        Watch w;
        std::vector<std::string> lines;
        w.add(ms(1000), "point jump");
        w.update(ring.get(), ms(1900), hz, [&](const std::string& l) { lines.push_back(l); });
        check(lines.empty(), "not before +1.0 s");
        w.update(ring.get(), ms(2000), hz, [&](const std::string& l) { lines.push_back(l); });
        check(lines.size() == 1 && has(lines[0], "point jump") && w.count == 0, "then one line");
        for (int i = 0; i < 12; ++i)
            w.add(ms(5000), "jump");
        check(w.count == 8 && w.dropped == 4, "at most 8 waiting");
        w.update(nullptr, ms(9000), hz, [&](const std::string& l) { lines.push_back(l); });
        check(lines.size() == 1 && w.count == 0, "no ring: no lines, nothing left waiting");
        w.add(0, "x");
        w.update(ring.get(), ms(9000), 0, [&](const std::string& l) { lines.push_back(l); });
        check(lines.size() == 1, "no frequency: nothing");
    }
    // The setting: on by default (diagnostics), off with sound_trace=0.
    {
        using namespace vr_settings;
        Values v;
        check(v.soundTrace, "default on");
        const Row* row = rowFor(Item::soundTrace);
        check(row && std::strstr(row->help, "Changes nothing") != nullptr, "help: diagnostics only");
        check(choose(Item::soundTrace, 0, v) && !v.soundTrace, "OFF");
        check(tabOnlyText(v).find("sound_trace=0") != std::string::npos, "saved as sound_trace");
        check(withTabOnly(Values{}, "sound_trace=0\n").soundTrace == false, "read back off");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::nativeAnimation)].items)
            listed = listed || item == Item::soundTrace;
        check(listed, "on NATIVE ANIMATION");
    }
    std::printf("sound trace: %d checks passed\n", passed);
    return 0;
}
