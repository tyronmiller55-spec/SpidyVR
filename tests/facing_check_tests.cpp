// FACING CHECK (facing_check.hpp; the user, November 12 07:14: "it turns me 180 on
// landing or launch"). The 180-degree cases from the 06:37 session (319e734), each
// replayed: the landing whose hero faced the game's way, the latch on a head bone that
// had already turned, the sway that slid the view, the point launch and the jump
// handoff whose view came out turned; and the forced hero turn that fixes him.
#include "spidy/facing_check.hpp"
#include "spidy/view_yaw_lock.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::facing_check;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float deg = 3.14159265f / 180.f;
constexpr float dt = 1.f / 90;
bool near(float a, float b, float tol) {
    return std::abs(wrap(a - b)) <= tol;
}
Sample anim(float view, float hero, bool fixable = true) {
    Sample s;
    s.viewOk = true;
    s.view = view;
    s.viewFixable = fixable;
    s.heroMatters = true;
    s.heroOk = true;
    s.hero = hero;
    return s;
}
Sample vr(float view) {
    Sample s;
    s.viewOk = true;
    s.view = view;
    s.viewFixable = true;
    return s;
}
// The game turns the hero toward the walk stick's way, ~250 deg/s, a frame late.
struct Hero {
    float yaw;
    void step(const view_yaw_lock::TurnInPlace::Out& o) {
        if (!o.push)
            return;
        const float d = wrap(o.pushYaw - yaw);
        const float most = 250 * deg * dt;
        yaw = wrap(yaw + std::clamp(d, -most, most));
    }
};
} // namespace

