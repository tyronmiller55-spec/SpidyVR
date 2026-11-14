// GROUND KICKS (ground_kick.hpp, K3; November 14): the stick's flicks and half circle, and the kick standing in
// the swing module (air_kick.hpp's Kick with the ground setting): no lunge, the speed gate, the sweep, the
// knock-down's flight. Portable: no Windows, no headset, no game.
#include "spidy/air_kick.hpp"
#include "spidy/ground_kick.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace spidy;
using air_kick::Gesture;
using air_kick::Kick;
using air_kick::Refusal;
using air_kick::Zone;
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
using Samples = std::vector<std::pair<float, float>>;
struct Fired {
    air_kick::Flick last;
    int count{};
};
Fired run(const Samples& stick, float dt = .011f, bool engaged = true) {
    ground_kick::Gestures g;
    Fired f;
    for (const auto& [x, y] : stick) {
        const auto k = g.update(dt, x, y, engaged);
        if (k.gesture != Gesture::none) {
            f.last = k;
            ++f.count;
        }
    }
    return f;
}
std::pair<float, float> at(float degrees, float size) {
    return {std::sin(degrees * air_kick::deg) * size, std::cos(degrees * air_kick::deg) * size};
}
void rest(Samples& s, int n = 5) {
    for (int i = 0; i < n; ++i)
        s.push_back({0, 0});
}
// A flick: rest, then 0.5 and 1.0 at `degrees` within 22 ms, held `hold` samples.
Samples flickAt(float degrees, int hold = 5) {
    Samples s;
    rest(s);
    s.push_back(at(degrees, .5f));
    s.push_back(at(degrees, 1.f));
    for (int i = 0; i < hold; ++i)
        s.push_back(at(degrees, 1.f));
    return s;
}
// A half circle from the `from` side (+-90) through the bottom to the other, `steps` samples of 11 ms.
Samples halfCircle(int from, int steps, float size = 1.f) {
    Samples s;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps; // 0..1
        const float degrees = static_cast<float>(from) * (90.f + 180.f * t); // 90 .. 270 = through 180
        s.push_back(at(degrees, size));
    }
    return s;
}
Kick::Target enemy(std::uint64_t id, Vec3 feet) {
    return {id, feet, 1.8f, .3f};
}
// Standing: the ground setting on, the air's off, slow.
Kick::Frame standing(std::span<const Kick::Target> targets) {
    Kick::Frame f;
    f.feet = {0, 10, 0};
    f.eyeOffset = {0, 1.6f, 0};
    f.gaze = {0, 0, -1};
    f.targets = targets;
    f.grounded = true;
    f.ground = true;
    f.enabled = false;
    f.speed = 0;
    return f;
}
struct Seen {
    int hits{}, misses{};
    Kick::Output hit;
    float maxFoot{}, lowest = 1e9f, highest = -1e9f;
};
Seen play(Kick& k, Kick::Frame f, float seconds = 1.f) {
    Seen s;
    const float dt = 1.f / 90.f;
    f.seconds = dt;
    for (float t = 0; t < seconds && k.busy(); t += dt) {
        const auto o = k.update(f);
        s.maxFoot = std::max(s.maxFoot, o.foot);
        if (k.busy()) {
            s.lowest = std::min(s.lowest, k.foot().y);
            s.highest = std::max(s.highest, k.foot().y);
        }
        if (o.event == Kick::Event::hit) {
            ++s.hits;
            s.hit = o;
        } else if (o.event == Kick::Event::miss) {
            ++s.misses;
        }
    }
    return s;
}
} // namespace

