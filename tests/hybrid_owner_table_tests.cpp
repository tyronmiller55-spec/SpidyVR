// HYBRID OWNER TRANSITION TABLE (beta polish H3): every way into SpidyVR, and what a touchdown does to each of them
// at once, after the least stint and on a hard landing; then the ANIM-side rows (zips, perches, the crawl, the run
// release, the landing blend-in, combat, scenes). Each row drives a fresh HybridOwner through the frames that make
// the takeover, so a change to one rule shows up here as a named row. Portable.
#include "spidy/hybrid_owner.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace spidy;
using namespace spidy::hybrid_owner;
namespace {
int passed = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        std::exit(1);
    }
    ++passed;
}
constexpr int64_t ms = 1'000'000;

Frame air(float velY = -1.f) {
    Frame f;
    f.state.playing = f.state.fresh = true;
    f.velY = velY;
    return f;
}
Frame ground(float velY = -2.f) {
    Frame f = air(velY);
    f.state.grounded = true;
    return f;
}

// Drives `o` from the animation into SpidyVR by `why`; returns the time of the switch.
int64_t enter(HybridOwner& o, EnterVr why, int64_t t) {
    switch (why) {
    case EnterVr::cancel: {
        o.update(air(), t);
        Frame f = air();
        f.cancelGrip = true;
        o.update(f, t += 10 * ms);
        return t;
    }
    case EnterVr::web: {
        o.update(air(), t);
        Frame f = air();
        f.webRising = true;
        o.update(f, t += 10 * ms);
        return t;
    }
    case EnterVr::swing: { // a zip the game never took: the swing still carries him
        o.update(air(), t);
        Frame z = air();
        z.zipPress = true;
        o.update(z, t += 10 * ms);
        Frame f = air();
        f.state.owned = true;
        o.update(f, t += zipAskNs + 10 * ms);
        return t;
    }
    case EnterVr::punch: {
        Frame f = air();
        f.strike = true;
        o.update(f, t);
        return t;
    }
    case EnterVr::pointJump: {
        Frame f = ground(0);
        f.pointJump = true;
        o.update(f, t);
        Frame up = air(6.f);
        o.update(up, t += 20 * ms);
        return t - 20 * ms;
    }
    case EnterVr::sidestep: {
        o.update(ground(0), t);
        Frame f = ground(0);
        f.sidestep = true;
        o.update(f, t += 10 * ms);
        return t;
    }
    case EnterVr::wallRun: {
        Frame f = air();
        f.state.wall = surfaceWall;
        o.update(f, t);
        return t;
    }
    case EnterVr::timed: {
        Frame f = air(-1.f);
        f.jumpHandoffNs = 300 * ms;
        o.update(f, t);
        o.update(f, t += 300 * ms);
        return t;
    }
    case EnterVr::flip:
        o.update(air(), t);
        o.breakForFlip(air().state, false, t += 10 * ms);
        o.update(air(), t);
        return t;
    case EnterVr::move:
        o.update(air(), t);
        o.breakForMove(air().state, false, t += 10 * ms);
        o.update(air(), t);
        return t;
    case EnterVr::handstand: { // the hands catch him in the landing's first frames
        t = enter(o, EnterVr::web, t);
        o.update(ground(), t += landMinStintNs + ms);
        Frame f = ground(0);
        f.handSupport = true;
        o.update(f, t += 20 * ms);
        o.update(air(0), t += 10 * ms); // he rocks off the floor: the hands let go
        return t - 10 * ms;
    }
    default:
        return t;
    }
}

// Whether a touchdown this soon after the takeover waits for the least stint (HANDOFF CHURN).
// MINIMUM STINT (beta polish H1): every takeover but a handstand's or a swing's; a hard landing still hands back at
// once, except after a punch or a grip cancel.
bool heldAtTouchdown(EnterVr why, float velY) {
    if (why == EnterVr::handstand || why == EnterVr::swing || why == EnterVr::none)
        return false;
    return why == EnterVr::punch || why == EnterVr::cancel || !(velY < hybrid_owner::hardLandVel);
}

