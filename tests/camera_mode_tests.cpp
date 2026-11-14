// CAMERA MODES (camera_mode.hpp; the user, November 13 15:42): the animation's
// head ride and its blends, the shoulder limit, the snap/realign's taps, the
// perch watch, the view lock's shown yaw with the ride and the limit, the
// owner's perch, the settings.
#include "spidy/camera_mode.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::camera_mode;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float deg = 3.14159265f / 180.f;
bool near(float a, float b, float eps = 1e-3f) {
    return std::abs(wrap(a - b)) <= eps;
}
RideIn standing() {
    RideIn in;
    in.lockOn = true;
    in.animOwns = true;
    in.speed = 0;
    in.moveStick = 0;
    return in;
}
} // namespace

int main() {
    // ---- When the ride is wanted ----
    {
        check(rideWanted(standing()) == RideWhy::still, "standing still in the animation: ridden");
        RideIn in = standing();
        in.landing = true;
        in.sinceAnim = .1f;
        in.moveStick = .9f;
        check(rideWanted(in) == RideWhy::landing, "a landing's first 0.3 s: ridden whatever the stick says");
        in.sinceAnim = .4f;
        check(rideWanted(in) == RideWhy::none, "then the walk stick takes him into the run: locomotion");
        in.moveStick = 0;
        check(rideWanted(in) == RideWhy::landing, "the stick idle: the landing's animation still ridden");
        in.sinceAnim = 2;
        check(rideWanted(in) == RideWhy::still, "after it, standing still");
        in = standing();
        in.speed = 5;
        in.moveStick = .1f;
        check(rideWanted(in) == RideWhy::none, "sliding/running on: locomotion");
        in.perched = true;
        check(rideWanted(in) == RideWhy::perch, "a perch: ridden");
        in = standing();
        in.moveStick = .5f;
        check(rideWanted(in) == RideWhy::none, "walking: locomotion (Hybrid v2)");
        bool RideIn::*const gates[] = {&RideIn::fighting, &RideIn::onWall, &RideIn::turning, &RideIn::legs,
                                       &RideIn::snapHold};
        for (auto g : gates) {
            in = standing();
            in.*g = true;
            check(rideWanted(in) == RideWhy::none, "a fight, wall, R-stick turn, legs or snap: no ride");
        }
        in = standing();
        in.animOwns = false;
        check(rideWanted(in) == RideWhy::none, "SpidyVR's body (the air): no ride");
        in = standing();
        in.enabled = false;
        check(rideWanted(in) == RideWhy::none, "anim_head_ride=0: never");
        in = standing();
        in.lockOn = false;
        check(rideWanted(in) == RideWhy::none, "no head lock: no ride");
    }
    // ---- The mode and its blends ----
    {
        HeadRide r;
        check(r.mode == Mode::locomotion && r.eased() == 0.f, "starts in locomotion");
        auto s = r.update(1.f / 90, true);
        check(s.entered && r.mode == Mode::animHeadRide && r.enters == 1, "entered");
        float last = r.eased();
        bool monotone = true;
        for (int i = 0; i < 30; ++i) {
            r.update(1.f / 90, true);
            monotone = monotone && r.eased() >= last;
            last = r.eased();
        }
        check(monotone && r.eased() == 1.f, "eases in to the head within 0.25 s (30 frames at 90 Hz is 0.33 s)");
        // Never a step bigger than the blend allows at 90 Hz (smoothstep's peak slope 1.5).
        HeadRide q;
        float prev = 0, biggest = 0;
        for (int i = 0; i < 40; ++i) {
            q.update(1.f / 90, true);
            biggest = std::max(biggest, q.eased() - prev);
            prev = q.eased();
        }
        check(biggest <= 1.5f / (90 * HeadRide::enterSeconds) + 1e-4f, "entry: no pop (largest step within the blend)");
        s = r.update(1.f / 90, false);
        check(s.exited && r.mode == Mode::locomotion && r.active(), "exit: locomotion, still blending out");
        for (int i = 0; i < 30; ++i)
            r.update(1.f / 90, false);
        check(r.eased() == 0.f && !r.active(), "eased out within 0.2 s");
        r.update(1.f / 90, true);
        r.reset();
        check(r.weight == 0.f && r.mode == Mode::locomotion, "reset (a realign): at once");
        // The shown yaw with the ride.
        check(near(rideYaw(10 * deg, 50 * deg, 0), 10 * deg), "ride 0: the lock's");
        check(near(rideYaw(10 * deg, 50 * deg, 1), 50 * deg), "ride 1: the head bone");
        check(near(rideYaw(170 * deg, -170 * deg, .5f), 180 * deg), "the short way round");
    }
    // ---- The view lock's shown yaw: ride then limit, on every anchor ----
    {
        view_yaw_lock::State s;
        s.on = true;
        s.yaw = 0;
        s.sway = 0;
        s.heroYaw = 0;
        check(near(view_yaw_lock::shownYaw(s, 40 * deg), 0), "no ride: the lock");
        s.ride = 1;
        check(near(view_yaw_lock::shownYaw(s, 40 * deg), 40 * deg), "riding: the head bone");
        s.limit = -5 * deg;
        check(near(view_yaw_lock::shownYaw(s, 40 * deg), 35 * deg), "the limit added last");
        s.on = false;
        check(near(view_yaw_lock::shownYaw(s, 40 * deg), 40 * deg), "lock off: the bone, nothing added");
        // The pose job's arm anchor is turned the same (turnHead).
        s.on = true;
        s.ride = .5f;
        s.limit = 0;
        Mat4 h{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1}; // facing -z: yaw 0
        float d = 0;
        view_yaw_lock::turnHead(h, s, &d);
        check(near(d, 0), "turnHead: a bone at the lock is left alone");
    }
    // ---- The shoulder limit ----
    {
        ShoulderLimit l;
        check(l.correction(30 * deg, 0) == 0.f, "inside the shoulders: nothing");
        ShoulderLimit a;
        const float c = a.correction(120 * deg, 0);
        check(near(120 * deg + c, 90 * deg), "past the left shoulder: held at it");
        // Turning on past 180 keeps it at the same shoulder (no flip at the back).
        float shown = 0;
        for (int y = 120; y <= 260; y += 10) {
            shown = wrap(y * deg + a.correction(wrap(y * deg), 0));
            check(near(shown, 90 * deg), "past the back: still the same shoulder");
        }
        // Only once really round to the other shoulder does it let go there.
        const float back = wrap(275 * deg);
        check(a.correction(back, 0) == 0.f, "at the other shoulder: inside again, no hold");
        // The old rule's runaway: never more than 180 of correction.
        ShoulderLimit w;
        float worst = 0;
        for (int y = 0; y <= 720; y += 7)
            worst = std::max(worst, std::abs(w.correction(wrap(y * deg), 0)));
        check(worst <= 180 * deg + 1e-3f, "two full turns of the head: never the 270-degree runaway");
        // The body turning away under a still view: the view follows at the shoulder.
        ShoulderLimit b;
        for (int y = 0; y <= 170; y += 10) {
            const float chest = wrap(-y * deg);
            const float v = wrap(0 + b.correction(0, chest));
            check(std::abs(wrap(v - chest)) <= 90 * deg + 1e-4f, "the chest turning away: kept within his shoulders");
        }
        ShoulderLimit n;
        check(n.correction(NAN, 0) == 0.f && !n.primed, "unknown: nothing");
        // Where the shoulders face.
        bool used = false;
        check(near(shouldersYaw(true, 30 * deg, 0, &used), 30 * deg) && used, "the chest bone when it is a twist");
        check(near(shouldersYaw(true, 150 * deg, 0, &used), 0) && !used, "a chest 150 off his facing: his facing");
        check(near(shouldersYaw(false, 30 * deg, 10 * deg, &used), 10 * deg) && !used, "no chest: his facing");
        check(viewLimitActive.load(), "the limit's global switch: on by default");
    }
    // ---- The snap / realign ----
    {
        SnapRealign s;
        // Smooth mode: a quick tap right is one snap right (- yaw), on release.
        auto o = s.update(.01f, .9f, true, 30);
        check(o.began && !o.fire, "smooth: the push starts, nothing yet");
        o = s.update(.1f, .9f, true, 30);
        check(!o.fire, "still out");
        o = s.update(.05f, .1f, true, 30);
        check(o.fire && near(o.turn, -30 * deg), "smooth: back within 0.25 s: one snap right");
        // A long push is the smooth turn, no snap.
        s.update(.01f, -.9f, true, 30);
        for (int i = 0; i < 4; ++i)
            o = s.update(.1f, -.9f, true, 30);
        o = s.update(.01f, 0, true, 30);
        check(!o.fire, "smooth: a held push turns smoothly, no snap");
        // Snap mode: fires on the way out, once per push; left is + yaw.
        SnapRealign k;
        o = k.update(.01f, -.8f, false, 30);
        check(o.fire && near(o.turn, 30 * deg), "snap: fires on the way out, left");
        o = k.update(.5f, -.8f, false, 30);
        check(!o.fire, "once per push");
        k.update(.01f, 0, false, 30);
        // Twelve taps: the full 360 round.
        float heading = 0;
        for (int i = 0; i < 12; ++i) {
            o = k.update(.01f, .9f, false, 30);
            heading = wrap(heading + o.turn);
            k.update(.01f, 0, false, 30);
        }
        check(near(heading, 0) && k.taps == 13, "taps keep turning, the full 360");
        check(near(SnapRealign::stepRadians(0), 45 * deg), "SNAP TURN 0: 45-degree steps");
        // The outside request (a stacked camera mode's exit).
        float y = 0;
        check(!takeRealign(y), "no request");
        realignToHeading(1.f);
        check(takeRealign(y) && y == 1.f && !takeRealign(y), "a request, taken once");
        realignToHeading();
        check(takeRealign(y) && std::isnan(y), "NaN: where the player looks");
        check(!snapInputYielded.load(), "the stick is the snap's by default");
    }
    // ---- The perch watch ----
    {
        PerchWatch p;
        PerchWatch::In in;
        in.dt = .05f;
        in.airborne = true;
        in.free = true;
        bool began = false;
        for (int i = 0; i < 4; ++i)
            began = began || p.update(in).began;
        check(!began && !p.perched, "0.2 s still: not yet");
        began = p.update(in).began || p.update(in).began;
        check(began && p.perched && p.perches == 1, "0.25 s still off the ground, free: a perch");
        in.up = -3;
        check(p.update(in).ended && !p.perched, "falling off: left at once");
        PerchWatch q;
        in.up = 0;
        in.free = false; // a web
        for (int i = 0; i < 20; ++i)
            q.update(in);
        check(!q.perched, "hanging on a web: no perch");
        PerchWatch apex;
        in.free = true;
        for (int i = 0; i < 20; ++i) {
            in.up = 3.f - .5f * i; // a jump's apex passes through 0 quickly
            apex.update(in);
        }
        check(!apex.perched, "a jump's apex: no perch");
    }
    // ---- The owner: a perch is the animation's, as a landing ----
    {
        hybrid_owner::HybridOwner o;
        o.domain = hybrid_owner::Domain::vr;
        hybrid_owner::Frame f;
        f.state.playing = f.state.fresh = true;
        f.state.grounded = false;
        f.perched = true;
        o.update(f, 1'000'000'000);
        check(o.domain == hybrid_owner::Domain::anim && o.lastEnterAnim == hybrid_owner::EnterAnim::land,
              "a perch: the animation's (a landing)");
        hybrid_owner::HybridOwner w;
        w.domain = hybrid_owner::Domain::vr;
        f.state.wall = hybrid_owner::surfaceWall;
        w.update(f, 1'000'000'000);
        check(w.domain == hybrid_owner::Domain::vr, "Spidy's wall: never a perch");
    }
    // ---- Settings ----
    {
        vr_settings::Values v;
        check(v.animHeadRide && v.shoulderLimit && v.snapRealign, "on by default");
        vr_settings::Values p =
            vr_settings::withTabOnly(vr_settings::Values{}, "anim_head_ride=0\nshoulder_limit=0\nsnap_realign=0\n");
        check(!p.animHeadRide && !p.shoulderLimit && !p.snapRealign, "each turns off");
    }
    std::printf("camera mode: %d checks passed\n", passed);
    return 0;
}
