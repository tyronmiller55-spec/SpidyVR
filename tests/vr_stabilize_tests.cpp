// VR STABILIZATION (vr_stabilize.hpp, November 14), STEADY (headset_smoothing's fourth bank), HORIZON LEVEL
// and the raised ranges. Portable: no Windows, no headset.
#include "spidy/camera_motion.hpp"
#include "spidy/headset_smoothing.hpp"
#include "spidy/vr_settings.hpp"
#include "spidy/vr_stabilize.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace spidy;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float deg = 3.14159265f / 180;
float degreesOf(Quat q) {
    return camera_motion::tiltAngle(q) / deg;
}
} // namespace

int main() {
    using vr_settings::Values;
    // Defaults change nothing: the bank is OFF with no response times, the amounts whole.
    {
        const Values d;
        const auto s = vr_stabilize::fromValues(d);
        check(s.on && !s.any(), "defaults: VR STABILIZATION is on but changes nothing");
        check(camera_motion::fromValues(d).identity(), "defaults: the camera amounts are the identity");
        check(!headset_smoothing::fromValues(d).any(), "defaults: no smoothing");
        vr_stabilize::TiltFilter filter;
        const Quat tilt = Quat::around({1, 0, 0}, 20 * deg);
        const auto r = filter.apply(Quat::yaw(.7f), tilt, .011f, s, false);
        check(!r.changed && std::abs(degreesOf(r.tilt) - 20.f) < .01f,
              "the identity leaves the tilt as it is");
    }
    // STEADY: the fourth bank, 350/300/450 ms, held on hardest, up to 1000 ms.
    {
        Values v;
        check(v.steadyVerticalMs == 350 && v.steadyHorizontalMs == 300 && v.steadyAngleMs == 450,
              "STEADY defaults");
        v.cameraStabilization = 3;
        const auto p = headset_smoothing::fromValues(v);
        check(std::abs(p.verticalSec - .35f) < 1e-5f && std::abs(p.horizontalSec - .3f) < 1e-5f &&
                  std::abs(p.angleSec - .45f) < 1e-5f,
              "STEADY's response times");
        v.cameraStabilization = 2;
        const auto heavy = headset_smoothing::fromValues(v);
        check(p.angleBeta < heavy.angleBeta && p.positionBeta < heavy.positionBeta,
              "STEADY damps speed hardest");
        v.cameraStabilization = 3;
        v.steadyAngleMs = 5000;
        check(std::abs(headset_smoothing::fromValues(vr_settings::sanitized(v)).angleSec - 1.f) < 1e-5f,
              "response times go to 1000 ms, no further");
        check(vr_settings::sanitized(v).cameraStabilization == 3, "STEADY is a valid bank");
        v.cameraStabilization = 9;
        check(vr_settings::sanitized(v).cameraStabilization == 3, "a bank past STEADY is clamped to it");
        const auto* row = vr_settings::rowFor(vr_settings::Item::cameraStabilization);
        check(row && row->choices.size() == 4 && std::strcmp(row->choices[3], "STEADY") == 0,
              "STABILIZATION's row");
        const auto* ms = vr_settings::rowFor(vr_settings::Item::steadyAngle);
        check(ms && ms->choices.size() == 11 && std::strcmp(ms->choices[10], "1000 MS") == 0 &&
                  ms->choices.size() <= vr_settings::maxChoices,
              "the response rows reach 1000 ms");
        check(headset_smoothing::maxVerticalLag > .25f && headset_smoothing::maxHorizontalLag > .35f &&
                  camera_motion::maxBobRemovedVertical > .15f && camera_motion::maxBobRemovedSide > .2f,
              "the lag and bob-removal caps are raised");
    }
    // Old files stay valid: the old keys and values read as they did, the new keys default.
    {
        const Values old =
            vr_settings::withTabOnly(Values{}, "camera_stabilization=2\ncamera_heavy_angle_ms=500\n");
        check(old.cameraStabilization == 2 && old.heavyAngleMs == 500 && old.steadyAngleMs == 450 &&
                  old.cameraHorizonLevelPct == 0 && old.cameraVrStabilization == 1,
              "an older file keeps its bank and gets the new defaults (no migration needed)");
        const Values fresh = vr_settings::withTabOnly(
            Values{}, "camera_stabilization=3\ncamera_steady_vertical_ms=800\ncamera_horizon_level_pct=75\n"
                      "camera_vr_stabilization=0\n");
        check(fresh.cameraStabilization == 3 && fresh.steadyVerticalMs == 800 &&
                  fresh.cameraHorizonLevelPct == 75 && fresh.cameraVrStabilization == 0,
              "the new keys read back");
        const Values round = vr_settings::withTabOnly(Values{}, vr_settings::tabOnlyText(fresh));
        check(round.steadyVerticalMs == 800 && round.cameraHorizonLevelPct == 75 &&
                  round.cameraVrStabilization == 0,
              "and round-trip");
    }
    // HORIZON LEVEL: folded into the amounts, in the animation and in SpidyVR.
    {
        Values v;
        v.cameraHorizonLevelPct = 100;
        const auto a = camera_motion::fromValues(v);
        check(a.pitch == 0 && a.roll == 0, "HORIZON LEVEL 100% levels pitch and roll");
        v.cameraHorizonLevelPct = 50;
        v.cameraRollPct = 50;
        const auto b = camera_motion::fromValues(v);
        check(std::abs(b.roll - .25f) < 1e-5f && std::abs(b.pitch - .5f) < 1e-5f,
              "it multiplies the amounts");
    }
    // The tilt filter.
    {
        Values v;
        v.cameraHorizonLevelPct = 100;
        const auto s = vr_stabilize::fromValues(v);
        check(s.any() && s.levels(), "HORIZON LEVEL switches VR STABILIZATION on");
        const Quat yaw = Quat::yaw(1.1f);
        // A 25 degree roll about the heading's forward is levelled, about the head.
        const Vec3 forward = yaw.rotate({0, 0, -1});
        vr_stabilize::TiltFilter filter;
        const Quat roll = Quat::around({0, 0, -1}, 25 * deg);
        auto r = filter.apply(yaw, roll, .011f, s, false);
        check(r.changed && r.shownDegrees < 1.f && r.rawDegrees > 24.f, "a swing's roll is levelled out");
        // The heading is kept: the forward line does not move.
        const Vec3 f2 = (yaw * r.tilt).rotate({0, 0, -1});
        check(length(f2 - forward) < .02f, "where the heading looks is kept");
        // A flip passes whole.
        vr_stabilize::TiltFilter flip;
        const Quat upside = Quat::around({0, 0, -1}, 170 * deg);
        r = flip.apply(yaw, upside, .011f, s, false);
        check(r.shownDegrees > 160.f, "a flip past 170 degrees is not levelled");
        // A surface's tilt is never levelled.
        vr_stabilize::TiltFilter wall;
        const Quat onWall = Quat::around({0, 0, -1}, 80 * deg);
        r = wall.apply(yaw, onWall, .011f, s, true);
        check(std::abs(r.shownDegrees - 80.f) < 1.f, "a wall's tilt is deliberate: not levelled");
        // Level at 50%: half the roll.
        v.cameraHorizonLevelPct = 50;
        vr_stabilize::TiltFilter half;
        r = half.apply(yaw, Quat::around({0, 0, -1}, 20 * deg), .011f, vr_stabilize::fromValues(v), false);
        check(std::abs(r.shownDegrees - 10.f) < 1.5f, "50% removes half the roll");
    }
    // The ANGLE response: eased, lag capped, real motion through, flips not eased, snaps cut.
    {
        Values v;
        v.cameraStabilization = 3;
        const auto s = vr_stabilize::fromValues(v);
        check(s.any() && !s.levels() && std::abs(s.angleSec - .45f) < 1e-5f,
              "STEADY's angle reaches the VR filter");
        const Quat yaw = Quat::yaw(0.f);
        vr_stabilize::TiltFilter filter;
        filter.apply(yaw, {}, .011f, s, false);
        // A step of 10 degrees: after one frame most of it is still to come.
        auto r = filter.apply(yaw, Quat::around({0, 0, -1}, 10 * deg), .011f, s, false);
        check(r.shownDegrees > .05f && r.shownDegrees < 4.f, "a step is eased, not taken at once");
        float last = r.shownDegrees;
        for (int i = 0; i < 300; ++i)
            last = filter.apply(yaw, Quat::around({0, 0, -1}, 10 * deg), .011f, s, false).shownDegrees;
        check(std::abs(last - 10.f) < .3f, "and arrives");
        // The lag cap: a steady 60 degree/second sweep never trails by more than the cap.
        vr_stabilize::TiltFilter sweep;
        sweep.apply(yaw, {}, .011f, s, false);
        float worst = 0;
        for (int i = 1; i <= 200; ++i) {
            const float wanted = std::min(60.f, 200.f * .011f * 60.f * (i / 200.f) * 5.f);
            const auto q = sweep.apply(yaw, Quat::around({0, 0, -1}, wanted * deg), .011f, s, false);
            worst = std::max(worst, wanted - q.shownDegrees);
        }
        check(worst <= vr_stabilize::maxLagDegrees + .5f, "the tilt never trails by more than the cap");
        // A jump past the snap angle is a cut.
        vr_stabilize::TiltFilter cut;
        cut.apply(yaw, {}, .011f, s, false);
        r = cut.apply(yaw, Quat::around({0, 0, -1}, 120 * deg), .011f, s, false);
        check(r.shownDegrees > 119.f, "a recenter-sized jump is not eased");
        // A long frame is no easing either (the hitch rule).
        vr_stabilize::TiltFilter slow;
        slow.apply(yaw, {}, .011f, s, false);
        r = slow.apply(yaw, Quat::around({0, 0, -1}, 10 * deg), .5f, s, false);
        check(r.shownDegrees > 9.f, "a half-second frame takes the target");
        // A flip: tilt past 170 is shown whole at once.
        vr_stabilize::TiltFilter fl;
        fl.apply(yaw, {}, .011f, s, false);
        for (int i = 0; i < 20; ++i)
            r = fl.apply(yaw, Quat::around({0, 0, -1}, (i * 9) * deg), .011f, s, false);
        r = fl.apply(yaw, Quat::around({0, 0, -1}, 175 * deg), .011f, s, false);
        check(r.shownDegrees > 160.f, "a flip is not eased");
        // VR STABILIZATION off: the identity whatever the bank.
        v.cameraVrStabilization = 0;
        check(!vr_stabilize::fromValues(v).any(), "VR STABILIZATION off");
    }
    // ANIM HEAD RIDE holds the angle on harder.
    {
        headset_smoothing::Params p;
        p.angleSec = .3f;
        p.angleBeta = .45f;
        const auto r = headset_smoothing::withRide(p);
        check(r.angleBeta < p.angleBeta && r.snapRad > p.snapRad && r.angleSec == p.angleSec,
              "ANIM HEAD RIDE's yaw");
        // A 60 degree jump is a cut at the default and eased in a ride.
        headset_smoothing::Filter f;
        Mat4 m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        f.update(m, .011f, p, nullptr);
        const Quat turn = Quat::yaw(60 * deg);
        Mat4 t = m;
        for (int row = 0; row < 3; ++row) {
            const Vec3 v = turn.rotate({m[row * 4], m[row * 4 + 1], m[row * 4 + 2]});
            t[row * 4] = v.x;
            t[row * 4 + 1] = v.y;
            t[row * 4 + 2] = v.z;
        }
        const Mat4 cutOut = f.update(t, .011f, p, nullptr);
        check(std::abs(cutOut[8] - t[8]) < 1e-4f, "past 45 degrees: a cut");
        headset_smoothing::Filter g;
        g.update(m, .011f, r, nullptr);
        const Mat4 eased = g.update(t, .011f, r, nullptr);
        check(std::abs(eased[8] - t[8]) > .05f, "in a ride a 60 degree turn is still eased");
    }
    // The diag line is on change: once at the start, again on a 10 degree move or a 5 s heartbeat, never idle.
    {
        vr_stabilize::LogGate gate;
        check(!gate.due(.011f, true, 1.f), "under 2 degrees: no line");
        check(gate.due(.011f, true, 6.f), "the first working frame logs");
        unsigned lines = 0;
        for (int i = 0; i < 90 * 4; ++i) // four seconds of the same tilt
            lines += gate.due(1.f / 90.f, true, 6.5f);
        check(lines == 0, "a steady tilt was logged again inside the heartbeat");
        check(gate.due(.011f, true, 17.f), "a 10 degree change logs");
        for (int i = 0; i < 90 * 6; ++i)
            lines += gate.due(1.f / 90.f, true, 17.f);
        check(lines == 1, "a steady tilt logs once on the 5 s heartbeat");
        check(!gate.due(.011f, false, 17.f) && gate.due(.011f, true, 17.f), "after it idled, the next work logs again");
    }
    std::printf("vr stabilize: %d checks passed\n", passed);
    return 0;
}