int main() {
    // --- 1. The 31.70 landing: SpidyVR showed 97, the latch's head bone 97 (the SpidyVR
    // body under the view), the actor's facing -80: the game's landing turned him 177.
    {
        Check c;
        c.arm(Event::landing, 97 * deg);
        const auto o = c.update(dt, anim(97 * deg, -80 * deg));
        check(o.view.verdict == Verdict::ok, "31.70 landing: the view kept (ok)");
        check(o.hero.verdict == Verdict::corrected && o.heroTurn, "31.70 landing: the hero 177 off is corrected");
        check(near(o.heroYaw, 97 * deg, .5f * deg), "31.70 landing: he turns to the view's way");
        check(!c.armed, "one check per event");
        // The forced turn brings him round, even with TURN-IN-PLACE's own conditions off.
        view_yaw_lock::TurnInPlace t;
        Hero h{-80 * deg};
        view_yaw_lock::TurnInPlace::In in;
        in.dt = dt;
        in.allowed = false; // the look's turn would not run (moving out of the landing)
        in.angle = 35 * deg;
        in.heroYaw = h.yaw;
        in.force = true;
        in.forceYaw = o.heroYaw;
        in.forceAllowed = true;
        auto out = t.update(in);
        check(out.started && out.forced, "the hero turn starts on the next frame");
        in.force = false;
        bool done = false;
        for (int i = 0; i < 300 && !done; ++i) {
            h.step(out);
            in.heroYaw = h.yaw;
            out = t.update(in);
            done = out.ended == view_yaw_lock::TurnInPlace::End::done;
        }
        check(done && out.forced, "the hero turn ends done");
        check(near(h.yaw, 97 * deg, 13 * deg), "the hero faces the view after it");
    }
    // --- 2. A latch a frame late, on the game's head that had already turned (408.44:
    // view 152 shown, the bone at -159 then 16): the view came out the hero's way.
    {
        Check c;
        c.arm(Event::landing, 152 * deg);
        view_yaw_lock::Lock lock;
        lock.latch(-24 * deg, 24 * deg, .2f); // bone already on the hero's way
        view_yaw_lock::State st{true, lock.yaw, .2f, 24 * deg};
        const float shown = view_yaw_lock::shownYaw(st, -24 * deg);
        check(std::abs(wrap(shown - 152 * deg)) > 135 * deg, "replay: the late latch shows ~180 off");
        const auto o = c.update(dt, anim(shown, 24 * deg));
        check(o.view.verdict == Verdict::corrected, "the late latch's view is corrected");
        lock.yaw = wrap(lock.yaw + o.viewFix);
        st.yaw = lock.yaw;
        check(near(view_yaw_lock::shownYaw(st, -24 * deg), 152 * deg, 1 * deg),
              "the view is back where SpidyVR showed it, the same frame");
        check(o.hero.verdict == Verdict::corrected, "and the hero 128 off turns too");
    }
    // Without the lock (head follow not Full / sway 100%) the view cannot be turned back:
    // it says so, and the hero's turn (the view rides on his head) is what fixes it.
    {
        Check c;
        c.arm(Event::landing, 97 * deg);
        const auto o = c.update(dt, anim(-83 * deg, -80 * deg, false));
        check(o.view.verdict == Verdict::unfixable && o.viewFix == 0, "no lock: the view is reported, not turned");
        check(o.heroTurn, "no lock: the hero turn still runs");
    }
    // --- 3. The sway on a 180 gap (31.73 -> 32.25: shown 97 slid to 60 as the game
    // turned the hero's head; 20% of 177 = 35 deg). Past 90 deg the bone is no sway.
    {
        view_yaw_lock::Lock lock;
        lock.latch(97 * deg, -80 * deg, .2f);
        view_yaw_lock::State st{true, lock.yaw, .2f, -80 * deg};
        const float atLatch = view_yaw_lock::shownYaw(st, 97 * deg);
        const float after = view_yaw_lock::shownYaw(st, -86 * deg);
        check(near(atLatch, 97 * deg, .5f * deg), "sway: the latch shows the bone");
        check(near(after, 97 * deg, 2 * deg), "sway: no 35 deg slide when the game turns his head round");
        // The wrap at 180: bone-hero +179 then -179 must not jump the view by 72.
        const float a = view_yaw_lock::shownYaw(st, 99 * deg), b = view_yaw_lock::shownYaw(st, 101 * deg);
        check(std::abs(wrap(a - b)) < 3 * deg, "sway: no jump across the 180 wrap");
        // Small sways are untouched (20% of 30 deg).
        const float small = view_yaw_lock::shownYaw(st, -50 * deg);
        check(near(small, wrap(lock.yaw + .2f * 30 * deg), .1f * deg), "sway: a head within 45 deg sways in full");
    }
    // --- 4. Point launches.
    {
        // Backward off a wall, the player looking at it (48 of 61 were wall launches):
        // the view stays the reference, no turn toward the launch.
        check(near(launchExpected(0, 180 * deg), 0, 0), "wall launch backward: expect the view");
        check(near(launchExpected(10 * deg, 40 * deg), 40 * deg, 0), "launch where you look: expect its way");
        Check c;
        c.arm(Event::pointLaunch, 0, launchExpected(0, 180 * deg));
        c.update(dt, vr(0)); // not sampled before the frame after: game_xr says viewOk only from frame 2
        check(!c.armed, "a launch that kept the view is ok and done");
        // The 1225.5 point jump edge: the view came out 158 off the shown one.
        Check d;
        d.arm(Event::pointLaunch, -68 * deg, launchExpected(-68 * deg, -40 * deg));
        const auto o = d.update(dt, vr(90 * deg));
        check(o.view.verdict == Verdict::corrected, "point launch: a 158 turn is corrected");
        check(near(90 * deg + o.viewFix, -68 * deg, .5f * deg), "point launch: back on the view before");
        check(o.hero.verdict == Verdict::none, "point launch in SpidyVR: no hero check (the body follows the view)");
    }
    // --- 5. The 41.70 jump handoff: shown 115, SpidyVR's view -122 (125 off).
    {
        Check c;
        c.arm(Event::jumpHandoff, 115 * deg);
        const auto o = c.update(dt, vr(-122 * deg));
        check(o.view.verdict == Verdict::corrected && near(-122 * deg + o.viewFix, 115 * deg, .5f * deg),
              "jump handoff: the 125 turn is put back");
        Check ok;
        ok.arm(Event::jumpHandoff, 115 * deg);
        check(ok.update(dt, vr(100 * deg)).view.verdict == Verdict::ok, "jump handoff: 15 deg is a head turn, ok");
    }
    // --- 6. Re-entry within tolerance, missing samples, unknown view.
    {
        Check c;
        c.arm(Event::animReentry, 30 * deg);
        const auto o = c.update(dt, anim(60 * deg, 110 * deg));
        check(o.view.verdict == Verdict::ok && o.hero.verdict == Verdict::ok && !o.heroTurn,
              "re-entry: 30 deg view, 80 deg hero are within tolerance");
        Check m;
        m.arm(Event::landing, 0);
        Out last;
        for (int i = 0; i < 40 && m.armed; ++i)
            last = m.update(dt, {});
        check(!m.armed && last.view.verdict == Verdict::missed, "no sample within 0.3 s: missed, not stuck");
        Check n;
        n.arm(Event::landing, NAN);
        check(!n.armed, "no known view before: nothing to check");
        // The hero waits for his sample after the view's.
        Check w;
        w.arm(Event::landing, 0);
        Sample s = anim(0, 0);
        s.heroOk = false;
        const auto o1 = w.update(dt, s);
        check(o1.view.verdict == Verdict::ok && o1.hero.verdict == Verdict::none && w.armed, "the hero is awaited");
        const auto o2 = w.update(dt, anim(0, 170 * deg));
        check(o2.hero.verdict == Verdict::corrected && !w.armed, "and checked when it comes");
    }
    // --- 7. The forced turn's rules (the 06:37 landings: 18 interrupted, 26 never ran).
    {
        using TIP = view_yaw_lock::TurnInPlace;
        TIP::In in;
        in.dt = dt;
        in.angle = 35 * deg;
        in.heroYaw = 0;
        in.force = true;
        in.forceYaw = 170 * deg;
        in.forceAllowed = false; // a landing's frames
        TIP t;
        auto o = t.update(in);
        check(!o.started, "forced: waits while it cannot push");
        in.force = false;
        for (int i = 0; i < 20; ++i)
            o = t.update(in);
        in.forceAllowed = true;
        o = t.update(in);
        check(o.started && o.forced, "forced: starts once allowed (within 0.4 s)");
        // A 0.2 s gap rides out; it is not an interruption.
        in.forceAllowed = false;
        bool interrupted = false;
        for (int i = 0; i < 18; ++i)
            interrupted |= t.update(in).ended == TIP::End::interrupted;
        check(!interrupted && t.turning, "forced: rides out a 0.2 s gap");
        in.forceAllowed = true;
        check(t.update(in).push, "forced: pushes again after it");
        // A relatch of the yaw lock does not cancel it.
        t.resetLook();
        check(t.turning, "forced: a lock relatch keeps it");
        t.reset();
        // Dropped after 0.4 s without a frame to push.
        TIP d;
        in.force = true;
        in.forceAllowed = false;
        o = d.update(in);
        in.force = false;
        bool dropped = false;
        for (int i = 0; i < 60; ++i)
            dropped |= d.update(in).dropped;
        check(dropped && !d.turning, "forced: dropped after 0.4 s, said so");
        // Already facing: nothing to do.
        TIP f;
        in.force = true;
        in.forceAllowed = true;
        in.forceYaw = 5 * deg;
        check(!f.update(in).started, "forced: already within 12 deg, no turn");
        // The look's turn still needs its own conditions and angle.
        TIP l;
        TIP::In look;
        look.dt = dt;
        look.allowed = true;
        look.angle = 35 * deg;
        look.heading = 20 * deg;
        check(!l.update(look).started, "the look's turn: under its angle, none");
    }
    // --- 8. LANDING FACING SNAP: a forced turn that starts past 135 deg pushes hard.
    {
        using TIP = view_yaw_lock::TurnInPlace;
        auto start = [&](float gapDeg, TIP& t) {
            TIP::In in;
            in.dt = dt;
            in.heroYaw = 0;
            in.force = true;
            in.forceYaw = gapDeg * deg;
            in.forceAllowed = true;
            return t.update(in);
        };
        TIP back;
        const auto o = start(170, back);
        check(o.started && o.forced && o.hard && o.push, "a landing 170 deg off: the snap (hard push)");
        check(near(o.tilt, view_yaw_lock::hardTilt, 1e-4f), "the snap pushes at the hard tilt");
        TIP::In go;
        go.dt = dt;
        go.heroYaw = 40 * deg; // under way, 130 deg left: still the snap's push
        go.forceYaw = 170 * deg;
        go.forceAllowed = true;
        check(near(back.update(go).tilt, view_yaw_lock::hardTilt, 1e-4f), "stays hard until it arrives");
        go.heroYaw = 165 * deg;
        const auto done = back.update(go);
        check(done.ended == TIP::End::done && done.hard, "arrives: done, and said it was the snap's");
        TIP side;
        const auto o2 = start(100, side);
        check(o2.started && !o2.hard && near(o2.tilt, view_yaw_lock::pushTilt, 1e-4f),
              "100 deg off: the ordinary walk turn");
        TIP edge;
        check(!start(134, edge).hard, "134 deg: not the snap");
        TIP edge2;
        check(start(-140, edge2).hard, "-140 deg: the snap, either way");
        TIP again;
        start(170, again);
        again.reset();
        const auto o3 = start(90, again);
        check(o3.started && !o3.hard, "reset: the next turn is judged afresh");
    }
    std::printf("facing check tests: %d passed\n", passed);
    return 0;
}
