// LANDING BLEND-IN (the user, Nov 14 t73u and the answers to t73s656: "landing and leaving the ground for a jump is animation
// where it can be, SpidyVR should only take over as a blend out/into those types of animations"; "hand back to the game's
// landing animation just before touchdown (time-to-impact blend)"; setting air_landing_blend, default ON).
//
// SpidyVR owns the air. As the ground comes up this works out the time to impact from a ray straight down (native ray tag 8, in
// the roof ray's slot while he is in SpidyVR's air; game_xr.cpp) and his vertical speed, then:
//   1. the body comes upright over the last 0.3 s (weight 0 at 0.30 s to impact, 1 at 0.10 s), so the game's landing animation
//      starts from upright. The body only; the view and the heading are never touched (REFERENCE.md "camera leads"). Never while
//      a handstand wants him inverted (the caller's `keepTilt`).
//   2. at the touchdown the owner hands the body to the game as it always did (hybrid_owner EnterAnim::land) with the 180 ms eye
//      blend (hybrid_handoff.hpp): THIS IS THE FALLBACK and it is what ships on.
//   3. only with `release` (air_landing_release, on by default since revision 14 for further testing; UNVERIFIED): handBack() says so at 0.2 s to impact and the owner hands
//      the animation the body before the touchdown (EnterAnim::predicted). That relies on the game keeping his velocity and
//      choosing its normal landing clip when the body is let go in mid-air (docs/AIR_TAKEOVER.md, "the probe"); nothing here
//      writes a velocity.
// The Report at each touchdown says how good the prediction was, which is the measurement the release needs first.
// Pure, no Windows.
#pragma once

#include <algorithm>
#include <cmath>

