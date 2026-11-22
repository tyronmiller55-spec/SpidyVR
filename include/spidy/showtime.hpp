// SHOWTIME (EXPERIMENTAL, off by default; the plan's Pillar 6, the user, Nov 13 3:50 PM PT, built Nov 14 on t70u): hold X and B and
// the view steps outside Spidey - the headset still looks and leans 1:1 - and a let-go brings it back onto his head.
//
// HOW IT COEXISTS WITH "ANIMATION NEVER TURNS THE VIEW" (the user's standing rule, Nov 13/14: the headset's direction is the single
// per-frame view-heading authority, and animation never turns the player's view yaw in normal play). Showtime is an explicit,
// player-chosen camera mode, and it keeps the rule where it matters:
//   - Showtime never writes the view's heading authority. HybridHeading, the view yaw lock, the stick basis and the facing paths
//     are not touched while the camera is out, nor at the exit. What the headset looks along relative to its own latch is the
//     same before, during and after; the exit puts the eyes back on his head with exactly the heading they left.
//   - While out, the camera's own yaw (the "rig yaw", which turns the position round him) changes ONLY from the player's right
//     stick (the orbit), added to the head's own turn. No animation, dodge, landing, handoff, owner switch, enemy or finisher
//     turns it: a dodge or a flip that spins him spins him in the picture, not the picture. At entry the rig yaw starts at the
//     heading the player was looking along (not at his body's), so the cut turns nothing.
//   - The right stick belongs to the orbit while out: no snap or smooth turn of the play heading, no zip, no heal.
//   - The play-side views (hands, markers, aim) are not drawn from the outside camera (scene mode): the player's hands are not in
//     the picture.
// Pure; no Windows. See docs/SHOWTIME.md.
#pragma once
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::showtime {

inline constexpr float chordSeconds = .2f; // X and B held this long before the camera leaves
inline constexpr float maxTeleportMetres = 30.f; // a feet jump of this much in a frame is a teleport or a load

// ---- The chord: hold X + B ----
// X first: B never reaches the game (no dodge). B first: B passes at once (the dodge, perfect-dodge timing kept), then X and
// B held 200 ms enter. Y held blocks it (Y+X+B is the passive legs' flail, PASSIVE_LEGS.md). A+B, hold B then A: the point jump,
// no X: not a chord. From the moment both are down X is Showtime's (no gadget wheel, no Back); once entered B is too.
enum class Lead : uint8_t { none, x, b };
inline const char* leadName(Lead l) {
    return l == Lead::x ? "X" : l == Lead::b ? "B" : "none";
}
class Chord {
  public:
    struct Input {
        float seconds{};
        bool x{}, y{}, b{};
        bool allowed{}; // Showtime on and a chord may start (in play, not calibrating, no prompt naming X or B)
    };
    struct Output {
        bool pending{}, active{};   // both held, waiting; held long enough
        bool entered{}, ended{};    // on this frame
        Lead lead{};
        bool swallowX{}, swallowB{}; // what must not reach the game or the mod this frame
        float held{};
    };
    Output update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        Output out;
        const bool xPressed = in.x && !xWas_, bPressed = in.b && !bWas_;
        xWas_ = in.x;
        bWas_ = in.b;
        if (!in.allowed) {
            const bool was = active_;
            reset();
            out.ended = was;
            return out;
        }
        if (blocked_ && !in.x && !in.b)
            blocked_ = false;
        if (!pending_ && !active_) {
            // Y with the pair: the flail's (or not a chord at all): nothing of Showtime's until both are let go of.
            if (in.x && in.b && in.y && (xPressed || bPressed))
                blocked_ = true;
            if (!blocked_ && in.x && in.b && !in.y && (xPressed || bPressed)) {
                pending_ = true;
                held_ = 0;
                // Which was down first: B already down when X came is B first; both on one frame, or B after X, X first (so
                // B never reaches the game).
                lead_ = xPressed && !bPressed ? Lead::b : Lead::x;
            }
        }
        if (pending_ || active_) {
            if (!in.x || !in.b || (pending_ && in.y)) {
                out.ended = active_;
                if (pending_ && in.y)
                    blocked_ = true;
                // X first: B was never the game's. Entered: the game saw B go away at the entry. Either way a B still down is
                // held back until it is let go of, never a fresh press for the game.
                const bool keepB = in.b && (lead_ == Lead::x || active_);
                pending_ = active_ = false;
                held_ = 0;
                if (keepB)
                    bHeldBack_ = true;
                lead_ = Lead::none;
            } else if (pending_) {
                held_ += dt;
                if (held_ >= chordSeconds) {
                    pending_ = false;
                    active_ = true;
                    out.entered = true;
                }
            }
        }
        if (bHeldBack_ && !in.b)
            bHeldBack_ = false;
        out.pending = pending_;
        out.active = active_;
        out.lead = lead_;
        out.held = held_;
        out.swallowX = pending_ || active_;
        out.swallowB = active_ || (pending_ && lead_ == Lead::x) || bHeldBack_;
        return out;
    }
    void reset() {
        *this = {};
    }
    bool active() const {
        return active_;
    }

  private:
    bool xWas_{}, bWas_{}, pending_{}, active_{}, blocked_{}, bHeldBack_{};
    float held_{};
    Lead lead_{};
};

