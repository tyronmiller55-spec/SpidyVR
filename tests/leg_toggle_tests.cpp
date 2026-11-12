// LEG TOGGLE (leg_toggle.hpp, November 12): the thumb rest tap (deliberate
// taps only), X+Y held (single X / Y untouched, both kept from the game while
// held), the toggle (off each session, on frame, easing back), the setting and
// the Touch layout's thumb rest extra. Portable.
#include "spidy/controller_layout.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/leg_control.hpp"
#include "spidy/leg_toggle.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::leg_toggle;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float dt = 1.f / 90;
// Frames of `touched` for `seconds`; whether any frame tapped.
bool run(ThumbrestTap& t, bool touched, float seconds, float stick = 0, bool click = false) {
    bool any = false;
    for (int i = 0, n = static_cast<int>(std::lround(seconds / dt)); i < n; ++i)
        any = t.update(dt, touched, stick, click) || any;
    return any;
}
int runXY(XYHold& h, bool x, bool y, float seconds, bool& masked) {
    int taps = 0;
    masked = false;
    for (int i = 0, n = static_cast<int>(std::lround(seconds / dt)); i < n; ++i) {
        const auto o = h.update(dt, x, y);
        taps += o.tap;
        masked = masked || o.mask;
    }
    return taps;
}
} // namespace