namespace spidy::landing_blend {
struct Params {
    bool enabled = true;
    bool release = false;       // air_landing_release: hand the body back in the air (unverified)
    // UPRIGHT EASE (beta polish F1, the owner's D9; the Nov 19 log: 51% of 264 landings 0% upright at the touchdown,
    // 62% of first predictions over 100 ms off, mostly late): the body starts coming upright earlier (0.40 s to impact,
    // fully at 0.12 s) and, whatever the prediction says, by the clock too: fully upright uprightClock s after the
    // descent was first predicted.
    float uprightFrom = .40f;   // s to impact: the body starts coming upright
    float uprightTo = .12f;     // s to impact: fully upright
    float uprightClock = .45f;  // s from the first prediction: fully upright by then (0: off)
    float handBackAt = .20f;    // s to impact: release (only with `release`)
    float maxHeight = 14.f;     // m: the ray's reach; beyond it there is no prediction
    float minFall = 2.f;        // m/s down: slower than this is not a landing approach
    float gravity = 9.8f;       // m/s^2
};
// Seconds until the feet reach the ground from `height` m above it at vertical speed `vy` (m/s, up +), under `gravity`; NaN when
// there is no impact ahead (rising off the ground without enough height, bad numbers).
inline float timeToImpact(float height, float vy, float gravity = 9.8f) {
    if (!std::isfinite(height) || !std::isfinite(vy) || !(gravity > 0) || height < 0)
        return NAN;
    const float disc = vy * vy + 2 * gravity * height;
    const float t = (vy + std::sqrt(disc)) / gravity;
    return t >= 0 && std::isfinite(t) ? t : NAN;
}
struct Input {
    float seconds = 0;
    bool air = false;       // he is in SpidyVR's air (not grounded, not held by a wall or a web zip)
    bool rayOk = false;     // the last downward ray found the ground
    float height = 0;       // m above the ground at that reading
    float heightAge = 0;    // s since the reading (the bridge lags a frame or two)
    float vy = 0;           // m/s, up +
    bool keepTilt = false;  // a handstand wants him inverted, or he is held by hands/webs: no upright
};
struct Report {
    bool valid{};
    float firstTti = NAN, firstHeight = NAN, firstVy = NAN; // the first prediction of the descent
    float lastTti = NAN;                                    // the last before the touchdown
    float actual = NAN;                                     // s from the first prediction to the touchdown
    float errorMs = NAN;                                    // first prediction - actual, ms (+ late)
    float uprightAtTouch = 0;
    // UPRIGHT EASE: what brought him upright (or kept him not): the time to impact, the clock, a handstand or a hold
    // keeping the tilt, or neither in time.
    enum class Why : unsigned char { tooSoon, impact, clock, kept } why{};
};
inline const char* whyName(Report::Why w) {
    return w == Report::Why::impact  ? "the time to impact"
           : w == Report::Why::clock ? "the clock (the prediction was late)"
           : w == Report::Why::kept  ? "kept tilted (a handstand or a hold)"
                                     : "touched down too soon";
}
// F2 (beta polish D9): an inverted touchdown (invertedDegrees or more). Two palms in reach: the handstand catch;
// else he comes upright - at rollSpeed or more of level speed logged as a roll (the body takes the upright ease;
// a tumble roll pose is not made yet). Before the touchdown the tilt is kept for the catch only while both arms
// are up (keepInverted), else the upright ease (F1) runs on an inverted fall too.
constexpr float invertedDegrees = 100.f, rollSpeed = 4.f;
enum class Inverted : unsigned char { handstand, roll, upright };
inline Inverted invertedLanding(bool caught, float levelSpeed) {
    if (caught)
        return Inverted::handstand;
    return std::isfinite(levelSpeed) && levelSpeed >= rollSpeed ? Inverted::roll : Inverted::upright;
}
inline const char* invertedName(Inverted i) {
    return i == Inverted::handstand ? "handstand" : i == Inverted::roll ? "roll" : "upright";
}
inline bool keepInverted(bool inverted, bool armsUp) { return inverted && armsUp; }
struct Out {
    float tti = NAN;
    float upright = 0;      // 0..1: how far the body has come upright
    bool handBack{};        // the release point (only with Params::release)
    bool predicting{};
};
class Tracker {
  public:
    Out update(const Input& in, const Params& p) {
        Out out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        report_ = {};
        if (!p.enabled || !in.air) {
            if (descending_ && p.enabled)
                finish();
            reset();
            return out;
        }
        float tti = NAN;
        const bool falling = std::isfinite(in.vy) && in.vy < -p.minFall;
        if (in.rayOk && falling && std::isfinite(in.height) && in.height >= 0 && in.height <= p.maxHeight) {
            const float age = std::isfinite(in.heightAge) ? std::clamp(in.heightAge, 0.f, .25f) : 0.f;
            const float h = std::max(0.f, in.height + in.vy * age); // he has fallen since the reading
            tti = timeToImpact(h, in.vy, p.gravity);
            // The reading's own age has gone by too.
            if (std::isfinite(tti))
                tti = std::max(0.f, tti);
        }
        if (std::isfinite(tti)) {
            if (!descending_) {
                descending_ = true;
                firstTti_ = tti, firstHeight_ = in.height, firstVy_ = in.vy;
                sinceFirst_ = 0;
                upright_ = 0;
            }
            sinceFirst_ += dt;
            lastTti_ = tti;
            out.tti = tti;
            out.predicting = true;
            if (!in.keepTilt) {
                const float u = std::clamp((p.uprightFrom - tti) / std::max(p.uprightFrom - p.uprightTo, .01f), 0.f, 1.f);
                const float byImpact = u * u * (3 - 2 * u);
                easeByClock(p, byImpact);
            }
            out.handBack = p.release && tti <= p.handBackAt && !in.keepTilt;
        } else if (descending_) {
            // No reading this frame: the descent goes on by the clock, the weight never backs off.
            sinceFirst_ += dt;
            if (in.vy > 0)
                reset(); // a web or a push took him up again
            else if (!in.keepTilt)
                easeByClock(p, 0.f);
        }
        if (in.keepTilt) {
            upright_ = 0;
            kept_ = descending_;
        }
        out.upright = upright_;
        return out;
    }
    // The touchdown frame: `grounded` seen after the air. Call update() first; the report is set on the frame the air ends.
    const Report& report() const { return report_; }
    // Forget the descent (the report stays until the next update).
    void reset() {
        descending_ = false;
        upright_ = 0;
        kept_ = byClock_ = false;
        lastTti_ = NAN;
        sinceFirst_ = 0;
    }

  private:
    void finish() {
        report_.valid = true;
        report_.firstTti = firstTti_;
        report_.firstHeight = firstHeight_;
        report_.firstVy = firstVy_;
        report_.lastTti = lastTti_;
        report_.actual = sinceFirst_;
        report_.errorMs = (firstTti_ - sinceFirst_) * 1000.f;
        report_.uprightAtTouch = upright_;
        report_.why = kept_ && upright_ <= 0.f ? Report::Why::kept
                      : upright_ <= 0.f        ? Report::Why::tooSoon
                      : byClock_               ? Report::Why::clock
                                               : Report::Why::impact;
    }
    // The upright weight: the time to impact's, or the clock's since the first prediction when it is further along.
    void easeByClock(const Params& p, float byImpact) {
        float byClock = 0.f;
        if (p.uprightClock > 0.f) {
            const float c = std::clamp(sinceFirst_ / p.uprightClock, 0.f, 1.f);
            byClock = c * c * (3 - 2 * c);
        }
        const float w = std::max(byImpact, byClock);
        if (w > upright_) {
            upright_ = w;
            byClock_ = byClock > byImpact;
        }
    }
    Report report_{};
    bool descending_{}, kept_{}, byClock_{};
    float firstTti_ = NAN, firstHeight_ = NAN, firstVy_ = NAN, lastTti_ = NAN, sinceFirst_{}, upright_{};
};
} // namespace spidy::landing_blend
