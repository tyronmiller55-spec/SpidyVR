// WEB FIRE THROUGH A HANDOFF (the polish pass's follow-up, Nov 14, t70u: "Sometimes nothing happens, but I can see a switch to
// SpideyVR happening. We have to get the webs working from animation properly").
//
// What happened. In a midair animation the web press (the grip, or the trigger when WEB BUTTON is swapped) was the AIR CANCEL
// (animation_cancel.hpp, docs/OWNERSHIP.md "Midair animation + grip"): the owner flipped to SpidyVR on that frame and the press
// was CONSUMED - the gate blocked that hand's web until BOTH hands had been released, "so a cancel is never a web". The player saw
// the switch and no web. The last session (game-vr-20261114-104706) has 55 such cancels; many were followed within a frame by
// "vr->anim (land 0 ms)" (the press consumed on the ground), so nothing at all followed a squeeze; and the player's keeping the
// grip squeezed, the normal way to shoot, did
// nothing until he let go and pressed again.
//
// The fix. The cancel still flips the owner, on that frame, but the press is QUEUED through the handoff, never dropped: the
// swing gets it as a fresh rising edge on the next frame (the owner flip's re-origin of the tracking space, the "G->T" line,
// has happened by then, so the aim comes from the settled frame) and the web's own rules take it from there (the miss, the
// attach, the cooldowns). A player who let go in between still gets the web: the grip is held down for `minHold` for him.
// Pure.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::web_fire {
inline constexpr float maxWait = .35f; // a queued press that has not been fired by then is dropped, and says so
inline constexpr float minHold = .2f;  // a tap through a handoff is held down this long (a web needs a squeeze to shoot)
inline constexpr int settleFrames = 1; // frames between the cancel's frame and the fire

// The queue: one entry per hand.
class Queue {
  public:
    struct Out {
        unsigned pass{};    // hands whose web is no longer blocked: the swing sees them (they are fired this frame)
        unsigned force{};   // of those, hands the player has released: the grip is held down for him
        unsigned fired{};   // hands that began firing this frame
        unsigned expired{}; // hands whose queued press ran out of time
        float waited[2]{};  // how long a hand's press waited (valid for `fired`)
    };
    // The cancel's frame: these hands' presses wait.
    void queue(unsigned hands) {
        for (unsigned i = 0; i < 2; ++i)
            if (hands & (1u << i) && state_[i] == State::idle) {
                state_[i] = State::waiting;
                age_[i] = 0;
                frames_[i] = 0;
            }
    }
    // Every frame, after the gate: `physical` is the web buttons as squeezed now, `focused` whether the swing is being driven.
    Out update(float dt, unsigned physical, bool focused) {
        Out out;
        const float step = std::isfinite(dt) && dt > 0.f ? std::min(dt, .1f) : 0.f;
        for (unsigned i = 0; i < 2; ++i) {
            const unsigned bit = 1u << i;
            const bool held = (physical & bit) != 0;
            switch (state_[i]) {
            case State::idle:
                break;
            case State::waiting:
                age_[i] += step;
                ++frames_[i];
                if (!focused || age_[i] > maxWait) {
                    state_[i] = State::idle;
                    out.expired |= bit;
                } else if (frames_[i] > settleFrames) {
                    state_[i] = State::firing;
                    hold_[i] = 0;
                    out.fired |= bit;
                    out.waited[i] = age_[i];
                }
                if (state_[i] != State::firing)
                    break;
                [[fallthrough]];
            case State::firing:
                hold_[i] += step;
                // Done once the squeeze is let go of and the tap has been held long enough; the swing's own rules keep
                // the web as long as the player squeezes.
                if (!focused || (!held && hold_[i] >= minHold)) {
                    state_[i] = State::idle;
                    break;
                }
                out.pass |= bit;
                if (!held)
                    out.force |= bit;
                break;
            }
        }
        return out;
    }
    bool busy() const {
        return state_[0] != State::idle || state_[1] != State::idle;
    }
    unsigned waiting() const {
        return (state_[0] == State::waiting ? 1u : 0u) | (state_[1] == State::waiting ? 2u : 0u);
    }
    void reset() {
        *this = {};
    }

  private:
    enum class State : uint8_t { idle, waiting, firing };
    State state_[2]{};
    float age_[2]{}, hold_[2]{};
    int frames_[2]{};
};

// The gate's decision with the queued presses let through: a hand that is firing is not blocked.
struct Passed {
    unsigned webBlocked{}, shotBlocked{};
};
inline Passed letThrough(unsigned webBlocked, unsigned shotBlocked, unsigned pass) {
    return {webBlocked & ~pass, shotBlocked};
}

// The shot's line may be a steadied one (the marker's), but never more than `maxDegrees` off the controller's own line at the
// squeeze: the marker's low-pass lagged a flick in a fight by a good deal more, and the web went there.
inline constexpr float maxFilterLagDeg = 2.5f;
inline Vec3 limitLag(Vec3 raw, Vec3 steadied, float maxDegrees = maxFilterLagDeg) {
    const float lr = length(raw), ls = length(steadied);
    if (!finite(raw) || lr < .5f || !finite(steadied) || ls < .5f)
        return finite(raw) && lr >= .5f ? raw / lr : steadied;
    const Vec3 a = raw / lr, b = steadied / ls;
    const float c = std::clamp(dot(a, b), -1.f, 1.f);
    const float angle = std::acos(c);
    const float limit = maxDegrees * 3.14159265f / 180.f;
    if (angle <= limit)
        return b;
    // Turn `a` toward `b` by `limit`.
    const Vec3 side = b - a * c;
    const float ln = length(side);
    if (ln < 1e-6f)
        return a;
    return normalized(a * std::cos(limit) + side / ln * std::sin(limit));
}

