// FULL head lock on the STABLE heading (rotation desync, Step B; the user,
// November 12 05:56). With HEAD LOCK: FULL and VIEW YAW SOURCE: STABLE the
// eyes keep the head bone's position, pitch and roll, but their yaw comes from
// a heading of the lock's own (`Lock::yaw`): latched from the head bone when
// the lock starts and turned only by the right stick (smooth deg/s or snap
// flicks) - never by the hero turning. ANIM YAW SWAY adds a share of the head
// bone's yaw sway back (the head's yaw relative to the hero's facing):
//   shown = lock + sway x wrap(headBone - hero)
// 100% is the old feel: the lock is off and the eyes ride the head bone as
// before (Step A steering).
//
// Every place the Full head anchor is resolved turns it the same way
// (`turnHead` / `turnModel`): the render thread's eyes (game_fp::frame), the XR
// thread's hand map (webs, balls, markers, shooter lock) and the pose job's
// arm IK targets, so hands and body stay where the view is.
//
// The hero follows the heading by TURN-IN-PLACE: standing (left stick idle,
// on the ground, the animation owning), once the heading is more than TURN-IN-
// PLACE ANGLE from his facing (or 20 degrees while the right stick turns), the
// game's walk stick points at the heading - a closed loop on his measured
// facing, so it cannot keep spinning or go the wrong way - until he is within
// 12 degrees. No facing address is written: the game turns him its own way, at
// its own speed. The view never waits for him.
//
// Also: the divergence metric (view vs hero and view vs head bone; mean, p95,
// max, share over 45 degrees) and the right-stick turn watch (the view's turn
// rate against the setting, the hero's). Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include "spidy/stable_yaw.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace spidy::view_yaw_lock {

using stable_yaw::degrees;
using stable_yaw::wrap;

inline float swayOf(int pct) {
    return static_cast<float>(std::clamp(pct, 0, 100)) / 100.f;
}
// The lock runs with HEAD LOCK: FULL, VIEW YAW SOURCE: STABLE and a sway under 100%.
inline bool lockOn(bool fullHead, bool stableSource, int swayPct) {
    return fullHead && stableSource && swayPct < 100;
}

// What the head anchors are turned with (published by the XR worker).
struct State {
    bool on = false;
    float yaw = 0;     // the lock's heading (world, rig yaw: 0 facing -z)
    float sway = 0;    // 0..1
    float heroYaw = 0; // the hero's facing (the actor's forward)
    // ANIM HEAD RIDE (camera_mode.hpp): 0..1, how far the shown yaw is taken
    // from the lock onto the head bone itself (eased entry and exit).
    float ride = 0;
    // SHOULDER LIMIT (camera_mode::ShoulderLimit): this frame's correction,
    // radians, added last (after the ride), so the view stays within the shoulders.
    float limit = 0;
};

// The yaw the eyes' anchor shows for a head bone yaw.
// The sway's share of the head bone's yaw off the hero's facing. A head turned within
// 45 degrees of the body is the animation's sway (all of it); past 90 the bone is not
// the animation's head over the hero - the SpidyVR body under the view on a landing's
// first frames, the hero facing the other way (06:37 session: 52 of 199 latches more
// than 135 degrees apart) - and none of it counts. Without the fade, a 180-degree gap
// put 36 degrees of sway (20%) into the latch that slid out as the game turned the
// hero, and the wrap at 180 could jump the view by 72.
inline float swayOffset(float boneYaw, float heroYaw, float sway) {
    if (!std::isfinite(boneYaw) || !std::isfinite(heroYaw) || !std::isfinite(sway))
        return 0.f;
    const float off = wrap(boneYaw - heroYaw);
    const float a = std::abs(off);
    constexpr float full = 45.f * degrees, none = 90.f * degrees;
    const float k = std::clamp(sway, 0.f, 1.f);
    if (k >= 1.f) // 100% is the head bone itself (the old feel; the lock is off there anyway)
        return off;
    const float fade = a <= full ? 1.f : a >= none ? 0.f : (none - a) / (none - full);
    return k * off * fade;
}

inline float shownYaw(const State& s, float boneYaw) {
    if (!s.on || !std::isfinite(s.yaw) || !std::isfinite(boneYaw))
        return boneYaw;
    const float hero = std::isfinite(s.heroYaw) ? s.heroYaw : boneYaw;
    float shown = wrap(s.yaw + swayOffset(boneYaw, hero, s.sway));
    if (std::isfinite(s.ride) && s.ride > 0.f)
        shown = wrap(shown + std::min(s.ride, 1.f) * wrap(boneYaw - shown));
    if (std::isfinite(s.limit) && s.limit != 0.f)
        shown = wrap(shown + s.limit);
    return shown;
}

