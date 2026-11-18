// PUNCH VIEW EASE (the user, November 12 01:13, t66u: "hold and ease"). At a
// punch's ANIM -> VR handoff the position lock puts tracking over the actor's
// feet at once, and the view used to jump there with it (median 0.69 m in the
// 22:38 session). Instead the whole tracked placement - eyes, head, hands,
// webs and the body drawn on them, as GameTrackingRig's other placement
// offsets (standOff, runViewOffset, headPush) - starts offset by what the
// switch would have moved the eyes, so the first VR frame shows the eyes where
// the last animated frame did, and that offset eases to zero:
//   - over `duration` with an ease-out (cubic: fast first, settling last);
//   - never slower than a motion-masked bleed of bleedBase + bleedPerSpeed x
//     the player's speed (EngineBot's 0.10 m/s + 0.2 x speed), so it finishes
//     sooner while he moves and the motion hides it;
//   - ended at once (cancel) on a teleport, a large turn, a recenter, another
//     handoff, or anything not finite.
// The lock underneath (the rig's origin over the feet) is not changed: only
// the placement the frame is drawn from carries the offset. Pure; no Windows.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::view_ease {

inline constexpr float bleedBase = .10f, bleedPerSpeed = .2f;
// Larger than any lunge in one frame: a teleport.
inline constexpr float teleportMetres = 5.f;
// A snap turn (or more) in one frame, or this much turn since the start.
inline constexpr float turnFrame = .35f, turnTotal = .8f;
// Offsets smaller than this are no ease; larger than this are no handoff's.
inline constexpr float minOffset = .01f, maxOffset = 3.f;
// HANDOFF SOFT PLACE (the 2 AM log of Nov 18, approved 1:53 PM): an offset over maxOffset (6 takeovers of 3.9 to
// 11.2 m had no ease at all, the view cut there in one frame) keeps its last `farKeep` metres (begin()'s argument;
// 0 the old refusal) and eases those; the rest is placed at once. Past farMax it is a teleport: none.
inline constexpr float farMax = 60.f;

enum class End : uint8_t { none, finished, teleport, turn, handoff, recenter, invalid, reset, landed };
inline const char* endName(End e) {
    switch (e) {
    case End::finished:
        return "finished";
    case End::teleport:
        return "cancelled (teleport)";
    case End::turn:
        return "cancelled (large turn)";
    case End::handoff:
        return "cancelled (another handoff)";
    case End::recenter:
        return "cancelled (recenter)";
    case End::invalid:
        return "cancelled (invalid)";
    case End::reset:
        return "cancelled (tracking reset)";
    case End::landed:
        return "cancelled (landed)";
    default:
        return "none";
    }
}

// The ease's weight left (1 at the start, 0 at the end): (1 - t)^3.
// HANDOFF EASE CURVE (handoff_ease_curve 1): what is left of the offset on a smootherstep - at rest at both ends, so the
// glide never jolts in (the cubic ease-out starts at three times the average speed).
inline float remainingGentle(float elapsed, float duration) {
    if (!(duration > 0))
        return 0.f;
    const float u = std::clamp(elapsed / duration, 0.f, 1.f);
    return 1.f - u * u * u * (u * (u * 6.f - 15.f) + 10.f);
}
inline float remaining(float elapsed, float duration) {
    if (!(duration > 0) || !std::isfinite(elapsed))
        return 0.f;
    const float u = std::clamp(elapsed / duration, 0.f, 1.f);
    const float r = 1 - u;
    return r * r * r;
}

