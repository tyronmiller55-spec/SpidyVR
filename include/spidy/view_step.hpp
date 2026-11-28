#pragma once
// VIEW STEP: what moves the view that the player's head did not (0.8.7-na-beta3r; the owner, Nov 27 t157u / t158s981,
// and the cap fix t162s1012 "cap only real landing switches"; plan handoff-blend-1127.md O1 + O4, regression
// beta3q-regression-1127.md Option A).
//
// MEASURE: every frame, the shown eye's change minus what the headset itself did and what the view's anchor did (the
// rig - SpidyVR's own placement, flips and snap turns included - or the animated head the view sits on, its lock turn
// included). What is left is a step no head made: a handoff's blend or snap, a place, a blend's end, a fresh latch, a
// reseat, a guard's put-back. On a source switch (a new owner or eye mode, or the anchor jumping), the view is
// expected to go on as it was going (the old anchor's velocity): the switch's own step is what is left.
//
// VIEW JUMP LOG (view_jump_log): logging only. Any frame with more than 2 cm or 1 degree left is logged with its cause.
//
// SMOOTH VIEW HANDOFF (smooth_view_handoff): one offset on the shown view that always decays - a critically damped
// spring (settles in `settleSeconds`, 250-350 ms; at most `maxSpeed` 1.5 m/s), seeded on a switch with the old
// source's velocity so the speed carries through - and a comfort cap: while a real handoff is under way (a blend, a
// switch's first `windowSeconds`, a landing snap or place, a latch or guard frame; `capWindowBits`) no more than
// `capM` (2 cm) and `capRad` (1 degree) of non-head motion reaches the eyes in a frame; the excess goes into the
// offset. An anchor jump, a head-seat reseat, a swing, a zip and SpidyVR's air are not a handoff: they are not
// capped, and any leftover offset is dropped so it cannot hold the head off the body (beta3r). A real teleport
// (over `teleportM` or `teleportRad`) is let through and the offset dropped. Portable, pure.
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace spidy::view_step {

inline constexpr float pi = 3.14159265f;
inline constexpr float degrees = pi / 180.f;
inline constexpr float logM = .02f;     // VIEW JUMP LOG: over 2 cm ...
inline constexpr float logDeg = 1.f;    // ... or 1 degree in a frame
inline constexpr float anchorCutM = .4f; // the anchor's own motion changed this much in a frame: a cut, not travel

// A rotation as its axis times its angle (radians), and back.
inline Vec3 toRotVec(Quat q) {
    if (q.w < 0)
        q = {-q.x, -q.y, -q.z, -q.w};
    const float s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
    if (!(s > 1e-7f))
        return {2 * q.x, 2 * q.y, 2 * q.z};
    const float angle = 2.f * std::atan2(s, std::clamp(q.w, -1.f, 1.f));
    return Vec3{q.x, q.y, q.z} * (angle / s);
}
inline Quat fromRotVec(Vec3 r) {
    const float a = length(r);
    return a > 1e-7f ? Quat::around(r, a) : Quat{};
}
inline Quat unit(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f && std::isfinite(n) ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : Quat{};
}
inline bool finitePose(const Pose& p) {
    return finite(p.position) && std::isfinite(p.orientation.x) && std::isfinite(p.orientation.y) &&
           std::isfinite(p.orientation.z) && std::isfinite(p.orientation.w);
}

// What caused a step (game_xr sets the bits during the frame; the highest one names it).
enum Cause : uint32_t {
    ownerSwitch = 1u << 0,  // the owner switched (vr->anim or anim->vr) this frame
    sourceChange = 1u << 1, // the eye command's mode changed (tracked eyes / on the head / level)
    anchorCut = 1u << 2,    // the view's anchor jumped (the rig placed, a head cut)
    landingSnap = 1u << 3,  // LANDING VIEW SNAP placed part of the gap at once
    place = 1u << 4,        // HANDOFF PLACE placed the gap at once
    blend = 1u << 5,        // a handoff blend is under way
    blendEnd = 1u << 6,     // a handoff blend ended this frame (back onto the head lock)
    latch = 1u << 7,        // the head lock latched afresh
    seat = 1u << 8,         // the head seat reseated
    guard = 1u << 9,        // the view guard put a turn back
};
inline const char* causeName(uint32_t bits) {
    if (bits & ownerSwitch) return "owner switch";
    if (bits & sourceChange) return "view source change";
    if (bits & anchorCut) return "anchor jump";
    if (bits & landingSnap) return "landing view snap";
    if (bits & place) return "handoff place";
    if (bits & blendEnd) return "blend end";
    if (bits & latch) return "head lock latch";
    if (bits & seat) return "head seat reseat";
    if (bits & guard) return "view guard";
    if (bits & blend) return "handoff blend";
    return "unknown";
}
// Every bit set, "a+b+c", into `out` (at least 1 byte).
inline void causeList(uint32_t bits, char* out, size_t size) {
    static const char* names[] = {"switch", "source", "anchor", "snap", "place", "blend",
                                  "blend-end", "latch", "seat", "guard"};
    size_t n = 0;
    out[0] = 0;
    for (unsigned i = 0; i < 10; ++i)
        if (bits & (1u << i)) {
            const int w = std::snprintf(out + n, size - n, "%s%s", n ? "+" : "", names[i]);
            if (w < 0 || static_cast<size_t>(w) >= size - n)
                break;
            n += static_cast<size_t>(w);
        }
    if (!n)
        std::snprintf(out, size, "none");
}

// The comfort cap's window (beta3r, t162s1012): a real handoff only. An anchor cut (a 0.4 m rig step in the air,
// a swing or a zip) and a head-seat reseat (it keeps its own glide) are not in this. `afterSwitch` is the
// switch's first windowSeconds.
inline constexpr uint32_t capWindowBits =
    ownerSwitch | sourceChange | landingSnap | place | blend | blendEnd | latch | guard;
inline bool capWindow(uint32_t cause, bool afterSwitch) {
    return (cause & capWindowBits) != 0 || afterSwitch;
}

struct Frame {
    bool ok = false;      // the shown eye is known (not the flat screen, not Showtime)
    Pose shown{};         // the shown left eye, in the world, before any smoothing offset
    Pose anchor{};        // what the view sits on: the rig (tracked eyes) or the animated head
    bool anchorOk = false;
    Pose tracked{};       // the same eye as the headset tracks it (tracking space)
    bool trackedOk = false;
    uint32_t source = 0;  // the view's source (eye mode, owner): a change is a switch
    float relTurn = 0;    // the player's own turn (radians, about up) applied on the view this frame, not via the anchor
    bool playerCut = false; // the player's own cut (a recenter, a snap or realign): not measured
    float dt = 0;
};
struct Step {
    bool measured = false;
    bool cut = false;       // a switch frame: expected to go on as it was going
    bool anchorJump = false;
    Vec3 pos{};             // the step no head made, world, metres
    Vec3 rot{};             // the same, a rotation vector (radians), world
    float metres = 0, deg = 0;
    bool seed = false;      // the first frame after a switch: the old source's velocity minus the new one's
    Vec3 seedVelocity{};    // m/s
};

class Measure {
  public:
    Step step(const Frame& f) {
        Step s;
        const float dt = std::isfinite(f.dt) ? std::clamp(f.dt, 0.f, .1f) : 0.f;
        if (!f.ok || !f.trackedOk || !finitePose(f.shown) || !finitePose(f.tracked)) {
            have_ = false;
            return s;
        }
        if (!have_ || f.playerCut) {
            prev_ = f;
            have_ = true;
            lastAnchorStep_ = {};
            lastDt_ = dt;
            seedPending_ = false;
            return s;
        }
        // The headset's own change, in the world as last shown.
        const Quat map = unit(prev_.shown.orientation * prev_.tracked.orientation.conjugate());
        const Vec3 headset = map.rotate(f.tracked.position - prev_.tracked.position);
        bool cut = f.source != prev_.source || !f.anchorOk || !prev_.anchorOk || !finitePose(f.anchor) ||
                   !finitePose(prev_.anchor);
        Pose expected{};
        Vec3 anchorStep{};
        if (!cut) {
            const Pose rel = compose(inverse(prev_.anchor), prev_.shown);
            const Pose dT = compose(inverse(prev_.tracked), f.tracked);
            expected = compose(f.anchor, compose(rel, dT));
            anchorStep = expected.position - prev_.shown.position - headset;
            const float scale = lastDt_ > 1e-4f ? std::clamp(dt / lastDt_, 0.f, 2.f) : 1.f;
            if (length(anchorStep - lastAnchorStep_ * scale) > anchorCutM) {
                cut = true;
                s.anchorJump = true;
            }
        }
        if (cut) {
            // Go on as it was going: the old anchor's motion and the headset's.
            const float scale = lastDt_ > 1e-4f ? std::clamp(dt / lastDt_, 0.f, 2.f) : 1.f;
            expected.position = prev_.shown.position + lastAnchorStep_ * scale + headset;
            expected.orientation = map * f.tracked.orientation;
            if (!seedPending_ && lastDt_ > 1e-4f) {
                carryFrom_ = lastAnchorStep_ / lastDt_;
                seedPending_ = true;
            }
            anchorStep = lastAnchorStep_ * scale;
        } else if (seedPending_) {
            seedPending_ = false;
            if (dt > 1e-4f) {
                s.seed = true;
                s.seedVelocity = carryFrom_ - anchorStep / dt;
            }
        }
        s.measured = true;
        s.cut = cut;
        s.pos = f.shown.position - expected.position;
        Quat r = unit(f.shown.orientation * unit(expected.orientation).conjugate());
        if (f.relTurn != 0.f && std::isfinite(f.relTurn))
            r = unit(Quat::yaw(-f.relTurn) * r);
        s.rot = toRotVec(r);
        if (!finite(s.pos) || !finite(s.rot))
            s = Step{};
        s.metres = length(s.pos);
        s.deg = length(s.rot) / degrees;
        lastAnchorStep_ = finite(anchorStep) ? anchorStep : Vec3{};
        lastDt_ = dt;
        prev_ = f;
        return s;
    }
    void reset() {
        have_ = false;
        seedPending_ = false;
    }

  private:
    Frame prev_{};
    bool have_ = false, seedPending_ = false;
    Vec3 lastAnchorStep_{}, carryFrom_{};
    float lastDt_ = 0;
};

struct Params {
    float settleSeconds = .3f; // smooth_view_handoff_ms / 1000: the spring settles (95%) in this
    float maxSpeed = 1.5f;     // smooth_view_handoff_speed_cm_s / 100: the offset never moves faster
    float capM = .02f;         // smooth_view_cap_mm / 1000: a frame's non-head motion, while a handoff is under way
    float capRad = 1.f * degrees; // smooth_view_cap_cdeg / 100 degrees
    float teleportM = 3.f, teleportRad = 90.f * degrees; // a real teleport: let through
    float maxOffsetM = 2.f, maxOffsetRad = 60.f * degrees;
    float windowSeconds = .6f; // a switch's handoff window (the cap applies to the whole step in it)
};
inline Params paramsOf(int ms, int speedCmS, int capMm, int capCdeg) {
    Params p;
    p.settleSeconds = static_cast<float>(std::clamp(ms, 100, 1000)) * .001f;
    p.maxSpeed = static_cast<float>(std::clamp(speedCmS, 20, 1000)) * .01f;
    p.capM = static_cast<float>(std::clamp(capMm, 5, 200)) * .001f;
    p.capRad = static_cast<float>(std::clamp(capCdeg, 25, 1000)) * .01f * degrees;
    return p;
}

class Smoother {
  public:
    struct Out {
        Vec3 offset{};    // add to the shown view's position (world)
        Vec3 turn{};      // rotate the shown view about its eye centre by this (rotation vector, world)
        float shownM = 0, shownDeg = 0; // the non-head motion the eyes showed this frame, after the cap
        bool capped = false, teleport = false;
    };
    // `window`: a real handoff is under way (the cap applies to the whole of the step). Outside it the step is
    // left as it is and any leftover offset is dropped (the head stays on the body).
    Out update(float dtIn, const Step& s, bool window, const Params& p) {
        Out out;
        const float dt = std::isfinite(dtIn) ? std::clamp(dtIn, 0.f, .1f) : 0.f;
        const Vec3 r = s.measured ? s.pos : Vec3{};
        const Vec3 rr = s.measured ? s.rot : Vec3{};
        if (s.measured && (length(r) > p.teleportM || length(rr) > p.teleportRad)) {
            reset();
            out.teleport = true;
            out.shownM = length(r);
            out.shownDeg = length(rr) / degrees;
            return out;
        }
        if (!window) {
            reset();
            out.shownM = length(r);
            out.shownDeg = length(rr) / degrees;
            return out;
        }
        if (s.seed && window && finite(s.seedVelocity))
            v_ = limited(s.seedVelocity, p.maxSpeed);
        const Vec3 o0 = o_, ro0 = ro_;
        if (dt > 0) {
            const float w = 4.744f / std::max(.05f, p.settleSeconds); // (1 + wt) e^-wt = 5% at wt = 4.744
            v_ += (o_ * (-w * w) - v_ * (2.f * w)) * dt;
            v_ = limited(v_, p.maxSpeed);
            o_ += v_ * dt;
            rv_ += (ro_ * (-w * w) - rv_ * (2.f * w)) * dt;
            ro_ += rv_ * dt;
        }
        // The offset's own motion never above the cap; while a handoff is under way, the whole step neither.
        Vec3 dO = limited(o_ - o0, p.capM), dR = limited(ro_ - ro0, p.capRad);
        if (window) {
            const Vec3 step = r + dO, rstep = rr + dR;
            if (length(step) > p.capM) {
                dO = limited(step, p.capM) - r;
                out.capped = true;
            }
            if (length(rstep) > p.capRad) {
                dR = limited(rstep, p.capRad) - rr;
                out.capped = true;
            }
        }
        o_ = limited(o0 + dO, p.maxOffsetM);
        ro_ = limited(ro0 + dR, p.maxOffsetRad);
        if (length(o_) < 1e-5f && length(v_) < 1e-4f)
            o_ = v_ = {};
        if (length(ro_) < 1e-6f && length(rv_) < 1e-5f)
            ro_ = rv_ = {};
        out.offset = o_;
        out.turn = ro_;
        out.shownM = length(r + (o_ - o0));
        out.shownDeg = length(rr + (ro_ - ro0)) / degrees;
        return out;
    }
    bool active() const {
        return length(o_) > 0 || length(ro_) > 0 || length(v_) > 0 || length(rv_) > 0;
    }
    Vec3 offset() const {
        return o_;
    }
    Vec3 turn() const {
        return ro_;
    }
    void reset() {
        o_ = v_ = ro_ = rv_ = {};
    }

  private:
    Vec3 o_{}, v_{}, ro_{}, rv_{};
};

// The shown eyes moved by the offset: `turn` about their centre, then `offset`.
inline Pose applied(const Pose& eye, Vec3 centre, Vec3 offset, Vec3 turn) {
    const Quat q = fromRotVec(turn);
    return {centre + offset + q.rotate(eye.position - centre), unit(q * eye.orientation)};
}
// The same on a native world matrix (rows the axes, then the position), its axes' lengths kept.
inline Mat4 appliedNative(const Mat4& m, Vec3 centre, Vec3 offset, Vec3 turn) {
    const Quat q = fromRotVec(turn);
    Mat4 out = m;
    for (int row = 0; row < 3; ++row) {
        const Vec3 a = q.rotate(Vec3{m[row * 4], m[row * 4 + 1], m[row * 4 + 2]});
        out[row * 4] = a.x, out[row * 4 + 1] = a.y, out[row * 4 + 2] = a.z;
    }
    const Vec3 p = centre + offset + q.rotate(Vec3{m[12], m[13], m[14]} - centre);
    out[12] = p.x, out[13] = p.y, out[14] = p.z;
    return out;
}
// A pose in the world back into a head's own axes (native rows right, down, forward, position): the inverse of
// native_eyes::onHead for a head whose axes are perpendicular (each divided by its own length).
inline Mat4 offHead(const Mat4& world, const Mat4& head) {
    const Vec3 h[3] = {{head[0], head[1], head[2]}, {head[4], head[5], head[6]}, {head[8], head[9], head[10]}};
    float n2[3];
    for (int k = 0; k < 3; ++k) {
        n2[k] = dot(h[k], h[k]);
        if (!(n2[k] > 1e-12f))
            n2[k] = 1;
    }
    Mat4 rel{};
    for (int row = 0; row < 3; ++row) {
        const Vec3 w{world[row * 4], world[row * 4 + 1], world[row * 4 + 2]};
        for (int k = 0; k < 3; ++k)
            rel[row * 4 + k] = dot(w, h[k]) / n2[k];
    }
    const Vec3 p = Vec3{world[12], world[13], world[14]} - Vec3{head[12], head[13], head[14]};
    for (int k = 0; k < 3; ++k)
        rel[12 + k] = dot(p, h[k]) / n2[k];
    rel[15] = 1;
    return rel;
}

// VIEW JUMP LOG's account: lines kept to `perSecond` a second, and a once-a-second count.
struct JumpLog {
    uint32_t perSecond = 8;
    float clock = 0;
    uint32_t lines = 0, jumps = 0, frames = 0, capped = 0;
    float worstM = 0, worstDeg = 0;
    uint32_t worstCause = 0;
    uint32_t total = 0;
    // A frame's step: true when its own line is due (a jump, under the line budget).
    bool add(float metres, float deg, uint32_t cause, bool wasCapped) {
        ++frames;
        capped += wasCapped ? 1u : 0u;
        if (!(metres > logM || deg > logDeg))
            return false;
        ++jumps;
        ++total;
        if (metres > worstM || deg > worstDeg)
            worstCause = cause;
        worstM = std::max(worstM, metres);
        worstDeg = std::max(worstDeg, deg);
        if (lines >= perSecond)
            return false;
        ++lines;
        return true;
    }
    // The once-a-second count is due (only after a jump).
    bool tick(float dt) {
        clock += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (clock < 1.f)
            return false;
        clock = 0;
        lines = 0;
        if (jumps > 0)
            return true; // the caller logs the count, then clear()s
        clear();
        return false;
    }
    void clear() {
        jumps = frames = capped = 0;
        worstM = worstDeg = 0;
        worstCause = 0;
    }
};

} // namespace spidy::view_step
