// VIEW YAW SOURCE: STABLE (rotation desync, Step A; the user, November 12
// 05:07, t70u). HYBRID parkour steering aimed the left stick at the yaw the
// head-locked view shows: the hero's HEAD BONE yaw + the headset's turn since
// the view latched + the Level R-stick turn. The head bone turns with the hero,
// so a held side push chased itself - the steering target turned as the hero
// turned toward it, and he ran in circles (26% of steer samples more than 45
// degrees apart in the 03:49 session, chase episodes of up to 106 s).
//
// STABLE steers from a world heading of its own instead:
//   - latched at the handoff from the old formula (head bone + HMD + turn);
//   - while the left stick is idle (no steering, so no loop) it keeps
//     following that live yaw, so it starts every run where he looks;
//   - while the left stick is held its base is FROZEN: only the real HMD yaw
//     (tracked - reference), the Level R-stick turn (animHeadTurn) and its own
//     R-stick turn (smooth deg/s or snap flicks, outside Level) change it -
//     never the head bone.
// The game camera follows the same heading (the caller feeds it to
// CameraLook). BODY (OLD) keeps the old formula for comparison.
//
// ANIM YAW SWAY (a little of the head bone's yaw added back on top) is Step B:
// Step A does not change what the eyes show, only what steering and the game
// camera aim at.
//
// Also here: the hero-vs-steer yaw watch (10 Hz lines, an event when the gap
// crosses 45 degrees) and the runaway-turn detector (the hero sweeping 90
// degrees or more one way within 1 s while the stick and the heading hold
// still). Pure; no Windows.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::stable_yaw {

inline constexpr float twoPi = 6.2831853f;
inline constexpr float degrees = 3.14159265f / 180.f;
inline float wrap(float a) {
    return std::remainder(a, twoPi);
}

enum class Source : int { body = 0, stable = 1 };
inline Source sourceOf(int setting) {
    return setting == 0 ? Source::body : Source::stable;
}
inline const char* sourceName(Source s) {
    return s == Source::body ? "body" : "stable";
}

// Filter time constant for the heading in STABLE (seconds): it no longer
// chases the hero, so it only smooths snaps and HMD jitter.
inline constexpr float stableTau = .1f;
// Left-stick tilt that counts as steering (hybrid_steer::deadzone).
inline constexpr float steerZone = .15f;
// The left stick idle this long before the base follows the live yaw again
// (a quick re-grip keeps the heading).
inline constexpr float relatchSeconds = .2f;
// Right stick: smooth-turn floor and snap hysteresis (as Level's turn).
inline constexpr float turnZone = .15f, flickOn = .7f, flickOff = .4f;

struct Input {
    float dt = 0;
    float headBoneYaw = 0;    // lookYaw of the animated head's forward (the old source)
    float hmdDelta = 0;       // trackedYaw - refYaw (0 without a reference)
    float headTurn = 0;       // animHeadTurn (Level's R-stick turn), 0 without a reference
    float leftTilt = 0;       // |left stick|
    float rightX = 0;         // the R-stick X steering may use (hybridInputR: its gates applied)
    bool levelTurns = false;  // Level owns the R stick (animHeadTurn already turns)
    int smoothDegPerSec = 0;  // values.smoothTurn
    int snapDeg = 0;          // values.snapTurn
};

// The steering heading. update() returns the unfiltered target yaw (radians,
// wrapped); filter it with hybrid_steer::YawFilter at stableTau.
struct Heading {
    float base = 0;       // the head-bone yaw latched (world)
    float ownTurn = 0;    // its own R-stick turn while steering outside Level
    float idleFor = 0;    // seconds the left stick has been idle
    bool set = false;
    bool frozen = false;  // base held (steering)
    bool flick = false;
    uint32_t relatches = 0;

    static float oldTarget(const Input& in) {
        return wrap(in.headBoneYaw + in.hmdDelta + in.headTurn);
    }

