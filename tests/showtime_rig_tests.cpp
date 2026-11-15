// SHOWTIME's rig (showtime_rig.hpp) and the latch (showtime.hpp).
#include "spidy/showtime.hpp"
#include "spidy/showtime_rig.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spidy;
using namespace spidy::showtime;
namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
constexpr float dt = 1.f / 90.f;
OrbitRig::Input at(Vec3 feet) {
    OrbitRig::Input in;
    in.seconds = dt;
    in.feet = feet;
    in.turning = true;
    return in;
}
void springSettles() {
    float x = 0, v = 0;
    float peak = 0;
    for (int i = 0; i < 400; ++i) {
        spring(x, v, 1.f, 6.f, dt);
        peak = std::max(peak, x);
    }
    check(peak <= 1.0001f, "critically damped: no overshoot");
    check(std::abs(x - 1.f) < .001f, "settles on the target");
    float y = 0, w = 0;
    for (int i = 0; i < 72; ++i) spring(y, w, 1.f, 6.f, dt);
    check(y > .9f && y < .999f, "about 0.8 s to settle at 6 rad/s");
    // exact for any step: one big step equals many small ones
    float a = 0, av = 0, b = 0, bv = 0;
    spring(a, av, 1.f, 6.f, .1f);
    for (int i = 0; i < 10; ++i) spring(b, bv, 1.f, 6.f, .01f);
    check(std::abs(a - b) < 1e-4f, "step size does not matter");
}
void placement() {
    OrbitRig rig;
    RigParams p;
    rig.begin({0, 0, 0}, 0.f, p);
    const auto o = rig.update(at({0, 0, 0}), p);
    // yaw 0 faces -z; the camera is behind him (+z) and 35 degrees to one side, 2.5 m out and 0.3 m above the pivot (his pelvis, 1 m up)
    check(std::abs(std::hypot(o.position.x, o.position.z - 0.f) - 2.5f) < .01f, "2.5 m out from the pivot");
    check(std::abs(o.position.y - 1.3f) < .01f, "pivot + HEIGHT");
    check(o.position.z > 2.0f && std::abs(o.position.x) > 1.2f, "behind and off his back");
    check(o.yaw == 0.f, "the yaw is the entry heading: the cut turns nothing");
    // 0 degrees off his back: straight behind
    RigParams q;
    q.angleDeg = 0;
    rig.begin({0, 0, 0}, 0.f, q);
    const auto s = rig.update(at({0, 0, 0}), q);
    check(std::abs(s.position.x) < .01f && std::abs(s.position.z - 2.5f) < .01f, "0 degrees: straight behind");
}
void yawOnlyFromTheStick() {
    OrbitRig rig;
    RigParams p;
    rig.begin({0, 0, 0}, 1.f, p);
    Vec3 feet{};
    // he runs in circles and the picture's yaw does not move
    for (int i = 0; i < 600; ++i) {
        feet = {std::sin(i * .05f) * 8.f, 0, std::cos(i * .05f) * 8.f};
        rig.update(at(feet), p);
    }
    check(std::abs(rig.yaw() - 1.f) < 1e-6f, "his motion never turns the rig");
    // not the player's (fading back): the stick does nothing
    auto in = at(feet);
    in.stickX = 1.f;
    in.turning = false;
    for (int i = 0; i < 90; ++i) rig.update(in, p);
    check(std::abs(rig.yaw() - 1.f) < 1e-6f, "no orbit while the view is going back");
    in.turning = true;
    for (int i = 0; i < 90; ++i) rig.update(in, p);
    check(rig.yaw() < 1.f - .5f, "stick right turns the orbit (clockwise from above, as a right turn)");
    // eased: the first frames turn less than the later ones
    OrbitRig e;
    e.begin({}, 0.f, p);
    auto s = at({});
    s.stickX = 1.f;
    e.update(s, p);
    const float first = std::abs(e.yaw());
    for (int i = 0; i < 20; ++i) e.update(s, p);
    const float before = std::abs(e.yaw());
    e.update(s, p);
    check(first < (before - first) / 20.f + 1e-6f || first < .02f, "eased in");
}
void snapOrbit() {
    OrbitRig rig;
    RigParams p;
    p.snap = true;
    rig.begin({}, 0.f, p);
    auto s = at({});
    s.stickX = 1.f;
    float maxBlink = 0;
    for (int i = 0; i < 30; ++i) maxBlink = std::max(maxBlink, rig.update(s, p).blink);
    check(std::abs(rig.yaw() + 30.f * 3.14159265f / 180) < 1e-4f, "one flick: 30 degrees");
    check(maxBlink > .8f, "under a blink");
    // held: one step only
    for (int i = 0; i < 90; ++i) rig.update(s, p);
    check(std::abs(rig.yaw() + 30.f * 3.14159265f / 180) < 1e-4f, "a held stick snaps once");
    s.stickX = 0;
    for (int i = 0; i < 9; ++i) rig.update(s, p);
    s.stickX = -1.f;
    for (int i = 0; i < 30; ++i) rig.update(s, p);
    check(std::abs(rig.yaw()) < 1e-4f, "and back");
}
void heightStick() {
    OrbitRig rig;
    RigParams p;
    rig.begin({}, 0.f, p);
    auto s = at({});
    s.stickY = 1.f;
    for (int i = 0; i < 900; ++i) rig.update(s, p);
    check(std::abs(rig.height() - 1.5f) < 1e-4f, "up: +1.5 m at most");
    s.stickY = -1.f;
    for (int i = 0; i < 1800; ++i) rig.update(s, p);
    check(std::abs(rig.height() + .5f) < 1e-4f, "down: -0.5 m at most");
}
void followAndLag() {
    OrbitRig rig;
    RigParams p;
    rig.begin({}, 0.f, p);
    // He zips at 25 m/s: the camera follows, never cuts (no frame moves more than the zip itself), and keeps within a few metres.
    Vec3 feet{};
    Vec3 last = rig.update(at(feet), p).position;
    float worst = 0, maxLag = 0;
    for (int i = 0; i < 360; ++i) {
        feet.z -= 25.f * dt;
        const auto o = rig.update(at(feet), p);
        worst = std::max(worst, length(o.position - last));
        last = o.position;
        maxLag = std::max(maxLag, o.lag);
    }
    check(worst < 25.f * dt * 1.5f, "never a cut: no step bigger than the zip's");
    check(maxLag < 6.5f, "he is not lost in a zip");
    // omega rises toward 12 with the lag
    OrbitRig r2;
    r2.begin({}, 0.f, p);
    const auto far = r2.update(at({0, 0, -50}), p);
    check(far.omega > 11.f, "catch-up toward 12 rad/s");
    // SOFT lags more than TIGHT
    RigParams soft = p, tight = p;
    soft.follow = 0;
    tight.follow = 2;
    OrbitRig a, b;
    a.begin({}, 0.f, soft);
    b.begin({}, 0.f, tight);
    Vec3 f{};
    float la = 0, lb = 0;
    for (int i = 0; i < 90; ++i) {
        f.x += 8.f * dt;
        la = a.update(at(f), soft).lag;
        lb = b.update(at(f), tight).lag;
    }
    check(la > lb, "SOFT trails more than TIGHT");
    check(horizontalOmega(0) == 4.f && horizontalOmega(1) == 6.f && horizontalOmega(2) == 9.f, "4 / 6 / 9 rad/s");
}
void vignette() {
    OrbitRig rig;
    RigParams p;
    rig.begin({}, 0.f, p);
    Vec3 f{};
    float rest = 0;
    for (int i = 0; i < 90; ++i) rest = rig.update(at(f), p).vignette;
    check(rest < .001f, "still: no vignette");
    float fast = 0;
    for (int i = 0; i < 360; ++i) {
        f.z -= 20.f * dt;
        fast = rig.update(at(f), p).vignette;
    }
    check(fast > .2f && fast <= .25f + 1e-4f, "LIGHT: 25 % at swing speed");
    RigParams strong = p;
    strong.vignette = 2;
    OrbitRig s;
    s.begin({}, 0.f, strong);
    Vec3 g{};
    float v2 = 0;
    for (int i = 0; i < 360; ++i) {
        g.z -= 20.f * dt;
        v2 = s.update(at(g), strong).vignette;
    }
    check(v2 > .38f && v2 <= .45f + 1e-4f, "STRONG: 45 %");
    RigParams off = p;
    off.vignette = 0;
    OrbitRig z;
    z.begin({}, 0.f, off);
    Vec3 h{};
    float v3 = 1;
    for (int i = 0; i < 200; ++i) {
        h.z -= 20.f * dt;
        v3 = z.update(at(h), off).vignette;
    }
    check(v3 == 0.f, "OFF");
    check(ramp(5.f, 3.f, 12.f) > 0 && ramp(2.f, 3.f, 12.f) == 0 && ramp(13.f, 3.f, 12.f) == 1, "starts at 3 m/s, full at 12");
}
void pullIn() {
    OrbitRig rig;
    RigParams p;
    p.angleDeg = 0;
    rig.begin({}, 0.f, p);
    auto in = at({});
    // the game's camera sits behind him at 2.5 m in open space
    in.stockOk = true;
    in.stock = {0, 1.5f, 2.5f};
    for (int i = 0; i < 90; ++i) rig.update(in, p);
    // a wall: the game's camera is pulled in to 1.2 m
    in.stock = {0, 1.5f, 1.2f};
    OrbitRig::Output o;
    for (int i = 0; i < 45; ++i) o = rig.update(in, p);
    check(o.pull > 0 && o.position.z < 1.4f, "pulled in fast in front of the wall");
    check(o.closeFade == 0.f, "1.2 m is not too close");
    // too near him: black
    in.stock = {0, 1.5f, .5f};
    for (int i = 0; i < 90; ++i) o = rig.update(in, p);
    check(o.closeFade > .9f, "nearer than 0.8 m: the picture is black");
    // out again: slowly
    in.stock = {0, 1.5f, 2.5f};
    o = rig.update(in, p);
    check(o.position.z < 1.f, "pushed out slowly");
    for (int i = 0; i < 600; ++i) o = rig.update(in, p);
    check(o.position.z > 2.4f, "and all the way");
    // a stock camera on the other side of him says nothing about ours
    in.stock = {0, 1.5f, -1.f};
    for (int i = 0; i < 90; ++i) o = rig.update(in, p);
    check(o.pull == 0.f && o.position.z > 2.4f, "the game's camera elsewhere is no limit");
}
void latch() {
    Latch l;
    bool want{};
    const auto run = [&](bool enabled, bool active, float seconds) {
        for (float s = 0; s < seconds - 1e-6f; s += dt) want = l.update(enabled, active, dt);
    };
    // OFF: the chord is the camera
    run(false, true, .5f);
    check(want, "held: out");
    run(false, false, .5f);
    check(!want && !l.latched(), "let go: back");
    // ON: one press and a let-go goes back
    l.reset();
    run(true, true, .5f);
    run(true, false, 2.f);
    check(!want, "a single press does not latch");
    // a second press within 0.8 s of the first's end latches after the let-go
    l.reset();
    run(true, true, .5f);
    run(true, false, .5f);
    run(true, true, .5f);
    run(true, false, .1f);
    check(want && l.latched(), "double press: stays out after the let-go");
    run(true, false, 5.f);
    check(want, "and stays");
    // the next press takes it off, the let-go leaves
    run(true, true, .3f);
    check(want && !l.latched(), "pressed again: held, latch off");
    run(true, false, .1f);
    check(!want, "let go: back");
    // too slow a second press does not latch
    l.reset();
    run(true, true, .5f);
    run(true, false, 1.5f);
    run(true, true, .5f);
    run(true, false, .1f);
    check(!want && !l.latched(), "1.5 s later is not a double press");
    // turned off while latched: gone
    l.reset();
    run(true, true, .5f);
    run(true, false, .3f);
    run(true, true, .5f);
    run(true, false, .1f);
    check(l.latched(), "latched");
    run(false, false, .1f);
    check(!want && !l.latched(), "the row turned off");
    l.reset();
    run(true, true, .5f);
    run(true, false, .3f);
    run(true, true, .5f);
    run(true, false, .1f);
    l.clear();
    run(true, false, .1f);
    check(!want, "a forced exit clears it");
}
void autoMoments() {
    Auto a;
    check(!a.update(dt, true), "nothing triggered: not wanted");
    a.trigger(Why::dodge);
    bool want = false;
    for (int i = 0; i < 20; ++i) want = a.update(dt, true);
    check(want && a.why() == Why::dodge, "a dodge on: out");
    float t = 0;
    while ((want = a.update(dt, false)) && t < 1.f) t += dt;
    check(!want && t > .27f && t < .33f, "0.3 s after the dodge ends");
    // never seen: gives up after 0.5 s and the tail
    a.trigger(Why::dodge);
    t = 0;
    while (a.update(dt, false) && t < 2.f) t += dt;
    check(t > .75f && t < .85f, "a dodge never seen is dropped");
    // a dodge again in the tail keeps it
    a.trigger(Why::dodge);
    for (int i = 0; i < 9; ++i) a.update(dt, true);
    for (int i = 0; i < 18; ++i) a.update(dt, false);
    check(a.update(dt, true), "a second dodge inside the tail keeps it out");
    // the cap
    a.trigger(Why::dodge);
    t = 0;
    while (a.update(dt, true) && t < 6.f) t += dt;
    check(t > 3.9f && t < 4.1f, "never out longer than 4 s");
    a.trigger(Why::dodge);
    a.clear();
    check(!a.update(dt, true), "cleared");
}
} // namespace
int main() {
    springSettles();
    placement();
    yawOnlyFromTheStick();
    snapOrbit();
    heightStick();
    followAndLag();
    vignette();
    pullIn();
    latch();
    autoMoments();
    std::puts("showtime rig tests passed");
}
