// SHOWTIME's camera rig (EXPERIMENTAL; the plan's "Camera and comfort", S1). Pure; no Windows. See docs/SHOWTIME.md.
//
// The rig is a level frame: a place and a yaw, never a roll or a pitch. The place is Spidey's pivot (his pelvis, smoothed) plus an
// offset: DISTANCE behind along the orbit heading, ORBIT ANGLE off his back, HEIGHT above the pivot. The pivot follows him on a
// critically damped spring in every action (no dead zone, no hold, no hop), so the picture is continuous at swing speed; the
// orbit itself is not smoothed (it is the player's own input). The yaw changes ONLY from the player's right stick.
//   - the follow: horizontal omega 4 / 6 / 9 rad/s (SOFT / NORMAL / TIGHT), vertical 3.5; the omega rises toward 12 as the lag
//     goes from 3 to 6 m, so a zip never loses him and the motion is still continuous. A look-ahead of 0.12 s of his velocity,
//     low-passed at 2 Hz.
//   - the orbit: the stick turns at the player's turn speed, eased in and out over 0.15 s; or in 30 degree steps under a 100 ms
//     blink (the yaw moves at the blink's black).
//   - the stick's up and down: the height, -0.5 to +1.5 m in all.
//   - collision: the game's own camera as a pull-in (below), and SHOWTIME RAY FAN's nearest hit (showtime_fan.hpp) as a
//     limit; either pulls in fast (omega 20) and lets out slowly (omega 3).
//   - SPEED LEAD (beta2): from 8 to 25 m/s the pivot is carried by up to speedLead of his velocity each step before its
//     spring, so the follow's lag at swing speed (2 v / omega: about 3.5 m logged) shrinks; slower, as before.
//   - the vignette: from the rig's speed or acceleration (never from head motion), LIGHT 25 % or STRONG 45 %.
//   - a camera nearer him than 0.8 m fades to black.
#pragma once
#include "math.hpp"
#include "showtime_fan.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::showtime {

inline constexpr float minDistance = .8f;       // below it the picture fades to black
inline constexpr float pelvisHeight = 1.f;      // his pivot above his feet
inline constexpr float lookAheadSeconds = .12f;
inline constexpr float heightMin = -.5f, heightMax = 1.5f;

struct RigParams {
    float distance = 2.5f;     // m
    float height = .3f;        // m above the pivot
    float angleDeg = 35.f;     // off his back
    int follow = 1;            // 0 SOFT, 1 NORMAL, 2 TIGHT
    bool snap = false;         // ORBIT TURN: false smooth, true 30 degree steps
    float turnRadians = 2.09f; // the player's smooth turn speed, rad/s
    int vignette = 1;          // 0 OFF, 1 LIGHT, 2 STRONG
    float speedLead = 0.f;     // SPEED LEAD: 0..1 of his velocity at 25 m/s (showtime_fan::speedLead)
};
inline float horizontalOmega(int follow) {
    return follow <= 0 ? 4.f : follow == 1 ? 6.f : 9.f;
}
inline float vignetteStrength(int level) {
    return level <= 0 ? 0.f : level == 1 ? .25f : .45f;
}
inline float ramp(float v, float from, float to) {
    return to > from ? std::clamp((v - from) / (to - from), 0.f, 1.f) : 0.f;
}
// A critically damped spring, exact for any step.
inline void spring(float& x, float& v, float target, float omega, float dt) {
    const float e = std::exp(-omega * dt);
    const float d = x - target, t = v + omega * d;
    x = target + (d + t * dt) * e;
    v = (v - omega * t * dt) * e;
}

