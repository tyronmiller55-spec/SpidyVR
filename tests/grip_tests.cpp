// GRIP ACROSS THE GAME (grip.hpp, G1; November 14): the wall brake's deceleration and stop, the fling's
// history, mean, formula, caps and clears, and the grip state machine. (The swing's brake and fling on
// a simulated wall, the ground and the air are in core_tests.) Portable: no Windows, no headset, no game.
#include "spidy/grip.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::grip;
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
bool same(Vec3 a, Vec3 b, float eps = 1e-4f) {
    return length(a - b) < eps;
}
// Seconds to stop from `speed` with `hands` hands braking at `press` metres.
float stopTime(float speed, int hands, float press, float dt = .005f) {
    const Config c;
    float t = 0;
    while (speed >= c.stopSpeed && t < 60) {
        speed = brakeSpeed(speed, hands * brakeDecel(press, c), dt);
        t += dt;
    }
    return t;
}
} // namespace

int main() {
    const Config c;
    // The brake: friction on gravity and the press, capped.
    check(near(brakeDecel(0), .9f * 9.81f), "no press: mu g");
    check(near(brakeDecel(.05f), std::min(14.f, .9f * (9.81f + 2.f))), "press adds k d");
    check(near(brakeDecel(.5f), 14.f) && near(brakeDecel(.15f), 14.f) && brakeDecel(.1f) < 14.f,
          "capped at aMax");
    check(near(brakeDecel(-1), brakeDecel(0)) && near(brakeDecel(NAN), brakeDecel(0)), "bad press is none");
    // One hand stops 10 m/s in about 1.2 s, two in about 0.6 s (the plan's feel).
    check(near(stopTime(10, 1, 0), 1.1f, .15f), "one hand stops 10 m/s in about 1.1 to 1.2 s");
    check(near(stopTime(10, 2, 0), .55f, .1f), "two hands stop 10 m/s in about 0.6 s");
    check(stopTime(10, 1, .1f) < stopTime(10, 1, 0), "a harder press stops sooner");
    check(near(brakeSpeed(3, 100, 1), 0) && near(brakeSpeed(0, 5, .1f), 0), "never below zero");
    check(near(brakeSpeed(3, 0, 1), 3) && near(brakeSpeed(3, 5, 0), 3) && near(brakeSpeed(3, NAN, .1f), 3),
          "no braking, no change");
    check(brakeBegins(.5f) && !brakeBegins(.49f) && brakeEnds(.49f) && !brakeEnds(.5f) && !brakeBegins(NAN),
          "the brake begins at 0.5 m/s and ends under it");
    check(grainHz(0) == 30.f && near(grainHz(12), 160.f) && near(grainHz(50), 160.f) && grainHz(6) > 90 &&
              grainHz(6) < 100,
          "grain 30 to 160 Hz by slide speed");
    check(near(grainAmplitude(0), .15f) && near(grainAmplitude(14), .9f) && grainAmplitude(7) > .5f &&
              grainAmplitude(7) < .6f,
          "grain amplitude by braking force");

    // The fling's formula.
    check(same(flingVelocity({-2, 0, 0}), {2.6f, 0, 0}), "the other way at 1.3 x the hand speed");
    check(near(length(flingVelocity({0, 0, -20})), 9.f), "capped at vMax");
    check(length(flingVelocity({.49f, 0, 0})) == 0 && length(flingVelocity({0, 0, 0})) == 0,
          "under 0.5 m/s: none");
    check(near(length(flingVelocity({.5f, 0, 0})), .65f), "0.5 m/s: 0.65");
    check(length(flingVelocity({NAN, 0, 0})) == 0, "NaN: none");
    {
        Config low;
        low.vMax = 4;
        check(near(length(flingVelocity({10, 0, 0}, low)), 4.f), "the cap is the setting's");
    }
    // No speed above the cap from any hand speed, and never more than gain x.
    for (float s = .5f; s < 40; s += .37f) {
        const float v = length(flingVelocity({s, 0, 0}));
        check(v <= 9.f + 1e-4f && v <= 1.3f * s + 1e-4f, "fling <= min(vMax, gain s)");
    }

    // The history's mean over the last 60 ms.
    {
        History h;
        check(length(h.mean()) == 0, "empty: none");
        h.add({1, 0, 0}, .011f);
        check(length(h.mean()) == 0, "one short frame is not a mean");
        for (int i = 0; i < 10; ++i)
            h.add({-3, 0, 0}, .011f);
        check(same(h.mean(.06f), {-3, 0, 0}), "the old sample is outside the window");
        h.clear();
        h.add({4, 0, 0}, .03f);
        h.add({2, 0, 0}, .03f);
        check(same(h.mean(.06f), {3, 0, 0}), "time-weighted mean");
        h.add({0, 0, 0}, .03f);
        check(same(h.mean(.06f), {1, 0, 0}), "the newest 60 ms only");
        for (int i = 0; i < 40; ++i)
            h.add({1, 1, 1}, .011f);
        check(h.size() == History::capacity, "bounded");
        h.add({NAN, 0, 0}, .011f);
        h.add({1, 1, 1}, NAN);
        check(h.size() == History::capacity, "bad samples are dropped");
    }

    // The grip: press, pull, let go.
    {
        Hand hand;
        Hand::Frame f;
        f.dt = .011f;
        f.valid = true;
        f.contact = true;
        f.squeeze = .8f;
        auto out = hand.update(f);
        check(out.pressed && hand.gripping() && hand.kind() == Hand::Kind::surface, "pressed on a surface");
        f.contact = false;
        f.velocity = {0, 0, 1.f};
        for (int i = 0; i < 8; ++i)
            hand.update(f);
        f.squeeze = .2f;
        out = hand.update(f);
        check(out.released && same(out.launch, {0, 0, -1.3f}) && near(out.handSpeed, 1.f),
              "let go: flung away");
        check(!hand.gripping(), "no longer gripping");
        // Slow pull: nothing.
        f.squeeze = .8f;
        f.contact = true;
        hand.update(f);
        f.contact = false;
        f.velocity = {.3f, 0, 0};
        for (int i = 0; i < 8; ++i)
            hand.update(f);
        f.squeeze = .1f;
        out = hand.update(f);
        check(out.released && length(out.launch) == 0, "a slow pull flings nothing");
        // A grip pressed with nothing there is no grip.
        f.squeeze = .9f;
        f.contact = false;
        out = hand.update(f);
        check(!out.pressed && !hand.gripping(), "a grip on nothing");
        f.squeeze = 0;
        out = hand.update(f);
        check(!out.released, "and nothing to let go of");
        // The hysteresis: pressed at .65, held to .35.
        f.contact = true;
        f.squeeze = .6f;
        check(!hand.update(f).pressed, "0.6 is not a press");
        f.squeeze = .7f;
        check(hand.update(f).pressed, "0.7 is");
        f.contact = false;
        f.squeeze = .4f;
        hand.update(f);
        check(hand.gripping(), "0.4 still holds");
        f.squeeze = .3f;
        check(hand.update(f).released, "0.3 lets go");
    }
    // The history clears on a tracking loss, a snap turn, a long frame and a re-grip.
    {
        const auto pull = [&](auto spoil) {
            Hand hand;
            Hand::Frame f;
            f.dt = .011f;
            f.valid = true;
            f.contact = true;
            f.squeeze = .9f;
            hand.update(f);
            f.contact = false;
            f.velocity = {0, 2, 0};
            for (int i = 0; i < 8; ++i)
                hand.update(f);
            spoil(hand, f);
            f.squeeze = .1f;
            f.velocity = {0, 2, 0};
            f.turned = false;
            f.valid = true;
            f.dt = .011f;
            return hand.update(f);
        };
        auto none = pull([](Hand&, Hand::Frame&) {});
        check(none.released && near(length(none.launch), 2.6f), "control: a pull flings");
        auto lost = pull([](Hand& h, Hand::Frame& f) {
            f.valid = false;
            h.update(f);
        });
        check(!lost.released && length(lost.launch) == 0, "tracking loss: no fling");
        auto snap = pull([](Hand& h, Hand::Frame& f) {
            f.turned = true;
            h.update(f);
            f.turned = false;
        });
        check(snap.released && length(snap.launch) == 0, "a snap turn clears the history");
        auto turnedAtRelease = pull([](Hand&, Hand::Frame&) {});
        check(turnedAtRelease.released, "control");
        auto longFrame = pull([](Hand& h, Hand::Frame& f) {
            f.dt = .2f;
            h.update(f);
            f.dt = .011f;
        });
        check(longFrame.released && length(longFrame.launch) == 0, "a long frame clears the history");
        auto glitch = pull([](Hand& h, Hand::Frame& f) {
            f.velocity = {0, 100, 0};
            h.update(f);
        });
        check(glitch.released && length(glitch.launch) == 0, "a tracking jump clears the history");
        // A re-grip starts again: pull, let go (flung), grip again, let go at once: nothing.
        Hand hand;
        Hand::Frame f;
        f.dt = .011f;
        f.valid = true;
        f.contact = true;
        f.squeeze = .9f;
        hand.update(f);
        f.contact = false;
        f.velocity = {0, 3, 0};
        for (int i = 0; i < 8; ++i)
            hand.update(f);
        f.squeeze = .1f;
        check(hand.update(f).released, "first let go");
        f.squeeze = .9f;
        f.contact = true;
        hand.update(f);
        f.squeeze = .1f;
        f.contact = false;
        const auto again = hand.update(f);
        check(again.released && length(again.launch) == 0, "re-grip: history cleared");
    }

    std::printf("grip tests: %d checks passed\n", passed);
    return 0;
}