int main() {
    // A deliberate tap toggles on release; a resting thumb, a brush straight
    // after another touch, a moving stick or a click do not.
    {
        ThumbrestTap t;
        check(!run(t, false, .5f), "idle");
        check(!run(t, true, .12f), "no tap while touched");
        check(t.update(dt, false, 0, false), "a 0.12 s tap toggles on the frame of release");
        check(!run(t, false, .5f), "one tap, one toggle");
        check(!run(t, true, 1.f) && !run(t, false, .1f), "a resting thumb (1 s) is no tap");
        run(t, false, .5f);
        check(!run(t, true, .1f, .8f) && !run(t, false, .1f, .8f), "stick pushed: no tap");
        run(t, false, .5f);
        check(!run(t, true, .1f, 0, true) && !run(t, false, .1f), "stick clicked (slow-mo): no tap");
        run(t, false, .5f);
        run(t, true, .1f);
        check(t.update(dt, false, 0, false), "a tap");
        // A second touch soon after (under idleSeconds) is a brush, not a tap.
        run(t, false, .05f);
        check(!run(t, true, .1f) && !run(t, false, .05f), "a touch straight after another is no tap");
        run(t, false, .5f);
        check(!run(t, true, dt) && !t.update(dt, false, 0, false), "a one-frame flicker is no tap");
        ThumbrestTap nan;
        run(nan, false, .5f);
        check(!run(nan, true, .1f, NAN) && !run(nan, false, .1f), "a non-finite stick is not still");
    }
    // X+Y held 0.2 s toggles once; a single X or Y is never masked; both are
    // masked from the frame both are down until both are let go of.
    {
        XYHold h;
        bool masked = false;
        check(runXY(h, true, false, .6f, masked) == 0 && !masked, "X alone (the wheel's hold) untouched");
        runXY(h, false, false, .1f, masked);
        check(runXY(h, false, true, .3f, masked) == 0 && !masked, "Y alone untouched");
        runXY(h, false, false, .1f, masked);
        check(runXY(h, true, true, .1f, masked) == 0 && masked, "X+Y 0.1 s: masked, no toggle yet");
        check(runXY(h, true, true, .15f, masked) == 1 && masked, "X+Y past 0.2 s: one toggle");
        check(runXY(h, true, true, 1.f, masked) == 0, "held on: no repeat");
        check(runXY(h, true, false, .5f, masked) == 0 && masked, "X still down after: still masked (no wheel)");
        check(runXY(h, false, false, dt, masked) == 0 && !masked, "both up: free");
        check(runXY(h, true, true, .1f, masked) == 0 && runXY(h, false, false, .1f, masked) == 0,
              "a short X+Y does nothing");
        check(XYHold::holdSeconds >= .15f && XYHold::holdSeconds <= .25f, "about 200 ms");
    }
    // The toggle: off at the start, flips on taps, master off resets, ALWAYS
    // ON is on, easing after off.
    {
        Toggle g;
        check(!g.update(dt, Input::thumbrest, true, false), "starts OFF");
        check(g.update(dt, Input::thumbrest, true, true) && g.changed(), "a tap: ON on that frame");
        check(g.update(dt, Input::thumbrest, true, false) && !g.changed(), "stays ON");
        check(!g.update(dt, Input::thumbrest, true, true) && g.changed() && g.easing(), "a tap: OFF, easing");
        for (int i = 0; i < 90; ++i)
            g.update(dt, Input::thumbrest, true, false);
        check(!g.easing(), "eased within a second");
        g.update(dt, Input::xyHold, true, true);
        check(g.on(), "X+Y toggles too");
        check(!g.update(dt, Input::xyHold, false, false) && !g.on() && !g.easing(), "LEG CONTROL off: off");
        check(g.update(dt, Input::always, true, false), "ALWAYS ON");
        check(g.toggles() == 3, "toggle count");
    }
    // Inputs and the setting: THUMB REST TAP default, X+Y without a binding.
    {
        check(fromSetting(0) == Input::thumbrest && fromSetting(1) == Input::xyHold &&
                  fromSetting(2) == Input::always && fromSetting(9) == Input::thumbrest,
              "setting values");
        check(effective(Input::thumbrest, false) == Input::xyHold && effective(Input::thumbrest, true) == Input::thumbrest &&
                  effective(Input::xyHold, true) == Input::xyHold,
              "no thumb rest binding: X+Y");
        using namespace vr_settings;
        Values v;
        check(v.legToggleInput == 0, "default THUMB REST TAP");
        check(inactiveReason(Item::legToggleInput, v) != nullptr, "needs LEG CONTROL");
        v.legControl = true;
        v.controlledFlips = true;
        check(inactiveReason(Item::legToggleInput, v) == nullptr, "with LEG CONTROL");
        check(choose(Item::legToggleInput, 1, v) && v.legToggleInput == 1, "HOLD X+Y");
        check(choose(Item::legToggleInput, 2, v) && v.legToggleInput == 2, "ALWAYS ON");
        const Row* row = rowFor(Item::legToggleInput);
        check(row && row->choices.size() == 3 && std::strcmp(row->choices[0], "THUMB REST TAP") == 0 &&
                  std::strcmp(row->choices[1], "HOLD X+Y") == 0,
              "the row's choices");
        bool listed = false;
        for (const auto& screen : screens())
            for (Item item : screen.items)
                listed = listed || item == Item::legToggleInput;
        check(listed, "on the LEGS tab");
        check(tabOnlyText(v).find("leg_toggle_input=2") != std::string::npos, "saved as leg_toggle_input");
    }
    // The Touch layout: its bindings as before, the thumb rest an extra; no
    // other layout has it; it is not one of the Frame's own.
    {
        bool touch = false;
        for (const auto& layout : controllerLayouts()) {
            for (const auto& b : layout.bindings)
                check(b.input != ControllerInput::thumbrest, "the thumb rest is never a required binding");
            if (layout.kind == ControllerKind::touch) {
                touch = layout.extras.size() == 1 && layout.extras[0].input == ControllerInput::thumbrest &&
                        std::strcmp(layout.extras[0].path, "/user/hand/left/input/thumbrest/touch") == 0;
            } else {
                check(layout.extras.empty(), "only Touch has extras");
            }
        }
        check(touch, "Touch's left thumb rest extra");
        check(!frameOnly(ControllerInput::thumbrest), "not the Frame's");
    }
    // Flip break with the legs toggled off: the right stick's up/down is the
    // zip's and heal's, no break (in.legs false); toggled on it breaks; turns
    // never do.
    {
        hybrid_owner::FlipInput in;
        in.rightValid = true;
        in.rightY = .9f;
        in.legs = false;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::none, "legs off: no right-stick break");
        in.legs = true;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::rightPitch, "legs on: pitch breaks");
        in.rightY = 0;
        in.rightX = 1;
        check(hybrid_owner::flipBreak(in) == hybrid_owner::FlipBreak::none, "a turn never breaks");
    }
    // Easing: critically damped at 2.5 Hz the legs are straight within easeSeconds.
    {
        leg_control::Simulation sim;
        leg_control::Params p;
        leg_control::Motion m;
        for (int i = 0; i < 90; ++i)
            sim.update(dt, true, 0, 1, m, p);
        p.frequencyHz = 2.5f;
        p.dampingRatio = 1;
        p.looseness = 0;
        leg_control::Output out;
        for (int i = 0, n = static_cast<int>(Toggle::easeSeconds / dt); i < n; ++i)
            out = sim.update(dt, true, 0, 0, m, p);
        float worst = 0;
        for (int l = 0; l < 2; ++l)
            worst = std::max({worst, std::abs(out.legs[l].pitch - leg_control::centrePose[l].pitch),
                              std::abs(out.legs[l].side - leg_control::centrePose[l].side),
                              std::abs(out.legs[l].knee - leg_control::centrePose[l].knee)});
        char what[96];
        std::snprintf(what, sizeof what, "eased to straight (%.3f rad left)", worst);
        check(worst < .05f, what);
    }
    std::printf("leg toggle: %d checks passed\n", passed);
    return 0;
}
