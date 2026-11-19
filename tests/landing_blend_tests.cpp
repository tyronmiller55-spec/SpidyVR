// LANDING BLEND-IN (landing_blend.hpp): time to impact, the upright ramp, the release point, the touchdown report.
#include "spidy/landing_blend.hpp"

#include <cstdlib>
#include <string>
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
    {
        // F2: the inverted touchdown table.
        check(invertedLanding(true, 9.f) == Inverted::handstand, "F2: a caught inverted landing is not the handstand");
        check(invertedLanding(false, 4.5f) == Inverted::roll, "F2: a fast inverted landing is not the roll");
        check(invertedLanding(false, 3.9f) == Inverted::upright, "F2: a slow inverted landing is not the upright ease");
        check(invertedLanding(false, NAN) == Inverted::upright, "F2: an unknown speed is not the upright ease");
        check(std::string(invertedName(Inverted::roll)) == "roll", "F2: the roll's log name");
        check(keepInverted(true, true) && !keepInverted(true, false) && !keepInverted(false, true),
              "F2: the tilt is kept for the catch only inverted with both arms up");
    }
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
        // A fall from 8 m: the upright ramp 0 above 0.4 s, 1 at 0.12 s, never backing off; the touchdown report is within a frame
        // or two. (The clock's ease off here: below.)
        Params q = p;
        q.uprightClock = 0;
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
            const Out o = t.update(in, q);
            if (t.report().valid)
                rep = t.report();
            if (o.predicting) {
                if (w0 < 0)
                    w0 = o.upright;
                mono = mono && o.upright + 1e-6f >= prevW;
                prevW = o.upright;
                if (o.tti > .4f)
                    check(o.upright == 0, "no upright before 0.4 s to impact");
                if (o.tti <= .12f)
                    rampedBy01 = rampedBy01 || o.upright > .98f;
                if (tAt03 < 0 && o.tti <= .3f)
                    tAt03 = f.t;
            }
        }
        check(w0 == 0, "the descent begins upright-free");
        check(mono, "the upright weight never backs off");
        check(rampedBy01, "fully upright by 0.12 s to impact");
        check(rep.valid && std::abs(rep.errorMs) < 40.f, "the first prediction matches the touchdown within 40 ms");
        check(rep.uprightAtTouch > .98f && rep.why == Report::Why::impact, "upright at the touchdown, by the time to impact");
        check(!t.update(in, p).predicting, "nothing after the touchdown");
    }
    {
        // UPRIGHT EASE (F1, the Nov 19 log: 51% of landings 0% upright, predictions mostly late): the ray reads the ground
        // 3 m lower than he lands (a ledge), so the time to impact never gets under 0.4 s; the clock still brings him
        // upright, and the report says so.
        Fall f{3.5f, -4};
        Tracker t;
        Input in;
        in.seconds = 1.f / 90;
        in.air = in.rayOk = true;
        Report rep;
        int guard = 0;
        while (f.air && guard++ < 1000) {
            f.step(1.f / 90);
            in.height = std::max(f.h, 0.f) + 3.f;
            in.vy = f.vy;
            in.air = f.air;
            t.update(in, p);
            if (t.report().valid)
                rep = t.report();
        }
        check(rep.valid && rep.uprightAtTouch > .5f && rep.why == Report::Why::clock,
              "a late prediction: the clock brings him upright");
        Params noClock = p;
        noClock.uprightClock = 0;
        Fall g{3.5f, -4};
        Tracker u;
        Report old;
        guard = 0;
        in.air = true;
        while (g.air && guard++ < 1000) {
            g.step(1.f / 90);
            in.height = std::max(g.h, 0.f) + 3.f;
            in.vy = g.vy;
            in.air = g.air;
            u.update(in, noClock);
            if (u.report().valid)
                old = u.report();
        }
        check(old.valid && old.uprightAtTouch < rep.uprightAtTouch, "without the clock (as before): less upright");
        check(std::string(whyName(Report::Why::kept)).find("handstand") != std::string::npos, "the reasons read");
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
