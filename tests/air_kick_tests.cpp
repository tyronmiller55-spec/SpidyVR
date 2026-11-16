// AIR KICKS (air_kick.hpp, K2; November 14): the stick's gestures, the zone and the gaze, the foot's bend,
// the kick's strength, the timeline and legs' pose, and the kick in the swing module against fighting
// enemies. Portable: no Windows, no headset, no game.
#include "spidy/air_kick.hpp"
#include "spidy/leg_layers.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace spidy;
using namespace spidy::air_kick;
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
// The gestures fired by a stick path sampled every `dt`; the last, and how many.
struct Fired {
    Flick last;
    int count{};
};
Fired run(const Samples& stick, float dt = .011f, bool engaged = true) {
    Gestures g;
    Fired f;
    for (const auto& [x, y] : stick) {
        const Flick k = g.update(dt, x, y, engaged);
        if (k.gesture != Gesture::none) {
            f.last = k;
            ++f.count;
        }
    }
    return f;
}
// A stick at `degrees` from up and `size`.
std::pair<float, float> at(float degrees, float size) {
    return {std::sin(degrees * deg) * size, std::cos(degrees * deg) * size};
}
Samples flickAt(float degrees) {
    Samples s;
    for (int i = 0; i < 5; ++i)
        s.push_back({0, 0});
    s.push_back(at(degrees, .5f));
    s.push_back(at(degrees, 1.f));
    for (int i = 0; i < 5; ++i)
        s.push_back(at(degrees, 1.f));
    return s;
}
// A fighting enemy standing at `feet`.
Kick::Target enemy(std::uint64_t id, Vec3 feet) {
    return {id, feet, 1.8f, .3f};
}
// Steps a kick every 1/90 s with `targets` until idle or `seconds`; the events seen.
struct Seen {
    int started{}, hits{}, misses{}, refused{};
    Kick::Output hit;
    float maxFoot{};
    float took{};
};
Seen play(Kick& k, Kick::Frame f, float seconds = 1.f) {
    Seen s;
    const float dt = 1.f / 90.f;
    f.seconds = dt;
    for (float t = 0; t < seconds; t += dt) {
        const auto o = k.update(f);
        s.maxFoot = std::max(s.maxFoot, o.foot);
        if (o.event == Kick::Event::hit) {
            ++s.hits;
            s.hit = o;
        } else if (o.event == Kick::Event::miss) {
            ++s.misses;
        }
        if (!k.busy()) {
            s.took = t;
            break;
        }
    }
    return s;
}
Kick::Frame frame(std::span<const Kick::Target> targets) {
    Kick::Frame f;
    f.feet = {0, 10, 0};
    f.eyeOffset = {0, 1.6f, 0};
    f.gaze = {0, 0, -1};
    f.targets = targets;
    return f;
}
} // namespace

