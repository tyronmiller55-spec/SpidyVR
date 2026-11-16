// PASSIVE LEGS, AIR CROUCH, REACH SPREAD and FLAIL (leg_layers.hpp; the user,
// November 12 06:18): the layers' priority, the passive spring with no stick,
// the coupling share, the auto poses' thresholds, the flail's B, no pops.
#include "spidy/leg_layers.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::leg_layers;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float dt = 1.f / 90;
Input air() {
    Input in;
    in.dt = dt;
    in.air = true;
    in.crouch.upDot = .5f; // tilted: no crouch unless a test asks
    return in;
}
float legsDelta(const Legs& a, const Legs& b) {
    float m = 0;
    for (int i = 0; i < 2; ++i)
        m = std::max({m, std::abs(a[i].pitch - b[i].pitch), std::abs(a[i].side - b[i].side),
                      std::abs(a[i].knee - b[i].knee)});
    return m;
}
} // namespace

int main() {
    Settings set;
    // PASSIVE: shown 60%, 15% coupling, the centre pose (no stick).
    {
        Mixer m;
        Output o;
        for (int i = 0; i < 30; ++i)
            o = m.update(air(), set);
        check(o.layer == Layer::passive && o.run && !o.useStick, "in the air: passive");
        check(std::abs(o.amount - .6f) < 1e-4f && std::abs(o.couplingScale - .15f) < 1e-6f, "shown 60%, coupling 15%");
        Mixer m2;
        o = m2.update(air(), set);
        check(o.amount < .1f && o.layerChanged, "eased in (no pop): 0.2 s full scale");
        Settings off = set;
        off.passivePct = 0;
        Mixer m3;
        check(m3.update(air(), off).layer == Layer::off, "PASSIVE LEGS OFF: nothing");
        Input ground = air();
        ground.air = false;
        check(m.update(ground, set).layer == Layer::off && !m.update(ground, set).run, "on the ground: off");
    }
    // The passive spring: momentum moves the legs, no stick; the turn is 15% of the active's.
    {
        leg_control::Simulation still, spun, braked, active;
        const auto p = leg_control::fromPercent(100, 100, 50);
        auto pp = p;
        pp.coupling *= .15f;
        leg_control::Motion none, spin, brake;
        spin.angularVelocity = {0, 0, 6};             // a yaw spin
        brake.specificForce = {-12, 0, 0};            // slowing down hard (force back)
        leg_control::Output o1{}, o2{}, o3{};
        float passiveTurn = 0, activeTurn = 0;
        for (int i = 0; i < 90; ++i) {
            o1 = still.updateTarget(dt, true, leg_control::centrePose, 0, 0, none, pp);
            o2 = spun.updateTarget(dt, true, leg_control::centrePose, 0, 0, spin, pp);
            o3 = braked.updateTarget(dt, true, leg_control::centrePose, 0, 0, brake, pp);
        }
        check(legsDelta(o1.legs, leg_control::centrePose) < 1e-3f, "no motion: the legs stay as they hang");
        check(o2.legs[0].side - o2.legs[1].side > .2f, "a spin throws them apart");
        check(o3.legs[0].pitch > .15f && o3.legs[1].pitch > .15f, "slowing down swings them forward");
        leg_control::Simulation ps, as;
        leg_control::Motion swing;
        for (int i = 0; i < 180; ++i) {
            swing.specificForce = {14.f * std::sin(i * dt * 6.f), 0, 9.8f};
            passiveTurn += std::abs(ps.updateTarget(dt, true, leg_control::centrePose, 0, 0, swing, pp).reactionPitch);
            activeTurn += std::abs(as.updateTarget(dt, true, leg_control::centrePose, 0, 0, swing, p).reactionPitch);
        }
        check(activeTurn > 0 && std::abs(passiveTurn / activeTurn - .15f) < .01f, "passive turn: 15% of the active's");
    }
    // AIR CROUCH thresholds.
    {
        CrouchInput c;
        c.upDot = std::cos(20 * 3.14159265f / 180);
        c.airSeconds = .5f;
        c.vy = -1;
        c.spin = .5f;
        check(crouchWanted(c, false), "upright, 0.5 s up, calm: crouch");
        CrouchInput hop = c;
        hop.airSeconds = .3f;
        hop.vy = 3;
        check(!crouchWanted(hop, false), "a 0.3 s hop still rising: no");
        CrouchInput apex = hop;
        apex.airSeconds = .2f;
        apex.vy = .5f;
        check(crouchWanted(apex, false), "near the apex after 0.15 s: crouch");
        CrouchInput tilt = c;
        tilt.upDot = std::cos(35 * 3.14159265f / 180);
        check(!crouchWanted(tilt, false) && crouchWanted(tilt, true), "35 deg: not in, but stays (out past 40)");
        tilt.upDot = std::cos(45 * 3.14159265f / 180);
        check(!crouchWanted(tilt, true), "45 deg: out");
        CrouchInput spin = c;
        spin.spin = 2;
        check(!crouchWanted(spin, false) && crouchWanted(spin, true), "2 rad/s: not in, stays");
        spin.spin = 4;
        check(!crouchWanted(spin, true), "a flip's spin: out");
        CrouchInput web = c;
        web.webbed = true;
        check(!crouchWanted(web, false), "on a web: no");
        CrouchInput fall = c;
        fall.vy = -8;
        check(!crouchWanted(fall, true), "falling fast: the legs come down");
        // In the mixer: eased in over 0.25 s, then out.
        Mixer m;
        Input in = air();
        in.crouch = c;
        Output o;
        float last = 0, maxStep = 0;
        for (int i = 0; i < 90; ++i) {
            o = m.update(in, set);
            maxStep = std::max(maxStep, o.crouchWeight - last);
            last = o.crouchWeight;
        }
        check(o.layer == Layer::autoPose && o.crouchWeight == 1 && std::abs(o.amount - 1) < 1e-4f, "crouch: fully shown");
        check(maxStep <= dt / crouchInSeconds + 1e-5f, "eased (no pop)");
        check(std::abs(o.pose[0].knee - crouchPose[0].knee) < 1e-4f, "the crouch pose");
        Settings noCrouch = set;
        noCrouch.crouch = false;
        Mixer m2;
        for (int i = 0; i < 90; ++i)
            o = m2.update(in, noCrouch);
        check(o.layer == Layer::passive && o.crouchWeight == 0, "AIR CROUCH off: never");
    }
    // AIR CROUCH FLICKER (the 5061b4a playtest: 41% of 523 crouches ended inside 0.35 s): a
    // minimum on-time, exits that have to last, and a fall start above -5.
    {
        auto calm = [&] {
            Input in = air();
            in.crouch.upDot = 1;
            in.crouch.vy = 0;
            in.crouch.spin = 0;
            return in;
        };
        // The fall's start is higher than its stop: -6 m/s neither starts nor, once on, stops one.
        CrouchInput f;
        f.upDot = 1;
        f.airSeconds = .5f;
        f.vy = -6;
        check(!crouchWanted(f, false) && crouchWanted(f, true),
              "-6 m/s: not in (above -5 only), stays (out at -7)");
        f.vy = -4.5f;
        check(crouchWanted(f, false), "-4.5 m/s: in");
        // Crouch on, then a spin spike of a few frames: it stays.
        Mixer m;
        Output o;
        for (int i = 0; i < 60; ++i)
            o = m.update(calm(), set);
        check(o.crouchOn, "the crouch is on");
        Input spike = calm();
        spike.crouch.spin = 4;
        int offs = 0;
        for (int i = 0; i < 6; ++i) // 67 ms of spin
            offs += !m.update(spike, set).crouchOn;
        check(offs == 0, "a spin spike of 67 ms (under the exit's 0.12 s) does not drop it");
        o = m.update(calm(), set);
        check(o.crouchOn, "and the spike is forgotten when it ends");
        for (int i = 0; i < 6; ++i)
            m.update(spike, set);
        o = m.update(calm(), set);
        check(o.crouchOn, "a second spike after a calm frame starts the exit's clock again");
        // A lasting reason: gone after the exit time, once the minimum has passed.
        Mixer held;
        for (int i = 0; i < 60; ++i)
            held.update(calm(), set);
        Input flip = calm();
        flip.crouch.spin = 6;
        bool gone = false;
        int frames = 0;
        while (!gone && frames < 90) {
            gone = !held.update(flip, set).crouchOn;
            ++frames;
        }
        check(gone && frames * dt >= crouchExitSeconds - dt && frames * dt <= crouchExitSeconds + 3 * dt,
              "a lasting spin lets it go after 0.12 s");
        // The minimum: it starts, and an immediate reason to leave waits for 0.35 s.
        Mixer fresh;
        int on = 0;
        for (int i = 0; i < 30 && !on; ++i)
            on = fresh.update(calm(), set).crouchOn ? i + 1 : 0;
        check(on > 0, "a fresh crouch starts");
        Input fall = calm();
        fall.crouch.vy = -9;
        int stayed = 0;
        while (fresh.update(fall, set).crouchOn && stayed < 200)
            ++stayed;
        check((stayed + 1) * dt >= crouchMinOnSeconds - 2 * dt &&
                  (stayed + 1) * dt <= crouchMinOnSeconds + 3 * dt,
              "a fall right after the start: on for 0.35 s in all, then off");
        // A web's attach at once does the same.
        Mixer webbed;
        for (int i = 0; i < 40; ++i)
            webbed.update(calm(), set);
        Input web = calm();
        web.crouch.webbed = true;
        int w = 0;
        for (int i = 0; i < 8; ++i)
            w += !webbed.update(web, set).crouchOn;
        check(w == 0, "a web attached for under 0.12 s is not an exit");
        // Landing is at once, whatever the minimum.
        Mixer land;
        for (int i = 0; i < 60; ++i)
            land.update(calm(), set);
        Input ground = calm();
        ground.air = false;
        check(!land.update(ground, set).crouchOn && land.update(ground, set).crouchWeight == 0,
              "touching the ground ends it at once");
        // The noise at the fall's edge: vy wobbling across -7 flickers no more than once.
        Mixer edge;
        int changes = 0;
        Input wob = calm();
        for (int i = 0; i < 180; ++i) {
            wob.crouch.vy = -6.f + (i % 2 ? 1.5f : -2.f); // -7.5 / -4.5 alternately
            changes += edge.update(wob, set).crouchChanged;
        }
        check(changes <= 2, "vy flipping across -7 every frame: at most the on and the off");
    }
    // REACH SPREAD.
    {
        check(handBetween({.1f, .05f, -.1f}, false), "a hand below the pelvis between the thighs");
        check(!handBetween({.1f, .2f, -.1f}, false) && handBetween({.1f, .2f, -.1f}, true), "20 cm out: not in, stays");
        check(!handBetween({.1f, .05f, .02f}, false) && handBetween({.1f, .05f, .02f}, true), "at the pelvis: not in, stays");
        check(!handBetween({.1f, .05f, .06f}, true), "above: out");
        check(!handBetween({-.3f, 0, -.2f}, false) && !handBetween({.5f, 0, -.2f}, false), "behind or far ahead: no");
        check(std::abs(pelvisDrop(1.6f) - .704f) < 1e-3f && pelvisDrop(3) == .9f, "the pelvis from the eyes");
        Mixer m;
        Input in = air();
        in.handBetween[1] = true;
        Output o;
        for (int i = 0; i < 40; ++i)
            o = m.update(in, set);
        check(o.reachOn && o.reachWeight == 1 && o.pose[0].side > .4f && o.pose[0].pitch > .6f && o.pose[0].knee > .8f,
              "spread, lifted forward, knees bent");
        in.crouch.upDot = 1;
        in.crouch.spin = 0;
        for (int i = 0; i < 90; ++i)
            o = m.update(in, set);
        check(o.crouchWeight == 1 && o.reachWeight == 1 && o.pose[0].side > .5f, "crouch and reach together");
    }
    // FLAIL and the priority: active > flail > auto > passive.
    {
        Mixer m;
        Input in = air();
        in.flail = true;
        Output o = m.update(in, set);
        check(o.layer == Layer::flail && !o.useStick && o.couplingScale == 1, "flail");
        float firstPeak = -1;
        for (int i = 1; i < 90; ++i) {
            const Legs prev = o.pose;
            o = m.update(in, set);
            if (firstPeak < 0 && o.pose[0].pitch < prev[0].pitch && i > 1)
                firstPeak = i * dt;
        }
        check(firstPeak > .08f && firstPeak < .16f, "a 2.2 Hz cycle (left leg's first peak at a quarter)");
        check(std::abs(flailPose(1.5708f)[0].pitch - flailPose(1.5708f + 3.14159f)[1].pitch) < 1e-3f,
              "legs in opposition");
        in.active = true;
        check(m.update(in, set).layer == Layer::flail, "Y+X engaged, stick at rest: the flail");
        in.stickX = .6f;
        check(m.update(in, set).layer == Layer::active, "the right stick pushed: active wins");
        in.flail = false;
        in.stickX = 0;
        check(m.update(in, set).layer == Layer::active, "engaged without B: active");
        in.active = false;
        in.easing = true;
        check(m.update(in, set).layer == Layer::easing, "let go of: easing");
        in.easing = false;
        in.handBetween[0] = true;
        check(m.update(in, set).layer == Layer::autoPose, "then the auto pose");
        in.handBetween[0] = false;
        for (int i = 0; i < 40; ++i)
            o = m.update(in, set);
        check(o.layer == Layer::passive, "then passive");
    }
    // No pops: the spring across every layer change.
    {
        Mixer m;
        leg_control::Simulation sim;
        const auto p = leg_control::fromPercent(100, 100, 50);
        Legs last = leg_control::centrePose;
        float worst = 0, worstAmount = 0, lastAmount = 0;
        for (int i = 0; i < 90 * 6; ++i) {
            Input in = air();
            const int s = i / 90;
            if (s == 1)
                in.crouch = {1, 0, 0, 0, false};
            if (s == 2)
                in.flail = true;
            if (s == 3) {
                in.active = true;
                in.stickY = 1;
            }
            if (s == 4)
                in.handBetween[0] = true;
            const auto o = m.update(in, set);
            auto pp = p;
            pp.coupling *= o.couplingScale;
            const auto out = o.useStick ? sim.update(dt, o.run, in.stickX, in.stickY, {}, pp)
                                        : sim.updateTarget(dt, o.run, o.pose, in.stickX, in.stickY, {}, pp);
            if (i > 0)
                worst = std::max(worst, legsDelta(out.legs, last));
            worstAmount = std::max(worstAmount, std::abs(o.amount - lastAmount));
            last = out.legs;
            lastAmount = o.amount;
        }
        check(worst < .16f, "no frame moves a leg angle more than 0.16 rad (14 rad/s cap) across six layers");
        check(worstAmount <= dt / .2f + 1e-5f, "the shown share eases");
    }
    // FLAIL's B gate.
    {
        FlailGate g;
        auto o = g.update(true, true, true, true, true);
        check(o.started && o.swallowB && o.flail, "in the air, Y+X + B: B taken, flailing");
        o = g.update(true, true, false, true, true);
        check(o.swallowB && !o.flail, "X let go of: B still the legs' (no dodge or tuck), no flail");
        o = g.update(true, false, true, true, true);
        check(o.swallowB && !o.flail, "landed with B held: still not a dodge");
        o = g.update(true, true, true, true, false);
        check(o.ended && !o.swallowB, "B let go of: done");
        FlailGate h;
        check(!h.update(true, true, false, false, true).swallowB, "B alone: the tuck (passes)");
        check(!h.update(true, false, true, true, true).swallowB, "on the ground: B passes");
        check(!h.update(false, true, true, true, true).swallowB, "LEG CONTROL unavailable: B passes");
        FlailGate k;
        k.update(true, true, false, false, true);
        check(k.update(true, true, true, true, true).flail, "B held, then Y+X: taken over");
    }
    // The settings.
    {
        using namespace vr_settings;
        Values v;
        check(v.passiveLegsPct == 60 && v.passiveCouplingPct == 15 && v.legAirCrouch && v.legReachSpread,
              "defaults: passive 60%, coupling 15%, crouch and reach on");
        const Row* r = rowFor(Item::passiveLegs);
        check(r && std::strcmp(r->title, "PASSIVE LEGS") == 0 && std::strcmp(r->choices[0], "OFF") == 0, "PASSIVE LEGS row");
        check(choose(Item::passiveLegs, 0, v) && v.passiveLegsPct == 0, "OFF is 0");
        check(rowFor(Item::passiveCoupling) && rowFor(Item::legAirCrouch) && rowFor(Item::legReachSpread), "the rows");
        check(withTabOnly(Values{}, "passive_coupling_pct=25\nleg_air_crouch=0\n").passiveCouplingPct == 25 &&
                  !withTabOnly(Values{}, "leg_air_crouch=0\n").legAirCrouch,
              "keys read back");
        int found = 0;
        for (Item item : screens()[static_cast<size_t>(Category::legs)].items)
            found += item == Item::passiveLegs || item == Item::passiveCoupling || item == Item::legAirCrouch ||
                     item == Item::legReachSpread;
        check(found == 4, "on VR LEGS (ADVANCED)");
        check(inactiveReason(Item::passiveLegs, Values{}) == nullptr, "PASSIVE LEGS needs no LEG CONTROL");
    }
    std::printf("passive legs: %d checks passed\n", passed);
    return 0;
}
