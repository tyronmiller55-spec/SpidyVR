// POLISH PASS, slice 6 (runtime): where the longest visit went (hitch_guard.hpp SectionWindow), the LOW FPS watch
// (low_fps.hpp), the quiet log's new kinds (diag_log.hpp Throttle), the new defaults (STEADY, the scene at 50%) and
// their one-time migration. Portable.
#include "spidy/diag_log.hpp"
#include "spidy/eye_resolution.hpp"
#include "spidy/headset_smoothing.hpp"
#include "spidy/hitch_guard.hpp"
#include "spidy/low_fps.hpp"
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
    using namespace hitch_guard;
    // SECTIONS: the worst visit's split, the peaks, the share nothing holds.
    {
        SectionWindow w;
        double a[sectionCount]{}, b[sectionCount]{};
        a[0] = 10;
        a[2] = 30;
        b[3] = 4000;
        b[5] = 18000; // a punch scan
        b[6] = 100;
        w.add(60., a);
        w.add(25000., b);
        w.add(70., a);
        const auto account = w.take();
        check(near(account.worstUs, 25000.f, .5f), "the worst visit's total");
        check(near(account.worst[5], 18000.f, .5f) && near(account.worst[3], 4000.f, .5f), "the worst visit's sections");
        check(near(account.restUs, 25000.f - 4000.f - 18000.f - 100.f, .5f), "the untimed share");
        check(near(account.peak[2], 30.f, .01f) && near(account.peak[5], 18000.f, .5f), "per-section peaks");
        check(near(w.take().worstUs, 0.f), "a taken window starts empty");
        // not finite: ignored
        SectionWindow n;
        double bad[sectionCount]{};
        bad[1] = NAN;
        n.add(NAN, bad);
        n.add(10., bad);
        check(near(n.take().worstUs, 10.f), "a non-finite visit is passed over");
        char line[420];
        sectionLine(account, line, sizeof line);
        check(std::strstr(line, "longest visit 25000 us") && std::strstr(line, "punch scan 18000") &&
                  std::strstr(line, "untimed 2900") && std::strstr(line, "section peaks us:"),
              "the sections' line");
        char tiny[30];
        sectionLine(account, tiny, sizeof tiny);
        check(std::strlen(tiny) < sizeof tiny, "the line fits a small buffer");
    }
    // LOW FPS: a minute under 15 fps, once; back after two windows at 24+; pauses and stops are no measurement.
    {
        low_fps::Watch w;
        const auto run = [&](float seconds, float fps, low_fps::Event* last = nullptr) {
            int slow = 0, normal = 0;
            for (float t = 0; t < seconds; t += 1.f / fps) {
                const auto e = w.frame(1.f / fps);
                slow += e == low_fps::Event::slow;
                normal += e == low_fps::Event::normal;
            }
            (void)last;
            return std::make_pair(slow, normal);
        };
        check(run(120, 36).first == 0 && !w.slow(), "36 fps is fine");
        check(run(50, 9).first == 0 && !w.slow(), "under a minute of 9 fps says nothing");
        check(run(20, 9).first == 1 && w.slow(), "a minute of 9 fps is slow, once");
        check(near(w.fps(), 9.f, .6f), "the window's rate");
        check(run(60, 9).first == 0 && w.slow(), "and not again");
        w.reset();
        run(70, 8);
        check(w.slow(), "slow again after a reset");
        check(run(4, 40).second == 0 && w.slow(), "less than two good windows is still slow");
        check(run(12, 40).second == 1 && !w.slow(), "two good windows end it");
        // 20 fps is neither slow nor good: it holds the state
        low_fps::Watch h;
        for (float t = 0; t < 70; t += .05f)
            h.frame(.05f);
        check(!h.slow(), "20 fps is not slow");
        low_fps::Watch p;
        check(p.frame(120.f) == low_fps::Event::none && p.frame(-1.f) == low_fps::Event::none &&
                  p.frame(NAN) == low_fps::Event::none,
              "a pause and bad frames are no measurement");
        char text[200];
        low_fps::line(low_fps::Event::slow, text, sizeof text, 8.7f, 65.f);
        check(std::strstr(text, "low fps:") && std::strstr(text, "9 fps"), "the slow line");
        low_fps::line(low_fps::Event::normal, text, sizeof text, 40.f, 0);
        check(std::strstr(text, "low fps over"), "the over line");
    }
    // QUIET LOG: the playtest's repeats are throttled, a new reason is not.
    {
        diag_log::Throttle t;
        t.level(2); // VERBOSE: the 2 s heartbeat these were written for
        check(diag_log::Throttle::kindOf("point jump: denied no contact (A and B)") >= 0 &&
                  diag_log::Throttle::kindOf("sidestep: denied not sideways") >= 0 &&
                  diag_log::Throttle::kindOf("handstand: no catch at the touchdown: x") >= 0 &&
                  diag_log::Throttle::kindOf("hybrid: adopt never seeded (no frame)") >= 0 &&
                  diag_log::Throttle::kindOf("handstand: caught") < 0,
              "the new kinds");
        check(t.allow("point jump: denied no contact (A and B went on)", 1), "the first is kept");
        check(!t.allow("point jump: denied no contact (A and B went on)", 1.5), "the repeat within 2 s is dropped");
        check(t.allow("point jump: denied buffer expired (let go of after 181 ms)", 1.6), "a new reason is kept");
        check(t.allow("point jump: denied no contact (A and B went on)", 4), "the heartbeat keeps one");
    }
    // DEFAULTS: the scene at 50%, STEADY, and the one-time migration of what was saved at the old defaults.
    {
        check(defaultEyeUpscale == 50 && minimumEyeUpscale == 50 && validEyeUpscale(defaultEyeUpscale), "scene at 50%");
        const vr_settings::Values fresh;
        check(fresh.steadyVerticalMs == 200 && fresh.steadyHorizontalMs == 200 && fresh.steadyAngleMs == 200,
              "STEADY's new defaults");
        check(headset_smoothing::bankAngleBeta[3] > .12f && headset_smoothing::bankAngleBeta[3] < headset_smoothing::bankAngleBeta[2],
              "STEADY still damps hardest but lets a quick move through sooner");
        check(headset_smoothing::bankPositionBeta[3] < headset_smoothing::bankPositionBeta[2], "STEADY's position term");
        const std::string old = "camera_steady_vertical_ms=350\ncamera_steady_horizontal_ms=300\ncamera_steady_angle_ms=450\n"
                                "movement_defaults=1\n";
        const auto migrated = vr_settings::withMovementDefaults(vr_settings::withTabOnly({}, old), old);
        check(migrated.movementDefaults == vr_settings::movementDefaultsRevision, "the revision is current");
        // the steady keys are not tab-only keys: they come from the profile; the migration works on the values
        vr_settings::Values saved;
        saved.steadyVerticalMs = 350;
        saved.steadyHorizontalMs = 300;
        saved.steadyAngleMs = 450;
        const std::string rev1 = "movement_defaults=1\n";
        const auto after = vr_settings::withMovementDefaults(saved, rev1);
        check(after.steadyVerticalMs == 200 && after.steadyHorizontalMs == 200 && after.steadyAngleMs == 200,
              "the old defaults become the new ones");
        vr_settings::Values tuned;
        tuned.steadyVerticalMs = 350;
        tuned.steadyHorizontalMs = 250;
        tuned.steadyAngleMs = 450;
        check(vr_settings::withMovementDefaults(tuned, rev1).steadyHorizontalMs == 250, "a tuned bank stays");
        const std::string rev2 = "movement_defaults=2\n";
        check(vr_settings::withMovementDefaults(saved, rev2).steadyVerticalMs == 350, "a file that has seen it keeps its own");
    }
#if defined(_WIN32)
    // The flushed VR-start log (diag_log::durable): a crash before the launcher's
    // next poll still leaves the last OpenXR step on disk.
    {
        const char* path = "spidy-vr-start-test.log";
        std::remove(path);
        diag_log::durableBegin(path);
        check(std::strcmp(diag_log::durablePath(), path) == 0, "durable path");
        diag_log::durable("vr start: frame 1 wait calling xrWaitFrame");
        diag_log::durableClose();
        std::FILE* f = std::fopen(path, "rb");
        check(f != nullptr, "durable file exists");
        char buf[1024]{};
        const auto n = f ? std::fread(buf, 1, sizeof(buf) - 1, f) : 0;
        if (f)
            std::fclose(f);
        std::remove(path);
        check(n > 0 && std::strstr(buf, "spidy vr-start log:") != nullptr, "durable header");
        check(std::strstr(buf, "vr start: frame 1 wait calling xrWaitFrame") != nullptr, "durable step");
    }
#endif
    std::printf("polish runtime: %d checks passed\n", passed);
    return 0;
}