int main() {
    const Config c;
    // ---- the angle and the gesture's class ----
    check(near(stickAngle(0, 1), 0) && near(stickAngle(1, 0), 90) && near(stickAngle(-1, 0), -90) &&
              near(std::abs(stickAngle(0, -1)), 180),
          "the stick's angle from up, + right");
    check(classify(0) == Gesture::doubleDive && classify(19) == Gesture::doubleDive &&
              classify(-19) == Gesture::doubleDive,
          "straight up is the double dive");
    check(classify(21) == Gesture::dive && classify(-45) == Gesture::dive && classify(69) == Gesture::dive,
          "up-left and up-right are one-leg dives");
    check(classify(71) == Gesture::freeform && classify(-120) == Gesture::freeform &&
              classify(180) == Gesture::freeform,
          "the rest is freeform");

    // ---- the stick's gestures ----
    {
        Fired f = run(flickAt(0));
        check(f.count == 1 && f.last.gesture == Gesture::doubleDive, "a quick flick up is a double dive");
        f = run(flickAt(40));
        check(f.count == 1 && f.last.gesture == Gesture::dive && f.last.side == 1,
              "up-right is the right leg's dive");
        f = run(flickAt(-40));
        check(f.count == 1 && f.last.gesture == Gesture::dive && f.last.side == -1,
              "up-left is the left leg's dive");
        f = run(flickAt(150));
        check(f.count == 1 && f.last.gesture == Gesture::freeform && f.last.side == 1,
              "a flick down is freeform, at once");
        // A slow push is the legs' pose, never a kick.
        Samples slow;
        for (int i = 0; i <= 40; ++i)
            slow.push_back({0, i / 40.f});
        check(run(slow).count == 0, "a slow push kicks nothing");
        check(run(flickAt(0), .011f, false).count == 0, "legs not engaged: no gesture");
        // A flick needs the stick back under 0.3 before the next.
        Samples twice = flickAt(0);
        twice.push_back(at(0, 0.5f));
        twice.push_back(at(0, 1.f));
        check(run(twice).count == 1, "no second flick without coming back");
        Samples back = flickAt(0);
        for (int i = 0; i < 3; ++i)
            back.push_back({0, 0});
        for (const auto& s : flickAt(0))
            back.push_back(s);
        check(run(back).count == 2, "a second flick after coming back");
        // Not finite: nothing.
        Gestures g;
        check(g.update(.011f, NAN, 1, true).gesture == Gesture::none, "NaN is no gesture");
    }
    {
        // A sideways flick that comes back is freeform, once the stick is back in.
        Samples s;
        for (int i = 0; i < 5; ++i)
            s.push_back({0, 0});
        s.push_back(at(90, .5f));
        s.push_back(at(90, 1.f));
        s.push_back(at(90, 1.f));
        const Fired waiting = run(s);
        check(waiting.count == 0, "a sideways flick waits for its quarter circle");
        s.push_back(at(90, .2f));
        const Fired freeform = run(s);
        check(freeform.count == 1 && freeform.last.gesture == Gesture::freeform && freeform.last.side == 1 &&
                  near(freeform.last.angle, 90, 1),
              "a sideways flick let go of is a freeform blow");
        // Held at the side past the roundhouse's time: freeform.
        Samples held = s;
        held.pop_back();
        for (int i = 0; i < 40; ++i)
            held.push_back(at(90, 1.f));
        const Fired late = run(held);
        check(late.count == 1 && late.last.gesture == Gesture::freeform,
              "held at the side: freeform after 0.25 s");
        // The quarter circle: a side to up along the rim.
        Samples round;
        for (int i = 0; i < 5; ++i)
            round.push_back({0, 0});
        round.push_back(at(90, .5f));
        for (float a = 90; a >= 0; a -= 15)
            round.push_back(at(a, 1.f));
        const Fired quarter = run(round);
        check(quarter.count == 1 && quarter.last.gesture == Gesture::roundhouse && quarter.last.side == 1,
              "a quarter circle from the right to up is a roundhouse");
        Samples left;
        for (int i = 0; i < 5; ++i)
            left.push_back({0, 0});
        left.push_back(at(-90, .5f));
        for (float a = -90; a <= 0; a += 15)
            left.push_back(at(a, 1.f));
        const Fired quarterLeft = run(left);
        check(quarterLeft.count == 1 && quarterLeft.last.gesture == Gesture::roundhouse &&
                  quarterLeft.last.side == -1,
              "and from the left");
        // Through the middle (not along the rim) is no roundhouse: a freeform, then nothing more.
        Samples cut;
        for (int i = 0; i < 5; ++i)
            cut.push_back({0, 0});
        cut.push_back(at(90, .5f));
        cut.push_back(at(90, 1.f));
        cut.push_back({0, .1f});
        cut.push_back({0, 0});
        const Fired through = run(cut);
        check(through.count == 1 && through.last.gesture == Gesture::freeform,
              "through the middle is freeform");
        cut.push_back(at(0, 1.f));
        const Fired then = run(cut);
        check(then.count == 2 && then.last.gesture == Gesture::doubleDive,
              "and a fresh flick up after it is a dive");
        // A roundhouse that is too slow (0.25 s) is freeform.
        Samples slowArc;
        for (int i = 0; i < 5; ++i)
            slowArc.push_back({0, 0});
        slowArc.push_back(at(90, .5f));
        slowArc.push_back(at(90, 1.f));
        for (int i = 0; i < 30; ++i)
            slowArc.push_back(at(90, 1.f));
        for (float a = 90; a >= 0; a -= 15)
            slowArc.push_back(at(a, 1.f));
        const Fired late2 = run(slowArc);
        check(late2.count == 1 && late2.last.gesture == Gesture::freeform,
              "a slow quarter circle is a freeform");
    }

    // ---- the zone and the gaze ----
    check(zoneAt(.9f) == Zone::head && zoneAt(.81f) == Zone::head && zoneAt(.8f) == Zone::chest &&
              zoneAt(.5f) == Zone::chest && zoneAt(.35f) == Zone::chest && zoneAt(.34f) == Zone::legs,
          "head over 80 %, legs under 35 %");
    check(zoneAt(zonePoint({}, 2.f, Zone::head).y / 2.f) == Zone::head &&
              zoneAt(zonePoint({}, 2.f, Zone::chest).y / 2.f) == Zone::chest &&
              zoneAt(zonePoint({}, 2.f, Zone::legs).y / 2.f) == Zone::legs,
          "the zone points fall in their zones");
    check(zonesFor(Gesture::doubleDive) == 2u && zonesFor(Gesture::roundhouse) == 1u &&
              zonesFor(Gesture::freeform) == 7u && (zonesFor(Gesture::dive) & 3u) == 3u &&
              !(zonesFor(Gesture::dive) & 4u),
          "the zones each gesture goes for");
    {
        const Vec3 eye{0, 1.6f, 0};
        const auto weight = [&](float degrees) {
            return gazeWeight({0, 0, -1}, eye,
                              eye + Vec3{std::sin(degrees * deg), 0, -std::cos(degrees * deg)} * 2.f);
        };
        check(near(weight(0), 1) && near(weight(8), 1, 1e-2f) && near(weight(25), 0, 1e-2f) &&
                  weight(30) == 0 && weight(16) > 0 && weight(16) < 1,
              "the gaze weight: full inside 8 degrees, none at 25");
        check(weight(12) > weight(20), "and falls off with the angle");
        check(gazeWeight({NAN, 0, 0}, eye, {0, 0, -3}) == 0, "a bad gaze weighs nothing");
    }
    {
        const Vec3 pelvis{0, .9f, 0}, rest = pelvis + Vec3{0, 0, -1.5f};
        check(length(bentFoot(pelvis, rest, {5, .9f, -5}, 0) - rest) < 1e-6f, "weight 0: the foot stays");
        const Vec3 far = bentFoot(pelvis, rest, {5, .9f, -5}, 1);
        check(length(far - rest) <= c.bendMetres + 1e-4f, "bent at most 0.25 m");
        check(near(length(far - pelvis), 1.5f, 1e-3f), "the foot keeps its reach");
        const float angle =
            std::acos(std::clamp(dot(normalized(far - pelvis), normalized(rest - pelvis)), -1.f, 1.f));
        check(angle <= c.bendDegrees * deg + 1e-3f, "and at most 15 degrees");
        const Vec3 half = bentFoot(pelvis, rest, {5, .9f, -5}, .5f);
        check(length(half - rest) < length(far - rest), "a half weight bends half as far");
        const Vec3 small = bentFoot(pelvis, rest, {.1f, .9f, -1.5f}, 1);
        check(near(small.x, .1f, 2e-2f), "a point near the line is reached");
    }

    // ---- the strength ----
    check(strengthOf(2.9f) == 0 && strengthOf(3.f) == 0 && near(strengthOf(6.5f), .5f) &&
              strengthOf(10.f) == 1 && strengthOf(40.f) == 1 && strengthOf(NAN) == 0,
          "no blow under 3 m/s, full at 10");
    check(flies(.7f) && !flies(.69f), "a hard kick flies him");
    {
        const Vec3 f = enemyFlight({0, 0, -1}, 12.f);
        check(near(f.z, -9.6f) && near(f.y, 1.5f), "the flight: 80 % of the foot's speed and a little up");
        check(length(enemyFlight({0, 0, -1}, 100.f)) < 14.f + 1.6f, "the flight is capped");
        check(length(enemyFlight({}, 10.f)) == 0, "no way, no flight");
    }

    // ---- the timeline and the legs ----
    for (const Gesture g : {Gesture::dive, Gesture::doubleDive, Gesture::roundhouse, Gesture::freeform}) {
        check(extendOf(g) > 0 && windowOf(g) > extendOf(g) && totalOf(g) > windowOf(g),
              "the timeline's order");
        check(amountAt(g, 0) == 0 && near(amountAt(g, windowOf(g)), 1) &&
                  near(amountAt(g, totalOf(g)), 0, 1e-4f),
              "the kick goes out, is held, and comes back");
        float last = 0;
        bool rising = true;
        for (float t = 0; t <= totalOf(g); t += .005f) {
            const float a = amountAt(g, t);
            check(a >= 0 && a <= 1.0001f, "the amount stays in 0..1");
            if (t > windowOf(g))
                rising = false;
            check(rising ? a >= last - 1e-6f : a <= last + 1e-6f, "out then back, no overshoot");
            last = a;
        }
        for (int side : {-1, 1})
            for (float angle : {0.f, 40.f, 90.f, 150.f})
                for (float t = 0; t <= totalOf(g); t += .02f) {
                    const auto legs = legPose(g, side, angle, t);
                    for (const auto& leg : legs)
                        check(std::isfinite(leg.pitch) && leg.pitch >= leg_control::pitchMin &&
                                  leg.pitch <= leg_control::pitchMax &&
                                  std::abs(leg.side) <= leg_control::sideMax && leg.knee >= 0 &&
                                  leg.knee <= leg_control::kneeMax,
                              "a leg pose within leg_control's limits");
                }
    }
    {
        const float t = windowOf(Gesture::dive);
        const auto right = legPose(Gesture::dive, 1, 40, t), left = legPose(Gesture::dive, -1, -40, t);
        check(right[1].pitch > 1.3f && right[1].knee < .2f && right[0].knee > .8f,
              "a right dive: the right leg straight out, the left bent");
        check(near(left[0].pitch, right[1].pitch) && near(left[0].side, -right[1].side) &&
                  near(left[1].knee, right[0].knee),
              "the left dive is the right's mirror");
        const auto both = legPose(Gesture::doubleDive, 1, 0, windowOf(Gesture::doubleDive));
        check(near(both[0].pitch, both[1].pitch) && near(both[0].side, -both[1].side) && both[0].pitch > 1.2f,
              "the double dive: both legs out together");
        const auto start = legPose(Gesture::roundhouse, 1, 0, .001f);
        const auto end = legPose(Gesture::roundhouse, 1, 0, windowOf(Gesture::roundhouse));
        check(std::abs(start[1].side) > std::abs(end[1].side) && start[1].side < 0 &&
                  near(end[1].side, 0, 1e-3f),
              "the roundhouse's leg comes in from its side (to the right: a negative side angle)");
        const auto back = legPose(Gesture::freeform, 1, 150, windowOf(Gesture::freeform));
        check(back[0].pitch < 0 && back[1].pitch < 0, "a freeform flick down kicks back");
        const auto out = legPose(Gesture::freeform, -1, -90, windowOf(Gesture::freeform));
        check(out[0].side > .5f && out[1].side == 0, "a freeform flick to the left kicks the left leg out");
        const auto none = legPose(Gesture::none, 1, 0, .1f);
        check(none[0].pitch == leg_control::centrePose[0].pitch &&
                  none[1].knee == leg_control::centrePose[1].knee,
              "no gesture, the centre pose");
    }
    check(sweepYaw(Gesture::dive, 1, 0) == 0 &&
              near(sweepYaw(Gesture::roundhouse, 1, 0), c.roundhouseArc * deg) &&
              near(sweepYaw(Gesture::roundhouse, -1, 0), -c.roundhouseArc * deg) &&
              near(sweepYaw(Gesture::roundhouse, 1, extendOf(Gesture::roundhouse)), 0),
          "the roundhouse's foot sweeps in from its side to the gaze");
    check(reachAt(Gesture::dive, 1.f) == c.reach + c.lunge && reachAt(Gesture::dive, 0) == 0 &&
              near(reachAt(Gesture::freeform, 1.f), (c.reach + c.lunge) * c.freeformReach) &&
              near(reachAt(Gesture::roundhouse, 0), c.reach + c.lunge),
          "the foot's reach: 1.15 m plus the lunge, a freeform blow's shorter");

    // ---- the leg layers' kick layer ----
    {
        leg_layers::Mixer mixer;
        leg_layers::Settings settings;
        leg_layers::Input in;
        in.dt = .01f;
        in.air = true;
        in.active = true;
        in.stickY = 1;
        in.kick = true;
        in.kickPose = legPose(Gesture::doubleDive, 1, 0, windowOf(Gesture::doubleDive));
        in.kickSpin = .3f;
        const auto out = mixer.update(in, settings);
        check(out.layer == leg_layers::Layer::kick && !out.useStick && out.run &&
                  near(out.couplingScale, .3f) && near(out.pose[0].pitch, in.kickPose[0].pitch),
              "the kick layer beats the stick, with the spin reaction scaled by KICK SPIN");
        in.kick = false;
        check(mixer.update(in, settings).layer == leg_layers::Layer::active,
              "without the kick the stick is the legs'");
        in.kick = true;
        in.air = false;
        check(mixer.update(in, settings).layer == leg_layers::Layer::off,
              "on the ground there is no kick layer");
    }

    // ---- the kick in the swing module ----
    {
        Kick k;
        std::vector<Kick::Target> none;
        auto f = frame(none);
        // Refusals.
        auto o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.event == Kick::Event::started && k.busy() && k.phase() == Kick::Phase::kicking,
              "a gesture starts a kick");
        check(k.ask(f, {Gesture::dive, 1, 40, 1}).refusal == Refusal::busy, "not while one is under way");
        k.reset();
        f.enabled = false;
        check(k.ask(f, {Gesture::dive, 1, 40, 1}).refusal == Refusal::off, "off");
        f.enabled = true;
        f.legs = false;
        check(k.ask(f, {Gesture::dive, 1, 40, 1}).refusal == Refusal::legs, "legs");
        f.legs = true;
        f.grounded = true;
        check(k.ask(f, {Gesture::dive, 1, 40, 1}).refusal == Refusal::grounded, "ground");
        f.grounded = false;
        f.wall = true;
        check(k.ask(f, {Gesture::dive, 1, 40, 1}).refusal == Refusal::wall, "wall");
        f.wall = false;
        check(k.ask(f, {Gesture::none, 0, 0, 0}).refusal == Refusal::noGesture, "no gesture");
        check(!k.busy(), "a refusal starts nothing");
        // A miss: nobody there.
        k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        Seen s = play(k, f);
        check(s.hits == 0 && s.misses == 1 && !k.busy() && s.took > .4f && s.took < .6f,
              "a kick at nobody is a miss and ends");
        check(s.maxFoot > 8.f, "the foot is fast");
        // The cooldown.
        check(k.ask(f, {Gesture::doubleDive, 1, 0, 1}).refusal == Refusal::busy, "a moment between kicks");
        f.seconds = .05f;
        for (int i = 0; i < 4; ++i)
            k.update(f);
        check(k.ask(f, {Gesture::doubleDive, 1, 0, 1}).event == Kick::Event::started, "then another");
    }
    {
        // A double dive at an enemy 1.3 m ahead at chest height: the foot meets his capsule.
        Kick k;
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -1.5f})};
        const auto f = frame(ts);
        const auto o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.event == Kick::Event::started && o.target == 7 && o.zone == Zone::chest,
              "the gaze picks him, the chest");
        const Seen s = play(k, f);
        check(s.hits == 1 && s.misses == 0 && s.hit.target == 7, "the foot lands a blow");
        check(s.hit.strength > .9f && s.hit.fly && s.hit.foot > 8.f, "a hard kick flies him");
        check(s.hit.enemy.z < -5.f && s.hit.enemy.y > 0 && length(s.hit.way) > .99f && s.hit.way.z < -.5f,
              "his flight goes the way the foot went");
        check(!k.busy() || k.phase() == Kick::Phase::recovering, "after the blow the kick winds down");
        check(!k.busy(), "and ends");
    }
    {
        // Out of reach (4 m): a miss.
        Kick k;
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -4.f})};
        const auto f = frame(ts);
        const auto o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.target == 0, "too far to be picked");
        const Seen s = play(k, f);
        check(s.hits == 0 && s.misses == 1, "out of reach is a miss");
    }
    {
        // The gaze picks between two: the one it points at.
        Kick k;
        std::vector<Kick::Target> ts{enemy(1, {-1.2f, 10, -1.2f}), enemy(2, {.1f, 10, -1.4f})};
        auto f = frame(ts);
        auto o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.target == 2, "straight ahead: the one in the gaze");
        k.reset();
        f.gaze = normalized(Vec3{-1.2f, 10 + .65f - 11.6f, -1.2f});
        o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.target == 1, "looking at the other: that one");
        // Outside the cone of the gaze: nobody.
        k.reset();
        f.gaze = {1, 0, 0};
        check(k.ask(f, {Gesture::doubleDive, 1, 0, 1}).target == 0, "looking away: nobody is picked");
    }
    {
        // Zones: a roundhouse goes for the head, a freeform for the nearest.
        Kick k;
        std::vector<Kick::Target> ts{enemy(3, {0, 10, -1.5f})};
        auto f = frame(ts);
        check(k.ask(f, {Gesture::roundhouse, 1, 0, 1}).zone == Zone::head, "a roundhouse: the head");
        k.reset();
        f.gaze = normalized(Vec3{0, (10 + .2f * 1.8f) - 11.6f, -1.5f});
        check(k.ask(f, {Gesture::freeform, 1, 90, 1}).zone == Zone::legs,
              "a freeform blow: the zone the gaze is on");
        k.reset();
        check(k.ask(f, {Gesture::doubleDive, 1, 0, 1}).zone == Zone::chest,
              "a double dive: always the chest");
    }
    {
        // A roundhouse sweeps in from the side and hits the enemy at the gaze at the end of its arc.
        Kick k;
        std::vector<Kick::Target> ts{enemy(4, {0, 10, -1.3f})};
        const auto f = frame(ts);
        k.ask(f, {Gesture::roundhouse, 1, 0, 1});
        const Seen s = play(k, f);
        check(s.hits == 1 && s.hit.target == 4 && s.hit.way.x < -.2f,
              "the roundhouse lands, the foot moving in from the right");
        // A target the arc starts on is hit sooner.
        Kick k2;
        std::vector<Kick::Target> side{enemy(5, {1.5f, 10, -.2f})};
        auto f2 = frame(side);
        k2.ask(f2, {Gesture::roundhouse, 1, 0, 1});
        check(play(k2, f2).hits == 1, "an enemy on the side it starts from is hit by the sweep");
        // From the left it misses him.
        Kick k3;
        k3.ask(f2, {Gesture::roundhouse, -1, 0, 1});
        check(play(k3, f2).hits == 0, "from the other side it does not");
    }
    {
        // A freeform blow is weaker: it may not fly him.
        Kick k;
        std::vector<Kick::Target> ts{enemy(6, {0, 10, -1.0f})};
        const auto f = frame(ts);
        k.ask(f, {Gesture::freeform, 1, 150, 1});
        const Seen s = play(k, f);
        check(s.hits == 1, "a freeform blow lands at close range");
        check(s.hit.strength < 1.f && s.hit.foot < s.maxFoot + 1e-3f && s.hit.foot > c.minFoot,
              "slower and shorter than a dive");
        Kick d;
        d.ask(f, {Gesture::doubleDive, 1, 0, 1});
        const Seen sd = play(d, f);
        check(sd.maxFoot > s.maxFoot, "a dive's foot is faster than a freeform's");
    }
    {
        // Another enemy in the way is hit as well as the one picked: whoever the foot meets first.
        Kick k;
        std::vector<Kick::Target> ts{enemy(8, {0, 10, -3.5f}), enemy(9, {.2f, 10, -1.1f})};
        const auto f = frame(ts);
        const auto o = k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        check(o.target == 9, "the one in reach is the one picked");
        const Seen s = play(k, f);
        check(s.hits == 1 && s.hit.target == 9, "he is hit");
    }
    {
        // Lost: the player lands, the kick ends where it is without a hit.
        Kick k;
        std::vector<Kick::Target> none;
        auto f = frame(none);
        k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        f.seconds = .02f;
        k.update(f);
        f.grounded = true;
        const auto o = k.update(f);
        check(o.lost && o.event == Kick::Event::miss && !k.busy(), "landing ends the kick");
        // The setting off mid-kick too.
        k.reset();
        k.ask(frame(none), {Gesture::dive, 1, 40, 1});
        auto off = frame(none);
        off.enabled = false;
        off.seconds = .02f;
        check(k.update(off).lost && !k.busy(), "switching it off ends the kick");
        // Progress runs 0..1.
        k.reset();
        k.ask(frame(none), {Gesture::dive, 1, 40, 1});
        check(k.progress() == 0, "progress starts at 0");
        auto step = frame(none);
        step.seconds = .1f;
        k.update(step);
        check(k.progress() > .1f && k.progress() < 1.f, "and runs on");
    }
    {
        // A hit's foot never uses a dt of 0 (a repeated sample) as a speed.
        Kick k;
        std::vector<Kick::Target> ts{enemy(7, {0, 10, -1.5f})};
        auto f = frame(ts);
        k.ask(f, {Gesture::doubleDive, 1, 0, 1});
        f.seconds = 0;
        for (int i = 0; i < 5; ++i)
            check(k.update(f).event == Kick::Event::none, "a zero step does nothing");
        f.seconds = NAN;
        check(k.update(f).event == Kick::Event::none, "NaN steps do nothing");
    }
    std::printf("air_kick: %d checks passed\n", passed);
    return 0;
}
