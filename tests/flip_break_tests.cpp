// FLIP BREAK and the November 11 23:39 / 23:41 ownership rules
// (hybrid_owner.hpp): a flip's pitch or tilt, or the grip cancel, breaks a
// midair animation on the frame it is read; the right stick's left / right
// never does; a jump stays the animation's to the landing; landing always
// returns to the animation. Portable.
#include "spidy/hybrid_owner.hpp"
#include "spidy/vr_settings.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::hybrid_owner;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr int64_t frame = 11'000'000;

void inputs() {
    FlipInput basic;
    basic.rightValid = basic.leftValid = true;
    basic.basicFlips = true;
    for (float x : {-1.f, -.6f, .6f, 1.f}) {
        basic.rightX = x;
        basic.rightY = 0;
        check(flipBreak(basic) == FlipBreak::none, "right stick left/right (a turn) keeps the animation");
    }
    basic.rightX = 0;
    basic.rightY = .15f;
    check(flipBreak(basic) == FlipBreak::none, "noise under the dead zone keeps it");
    basic.rightY = -.3f;
    check(flipBreak(basic) == FlipBreak::rightPitch, "right stick down (pitch) breaks with FLIPS");
    basic.rightX = .9f;
    basic.rightY = .25f;
    check(flipBreak(basic) == FlipBreak::rightPitch, "a diagonal with pitch past the dead zone breaks");
    basic.rightY = .1f;
    check(flipBreak(basic) == FlipBreak::none, "a turn with a little pitch noise keeps it");
    basic.rightY = 1;
    basic.zipPriority = true;
    check(flipBreak(basic) == FlipBreak::none, "FLIPS' zip flick is a zip, not a break");
    FlipInput legs;
    legs.rightValid = true;
    legs.legs = true;
    legs.rightY = .5f;
    check(flipBreak(legs) == FlipBreak::rightPitch, "with LEG CONTROL right stick up/down breaks");
    FlipInput plain;
    plain.rightValid = plain.leftValid = true;
    plain.rightY = 1;
    plain.leftX = plain.leftY = 1;
    check(flipBreak(plain) == FlipBreak::none,
          "with no flips on the stick (zip/heal, air steering) nothing breaks");
    FlipInput controlled = plain;
    controlled.rightY = 0;
    controlled.controlledActive = true;
    controlled.leftX = .4f;
    controlled.leftY = 0;
    check(flipBreak(controlled) == FlipBreak::leftStick, "CONTROLLED FLIPS on: left stick tilt breaks");
    controlled.controlledActive = false;
    check(flipBreak(controlled) == FlipBreak::none, "CONTROLLED FLIPS not toggled on: the left stick steers the jump");
    FlipInput realistic = controlled;
    realistic.realisticSteering = true;
    realistic.leftX = 0;
    realistic.leftY = -.5f;
    check(flipBreak(realistic) == FlipBreak::leftStick, "REALISTIC FLIPS steering: left stick pitch breaks");
}

// FRESH PUSH: a left stick held through takeoff is the run's, not a flip's.
void freshPush() {
    // Held at takeoff: nothing until it rests or moves clearly.
    FreshPush held;
    held.ground(true, 0.f, .9f);
    check(!held.allows(true, 0.f, .9f), "held through takeoff: no break (the run's)");
    check(!held.allows(true, .1f, .95f), "a wobble of the held stick still no break");
    check(!held.allows(true, .6f, .7f), "40 deg from the takeoff direction: still the run's");
    check(!held.allows(false, 0.f, 0.f) && !held.armed(), "no sample: nothing changes");
    check(held.allows(true, .9f, .2f), "moved clearly (> 60 deg): a flip's");
    check(held.allows(true, .9f, .2f) && held.allows(true, 0.f, .9f),
          "once armed it stays armed in this air");
    // Let go, then push: fresh.
    FreshPush rest;
    rest.ground(true, .8f, 0.f);
    check(!rest.allows(true, .8f, 0.f), "still held");
    check(!rest.allows(true, .15f, .1f) || rest.armed(), "rest sample arms it");
    check(rest.armed() && rest.allows(true, .0f, .7f), "after rest, the next push breaks");
    // Not held at takeoff: breaks at once (as before).
    FreshPush clear;
    clear.ground(true, .05f, .1f);
    check(clear.armed() && clear.allows(true, 0.f, -.8f), "a stick at rest at takeoff breaks on a push");
    FreshPush noSample;
    noSample.ground(false, 0.f, 0.f);
    check(noSample.armed() && !noSample.allows(false, 0.f, 0.f), "no hand: never a break");
    // The next jump starts fresh again.
    held.ground(true, 0.f, .9f);
    check(!held.armed() && !held.allows(true, 0.f, .9f), "a new takeoff re-reads the stick");
    // With flipBreak: the stick held through takeoff under REALISTIC steering.
    FlipInput in;
    in.realisticSteering = true;
    FreshPush gate;
    gate.ground(true, 0.f, .9f);
    in.leftX = 0.f;
    in.leftY = .9f;
    in.leftValid = gate.allows(true, in.leftX, in.leftY);
    check(flipBreak(in) == FlipBreak::none, "held run stick: no left-stick break");
    in.leftY = 0.f;
    in.leftValid = gate.allows(true, in.leftX, in.leftY);
    check(flipBreak(in) == FlipBreak::none, "at rest: nothing");
    in.leftY = -.6f;
    in.leftValid = gate.allows(true, in.leftX, in.leftY);
    check(flipBreak(in) == FlipBreak::leftStick, "a fresh push after rest breaks");
}