// ---- The fade ----
// FADE TIME is the whole cut: the picture goes to black over the first half, the eyes change place under it, and it clears
// over the second. 0 is a plain cut.
class Fade {
  public:
    float level() const {
        return level_;
    }
    // Rise toward black (`rise`), or clear. Returns true on the frame the picture is fully black (the cut's moment).
    bool step(float dt, float fadeSeconds, bool rise) {
        const float half = std::max(fadeSeconds, 0.f) * .5f;
        if (half <= 1e-4f) {
            const bool black = rise;
            level_ = 0;
            return black;
        }
        const float d = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) / half : 0.f;
        level_ = std::clamp(level_ + (rise ? d : -d), 0.f, 1.f);
        return rise && level_ >= 1.f;
    }
    void reset() {
        level_ = 0;
    }

  private:
    float level_{};
};

// ---- The machine ----
// strike: the game's own animated web strike (its web rush; since 0.8.7-na-beta3d), punch: SpidyVR's manual
// punch that flies him to an enemy (WebStrike; until beta3d this was "web strike").
enum class Why : uint8_t { none, chord, finisher, dodge, strike, punch };
inline const char* whyName(Why w) {
    switch (w) {
    case Why::chord:
        return "chord";
    case Why::finisher:
        return "finisher";
    case Why::dodge:
        return "dodge";
    case Why::strike:
        return "web strike";
    case Why::punch:
        return "punch";
    default:
        return "none";
    }
}
enum class ExitWhy : uint8_t { none, release, disabled, menu, teleport, control };
inline const char* exitName(ExitWhy w) {
    switch (w) {
    case ExitWhy::release:
        return "release";
    case ExitWhy::disabled:
        return "disabled";
    case ExitWhy::menu:
        return "menu";
    case ExitWhy::teleport:
        return "teleport";
    case ExitWhy::control:
        return "lost control";
    default:
        return "none";
    }
}
enum class State : uint8_t { head, toOut, out, toHead };
class Machine {
  public:
    struct Input {
        float seconds{};
        float fadeSeconds = .15f;
        bool wantOut{};             // the chord is held (or an auto moment is on)
        Why wantWhy = Why::chord;
        bool allowed{};             // Showtime may have the view now
        ExitWhy forced = ExitWhy::none; // leave at once (under the fade): menu, a teleport, control lost
    };
    struct Output {
        State state = State::head;
        bool out{};              // the eyes are outside this frame
        bool entered{}, left{};  // the cut's frame
        Why enterWhy{};
        ExitWhy exitWhy{};
        float fade{};            // 0 clear .. 1 black
        float outSeconds{};
    };
    Output update(const Input& in) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        Output o;
        bool wanted = in.wantOut && in.allowed && in.forced == ExitWhy::none;
        ExitWhy why = in.forced;
        if (!in.allowed && why == ExitWhy::none)
            why = ExitWhy::disabled;
        if (wanted && state_ == State::head) {
            enterWhy_ = in.wantWhy;
            started_ = 0;
        }
        if (state_ != State::head)
            started_ += dt;
        const bool instant = in.fadeSeconds <= 1e-4f;
        switch (state_) {
        case State::head:
            if (wanted) {
                if (instant) {
                    state_ = State::out;
                    o.entered = true;
                    o.enterWhy = enterWhy_;
                    outSeconds_ = 0;
                } else {
                    state_ = State::toOut;
                    clearing_ = false;
                }
            }
            break;
        case State::toOut:
            if (!wanted) {
                state_ = State::head; // let go under the fade: the picture clears again
                clearing_ = true;
            } else if (fade_.step(dt, in.fadeSeconds, true)) {
                state_ = State::out;
                o.entered = true;
                o.enterWhy = enterWhy_;
                outSeconds_ = 0;
                clearing_ = true;
            }
            break;
        case State::out:
            outSeconds_ += dt;
            if (!wanted) {
                exitWhy_ = why == ExitWhy::none ? ExitWhy::release : why;
                if (instant) {
                    state_ = State::head;
                    o.left = true;
                    o.exitWhy = exitWhy_;
                } else {
                    state_ = State::toHead;
                    clearing_ = false;
                }
            }
            break;
        case State::toHead:
            outSeconds_ += dt;
            if (wanted) {
                state_ = State::out;
                clearing_ = true;
            } else if (fade_.step(dt, in.fadeSeconds, true)) {
                state_ = State::head;
                o.left = true;
                o.exitWhy = exitWhy_;
                clearing_ = true;
            }
            break;
        }
        if (clearing_ && !(o.entered || o.left) && (state_ == State::head || state_ == State::out)) {
            fade_.step(dt, in.fadeSeconds, false);
            if (fade_.level() <= 0.f)
                clearing_ = false;
        }
        if (instant) {
            fade_.reset();
            clearing_ = false;
        }
        o.state = state_;
        o.out = state_ == State::out || state_ == State::toHead;
        o.fade = fade_.level();
        o.outSeconds = outSeconds_;
        return o;
    }
    State state() const {
        return state_;
    }
    bool out() const {
        return state_ == State::out || state_ == State::toHead;
    }
    void reset() {
        *this = {};
    }

  private:
    State state_ = State::head;
    Fade fade_;
    bool clearing_{};
    Why enterWhy_{};
    ExitWhy exitWhy_{};
    float outSeconds_{}, started_{};
};

