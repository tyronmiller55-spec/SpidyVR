#pragma once
// FACING CHECK (the user, November 12 07:14: "there are times where it turns me 180 on
// landing or launch. Maybe we need a check on each event").
//
// On each ownership event - a landing, a point launch, the timed jump handoff, the
// animation taking the hero back for anything else (re-entry), and any other SpidyVR
// takeover - the facing the player expects is known just before it: the view the
// headset showed (for a point launch, the launch direction when the player launched
// roughly where they looked; a backward launch off a wall keeps the view). Right after
// it, within a few frames, the view and (while the animation owns him) the hero's facing
// are compared against it:
//  - the view more than 45 degrees off what was shown is a turn the player did not make:
//    it is put back (the anim view's yaw lock, or the tracking rig's yaw in SpidyVR);
//  - the hero more than 45 degrees off (90 until the G1 playtest) is the game's facing (the actor's own, stale or
//    the move's), not the player's: the walk stick turns him to it (TURN-IN-PLACE's
//    closed loop, forced, no facing write).
// Each logs 'facing check: <event> <view|hero> expected X got Y (ok|corrected|...)'.
//
// The 06:37 session (319e734): at 52 of 199 yaw-lock latches the head bone (the SpidyVR
// body, under the view) and the actor's facing were more than 135 degrees apart; after a
// landing the game's animation turned the hero onto the actor's facing within ~0.3 s,
// and TURN-IN-PLACE fixed only 8 of the 52 landings where he ended more than 90 degrees
// off the view (18 interrupted, 26 never ran). No game address is guessed here.
#include "spidy/stable_yaw.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::facing_check {

using stable_yaw::degrees;
using stable_yaw::wrap;

enum class Event : uint8_t { landing, pointLaunch, jumpHandoff, animReentry, takeover };
inline const char* eventName(Event e) {
    switch (e) {
    case Event::landing: return "landing";
    case Event::pointLaunch: return "point launch";
    case Event::jumpHandoff: return "jump handoff";
    case Event::animReentry: return "anim re-entry";
    default: return "takeover";
    }
}

inline constexpr float viewTolerance = 45.f * degrees; // past this the view turned without the player
inline constexpr float heroTolerance = 45.f * degrees; // past this the hero faces the game's way (was 90: G1 playtest)
inline constexpr float launchCone = 90.f * degrees;    // launch direction counts within this of the view
inline constexpr float windowSeconds = .3f;            // the check's reach after the event

// A point launch: the launch direction when it is within a quarter turn of the view
// (the player launched where they looked), else the view (a backward wall launch).
inline float launchExpected(float viewBefore, float launchYaw) {
    if (!std::isfinite(launchYaw))
        return viewBefore;
    if (!std::isfinite(viewBefore))
        return launchYaw;
    return std::abs(wrap(launchYaw - viewBefore)) <= launchCone ? launchYaw : viewBefore;
}

enum class Verdict : uint8_t { none, ok, corrected, unfixable, missed };
inline const char* verdictName(Verdict v) {
    switch (v) {
    case Verdict::ok: return "ok";
    case Verdict::corrected: return "corrected";
    case Verdict::unfixable: return "off, not fixable here";
    case Verdict::missed: return "not sampled";
    default: return "none";
    }
}

struct Sample {
    bool viewOk = false;     // the view this frame is known (after the new owner's first frame)
    float view = 0;          // its yaw (world, rig convention: 0 facing -z)
    bool viewFixable = false; // the view can be turned back (yaw lock on, or SpidyVR's rig)
    bool heroMatters = false; // the animation owns the hero (his facing shows)
    bool heroOk = false;
    float hero = 0;          // the actor's facing
};
struct Report {
    Verdict verdict = Verdict::none;
    float expected = 0, got = 0;
};
struct Out {
    Report view, hero;      // set on the frame each is decided
    float viewFix = 0;      // turn the view by this (radians) now
    bool heroTurn = false;  // start a forced turn of the hero to heroYaw
    float heroYaw = 0;
};

struct Check {
    bool armed = false, viewDone = false, heroDone = false;
    Event event = Event::landing;
    float expected = 0; // the facing the player expects after it (hero)
    float keep = 0;     // the view before it (the view's own reference)
    float age = 0;
    uint32_t events = 0, viewFixes = 0, heroFixes = 0;

    void arm(Event e, float viewBefore, float expectedYaw = NAN) {
        if (!std::isfinite(viewBefore)) {
            armed = false;
            return;
        }
        event = e;
        keep = wrap(viewBefore);
        expected = std::isfinite(expectedYaw) ? wrap(expectedYaw) : keep;
        armed = true;
        viewDone = heroDone = false;
        age = 0;
        ++events;
    }
    Out update(float dt, const Sample& s) {
        Out out;
        if (!armed)
            return out;
        age += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (!viewDone && s.viewOk && std::isfinite(s.view)) {
            viewDone = true;
            const float err = wrap(keep - s.view);
            out.view.expected = keep;
            out.view.got = wrap(s.view);
            if (std::abs(err) <= viewTolerance) {
                out.view.verdict = Verdict::ok;
            } else if (s.viewFixable) {
                out.view.verdict = Verdict::corrected;
                out.viewFix = err;
                ++viewFixes;
            } else {
                out.view.verdict = Verdict::unfixable;
            }
        }
        if (!heroDone && s.heroMatters && s.heroOk && std::isfinite(s.hero)) {
            heroDone = true;
            out.hero.expected = expected;
            out.hero.got = wrap(s.hero);
            if (std::abs(wrap(expected - s.hero)) <= heroTolerance) {
                out.hero.verdict = Verdict::ok;
            } else {
                out.hero.verdict = Verdict::corrected;
                out.heroTurn = true;
                out.heroYaw = expected;
                ++heroFixes;
            }
        }
        // In SpidyVR the body is built under the view: the hero needs no check there.
        if (!heroDone && s.viewOk && !s.heroMatters)
            heroDone = true;
        if (age > windowSeconds) {
            if (!viewDone) {
                out.view.verdict = Verdict::missed;
                out.view.expected = keep;
            }
            if (!heroDone && s.heroMatters) {
                out.hero.verdict = Verdict::missed;
                out.hero.expected = expected;
            }
            viewDone = heroDone = true;
        }
        if (viewDone && heroDone)
            armed = false;
        return out;
    }
    void reset() {
        armed = false;
    }
};

} // namespace spidy::facing_check
