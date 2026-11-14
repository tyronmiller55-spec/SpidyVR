// MANUAL SWING KICK (swing_kick.hpp, K1; November 14): the arm gate, the cone, the flick, the rope and
// steering assist's clamps, the foot's swept sphere, the kick's states, and the swing's assist in a
// simulated web. Portable: no Windows, no headset, no game.
#include "spidy/swing.hpp"
#include "spidy/swing_kick.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::swing_kick;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
bool same(Vec3 a, Vec3 b) {
    return length(a - b) < 1e-6f;
}
bool near(float a, float b, float eps = 1e-3f) {
    return std::isfinite(a) && std::abs(a - b) <= eps;
}
constexpr float deg = pi / 180.f;
ArmInput good() {
    ArmInput in;
    in.enabled = in.legs = in.swinging = in.target = true;
    in.speed = 12;
    in.distance = 8;
    in.cone = 1;
    return in;
}
// A world with one point anchor, as the swing's own tests have.
struct Anchor : World {
    Vec3 anchor{0, 20, 0};
    std::optional<RayHit> raycast(Vec3 o, Vec3 d, float distance) const override {
        const float t = dot(anchor - o, d);
        if (t > 0 && t <= distance && length(o + d * t - anchor) < .02f)
            return RayHit{anchor, {}, 1, true};
        return {};
    }
    MoveResult sweep(Vec3, Vec3 to, float) const override {
        return {to, {}, false};
    }
    bool exists(std::uint64_t id) const override {
        return id == 1;
    }
};
// Flicks and pushes: samples every 11 ms.
bool flicks(const std::vector<std::pair<float, float>>& stick, float dt = .011f) {
    Flick f;
    bool fired = false;
    for (const auto& [x, y] : stick)
        fired |= f.update(dt, x, y, true);
    return fired;
}
} // namespace

