// FULL head lock on the stable heading (view_yaw_lock.hpp; rotation desync
// Step B, the user, November 12 05:56): the eyes' yaw from the lock with the
// head bone's position, pitch and roll; ANIM YAW SWAY; the hands' and arms'
// anchors turned alike; the lock's R-stick turns (smooth rate, snaps);
// TURN-IN-PLACE as a closed loop; the divergence metric; the settings.
#include "spidy/animated_view.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::view_yaw_lock;
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
    return std::abs(wrap(a - b)) <= eps;
}
constexpr float deg = 3.14159265f / 180.f;
// A head as game_fp gives it: yaw, then pitch (+ up) and roll.
Mat4 head(float yaw, float pitch, float roll, Vec3 at = {1, 2, 3}) {
    const Quat q = Quat::yaw(yaw) * Quat::around({1, 0, 0}, pitch) * Quat::around({0, 0, -1}, roll);
    return animated_view::toNative({at, q});
}
} // namespace

int main() {
    // The shown yaw.
    {
        State s;
        s.on = true;
        s.yaw = 10 * deg;
        s.heroYaw = 50 * deg;
        s.sway = 0;
        check(near(shownYaw(s, 58 * deg), 10 * deg), "sway 0%: the lock's yaw only");
        s.sway = .2f;
        check(near(shownYaw(s, 58 * deg), 10 * deg + .2f * 8 * deg), "sway 20%: a fifth of the head's sway (bone - hero)");
        s.sway = 1;
        s.heroYaw = s.yaw;
        check(near(shownYaw(s, 58 * deg), 58 * deg), "sway 100% with the hero on the heading: the head bone");
        s.on = false;
        check(near(shownYaw(s, 58 * deg), 58 * deg), "off: the head bone");
        s.on = true;
        s.heroYaw = NAN;
        s.sway = .5f;
        check(near(shownYaw(s, 70 * deg), 10 * deg), "no hero facing: no sway term");
        s.yaw = 170 * deg;
        s.heroYaw = -170 * deg;
        s.sway = .5f;
        check(near(shownYaw(s, -160 * deg), 175 * deg), "wraps across 180");
        check(lockOn(true, true, 20) && !lockOn(true, true, 100) && !lockOn(false, true, 20) && !lockOn(true, false, 20),
              "on with FULL + STABLE under 100%");
    }
    // The anchor: yaw from the lock; position, pitch and roll from the head bone.
    {
        const Mat4 bone = head(40 * deg, 25 * deg, 10 * deg);
        float y = 0;
        check(nativeYaw(bone, y) && near(y, 40 * deg), "a head's yaw read as game_fp reads it");
        State s;
        s.on = true;
        s.yaw = -30 * deg;
        s.heroYaw = 40 * deg;
        s.sway = 0;
        Mat4 m = bone;
        float d = 0;
        check(turnHead(m, s, &d) && near(d, -70 * deg), "turned by lock - bone");
        check(nativeYaw(m, y) && near(y, -30 * deg), "its yaw is the lock's");
        check(m[12] == bone[12] && m[13] == bone[13] && m[14] == bone[14], "the eyes' place kept");
        check(std::abs(m[9] - bone[9]) < 1e-5f && std::abs(m[1] - bone[1]) < 1e-5f && std::abs(m[5] - bone[5]) < 1e-5f,
              "pitch and roll kept (every axis keeps its height)");
        float dots = 0;
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b) {
                float dp = 0;
                for (int c = 0; c < 3; ++c)
                    dp += m[a * 4 + c] * m[b * 4 + c];
                dots += std::abs(dp - (a == b ? 1.f : 0.f));
            }
        check(dots < 1e-4f, "still orthonormal");
        Mat4 off = bone;
        State none;
        check(!turnHead(off, none) && off == bone, "off: untouched");
        // Steep pitch: the crown's yaw (as game_fp).
        Mat4 steep = head(60 * deg, 85 * deg, 0);
        check(turnHead(steep, s) && nativeYaw(steep, y) && near(y, -30 * deg, 1e-2f), "looking steeply up: still turned onto the lock");
        // The pose job's model-space anchor: the same world turn.
        const Quat toWorld = Quat::yaw(.7f) * Quat::around({1, 0, 0}, .05f);
        const Quat toModel = toWorld.conjugate();
        const Pose worldPose = animated_view::fromNative(bone);
        Mat4 model = animated_view::toNative({toModel.rotate(worldPose.position), toModel * worldPose.orientation});
        check(turnModel(model, toWorld, s), "model anchor turned");
        const Pose back = animated_view::fromNative(model);
        Mat4 world = animated_view::toNative({toWorld.rotate(back.position), toWorld * back.orientation});
        Mat4 direct = bone;
        turnHead(direct, s);
        float err = 0;
        for (int i = 0; i < 12; ++i)
            err += std::abs(world[i] - direct[i]);
        check(err < 1e-3f, "arms' anchor (model space) == eyes' anchor (world)");
        // Hands: a grip resolved on the turned anchor turns with the view.
        const Pose grip{{.2f, -.3f, -.4f}, Quat{}};
        const Pose onBone = animated_view::resolve(bone, grip), onLock = animated_view::resolve(direct, grip);
        const Vec3 rel = Quat::yaw(70 * deg).rotate(onLock.position - Vec3{1, 2, 3});
        check(length(rel - (onBone.position - Vec3{1, 2, 3})) < 1e-3f, "the hands sit where the view is (same turn)");
    }
    // The published state.
    {
        State s;
        s.on = true;
        s.yaw = .3f;
        s.sway = .2f;
        s.heroYaw = .1f;
        publish(s);
        const State l = live();
        check(l.on && l.yaw == .3f && l.sway == .2f && l.heroYaw == .1f, "published, read back");
        publish({});
        check(!live().on, "off published");
    }
    // The lock's heading: latched to show the head bone; turned only by the R stick.
    {
        Lock lock;
        lock.latch(50 * deg, 30 * deg, .2f);
        State s;
        s.on = true;
        s.yaw = lock.yaw;
        s.sway = .2f;
        s.heroYaw = 30 * deg;
        check(near(shownYaw(s, 50 * deg), 50 * deg), "the first frame shows exactly the head bone");
        const float start = lock.yaw;
        float sum = 0;
        for (int i = 0; i < 90; ++i)
            sum += lock.turn(1.f / 90, 1.f, 120, 30);
        check(near(lock.yaw, start - 120 * deg, 1e-3f) && std::abs(sum + 120 * deg) < 1e-3f,
              "smooth 120 deg/s: full right for 1 s turns 120 deg (right is -yaw)");
        lock.turn(1.f / 90, .1f, 120, 30);
        check(near(lock.yaw, start - 120 * deg), "inside the zone: no turn");
        float half = 0;
        for (int i = 0; i < 90; ++i)
            half += lock.turn(1.f / 90, -.5f, 120, 30);
        check(std::abs(half - 60 * deg) < 1e-3f, "half left: 60 deg/s left");
        Lock snap;
        snap.latch(0, 0, 0);
        float by = 0;
        for (int i = 0; i < 30; ++i)
            by += snap.turn(1.f / 90, .9f, 0, 30);
        check(std::abs(by + 30 * deg) < 1e-4f && snap.snaps == 1, "snap 30: one flick, one 30 deg step");
        snap.turn(1.f / 90, .5f, 0, 30);
        snap.turn(1.f / 90, .9f, 0, 30);
        check(snap.snaps == 1, "no re-arm until under 0.4");
        snap.turn(1.f / 90, .2f, 0, 30);
        snap.turn(1.f / 90, -.9f, 0, 30);
        check(snap.snaps == 2 && near(snap.yaw, 0), "re-armed; a left flick back");
        check(snap.turning(0, 0), "a snap counts as turning for 0.3 s");
        for (int i = 0; i < 30; ++i)
            snap.turn(1.f / 90, 0, 0, 30);
        check(!snap.turning(0, 0), "then not");
        Lock unset;
        check(unset.turn(.1f, 1, 120, 0) == 0, "no turn before a latch");
    }
    // TURN-IN-PLACE.
    {
        // A simulated game: the hero turns toward the stick at `rate` deg/s
        // after `lag` s, not past it in a frame.
        struct Game {
            float hero = 0, rate = 300 * deg, lag = .1f, pushedFor = 0;
            int sign = 1;
            void step(float dt, const TurnInPlace::Out& o) {
                pushedFor = o.push ? pushedFor + dt : 0;
                if (!o.push || pushedFor < lag)
                    return;
                const float gap = wrap(o.pushYaw - hero);
                const float step = std::min(std::abs(gap), rate * dt);
                hero = wrap(hero + sign * std::copysign(step, gap));
            }
        };
        const float dt = 1.f / 90;
        TurnInPlace t;
        TurnInPlace::In in;
        in.dt = dt;
        in.allowed = true;
        in.angle = angleOf(35);
        in.heading = 30 * deg;
        Game g;
        auto o = t.update((in.heroYaw = g.hero, in));
        check(!o.push && !o.started, "30 deg under 35: he stays");
        in.heading = 40 * deg;
        o = t.update((in.heroYaw = g.hero, in));
        check(o.push && o.started && near(o.pushYaw, 40 * deg) && o.tilt == pushTilt, "40 deg: the walk stick points at the heading");
        int frames = 0;
        float maxPast = 0;
        while (frames < 300) {
            g.step(dt, o);
            o = t.update((in.heroYaw = g.hero, in));
            maxPast = std::max(maxPast, wrap(g.hero - in.heading));
            ++frames;
            if (o.ended != TurnInPlace::End::none)
                break;
        }
        check(o.ended == TurnInPlace::End::done && std::abs(o.gap) < stopAngle && !o.wrongWay, "done within 12 deg");
        check(maxPast <= 0, "never turned past the heading (no spin)");
        // A game that turns the wrong way at first: the loop still points at the heading.
        TurnInPlace w;
        Game bad;
        bad.sign = -1;
        in.heading = 90 * deg;
        o = w.update((in.heroYaw = bad.hero, in));
        for (int i = 0; i < 20; ++i) {
            bad.step(dt, o);
            o = w.update((in.heroYaw = bad.hero, in));
        }
        check(o.push && near(o.pushYaw, 90 * deg), "turning the wrong way: still pointing at the heading");
        bad.sign = 1;
        for (int i = 0; i < 300 && o.ended == TurnInPlace::End::none; ++i) {
            bad.step(dt, o);
            o = w.update((in.heroYaw = bad.hero, in));
        }
        check(o.ended == TurnInPlace::End::done && o.wrongWay && w.wrongWays == 1, "it ends at the heading; the wrong way is logged");
        // No response: harder after 0.3 s, then it gives up and cools down.
        TurnInPlace dead;
        in.heading = 60 * deg;
        in.heroYaw = 0;
        o = dead.update(in);
        for (int i = 0; i < 40; ++i)
            o = dead.update(in);
        check(o.tilt == pushTiltHard, "no turn after 0.3 s: the harder tilt");
        for (int i = 0; i < 300 && o.ended == TurnInPlace::End::none; ++i)
            o = dead.update(in);
        check(o.ended == TurnInPlace::End::gaveUp && !o.push, "gave up after 2.5 s");
        o = dead.update(in);
        check(!o.started, "cooling down");
        // Interrupted (the left stick, a jump, a fight); off at 0.
        TurnInPlace cut;
        o = cut.update(in);
        in.allowed = false;
        o = cut.update(in);
        check(o.ended == TurnInPlace::End::interrupted && !o.push, "interrupted");
        in.allowed = true;
        in.angle = angleOf(0);
        TurnInPlace offT;
        check(!offT.update(in).push, "OFF: never");
        in.angle = angleOf(35);
        // The R stick: follows past 20 deg.
        TurnInPlace r;
        in.heading = 25 * deg;
        in.heroYaw = 0;
        in.rTurning = true;
        o = r.update(in);
        check(o.started && o.byStick, "R stick turning: follows past 20 deg");
        in.rTurning = false;
    }
    // A standing smooth turn, full right for 3 s at 120 deg/s, the game turning
    // him at up to 300 deg/s: the view turns exactly 360 deg; he keeps up and
    // never spins past.
    {
        const float dt = 1.f / 90;
        Lock lock;
        lock.latch(0, 0, .2f);
        TurnInPlace t;
        TurnWatch watch;
        float hero = 0, heroSwept = 0, pushedFor = 0, maxBehind = 0;
        TurnWatch::Out ended;
        for (int i = 0; i < 90 * 5; ++i) {
            const float x = i < 270 ? 1.f : 0.f;
            const float d = lock.turn(dt, x, 120, 30);
            TurnInPlace::In in;
            in.dt = dt;
            in.allowed = true;
            in.heading = lock.yaw;
            in.heroYaw = hero;
            in.angle = angleOf(35);
            in.rTurning = lock.turning(x, 120);
            const auto o = t.update(in);
            pushedFor = o.push ? pushedFor + dt : 0;
            if (o.push && pushedFor > .1f) {
                const float gap = wrap(o.pushYaw - hero);
                const float step = std::copysign(std::min(std::abs(gap), 300 * deg * dt), gap);
                hero = wrap(hero + step);
                heroSwept += step;
            }
            const float behind = wrap(lock.yaw - hero);
            maxBehind = std::max(maxBehind, std::abs(behind));
            const auto w = watch.update(dt, in.rTurning, d, hero, false);
            if (w.ended)
                ended = w;
        }
        check(ended.ended && std::abs(ended.viewDeg + 360.f) < .5f && std::abs(ended.viewRate - 120.f) < 1.f,
              "the view: 360 deg at 120 deg/s");
        check(std::abs(wrap(lock.yaw - hero)) < stopAngle + 1e-3f, "the hero ends facing the heading");
        check(heroSwept < 0 && std::abs(heroSwept / deg + 360.f) < 13.f, "he turned the same way, about 360 deg (no extra spin)");
        check(maxBehind < 40 * deg, "he keeps within 40 deg while it turns (20 deg follow + the game's lag)");
    }
    // Divergence.
    {
        Divergence d;
        for (int i = 0; i < 90; ++i)
            d.add(1.f / 90, (i < 80 ? 10.f : 60.f) * deg, 2 * deg);
        const auto s = d.summary();
        check(s.samples == 90 && std::abs(s.meanHero - (80 * 10 + 10 * 60) / 90.f) < .1f, "mean view-hero");
        check(s.p95Hero == 60 && s.maxHero > 59.9f && std::abs(s.over45 - 10.f / 90) < 1e-3f, "p95, max, over 45");
        check(std::abs(s.meanBone - 2) < .01f && s.p95Bone == 2, "view-headbone");
        d.reset();
        check(d.summary().samples == 0, "reset");
    }
    // The settings.
    {
        using namespace vr_settings;
        Values v;
        check(v.animYawSwayPct == 20 && v.turnInPlaceDeg == 35, "defaults: 20% sway, 35 deg");
        const Row* sway = rowFor(Item::animYawSway);
        check(sway && std::strcmp(sway->title, "ANIM YAW SWAY") == 0 &&
                  std::strcmp(sway->choices[0], "0% (STEADY)") == 0 &&
                  std::strcmp(sway->choices[sway->choices.size() - 1], "100% (OLD FEEL)") == 0,
              "ANIM YAW SWAY 0-100%, 100% the old feel");
        check(choose(Item::animYawSway, sway->choices.size() - 1, v) && v.animYawSwayPct == 100 &&
                  !lockOn(true, true, v.animYawSwayPct),
              "100%: the lock is off (the old feel)");
        check(tabOnlyText(v).find("anim_yaw_sway_pct=100") != std::string::npos, "saved as anim_yaw_sway_pct");
        const Row* tip = rowFor(Item::turnInPlace);
        check(tip && std::strcmp(tip->choices[0], "OFF") == 0 && std::strcmp(tip->choices[4], "35 DEG") == 0,
              "TURN-IN-PLACE ANGLE: OFF .. 90, 35 default");
        check(choose(Item::turnInPlace, 0, v) && v.turnInPlaceDeg == 0, "OFF is 0");
        check(withTabOnly(Values{}, "turn_in_place_deg=45\n").turnInPlaceDeg == 45, "read back");
        bool a = false, b = false;
        for (Item item : screens()[static_cast<size_t>(Category::camera)].items) {
            a = a || item == Item::animYawSway;
            b = b || item == Item::turnInPlace;
        }
        check(a && b, "both on VR CAMERA & STABILIZATION");
    }
    std::printf("view yaw lock: %d checks passed\n", passed);
    return 0;
}
