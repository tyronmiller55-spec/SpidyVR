// JUMP TO SPIDYVR (hybrid_owner.hpp, November 12 05:41) and AIR VIEW EASE
// (view_ease.hpp): plain jumps, falls, zip launches, vaults and wall launches
// hand to SpidyVR on a timer; STAY TO LANDING keeps the old rule; the same-
// frame breaks (flip, grip cancel, web, wall), point jump and landing are as
// before; the view ease each switch takes; the settings. Portable.
#include "spidy/hybrid_owner.hpp"
#include "spidy/view_ease.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::hybrid_owner;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr int64_t ms = 1'000'000;
constexpr int64_t frameNs = 11 * ms; // ~90 Hz

struct Sim {
    HybridOwner owner;
    Frame f;
    int64_t now = 1'000 * ms;
    int vrEdges = 0, animEdges = 0;
    EnterVr lastVr = EnterVr::none;
    EnterAnim lastAnim = EnterAnim::none;
    int64_t vrAt = -1;
    explicit Sim(int64_t timer) {
        f.state.playing = f.state.fresh = true;
        f.state.grounded = true;
        f.jumpHandoffNs = timer;
    }
    void run(int64_t durationNs) {
        for (int64_t t = 0; t < durationNs; t += frameNs) {
            now += frameNs;
            owner.update(f, now);
            if (owner.enterVr) {
                ++vrEdges;
                lastVr = owner.lastEnterVr;
                vrAt = now;
            }
            if (owner.enterAnim) {
                ++animEdges;
                lastAnim = owner.lastEnterAnim;
            }
            f.jumpAsk = false;
            f.zipPress = false;
            f.cancelGrip = false;
            f.webRising = false;
            f.pointJump = false;
        }
    }
    void ground(int64_t d = 200 * ms) {
        f.state.grounded = true;
        f.state.wall = 0;
        f.velY = 0;
        run(d);
    }
    // Leave the ground (after a jump press when `ask`) and go up then down.
    void takeoff(bool ask, float up = 5.f) {
        if (ask) {
            f.jumpAsk = true;
            run(frameNs);
        }
        f.state.grounded = false;
        f.state.wall = 0;
        f.velY = up;
    }
};
} // namespace

