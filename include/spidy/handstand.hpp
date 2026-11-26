// HANDSTAND, phase H1 (EngineBot's "SpidyVR - Handstand & Point Jump" page,
// its 12-step H1 checklist). REALISTIC FLIPS only; experimental, off by
// default (spidy_vr.ini `handstand=1`).
//
// Come down inverted (100 degrees or more from upright) in SpidyVR's air with
// both palms on a floor (HandCollision's ray-confirmed floor planes) and
// little spin: the hands CATCH. The body then BALANCES on them - a linear
// inverted pendulum about the palms, steered by its divergent component of
// motion (DCM) so that a small lean settles - and the view holds that lean
// (RealisticFlip's support override). It ends by:
//  - push: the tracked palms driven into the floor (arms extended fast):
//    a launch off the hands (the point jump's swing path, no smoke);
//  - step-down: the palms lifted without a push - he drops to his feet;
//  - fall: the DCM out of the support for fallSeconds, or a lean over
//    fallLeanDegrees - he tips over;
//  - grip, tracking lost, disabled.
//
// THE TOUCHDOWN CATCH (Nov 13 playtest, 99b8154: 358 landings out of
// SpidyVR's air, no handstand line at all): the game's capsule is upright, so
// it touches the floor with its feet while the drawn body is still inverted
// above it, and the palms rarely reach a ray-confirmed floor first. The frame
// it touches, `vrAir` is already false (the swing is not airborne) and the
// landing hands him to the animation - the crouched landing. So a catch is
// also taken on the game's touchdown (`touchdown`: SpidyVR still owns him and
// the game reports ground) within touchdownGrace of SpidyVR's air, the palms
// counted on the floor at `floorY` when within touchdownReach of it (the eyes'
// drop then brings them down); while catching, palms within floorReach of
// `floorY` count as on it, and the catch has touchdownSettle to put them there.
// Each touchdown out of SpidyVR's air with HANDSTAND on reports why there was
// no catch (Out::refused), for the log.
// While it catches, balances and for leavingSeconds after, SpidyVR keeps the
// body on the ground (holdVr: hybrid_owner::Frame::handSupport), so the
// landing rule does not hand him to the animation mid-handstand.
//
// THE LANDING CATCH, THE SECOND PASS (Nov 13, 22:24 playtest: 28 touchdowns
// judged out of 63 SpidyVR landings, the one catch fell after 63 ms with a 43.7
// degree lean): the balance starts from the landing's speed and spin DAMPED
// (entryKeep, entrySpinKeep, at most entryMaxSpeed: the floor takes the rest, and
// the palms' support is ~0.15 m wide, a divergent 0.5 m/s at the pendulum's 3.2/s)
// instead of the full ~9 m/s; a grip squeezed at the touchdown (it is how a web is
// aimed, 8 of the 28 refusals) no longer refuses a catch with both palms down, and
// does not let go of it until it has been released once; palms within touchdownReach
// count at the touchdown as far as the eyes' drop and an arm reach them; and a
// refusal lists every gate that failed (Out::failed), not just the first.
//
// Leg control (Y+X) stays usable: the legs move the drawn body; the balance
// reads only the hands and the head.
//
// Pure; no Windows.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::handstand {

