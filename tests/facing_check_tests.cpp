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
        const auto o = c.update(dt, anim(60 * deg, 70 * deg));
        check(o.view.verdict == Verdict::ok && o.hero.verdict == Verdict::ok && !o.heroTurn,
              "re-entry: 30 deg view, 40 deg hero are within tolerance");
        // The hero's tolerance is 45 deg (90 until the G1 playtest: half of the landings 45 to 90 off were "ok").
        Check t45;
        t45.arm(Event::landing, 30 * deg);
        const auto edge = t45.update(dt, anim(30 * deg, 110 * deg)); // 80 deg off
        check(edge.hero.verdict == Verdict::corrected && edge.heroTurn && near(edge.heroYaw, 30 * deg, .5f * deg),
              "landing: a hero 80 deg off is turned now (was ok under 90)");
        Check t44;
        t44.arm(Event::landing, 30 * deg);
        check(t44.update(dt, anim(30 * deg, 73 * deg)).hero.verdict == Verdict::ok, "landing: 43 deg off is ok");
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
        // Kept pending (G1 playtest): a fight for 1.5 s does not drop it; it starts at the first legal frame.
        TIP d;
        in.force = true;
        in.forceAllowed = false;
        in.forceBlock = view_yaw_lock::Block::fight;
        o = d.update(in);
        in.force = false;
        bool dropped = false;
        for (int i = 0; i < 135; ++i) // 1.5 s
            dropped |= d.update(in).dropped;
        check(!dropped && !d.turning, "forced: still pending after 1.5 s of a fight");
        in.forceAllowed = true;
        in.forceBlock = view_yaw_lock::Block::none;
        check(d.update(in).started, "forced: starts at the first legal frame after the fight");
        // Dropped after 2.5 s without a frame to push, naming what blocked it longest.
        TIP e;
        in.force = true;
        in.forceAllowed = false;
        in.forceBlock = view_yaw_lock::Block::dodge;
        e.update(in);
        in.force = false;
        TIP::Out dropOut;
        for (int i = 0; i < 90 * 3 && !dropOut.dropped; ++i) {
            in.forceBlock = i < 20 ? view_yaw_lock::Block::airborne : view_yaw_lock::Block::fight;
            dropOut = e.update(in);
        }
        check(dropOut.dropped && !e.turning, "forced: dropped after 2.5 s, said so");
        check(dropOut.dropWaited > 2.4f && dropOut.dropWaited < 2.6f, "forced: waited 2.5 s");
        check(dropOut.dropWhy == view_yaw_lock::Block::fight && dropOut.dropBlocked > 2.2f,
              "forced: dropped for the fight, the longest blocker");
        // The player's own left stick wins: held for 0.25 s it drops the turn (he is walking where he wants).
        TIP w;
        in.force = true;
        in.forceAllowed = false;
        in.forceBlock = view_yaw_lock::Block::stick;
        w.update(in);
        in.force = false;
        dropOut = {};
        int frames = 0;
        for (; frames < 90 && !dropOut.dropped; ++frames)
            dropOut = w.update(in);
        check(dropOut.dropped && dropOut.dropWhy == view_yaw_lock::Block::stick && dropOut.dropWaited < .4f,
              "forced: the left stick held drops it within 0.4 s, and says it won");
        // A tap of the stick (0.1 s) does not.
        TIP tap;
        in.force = true;
        in.forceBlock = view_yaw_lock::Block::stick;
        tap.update(in);
        in.force = false;
        bool tapDropped = false;
        for (int i = 0; i < 9; ++i)
            tapDropped |= tap.update(in).dropped;
        in.forceBlock = view_yaw_lock::Block::fight;
        for (int i = 0; i < 9; ++i)
            tapDropped |= tap.update(in).dropped;
        in.forceBlock = view_yaw_lock::Block::stick;
        for (int i = 0; i < 9; ++i)
            tapDropped |= tap.update(in).dropped;
        in.forceAllowed = true;
        in.forceBlock = view_yaw_lock::Block::none;
        check(!tapDropped && tap.update(in).started, "forced: two stick taps (0.1 s each) do not drop it");
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
    std::printf("facing check tests: %d passed\n", passed);
    return 0;
}
