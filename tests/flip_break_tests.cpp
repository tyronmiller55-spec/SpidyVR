// FLIP BREAK and the November 11 23:39 / 23:41 ownership rules
// (hybrid_owner.hpp): a flip's pitch or tilt, or the grip cancel, breaks a
// midair animation on the frame it is read; the right stick's left / right
// never does; a jump stays the animation's to the landing; landing always
// returns to the animation. Portable.
#include "spidy/hybrid_owner.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

Frame air() {
    Frame f{};
    f.state.playing = f.state.fresh = true;
    return f;
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
    sameFrame();
    gripCancel();
    jumpStays();
    std::printf("%d flip break checks passed\n", passed);
    return 0;
}
