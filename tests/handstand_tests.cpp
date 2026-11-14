// HANDSTAND, phase H1 (handstand.hpp, November 12): the catch (inverted,
// both palms on a floor, little spin), the DCM balance on the palms, its
// exits (push, step-down, fall, grip, tracking, disabled, missed), the push's
// launch, the eyes' drop, RealisticFlip's support override and release,
// HandCollision's floor-only contacts, the owner's hold on the ground and the
// settings. Portable.
#include "spidy/hand_collision.hpp"
#include "spidy/handstand.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/realistic_flip.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::handstand;
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
Params on() {
    Params p;
    p.enabled = true;
    return p;
}
Handstand::Input inverted() {
    Handstand::Input in;
    in.seconds = dt;
    in.vrAir = true;
    in.tracked = true;
    in.floorPalms = 3;
    in.palms[0] = {-.2f, 0, 0};
    in.palms[1] = {.2f, 0, 0};
    in.bodyUp = {0, -1, 0};
    return in;
}
} // namespace

int main() {
    // Geometry helpers.
    {
        check(std::abs(invertedDegrees({0, 1, 0})) < 1e-3f &&
                  std::abs(invertedDegrees({0, -1, 0}) - 180) < 1e-3f &&
                  std::abs(invertedDegrees({1, 0, 0}) - 90) < 1e-3f && invertedDegrees({NAN, 0, 0}) == 0,
              "inverted degrees");
        check(eyeDropFor(90, .9f) == 0 && eyeDropFor(100, .9f) == 0 &&
                  std::abs(eyeDropFor(160, .9f) - .9f) < 1e-6f &&
                  std::abs(eyeDropFor(130, .9f) - .45f) < 1e-6f,
              "the eyes' drop: none to 100, all from 160");
        Support s;
        s.a = {-.2f, 0, 0};
        s.b = {.2f, 0, 0};
        s.radius = .08f;
        bool in = false;
        s.clamp({.1f, 5, .05f}, &in);
        check(in, "between the palms is inside");
        const Vec3 out = s.clamp({0, 0, .5f}, &in);
        check(!in && std::abs(out.z - .08f) < 1e-5f, "in front of the palms clamps to the radius");
    }
    // The catch: inverted >= 100 degrees, both palms on a floor, little spin, VR's air.
    {
        Handstand h;
        auto in = inverted();
        const auto o = h.update(in, on());
        check(o.caughtNow && o.state == State::catching && o.holdVr && o.supporting, "caught");
        Handstand off;
        check(!off.update(in, Params{}).caughtNow, "off by default");
        Handstand upright;
        auto u = inverted();
        u.bodyUp = normalized(Vec3{1, -.1f, 0}); // 96 degrees
        check(!upright.update(u, on()).caughtNow, "under 100 degrees: no catch");
        Handstand spin;
        auto sp = inverted();
        sp.angular = {6, 0, 0};
        check(!spin.update(sp, on()).caughtNow, "a fast spin is a handspring (H2): no catch");
        Handstand slower;
        auto sl = inverted();
        sl.angular = {4, 0, 0};
        check(slower.update(sl, on()).caughtNow, "4 rad/s catches (the limit is 5 since Nov 13)");
        Handstand one;
        auto op = inverted();
        op.floorPalms = 1;
        check(!one.update(op, on()).caughtNow, "one palm: no catch");
        Handstand anim;
        auto an = inverted();
        an.vrAir = false;
        check(!anim.update(an, on()).caughtNow, "not in SpidyVR's air: no catch");
        Handstand grip;
        auto gr = inverted();
        gr.grip = true;
        check(grip.update(gr, on()).caughtNow,
              "a grip held with both palms down: still a catch (the landing pass)");
        Handstand gripHigh;
        auto gh = inverted();
        gh.grip = true;
        gh.floorPalms = 1;
        check(!gripHigh.update(gh, on()).caughtNow, "a grip with one palm up: no catch");
    }
    // Balance: a small lean settles over the palms; held, no exit; the catch turns to balance.
    {
        Handstand h;
        auto in = inverted();
        in.bodyUp = normalized(Vec3{.05f, -1, 0}); // feet a little over to -x
        h.update(in, on());
        Handstand::Out o;
        for (int i = 0; i < 90 * 3; ++i)
            o = h.update(in, on());
        check(o.state == State::balance && o.exitNow == Exit::none && o.holdVr, "3 s on the hands");
        check(o.balance.leanDegrees < 2.f, "the lean settled");
        // The stick moves the aim a little; still standing.
        in.aim = {.08f, 0, 0};
        for (int i = 0; i < 90; ++i)
            o = h.update(in, on());
        check(o.state == State::balance && o.lean.x > .02f, "the stick leans him toward it");
    }
    // Assist off: the same small lean is not steered back and tips over.
    {
        Params p = on();
        p.assist = false;
        Handstand h;
        auto in = inverted();
        in.bodyUp = normalized(Vec3{.05f, -1, 0});
        h.update(in, p);
        Exit why = Exit::none;
        for (int i = 0; i < 90 * 5 && why == Exit::none; ++i)
            why = h.update(in, p).exitNow;
        check(why == Exit::fall, "no assist: a lean falls");
    }
    // Fall: caught with the CoM far out and moving out.
    {
        Handstand h;
        auto in = inverted();
        in.bodyUp = normalized(Vec3{0, -1, .5f});
        in.velocity = {0, 0, -1.5f};
        h.update(in, on());
        Handstand::Out o;
        Exit why = Exit::none;
        for (int i = 0; i < 90 && why == Exit::none; ++i) {
            o = h.update(in, on());
            why = o.exitNow;
        }
        check(why == Exit::fall && length(o.releaseSpin) > 0, "out of the support: a fall, with a spin");
        check(o.state == State::leaving && o.holdVr && !o.supporting, "leaving: VR holds a moment");
        for (int i = 0; i < 40; ++i)
            o = h.update(in, on());
        check(o.state == State::off && !o.holdVr, "then off");
        check(!o.caughtNow, "no new catch with the palms still down");
        in.floorPalms = 0;
        h.update(in, on());
        in.floorPalms = 3;
        in.velocity = {};
        in.bodyUp = {0, -1, 0};
        check(h.update(in, on()).caughtNow, "palms off and on again: a new catch");
    }
    // Push: the arms driven into the floor launch him up; step-down; grip; tracking; missed.
    {
        Handstand h;
        auto in = inverted();
        for (int i = 0; i < 30; ++i)
            h.update(in, on());
        in.pushInto = 1.5f;
        Handstand::Out o;
        Exit why = Exit::none;
        for (int i = 0; i < 20 && why == Exit::none; ++i) {
            o = h.update(in, on());
            why = o.exitNow;
        }
        check(why == Exit::push && o.launch.y > 2.f && o.launch.y <= 7.f, "a push: a launch up");
        check(std::abs(launch(10, {0, 1, 0}, {9, 0, 0}, on()).y - 7) < 1e-5f &&
                  std::abs(launch(10, {0, 1, 0}, {9, 0, 0}, on()).x - 4) < 1e-5f,
              "the launch's caps (7 up, 4 across)");
        Handstand slow;
        auto s = inverted();
        for (int i = 0; i < 30; ++i)
            slow.update(s, on());
        s.pushInto = .3f;
        Exit w = Exit::none;
        for (int i = 0; i < 30; ++i)
            w = std::max(w, slow.update(s, on()).exitNow);
        check(w == Exit::none, "a slow press is no push");
        Handstand step;
        auto st = inverted();
        for (int i = 0; i < 30; ++i)
            step.update(st, on());
        st.floorPalms = 0;
        check(step.update(st, on()).exitNow == Exit::stepDown, "palms lifted: step-down");
        Handstand grip;
        auto gr = inverted();
        grip.update(gr, on());
        gr.grip = true;
        check(grip.update(gr, on()).exitNow == Exit::grip, "a grip lets go");
        // A grip squeezed at the catch is carried: no exit until it is released once and squeezed again.
        Handstand carried;
        auto ca = inverted();
        ca.grip = true;
        const auto caught = carried.update(ca, on());
        check(caught.caughtNow && caught.gripCarried, "the catch takes the grip in");
        Exit seen = Exit::none;
        for (int i = 0; i < 20; ++i)
            seen = carried.update(ca, on()).exitNow != Exit::none ? Exit::grip : seen;
        check(seen == Exit::none, "a carried grip does not let go");
        ca.grip = false;
        carried.update(ca, on());
        ca.grip = true;
        check(carried.update(ca, on()).exitNow == Exit::grip, "released and squeezed again: it lets go");
        Handstand lost;
        auto lo = inverted();
        lost.update(lo, on());
        lo.tracked = false;
        check(lost.update(lo, on()).exitNow == Exit::tracking, "tracking lost lets go");
        Handstand miss;
        auto mi = inverted();
        miss.update(mi, on());
        mi.floorPalms = 0;
        Exit m = Exit::none;
        for (int i = 0; i < 20; ++i)
            if (const auto e = miss.update(mi, on()).exitNow; e != Exit::none)
                m = e;
        check(m == Exit::missed, "palms off before the catch held: missed");
        Handstand dis;
        auto di = inverted();
        dis.update(di, on());
        check(dis.update(di, Params{}).exitNow == Exit::disabled, "turned off: let go");
    }
    // The view's lean: straight down, turned toward the CoM.
    {
        const Quat flipped = Quat::around({1, 0, 0}, 3.14159265f * .9f);
        const Quat held = supportTilt(flipped, {}, 1);
        check(held.rotate({0, 1, 0}).y < -.999f, "no lean: straight down");
        const Vec3 feet = -supportTilt(flipped, {.1f, 0, 0}, 1).rotate({0, 1, 0});
        check(feet.x > .09f && feet.y > .99f * std::cos(std::atan(.1f)), "the feet over the CoM");
    }
    // RealisticFlip: the hands' tilt overrides; release goes on with the spin.
    {
        RealisticFlipSettings s;
        s.enabled = true;
        RealisticFlip flip;
        RealisticFlight f;
        f.valid = true;
        f.handSupport = true;
        f.supportTilt = Quat::around({1, 0, 0}, 3.f);
        f.attached = true;
        f.pull = {0, 1, 0};
        f.velocity = {0, 0, 20};
        const Quat q = flip.update(f, true, false, 0, dt, s);
        check(std::abs(q.w - f.supportTilt.w) < 1e-5f && length(flip.angularVelocity()) == 0,
              "supported: the hands' tilt, no web turn");
        flip.release({2, 0, 0});
        check(std::abs(flip.angularVelocity().x - 2) < 1e-5f, "released with the tip's spin");
        RealisticFlight free;
        free.valid = true;
        flip.update(free, true, false, 0, dt, s);
        check(flip.angularVelocity().x > 1.f, "the spin goes on in the air");
        RealisticFlight bad = f;
        bad.supportAngular = {NAN, 0, 0};
        flip.update(bad, true, false, 0, dt, s);
        check(length(flip.angularVelocity()) == 0, "a bad support spin is none");
    }
    // HandCollision: floor contacts only from floors.
    {
        HandCollision c;
        std::array<Pose, 2> grips{};
        check(c.floorContacts(grips) == 0, "nothing without rays");
        check(length(c.floorNormal(0)) == 0 && length(c.floorNormal(5)) == 0, "no floor normal");
    }
    // The owner: the hands hold VR on the ground; then the landing.
    {
        using namespace hybrid_owner;
        HybridOwner owner;
        Frame f;
        f.state.playing = f.state.fresh = true;
        f.state.grounded = false;
        f.cancelGrip = true; // into VR in the air
        int64_t now = 1'000'000'000;
        owner.update(f, now += 11'000'000);
        f.cancelGrip = false;
        check(owner.domain == Domain::vr, "VR in the air");
        f.handSupport = true;
        f.state.grounded = true;
        for (int i = 0; i < 30; ++i)
            owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::vr, "the hands hold him on the ground");
        f.handSupport = false;
        owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::anim && owner.lastEnterAnim == EnterAnim::land, "let go: the landing");
        // The landing's first frames: a catch judged after the hand-off takes him back to VR.
        {
            HybridOwner late;
            Frame l;
            l.state.playing = l.state.fresh = true;
            l.cancelGrip = true;
            int64_t t = 1'000'000'000;
            late.update(l, t += 11'000'000);
            l.cancelGrip = false;
            l.state.grounded = true;
            late.update(l, t += 11'000'000);
            check(late.domain == Domain::anim && late.lastEnterAnim == EnterAnim::land, "landed: ANIM");
            for (int i = 0; i < 4; ++i)
                late.update(l, t += 11'000'000);
            l.handSupport = true;
            late.update(l, t += 11'000'000);
            check(late.domain == Domain::vr && late.lastEnterVr == EnterVr::handstand &&
                      std::strcmp(enterVrName(EnterVr::handstand), "handstand") == 0,
                  "a catch 55 ms after the landing: back to VR (handstand)");
            for (int i = 0; i < 20; ++i)
                late.update(l, t += 11'000'000);
            check(late.domain == Domain::vr, "and held there");
            // Too late: the grace (0.25 s) has passed.
            HybridOwner slow;
            Frame s2;
            s2.state.playing = s2.state.fresh = true;
            s2.cancelGrip = true;
            int64_t u = 1'000'000'000;
            slow.update(s2, u += 11'000'000);
            s2.cancelGrip = false;
            s2.state.grounded = true;
            slow.update(s2, u += 11'000'000);
            for (int i = 0; i < 30; ++i)
                slow.update(s2, u += 11'000'000);
            s2.handSupport = true;
            slow.update(s2, u += 11'000'000);
            check(slow.domain == Domain::anim, "a catch 0.35 s after the landing: the animation keeps him");
        }
        HybridOwner anim;
        Frame g;
        g.state.playing = g.state.fresh = g.state.grounded = true;
        g.handSupport = true;
        anim.update(g, now += 11'000'000);
        check(anim.domain == Domain::anim, "handSupport never takes the body from ANIM");
    }
    // Settings: on by default (playtest, t86u); REALISTIC FLIPS needed; keys; clamps.
    {
        using namespace vr_settings;
        Values v;
        check(v.handstand && !v.realisticFlips && !paramsOf(v).enabled,
              "on by default, but nothing without REALISTIC FLIPS");
        v.handstand = false;
        v.realisticFlips = true;
        check(!paramsOf(v).enabled, "handstand=0 turns it off");
        v.handstand = true;
        v.realisticFlips = false;
        check(!paramsOf(v).enabled, "needs REALISTIC FLIPS");
        v.realisticFlips = true;
        check(paramsOf(v).enabled && std::abs(paramsOf(v).comHeight - .95f) < 1e-6f && paramsOf(v).assist &&
                  std::abs(paramsOf(v).gain - 3) < 1e-6f && std::abs(paramsOf(v).pushGain - 2.2f) < 1e-6f &&
                  paramsOf(v).pushMaxNormal == 7 && paramsOf(v).fallLeanDegrees == 35 &&
                  std::abs(paramsOf(v).eyeDrop - .9f) < 1e-6f,
              "on with REALISTIC FLIPS");
        const std::string text = tabOnlyText(Values{});
        check(text.find("handstand=1\n") != std::string::npos &&
                  text.find("handstand_eye_drop_cm=90\n") != std::string::npos,
              "spidy_vr.ini keys");
        const Values read = withTabOnly(Values{}, "handstand=1\nhandstand_height_cm=500\n");
        check(read.handstand && read.handstandHeightCm == 140, "read, clamped");
    }
    // THE TOUCHDOWN CATCH (Nov 13 playtest: 358 landings, no handstand line):
    // the game's upright capsule touches first and the frame it does vrAir is
    // false - the catch is taken there, the palms near the floor counted.
    {
        const auto touchdown = [] {
            auto in = inverted();
            in.vrAir = false;
            in.touchdown = true;
            in.floorPalms = 0;
            in.floorKnown = true;
            in.floorY = 0;
            in.palms[0] = {-.2f, .5f, 0};
            in.palms[1] = {.2f, .5f, 0};
            return in;
        };
        const auto fly = [](Handstand& h) {
            auto a = inverted();
            a.floorPalms = 0; // no ray-confirmed floor in the air
            a.floorKnown = true;
            a.floorY = 0;
            a.palms[0] = {-.2f, -.5f, 0}; // under the actor's feet: no floor in the air
            a.palms[1] = {.2f, -.5f, 0};
            return h.update(a, on());
        };
        Handstand h;
        check(!fly(h).caughtNow, "in the air, palms under the actor's feet are no floor");
        const auto o = h.update(touchdown(), on());
        check(o.caughtNow && o.touchdownCatch && o.touchdownNow && o.holdVr && o.refused == Refusal::none,
              "inverted at the touchdown: the catch, held for SpidyVR");
        // Palms within floorReach while catching: balance.
        auto down = touchdown();
        down.palms[0].y = down.palms[1].y = .08f;
        Handstand::Out b{};
        for (int i = 0; i < 30; ++i)
            b = h.update(down, on());
        check(b.state == State::balance, "the palms brought down onto the floor: balance");

        Handstand high;
        fly(high);
        high.update(touchdown(), on());
        auto stay = touchdown();
        Handstand::Out m{};
        int frames = 0;
        for (; frames < 200 && m.exitNow == Exit::none; ++frames)
            m = high.update(stay, on());
        check(m.exitNow == Exit::missed && frames * dt > .55f && frames * dt < .7f,
              "palms left high after a touchdown catch: missed after the settle (0.6 s)");

        Handstand late;
        fly(late);
        auto idle = touchdown();
        idle.touchdown = false;
        for (int i = 0; i < 30; ++i) // 0.33 s of neither air nor ground
            late.update(idle, on());
        const auto l = late.update(touchdown(), on());
        check(!l.caughtNow && !l.touchdownNow, "a touchdown long after SpidyVR's air: no catch");

        Handstand up;
        fly(up);
        auto u = touchdown();
        u.bodyUp = normalized(Vec3{1, .3f, 0});
        const auto ur = up.update(u, on());
        check(!ur.caughtNow && ur.touchdownNow && ur.refused == Refusal::upright && ur.inverted < 80,
              "upright at the touchdown: no catch, the reason and the angle reported");
        Handstand fast;
        fly(fast);
        auto f = touchdown();
        f.angular = {7, 0, 0};
        const auto fr = fast.update(f, on());
        check(!fr.caughtNow && fr.refused == Refusal::spin && fr.spin > 6.9f, "spinning at the touchdown: refused");
        Handstand far;
        fly(far);
        auto fa = touchdown();
        fa.palms[0].y = fa.palms[1].y = 1.8f;
        const auto ar = far.update(fa, on());
        check(!ar.caughtNow && ar.refused == Refusal::palmsHigh && ar.palmGap > 1.7f,
              "palms 1.8 m up at the touchdown: refused");
        // The 22:24 playtest's refusal, palms 1.54 m over the floor at 121 degrees: reachable now.
        Handstand reach;
        fly(reach);
        auto rc = touchdown();
        rc.palms[0].y = rc.palms[1].y = 1.54f;
        check(reach.update(rc, on()).caughtNow, "palms 1.54 m up (eyes' drop and an arm): a catch");
        // Every failing gate is listed, the first being the reason.
        Handstand many;
        fly(many);
        auto mu = touchdown();
        mu.bodyUp = normalized(Vec3{1, .3f, 0});
        mu.angular = {7, 0, 0};
        mu.palms[0].y = mu.palms[1].y = 1.8f;
        const auto mo = many.update(mu, on());
        check(mo.refused == Refusal::spin || mo.refused == Refusal::upright,
              "first failing gate is the reason");
        check((mo.failed & refusalBit(Refusal::upright)) && (mo.failed & refusalBit(Refusal::spin)) &&
                  (mo.failed & refusalBit(Refusal::palmsHigh)),
              "all three failed gates are reported");
        char list[200];
        refusalList(mo.failed, list, sizeof list);
        check(std::strstr(list, "not inverted enough") && std::strstr(list, "spinning too fast") &&
                  std::strstr(list, "palms too far"),
              "the refusal list names every failed gate");
        // Seeded damped: a landing at 9.3 m/s and 4 rad/s no longer falls inside 100 ms.
        Handstand damped;
        fly(damped);
        auto fast9 = touchdown();
        fast9.velocity = {9.3f, 0, 0};
        fast9.angular = {0, 0, 4};
        fast9.palms[0].y = fast9.palms[1].y = .08f;
        const auto dm = damped.update(fast9, on());
        check(dm.caughtNow, "a fast landing is caught");
        bool fell = false;
        for (int i = 0; i < 10; ++i) // 111 ms
            fell = fell || damped.update(fast9, on()).exitNow == Exit::fall;
        check(!fell, "the damped start does not fall inside 100 ms (it did at 9.3 m/s: the 63 ms fall)");
        check(length(damped.update(fast9, on()).balance.v) < 3.f,
              "the balance's speed is nowhere near the landing's");
        Handstand dis;
        fly(dis);
        const auto dr = dis.update(touchdown(), Params{});
        check(!dr.caughtNow && dr.refused == Refusal::disabled, "off: refused as disabled");
        // Once per touchdown: the next ground frame reports nothing new.
        const auto again = dis.update(touchdown(), Params{});
        check(!again.touchdownNow, "the touchdown reports once");
    }
    // THE CATCH'S SEED (G1 playtest): 8 of 8 catches fell within 160 ms because the centre of mass started 0.2 to
    // 0.65 m off the palms (the landing's 137 to 167 degrees of inversion), 2 to 5 times outside the 0.12 m support.
    {
        const float deg = 3.14159265f / 180.f;
        const auto landing = [&](float invertedDeg) {
            auto in = inverted();
            in.bodyUp = {0, std::cos(invertedDeg * deg), std::sin(invertedDeg * deg)};
            in.velocity = {0, -3.f, 2.5f};
            in.angular = {3.f, 0, 0};
            return in;
        };
        // seedInside: near is left alone, far is brought to share x radius with its direction, along the segment too.
        Support sup;
        sup.a = {-.2f, 0, 0};
        sup.b = {.2f, 0, 0};
        sup.radius = .12f;
        check(std::abs(seedInside(sup, {.1f, 0, .05f}, .6f).z - .05f) < 1e-6f, "seed: already near the palms: as it is");
        const Vec3 sd = seedInside(sup, {.1f, 0, .65f}, .6f);
        check(std::abs(sd.x - .1f) < 1e-5f && std::abs(sd.z - .072f) < 1e-5f, "seed: .65 m off becomes .072 m, along the segment kept");
        const Vec3 sd2 = seedInside(sup, {.5f, 0, 0}, .6f);
        check(std::abs(sd2.x - (.2f + .072f)) < 1e-5f && std::abs(sd2.z) < 1e-5f, "seed: past a palm: the radius from the palm");
        for (const float tilt : {137.f, 147.f, 155.f, 167.f}) {
            Handstand h;
            auto in = landing(tilt);
            const auto c = h.update(in, on());
            check(c.caughtNow && c.frame == 0, "seed: caught, frame 0");
            const float rawWant = .95f * std::sin(tilt * deg);
            check(std::abs(c.seedRaw - rawWant) < .01f && c.seedOffset <= .12f * .6f + 1e-4f,
                  "seed: the raw offset is the landing's, the seeded one inside 0.6 of the radius");
            in.velocity = {};
            in.angular = {};
            bool fell = false;
            State last = c.state;
            for (int i = 0; i < 45 && !fell; ++i) { // 0.5 s, no input
                const auto o = h.update(in, on());
                fell = o.exitNow == Exit::fall;
                last = o.state;
                check(o.frame == i + 1, "seed: the frame counts on");
            }
            check(!fell && last == State::balance, "a 137 to 167 degree catch survives 0.5 s with no input");
            // The old start (the offset as the landing left it) fell: the seed is what holds it.
            Handstand old;
            Params op = on();
            op.seedShare = 10.f;
            old.update(landing(tilt), op);
            bool oldFell = false;
            for (int i = 0; i < 45 && !oldFell; ++i)
                oldFell = old.update(in, op).exitNow == Exit::fall;
            if (tilt < 150.f)
                check(oldFell, "seed: without it (the old start) a 137 or 147 degree catch falls");
        }
        // The palms re-placed 0.5 m in the frame after the catch (the handoff's origin, the support tilt) is not a lean.
        {
            Handstand h;
            auto in = landing(150.f);
            h.update(in, on());
            in.velocity = {};
            in.angular = {};
            in.palms[0].z = in.palms[1].z = .5f;
            const auto o1 = h.update(in, on());
            check(o1.reseeded && std::abs(o1.shift - .5f) < 1e-4f && std::abs(o1.centreJump - .5f) < 1e-4f,
                  "re-seed: the first frame after the catch took the palms' 0.5 m");
            bool fell = false;
            for (int i = 0; i < 45 && !fell; ++i)
                fell = h.update(in, on()).exitNow == Exit::fall;
            check(!fell, "a catch followed by a 0.5 m palm-centre jump does not fall");
            // Without the re-seed (a jump later, within the grace, past the jump threshold) it falls after the grace...
            Handstand n;
            Params np = on();
            np.reposeJump = 10.f;
            auto a = landing(150.f);
            n.update(a, np);
            a.velocity = {};
            a.angular = {};
            for (int i = 0; i < 4; ++i)
                n.update(a, np);
            a.palms[0].z = a.palms[1].z = .6f;
            bool early = false, later = false;
            for (int i = 0; i < 45; ++i) {
                const bool fe = n.update(a, np).exitNow == Exit::fall;
                (i < 8 ? early : later) |= fe;
            }
            check(!early && later, "no re-seed: the jump is a fall, but not inside the 0.15 s grace");
            // ...and with it, a jump while catching is carried.
            Handstand r;
            auto b = landing(150.f);
            r.update(b, on());
            b.velocity = {};
            b.angular = {};
            for (int i = 0; i < 4; ++i)
                r.update(b, on());
            b.palms[0].z = b.palms[1].z = .6f;
            const auto jr = r.update(b, on());
            check(jr.reseeded && std::abs(jr.shift - .6f) < 1e-4f, "re-seed: a jump while catching is carried");
            bool rf = false;
            for (int i = 0; i < 45 && !rf; ++i)
                rf = r.update(b, on()).exitNow == Exit::fall;
            check(!rf, "re-seed: no fall after a palm jump in the grace");
        }
        // The catching state ignores the fall test for 0.15 s: a start far outside does not fall inside it.
        {
            Handstand h;
            Params q = on();
            q.seedShare = 10.f; // the old, far start
            auto in = landing(137.f);
            h.update(in, q);
            in.velocity = {};
            in.angular = {};
            bool early = false;
            int fellAt = -1;
            for (int i = 0; i < 45; ++i) {
                const auto o = h.update(in, q);
                if (o.exitNow == Exit::fall) {
                    fellAt = i;
                    break;
                }
                early |= i < 12 && o.exitNow != Exit::none;
            }
            check(!early && fellAt >= 12, "catching: no fall test inside catchGrace (13 frames), even from the old start");
        }
        // Gone: no frame.
        Handstand g;
        check(g.update(landing(90.f), on()).frame == -1, "no catch, no frame");
    }
    std::printf("handstand: %d checks passed\n", passed);
    return 0;
}
