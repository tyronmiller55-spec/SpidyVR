// WEB FIRE THROUGH A HANDOFF (web_fire.hpp): the squeeze the air cancel consumed fires a web on the same press.
#include "spidy/animation_cancel.hpp"
#include "spidy/web_fire.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spidy;
namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
// XR's per-frame order: the gate, the press accounting, the queue, the gate's blocks let through, then suppress + force.
struct Rig {
    animation_cancel::Gate gate;
    web_fire::Queue queue;
    unsigned prev{};
    // What the swing is given as the web buttons this frame (the grip it sees), and whether it was cancelled.
    unsigned seen{};
    bool cancelled{};
    unsigned fired{};
    void frame(unsigned physical, bool animOwns = true, float dt = 1.f / 90.f) {
        auto d = gate.update(true, animOwns, true, physical, 0, 3);
        const unsigned pressed = physical & ~prev & 3;
        prev = physical;
        const unsigned blocked = d.webBlocked;
        queue.queue(pressed & blocked);
        const auto out = queue.update(dt, physical, true);
        d.webBlocked &= ~out.pass;
        unsigned grip = physical;
        for (unsigned i = 0; i < 2; ++i)
            if (d.webBlocked & (1u << i)) grip &= ~(1u << i);
        grip |= out.force;
        seen = grip;
        cancelled = d.cancel;
        fired = (out.fired | (pressed & ~blocked)) & 3;
    }
};
// The swing's own rule (swing.cpp): a web shoots on the rising edge it sees (grip > .65 from below .35), and goes with the grip.
struct Swing {
    bool held{}, attached{};
    unsigned shots{};
    void frame(bool grip) {
        if (grip && !held) { attached = true; ++shots; }
        if (!grip && held) attached = false;
        held = grip;
    }
};
void cancelledPressFires() {
    Rig r; Swing s;
    for (int i = 0; i < 3; ++i) { r.frame(0); s.frame(r.seen & 1); }
    // Squeeze and hold: frame 0 is the cancel (the swing sees nothing), the next frame the web shoots, and it stays.
    r.frame(1); check(r.cancelled, "the first squeeze in the air cancels"); check(!(r.seen & 1), "the cancel frame gives the swing no grip");
    s.frame(r.seen & 1); check(s.shots == 0, "the cancel is not a web on its own frame");
    r.frame(1); s.frame(r.seen & 1);
    check(s.shots == 1 && s.attached, "the same squeeze fires a web one frame later");
    for (int i = 0; i < 40; ++i) { r.frame(1); s.frame(r.seen & 1); }
    check(s.shots == 1 && s.attached, "it is held while the hand squeezes, and shoots once");
    r.frame(0); s.frame(r.seen & 1);
    check(!s.attached, "it is let go with the squeeze");
    // And the next press (the gate has rearmed on release) is a plain press in the air: a cancel again, queued again.
    r.frame(0); s.frame(r.seen & 1);
    r.frame(1); s.frame(r.seen & 1); r.frame(1); s.frame(r.seen & 1);
    check(s.shots == 2, "the next squeeze fires too");
}
void tapIsHeldThroughTheHandoff() {
    Rig r; Swing s;
    for (int i = 0; i < 3; ++i) { r.frame(0); s.frame(r.seen & 1); }
    // A tap shorter than the handoff: down for one frame, up the next.
    r.frame(1); s.frame(r.seen & 1);
    r.frame(0); s.frame(r.seen & 1);
    check(s.shots == 1 && s.attached, "a quick tap still shoots a web");
    int frames = 0;
    while (s.attached && frames < 100) { r.frame(0); s.frame(r.seen & 1); ++frames; }
    check(frames >= static_cast<int>(web_fire::minHold * 90.f) - 3, "it is held for the tap's minimum");
    check(frames <= static_cast<int>(web_fire::minHold * 90.f) + 3, "and then let go");
}
void otherHandIsNotDropped() {
    Rig r; Swing l, rr;
    for (int i = 0; i < 3; ++i) r.frame(0);
    // The left squeezes (the cancel), the right squeezes two frames later, while the gate waits for both to be let go.
    r.frame(1); l.frame(r.seen & 1); rr.frame(r.seen & 2);
    r.frame(1); l.frame(r.seen & 1); rr.frame(r.seen & 2);
    r.frame(3); l.frame(r.seen & 1); rr.frame(r.seen & 2);
    check(l.shots == 1 && !rr.shots, "right is waiting its frame");
    r.frame(3); l.frame(r.seen & 1); rr.frame(r.seen & 2);
    check(l.shots == 1 && rr.shots == 1, "the other hand's press during the cancel's wait fires too");
}
void ownerFlipsBackAtOnce() {
    // The 0 ms case of the last session: the cancel flipped to VR and the body landed back in ANIM the next frame. The press
    // goes through either way (the queue does not ask who owns the body).
    Rig r; Swing s;
    for (int i = 0; i < 3; ++i) r.frame(0);
    r.frame(1, true); s.frame(r.seen & 1);
    r.frame(1, false); s.frame(r.seen & 1); // grounded again, the owner is ANIM with no air to cancel
    check(s.shots == 1, "the web fires whichever owner the next frame has");
}
void expires() {
    web_fire::Queue q;
    q.queue(1);
    unsigned expired{};
    for (int i = 0; i < 100 && !expired; ++i) expired = q.update(.1f, 1, false).expired; // control lost
    check(expired == 1 && !q.busy(), "a press with no control is dropped, and said so");
    web_fire::Queue t;
    t.queue(2);
    t.update(.4f, 2, true); // a hitch longer than the wait counts as one capped step, never as a drop
    const auto o = t.update(.4f, 2, true);
    check(o.expired == 0 && (o.fired & 2) && (o.pass & 2), "a press after a hitch still fires on the second frame");
}
void tracker() {
    web_fire::Tracker t;
    web_fire::Press p;
    t.press(0, web_fire::Owner::anim, true, -1, -1, 10, 5, 0, "queued");
    check(!t.update(0, .011f, 10, 5, 0, false, 0), "a queued press is waiting, not missing");
    t.fired(0, .022f, 1.5f, 9.f, "this frame's mapped hand");
    check(!t.update(0, .011f, 10, 5, 0, false, 0), "a fired press waits for the swing");
    check(t.update(0, .011f, 11, 5, 0, true, 14.f), "an attach answers it");
    check(t.finished(0, p) && p.outcome == web_fire::Outcome::attached && p.distance == 14.f && p.queued, "attached, with distance");
    check(std::abs(p.aimError - 1.5f) < 1e-4f && std::abs(p.rigError - 9.f) < 1e-4f, "the aim error is kept");
    // A web that was already on is not this press's.
    t.press(1, web_fire::Owner::vr, false, -1, -1, 11, 5, 0);
    t.fired(1, 0, 0, 0, "x");
    check(!t.update(1, .011f, 11, 5, 0, true, 3.f), "a web already on does not answer a press");
    check(t.update(1, .011f, 11, 6, 0, false, 0) && t.finished(1, p) && p.outcome == web_fire::Outcome::missed, "a miss answers it");
    t.press(0, web_fire::Owner::anim, true, -1, -1, 0, 0, 0);
    t.dropped(0, "lost control");
    check(t.finished(0, p) && p.outcome == web_fire::Outcome::dropped && !t.open(0), "a dropped press is accounted");
    t.press(0, web_fire::Owner::anim, false, -1, -1, 0, 0, 0);
    t.fired(0, 0, 0, 0, "x");
    bool done{};
    for (int i = 0; i < 100 && !done; ++i) done = t.update(0, .02f, 0, 0, 0, false, 0);
    check(done && t.finished(0, p) && p.outcome == web_fire::Outcome::unanswered, "a press nothing answered says so");
}
void lag() {
    const Vec3 raw{0, 0, -1};
    const float a = 10.f * 3.14159265f / 180.f;
    const Vec3 lagged{std::sin(a), 0, -std::cos(a)};
    const Vec3 out = web_fire::limitLag(raw, lagged);
    check(std::abs(web_fire::angleDeg(raw, out) - web_fire::maxFilterLagDeg) < .05f, "the steadied line is held within the limit of the raw");
    check(web_fire::angleDeg(out, lagged) > 7.f, "and it is the raw's side");
    const float b = 1.f * 3.14159265f / 180.f;
    const Vec3 near{std::sin(b), 0, -std::cos(b)};
    check(web_fire::angleDeg(web_fire::limitLag(raw, near), near) < .01f, "a line within the limit is kept as it is");
    check(web_fire::angleDeg(raw, raw) < .01f && web_fire::angleDeg({}, raw) == -1.f, "angles");
}
} // namespace
int main() {
    cancelledPressFires();
    tapIsHeldThroughTheHandoff();
    otherHandIsNotDropped();
    ownerFlipsBackAtOnce();
    expires();
    tracker();
    lag();
    std::puts("web_fire tests passed");
}
