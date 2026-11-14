// HEADSET FIRST (hybrid_steer::Basis, hybrid_movement::viewRunDirection / Gate::updateView, the setting):
// the stick's basis is where the player looks at the press and follows only the player's own turns after;
// the whole stick circle runs him, the rear half backward; no NaN. Portable.
#include "spidy/hybrid_movement.hpp"
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