// ---- LATCH: a double press keeps the camera out ----
// OFF (the default): out only while the chord is held. DOUBLE PRESS: a chord entered again within `window` of the last one's
// end latches the camera out after the let-go; the next chord press (its rising edge) takes the latch off, and the let-go after
// it leaves. Any forced exit (menu, teleport, control lost) clears it.
class Latch {
  public:
    static constexpr float window = .8f;
    // `active`: the chord is held long enough (Chord::Output::active). Returns whether the camera is wanted out.
    bool update(bool enabled, bool active, float seconds) {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        clock_ += dt;
        if (!enabled) {
            latched_ = false;
            was_ = active;
            return active;
        }
        const bool rose = active && !was_, fell = !active && was_;
        was_ = active;
        if (rose) {
            if (latched_) {
                latched_ = false; // pressed again: the latch comes off, the let-go leaves
                armed_ = false;
            } else if (hasEnd_ && clock_ - endAt_ <= window)
                armed_ = true; // the second press: the let-go keeps it
        }
        if (fell) {
            endAt_ = clock_;
            hasEnd_ = true;
            if (armed_) {
                latched_ = true;
                armed_ = false;
            }
        }
        return active || latched_;
    }
    bool latched() const {
        return latched_;
    }
    void clear() {
        latched_ = armed_ = false;
    }
    void reset() {
        *this = {};
    }

  private:
    float clock_{}, endAt_{};
    bool was_{}, latched_{}, armed_{}, hasEnd_{};
};