inline float yawOf(Vec3 forward) {
    return std::atan2(-forward.x, -forward.z);
}

// A native head pose (rows right, down, forward, position; game_fp::frame's
// layout): its yaw, as game_fp reads it - the forward without its height, or,
// looking steeply up or down, where the crown points.
inline bool nativeYaw(const Mat4& m, float& yaw) {
    float ax = m[8], az = m[10];
    if (ax * ax + az * az < .09f) {
        const float s = m[9] < 0 ? -1.f : 1.f;
        ax = s * m[4];
        az = s * m[6];
    }
    if (!(ax * ax + az * az > 1e-6f) || !std::isfinite(ax) || !std::isfinite(az))
        return false;
    yaw = std::atan2(-ax, -az);
    return true;
}

// Its axes turned by `q` about its own position (the eyes' place kept).
inline Mat4 turned(const Mat4& m, Quat q) {
    Mat4 out = m;
    for (int r = 0; r < 3; ++r) {
        const Vec3 v = q.rotate({m[r * 4], m[r * 4 + 1], m[r * 4 + 2]});
        out[r * 4] = v.x;
        out[r * 4 + 1] = v.y;
        out[r * 4 + 2] = v.z;
    }
    return out;
}

// The world head anchor turned onto the shown yaw: pitch, roll and position
// stay the head bone's. `delta` (optional): the turn applied (radians).
inline bool turnHead(Mat4& head, const State& s, float* delta = nullptr) {
    if (delta)
        *delta = 0;
    float bone = 0;
    if (!s.on || !nativeYaw(head, bone))
        return false;
    const float d = wrap(shownYaw(s, bone) - bone);
    if (!std::isfinite(d))
        return false;
    head = turned(head, Quat::yaw(d));
    if (delta)
        *delta = d;
    return true;
}

// The pose job's anchor, in the actor's model space (`toWorld` turns model
// into world): the same world turn, expressed in the model.
inline bool turnModel(Mat4& model, Quat toWorld, const State& s, float* delta = nullptr) {
    if (delta)
        *delta = 0;
    if (!s.on)
        return false;
    const Vec3 f = toWorld.rotate({model[8], model[9], model[10]});
    const Vec3 down = toWorld.rotate({model[4], model[5], model[6]});
    const Mat4 world = {0, 0, 0, 0, down.x, down.y, down.z, 0, f.x, f.y, f.z, 0, 0, 0, 0, 1};
    float bone = 0;
    if (!nativeYaw(world, bone))
        return false;
    const float d = wrap(shownYaw(s, bone) - bone);
    if (!std::isfinite(d))
        return false;
    model = turned(model, toWorld.conjugate() * Quat::yaw(d) * toWorld);
    if (delta)
        *delta = d;
    return true;
}

// The published state (the render thread reads it in game_fp::frame).
inline std::atomic<bool> liveOn{false};
inline std::atomic<float> liveYaw{0}, liveSway{0}, liveHero{0}, liveRide{0}, liveLimit{0};
inline void publish(const State& s) {
    liveRide.store(s.ride, std::memory_order_relaxed);
    liveLimit.store(s.limit, std::memory_order_relaxed);
    liveYaw.store(s.yaw, std::memory_order_relaxed);
    liveSway.store(s.sway, std::memory_order_relaxed);
    liveHero.store(s.heroYaw, std::memory_order_relaxed);
    liveOn.store(s.on, std::memory_order_release);
}
inline State live() {
    State s;
    s.on = liveOn.load(std::memory_order_acquire);
    s.yaw = liveYaw.load(std::memory_order_relaxed);
    s.sway = liveSway.load(std::memory_order_relaxed);
    s.heroYaw = liveHero.load(std::memory_order_relaxed);
    s.ride = liveRide.load(std::memory_order_relaxed);
    s.limit = liveLimit.load(std::memory_order_relaxed);
    return s;
}

// The lock's heading. Right stick: smooth turning at the SMOOTH TURN rate, or
// SNAP TURN flicks (hysteresis as Level's: on past 0.7, re-armed under 0.4).
inline constexpr float turnZone = .15f, flickOn = .7f, flickOff = .4f;
struct Lock {
    bool set = false;
    float yaw = 0;
    bool flick = false;
    float sinceSnap = 1e9f;
    uint32_t latches = 0, snaps = 0;

