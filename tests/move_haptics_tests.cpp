// MOVE HAPTICS (move_haptics.hpp, November 12): takeoffs (jump, wall jump, zip
// launch), air zip launches, landings scaled by fall speed with step-downs
// skipped, the point jump's double thump, the 0-200% scale with its own
// strength, and the setting. Portable.
#include "spidy/move_haptics.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace spidy;
using namespace spidy::move_haptics;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
}
constexpr float dt = 1.f / 90;

// A small world: a frame at a time, the events seen.
struct Sim {
    Detector d;
    std::vector<Event> events;
    Input in;
    Sim() {
        in.dt = dt;
        in.live = in.fresh = true;
    }
    void frames(int n, bool airborne, Vec3 v, bool onWall = false, bool zip = false) {
        for (int i = 0; i < n; ++i) {
            in.airborne = airborne;
            in.onWall = onWall;
            in.velocity = v;
            in.zipPressed = zip && i == 0;
            const auto e = d.update(in);
            if (e.kind != Kind::none)
                events.push_back(e);
        }
    }
    size_t count(Kind k) const {
        size_t n = 0;
        for (const auto& e : events)
            n += e.kind == k;
        return n;
    }
};
} // namespace

int main() {
    // The curves.
    {
        check(near(launchStrength(0), .35f) && near(launchStrength(25), 1) && near(launchStrength(100), 1),
              "launch strength 0.35..1 over 0..25 m/s");
        check(launchStrength(5) < launchStrength(15), "a faster launch is stronger");
        check(near(landingStrength(4), .2f) && near(landingStrength(25), 1) && near(landingStrength(14.5f), .6f),
              "landing strength 0.2..1 over 4..25 m/s");
        check(near(landingSeconds(0), .04f) && near(landingSeconds(1), .16f), "landing length 40..160 ms");
        check(near(launchSeconds(0), .06f) && near(launchSeconds(1), .14f), "launch length 60..140 ms");
        check(!std::isnan(launchStrength(NAN)) && !std::isnan(landingStrength(NAN)), "NaN speeds give finite strengths");
    }
    // The scale: 100% as is, 50% half, 0 none, 200% caps at 1 and lengthens up to 1.5x.
    {
        const Pulse p{-1, .6f, .1f};
        check(near(scaled(p, 1).amplitude, .6f) && near(scaled(p, 1).seconds, .1f), "100%");
        check(near(scaled(p, .5f).amplitude, .3f), "50%");
        check(scaled(p, 0).amplitude == 0 && scaled(p, NAN).amplitude == 0, "OFF / NaN: none");
        const Pulse big = scaled({-1, .9f, .1f}, 2);
        check(near(big.amplitude, 1) && near(big.seconds, .15f), "200%: amplitude 1, 1.5x length");
        const Pulse some = scaled({-1, .6f, .1f}, 2);
        check(near(some.amplitude, 1) && near(some.seconds, .12f), "200% of 0.6: 1.2x length");
        check(scaled({-1, 1, .45f}, 2).seconds <= .5f, "never past the runtime's 0.5 s");
    }
    // A game jump and its landing.
    {
        Sim s;
        s.frames(30, false, {0, 0, 0});
        s.frames(1, true, {2, 5, 0});
        check(s.events.size() == 1 && s.events[0].kind == Kind::jump, "jump on the takeoff");
        check(near(s.events[0].pulse.amplitude, launchStrength(std::hypot(2.f, 5.f))), "scaled by launch speed");
        check(s.events[0].pulse.hand == -1, "both hands");
        s.frames(60, true, {2, -10, 0});
        s.frames(1, false, {0, 0, 0});
        check(s.events.size() == 2 && s.events[1].kind == Kind::landing, "landing");
        check(near(s.events[1].speed, 10) && near(s.events[1].pulse.amplitude, landingStrength(10)),
              "landing by the fall speed just before");
    }
    // A harder fall is a harder landing.
    {
        Sim soft, hard;
        for (Sim* s : {&soft, &hard}) {
            s->frames(10, false, {});
            s->frames(1, true, {0, 4, 0});
        }
        soft.frames(60, true, {0, -6, 0});
        hard.frames(60, true, {0, -20, 0});
        soft.frames(1, false, {});
        hard.frames(1, false, {});
        check(soft.count(Kind::landing) == 1 && hard.count(Kind::landing) == 1, "both land");
        check(hard.events.back().pulse.amplitude > soft.events.back().pulse.amplitude, "harder fall, stronger");
    }
    // Step-downs: too short in the air, or too slow coming down: none.
    {
        Sim s;
        s.frames(10, false, {});
        s.frames(15, true, {3, -4, 0}); // 0.17 s
        s.frames(10, false, {});
        check(s.events.empty(), "a step-down (0.17 s in the air) is no landing");
        s.frames(60, true, {0, -2, 0});
        s.frames(10, false, {});
        check(s.events.empty(), "coming down at 2 m/s is no landing");
        s.frames(60, true, {0, -1, 0});
        check(s.count(Kind::jump) == 0, "walking off a ledge is no jump");
    }
    // Wall jump, and a landing on a wall at 0.7x.
    {
        Sim s;
        s.frames(30, false, {}, true);
        s.frames(1, true, {6, 1, 0});
        check(s.count(Kind::wallJump) == 1, "wall jump off a wall at 6 m/s");
        Sim slow;
        slow.frames(30, false, {}, true);
        slow.frames(1, true, {2, 0, 0});
        check(slow.events.empty(), "dropping off a wall slowly is no jump");
        Sim w;
        w.frames(10, false, {});
        w.frames(1, true, {0, 4, 0});
        w.frames(60, true, {0, -10, 0});
        w.frames(1, true, {0, -10, 0}, true);
        check(w.count(Kind::wallLanding) == 1 && near(w.events.back().pulse.amplitude, landingStrength(10) * .7f),
              "landing on a wall: 0.7x");
    }
    // Zip launch: from the ground with a zip pressed within 3 s; and in the air once.
    {
        Sim s;
        s.frames(5, false, {}, false, true);
        s.frames(1, true, {0, 8, 4});
        check(s.count(Kind::zipLaunch) == 1, "zip launch from the ground");
        Sim late;
        late.frames(1, false, {}, false, true);
        late.frames(300, false, {});
        late.frames(1, true, {0, 8, 4});
        check(late.count(Kind::jump) == 1 && late.count(Kind::zipLaunch) == 0, "a zip 3.3 s ago: a plain jump");
        Sim air;
        air.frames(10, false, {});
        air.frames(60, true, {0, -2, 0}); // walked off: no event
        air.frames(1, true, {0, -2, 0}, false, true);
        for (int i = 0; i < 10; ++i)
            air.frames(1, true, {0, -2.f + i * 1.2f, 0});
        air.frames(20, true, {0, 9, 0});
        check(air.count(Kind::zipLaunch) == 1, "an air zip: up-speed rising 6 m/s after a zip press, once");
        Sim webbed;
        webbed.in.webbed = true;
        webbed.frames(5, false, {}, false, true);
        webbed.frames(1, true, {0, 8, 0});
        check(webbed.events.empty(), "a web's yank is no jump");
    }
    // The up-speed may build over the first frames in the air.
    {
        Sim s;
        s.frames(10, false, {});
        s.frames(1, true, {0, 1, 0});
        s.frames(1, true, {0, 4, 0});
        check(s.count(Kind::jump) == 1, "a jump classified as its speed builds (0.15 s)");
        Sim late;
        late.frames(10, false, {});
        late.frames(20, true, {0, 1, 0});
        late.frames(1, true, {0, 4, 0});
        check(late.events.empty(), "not after 0.15 s");
    }
    // A point jump: its own double thump, takeoffs within 0.4 s are its own.
    {
        Sim s;
        s.frames(10, false, {});
        s.d.pointJump();
        s.frames(1, true, {0, 12, 6});
        check(s.events.empty(), "the point jump's takeoff is not a second jump");
        check(near(pointFirst().amplitude, 1) && near(pointFirst().seconds, .07f) && near(pointSecond().amplitude, .85f) &&
                  near(pointSecond().seconds, .1f) && near(pointGap, .11f),
              "double thump 1.0/70 ms, 110 ms, 0.85/100 ms");
        Scheduler sched;
        std::vector<float> at;
        float clock = 0;
        sched.add(pointGap, pointSecond());
        for (int i = 0; i < 30; ++i) {
            clock += dt;
            sched.update(dt, [&](const Pulse&) { at.push_back(clock); });
        }
        check(at.size() == 1 && at[0] >= pointGap - 1e-4f && at[0] < pointGap + dt + 1e-4f, "second thump on time, once");
    }
    // Quiet outside VR play and across stale data; no edges from a gap.
    {
        Sim s;
        s.in.live = false;
        s.frames(10, false, {});
        s.frames(10, true, {0, 8, 0});
        check(s.events.empty(), "not live: nothing");
        Sim n;
        n.frames(10, false, {});
        n.frames(1, true, {NAN, 8, 0});
        n.frames(1, true, {0, 8, 0});
        check(n.events.empty(), "NaN velocity: no edge across it");
    }
    // A flicker of the airborne flag right after a jump is not another jump.
    {
        Sim s;
        s.frames(10, false, {});
        s.frames(1, true, {0, 5, 0});
        s.frames(1, false, {0, 5, 0});
        s.frames(1, true, {0, 5, 0});
        check(s.count(Kind::jump) == 1, "one jump through a flicker");
    }
    // The setting: 100% by default, 0..200 in 25s, on FEEDBACK, a personal key.
    {
        using namespace vr_settings;
        Values v;
        check(v.moveHapticsPct == 100, "default 100%");
        const Row* row = rowFor(Item::moveHaptics);
        check(row && row->choices.size() == 9 && std::strcmp(row->choices[0], "OFF") == 0 &&
                  std::strcmp(row->choices[8], "200%") == 0,
              "choices OFF..200%");
        check(row && std::strstr(row->help, "CONTROLLER VIBRATION") != nullptr, "help says vibration doesn't scale it");
        check(choose(Item::moveHaptics, 8, v) && v.moveHapticsPct == 200, "200%");
        check(choose(Item::moveHaptics, 0, v) && v.moveHapticsPct == 0, "OFF");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::feedback)].items)
            listed = listed || item == Item::moveHaptics;
        check(listed, "on FEEDBACK");
        Values w;
        w.moveHapticsPct = 150;
        check(tabOnlyText(w).find("move_haptics_pct=150") != std::string::npos, "saved as move_haptics_pct");
        check(withTabOnly(Values{}, "move_haptics_pct=175\n").moveHapticsPct == 175, "read back");
        check(withTabOnly(Values{}, "move_haptics_pct=900\n").moveHapticsPct <= 200, "sanitized to 200");
        check(inactiveReason(Item::moveHaptics, Values{}) == nullptr, "always available");
    }
    std::printf("move haptics: %d checks passed\n", passed);
    return 0;
}