// ---- Automatic moments (AUTO: DODGES) ----
// A dodge steps the camera out by itself: PERFECT ONLY on the game's perfect dodge (the moment that slows its time), ALL on any dodge.
// It ends 0.3 s after the dodge does, unless the chord is held (the machine keeps the camera while either wants it). If the cause is
// never seen live within 0.5 s it gives up; nothing keeps it out longer than 4 s. Why::finisher is not triggered by anything in this
// build (AUTO: FINISHERS only logs).
class Auto {
  public:
    static constexpr float tailSeconds = .3f, giveUpSeconds = .5f, capSeconds = 4.f;
    // AUTO: FINISHERS (showtime_auto_finisher, 0.8.7-na-beta3): a Y+B finisher plays up
    // to 6.3 s (the Nov 19-21 logs), so its moment may last this long; the tail and the give-up are the same.
    static constexpr float finisherCapSeconds = 10.f;
    void trigger(Why why, float cap = capSeconds) {
        why_ = why;
        cap_ = std::isfinite(cap) && cap > 0.f ? cap : capSeconds;
        on_ = true;
        t_ = tail_ = 0;
        seen_ = false;
    }
    // `live`: the cause is going on now (the dodge). True while the camera is wanted out.
    bool update(float seconds, bool live) {
        if (!on_)
            return false;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        t_ += dt;
        if (live) {
            seen_ = true;
            tail_ = 0;
        } else if (seen_ || t_ > giveUpSeconds) {
            tail_ += dt;
            if (tail_ >= tailSeconds) {
                on_ = false;
                return false;
            }
        }
        if (t_ > cap_) {
            on_ = false;
            return false;
        }
        return true;
    }
    bool on() const {
        return on_;
    }
    Why why() const {
        return why_;
    }
    void clear() {
        on_ = false;
    }

  private:
    Why why_ = Why::dodge;
    float t_{}, tail_{}, cap_ = capSeconds;
    bool on_{}, seen_{};
};

// HOW OFTEN (showtime_auto_<kind>_every / _cooldown_ms, 0.8.7-na-beta3b; the owner, Nov 21 t88u: "different
// timings for how often it fires"): one gate per kind of moment. Every event of the kind counts; the camera
// goes out on the Nth (every 1: each one) once at least cooldownMs have passed since that kind last took it
// out (0: no wait). An event held back by the cooldown leaves the count full, so the first one after the wait
// goes out.
struct AutoGate {
    int count{};
    int64_t lastMs = -1;
    enum class Result : uint8_t { fire, counting, cooling };
    Result event(int64_t nowMs, int every, int cooldownMs) {
        const int n = std::clamp(every, 1, 10);
        count = (std::min)(count + 1, n);
        if (count < n)
            return Result::counting;
        if (cooldownMs > 0 && lastMs >= 0 && nowMs - lastMs < cooldownMs)
            return Result::cooling;
        count = 0;
        lastMs = nowMs;
        return Result::fire;
    }
    bool fire(int64_t nowMs, int every, int cooldownMs) {
        return event(nowMs, every, cooldownMs) == Result::fire;
    }
};

// What keeps an automatic moment going (Auto::update's `live`): the dodge while it dodges, the game's
// finisher while its scene plays (finisher_handoff::Watch::active), the game's web strike while its web rush
// is the hero's dodge (WebRush::live), a punch while SpidyVR's WebStrike is not idle.
inline bool autoLive(Why why, bool dodging, bool finisher, bool strike, bool punch = false) {
    switch (why) {
    case Why::dodge:
        return dodging;
    case Why::finisher:
        return finisher;
    case Why::strike:
        return strike;
    case Why::punch:
        return punch;
    default:
        return false;
    }
}

