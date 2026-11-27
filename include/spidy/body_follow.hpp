#pragma once
// BODY FOLLOWS VIEW (0.8.7-na-beta3q, settings body_follows_view, revision 28; the owner, Nov 27 t158u: "a way ... so
// that the player NEVER is looking behind the body", and his answer "Build body follows view"; plan
// never-behind-body-1127.md Option B). The body follows the head; the view is never clamped or written.
//
// FOLLOW (the actor, on the ground): the gap between where the player looks (shown + headset, hsViewNow) and the
// hero's facing is watched every frame. Past `softDeg` for `softSeconds`, or past `hardDeg` at once (re-armed under
// `rearmDeg`: the rear snap's 100/80 hysteresis), the hero is brought round onto the view by the game's own
// walk-stick turn (view_yaw_lock::TurnInPlace's forced turn, the same lever as the landing turn), until he faces it
// within `doneDeg` (the facing hold's arrive angle). It never gives up: a blocker (a dodge, a zip, the leg stick, a
// locked or midair animation, the air, a wall) makes it wait, and it starts again once the blocker clears. It is the
// lowest priority: a facing check, a heading hold, a punch track or the landing body's turn keep theirs. The player's
// left stick wins (it runs him along the view anyway). In a fight's tail it waits, unless FIGHTS (body_follow_fights,
// off) lets the hard catch turn him there.
//
// CATCH (the drawn body, pose only): where the game cannot turn him yet, the DRAWN body is turned about the head so
// his torso is never more than `catchDeg` (90) off the view (native_body::setYieldTilt, as LANDING BODY TO VIEW: no
// game write, the camera does not move). It eases in and out, keeps its side through +-180, and is merged with the
// landing body's turn so the pose has one writer. Off on walls, in Showtime and in SpidyVR's own air (SpidyVR's body
// already follows the headset there: body_ik's yawDeadZone).
//
// Off: beta3p exactly (nothing evaluated). Portable, pure.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::body_follow {

inline constexpr float pi = 3.14159265f;
inline constexpr float degrees = pi / 180.f;
inline constexpr float rearmDeg = 80.f;   // the hard catch re-arms under this (the rear snap's hysteresis)
inline constexpr float doneDeg = 12.f;    // arrived (hybrid_movement::Heading::arriveAngle, TurnInPlace's stopAngle)
inline constexpr float catchDeg = 90.f;   // the drawn torso is never more than this off the view
inline constexpr float catchEaseSeconds = .25f;

inline float wrapPi(float a) {
    if (!std::isfinite(a))
        return 0.f;
    a = std::fmod(a + pi, 2.f * pi);
    if (a < 0)
        a += 2.f * pi;
    return a - pi;
}

struct Tuning {
    float softDeg = 60.f;     // body_follow_soft_deg
    float softSeconds = .4f;  // body_follow_soft_ms / 1000
    float hardDeg = 100.f;    // body_follow_hard_deg
};
// From the settings' whole numbers (clamped as vr_settings sanitizes them).
inline Tuning tuningOf(int softDeg, int softMs, int hardDeg) {
    Tuning t;
    t.hardDeg = static_cast<float>(std::clamp(hardDeg, 60, 170));
    t.softDeg = static_cast<float>(std::clamp(softDeg, 20, 170));
    if (t.softDeg > t.hardDeg)
        t.softDeg = t.hardDeg;
    t.softSeconds = static_cast<float>(std::clamp(softMs, 0, 3000)) * .001f;
    return t;
}

enum class Why : uint8_t { none, soft, hard };
inline const char* whyName(Why w) {
    return w == Why::soft ? "soft" : w == Why::hard ? "hard" : "none";
}
enum class End : uint8_t { none, arrived, cancelled };

struct Follow {
    struct In {
        float dt = 0;
        float gap = NAN;      // the view minus the hero's facing (radians); NAN: not known
        bool live = false;    // the switch, HYBRID + HEADSET FIRST, the head lock, the animation's domain, no Showtime
        bool canTurn = false; // the walk stick can turn him now (grounded, his own, no stick, dodge, zip, legs, wall)
        bool busy = false;    // another turn has him (facing check, heading hold, punch track, landing body)
        bool fight = false;   // a fight's tail holds turns off
        bool fightsAllowed = false; // FIGHTS (body_follow_fights): the hard catch may turn him in a fight
    };
    struct Out {
        bool force = false;   // turn him onto the view this frame (TurnInPlace's force, forceYaw = the view)
        bool fightTurn = false; // the force is a hard catch in a fight's tail (the fight gate lets it through)
        bool started = false; // engaged this frame
        End ended = End::none;
        Why why = Why::none;  // on started / while engaged
        float startGap = 0;   // on started (radians)
        float seconds = 0;    // on ended: since it engaged
        float waited = 0;     // on ended: of that, waiting on a blocker
    };
    bool engaged = false, hardArmed = true;
    Why why = Why::none;
    float dwell = 0, age = 0, waited = 0, startGap = 0;
    uint32_t starts = 0, softStarts = 0, hardStarts = 0, arrivals = 0;