int main() {
    // ---- the stick: flicks ----
    {
        const auto up = run(flickAt(0));
        check(up.count == 1 && up.last.gesture == Gesture::freeform && near(up.last.angle, 0, 1.f),
              "a flick up is a front kick (freeform)");
        const auto diag = run(flickAt(45));
        check(diag.count == 1 && diag.last.gesture == Gesture::freeform && diag.last.side == 1,
              "up-right is a front kick of the right leg");
        const auto diagL = run(flickAt(-45));
        check(diagL.count == 1 && diagL.last.side == -1, "up-left, the left");
        check(run(flickAt(180)).count == 0 && run(flickAt(150)).count == 0, "a flick down is nothing");
        // A slow push is the turn's, never a kick.
        Samples slow;
        rest(slow);
        for (int i = 1; i <= 20; ++i)
            slow.push_back(at(0, i * .05f));
        check(run(slow).count == 0, "a slow push kicks nothing");
        check(run(flickAt(0), .011f, false).count == 0, "not engaged: nothing");
        Samples bad = flickAt(0);
        bad[6] = {NAN, 1.f};
        check(run(bad).count <= 1, "a NaN sample does not crash");
    }
    // ---- a sideways flick waits for a sweep ----
    {
        // The flick to the right side, held there: after 0.6 s a side kick.
        Samples held = flickAt(90, 0);
        for (int i = 0; i < 70; ++i)
            held.push_back(at(90, 1.f));
        const auto side = run(held);
        check(side.count == 1 && side.last.gesture == Gesture::freeform && side.last.side == 1,
              "a flick sideways, held, is a side kick after the wait");
        // The flick, the stick back at once: a side kick when it is back, well before the wait.
        Samples back = flickAt(90, 2);
        back.push_back(at(90, .4f));
        back.push_back({0, 0});
        const auto early = run(back);
        check(early.count == 1 && early.last.gesture == Gesture::freeform, "the stick back: the side kick");
        // The flick to the right side then the half circle through the bottom to the left: a sweep.
        Samples sweep = flickAt(90, 2);
        const auto circle = halfCircle(1, 24);
        sweep.insert(sweep.end(), circle.begin() + 1, circle.end());
        const auto s = run(sweep);
        check(s.count == 1 && s.last.gesture == Gesture::sweep && s.last.side == 1,
              "flick then a half circle across the bottom is a sweep, from the side it began on");
        // The same from the left.
        Samples left = flickAt(-90, 2);
        const auto circleL = halfCircle(-1, 24);
        left.insert(left.end(), circleL.begin() + 1, circleL.end());
        const auto l = run(left);
        check(l.count == 1 && l.last.gesture == Gesture::sweep && l.last.side == -1, "and the other way round");
        // Drawn from a stick resting at the side: a sweep too, no kick before it.
        Samples drawn;
        rest(drawn);
        for (int i = 1; i <= 10; ++i)
            drawn.push_back(at(90, .1f * i)); // slow to the rim
        for (int i = 0; i < 10; ++i)
            drawn.push_back(at(90, 1.f));
        const auto c2 = halfCircle(1, 24);
        drawn.insert(drawn.end(), c2.begin() + 1, c2.end());
        const auto d = run(drawn);
        check(d.count == 1 && d.last.gesture == Gesture::sweep, "a half circle drawn from a resting stick sweeps");
        // Too slow: 0.6 s is the limit (here 40 samples of 22 ms).
        Samples slowArc = flickAt(90, 0);
        const auto slowCircle = halfCircle(1, 40);
        Fired slowRun;
        {
            ground_kick::Gestures g;
            for (const auto& [x, y] : slowArc)
                g.update(.011f, x, y, true);
            for (const auto& [x, y] : slowCircle) {
                const auto k = g.update(.022f, x, y, true);
                if (k.gesture != Gesture::none) {
                    slowRun.last = k;
                    ++slowRun.count;
                }
            }
        }
        check(slowRun.count == 1 && slowRun.last.gesture == Gesture::freeform,
              "too slow a half circle is the side kick, not a sweep");
        // A half circle that stays near the centre (under 0.5) is no sweep.
        Samples small = flickAt(90, 2);
        const auto smallCircle = halfCircle(1, 24, .3f);
        small.insert(small.end(), smallCircle.begin() + 1, smallCircle.end());
        const auto sm = run(small);
        check(sm.last.gesture != Gesture::sweep, "a small half circle is no sweep");
        // A drawn stick at a side that is never completed: no kick at all (no turning kicks).
        Samples turn;
        rest(turn);
        for (int i = 1; i <= 10; ++i)
            turn.push_back(at(90, .1f * i));
        for (int i = 0; i < 80; ++i)
            turn.push_back(at(90, 1.f));
        check(run(turn).count == 0, "holding the stick at a side to turn is no kick");
        // One sweep fires once.
        check(s.count == 1 && d.count == 1, "a sweep fires once");
    }
    // ---- the kick standing ----
    {
        std::vector<Kick::Target> none;
        Kick k;
        auto f = standing(none);
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).event == Kick::Event::started, "a kick starts standing");
        k.reset();
        f.ground = false;
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).refusal == Refusal::grounded,
              "ground kicks off: refused on the ground");
        f.ground = true;
        f.speed = 2.5f;
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).refusal == Refusal::moving, "faster than 2 m/s: refused");
        f.speed = NAN;
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).refusal == Refusal::moving, "a speed that is not a number: refused");
        f.speed = 1.9f;
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).event == Kick::Event::started, "1.9 m/s is slow enough");
        k.reset();
        f.legs = false;
        check(k.ask(f, {Gesture::freeform, 1, 0, 1}).refusal == Refusal::legs, "the legs must be engaged");
        f.legs = true;
        f.grounded = false;
        check(k.ask(f, {Gesture::sweep, 1, 90, 1}).refusal == Refusal::off, "air kicks off, in the air: off");
        f.enabled = true;
        check(k.ask(f, {Gesture::sweep, 1, 90, 1}).refusal == Refusal::groundOnly, "a sweep is for the ground");
        check(k.ask(f, {Gesture::dive, 1, 0, 1}).event == Kick::Event::started,
              "an air kick still works with ground kicks on");
        k.reset();
        // The two settings are separate: ground on, air off, in the air: refused.
        f.enabled = false;
        check(k.ask(f, {Gesture::dive, 1, 0, 1}).refusal == Refusal::off, "ground kicks do not allow air kicks");
    }
    {
        // A front kick at an enemy 0.7 m ahead at the chest: the foot reaches 0.8 m (0.7 of 1.15, no lunge).
        Kick k;
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -1.f})};
        const auto f = standing(ts);
        const auto o = k.ask(f, {Gesture::freeform, 1, 0, 1});
        check(o.event == Kick::Event::started && o.target == 7, "the gaze picks him");
        const Seen s = play(k, f);
        check(s.hits == 1 && s.hit.target == 7 && s.hit.strength > 0, "the front kick lands");
        check(s.hit.fly ? s.hit.enemy.y <= 1.f && length(Vec3{s.hit.enemy.x, 0, s.hit.enemy.z}) <= 6.f + 1e-3f : true,
              "a ground blow is a knock-down: flat and at most 6 m/s");
        check(k.onGround(), "the kick knows it began standing");
    }
    {
        // No lunge: an enemy 1.6 m away (chest) is out of a standing freeform's reach, in the air's.
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -1.7f})};
        Kick g;
        const auto o = g.ask(standing(ts), {Gesture::freeform, 1, 0, 1});
        check(o.target == 0, "too far to kick standing (no lunge)");
        Kick a;
        auto f = standing(ts);
        f.grounded = false;
        f.enabled = true;
        f.feet = {0, 10, 0};
        const auto oa = a.ask(f, {Gesture::freeform, 1, 0, 1});
        check(oa.target == 7, "the same enemy is in the air kick's reach");
    }
    {
        // The sweep: low, from the side through the front, takes the legs.
        Kick k;
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -.8f})};
        const auto f = standing(ts);
        const auto o = k.ask(f, {Gesture::sweep, 1, 90, 1});
        check(o.event == Kick::Event::started && o.target == 7 && o.zone == Zone::legs,
              "a sweep goes for the legs");
        const Seen s = play(k, f);
        check(s.hits == 1 && s.hit.zone == Zone::legs, "the sweep lands on the legs");
        check(s.lowest > 10.f && s.highest < 10.4f && near(s.lowest, 10.12f, .01f) && near(s.highest, 10.12f, .01f),
              "the foot stays ankle high (12 cm) all the way round");
        check(s.hit.strength > .4f && s.hit.fly, "a sweep is hard enough to knock him down");
        check(s.hit.enemy.y <= .5f + 1e-3f && length(Vec3{s.hit.enemy.x, 0, s.hit.enemy.z}) <= 6.f + 1e-3f,
              "the knock-down is low and at most 6 m/s");
    }
    {
        // The sweep's foot at the start is on the side, at the end on the other side, through the front.
        const air_kick::Config c;
        check(near(air_kick::sweepYaw(Gesture::sweep, 1, 0, c), 90.f * air_kick::deg, 1e-4f), "starts at the side");
        check(near(air_kick::sweepYaw(Gesture::sweep, 1, c.sweepExtend, c), -90.f * air_kick::deg, 1e-4f),
              "ends at the other");
        check(near(air_kick::sweepYaw(Gesture::sweep, -1, c.sweepExtend * .5f, c), 0, 1e-4f), "through the front");
        check(near(air_kick::reachAt(Gesture::sweep, 0, c), c.sweepReach) &&
                  near(air_kick::reachAt(Gesture::sweep, .2f, c), c.sweepReach),
              "round the pelvis at 0.9 m");
        check(air_kick::zonesFor(Gesture::sweep) == (1u << static_cast<int>(Zone::legs)), "legs only");
        const auto air = air_kick::legPose(Gesture::sweep, 1, 90, .2f);
        check(near(air[0].pitch, 0) && near(air[1].pitch, 0), "a sweep has no air pose");
        check(std::string(air_kick::gestureName(Gesture::sweep)) == "sweep", "the name");
        // The knock-down's flight.
        const Vec3 fl = air_kick::groundFlight({1, 0, 0}, 20.f, c);
        check(near(fl.x, 6.f) && near(fl.y, .5f), "the flight is capped and flat");
        const Vec3 weak = air_kick::groundFlight({0, 0, -1}, 4.f, c);
        check(near(weak.z, -2.f) && near(weak.y, .5f), "and half the foot's speed");
    }
    {
        // Lost: the player runs, jumps or switches it off: the kick ends where it is.
        std::vector<Kick::Target> none;
        Kick k;
        auto f = standing(none);
        f.seconds = .02f;
        k.ask(f, {Gesture::freeform, 1, 0, 1});
        k.update(f);
        f.speed = 3.2f; // 2 + 1.5 slack = 3.5: still fine
        check(!k.update(f).lost, "a little speed does not end a standing kick");
        f.speed = 4.f;
        check(k.update(f).lost && !k.busy(), "running ends it");
        k.reset();
        k.ask(standing(none), {Gesture::freeform, 1, 0, 1});
        auto air = standing(none);
        air.grounded = false;
        air.enabled = true;
        air.seconds = .02f;
        check(k.update(air).lost && !k.busy(), "leaving the ground ends a standing kick");
        k.reset();
        k.ask(standing(none), {Gesture::freeform, 1, 0, 1});
        auto off = standing(none);
        off.ground = false;
        off.seconds = .02f;
        check(k.update(off).lost && !k.busy(), "switching it off ends it");
        // An air kick that lands also ends (as it did).
        k.reset();
        auto a = standing(none);
        a.grounded = false;
        a.enabled = true;
        k.ask(a, {Gesture::dive, 1, 0, 1});
        auto land = standing(none);
        land.seconds = .02f;
        check(k.update(land).lost && !k.busy(), "an air kick that lands ends");
    }
    std::printf("ground_kick: %d checks passed\n", passed);
    return 0;
}
