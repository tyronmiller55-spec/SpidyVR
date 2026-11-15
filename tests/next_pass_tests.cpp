// NEXT PASS (the owner's decisions, Nov 15: handoff hitches, body yaw, handstands): the pure parts of each change.
// Portable.
#include "spidy/camera_mode.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/hybrid_handoff.hpp"
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
    // LANDING REAR TURN: the facing check's rear rule follows the setting.
    {
        constexpr float deg = 3.14159265f / 180.f;
        check(vr_settings::Values{}.landingRearTurn, "rear turn on by default");
        facing_check::Check fc;
        fc.arm(facing_check::Event::landing, 0.f);
        facing_check::Sample fs;
        fs.heroMatters = fs.heroOk = true;
        fs.hero = 150 * deg;
        fs.rearTurns = false;
        check(fc.update(.01f, fs).hero.verdict == facing_check::Verdict::kept, "rear turn off: kept (as before)");
        fc.arm(facing_check::Event::landing, 0.f);
        fs.rearTurns = true;
        const auto r = fc.update(.01f, fs);
        check(r.hero.verdict == facing_check::Verdict::corrected && r.heroTurn, "rear turn on: he turns");
    }
    std::printf("next pass: %d checks passed\n", passed);
    return 0;
}
