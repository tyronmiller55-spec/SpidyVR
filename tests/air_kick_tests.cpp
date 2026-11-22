// AIR KICKS (air_kick.hpp, K2; November 14): the stick's gestures, the zone and the gaze, the foot's bend,
// the kick's strength, the timeline and legs' pose, and the kick in the swing module against fighting
// enemies. Portable: no Windows, no headset, no game.
#include "spidy/air_kick.hpp"
#include "spidy/leg_layers.hpp"
#include <cmath>
#include <cstdio>
#include <string>
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
    // The flick that breaks a midair animation (Nov 18): the stick passes the break's 0.2 in the animation and 0.8
    // a frame later in SpidyVR. A watch that starts only in SpidyVR never saw the stick at rest and loses the
    // flick; watched from the legs' chord on (game_xr's kickWatch), it is the kick.
    {
        Gestures late;
        late.update(.014f, 0.f, .1f, false);           // the animation: not watched
        late.update(.014f, 0.f, .25f, false);          // the break's frame: still the animation
        const Flick lost = late.update(.014f, 0.f, .9f, true); // SpidyVR: over 0.8, no rest seen
        check(lost.gesture == Gesture::none, "a watch begun mid-flick cannot see it (the old loss)");
        Gestures watched;
        watched.update(.014f, 0.f, .1f, true);
        watched.update(.014f, 0.f, .25f, true);
        const Flick taken = watched.update(.014f, 0.f, .9f, true);
        check(taken.gesture == Gesture::doubleDive, "watched from the chord on, the breaking flick is the kick");
    }
    {
        swing_kick::Flick late, watched;
        late.update(.014f, 0.f, .2f, false);
        check(!late.update(.014f, 0.f, .9f, true), "the swing kick's flick: lost to a late watch");
        watched.update(.014f, 0.f, .2f, true);
        check(watched.update(.014f, 0.f, .9f, true), "the swing kick's flick: taken when watched from the chord");
    }
    // KICK TARGET FALLBACK (the 2 AM log: 25 of 25 kicks started with target=0): an enemy 60 degrees off the gaze,
    // within reach, is the target, and the kick goes his way; one behind is not.
    {
        const float a = 60.f * 3.14159265f / 180.f;
        const Kick::Target side[] = {enemy(9, Vec3{std::sin(a) * 1.2f, 10, -std::cos(a) * 1.2f})};
        Kick k;
        const auto o = k.ask(frame(side), {Gesture::dive, 1, 30, .9f});
        check(o.target == 9 && k.fallback(), "60 degrees off the gaze, in reach: the fallback target");
        const Kick::Target behind[] = {enemy(9, Vec3{0, 10, 1.2f})};
        Kick b;
        check(b.ask(frame(behind), {Gesture::dive, 1, 30, .9f}).target == 0 && !b.fallback(), "behind: none");
        const Kick::Target ahead[] = {enemy(5, Vec3{0, 10, -1.2f})};
        Kick g;
        check(g.ask(frame(ahead), {Gesture::dive, 1, 30, .9f}).target == 5 && !g.fallback(), "in the gaze: no fallback");
    }
    // KICK LOCK TARGET (the Nov 19 log: an enemy 1.9 m behind, the punches locked on him, the air kick target=0): the
    // punches' lock is the target when nothing is in the gaze or in front and the leg reaches him; out of reach, not.
    {
        const Kick::Target behind[] = {enemy(9, Vec3{0, 10, 1.2f})};
        auto f = frame(behind);
        f.locked = 9;
        Kick k;
        const auto o = k.ask(f, {Gesture::dive, 1, 30, .9f});
        check(o.target == 9 && k.fallback() && k.lockAimed(), "behind but the punches' lock, in reach: the target");
        f.locked = 4;
        Kick other;
        check(other.ask(f, {Gesture::dive, 1, 30, .9f}).target == 0 && !other.lockAimed(), "a lock on someone else: none");
        const Kick::Target far[] = {enemy(9, Vec3{0, 10, 4.f})};
        auto ff = frame(far);
        ff.locked = 9;
        Kick out;
        check(out.ask(ff, {Gesture::dive, 1, 30, .9f}).target == 0, "the lock out of the leg's reach: none");
        const Kick::Target ahead[] = {enemy(5, Vec3{0, 10, -1.2f}), enemy(9, Vec3{0, 10, 1.2f})};
        auto fa = frame(ahead);
        fa.locked = 9;
        Kick g;
        check(g.ask(fa, {Gesture::dive, 1, 30, .9f}).target == 5 && !g.lockAimed(), "the gaze's enemy comes first");
    }
    // KICK LUNGE (beta3f; the beta3b and beta3c logs: 11 kicks, none with an enemy in the leg's reach): off
    // the ground, an enemy up to lungeReach past the leg is the target and the kick carries the body at him;
    // off (0), beta3e's reach; standing, never.
    {
        const Kick::Target far[] = {enemy(5, Vec3{0, 10, -3.5f})};
        Kick off;
        const auto o0 = off.ask(frame(far), {Gesture::dive, 1, 30, .9f});
        check(o0.target == 0 && !off.lunged() && o0.lungeSpeed == 0,
              "off: 3.5 m is out of the leg's reach (beta3e)");
        Config lc;
        lc.lungeReach = 2.5f;
        Kick k(lc);
        const auto o = k.ask(frame(far), {Gesture::dive, 1, 30, .9f});
        check(o.target == 5 && k.lunged(), "on: 3.5 m ahead is the lunge's");
        check(o.lungeWay.z < -.9f && o.lungeSpeed > 1.f && o.lungeSpeed <= lc.lungeSpeed && o.lungeGap > 1.f,
              "the lunge goes at him, as fast as the window needs, at most its cap");
        // Carried along the lunge, the foot lands within the kick's window.
        auto f = frame(far);
        Vec3 feet = f.feet;
        bool hit = false;
        for (int i = 0; i < 60 && !hit && k.busy(); ++i) {
            feet = feet + o.lungeWay * (o.lungeSpeed / 90.f);
            f.feet = feet;
            f.seconds = 1.f / 90;
            hit = k.update(f).event == Kick::Event::hit;
        }
        check(hit, "the lunge did not bring the foot onto him");
        // Within the leg's reach: no lunge (the kick as before).
        const Kick::Target close[] = {enemy(6, Vec3{0, 10, -1.2f})};
        Kick c(lc);
        const auto oc = c.ask(frame(close), {Gesture::dive, 1, 30, .9f});
        check(oc.target == 6 && !c.lunged() && oc.lungeSpeed == 0, "in reach: no lunge");
        // Past the lunge's reach: none.
        const Kick::Target beyond[] = {enemy(7, Vec3{0, 10, -6.f})};
        Kick b(lc);
        check(b.ask(frame(beyond), {Gesture::dive, 1, 30, .9f}).target == 0 && !b.lunged(),
              "6 m: past the lunge");
        // Standing (GROUND KICKS): never a lunge.
        auto g = frame(far);
        g.grounded = true;
        g.ground = true;
        Kick s(lc);
        const auto og = s.ask(g, {Gesture::freeform, 1, 0, .9f});
        check(og.event == Kick::Event::started && og.target == 0 && !s.lunged(), "a standing kick lunged");
    }
    // GROUND LUNGE (beta3g): standing, an enemy past the leg's reach (up to groundLungeReach past it) is the
    // target and the kick steps the body at him, at most groundLungeSpeed; the kick goes on while the burst
    // runs faster than a standing kick may start; off (0), beta3f's none.
    {
        const Kick::Target out[] = {enemy(5, Vec3{0, 10, -2.6f})};
        auto g = frame(out);
        g.grounded = g.ground = true;
        Kick off;
        check(off.ask(g, {Gesture::dive, 1, 30, .9f}).target == 0 && !off.lunged(), "ground lunge off: out of reach");
        Config gc;
        gc.groundLungeReach = 2.f;
        Kick k(gc);
        const auto o = k.ask(g, {Gesture::dive, 1, 30, .9f});
        check(o.event == Kick::Event::started && o.target == 5 && k.lunged() && k.onGround(), "no ground lunge");
        check(o.lungeWay.z < -.9f && o.lungeSpeed > 1.f && o.lungeSpeed <= gc.groundLungeSpeed + 1e-4f,
              "the ground lunge's way and speed");
        // Stepping in at the burst's speed (past groundSpeed + slack): the kick goes on and lands.
        auto f = g;
        bool hit = false, lost = false;
        for (int i = 0; i < 60 && !hit && k.busy(); ++i) {
            f.feet = f.feet + Vec3{0, 0, -o.lungeSpeed / 90.f};
            f.speed = o.lungeSpeed;
            f.seconds = 1.f / 90;
            const auto u = k.update(f);
            hit = u.event == Kick::Event::hit;
            lost = lost || u.lost;
        }
        check(hit && !lost, "the ground lunge did not land (or the burst's speed ended the kick)");
        // A sweep never lunges.
        Kick sw(gc);
        sw.ask(g, {Gesture::sweep, 1, 0, .9f});
        check(!sw.lunged(), "a sweep lunged");
        // KICK GROUND SPEED (beta3g): the start's limit is the config's (450 cm/s from the setting).
        Config fast;
        fast.groundSpeed = 4.5f;
        auto moving = frame(out);
        moving.grounded = moving.ground = true;
        moving.speed = 4.f;
        Kick slowK, fastK(fast);
        check(slowK.ask(moving, {Gesture::dive, 1, 30, .9f}).refusal == Refusal::moving, "4 m/s at the old 3 m/s");
        check(fastK.ask(moving, {Gesture::dive, 1, 30, .9f}).event == Kick::Event::started, "4 m/s refused at 4.5");
    }
    // KICK REBOUND (beta3g): up 3 m/s and back 2.5 along the kick's level way.
    {
        const Vec3 r = rebound({0, -.6f, -.8f});
        check(near(r.y, 3.f) && near(r.z, 2.5f) && near(r.x, 0.f), "the rebound");
        check(near(rebound({0, -1, 0}).y, 3.f) && near(length(Vec3{rebound({0, -1, 0}).x, 0, rebound({0, -1, 0}).z}), 0),
              "straight down: up only");
        check(finite(rebound({NAN, 0, 0})), "a NaN way");
    }
    // KICK TARGET RING (beta3g): the gaze's enemy within reach, else the nearest in front, else the lock; none
    // out of reach.
    {
        const Kick::Target ts[] = {enemy(5, Vec3{-1.f, 10, -1.f}), enemy(6, Vec3{0, 10, -1.3f}), enemy(7, Vec3{0, 10, 1.2f})};
        const auto all = std::span<const Kick::Target>(ts);
        check(preview(all, {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0).id == 6, "the gaze's enemy");
        const auto p = preview(all, {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0);
        check(near(p.point.z, -1.3f) && p.radius > .2f, "the ring sits on his chest");
        const Kick::Target behind[] = {enemy(7, Vec3{0, 10, 1.2f})};
        check(preview(std::span<const Kick::Target>(behind), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0).id == 0,
              "behind, no lock: none");
        check(preview(std::span<const Kick::Target>(behind), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 7).id == 7,
              "behind, the lock: him");
        const Kick::Target far[] = {enemy(8, Vec3{0, 10, -3.5f})};
        Config lc;
        check(preview(std::span<const Kick::Target>(far), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0, lc).id == 0,
              "out of reach: none");
        lc.lungeReach = 2.5f;
        check(preview(std::span<const Kick::Target>(far), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0, lc).id == 8,
              "the lunge's reach");
        check(preview(std::span<const Kick::Target>(far), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, true, 0, lc).id == 0,
              "standing: the lunge's reach is the ground lunge's (off)");
        check(preview(std::span<const Kick::Target>(), {0, 10, 0}, {0, 1.6f, 0}, {0, 0, -1}, false, 0).id == 0, "none");
    }
    // KICK LEAN and KICK MOMENTUM (beta3g): the dive arches back, the roundhouse twists, none at rest; the
    // legs swing with the travel and keep their limits; the torso goes on the channel in the kick layer.
    {
        const float mid = extendOf(Gesture::dive) + .05f;
        check(kickLean(Gesture::dive, 1, 30, mid).pitch < -.3f, "the dive does not arch back");
        check(kickLean(Gesture::doubleDive, 1, 0, mid).pitch < kickLean(Gesture::dive, 1, 30, mid).pitch,
              "the double dive arches more");
        check(std::abs(kickLean(Gesture::roundhouse, 1, 60, .15f).yaw) > .1f, "the roundhouse does not twist");
        check(kickLean(Gesture::dive, 1, 30, 0).pitch == 0 && kickLean(Gesture::sweep, 1, 0, mid).pitch == 0,
              "a lean at rest or on a sweep");
        const auto pose = legPose(Gesture::dive, 1, 30, mid);
        const auto forward = momentumPose(pose, 10.f, 0.f, 0.f), back = momentumPose(pose, -10.f, 0.f, 0.f);
        check(forward[0].pitch >= pose[0].pitch && back[0].pitch < pose[0].pitch, "the legs do not carry the travel");
        check(back[0].pitch > pose[0].pitch - momentumMaxRadians - 1e-4f, "past the momentum's most");
        const auto wild = momentumPose(pose, 1e4f, -1e4f, 0.f);
        for (const auto& leg : wild)
            check(leg.pitch <= leg_control::pitchMax && std::abs(leg.side) <= leg_control::sideMax, "past a leg's limit");
        const auto same = momentumPose(pose, NAN, 0.f, 0.f);
        check(same[1].pitch == pose[1].pitch, "a NaN travel moved the legs");
        leg_layers::Mixer mixer;
        leg_layers::Input in;
        in.air = true;
        in.kick = true;
        in.kickPose = pose;
        auto o = mixer.update(in, {});
        check(o.layer == leg_layers::Layer::kick && !o.torsoOn, "KICK LEAN off: the torso was driven");
        in.kickLeanOn = true;
        in.kickTorso = {-.42f, 0, 5.f};
        o = mixer.update(in, {});
        check(o.torsoOn && near(o.torso.pitch, -.42f) && near(o.torso.yaw, .7f), "the kick's lean on the channel (clamped)");
    }
    // KICK CHAMBER (beta3g; t100u "we need lots of animation work there"): the kicking leg winds up (knee
    // bent, thigh part way), snaps out straight past its line a little, then folds back as it comes home.
    {
        const float e = extendOf(Gesture::dive), w = windowOf(Gesture::dive);
        const auto wind = shapedLegPose(Gesture::dive, 1, 30, .4f * e);
        const auto line = shapedLegPose(Gesture::dive, 1, 30, e);
        const auto held = shapedLegPose(Gesture::dive, 1, 30, e + .5f * (w - e));
        const auto fold = shapedLegPose(Gesture::dive, 1, 30, w + .5f * Config{}.recover);
        const auto stiff = legPose(Gesture::dive, 1, 30, e);
        check(wind[1].knee > .6f && wind[1].pitch < .8f * stiff[1].pitch, "no chamber: the knee not drawn up");
        check(line[1].knee < .2f && near(line[1].pitch, stiff[1].pitch, .05f), "not out straight on its line");
        check(held[1].pitch > line[1].pitch, "no overshoot through the line");
        check(fold[1].knee > legPose(Gesture::dive, 1, 30, w + .5f * Config{}.recover)[1].knee + .2f, "no recoil fold");
        const auto rest = shapedLegPose(Gesture::dive, 1, 30, totalOf(Gesture::dive) + .01f);
        check(std::abs(rest[1].pitch - leg_control::centrePose[1].pitch) < .05f, "not home after the kick");
        check(shapedLegPose(Gesture::dive, 1, 30, .4f * e)[0].knee == legPose(Gesture::dive, 1, 30, .4f * e)[0].knee,
              "the other leg was shaped");
        for (float t = 0; t < totalOf(Gesture::roundhouse) + .05f; t += .01f)
            for (const auto& leg : shapedLegPose(Gesture::roundhouse, -1, 60, t))
                check(std::isfinite(leg.pitch) && leg.knee <= leg_control::kneeMax && leg.knee >= 0, "a leg out of its range");
        check(kickLean(Gesture::dive, 1, 30, .4f * e).pitch > kickLean(Gesture::dive, 1, 30, e).pitch + .2f,
              "no curl into the wind-up");
        leg_layers::Mixer quick, slow;
        leg_layers::Input in;
        in.air = in.kick = true;
        in.dt = .03f;
        in.kickQuick = true;
        const float fast = quick.update(in, {}).amount;
        in.kickQuick = false;
        check(fast > .45f && slow.update(in, {}).amount < .2f, "the kick layer's quick blend");
    }
    // KICK FLICK MISSES: a slow push and a short one are logged with why; a flick and a resting thumb are not.
    {
        FlickWatch w;
        const float dt = 1.f / 72;
        FlickMiss m;
        for (int i = 0; i <= 30; ++i)
            m = w.update(dt, 0, std::min(1.f, .3f + i * .03f), true, false); // 0.3 -> 1 over ~0.33 s
        check(!m.why, "still out: nothing yet");
        m = w.update(dt, 0, 0, true, false);
        check(m.why && std::string(m.why) == "too slow" && m.peak > .99f && m.highSeconds > .15f, "a slow push: too slow");
        for (int i = 0; i < 10; ++i)
            m = w.update(dt, 0, .6f, true, false);
        m = w.update(dt, 0, .1f, true, false);
        check(m.why && std::string(m.why) == "too small" && near(m.peak, .6f) && m.highSeconds < 0, "a short push: too small");
        w.update(dt, 0, .9f, true, true);
        m = w.update(dt, 0, .1f, true, false);
        check(!m.why, "a push a kick came of: no miss");
        w.update(dt, 0, .45f, true, false);
        m = w.update(dt, 0, .1f, true, false);
        check(!m.why, "a resting thumb: no try");
        // R2 (the 2:23 AM log): past 0.75 at 14 ms is no slow flick.
        FlickWatch fast;
        m = fast.update(dt, 0, 1.f, true, false); // the legs engage with the stick already out
        m = fast.update(dt, 0, 1.f, true, false);
        m = fast.update(dt, 0, 0, true, false);
        check(m.why && std::string(m.why).rfind("not armed", 0) == 0, "out when the legs engaged: not armed");
        FlickWatch armed;
        armed.update(dt, 0, .1f, true, false);
        armed.update(dt, 0, 1.f, true, false);
        m = armed.update(dt, 0, .1f, true, false);
        check(m.why && std::string(m.why).rfind("not taken", 0) == 0, "armed and in time but no kick: not taken");
        FlickWatch slow;
        slow.update(dt, 0, .1f, true, false);
        for (int i = 0; i <= 30; ++i)
            slow.update(dt, 0, std::min(1.f, .36f + i * .03f), true, false);
        m = slow.update(dt, 0, 0, true, false);
        check(m.why && std::string(m.why) == "too slow", "still too slow when it is");
    }
    std::printf("air_kick: %d checks passed\n", passed);
    return 0;
}