int main() {
    const Config c;
    // The arm gate: swinging, legs engaged, ringed enemy 3-14 m, 9 m/s, in the cone.
    {
        check(armCheck(good(), c) == Refusal::none, "a good swing arms");
        auto in = good();
        in.enabled = false;
        check(armCheck(in, c) == Refusal::off, "off: swing_kick_manual is off");
        in = good();
        in.legs = false;
        check(armCheck(in, c) == Refusal::legs, "legs not engaged");
        in = good();
        in.swinging = false;
        check(armCheck(in, c) == Refusal::notSwinging, "not swinging (no web, or the ground)");
        in = good();
        in.target = false;
        check(armCheck(in, c) == Refusal::noTarget, "nobody ringed");
        in = good();
        in.distance = 2.99f;
        check(armCheck(in, c) == Refusal::tooClose, "3 m is the least");
        in.distance = 3.f;
        check(armCheck(in, c) == Refusal::none, "3 m arms");
        in.distance = 14.f;
        check(armCheck(in, c) == Refusal::none, "14 m arms");
        in.distance = 14.01f;
        check(armCheck(in, c) == Refusal::tooFar, "14 m is the most");
        in = good();
        in.speed = 8.99f;
        check(armCheck(in, c) == Refusal::tooSlow, "9 m/s is the least");
        in.speed = 9.f;
        check(armCheck(in, c) == Refusal::none, "9 m/s arms");
        in = good();
        in.cone = std::cos(30 * deg);
        check(armCheck(in, c) == Refusal::none, "dot .866 is on the cone's edge");
        in.cone = .86f;
        check(armCheck(in, c) == Refusal::outsideCone, "outside the cone");
        in.distance = std::nanf("");
        check(armCheck(in, c) != Refusal::none, "a nan distance never arms");
        check(std::string(refusalName(Refusal::tooFar)) == "too far", "refusals have names");
    }
    // The cone is the VELOCITY's, not the view's.
    {
        const Vec3 v{0, -3, -10};
        check(inCone(v, {0, -2, -8}, 30), "an enemy ahead along the swing");
        check(!inCone(v, {10, 0, 0}, 30), "one off to the side is outside");
        check(!inCone(v, {0, 0, 10}, 30), "one behind is outside");
        check(near(coneDegrees({1, 0, 0}, {1, 1, 0}), 45, .01f), "45 degrees");
        // 29 degrees in, 31 out.
        const Vec3 a{std::sin(29 * deg), 0, -std::cos(29 * deg)},
            b{std::sin(31 * deg), 0, -std::cos(31 * deg)};
        check(inCone({0, 0, -1}, a, 30) && !inCone({0, 0, -1}, b, 30), "30 degrees is the edge");
        check(coneDot({}, {1, 0, 0}) == 0 && !inCone({}, {1, 0, 0}, 30), "no velocity, no cone");
        check(near(std::cos(30 * deg), .866f, 1e-3f), "cos 30 = .866");
    }
    // The flick: under 0.3 to over 0.8 within 80 ms; a slow push stays the pose; up only.
    {
        check(flicks({{0, 0}, {0, .95f}}), "rest to a full push in one sample is a flick");
        check(flicks({{0, .1f}, {0, .5f}, {0, .95f}}), "three samples (22 ms) is a flick");
        check(flicks({{0, .29f}, {0, .5f}, {0, .7f}, {0, .85f}}), "0.3 to 0.8 in 33 ms is a flick");
        std::vector<std::pair<float, float>> slow{{0, 0}};
        for (int i = 1; i <= 20; ++i)
            slow.push_back({0, i * .05f});
        check(!flicks(slow), "a slow push (220 ms) is the jackknife pose, not a kick");
        check(!flicks({{0, 0}, {0, .6f}}), "a push that does not pass 0.8 is not a flick");
        check(!flicks({{0, 0}, {0, -.95f}}), "down is not a kick");
        check(!flicks({{0, 0}, {.95f, 0}}), "sideways is not a kick");
        check(!flicks({{0, 0}, {.95f, .1f}}), "nearly sideways is not a kick");
        check(flicks({{0, 0}, {.5f, .8f}}), "up and a little right is (within 50 degrees of up)");
        // Over 80 ms: 0.29 ... then 90 ms to 0.9.
        check(!flicks({{0, .2f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .5f},
                       {0, .95f}}),
              "100 ms past the rest mark is a push");
        // One flick per push: held up, no second; it needs the stick to rest again.
        Flick f;
        int fired = 0;
        for (float y : {0.f, .95f, .95f, .95f, .95f})
            fired += f.update(.011f, 0, y, true);
        check(fired == 1, "a held push fires once");
        for (float y : {0.f, 0.f, .95f})
            fired += f.update(.011f, 0, y, true);
        check(fired == 2, "and again after the stick rests");
        // Legs not engaged: nothing, and a stick already up when they engage waits for rest.
        Flick g;
        check(!g.update(.011f, 0, 0, false) && !g.update(.011f, 0, .95f, false), "legs off: no flick");
        check(!g.update(.011f, 0, .95f, true), "engaged with the stick already up: no flick");
        check(!g.update(.011f, 0, .0f, true) && g.update(.011f, 0, .95f, true), "after a rest it flicks");
        check(!g.update(.011f, std::nanf(""), .95f, true), "a nan stick is nothing");
    }
    // The rope assist: toward |A - P|, at most 1.2 m from where it began, 2 m/s, scaled by the weight.
    {
        const float start = 20;
        float len = start;
        for (int i = 0; i < 20; ++i)
            len = ropeLength(start, len, 40, 1, .011f, 1.5f, c);
        check(near(len, start + 2 * .011f * 20, 1e-3f), "2 m/s: 20 steps of 11 ms lengthen it .44 m");
        for (int i = 0; i < 200; ++i)
            len = ropeLength(start, len, 40, 1, .011f, 1.5f, c);
        check(near(len, start + 1.2f, 1e-3f), "never past 1.2 m longer, however far he is");
        len = start;
        for (int i = 0; i < 400; ++i)
            len = ropeLength(start, len, 5, 1, .011f, 1.5f, c);
        check(near(len, start - 1.2f, 1e-3f), "never past 1.2 m shorter");
        len = start;
        for (int i = 0; i < 400; ++i)
            len = ropeLength(start, len, 20.5f, 1, .011f, 1.5f, c);
        check(near(len, 20.5f, 1e-3f), "a near target is reached, not passed");
        check(near(ropeLength(start, start, 40, .5f, .1f, 1.5f, c), start + .1f, 1e-4f),
              "half weight, half the rate");
        check(ropeLength(start, start, 40, 0, .1f, 1.5f, c) == start, "no weight, no change");
        check(ropeLength(2.f, 2.f, 0.f, 1, 1.f, 1.5f, c) >= 1.5f, "never under the shortest rope");
        check(ropeLength(start, start, std::nanf(""), 1, .1f, 1.5f, c) == start,
              "a nan target changes nothing");
        // The plan's formula: length_new = length + clamp(|A - P| - length, +-1.2).
        const float planned = start + std::clamp(23.f - start, -1.2f, 1.2f);
        float x = start;
        for (int i = 0; i < 300; ++i)
            x = ropeLength(start, x, 23, 1, .02f, 1.5f, c);
        check(near(x, planned, 1e-3f), "the settled length is the plan's formula");
    }
    // The side steering: at most 2.5 m/s^2, toward him, and the speed is never raised.
    {
        float a{};
        const Vec3 v{0, 0, -15};
        Vec3 now = v;
        for (int i = 0; i < 90; ++i) {
            now = steered(now, {20, 0, -10}, 1, .011f, c, &a);
            check(a <= c.steer + 1e-3f, "steering within 2.5 m/s^2");
            check(length(now) <= length(v) + 1e-3f, "the speed is never raised");
        }
        check(now.x > .5f && near(length(now), 15, 1e-3f), "it turned toward him, its length kept");
        Vec3 first = steered(v, {20, 0, -10}, 1, 1.f / 60, c, &a);
        check(near(a, c.steer, .05f) && near(length(first - v) * 60, c.steer, .05f),
              "2.5 m/s^2 across, no more");
        first = steered(v, {20, 0, -10}, .5f, 1.f / 60, c, &a);
        check(near(a, c.steer * .5f, .05f), "half weight, half the steering");
        check(same(steered(v, {0, 0, -30}, 1, .011f, c, &a), v) && a == 0, "dead ahead: nothing to steer");
        check(steered({}, {1, 0, 0}, 1, .011f, c, &a).x == 0, "no speed, no steering");
        check(same(steered(v, {20, 0, -10}, 0, .011f, c, &a), v), "no weight, none");
        check(same(steered(v, {std::nanf(""), 0, 0}, 1, .011f, c, &a), v), "a nan aim changes nothing");
        // Up and down count as side: it steers in 3D, not only across.
        const Vec3 up = steered(v, {0, 10, -10}, 1, .05f, c, &a);
        check(up.y > 0, "it steers up too");
    }
    // The foot's swept 10 cm sphere meets the capsule.
    {
        const Vec3 feet{0, 0, -8};
        check(footHits({0, 1, -5}, {0, 1, -7.7f}, .1f, feet, 1.8f, .3f), "a foot through the capsule hits");
        check(footHits({0, 1, -5}, {0, 1, -7.62f}, .1f, feet, 1.8f, .3f),
              "within the foot's radius plus his");
        check(!footHits({0, 1, -5}, {0, 1, -7.5f}, .1f, feet, 1.8f, .3f), "short of 0.4 m from his axis: no");
        check(!footHits({1, 1, -5}, {1, 1, -11}, .1f, feet, 1.8f, .3f), "passing a metre beside him: no");
        check(footHits({.35f, 1, -5}, {.35f, 1, -11}, .1f, feet, 1.8f, .3f),
              "grazing the side within .4 m hits");
        check(!footHits({0, 2.5f, -5}, {0, 2.5f, -11}, .1f, feet, 1.8f, .3f), "over his head: no");
        check(footHits({0, 1.85f, -5}, {0, 1.85f, -11}, .1f, feet, 1.8f, .3f),
              "his head's top and the foot's sphere");
        check(footHits({0, -.2f, -5}, {0, -.2f, -11}, .1f, feet, 1.8f, .3f) == false, "under his feet: no");
        // The sweep catches a foot that passes through him in one step (no tunnelling).
        check(footHits({0, 1, -3}, {0, 1, -13}, .1f, feet, 1.8f, .3f), "a long step through him still hits");
        check(!footHits({std::nanf(""), 1, -5}, {0, 1, -7}, .1f, feet, 1.8f, .3f), "a nan foot hits nothing");
        // The foot's place: the leg's reach from the pelvis toward him.
        const Vec3 foot = footPoint({0, 0, 0}, {0, .9f, -10}, c);
        check(near(foot.z, -.95f, 1e-3f) && near(foot.y, .9f, 1e-3f), "the foot reaches .95 m toward him");
        check(near(segmentDistance({0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}), 1), "parallel segments");
        check(near(segmentDistance({0, 0, 0}, {1, 0, 0}, {.5f, 1, -1}, {.5f, 1, 1}), 1), "crossing segments");
        check(near(segmentDistance({0, 0, 0}, {0, 0, 0}, {3, 4, 0}, {3, 4, 0}), 5), "two points");
    }
    // The blow's velocities: 70 % for him, 60 % kept.
    {
        const Vec3 v{10, -4, -12};
        check(length(enemyVelocity(v, c) - v * .7f) < 1e-5f, "his flight takes 70% of the player's velocity");
        check(length(playerKeeps(v, c) - v * .6f) < 1e-5f, "the player keeps 60%");
    }
    // The kick's states.
    {
        const Vec3 feet{0, 0, 0};
        Kick::Frame f;
        f.seconds = .011f;
        f.swinging = true;
        f.feet = feet;
        f.velocity = {0, -2, -14};
        f.target = {true, 77, {0, 0, -9}, 1.8f, .3f};
        Kick kick;
        check(!kick.busy() && !kick.update(f).assist, "idle: no assist");
        // Refused: slow.
        auto slow = f;
        slow.velocity = {0, 0, -5};
        auto out = kick.ask(slow);
        check(out.event == Kick::Event::none && out.refusal == Refusal::tooSlow && !kick.busy(),
              "a slow swing is refused");
        auto off = f;
        off.enabled = false;
        check(kick.ask(off).refusal == Refusal::off, "off is refused");
        auto nolegs = f;
        nolegs.legs = false;
        check(kick.ask(nolegs).refusal == Refusal::legs, "legs off is refused");
        auto side = f;
        side.target.feet = {9, 0, 0};
        out = kick.ask(side);
        check(out.refusal == Refusal::outsideCone && out.cone > 60.f,
              "an enemy to the side is refused with its angle");
        // Armed.
        out = kick.ask(f);
        check(out.event == Kick::Event::armed && out.target == 77 && out.cone < 20.f &&
                  near(out.speed, length(f.velocity), 1e-3f) && kick.active(),
              "armed at the ringed enemy, with his cone and the speed");
        // On the way: assist at full weight, aimed at his middle.
        auto step = f;
        step.feet = {0, 0, -1};
        out = kick.update(step);
        check(out.assist && out.weight == 1 && out.event == Kick::Event::none &&
                  length(out.aim - f.target.middle()) < 1e-4f,
              "assisting toward his middle");
        // Reaches him: a hit, with 70 % and 60 %.
        auto close = f;
        close.feet = {0, 0, -7.4f};
        Kick::Output hit;
        for (int i = 0; i < 4 && hit.event != Kick::Event::hit; ++i) {
            close.feet.z -= .2f;
            hit = kick.update(close);
        }
        check(hit.event == Kick::Event::hit, "the foot met his capsule");
        check(length(hit.enemy - f.velocity * .7f) < 1e-4f && length(hit.keep - f.velocity * .6f) < 1e-4f,
              "a hit: his flight 70%, the player keeps 60%");
        check(kick.phase() == Kick::Phase::fading, "after a hit the assist fades");
        // The fade: 0.35 s, weight falling, then idle.
        float weight = 1, seconds = 0;
        auto fade = close;
        fade.seconds = .05f;
        while (kick.busy() && seconds < 1) {
            const auto o = kick.update(fade);
            if (o.assist)
                check(o.weight <= weight + 1e-6f && o.weight >= 0, "the weight only falls");
            weight = o.assist ? o.weight : 0;
            seconds += fade.seconds;
        }
        check(!kick.busy() && seconds >= .3f && seconds <= .45f, "the fade takes about 0.35 s");
        // A miss: he is passed.
        kick.ask(f);
        auto passing = f;
        passing.feet = {5, 0, -20};
        out = kick.update(passing);
        check(out.event == Kick::Event::miss && kick.phase() == Kick::Phase::fading && out.assist,
              "passed him: a miss that fades");
        kick.reset();
        // A miss: the web lets go (no assist, nothing to fade on).
        kick.ask(f);
        auto released = f;
        released.swinging = false;
        released.feet = {0, 0, -1};
        out = kick.update(released);
        check(out.event == Kick::Event::miss && out.lost && !out.assist && !kick.busy(),
              "the web let go: a miss, no assist");
        // A miss: the ringed enemy changed or fell.
        kick.ask(f);
        auto gone = f;
        gone.target.valid = false;
        gone.feet = {0, 0, -1};
        out = kick.update(gone);
        check(out.event == Kick::Event::miss && out.lost, "the target gone: a miss");
        kick.reset();
        // A miss: too long armed.
        kick.ask(f);
        auto late = f;
        late.seconds = .1f;
        late.feet = {0, 0, -.5f};
        Kick::Event last{};
        for (int i = 0; i < 20 && last != Kick::Event::miss; ++i)
            last = kick.update(late).event;
        check(last == Kick::Event::miss, "armed no longer than 1.6 s");
        // Only the ringed enemy: a different one in the frame is no hit.
        kick.reset();
        kick.ask(f);
        auto other = f;
        other.target.id = 78;
        other.feet = {0, 0, -8};
        check(kick.update(other).event == Kick::Event::miss, "another enemy's capsule is not a hit");
        // Reset.
        kick.reset();
        check(!kick.busy(), "reset");
    }
    // The swing's assist in a simulated web: no speed added, the rope within 1.2 m, a weight of 0 changes
    // nothing.
    {
        const auto run = [](bool assist, float weight, float* ropeAtEnd, float* maxSpeed, Vec3* endVelocity,
                            float* steer, float* rope) {
            Anchor world;
            SwingConfig config;
            config.gravity = 0;
            config.airAcceleration = 0;
            Swing swing(config);
            Input in;
            in.hands[0] = {{{0, 0, 0}, {.70710678f, 0, 0, .70710678f}}, {0, 0, 0}, true, 0, 1};
            Body body{{}, {}, false};
            auto intent = swing.predictNativeStep(1.f / 90, in, world, body);
            for (int i = 0; i < 60; ++i) {
                body = {intent.target, intent.velocity, false};
                intent = swing.predictNativeStep(1.f / 90, in, world, body);
            }
            // The swing is on: give the body a tangential speed on the rope.
            const float start = swing.webs()[0].length;
            body = {intent.target, {12, 0, 0}, false};
            *maxSpeed = 12;
            for (int i = 0; i < 180; ++i) {
                Swing::KickAssist a;
                a.on = assist;
                a.aim = {30, 8, -40};
                a.weight = weight;
                swing.kickAssist(a);
                intent = swing.predictNativeStep(1.f / 90, in, world, body);
                body = {intent.target, intent.velocity, false};
                *maxSpeed = std::max(*maxSpeed, length(intent.velocity));
            }
            *ropeAtEnd = swing.webs()[0].length - start;
            *endVelocity = body.velocity;
            *steer = swing.kickReport().steer;
            *rope = swing.kickReport().rope;
        };
        float rope0{}, speed0{}, steer0{}, report0{}, rope1{}, speed1{}, steer1{}, report1{}, rope2{},
            speed2{}, steer2{}, report2{};
        Vec3 v0, v1, v2;
        run(false, 1, &rope0, &speed0, &v0, &steer0, &report0);
        run(true, 1, &rope1, &speed1, &v1, &steer1, &report1);
        run(true, 0, &rope2, &speed2, &v2, &steer2, &report2);
        check(speed1 <= 12 + 1e-2f, "the assist adds no speed (no gravity, no acceleration, a taut web)");
        check(steer1 > 0 && steer1 <= 2.5f + 1e-3f, "the swing steered, within 2.5 m/s^2");
        check(std::abs(report1) <= 1.2f + 1e-3f && std::abs(rope1) <= 1.2f + 1e-3f,
              "the rope moved at most 1.2 m");
        check(length(v1 - v0) > .05f || std::abs(rope1 - rope0) > .05f, "the assist changed the swing");
        check(length(v2 - v0) < 1e-4f && std::abs(rope2 - rope0) < 1e-4f && steer2 == 0,
              "a weight of 0 changes nothing");
        std::printf("  assist run: rope %+.2f m, steer %.2f m/s^2, end speed %.2f (no assist: %.2f)\n", rope1,
                    steer1, length(v1), length(v0));
    }
    // The settings: manual off, auto on, in spidy_vr.ini, as switches.
    {
        using namespace vr_settings;
        Values v;
        check(!v.swingKickManual && v.swingKickAuto, "manual off and the auto dive on by default");
        check(defaultChoice(Item::swingKickManual) == 0 && defaultChoice(Item::swingKickAuto) == 1,
              "default choices");
        check(choose(Item::swingKickManual, 1, v) && v.swingKickManual && choose(Item::swingKickAuto, 0, v) &&
                  !v.swingKickAuto,
              "choices set them");
        const Values back = withTabOnly({}, tabOnlyText(v));
        check(back.swingKickManual && !back.swingKickAuto, "spidy_vr.ini round-trips them");
        const Values old = withTabOnly({}, "camera_stabilization=1\n");
        check(!old.swingKickManual && old.swingKickAuto, "an older file keeps the dive and the kick off");
        const auto* row = rowFor(Item::swingKickManual);
        check(row && std::string(row->title).find("EXPERIMENTAL") != std::string::npos,
              "its row says EXPERIMENTAL");
    }
    std::printf("swing kick: %d checks passed\n", passed);
    return 0;
}
