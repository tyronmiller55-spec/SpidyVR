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
//
// HYBRID v2 (the user, November 12 t79u / 13 t86u):
// - The left stick's front half is free movement: he turns and runs wherever it points
//   (up to 90 degrees either side, sent to the game's own run), while the view keeps the
//   heading - the head can turn 90 degrees, the camera does not chase his body.
// - Within 15 degrees of forward the headset steers: he runs where the player looks,
//   clamped to the same 30-degree window (heading +-15), so running forward has nuance
//   without the view ever being taken. 15-25 degrees blends gaze into the stick (no step).
//   The heading never follows that steering, so nothing feeds back (look right -> he
//   runs right -> the view stays -> the gaze stays: no spiral).
// - Past 100 degrees the rear snap turns the view like a head turn (REAR SNAP TURN, 200 ms
//   by default, eased), not a frame switch; re-arm under 80 or neutral as before.
// - The heading is the player's while the left stick has driven him on the ground: let
//   go and he stays where he is, the view stays where it is (no turn-in-place, no
//   rejoin); a hold that times out on the ground keeps the heading too. Only the game's
//   own turns (parkour, combat, walls) are followed again.
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

// The headset steers within this much of forward (the 30-degree window), blending
// into the stick's own direction by blendAngle; free movement stops at freeAngle.
inline constexpr float coneAngle = 15.f * degrees, blendAngle = 25.f * degrees, freeAngle = 90.f * degrees;
inline constexpr float gazeTau = .1f; // the gaze's low-pass (head micro-motion)

enum class Zone : uint8_t { none, cone, blend, free, edge, rear, held };
inline const char* zoneName(Zone z) {
    switch (z) {
    case Zone::cone: return "head cone";
    case Zone::blend: return "blend";
    case Zone::free: return "free";
    case Zone::edge: return "edge (90)";
    case Zone::rear: return "rear";
    case Zone::held: return "after snap";
    default: return "none";
    }
}
// The run direction (heading-relative, radians, + right) for a stick at `a` with the
// gaze `gaze` (same convention, NAN: none -> straight ahead in the cone).
inline float runDirection(float a, float gaze) {
    const float g = std::isfinite(gaze) ? std::clamp(gaze, -coneAngle, coneAngle) : 0.f;
    const float abs = std::abs(a);
    if (abs <= coneAngle)
        return g;
    if (abs < blendAngle) {
        const float t = (abs - coneAngle) / (blendAngle - coneAngle);
        return g + (a - g) * t;
    }
    return std::clamp(a, -freeAngle, freeAngle);
}

