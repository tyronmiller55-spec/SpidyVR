// HAND GRAB (hand_grab.hpp, G2; November 14): the math (reach, mass, hold cost, spring, lift, least-squares
// release, throw and slam) and WebGrab holding, carrying and throwing bodies by hand against stub targets.
// (The game adapter and the settings are in core_tests and the launcher tests.) Portable: no Windows, no
// headset, no game.
#include "spidy/hand_grab.hpp"
#include "spidy/web_grab.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::hand_grab;
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
bool same(Vec3 a, Vec3 b, float eps = 1e-3f) {
    return length(a - b) < eps;
}

// An empty world: nothing a ray meets.
struct Open : WorldQueries {
    std::optional<RayHit> raycast(Vec3, Vec3, float) const override {
        return {};
    }
    bool exists(std::uint64_t) const override {
        return false;
    }
};
// Targets that fall under no law but the grab's: a commanded one moves as the command's law gives it, the
// rest stay put.
struct Bodies : TargetQueries {
    std::vector<GrabTarget> items;
    void add(std::uint64_t id, Vec3 position, float mass, TargetKind kind = TargetKind::Object,
             float radius = .3f) {
        items.push_back({id, kind, position, {}, mass, radius});
    }
    std::optional<GrabTarget> pick(Vec3, Vec3, float, float) const override {
        return {};
    }
    std::optional<GrabTarget> find(std::uint64_t id) const override {
        for (const auto& t : items)
            if (t.id == id)
                return t;
        return {};
    }
    void within(Vec3 point, float reach, std::vector<GrabTarget>& out) const override {
        for (auto t : items) {
            t.away = surfaceReach(point, t.position, t.radius);
            if (t.away <= reach)
                out.push_back(t);
        }
    }
    void apply(float dt, const std::vector<TargetCommand>& commands) {
        for (auto& t : items) {
            bool commanded = false;
            for (const auto& c : commands)
                if (c.id == t.id) {
                    t.velocity = advance(c, t.position, t.velocity, dt, {0, -9.81f, 0});
                    commanded = true;
                }
            if (!commanded && length(t.velocity) > 0)
                t.velocity.y -= 9.81f * dt;
            t.position += t.velocity * dt;
        }
    }
};
// Both hands tracked, aiming along -Z; `squeeze` per hand (the physical grip, which is not the web button).
struct Hands {
    WebGrab grab;
    Open world;
    Bodies bodies;
    Body player{{0, 1, 0}, {}, true};
    Input in;
    std::vector<TargetCommand> commands;
    float clock = 0;
    explicit Hands(bool on = true) {
        auto c = grab.config().hand;
        c.enabled = on;
        grab.handGrab(c);
        for (int i = 0; i < 2; ++i) {
            in.hands[i].tracked = true;
            in.hands[i].aim.position = {i ? .3f : -.3f, 1.2f, 0};
            in.hands[i].wallPosition = in.hands[i].aim.position;
        }
    }
    void palm(int hand, Vec3 at) {
        in.hands[hand].aim.position = in.hands[hand].wallPosition = at;
    }
    void squeeze(int hand, float v) {
        in.hands[hand].wallGrip = v;
    }
    // `seconds` of 90 Hz samples, physics at 360 Hz between them; `hand` moves by `velocity` meanwhile.
    void run(float seconds, Vec3 velocity0 = {}, Vec3 velocity1 = {}, unsigned kept = 0) {
        const float dt = 1.f / 90;
        for (float t = 0; t < seconds - dt / 2; t += dt) {
            palm(0, in.hands[0].aim.position + velocity0 * dt);
            palm(1, in.hands[1].aim.position + velocity1 * dt);
            out = grab.claim(dt, in, world, bodies, player, kept);
            for (const auto& e : grab.events())
                if (e.kind == GrabEventKind::Throw) {
                    ++throwEvents;
                    thrownVelocity = e.velocity;
                }
            for (int s = 0; s < 4; ++s) {
                commands.clear();
                grab.step(dt / 4, world, bodies, commands);
                bodies.apply(dt / 4, commands);
                last = commands;
            }
            clock += dt;
        }
    }
    Input out;
    unsigned throwEvents{};
    Vec3 thrownVelocity{};
    std::vector<TargetCommand> last;
    std::optional<GrabTarget> target(std::uint64_t id) const {
        return bodies.find(id);
    }
};
unsigned events(const WebGrab& grab, GrabEventKind kind) {
    unsigned n = 0;
    for (const auto& e : grab.events())
        n += e.kind == kind;
    return n;
}
} // namespace

