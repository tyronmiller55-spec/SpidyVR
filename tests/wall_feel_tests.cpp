// WALL FEEL (0.8.7-na-beta3n, wall_feel.hpp): the wall run back on a slide (WALL RUN PUSH), the grip fling
// off a wall any way (WALL FLING ANY WAY) and the soft palms (STICKY PALMS), on a simulated wall; each off is
// beta3m's wall. Portable: no Windows, no headset, no game.
#include "spidy/game_swing.hpp"
#include "spidy/swing.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
    std::printf("%d wall feel checks passed\n", passed);
    return 0;
}