struct Params {
    bool enabled = false;
    float comHeight = .95f; // the centre of mass above the palms, m (h)
    float gain = 5.f;       // kxi: how hard the DCM is steered back (3 until the balance pass)
    bool assist = true;     // off: the centre of pressure stays mid-palms (no steering)
    // THE BALANCE PASS (the user, Nov 14 t67u: "Let's give handstands a lot more balance. Make it hard to fall out of it,
    // but easy to leap out of."): the last session had 30 catches held a median 0.27 s, falling at a 6 degree lean with the
    // centre of mass 0.11 m off. The support was palmRadius + margin = 0.12 m around the palms' segment and the divergent
    // component only had to be outside it for 0.12 s. Now: 0.18 m (margin .12), a fall needs it outside the support grown by
    // fallSlack (.06) for fallSeconds (.5), the steering is 5 (was 3) and the pendulum is damped (damping 2.5 /s), no fall
    // test in the first fallGrace (.6 s) of a catch, and a palm re-placement of more than reposeJump in a frame keeps the
    // centre of mass with the palms in either state (the catch log showed 0.17 to 0.22 m every other frame all the way).
    float palmRadius = .06f, margin = .12f;
    float fallSlack = .06f;  // m: the fall test's region is the support grown by this
    float damping = 2.5f;    // 1/s: the centre of mass's speed bleeds off at this rate
    float fallGrace = .6f;   // s from the catch: no fall test (the catch settle; catchGrace is the missed-palms wait)
    // LEAP OUT: A pressed, the left stick held hard for leapStickSeconds, or a push off the palms (pushSpeed, lower than
    // it was) leaves at once, in a jump along the view's direction: leapUp m/s up, leapForward m/s along it.
    bool leap = true;
    float leapStick = .9f, leapStickSeconds = .15f, leapUp = 5.5f, leapForward = 4.f;
    float catchDegrees = 100; // inverted at least this far to catch
    float catchSpin = 5.f;    // rad/s; faster is a handspring (H2): no catch (3 before Nov 13)
    float fallSeconds = .5f, fallLeanDegrees = 50; // .12 s and 35 degrees until the balance pass
    float pushSpeed = .45f, pushGain = 2.2f, pushWindow = .05f; // m/s into the floor
    float pushMaxNormal = 7.f, pushMaxAcross = 4.f;
    float catchGrace = .15f, leavingSeconds = .3f;
    // THE TOUCHDOWN CATCH (above).
    float touchdownGrace = .25f;  // s after SpidyVR's air a touchdown can still catch
    // ARMS UP (the user, Nov 14 t71u: "seems to activate even if my arms aren't up, and it pops me out"; the b9b2196 session:
    // the touchdown counted a palm on the floor at 1.6 m, and the palms of the 173 judged touchdowns were 1.13 / 1.51 / 1.83 m
    // over it, standing hand height, so 15 of 28 catches ended "missed" at the 0.6 s settle). Now a palm counts at the touchdown
    // only when it is within touchdownReach (1.0 m: the eyes' drop, .9, and a little) AND clearly below the head (armsBelowHead,
    // .2 m: the hands toward the floor, past the head, the way a handstand reaches), both for armsHoldSeconds (.12 s).
    float touchdownReach = 1.0f;
    float armsBelowHead = .2f, armsHoldSeconds = .12f;
    // ARMS OVER HEAD (the owner, Nov 16; handstand_arms_over_head_cm): the arms-up test in the player's own frame - both palms
    // at least this far over the head as he holds them (Input::palmRel.y), at any flip angle. The world test above
    // (armsBelowHead) counted hands held over the head only past ~125 degrees inverted (0.35 m up reads 0.35 cos(theta)
    // below the head: 0.06 m at 100, 0.16 m at 117). 0: the world test. A palm with no tracked offset is no test.
    float armsOverHead = .10f;
    // HANDSTAND GRACE (the owner, Nov 17: "give some grace for getting into one"): the arms over the head up to
    // armsGraceBefore before the touchdown count at it (and their hold with them), and the touchdown can catch for
    // touchdownGrace after SpidyVR's air (.25 s before; handstand_grace_after_ms).
    float armsGraceBefore = .3f;
    // TIP-IN (the owner, Nov 17: "tip the player into the handstand when it's clear they're attempting one"): inverted
    // at least tipInDegrees, not spinning past catchSpin, the arms over the head (now or in the grace): the catch takes
    // the palms from as far as tipInReach over the floor and waits for no hold; the catch then sets them on the floor.
    bool tipIn = true;
    float tipInDegrees = 120.f, tipInReach = 2.f;
    // A LEAP IS A FRESH PUSH: a stick already held at the catch has to be let go of first, and A pressed before the catch is not
    // a press now (9 of 11 leaps in that session came 0.01 s after the catch).
    // (A pressed after the catch is already a fresh press: only a held A or stick is carried across it.)
    // THE LANDING CATCH, THE SECOND PASS (above).
    float entryKeep = .12f;       // share of the landing's horizontal speed the balance starts with
    float entrySpinKeep = .2f;    // share of the flip's spin it starts with
    float entryMaxSpeed = .5f;    // m/s: the balance's start never exceeds this
    float floorReach = .12f;      // m: palms this close to the floor count as on it
    float touchdownSettle = .6f;  // s a touchdown catch has to bring the palms down
    float eyeDrop = .9f;    // the inverted eyes' drop toward the floor, m
    float stickLean = .08f; // m the left stick moves the balance's aim
    // THE CATCH'S SEED (G1 playtest: 8 of 8 catches fell within 160 ms): the centre of mass starts over the
    // palms, its offset no more than seedShare of the support's radius (it was 0.2 to 0.65 m, 2 to 5 times
    // outside a 0.12 m support, from the landing's 137 to 167 degrees of inversion). The body settles over
    // the palms during the catching phase.
    float seedShare = .6f;
    // A palm centre that moves further than this in a frame while catching is the palms being re-placed (the
    // support tilt, the origin of the handoff), not a lean: the centre of mass goes with it.
    float reposeJump = .04f; // m
    // TRACKING LOST (the 2 AM log of Nov 18, approved 1:53 PM): one handstand ended "tracking lost" with the centre of
    // mass 159.74 m off the palms - a lost palm read at the world's origin (-1.00 m over the floor) became the support
    // on the exit frame. Now a palm that is not tracked, not finite, or that moves more than glitchJump in a frame is
    // a glitch: the support keeps the last good palms, the balance holds, and only a loss lasting trackingGrace ends
    // the handstand.
    float glitchJump = 1.f, trackingGrace = .25f;
    // ABSURD ARMS (the Nov 19 log: palms at -33.20 m and -22.52 m, the arms NaN, a window held 236 ms): a palm farther
    // than sanePalmReach from the head (sanePalmFloor from the floor, no head), or a palm-from-head axis that is not a
    // number or past sanePalmReach, is no reading: the frame counts as untracked (no window, no catch; a catch rides it
    // out as a tracking glitch).
    float sanePalmReach = 3.f, sanePalmFloor = 5.f;
    // STICKY HANDSTANDS (beta2; the owner, Nov 19 t79u: "making handstands \"sticky\" or easy to get into
    // would help"; the na-beta1 log's t=657.9 catch ended "tracking lost" after 0.26 s: the palms and the
    // head moved together by 1.2 m - the rig re-placed, not the hands - and the glitch test kept comparing to
    // palms left behind). Off here, on from the settings (handstand_sticky, on by default):
    //  - a frame whose palms jumped together WITH the head (each palm-from-head within rigShift of the good
    //  one's) is the
    //    rig moving, not a glitch: the good palms follow it, and the catch's re-seed moves the body with
    //    them;
    //  - a real loss is held for stickyGrace (0.4 s; handstand_sticky_grace_ms) instead of trackingGrace
    //  (0.25 s);
    //  - holding, the palms count on the floor from holdReachScale x floorReach (1.5 x), and must be off it
    //  for
    //    offSeconds (0.15 s) before a step-down or a miss - the in and out of the hold are not the same line;
    //  - the entry is wider: the arms-up test in force is entrySlack (0.05 m) easier (ARMS OVER HEAD at least
    //  0.02 m;
    //    with it at 0, the world test at least 0.05 m) and its hold at most stickyHold (0.08 s). The arms
    //    still have to be up.
    bool sticky = false;
    float rigShift = .3f, stickyGrace = .4f, holdReachScale = 1.5f, offSeconds = .15f;
    float entrySlack = .05f, stickyHold = .08f;
    // GRIP HOLDS THE HANDSTAND (0.8.7-na-beta3n, handstand_grip_hold; the owner, Nov 26 t146u: "I want handstands to
    // lock easily, and grip to be used to 'hold' the stance. Everything else would stay the same, and pushing out of
    // it, or pushing off of anything, requires me to let go of the grip"). Off here, on from the settings:
    //  - the catch: a grip squeezed (Input::grip) near the floor, inverted at least catchDegrees and not spinning past
    //    catchSpin, is the intent - the arms-over-head test and its hold are not asked, and the palms count from as
    //    far as the tip-in's reach (tipInReach, 2 m, not touchdownReach's 1.6 m), as the tip-in's do;
    //  - the hold: while a grip is held the handstand is locked - no fall (the centre of mass kept inside the
    //    support), no step-down or miss, no push, no leap; a squeeze no longer ends it (Exit::grip);
    //  - the way out: let go of the grip, then everything is as before (push, leap, step-down, fall). A push or a
    //    stick held through the release goes at once: letting go is the launch, as the grip fling's.
    // Tracking lost and the switch turned off still end it.
    bool gripHold = false;
};

