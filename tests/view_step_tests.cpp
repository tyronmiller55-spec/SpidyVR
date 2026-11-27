// VIEW STEP (view_step.hpp, beta3q): the measure (the headset's own motion and the anchor's are not steps; a jump, a
// blend's leftover and a switch's step are), the smoothing (the spring settles, never faster than its speed, the
// comfort cap while a handoff is under way, a teleport let through, the switch's speed carried), the jump log's
// account, the head-axes round trip, and the switches' defaults.
#include "spidy/view_step.hpp"
#include "spidy/animated_view.hpp"
#include "spidy/native_eye_frame.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::view_step;
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
    return std::abs(a - b) <= e;
}
constexpr float dt = 1.f / 90.f;
Pose tracked(float t) { // a head looking about and leaning
    return {{.1f * std::sin(t * 2.f), 1.6f + .05f * std::sin(t * 3.f), .1f * std::cos(t * 1.5f)},
            unit(Quat::yaw(.8f * std::sin(t)) * Quat::around({1, 0, 0}, .3f * std::sin(t * 1.3f)))};
}
Pose rig(float t) { // a rig travelling at 12 m/s and turning (SpidyVR's own motion)
    return {{12.f * t, 2.f * std::sin(t), -3.f * t}, unit(Quat::yaw(.5f * t) * Quat::around({0, 0, 1}, .4f * std::sin(t)))};
}
Frame frameAt(float t, Pose anchor, Pose shown, Pose head, uint32_t source = 1) {
    Frame f;
    f.ok = f.anchorOk = f.trackedOk = true;
    f.anchor = anchor;
    f.shown = shown;
    f.tracked = head;
    f.source = source;
    f.dt = dt;
    (void)t;
    return f;
}
} // namespace

