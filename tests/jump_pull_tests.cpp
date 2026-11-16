// JUMP PULL (jump_pull.hpp, the polish pass slice 8): who is picked (gaze cone, stick cone, range, nearness), how the
// pull fades and is capped per jump, the swing applying it in its own air only (no web, after the settle), and the
// settings' defaults. Portable.
#include "spidy/jump_pull.hpp"
#include "spidy/swing.hpp"
#include "spidy/vr_settings.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::jump_pull;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
struct Open : World {
    std::optional<RayHit> raycast(Vec3, Vec3, float) const override {
        return {};
    }
    MoveResult sweep(Vec3, Vec3 to, float) const override {
        return {to, {}, false};
    }
    bool exists(std::uint64_t) const override {
        return false;
    }
};
} // namespace

int main() {
    const Vec3 head{0, 1.6f, 0};
    Config on;
    on.on = true;
    const Vec3 gaze{0, 0, -1};
    const Candidate ahead{7, {0, 1, -6}};
    // The defaults the user was told: 20 deg gaze, 25 deg stick, 12 m, 1.5 m/s.
    check(on.gazeDeg == 20.f && on.stickDeg == 25.f && on.rangeM == 12.f && on.capMps == 1.5f, "defaults 20/25/12/1.5");
    {
        const vr_settings::Values v;
        check(v.jumpPull && v.jumpPullGazeDeg == 20 && v.jumpPullStickDeg == 25 && v.jumpPullRangeM == 12 &&
                  v.jumpPullCapDmps == 15,
              "settings defaults on, 20, 25, 12, 15");
    }
    // Off picks nobody.
    check(!choose(Config{}, head, gaze, {}, &ahead, 1).valid, "off picks nobody");
    // Straight ahead: picked by the gaze, at full weight (6 m is inside .6 of the range).
    {
        const Pick p = choose(on, head, gaze, {}, &ahead, 1);
        check(p.valid && p.id == 7 && !p.byStick && p.weight > .95f, "an enemy ahead is picked by the gaze at full weight");
    }
    // Out of the gaze cone and no stick: nobody. Inside the stick's cone with the stick held: picked by the stick.
    {
        const Candidate side{8, {-6, 1, -2}};
        check(!choose(on, head, gaze, {}, &side, 1).valid, "an enemy 70 deg off the gaze and no stick is not picked");
        const Pick p = choose(on, head, gaze, {-1, 0, -.3f}, &side, 1);
        check(p.valid && p.byStick && p.id == 8, "the stick toward him picks him");
        check(!choose(on, head, gaze, {1, 0, 0}, &side, 1).valid, "the stick away from him does not");
    }
    // Range and nearness: 14 m is out, 1 m is too near (no pulling into him), 11 m fades.
    {
        const Candidate far{1, {0, 1, -14}}, near{2, {0, 1, -.8f}}, edge{3, {0, 1, -11}};
        check(!choose(on, head, gaze, {}, &far, 1).valid, "past the range is out");
        check(!choose(on, head, gaze, {}, &near, 1).valid, "inside 1.5 m is out");
        const Pick p = choose(on, head, gaze, {}, &edge, 1);
        check(p.valid && p.weight < .6f && p.weight > 0.f, "near the range's end the weight fades");
    }
    // Two enemies: the nearer in angle wins.
    {
        const std::array<Candidate, 2> two{{{1, {-2.0f, 1.6f, -6}}, {2, {.3f, 1.6f, -6}}}};
        check(choose(on, head, gaze, {}, two.data(), two.size()).id == 2, "the enemy nearer the gaze wins");
    }
    // The cone's edge fades: 19 deg off has a small weight, 5 deg off has all.
    {
        const float r = 6.f;
        const auto at = [&](float deg) { return Candidate{1, {-std::sin(deg * degrees) * r, 1.6f, -std::cos(deg * degrees) * r}}; };
        const Candidate a = at(5.f), b = at(18.f);
        check(choose(on, head, gaze, {}, &a, 1).weight > .99f, "5 deg off: full weight");
        const float w = choose(on, head, gaze, {}, &b, 1).weight;
        check(w > 0.f && w < .5f, "18 deg off (cone 20): the fade is nearly out");
    }
    // Bad numbers are nobody.
    {
        const Candidate bad{1, {NAN, 0, 0}};
        check(!choose(on, head, gaze, {}, &bad, 1).valid, "a NaN enemy is nobody");
        check(!choose(on, head, {NAN, 0, 0}, {}, &ahead, 1).valid, "a NaN gaze picks nobody");
    }
    // The step: nothing during the settle, accelerates toward him, vertical share halved, the cap holds over many steps.
    {
        check(step(on, {}, {0, 0, -6}, 1.f, 1.f / 90, .05f, 0.f).added == 0.f, "the jump's first 0.15 s are left alone");
        const Step s = step(on, {}, {0, 6, -6}, 1.f, 1.f / 90, .5f, 0.f);
        check(s.added > 0 && std::fabs(s.added - 4.f / 90) < 1e-5f, "one step adds 4 m/s^2 * dt");
        check(std::fabs(length(s.dv) - s.added) < 1e-5f, "dv is the added speed along the line");
        check(s.dv.y < s.dv.z * -1.f && s.dv.y > 0, "the vertical share is smaller (a pull is not a lift)");
        float pulled = 0;
        Vec3 v{};
        for (int i = 0; i < 90 * 5; ++i) {
            const Step t = step(on, {}, {0, 0, -6}, 1.f, 1.f / 90, 1.f, pulled);
            v += t.dv;
            pulled += t.added;
        }
        check(std::fabs(pulled - 1.5f) < 1e-3f && length(v) <= 1.5f + 1e-3f, "a jump adds at most the cap (1.5 m/s)");
        check(step(on, {}, {0, 0, -6}, 1.f, 1.f / 90, 1.f, 1.5f).added == 0.f, "past the cap: nothing");
        check(step(on, {}, {0, 0, -6}, 0.f, 1.f / 90, 1.f, 0.f).added == 0.f, "weight 0: nothing");
        check(step(on, {}, {0, 0, -6}, 1.f, NAN, 1.f, 0.f).added == 0.f, "NaN dt: nothing");
        check(step(Config{}, {}, {0, 0, -6}, 1.f, 1.f / 90, 1.f, 0.f).added == 0.f, "off: nothing");
    }
    // The swing's own air: pulled toward the aim after the settle, none with it off, none with a web out.
    {
        const auto run = [](bool pull, float seconds, float* pulledOut, Vec3* velocity) {
            Open world;
            SwingConfig config;
            config.gravity = 0;
            config.airAcceleration = 0;
            Swing swing(config);
            Input in;
            Body body{{0, 3, 0}, {}, false};
            auto intent = swing.predictNativeStep(1.f / 90, in, world, body);
            for (int i = 0; i < static_cast<int>(seconds * 90); ++i) {
                body = {intent.target, intent.velocity, false};
                Config c;
                c.on = true;
                swing.jumpPull(pull, c, {0, 3, -8}, 1.f);
                intent = swing.predictNativeStep(1.f / 90, in, world, body);
            }
            *pulledOut = swing.jumpPullAdded();
            *velocity = intent.velocity;
        };
        float p0{}, p1{}, p2{};
        Vec3 v0, v1, v2;
        run(false, 3.f, &p0, &v0);
        run(true, 3.f, &p1, &v1);
        run(true, .1f, &p2, &v2);
        check(p0 == 0.f && length(v0) < 1e-4f, "off: the air is untouched");
        check(p1 > 1.4f && p1 <= 1.5f + 1e-3f && v1.z < -1.3f, "on: pulled toward the aim up to the cap");
        check(p2 == 0.f && length(v2) < 1e-4f, "the first 0.15 s are left alone in the swing too");
    }
    std::printf("jump pull tests passed (%d checks)\n", passed);
    return 0;
}
