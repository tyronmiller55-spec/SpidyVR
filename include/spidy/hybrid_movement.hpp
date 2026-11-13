#pragma once
// MOVEMENT MODE: HYBRID (the user, November 12 t75u: "the new movement frequently causes
// Spiderman to face the opposite direction of the headset. We need the headset to stay
// locked to Spiderman's head ... Since there is no backward run, we'll replace that with
// a snap turn anytime the left stick is pointed to the rear").
//
// - The view: on the hero's head bone, its yaw included. One HEADING (the right stick's)
//   is what he faces; while he faces it the view is exactly the head (no divergence).
//   While he is being brought onto it - a rear snap, a right-stick turn, a landing whose
//   game facing is the old one (the facing check) - the view holds the heading and the
//   head's yaw sway is faded out, then faded back in once he arrives (no pop). If he
//   cannot get there in 2.5 s the heading rejoins him (the view eases onto the head).
// - The left stick's front half (within 90 degrees of forward): he runs along the
//   heading. The game has no strafe or backpedal: its run always turns the hero toward
//   where he runs, so a sideways push cannot move him sideways without turning him (and,
//   head-locked, the view). The best that keeps the facing: the stick's forward share
//   (tilt x cos angle) runs him straight along the heading; turning is the right stick's.
// - The rear half: past 100 degrees one snap - the heading (and so the view, instantly
//   by default) goes to the stick's exact direction, and he runs that way at the stick's
//   tilt; under 80 degrees or neutral re-arms. The stick is read once it settles (or
//   after 60 ms) so a quick swing to the back snaps to where it lands, not to 100.
//   The hero turns at the game's own pace (no facing write exists): the view snaps, his
//   body comes round under it.
// - In SpidyVR's air the game's facing for the hero is the actor's own and nothing turns
//   it; a landing plays on it. HYBRID keeps it on the view in the air the way SOFT
//   LANDING already hands the game its stick there (the swing still moves the body), and
//   the facing check holds the heading through the landing.
#include "spidy/stable_yaw.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::hybrid_movement {

using stable_yaw::degrees;
using stable_yaw::wrap;

enum class Mode : uint8_t { hybrid = 0, stable = 1, old = 2 };
inline Mode modeOf(int v) {
    return v == 1 ? Mode::stable : v == 2 ? Mode::old : Mode::hybrid;
}
inline const char* modeName(Mode m) {
    return m == Mode::stable ? "STABLE" : m == Mode::old ? "OLD" : "HYBRID";
}
// The yaw source the steering below the mode uses: OLD is the head bone (BODY), the rest
// the stable heading. VIEW YAW SOURCE's own row is superseded.
inline stable_yaw::Source viewSource(int movementMode) {
    return modeOf(movementMode) == Mode::old ? stable_yaw::Source::body : stable_yaw::Source::stable;
}

inline constexpr float snapAngle = 100.f * degrees, rearmAngle = 80.f * degrees;
inline constexpr float snapTilt = .5f, neutralTilt = .25f, deadzone = .15f;
inline constexpr float settleSeconds = .06f, settleRate = 2.f * degrees; // per frame

// The stick's angle off forward, radians, + to the right (x right, y forward).
inline float stickAngle(float x, float y) {
    return std::atan2(x, y);
}
// The world yaw (rig convention, + to the left) the stick points at, from a heading.
inline float stickYaw(float heading, float x, float y) {
    return wrap(heading - stickAngle(x, y));
}

struct Gate {
    bool armed = true, holding = false, settling = false;
    float settle = 0, lastAngle = 0;
    uint32_t snaps = 0;
    struct Out {
        bool snapped = false;
        float snapAngle = 0; // the stick's angle at the snap (radians, + right)
        // The stick to send, heading-relative (x right, y forward) and its tilt.
        float x = 0, y = 0;
        bool drives = false; // the left stick runs him along the heading this frame
        bool rear = false;   // in the rear dead band / settling (no run)
    };
    Out update(float dt, float x, float y) {
        Out out;
        if (!std::isfinite(x) || !std::isfinite(y)) {
            x = y = 0;
        }
        const float tilt = std::min(std::hypot(x, y), 1.f);
        const float a = stickAngle(x, y);
        const float abs = std::abs(a);
        if (tilt < neutralTilt) {
            armed = true;
            holding = settling = false;
        } else if (abs < rearmAngle) {
            armed = true;
            holding = settling = false;
        }
        if (holding && tilt >= deadzone) {
            // Snapped: he runs along the new heading while the stick stays back.
            out.y = tilt;
            out.drives = true;
            lastAngle = a;
            return out;
        }
        if (armed && tilt >= snapTilt && abs > snapAngle) {
            if (!settling) {
                settling = true;
                settle = 0;
            } else {
                settle += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
            }
            const bool still = std::abs(wrap(a - lastAngle)) < settleRate;
            lastAngle = a;
            if ((still && settle > 0) || settle >= settleSeconds) {
                armed = false;
                holding = true;
                settling = false;
                ++snaps;
                out.snapped = true;
                out.snapAngle = a;
                out.y = tilt;
                out.drives = true;
                return out;
            }
            out.rear = true;
            return out;
        }
        settling = false;
        lastAngle = a;
        if (tilt >= deadzone && abs <= 90.f * degrees) {
            out.y = tilt * std::cos(a);
            out.drives = out.y > .05f;
        } else if (tilt >= deadzone) {
            out.rear = true; // 90-100 degrees, or the back half not armed
        }
        return out;
    }
    void reset() {
        armed = true;
        holding = settling = false;
    }
};

