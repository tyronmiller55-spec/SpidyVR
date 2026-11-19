// BETA POLISH PASS (Nov 19, plans/beta-polish-pass.md): the portable pieces of the batch 1 safety and log items and
// the batch 4 feel items that have no suite of their own. Portable.
#include "spidy/diag_log.hpp"
#include "spidy/eye_stall.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
} // namespace

int main() {
    // R1: the eye stall watch forgives a module start on the eye worker, not a real stall.
    {
        using namespace eye_stall;
        Watch w;
        check(judge(10000, 0, w) == Verdict::fine, "nothing shown yet is the first-image deadline's");
        check(judge(12999, 10000, w) == Verdict::fine, "under 3 s is fine");
        check(judge(13000, 10000, w) == Verdict::stalled, "3 s with no start is a stall");
        // 2:17 AM: native swinging took 3682 ms right after a pair was shown.
        w.begin(10010);
        check(w.starting(), "a start under way");
        check(judge(13500, 10000, w) == Verdict::forgivenStart, "during the start: forgiven");
        w.end(13692);
        check(w.lastStartMs() == 3682, "the start's length");
        check(judge(13700, 10000, w) == Verdict::forgivenStart, "just after the start: forgiven");
        check(judge(16691, 10000, w) == Verdict::forgivenStart, "the count restarts at the start's end");
        check(judge(16692, 10000, w) == Verdict::stalled, "3 s after the start with no pair is a stall");
        // a start longer than 10 s still ends VR
        Watch longStart;
        longStart.begin(10000);
        check(judge(19999, 9000, longStart) == Verdict::forgivenStart, "a long start under 10 s");
        check(judge(20000, 9000, longStart) == Verdict::stalled, "a start running 10 s stalls");
        longStart.end(21000);
        check(judge(21100, 9000, longStart) == Verdict::stalled, "a finished 11 s start is not forgiven");
        // a start before the last shown pair forgives nothing
        Watch old;
        old.begin(1000);
        old.end(1500);
        check(judge(13000, 10000, old) == Verdict::stalled, "an older start forgives nothing");
    }
    // G4: LOG LEVEL - the default repeats the noisy kinds once per 5 s and takes in the head probe, forearm aim and
    // player torso; VERBOSE is the old 2 s and leaves those three alone; QUIET is 30 s.
    {
        using diag_log::Throttle;
        check(Throttle::kindOf("head probe: anim live: x") >= 0 && Throttle::kindOf("player torso: active 1") >= 0 &&
                  Throttle::kindOf("forearm aim: left") >= 0,
              "the beta's kinds at the default level");
        check(Throttle::kindOf("head probe: anim live: x", 2) < 0 && Throttle::kindOf("forearm sign: x", 2) == 1,
              "verbose leaves the beta's kinds out");
        auto keptOver60 = [](int level) {
            Throttle t;
            t.level(level);
            int kept = 0;
            for (int i = 0; i < 600; ++i)
                kept += t.allow(("head probe: anim live " + std::to_string(i)).c_str(), i * .1) ? 1 : 0;
            return kept;
        };
        check(keptOver60(1) >= 12 && keptOver60(1) <= 13, "default: once per 5 s");
        check(keptOver60(0) >= 2 && keptOver60(0) <= 3, "quiet: once per 30 s");
        check(keptOver60(2) == 600, "verbose: the head probe is not throttled");
        Throttle t;
        check(t.allow("head probe: state anim -> vr", 0) && t.allow("head probe: owner in vr: body x", .1),
              "a changed shape is kept at once");
        vr_settings::Values v;
        check(v.logLevel == 1, "log_level defaults to 1");
        check(vr_settings::withTabOnly({}, "log_level=7\n").logLevel == 2 &&
                  vr_settings::withTabOnly({}, "log_level=0\n").logLevel == 0,
              "log_level is clamped and read");
    }
    std::printf("beta polish: %d checks passed\n", passed);
    return 0;
}
