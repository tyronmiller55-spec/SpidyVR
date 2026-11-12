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
    bool begin(Vec3 offset, float durationSeconds) {
        if (active_)
            stop(End::handoff);
        if (!finite(offset) || !std::isfinite(durationSeconds) || durationSeconds <= 0)
            return false;
        const float m = length(offset);
        if (!(m >= minOffset) || m > maxOffset)
            return false;
        dir_ = offset / m;
        start_ = magnitude_ = m;
        duration_ = durationSeconds;
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
        // The first VR frame holds the shown eyes exactly (elapsed 0), then it eases.
        const float curve = start_ * remaining(elapsed_, duration_);
        elapsed_ += dt;
        const float bled = magnitude_ - (bleedBase + bleedPerSpeed * std::min(speed, 60.f)) * dt;
        magnitude_ = std::max(0.f, std::min(curve, bled));
        if (magnitude_ <= 1e-3f || elapsed_ >= duration_) {
            stop(End::finished);
            return {};
        }
        return dir_ * magnitude_;
    }
    void cancel(End why) {
        if (active_)
            stop(why);
    }
    bool active() const {
        return active_;
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
    void stop(End why) {
        active_ = false;
        magnitude_ = 0;
        lastEnd_ = why;
        ++ends_;
    }
    bool active_{}, haveLast_{};
    Vec3 dir_{}, lastFeet_{};
    float start_{}, magnitude_{}, duration_{}, elapsed_{}, lastYaw_{}, turned_{};
    End lastEnd_ = End::none;
    unsigned ends_{}, begins_{};
};

} // namespace spidy::view_ease
