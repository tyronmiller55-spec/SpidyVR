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
        sp.angular = {4, 0, 0};
        check(!spin.update(sp, on()).caughtNow, "a fast spin is a handspring (H2): no catch");
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
        check(!grip.update(gr, on()).caughtNow, "a grip held: no catch");
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
    std::printf("handstand: %d checks passed\n", passed);
    return 0;
}