template <class V> Params paramsOf(const V& v) {
    Params p;
    p.enabled = v.handstand && v.realisticFlips;
    p.assist = v.handstandAssist;
    p.comHeight = std::clamp(v.handstandHeightCm, 60, 140) / 100.f;
    p.gain = std::clamp(v.handstandGainPct, 50, 600) / 100.f;
    p.margin = std::clamp(v.handstandSupportCm, 10, 30) / 100.f - p.palmRadius;
    p.fallSeconds = std::clamp(v.handstandGraceMs, 100, 1500) / 1000.f;
    p.damping = std::clamp(v.handstandDampingPct, 0, 600) / 100.f;
    p.leap = v.handstandLeap;
    p.pushGain = std::clamp(v.handstandPushGain, 100, 400) / 100.f;
    p.pushMaxNormal = static_cast<float>(std::clamp(v.handstandPushMax, 2, 12));
    p.fallLeanDegrees = static_cast<float>(std::clamp(v.handstandFallDegrees, 15, 60));
    p.eyeDrop = std::clamp(v.handstandEyeDropCm, 0, 120) / 100.f;
    // THE CATCH STICKS (the 9790d67 playtest; the owner, Nov 15): the hold while catching and the touchdown's reach.
    p.floorReach = std::clamp(v.handstandCatchHoldCm, 5, 60) / 100.f;
    p.touchdownReach = std::clamp(v.handstandReachCm, 60, 200) / 100.f;
    p.armsOverHead = std::clamp(v.handstandArmsOverHeadCm, 0, 60) / 100.f;
    p.armsGraceBefore = std::clamp(v.handstandGraceBeforeMs, 0, 1000) / 1000.f;
    p.touchdownGrace = std::clamp(v.handstandGraceAfterMs, 100, 1000) / 1000.f;
    p.tipIn = v.handstandTipIn;
    // HANDSTAND ENTRY ANGLE (beta3; the owner, Nov 21: "Lower the handstand tip-in angle too"): the catch and
    // the tip-in from 60 degrees inverted by default (100 and 120 before), so a handstand starts well before
    // upside down.
    p.tipInDegrees = static_cast<float>(std::clamp(v.handstandTipInDeg, 45, 180));
    p.catchDegrees = static_cast<float>(std::clamp(v.handstandCatchDeg, 45, 150));
    p.tipInReach = std::clamp(v.handstandTipInReachCm, 60, 300) / 100.f;
    // STICKY HANDSTANDS (above): the wider entry and the longer holds, from the settings.
    p.sticky = v.handstandSticky;
    p.gripHold = v.handstandGripHold;
    if (p.sticky) {
        p.stickyGrace = std::clamp(v.handstandStickyGraceMs, 250, 1000) / 1000.f;
        p.trackingGrace = std::max(p.trackingGrace, p.stickyGrace);
        // The arms-up test in force (ARMS OVER HEAD, or with it at 0 the world test) is the one made easier.
        if (p.armsOverHead > 0.f)
            p.armsOverHead = std::max(.02f, p.armsOverHead - p.entrySlack);
        else
            p.armsBelowHead = std::max(.05f, p.armsBelowHead - p.entrySlack);
        p.armsHoldSeconds = std::min(p.armsHoldSeconds, p.stickyHold);
    }
    return p;
}

enum class State : uint8_t { off, catching, balance, leaving };
enum class Exit : uint8_t { none, push, stepDown, fall, grip, tracking, disabled, missed, leap };
// Why a touchdown out of SpidyVR's air did not catch (the log's reason).
// (Nov 17) A held grip is no gate (the catch carries it; it lets go after one release): Refusal::grip is kept for the
// numbering and never set. palmsHigh is the palms' height alone now, armsDown the hands-over-head gesture.
enum class Refusal : uint8_t { none, disabled, untracked, grip, notArmed, upright, spin, palmsHigh, armsDown };
inline constexpr unsigned lastRefusal = static_cast<unsigned>(Refusal::armsDown);
inline const char* refusalName(Refusal r) {
    switch (r) {
    case Refusal::disabled:
        return "handstand off or REALISTIC FLIPS off";
    case Refusal::untracked:
        return "head or a hand untracked";
    case Refusal::grip:
        return "a grip held";
    case Refusal::notArmed:
        return "palms not lifted since the last exit";
    case Refusal::upright:
        return "not inverted enough";
    case Refusal::spin:
        return "spinning too fast";
    case Refusal::palmsHigh:
        return "palms too far over the floor";
    case Refusal::armsDown:
        return "hands not over the head";
    default:
        return "none";
    }
}
// The failed gates of a touchdown, one bit per Refusal (1 << value): "a, b, c".
inline unsigned refusalBit(Refusal r) {
    return r == Refusal::none ? 0u : 1u << static_cast<unsigned>(r);
}
inline int refusalList(unsigned failed, char* out, size_t size) {
    if (!out || size == 0)
        return 0;
    out[0] = 0;
    size_t used = 0;
    for (unsigned r = 1; r <= lastRefusal; ++r) {
        if (!(failed & (1u << r)))
            continue;
        const char* name = refusalName(static_cast<Refusal>(r));
        const int n = std::snprintf(out + used, size - used, "%s%s", used ? "; " : "", name);
        if (n < 0 || static_cast<size_t>(n) >= size - used)
            break;
        used += static_cast<size_t>(n);
    }
    return static_cast<int>(used);
}
inline const char* stateName(State s) {
    switch (s) {
    case State::catching:
        return "catching";
    case State::balance:
        return "balance";
    case State::leaving:
        return "leaving";
    default:
        return "off";
    }
}
inline const char* exitName(Exit e) {
    switch (e) {
    case Exit::push:
        return "push";
    case Exit::stepDown:
        return "step-down";
    case Exit::fall:
        return "fall";
    case Exit::grip:
        return "grip";
    case Exit::tracking:
        return "tracking lost";
    case Exit::disabled:
        return "disabled";
    case Exit::missed:
        return "missed (palms off before the catch held)";
    case Exit::leap:
        return "leap";
    default:
        return "none";
    }
}

// How far the body is inverted, degrees (0 upright, 180 straight down),
// from its up in the world.
inline float invertedDegrees(Vec3 bodyUp) {
    if (!finite(bodyUp) || length(bodyUp) < .5f)
        return 0;
    const float c = std::clamp(normalized(bodyUp).y, -1.f, 1.f);
    return std::acos(c) * 180.f / 3.14159265f;
}

// The eyes' drop toward the floor for an inversion: none to 100 degrees, the
// whole of `drop` from 160 (smoothstep between).
inline float eyeDropFor(float degrees, float drop) {
    const float t = std::clamp((degrees - 100.f) / 60.f, 0.f, 1.f);
    return drop * t * t * (3 - 2 * t);
}

// The support region: the segment between the palms, widened by a radius.
struct Support {
    Vec3 a{}, b{}; // palms, level (y ignored)
    float radius = .08f;
    Vec3 centre() const {
        return Vec3{(a.x + b.x) * .5f, 0, (a.z + b.z) * .5f};
    }
    // The nearest point of the region to p (level), and whether p was in it.
    Vec3 clamp(Vec3 p, bool* inside = nullptr) const {
        const Vec3 pa{p.x - a.x, 0, p.z - a.z}, ba{b.x - a.x, 0, b.z - a.z};
        const float len2 = ba.x * ba.x + ba.z * ba.z;
        const float t = len2 > 1e-8f ? std::clamp((pa.x * ba.x + pa.z * ba.z) / len2, 0.f, 1.f) : 0.f;
        const Vec3 nearest{a.x + ba.x * t, 0, a.z + ba.z * t};
        const Vec3 off{p.x - nearest.x, 0, p.z - nearest.z};
        const float d = std::hypot(off.x, off.z);
        if (inside)
            *inside = d <= radius;
        if (d <= radius)
            return Vec3{p.x, 0, p.z};
        return nearest + off * (radius / d);
    }
};

