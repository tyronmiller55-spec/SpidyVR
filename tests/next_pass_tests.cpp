// NEXT PASS (the owner's decisions, Nov 15: handoff hitches, body yaw, handstands): the pure parts of each change.
// Portable.
#include "spidy/camera_mode.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/hybrid_handoff.hpp"
#include "spidy/head_seat.hpp"
#include "spidy/landing_body.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
}
} // namespace

int main() {
    // ONE HANDOFF BLEND (handoff_one_blend): one length, AIR VIEW EASE's; the place and the angle on their own weights.
    {
        using namespace hybrid_handoff;
        check(oneBlendNs(true, 250) == 250'000'000 && oneBlendNs(false, 250) == handoffNs &&
                  oneBlendNs(true, 0) == handoffNs && oneBlendNs(true, 1000) == 400'000'000,
              "the blend's length: AIR VIEW EASE's (150-400 ms), else 180 ms");
        check(near(blendAlphaFor(125'000'000, 250'000'000), .5f) && blendAlphaFor(0, 250'000'000) == 0.f &&
                  blendAlphaFor(250'000'000, 250'000'000) == 1.f && near(blendAlphaFor(90'000'000, 0), blendAlpha(90'000'000)),
              "the same smoothstep over the length (0: 180 ms)");
        check(!blendDoneFor(200'000'000, 250'000'000) && blendDoneFor(250'000'000, 250'000'000) &&
                  blendDoneFor(handoffNs, 0),
              "done at its length");
        const Pose from{{0, 1, 0}, Quat{}}, to{{1, 1, 0}, Quat::yaw(1.f)};
        const Pose p = blendEyeSplit(from, {}, to, 1.f, 0.f);
        check(near(p.position.x, 1.f) && near(p.orientation.w, 1.f), "the place carried, the angle still the old");
        const Pose q = blendEyeSplit(from, {}, to, .5f, .5f), r = blendEye(from, {}, to, .5f);
        check(near(q.position.x, r.position.x) && near(q.orientation.y, r.orientation.y), "equal weights: blendEye");
        check(vr_settings::Values{}.handoffOneBlend, "on by default");
    }
    // SNAP BODY TURN / SNAP TAP WINDOW: on by default, 120 ms, the turn named in the logs.
    {
        const vr_settings::Values v{};
        check(v.snapBodyTurn && v.snapTapMs == 120, "snap body turn on, tap window 120 ms");
        check(near(camera_mode::SnapRealign::tapSecondsOf(v.snapTapMs), .12f), "the window in seconds");
        check(std::string(facing_check::eventName(facing_check::Event::realign)) == "snap realign" &&
                  std::string(facing_check::eventName(facing_check::Event::dodge)) == "dodge re-face",
              "the hero turns' names");
    }
    // DODGE RE-FACE: on by default; due when the dodge's tail opens (0.4 s after the button), gone when it closes.
    {
        check(vr_settings::Values{}.dodgeReface, "dodge re-face on by default");
        view_yaw_lock::DodgeTail t;
        bool armed = false, pending = false;
        int dueAt = -1;
        for (int i = 0; i < 400; ++i) {
            const bool dodge = i < 30;
            t.update(.01f, dodge);
            if (dodge) {
                armed = true;
                pending = false;
            } else if (armed && t.open()) {
                armed = false;
                pending = true;
                dueAt = i;
            } else if (!t.open()) {
                pending = false;
            }
        }
        check(dueAt >= 69 && dueAt <= 71, "due 0.4 s after the dodge button is let go");
        check(!pending, "and not left due once the tail has closed (taken once in game)");
    }
    // LANDING BODY TO VIEW / LANDING REAR TURN.
    {
        using namespace landing_body;
        constexpr float deg = 3.14159265f / 180.f, dt = 1.f / 90.f;
        const vr_settings::Values v{};
        check(v.landingBodyToView && v.landingBodyEaseMs == 500 && v.landingRearTurn, "on, 500 ms, rear turn on");
        // The turn's sign: the rig's yaw convention.
        const Vec3 f = Quat::yaw(.3f).rotate({0, 0, -1});
        check(near(hybrid_handoff::lookYaw(turnOf(.5f).rotate(f)), .8f), "turnOf(a) turns a facing by +a");
        // A 60-degree landing: eases in (no snap), holds while he comes round, eases out, ends.
        Ease e;
        e.begin(.5f, true);
        float hero = 0, view = 60 * deg, last = 0, worstStep = 0, t = 0;
        bool started = false, out = false, past = false;
        End ended = End::none;
        for (int i = 0; i < 600 && ended == End::none; ++i, t += dt) {
            const bool turning = t > .3f && std::abs(view - hero) > 12 * deg;
            if (turning)
                hero += std::min(142 * deg * dt, view - hero);
            const auto o = e.update(dt, view - hero, turning, false, false);
            started |= o.started;
            out |= o.easingOut == End::arrived;
            ended = o.ended;
            worstStep = std::max(worstStep, std::abs(o.yaw - last));
            last = o.yaw;
            past |= std::abs(hero + o.yaw - view) > 60 * deg + 1e-3f;
        }
        check(!past, "never turned past the view");
        check(started && out && ended == End::arrived, "in, held while he turns, out once he arrives");
        check(worstStep < 3 * deg, "smooth: under 3 degrees a frame (no snap)");
        // Small gaps are left alone; a rear gap only with LANDING REAR TURN.
        Ease small;
        small.begin(.5f, true);
        check(small.update(dt, 8 * deg, false, false, false).ended == End::skippedSmall, "8 degrees: nothing to show");
        Ease rear;
        rear.begin(.5f, false);
        check(rear.update(dt, 150 * deg, false, false, false).ended == End::skippedRear, "rear, rear turn off: kept");
        Ease rearOn;
        rearOn.begin(.5f, true);
        float y = 0;
        for (int i = 0; i < 60; ++i)
            y = rearOn.update(dt, (i % 2 ? 179.f : -179.f) * deg, true, false, false).yaw;
        check(std::abs(y) > 170 * deg && y < 0, "a gap round +-180: kept the same way round, no flip");
        // The left stick and a takeover end it.
        Ease drive;
        drive.begin(.5f, true);
        drive.update(dt, 50 * deg, false, false, false);
        check(drive.update(dt, 50 * deg, false, true, false).easingOut == End::driving, "the left stick: eases out");
        Ease cut;
        cut.begin(.5f, true);
        cut.update(dt, 50 * deg, false, false, false);
        const auto c = cut.update(dt, 50 * deg, false, false, true);
        check(c.ended == End::cancelled && c.yaw == 0 && !cut.active(), "a takeover: gone at once");
        // No turn ever starts: it does not hold the body off his facing for long.
        Ease idle;
        idle.begin(.5f, true);
        int frames = 0;
        while (idle.active() && frames < 1000)
            idle.update(dt, 40 * deg, false, false, false), ++frames;
        check(frames * dt < 1.7f, "no turn: back on his facing within 1.6 s");
        // The facing check's rear rule follows the setting.
        facing_check::Check fc;
        fc.arm(facing_check::Event::landing, 0.f);
        facing_check::Sample fs;
        fs.heroMatters = fs.heroOk = true;
        fs.hero = 150 * deg;
        fs.rearTurns = false;
        check(fc.update(dt, fs).hero.verdict == facing_check::Verdict::kept, "rear turn off: kept (as before)");
        fc.arm(facing_check::Event::landing, 0.f);
        fs.rearTurns = true;
        const auto r = fc.update(dt, fs);
        check(r.hero.verdict == facing_check::Verdict::corrected && r.heroTurn, "rear turn on: he turns");
    }
    // HANDOFF GLIDE SPEED: long enough that the peak speed stays under the setting; never shorter, capped.
    {
        using namespace hybrid_handoff;
        const vr_settings::Values v{};
        check(v.handoffGlideCmS == 200 && v.handoffGlideMaxMs == 700 && v.handoffEaseCurve == 1, "defaults 2 m/s, 700 ms, gentle");
        check(glideMs(.3f, 250, 200, 700, peakSmootherstep) == 282, "0.3 m gentle at 2 m/s: 282 ms");
        check(glideMs(.2f, 250, 200, 700, peakSmootherstep) == 250, "a short offset keeps the ease's 250 ms");
        check(glideMs(.6f, 250, 200, 700, peakSmootherstep) == 563, "0.6 m: 563 ms");
        check(glideMs(2.f, 250, 200, 700, peakSmootherstep) == 700, "capped at the longest");
        check(glideMs(.9f, 250, 0, 700, peakSmoothstep) == 250, "0: the fixed length");
        check(glideMs(.9f, 900, 200, 700, peakSmoothstep) == 900, "a base over the cap stays");
        check(glideMs(NAN, 250, 200, 700, peakSmoothstep) == 250, "a bad distance: the base");
        const float peakSpeed = peakSmootherstep * .6f / (glideMs(.6f, 250, 200, 700, peakSmootherstep) * .001f);
        check(peakSpeed <= 2.f + 1e-3f, "the peak speed stays under 2 m/s");
    }
    // HANDOFF HEAD LEADS: the switch's angle at 0, the live view at 1, and head turns pass 1:1 in between.
    {
        using namespace hybrid_handoff;
        const vr_settings::Values v{};
        check(v.handoffHeadLeads && v.handoffAngleMs == 300, "defaults on, 300 ms");
        const auto qdot = [](Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; };
        const Quat from = arc({0, 0, 1}, normalized(Vec3{0, -.6f, 1}));
        const Quat to0 = Quat::yaw(0);
        const Quat off = oriOffsetOf(from, to0);
        const Quat a0 = headLeadOri(off, to0, 0);
        check(std::abs(std::abs(qdot(a0, from)) - 1.f) < 1e-4f, "starts on the switch's view");
        const Quat to1 = Quat::yaw(.5f);
        const Quat a1 = headLeadOri(off, to1, 1);
        check(std::abs(std::abs(qdot(a1, to1)) - 1.f) < 1e-4f, "ends on the live view");
        // Mid-blend, a head turn changes the shown view by exactly that turn.
        const Quat m0 = headLeadOri(off, to0, .5f), m1 = headLeadOri(off, to1, .5f);
        const Quat turn = m1 * m0.conjugate();
        const Quat want = to1 * to0.conjugate();
        check(std::abs(std::abs(turn.w) - std::abs(want.w)) < 1e-4f, "a head turn mid-blend moves the view by the same angle");
    }
    // LANDING BLEND: its own setting, defaulting to the old length.
    {
        const vr_settings::Values v{};
        check(v.landingBlendMs == 250 && v.landingBlendMs == v.airViewEaseMs, "landing blend 250 ms, as before");
        check(hybrid_handoff::oneBlendNs(true, v.landingBlendMs) == 250'000'000, "250 ms blend");
    }
    // HEAD SEAT GLIDE: the glide runs its own length.
    {
        const vr_settings::Values v{};
        check(v.headSeatGlideMs == 250, "head seat glide 250 ms");
        check(head_seat::alpha(125'000'000, 250'000'000) == .5f && head_seat::alpha(180'000'000) == 1.f &&
                  head_seat::alpha(180'000'000, 250'000'000) < 1.f,
              "250 ms glide halfway at 125 ms; the old 180 ms default stays");
    }
    // TURN SPEED: the walk stick's push follows turn_push_pct (55 by default; 35 the old fixed push).
    {
        const vr_settings::Values v{};
        check(v.turnPushPct == 55 && v.landingBodyHoldTurn, "turn push 55, landing body holds the turn");
        view_yaw_lock::TurnInPlace t;
        view_yaw_lock::TurnInPlace::In in;
        check(in.push == view_yaw_lock::pushTilt, "the old push unless set");
        in.dt = 1.f / 72;
        in.force = true;
        in.forceYaw = 2.f;
        in.forceAllowed = true;
        in.heroYaw = 0;
        in.push = .55f;
        auto o = t.update(in);
        check(o.push && std::abs(o.tilt - .55f) < 1e-6f, "pushes at 55%");
        in.force = false;
        for (int i = 0; i < 30; ++i)
            o = t.update(in);
        check(o.push && std::abs(o.tilt - .55f) < 1e-6f, "stuck: never less than the push");
        in.push = .35f;
        view_yaw_lock::TurnInPlace t2;
        in.force = true;
        t2.update(in);
        in.force = false;
        for (int i = 0; i < 30; ++i)
            o = t2.update(in);
        check(std::abs(o.tilt - view_yaw_lock::pushTiltHard) < 1e-6f, "at 35: the old hard push when stuck");
    }
    // LANDING BODY HOLDS THE TURN: a pause in the walk stick's turn is no arrival while the gap is still wide.
    {
        const auto run = [](bool hold) {
            landing_body::Ease e;
            e.holdToArrival = hold;
            e.begin(.2f, true);
            landing_body::End out = landing_body::End::none;
            for (int i = 0; i < 30; ++i) // ease in, the turn running
                e.update(1.f / 72, 1.0f, true, false, false);
            for (int i = 0; i < 5; ++i) { // the turn pauses, 40 degrees left
                const auto o = e.update(1.f / 72, .7f, false, false, false);
                if (o.easingOut != landing_body::End::none)
                    out = o.easingOut;
            }
            for (int i = 0; i < 5 && out == landing_body::End::none; ++i) { // he arrives
                const auto o = e.update(1.f / 72, .1f, true, false, false);
                if (o.easingOut != landing_body::End::none)
                    out = o.easingOut;
            }
            return out;
        };
        check(run(false) == landing_body::End::arrived, "off: the pause reads as arrived");
        check(run(true) == landing_body::End::arrived, "on: arrived once inside 12 degrees");
        landing_body::Ease e;
        e.holdToArrival = true;
        e.begin(.2f, true);
        for (int i = 0; i < 30; ++i)
            e.update(1.f / 72, 1.0f, true, false, false);
        bool early = false;
        for (int i = 0; i < 5; ++i)
            early = early || e.update(1.f / 72, .7f, false, false, false).easingOut != landing_body::End::none;
        check(!early, "on: still held through the pause");
    }
    std::printf("next pass: %d checks passed\n", passed);
    return 0;
}