    float update(const Input& in) {
        const bool ok = std::isfinite(in.headBoneYaw) && std::isfinite(in.hmdDelta) && std::isfinite(in.headTurn);
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        const bool steering = std::isfinite(in.leftTilt) && in.leftTilt >= steerZone;
        idleFor = steering ? 0.f : idleFor + dt;
        if (!set) {
            if (!ok)
                return 0.f;
            base = in.headBoneYaw;
            ownTurn = 0;
            set = true;
            frozen = steering;
            ++relatches;
        } else if (!steering && idleFor >= relatchSeconds && ok) {
            // Idle: follow where he looks. Fold the own turn into the base so
            // nothing jumps when it is cleared.
            if (frozen || ownTurn != 0)
                ++relatches;
            base = in.headBoneYaw;
            ownTurn = 0;
            frozen = false;
        } else if (steering) {
            frozen = true;
        }
        // Own R-stick turn: only while steering, outside Level (Level's
        // animHeadTurn already turns the heading and the eyes).
        const float x = std::isfinite(in.rightX) ? in.rightX : 0.f;
        if (steering && !in.levelTurns) {
            if (in.smoothDegPerSec > 0) {
                if (std::abs(x) > turnZone)
                    ownTurn -= x * static_cast<float>(in.smoothDegPerSec) * degrees * dt;
                flick = false;
            } else if (in.snapDeg > 0) {
                if (!flick && std::abs(x) > flickOn) {
                    flick = true;
                    ownTurn -= (x > 0 ? 1.f : -1.f) * static_cast<float>(in.snapDeg) * degrees;
                } else if (flick && std::abs(x) < flickOff)
                    flick = false;
            }
            ownTurn = wrap(ownTurn);
        } else if (std::abs(x) < flickOff)
            flick = false;
        const float hmd = std::isfinite(in.hmdDelta) ? in.hmdDelta : 0.f;
        const float turn = std::isfinite(in.headTurn) ? in.headTurn : 0.f;
        return wrap(base + hmd + turn + ownTurn);
    }
    void reset() {
        *this = Heading{};
    }
};

// In STABLE, the right stick turns the heading itself while the left stick is
// held, so composeMove gets no extra turn request then (it would turn the hero
// twice). With the left stick idle the bounded facing request stays.
inline float composeTurn(Source s, float rightX, float leftTilt) {
    if (!std::isfinite(rightX))
        return 0.f;
    if (s == Source::stable && std::isfinite(leftTilt) && leftTilt >= steerZone)
        return 0.f;
    return rightX;
}

// Hero-vs-steer yaw watch: a line at `rate` Hz while steering, and an event
// when the gap crosses 45 degrees (out again below 40).
inline constexpr float gapOn = 45.f * degrees, gapOff = 40.f * degrees;
struct GapWatch {
    float sinceLine = 1e9f;
    bool wide = false;
    uint32_t crossings = 0;
    float wideFor = 0;
    enum class Edge : uint8_t { none, opened, closed };
    struct Out {
        bool line = false;
        Edge edge = Edge::none;
        float wideSeconds = 0; // on closed: how long it stayed wide
    };
    Out update(float dt, bool steering, float gap, float rateHz = 10.f) {
        Out out;
        const float d = std::isfinite(dt) ? std::max(dt, 0.f) : 0.f;
        sinceLine += d;
        if (!steering || !std::isfinite(gap)) {
            if (wide) {
                out.edge = Edge::closed;
                out.wideSeconds = wideFor;
            }
            wide = false;
            wideFor = 0;
            return out;
        }
        const float g = std::abs(wrap(gap));
        if (!wide && g > gapOn) {
            wide = true;
            wideFor = 0;
            ++crossings;
            out.edge = Edge::opened;
        } else if (wide && g < gapOff) {
            wide = false;
            out.edge = Edge::closed;
            out.wideSeconds = wideFor;
        }
        if (wide)
            wideFor += d;
        if (rateHz > 0 && sinceLine >= 1.f / rateHz - 1e-4f) {
            sinceLine = 0;
            out.line = true;
        }
        return out;
    }
};