Frame air() {
    Frame f{};
    f.state.playing = f.state.fresh = true;
    return f;
}

// KEEP PUNCHES IN ANIMATION (PUNCH LUNGE off): no strike reaches the owner, while the webs, the
// grip cancel, a point jump and Spidy's wall still take the body as before.
void punchesStayAnimated() {
    check(!strikeTakesOver(true, false) && strikeTakesOver(true, true) && !strikeTakesOver(false, true) &&
              !strikeTakesOver(false, false),
          "a strike takes over only with FULL ARMS and PUNCH LUNGE both on");
    // The hold never starts with the setting off, strike after strike.
    StrikeHold hold;
    int64_t t = 1'000'000'000;
    bool held = false;
    for (int i = 0; i < 67; ++i) {
        t += frame;
        held |= hold.update(true, true, i, strikeTakesOver(true, false), t).hold;
        t += frame;
        held |= hold.update(true, false, i, strikeTakesOver(true, false), t).hold;
    }
    check(!held, "67 punches' strikes: none holds SpidyVR with PUNCH LUNGE off");
    StrikeHold old;
    check(old.update(true, true, 1, strikeTakesOver(true, true), t += frame).hold,
          "PUNCH LUNGE on: the old FULL ARMS hold");
    // The owner stays with the animation through a punch on the ground and in the air.
    HybridOwner o;
    Frame f = air();
    f.state.grounded = true;
    int64_t now = 2'000'000'000;
    o.update(f, now);
    for (int i = 0; i < 30; ++i) {
        f.strike = StrikeHold{}.update(true, true, i, strikeTakesOver(true, false), now).hold; // always false
        o.update(f, now += frame);
        check(o.domain == Domain::anim && !o.enterVr, "a ground punch stays in the animation");
    }
    // The other takeovers are untouched.
    HybridOwner grip;
    Frame g = air();
    grip.update(g, now += frame);
    g.state.grounded = false;
    for (int i = 0; i < 10; ++i)
        grip.update(g, now += frame);
    g.cancelGrip = true;
    check(grip.update(g, now += frame) == Domain::vr && grip.lastEnterVr == EnterVr::cancel,
          "the grip cancel still takes the air");
    HybridOwner web;
    Frame w = air();
    web.update(w, now += frame);
    w.state.grounded = false;
    for (int i = 0; i < 10; ++i)
        web.update(w, now += frame);
    w.webRising = true;
    check(web.update(w, now += frame) == Domain::vr && web.lastEnterVr == EnterVr::web,
          "a SpidyVR web still takes the air");
    HybridOwner point;
    Frame p = air();
    p.state.grounded = true;
    point.update(p, now += frame);
    p.pointJump = true;
    check(point.update(p, now += frame) == Domain::vr && point.lastEnterVr == EnterVr::pointJump,
          "a point jump still takes the body");
    HybridOwner wall;
    Frame k = air();
    wall.update(k, now += frame);
    k.state.grounded = false;
    for (int i = 0; i < 10; ++i)
        wall.update(k, now += frame);
    k.state.wall = surfaceWall;
    check(wall.update(k, now += frame) == Domain::vr && wall.lastEnterVr == EnterVr::wallRun,
          "Spidy's wall still takes the body");
    // The setting: off by default, kept as punch_lunge, launcher-only.
    vr_settings::Values v;
    check(!v.punchLunge && v.animatedFullArms, "PUNCH LUNGE off by default, FULL ARMS as before");
    check(vr_settings::tabOnlyText(v).find("punch_lunge=0\n") != std::string::npos &&
              vr_settings::withTabOnly({}, "punch_lunge=1\n").punchLunge &&
              !vr_settings::withTabOnly({}, "").punchLunge,
          "punch_lunge saved and read");
    const auto* row = vr_settings::rowFor(vr_settings::Item::punchLunge);
    check(row && std::string(row->title) == "PUNCH LUNGE" && row->choices.empty() &&
              row->place == vr_settings::Place::launcher,
          "a launcher toggle");
}