int main() {
    const Config c;
    // The config.
    check(valid(c) && !c.enabled, "default config valid and off");
    Config bad = c;
    bad.force = 0;
    check(!valid(bad), "no force is invalid");
    bad = c;
    bad.omega = NAN;
    check(!valid(bad), "NaN spring invalid");
    bad = c;
    bad.tiredFloor = 0;
    check(!valid(bad), "no floor invalid");
    check(near(c.reach, .35f) && near(c.ahead, .45f) && near(c.force, 1200) && near(c.throwGain, 1.6f) &&
              near(c.maxThrow, 18) && near(c.slamJerk, 4) && near(c.slamStrike, 8) &&
              near(c.knockDamage, .25f) && near(c.reknock, .8f),
          "the plan's numbers");
    // One hand lifts 90 kg at 1200 N, more with a stronger grip.
    check(near(liftMass(c), 90), "one hand lifts 90 kg");
    Config strong = c;
    strong.force = 2400;
    check(near(liftMass(strong), 180), "twice the force, twice the mass");

    // Reach: from the palm to a sphere's surface.
    check(near(surfaceReach({0, 0, 0}, {1, 0, 0}, .4f), .6f) &&
              near(surfaceReach({0, 0, 0}, {.2f, 0, 0}, .4f), 0),
          "reach to a surface, zero inside");
    check(std::isinf(surfaceReach({NAN, 0, 0}, {}, 1)), "a bad palm reaches nothing");

    // Mass from the capsule: a street thug 80 kg, a wider and taller one more, a drone less.
    check(near(enemyMass(.4f, 1.6f, .45f), 80, .5f), "a street thug is 80 kg");
    check(enemyMass(.5f, 2.1f, .65f) > 160, "a brute is over 160 kg");
    check(enemyMass(.3f, .9f, .25f) < 40, "a drone is light");
    check(near(enemyMass(NAN, 1, 1), 80) && near(enemyMass(2, 1, .4f), 80),
          "an unreadable capsule is standard");

    // Where the held body goes: an enemy's chest 0.45 m ahead, a prop its radius and 15 cm.
    check(near(holdAhead(true, .45f), .45f) && near(holdAhead(false, .4f), .55f), "hold places");

    // Hold cost: nothing at first, a little over a minute for a thug, quick for a heavy, none at 0.
    check(near(tiredness(0, 80), 1) && near(tiredness(-3, 80), 1), "fresh grip");
    check(near(tiredness(60, 80), .75f) && near(tiredness(30, 80), .875f) && near(tiredness(600, 80), .75f),
          "a thug tires the hand to 75% over 60 s and no further");
    check(tiredness(10, 160) < tiredness(10, 80) && near(tiredness(30, 200), .75f),
          "a heavy tires it sooner");
    check(lifts(80, 1, tiredness(600, 80, c), c), "a tired hand still lifts a thug");
    Config free = c;
    free.holdCost = 0;
    check(near(tiredness(500, 300, free), 1), "hold cost 0 never tires");
    Config dear = c;
    dear.holdCost = 3;
    check(tiredness(20, 80, dear) < tiredness(20, 80), "a higher hold cost tires sooner");
    check(near(tiredness(NAN, 80), 1) && near(tiredness(10, NAN), 1), "bad numbers do not tire");

    // The spring: zero in place, towards the place, damped, capped at the force.
    check(same(springForce({0, 1, 0}, {}, {0, 1, 0}, {}, 80, 1200, c), {}), "no force in place");
    const Vec3 pull = springForce({0, 0, 0}, {}, {.05f, 0, 0}, {}, 80, 1e9f, c);
    check(near(pull.x, 80 * 18 * 18 * .05f, .5f) && near(pull.y, 0), "F = m w^2 e");
    check(near(springForce({0, 0, 0}, {1, 0, 0}, {}, {}, 80, 1e9f, c).x, -80 * 2 * 18, .5f),
          "damped by -2 w m v");
    check(near(springForce({0, 0, 0}, {}, {2, 0, 0}, {}, 80, 1200, c).x, 1200, .5f), "capped at 1200 N");
    check(near(springForce({0, 0, 0}, {}, {5, 5, 0}, {}, 80, 600, c).x, 600 / std::sqrt(2.f), .5f),
          "capped in length");
    // Lifting: the force against the weight.
    check(lifts(80, 1, 1, c) && lifts(90, 1, 1, c) && !lifts(183, 1, 1, c) && lifts(183, 2, 1, c),
          "one hand lifts a thug, a brute needs two");
    check(!lifts(80, 1, .4f, c) || holdAcceleration(80, 1, .4f, c) > 9.81f,
          "tired force is the floor's share");
    check(near(holdAcceleration(100, 2, .5f, c), 12) && near(holdAcceleration(0, 1, 1, c), 1200),
          "force over mass, two hands, tired");
    // The velocity asked: the hand's own, the error closing at w/2, gravity of the step taken out first.
    check(same(holdVelocity({0, 0, 0}, {.1f, 0, 0}, {}, .01f, 1e6f, c), {.9f, .0981f, 0}, 1e-3f),
          "closing at w/2 m/s per metre");
    check(same(holdVelocity({0, 0, 0}, {0, 0, 0}, {2, 0, 0}, 0, 100, c), {2, 0, 0}),
          "carried at the hand's velocity");
    check(length(holdVelocity({0, 0, 0}, {100, 0, 0}, {}, 0, 1e6f, c)) <= 25.001f, "no faster than 25 m/s");
    check(holdVelocity({0, 0, 0}, {1, 0, 0}, {}, 0, 4, c).x <
              holdVelocity({0, 0, 0}, {1, 0, 0}, {}, 0, 400, c).x,
          "a weak grip closes slower: it must stop in time");

    // The hold law settles on its place with no overshoot, light or heavy, and sags when it cannot lift.
    for (const float mass : {30.f, 80.f, 90.f}) {
        Vec3 x{0, 0, 0}, v{};
        float over = 0;
        for (int i = 0; i < 480; ++i) {
            TargetCommand cmd;
            cmd.mode = TargetCommand::Mode::Follow;
            cmd.maxAcceleration = holdAcceleration(mass, 1, 1, c);
            cmd.velocity = holdVelocity(x, {.5f, .3f, 0}, {}, 1.f / 240, cmd.maxAcceleration, c);
            v = advance(cmd, x, v, 1.f / 240, {0, -9.81f, 0});
            x += v / 240.f;
            over = std::max(over, x.x - .5f);
        }
        check(length(x - Vec3{.5f, .3f, 0}) < .01f && over < .02f, "the hold settles without overshoot");
    }
    {
        Vec3 x{0, 0, 0}, v{};
        for (int i = 0; i < 480; ++i) {
            TargetCommand cmd;
            cmd.mode = TargetCommand::Mode::Follow;
            cmd.maxAcceleration = holdAcceleration(300, 1, 1, c);
            cmd.velocity = holdVelocity(x, {0, 0, 0}, {}, 1.f / 240, cmd.maxAcceleration, c);
            v = advance(cmd, x, v, 1.f / 240, {0, -9.81f, 0});
            x += v / 240.f;
        }
        check(x.y < -2, "300 kg in one hand is not lifted: it sags");
    }

    // Samples: the least-squares slope of the last five.
    {
        Samples s;
        check(same(s.velocity(), {}) && s.size() == 0, "no samples, no velocity");
        s.push(0, {0, 0, 0});
        check(same(s.velocity(), {}), "one sample, no velocity");
        for (int i = 1; i < 12; ++i)
            s.push(.01f, {3 * .01f * i, 0, -1 * .01f * i});
        check(s.size() == 5 && same(s.velocity(), {3, 0, -1}, 1e-3f), "constant velocity read back");
        // Old samples are forgotten: a hand that was still and now moves fast reads the fast part.
        Samples t;
        t.push(0, {});
        for (int i = 1; i <= 10; ++i)
            t.push(.01f, {});
        for (int i = 1; i <= 5; ++i)
            t.push(.01f, {0, -.06f * i, 0});
        check(near(t.velocity().y, -6, .05f), "only the last five count");
        // Jitter in the spacing does not bend the slope (a slope of positions against their times).
        Samples u;
        u.push(0, {0, 0, 0});
        float at = 0;
        for (const float step : {.008f, .013f, .009f, .012f, .01f}) {
            at += step;
            u.push(step, {2 * at, 0, 0});
        }
        check(near(u.velocity().x, 2, .01f), "uneven spacing reads the true slope");
        // A line plus noise: the slope is nearer the line than a two-point difference.
        Samples w;
        w.push(0, {0, 0, 0});
        const float noise[] = {.004f, -.003f, .002f, -.004f, .003f};
        for (int i = 1; i <= 4; ++i)
            w.push(.0111f, {2 * .0111f * i + noise[i], 0, 0});
        check(std::abs(w.velocity().x - 2) <
                  std::abs((.0111f * 8 + noise[4] - (noise[3])) / .0111f - 2) + 1e-3f,
              "smoother than the last difference");
        w.push(.5f, {});
        check(w.size() == 0, "a long gap clears the history");
        w.push(.01f, {NAN, 0, 0});
        check(w.size() == 0, "a bad sample clears it");
        w.push(.01f, {});
        w.push(-1.f, {});
        check(w.size() == 0, "a negative step clears it");
    }

    // Release: slower than 1.5 m/s is let go of; a throw is the hand's velocity times up to 1.6 (less for
    // heavy) at most 18 m/s; a hand down faster than 4 m/s slams, at least 8 m/s.
    {
        const auto slow = release({1, 0, 0}, 30, c);
        check(!slow.thrown && !slow.slam && same(slow.velocity, {1, 0, 0}), "slow: only let go of");
        const auto thrown = release({5, 0, 0}, 30, c);
        check(thrown.thrown && !thrown.slam && near(thrown.velocity.x, 8, .01f), "a light body: x1.6");
        const auto fast = release({20, 0, 0}, 30, c);
        check(near(length(fast.velocity), 18), "at most 18 m/s");
        const auto heavy = release({5, 0, 0}, 360, c);
        check(heavy.thrown && near(heavy.velocity.x, 5 * (1 + .6f * .25f), .01f),
              "a heavy body gets less of the gain");
        const auto thug = release({5, 0, 0}, 80, c);
        check(near(thug.velocity.x, 8, .01f), "a thug is within one hand: full gain");
        const auto slam = release({0, -5, 0}, 80, c);
        check(slam.slam && slam.thrown && near(slam.velocity.y, -8, .01f) && near(slam.velocity.x, 0),
              "a downward jerk of 5 m/s slams at 8 m/s");
        const auto hard = release({1, -10, 0}, 80, c);
        check(hard.slam && near(length(hard.velocity), 16.f * 1.f * std::sqrt(101.f) / 10.f, .1f) &&
                  hard.velocity.y < -15,
              "a harder slam keeps its own way and speed times the gain");
        const auto notYet = release({0, -3.9f, 0}, 80, c);
        check(!notYet.slam && notYet.thrown && near(notYet.velocity.y, -3.9f * 1.6f, .01f),
              "3.9 m/s down is a throw");
        const auto diag = release({3, -4.2f, 0}, 80, c);
        check(diag.slam, "slam is the downward component over 4 m/s");
        const auto huge = release({0, -40, 0}, 80, c);
        check(huge.slam && near(length(huge.velocity), 18), "a slam is capped too");
        check(!release({NAN, 0, 0}, 80, c).thrown, "a bad velocity throws nothing");
    }

    // WebGrab: off by default, nothing within the palm's reach is taken.
    {
        Hands rig(false);
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, -.3f});
        rig.squeeze(0, 1);
        rig.run(.1f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None && rig.grab.handReport().grabs == 0,
              "off: nothing taken");
        check(!WebGrab{}.config().hand.enabled, "off by default");
    }
    // On: a squeeze with the palm at a prop takes it by hand, not by web.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.5f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.in.hands[0].grip = 1; // the web button is the same grip
        rig.run(1.f / 90);
        const auto& g = rig.grab.grabs()[0];
        check(g.phase == GrabPhase::Held && g.hand && g.target == 7 && near(g.mass, 30),
              "the prop is held by hand");
        check(events(rig.grab, GrabEventKind::Grab) == 1 && rig.grab.handReport().props == 1 &&
                  rig.grab.handReport().lastKind == 2 && rig.grab.handReport().lastAction == 1,
              "one grab, a prop, held");
        check(near(rig.out.hands[0].grip, 0) && near(rig.out.hands[0].wallGrip, 0),
              "the swing sees neither the grip nor a wall grip on that hand");
        check(rig.grab.grabs()[1].phase == GrabPhase::None, "the other hand holds nothing");
        // And the web grip that closed with it shoots nothing.
        check(rig.grab.forSwing(rig.in).hands[0].grip == 0, "the grip is the hand grab's");
    }
    // Out of reach: nothing is taken by hand (0.35 m from the surface), and a squeeze does not repeat.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.7f}, 30); // surface .4 m from the palm
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.1f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "0.4 m away: not taken");
        rig.palm(0, {-.3f, 1.2f, -.1f}); // surface .3 m away now, but the squeeze began elsewhere
        rig.run(.1f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "a squeeze held on does not take it later");
        rig.squeeze(0, 0);
        rig.run(.1f);
        rig.squeeze(0, 1);
        rig.run(.05f);
        check(rig.grab.grabs()[0].phase == GrabPhase::Held, "a new squeeze within reach does");
    }
    // Priority: an enemy within reach before a prop, though the prop is nearer; the nearer enemy first.
    {
        Hands rig;
        rig.bodies.add(1, {-.3f, 1.2f, -.4f}, 30);                               // prop, surface 0.1 m
        rig.bodies.add(2, {-.3f, 1.2f, -.75f}, 80, TargetKind::Character, .45f); // enemy, surface 0.3 m
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.02f);
        check(rig.grab.grabs()[0].target == 2 && rig.grab.handReport().enemies == 1,
              "the enemy before the prop");
        Hands two;
        two.bodies.add(3, {-.3f, 1.2f, -.9f}, 80, TargetKind::Character, .45f); // surface 0.45
        two.bodies.add(4, {-.3f, 1.2f, -.7f}, 80, TargetKind::Character, .45f); // surface 0.25
        two.palm(0, {-.3f, 1.2f, 0});
        two.squeeze(0, 1);
        two.run(.02f);
        check(two.grab.grabs()[0].target == 4, "the nearer enemy");
        Hands three;
        three.bodies.add(5, {-.3f, 1.2f, -.4f}, 30);
        three.bodies.add(6, {-.3f, 1.2f, -.55f}, 30);
        three.palm(0, {-.3f, 1.2f, 0});
        three.squeeze(0, 1);
        three.run(.02f);
        check(three.grab.grabs()[0].target == 5, "the nearer prop");
    }
    // Bodies too heavy for any grab, bad ones and ones a squeeze kept for the swing are not taken.
    {
        Hands rig;
        rig.bodies.add(1, {-.3f, 1.2f, -.3f}, 500);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.05f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "over the grab's mass limit: not taken");
        Hands kept;
        kept.bodies.add(1, {-.3f, 1.2f, -.3f}, 30);
        kept.palm(0, {-.3f, 1.2f, 0});
        kept.squeeze(0, 1);
        kept.run(.05f, {}, {}, 1);
        check(kept.grab.grabs()[0].phase == GrabPhase::None, "a press the swing keeps takes nothing");
        Hands nan;
        nan.bodies.add(1, {NAN, 1.2f, -.3f}, 30);
        nan.palm(0, {-.3f, 1.2f, 0});
        nan.squeeze(0, 1);
        nan.run(.05f);
        check(nan.grab.grabs()[0].phase == GrabPhase::None, "a body with no position is not taken");
    }
    // The hold: the prop comes to the palm's place and follows the hand.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(1.5f);
        const Vec3 place = rig.in.hands[0].aim.position + Vec3{0, 0, -1} * (.3f + .15f);
        check(same(rig.target(7)->position, place, .02f), "held .45 m ahead of the palm along its aim");
        rig.run(1.f, {1, 0, 0});
        const Vec3 moved = rig.in.hands[0].aim.position + Vec3{0, 0, -.45f};
        check(same(rig.target(7)->position, moved, .06f) && rig.grab.grabs()[0].phase == GrabPhase::Held,
              "it follows the hand");
        check(!rig.last.empty() && rig.last[0].mode == TargetCommand::Mode::Follow && !rig.last[0].carried,
              "a prop is a Follow command, not carried");
        check(rig.grab.grabs()[0].tension >= 0 && rig.grab.grabs()[0].tension < .6f,
              "a light prop is easy to hold");
    }
    // An enemy: held at his chest ahead of the palm, carried (the game's flight takes the light blow).
    {
        Hands rig;
        rig.bodies.add(9, {-.3f, 1.2f, -.7f}, 80, TargetKind::Character, .45f);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(1.5f);
        check(same(rig.target(9)->position, rig.in.hands[0].aim.position + Vec3{0, 0, -.45f}, .02f),
              "an enemy's chest is 0.45 m ahead of the palm");
        check(!rig.last.empty() && rig.last[0].carried && rig.last[0].mode == TargetCommand::Mode::Follow &&
                  near(rig.last[0].maxAcceleration, 1200.f / 80, 1.f),
              "carried, 1200 N over 80 kg");
        // He tires the hand a little over a long hold.
        rig.run(30);
        check(rig.grab.grabs()[0].phase == GrabPhase::Held && rig.grab.grabs()[0].tired < .9f &&
                  rig.grab.grabs()[0].tired > .75f && rig.last[0].maxAcceleration < 14 &&
                  rig.last[0].maxAcceleration > 9.9f,
              "half a minute of holding tires the hand a little, and it still holds him");
        // With hold cost 0 it does not.
        Hands idle;
        auto cfg = idle.grab.config().hand;
        cfg.holdCost = 0;
        idle.grab.handGrab(cfg);
        idle.bodies.add(9, {-.3f, 1.2f, -.7f}, 80, TargetKind::Character, .45f);
        idle.palm(0, {-.3f, 1.2f, 0});
        idle.squeeze(0, 1);
        idle.run(60);
        check(near(idle.grab.grabs()[0].tired, 1), "hold cost 0: never tired");
    }
    // Weight: a 150 kg body sags in one hand (8 m/s^2 < g) and rides in two.
    {
        Hands one;
        one.bodies.add(3, {-.3f, 1.2f, -.4f}, 150);
        one.palm(0, {-.3f, 1.2f, 0});
        one.squeeze(0, 1);
        one.run(2.f);
        Hands both;
        both.bodies.add(3, {0, 1.2f, -.4f}, 150);
        both.palm(0, {-.1f, 1.2f, 0});
        both.palm(1, {.1f, 1.2f, 0});
        both.squeeze(0, 1);
        both.squeeze(1, 1);
        both.run(2.f);
        const Vec3 place{0, 1.2f, -.45f - .0f};
        check(one.target(3)->position.y < .8f, "150 kg in one hand sags");
        check(both.grab.grabs()[0].hand && both.grab.grabs()[1].hand && both.grab.grabs()[1].target == 3,
              "both hands hold it");
        check(std::abs(both.target(3)->position.y - 1.2f) < .05f &&
                  std::abs(both.target(3)->position.z - place.z - .15f + .0f) < .5f,
              "two hands carry it");
        check(near(both.last[0].maxAcceleration, 2400.f / 150, .1f), "two hands double the force");
        // One command for the two.
        unsigned n = 0;
        for (const auto& cmd : both.last)
            n += cmd.id == 3;
        check(n == 1, "one command for both hands");
    }
    // Throw: let go with the hand moving: x1.6 of the hand's velocity, once.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.5f);
        rig.run(.15f, {5, 0, 0});
        rig.squeeze(0, 0);
        rig.grab.claim(
            1.f / 90,
            [&] {
                rig.palm(0, rig.in.hands[0].aim.position + Vec3{5, 0, 0} / 90.f);
                return rig.in;
            }(),
            rig.world, rig.bodies, rig.player);
        check(events(rig.grab, GrabEventKind::Throw) == 1, "a throw event");
        Vec3 launched{};
        for (const auto& e : rig.grab.events())
            if (e.kind == GrabEventKind::Throw)
                launched = e.velocity;
        check(near(launched.x, 8, .3f) && std::abs(launched.y) < .1f, "thrown at 1.6 times the hand's 5 m/s");
        std::vector<TargetCommand> out;
        rig.grab.step(.005f, rig.world, rig.bodies, out);
        check(!out.empty() && out[0].thrown && out[0].mode == TargetCommand::Mode::Launch &&
                  same(out[0].velocity, launched),
              "the launch goes out with the next step");
        check(rig.grab.grabs()[0].phase == GrabPhase::None && rig.grab.handReport().throws == 1 &&
                  rig.grab.handReport().lastAction == 2 && near(rig.grab.handReport().lastSpeed, 8, .3f),
              "the report counts one throw");
    }
    // Slam: let go with the hand moving down fast: 8 m/s at least.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.5f);
        rig.run(.15f, {0, -5.f, 0});
        rig.squeeze(0, 0);
        rig.run(1.f / 90, {0, -5.f, 0});
        check(rig.grab.handReport().slams == 1 && rig.grab.handReport().throws == 0 &&
                  rig.grab.handReport().lastAction == 3 && rig.grab.handReport().lastSpeed >= 8 - .1f,
              "a downward jerk slams at 8 m/s or more");
    }
    // A slow let go is only that: no launch, a release.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.5f);
        rig.squeeze(0, 0);
        rig.run(1.f / 90);
        check(events(rig.grab, GrabEventKind::Release) == 1 && events(rig.grab, GrabEventKind::Throw) == 0 &&
                  rig.grab.handReport().drops == 1 && rig.grab.grabs()[0].phase == GrabPhase::None,
              "still, the hand only lets go");
        std::vector<TargetCommand> out;
        rig.grab.step(.005f, rig.world, rig.bodies, out);
        check(out.empty(), "no command: the body keeps what it has");
    }
    // Two hands let go together: one throw, from the mean of both hands.
    {
        Hands rig;
        rig.bodies.add(3, {0, 1.2f, -.4f}, 30);
        rig.palm(0, {-.1f, 1.2f, 0});
        rig.palm(1, {.1f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.squeeze(1, 1);
        rig.run(.5f);
        rig.run(.15f, {4, 0, 0}, {4, 0, 0});
        rig.squeeze(0, 0);
        rig.squeeze(1, 0);
        rig.run(1.f / 90, {4, 0, 0}, {4, 0, 0});
        check(rig.grab.handReport().throws == 1 && rig.throwEvents == 2,
              "both hands letting go throw it once");
        check(near(rig.thrownVelocity.x, 6.4f, .4f), "at the hands' 4 m/s times 1.6");
        check(rig.bodies.items[0].velocity.x > 6, "the body flew off at that");
    }
    // One hand letting go while the other holds: no throw.
    {
        Hands rig;
        rig.bodies.add(3, {0, 1.2f, -.4f}, 30);
        rig.palm(0, {-.1f, 1.2f, 0});
        rig.palm(1, {.1f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.squeeze(1, 1);
        rig.run(.5f);
        rig.squeeze(0, 0);
        rig.run(.1f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None && rig.grab.grabs()[1].phase == GrabPhase::Held &&
                  rig.grab.handReport().throws == 0,
              "the other hand keeps it");
        check(near(rig.last[0].maxAcceleration, 1200.f / 30, .1f), "and holds it alone");
    }
    // Switching it off lets go without a throw; a body shaken off the hand is lost.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.3f);
        auto cfg = rig.grab.config().hand;
        cfg.enabled = false;
        check(rig.grab.handGrab(cfg), "switched off");
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "off during play: let go");
        rig.run(.1f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "the squeeze held on takes nothing");
        cfg.force = -1;
        check(!rig.grab.handGrab(cfg), "an invalid config is refused");
        Hands far;
        far.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        far.palm(0, {-.3f, 1.2f, 0});
        far.squeeze(0, 1);
        far.run(.3f);
        far.bodies.items[0].position = {5, 1.2f, 5};
        far.run(.05f);
        check(far.grab.grabs()[0].phase == GrabPhase::None && far.grab.handReport().lost == 1,
              "carried away: lost");
    }
    // The swing asks before the claim: a squeeze that begins with a body in reach, or a hand that holds one,
    // belongs to the hand grab (no wall grip), others not.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        check(rig.grab.handClaims(0, rig.in.hands[0], rig.bodies), "a squeeze beginning in reach is claimed");
        check(!rig.grab.handClaims(1, rig.in.hands[1], rig.bodies), "a hand not squeezing is not");
        Hands off(false);
        off.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        off.palm(0, {-.3f, 1.2f, 0});
        off.squeeze(0, 1);
        check(!off.grab.handClaims(0, off.in.hands[0], off.bodies), "off: never claimed");
        Hands away;
        away.bodies.add(7, {-.3f, 1.2f, -2}, 30);
        away.palm(0, {-.3f, 1.2f, 0});
        away.squeeze(0, 1);
        check(!away.grab.handClaims(0, away.in.hands[0], away.bodies), "nothing in reach: not claimed");
        rig.run(.1f);
        check(rig.grab.handClaims(0, rig.in.hands[0], rig.bodies), "a hand holding by palm stays claimed");
        rig.squeeze(0, 0);
        rig.run(.1f);
        check(!rig.grab.handClaims(0, rig.in.hands[0], rig.bodies), "and is not once it let go");
        Hands held;
        held.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        held.palm(0, {-.3f, 1.2f, 0});
        held.squeeze(0, 1);
        held.run(.05f, {}, {}, 1); // kept for the swing: the squeeze is under way but took nothing
        check(!held.grab.handClaims(0, held.in.hands[0], held.bodies),
              "a squeeze already under way is not a new claim");
    }
    // A tracking loss or an unfocused game drops the hold.
    {
        Hands rig;
        rig.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        rig.palm(0, {-.3f, 1.2f, 0});
        rig.squeeze(0, 1);
        rig.run(.3f);
        rig.in.hands[0].tracked = false;
        rig.run(.05f);
        check(rig.grab.grabs()[0].phase == GrabPhase::None, "a lost hand drops it");
        Hands focus;
        focus.bodies.add(7, {-.3f, 1.2f, -.4f}, 30);
        focus.palm(0, {-.3f, 1.2f, 0});
        focus.squeeze(0, 1);
        focus.run(.3f);
        focus.in.focused = false;
        focus.run(.05f);
        check(focus.grab.grabs()[0].phase == GrabPhase::None, "an unfocused game drops it");
    }
    // A fresh squeeze at the palm's place with the squeeze kept on the web path is unchanged: the hand
    // grab touches no web grab: a web press with the palm far from everything still grabs by web.
    {
        Hands rig;
        struct Far : Bodies {
            std::optional<GrabTarget> pick(Vec3, Vec3, float, float) const override {
                return items.empty() ? std::nullopt : std::optional<GrabTarget>(items[0]);
            }
        } far;
        far.add(7, {-.3f, 1.2f, -6}, 30);
        rig.in.hands[0].grip = 1;
        rig.grab.claim(1.f / 90, rig.in, rig.world, far, rig.player);
        check(rig.grab.grabs()[0].phase == GrabPhase::Tethered && !rig.grab.grabs()[0].hand,
              "a web press away from every body still shoots the web");
    }

    std::printf("hand grab tests: %d checks passed\n", passed);
    return 0;
}