// Runaway turn: the hero's yaw sweeps 90 degrees or more one way within 1 s
// while the left stick is held steady (direction within 20 degrees for the
// window and 0.3 s before), the right stick is idle and the player's own turn
// (`headingYaw`: the headset's yaw since the latch + the R-stick turns; NOT
// the view, which under BODY turns with the hero) moves less than 30 degrees.
// Rearms 1 s after a report.
inline constexpr float runawaySweep = 90.f * degrees, runawayWindow = 1.f, runawayLead = .3f,
                       stickSteady = 20.f * degrees, headingSteady = 30.f * degrees, rearmSeconds = 1.f;
struct Runaway {
    struct Sample {
        float t, hero, heading;
    };
    static constexpr size_t capacity = 256;
    std::array<Sample, capacity> ring{};
    size_t head = 0, count = 0;
    float clock = 0, heroUnwrapped = 0, headingUnwrapped = 0, lastHero = 0, lastHeading = 0;
    float stickAngle = 0, stickSince = 0; // the stick direction held and since when
    float lastReport = -1e9f;
    bool haveLast = false, haveStick = false;
    uint32_t reports = 0;

    struct Report {
        bool fired = false;
        float sweep = 0;   // radians, signed (the hero's yaw change)
        float seconds = 0; // over this long
        float headingMove = 0;
    };

    void clear() {
        count = 0;
        haveLast = false;
        haveStick = false;
    }

    Report update(float dt, float heroYaw, float headingYaw, bool steering, float stickX, float stickY,
                  float rightX) {
        Report r;
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .25f) : 0.f;
        clock += d;
        const bool rIdle = !std::isfinite(rightX) || std::abs(rightX) < turnZone;
        if (!steering || !rIdle || !std::isfinite(heroYaw) || !std::isfinite(headingYaw) ||
            !std::isfinite(stickX) || !std::isfinite(stickY)) {
            clear();
            return r;
        }
        const float angle = std::atan2(stickX, stickY);
        if (!haveStick || std::abs(wrap(angle - stickAngle)) > stickSteady) {
            stickAngle = angle;
            stickSince = clock;
            haveStick = true;
            count = 0; // the window restarts with the stick
        }
        if (!haveLast) {
            heroUnwrapped = heroYaw;
            headingUnwrapped = headingYaw;
            haveLast = true;
        } else {
            heroUnwrapped += wrap(heroYaw - lastHero);
            headingUnwrapped += wrap(headingYaw - lastHeading);
        }
        lastHero = heroYaw;
        lastHeading = headingYaw;
        ring[head] = {clock, heroUnwrapped, headingUnwrapped};
        head = (head + 1) % capacity;
        count = std::min(count + 1, capacity);
        // Oldest sample inside the window.
        const Sample* oldest = nullptr;
        for (size_t k = count; k > 0; --k) {
            const Sample& s = ring[(head + capacity - k) % capacity];
            if (clock - s.t <= runawayWindow + 1e-4f) {
                oldest = &s;
                break;
            }
        }
        if (!oldest || clock - stickSince < runawayWindow + runawayLead - 1e-4f ||
            clock - lastReport < rearmSeconds)
            return r;
        const float sweep = heroUnwrapped - oldest->hero;
        const float headingMove = headingUnwrapped - oldest->heading;
        if (std::abs(sweep) >= runawaySweep && std::abs(headingMove) < headingSteady) {
            r.fired = true;
            r.sweep = sweep;
            r.seconds = clock - oldest->t;
            r.headingMove = headingMove;
            lastReport = clock;
            ++reports;
        }
        return r;
    }
};

} // namespace spidy::stable_yaw
