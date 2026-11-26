// WALL FEEL (0.8.7-na-beta3n, wall_feel.hpp): the wall run back on a slide (WALL RUN PUSH), the grip fling
// off a wall any way (WALL FLING ANY WAY) and the soft palms (STICKY PALMS), on a simulated wall; each off is
// beta3m's wall. Portable: no Windows, no headset, no game.
#include "spidy/game_swing.hpp"
#include "spidy/swing.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <utility>

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
bool near(float a, float b, float eps = 1e-3f) {
    return std::isfinite(a) && std::abs(a - b) <= eps;
}
// A block from x = 5 to 15: its face at x = 5 looks down -x (core_tests' Building).
struct Building : World {
    Vec3 low{5, 0, -200}, high{15, 300, 200};
    std::optional<RayHit> raycast(Vec3 o, Vec3 d, float distance) const override {
        const float from[3]{o.x, o.y, o.z}, way[3]{d.x, d.y, d.z}, lo[3]{low.x, low.y, low.z},
            hi[3]{high.x, high.y, high.z};
        float enter = 0, leave = distance, normal[3]{};
        for (int axis = 0; axis < 3; ++axis) {
            if (std::abs(way[axis]) < 1e-8f) {
                if (from[axis] < lo[axis] || from[axis] > hi[axis])
                    return {};
                continue;
            }
            float first = (lo[axis] - from[axis]) / way[axis], second = (hi[axis] - from[axis]) / way[axis];
            float out = -1;
            if (first > second) {
                std::swap(first, second);
                out = 1;
            }
            if (first > enter) {
                enter = first;
                normal[0] = normal[1] = normal[2] = 0;
                normal[axis] = out;
            }
            leave = std::min(leave, second);
            if (enter > leave)
                return {};
        }
        if (enter <= 0)
            return {};
        return RayHit{o + d * enter, {normal[0], normal[1], normal[2]}, 7, true};
    }
    MoveResult sweep(Vec3, Vec3 to, float) const override {
        return {to, {}, false};
    }
    bool exists(std::uint64_t id) const override {
        return id == 7;
    }
};
SwingConfig walled() {
    SwingConfig c;
    c.walls = true;
    return c;
}
constexpr float dt = 1.f / 60;
struct WallRig {
    Building w;
    Swing s{walled()};
    Input in;
    Body body{{3.5f, 100, 0}, {10, 0, -10}, false};
    void advance(int frames = 1) {
        for (int i = 0; i < frames; ++i) {
            const auto motion = s.predictNativeStep(dt, in, w, body);
            check(motion.valid, "a prediction failed");
            body = {motion.target, motion.velocity, false};
        }
    }
    // Onto the wall, sliding (GRIP TO STICK on, 50%).
    void onWall() {
        s.wallGripToStick(true, 50);
        for (int i = 0; i < 30 && !s.wall().on; ++i)
            advance();
        check(s.wall().on && !s.wall().stuck, "the wall did not take the body, sliding");
    }
};
} // namespace

