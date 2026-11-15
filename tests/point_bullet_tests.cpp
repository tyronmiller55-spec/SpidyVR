// POINT JUMP BULLET (point_bullet.hpp): the body takes the raw eye angle, eased, and lets go.
#include "spidy/point_bullet.hpp"

#include <cstdlib>
#include <iostream>

using namespace spidy;
using namespace spidy::point_bullet;
namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL " << what << "\n";
        ++failures;
    }
}
Vec3 up(Quat q) { return q.rotate({0, 1, 0}); }
Vec3 look(float pitchDeg, float yawDeg = 0) {
    const float p = pitchDeg * 3.14159265f / 180, y = yawDeg * 3.14159265f / 180;
    return {-std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)};
}
Quat settle(Bullet& b, const Params& p, float seconds, Input in = {}) {
    Quat q{};
    in.seconds = 1.f / 90;
    in.speed = 20;
    for (int i = 0; i < static_cast<int>(seconds * 90); ++i)
        q = b.update(in, p);
    return q;
}
float angleBetween(Vec3 a, Vec3 b) {
    return std::acos(std::clamp(dot(normalized(a), normalized(b)), -1.f, 1.f)) * 57.2957795f;
}
} // namespace

int main() {
    Params p;
    {
        // Any eye angle: up along the look at full weight, down included.
        for (float pitch : {70.f, 40.f, 0.f, -30.f, -89.f}) {
            Bullet b;
            b.launch(look(pitch, 30), {-.5f, 0, -.87f}, p);
            const Quat q = settle(b, p, .3f);
            check(angleBetween(up(q), look(pitch, 30)) < .5f, "the body's axis did not take the look");
        }
        Bullet down;
        down.launch({0, -1, 0}, {0, 0, -1}, p);
        const Quat q = settle(down, p, .3f);
        check(up(q).y < -.999f, "straight down: the body is not head down");
        check(std::abs(down.degrees() - 180.f) < .5f, "straight down is a half turn");
        // The dive goes over about the heading's right (a front flip's axis): no sideways drift, his forward ends behind.
        check(std::abs(q.rotate({0, 0, -1}).x) < .01f && q.rotate({0, 0, -1}).z > .99f,
              "straight down: not turned over about the heading's right");
    }
    {
        // Eased over about 120 ms: slow at the start, there at 0.12 s, never a jump.
        Bullet b;
        b.launch(look(0), {0, 0, -1}, p);
        Input in;
        in.seconds = 1.f / 90;
        in.speed = 20;
        Quat prev{};
        float worst = 0, first = 0;
        for (int i = 0; i < 20; ++i) {
            const Quat q = b.update(in, p);
            const float step = angleBetween(up(q), up(prev));
            if (i == 0)
                first = step;
            worst = std::max(worst, step);
            prev = q;
        }
        check(first < 1.5f, "the turn starts with a jump");
        check(worst < 18.f, "a step of more than 18 degrees a frame");
        check(angleBetween(up(prev), look(0)) < 1.f, "not onto the look by about 0.2 s");
        Bullet half;
        half.launch(look(0), {0, 0, -1}, p);
        const Quat q = settle(half, p, .06f);
        const float a = angleBetween(up(q), {0, 1, 0});
        check(a > 30.f && a < 60.f, "halfway through the ease it is not about halfway");
    }
    {
        // It lets go: touchdown (fast), slow speed, a web, A, hands holding him, a timeout.
        auto lets = [&](Input in, float fade) {
            Bullet b;
            b.launch(look(30), {0, 0, -1}, p);
            settle(b, p, .5f);
            in.seconds = 1.f / 90;
            const int steps = static_cast<int>(fade * 90) + 4;
            Quat q{};
            for (int i = 0; i < steps; ++i)
                q = b.update(in, p);
            return !b.live() && angleBetween(up(q), {0, 1, 0}) < .01f;
        };
        Input fast;
        fast.speed = 20;
        Input landed = fast;
        landed.grounded = true;
        check(lets(landed, p.easeLanded), "touchdown does not bring him upright in 0.12 s");
        Input webbed = fast;
        webbed.webbed = true;
        check(lets(webbed, p.easeOut), "a web does not let go");
        Input a = fast;
        a.aPressed = true;
        check(lets(a, p.easeOut), "A does not let go");
        Input hands = fast;
        hands.handsHold = true;
        check(lets(hands, p.easeOut), "hands holding him do not let go");
        Input slow;
        slow.speed = 2;
        check(lets(slow, p.easeOut), "a slow body keeps the bullet");
        Bullet t;
        t.launch(look(30), {0, 0, -1}, p);
        settle(t, p, p.maxHold + 1.f);
        check(!t.live(), "never lets go on its own");
        // It does not let go on the ground in the first 0.1 s (the launch frame is still grounded).
        Bullet g;
        g.launch(look(30), {0, 0, -1}, p);
        Input launchFrame;
        launchFrame.seconds = 1.f / 90;
        launchFrame.grounded = true;
        launchFrame.speed = 15;
        for (int i = 0; i < 4; ++i)
            g.update(launchFrame, p);
        check(g.live() && g.letGoes() == 0, "the launch's own grounded frames let it go");
    }
    {
        // Off, or no look: nothing. A new launch restarts.
        Bullet b;
        Params off = p;
        off.enabled = false;
        b.launch(look(30), {0, 0, -1}, off);
        check(!b.live(), "off still launched");
        b.launch({0, 0, 0}, {0, 0, -1}, p);
        check(!b.live(), "no look still launched");
        b.launch({NAN, 0, 0}, {0, 0, -1}, p);
        check(!b.live(), "a bad look still launched");
        Input in;
        in.seconds = 1.f / 90;
        in.speed = 20;
        check(angleBetween(up(b.update(in, p)), {0, 1, 0}) < .001f, "an idle bullet turned the body");
        b.launch(look(10), {0, 0, -1}, p);
        settle(b, p, .3f);
        b.launch(look(-50), {0, 0, -1}, p);
        const Quat q = settle(b, p, .3f);
        check(angleBetween(up(q), look(-50)) < .5f && b.launches() == 2, "a second launch did not retarget");
        // Frame hitches never turn it more than a step.
        Bullet h;
        h.launch(look(0), {0, 0, -1}, p);
        Input big;
        big.seconds = 5.f;
        big.speed = 20;
        const Quat q2 = h.update(big, p);
        check(std::isfinite(q2.w) && h.weight() < .99f, "a long frame is not capped at 0.1 s");
    }
    if (failures) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "Point bullet: any eye angle incl. straight down, eased over 120 ms, lets go on landing/web/A/hands/slow/timeout\n";
    return 0;
}