// p brought inside `share` of the support's radius around the palms' segment, keeping its direction from the
// nearest point of the segment (level). A point already that close is returned as it is.
inline Vec3 seedInside(const Support& s, Vec3 p, float share) {
    const Vec3 pa{p.x - s.a.x, 0, p.z - s.a.z}, ba{s.b.x - s.a.x, 0, s.b.z - s.a.z};
    const float len2 = ba.x * ba.x + ba.z * ba.z;
    const float t = len2 > 1e-8f ? std::clamp((pa.x * ba.x + pa.z * ba.z) / len2, 0.f, 1.f) : 0.f;
    const Vec3 nearest{s.a.x + ba.x * t, 0, s.a.z + ba.z * t};
    const Vec3 off{p.x - nearest.x, 0, p.z - nearest.z};
    const float d = std::hypot(off.x, off.z);
    const float reach = std::max(s.radius, 0.f) * std::max(share, 0.f);
    if (d <= reach || d < 1e-6f)
        return Vec3{p.x, 0, p.z};
    return nearest + off * (reach / d);
}

// The linear inverted pendulum on the palms, level: x the centre of mass's
// offset, v its velocity. u (the centre of pressure) steered by the DCM:
//   w0 = sqrt(g / h); xi = x + v / w0; u = xi + (k / w0) (xi - xiRef),
// held inside the support; x'' = w0^2 (x - u). Substeps of 1/120 s at most.
class Balance {
  public:
    void start(Vec3 x, Vec3 v) {
        x_ = Vec3{x.x, 0, x.z};
        v_ = Vec3{v.x, 0, v.z};
        outside_ = 0;
    }
    struct Out {
        Vec3 x{}, v{}, xi{}, u{};
        float leanDegrees{};
        bool fell{};
    };
    Out step(float seconds, const Support& s, Vec3 aim, const Params& p) {
        Out out;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        const float h = std::max(p.comHeight, .3f);
        const float w0 = std::sqrt(9.81f / h);
        const Vec3 ref = s.clamp(finite(aim) ? aim : s.centre());
        bool in = true;
        int n = std::max(1, static_cast<int>(std::ceil(dt * 120.f)));
        const float sub = dt / n;
        for (int i = 0; i < n; ++i) {
            const Vec3 xi = x_ + v_ / w0;
            const Vec3 wanted = p.assist ? xi + (xi - ref) * (p.gain / w0) : s.centre();
            bool inside = true;
            const Vec3 u = s.clamp(wanted, &inside);
            in = in && inside;
            const Vec3 acc = (x_ - u) * (w0 * w0);
            v_ += acc * sub;
            if (p.damping > 0.f)
                v_ = v_ * std::exp(-p.damping * sub);
            x_ += v_ * sub;
            out.u = u;
        }
        out.x = x_;
        out.v = v_;
        out.xi = x_ + v_ / w0;
        // The fall test's region: the support grown by fallSlack (the steering still works within the support itself).
        Support fallRegion = s;
        fallRegion.radius += std::max(p.fallSlack, 0.f);
        bool xiInside = true;
        fallRegion.clamp(out.xi, &xiInside);
        outside_ = xiInside ? 0.f : outside_ + dt;
        const Vec3 lean{x_.x - s.centre().x, 0, x_.z - s.centre().z};
        out.leanDegrees = std::atan2(std::hypot(lean.x, lean.z), h) * 180.f / 3.14159265f;
        out.fell = outside_ >= p.fallSeconds || out.leanDegrees > p.fallLeanDegrees;
        return out;
    }
    Vec3 x() const {
        return x_;
    }
    Vec3 v() const {
        return v_;
    }
    // The palms were re-placed by `by` (level): the centre of mass goes with them, so it reads as no lean.
    void shift(Vec3 by) {
        x_ += Vec3{by.x, 0, by.z};
    }
    // GRIP HOLDS THE HANDSTAND: the centre of mass kept inside the support, still once it reaches its edge.
    void steady(const Support& s) {
        bool inside = true;
        const Vec3 c = s.clamp(x_, &inside);
        if (!inside) {
            x_ = c;
            v_ = {};
        }
        outside_ = 0;
    }
    // The fall timer starts again (the catching phase is a settle, not a balance yet).
    void forgive() {
        outside_ = 0;
    }
    float outside() const {
        return outside_;
    }

  private:
    Vec3 x_{}, v_{};
    float outside_{};
};

// The push: the palms' tracked speed into the floor (m/s, + into it),
// averaged over `window`; over pushSpeed, the launch.
class Push {
  public:
    float update(float seconds, float into, float window) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (!std::isfinite(into))
            into = 0;
        const float a = window > 1e-4f ? 1 - std::exp(-dt / window) : 1.f;
        avg_ += (into - avg_) * a;
        return avg_;
    }
    void reset() {
        avg_ = 0;
    }

  private:
    float avg_{};
};
// The launch off the hands: the push's speed scaled, along the floor's
// normal, with the body's own across, each capped.
inline Vec3 launch(float pushSpeed, Vec3 normal, Vec3 across, const Params& p) {
    if (!finite(normal) || length(normal) < .5f)
        normal = {0, 1, 0};
    normal = normalized(normal);
    const float up = std::clamp(pushSpeed * p.pushGain, 0.f, p.pushMaxNormal);
    Vec3 side = finite(across) ? across - normal * dot(across, normal) : Vec3{};
    side = limited(side, p.pushMaxAcross);
    return normal * up + side;
}

// The leap out of a handstand: leapUp m/s up and leapForward m/s along the view's level direction (straight up when there is
// none), the way a jump out of a run goes.
inline Vec3 leapLaunch(Vec3 viewForward, const Params& p) {
    Vec3 level{finite(viewForward) ? viewForward.x : 0.f, 0.f, finite(viewForward) ? viewForward.z : 0.f};
    level = length(level) > .2f ? normalized(level) : Vec3{};
    return Vec3{0, std::max(p.leapUp, 0.f), 0} + level * std::max(p.leapForward, 0.f);
}

