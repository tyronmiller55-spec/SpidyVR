// LEVEL KEEPS HIM WITH YOU (level_head_lock.hpp, beta3p): when LEVEL joins the head lock, the level frame for a
// heading, the latch's heading without the crown rule, the cuts that re-latch, and the switch's default.
#include "spidy/level_head_lock.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/vr_settings.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy::level_head_lock;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
bool near(float a, float b, float e = 1e-4f) {
    return std::abs(a - b) <= e;
}
float wrap(float a) {
    return std::remainder(a, 6.28318531f);
}
// A head pose (rows right, down, forward) facing `yaw` and pitched up by `pitch` (radians).
std::array<float, 16> head(float yaw, float pitch) {
    const float fx = -std::sin(yaw) * std::cos(pitch), fy = std::sin(pitch), fz = -std::cos(yaw) * std::cos(pitch);
    // down: perpendicular to forward, in the vertical plane, pointing down-ish
    const float dx = -std::sin(yaw) * std::sin(pitch), dy = -std::cos(pitch), dz = -std::cos(yaw) * std::sin(pitch);
    return {std::cos(yaw), 0, -std::sin(yaw), 0, dx, dy, dz, 0, fx, fy, fz, 0, 1, 2, 3, 1};
}
} // namespace

int main() {
    // Who joins the lock.
    check(joins(true, true, true, true), "LEVEL, HYBRID, HEADSET FIRST and the switch: on the lock");
    check(!joins(false, true, true, true), "the switch off: LEVEL as beta3o");
    check(!joins(true, false, true, true), "FULL or softlock: not this switch's");
    check(!joins(true, true, false, true), "not HYBRID: LEVEL as beta3o");
    check(!joins(true, true, true, false), "HEADSET FIRST off (the bone's sway): LEVEL as beta3o");

    // The level frame for a heading is the old latch's frame for that heading.
    for (float yaw : {0.f, .7f, -2.4f, 3.1f}) {
        float m[16];
        axes(yaw, m);
        check(near(std::atan2(-m[8], -m[10]), yaw), "the frame reads back its heading (levelYaw)");
        check(near(m[9], 0) && near(m[1], 0) && near(m[5], -1), "the frame is level: no pitch, no roll");
        check(near(m[0] * m[8] + m[2] * m[10], 0), "right and forward are square");
        // The old latch: ax, az = forward without height; level = {-az, 0, ax, 0, 0,-1,0,0, ax,0,az,0, ...}
        const auto h = head(yaw, .3f);
        float ax = h[8], az = h[10];
        const float l = std::sqrt(ax * ax + az * az);
        ax /= l;
        az /= l;
        check(near(m[0], -az) && near(m[2], ax) && near(m[8], ax) && near(m[10], az), "the old latch's layout");
    }

    // The latch's heading: a head facing somewhere is its forward without its height, as before.
    float yaw = 0;
    check(latchYaw(head(1.f, .2f).data(), true, yaw) && near(yaw, 1.f), "a level-ish head: its forward");
    check(latchYaw(head(1.f, .2f).data(), false, yaw) && near(yaw, 1.f), "off: the same");
    // Looking steeply up (a backflip, a wall run up): off, the crown rule's yaw; on, no latch.
    const auto up = head(0.f, 1.45f);
    check(latchYaw(up.data(), false, yaw) && near(yaw, std::atan2(-up[4], -up[6])),
          "off (beta3o): the crown rule's yaw, from the head's second axis");
    check(!latchYaw(up.data(), true, yaw), "on: a head looking steeply up gives no heading");
    check(!latchYaw(head(0.f, -1.45f).data(), true, yaw), "on: steeply down gives none either");
    // The lock's view (game_fp::frame turns the head onto the lock's yaw): nativeYaw reads the lock's yaw back,
    // even pitched steeply - so the level frame on the lock never faces behind.
    {
        spidy::view_yaw_lock::State s;
        s.on = true;
        s.yaw = .5f;
        s.sway = 0;
        s.heroYaw = .5f;
        spidy::Mat4 h{};
        const auto steep = head(2.f, 1.45f);
        for (int i = 0; i < 16; ++i)
            h[i] = steep[i];
        check(spidy::view_yaw_lock::turnHead(h, s), "the lock turns the head");
        float shown = 0;
        check(spidy::view_yaw_lock::nativeYaw(h, shown) && near(wrap(shown - .5f), 0, 1e-3f),
              "a steep head on the lock reads the lock's yaw");
        const auto flat = head(-1.f, .1f);
        for (int i = 0; i < 16; ++i)
            h[i] = flat[i];
        spidy::view_yaw_lock::turnHead(h, s);
        check(spidy::view_yaw_lock::nativeYaw(h, shown) && near(wrap(shown - .5f), 0, 1e-3f),
              "any bone yaw on the lock (sway 0): the lock's yaw");
    }

    // The cuts: off, every cut re-latches; on, the one-metre jump cuts (a fast dive) do not.
    check(cutKey(7, 3, false) == 7, "off: every cut");
    check(cutKey(7, 3, true) == 4, "on: the jump cuts left out");
    check(cutKey(8, 4, true) == cutKey(7, 3, true), "on: a fast dive's jump cut does not re-latch");
    check(cutKey(8, 3, true) != cutKey(7, 3, true), "on: a yield's start or another skeleton still does");
    check(cutKey(1, 2, true) == 0xffffffffu, "the counters wrap together");

    // The switch: on by default, a key of its own, revision 27 turns it on once.
    using namespace spidy::vr_settings;
    check(Values{}.levelHeadLock, "LEVEL KEEPS HIM WITH YOU is on by default");
    check(movementDefaultsRevision == 27, "revision 27");
    const std::string old = "movement_defaults=26\nlevel_head_lock=0\n";
    check(withMovementDefaults(withTabOnly({}, old), old).levelHeadLock, "revision 27 turns it on once");
    const std::string kept = "movement_defaults=27\nlevel_head_lock=0\n";
    check(!withMovementDefaults(withTabOnly({}, kept), kept).levelHeadLock, "a 0 after revision 27 stays");
    check(tabOnlyText(Values{}).find("level_head_lock=1\n") != std::string::npos, "the key is saved");

    std::printf("level_head_lock: %d checks passed\n", passed);
    return 0;
}
