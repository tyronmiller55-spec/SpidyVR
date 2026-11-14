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
    float gain = 3.f;       // kxi: how hard the DCM is steered back
    bool assist = true;     // off: the centre of pressure stays mid-palms (no steering)
    float palmRadius = .06f, margin = .06f;
    float catchDegrees = 100; // inverted at least this far to catch
    float catchSpin = 5.f;    // rad/s; faster is a handspring (H2): no catch (3 before Nov 13)
    float fallSeconds = .12f, fallLeanDegrees = 35;
    float pushSpeed = .6f, pushGain = 2.2f, pushWindow = .05f; // m/s into the floor
    float pushMaxNormal = 7.f, pushMaxAcross = 4.f;
    float catchGrace = .15f, leavingSeconds = .3f;
    // THE TOUCHDOWN CATCH (above).
    float touchdownGrace = .25f;  // s after SpidyVR's air a touchdown can still catch
    float touchdownReach = 1.6f;  // m: palms this far over the floor at the touchdown count (the
                                  // eyes' drop, .9, and an arm, .7: where the palms come to rest)
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
};

template <class V> Params paramsOf(const V& v) {
    Params p;
    p.enabled = v.handstand && v.realisticFlips;
    p.assist = v.handstandAssist;
    p.comHeight = std::clamp(v.handstandHeightCm, 60, 140) / 100.f;
    p.gain = std::clamp(v.handstandGainPct, 50, 600) / 100.f;
    p.pushGain = std::clamp(v.handstandPushGain, 100, 400) / 100.f;
    p.pushMaxNormal = static_cast<float>(std::clamp(v.handstandPushMax, 2, 12));
    p.fallLeanDegrees = static_cast<float>(std::clamp(v.handstandFallDegrees, 15, 60));
    p.eyeDrop = std::clamp(v.handstandEyeDropCm, 0, 120) / 100.f;
    return p;
}

