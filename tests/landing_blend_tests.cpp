// LANDING BLEND-IN (landing_blend.hpp): time to impact, the upright ramp, the release point, the touchdown report.
#include "spidy/landing_blend.hpp"

#include <cstdlib>
#include <iostream>

using namespace spidy::landing_blend;
namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL " << what << "\n";
        ++failures;
    }
}
// A fall from `h0` m at `vy0` m/s: the ground ray reads the true height every frame (lag `lag` s of fall), 90 Hz.
struct Fall {
    float h, vy, t = 0;
    bool air = true;
    void step(float dt, float g = 9.8f) {
        vy -= g * dt;
        h += vy * dt;
        t += dt;
        if (h <= 0)
            air = false;
    }
};
} // namespace

int main() {
    Params p;
    {
        check(std::abs(timeToImpact(0, -9) - 0) < 1e-4f, "on the ground: no time");
        check(std::abs(timeToImpact(3, -9) - 0.288f) < .003f, "3 m at 9 m/s down: 0.288 s");
        check(std::abs(timeToImpact(10, 0) - std::sqrt(2.f * 10 / 9.8f)) < 1e-4f, "10 m from rest");
        check(timeToImpact(5, 6) > 1.f, "rising: the arc's whole time");
        check(!std::isfinite(timeToImpact(-1, 0)) && !std::isfinite(timeToImpact(NAN, 0)) &&
                  !std::isfinite(timeToImpact(1, INFINITY)),
              "no answer for bad numbers");
    }
    {
        // A fall from 8 m: the upright ramp 0 above 0.3 s, 1 at 0.1 s, never backing off; the touchdown report is within a frame or two.
        Fall f{8, 0};
        Tracker t;
        Input in;
        in.seconds = 1.f / 90;
        in.air = true;
        in.rayOk = true;
        float w0 = -1, prevW = 0;
        bool mono = true, rampedBy01 = false;
        float tAt03 = -1;
        Report rep;
        int guard = 0;
        while (f.air && guard++ < 1000) {
            f.step(1.f / 90);
            in.height = std::max(f.h, 0.f);
            in.vy = f.vy;
            in.air = f.air;
            const Out o = t.update(in, p);
            if (t.report().valid)
                rep = t.report();
            if (o.predicting) {
                if (w0 < 0)
                    w0 = o.upright;
                mono = mono && o.upright + 1e-6f >= prevW;
                prevW = o.upright;
                if (o.tti > .3f)
                    check(o.upright == 0, "no upright before 0.3 s to impact");
                if (o.tti <= .1f)
                    rampedBy01 = rampedBy01 || o.upright > .98f;
                if (tAt03 < 0 && o.tti <= .3f)
                    tAt03 = f.t;
            }
        }
        check(w0 == 0, "the descent begins upright-free");
        check(mono, "the upright weight never backs off");
        check(rampedBy01, "fully upright by 0.1 s to impact");
        check(rep.valid && std::abs(rep.errorMs) < 40.f, "the first prediction matches the touchdown within 40 ms");
        check(rep.uprightAtTouch > .98f, "upright at the touchdown");
        check(!t.update(in, p).predicting, "nothing after the touchdown");
    }
    {
        // Ray lag: a reading 3 frames old, corrected by the fall since.
        Fall f{4, -6};
        const float truth = timeToImpact(f.h, f.vy);
        Tracker t;
        Input in;
        in.seconds = 1.f / 90;
        in.air = in.rayOk = true;
        in.height = f.h + 6.f * 3 / 90.f; // read 3 frames ago
        in.heightAge = 3.f / 90;
        in.vy = f.vy;
        const Out o = t.update(in, p);
        check(std::abs(o.tti - truth) < .04f, "a lagged reading is brought up to now");
    }
    {
        // Not a landing: rising, slow, out of the ray's reach, no ground found, off, or kept inverted.
        auto tti = [&](float h, float vy, bool ok, bool air) {
            Tracker t;
            Input in;
            in.seconds = 1.f / 90;
            in.air = air;
            in.rayOk = ok;
            in.height = h;
            in.vy = vy;
            return t.update(in, p).predicting;
        };
        check(!tti(3, 4, true, true), "rising: no landing predicted");
        check(!tti(3, -1, true, true), "a slow drift down is not an approach");
        check(!tti(30, -9, true, true), "past the ray's reach: none");
        check(!tti(3, -9, false, true), "no ground found: none");
        check(!tti(3, -9, true, false), "not in SpidyVR's air: none");
        check(tti(3, -9, true, true), "the plain case predicts");
        Params off = p;
        off.enabled = false;
        Tracker t;
        Input in;
        in.seconds = 1.f / 90;
        in.air = in.rayOk = true;
        in.height = .5f;
        in.vy = -9;
        check(!t.update(in, off).predicting && t.update(in, off).upright == 0, "off: nothing");
        Tracker keep;
        in.keepTilt = true;
        const Out o = keep.update(in, p);
        check(o.predicting && o.upright == 0 && !o.handBack, "a handstand keeps his tilt: no upright, no release");
    }
    {
        // The release point: only with `release`, at 0.2 s to impact.
        Params rel = p;
        rel.release = true;
        auto handBackAt = [&](const Params& pp) {
            Fall f{8, 0};
            Tracker t;
            Input in;
            in.seconds = 1.f / 90;
            in.air = in.rayOk = true;
            float at = -1;
            int guard = 0;
            while (f.air && guard++ < 1000) {
                f.step(1.f / 90);
                in.height = std::max(f.h, 0.f);
                in.vy = f.vy;
                const Out o = t.update(in, pp);
                if (o.handBack && at < 0)
                    at = o.tti;
            }
            return at;
        };
        const float at = handBackAt(rel);
        check(at > .17f && at <= .2f, "the release point is at 0.2 s to impact");
        check(handBackAt(p) < 0, "off by default: never released");
    }
    {
        // A web or a push takes him up again: the descent is forgotten.
        Tracker t;
        Input in;
        in.seconds = 1.f / 90;
        in.air = in.rayOk = true;
        in.height = 1;
        in.vy = -9;
        for (int i = 0; i < 5; ++i)
            t.update(in, p);
        in.vy = 8;
        in.rayOk = false;
        t.update(in, p);
        in.air = false;
        t.update(in, p);
        check(!t.report().valid || t.report().actual < .2f, "a descent cut by a push is not reported as a landing");
    }
    if (failures) {
        std::cerr << failures << " checks failed\n";
        return 1;
    }
    std::cout << "Landing blend: time to impact, upright ramp 0.3-0.1 s, release point 0.2 s (gated), touchdown report\n";
    return 0;
}