void takeovers() {
    const EnterVr all[] = {EnterVr::cancel,  EnterVr::web,  EnterVr::swing, EnterVr::punch,
                           EnterVr::pointJump, EnterVr::sidestep, EnterVr::wallRun, EnterVr::timed,
                           EnterVr::flip,    EnterVr::move, EnterVr::handstand};
    for (EnterVr why : all) {
        const std::string name = enterVrName(why);
        {
            HybridOwner o;
            enter(o, why, 1'000 * ms);
            check(o.domain == Domain::vr && o.vrBy == why, name + ": takes SpidyVR");
        }
        for (float velY : {-2.f, -8.f}) {
            HybridOwner o;
            const int64_t t = enter(o, why, 1'000 * ms);
            o.update(ground(velY), t + 30 * ms);
            const bool held = heldAtTouchdown(why, velY);
            check((o.domain == Domain::vr) == held,
                  name + (held ? ": a touchdown inside the least stint waits" : ": a touchdown hands back at once") +
                      (velY < -6.f ? " (hard landing)" : ""));
            o.update(ground(velY), t + landMinStintNs + ms);
            check(o.domain == Domain::anim, name + ": past the least stint the landing hands back");
            check(o.lastVrNs > 0, name + ": the stint's length is kept");
            check(o.landingsHeld == (held ? 1u : 0u) && (!held || o.landHeldWhy == 1),
                  name + ": a held landing is counted once, as the minimum stint's");
        }
        // RISING TOUCHDOWN (F2): ground while rising 10 m/s inside 600 ms is no landing; falling after, it is.
        if (why != EnterVr::handstand) {
            HybridOwner o;
            const int64_t t = enter(o, why, 1'000 * ms);
            o.update(ground(10.f), t + 200 * ms);
            check(o.domain == Domain::vr && o.landingsHeld == 1 && o.landHeldWhy == 2,
                  name + ": a rising touchdown keeps SpidyVR");
            o.update(ground(10.f), t + 400 * ms);
            check(o.landingsHeld == 1, name + ": counted once a stint");
            o.update(ground(-2.f), t + 450 * ms);
            check(o.domain == Domain::anim, name + ": then a real landing hands back");
        }
    }
}

void animRows() {
    // ANIM on the ground: grip, a web hold or a jump ask switches nothing.
    {
        HybridOwner o;
        Frame f = ground();
        f.webIntent = f.jumpAsk = true;
        o.update(f, 10 * ms);
        check(o.domain == Domain::anim, "ground: inputs keep the animation");
    }
    // A scene freezes either domain.
    {
        HybridOwner o;
        enter(o, EnterVr::web, 0);
        Frame f = ground();
        f.cutscene = true;
        o.update(f, 2'000 * ms);
        check(o.domain == Domain::vr, "scene: VR is frozen");
        HybridOwner a;
        Frame c = air();
        c.cutscene = true;
        c.cancelGrip = true;
        a.update(c, 10 * ms);
        check(a.domain == Domain::anim, "scene: ANIM is frozen, grip cancel included");
    }
    // A zip from SpidyVR's air (no swing holding him) is the animation's at once.
    {
        HybridOwner o;
        const int64_t t = enter(o, EnterVr::cancel, 0);
        Frame z = air();
        z.zipPress = true;
        o.update(z, t + 200 * ms);
        check(o.domain == Domain::anim && o.lastEnterAnim == EnterAnim::zip, "zip from VR air: ANIM on the press");
    }
    // From the swing, VR waits for the game to begin the zip; instant zip switches on the press.
    {
        HybridOwner o;
        int64_t t = enter(o, EnterVr::web, 0);
        Frame z = air();
        z.state.owned = z.state.webbed = true;
        z.zipPress = true;
        o.update(z, t += 200 * ms);
        check(o.domain == Domain::vr, "zip from the swing: VR waits for the game");
        Frame took = air();
        took.state.zipping = true;
        o.update(took, t += 20 * ms);
        check(o.domain == Domain::anim && o.lastEnterAnim == EnterAnim::zip, "zip from the swing: ANIM once the game takes it");
        HybridOwner i;
        int64_t u = enter(i, EnterVr::web, 0);
        z.zipInstant = true;
        i.update(z, u += 200 * ms);
        check(i.domain == Domain::anim, "instant zip from the swing: ANIM on the press");
    }
    // Perch and the game's crawl hand back like the ground (land / mount).
    {
        HybridOwner o;
        const int64_t t = enter(o, EnterVr::web, 0);
        Frame p = air(0);
        p.perched = true;
        o.update(p, t + landMinStintNs + ms);
        check(o.domain == Domain::anim && o.lastEnterAnim == EnterAnim::land, "perch: lands");
        HybridOwner c;
        const int64_t u = enter(c, EnterVr::web, 0);
        Frame g = air(0);
        g.state.wall = surfaceGame;
        c.update(g, u + landMinStintNs + ms);
        check(c.domain == Domain::anim && c.lastEnterAnim == EnterAnim::mount, "game crawl: mount");
    }
    // A wall run out of a Native Run hands back when the stick is let go on the wall.
    {
        HybridOwner o;
        Frame run = ground();
        run.state.runRt = true;
        o.update(run, 0);
        Frame w = air();
        w.state.wall = surfaceWall;
        w.moveStick = .8f;
        o.update(w, 50 * ms);
        check(o.domain == Domain::vr && o.runStint, "native run onto Spidy's wall: VR, a run stint");
        w.moveStick = .05f;
        o.update(w, 100 * ms);
        check(o.domain == Domain::anim && o.lastEnterAnim == EnterAnim::runRelease, "stick let go: run release");
        o.update(w, 150 * ms);
        check(o.domain == Domain::anim, "the same wall stays the animation's");
    }
    // LANDING BLEND-IN: due after the least dwell, handed back; no landing within landLostNs gives the air back.
    {
        HybridOwner o;
        int64_t t = enter(o, EnterVr::web, 0);
        Frame f = air();
        f.landHandBack = true;
        f.landEta = .3f;
        o.update(f, t + 100 * ms);
        check(o.domain == Domain::vr, "blend-in: not before the least dwell");
        o.update(f, t += minDwellNs);
        check(o.domain == Domain::anim && o.lastEnterAnim == EnterAnim::predicted && o.handedBack(), "blend-in: handed back");
        Frame lost = air();
        lost.landEta = NAN;
        o.update(lost, t += 10 * ms);
        check(o.domain == Domain::anim, "blend-in: a lost prediction waits");
        o.update(lost, t + landLostNs + minDwellNs);
        check(o.domain == Domain::vr && o.vrBy == EnterVr::timed && o.lastTimedWhy == TimedWhy::noLanding,
              "blend-in: no landing came, the air is SpidyVR's again");
    }
    // Combat holds the jump timer; a strike still takes the body.
    {
        HybridOwner o;
        Frame f = air(-1.f);
        f.jumpHandoffNs = 300 * ms;
        f.state.fighting = true;
        o.update(f, 0);
        o.update(f, 400 * ms);
        check(o.domain == Domain::anim, "fight: the jump timer waits");
        f.strike = true;
        o.update(f, 410 * ms);
        check(o.domain == Domain::vr && o.vrBy == EnterVr::punch, "fight: a strike takes the body");
    }
    // A ground web cancel is refused in a fight and in the parkour's first 150 ms.
    {
        HybridOwner o;
        Frame f = ground();
        f.state.fighting = true;
        f.groundWeb = true;
        o.update(f, 0);
        check(o.domain == Domain::anim, "ground web in a fight: refused");
        HybridOwner p;
        Frame pk = air();
        pk.state.runRt = true;
        pk.state.wall = surfaceFloor;
        pk.groundWeb = true;
        pk.perched = true;
        pk.parkourSeconds = .05f;
        p.update(pk, 0);
        check(p.domain == Domain::anim, "ground web in parkour's first 150 ms: refused");
    }
}
} // namespace

int main() {
    takeovers();
    animRows();
    std::printf("hybrid owner table: %d checks passed\n", passed);
    return 0;
}
