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
bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
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
        check(d.punchAutotarget && d.punchTrackConeDeg == 120 && d.punchTrackRangeM == 10 && !d.punchViewSnap,
              "the defaults: on, 120 deg, 10 m, view snap off");
        const auto text = tabOnlyText(d);
        check(text.find("punch_autotarget=1\n") != std::string::npos &&
                  text.find("punch_track_cone_deg=120\n") != std::string::npos &&
                  text.find("punch_track_range_m=10\n") != std::string::npos &&
                  text.find("punch_view_snap=0\n") != std::string::npos,
              "the ini keys write");
        const auto read = withTabOnly({}, "punch_autotarget=0\npunch_track_cone_deg=90\npunch_track_range_m=10\npunch_view_snap=1\n");
        check(!read.punchAutotarget && read.punchTrackConeDeg == 90 && read.punchTrackRangeM == 10 && read.punchViewSnap,
              "the ini keys read");
        const auto wild = withTabOnly({}, "punch_track_cone_deg=500\npunch_track_range_m=-4\n");
        check(wild.punchTrackConeDeg == 120 && wild.punchTrackRangeM == 3, "and clamp");
        Values v;
        check(choose(Item::punchTrackCone, 0, v) && v.punchTrackConeDeg == 15 && choose(Item::punchTrackRange, 3, v) &&
                  v.punchTrackRangeM == 8 && choose(Item::punchViewSnap, 1, v) && v.punchViewSnap &&
                  choose(Item::punchAutotarget, 0, v) && !v.punchAutotarget,
              "the rows set the values");
        check(defaultChoice(Item::punchTrackCone) == 5 && defaultChoice(Item::punchTrackRange) == 4, "the default steps");
        check(inactiveReason(Item::punchTrackCone, v) != nullptr, "the cone waits for the switch");
    }
    // Nov 16 (the owner): 60 degrees and 8 m by default; an untuned 45 / 6 moves to them, a tuned value stays.
    {
        spidy::vr_settings::Values old;
        old.punchTrackConeDeg = 45;
        old.punchTrackRangeM = 6;
        const auto moved = spidy::vr_settings::withMovementDefaults(old, "movement_defaults=4\n");
        check(moved.punchTrackConeDeg == 120 && moved.punchTrackRangeM == 10, "the old defaults become (now) 120 deg, 10 m");
        old.punchTrackConeDeg = 30;
        old.punchTrackRangeM = 4;
        const auto kept = spidy::vr_settings::withMovementDefaults(old, "movement_defaults=4\n");
        check(kept.punchTrackConeDeg == 30 && kept.punchTrackRangeM == 4, "tuned values stay");
        Config wide;
        wide.on = true;
        wide.coneDeg = 60.f;
        wide.rangeM = 8.f;
        const float s50 = std::sin(50.f * 3.14159265f / 180.f), c50 = std::cos(50.f * 3.14159265f / 180.f);
        check(decide(wide, true, head, {5 * s50, 0, -5 * c50}, {0, 1.6f, -7.f}).why == Why::tracked,
              "50 deg off and 7 m away: tracked at the new defaults");
    }
    // Nov 17 (the owner, "open up punch tracking and lengthen it to 10m"): 90 degrees and 10 m; an untuned 60 / 8 moves.
    {
        const spidy::vr_settings::Values fresh;
        check(fresh.punchTrackConeDeg == 120 && fresh.punchTrackRangeM == 10 && !fresh.punchTrackByLook &&
                  fresh.movementDefaults == spidy::vr_settings::movementDefaultsRevision,
              "the defaults: 120 deg, 10 m, by look off, the current revision");
        spidy::vr_settings::Values old;
        old.punchTrackConeDeg = 60;
        old.punchTrackRangeM = 8;
        auto m = spidy::vr_settings::withMovementDefaults(old, "movement_defaults=5\n");
        check(m.punchTrackConeDeg == 120 && m.punchTrackRangeM == 10 && m.movementDefaults == spidy::vr_settings::movementDefaultsRevision, "60 / 8 become (now) 120 / 10");
        // Revision 11 (the 2 AM log): an untuned 90 becomes 120; a 90 chosen after it stays.
        old.punchTrackConeDeg = 90;
        check(spidy::vr_settings::withMovementDefaults(old, "movement_defaults=10\n").punchTrackConeDeg == 120, "90 becomes 120");
        check(spidy::vr_settings::withMovementDefaults(old, "movement_defaults=11\n").punchTrackConeDeg == 90, "a 90 chosen after stays");
        {
            Config w;
            w.on = true;
            w.coneDeg = 120.f;
            w.rangeM = 10.f;
            const float s105 = std::sin(105.f * 3.14159265f / 180.f), c105 = std::cos(105.f * 3.14159265f / 180.f);
            check(decide(w, true, head, {5 * s105, 0, -5 * c105}, {0, 1.6f, -7.f}).why == Why::tracked,
                  "a throw 105 degrees off the line to him: tracked at 120");
        }
        old.punchTrackConeDeg = 45;
        old.punchTrackRangeM = 4;
        m = spidy::vr_settings::withMovementDefaults(old, "movement_defaults=5\n");
        check(m.punchTrackConeDeg == 45 && m.punchTrackRangeM == 4, "tuned values stay");
        old.punchTrackConeDeg = 60;
        m = spidy::vr_settings::withMovementDefaults(old, "movement_defaults=6\n");
        check(m.punchTrackConeDeg == 60, "a 60 chosen after revision 6 stays");
        const auto read = spidy::vr_settings::withTabOnly({}, "punch_track_by_look=1\n");
        check(read.punchTrackByLook, "punch_track_by_look reads");
        Config c;
        c.on = true;
        c.coneDeg = 90.f;
        c.rangeM = 10.f;
        const float s80 = std::sin(80.f * 3.14159265f / 180.f), c80 = std::cos(80.f * 3.14159265f / 180.f);
        check(decide(c, true, head, {5 * s80, 0, -5 * c80}, {0, 1.6f, -9.5f}).why == Why::tracked,
              "80 deg off at 9.5 m: tracked at 90 / 10");
        check(decide(c, true, head, {0, 0, -5}, {0, 1.6f, -10.5f}).why == Why::tooFar, "10.5 m: too far");
        // BY LOOK: a fist thrown 120 deg off him, the player looking right at him.
        const Vec3 wide{5 * std::sin(2.0944f), 0, -5 * std::cos(2.0944f)};
        auto v = decide(c, true, head, wide, {0, 1.6f, -5.f}, {0, 0, -1});
        check(v.why == Why::outsideCone && !v.byLook && v.lookDeg < 0, "by look off: outside the cone");
        c.byLook = true;
        v = decide(c, true, head, wide, {0, 1.6f, -5.f}, {0, 0, -1});
        check(v.why == Why::tracked && v.byLook && near(v.lookDeg, 0.f, .5f) && near(v.toward.z, -1.f),
              "by look on: tracked on the look");
        v = decide(c, true, head, wide, {0, 1.6f, -5.f}, {0, 0, 1});
        check(v.why == Why::outsideCone && near(v.lookDeg, 180.f, .5f), "looking away: still outside");
        v = decide(c, true, head, wide, {0, 1.6f, -5.f}, {});
        check(v.why == Why::outsideCone && v.lookDeg < 0, "no look: the fist alone");
        v = decide(c, true, head, {0, 0, -5}, {0, 1.6f, -5.f}, {0, 0, 1});
        check(v.why == Why::tracked && !v.byLook, "the fist's line first");
    }
    std::printf("punch track: %d checks passed\n", passed);
    return 0;
}