int main() {
    // The headset and the anchor (travel, SpidyVR's turns, an animated head's turn) are not steps.
    {
        Measure m;
        float worst = 0, worstDeg = 0;
        bool allMeasured = true;
        for (int i = 0; i < 300; ++i) {
            const float t = i * dt;
            const Pose a = rig(t), h = tracked(t);
            const auto s = m.step(frameAt(t, a, compose(a, h), h));
            if (i > 0)
                allMeasured = allMeasured && s.measured && !s.cut;
            worst = std::max(worst, s.metres);
            worstDeg = std::max(worstDeg, s.deg);
        }
        check(allMeasured, "measured every frame, no cut");
        check(worst < 1e-3f && worstDeg < .05f, "the rig and the headset: no step");
    }
    // A constant offset on the head (the animation's head lock: eyes relative to the bone) is not a step either.
    {
        Measure m;
        const Pose k{{0, -.05f, .08f}, Quat::yaw(.3f)};
        float worst = 0;
        for (int i = 0; i < 200; ++i) {
            const float t = i * dt;
            const Pose a{{3.f * t, 1.f + .3f * std::sin(5 * t), 0}, unit(Quat::around({1, 0, 0}, 2.f * t))}; // a flip
            const Pose h = tracked(t);
            const auto s = m.step(frameAt(t, a, compose(a, compose(k, h)), h));
            worst = std::max(worst, s.metres + s.deg * .001f);
        }
        check(worst < 2e-3f, "a head-locked view through a flip: no step");
    }
    // A 30 cm jump on one frame is a step of 30 cm; the next frame is quiet again.
    {
        Measure m;
        Step s;
        for (int i = 0; i < 20; ++i) {
            const float t = i * dt;
            Pose shown = compose(rig(t), tracked(t));
            if (i >= 10)
                shown.position += Vec3{.3f, 0, 0};
            s = m.step(frameAt(t, rig(t), shown, tracked(t)));
            if (i == 10)
                check(near(s.metres, .3f, 2e-3f) && !s.cut, "the jump measured");
            if (i == 11)
                check(s.metres < 5e-3f, "quiet after it (a world offset under a turning rig: millimetres)");
        }
        check(s.metres < 5e-3f, "quiet at the end");
    }
    // A switch (a new source) expects the view to go on as it was going: a stop is a step of the speed's frame.
    {
        Measure m;
        Step s;
        const Pose h = tracked(0);
        for (int i = 0; i < 10; ++i) {
            const Pose a{{10.f * i * dt, 0, 0}, {}};
            s = m.step(frameAt(0, a, compose(a, h), h, 1));
        }
        const Pose stop{{10.f * 9 * dt, 0, 0}, {}}; // the new anchor where the old one was, not moving on
        s = m.step(frameAt(0, stop, compose(stop, h), h, 2));
        check(s.cut && near(s.metres, 10.f * dt, 2e-3f), "the switch: the speed's frame left over");
        s = m.step(frameAt(0, stop, compose(stop, h), h, 2));
        check(s.seed && near(s.seedVelocity.x, 10.f, .05f) && s.metres < 1e-3f, "the next frame seeds the old speed");
    }
    // The anchor jumping on its own (a placed rig) is a cut too.
    {
        Measure m;
        Step s;
        const Pose h = tracked(0);
        for (int i = 0; i < 5; ++i)
            s = m.step(frameAt(0, {{0, 0, 0}, {}}, compose({{0, 0, 0}, {}}, h), h));
        const Pose jumped{{1.f, 0, 0}, {}};
        s = m.step(frameAt(0, jumped, compose(jumped, h), h));
        check(s.cut && s.anchorJump && near(s.metres, 1.f, 2e-3f), "an anchor jump of 1 m: a cut, 1 m left over");
    }
    // The player's own turn applied on the view (not via the anchor) is not a step; a cut is not measured.
    {
        Measure m;
        const Pose h = tracked(0), a{};
        m.step(frameAt(0, a, compose(a, h), h));
        Frame f = frameAt(0, a, compose({{}, Quat::yaw(.2f)}, compose(a, h)), h);
        f.shown.position = compose(a, h).position; // turned in place
        f.relTurn = .2f;
        const auto s = m.step(f);
        check(s.measured && s.deg < .05f, "the player's own turn: no step");
        Frame g = f;
        g.shown.position += Vec3{2, 0, 0};
        g.playerCut = true;
        check(!m.step(g).measured, "a recenter or a snap: not measured");
    }

    // SMOOTHER: an absorbed 30 cm step reaches the eyes at most 2 cm a frame, settles, and never above 1.5 m/s.
    {
        const Params p{};
        check(near(p.settleSeconds, .3f) && near(p.maxSpeed, 1.5f) && near(p.capM, .02f) && near(p.capRad, degrees),
              "the plan's numbers: 300 ms, 1.5 m/s, 2 cm, 1 deg");
        Smoother sm;
        Step jump;
        jump.measured = true;
        jump.pos = {.3f, 0, 0};
        jump.metres = .3f;
        auto o = sm.update(dt, jump, true, p);
        check(o.capped && near(o.shownM, .02f, 1e-4f), "capped to 2 cm on the jump's frame");
        check(near(o.offset.x, -.28f, 1e-3f), "the rest held as the offset");
        Step quiet;
        quiet.measured = true;
        float worstStep = 0, t = 0;
        Vec3 last = o.offset;
        while (t < 1.f) {
            o = sm.update(dt, quiet, true, p);
            worstStep = std::max(worstStep, length(o.offset - last));
            last = o.offset;
            t += dt;
        }
        check(worstStep <= 1.5f * dt + 1e-4f, "never faster than 1.5 m/s");
        check(length(o.offset) < .01f, "settled within a second");
        // Settling time: the plan's 300 ms is for the spring (95%); the speed cap stretches a 28 cm one.
        Smoother small;
        Step s5;
        s5.measured = true;
        s5.pos = {.05f, 0, 0};
        small.update(dt, s5, true, p);
        float tt = 0;
        while (length(small.offset()) > .05f * .03f && tt < 2.f) {
            small.update(dt, quiet, true, p);
            tt += dt;
        }
        check(tt > .25f && tt < .45f, "a 3 cm offset settles in about 300 ms");
    }
    // Rotation: a 20 degree step is shown 1 degree a frame at most, then settles.
    {
        const Params p{};
        Smoother sm;
        Step turn;
        turn.measured = true;
        turn.rot = {0, 20.f * degrees, 0};
        auto o = sm.update(dt, turn, true, p);
        check(o.capped && near(o.shownDeg, 1.f, 1e-3f), "1 degree on the step's frame");
        Step quiet;
        quiet.measured = true;
        float worst = 0;
        for (int i = 0; i < 180; ++i) {
            o = sm.update(dt, quiet, false, p);
            worst = std::max(worst, o.shownDeg);
        }
        check(worst <= 1.f + 1e-3f && length(o.turn) < .2f * degrees, "decays at most 1 degree a frame, settled");
    }
    // Outside a handoff the step passes as it is (SpidyVR's own air); a teleport passes and drops the offset.
    {
        const Params p{};
        Smoother sm;
        Step jump;
        jump.measured = true;
        jump.pos = {.3f, 0, 0};
        auto o = sm.update(dt, jump, false, p);
        check(!o.capped && near(o.shownM, .3f, 1e-4f) && length(o.offset) == 0.f, "no window: not capped");
        sm.update(dt, jump, true, p);
        Step far;
        far.measured = true;
        far.pos = {5.f, 0, 0};
        o = sm.update(dt, far, true, p);
        check(o.teleport && length(o.offset) == 0.f && !sm.active(), "a 5 m teleport passes, the offset dropped");
    }
    // The switch's speed carried: seeded, the offset moves on the old way before it comes back.
    {
        const Params p{};
        Smoother sm;
        Step seed;
        seed.measured = true;
        seed.seed = true;
        seed.seedVelocity = {10.f, 0, 0};
        auto o = sm.update(dt, seed, true, p);
        check(o.offset.x > 0 && o.offset.x <= 1.5f * dt + 1e-4f, "seeded with the old speed, capped at 1.5 m/s");
        Step quiet;
        quiet.measured = true;
        for (int i = 0; i < 120; ++i)
            o = sm.update(dt, quiet, true, p);
        check(std::abs(o.offset.x) < .005f, "and settles");
    }
    check(near(paramsOf(300, 150, 20, 100).capM, .02f, 1e-6f) && near(paramsOf(5000, 1, 1, 1).settleSeconds, 1.f),
          "paramsOf clamps");

    // The jump log: over 2 cm or 1 degree, at most 8 lines a second, a count once a second.
    {
        JumpLog log;
        check(!log.add(.01f, .5f, 0, false), "under the lines: no jump");
        int lines = 0;
        for (int i = 0; i < 20; ++i)
            lines += log.add(.05f, 0, blend, false) ? 1 : 0;
        check(lines == 8 && log.jumps == 20, "8 lines, 20 counted");
        bool due = false;
        for (int i = 0; i < 95; ++i)
            due = log.tick(dt) || due;
        check(due && log.worstCause == blend, "the count is due, with the worst's cause");
        log.clear();
        check(log.add(.05f, 0, 0, false), "lines again after the second");
        char text[96];
        causeList(ownerSwitch | landingSnap | blend, text, sizeof(text));
        check(std::string(text) == "switch+snap+blend", "the cause list");
        check(std::string(causeName(blend | landingSnap)) == "landing view snap", "the cause's name");
    }

    // The head-axes round trip, and onHead is the anchor composed with the eye.
    {
        const Pose head{{1, 2, 3}, unit(Quat::yaw(.7f) * Quat::around({1, 0, 0}, .4f))};
        const Mat4 headM = animated_view::toNative(head);
        const Pose eye{{.03f, -.02f, .05f}, unit(Quat::yaw(-.2f))};
        const Mat4 axes = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const Mat4 rel = native_view::relativePose(axes, eye);
        const Mat4 world = native_eyes::onHead(rel, headM);
        const Mat4 back = offHead(world, headM);
        float err = 0;
        for (int i = 0; i < 16; ++i)
            err = std::max(err, std::abs(back[i] - rel[i]));
        check(err < 1e-5f, "offHead undoes onHead");
        const Pose composed = compose(animated_view::fromNative(headM), eye);
        const Pose resolved = animated_view::fromNative(world);
        check(length(composed.position - resolved.position) < 1e-5f &&
                  length(toRotVec(unit(composed.orientation * resolved.orientation.conjugate()))) < 1e-4f,
              "onHead is the head composed with the eye");
        const Pose moved = applied(resolved, resolved.position, {0, .1f, 0}, {0, 0, 0});
        check(near(moved.position.y, resolved.position.y + .1f, 1e-5f), "applied moves the eye");
    }

    // The switches: on by default, their own keys, revision 28 turns them on once, a 0 after it stays.
    using namespace spidy::vr_settings;
    check(Values{}.viewJumpLog, "VIEW JUMP LOG on by default");
    const std::string old = "movement_defaults=27\nview_jump_log=0\n";
    check(withMovementDefaults(withTabOnly({}, old), old).viewJumpLog, "revision 28 turns the log on once");
    const std::string kept = "movement_defaults=28\nview_jump_log=0\n";
    check(!withMovementDefaults(withTabOnly({}, kept), kept).viewJumpLog, "a 0 after revision 28 stays");
    check(tabOnlyText(Values{}).find("view_jump_log=1\n") != std::string::npos, "its key in spidy_vr.ini");

    std::printf("view_step: %d checks passed\n", passed);
    return 0;
}