    // Latched so the first frame shows exactly the head bone (no jump at the start).
    void latch(float boneYaw, float heroYaw, float sway) {
        if (!std::isfinite(boneYaw))
            return;
        const float h = std::isfinite(heroYaw) ? heroYaw : boneYaw;
        yaw = wrap(boneYaw - swayOffset(boneYaw, h, sway));
        set = true;
        flick = false;
        ++latches;
    }
    // This frame's turn (radians; + turns left, as the rig's yaw).
    float turn(float dt, float x, int smoothDegPerSec, int snapDeg) {
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        sinceSnap += d;
        if (!set || !std::isfinite(x))
            return 0.f;
        float by = 0;
        if (smoothDegPerSec > 0) {
            if (std::abs(x) > turnZone)
                by = -x * static_cast<float>(smoothDegPerSec) * degrees * d;
            flick = false;
        } else if (snapDeg > 0) {
            if (!flick && std::abs(x) > flickOn) {
                flick = true;
                by = -(x > 0 ? 1.f : -1.f) * static_cast<float>(snapDeg) * degrees;
                sinceSnap = 0;
                ++snaps;
            } else if (flick && std::abs(x) < flickOff)
                flick = false;
        }
        yaw = wrap(yaw + by);
        return by;
    }
    // The right stick is turning (held past the zone with smooth turning, or a snap within 0.3 s).
    bool turning(float x, int smoothDegPerSec) const {
        if (smoothDegPerSec > 0)
            return std::isfinite(x) && std::abs(x) > turnZone;
        return sinceSnap < .3f;
    }
    void reset() {
        *this = Lock{};
    }
};

// TURN-IN-PLACE.
inline constexpr float followAngle = 20.f * degrees; // while the right stick turns
inline constexpr float stopAngle = 12.f * degrees;
inline constexpr float giveUpSeconds = 2.5f, cooldownSeconds = 1.f;
inline constexpr float pushTilt = .35f, pushTiltHard = .5f; // harder after 0.3 s without 5 degrees
inline constexpr float forceWaitSeconds = .4f, forcePauseSeconds = .25f; // facing check turns
inline float angleOf(int setting) {
    return setting > 0 ? static_cast<float>(std::clamp(setting, 1, 180)) * degrees : 0.f;
}
struct TurnInPlace {
    enum class End : uint8_t { none, done, gaveUp, interrupted };
    struct In {
        float dt = 0;
        bool allowed = false; // standing, left stick idle, grounded, the animation owns, no fight
        float heading = 0;    // where the player looks (the lock + the headset's turn)
        float heroYaw = 0;
        float angle = 0;      // TURN-IN-PLACE ANGLE (radians); 0: off
        bool rTurning = false;
        // FACING CHECK (facing_check.hpp): turn him to forceYaw now, whatever the angle and
        // the cooldown; forceAllowed replaces `allowed` for it (a landing's frames: the
        // animation still settling, the hero may still be moving).
        bool force = false;
        float forceYaw = 0;
        bool forceAllowed = false;
    };
    struct Out {
        bool push = false;
        float pushYaw = 0;   // the world yaw the walk stick points at
        float tilt = 0;      // its tilt
        bool started = false;
        End ended = End::none;
        float gap = 0;       // heading - hero now
        float startGap = 0;  // at the start (on started / ended)
        float swept = 0;     // the hero's turn (on ended)
        float seconds = 0;   // how long (on ended)
        bool wrongWay = false; // on ended: he went more than 20 degrees the other way at some point
        bool byStick = false;  // started by the right stick (not the headset / drift)
        bool forced = false;   // on started / ended: a facing check's turn
        bool dropped = false;  // a facing check's turn that never got to start (0.4 s)
    };
    bool turning = false;
    float elapsed = 0, swept = 0, startGap = 0, lastHero = 0, cooldown = 0, worst = 0;
    bool byStick = false;
    bool forced = false, forcePending = false;
    float forcedYaw = 0, forceWait = 0, paused = 0;
    uint32_t starts = 0, done = 0, gaveUp = 0, interrupted = 0, wrongWays = 0, forcedStarts = 0;

