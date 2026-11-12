// LEG HOLD (leg_hold.hpp, November 12, t67u): the Y+X chord (together, or one
// a few frames before the other within 80 ms), suppression of both buttons on
// press, hold and release, single X / Y passing (late by the window, or as a
// tap), stick-held persistence after release, the neutral cancel with
// hysteresis, the gate, easing; and that LEG TOGGLE's setting is gone. Portable.
#include "spidy/hybrid_owner.hpp"
#include "spidy/leg_hold.hpp"
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
    void seconds(float s, bool x, bool y, float sx = 0, float sy = 0, bool allowed = true, bool master = true) {
        frames(static_cast<int>(std::lround(s / dt)), x, y, sx, sy, allowed, master);
    }
};
} // namespace

int main() {
    // The window: 80-120 ms, chosen 80.
    check(chordSeconds >= .08f && chordSeconds <= .12f, "chord window 80-120 ms");
    check(neutralSeconds >= .05f && neutralSeconds <= .1f && neutralLow < neutralHigh, "neutral hold and hysteresis");
    // Both together: engaged on that frame; neither button reaches the game on
    // the press, through the hold, or on the release.
    {
        Run r;
        r.frames(3, false, false);
        r.frames(1, true, true);
        check(r.engaged && r.engagedNow && r.why == Why::chord && !r.sawX && !r.sawY, "together: engaged, both held back");
        r.seconds(1.f, true, true);
        check(r.engaged && !r.sawX && !r.sawY, "held 1 s (past the wheel's 0.3 s): nothing reaches the game");
        r.frames(1, true, false, .8f, 0);
        r.frames(3, false, false, .8f, 0);
        check(!r.sawX && !r.sawY && r.xFrames == 0 && r.yFrames == 0, "let go one then the other: no release edge, no tap");
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
        check(r.engagedNow && !r.sawX && !r.sawY, first ? "X then Y inside 80 ms: chord" : "Y then X inside 80 ms: chord");
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
        check(r.yFrames >= 3 && r.yFrames <= static_cast<int>(tapSeconds / dt) + 1, "the tap lasts about 50 ms");
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
        check(!r.engaged && r.disengagedNow && r.why == Why::neutral, "neutral held ~80 ms: disengaged, reason neutral");
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
        check(!r.engaged && r.why == Why::gate && !r.sawX && !r.sawY, "gate: disengaged, buttons still held back");
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
    // Flip break: right-stick pitch breaks only with the legs engaged; turns never.
    {
        hybrid_owner::FlipInput in;
        in.rightValid = true;
        in.rightY = .9f;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::none, "legs off: no right-stick break");
        in.legs = true;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::rightPitch, "legs engaged: pitch breaks");
        in.rightY = 0;
        in.rightX = 1;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::none, "a turn never breaks");
    }
    // LEG TOGGLE is gone: no row, no key; an old leg_toggle_input line is ignored.
    {
        using namespace vr_settings;
        for (const auto& row : rows())
            check(std::strcmp(row.title, "LEG TOGGLE") != 0, "no LEG TOGGLE row");
        Values v;
        check(tabOnlyText(v).find("leg_toggle_input") == std::string::npos, "no leg_toggle_input key written");
        Values read = withTabOnly(v, "leg_toggle_input=1\nleg_control=1\n");
        check(read.legControl && tabOnlyText(read).find("leg_toggle_input") == std::string::npos,
              "an old leg_toggle_input line is ignored, the rest read");
    }
    std::printf("leg hold: %d checks passed\n", passed);
    return 0;
}
