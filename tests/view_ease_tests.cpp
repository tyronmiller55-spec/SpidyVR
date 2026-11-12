// PUNCH VIEW EASE (view_ease.hpp, November 12): the hold at the switch, the
// ease-out to zero, the motion-masked bleed floor, every cancel, no NaNs, the
// rig's start offset (shown eyes minus the lock's) with the lock unchanged,
// and the setting. Portable.
#include "spidy/game_tracking.hpp"
#include "spidy/view_ease.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::view_ease;
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
constexpr float dt = 1.f / 90;
bool finiteVec(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace

int main() {
    // The curve: 1 at the start, (1-t)^3, 0 at the end; monotonic.
    {
        check(near(remaining(0, .25f), 1) && near(remaining(.25f, .25f), 0) && near(remaining(.125f, .25f), .125f),
              "ease-out cubic");
        float last = 2;
        bool down = true;
        for (int i = 0; i <= 30; ++i) {
            const float r = remaining(i * .01f, .25f);
            down = down && r <= last;
            last = r;
        }
        check(down, "never grows");
        check(remaining(.1f, 0) == 0 && remaining(NAN, .25f) == 0, "no duration / NaN: done");
    }
    // Standing still: the first frame shows (nearly) the whole offset, it
    // eases to zero within the duration, never grows, along one direction.
    {
        Ease e;
        const Vec3 off{.3f, .1f, -.5f};
        check(e.begin(off, .25f) && e.active() && near(e.startOffset(), length(off)), "begins");
        Vec3 first = e.step(dt, {0, 0, 0}, 0);
        check(length(first) > .9f * length(off) && dot(normalized(first), normalized(off)) > .999f,
              "first frame: held at the shown eyes");
        float last = length(first);
        int frames = 1;
        bool down = true;
        while (e.active() && frames < 200) {
            const Vec3 o = e.step(dt, {0, 0, 0}, 0);
            down = down && length(o) <= last + 1e-6f && finiteVec(o);
            last = length(o);
            ++frames;
        }
        check(down, "monotonic, finite");
        check(!e.active() && frames <= static_cast<int>(.25f / dt) + 2, "done within 250 ms");
        check(e.lastEnd() == End::finished && e.ends() == 1, "finished");
        check(length(e.step(dt, {}, 0)) == 0, "idle: zero");
    }
    // Moving: the bleed floor (0.10 m/s + 0.2 x speed) ends it sooner.
    {
        Ease still, moving;
        still.begin({0, 0, .6f}, .4f);
        moving.begin({0, 0, .6f}, .4f);
        int a = 0, b = 0;
        Vec3 feet{};
        while (still.active() && a < 400) {
            still.step(dt, {}, 0);
            ++a;
        }
        while (moving.active() && b < 400) {
            feet = feet + Vec3{20 * dt, 0, 0}; // 20 m/s lunge
            moving.step(dt, feet, 0);
            ++b;
        }
        check(b < a, "motion bleeds it sooner");
        Ease slowBleed;
        slowBleed.begin({0, 0, .6f}, 10.f); // a long ease: the floor alone ends it
        int c = 0;
        while (slowBleed.active() && c < 2000) {
            slowBleed.step(dt, {}, 0);
            ++c;
        }
        check(c * dt < .6f / bleedBase + .1f, "never slower than 0.10 m/s standing");
    }
    // Cancels: teleport, large turn (one frame, or in total), another
    // handoff, invalid input.
    {
        Ease e;
        e.begin({0, 0, .5f}, .25f);
        e.step(dt, {0, 0, 0}, 0);
        check(length(e.step(dt, {10, 0, 0}, 0)) == 0 && !e.active() && e.lastEnd() == End::teleport, "teleport");
        e.begin({0, 0, .5f}, .25f);
        e.step(dt, {}, 0);
        check(length(e.step(dt, {}, .6f)) == 0 && e.lastEnd() == End::turn, "a snap turn");
        e.begin({0, 0, .5f}, 1.f);
        e.step(dt, {}, 0);
        float yaw = 0;
        while (e.active()) {
            yaw += .2f;
            e.step(dt, {}, yaw);
        }
        check(e.lastEnd() == End::turn && yaw <= turnTotal + .25f, "a large smooth turn");
        e.begin({0, 0, .5f}, .25f);
        check(e.begin({0, .2f, 0}, .25f) && e.lastEnd() == End::handoff && near(e.startOffset(), .2f),
              "another handoff replaces it");
        e.cancel(End::handoff);
        check(!e.active(), "cancelled");
        e.begin({0, 0, .5f}, .25f);
        check(length(e.step(dt, {NAN, 0, 0}, 0)) == 0 && e.lastEnd() == End::invalid, "NaN feet");
        e.begin({0, 0, .5f}, .25f);
        check(length(e.step(dt, {}, NAN)) == 0 && e.lastEnd() == End::invalid, "NaN yaw");
        e.begin({0, 0, .5f}, .25f);
        const Vec3 o = e.step(NAN, {}, 0);
        check(finiteVec(o) && e.active(), "NaN frame time: held, finite");
        check(!e.begin({NAN, 0, 0}, .25f) && !e.begin({0, 0, .5f}, NAN) && !e.begin({0, 0, .5f}, 0),
              "invalid begins refused");
        check(!e.begin({0, 0, .005f}, .25f), "a tiny offset is no ease");
        check(!e.begin({0, 0, 4.f}, .25f), "an offset no handoff makes is refused");
        check(std::strcmp(endName(End::teleport), "cancelled (teleport)") == 0, "names");
    }
    // The rig: after the lock, the ease starts at the shown eyes minus where
    // the lock put the tracked head; the lock itself is not moved.
    {
        GameTrackingRig rig;
        const Pose head{{.2f, 1.6f, -.1f}, {}};
        const Vec3 feet{10, 0, 5};
        const Vec3 shown{10.5f, 1.7f, 5.3f};
        rig.lockToFeet(feet, shown, 0, head);
        const float residualBefore = rig.residualAcross(head, feet);
        check(rig.beginViewEase(shown, head, .25f), "rig begins");
        const Vec3 lockedEyes = feet + Vec3{0, head.position.y, 0};
        check(near(rig.viewEase().startOffset(), length(shown - lockedEyes), 1e-3f), "offset = shown - locked eyes");
        check(near(rig.residualAcross(head, feet), residualBefore) && near(residualBefore, 0), "the lock is unchanged");
        check(!rig.beginViewEase({NAN, 0, 0}, head, .25f) && !rig.viewEase().active(), "NaN eyes: none");
        rig.beginViewEase(shown, head, .25f);
        rig.reset();
        check(!rig.viewEase().active() && rig.viewEase().lastEnd() == End::reset, "a tracking reset ends it");
    }
    // The setting: 250 ms by default, 0 the old snap, on the camera tab, a personal key.
    {
        using namespace vr_settings;
        Values v;
        check(v.punchViewEaseMs == 250, "default 250 ms");
        check(choose(Item::punchViewEase, 0, v) && v.punchViewEaseMs == 0, "OFF (SNAP)");
        const Row* row = rowFor(Item::punchViewEase);
        check(row && row->choices.size() == 6 && std::strcmp(row->choices[0], "OFF (SNAP)") == 0 &&
                  std::strcmp(row->choices[3], "250 MS") == 0,
              "choices");
        check(inactiveReason(Item::punchViewEase, v) == nullptr, "always available");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::camera)].items)
            listed = listed || item == Item::punchViewEase;
        check(listed, "on VR CAMERA & STABILIZATION");
        check(tabOnlyText(v).find("punch_view_ease_ms=0") != std::string::npos, "saved as punch_view_ease_ms");
    }
    std::printf("punch view ease: %d checks passed\n", passed);
    return 0;
}
