// PUNCH AUTO-TARGET (punch_track.hpp, the polish pass slice 7): a throw is judged against the TARGETED enemy only, in a cone
// and a range; nobody targeted is no tracking; the settings' defaults and ranges. Portable.
#include "spidy/facing_check.hpp"
#include "spidy/game_punch.hpp"
#include "spidy/punch_track.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace spidy;
using namespace spidy::punch_track;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
} // namespace

int main() {
    const Vec3 head{0, 1.6f, 0};
    Config on;
    on.on = true;
    // The targeted enemy 3 m ahead (-z), punch straight at him.
    {
        const auto v = decide(on, true, head, {0, 0, -5}, {0, 1.0f, -3.f});
        check(v.why == Why::tracked && v.distance > 3.f && v.angleDeg < 15.f, "a punch at the targeted enemy tracks");
        check(std::fabs(length(v.toward) - 1.f) < 1e-3f, "toward is a unit line");
        check(v.toward.z < -.9f, "toward points at him");
    }
    // Off: nothing, whoever is there.
    check(decide(Config{}, true, head, {0, 0, -5}, {0, 1, -3}).why == Why::off, "auto-target off tracks nobody");
    // Nobody targeted: no tracking, however close an enemy is (the punch still does its damage; this is only the turn).
    check(decide(on, false, head, {0, 0, -5}, {0, 1, -1}).why == Why::noTarget, "nobody targeted: no tracking");
    // The cone: 45 degrees by default: 30 off is in, 60 off is out; the cone setting moves it.
    {
        const Vec3 enemy{0, 1.6f, -3.f};
        const float c30 = std::cos(30 * degrees), s30 = std::sin(30 * degrees);
        const float c60 = std::cos(60 * degrees), s60 = std::sin(60 * degrees);
        check(decide(on, true, head, {5 * s30, 0, -5 * c30}, enemy).why == Why::tracked, "30 deg off is in the 45 deg cone");
        const auto out = decide(on, true, head, {5 * s60, 0, -5 * c60}, enemy);
        check(out.why == Why::outsideCone && std::fabs(out.angleDeg - 60.f) < 1.f, "60 deg off is outside it");
        check(out.toward.x == 0 && out.toward.y == 0 && out.toward.z == 0, "no line when not tracked");
        Config wide = on;
        wide.coneDeg = 90.f;
        check(decide(wide, true, head, {5 * s60, 0, -5 * c60}, enemy).why == Why::tracked, "a 90 deg cone takes it");
        Config narrow = on;
        narrow.coneDeg = 15.f;
        check(decide(narrow, true, head, {5 * s30, 0, -5 * c30}, enemy).why == Why::outsideCone, "a 15 deg cone does not");
        // a punch the other way (away from him) is not toward him
        check(decide(on, true, head, {0, 0, 5}, enemy).why == Why::outsideCone, "a punch away from him is outside the cone");
    }
    // The range: 6 m by default.
    {
        check(decide(on, true, head, {0, 0, -5}, {0, 1.6f, -5.5f}).why == Why::tracked, "5.5 m is in range");
        const auto far = decide(on, true, head, {0, 0, -5}, {0, 1.6f, -7.f});
        check(far.why == Why::tooFar && std::fabs(far.distance - 7.f) < .01f, "7 m is too far");
        Config near3 = on;
        near3.rangeM = 3.f;
        check(decide(near3, true, head, {0, 0, -5}, {0, 1.6f, -4.f}).why == Why::tooFar, "a 3 m range stops at 3 m");
    }
    // Degenerate input is no direction.
    check(decide(on, true, head, {0, 0, 0}, {0, 1, -3}).why == Why::noDirection, "a still fist has no direction");
    check(decide(on, true, head, {0, 0, -5}, head).why == Why::noDirection, "an enemy on the head has none");
    check(decide(on, true, head, {NAN, 0, -5}, {0, 1, -3}).why == Why::noDirection, "NaN has none");
    // Throws: once per swing, from rest; a busy hand throws nothing.
    {
        Throws t;
        const float dt = 1.f / 90.f;
        check(!t.update(dt, 0.f, true) && !t.update(dt, 1.f, true) && !t.update(dt, 2.f, true), "under minSpeed is no throw");
        check(t.update(dt, 3.f, true), "a fist over minSpeed from rest is a throw");
        check(!t.update(dt, 6.f, true) && !t.update(dt, 7.f, true), "the same swing throws once");
        check(!t.update(dt, 1.f, true), "slowing re-arms");
        check(t.update(dt, 3.f, true), "and the next swing throws");
        // after rearmTime a still-fast fist throws again
        Throws held;
        check(held.update(dt, 5.f, true), "first");
        bool again = false;
        for (float s = 0; s < .6f; s += dt)
            again = again || held.update(dt, 5.f, true);
        check(again, "after rearmTime a fist still going is a new throw");
        Throws busy;
        check(!busy.update(dt, 8.f, false), "a busy hand (a web, a grab, a wall) throws nothing");
        check(!busy.update(dt, 8.f, true), "and is not armed by the busy swing it came out of");
        check(!busy.update(dt, NAN, true) && !busy.update(NAN, 3.f, true), "bad numbers throw nothing");
    }
    // Names, the event, the structs.
    check(std::string(whyName(Why::tracked)) == "tracked" && std::string(whyName(Why::outsideCone)) == "outside the cone" &&
              std::string(whyName(Why::tooFar)) == "too far",
          "the verdicts' names");
    check(std::string(facing_check::eventName(facing_check::Event::punchTrack)) == "punch track", "the facing check's event");
    check(sizeof(game_punch::TrackAsk) == 32 && sizeof(game_punch::TrackLive) == 88, "the side channel's sizes");
    // Settings: on, 45 degrees, 6 m, the view snap off; the ini keys; the ranges.
    {
        using namespace vr_settings;
        const Values d;
        check(d.punchAutotarget && d.punchTrackConeDeg == 45 && d.punchTrackRangeM == 6 && !d.punchViewSnap,
              "the defaults: on, 45 deg, 6 m, view snap off");
        const auto text = tabOnlyText(d);
        check(text.find("punch_autotarget=1\n") != std::string::npos &&
                  text.find("punch_track_cone_deg=45\n") != std::string::npos &&
                  text.find("punch_track_range_m=6\n") != std::string::npos &&
                  text.find("punch_view_snap=0\n") != std::string::npos,
              "the ini keys write");
        const auto read = withTabOnly({}, "punch_autotarget=0\npunch_track_cone_deg=90\npunch_track_range_m=10\npunch_view_snap=1\n");
        check(!read.punchAutotarget && read.punchTrackConeDeg == 90 && read.punchTrackRangeM == 10 && read.punchViewSnap,
              "the ini keys read");
        const auto wild = withTabOnly({}, "punch_track_cone_deg=500\npunch_track_range_m=-4\n");
        check(wild.punchTrackConeDeg == 90 && wild.punchTrackRangeM == 3, "and clamp");
        Values v;
        check(choose(Item::punchTrackCone, 0, v) && v.punchTrackConeDeg == 15 && choose(Item::punchTrackRange, 4, v) &&
                  v.punchTrackRangeM == 10 && choose(Item::punchViewSnap, 1, v) && v.punchViewSnap &&
                  choose(Item::punchAutotarget, 0, v) && !v.punchAutotarget,
              "the rows set the values");
        check(defaultChoice(Item::punchTrackCone) == 2 && defaultChoice(Item::punchTrackRange) == 2, "the default steps");
        check(inactiveReason(Item::punchTrackCone, v) != nullptr, "the cone waits for the switch");
    }
    std::printf("punch track: %d checks passed\n", passed);
    return 0;
}
