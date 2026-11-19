// SHOWTIME (showtime.hpp): the chord, the fade, the machine, the frame and the stick.
#include "spidy/showtime.hpp"
#include "spidy/showtime_body.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spidy;
using namespace spidy::showtime;
namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
constexpr float dt = 1.f / 90.f;
struct Run {
    Chord chord;
    Chord::Output last;
    bool x{}, y{}, b{}, allowed = true;
    float t{};
    unsigned entered{}, ended{};
    bool sawB{}, sawX{}; // what reached the game
    void frames(float seconds) {
        for (float s = 0; s < seconds - 1e-6f; s += dt) {
            last = chord.update({dt, x, y, b, allowed});
            t += dt;
            entered += last.entered;
            ended += last.ended;
            sawB |= b && !last.swallowB;
            sawX |= x && !last.swallowX;
        }
    }
};
void xLead() {
    Run r;
    r.x = true; r.frames(.1f);
    r.b = true; r.frames(dt);
    check(r.last.pending && r.last.lead == Lead::x, "X then B: X first");
    check(r.last.swallowB, "X first: B never reaches the game (no dodge)");
    check(!r.sawB, "B was never the game's");
    r.frames(.15f);
    check(r.last.pending && !r.last.active, "not before 200 ms");
    r.frames(.06f);
    check(r.last.active && r.entered == 1, "entered after 200 ms of both");
    r.frames(1.f);
    check(r.entered == 1 && r.last.swallowB && r.last.swallowX, "both stay the chord's while held");
    r.b = false; r.frames(dt);
    check(r.ended == 1 && !r.last.active, "let go of B: ended");
    check(!r.sawB, "and B never reached the game");
}
void bLead() {
    Run r;
    r.b = true; r.frames(.1f);
    check(r.sawB, "B first: the dodge's B passes at once");
    r.x = true; r.frames(dt);
    check(r.last.pending && r.last.lead == Lead::b && !r.last.swallowB && r.last.swallowX, "B first: B passes, X is the chord's");
    r.frames(.25f);
    check(r.last.active && r.entered == 1, "B first enters too");
    check(r.last.swallowB, "entered: B is the chord's now");
    // X let go first with B still down: the game must not see B as a new press.
    r.x = false; r.frames(dt);
    check(r.last.swallowB, "B held back after X is let go");
    r.b = false; r.frames(dt);
    check(!r.last.swallowB, "and free again once B is up");
}
void tapDoesNothing() {
    Run r;
    r.x = r.b = true; r.frames(.12f);
    r.x = r.b = false; r.frames(dt);
    check(r.entered == 0 && r.ended == 0, "a short X+B tap enters nothing");
    check(!r.last.swallowX && !r.last.swallowB, "and holds nothing back after");
}
void yBlocks() {
    Run r;
    r.y = true; r.x = true; r.frames(.05f);
    r.b = true; r.frames(.5f);
    check(r.entered == 0 && !r.last.pending, "Y held: not Showtime");
    check(!r.last.swallowB && !r.last.swallowX, "and nothing of the flail is taken");
    r.y = false; r.frames(.5f);
    check(r.entered == 0, "Y let go with the pair still down: still no chord until both are let go");
    r.x = r.b = false; r.frames(dt);
    r.x = r.b = true; r.frames(.3f);
    check(r.entered == 1, "after both are let go it works");
    Run q;
    q.x = q.b = true; q.frames(.1f);
    q.y = true; q.frames(dt);
    check(!q.last.pending && !q.last.active, "Y pressed during the wait ends it");
}
void aThenB() {
    Run r; // A+B point jump: no X at all
    r.b = true; r.frames(.5f);
    check(r.entered == 0 && !r.last.pending, "B alone, or A+B, is no chord");
}
void notAllowed() {
    Run r;
    r.allowed = false;
    r.x = r.b = true; r.frames(1.f);
    check(r.entered == 0 && !r.last.swallowX && !r.last.swallowB, "Showtime off: X and B pass as they are");
    r.allowed = true; r.frames(.01f);
    Run a;
    a.x = a.b = true; a.frames(.1f);
    a.allowed = false; a.frames(dt);
    check(!a.last.pending, "no longer allowed: the wait ends");
}
void fade() {
    Machine m;
    Machine::Input in;
    in.seconds = dt;
    in.fadeSeconds = .15f;
    in.allowed = true;
    in.wantOut = true;
    float maxFade{};
    int entered{}, framesToCut{};
    bool cut{};
    for (int i = 0; i < 100 && !cut; ++i) {
        const auto o = m.update(in);
        maxFade = std::max(maxFade, o.fade);
        entered += o.entered;
        cut = o.entered;
        framesToCut = i + 1;
        check(cut || !o.out, "still in the head until black");
    }
    check(entered == 1 && maxFade >= .999f, "black at the cut");
    check(std::abs(framesToCut * dt - .075f) < .02f, "the cut is half the fade time in");
    float f = 1;
    for (int i = 0; i < 30; ++i) f = std::min(f, m.update(in).fade);
    check(f == 0.f && m.out(), "cleared, and out");
    in.wantOut = false;
    int left{};
    for (int i = 0; i < 100; ++i) {
        const auto o = m.update(in);
        left += o.left;
        if (o.left) { check(o.exitWhy == ExitWhy::release && o.fade >= .999f && !o.out, "the exit's cut is under black"); }
    }
    check(left == 1 && !m.out() && m.state() == State::head, "left once");
}
void instant() {
    Machine m;
    Machine::Input in;
    in.seconds = dt;
    in.fadeSeconds = 0;
    in.allowed = true;
    in.wantOut = true;
    const auto o = m.update(in);
    check(o.entered && o.out && o.fade == 0.f, "no fade: a plain cut");
    in.wantOut = false;
    const auto q = m.update(in);
    check(q.left && !q.out, "and out again");
}
void forcedExit() {
    Machine m;
    Machine::Input in;
    in.seconds = dt;
    in.fadeSeconds = .15f;
    in.allowed = true;
    in.wantOut = true;
    for (int i = 0; i < 60; ++i) m.update(in);
    check(m.out(), "out");
    in.forced = ExitWhy::teleport;
    ExitWhy why{};
    for (int i = 0; i < 60; ++i) { const auto o = m.update(in); if (o.left) why = o.exitWhy; }
    check(why == ExitWhy::teleport && !m.out(), "a teleport leaves, and says why");
    in.forced = ExitWhy::none;
    in.wantOut = true;
    for (int i = 0; i < 3; ++i) m.update(in);
    in.allowed = false;
    for (int i = 0; i < 80; ++i) m.update(in);
    check(!m.out(), "Showtime turned off: back in the head");
}
void abortWhileFading() {
    Machine m;
    Machine::Input in;
    in.seconds = dt;
    in.fadeSeconds = .3f;
    in.allowed = true;
    in.wantOut = true;
    for (int i = 0; i < 5; ++i) m.update(in);
    check(m.state() == State::toOut, "fading out");
    in.wantOut = false;
    unsigned entered{};
    for (int i = 0; i < 80; ++i) entered += m.update(in).entered;
    check(entered == 0 && m.state() == State::head, "let go before the cut: it never left, and the picture clears");
}
void frameAndStick() {
    const Mat4 f = levelFrame({1, 2, 3}, 0);
    check(f[8] == 0.f && std::abs(f[10] - -1.f) < 1e-6f && f[12] == 1.f && f[13] == 2.f && f[14] == 3.f, "yaw 0 faces -z");
    const float yaw = .7f;
    const Mat4 g = levelFrame({}, yaw);
    check(std::abs(yawOfForward(g[8], g[10]) - yaw) < 1e-5f, "the frame's yaw reads back");
    float x = 0, y = 1;
    rotateStick(x, y, 3.14159265f / 2);
    check(std::abs(x - -1.f) < 1e-5f && std::abs(y) < 1e-5f, "forward turned a quarter is left (the rig's yaw is counter-clockwise)");
    x = .3f; y = -.4f;
    rotateStick(x, y, .5f);
    check(std::abs(std::hypot(x, y) - .5f) < 1e-5f, "a turn keeps the stick's length");
}
void bodyModes() {
    BodyIn in;
    check(bodyFor(in).body == Body::firstPerson && !bodyFor(in).showHead, "in first person nothing changes");
    in.out = true;
    for (Owner o : {Owner::anim, Owner::vr, Owner::frozen}) {
        in.owner = o;
        in.body = bodyGamePose;
        const BodyOut g = bodyFor(in);
        check(g.showHead && !g.naturalArms && !g.pauseArmOverlay, "GAME POSE: the head only, as before");
        in.body = bodyAsInPlay;
        const BodyOut p = bodyFor(in);
        check(!p.showHead && !p.naturalArms && !p.pauseArmOverlay && p.body == Body::asInPlay, "AS IN PLAY: untouched");
    }
    in.body = bodyNatural;
    in.owner = Owner::vr;
    BodyOut n = bodyFor(in);
    check(n.showHead && n.naturalArms && !n.pauseArmOverlay && n.body == Body::natural, "NATURAL ARMS, VR: his own arm length");
    in.owner = Owner::anim;
    n = bodyFor(in);
    check(n.showHead && !n.naturalArms && n.pauseArmOverlay && n.body == Body::game, "NATURAL ARMS, animation: the game's arms");
    in.owner = Owner::frozen;
    n = bodyFor(in);
    check(n.showHead && !n.naturalArms && !n.pauseArmOverlay && n.body == Body::game, "a scene: the game's, camera only");
    in.body = bodyGameSwing;
    in.owner = Owner::vr;
    in.gamePoseAvailable = true;
    n = bodyFor(in);
    check(n.yieldPose && n.pauseArmOverlay && n.showHead && n.body == Body::game, "GAME SWING POSE: the game's swing pose");
    in.gamePoseAvailable = false;
    n = bodyFor(in);
    check(!n.yieldPose && n.naturalArms && n.body == Body::natural, "no game pose: natural arms");
    in.body = bodyNatural;
    in.gamePoseAvailable = true;
    check(!bodyFor(in).yieldPose, "only GAME SWING POSE ever yields");
    in.out = false;
    in.body = bodyGameSwing;
    check(!bodyFor(in).yieldPose, "never in first person");
}
} // namespace
int main() {
    xLead();
    bLead();
    tapDoesNothing();
    yBlocks();
    aThenB();
    notAllowed();
    fade();
    instant();
    forcedExit();
    abortWhileFading();
    frameAndStick();
    bodyModes();
    std::puts("showtime tests passed");
}