    Out update(const In& in, const Tuning& t) {
        Out out;
        const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
        if (!in.live || !std::isfinite(in.gap)) {
            if (engaged)
                end(out, End::cancelled);
            dwell = 0;
            return out;
        }
        const float a = std::abs(wrapPi(in.gap)) / degrees;
        if (a < rearmDeg)
            hardArmed = true;
        if (engaged) {
            age += dt;
            if (a < doneDeg) {
                end(out, End::arrived);
                ++arrivals;
                return out;
            }
        } else {
            dwell = a > t.softDeg ? dwell + dt : 0.f;
            const bool hard = hardArmed && a > t.hardDeg;
            if (hard || (a > t.softDeg && dwell >= t.softSeconds)) {
                engaged = true;
                why = hard ? Why::hard : Why::soft;
                if (hard)
                    hardArmed = false;
                age = waited = 0;
                startGap = wrapPi(in.gap);
                ++starts;
                ++(hard ? hardStarts : softStarts);
                out.started = true;
                out.startGap = startGap;
            }
        }
        if (engaged) {
            out.why = why;
            // A hard catch past the line stays hard while it lasts (the fight gate reads it).
            if (a > t.hardDeg)
                why = out.why = Why::hard;
            const bool fightOk = !in.fight || (out.why == Why::hard && in.fightsAllowed);
            out.force = in.canTurn && !in.busy && fightOk;
            out.fightTurn = out.force && in.fight;
            if (!out.force)
                waited += dt;
        }
        return out;
    }
    void reset() {
        engaged = false;
        hardArmed = true;
        why = Why::none;
        dwell = age = waited = startGap = 0;
    }

  private:
    void end(Out& out, End e) {
        out.ended = e;
        out.why = why;
        out.startGap = startGap;
        out.seconds = age;
        out.waited = waited;
        engaged = false;
        why = Why::none;
        dwell = 0;
    }
};

// The drawn body's catch: the yaw (radians, the rig's convention, + toward the view as landing_body's offset) that
// keeps the torso within catchDeg of the view, eased in and out over catchEaseSeconds, its side kept through +-180.
struct Catch {
    float w = 0, excess = 0, lastGap = 0, offset = 0;
    bool known = false;
    uint32_t begun = 0;
    float worstDeg = 0; // the largest drawn turn since the last read (diag)

    bool active() const {
        return offset != 0.f || w > 0.f;
    }
    // `gap`: the view minus the hero's facing; `live`: the catch may draw (see game_xr). Returns the drawn turn.
    float update(float dt, float gap, bool live) {
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const float rate = d / catchEaseSeconds;
        if (live && std::isfinite(gap)) {
            float g = wrapPi(gap);
            if (known && std::abs(g - lastGap) > pi) {
                // the view crossed straight behind him: keep the same way round ...
                g += g < 0 ? 2.f * pi : -2.f * pi;
                // ... until it is under catchDeg the other way (then the short way is the same side again).
                if (std::abs(g) > 2.f * pi - catchDeg * degrees)
                    g = wrapPi(g);
            }
            lastGap = g;
            known = true;
            const float lim = catchDeg * degrees;
            const float e = std::abs(g) > lim ? (g > 0 ? g - lim : g + lim) : 0.f;
            if (e != 0.f) {
                if (w == 0.f && excess == 0.f)
                    ++begun;
                excess = e;
                w = std::min(1.f, w + rate);
            } else {
                // inside the limit: nothing to draw; the weight eases out from what was shown
                w = std::max(0.f, w - rate);
                if (w == 0.f)
                    excess = 0;
            }
        } else {
            known = false;
            w = std::max(0.f, w - rate);
            if (w == 0.f)
                excess = 0;
        }
        const float s = w * w * (3.f - 2.f * w);
        offset = excess * s;
        worstDeg = std::max(worstDeg, std::abs(offset) / degrees);
        return offset;
    }
    void reset() {
        w = excess = lastGap = offset = 0;
        known = false;
    }
};

// One writer for the drawn body's yaw: the landing body's turn (landing_body::Ease) and the catch. The landing's turn
// while it runs, never less than the catch on the same side (so the torso stays within catchDeg); the catch alone
// otherwise. Opposite sides (the landing ease went the other way round): the landing's.
inline float merged(float landing, bool landingActive, float caught) {
    if (!landingActive || landing == 0.f)
        return caught;
    if (caught == 0.f || (landing > 0) != (caught > 0))
        return landing;
    return std::abs(caught) > std::abs(landing) ? caught : landing;
}

} // namespace spidy::body_follow
