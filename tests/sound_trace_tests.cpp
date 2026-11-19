// SOUND TRACE (sound_trace.hpp, November 12): the lock-free ring (written,
// read back in a window, torn and overwritten entries skipped), the event
// lines (hero ids first with ms offsets, nearby others, caps), the wait for a
// window to close, and the setting. Portable.
#include "spidy/hit_sound.hpp"
#include "spidy/sound_guard.hpp"
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
        check(ring->magic == 0x53535443 && ring->version == 1 && ring->bytes == sizeof(Ring), "header");
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
    // The setting: off by default since the beta (D7, movement_defaults 13: an older file's sound_trace=1 turns off once).
    {
        using namespace vr_settings;
        Values v;
        check(!v.soundTrace, "default off (beta)");
        const std::string old = "sound_trace=1\nmovement_defaults=12\n";
        check(!withMovementDefaults(withTabOnly({}, old), old).soundTrace, "revision 13: an old file's trace turns off");
        const std::string chosen = "sound_trace=1\nmovement_defaults=13\n";
        check(withMovementDefaults(withTabOnly({}, chosen), chosen).soundTrace, "on chosen after revision 13 stays on");
        v.soundTrace = true;
        const Row* row = rowFor(Item::soundTrace);
        check(row && std::strstr(row->help, "Changes nothing") != nullptr, "help: diagnostics only");
        check(choose(Item::soundTrace, 0, v) && !v.soundTrace, "OFF");
        check(tabOnlyText(v).find("sound_trace=0") != std::string::npos, "saved as sound_trace");
        check(withTabOnly(Values{}, "sound_trace=0\n").soundTrace == false, "read back off");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::nativeAnimation)].items)
            listed = listed || item == Item::soundTrace;
        check(listed, "on NA CORE");
    }
    // HERO CUE GUARD (sound_guard.hpp, A4/D6): the game's grunt within 300 ms skips our guarded jump set.
    {
        using namespace sound_guard;
        const int64_t hz = 10'000'000;
        check(!nativeSounding(0, hz, hz), "no grunt yet: ours plays");
        check(nativeSounding(hz, hz + hz / 10, hz), "grunt 100 ms ago: skipped");
        check(nativeSounding(hz, hz + hz * 299 / 1000, hz), "299 ms: skipped");
        check(!nativeSounding(hz, hz + hz * 300 / 1000, hz), "300 ms: ours plays");
        check(!nativeSounding(2 * hz, hz, hz), "a grunt stamped after now: ours plays");
        check(!nativeSounding(hz, hz, 0), "no frequency: ours plays");
        check(encode(jumpGrunt, true) == (0xdab33491ull | (1ull << 32)) && encode(jumpGrunt, false) == 0xdab33491ull,
              "SpidySoundCue: the id low, the guard bit 32");
        check(static_cast<uint32_t>(encode(jumpSet[2], true)) == 0xeedb2021u, "the id survives the guard bit");
    }
    // JUMP SOUNDS and WALL JUMP SOUND: on by default, POINT JUMP's tab, saved and read back.
    {
        using namespace vr_settings;
        Values v;
        check(v.pointJumpSound && v.wallJumpSound, "jump sounds on by default");
        using sound_guard::wallJumpId;
        bool jump = false, wall = false;
        for (Item item : screens()[static_cast<size_t>(Category::pointJump)].items) {
            jump = jump || item == Item::pointJumpSound;
            wall = wall || item == Item::wallJumpSound;
        }
        check(jump && wall, "on POINT JUMP's tab");
        check(isNativeAnimation(Item::pointJumpSound) && isNativeAnimation(Item::wallJumpSound), "NA settings");
        check(choose(Item::pointJumpSound, 0, v) && !v.pointJumpSound, "JUMP SOUNDS OFF");
        check(v.wallJumpSound == 1 && wallJumpId(v.wallJumpSound) == 0xd321a6f9u, "wall jump: d321a6f9 by default");
        check(choose(Item::wallJumpSound, 2, v) && wallJumpId(v.wallJumpSound) == 0x110d202bu, "SOUND B: 110d202b");
        check(wallJumpId(3) == 0 && wallJumpId(-1) == 0, "no third candidate (not 700c7633, a landing sound)");
        check(withTabOnly(Values{}, "wall_jump_sound=7\n").wallJumpSound == 2, "read back clamped");
        check(choose(Item::wallJumpSound, 0, v) && !v.wallJumpSound, "WALL JUMP SOUND OFF");
        const std::string text = tabOnlyText(v);
        check(text.find("point_jump_sound=0") != std::string::npos && text.find("wall_jump_sound=0") != std::string::npos,
              "saved");
        const Values back = withTabOnly(Values{}, text);
        check(!back.pointJumpSound && !back.wallJumpSound, "read back");
        check(!inactiveReason(Item::wallJumpSound, Values{}), "wall jump sound needs no point jump");
    }
    // LOUDER HITS (hit_sound.hpp, A1/A3, D5): a jab Medium, a full swing Heavy, a kick Heavy, a flying kick and a
    // finisher Extreme; never Light. HERO ATTACK SOUND: one a hand every 250 ms, off by default.
    {
        using namespace hit_sound;
        check(intensity({false, false, false, false, .1f}) == 2, "a soft jab: Medium (was Light)");
        check(intensity({false, false, false, false, .59f}) == 2, "a firm jab: Medium");
        check(intensity({false, false, false, false, .6f}) == 3, "a full swing (0.6): Heavy");
        check(intensity({false, true, false, false, .1f}) == 3, "a special blow: Heavy");
        check(intensity({false, false, true, false, .1f}) == 3, "a kick: at least Heavy");
        check(intensity({false, false, true, true, .1f}) == 4, "a kick that flies him: Extreme");
        check(intensity({true, false, false, false, 0.f}) == 4, "a finisher: Extreme");
        const int64_t hz = 1000;
        check(attackDue(0, 5, hz) && !attackDue(100, 349, hz) && attackDue(100, 350, hz), "one a hand every 250 ms");
        check(vr_settings::Values{}.attackSound == 0, "HERO ATTACK SOUND: none by default (no id guessed)");
        check(vr_settings::withTabOnly({}, "attack_sound=-625265519\n").attackSound == -625265519,
              "an id past 0x7fffffff reads as its signed 32 bits");
    }
    std::printf("sound trace: %d checks passed\n", passed);
    return 0;
}
