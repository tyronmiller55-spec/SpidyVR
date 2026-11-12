// LEG CONTROL (leg_control.hpp, leg_hold.hpp): the Y+X hold (together, or one
// a few frames before the other within 80 ms), both buttons held back on
// press, hold and release, a lone X or Y passing (late by the window, or as a
// tap), the legs kept by the stick after letting go, the neutral cancel with
// hysteresis, the gate and the easing; the stick's pose blend, the loose legs'
// spring staying finite and in range, the inertia and the reaction; REALISTIC
// FLIPS keeping angular momentum as the legs tuck; the settings. Portable.
#include "spidy/leg_control.hpp"
#include "spidy/leg_hold.hpp"
#include "spidy/realistic_flip.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::leg_hold;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float dt = 1.f / 72;
struct Run {
    Hold h;
    bool sawX{}, sawY{}, engaged{}, engagedNow{}, disengagedNow{};
    Why why = Why::none;
    int xFrames{}, yFrames{};
    // n frames of this input; records what reached the game.
    void frames(int n, bool x, bool y, float sx = 0, float sy = 0, bool allowed = true, bool master = true) {
        sawX = sawY = engagedNow = disengagedNow = false;
        for (int i = 0; i < n; ++i) {
            Hold::Input in;
            in.seconds = dt;
            in.x = x;
            in.y = y;
            in.stickX = sx;
            in.stickY = sy;
            in.allowed = allowed;
            in.master = master;
            const auto o = h.update(in);
            sawX = sawX || o.x;
            sawY = sawY || o.y;
            xFrames += o.x;
            yFrames += o.y;
            engaged = o.engaged;
            if (o.engagedNow)
                engagedNow = true, why = o.why;
            if (o.disengagedNow)
                disengagedNow = true, why = o.why;
        }
    }
    void seconds(float s, bool x, bool y, float sx = 0, float sy = 0, bool allowed = true,
                 bool master = true) {
        frames(static_cast<int>(std::lround(s / dt)), x, y, sx, sy, allowed, master);
    }
};
} // namespace

