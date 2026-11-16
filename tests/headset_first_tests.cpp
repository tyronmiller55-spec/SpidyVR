// HEADSET FIRST (hybrid_steer::Basis, hybrid_movement::viewRunDirection / Gate::updateView, the setting):
// the stick's basis is where the player looks at the press and follows only the player's own turns after;
// the whole stick circle runs him, the rear half backward; no NaN. Portable.
#include "spidy/direction_report.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/hybrid_movement.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/hybrid_steer.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace spidy;
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
constexpr float deg = 3.14159265f / 180.f;
} // namespace

int main() {
    using hybrid_steer::Basis;
    // Resting: the live view. At the press: latched. Held: only the player's turns move it.
    {
        Basis b;
        check(near(b.update(false, 10 * deg, 0, 0), 10 * deg), "resting basis is the view");
        check(near(b.update(false, 40 * deg, 0, 0), 40 * deg), "and follows it");
        check(near(b.update(true, 40 * deg, 0, 0), 40 * deg) && b.held && b.presses == 1, "the press latches the view");
        // The body turning (the view's animation term) moves the view 60 deg; the player did nothing.
        check(near(b.update(true, 100 * deg, 0, 0), 40 * deg), "a turn the player did not make does not steer");
        check(near(b.update(true, 100 * deg, 0, 0), 40 * deg), "still latched");
        // The head turns 15 deg left, then the right stick 10 deg.
        check(near(b.update(true, 0, 15 * deg, 0), 55 * deg), "the head's own turn steers");
        check(near(b.update(true, 0, 15 * deg, 10 * deg), 65 * deg), "the right stick's turn steers");
        check(near(b.update(true, 0, 15 * deg, 0), 65 * deg), "and a steady head holds it");
        // Release: back to the view.
        check(near(b.update(false, 5 * deg, 15 * deg, 0), 5 * deg) && !b.held, "released: the view again");
        // Action edge while held: the view.
        b.update(true, 20 * deg, 0, 0);
        b.update(true, 90 * deg, 0, 0);
        check(near(b.update(true, 90 * deg, 0, 0, true), 90 * deg) && b.resyncs == 1, "an action re-syncs to the view");
    }
    // Wrap-around and bad input.
    {
        Basis b;
        b.update(true, 170 * deg, 0, 0);
        const float y = b.update(true, 0, 20 * deg, 0);
        check(near(y, -170 * deg, 1e-3f), "the basis wraps past 180");
        const float z = b.update(true, NAN, NAN, NAN);
        check(std::isfinite(z) && near(z, y), "NaN input keeps the basis");
        Basis c;
        check(std::isfinite(c.update(true, NAN, 0, 0)), "a press without a view is finite");
    }
    // The run direction over the whole circle.
    {
        using namespace hybrid_movement;
        check(near(viewRunDirection(0), 0) && near(viewRunDirection(10 * deg), 0), "straight on inside the cone");
        const float mid = viewRunDirection(20 * deg);
        check(mid > 0 && mid < 20 * deg, "blend between 15 and 25");
        check(near(viewRunDirection(60 * deg), 60 * deg) && near(viewRunDirection(-60 * deg), -60 * deg), "free to the side");
        check(near(viewRunDirection(135 * deg), 135 * deg) && near(viewRunDirection(-179 * deg), -179 * deg),
              "the rear half is the stick's own angle: backward");
        float last = -1;
        bool mono = true;
        for (int a = 0; a <= 180; ++a) {
            const float d = std::abs(viewRunDirection(a * deg));
            mono = mono && d >= last - 1e-5f;
            last = d;
        }
        check(mono, "the run direction grows monotonic with the stick angle");
        Gate g;
        auto out = g.updateView(0, 1);
        check(out.drives && near(out.dir, 0) && out.zone == Zone::cone && near(out.y, 1), "forward");
        out = g.updateView(0, -1);
        check(out.drives && near(std::abs(out.dir), 180 * deg) && out.zone == Zone::rear, "back runs backward, no snap");
        check(g.snaps == 0 && !out.snapped, "no rear snap");
        out = g.updateView(-.7f, .7f); // diagonal left
        check(out.drives && near(out.dir, -45 * deg, 1e-2f) && out.zone == Zone::free, "diagonal left is 45 deg left");
        out = g.updateView(.05f, .05f);
        check(!out.drives, "inside the dead zone nothing");
        out = g.updateView(NAN, 1);
        check(!out.drives, "NaN stick is nothing");
        // The world direction the stick sends: basis + dir.
        const float basis = 30 * deg;
        const auto world = hybrid_steer::viewForward(std::remainder(basis - hybrid_movement::viewRunDirection(hybrid_movement::stickAngle(1, 0)), 6.2831853f));
        const auto expect = hybrid_steer::viewForward(basis - 90 * deg);
        check(near(world.x, expect.x) && near(world.z, expect.z), "stick right runs 90 deg right of the basis");
    }
    // LANDINGS FACE THE VIEW: a hold arrives when the hero faces the VIEW, not the heading's own yaw.
    {
        using namespace hybrid_movement;
        Heading h;
        h.latch(0);
        h.begin(Heading::Hold::facing, 0.f); // the heading the view holds on the landing
        Heading::In in;
        in.dt = 1.f / 90.f;
        in.heroOk = true;
        in.steerable = true;
        in.hero = 120 * deg; // the game faces its momentum
        in.view = NAN;
        auto out = h.update(in);
        check(out.heroTurn && h.hold == Heading::Hold::facing, "a landing 120 deg off the heading: the hero is turned");
        // The player looked 112 deg to the left meanwhile: the hero is at 120, the view at 112: he has arrived
        // (within 12 deg of the view), though he is 120 off the heading.
        in.view = 112 * deg;
        out = h.update(in);
        check(!out.heroTurn && h.hold == Heading::Hold::none && h.arrived == 1, "arrived at the view, not at the heading");
        // Not arrived while the view is elsewhere.
        Heading g;
        g.latch(0);
        g.begin(Heading::Hold::facing, 0.f);
        in.view = -90 * deg;
        out = g.update(in);
        check(out.heroTurn && g.hold == Heading::Hold::facing, "still turning toward a view 210 deg away");
        // The heading never follows the view into the hero's facing (no rest drift from the view's gap).
        Heading k;
        k.latch(30 * deg);
        Heading::In idle = in;
        idle.view = 100 * deg;
        idle.hero = 30 * deg;
        k.update(idle);
        check(near(k.yaw, 30 * deg, 1e-3f), "the heading follows the hero as before when nothing holds it");
    }
    // THE VIEW IS YOURS: the heading does not follow the hero, the head's sway is not shown.
    {
        using namespace hybrid_movement;
        Heading h;
        h.latch(10 * deg);
        Heading::In in;
        in.dt = 1.f / 90.f;
        in.heroOk = true;
        in.steerable = true;
        in.viewOwn = true;
        for (int i = 0; i < 200; ++i) {
            in.hero = (i * 2) * deg; // the hero turns away, 400 deg over the run
            h.update(in);
        }
        check(near(h.yaw, 10 * deg, 1e-4f) && h.swayWeight == 0 && !h.following, "the heading stays where the player has it");
        // Without it (the old HYBRID) the heading follows him.
        Heading o;
        o.latch(10 * deg);
        Heading::In old = in;
        old.viewOwn = false;
        old.hero = 90 * deg;
        for (int i = 0; i < 90; ++i)
            o.update(old);
        check(near(o.yaw, 90 * deg, 1e-2f) && o.swayWeight > 0, "the old heading follows the hero");
        // The right stick still turns it, and holds still arrive.
        h.turn(20 * deg);
        check(near(h.yaw, 30 * deg, 1e-4f), "the player's turn moves it");
        // A shown yaw with sway 0, no ride, no limit is exactly the lock.
        view_yaw_lock::State st;
        st.on = true;
        st.yaw = 1.f;
        st.sway = 0;
        st.heroYaw = -2.f;
        check(near(view_yaw_lock::shownYaw(st, 2.5f), 1.f), "shown yaw is the lock whatever the head bone does");
    }
    // The facing check measures and logs the view but never puts it back when it is the player's.
    {
        facing_check::Check c;
        c.arm(facing_check::Event::landing, 0.f);
        facing_check::Sample smp;
        smp.viewOk = true;
        smp.view = 120 * deg;
        smp.viewFixable = true;
        smp.viewOwned = true;
        const auto out = c.update(.01f, smp);
        check(out.view.verdict == facing_check::Verdict::kept && out.viewFix == 0.f && c.viewFixes == 0,
              "a turned view is kept, not corrected");
        facing_check::Check d;
        d.arm(facing_check::Event::landing, 0.f);
        smp.viewOwned = false;
        const auto fixed = d.update(.01f, smp);
        check(fixed.view.verdict == facing_check::Verdict::corrected && near(fixed.viewFix, -120 * deg), "off: corrected as before");
    }
    // The view guard takes back an imposed turn and nothing else.
    {
        direction_report::ViewGuard g;
        check(g.correct(1 * deg, .01f) == 0.f && g.correct(NAN, .01f) == 0.f && g.corrections == 0, "small or bad: nothing");
        check(near(g.correct(20 * deg, .01f), -20 * deg) && g.corrections == 1, "a 20 degree imposed turn is taken back");
        check(near(g.correct(-35 * deg, .01f), 35 * deg) && near(g.worst, 35 * deg), "either way");
        check(g.logDue() && !g.logDue(), "logged once a second");
        // End to end with the reporter: the animation turns the view by 25 deg on a frame; the guard puts it back
        // and the next frame is clean.
        direction_report::Reporter r;
        direction_report::Frame f;
        f.ok = true;
        f.seconds = .011f;
        f.view = 0;
        r.step(f);
        f.view = 25 * deg;
        r.step(f);
        const float fix = g.correct(r.lastImposed(), f.seconds);
        check(near(fix, -25 * deg), "the reporter hands the guard the signed turn");
        r.rebase(25 * deg + fix);
        f.view = 25 * deg + fix;
        r.step(f);
        check(near(r.lastImposed(), 0.f, 1e-3f), "after the rebase the corrected frame is clean: the guard acts once");
    }
    // The setting: on by default, saved and read.
    {
        vr_settings::Values v;
        check(v.headsetFirst, "headset first is on by default");
        const auto off = vr_settings::withTabOnly(v, "headset_first=0\n");
        check(!off.headsetFirst, "turns off");
        check(vr_settings::tabOnlyText(v).find("headset_first=1") != std::string::npos, "saved as headset_first");
        const auto* row = vr_settings::rowFor(vr_settings::Item::headsetFirst);
        check(row && row->place == vr_settings::Place::launcher, "a launcher row");
        bool listed = false;
        for (auto item : vr_settings::screens()[static_cast<size_t>(vr_settings::Category::camera)].items)
            listed = listed || item == vr_settings::Item::headsetFirst;
        check(listed, "on the camera page");
    }
    std::printf("headset first: %d checks passed\n", passed);
    return 0;
}