enum class State : uint8_t { off, catching, balance, leaving };
enum class Exit : uint8_t { none, push, stepDown, fall, grip, tracking, disabled, missed };
// Why a touchdown out of SpidyVR's air did not catch (the log's reason).
enum class Refusal : uint8_t { none, disabled, untracked, grip, notArmed, upright, spin, palmsHigh };
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
    for (unsigned r = 1; r <= static_cast<unsigned>(Refusal::palmsHigh); ++r) {
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
            x_ += v_ * sub;
            out.u = u;
        }
        out.x = x_;
        out.v = v_;
        out.xi = x_ + v_ / w0;
        bool xiInside = true;
        s.clamp(out.xi, &xiInside);
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
        Vec3 floorNormal{0, 1, 0};
        Vec3 bodyUp{0, 1, 0}; // the body's up in the world (the flip's tilt)
        Vec3 angular{};       // the flip's spin, rad/s
        Vec3 velocity{};      // the body's, world m/s
        float pushInto{};     // palms' tracked speed into the floor, m/s (+ into)
        Vec3 aim{};           // the left stick's aim for the balance (level, m, from the support's centre)
        // THE TOUCHDOWN CATCH: SpidyVR owns him and the game reports ground
        // this frame; the floor's height under him (the actor's feet).
        bool touchdown{};
        bool aloft{}; // SpidyVR owns him off the ground (vrAir or not): the grace's start
        bool floorKnown{};
        float floorY{};
    };
    struct Out {
        State state{};
        bool holdVr{};     // SpidyVR keeps the body (Frame::handSupport)
        bool supporting{}; // the view holds the balance's lean (RealisticFlight::handSupport)
        bool caughtNow{};
        Exit exitNow{};     // this frame's exit (none: none)
        Vec3 launch{};      // with Exit::push: the launch velocity
        Vec3 releaseSpin{}; // the spin the flip goes on with
        Balance::Out balance{};
        Vec3 lean{}; // the CoM's level offset from the support's centre
        // A touchdown out of SpidyVR's air this frame (first ground frame),
        // and why it did not catch (none: it caught, or no touchdown).
        bool touchdownNow{};
        bool touchdownCatch{}; // this frame's catch was the touchdown's
        Refusal refused{};
        float inverted{}, spin{}, palmGap{}; // at the touchdown: degrees, rad/s, m (the higher palm)
        unsigned failed{};  // every gate that failed at the touchdown (refusalBit); 0: none or no touchdown
        bool gripCarried{}; // the catch took a squeezed grip: it lets go only after a release
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
    };
    Out update(const Input& in, const Params& p) {
        Out out;
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        exit_ = Exit::none;
        support_.a = in.palms[0];
        support_.b = in.palms[1];
        support_.radius = p.palmRadius + p.margin;
        const float pushed = push_.update(dt, in.pushInto, p.pushWindow);
        // THE TOUCHDOWN CATCH: how long since SpidyVR's air, the touchdown's
        // first frame, and the palms' heights over the floor.
        airAge_ = in.vrAir || in.aloft ? 0.f : std::min(airAge_ + dt, 10.f);
        const bool touchdown = in.touchdown && !in.vrAir && airAge_ <= p.touchdownGrace;
        const bool touchdownNow = touchdown && !touchdownBefore_;
        touchdownBefore_ = in.touchdown && !in.vrAir;
        unsigned palmsDown = in.floorPalms & 3u;
        float palmGap = -10.f;
        // Only at a touchdown, or in a touchdown's catch: in the air the actor's
        // feet are no floor.
        const bool nearFloor = state_ == State::off ? touchdown : touchdownCaught_;
        if (in.floorKnown && std::isfinite(in.floorY)) {
            for (unsigned i = 0; i < 2; ++i) {
                const float gap = finite(in.palms[i]) ? in.palms[i].y - in.floorY : 1e9f;
                palmGap = std::max(palmGap, gap);
                const float reach = state_ == State::off ? p.touchdownReach : p.floorReach;
                if (nearFloor && gap <= reach)
                    palmsDown |= 1u << i;
            }
        }
        const bool both = (palmsDown & 3u) == 3u;
        // After an exit a new catch waits for the palms to come off the floor.
        if (!palmsDown)
            armed_ = true;
        const float spinNow = finite(in.angular) ? length(in.angular) : 0.f;
        out.touchdownNow = touchdownNow && state_ == State::off;
        out.touchdownSeen = in.touchdown;
        out.aloftSeen = in.aloft;
        out.vrAirSeen = in.vrAir;
        out.airAge = airAge_;
        if (out.touchdownNow) {
            out.inverted = invertedDegrees(in.bodyUp);
            out.spin = spinNow;
            out.palmGap = palmGap;
            // A grip squeezed with both palms down is no gate (the catch keeps it, below).
            const bool gripGate = in.grip && !both;
            out.failed = refusalBit(!p.enabled ? Refusal::disabled : Refusal::none) |
                         refusalBit(!in.tracked ? Refusal::untracked : Refusal::none) |
                         refusalBit(gripGate ? Refusal::grip : Refusal::none) |
                         refusalBit(!armed_ ? Refusal::notArmed : Refusal::none) |
                         refusalBit(out.inverted < p.catchDegrees ? Refusal::upright : Refusal::none) |
                         refusalBit(spinNow > p.catchSpin ? Refusal::spin : Refusal::none) |
                         refusalBit(!both ? Refusal::palmsHigh : Refusal::none);
            out.refused = Refusal::none;
            for (unsigned r = 1; r <= static_cast<unsigned>(Refusal::palmsHigh); ++r)
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
            state_ = State::catching;
            age_ = 0;
            push_.reset();
            out.caughtNow = true;
            break;
        }
        case State::catching:
        case State::balance: {
            age_ += dt;
            if (!in.grip)
                gripCarried_ = false; // released once: a new squeeze lets go
            Exit why = Exit::none;
            if (!p.enabled)
                why = Exit::disabled;
            else if (!in.tracked)
                why = Exit::tracking;
            else if (in.grip && !gripCarried_)
                why = Exit::grip;
            else if (pushed > p.pushSpeed && palmsDown)
                why = Exit::push;
            else if (!palmsDown && state_ == State::catching &&
                     age_ > (touchdownCaught_ ? p.touchdownSettle : p.catchGrace))
                why = Exit::missed;
            else if (!palmsDown && state_ == State::balance)
                why = Exit::stepDown;
            if (why == Exit::none) {
                if (state_ == State::catching && age_ >= p.catchGrace && both)
                    state_ = State::balance;
                // RE-SEED: the first frame after the catch (the handoff's origin, the support tilt) and any
                // palm re-placement while catching moves the palms, not the body.
                const Vec3 c = support_.centre();
                if (finite(c) && finite(centre_)) {
                    const Vec3 jump{c.x - centre_.x, 0, c.z - centre_.z};
                    out.centreJump = length(jump);
                    if (reseedPending_ || (state_ == State::catching && out.centreJump > p.reposeJump)) {
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
                // Catching is a settle for its first catchGrace: no fall test, and its timer starts clean.
                if (state_ == State::catching && age_ < p.catchGrace) {
                    balance_.forgive();
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
    bool touchdownBefore_{}, touchdownCaught_{}, gripCarried_{}, reseedPending_{};
    Vec3 centre_{};
    unsigned frame_{};
    float age_{};
    float airAge_ = 10.f;
    Balance balance_{};
    Balance::Out last_{};
    Push push_{};
    Support support_{};
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