// The heading and the view's hold on it.
struct Heading {
    enum class Hold : uint8_t { none, turn, snap, facing };
    float yaw = 0;
    bool set = false;
    Hold hold = Hold::none;
    float age = 0, swayWeight = 1, snapFrom = 0, snapTo = 0, snapT = 1, snapSeconds = 0;
    float drivingOff = 0; // seconds the hero has run more than 100 degrees off it
    float rest = 0;       // the heading's fading offset from him once nothing holds it
    bool following = false;
    bool easing = false;
    float easeT = 0, easeFrom = 0, easeSeconds = 0;
    uint32_t arrived = 0, gaveUp = 0;

    static constexpr float arriveAngle = 12.f * degrees, giveUpSeconds = 2.5f, followTau = .12f,
                           swayInSeconds = .3f;
    // LANDING VIEW HOLD (the 5061b4a playtest, Nov 12: the view stayed turned away from his body for
    // about a second after a landing, and at the 2.5 s give-up it snapped by a median of 56 degrees):
    // a facing hold (a landing, a takeover, an anim re-entry) lasts In::facingHoldSeconds (600 ms by
    // default), not 2.5 s, and only quickHoldSeconds in a fight or with the left stick held (the run brings
    // him round); the release then eases the heading onto him over easeSeconds (smoothstep) instead of
    // snapping.
    static constexpr float quickHoldSeconds = .15f;