class Ease {
  public:
    // offset: shown eyes minus where tracking puts them after the switch
    // (world). Returns whether an ease began.
    // HEAD HEIGHT FADE (head_height_fade_ms; the owner, Nov 15): with `verticalSeconds` longer than the ease, the
    // offset's height (the animation head's height over where tracking puts the eyes: median 0.27 m for a seated
    // player) fades on its own over `verticalSeconds` (the same ease-out, no motion bleed) while the level part eases
    // as before. 0 (or shorter): one offset, as before.
    // PUNCH CAMERA LOCK (punch_camera_lock_ms; the owner, Nov 15: "keep the camera locked"): `holdSeconds` holds the
    // whole offset (no bleed) before the ease starts, so a punch that lands back in the animation within it never
    // moves the view; the turn and teleport cancels still apply.
    bool begin(Vec3 offset, float durationSeconds, float verticalSeconds = 0, float holdSeconds = 0, bool gentle = false,
               float farKeep = 0) {
        if (active_)
            stop(End::handoff);
        farPlaced_ = 0;
        if (!finite(offset) || !std::isfinite(durationSeconds) || durationSeconds <= 0)
            return false;
        float m = length(offset);
        if (m > maxOffset && std::isfinite(farKeep) && farKeep > minOffset && farKeep <= maxOffset && m <= farMax) {
            farPlaced_ = m - farKeep;
            offset = offset * (farKeep / m);
            m = farKeep;
        }
        if (!(m >= minOffset) || m > maxOffset)
            return false;
        split_ = std::isfinite(verticalSeconds) && verticalSeconds > durationSeconds;
        Vec3 level = offset;
        vStart_ = vMagnitude_ = 0;
        vDuration_ = 0;
        if (split_) {
            vStart_ = vMagnitude_ = offset.y;
            vDuration_ = verticalSeconds;
            level.y = 0;
        }
        const float lm = length(level);
        dir_ = lm > 1e-6f ? level / lm : Vec3{};
        start_ = magnitude_ = lm;
        duration_ = durationSeconds;
        gentle_ = gentle;
        offset0_ = offset;
        hold_ = std::isfinite(holdSeconds) ? std::clamp(holdSeconds, 0.f, 2.f) : 0.f;
        held_ = 0;
        elapsed_ = 0;
        turned_ = 0;
        haveLast_ = false;
        active_ = true;
        ++begins_;
        return true;
    }
    // One frame: seconds since the last, the player's feet and the rig's yaw
    // (for teleports and turns). Returns this frame's offset (zero when idle).
    Vec3 step(float seconds, Vec3 feet, float yaw) {
        if (!active_)
            return {};
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!finite(feet) || !std::isfinite(yaw)) {
            stop(End::invalid);
            return {};
        }
        float speed = 0;
        if (haveLast_) {
            const float moved = length(feet - lastFeet_);
            if (!(moved <= teleportMetres)) {
                stop(End::teleport);
                return {};
            }
            const float turn = std::abs(std::remainder(yaw - lastYaw_, 6.2831853f));
            turned_ += turn;
            if (turn > turnFrame || turned_ > turnTotal) {
                stop(End::turn);
                return {};
            }
            speed = dt > 1e-4f ? moved / dt : 0.f;
        }
        lastFeet_ = feet;
        lastYaw_ = yaw;
        haveLast_ = true;
        // PUNCH CAMERA LOCK: the whole offset, unchanged, through the hold.
        if (held_ < hold_) {
            held_ += dt;
            return dir_ * magnitude_ + Vec3{0, vMagnitude_, 0};
        }
        // The first VR frame holds the shown eyes exactly (elapsed 0), then it eases.
        const auto left = gentle_ ? remainingGentle : remaining;
        const float curve = start_ * left(elapsed_, duration_);
        const float vCurve = split_ ? vStart_ * left(elapsed_, vDuration_) : 0.f;
        elapsed_ += dt;
        // The motion bleed only with the old curve (it would cut the gentle start short).
        const float bled =
            gentle_ ? curve : magnitude_ - (bleedBase + bleedPerSpeed * std::min(speed, 60.f)) * dt;
        magnitude_ = std::max(0.f, std::min(curve, bled));
        vMagnitude_ = vCurve;
        const bool levelDone = magnitude_ <= 1e-3f || elapsed_ >= duration_;
        const bool heightDone = !split_ || std::abs(vMagnitude_) <= 1e-3f || elapsed_ >= vDuration_;
        if (levelDone && heightDone) {
            stop(End::finished);
            return {};
        }
        if (levelDone)
            magnitude_ = 0;
        if (heightDone)
            vMagnitude_ = 0;
        return dir_ * magnitude_ + Vec3{0, vMagnitude_, 0};
    }
    // HANDOFF GLIDE SPEED: a new length for the ease just begun (before its first frame), same offset, hold and curve.
    bool retime(float durationSeconds, float verticalSeconds) {
        if (!active_ || elapsed_ > 0 || held_ > 0)
            return false;
        const uint32_t begins = begins_;
        active_ = false;
        const float placed = farPlaced_;
        const bool ok = begin(offset0_, durationSeconds, verticalSeconds, hold_, gentle_);
        farPlaced_ = placed;
        begins_ = begins;
        return ok;
    }
    void cancel(End why) {
        if (active_)
            stop(why);
    }
    bool active() const {
        return active_;
    }
    // HANDOFF SOFT PLACE: of the last begin()'s offset, the metres placed at once (0: all eased).
    float farPlaced() const {
        return farPlaced_;
    }
    float startOffset() const {
        return start_;
    }
    float duration() const {
        return duration_;
    }
    float elapsed() const {
        return elapsed_;
    }
    float magnitude() const {
        return active_ ? magnitude_ : 0.f;
    }
    // HEAD HEIGHT FADE: the height still faded in (m, signed; 0 without the split), and its length (s).
    float height() const {
        return active_ ? vMagnitude_ : 0.f;
    }
    float heightSeconds() const {
        return split_ ? vDuration_ : 0.f;
    }
    float startHeight() const {
        return vStart_;
    }
    // PUNCH CAMERA LOCK: still holding (before the ease), and how long it has held (s).
    bool holding() const {
        return active_ && held_ < hold_;
    }
    float held() const {
        return held_;
    }
    float holdSeconds() const {
        return hold_;
    }
    bool gentle() const {
        return gentle_;
    }
    // How the last ease ended, and how many have ended (to log each once).
    End lastEnd() const {
        return lastEnd_;
    }
    unsigned ends() const {
        return ends_;
    }
    unsigned begins() const {
        return begins_;
    }

  private:
    float farPlaced_{};
    void stop(End why) {
        active_ = false;
        magnitude_ = 0;
        vMagnitude_ = 0;
        lastEnd_ = why;
        ++ends_;
    }
    bool active_{}, haveLast_{}, split_{};
    float vStart_{}, vMagnitude_{}, vDuration_{};
    float hold_{}, held_{};
    bool gentle_{};
    Vec3 offset0_{};
    Vec3 dir_{}, lastFeet_{};
    float start_{}, magnitude_{}, duration_{}, elapsed_{}, lastYaw_{}, turned_{};
    End lastEnd_ = End::none;
    unsigned ends_{}, begins_{};
};

} // namespace spidy::view_ease