int main() {
    // The window: 80-120 ms, chosen 80.
    check(chordSeconds >= .08f && chordSeconds <= .12f, "chord window 80-120 ms");
    check(neutralSeconds >= .05f && neutralSeconds <= .1f && neutralLow < neutralHigh,
          "neutral hold and hysteresis");
    // Both together: engaged on that frame; neither button reaches the game on
    // the press, through the hold, or on the release.
    {
        Run r;
        r.frames(3, false, false);
        r.frames(1, true, true);
        check(r.engaged && r.engagedNow && r.why == Why::chord && !r.sawX && !r.sawY,
              "together: engaged, both held back");
        r.seconds(1.f, true, true);
        check(r.engaged && !r.sawX && !r.sawY, "held 1 s (past the wheel's 0.3 s): nothing reaches the game");
        r.frames(1, true, false, .8f, 0);
        r.frames(3, false, false, .8f, 0);
        check(!r.sawX && !r.sawY && r.xFrames == 0 && r.yFrames == 0,
              "let go one then the other: no release edge, no tap");
        check(r.engaged, "the stick moved: still engaged");
    }
    // One a few frames before the other: Y first then X 50 ms later, X first
    // then Y 70 ms later: a chord, nothing passes.
    for (int first = 0; first < 2; ++first) {
        Run r;
        r.frames(2, false, false);
        r.seconds(first ? .07f : .05f, first == 1, first == 0);
        check(!r.sawX && !r.sawY && !r.engaged, "the first waits for the other");
        r.frames(1, true, true);
        check(r.engagedNow && !r.sawX && !r.sawY,
              first ? "X then Y inside 80 ms: chord" : "Y then X inside 80 ms: chord");
        r.seconds(.3f, true, true);
        r.frames(5, false, false);
        check(r.xFrames == 0 && r.yFrames == 0, "no stray zip or gadget press at all");
    }
    // A lone Y: reaches the game 80 ms late and stays as held; a lone X too
    // (the gadget wheel then sees it); never engages.
    {
        Run r;
        r.seconds(.06f, false, true);
        check(!r.sawY, "lone Y held back inside the window");
        r.seconds(.04f, false, true);
        check(r.sawY && !r.engaged, "lone Y past 80 ms: the game's");
        r.seconds(.3f, false, true);
        check(r.sawY, "and stays held");
        r.frames(1, false, false);
        check(!r.sawY, "let go: released");
        Run x;
        x.seconds(.5f, true, false);
        check(x.sawX && x.xFrames >= static_cast<int>((.5f - chordSeconds) / dt) - 1 && !x.engaged,
              "lone X held: the game's after the window (the wheel's hold)");
        // Pressed alone and the other added after the window: no chord.
        x.frames(10, true, true);
        check(!x.engaged && x.sawY, "the other after the window: both the game's");
    }
    // A lone quick tap inside the window reaches the game as a tap.
    {
        Run r;
        r.frames(3, false, true);
        r.frames(1, false, false);
        check(r.sawY && !r.engaged, "a quick Y: a late tap");
        r.seconds(.2f, false, false);
        check(r.yFrames >= 3 && r.yFrames <= static_cast<int>(tapSeconds / dt) + 1,
              "the tap lasts about 50 ms");
    }
    // Not allowed (on the ground, a prompt, LEG CONTROL off): X and Y pass at once.
    {
        Run r;
        r.frames(1, true, true, 0, 0, false);
        check(r.sawX && r.sawY && !r.engaged, "not allowed: both the game's at once");
        Run y;
        y.frames(1, false, true, 0, 0, false);
        check(y.sawY, "not allowed: lone Y at once");
    }
    // Stick persistence and the neutral cancel.
    {
        Run r;
        r.frames(1, true, true);
        r.seconds(.5f, true, true, 0, 0);
        check(r.engaged, "neutral while Y+X held: still engaged");
        r.frames(1, false, false, .7f, .3f);
        r.seconds(2.f, false, false, -.5f, .6f);
        check(r.engaged && !r.disengagedNow, "let go, stick moved for 2 s: engaged");
        // Noise around the low threshold: between the two thresholds never cancels.
        r.seconds(1.f, false, false, .22f, 0);
        check(r.engaged, "between the thresholds: neither counts nor cancels");
        // Brief dips under neutral shorter than the hold do not cancel.
        for (int k = 0; k < 10; ++k) {
            r.frames(3, false, false, .05f, 0); // ~42 ms
            r.frames(2, false, false, .5f, 0);
        }
        check(r.engaged, "brief dips through neutral: engaged");
        r.seconds(neutralSeconds + 2 * dt, false, false, .05f, 0);
        check(!r.engaged && r.disengagedNow && r.why == Why::neutral,
              "neutral held ~80 ms: disengaged, reason neutral");
        // Pushing again does not re-engage: only the chord engages.
        r.seconds(.5f, false, false, .9f, 0);
        check(!r.engaged, "stick alone never engages");
    }
    // Released with the stick already neutral: disengages after the hold.
    {
        Run r;
        r.frames(1, true, true);
        r.seconds(.2f, true, true);
        r.frames(1, false, false);
        check(r.engaged, "just let go, neutral: not yet");
        r.seconds(.1f, false, false);
        check(!r.engaged && r.why == Why::neutral, "then neutral ends it");
    }
    // The gate ends it at once; the buttons stay held back until let go of.
    {
        Run r;
        r.frames(1, true, true);
        r.frames(5, true, true, .8f, 0, true, false);
        check(!r.engaged && r.why == Why::gate && !r.sawX && !r.sawY,
              "gate: disengaged, buttons still held back");
        r.frames(2, false, false, 0, 0, true, false);
        check(r.xFrames == 0 && r.yFrames == 0, "no release edges after the gate");
    }
    // Easing after a disengage, none while engaged.
    {
        Run r;
        r.frames(1, true, true);
        Hold::Input in;
        in.seconds = dt;
        in.master = in.allowed = true;
        in.x = in.y = true;
        check(!r.h.update(in).easing, "engaged: no easing");
        in.x = in.y = false;
        Hold::Output o;
        for (int i = 0; i < 20 && !o.disengagedNow; ++i)
            o = r.h.update(in);
        check(o.disengagedNow && o.easing, "disengaged: easing");
        for (int i = 0; i < static_cast<int>(easeSeconds / dt) + 2; ++i)
            o = r.h.update(in);
        check(!o.easing, "eased");
    }
    // Non-finite input: no NaN path engages or cancels wrongly.
    {
        Run r;
        r.frames(1, true, true);
        r.frames(1, false, false, NAN, NAN);
        Hold::Input in;
        in.seconds = NAN;
        in.master = in.allowed = true;
        in.stickX = NAN;
        const auto o = r.h.update(in);
        check(std::isfinite(static_cast<float>(o.engaged)), "NaN frame and stick: handled");
    }
    // The stick's pose: centre straight, each half-axis its own pose, continuous, in range.
    {
        using namespace leg_control;
        const Legs centre = blend(0, 0), forward = blend(0, 1), back = blend(0, -1);
        check(std::abs(centre[0].pitch - centrePose[0].pitch) < 1e-6f &&
                  std::abs(centre[1].side - centrePose[1].side) < 1e-6f,
              "centre: straight legs");
        check(std::abs(forward[0].pitch - forwardPose[0].pitch) < 1e-5f && forward[0].pitch > 1.f,
              "forward: a jackknife");
        check(back[0].pitch < 0 && back[0].side > 0 && back[1].side < 0, "back: a rearward splay");
        check(blend(-1, 0)[0].side > centre[0].side && blend(1, 0)[1].side < centre[1].side,
              "left and right: swung");
        check(std::abs(blend(.05f, .05f)[0].pitch - centre[0].pitch) < 1e-6f, "inside the dead zone: centre");
        const Legs a = blend(.5f, .5f), b = blend(.501f, .5f);
        check(std::abs(a[0].side - b[0].side) < .01f, "the blend is continuous");
        const Legs nan = blend(NAN, NAN);
        check(std::isfinite(nan[0].pitch) && std::isfinite(nan[1].knee), "NaN stick: centre");
    }
    // The loose legs: stable and in range for every setting, under wild motion.
    {
        using namespace leg_control;
        for (int stiff : {25, 100, 200})
            for (int damp : {25, 100, 200}) {
                Simulation sim;
                Motion m;
                m.specificForce = {40, -30, 55};
                m.angularVelocity = {15, -12, 18};
                Output o;
                for (int i = 0; i < 720; ++i) {
                    m.angularVelocity.x = i % 2 ? 15.f : -15.f;
                    o = sim.update(dt, true, std::sin(i * .3f), std::cos(i * .2f), m,
                                   fromPercent(stiff, damp, 100));
                }
                bool ok = o.active;
                for (const auto& l : o.legs)
                    ok = ok && std::isfinite(l.pitch) && l.pitch >= pitchMin && l.pitch <= pitchMax &&
                         std::abs(l.side) <= sideMax && l.knee >= 0 && l.knee <= kneeMax;
                ok = ok && std::abs(o.reactionPitch) <= reactionRateMax * dt + 1e-6f &&
                     std::abs(o.reactionRoll) <= reactionRateMax * dt + 1e-6f &&
                     o.spinScale >= spinScaleMin && o.spinScale <= spinScaleMax;
                check(ok, "the legs stay finite, in range and capped under any motion and setting");
            }
        // Pushed forward in still air the legs swing up and the body turns back (nose down).
        Simulation sim;
        float pitch = 0;
        Output o;
        for (int i = 0; i < 36; ++i) {
            o = sim.update(dt, true, 0, 1, {}, fromPercent(100, 100, 100));
            pitch += o.reactionPitch;
        }
        check(o.legs[0].pitch > .5f && pitch < 0, "a jackknife turns the body back, nose down");
        check(spinScale(blend(0, 1), 1) > 1 && spinScale(blend(0, 0), 1) <= 1.01f, "tucked spins faster");
        check(std::abs(spinScale(blend(0, 1), 0) - 1) < 1e-6f, "LEG COUPLING off: no spin change");
        const Output off = sim.update(dt, false, 0, 1, {}, {});
        check(!off.active && off.reactionPitch == 0 && off.spinScale == 1, "inactive: nothing turns");
    }
    // REALISTIC FLIPS: a tuck mid-spin speeds it (angular momentum kept); the legs' counter-turn
    // turns the body without adding spin; on the ground nothing.
    {
        RealisticFlipSettings settings;
        settings.enabled = true;
        const auto spin = [&](float legInertia, Vec3 legTurn, bool airborne) {
            RealisticFlip f;
            RealisticFlight flight;
            flight.valid = true;
            flight.velocity = {0, 0, -10};
            RealisticControls c;
            c.manualActive = true;
            c.manualAngular = {2, 0, 0};
            for (int i = 0; i < 30; ++i)
                f.update(flight, airborne, false, 0, dt, settings, c);
            c.manualActive = false;
            c.manualAngular = {};
            flight.legInertia = legInertia;
            flight.legTurn = legTurn;
            f.update(flight, airborne, false, 0, dt, settings, c);
            return f;
        };
        const float straight = length(spin(1, {}, true).angularVelocity());
        const float tucked = length(spin(.8f, {}, true).angularVelocity());
        check(straight > .5f && tucked > straight * 1.2f,
              "tucked: the spin speeds up by the inertia's change");
        const RealisticFlip turned = spin(1, {0, 0, .05f}, true), plain = spin(1, {}, true);
        check(std::abs(length(turned.angularVelocity()) - length(plain.angularVelocity())) < 1e-4f,
              "the legs' counter-turn adds no spin");
        const Quat a = turned.orientation(), b = plain.orientation();
        check(std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w) < .99999f,
              "the counter-turn turns the body");
        check(length(legTurnVector({}, .05f, 0)) > .049f && length(legTurnVector({}, NAN, NAN)) == 0,
              "the reaction's axes, and none from NaN");
    }
    // The settings: off by default, sanitized, the player's own in spidy_vr.ini.
    {
        using namespace vr_settings;
        Values v;
        check(!v.legControl && v.legStiffnessPct == 100 && v.legDampingPct == 100 && v.legCouplingPct == 50,
              "LEG CONTROL off by default");
        check(inactiveReason(Item::legControl, v) != nullptr,
              "unavailable without Controlled or Realistic flips");
        v.realisticFlips = true;
        check(inactiveReason(Item::legControl, v) == nullptr, "available with Realistic flips");
        v.realisticFlips = false;
        v.controlledFlips = true;
        check(inactiveReason(Item::legControl, v) == nullptr, "available with Controlled flips");
        check(inactiveReason(Item::legStiffness, v) != nullptr, "its rows wait for LEG CONTROL");
        Values read = withTabOnly(v, "leg_control=1\nleg_stiffness_pct=5\nleg_damping_pct=150\n");
        check(read.legControl && read.legStiffnessPct == 25 && read.legDampingPct == 150,
              "read back from spidy_vr.ini, sanitized");
    }
    std::printf("leg control: %d checks passed\n", passed);
    return 0;
}
