// NEXT PASS (the owner's decisions, Nov 15: handoff hitches, body yaw, handstands): the pure parts of each change.
// Portable.
#include "spidy/aim_watch.hpp"
#include "spidy/arm_frame.hpp"
#include "spidy/camera_mode.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/hybrid_handoff.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/head_seat.hpp"
#include "spidy/landing_body.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/web_aim.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

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
        check(v.snapBodyTurn && v.snapTapMs == 250, "snap body turn on, tap window 250 ms (Nov 18)");
        check(near(camera_mode::SnapRealign::tapSecondsOf(v.snapTapMs), .25f), "the window in seconds");
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
        check(v.handoffGlideCmS == 350 && v.handoffGlideMaxMs == 450 && v.handoffEaseCurve == 1, "defaults 3.5 m/s, 450 ms, gentle (Nov 18)");
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
        check(v.handoffHeadLeads && v.handoffAngleMs == 200, "defaults on, 200 ms (Nov 18)");
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
        check(v.landingBlendMs == 200 && v.landingBlendMs == v.airViewEaseMs, "landing blend 200 ms, as the air view ease (Nov 18)");
        check(hybrid_handoff::oneBlendNs(true, v.landingBlendMs) == 200'000'000, "200 ms blend");
    }
    // HEAD SEAT GLIDE: the glide runs its own length.
    {
        const vr_settings::Values v{};
        check(v.headSeatGlideMs == 165, "head seat glide 165 ms (Nov 18)");
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
    // INSTANT WEB ZIP: a zip pressed while the swing has him goes to the animation on the press; off, VR waits.
    {
        using namespace hybrid_owner;
        check(vr_settings::Values{}.webZipInstant, "instant web zip on by default");
        const auto press = [](bool instant) {
            HybridOwner o;
            Frame f{};
            f.state.playing = f.state.fresh = true;
            int64_t t = 0;
            o.domain = Domain::vr;
            f.state.owned = true; // swinging
            f.velY = -2.f;
            o.update(f, t);
            f.zipPress = true;
            f.zipInstant = instant;
            const Domain d = o.update(f, t += 11'000'000);
            return std::pair{d, o.lastEnterAnim};
        };
        const auto on = press(true), off = press(false);
        check(on.first == Domain::anim && on.second == EnterAnim::zip, "on: the zip's animation starts on the press");
        check(off.first == Domain::vr, "off: SpidyVR waits for the game");
        // A press the game never takes hands the swing back once the ask is over.
        HybridOwner o;
        Frame f{};
        f.state.playing = f.state.fresh = true;
        int64_t t = 0;
        o.domain = Domain::vr;
        f.state.owned = true;
        f.velY = -2.f;
        o.update(f, t);
        f.zipPress = f.zipInstant = true;
        o.update(f, t += 11'000'000);
        f.zipPress = false;
        Domain d = Domain::anim;
        for (int i = 0; i < 60 && d == Domain::anim; ++i)
            d = o.update(f, t += 11'000'000);
        check(d == Domain::vr, "a refused instant zip gives the swing back");
    }
    // HANDOFF PLACE (the owner, Nov 17): a long way onto the head is placed at once; a short one glides as before.
    {
        using namespace hybrid_handoff;
        const vr_settings::Values v{};
        check(v.handoffPlaceCm == 125, "the default: placed past 1.25 m (Nov 18)");
        auto g = landingGlide(46.76f, 250, 200, 700, 200, peakSmoothstep);
        check(g.place && g.ms == glideMs(2.f, 250, 200, 700, peakSmoothstep),
              "47 m (b318cb1's zip): placed, never a 67 m/s slide");
        // HANDOFF SOFT PLACE: the far part at once, the last placeCm glides.
        check(std::abs(g.placed - 44.76f) < 1e-3f, "47 m: 44.76 m placed, the last 2 m glide");
        g = landingGlide(43.62f, 200, 350, 450, 125, peakSmoothstep);
        check(g.place && std::abs(g.placed - 42.37f) < 1e-3f && g.ms == glideMs(1.25f, 200, 350, 450, peakSmoothstep),
              "the 2 AM log's 43.6 m at today's defaults: 42.4 m placed, 1.25 m glide");
        g = landingGlide(2.5f, 250, 200, 700, 200, peakSmoothstep);
        check(g.place, "2.5 m: placed");
        g = landingGlide(.6f, 250, 200, 700, 200, peakSmoothstep);
        check(!g.place && g.ms == glideMs(.6f, 250, 200, 700, peakSmoothstep), "0.6 m glides as before");
        g = landingGlide(1.9f, 250, 200, 700, 200, peakSmoothstep);
        check(!g.place && g.ms == 700, "under the threshold: the longest glide");
        g = landingGlide(46.f, 250, 200, 700, 0, peakSmoothstep);
        check(!g.place && g.ms == 700, "0: always glide (the old way)");
        g = landingGlide(NAN, 250, 200, 700, 200, peakSmoothstep);
        check(!g.place && g.ms == 250 && g.placed == 0, "a bad distance: the base, no place");
        // The source is the shown view carried with the player, not the tracked eyes.
        const Vec3 eyes[2] = {{-.03f, 1.6f, 0}, {.03f, 1.6f, 0}};
        const Vec3 s = shownSource(eyes, {0, 0, 0}, {5, 0, 0});
        check(near(s.x, 5.f) && near(s.y, 1.6f) && near(s.z, 0.f), "carried 5 m with the player");
        const Vec3 far = shownSource(eyes, {0, 0, 0}, {30, 0, 0});
        check(near(far.x, 0.f), "past the carry limit: not carried (a teleport)");
    }
    // A landing glide stretched once the head's first frame tells its distance (Nov 18): no step back.
    {
        using namespace hybrid_handoff;
        const int64_t ms = 1'000'000;
        // The old way: same start, longer length - the view went back by s(28/250) - s(28/700) of the way.
        const float before = blendAlphaFor(28 * ms, 250 * ms), oldAfter = blendAlphaFor(28 * ms, 700 * ms);
        check(before - oldAfter > .02f, "the old stretch stepped the view back (3% of the way at 28 ms)");
        const auto c = stretchKeepingProgress(1000 * ms + 28 * ms, 1000 * ms, 250 * ms, 700 * ms);
        check(c.durNs == 700 * ms && near(blendAlphaFor(1028 * ms - c.startNs, c.durNs), before, 1e-4f),
              "stretched at the same progress: the view stays where it was");
        check(blendAlphaFor(1028 * ms + 11 * ms - c.startNs, c.durNs) > before, "and keeps going onto the head");
        const auto late = stretchKeepingProgress(1000 * ms + 278 * ms, 1000 * ms, 250 * ms, 700 * ms);
        check(late.startNs == 1000 * ms && late.durNs == 250 * ms, "a blend already done (278 ms in) is not restarted");
        const auto shorter = stretchKeepingProgress(1010 * ms, 1000 * ms, 250 * ms, 200 * ms);
        check(shorter.durNs == 250 * ms && shorter.startNs == 1000 * ms, "never shortened");
        const auto atOnce = stretchKeepingProgress(1000 * ms, 1000 * ms, 250 * ms, 700 * ms);
        check(atOnce.startNs == 1000 * ms && atOnce.durNs == 700 * ms, "at the start: just longer");
    }
    // ARM LINE-UP THROUGH THE HANDOFF (the owner, Nov 17): in the landing blend's WORLD frame a hand goes through the
    // shown eyes, carried with the player as the render carries them; a source change's move is measured.
    {
        arm_frame::ViewMap m;
        m.valid = true;
        m.reference = {{0, 1.6f, 0}, {}};
        Pose eyes{{10, 50, -700}, {}};
        Mat4 a{};
        check(arm_frame::worldAnchor(animated_view::Frame::world, a, {}, eyes, m.anchor), "the world anchor: the eyes");
        const Pose hand{{.3f, 1.2f, -.4f}, {}};
        const Vec3 h0 = arm_frame::toGame(m, hand).position;
        eyes.position = eyes.position + hybrid_handoff::travel({0, 0, 0}, {2, 0, 0});
        arm_frame::worldAnchor(animated_view::Frame::world, a, {}, eyes, m.anchor);
        const Vec3 h1 = arm_frame::toGame(m, hand).position;
        check(near(h1.x - h0.x, 2.f) && near(h1.y, h0.y) && near(h1.z, h0.z), "carried 2 m: the hand 2 m with it");
        aim_watch::Lineup lu;
        auto o = lu.update(0, true, {0, 1, 0}, {0, 0, -1}, {0, 1, 0}, {0, 0, -1});
        check(!o.changed, "the first frame: no change");
        o = lu.update(3, true, {0, 1, 0}, {0, 0, -1}, {0, 1, 0}, {0, 0, -1});
        check(o.changed && o.from == 0 && o.to == 3 && near(o.jumpM, 0.f), "rig to the shown view, no move");
        o = lu.update(1, true, {.4f, 1, 0}, {0, 0, -1}, {.05f, 1, 0}, {0, 0, -1});
        check(o.changed && near(o.jumpM, .35f, 1e-3f), "a 0.4 m jump with the controller 0.05 m: 0.35 m beyond");
        aim_watch::MarkerJumps mj;
        mj.update(true, {0, 0, -1}, {0, 0, -1}, 6.f);
        const float r = 20.f * 3.14159265f / 180.f;
        check(!mj.update(true, {std::sin(r), 0, -std::cos(r)}, {std::sin(r), 0, -std::cos(r)}, 6.f),
              "the line turning with the controller: no jump");
        check(mj.update(true, {0, std::sin(r), -std::cos(r)}, {std::sin(r), 0, -std::cos(r)}, 6.f) && mj.jumps == 1,
              "the line turning 28 deg with the controller still: a jump");
        check(mj.frames == 2 && mj.worst > 20.f, "frames and the worst counted");
        mj.clearWindow();
        check(mj.jumps == 0 && mj.frames == 0, "the window clears");
    }
    // MARKER TWIST STEADY (the owner, Nov 17): a twist of the hand no longer swings the arm line round the controller.
    {
        const vr_settings::Values v{};
        check(v.markerTwistSteady && v.markerTurnRateDps == 360 && v.markerSwitchHoldMs == 150,
              "the defaults: on, 360 deg/s, 150 ms");
        const float d2r = 3.14159265f / 180.f;
        const float dt = 1.f / 90.f;
        // The forearm 45 degrees right of the controller's line, fixed in the world; the hand twists 120 degrees about
        // its own line in 0.1 s (9 frames), then holds.
        const Vec3 forearm{std::sin(45 * d2r), 0, -std::cos(45 * d2r)};
        const auto lineOf = [](const web_aim::ForearmAim& a, Quat aim) {
            return (aim * a.turn(100)).rotate({0, 0, -1});
        };
        web_aim::ForearmAim world, local;
        float worstWorld = 0, worstLocal = 0;
        for (int f = 0; f < 80; ++f) { // 40 frames to fade in, then the twist
            const float roll = std::clamp(f - 40, 0, 9) / 9.f * 120.f * d2r;
            const Quat aim = Quat::around({0, 0, -1}, roll);
            world.updateWorld(true, forearm, aim, dt, 360.f);
            const Vec3 l = normalized(aim.conjugate().rotate(forearm));
            local.update(true, arc({0, 0, -1}, l), dt, 180.f);
            if (f >= 40) {
                worstWorld = std::max(worstWorld, std::acos(std::clamp(dot(lineOf(world, aim), forearm), -1.f, 1.f)) / d2r);
                worstLocal = std::max(worstLocal, std::acos(std::clamp(dot(lineOf(local, aim), forearm), -1.f, 1.f)) / d2r);
            }
        }
        check(worstWorld < 2.f, "twist steady: the line stays on the forearm through a 120 deg twist");
        check(worstLocal > 30.f, "the old smoothing swung it round the controller (> 30 deg)");
        // Pointing: the controller and the forearm turn together 60 degrees in 0.1 s: the line follows 1:1.
        web_aim::ForearmAim point;
        float lagMax = 0;
        for (int f = 0; f < 60; ++f) {
            const float yaw = std::clamp(f - 40, 0, 9) / 9.f * 60.f * d2r;
            const Quat aim = Quat::around({0, 1, 0}, -yaw);
            const Vec3 fa = aim.rotate(Vec3{std::sin(45 * d2r), 0, -std::cos(45 * d2r)});
            point.updateWorld(true, fa, aim, dt, 360.f);
            if (f >= 40)
                lagMax = std::max(lagMax, std::acos(std::clamp(dot(lineOf(point, aim), fa), -1.f, 1.f)) / d2r);
        }
        check(lagMax < 1.f, "pointing passes 1:1 (no lag from the smoothing)");
        // A source switch: the line wants 60 degrees elsewhere at once; it goes at most 360 deg/s.
        web_aim::ForearmAim cap;
        cap.updateWorld(true, {0, 0, -1}, {}, dt, 360.f);
        const Vec3 away{std::sin(60 * d2r), 0, -std::cos(60 * d2r)};
        float worstStep = 0;
        for (int f = 0; f < 30; ++f) {
            cap.updateWorld(true, away, {}, dt, 360.f);
            worstStep = std::max(worstStep, cap.lastStepDegrees);
        }
        check(worstStep <= 360.f * dt + .01f, "a 60 deg switch: at most 4 deg a frame at 360 deg/s");
        // Straight back from the controller: the last turn is kept (no flip about an undefined axis).
        web_aim::ForearmAim back;
        back.updateWorld(true, {std::sin(150 * d2r), 0, -std::cos(150 * d2r)}, {}, dt, 0.f);
        Quat atEdge{};
        bool edge = false, kept = true;
        for (int f = 0; f < 200; ++f) {
            back.updateWorld(true, {std::sin(179 * d2r), 0, -std::cos(179 * d2r)}, {}, dt, 0.f);
            const float a = std::acos(std::clamp(dot(back.worldLine, Vec3{0, 0, -1}), -1.f, 1.f)) / d2r;
            if (a > 170.5f) {
                if (!edge)
                    atEdge = back.smoothed, edge = true;
                else
                    kept = kept && near(back.smoothed.x, atEdge.x) && near(back.smoothed.w, atEdge.w);
            }
        }
        check(edge && kept, "within 10 deg of straight back: the last turn is kept");
        // SWITCH HOLD: a flip back within 150 ms is ignored; one that holds is taken.
        web_aim::SwitchHold h;
        check(!h.update(false, dt, .15f), "starts off");
        for (int f = 0; f < 9; ++f)
            h.update(true, dt, .15f);
        check(!h.value, "0.1 s of the new answer: not yet");
        h.update(false, dt, .15f);
        for (int f = 0; f < 9; ++f)
            h.update(true, dt, .15f);
        check(!h.value, "a flip back restarts the wait");
        for (int f = 0; f < 6; ++f)
            h.update(true, dt, .15f);
        check(h.value, "held 150 ms: taken");
        check(h.update(false, dt, 0.f) == false, "0: at once");
    }
    // SNAP TAP WINDOW 120 -> 250 ms (the owner, Nov 18): revision 7 moves an untuned 120 once; a tuned value stays.
    {
        check(vr_settings::Values{}.movementDefaults == vr_settings::movementDefaultsRevision, "the revision");
        vr_settings::Values old;
        old.snapTapMs = 120;
        check(vr_settings::withMovementDefaults(old, "movement_defaults=6\n").snapTapMs == 250, "120 becomes 250");
        check(vr_settings::withMovementDefaults(old, "").snapTapMs == 250, "an old file with no revision: 250");
        old.snapTapMs = 180;
        check(vr_settings::withMovementDefaults(old, "movement_defaults=6\n").snapTapMs == 180, "a tuned 180 stays");
        old.snapTapMs = 120;
        check(vr_settings::withMovementDefaults(old, "movement_defaults=7\n").snapTapMs == 120,
              "120 chosen after revision 7 stays");
        check(vr_settings::withTabOnly({}, "snap_tap_ms=250\n").snapTapMs == 250, "the ini key reads");
    }
    // THE HANDOFF TUNE (the owner, Nov 18: "Change the handoff defaults"): revision 8 moves untuned values once.
    {
        using vr_settings::Values;
        check(vr_settings::movementDefaultsRevision >= 8, "revision 8");
        const Values fresh{};
        check(fresh.headHeightFadeMs == 275 && fresh.handoffGlideCmS == 350 && fresh.handoffGlideMaxMs == 450 &&
                  fresh.handoffPlaceCm == 125 && fresh.landingBlendMs == 200 && fresh.handoffAngleMs == 200 &&
                  fresh.airViewEaseMs == 200 && fresh.headSeatGlideMs == 165 && fresh.jumpHandoffMs == 250,
              "the new defaults");
        Values old;
        old.headHeightFadeMs = 500, old.handoffGlideCmS = 200, old.handoffGlideMaxMs = 700, old.handoffPlaceCm = 200;
        old.landingBlendMs = 250, old.handoffAngleMs = 300, old.airViewEaseMs = 250, old.headSeatGlideMs = 250;
        old.jumpHandoffMs = 300;
        const Values m = vr_settings::withMovementDefaults(old, "movement_defaults=7\n");
        check(m.headHeightFadeMs == 275 && m.handoffGlideCmS == 350 && m.handoffGlideMaxMs == 450 && m.handoffPlaceCm == 125 &&
                  m.landingBlendMs == 200 && m.handoffAngleMs == 200 && m.airViewEaseMs == 200 && m.headSeatGlideMs == 165 &&
                  m.jumpHandoffMs == 250,
              "every old default moves");
        Values tuned = old;
        tuned.handoffGlideCmS = 260, tuned.jumpHandoffMs = 400;
        const Values t = vr_settings::withMovementDefaults(tuned, "movement_defaults=7\n");
        check(t.handoffGlideCmS == 260 && t.jumpHandoffMs == 400 && t.handoffGlideMaxMs == 450, "tuned values stay, the rest move");
        check(vr_settings::withMovementDefaults(old, "movement_defaults=8\n").jumpHandoffMs == 300, "chosen after revision 8 stays");
    }
    // HAND SOURCE EASE (the 2 AM log: 2,839 marker jumps, the worst 175 degrees): a source change's step glides out.
    {
        aim_watch::SourceEase e;
        const Quat turned = Quat::yaw(1.5f);
        e.begin({1, 1.4f, -.3f}, turned, {0, 1.4f, -.3f}, Quat{});
        check(e.active(), "a 1 m, 86 degree step: eased");
        Vec3 p{0, 1.4f, -.3f};
        Quat q{};
        e.apply(0.f, p, q);
        check(std::abs(p.x - 1.f) < 1e-4f && std::abs(q.y - turned.y) < 1e-3f, "the first frame: where it was shown");
        p = {0, 1.4f, -.3f};
        q = {};
        e.apply(.075f, p, q);
        check(p.x > .4f && p.x < .6f, "half way through: half the step left");
        p = {0, 1.4f, -.3f};
        q = {};
        e.apply(.1f, p, q);
        check(p.x == 0.f && !e.active(), "after 150 ms: the new source alone");
        e.begin({10, 0, 0}, Quat{}, {0, 0, 0}, Quat{});
        check(!e.active(), "a 10 m step is a teleport: it snaps");
    }
    // Revision 14 (the owner, Nov 19: "leave all features on for further testing"): features that were on in
    // the owner's session and off by default come on once. A 0 chosen after revision 14 stays. Diagnostics stay off.
    {
        using vr_settings::Values;
        using vr_settings::withMovementDefaults;
        using vr_settings::withTabOnly;
        using vr_settings::movementDefaultsRevision;
        const std::string rev13 =
            "showtime=0\nshowtime_auto_finish=0\nshowtime_auto_dodge=0\nhand_grab=0\nkicks=0\n"
            "physical_tethers=0\nwall_brake=0\ngrip_fling=0\nair_landing_release=0\npoint_jump=0\n"
            "sound_trace=0\nhead_probe=0\nrun_logs=0\nmovement_defaults=13\n";
        const Values moved = withMovementDefaults(withTabOnly({}, rev13), rev13);
        check(moved.showtime && moved.showtimeAutoFinish && moved.showtimeAutoDodge == 1 && moved.handGrab &&
                  moved.kicks && moved.physicalTethers && moved.wallBrake && moved.gripFling &&
                  moved.airLandingRelease && moved.pointJump && !moved.soundTrace && !moved.headProbe &&
                  !moved.runLogs && moved.movementDefaults == movementDefaultsRevision,
              "revision 14 turns the session's features on, diagnostics stay off");
        const std::string rev14 =
            "showtime=0\nkicks=0\nhand_grab=0\npoint_jump=0\nshowtime_auto_dodge=0\nmovement_defaults=14\n";
        const Values kept = withMovementDefaults(withTabOnly({}, rev14), rev14);
        check(!kept.showtime && !kept.kicks && !kept.handGrab && !kept.pointJump && kept.showtimeAutoDodge == 0,
              "a 0 chosen after revision 14 stays");
    }
    std::printf("next pass: %d checks passed\n", passed);
    return 0;
}
