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
// The balance as it was before the balance pass (Nov 14): the earlier tests' numbers mean this one. `fresh()` is the
// defaults now.
Params on() {
    Params p;
    p.enabled = true;
    p.margin = .06f;
    p.fallSlack = 0.f;
    p.fallSeconds = .12f;
    p.fallLeanDegrees = 35.f;
    p.gain = 3.f;
    p.damping = 0.f;
    p.fallGrace = .15f;
    p.pushSpeed = .6f;
    p.leap = false;
    // Before the arms-up pass (Nov 14): the touchdown counted a palm within 1.6 m with no wait and no head test.
    p.touchdownReach = 1.6f;
    p.armsBelowHead = -10.f;
    p.armsOverHead = 0.f;
    p.armsHoldSeconds = 0.f;
    return p;
}
Params fresh() {
    Params p;
    p.enabled = true;
    return p;
}
// The defaults without the TIP-IN (Nov 17), for the hold's own tests.
Params noTip() {
    Params p = fresh();
    p.tipIn = false;
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
        Params p = on();
        Handstand h;
        auto in = inverted();
        in.bodyUp = normalized(Vec3{0, -1, .5f});
        in.velocity = {0, 0, -1.5f};
        h.update(in, p);
        Handstand::Out o;
        Exit why = Exit::none;
        for (int i = 0; i < 90 && why == Exit::none; ++i) {
            o = h.update(in, p);
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
        // TRACKING LOST (the 2 AM log's 159.74 m): a glitch is held through the grace, then lets go.
        {
            const auto o = lost.update(lo, on());
            check(o.exitNow == Exit::none && o.glitch && o.supporting, "one lost frame: held");
        }
        Exit lostWhy = Exit::none;
        for (int i = 0; i < 40 && lostWhy == Exit::none; ++i)
            lostWhy = lost.update(lo, on()).exitNow;
        check(lostWhy == Exit::tracking, "tracking lost for the grace lets go");
        {
            // A palm read at the world's origin for a frame: the support keeps the last good palms.
            Handstand g;
            auto gi = inverted();
            g.update(gi, on());
            for (int i = 0; i < 5; ++i)
                g.update(gi, on());
            auto bad = gi;
            bad.palms[0] = {0, 0, 0};
            bad.palms[0].x -= 300.f;
            const auto o = g.update(bad, on());
            check(o.glitch && o.exitNow == Exit::none && length(o.lean) < 1.f,
                  "a palm 300 m off in one frame: a glitch, the centre of mass still over the palms");
            const auto back = g.update(gi, on());
            check(!back.glitch && back.supporting, "the palm back: on as before");
        }
        {
            // ABSURD ARMS (the Nov 19 log: palms at -33.20 m and -22.52 m, the arms NaN): no catch on them, and they
            // are reported; the same frame with sane palms catches.
            auto sane = inverted();
            Handstand ok;
            check(ok.update(sane, on()).caughtNow, "absurd arms: the sane frame catches");
            auto bad = inverted();
            bad.palms[0].y -= 33.2f;
            bad.palms[1].y -= 22.5f;
            bad.palmRel[0] = {NAN, NAN, NAN};
            bad.palmRelKnown[0] = true;
            bad.floorKnown = true;
            bad.floorY = 0.f;
            Handstand no;
            const auto o = no.update(bad, on());
            check(!o.caughtNow && (o.absurd & 3u) == 3u && (o.absurd & 4u), "palms 33 m off and a NaN arm: no catch, reported");
            auto head = inverted();
            head.head = head.palms[0] + Vec3{0, 30.f, 0};
            head.headKnown = true;
            Handstand far;
            check(!far.update(head, on()).caughtNow, "palms 30 m from the head: no catch");
        }
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
            late.update(l, t += landMinStintNs); // HANDOFF CHURN: past the takeover's least stint
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
            slow.update(s2, u += landMinStintNs); // HANDOFF CHURN: past the takeover's least stint
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
        {
            vr_settings::Values d;
            d.realisticFlips = true;
            check(std::abs(paramsOf(d).floorReach - .35f) < 1e-6f && std::abs(paramsOf(d).touchdownReach - 1.6f) < 1e-6f &&
                      std::abs(paramsOf(d).armsBelowHead - .2f) < 1e-6f,
                  "the defaults: a 0.35 m catch hold, a 1.6 m reach, the below-head test kept");
            d.handstandCatchHoldCm = 12;
            d.handstandReachCm = 100;
            check(std::abs(paramsOf(d).floorReach - .12f) < 1e-6f && std::abs(paramsOf(d).touchdownReach - 1.f) < 1e-6f,
                  "12 and 100: the old hold and reach");
        }
        v.realisticFlips = true;
        check(paramsOf(v).enabled && std::abs(paramsOf(v).comHeight - .95f) < 1e-6f && paramsOf(v).assist &&
                  std::abs(paramsOf(v).gain - 5) < 1e-6f && std::abs(paramsOf(v).pushGain - 2.2f) < 1e-6f &&
                  paramsOf(v).pushMaxNormal == 7 && paramsOf(v).fallLeanDegrees == 50 &&
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
        // THE CATCH STICKS (the owner, Nov 15): with HANDSTAND CATCH HOLD's 0.35 m, palms 0.3 m over the floor hold the
        // catch through the settle (0.12 m dropped it: 3 of 3 in the 9790d67 playtest), and 0.5 m still misses.
        {
            auto holdP = on();
            holdP.floorReach = .35f;
            Handstand held;
            fly(held);
            held.update(touchdown(), holdP);
            auto near30 = touchdown();
            near30.palms[0].y = near30.palms[1].y = .3f;
            Handstand::Out ho{};
            for (int i = 0; i < 90 && ho.exitNow == Exit::none; ++i)
                ho = held.update(near30, holdP);
            check(ho.exitNow == Exit::none && ho.state == State::balance,
                  "palms 0.3 m over the floor with a 0.35 m hold: the catch holds into the balance");
            Handstand loose;
            fly(loose);
            loose.update(touchdown(), holdP);
            Handstand::Out lo{};
            for (int i = 0; i < 200 && lo.exitNow == Exit::none; ++i)
                lo = loose.update(stay, holdP);
            check(lo.exitNow == Exit::missed, "palms 0.5 m up with a 0.35 m hold: still missed at the settle");
        }

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
        // Each palm's height over the floor, every frame (the catch log's palm heights).
        Handstand gaps;
        fly(gaps);
        auto ga = touchdown();
        ga.palms[0].y = ga.floorY + .3f;
        ga.palms[1].y = ga.floorY + .05f;
        const auto gr = gaps.update(ga, on());
        check(std::abs(gr.palmGaps[0] - .3f) < 1e-4f && std::abs(gr.palmGaps[1] - .05f) < 1e-4f &&
                  std::abs(gr.floorY - ga.floorY) < 1e-6f,
              "each palm's height over the floor, left and right, and the floor's y");
        ga.floorKnown = false;
        const auto gn = gaps.update(ga, on());
        check(std::isnan(gn.palmGaps[0]) && std::isnan(gn.palmGaps[1]) && std::isnan(gn.floorY),
              "no floor: no palm heights");
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
            check(std::abs(c.seedRaw - rawWant) < .01f && c.seedOffset <= .18f * .6f + 1e-4f,
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
    // THE BALANCE PASS (Nov 14, t67u: "a lot more balance. Make it hard to fall out of it, but easy to leap out of."): the
    // last session had 30 catches held a median 0.27 s, falling at a 6 degree lean with the centre of mass 0.11 m off.
    {
        const float deg = 3.14159265f / 180.f;
        const auto landing = [&](float invertedDeg) {
            auto in = inverted();
            in.bodyUp = {0, std::cos(invertedDeg * deg), std::sin(invertedDeg * deg)};
            return in;
        };
        // The defaults: support .18 m, a fall needs .5 s out of the support grown by .06 m, steering 5, damping 2.5 /s, no
        // fall test for .6 s, 50 degrees, leap on, a push at .45 m/s.
        const Params d = fresh();
        check(std::abs(d.palmRadius + d.margin - .18f) < 1e-6f && std::abs(d.fallSeconds - .5f) < 1e-6f &&
                  d.fallLeanDegrees == 50 && d.gain == 5.f && d.damping == 2.5f && d.fallGrace == .6f && d.leap &&
                  std::abs(d.fallSlack - .06f) < 1e-6f && std::abs(d.pushSpeed - .45f) < 1e-6f,
              "balance pass: the new defaults");
        // How long a catch with no assist (a lean nobody steers) lasts: the wider support, the slack, the longer timer and
        // the grace hold it far longer than before.
        const auto holds = [&](Params p) {
            p.assist = false;
            Handstand h;
            auto in = landing(172.f); // ~.13 m of offset, the last session's 0.11 m
            h.update(in, p);
            in.velocity = {};
            in.angular = {};
            for (int i = 0; i < 90 * 20; ++i)
                if (h.update(in, p).exitNow == Exit::fall)
                    return (i + 1) * dt;
            return 20.f;
        };
        const float before = holds(on()), now = holds(d);
        check(before < .6f && now > before + .8f, "balance pass: an unsteered lean lasts over 0.8 s longer than before");
        // With the assist (the default) the same catch holds for ever, also with a 0.3 m palm-centre jump every other
        // frame (the catch log's 0.17 to 0.22 m).
        {
            Handstand h;
            auto in = landing(172.f);
            h.update(in, d);
            in.velocity = {};
            in.angular = {};
            bool fell = false;
            for (int i = 0; i < 90 * 8 && !fell; ++i) {
                const float z = (i & 1) ? .15f : -.15f;
                in.palms[0].z = in.palms[1].z = z;
                fell = h.update(in, d).exitNow == Exit::fall;
            }
            check(!fell, "balance pass: 8 s with the palms re-placed .3 m every other frame: no fall");
        }
        // A catch from a landing speed of 1.5 m/s the old way: the damped, widened balance keeps it.
        {
            Handstand h;
            auto in = landing(160.f);
            in.velocity = {1.5f, -3.f, -1.5f};
            h.update(in, d);
            in.velocity = {};
            in.angular = {};
            bool fell = false;
            State last{};
            for (int i = 0; i < 90 * 5 && !fell; ++i) {
                const auto o = h.update(in, d);
                fell = o.exitNow == Exit::fall;
                last = o.state;
            }
            check(!fell && last == State::balance, "balance pass: a fast landing is kept for 5 s");
        }
        // Damping: the same sway dies faster.
        {
            Balance a, b;
            Support s;
            s.a = {-.2f, 0, 0};
            s.b = {.2f, 0, 0};
            s.radius = .18f;
            Params q = d, none = d;
            q.assist = none.assist = false;
            none.damping = 0.f;
            a.start({0, 0, .05f}, {0, 0, -.2f});
            b.start({0, 0, .05f}, {0, 0, -.2f});
            for (int i = 0; i < 45; ++i) {
                a.step(dt, s, {}, q);
                b.step(dt, s, {}, none);
            }
            check(length(a.v()) < length(b.v()), "balance pass: damping bleeds the sway's speed");
        }
        // LEAP OUT. A's press, in the first frame and after; a held A at the catch is not one; into a jump along the view.
        const auto leapInput = [&] {
            auto in = landing(165.f);
            in.viewForward = {0, 0, -1};
            return in;
        };
        {
            Handstand h;
            auto in = leapInput();
            h.update(in, d);
            check(h.state() == State::catching, "leap: caught first");
            in.jump = true;
            const auto o = h.update(in, d);
            check(o.exitNow == Exit::leap && o.state == State::leaving, "leap: A while catching leaps out at once");
            check(std::abs(o.launch.y - 5.5f) < 1e-4f && std::abs(o.launch.z + 4.f) < 1e-4f && std::abs(o.launch.x) < 1e-4f,
                  "leap: 5.5 m/s up and 4 m/s along the view (-z)");
            check(o.heldSeconds > 0.f && o.heldSeconds < .1f, "leap: the exit says how long it held");
        }
        {
            Handstand h;
            auto in = leapInput();
            in.viewForward = {1, 0, 0};
            for (int i = 0; i < 90; ++i)
                h.update(in, d);
            check(h.state() == State::balance, "leap: balancing after a second");
            in.jump = true;
            const auto o = h.update(in, d);
            check(o.exitNow == Exit::leap && std::abs(o.launch.x - 4.f) < 1e-4f, "leap: A in the balance, along where he looks");
            check(o.heldSeconds > .9f, "leap: it held a second");
        }
        {
            Handstand h; // A held at the catch (the controlled-flip toggle, a grip's A): no leap until it is pressed again
            auto in = leapInput();
            in.jump = true;
            h.update(in, d);
            bool left = false;
            for (int i = 0; i < 45; ++i)
                left |= h.update(in, d).exitNow != Exit::none;
            check(!left, "leap: A held through the catch is not a press");
            in.jump = false;
            h.update(in, d);
            in.jump = true;
            check(h.update(in, d).exitNow == Exit::leap, "leap: released and pressed again leaps");
        }
        {
            Handstand h; // the stick: a light one is the balance's aim, a hard one held .15 s leaps
            auto in = leapInput();
            h.update(in, d);
            in.stick = .6f;
            bool left = false;
            for (int i = 0; i < 90 && !left; ++i)
                left = h.update(in, d).exitNow != Exit::none;
            check(!left, "leap: a stick at .6 is the balance's aim");
            in.stick = 1.f;
            int at = -1;
            for (int i = 0; i < 30 && at < 0; ++i)
                if (h.update(in, d).exitNow == Exit::leap)
                    at = i;
            check(at >= 11 && at <= 14, "leap: a hard stick held .15 s leaps");
            Handstand g; // a flick shorter than that does not
            auto gi = leapInput();
            g.update(gi, d);
            for (int i = 0; i < 8; ++i) {
                gi.stick = 1.f;
                left |= g.update(gi, d).exitNow != Exit::none;
            }
            gi.stick = 0.f;
            g.update(gi, d);
            for (int i = 0; i < 8; ++i) {
                gi.stick = 1.f;
                left |= g.update(gi, d).exitNow != Exit::none;
            }
            check(!left, "leap: two short flicks are not a hold");
        }
        {
            Params off = d; // switched off: A does nothing
            off.leap = false;
            Handstand h;
            auto in = leapInput();
            h.update(in, off);
            in.jump = true;
            in.stick = 1.f;
            bool left = false;
            for (int i = 0; i < 45; ++i)
                left |= h.update(in, off).exitNow == Exit::leap;
            check(!left, "leap: off means no leap");
        }
        {
            // Straight down view (no level direction): straight up; a push at .5 m/s leaves now (it did not at .6).
            const Params q = d;
            check(length(leapLaunch({0, -1, 0}, q) - Vec3{0, 5.5f, 0}) < 1e-5f, "leap: no level view is straight up");
            check(length(leapLaunch({NAN, 0, 0}, q) - Vec3{0, 5.5f, 0}) < 1e-5f, "leap: a NaN view is straight up");
            Handstand h;
            auto in = leapInput();
            h.update(in, q);
            in.pushInto = 1.0f;
            Exit why = Exit::none;
            for (int i = 0; i < 10 && why == Exit::none; ++i)
                why = h.update(in, q).exitNow;
            check(why == Exit::push, "push: a push of 1 m/s into the floor exits (threshold .45)");
        }
    }
    // The launcher's values reach the balance, and a saved profile at the old defaults is moved once (revision 3).
    {
        vr_settings::Values v;
        v.realisticFlips = true;
        const Params p = paramsOf(v);
        check(std::abs(p.palmRadius + p.margin - .18f) < 1e-6f && std::abs(p.fallSeconds - .5f) < 1e-6f &&
                  std::abs(p.damping - 2.5f) < 1e-6f && p.leap && p.fallLeanDegrees == 50 && p.gain == 5.f,
              "balance pass: the defaults reach the balance");
        v.handstandSupportCm = 12;
        v.handstandGraceMs = 120;
        v.handstandDampingPct = 0;
        v.handstandLeap = false;
        const Params o = paramsOf(v);
        check(std::abs(o.palmRadius + o.margin - .12f) < 1e-6f && std::abs(o.fallSeconds - .12f) < 1e-6f && o.damping == 0.f &&
                  !o.leap,
              "balance pass: the old values are still choosable");
        vr_settings::Values saved;
        saved.handstandFallDegrees = 35;
        saved.handstandGainPct = 300;
        const std::string rev2 = "movement_defaults=2\n";
        const auto moved = vr_settings::withMovementDefaults(saved, rev2);
        check(moved.handstandFallDegrees == 50 && moved.handstandGainPct == 500, "balance pass: the old defaults become the new");
        vr_settings::Values tuned;
        tuned.handstandFallDegrees = 40;
        tuned.handstandGainPct = 300;
        const auto kept = vr_settings::withMovementDefaults(tuned, rev2);
        check(kept.handstandFallDegrees == 40 && kept.handstandGainPct == 500, "balance pass: a tuned angle stays");
        check(vr_settings::withMovementDefaults(saved, "movement_defaults=3\n").handstandFallDegrees == 35,
              "balance pass: a file that has seen revision 3 keeps its own");
    }
    // HANDSTAND GRACE / TIP-IN (the owner, Nov 17: "loosen up handstands and give some grace for getting into one; maybe
    // you could tip the player into the handstand when it's clear they're attempting one"). s131456's attempts: 141-165
    // degrees, palms 1.15-1.41 m over the floor, the hands at or below the head at the touchdown.
    {
        vr_settings::Values d;
        d.realisticFlips = true;
        d.handstandSticky = false; // the grace and tip-in's own numbers (STICKY HANDSTANDS: core_tests)
        const Params P = paramsOf(d);
        check(d.handstandReachCm == 160 && std::abs(P.touchdownReach - 1.6f) < 1e-6f &&
                  std::abs(P.armsGraceBefore - .3f) < 1e-6f && std::abs(P.touchdownGrace - .4f) < 1e-6f && P.tipIn &&
                  P.tipInDegrees == 120.f && std::abs(P.tipInReach - 2.f) < 1e-6f,
              "the defaults: 1.6 m reach, 300 ms before, 400 ms after, tip-in from 120 deg within 2 m");
        const auto attempt = [](float degrees, float palmsUp, float armsY, bool air) {
            auto in = inverted();
            const float r = degrees * 3.14159265f / 180.f;
            in.bodyUp = {std::sin(r), std::cos(r), 0};
            in.angular = {.4f, 0, 0};
            in.floorPalms = 0;
            in.floorKnown = true;
            in.floorY = 0;
            in.vrAir = air;
            in.touchdown = !air;
            in.palms[0] = {-.2f, air ? -.5f : palmsUp, 0};
            in.palms[1] = {.2f, air ? -.5f : palmsUp, 0};
            in.palmRelKnown[0] = in.palmRelKnown[1] = true;
            in.palmRel[0] = {-.2f, armsY, 0};
            in.palmRel[1] = {.2f, armsY, 0};
            return in;
        };
        // Hands over the head 0.2 s in the air, lowered to the head 0.1 s before the touchdown: the grace counts them.
        {
            Handstand h;
            for (int i = 0; i < 18; ++i)
                h.update(attempt(145, 0, .25f, true), P);
            for (int i = 0; i < 9; ++i)
                h.update(attempt(145, 0, -.15f, true), P);
            const auto o = h.update(attempt(145, 1.41f, -.15f, false), P);
            check(o.caughtNow && o.touchdownCatch && o.armsFromGrace, "hands up 0.1 s before: caught at 1.41 m");
        }
        // The same without the grace: hands not over the head; the window closes with the reason.
        {
            Params g = P;
            g.armsGraceBefore = 0.f;
            Handstand h;
            for (int i = 0; i < 18; ++i)
                h.update(attempt(145, 0, .25f, true), g);
            for (int i = 0; i < 9; ++i)
                h.update(attempt(145, 0, -.15f, true), g);
            auto o = h.update(attempt(145, 1.41f, -.15f, false), g);
            check(!o.caughtNow && o.touchdownNow && o.refused == Refusal::armsDown &&
                      (o.failed & refusalBit(Refusal::armsDown)) && !(o.failed & refusalBit(Refusal::palmsHigh)),
                  "no grace: refused, hands not over the head (the palms' height passes)");
            bool closed = false;
            unsigned why = 0;
            for (int i = 0; i < 60 && !closed; ++i) {
                o = h.update(attempt(145, 1.41f, -.15f, false), g);
                closed = o.windowClosed;
                why = o.closedFailed;
            }
            check(closed && (why & refusalBit(Refusal::armsDown)) && !o.caughtNow,
                  "the window closes with every failing gate");
        }
        // A hold made in the air counts at once below the tip-in's angle.
        {
            Handstand h;
            for (int i = 0; i < 18; ++i)
                h.update(attempt(110, 0, .25f, true), P);
            const auto o = h.update(attempt(110, 1.5f, .25f, false), P);
            check(o.caughtNow && !o.tipIn, "110 deg, hands held up in the air, palms 1.5 m: caught with no wait");
        }
        // Raised only at the touchdown, below the tip-in's angle: the 0.12 s hold, then the catch.
        {
            Handstand h;
            for (int i = 0; i < 18; ++i)
                h.update(attempt(110, 0, -.3f, true), P);
            auto o = h.update(attempt(110, 1.5f, .25f, false), P);
            check(!o.caughtNow && o.refused == Refusal::none, "hands up at the touchdown: the hold first");
            int frames = 1;
            for (; frames < 40 && !o.caughtNow; ++frames)
                o = h.update(attempt(110, 1.5f, .25f, false), P);
            check(o.caughtNow && frames * dt >= .12f, "then the catch");
        }
        // TIP-IN: clearly attempting (150 deg, hands over the head), palms 1.9 m up: tipped in at once.
        {
            Handstand h;
            h.update(attempt(150, 0, -.3f, true), P);
            const auto o = h.update(attempt(150, 1.9f, .25f, false), P);
            check(o.caughtNow && o.tipIn, "150 deg, hands up, palms 1.9 m: tipped in");
            Params off = P;
            off.tipIn = false;
            Handstand n;
            n.update(attempt(150, 0, -.3f, true), off);
            const auto no = n.update(attempt(150, 1.9f, .25f, false), off);
            check(!no.caughtNow && no.refused == Refusal::palmsHigh, "tip-in off: palms too far over the floor");
            Handstand fast;
            fast.update(attempt(150, 0, -.3f, true), P);
            auto spin = attempt(150, 1.9f, .25f, false);
            spin.angular = {7, 0, 0};
            check(!fast.update(spin, P).caughtNow, "a handspring is no tip-in");
        }
        // A held grip is no gate: the catch carries it.
        {
            Handstand h;
            h.update(attempt(150, 0, .25f, true), P);
            auto g = attempt(150, 1.3f, .25f, false);
            g.grip = true;
            const auto o = h.update(g, P);
            check(o.caughtNow && o.gripCarried && o.gripHeld && !(o.failed & refusalBit(Refusal::grip)),
                  "a grip held: caught, carried");
        }
        // GRACE AFTER: a touchdown 0.33 s after SpidyVR's air still catches (0.25 s refused it).
        {
            Handstand h;
            h.update(attempt(150, 0, .25f, true), P);
            auto idle = attempt(150, 0, .25f, false);
            idle.touchdown = false;
            for (int i = 0; i < 30; ++i)
                h.update(idle, P);
            check(h.update(attempt(150, 1.3f, .25f, false), P).caughtNow, "0.33 s after the air: caught");
            Params old = P;
            old.touchdownGrace = .25f;
            Handstand o;
            o.update(attempt(150, 0, .25f, true), old);
            for (int i = 0; i < 30; ++i)
                o.update(idle, old);
            check(!o.update(attempt(150, 1.3f, .25f, false), old).caughtNow, "the old 0.25 s: no catch");
        }
        // Revision 6: a profile still on the 130 cm reach moves to 160; a tuned one stays.
        {
            vr_settings::Values saved;
            saved.handstandReachCm = 130;
            check(vr_settings::withMovementDefaults(saved, "movement_defaults=5\n").handstandReachCm == 160,
                  "130 becomes 160");
            saved.handstandReachCm = 110;
            check(vr_settings::withMovementDefaults(saved, "movement_defaults=5\n").handstandReachCm == 110,
                  "a tuned reach stays");
            const auto r = vr_settings::withTabOnly({}, "handstand_tip_in=0\nhandstand_tip_in_deg=20\n"
                                                        "handstand_grace_before_ms=5000\nhandstand_grace_after_ms=10\n");
            check(!r.handstandTipIn && r.handstandTipInDeg == 100 && r.handstandGraceBeforeMs == 1000 &&
                      r.handstandGraceAfterMs == 100,
                  "the ini keys read, clamped");
        }
    }
    // ARMS UP (Nov 14 t71u; the b9b2196 session: palms 1.13 / 1.51 / 1.83 m over the floor at the touchdowns that caught, 15 of 28
    // catches "missed" at the 0.6 s settle): a touchdown catch needs both palms within 1.0 m, clearly below the head, for 0.12 s.
    {
        const auto air = [](Handstand& h) {
            auto a = inverted();
            a.floorPalms = 0;
            a.floorKnown = true;
            a.floorY = 0;
            a.palms[0] = {-.2f, -.5f, 0};
            a.palms[1] = {.2f, -.5f, 0};
            return h.update(a, noTip());
        };
        const auto landing = [](float palmY, float headY) {
            auto in = inverted();
            // As held: palms below the head in the world are hands over it (inverted), above it hands at the sides.
            in.palmRel[0] = {-.2f, palmY < headY - .1f ? .35f : -.4f, 0};
            in.palmRel[1] = {.2f, in.palmRel[0].y, 0};
            in.palmRelKnown[0] = in.palmRelKnown[1] = true;
            in.vrAir = false;
            in.touchdown = true;
            in.floorPalms = 0;
            in.floorKnown = true;
            in.floorY = 0;
            in.headKnown = true;
            in.head = {0, headY, 0};
            in.palms[0] = {-.2f, palmY, 0};
            in.palms[1] = {.2f, palmY, 0};
            return in;
        };
        // Arms at his sides (palms 1.5 m, above the head at 0.8): never a catch, however long.
        Handstand sides;
        air(sides);
        bool caught = false;
        for (int i = 0; i < 40; ++i)
            caught = caught || sides.update(landing(1.5f, .8f), noTip()).caughtNow;
        check(!caught, "palms at standing hand height over the floor: no catch (they were counted at 1.6 m)");
        // Palms within reach but above the head (hands toward the feet): no catch.
        Handstand above;
        air(above);
        caught = false;
        for (int i = 0; i < 40; ++i)
            caught = caught || above.update(landing(.7f, .5f), noTip()).caughtNow;
        check(!caught, "palms within reach but above the head: no catch");
        // Both palms toward the floor, below the head: caught, but only after the hold.
        Handstand up;
        air(up);
        const auto first = up.update(landing(.6f, 1.0f), noTip());
        check(!first.caughtNow && first.armsUp, "palms toward the floor: not caught on the first frame, the hold is running");
        int n = 1;
        bool got = false;
        for (; n < 40 && !got; ++n)
            got = up.update(landing(.6f, 1.0f), noTip()).caughtNow;
        check(got && n * dt >= .1f && n * dt < .2f, "arms up for 0.12 s: the catch");
        // The hold starts over when the palms leave the reach.
        Handstand flicker;
        air(flicker);
        for (int i = 0; i < 8; ++i)
            flicker.update(landing(.6f, 1.0f), noTip());
        flicker.update(landing(1.5f, 1.0f), noTip());
        check(!flicker.update(landing(.6f, 1.0f), noTip()).caughtNow, "the hold starts over after palms drop out of reach");
        // A palm on a real floor contact needs no wait.
        Handstand real;
        auto r = inverted();
        r.floorKnown = true;
        r.floorY = 0;
        check(real.update(r, noTip()).caughtNow, "palms on a confirmed floor contact in SpidyVR's air: the catch, no wait");
    }
    // A LEAP IS A FRESH PUSH: the stick held at the catch is not a leap until it is let go of and pushed again.
    {
        auto caughtInput = [] {
            auto in = inverted();
            in.stick = 1.f;
            return in;
        };
        Handstand h;
        Params p = fresh();
        p.armsHoldSeconds = 0.f;
        check(h.update(caughtInput(), p).caughtNow, "caught with the stick held hard");
        bool leapt = false;
        for (int i = 0; i < 60; ++i) // 0.67 s with the stick still held
            leapt = leapt || h.update(caughtInput(), p).exitNow == Exit::leap;
        check(!leapt, "a stick held at the catch is no leap (9 of 11 leaps came 0.01 s after the catch)");
        auto rel = inverted();
        rel.stick = 0.f;
        for (int i = 0; i < 5; ++i)
            h.update(rel, p);
        for (int i = 0; i < 20 && !leapt; ++i)
            leapt = h.update(caughtInput(), p).exitNow == Exit::leap;
        check(leapt, "let go and pushed again: the leap");
        // A pressed before the catch is not a press now.
        Handstand a;
        auto held = inverted();
        held.jump = true;
        a.update(held, p);
        bool leapA = false;
        for (int i = 0; i < 20; ++i)
            leapA = leapA || a.update(held, p).exitNow == Exit::leap;
        check(!leapA, "A held through the catch is not a leap");
        auto press = held;
        press.jump = false;
        a.update(press, p);
        press.jump = true;
        check(a.update(press, p).exitNow == Exit::leap, "a fresh A press leaps");
    }
    // ARMS DIAGNOSTIC: the Out fields - each palm from the head (tracking), below the head in the world, the flip's
    // axis, and the touchdown window (from its first frame, about 0.25 s, gone with a catch or past the grace).
    {
        Handstand h;
        auto a = inverted();
        a.floorPalms = 0;
        h.update(a, fresh());
        auto in = inverted();
        in.vrAir = false;
        in.touchdown = true;
        in.floorPalms = 0;
        in.floorKnown = true;
        in.floorY = 0;
        in.headKnown = true;
        in.head = {0, 1.0f, 0};
        in.palms[0] = {-.2f, 1.4f, 0};
        in.palms[1] = {.2f, 1.3f, 0};
        in.palmRel[0] = {-.2f, -.4f, -.1f};
        in.palmRel[1] = {.2f, -.3f, -.1f};
        in.palmRelKnown[0] = in.palmRelKnown[1] = true;
        in.angular = {0, 0, 4.f};
        const auto o = h.update(in, fresh());
        check(o.touchdownNow && o.windowAge == 0.f, "the window opens at the touchdown");
        check(std::abs(o.palmRel[0].y + .4f) < 1e-5f && std::abs(o.palmRel[1].z + .1f) < 1e-5f, "palms from the head, as held");
        check(std::abs(o.worldBelow[0] + .4f) < 1e-5f && std::abs(o.worldBelow[1] + .3f) < 1e-5f,
              "below the head in the world (negative: above)");
        check(std::abs(o.flipAxis.z - 1.f) < 1e-5f, "the flip's axis, unit");
        float age = 0;
        int frames = 0;
        for (; frames < 60; ++frames) {
            const auto w = h.update(in, fresh());
            if (w.windowAge < 0)
                break;
            age = w.windowAge;
        }
        check(age > .2f && age <= .26f, "the window lasts about the touchdown grace");
        Handstand u;
        auto un = inverted();
        un.palmRelKnown[0] = un.palmRelKnown[1] = false;
        const auto uo = u.update(un, fresh());
        check(std::isnan(uo.palmRel[0].y) && std::isnan(uo.worldBelow[0]), "unknown: NAN");
    }
    // ARMS OVER HEAD (the owner, Nov 16): the arms-up test against the body. A rig rotated about the head by theta (the
    // flip, about x) maps the held offsets into the world; hands held 0.35 m over the head catch at 100, 117 and 150
    // degrees, hands at the chest (0.3 m below, 0.25 forward) never do. The old world test (0) refused 100 and 117.
    {
        const auto attempt = [](float degrees, Vec3 rel, const Params& p) {
            const float t = degrees * 3.14159265f / 180.f;
            const auto rot = [t](Vec3 v) {
                return Vec3{v.x, v.y * std::cos(t) - v.z * std::sin(t), v.y * std::sin(t) + v.z * std::cos(t)};
            };
            const Vec3 head{0, 1.0f, 0}; // the rig's head over the floor (the session's 0.84 to 1.58 m)
            Handstand h;
            auto a = inverted();
            a.floorPalms = 0;
            a.bodyUp = rot({0, 1, 0});
            h.update(a, p);
            bool caught = false;
            for (int i = 0; i < 40 && !caught; ++i) {
                auto in = inverted();
                in.vrAir = false;
                in.touchdown = true;
                in.floorPalms = 0;
                in.floorKnown = true;
                in.floorY = 0;
                in.headKnown = true;
                in.head = head;
                in.bodyUp = rot({0, 1, 0});
                for (unsigned k = 0; k < 2; ++k) {
                    const Vec3 r{k ? .2f : -.2f, rel.y, rel.z};
                    in.palmRel[k] = r;
                    in.palmRelKnown[k] = true;
                    in.palms[k] = head + rot(r);
                }
                caught = h.update(in, p).caughtNow;
            }
            return caught;
        };
        Params p = fresh();
        check(p.armsOverHead == .10f, "default: 10 cm over the head");
        const Vec3 overhead{0, .35f, 0}, chest{0, -.3f, -.25f};
        for (const float d : {100.f, 117.f, 150.f}) {
            char what[96];
            std::snprintf(what, sizeof what, "inverted %.0f deg, hands over the head: caught", d);
            check(attempt(d, overhead, p), what);
            std::snprintf(what, sizeof what, "inverted %.0f deg, hands at the chest: no catch", d);
            check(!attempt(d, chest, p), what);
        }
        Params old = fresh();
        old.armsOverHead = 0.f;
        check(!attempt(100.f, overhead, old) && !attempt(117.f, overhead, old) && attempt(150.f, overhead, old),
              "0: the old world test (hands over the head refused at 100 and 117, caught at 150)");
        // (STICKY HANDSTANDS, on by default since beta2, takes 5 cm off it: core_tests' sticky test.)
        vr_settings::Values plain;
        plain.handstandSticky = false;
        check(handstand::paramsOf(plain).armsOverHead == .10f, "the setting's default reaches the params");
    }
    std::printf("handstand: %d checks passed\n", passed);
    return 0;
}
