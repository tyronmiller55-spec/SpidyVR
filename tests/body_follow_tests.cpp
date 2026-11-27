// BODY FOLLOWS VIEW (body_follow.hpp, beta3q): the actor's follow (soft and hard lines, the re-arm, arrival, no give-up,
// the lowest priority, the stick and the fight gates), the drawn body's catch (never more than 90 degrees off the view,
// eased, its side kept through +-180), the one writer with the landing body, and the switch's defaults.
#include "spidy/body_follow.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy::body_follow;
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
    return std::abs(a - b) <= e;
}
float rad(float d) {
    return d * degrees;
}
Follow::In ground(float gapDeg, float dt = 1.f / 90.f) {
    Follow::In in;
    in.dt = dt;
    in.gap = rad(gapDeg);
    in.live = true;
    in.canTurn = true;
    return in;
}
// Runs `seconds` of frames at a fixed gap; true if it forced on any of them.
bool run(Follow& f, const Tuning& t, float gapDeg, float seconds, Follow::In base) {
    bool forced = false;
    for (float s = 0; s < seconds; s += base.dt) {
        base.gap = rad(gapDeg);
        forced = f.update(base, t).force || forced;
    }
    return forced;
}
} // namespace

int main() {
    const Tuning t{};
    check(near(t.softDeg, 60) && near(t.softSeconds, .4f) && near(t.hardDeg, 100), "the plan's tuning: 60 deg 0.4 s, 100 deg");
    check(rearmDeg == 80.f && doneDeg == 12.f && catchDeg == 90.f, "re-arm 80, done 12, catch 90");

    // Soft: past 60 for 0.4 s, not before; under 60 the dwell starts again.
    {
        Follow f;
        check(!run(f, t, 70, .3f, ground(0)), "soft: nothing before 0.4 s");
        check(!run(f, t, 50, .2f, ground(0)), "soft: under the line resets");
        check(!run(f, t, 70, .3f, ground(0)), "soft: the dwell started again");
        check(run(f, t, 70, .15f, ground(0)) && f.engaged && f.why == Why::soft, "soft: engaged after 0.4 s");
        auto o = f.update(ground(5), t);
        check(o.ended == End::arrived && !f.engaged && !o.force, "arrived under 12 deg");
        check(f.arrivals == 1 && f.softStarts == 1, "counted");
    }
    // Hard: past 100 at once; re-armed only under 80.
    {
        Follow f;
        auto o = f.update(ground(-120), t);
        check(o.started && o.force && o.why == Why::hard && near(o.startGap, rad(-120)), "hard: at once, with its gap");
        auto arrived = f.update(ground(5), t);
        check(arrived.ended == End::arrived && !f.engaged && f.hardArmed, "arrived under 12 (so under 80: re-armed)");
        check(f.update(ground(5), t).force == false, "nothing at 5 deg");
        Follow g;
        g.update(ground(110), t);
        g.update(ground(90), t); // still engaged, between the lines: no re-arm
        check(g.engaged && !g.hardArmed, "between 80 and 100: not re-armed");
    }
    // Never gives up: blocked for 10 s it waits, and turns him as soon as the blocker clears.
    {
        Follow f;
        auto blocked = ground(150);
        blocked.canTurn = false;
        check(!run(f, t, 150, 10.f, blocked), "blocked: no force");
        check(f.engaged && f.waited > 9.9f, "still engaged after 10 s, waiting");
        check(f.update(ground(150), t).force, "the blocker cleared: it turns him");
    }
    // Lowest priority: another turn under way, it waits; the stick (canTurn false) wins.
    {
        Follow f;
        auto busy = ground(130);
        busy.busy = true;
        check(!f.update(busy, t).force && f.engaged, "busy: engaged, no force");
        check(f.update(ground(130), t).force, "not busy: force");
    }
    // Fights: the soft follow waits; the hard catch waits too unless FIGHTS is on.
    {
        Follow f;
        auto fight = ground(70);
        fight.fight = true;
        check(!run(f, t, 70, 1.f, fight) && f.engaged, "fight: the soft follow waits");
        Follow g;
        auto hard = ground(140);
        hard.fight = true;
        check(!g.update(hard, t).force, "fight: the hard catch waits with FIGHTS off");
        Follow h;
        hard.fightsAllowed = true;
        auto o = h.update(hard, t);
        check(o.force && o.fightTurn, "fight: the hard catch turns him with FIGHTS on");
        Follow k;
        auto soft = ground(70);
        soft.fight = true;
        soft.fightsAllowed = true;
        check(!run(k, t, 70, 1.f, soft), "fight: the soft follow still waits with FIGHTS on");
    }
    // Not live (off, SpidyVR's air, Showtime, no view): nothing, and an engaged follow stands down.
    {
        Follow f;
        f.update(ground(150), t);
        auto off = ground(150);
        off.live = false;
        auto o = f.update(off, t);
        check(o.ended == End::cancelled && !o.force && !f.engaged, "off: stood down");
        Follow g;
        auto none = ground(0);
        none.gap = NAN;
        check(!g.update(none, t).force && !g.engaged, "no gap: nothing");
    }
    // Tuning from the settings: clamped, the soft line never past the hard one.
    {
        const Tuning a = tuningOf(60, 400, 100);
        check(near(a.softDeg, 60) && near(a.softSeconds, .4f) && near(a.hardDeg, 100), "tuningOf the defaults");
        const Tuning b = tuningOf(150, -5, 90);
        check(near(b.softDeg, 90) && near(b.softSeconds, 0) && near(b.hardDeg, 90), "tuningOf clamps");
    }

    // CATCH: nothing inside 90; past it the drawn turn eases in to the excess, so the torso is never more than 90 off.
    {
        Catch c;
        check(c.update(1.f / 90, rad(80), true) == 0.f && !c.active(), "inside 90: nothing drawn");
        float y = 0;
        for (int i = 0; i < 30; ++i)
            y = c.update(1.f / 90, rad(130), true);
        check(near(y, rad(40)), "eased in to the excess (130 - 90)");
        check(std::abs(rad(130) - y) <= rad(90) + 1e-3f, "the torso within 90 of the view");
        y = c.update(1.f / 90, rad(-130), true); // 130 one way to 130 the other: crossed +-180 (230 the same way)
        check(y > 0 && near(y, rad(140)), "through +-180: the same side (230 - 90)");
        for (int i = 0; i < 40; ++i)
            y = c.update(1.f / 90, NAN, false);
        check(y == 0.f && !c.active(), "eased out when not live");
        Catch d;
        for (int i = 0; i < 30; ++i)
            d.update(1.f / 90, rad(-100), true);
        check(near(d.offset, rad(-10)), "the other side: negative");
        // Past 270 the same way round it is the short way again (under 90 the other side).
        Catch e;
        e.update(1.f / 90, rad(170), true);
        float z = e.update(1.f / 90, rad(-170), true);
        check(z > 0, "just past 180: kept");
        z = e.update(1.f / 90, rad(-60), true);
        check(z == 0.f || std::abs(z) < rad(100), "back inside: no turn past the excess");
        // The first frame past the line eases in (no pop).
        Catch f;
        const float first = f.update(1.f / 90, rad(150), true);
        check(first > 0 && first < rad(5), "eased in: the first frame is small");
    }
    // One writer: the landing body's turn while it runs, never less than the catch on the same side.
    check(merged(0.f, false, rad(20)) == rad(20), "no landing: the catch");
    check(merged(rad(50), true, rad(20)) == rad(50), "landing larger: the landing's");
    check(merged(rad(10), true, rad(20)) == rad(20), "catch larger on the same side: the catch's");
    check(merged(rad(-30), true, rad(20)) == rad(-30), "opposite sides: the landing's");
    check(merged(rad(30), true, 0.f) == rad(30), "no catch: the landing's");

    // The switch: on by default, its own key, fights off, revision 28 turns it on once, a 0 after it stays.
    using namespace spidy::vr_settings;
    check(Values{}.bodyFollowsView && !Values{}.bodyFollowFights, "on, fights off by default");
    check(Values{}.bodyFollowSoftDeg == 60 && Values{}.bodyFollowSoftMs == 400 && Values{}.bodyFollowHardDeg == 100,
          "the tuning's defaults");
    check(movementDefaultsRevision >= 28, "revision 28");
    const std::string old = "movement_defaults=27\nbody_follows_view=0\n";
    check(withMovementDefaults(withTabOnly({}, old), old).bodyFollowsView, "revision 28 turns it on once");
    const std::string kept = "movement_defaults=28\nbody_follows_view=0\nbody_follow_soft_deg=200\n";
    const Values k = withMovementDefaults(withTabOnly({}, kept), kept);
    check(!k.bodyFollowsView, "a 0 after revision 28 stays");
    check(k.bodyFollowSoftDeg == 100, "the soft line clamped to the hard line");
    check(tabOnlyText(Values{}).find("body_follows_view=1\n") != std::string::npos, "its key in spidy_vr.ini");

    std::printf("body_follow: %d checks passed\n", passed);
    return 0;
}