int main() {
    // The blend's own steps.
    check(wall_feel::blend(0, 1, .25f, .125f) == .5f, "half the blend time: half way");
    check(wall_feel::blend(.5f, 0, .25f, .125f) == 0, "let go: back down");
    check(wall_feel::blend(0, .2f, .25f, .1f) == 0, "a stick barely at the wall starts nothing");
    check(wall_feel::blend(0, 1, 0, .01f) == 1 && wall_feel::blend(1, 0, 0, .01f) == 0, "0 ms: at once");
    check(wall_feel::blend(NAN, NAN, .25f, NAN) == 0, "bad input is no run");
    check(near(wall_feel::mix({0, -4, 0}, {0, 6, 0}, .5f).y, 1), "the mix");

    // The side channel: version 1 (beta3m's XR) still taken, version 2 carries the new switches.
    {
        game_swing::WallStickAsk a;
        a.flags = 3;
        check(game_swing::validWallStick(a), "a version 2 ask with WALL RUN PUSH was refused");
        a.runBlendMs = 1001;
        check(!game_swing::validWallStick(a), "a blend past 1000 ms was taken");
        game_swing::WallStickAsk old;
        old.version = 1;
        old.flags = 1;
        check(game_swing::validWallStick(old), "beta3m's version 1 ask was refused");
        old.flags = 3;
        check(!game_swing::validWallStick(old), "a version 1 ask with a version 2 switch was taken");
    }
    // WALL RUN PUSH off: beta3m - the stick pushed at the wall does nothing; the slide falls.
    {
        WallRig r;
        r.onWall();
        check(!r.s.wallRunPushEnabled(), "off by default in the solver (XR sends it)");
        r.in.move = {1, 0, 0};
        const float y = r.body.position.y;
        r.advance(60);
        check(r.s.wall().on && !r.s.wall().stuck && r.body.position.y < y - 1.f, "off, the stick ran the slide");
        check(r.s.wallRunBlend() == 0, "off, a blend");
    }
    // On: the stick at the wall smoothly turns the slide into the run up it; let go, it slides again.
    {
        WallRig r;
        r.s.wallRunPush(true, 250);
        r.onWall();
        r.advance(20);
        const float falling = r.body.velocity.y;
        check(falling < -1, "the slide did not slide first");
        r.in.move = {1, 0, 0};
        r.advance(3);
        check(r.s.wallRunBlend() > 0 && r.s.wallRunBlend() < .5f, "the blend did not start smoothly");
        check(r.body.velocity.y < 0, "the run snapped in at once");
        r.advance(30);
        check(near(r.s.wallRunBlend(), 1), "the blend did not reach the run");
        check(r.body.velocity.y > 2, "pushed at the wall, the body did not run up it");
        check(r.s.wall().on && !r.s.wall().stuck, "the run stuck the body (only a grip sticks)");
        // Angled at the wall and along it: up and along, the run's speed along it kept.
        r.in.move = normalized(Vec3{1, 0, -1});
        r.advance(30);
        check(r.body.velocity.y > .5f && r.body.velocity.z < -1, "angled, the run did not go up and along");
        // Along the wall only is no push: the slide comes back, smoothly.
        r.in.move = {0, 0, -1};
        r.advance(3);
        check(r.s.wallRunBlend() < 1 && r.s.wallRunBlend() > .5f, "let go, the run did not blend out smoothly");
        r.in.move = {};
        r.advance(60);
        check(r.s.wallRunBlend() == 0 && r.body.velocity.y < -1, "let go, the body did not slide again");
        // A high-speed run keeps its speed along the wall while pushed.
        WallRig fast;
        fast.s.wallRunPush(true, 0);
        fast.body.velocity = {6, 0, -20};
        fast.onWall();
        fast.in.move = normalized(Vec3{.5f, 0, -1});
        fast.advance(30);
        check(fast.body.velocity.z < -12, "the run lost its speed along the wall");
        check(fast.body.velocity.y > -1, "a pushed run fell like a slide");
    }
    // WALL FLING ANY WAY: gripping the wall (GRIP TO STICK, hand climbing off: the owner's settings), a pull
    // let go of flings him off it any way it points out; off, only a hard push straight out leaves (beta3m).
    {
        check(wall_feel::flingLeaves(.5f, true) && !wall_feel::flingLeaves(.5f, false) &&
                  wall_feel::flingLeaves(2.f, false) && !wall_feel::flingLeaves(-3.f, true) &&
                  !wall_feel::flingLeaves(NAN, true),
              "the leave speeds");
        const auto fling = [](bool anyWay, Vec3 pull) {
            WallRig r;
            r.s.gripSettings(false, true);
            r.s.wallFlingAnyWay(anyWay);
            r.body = {{4.1f, 50, 0}, {1, 0, 0}, false};
            r.s.wallGripToStick(true, 50);
            r.advance();
            check(r.s.wall().on, "the fling body is not on the wall");
            auto& h = r.in.hands[0];
            h.tracked = true;
            h.aim.orientation = arc({0, 0, -1}, {1, 0, 0});
            h.gripRelativeToHead = {.4f, 0, 0};
            h.wallPosition = {4.95f, r.body.position.y, 0};
            r.advance();
            h.wallPosition = {4.95f, r.body.position.y, 0};
            h.wallGrip = 1;
            r.advance();
            check(r.s.wall().stuck, "the grip did not stick the body");
            for (int i = 0; i < 10; ++i) {
                h.gripRelativeToHead += pull / 60.f;
                h.wallPosition = Vec3{4.95f, r.body.position.y, 0};
                r.advance();
            }
            h.wallGrip = 0;
            r.advance();
            return std::pair{r.s.wall().on, r.body.velocity};
        };
        // The hand drawn down and in at the wall (1 m/s each): the body goes up and out.
        const Vec3 upAndOut{1, -1, 0};
        const auto [onAfter, v] = fling(true, upAndOut);
        check(!onAfter, "on, an up-and-out fling did not leave the wall");
        check(v.x < -.5f && v.y > .5f, "on, the fling did not go along its own way (up and out)");
        const auto [offOn, offV] = fling(false, upAndOut);
        check(offOn && std::abs(offV.x) < .05f, "off, a soft push out left the wall (beta3m strokes it along)");
        // Pulled away from the wall (a fling into it) still strokes along it, on or off.
        const auto [inOn, inV] = fling(true, Vec3{-1, 0, 1});
        check(inOn && inV.z < -.5f, "a fling into the wall left it, or did not stroke along it");
    }
    // STICKY PALMS: an open palm on the slide's wall holds softly, only while it touches.
    {
        check(near(length(wall_feel::palms({0, -6, 0}, {-1, 0, 0}, {}, 1, 1, .5f, 65)), 6 * std::exp(-2.f), .01f),
              "a still palm's half second");
        check(near(wall_feel::palms({0, -6, 0}, {-1, 0, 0}, {}, 0, 1, .5f, 65).y, -6), "no palm, no hold");
        check(near(wall_feel::palms({0, -6, 0}, {-1, 0, 0}, {}, 1, 0, .5f, 65).y, -6), "0%, no hold");
        check(wall_feel::palms({0, 0, -8}, {-1, 0, 0}, {0, 0, 3}, 1, 1, .1f, 65).z < -8.5f, "a back sweep added nothing");
        check(wall_feel::palms({0, 0, -8}, {-1, 0, 0}, {0, 0, -3}, 1, 1, .1f, 65).z > -7.5f, "a forward sweep did not brake");
        check(near(wall_feel::palms({0, 0, -8}, {-1, 0, 0}, {5, 0, 0}, 1, 1, .1f, 65).x, 0), "a palm pushed into the wall");
        check(length(wall_feel::palms({0, 0, -8}, {-1, 0, 0}, {0, 0, 40}, 2, 2, 1, 10)) <= 10.001f, "past the most");
        check(length(wall_feel::smoothSweep({}, {0, 0, 30}, .01f)) == 0, "a tracking jump taken as a sweep");
        const auto slide = [](bool on, float squeeze, Vec3 sweepPerSecond, float seconds) {
            WallRig r;
            r.s.stickyPalms(on, 100);
            r.onWall();
            r.advance(20);
            auto& h = r.in.hands[1];
            h.tracked = true;
            h.aim.orientation = arc({0, 0, -1}, {1, 0, 0});
            h.gripRelativeToHead = {.4f, 0, 0};
            h.wallGrip = squeeze;
            const float before = r.body.velocity.y;
            unsigned touched = 0;
            for (int i = 0; i < static_cast<int>(seconds * 60); ++i) {
                h.wallPosition = {4.95f, r.body.position.y, r.body.position.z};
                h.gripRelativeToHead += sweepPerSecond / 60.f;
                r.advance();
                touched |= r.s.stickyPalmHands();
            }
            return std::tuple{before, r.body.velocity, touched, r.s.wall().stuck};
        };
        // Held still on the wall: the slide slows, it does not stop dead or stick.
        const auto [before, still, touched, stuck] = slide(true, 0, {}, .5f);
        check(touched == 2u && !stuck, "the open palm did not touch, or it stuck the body");
        check(before < -2 && still.y > before * .5f && still.y < -.05f, "a still palm did not slow the slide softly");
        // Off: beta3m, the palm does nothing.
        const auto [offBefore, off, offTouched, offStuck] = slide(false, 0, {}, .5f);
        check(!offTouched && off.y < offBefore, "off, a palm held the slide");
        // A palm squeezed is a grip, not a palm (GRIP TO STICK sticks the body).
        const auto [gb, gv, gripTouched, gripStuck] = slide(true, 1, {}, .1f);
        check(!gripTouched && gripStuck, "a grip counted as a palm");
        // Swept back along the wall (+z, the body runs -z): the run gains speed along the wall.
        WallRig run;
        run.s.stickyPalms(true, 100);
        run.s.wallRunPush(true, 0);
        run.body.velocity = {6, 0, -8};
        run.onWall();
        run.in.move = normalized(Vec3{.5f, 0, -1});
        run.advance(10);
        const float runSpeed = -run.body.velocity.z;
        auto& h = run.in.hands[0];
        h.tracked = true;
        h.aim.orientation = arc({0, 0, -1}, {1, 0, 0});
        h.gripRelativeToHead = {.4f, 0, 0};
        for (int i = 0; i < 20; ++i) {
            h.wallPosition = {4.95f, run.body.position.y, run.body.position.z};
            h.gripRelativeToHead.z += 3.f / 60; // a 3 m/s stroke back
            run.advance();
        }
        check(-run.body.velocity.z > runSpeed + 1, "a palm swept back did not push the run on");
        // Let go of the wall: nothing holds.
        h.wallPosition = {3.5f, run.body.position.y, 0};
        run.advance();
        check(run.s.stickyPalmHands() == 0, "a palm off the wall still held");
    }
    std::printf("%d wall feel checks passed\n", passed);
    return 0;
}
