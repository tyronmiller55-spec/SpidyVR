// NEXT PASS (the owner's decisions, Nov 15: handoff hitches, body yaw, handstands): the pure parts of each change.
// Portable.
#include "spidy/hybrid_handoff.hpp"
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
    std::printf("next pass: %d checks passed\n", passed);
    return 0;
}