// The state machine. One update a frame, after the hands are placed.
class Handstand {
  public:
    struct Input {
        float seconds{};
        bool vrAir{};          // SpidyVR has the body and he is in its air (or already supported)
        bool tracked{};        // head and both hands tracked
        bool grip{};           // either grip squeezed
        unsigned floorPalms{}; // bits: palms on a floor (HandCollision::floorContacts)
        Vec3 palms[2]{};       // placed palms, world
        Vec3 head{};           // the head, world (the arms-up test: the palms below it)
        bool headKnown{};
        Vec3 floorNormal{0, 1, 0};
        Vec3 bodyUp{0, 1, 0}; // the body's up in the world (the flip's tilt)
        Vec3 angular{};       // the flip's spin, rad/s
        Vec3 velocity{};      // the body's, world m/s
        float pushInto{};     // palms' tracked speed into the floor, m/s (+ into)
        bool jump{};          // A held (the leap is its press)
        float stick{};        // the left stick's tilt, 0..1
        Vec3 viewForward{0, 0, -1}; // where the player looks, level, unit (the leap's way)
        Vec3 aim{};           // the left stick's aim for the balance (level, m, from the support's centre)
        // THE TOUCHDOWN CATCH: SpidyVR owns him and the game reports ground
        // this frame; the floor's height under him (the actor's feet).
        bool touchdown{};
        bool aloft{}; // SpidyVR owns him off the ground (vrAir or not): the grace's start
        bool floorKnown{};
        float floorY{};
        // ARMS DIAGNOSTIC (s165800: 7 inverted touchdowns refused, the hands unseen): each palm from the head in the
        // tracking space (the rig's yaw frame: x side, y up, -z forward), and whether it is known.
        Vec3 palmRel[2]{};
        bool palmRelKnown[2]{};
    };
    struct Out {
        State state{};
        bool holdVr{};     // SpidyVR keeps the body (Frame::handSupport)
        bool supporting{}; // the view holds the balance's lean (RealisticFlight::handSupport)
        bool caughtNow{};
        Exit exitNow{};     // this frame's exit (none: none)
        Vec3 launch{};      // with Exit::push or Exit::leap: the launch velocity
        float heldSeconds{}; // with an exit: how long the catch lasted
        float offBalanceSeconds{}; // with an exit: how long the divergent component had been out of the fall region
        Vec3 releaseSpin{}; // the spin the flip goes on with
        Balance::Out balance{};
        Vec3 lean{}; // the CoM's level offset from the support's centre
        // A touchdown out of SpidyVR's air this frame (first ground frame),
        // and why it did not catch (none: it caught, or no touchdown).
        bool touchdownNow{};
        bool touchdownCatch{}; // this frame's catch was the touchdown's
        Refusal refused{};
        float inverted{}, spin{}, palmGap{}; // at the touchdown: degrees, rad/s, m (the higher palm)
        float headGap{}, armsHeld{};         // at the touchdown: the head's height over the floor, m; how long the arms were up, s
        bool armsUp{};                       // both palms low enough and below the head now (the touchdown's reach path)
        // Each palm's height over the floor this frame (left, right; m; NAN: no floor or no palm), and the floor's y
        // (NAN: none): the catch's palm heights, every frame (the 9790d67 playtest: 3 catches all missed, the palms unseen).
        float palmGaps[2]{NAN, NAN};
        float floorY = NAN;
        unsigned failed{};  // every gate that failed at the touchdown (refusalBit); 0: none or no touchdown
        bool gripCarried{}; // the catch took a squeezed grip: it lets go only after a release
        // GRIP HOLDS THE HANDSTAND: a grip holds the handstand locked this frame (no exit but tracking or off), and
        // this frame's catch took the grip as its intent.
        bool gripLocked{}, gripCatch{};
        // The touchdown's inputs as judged (for the log of a landing that was not judged).
        bool touchdownSeen{}, aloftSeen{}, vrAirSeen{};
        float airAge{};
        // THE CATCH'S GEOMETRY, for the log (G1 playtest): `frame` counts the updates since the catch (0: the catch
        // frame; -1: none), with the palm centre, how far it moved since the last frame, how far the centre of
        // mass was from it as the landing left it (`seedRaw`) and as seeded (`seedOffset`), the divergent
        // component's distance outside the support and for how long, and whether this frame re-placed the
        // centre of mass with the palms (`reseeded`, by `shift` metres).
        int frame = -1;
        Vec3 centre{};
        float centreJump{}, seedRaw{}, seedOffset{}, xiOutside{}, outsideSeconds{}, shift{}, span{};
        bool reseeded{};
        // ARMS DIAGNOSTIC: each palm from the head in the tracking space (Input::palmRel; NAN: unknown), how far each
        // palm is below the head in the world (m, + below; NAN: unknown), the flip's spin axis (world, unit; zero:
        // no spin), and the touchdown window: seconds since the touchdown while no catch is on (-1: not in it).
        Vec3 palmRel[2]{{NAN, NAN, NAN}, {NAN, NAN, NAN}};
        float worldBelow[2]{NAN, NAN};
        Vec3 flipAxis{};
        float windowAge = -1.f;
        unsigned reachDown{};
        // GRACE / TIP-IN (Nov 17): seconds since both hands were last over the head (10: not lately), how long that
        // stretch was held, whether this frame's touchdown judgement took them from the grace, whether the tip-in's
        // intent is clear, and the gates failing on each window frame; `windowClosed`: the window ended this frame
        // without a catch, `closedFailed` its last frame's gates, `gripHeld` a grip squeezed (no gate).
        float armsUpAge = 10.f, armsUpRun{};
        bool armsFromGrace{}, tipIn{}, windowClosed{}, gripHeld{};
        // TRACKING LOST: this frame's palms were a glitch (untracked, not finite, or a jump past glitchJump): the last
        // good support held.
        bool glitch{};
        // STICKY: this frame's palms jumped with the head (the rig moved; taken as good), and how long they
        // have been off the floor while holding (s).
        bool rigMoved{};
        float offFor{};
        // ABSURD ARMS: this frame's readings thrown out, bits 0/1 the palms, 2/3 the palms from the head (0 none).
        unsigned absurd{};
        unsigned failedNow{}, closedFailed{};
        float closedAge{};
    };
    // ABSURD ARMS: the readings that cannot be (bits as Out::absurd) marked unknown in `in`; a palm makes it untracked.
    static unsigned sanitize(Input& in, const Params& p) {
        unsigned bad = 0;
        for (int i = 0; i < 2; ++i) {
            const Vec3 palm = in.palms[i];
            const bool sane = finite(palm) &&
                              (in.headKnown && finite(in.head) ? length(palm - in.head) <= p.sanePalmReach
                               : in.floorKnown && std::isfinite(in.floorY)
                                   ? std::fabs(palm.y - in.floorY) <= p.sanePalmFloor
                                   : true);
            if (!sane && (in.tracked || finite(palm))) {
                bad |= 1u << i;
                in.palms[i] = Vec3{NAN, NAN, NAN};
            }
            if (in.palmRelKnown[i] && !(finite(in.palmRel[i]) && length(in.palmRel[i]) <= p.sanePalmReach)) {
                bad |= 4u << i;
                in.palmRelKnown[i] = false;
            }
        }
        if (bad & 3u)
            in.tracked = false;
        return bad;
    }
    Out update(const Input& raw, const Params& p) {
        Out out;
        Input in = raw;
        out.absurd = sanitize(in, p);
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        exit_ = Exit::none;
        // TRACKING LOST: a glitching palm keeps the last good support.
        const bool palmsFinite = in.tracked && finite(in.palms[0]) && finite(in.palms[1]);
        const bool palmsNear = !haveGood_ || (length(in.palms[0] - goodPalms_[0]) <= p.glitchJump &&
                                              length(in.palms[1] - goodPalms_[1]) <= p.glitchJump);
        // STICKY: the palms jumped together with the head - the rig re-placed, not the hands.
        const bool headNow = in.headKnown && finite(in.head);
        const bool rigMoved = p.sticky && palmsFinite && !palmsNear && haveGood_ && haveGoodHead_ &&
                              headNow &&
                              length((in.palms[0] - in.head) - (goodPalms_[0] - goodHead_)) <= p.rigShift &&
                              length((in.palms[1] - in.head) - (goodPalms_[1] - goodHead_)) <= p.rigShift;
        out.rigMoved = rigMoved;
        const bool palmsGood = palmsFinite && (palmsNear || rigMoved);
        const bool holding = state_ == State::catching || state_ == State::balance;
        if (palmsGood || !holding || !haveGood_) {
            support_.a = in.palms[0];
            support_.b = in.palms[1];
            if (palmsGood) {
                goodPalms_[0] = in.palms[0];
                goodPalms_[1] = in.palms[1];
                haveGood_ = true;
                haveGoodHead_ = headNow;
                if (headNow)
                    goodHead_ = in.head;
            } else if (!holding) {
                haveGood_ = haveGoodHead_ = false;
            }
        } else {
            support_.a = goodPalms_[0];
            support_.b = goodPalms_[1];
        }
        lostFor_ = palmsGood || !holding ? 0.f : lostFor_ + dt;
        out.glitch = holding && !palmsGood;
        support_.radius = p.palmRadius + p.margin;
        const float pushed = push_.update(dt, in.pushInto, p.pushWindow);
        // LEAP INPUT: A's press (not a held one), the left stick held hard for leapStickSeconds.
        const bool jumpPress = in.jump && !jumpWas_;
        jumpWas_ = in.jump;
        // A stick already held when the catch began has to come back below the line first (a fresh push).
        if (std::isfinite(in.stick) && in.stick < p.leapStick)
            stickFresh_ = true;
        stickHeld_ = stickFresh_ && std::isfinite(in.stick) && in.stick >= p.leapStick ? stickHeld_ + dt : 0.f;
        // THE TOUCHDOWN CATCH: how long since SpidyVR's air, the touchdown's
        // first frame, and the palms' heights over the floor.
        airAge_ = in.vrAir || in.aloft ? 0.f : std::min(airAge_ + dt, 10.f);
        const bool touchdown = in.touchdown && !in.vrAir && airAge_ <= p.touchdownGrace;
        const bool touchdownNow = touchdown && !touchdownBefore_;
        touchdownBefore_ = in.touchdown && !in.vrAir;
        unsigned palmsDown = in.floorPalms & 3u;
        float palmGap = -10.f;
        unsigned reachDown = 0;
        const float headGap = in.headKnown && finite(in.head) && in.floorKnown && std::isfinite(in.floorY)
                                  ? in.head.y - in.floorY
                                  : 1e9f;
        // Only at a touchdown, or in a touchdown's catch: in the air the actor's
        // feet are no floor.
        const bool nearFloor = state_ == State::off ? touchdown : touchdownCaught_;
        // HANDS OVER THE HEAD as the player holds them (Input::palmRel), in the air or not: when both were last,
        // and for how long (the grace before the touchdown counts them, and their hold).
        const bool overBoth = p.armsOverHead > 0.f && in.palmRelKnown[0] && in.palmRelKnown[1] && finite(in.palmRel[0]) &&
                              finite(in.palmRel[1]) && in.palmRel[0].y >= p.armsOverHead &&
                              in.palmRel[1].y >= p.armsOverHead;
        if (overBoth) {
            armsUpAge_ = 0.f;
            armsUpRun_ = std::min(armsUpRun_ + dt, 10.f);
        } else {
            armsUpAge_ = std::min(armsUpAge_ + dt, 10.f);
            if (armsUpAge_ > p.armsGraceBefore)
                armsUpRun_ = 0.f; // the stretch is out of the grace: forgotten
        }
        const bool armsRecent = !overBoth && armsUpRun_ > 0.f && armsUpAge_ <= p.armsGraceBefore;
        out.armsUpAge = armsUpAge_;
        out.armsUpRun = armsUpRun_;
        const float invertedNow = invertedDegrees(in.bodyUp);
        const float spinNow = finite(in.angular) ? length(in.angular) : 0.f;
        // TIP-IN: the intent is clear (inverted far, not a handspring, the hands over the head now or in the grace).
        const bool tipIntent = p.tipIn && state_ == State::off && nearFloor && invertedNow >= p.tipInDegrees &&
                               spinNow <= p.catchSpin && (overBoth || armsRecent);
        out.tipIn = tipIntent;
        // GRIP HOLDS THE HANDSTAND: a grip squeezed is the intent too, at the catch's own angle.
        const bool gripIntent = p.gripHold && in.grip && state_ == State::off && nearFloor &&
                                invertedNow >= p.catchDegrees && spinNow <= p.catchSpin;
        bool armsOk = true, reachOk = true;
        if (in.floorKnown && std::isfinite(in.floorY)) {
            out.floorY = in.floorY;
            for (unsigned i = 0; i < 2; ++i) {
                const float gap = finite(in.palms[i]) ? in.palms[i].y - in.floorY : 1e9f;
                palmGap = std::max(palmGap, gap);
                out.palmGaps[i] = gap < 1e8f ? gap : NAN;
                const float reach =
                    state_ == State::off
                        ? (tipIntent || gripIntent ? std::max(p.touchdownReach, p.tipInReach) : p.touchdownReach)
                        : p.floorReach * (p.sticky ? p.holdReachScale : 1.f);
                // The touchdown's reach path also needs the arms up: ARMS OVER HEAD, the palm over the head as the
                // player holds it (no tracked offset: no test; or both over it within the grace); 0, the old world
                // test - the palm clearly below the head (no head: no test).
                const bool armUpNow =
                    p.armsOverHead > 0.f
                        ? !in.palmRelKnown[i] || !finite(in.palmRel[i]) || in.palmRel[i].y >= p.armsOverHead
                        : !in.headKnown || gap + p.armsBelowHead <= headGap;
                const bool armsUp = armUpNow || armsRecent || gripIntent;
                if (armsUp && !armUpNow && nearFloor && state_ == State::off)
                    out.armsFromGrace = true;
                armsOk = armsOk && armsUp;
                reachOk = reachOk && gap <= reach;
                if (nearFloor && gap <= reach && (state_ != State::off || armsUp))
                    reachDown |= 1u << i;
            }
        } else {
            reachOk = false;
        }
        // Held for armsHoldSeconds before the reach counts (a touch on the floor's own contact is no wait; nor the
        // tip-in, nor a hold already made in the grace before the touchdown).
        const bool reachBoth = (reachDown & 3u) == 3u;
        if (state_ == State::off)
            armsHeld_ = reachBoth && nearFloor ? std::min(armsHeld_ + dt, 10.f) : 0.f;
        const bool heldBefore = armsUpRun_ >= p.armsHoldSeconds && armsUpAge_ <= p.armsGraceBefore;
        if (state_ != State::off || armsHeld_ >= p.armsHoldSeconds || ((in.floorPalms & 3u) == 3u) ||
            (reachBoth && (tipIntent || heldBefore || gripIntent)))
            palmsDown |= reachDown;
        out.armsUp = reachBoth;
        out.reachDown = reachDown & 3u;
        for (unsigned i = 0; i < 2; ++i) {
            out.palmRel[i] = in.palmRelKnown[i] && finite(in.palmRel[i]) ? in.palmRel[i] : Vec3{NAN, NAN, NAN};
            out.worldBelow[i] =
                in.headKnown && finite(in.head) && finite(in.palms[i]) ? in.head.y - in.palms[i].y : NAN;
        }
        if (finite(in.angular) && length(in.angular) > 1e-3f)
            out.flipAxis = normalized(in.angular);
        // The touchdown window (the log's throttled line): from the touchdown's first frame, while no catch is on.
        // EVERY REJECTION (Nov 17): the gates failing on each of its frames, and the last frame's when it closes.
        const bool bothNow = ((palmsDown & 3u) == 3u);
        const unsigned failedNow =
            refusalBit(!p.enabled ? Refusal::disabled : Refusal::none) |
            refusalBit(!in.tracked ? Refusal::untracked : Refusal::none) |
            refusalBit(!armed_ ? Refusal::notArmed : Refusal::none) |
            refusalBit(invertedNow < p.catchDegrees ? Refusal::upright : Refusal::none) |
            refusalBit(spinNow > p.catchSpin ? Refusal::spin : Refusal::none) |
            refusalBit(!bothNow && !reachOk ? Refusal::palmsHigh : Refusal::none) |
            refusalBit(!bothNow && !armsOk ? Refusal::armsDown : Refusal::none);
        out.gripHeld = in.grip;
        if (touchdownNow && state_ == State::off) {
            windowAge_ = 0.f;
            windowFailed_ = failedNow;
        } else if (windowAge_ >= 0.f) {
            windowAge_ += dt;
            if (windowAge_ > p.touchdownGrace || state_ != State::off || !touchdown) {
                if (state_ == State::off) {
                    out.windowClosed = true;
                    out.closedFailed = windowFailed_;
                    out.closedAge = windowAge_;
                }
                windowAge_ = -1.f;
            } else {
                windowFailed_ = failedNow;
            }
        }
        out.failedNow = windowAge_ >= 0.f ? failedNow : 0u;
        out.windowAge = windowAge_;
        out.armsHeld = armsHeld_;
        out.headGap = headGap < 1e8f ? headGap : 0.f;
        const bool both = bothNow;
        // After an exit a new catch waits for the palms to come off the floor.
        if (!palmsDown)
            armed_ = true;
        out.touchdownNow = touchdownNow && state_ == State::off;
        out.touchdownSeen = in.touchdown;
        out.aloftSeen = in.aloft;
        out.vrAirSeen = in.vrAir;
        out.airAge = airAge_;
        if (out.touchdownNow) {
            out.inverted = invertedNow;
            out.spin = spinNow;
            out.palmGap = palmGap;
            // A held grip is no gate (Nov 17: the catch carries it; Output::gripHeld says it was).
            out.failed = failedNow;
            out.refused = Refusal::none;
            for (unsigned r = 1; r <= lastRefusal; ++r)
                if (out.failed & (1u << r)) {
                    out.refused = static_cast<Refusal>(r);
                    break;
                }
        }
        switch (state_) {
        case State::off: {
            if (!p.enabled || !(in.vrAir || touchdown) || !in.tracked || !both || !armed_)
                break;
            const float spin = spinNow;
            if (invertedDegrees(in.bodyUp) < p.catchDegrees || spin > p.catchSpin)
                break;
            touchdownCaught_ = !in.vrAir;
            out.touchdownCatch = touchdownCaught_;
            // The CoM along the body's feet from the palms' centre.
            const Vec3 feetWay = finite(in.bodyUp) ? -normalized(in.bodyUp) : Vec3{0, 1, 0};
            const Vec3 c = support_.centre();
            const Vec3 raw = c + Vec3{feetWay.x, 0, feetWay.z} * p.comHeight;
            // Seeded over the palms: the offset the landing's tilt gave it is the body's, not a balance to lose.
            const Vec3 x0 = seedInside(support_, raw, p.seedShare);
            out.seedRaw = length(Vec3{raw.x - c.x, 0, raw.z - c.z});
            out.seedOffset = length(Vec3{x0.x - c.x, 0, x0.z - c.z});
            centre_ = c;
            frame_ = 0;
            reseedPending_ = true;
            // Damped: the floor takes most of the landing's speed and spin.
            const Vec3 v0 =
                finite(in.velocity) ? Vec3{in.velocity.x, 0, in.velocity.z} * p.entryKeep : Vec3{};
            const Vec3 spinSpeed = horizontal(
                cross((finite(in.angular) ? in.angular : Vec3{}) * p.entrySpinKeep, feetWay * p.comHeight));
            balance_.start(x0, limited(v0 + spinSpeed, p.entryMaxSpeed));
            last_ = {};
            last_.x = last_.xi = x0;
            gripCarried_ = in.grip;
            out.gripCarried = gripCarried_;
            out.gripCatch = gripIntent;
            offFor_ = 0.f;
            state_ = State::catching;
            age_ = 0;
            push_.reset();
            // The leap is a fresh push: a stick held now does not count until it is let go of once.
            stickFresh_ = !(std::isfinite(in.stick) && in.stick >= p.leapStick);
            stickHeld_ = 0.f;
            armsHeld_ = 0.f;
            out.caughtNow = true;
            break;
        }
        case State::catching:
        case State::balance: {
            age_ += dt;
            // STICKY: the palms off the floor for offSeconds before that ends it (none otherwise: at once, as
            // before).
            offFor_ = palmsDown ? 0.f : offFor_ + dt;
            const float offNeed = p.sticky ? p.offSeconds : 0.f;
            out.offFor = offFor_;
            if (!in.grip)
                gripCarried_ = false; // released once: a new squeeze lets go
            // GRIP HOLDS THE HANDSTAND: held, the stance is locked; the palms' time off the floor starts after it.
            const bool locked = p.gripHold && in.grip;
            out.gripLocked = locked;
            if (locked) {
                offFor_ = 0.f;
                out.offFor = 0.f;
            }
            Exit why = Exit::none;
            if (!p.enabled)
                why = Exit::disabled;
            else if (out.glitch && lostFor_ >= p.trackingGrace)
                why = Exit::tracking;
            else if (out.glitch)
                ; // TRACKING LOST: held through the grace (no other test on a glitching frame)
            else if (locked)
                ; // GRIP HOLDS THE HANDSTAND: no push, leap, step-down or miss until the grip is let go of
            else if (p.leap && (jumpPress || stickHeld_ >= p.leapStickSeconds))
                why = Exit::leap;
            else if (in.grip && !gripCarried_ && !p.gripHold)
                why = Exit::grip;
            else if (pushed > p.pushSpeed && palmsDown)
                why = Exit::push;
            else if (!palmsDown && offFor_ >= offNeed && state_ == State::catching &&
                     age_ > (touchdownCaught_ ? p.touchdownSettle : p.catchGrace))
                why = Exit::missed;
            else if (!palmsDown && offFor_ >= offNeed && state_ == State::balance)
                why = Exit::stepDown;
            if (why == Exit::none && !out.glitch) {
                if (state_ == State::catching && age_ >= p.catchGrace && both)
                    state_ = State::balance;
                // RE-SEED: the first frame after the catch (the handoff's origin, the support tilt) and any
                // palm re-placement while catching moves the palms, not the body.
                const Vec3 c = support_.centre();
                if (finite(c) && finite(centre_)) {
                    const Vec3 jump{c.x - centre_.x, 0, c.z - centre_.z};
                    out.centreJump = length(jump);
                    if (reseedPending_ || out.centreJump > p.reposeJump) {
                        balance_.shift(jump);
                        out.reseeded = true;
                        out.shift = out.centreJump;
                    }
                    centre_ = c;
                }
                reseedPending_ = false;
                const Vec3 aim =
                    support_.centre() + (finite(in.aim) ? limited(horizontal(in.aim), p.stickLean) : Vec3{});
                last_ = balance_.step(dt, support_, aim, p);
                // A catch is a settle for its first fallGrace: no fall test, and its timer starts clean.
                if (age_ < std::max(p.fallGrace, p.catchGrace)) {
                    balance_.forgive();
                    last_.fell = false;
                }
                // GRIP HOLDS THE HANDSTAND: no fall while held; the centre of mass stays over the palms.
                if (locked) {
                    balance_.steady(support_);
                    last_.x = balance_.x();
                    last_.v = balance_.v();
                    last_.fell = false;
                }
                if (last_.fell)
                    why = Exit::fall;
            }
            if (why != Exit::none) {
                exit_ = why;
                out.exitNow = why;
                if (why == Exit::push)
                    out.launch = launch(std::max(pushed, std::isfinite(in.pushInto) ? in.pushInto : 0.f),
                                        in.floorNormal, balance_.v(), p);
                if (why == Exit::leap)
                    out.launch = leapLaunch(in.viewForward, p);
                out.heldSeconds = age_;
                out.offBalanceSeconds = balance_.outside();
                // The tip's spin about the palms (world), for the flip to go on with.
                out.releaseSpin = why == Exit::fall
                                      ? cross(Vec3{0, 1, 0}, balance_.v()) / std::max(p.comHeight, .3f)
                                      : Vec3{};
                state_ = State::leaving;
                age_ = 0;
                armed_ = false;
                touchdownCaught_ = false;
                gripCarried_ = false;
            }
            break;
        }
        case State::leaving:
            age_ += dt;
            if (age_ >= p.leavingSeconds)
                state_ = State::off;
            break;
        }
        out.state = state_;
        out.supporting = state_ == State::catching || state_ == State::balance;
        out.holdVr = state_ != State::off;
        out.balance = last_;
        out.lean = balance_.x() - support_.centre();
        out.lean.y = 0;
        // The catch's geometry (the log's first frames): this update's, or none once the state is off.
        if (state_ == State::catching || state_ == State::balance || out.caughtNow) {
            out.frame = static_cast<int>(std::min<unsigned>(frame_, 1000000u));
            ++frame_;
            out.centre = support_.centre();
            out.span = finite(support_.a) && finite(support_.b)
                           ? std::hypot(support_.a.x - support_.b.x, support_.a.z - support_.b.z)
                           : 0.f;
            const Vec3 near = support_.clamp(last_.xi);
            out.xiOutside = std::hypot(last_.xi.x - near.x, last_.xi.z - near.z);
            out.outsideSeconds = balance_.outside();
        }
        return out;
    }
    State state() const {
        return state_;
    }
    Exit lastExit() const {
        return exit_;
    }
    void reset() {
        *this = {};
    }