    struct In {
        float dt = 0;
        bool heroOk = false;
        float hero = 0;
        bool driving = false; // the left stick runs him along it
        bool steerable = false; // the walk stick can turn him (grounded, the animation's)
        // LANDING VIEW HOLD: a facing hold's length (seconds; 2.5 is the old give-up), whether it is
        // cut to quickHoldSeconds (a fight, a dodge, the left stick held), and the release's ease
        // (seconds; 0 snaps onto him).
        float facingHoldSeconds = giveUpSeconds;
        bool quick = false;
        float easeSeconds = 0.f;
    };
    struct Out {
        bool heroTurn = false; // TURN-IN-PLACE forced to `yaw` (standing)
        Hold released = Hold::none;
        bool gaveUp = false;
        bool eased = false; // the hold timed out and the heading is easing onto him
    };
    void latch(float y) {
        yaw = wrap(y);
        set = true;
        hold = Hold::none;
        swayWeight = 1;
        snapT = 1;
        following = false;
        easing = false;
    }
    void begin(Hold h, float to, float seconds = 0) {
        if (!std::isfinite(to))
            return;
        if (h == Hold::snap && seconds > 0) {
            snapFrom = yaw;
            snapTo = wrap(to);
            snapT = 0;
            snapSeconds = seconds;
        } else {
            snapT = 1;
            yaw = wrap(to);
        }
        if (static_cast<int>(h) >= static_cast<int>(hold) || hold == Hold::none)
            hold = h;
        age = 0;
        set = true;
        following = false;
        easing = false;
    }
    // The right stick's turn of the heading (radians, this frame).
    void turn(float by) {
        if (!std::isfinite(by) || by == 0)
            return;
        yaw = wrap(yaw + by);
        if (snapT < 1)
            snapTo = wrap(snapTo + by);
        if (hold == Hold::none)
            hold = Hold::turn;
        age = 0;
        following = false;
        easing = false;
    }
    Out update(const In& in) {
        Out out;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        if (snapT < 1) {
            snapT = snapSeconds > 0 ? std::min(1.f, snapT + dt / snapSeconds) : 1.f;
            const float s = snapT * snapT * (3 - 2 * snapT);
            yaw = wrap(snapFrom + wrap(snapTo - snapFrom) * s);
        }
        if (!in.heroOk || !std::isfinite(in.hero)) {
            swayWeight = hold == Hold::none ? std::min(1.f, swayWeight + dt / swayInSeconds) : 0.f;
            return out;
        }
        const float gap = wrap(yaw - in.hero);
        if (hold != Hold::none) {
            age += dt;
            if (std::abs(gap) < arriveAngle && snapT >= 1) {
                out.released = hold;
                hold = Hold::none;
                ++arrived;
            } else if (age > holdLimit(in)) {
                out.released = hold;
                out.gaveUp = true;
                // The view comes onto him over easeSeconds (smoothstep), not at once. Driving, the
                // heading stays where the player looks and the run brings him: nothing to ease.
                const bool drive = in.driving && in.steerable;
                easing = !drive && in.easeSeconds > 0.f;
                easeT = 0;
                easeFrom = gap;
                easeSeconds = in.easeSeconds;
                out.eased = easing;
                if (!drive) {
                    // following starts here with no offset left over when the ease is off (the snap).
                    following = true;
                    rest = easing ? gap : 0.f;
                }
                hold = Hold::none;
                ++gaveUp;
            }
        }
        if (hold == Hold::none) {
            if (in.driving && in.steerable) {
                // He runs along it: the walk stick holds his facing; a hero that cannot
                // follow for 1.5 s (a wall, a scripted move) is followed instead.
                drivingOff = std::abs(gap) > 100.f * degrees ? drivingOff + dt : 0.f;
                if (drivingOff > 1.5f) {
                    drivingOff = 0;
                    yaw = in.hero;
                    out.gaveUp = true;
                    ++gaveUp;
                }
                following = false;
                easing = false;
            } else {
                drivingOff = 0;
                // Nothing to bring him onto: the heading is his facing (the view the head).
                // What was left when that began fades (0.12 s); his own turns are followed
                // at once, never lagged.
                if (!following) {
                    rest = gap;
                    following = true;
                }
                if (easing) {
                    easeT = std::min(1.f, easeT + dt / std::max(easeSeconds, .001f));
                    rest = easeFrom * (1.f - easeT * easeT * (3.f - 2.f * easeT));
                    if (easeT >= 1.f) {
                        easing = false;
                        rest = 0;
                    }
                } else {
                    rest *= std::exp(-dt / followTau);
                }
                if (std::abs(rest) < .05f * degrees)
                    rest = 0;
                yaw = wrap(in.hero + rest);
            }
            swayWeight = std::min(1.f, swayWeight + dt / swayInSeconds);
        } else {
            swayWeight = 0;
            following = false;
        }
        out.heroTurn = hold != Hold::none && in.steerable && !in.driving;
        return out;
    }
    void reset() {
        *this = Heading{};
    }

  private:
    // How long the hold in force may last for this frame's inputs.
    float holdLimit(const In& in) const {
        if (hold != Hold::facing)
            return giveUpSeconds;
        return in.quick ? std::min(quickHoldSeconds, in.facingHoldSeconds) : in.facingHoldSeconds;
    }
};
inline const char* holdName(Heading::Hold h) {
    switch (h) {
    case Heading::Hold::turn: return "turn";
    case Heading::Hold::snap: return "rear snap";
    case Heading::Hold::facing: return "facing check";
    default: return "none";
    }
}

// HYBRID in SpidyVR's air: the walk stick toward the view while the actor's facing is
// more than this off it (no web, no wall, the swing owns him), so the landing plays on it.
inline constexpr float airSyncAngle = 20.f * degrees, airSyncTilt = .3f;
// RETIRED (the 5061b4a playtest, Nov 12): in 170 syncs the game's facing did not move (median 0 deg,
// 19% moved at all) and the landings still came down a median 57 deg off the view (37 deg at the
// sync's start): the game does not turn its actor from a walk stick in SpidyVR's air. The gate still
// runs for the log (how long the gap lasted); the stick no longer goes out. true restores it.
inline constexpr bool airSyncStick = false;
inline bool airSyncWanted(bool hybrid, bool vrOwned, bool airborne, bool webbed, bool wall, float actorYaw,
                          float viewYaw) {
    if (!hybrid || !vrOwned || !airborne || webbed || wall || !std::isfinite(actorYaw) || !std::isfinite(viewYaw))
        return false;
    return std::abs(wrap(viewYaw - actorYaw)) > airSyncAngle;
}

} // namespace spidy::hybrid_movement