    Out update(const In& in) {
        Out out;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        cooldown = std::max(0.f, cooldown - dt);
        if (in.force && std::isfinite(in.forceYaw)) {
            forcePending = true;
            forcedYaw = wrap(in.forceYaw);
            forceWait = 0;
            if (turning) { // retarget the turn under way
                forced = true;
                forcePending = false;
            }
        }
        const bool wantForce = forced || forcePending;
        const float heading = wantForce ? forcedYaw : in.heading;
        const bool ok = (wantForce ? in.forceAllowed : in.allowed && in.angle > 0) && std::isfinite(heading) &&
                        std::isfinite(in.heroYaw);
        out.gap = ok ? wrap(heading - in.heroYaw) : 0.f;
        if (!ok) {
            if (forcePending && !turning) {
                forceWait += dt;
                if (forceWait > forceWaitSeconds) {
                    forcePending = false;
                    out.dropped = true;
                }
                return out;
            }
            // A forced turn rides out a short gap (a landing's frames) before it stops.
            if (turning && forced && paused < forcePauseSeconds) {
                paused += dt;
                return out;
            }
            if (turning)
                finish(out, End::interrupted);
            return out;
        }
        paused = 0;
        if (!turning) {
            const bool past = std::abs(out.gap) > in.angle && in.angle > 0;
            const bool follow = in.rTurning && std::abs(out.gap) > followAngle;
            const bool force = forcePending;
            if (force && std::abs(out.gap) < stopAngle) {
                forcePending = false; // already facing it
            } else if (force || (past && cooldown <= 0) || follow) {
                turning = true;
                forced = force;
                forcePending = false;
                elapsed = swept = worst = 0;
                startGap = out.gap;
                lastHero = in.heroYaw;
                byStick = !force && follow && !past;
                ++starts;
                forcedStarts += force ? 1u : 0u;
                out.started = true;
                out.startGap = startGap;
                out.byStick = byStick;
                out.forced = force;
            }
        } else {
            elapsed += dt;
            swept += wrap(in.heroYaw - lastHero);
            lastHero = in.heroYaw;
            const float against = startGap > 0 ? -swept : swept;
            worst = std::max(worst, against);
            if (std::abs(out.gap) < stopAngle) {
                finish(out, End::done);
                return out;
            }
            if (elapsed > giveUpSeconds && !in.rTurning) {
                finish(out, End::gaveUp);
                cooldown = cooldownSeconds;
                return out;
            }
        }
        if (turning) {
            out.push = true;
            out.pushYaw = heading;
            out.tilt = elapsed > .3f && std::abs(swept) < 5.f * degrees ? pushTiltHard : pushTilt;
        }
        return out;
    }
    void reset() {
        turning = forced = forcePending = false;
        cooldown = paused = forceWait = 0;
    }
    // A relatch of the lock resets the look's turn, never a facing check's.
    void resetLook() {
        if (!forced && !forcePending)
            reset();
    }

private:
    void finish(Out& out, End why) {
        out.ended = why;
        out.forced = forced;
        forced = false;
        paused = 0;
        out.startGap = startGap;
        out.swept = swept;
        out.seconds = elapsed;
        out.byStick = byStick;
        out.wrongWay = worst > 20.f * degrees;
        wrongWays += out.wrongWay ? 1u : 0u;
        (why == End::done ? done : why == End::gaveUp ? gaveUp : interrupted) += 1;
        turning = false;
    }
};
inline const char* endName(TurnInPlace::End e) {
    switch (e) {
    case TurnInPlace::End::done: return "done";
    case TurnInPlace::End::gaveUp: return "gave up";
    case TurnInPlace::End::interrupted: return "interrupted";
    default: return "none";
    }
}

// Divergence: |view - hero| and |view - head bone| (degrees), summarized per window.
struct Divergence {
    std::array<uint32_t, 181> viewHero{}, viewBone{};
    uint32_t n = 0;
    double sumHero = 0, sumBone = 0;
    float maxHero = 0, maxBone = 0, clock = 0;
    struct Summary {
        uint32_t samples = 0;
        float meanHero = 0, p95Hero = 0, maxHero = 0, over45 = 0; // over45: share 0..1
        float meanBone = 0, p95Bone = 0, maxBone = 0;
    };
    void add(float dt, float viewMinusHero, float viewMinusBone) {
        clock += std::isfinite(dt) ? std::max(dt, 0.f) : 0.f;
        if (!std::isfinite(viewMinusHero) || !std::isfinite(viewMinusBone))
            return;
        const float h = std::abs(wrap(viewMinusHero)) / degrees, b = std::abs(wrap(viewMinusBone)) / degrees;
        ++viewHero[std::min<size_t>(180, static_cast<size_t>(h + .5f))];
        ++viewBone[std::min<size_t>(180, static_cast<size_t>(b + .5f))];
        ++n;
        sumHero += h;
        sumBone += b;
        maxHero = std::max(maxHero, h);
        maxBone = std::max(maxBone, b);
    }
    static float p95(const std::array<uint32_t, 181>& bins, uint32_t n) {
        if (!n)
            return 0;
        const uint32_t want = (n * 95 + 99) / 100;
        uint32_t seen = 0;
        for (size_t d = 0; d < bins.size(); ++d) {
            seen += bins[d];
            if (seen >= want)
                return static_cast<float>(d);
        }
        return 180;
    }
    Summary summary() const {
        Summary s;
        s.samples = n;
        if (!n)
            return s;
        s.meanHero = static_cast<float>(sumHero / n);
        s.meanBone = static_cast<float>(sumBone / n);
        s.p95Hero = p95(viewHero, n);
        s.p95Bone = p95(viewBone, n);
        s.maxHero = maxHero;
        s.maxBone = maxBone;
        uint32_t over = 0;
        for (size_t d = 46; d < viewHero.size(); ++d)
            over += viewHero[d];
        s.over45 = static_cast<float>(over) / n;
        return s;
    }
    void reset() {
        *this = Divergence{};
    }
};