  private:
    static Vec3 horizontal(Vec3 v) {
        return Vec3{v.x, 0, v.z};
    }
    State state_{};
    Exit exit_{};
    bool armed_ = true;
    bool touchdownBefore_{}, touchdownCaught_{}, gripCarried_{}, reseedPending_{}, jumpWas_{};
    float stickHeld_{}, armsHeld_{};
    bool stickFresh_ = true;
    Vec3 centre_{};
    unsigned frame_{};
    float age_{};
    float airAge_ = 10.f;
    float windowAge_ = -1.f;
    unsigned windowFailed_{};
    float armsUpAge_ = 10.f, armsUpRun_{};
    Balance balance_{};
    Balance::Out last_{};
    Push push_{};
    Support support_{};
    Vec3 goodPalms_[2]{};
    bool haveGood_{};
    float lostFor_{};
    // STICKY: the head when the good palms were read, and how long the palms have been off the floor while
    // holding.
    Vec3 goodHead_{};
    bool haveGoodHead_{};
    float offFor_{};
};

// The view's lean on the hands: the body's up straight down, turned toward
// the CoM's offset (feet over it). `inverted` is the tilt at the catch (the
// tracking space's axes), `leanLocal` the offset in the same axes, h the
// CoM's height.
inline Quat supportTilt(Quat inverted, Vec3 leanLocal, float h) {
    const Vec3 up = inverted.rotate({0, 1, 0});
    if (!finite(up) || length(up) < .5f)
        return inverted;
    // Straight down first (the shortest turn), then the lean.
    const Quat straight = arc(up, {0, -1, 0}) * inverted;
    const Vec3 feet = normalized(
        Vec3{finite(leanLocal) ? leanLocal.x : 0.f, std::max(h, .3f), finite(leanLocal) ? leanLocal.z : 0.f});
    return arc({0, 1, 0}, feet) * straight;
}

} // namespace spidy::handstand