class OrbitRig {
  public:
    struct Input {
        float seconds{};
        Vec3 feet{};           // his feet now
        float stickX{}, stickY{}; // the right stick: orbit, height
        bool turning{};        // the orbit is the player's now (out, not fading back)
        bool stockOk{};        // the game's own camera's place was sampled in the last half second
        Vec3 stock{};
        bool raysOk{};        // SHOWTIME RAY FAN: a fresh fan batch was read
        float rayRatio = 1.f; // ... and the share of the wanted distance it leaves (showtime_fan::select)
    };
    struct Output {
        Vec3 position{};
        float yaw{};
        float vignette{};  // 0..1 for the grade
        float blink{};     // 0..1 black, added to the fade
        float closeFade{}; // 0..1 black: the camera is nearer him than minDistance
        float speed{}, lag{}, omega{}, pull{}; // for the log
        float rayLimit{};                      // the fan's limit in force (m), 0: none
        float lead{};                          // the speed lead's share this step
        Vec3 fanFrom{}, fanTo{};               // where the next fan goes: his pelvis to the wanted place
    };
    // The cut: the pivot, the orbit heading (the heading the player was looking along) and the params.
    void begin(Vec3 feet, float yaw, const RigParams& p) {
        *this = {};
        yaw_ = targetYaw_ = yaw;
        pivot_ = pivotOf(feet);
        lastFeet_ = feet;
        seen_ = true;
        height_ = std::clamp(p.height, heightMin, heightMax);
        adjust_ = 0;
        distance_ = p.distance;
    }
    Output update(const Input& in, const RigParams& p) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        Output o;
        if (!finite(in.feet) || dt <= 0) {
            return last_;
        }
        // His velocity from where his feet went, low-passed at 2 Hz for the look-ahead.
        const Vec3 v = (in.feet - lastFeet_) / dt;
        lastFeet_ = in.feet;
        if (finite(v) && length(v) < 200.f) {
            const float k = 1 - std::exp(-6.2831853f * 2.f * dt);
            lookV_ = lookV_ + (v - lookV_) * k;
            const float kk = 1 - std::exp(-6.2831853f * 4.f * dt);
            speedV_ = speedV_ + (v - speedV_) * kk;
        }
        const float speed = length(speedV_);
        const float accel = std::abs(speed - lastSpeed_) / dt;
        lastSpeed_ = speed;
        accelLow_ += (accel - accelLow_) * (1 - std::exp(-6.2831853f * 2.f * dt));
        // The orbit: the player's input only.
        orbit(in, p, dt);
        // The height from the stick's up and down: 1 m/s, the total kept in range.
        if (in.turning && std::abs(in.stickY) > .2f)
            adjust_ += in.stickY * 1.f * dt;
        height_ = std::clamp(p.height + adjust_, heightMin, heightMax);
        adjust_ = height_ - p.height;
        // The pivot's spring.
        const Vec3 aim = pivotOf(in.feet) + lookV_ * lookAheadSeconds;
        const float dx = aim.x - pivot_.x, dz = aim.z - pivot_.z;
        const float lag = std::hypot(dx, dz);
        float omega = horizontalOmega(p.follow);
        omega += (12.f - omega) * ramp(lag, 3.f, 6.f);
        // SPEED LEAD: carry the pivot with him first, so the spring has less to catch up.
        const float lead = showtime_fan::speedLead(speed, p.speedLead);
        if (lead > 0 && finite(lookV_)) {
            pivot_.x += lookV_.x * lead * dt;
            pivot_.z += lookV_.z * lead * dt;
        }
        spring(pivot_.x, vx_, aim.x, omega, dt);
        spring(pivot_.z, vz_, aim.z, omega, dt);
        spring(pivot_.y, vy_, aim.y, 3.5f, dt);
        // Where the camera sits: behind along the orbit heading, ORBIT ANGLE to one side.
        const float a = yaw_ - p.angleDeg * 0.017453292f;
        float want = std::max(p.distance, minDistance);
        float pull = 0.f;
        if (in.stockOk)
            pull = stockPull(in.stock, a, want, dt);
        float limit = pull > 0 ? std::min(want, pull) : want;
        float rayLimit = 0.f;
        if (in.raysOk && std::isfinite(in.rayRatio) && in.rayRatio < 1.f) {
            rayLimit = want * std::clamp(in.rayRatio, 0.f, 1.f);
            limit = std::min(limit, rayLimit);
        }
        // Fast in, slow out.
        const float w = limit < distance_ ? 20.f : 3.f;
        distance_ = limit < distance_ ? limit + (distance_ - limit) * std::exp(-w * dt)
                                      : distance_ + (limit - distance_) * (1 - std::exp(-w * dt));
        o.position = {pivot_.x + std::sin(a) * distance_, pivot_.y + height_, pivot_.z + std::cos(a) * distance_};
        o.yaw = yaw_;
        const float room = std::hypot(o.position.x - pivotOf(in.feet).x, o.position.z - pivotOf(in.feet).z);
        o.closeFade = ramp(minDistance + .3f - room, 0.f, .3f);
        // The vignette: from the rig's own speed or acceleration.
        const float level = std::max({ramp(speed, 3.f, 12.f), ramp(accelLow_, 6.f, 20.f)});
        vignette_ += (level - vignette_) * (1 - std::exp(-4.f * dt));
        o.vignette = vignette_ * vignetteStrength(p.vignette);
        o.blink = blinkLevel();
        o.speed = speed;
        o.lag = lag;
        o.omega = omega;
        o.pull = pull;
        o.rayLimit = rayLimit;
        o.lead = lead;
        o.fanFrom = pivotOf(in.feet);
        o.fanTo = {pivot_.x + std::sin(a) * want, pivot_.y + height_, pivot_.z + std::cos(a) * want};
        last_ = o;
        return o;
    }
    float yaw() const {
        return yaw_;
    }
    float height() const {
        return height_;
    }
    bool begun() const {
        return seen_;
    }

  private:
    static Vec3 pivotOf(Vec3 feet) {
        return {feet.x, feet.y + pelvisHeight, feet.z};
    }
    // The game's own camera as a pull-in: when it sits within 30 degrees of the way we would sit, and nearer him than we want,
    // it says how far the world lets a camera back there. Its open-space distance is what it reached in the last 3 s.
    float stockPull(Vec3 stock, float heading, float want, float dt) {
        const float dx = stock.x - pivot_.x, dz = stock.z - pivot_.z;
        const float d = std::hypot(dx, dz);
        stockSeen_ = std::max(d, stockSeen_ * std::exp(-dt / 3.f));
        if (d < .01f)
            return 0.f;
        const float cosine = (dx * std::sin(heading) + dz * std::cos(heading)) / d;
        if (cosine < std::cos(.5235988f) || stockSeen_ < want * .5f)
            return 0.f;
        return d < stockSeen_ * .9f ? d : 0.f;
    }
    void orbit(const Input& in, const RigParams& p, float dt) {
        if (blink_ > 0) {
            const float before = blink_;
            blink_ = std::max(0.f, blink_ - dt);
            if (before > blinkSeconds_ * .5f && blink_ <= blinkSeconds_ * .5f)
                yaw_ = targetYaw_;
            return;
        }
        const float x = in.turning && std::isfinite(in.stickX) ? in.stickX : 0.f;
        if (p.snap) {
            if (!flick_ && std::abs(x) > .7f) {
                flick_ = true;
                targetYaw_ = yaw_ - (x > 0 ? 1.f : -1.f) * .5235988f;
                blink_ = blinkSeconds_;
            } else if (flick_ && std::abs(x) < .4f)
                flick_ = false;
            return;
        }
        // Eased in and out over 0.15 s: the stick's input is low-passed.
        const float target = std::abs(x) > .15f ? x : 0.f;
        ease_ += (target - ease_) * (1 - std::exp(-dt / .05f));
        yaw_ -= ease_ * p.turnRadians * dt;
        yaw_ = std::remainder(yaw_, 6.2831853f);
        targetYaw_ = yaw_;
    }
    float blinkLevel() const {
        if (blink_ <= 0)
            return 0.f;
        const float half = blinkSeconds_ * .5f;
        return std::clamp(1.f - std::abs(blink_ - half) / half, 0.f, 1.f);
    }
    static constexpr float blinkSeconds_ = .1f;
    Vec3 pivot_{}, lastFeet_{}, lookV_{}, speedV_{};
    float vx_{}, vy_{}, vz_{};
    float yaw_{}, targetYaw_{}, ease_{}, blink_{}, height_ = .3f, adjust_{}, distance_ = 2.5f;
    float lastSpeed_{}, accelLow_{}, vignette_{}, stockSeen_{};
    bool flick_{}, seen_{};
    Output last_{};
};
} // namespace spidy::showtime