int main() {
    // STAY TO LANDING (0): the November 11 rule - the animation keeps the air to the landing.
    {
        Sim s(0);
        s.ground();
        s.takeoff(true);
        s.run(300 * ms);
        s.f.velY = -8;
        s.run(2'700 * ms);
        check(s.owner.domain == Domain::anim && s.vrEdges == 0, "STAY TO LANDING: anim for 3 s in the air");
        s.ground();
        check(s.vrEdges == 0 && s.animEdges == 0, "and it lands animated");
    }
    // 500 ms: a plain jump hands to VR 500 ms into the air, then lands back.
    {
        Sim s(500 * ms);
        s.ground();
        s.takeoff(true);
        const int64_t off = s.now;
        s.run(490 * ms);
        check(s.owner.domain == Domain::anim, "still the animation's at 490 ms");
        s.run(30 * ms);
        check(s.owner.domain == Domain::vr && s.lastVr == EnterVr::timed && s.vrEdges == 1, "VR at 500 ms (jump-timer)");
        check(s.owner.lastJumpKind == JumpKind::jump, "kind: jump");
        check(s.owner.lastJumpAirNs >= 500 * ms && s.owner.lastJumpAirNs < 520 * ms && s.vrAt - off >= 500 * ms,
              "after 500 ms in the air");
        check(std::strcmp(enterVrName(EnterVr::timed), "jump-timer") == 0, "named jump-timer");
        check(adoptsFromAir(EnterVr::timed, false), "seeds from the air pose (no pose pop)");
        s.f.velY = -6;
        s.run(400 * ms);
        check(s.owner.domain == Domain::vr, "the air stays VR's");
        s.ground();
        check(s.owner.domain == Domain::anim && s.lastAnim == EnterAnim::land, "landing on the feet: back to ANIM");
    }
    // A hop that lands inside the timer never switches; the next jump's timer starts afresh.
    {
        Sim s(500 * ms);
        s.ground();
        s.takeoff(true, 3);
        s.run(400 * ms);
        s.ground();
        check(s.vrEdges == 0, "a 400 ms hop stays animated");
        s.takeoff(true);
        s.run(300 * ms);
        check(s.owner.domain == Domain::anim, "a new jump: its own timer (300 ms in: anim)");
        s.run(220 * ms);
        check(s.owner.domain == Domain::vr, "then VR at its 500 ms");
    }
    // Each timer setting.
    for (int64_t t : {300, 400, 700, 1000}) {
        Sim s(t * ms);
        s.ground();
        s.takeoff(true);
        s.run((t - 20) * ms);
        const bool before = s.owner.domain == Domain::anim;
        s.run(40 * ms);
        check(before && s.owner.domain == Domain::vr, "each timer hands over at its time");
    }
    // Kinds: a fall (walked off), a fall that rises (a jump), a vault (NATIVE RUN), a wall launch.
    {
        Sim fall(500 * ms);
        fall.ground();
        fall.takeoff(false, -1);
        fall.run(520 * ms);
        check(fall.owner.lastJumpKind == JumpKind::fall, "walked off a ledge: fall");
        Sim rose(500 * ms);
        rose.ground();
        rose.takeoff(false, 4);
        rose.run(520 * ms);
        check(rose.owner.lastJumpKind == JumpKind::jump, "no press seen but it rose: jump");
        Sim vault(500 * ms);
        vault.f.state.runRt = true;
        vault.ground();
        vault.takeoff(false, 2);
        vault.run(520 * ms);
        check(vault.owner.lastJumpKind == JumpKind::vault && vault.lastVr == EnterVr::timed, "NATIVE RUN: vault");
        Sim wall(500 * ms);
        wall.f.state.grounded = false;
        wall.f.state.wall = surfaceGame; // the game's crawl: supported
        wall.run(300 * ms);
        check(wall.owner.domain == Domain::anim, "on the game's wall: anim");
        wall.f.state.wall = 0;
        wall.f.velY = 3;
        wall.run(520 * ms);
        check(wall.owner.lastJumpKind == JumpKind::wallLaunch && wall.owner.domain == Domain::vr, "wall launch");
        check(std::strcmp(jumpKindName(JumpKind::zipLaunch), "zip launch") == 0, "kind names");
    }
    // A zip: never handed over while it pulls; its launch's timer counts from the rise.
    {
        Sim s(500 * ms);
        s.ground();
        s.takeoff(false, 0);
        s.f.zipPress = true;
        s.f.state.zipping = true;
        s.run(frameNs);
        s.f.velY = 0;
        s.run(1'200 * ms);
        check(s.owner.domain == Domain::anim && s.vrEdges == 0, "a zip pulling 1.2 s: still the animation's");
        s.f.state.zipping = false;
        s.run(450 * ms); // past zipAskNs, not rising yet
        s.f.velY = 9; // the point launch rises
        const int64_t rise = s.now;
        s.run(480 * ms);
        check(s.owner.domain == Domain::anim, "the launch: anim before its 500 ms");
        s.run(40 * ms);
        check(s.owner.domain == Domain::vr && s.owner.lastJumpKind == JumpKind::zipLaunch && s.vrAt - rise >= 500 * ms,
              "zip launch: VR 500 ms after the rise");
        Sim stay(0);
        stay.ground();
        stay.takeoff(false, 0);
        stay.f.zipPress = true;
        stay.f.state.zipping = true;
        stay.run(300 * ms);
        stay.f.state.zipping = false;
        stay.run(450 * ms);
        stay.f.velY = 9;
        stay.run(800 * ms);
        stay.f.velY = -5;
        stay.run(1'000 * ms);
        check(stay.vrEdges == 0, "STAY TO LANDING: a zip launch plays to the landing");
    }
    // A short launch whose top comes before the timer: the timer still counts from the rise.
    {
        Sim s(700 * ms);
        s.ground();
        s.takeoff(false, 0);
        s.f.zipPress = true;
        s.f.state.zipping = true;
        s.run(200 * ms);
        s.f.state.zipping = false;
        s.run(450 * ms);
        s.f.velY = 5;
        const int64_t rise = s.now;
        s.run(250 * ms);
        s.f.velY = -3; // its top, then 150 ms falling ends the zip phase
        s.run(250 * ms);
        check(s.owner.domain == Domain::anim, "past the launch's top, before the timer: anim");
        s.run(250 * ms);
        check(s.owner.domain == Domain::vr && s.vrAt - rise >= 700 * ms && s.vrAt - rise < 760 * ms,
              "VR at 700 ms from the rise, not 700 ms from the top");
    }
    // A fight or dodge in the air holds the timer (aerial combat stays animated).
    {
        Sim s(500 * ms);
        s.ground();
        s.takeoff(true);
        s.f.state.fighting = true;
        s.run(800 * ms);
        check(s.owner.domain == Domain::anim, "fighting in the air: no handoff");
        s.f.state.fighting = false;
        s.run(200 * ms);
        check(s.owner.domain == Domain::anim, "the fight's 300 ms clear-out: still anim");
        s.run(200 * ms);
        check(s.owner.domain == Domain::vr && s.lastVr == EnterVr::timed, "then the timer (long since due)");
    }
    // The same-frame breaks come first: flip, grip cancel, web, Spidy's wall; point jump instant.
    {
        Sim flip(500 * ms);
        flip.ground();
        flip.takeoff(true);
        flip.run(100 * ms);
        check(flip.owner.breakForFlip(flip.f.state, false, flip.now) && flip.owner.domain == Domain::vr,
              "flip break at 100 ms, on that frame");
        flip.run(frameNs);
        check(flip.lastVr == EnterVr::flip, "reported as flip-break");
        Sim grip(500 * ms);
        grip.ground();
        grip.takeoff(true);
        grip.run(100 * ms);
        grip.f.cancelGrip = true;
        grip.run(frameNs);
        check(grip.lastVr == EnterVr::cancel, "grip cancel at once");
        Sim web(500 * ms);
        web.ground();
        web.takeoff(true);
        web.run(100 * ms);
        web.f.webRising = true;
        web.run(frameNs);
        check(web.lastVr == EnterVr::web, "a web at once");
        Sim wall(500 * ms);
        wall.ground();
        wall.takeoff(true);
        wall.run(100 * ms);
        wall.f.state.wall = surfaceWall;
        wall.run(frameNs);
        check(wall.lastVr == EnterVr::wallRun, "Spidy's wall at once");
        Sim point(500 * ms);
        point.ground();
        point.f.pointJump = true;
        point.run(frameNs);
        check(point.lastVr == EnterVr::pointJump && point.owner.domain == Domain::vr, "point jump: instant");
    }
    // Right-stick turning never breaks; its pitch does only with flips on the stick.
    {
        FlipInput in;
        in.rightValid = true;
        in.rightX = 1;
        in.basicFlips = true;
        check(flipBreak(in) == FlipBreak::none, "a right-stick turn never breaks");
        Sim s(500 * ms);
        s.ground();
        s.takeoff(true);
        s.f.rStickX = 1;
        s.run(300 * ms);
        check(s.owner.domain == Domain::anim, "turning in the air: the animation keeps it");
    }
    // The view ease each switch takes.
    {
        check(viewEaseMs(EnterVr::punch, 250, 300) == 250, "punch: PUNCH VIEW EASE");
        check(viewEaseMs(EnterVr::timed, 250, 300) == 300 && viewEaseMs(EnterVr::flip, 250, 300) == 300 &&
                  viewEaseMs(EnterVr::cancel, 250, 300) == 300,
              "timer, flip break, grip cancel: AIR VIEW EASE");
        check(viewEaseMs(EnterVr::timed, 250, 0) == 0, "AIR VIEW EASE OFF: the snap");
        check(viewEaseMs(EnterVr::web, 250, 300) == 0 && viewEaseMs(EnterVr::wallRun, 250, 300) == 0 &&
                  viewEaseMs(EnterVr::pointJump, 250, 300) == 0,
              "web, wall run, point jump: as before (snap)");
        // TAKEOVER VIEW EASE (the owner, Nov 15): every takeover gets AIR VIEW EASE; the punch keeps its own; off: as before.
        check(viewEaseMs(EnterVr::web, 250, 300, true) == 300 && viewEaseMs(EnterVr::wallRun, 250, 300, true) == 300 &&
                  viewEaseMs(EnterVr::pointJump, 250, 300, true) == 300 &&
                  viewEaseMs(EnterVr::handstand, 250, 300, true) == 300 &&
                  viewEaseMs(EnterVr::sidestep, 250, 300, true) == 300 &&
                  viewEaseMs(EnterVr::swing, 250, 300, true) == 300 && viewEaseMs(EnterVr::animEnd, 250, 300, true) == 300,
              "every takeover: AIR VIEW EASE");
        check(viewEaseMs(EnterVr::punch, 250, 300, true) == 250 && viewEaseMs(EnterVr::none, 250, 300, true) == 0 &&
                  viewEaseMs(EnterVr::web, 250, 0, true) == 0,
              "the punch keeps PUNCH VIEW EASE, no switch none, AIR VIEW EASE OFF the snap");
        check(vr_settings::Values{}.takeoverViewEase, "on by default");
        // A flip break's median 0.50 m jump (03:49 session): held, then eased.
        view_ease::Ease e;
        check(e.begin({0, 0, .5f}, .25f), "0.50 m: an ease");
        e.step(1.f / 90, {}, 0);
        e.cancel(view_ease::End::landed);
        check(!e.active() && e.lastEnd() == view_ease::End::landed &&
                  std::strcmp(view_ease::endName(view_ease::End::landed), "cancelled (landed)") == 0,
              "landing cancels it (named)");
    }
    // AIR TAKEOVER (Nov 14): the air is SpidyVR's 0.3 s after the feet leave the ground, sooner for a slow rise or a plain fall.
    {
        auto air = [&](int64_t timer, bool ask, float up, int64_t after, bool earlyRules) {
            Sim s(timer);
            if (earlyRules) {
                s.f.takeoffDoneVel = takeoffDoneVel;
                s.f.takeoffMinNs = takeoffMinNs;
                s.f.fallHandoffNs = 150 * ms;
            }
            s.ground();
            s.takeoff(ask, up);
            const int64_t off = s.now;
            s.run(after);
            return std::make_pair(s.owner.domain == Domain::vr, s.vrAt >= 0 ? (s.vrAt - off) / ms : int64_t{-1});
        };
        // A full jump (up 7 m/s, the velocity left constant for the test) is still the animation's inside the 300 ms and
        // the timer's after it.
        auto jump = air(300 * ms, true, 7.f, 280 * ms, true);
        check(!jump.first, "a full jump keeps the takeoff animation for the first 0.28 s");
        jump = air(300 * ms, true, 7.f, 340 * ms, true);
        check(jump.first && jump.second >= 300 && jump.second < 330, "a full jump hands over at 0.3 s");
        // A hop whose vertical speed is under 4 m/s: after the quarter second's dwell, not before, not at the timer.
        auto hop = air(300 * ms, true, 3.f, 240 * ms, true);
        check(!hop.first, "a slow hop still plays 0.24 s of takeoff (the minimum dwell)");
        hop = air(300 * ms, true, 3.f, 275 * ms, true);
        check(hop.first && hop.second >= 250 && hop.second < 275, "a slow rise hands over at 0.25 s, ahead of the timer");
        // Off (the early rules unset), a slow hop waits for the timer as before.
        hop = air(300 * ms, true, 3.f, 275 * ms, false);
        check(!hop.first, "without the early rules the timer rules");
        // A plain fall (no jump press, no rise): 0.15 s, no dwell.
        auto fall = air(300 * ms, false, -2.f, 140 * ms, true);
        check(!fall.first, "a plain fall plays 0.14 s of the game's fall");
        fall = air(300 * ms, false, -2.f, 175 * ms, true);
        check(fall.first && fall.second >= 150 && fall.second < 175, "a plain fall hands over at 0.15 s");
        // A fight, a dodge or a zip never hands over early; STAY TO LANDING (0) never does.
        {
            Sim s(300 * ms);
            s.f.takeoffDoneVel = takeoffDoneVel;
            s.f.takeoffMinNs = takeoffMinNs;
            s.f.fallHandoffNs = 150 * ms;
            s.ground();
            s.takeoff(false, -2.f);
            s.f.state.zipping = true;
            s.run(400 * ms);
            check(s.owner.domain == Domain::anim && s.lastVr == EnterVr::none, "a zip's air is not handed over early");
        }
        {
            Sim s(0);
            s.f.takeoffDoneVel = takeoffDoneVel;
            s.f.takeoffMinNs = takeoffMinNs;
            s.f.fallHandoffNs = 150 * ms;
            s.ground();
            s.takeoff(false, -2.f);
            s.run(1'000 * ms);
            check(s.owner.domain == Domain::anim, "STAY TO LANDING ignores the early rules");
        }
        // Landing is the animation's: VR's air returns to the animation on touchdown, as before.
        {
            Sim s(300 * ms);
            s.f.takeoffDoneVel = takeoffDoneVel;
            s.f.takeoffMinNs = takeoffMinNs;
            s.f.fallHandoffNs = 150 * ms;
            s.ground();
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            check(s.owner.domain == Domain::vr && s.owner.lastTimedWhy == TimedWhy::timer, "VR by the timer");
            s.f.velY = -9;
            s.run(300 * ms);
            s.ground();
            check(s.owner.domain == Domain::anim && s.lastAnim == EnterAnim::land, "and the landing is the animation's");
        }
        check(std::strcmp(timedWhyName(TimedWhy::slowRise), "vertical speed low") == 0 &&
                  std::strcmp(timedWhyName(TimedWhy::fall), "plain fall") == 0,
              "the rules are named for the log");
    }
    // LANDING BLEND-IN's release (air_landing_release, unverified, off by default): the body goes to the animation before the
    // touchdown, stays there to the landing, comes back to VR if no landing came; the touchdown handoff is unchanged without it.
    {
        auto flight = [&](bool release, float etaAt, int64_t& handAt, int64_t& off) {
            Sim s(300 * ms);
            s.ground();
            s.takeoff(true, 7.f);
            off = s.now;
            s.run(340 * ms); // VR by the timer
            s.run(300 * ms);
            s.f.velY = -9;
            s.f.state.grounded = false;
            for (int i = 0; i < 100; ++i) {
                s.f.landEta = etaAt - i * .02f;
                s.f.landHandBack = release && s.f.landEta <= .2f && s.f.landEta >= 0;
                s.run(20 * ms);
                if (s.animEdges && handAt < 0)
                    handAt = s.now;
                if (s.f.landEta < 0)
                    break;
            }
            return s.animEdges ? s.lastAnim : EnterAnim::none;
        };
        int64_t handAt = -1, off = 0;
        check(flight(false, 1.f, handAt, off) == EnterAnim::none && handAt < 0, "without the release the air stays VR's to the touchdown");
        handAt = -1;
        check(flight(true, 1.f, handAt, off) == EnterAnim::predicted && handAt > 0, "with the release the animation has the body before the touchdown");
        check(std::strcmp(enterAnimName(EnterAnim::predicted), "predicted-landing") == 0, "named for the log");
        // It lands from there: the touchdown makes no second switch, and the next jump starts afresh.
        {
            Sim s(300 * ms);
            s.ground();
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            s.run(300 * ms);
            s.f.velY = -9;
            s.f.landEta = .19f;
            s.f.landHandBack = true;
            s.run(40 * ms);
            check(s.owner.domain == Domain::anim && s.lastAnim == EnterAnim::predicted, "handed back at 0.19 s to impact");
            const int edges = s.animEdges;
            s.f.landHandBack = false;
            s.f.landEta = .1f;
            s.run(150 * ms);
            check(s.owner.domain == Domain::anim && s.vrEdges == 1, "kept to the landing");
            s.ground();
            check(s.owner.domain == Domain::anim && s.animEdges == edges, "no second switch at the touchdown");
            s.f.landEta = NAN;
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            check(s.owner.domain == Domain::vr, "the next jump is VR's again by its timer");
        }
        // Nothing to land on (a ledge's ray lost, a web took him up): back to VR.
        {
            Sim s(300 * ms);
            s.ground();
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            s.run(300 * ms);
            s.f.velY = -9;
            s.f.landEta = .19f;
            s.f.landHandBack = true;
            s.run(40 * ms);
            check(s.owner.domain == Domain::anim, "handed back");
            s.f.landHandBack = false;
            s.f.landEta = NAN;
            s.run(500 * ms);
            check(s.owner.domain == Domain::vr && s.owner.lastTimedWhy == TimedWhy::noLanding, "no landing came: VR's again");
            check(std::strcmp(timedWhyName(TimedWhy::noLanding), "no landing came") == 0, "named for the log");
        }
        // Never in a fight, a zip or a handstand, and not before the owner's 0.25 s.
        {
            Sim s(300 * ms);
            s.ground();
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            s.run(300 * ms);
            s.f.state.fighting = true;
            s.f.landEta = .15f;
            s.f.landHandBack = true;
            s.run(60 * ms);
            check(s.owner.domain == Domain::vr, "no release in a fight");
            s.f.state.fighting = false;
            s.f.handSupport = true;
            s.run(60 * ms);
            check(s.owner.domain == Domain::vr, "no release while the hands hold him");
        }
        {
            Sim s(300 * ms);
            s.ground();
            s.takeoff(true, 7.f);
            s.run(310 * ms);
            check(s.owner.domain == Domain::vr, "VR by the timer");
            s.f.landEta = .1f;
            s.f.landHandBack = true;
            s.run(100 * ms);
            check(s.owner.domain == Domain::vr, "the owner's minimum dwell: 0.25 s before it goes back");
            s.run(200 * ms);
            check(s.owner.domain == Domain::anim, "and then it does");
        }
    }
    // AIR TAKEOVER, slice 9: airborne moves the player asks the game for (web strike, finisher, a prompt's button) are animation
    // handoffs by request: the game takes him, plays it, and the air is SpidyVR's again unless a landing is near.
    {
        auto vrAir = [&](Sim& s) {
            s.f.takeoffDoneVel = takeoffDoneVel;
            s.f.takeoffMinNs = takeoffMinNs;
            s.f.fallHandoffNs = 150 * ms;
            s.ground();
            s.takeoff(true, 7.f);
            s.run(340 * ms);
            s.f.state.owned = true; // the swing has him in VR's air
            s.run(300 * ms);
            s.f.velY = 0;
        };
        {
            Sim s(300 * ms);
            vrAir(s);
            check(s.owner.domain == Domain::vr, "VR's air");
            s.f.zipPress = true; // the request
            s.run(frameNs);
            s.f.state.owned = false; // the game takes him: the swing yields
            s.run(40 * ms);
            check(s.owner.domain == Domain::anim && s.lastAnim == EnterAnim::zip, "the game took the request: the animation has the body");
            s.run(700 * ms);
            check(s.owner.domain == Domain::anim, "and plays it");
            s.f.state.owned = true; // its move ended: the swing has him back
            s.run(60 * ms);
            check(s.owner.domain == Domain::vr, "SpidyVR's air again after the move, no landing near");
        }
        {
            Sim s(300 * ms);
            vrAir(s);
            s.f.zipPress = true;
            s.run(600 * ms); // the game never takes it: owned stays true
            check(s.owner.domain == Domain::vr && s.animEdges == 0, "a request the game does not take changes nothing");
        }
        {
            // After a fall's start, the air stays the animation's when the ground is near, and goes to VR when it is not.
            for (bool near : {true, false}) {
                Sim s(300 * ms);
                s.f.takeoffDoneVel = takeoffDoneVel;
                s.f.takeoffMinNs = takeoffMinNs;
                s.f.fallHandoffNs = 150 * ms;
                s.ground();
                s.takeoff(false, -2.f);
                s.f.landEta = near ? .3f : .9f;
                s.run(400 * ms);
                check(near ? (s.owner.domain == Domain::anim && s.vrEdges == 0)
                           : (s.owner.domain == Domain::vr && s.lastVr == EnterVr::timed),
                      near ? "a short drop with the ground near is the animation's to the landing"
                           : "a long drop hands over at 0.15 s");
            }
        }
    }
    // The settings.
    {
        using namespace vr_settings;
        Values v;
        check(v.jumpHandoffMs == 250, "JUMP TO SPIDYVR: 250 ms by default (the handoff tune, Nov 18)");
        check(v.fallHandoffMs == 150, "FALL TO SPIDYVR: 150 ms by default");
        const Row* fallRow = rowFor(Item::fallHandoff);
        check(fallRow && fallRow->choices.size() == 11 && choose(Item::fallHandoff, 0, v) && v.fallHandoffMs == 0 &&
                  choose(Item::fallHandoff, 8, v) && v.fallHandoffMs == 250 && choice(Item::fallHandoff, Values{}) == 5,
              "FALL TO SPIDYVR choices: with the timer, then 50 to 400 ms in 25-50 ms steps, 150 the default");
        check(withTabOnly(Values{}, "fall_handoff_ms=400\n").fallHandoffMs == 400, "fall_handoff_ms read back");
        v.fallHandoffMs = 150;
        const Row* row = rowFor(Item::jumpHandoff);
        check(row && row->choices.size() == 11 && std::strcmp(row->choices[0], "STAY TO LANDING") == 0 &&
                  std::strcmp(row->choices[3], "250 MS (DEFAULT)") == 0 && std::strcmp(row->choices[10], "1000 MS") == 0 &&
                  choice(Item::jumpHandoff, Values{}) == 3,
              "choices STAY TO LANDING, then 150 to 1000 ms, 50 ms steps near the 300 ms default");
        check(choose(Item::jumpHandoff, 0, v) && v.jumpHandoffMs == 0, "STAY TO LANDING is 0");
        check(tabOnlyText(v).find("jump_handoff_ms=0") != std::string::npos, "saved as jump_handoff_ms");
        check(withTabOnly(Values{}, "jump_handoff_ms=700\n").jumpHandoffMs == 700, "read back");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::nativeAnimation)].items)
            listed = listed || item == Item::jumpHandoff;
        check(listed, "on NATIVE ANIMATION");
        check(v.airViewEaseMs == 200, "AIR VIEW EASE: 200 ms by default (Nov 18)");
        const Row* ease = rowFor(Item::airViewEase);
        check(ease && std::strcmp(ease->choices[0], "OFF (SNAP)") == 0, "OFF (SNAP) keeps the old snap");
        check(choose(Item::airViewEase, 0, v) && v.airViewEaseMs == 0, "OFF is 0");
        check(tabOnlyText(v).find("air_view_ease_ms=0") != std::string::npos, "saved as air_view_ease_ms");
        listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::camera)].items)
            listed = listed || item == Item::airViewEase;
        check(listed, "on VR CAMERA & STABILIZATION");
    }
    std::printf("jump handoff: %d checks passed\n", passed);
    return 0;
}
