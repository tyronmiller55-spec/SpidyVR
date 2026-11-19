// DIRECTION REPORT (direction_report.hpp) and the QUIET LOG (diag_log.hpp Throttle): the rates, the imposed
// view turn against the player's own, the consumers' skew, the momentum, the formats and the dedupe. Portable.
#include "spidy/diag_log.hpp"
#include "spidy/direction_report.hpp"
#include "spidy/view_yaw_lock.hpp"
#include <thread>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::direction_report;
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
constexpr float dt = 1.f / 90.f;
Frame still(float view = 0, float hmd = 0) {
    Frame f;
    f.seconds = dt;
    f.ok = true;
    f.view = view;
    f.hmd = hmd;
    f.shown = view - hmd;
    return f;
}
} // namespace

int main() {
    // Ten lines a second.
    {
        Reporter r;
        int lines = 0;
        for (int i = 0; i < 90; ++i)
            lines += r.step(still()).line ? 1 : 0;
        check(lines >= 9 && lines <= 11, "about ten report lines in a second");
    }
    // An action edge writes a line at once and is named, with its age.
    {
        Reporter r;
        for (int i = 0; i < 3; ++i)
            r.step(still());
        r.action(Action::land);
        const auto out = r.step(still());
        check(out.line, "an action edge writes a line on that frame");
        char text[256];
        r.format(text, sizeof(text), still());
        check(std::strstr(text, "action land") != nullptr && std::strstr(text, "direction: hmd") == text, "line names the action");
        check(r.count(Action::land) == 1 && r.last() == Action::land, "counted");
    }
    // The player's own turns are not imposed; the animation's are.
    {
        Reporter r;
        const float d = 10.f * stable_yaw::degrees;
        float view = 0, hmd = 0;
        r.step(still(view, hmd));
        for (int i = 0; i < 30; ++i) { // the head turns 10 degrees a frame
            hmd += d;
            view += d;
            Frame f = still(view, hmd);
            r.step(f);
        }
        check(near(r.window().imposedDeg, 0, .01f) && r.window().imposedFrames == 0, "head turns are the player's");
        for (int i = 0; i < 10; ++i) { // the right stick turns the rig 5 degrees a frame
            Frame f = still(view + 5.f * stable_yaw::degrees, hmd);
            f.shown = f.view - hmd;
            f.playerTurn = 5.f * stable_yaw::degrees;
            view = f.view;
            r.step(f);
        }
        check(r.window().imposedFrames == 0, "right stick turns are the player's");
        for (int i = 0; i < 5; ++i) { // the animation turns the view 20 degrees a frame
            view += 20.f * stable_yaw::degrees;
            r.step(still(view, hmd));
        }
        check(r.window().imposedFrames == 5 && near(r.window().imposedDeg, 100, .5f), "animation turns are imposed");
        check(near(r.window().worstStep, 20.f * stable_yaw::degrees, 1e-3f), "worst step");
        // a snap or a latch is a discontinuity, not an imposed turn
        Frame f = still(view + 3.f, hmd);
        f.discontinuity = true;
        r.step(f);
        check(r.window().imposedFrames == 5, "a discontinuity is not imposed");
        // wrap-around at 180
        Reporter w;
        w.step(still(3.13f, 0));
        w.step(still(-3.13f, 0));
        check(w.window().imposedFrames == 0, "a 0.02 rad step across the wrap is no turn");
    }
    // Skew of consumers: worst per window, cleared by the audit.
    {
        Reporter r;
        r.skew(Consumer::eyes, .1f, .1f);
        r.skew(Consumer::hands, .05f, .15f);
        r.skew(Consumer::arms, 3.1f, -3.1f);
        r.skew(Consumer::stick, NAN, 0);
        check(r.window().skew[0] == 0 && near(r.window().skew[1], .1f) && near(r.window().skew[2], 2.f * 3.14159265f - 6.2f, 1e-3f),
              "skew is the wrapped difference");
        check(r.window().skewFrames[1] == 1 && r.window().skewFrames[0] == 0 && r.window().skewFrames[3] == 0, "frames off");
        char text[320];
        r.formatAudit(text, sizeof(text));
        check(std::strstr(text, "direction audit:") == text && std::strstr(text, "worst skew eyes 0.0 hands 5.7") != nullptr,
              "audit text");
        check(r.window().skew[1] == 0, "audit clears the window");
    }
    // WHO imposed it, and the body's tilt (the b9b2196 playtest: 292 of 845 windows past 1000 deg/s, untraced).
    {
        Reporter r;
        Frame f = still(0.f, 0.f);
        f.ride = .4f;
        r.step(f);
        f = still(.5f, 0.f);
        f.ride = .5f;
        r.step(f); // 0.5 rad imposed while the ride is on
        f = still(1.f, 0.f);
        f.limit = .2f;
        f.tiltDegrees = 170.f;
        r.step(f); // 0.5 rad imposed with the limit
        f = still(1.5f, 0.f);
        f.ride = 0.f;
        f.limit = 0.f;
        r.step(f); // the step after a limit frame still counts with it (the correction's own undo)
        f = still(2.f, 0.f);
        r.step(f); // neither
        const auto& w = r.window();
        check(near(w.imposedRideDeg, .5f * 57.29578f, .5f) && near(w.imposedLimitDeg, 2 * .5f * 57.29578f, .5f) &&
                  w.imposedFrames == 4,
              "imposed degrees split by the head ride and the shoulder limit; the rest is neither");
        check(near(w.worstTilt, 170.f, .01f) && w.invertedFrames == 1, "the audit keeps the worst tilt and the frames past 100");
        char text[512];
        r.formatAudit(text, sizeof(text));
        check(std::strstr(text, "head ride had") && std::strstr(text, "body tilt worst 170 deg, 1 frames past 100"),
              "the audit text names the head ride, the shoulder limit and the tilt");
    }
    // The audit as two lines, each within the diag's text (the 9790d67 playtest: one line lost the head ride,
    // the shoulder limit and the tilt to the 239-character cut), with the window's worst numbers.
    {
        Reporter r;
        Frame f = still(0.f, 0.f);
        f.ride = .4f;
        f.tiltDegrees = 179.f;
        for (int i = 0; i < 2000; ++i) {
            f.hmd = f.view = f.shown = static_cast<float>(i % 2) * 3.f;
            f.limit = static_cast<float>(i % 3) * .1f;
            r.step(f);
        }
        for (unsigned c = 0; c < 4; ++c)
            for (int i = 0; i < 1500; ++i)
                r.skew(static_cast<Consumer>(c), 0.f, 3.1f);
        char text[512], more[256];
        r.formatAudit(text, sizeof(text), more, sizeof(more));
        check(std::strstr(text, "direction audit:") == text && std::strstr(text, "worst skew eyes") &&
                  std::strlen(text) <= 228,
              "the audit's first line holds the turn and the skew within the diag's text");
        check(std::strstr(more, "direction audit 2:") == more && std::strstr(more, "head ride had") &&
                  std::strstr(more, "shoulder limit") && std::strstr(more, "body tilt worst 179 deg") &&
                  std::strlen(more) <= 228,
              "the audit's second line holds the head ride, the shoulder limit and the tilt within the diag's text");
        check(r.window().imposedFrames == 0, "the two-line audit clears the window too");
    }
    // RE-SYNC: the published block is read whole, with its action serial; a consumer on an older serial is
    // counted as behind.
    {
        view_yaw_lock::State a;
        a.on = true;
        a.yaw = .5f;
        a.sway = .25f;
        a.heroYaw = -1.f;
        a.ride = .5f;
        a.limit = .125f;
        const uint32_t was = view_yaw_lock::actionSerial();
        view_yaw_lock::publish(a);
        uint32_t serial = 99;
        auto b = view_yaw_lock::live(&serial);
        check(b.on && b.yaw == .5f && b.sway == .25f && b.heroYaw == -1.f && b.ride == .5f && b.limit == .125f,
              "publish then live round trip");
        check(serial == was, "the serial published is the one current");
        const uint32_t next = view_yaw_lock::bumpAction();
        check(next == was + 1 && view_yaw_lock::actionSerial() == next, "an action bumps the serial");
        view_yaw_lock::live(&serial);
        check(serial == was, "a bump is visible to consumers only with the next publish");
        view_yaw_lock::publish(a);
        view_yaw_lock::live(&serial);
        check(serial == next, "the next publish carries it");
        view_yaw_lock::noteUse(1, a, serial);
        check(view_yaw_lock::usedYaw(1) == .5f && view_yaw_lock::usedAction(1) == serial && view_yaw_lock::usedYaw(7) == 0.f,
              "a consumer's use is kept");
        Reporter r;
        r.behind(Consumer::eyes, serial, serial);
        r.behind(Consumer::hands, serial - 1, serial);
        check(r.window().behind[0] == 0 && r.window().behind[1] == 1 && r.window().behindChecked == 1, "behind counts");
        // A reader racing the writer never sees a torn block: yaw, sway and ride always agree.
        view_yaw_lock::State calm; // a block whose fields agree, as the writer below keeps them
        calm.on = true;
        view_yaw_lock::publish(calm);
        std::atomic<bool> stop{false};
        std::atomic<int> torn{0}, reads{0};
        std::thread reader([&] {
            while (!stop.load()) {
                const auto s = view_yaw_lock::live();
                if (s.yaw != s.sway || s.ride != s.yaw)
                    torn.fetch_add(1);
                reads.fetch_add(1);
            }
        });
        // The reader must be running before the writer starts (a busy CI machine may not schedule it in time otherwise).
        while (reads.load() == 0)
            std::this_thread::yield();
        for (int i = 1; i <= 200000; ++i) {
            view_yaw_lock::State t;
            t.on = true;
            t.yaw = static_cast<float>(i % 1000);
            t.sway = t.yaw;
            t.ride = t.yaw;
            view_yaw_lock::publish(t);
        }
        stop = true;
        reader.join();
        check(reads.load() > 0, "the reader ran");
        // the reader gives up after eight tries on a write in progress, so a rare torn read is bounded; none is expected
        check(torn.load() == 0, "no torn reads");
    }
    // The audit comes once a second.
    {
        Reporter r;
        int audits = 0;
        for (int i = 0; i < 180; ++i)
            audits += r.step(still()).audit ? 1 : 0;
        check(audits == 1 || audits == 2, "audit once a second");
    }
    // Momentum.
    {
        float speed = 0;
        check(near(momentumYaw(0, -1, .1f, 1.f, &speed), 0) && near(speed, 10), "forward is yaw 0");
        check(near(momentumYaw(-1, 0, .1f), 3.14159265f / 2), "left is + 90");
        check(!std::isfinite(momentumYaw(.01f, 0, .1f)), "under 1 m/s no momentum");
        check(!std::isfinite(momentumYaw(NAN, 0, .1f)) && !std::isfinite(momentumYaw(1, 1, 0)), "bad input");
        check(!std::isfinite(momentumYaw(500, 0, .1f)), "a teleport is no momentum");
    }
    // No NaN in the line.
    {
        Reporter r;
        Frame f;
        f.ok = true;
        char text[256];
        r.format(text, sizeof(text), f);
        check(std::strstr(text, "nan") == nullptr && std::strstr(text, "-999") != nullptr, "unknown angles print -999");
    }
    // A frame without a view writes no line and breaks the chain.
    {
        Reporter r;
        r.step(still(0, 0));
        Frame off;
        off.ok = false;
        off.seconds = .2f;
        check(!r.step(off).line, "no line without a view");
        r.step(still(1.5f, 0));
        check(r.window().imposedFrames == 0, "the chain restarts after a gap");
    }
    // QUIET LOG: the noisy kinds are kept on a change and on the heartbeat only.
    {
        using diag_log::Throttle;
        Throttle t;
        t.level(2); // VERBOSE: the 2 s heartbeat these were written for (log_level 1, the default, is 5 s)
        check(Throttle::kindOf("forearm sign: left +1.2 deg") == 1 && Throttle::kindOf("view yaw: lock 1") == -1, "kinds");
        int kept = 0;
        for (int i = 0; i < 600; ++i) // 60 s at 10 Hz
            kept += t.allow(("forearm sign: left " + std::to_string(i) + " deg positive").c_str(), i * .1) ? 1 : 0;
        check(kept >= 30 && kept <= 32, "same-shape noise is kept every 2 s");
        check(t.allow("forearm sign: right 3 deg positive", 60.5), "a changed shape is kept at once");
        check(!t.allow("forearm sign: right 4 deg positive", 60.6), "then quiet again");
        check(t.allow("spherical aim: arms trusted", 61) && t.allow("spherical aim: arms misaligned", 61.1),
              "each shape of a kind is a change");
        check(t.allow("hybrid: vr->anim (land 12 ms)", 1) && t.allow("hybrid: vr->anim (land 12 ms)", 1.01), "other lines pass");
        check(t.dropped > 0 && t.kept > 0, "counts");
        // slow mode: noisy only on change, the rest once per shape in 10 s
        Throttle s;
        s.slow(true);
        check(s.allow("tracking lock: Lw 1.00", 0) && !s.allow("tracking lock: Lw 0.50", 5), "slow: noisy lines only on a change");
        check(s.allow("a line 3", 0) && !s.allow("a line 4", 3) && s.allow("a line 5", 11), "slow: one per shape in 10 s");
        check(s.allow("other thing", 3), "slow: a new shape passes");
        s.slow(false);
        check(s.allow("a line 6", 11.5) && s.allow("a line 7", 11.6), "live again: everything passes");
        // a clock that restarts keeps the line
        Throttle c;
        check(c.allow("animated hand: x 1", 100) && c.allow("animated hand: x 2", 5), "a clock reset is not a drop");
    }
    std::printf("direction report: %d checks passed\n", passed);
    return 0;
}