// ---- The account of each press (the `web fire:` log line) ----
enum class Owner : uint8_t { anim, vr };
enum class Outcome : uint8_t { open, attached, missed, obstructed, blocked, dropped, unanswered };
inline const char* ownerName(Owner o) {
    return o == Owner::anim ? "ANIM" : "VR";
}
inline const char* outcomeName(Outcome o) {
    switch (o) {
    case Outcome::attached:
        return "web attached";
    case Outcome::missed:
        return "no web: missed";
    case Outcome::obstructed:
        return "no web: obstructed";
    case Outcome::blocked:
        return "no web: blocked";
    case Outcome::dropped:
        return "no web: dropped";
    case Outcome::unanswered:
        return "no web: no answer";
    default:
        return "open";
    }
}
// The angle (degrees) between two directions; -1 when either is no direction.
inline float angleDeg(Vec3 a, Vec3 b) {
    const float la = length(a), lb = length(b);
    if (!finite(a) || !finite(b) || la < 1e-4f || lb < 1e-4f)
        return -1.f;
    const float c = std::clamp(dot(a, b) / (la * lb), -1.f, 1.f);
    return std::acos(c) * 180.f / 3.14159265f;
}

// One press, from its rising edge to its answer. The swing's account (Data::attaches, misses, obstructed and the hand's web) is
// what answers it; `window` is how long it waits for one.
struct Press {
    bool open{};
    Owner owner{};
    bool queued{};
    float age{}, firedAfter{}; // since the squeeze; since it was let through the handoff (-1: not yet)
    float aimError{-1.f}, rigError{-1.f};
    uint64_t attaches{}, misses{}, obstructed{};
    bool sawFree{}; // this hand's web was seen off after the press (a web already on is not this press's)
    const char* reason{""};
    const char* source{""};
    Outcome outcome{};
    float distance{};
};
class Tracker {
  public:
    static constexpr float window = .6f;
    void press(unsigned hand, Owner owner, bool queued, float aimError, float rigError, uint64_t attaches, uint64_t misses,
               uint64_t obstructed, const char* reason = "") {
        auto& p = press_[hand & 1];
        p = {};
        p.open = true;
        p.owner = owner;
        p.queued = queued;
        p.firedAfter = queued ? -1.f : 0.f;
        p.aimError = aimError;
        p.rigError = rigError;
        p.attaches = attaches;
        p.misses = misses;
        p.obstructed = obstructed;
        p.reason = reason;
    }
    // The press was let through the handoff `after` seconds into its wait, aimed `aimError` degrees off the controller's current
    // line (`rigError`: how far the rig's line, before the frame's mapping, was off it), taken from `source`.
    void fired(unsigned hand, float after, float aimError, float rigError, const char* source) {
        auto& p = press_[hand & 1];
        if (!p.open)
            return;
        p.firedAfter = std::max(after, 0.f);
        p.aimError = aimError;
        p.rigError = rigError;
        p.source = source;
    }
    // The press was dropped before it could fire (a queue that ran out, no control).
    void dropped(unsigned hand, const char* why) {
        auto& p = press_[hand & 1];
        if (!p.open)
            return;
        p.open = false;
        p.outcome = Outcome::dropped;
        p.reason = why;
        done_[hand & 1] = true;
    }
    // Each frame, with the swing's counters and whether this hand's web is on something now. True with the press finished
    // (take it with `finished`).
    bool update(unsigned hand, float dt, uint64_t attaches, uint64_t misses, uint64_t /*obstructed*/, bool attachedNow,
                float anchorDistance) {
        auto& p = press_[hand & 1];
        if (!p.open)
            return false;
        p.age += std::isfinite(dt) && dt > 0.f ? std::min(dt, .1f) : 0.f;
        // Only once it has been let through: a queued press is waiting, not missing.
        const bool through = p.firedAfter >= 0.f;
        if (!attachedNow)
            p.sawFree = true;
        const auto& other = press_[1 - (hand & 1)];
        const bool shared = other.open && other.firedAfter >= 0.f;
        if (through && (attaches > p.attaches || (attachedNow && p.sawFree))) {
            p.outcome = Outcome::attached;
            p.distance = anchorDistance;
        } else if (through && misses > p.misses) {
            p.outcome = Outcome::missed;
            p.reason = shared ? "no surface within reach along the aim (the other hand's press was open too)"
                              : "no surface within reach along the aim";
        } else if (p.age > window) {
            p.outcome = Outcome::unanswered;
            p.reason = through ? "the swing answered neither an attach nor a miss" : "never let through";
        } else {
            return false;
        }
        p.open = false;
        done_[hand & 1] = true;
        return true;
    }
    bool finished(unsigned hand, Press& out) {
        if (!done_[hand & 1])
            return false;
        done_[hand & 1] = false;
        out = press_[hand & 1];
        return true;
    }
    bool open(unsigned hand) const {
        return press_[hand & 1].open;
    }

  private:
    Press press_[2]{};
    bool done_[2]{};
};
} // namespace spidy::web_fire