// Where the camera may go out (game_xr's stOk), and the first thing that keeps it in (for the log). AUTO:
// FINISHERS (autoScene: its automatic moment is on): the Y+B finisher is a first-person scene, not play - the
// game's follow camera is not the one in use, so neither gameplay nor the tracking rig is (GameTrackingRig:
// Inactive notPlaying, "a menu, a scene, the game's own camera"). Until 0.8.7-na-beta3d the moment got past
// `gameplay` but not `rigActive`: the first frame of the scene cleared the trigger it had just been given and
// the camera never went out (the beta3b log: finishers at 1033.984 s and 1052.234 s, the head probe "combat
// -> cutscene" across them, no "showtime: enter (finisher)"). Now it gets past both.
struct OkInput {
    bool enabled{}, body{}, flatScreen{}, viewing{}, gameplay{}, rigActive{}, spiderMan{}, calibrating{},
        phone{}, padLatched{}, autoScene{};
};
inline const char* heldBackBy(const OkInput& in) {
    if (!in.enabled)
        return "Showtime off";
    if (!in.body)
        return "the body off";
    if (in.flatScreen)
        return "the flat screen";
    if (!in.viewing)
        return "no view";
    if (!in.gameplay && !in.autoScene)
        return "not in play";
    if (!in.rigActive && !in.autoScene)
        return "the tracking rig off";
    if (!in.spiderMan)
        return "not Spider-Man";
    if (in.calibrating)
        return "calibrating";
    if (in.phone)
        return "the phone open";
    if (in.padLatched)
        return "the pad layer latched";
    return nullptr;
}
inline bool ok(const OkInput& in) {
    return heldBackBy(in) == nullptr;
}

// AUTO: STRIKES vs AUTO: PUNCHES (0.8.7-na-beta3d; the owner, Nov 21 t93u: "Let's only have it on animation
// web strikes, not manual SpidyVR punches. Or, if we do, let's give it a toggle"). The game's own web strike
// is HeroStateWebRush (docs/REFERENCE.md), which runs as a dodge of kind 8 in the hero's dodge manager
// (game_dodge.hpp: "8 the web rush"; SpidyDodgeLive, the hook DODGE TUMBLE reads; the beta3b log: "the game's
// dodge began (kind 8)" at each web strike). SpidyVR's manual punch is its own WebStrike
// (game_punch::Strikes::opened), which never runs a kind 8 dodge. WebRush reads SpidyDodgeLive once a frame:
// began() is true for the frame a web rush starts (the manager's count of dodges moved on and the latest is
// of kind 8, not Spidy's own web dodge); the first sample only takes the count.
struct WebRush {
    static constexpr uint32_t kind = 8;
    uint64_t seen = ~0ull;
    bool began(uint64_t starts, uint32_t lastKind, uint32_t ours) {
        if (seen == ~0ull || starts < seen) {
            seen = starts;
            return false;
        }
        if (starts == seen)
            return false;
        seen = starts;
        return lastKind == kind && !ours;
    }
    // The rush goes on: the dodge on now is of kind 8.
    static bool live(uint32_t kindNow) {
        return kindNow == kind;
    }
};

// ---- The outside camera's frame ----
// The level frame game_fp hands the eyes (game_fp.cpp cameraFrame's layout: columns right, down, forward, position; the game's
// camera looks along +z, so the forward column is (ax, az) and the yaw the rig has it, 0 facing -z, is atan2(-ax, -az)).
inline Mat4 levelFrame(Vec3 position, float yaw) {
    const float ax = -std::sin(yaw), az = -std::cos(yaw);
    return {-az, 0, ax, 0, 0, -1, 0, 0, ax, 0, az, 0, position.x, position.y, position.z, 1};
}
inline float yawOfForward(float ax, float az) {
    return std::atan2(-ax, -az);
}
inline float wrap(float a) {
    return std::remainder(a, 6.2831853f);
}

// ---- What the player's hands do while he is out ----
// WATCH ONLY (the default): nothing new starts: no web, ball, punch or strike, no walking, jump or dodge. A web already out stays
// held (its hand keeps its squeeze). KEEP CONTROL: the left stick walks relative to the camera's heading, A jumps, B dodges.
enum class While : uint8_t { watch, keep };
// The left stick turned by the camera's orbit since entry (`delta`, rig yaw now minus at entry): the stick's basis is the view's,
// and the view the walking runs from is the head's, not the orbit's. Yaw as the rig has it (counter-clockwise from above).
inline void rotateStick(float& x, float& y, float delta) {
    if (!std::isfinite(delta) || !std::isfinite(x) || !std::isfinite(y))
        return;
    const float c = std::cos(delta), s = std::sin(delta);
    const float nx = c * x - s * y, ny = s * x + c * y;
    x = nx;
    y = ny;
}
} // namespace spidy::showtime