struct Gate {
    bool armed = true, holding = false, settling = false;
    float settle = 0, lastAngle = 0;
    float gazeSteady = 0;
    bool gazeSet = false;
    uint32_t snaps = 0;
    struct Out {
        bool snapped = false;
        float snapAngle = 0; // the stick's angle at the snap (radians, + right)
        // The run: its direction off the heading (radians, + right) and tilt.
        float dir = 0, y = 0;
        bool drives = false; // the left stick runs him this frame
        bool rear = false;   // in the rear dead band / settling (no run)
        Zone zone = Zone::none;
    };
    // `gaze`: the headset's yaw off the heading (radians, + right; NAN none).
    Out update(float dt, float x, float y, float gaze = NAN) {
        Out out;
        if (!std::isfinite(x) || !std::isfinite(y)) {
            x = y = 0;
        }
        const float step = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        float g = NAN;
        if (std::isfinite(gaze)) {
            const float want = std::clamp(wrap(gaze), -coneAngle, coneAngle);
            if (!gazeSet) {
                gazeSteady = want;
                gazeSet = true;
            } else {
                gazeSteady += (want - gazeSteady) * (1 - std::exp(-step / gazeTau));
            }
            g = gazeSteady;
        } else {
            gazeSet = false;
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
            // Snapped: he runs along the new heading while the stick stays back (the
            // headset still steers within the window).
            out.y = tilt;
            out.dir = runDirection(0.f, g);
            out.drives = true;
            out.zone = Zone::held;
            lastAngle = a;
            return out;
        }
        if (armed && tilt >= snapTilt && abs > snapAngle) {
            if (!settling) {
                settling = true;
                settle = 0;
            } else {
                settle += step;
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
                out.dir = runDirection(0.f, g);
                out.drives = true;
                out.zone = Zone::held;
                return out;
            }
            out.rear = true;
            out.zone = Zone::rear;
            return out;
        }
        settling = false;
        lastAngle = a;
        if (tilt >= deadzone && abs <= snapAngle) {
            // Free movement over the front half (to the snap line; past 90 he runs at 90).
            out.y = tilt;
            out.dir = runDirection(a, g);
            out.drives = true;
            out.zone = abs <= coneAngle  ? Zone::cone
                       : abs < blendAngle ? Zone::blend
                       : abs <= freeAngle ? Zone::free
                                          : Zone::edge;
        } else if (tilt >= deadzone) {
            out.rear = true; // the back half below the snap's tilt, or not armed
            out.zone = Zone::rear;
        }
        return out;
    }
    void reset() {
        armed = true;
        holding = settling = false;
        gazeSet = false;
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
    // v2: the player's heading - set once the left stick has driven him on the ground (or
    // a hold timed out there); the heading then stays where it is, never following him,
    // until the game itself owns his turns (not steerable for anchorLossSeconds).
    bool anchored = false;
    float anchorLost = 0;
    uint32_t arrived = 0, gaveUp = 0, kept = 0;

    static constexpr float arriveAngle = 12.f * degrees, giveUpSeconds = 2.5f, followTau = .12f,
                           swayInSeconds = .3f, anchorLossSeconds = .25f;

    struct In {
        float dt = 0;
        bool heroOk = false;
        float hero = 0;
        bool driving = false; // the left stick runs him along it
        bool steerable = false; // the walk stick can turn him (grounded, the animation's)
    };
    struct Out {
        bool heroTurn = false; // TURN-IN-PLACE forced to `yaw` (standing)
        Hold released = Hold::none;
        bool gaveUp = false;
        bool kept = false; // v2: a hold timed out on the ground; the heading stays (no swing)
        bool unanchored = false; // the game took his turns back; the heading follows him again
    };
    void latch(float y) {
        yaw = wrap(y);
        set = true;
        hold = Hold::none;
        swayWeight = 1;
        snapT = 1;
        following = false;
        anchored = false;
        anchorLost = 0;
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
            } else if (age > giveUpSeconds) {
                out.released = hold;
                hold = Hold::none;
                if (in.steerable) {
                    // v2: on his feet the heading is the player's: it stays, and so does
                    // the view (5061b4a rejoined the head here: 118-158 degree swings).
                    out.kept = true;
                    anchored = true;
                    anchorLost = 0;
                    ++kept;
                } else {
                    out.gaveUp = true;
                    ++gaveUp;
                }
            }
        }
        if (hold == Hold::none) {
            if (in.driving && in.steerable) {
                // He runs where the stick says, the heading stays the player's.
                anchored = true;
                anchorLost = 0;
                following = false;
                drivingOff = 0;
            } else if (anchored) {
                // Let go: he stays, the view stays. The game's own turns (parkour, a
                // fight, a wall) take the anchor away after a moment.
                anchorLost = in.steerable ? 0.f : anchorLost + dt;
                if (anchorLost > anchorLossSeconds) {
                    anchored = false;
                    anchorLost = 0;
                    out.unanchored = true;
                }
                following = false;
            }
            if (!anchored && !(in.driving && in.steerable)) {
                drivingOff = 0;
                // Nothing to bring him onto: the heading is his facing (the view the head).
                // What was left when that began fades (0.12 s); his own turns are followed
                // at once, never lagged.
                if (!following) {
                    rest = gap;
                    following = true;
                }
                rest *= std::exp(-dt / followTau);
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