void sameFrame() {
    HybridOwner o;
    Frame f = air();
    f.state.grounded = true;
    int64_t t = 0;
    o.update(f, t);
    // Ground: a flip input switches nothing (the jump breaks once airborne).
    check(!o.breakForFlip(f.state, false, t += frame) && o.domain == Domain::anim, "a ground break");
    f.jumpAsk = true;
    o.update(f, t += frame);
    f.jumpAsk = false;
    f.state.grounded = false;
    f.velY = 6;
    o.update(f, t += frame);
    check(o.domain == Domain::anim, "the jump is the animation's");
    // The first airborne frames with pitch: VR on that frame, before the
    // owner's update; the update keeps the switch as this frame's.
    t += frame;
    check(o.breakForFlip(f.state, false, t), "no break in the jump");
    check(o.domain == Domain::vr && !o.animOwns(true, f.state), "the frame's owner is not SpidyVR at once (M1)");
    check(o.update(f, t) == Domain::vr && o.enterVr && o.lastEnterVr == EnterVr::flip, "the switch not reported");
    check(o.update(f, t += frame) == Domain::vr && !o.enterVr, "reported twice");
    check(!o.breakForFlip(f.state, false, t), "a second break from VR");
    // Landing on the feet always returns, whatever the flip left.
    f.state.grounded = true;
    check(o.update(f, t += frame) == Domain::anim && o.enterAnim && o.lastEnterAnim == EnterAnim::land,
          "landing did not snap back to the animation");
    // Not in a cutscene, not on Spidy's wall.
    HybridOwner c;
    Frame g = air();
    c.update(g, 0);
    check(!c.breakForFlip(g.state, true, frame), "a cutscene broke");
    g.state.wall = surfaceWall;
    check(!c.breakForFlip(g.state, false, frame), "the wall broke");
}

void gripCancel() {
    HybridOwner o;
    Frame f = air();
    f.state.grounded = true;
    int64_t t = 0;
    o.update(f, t);
    f.state.grounded = false;
    f.velY = 4;
    o.update(f, t += frame);
    // The cancel the gate consumed (no shot): this frame, as a flip.
    f.cancelGrip = f.cancelGripHeld = true;
    t += frame;
    check(o.breakForCancel(f.state, false, t) && o.domain == Domain::vr, "the cancel not this frame's");
    check(o.update(f, t) == Domain::vr && o.enterVr && o.lastEnterVr == EnterVr::cancel, "cancel not reported");
    f.cancelGrip = false;
    check(o.update(f, t += frame) == Domain::vr, "the cancel did not hold the air");
    f.state.grounded = true;
    check(o.update(f, t += frame) == Domain::anim && o.lastEnterAnim == EnterAnim::land, "landing kept VR");
}

void jumpStays() {
    HybridOwner o;
    Frame f = air();
    f.state.grounded = true;
    int64_t t = 0;
    o.update(f, t);
    f.state.grounded = false;
    f.rStickX = 1; // turning: kept
    for (int i = 0; i < 600; ++i) {
        f.velY = 8 - .1f * static_cast<float>(i);
        check(o.update(f, t += frame) == Domain::anim && !o.enterVr, "a jump left the animation unbroken");
    }
    f.state.grounded = true;
    check(o.update(f, t += frame) == Domain::anim && !o.enterAnim, "the landing was a switch");
}
} // namespace

int main() {
    inputs();
    freshPush();
    punchesStayAnimated();
    sameFrame();
    gripCancel();
    jumpStays();
    std::printf("%d flip break checks passed\n", passed);
    return 0;
}