// One right-stick turn (held, or a snap) from start to release: the view's
// turn against the setting, and the hero's.
struct TurnWatch {
    bool active = false;
    float seconds = 0, view = 0, hero = 0, lastHero = 0, idle = 0;
    uint32_t snaps = 0;
    struct Out {
        bool ended = false;
        float seconds = 0, viewDeg = 0, heroDeg = 0, viewRate = 0;
        uint32_t snaps = 0;
    };
    Out update(float dt, bool turningNow, float viewDelta, float heroYaw, bool snapped) {
        Out out;
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (turningNow && !active) {
            active = true;
            seconds = view = hero = idle = 0;
            snaps = 0;
            lastHero = heroYaw;
        }
        if (!active)
            return out;
        seconds += d;
        view += std::isfinite(viewDelta) ? viewDelta : 0.f;
        if (std::isfinite(heroYaw) && std::isfinite(lastHero))
            hero += wrap(heroYaw - lastHero);
        lastHero = heroYaw;
        snaps += snapped ? 1u : 0u;
        idle = turningNow ? 0.f : idle + d;
        if (idle >= .5f) { // the hero's follow-through counted for half a second
            out.ended = true;
            const float held = std::max(seconds - idle, 1e-3f);
            out.seconds = held;
            out.viewDeg = view / degrees;
            out.heroDeg = hero / degrees;
            out.viewRate = std::abs(out.viewDeg) / held;
            out.snaps = snaps;
            active = false;
        }
        return out;
    }
};

// THE 90-DEGREE RULE (the user, Nov 13: "a hard, global restriction about the
// head turning past 90 in either way. It should never happen"). The view the
// player sees (the shown yaw + the headset's own yaw) never points more than
// limitDegrees off the hero's facing. Whatever made the gap - the headset
// turned, the heading held while the game turned him (a fight, a landing, a
// free run, a rear snap's turn) - the excess comes off the anchors' yaw for
// this frame only:
//   d = unwrap(shown + hmd - hero), c = clamp(d, -L, L) - d, published yaw += c
// Nothing reads the correction back (the heading, the stick, the turn, the
// hybrid gaze keep theirs), so it cannot feed a loop: the moment the gap is
// back under L, c is 0 again. d is unwrapped against the frame before, so a
// gap crossing 180 keeps its side and never flips the view across. The eyes,
// the hand map and the arms' anchor read the published yaw, so all three keep
// the same rule. SpidyVR's own air (the rig's body under the headset) needs
// none: the lock is off there.
struct ViewLimit {
    static constexpr float limitDegrees = 90.f;
    float last = 0;   // the gap before it was held, radians (unwrapped)
    bool primed = false;
    unsigned held = 0; // frames held at the limit (for the log)
    float worst = 0;   // the largest excess held, radians (for the log)
    // The correction (radians) for this frame's view and hero yaw; 0 when
    // either is unknown (and the history starts again).
    float correction(float viewYaw, float heroYaw) {
        if (!std::isfinite(viewYaw) || !std::isfinite(heroYaw)) {
            primed = false;
            return 0.f;
        }
        const float raw = wrap(viewYaw - heroYaw);
        float d = primed ? last + wrap(raw - last) : raw;
        // Two turns away is the same gap: keep it within one turn of zero.
        if (std::abs(d) > 2 * 3.14159265f)
            d = raw;
        last = d;
        primed = true;
        const float limit = limitDegrees * degrees;
        const float c = std::clamp(d, -limit, limit) - d;
        if (c != 0.f) {
            ++held;
            worst = std::max(worst, std::abs(c));
        }
        return c;
    }
    void reset() {
        primed = false;
        last = 0;
    }
};

} // namespace spidy::view_yaw_lock
