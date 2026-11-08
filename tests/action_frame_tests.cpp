#include "spidy/animated_view.hpp"
#include "spidy/animation_cancel.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/game_swing.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/run_update.hpp"
#include "spidy/punch.hpp"
#include "spidy/shooter.hpp"
#include "spidy/tracking.hpp"
#include "spidy/web_aim.hpp"
#include <cstdio>
#include <cstdlib>
using namespace spidy;
namespace {
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
struct Open : WorldQueries {
    std::optional<RayHit> raycast(Vec3, Vec3, float) const override { return {}; }
    bool exists(std::uint64_t) const override { return false; }
};
struct OpenMotion : World {
    std::optional<RayHit> raycast(Vec3, Vec3, float) const override { return {}; }
    bool exists(std::uint64_t) const override { return false; }
    MoveResult sweep(Vec3, Vec3 to, float) const override { return {to, {}, false}; }
};
void punchFrameTests() {
    for (unsigned hand=0;hand<2;++hand) for(bool physical : {false,true}) {
        Punches punches;
        std::vector<PunchEvent> events;
        const Vec3 root{145,20,-735};
        const Quat map=Quat::yaw(1.2f);
        const float side=hand ? -.15f:.15f;
        const PunchTarget target{7,root+map.rotate({side,0,-1}),1.8f,.3f};
        const std::array<PunchTarget,1> targets{target};
        for(unsigned i=0;i<=12;++i) {
            const float z=-.2f-.8f*i/12.f;
            std::array<PunchHand,2> hands{};
            const Vec3 controller{side,1.4f,physical ? z:-.2f};
            hands[hand]={true,root+map.rotate({side,1.4f,z}),controller,false,map,true};
            punches.update(1.f/90,hands,targets,events);
        }
        if (physical) {
            check(events.size()==1 && events[0].damage>0,"physical animated hand contact did not damage");
            check(events[0].hand==static_cast<int>(hand) &&
                  dot(events[0].direction,map.rotate({0,0,-1}))>.99f,"contact velocity used rig rather than displayed axes");
        } else check(events.empty(),"passive animation motion caused punch damage");
    }
    std::printf("PASS: both-hand mapped physical contact damage; passive animation causes no blow\n");
}
void punchDiscontinuityTests() {
    for(unsigned hand=0;hand<2;++hand) for(bool stopped : {false,true})
      for(const Quat map : {Quat::yaw(1.2f),Quat::yaw(-1.7f)}) {
        Punches punches;std::vector<PunchEvent> events;std::array<PunchHand,2> hands{};
        const Vec3 root{145,20,-735};
        const std::array<PunchTarget,1> targets{{{7,root,1.8f,.3f}}};
        hands[hand]={true,root+map.rotate({-10,1.2f,0}),{0,1.2f,0},false,map,true};
        punches.update(.01f,hands,targets,events);
        if(stopped){hands[hand].relative.x=.3f;punches.update(.01f,hands,targets,events);}
        hands[hand].fist=root+map.rotate({stopped?-.1f:10.f,1.2f,0});
        if(!stopped)hands[hand].relative.x=.05f;
        punches.update(.01f,hands,targets,events);
        check(events.empty(),"authored/root relocation or residual physical speed caused a phantom contact");
        // Hand validity loss/reacquisition and contact-mode changes seed new
        // history, never sweep from the old owner/world position.
        hands[hand].tracked=false;punches.update(.01f,hands,targets,events);
        hands[hand].tracked=true;hands[hand].relative.x+=.3f;
        punches.update(.01f,hands,targets,events);
        check(events.empty(),"reacquired hand swept across stale history");
        hands[hand].physicalSweep=false;hands[hand].relative.x+=.3f;
        punches.update(.01f,hands,targets,events);
        hands[hand].physicalSweep=true;hands[hand].relative.x+=.3f;
        punches.update(.01f,hands,targets,events);
        check(events.empty(),"ownership/contact policy switch swept stale history");
    }
    // Both simultaneous grip/trigger hands are suppressed before any solver,
    // not only the hand which happened to generate the cancellation edge.
    animation_cancel::Gate gate;auto decision=gate.update(true,true,true,3,3);
    Input in;in.focused=true;
    for(auto& hand:in.hands){hand.tracked=true;hand.grip=hand.trigger=1;hand.aim={{0,2,0},{}};}
    animation_cancel::suppress(in.hands,decision);
    std::array<ShooterHand,2> shots;
    for(unsigned i=0;i<2;++i)shots[i]={true,in.hands[i].aim,in.hands[i].trigger,false};
    Shooter shooter;check(shooter.update(.02f,shots)==0,"two-hand cancel dispatched a ball");
    Swing swing;OpenMotion world;swing.reset({{0,1,0},{},true});swing.update(.02f,in,world);
    check(!swing.webs()[0].attached&&!swing.webs()[1].attached,"two-hand cancel dispatched a web");
    std::puts("PASS: both-hand rotated phantom-sweep, residual-speed, reacquisition/owner-history and simultaneous cancel regressions");
}
void activationTests() {
    for (const float stick : {0.f,.2f,.8f}) {
        // Ownership does not depend on a run threshold at all: idle, walking
        // and running all restore native on the exact grip-release frame.
        hybrid_owner::HybridOwner owner;
        hybrid_owner::Frame f;
        f.state.playing=f.state.fresh=f.state.grounded=true;
        owner.update(f,1);
        f.cancelGrip=f.cancelGripHeld=true;
        owner.update(f,2);
        check(owner.enterVr,"ground grip did not cancel");
        f.cancelGrip=false;
        owner.update(f,3);
        check(owner.domain==hybrid_owner::Domain::vr,"held ground cancel bounced");
        f.cancelGripHeld=false; f.speedH=stick*20;
        owner.update(f,4); // only 2ns after cancel, deliberately no dwell
        animated_moves::Handoff handoff;
        handoff.update(false);
        handoff.update(owner.animOwns(true,f.state));
        check(handoff.enter && handoff.yielding,"owner returned but production handoff deferred native ownership");
        check(owner.enterAnim && owner.lastEnterAnim==hybrid_owner::EnterAnim::ground,
              "ground release delayed native reactivation");
        owner.update(f,5);
        check(!owner.enterVr && !owner.enterAnim,"ground release synthesized another edge");
    }
    web_aim::ForearmAim correction;
    for(unsigned i=0;i<30;++i)correction.update(true,Quat::yaw(1.2f),.02f,180.f);
    check(correction.weight>.99f,"forearm fixture did not prime");
    correction.reset(); // owner-edge operation in XR
    correction.update(false,{},.02f,180.f); // old owner's serial is rejected
    check(web_aim::turnAngle(correction.turn(100))<.0001f,"prior-owner correction survived owner reset");
    hybrid_owner::HybridOwner owner;
    hybrid_owner::Frame f;
    f.state.playing=f.state.fresh=true;
    f.state.grounded=true; f.cancelGrip=f.cancelGripHeld=true;
    owner.update(f,1);
    f.cancelGrip=f.cancelGripHeld=false; f.groundAction=true;
    owner.update(f,2);
    check(owner.domain==hybrid_owner::Domain::vr,"active takeoff was cut short");
    f.groundAction=false;
    owner.update(f,3);
    check(owner.enterAnim,"cleared action retained artificial queue delay");
    owner={}; f={}; f.state.playing=f.state.fresh=true;
    owner.update(f,1);
    check(owner.domain==hybrid_owner::Domain::anim,"all air was incorrectly forced OG");
    owner.domain=hybrid_owner::Domain::vr; f.state.grounded=true;
    f.state.wall=hybrid_owner::surfaceWall;
    owner.update(f,2);
    check(owner.domain==hybrid_owner::Domain::vr,"wallrunning was forced native as ground");
    f.state.wall=hybrid_owner::surfaceGame; f.state.grounded=false;
    owner.update(f,3);
    check(owner.enterAnim,"native crawl wrongly retained OG body");
    std::printf("PASS: exact-frame idle/walk/run release, actual-action deferral and air/wall policy\n");
}
void cancellationTests() {
    using namespace animation_cancel;
    for (unsigned hand = 0; hand < 2; ++hand) for (bool grounded : {false, true}) {
        Gate gate;
        gate.update(true, true, true, 0, 0);
        const unsigned web = 1u << hand;
        auto d = gate.update(true, true, true, web, 3);
        check(d.cancel && d.holdVr && (d.webBlocked & web), "cancel gesture not consumed");
        hybrid_owner::HybridOwner owner;
        hybrid_owner::Frame f;
        f.state.playing = f.state.fresh = true;
        f.state.grounded = grounded;
        f.state.fighting = true; // grounded combat must also cancel
        f.zipPress = f.jumpAsk = true; // cancel overrides stale asks
        f.cancelGrip = d.cancel;
        f.cancelGripHeld = d.holdVr;
        owner.update(f, 1'000'000'000);
        check(owner.enterVr && owner.lastEnterVr == hybrid_owner::EnterVr::cancel,
              "cancel did not win owner transition");
        check(hybrid_owner::adoptsFromAir(owner.lastEnterVr, grounded) == !grounded,
              "cancel changed grounded/air pose adoption policy");
        OpenMotion world;
        Swing solver;
        const Body before{{5,20,12},{3,2,-1},grounded};
        solver.reset(before);
        solver.releaseAll(); solver.leaveWall();
        check(length(solver.body().position-before.position)<1e-6f &&
              length(solver.body().velocity-before.velocity)<1e-6f,
              "action-only cleanup changed world position or momentum");
        Shooter balls;
        const auto input = [&](Decision decision, bool held, bool trigger) {
            Input in; in.focused = true;
            for (unsigned i=0;i<2;++i) {
                in.hands[i].tracked = true;
                in.hands[i].aim = {{5+(i ? -.3f:.3f),21,12}, {}};
                in.hands[i].grip = held && i==hand ? 1.f : 0.f;
                in.hands[i].trigger = trigger ? 1.f : 0.f;
            }
            suppress(in.hands, decision);
            return in;
        };
        const auto step = [&](Decision decision, bool held, bool trigger) {
            auto in = input(decision, held, trigger);
            std::array<ShooterHand,2> h;
            for (unsigned i=0;i<2;++i) h[i] = {in.hands[i].tracked,in.hands[i].aim,in.hands[i].trigger,false};
            const auto shots = balls.update(.02f,h);
            solver.update(.02f,in,world);
            return shots;
        };
        check(step(d,true,true)==0, "cancel fired a web ball");
        check(!solver.webs()[0].attached && !solver.webs()[1].attached, "cancel fired a grip web");
        f.cancelGrip=false;
        for(unsigned i=0;i<12;++i) {
            d=gate.update(true,false,true,web,3);
            check(!d.cancel && d.holdVr, "held cancel retriggered or rearmed");
            f.cancelGripHeld=d.holdVr;
            owner.update(f,2'000'000'000+static_cast<int64_t>(i)*1'000'000'000);
            check(owner.domain==hybrid_owner::Domain::vr && !owner.enterAnim && !owner.enterVr,
                  "held cancel bounced ownership or reseeded pose");
            check(step(d,true,true)==0,"still-held trigger fired after cancel");
            check(!solver.webs()[0].attached && !solver.webs()[1].attached,"held cancel fired a web");
        }
        d=gate.update(true,false,true,0,0);
        check(!d.holdVr && !d.cancel,"release did not rearm");
        check(step(d,false,false)==0,"release fired a ball");
        f.cancelGripHeld=false; f.zipPress=f.jumpAsk=false;
        owner.update(f,19'000'000'000);
        if (grounded) {
            check(owner.enterAnim,"standing release did not restore native that frame");
            d=gate.update(true,true,true,web,3);
            check(d.cancel && step(d,true,true)==0,"fresh ground grip was not another consumed cancel");
        } else {
            check(owner.domain==hybrid_owner::Domain::vr,"air release unexpectedly forced animation");
            d=gate.update(true,false,true,web,3);
            check(!d.cancel && !d.webBlocked && !d.shotBlocked,"ordinary VR press intercepted");
            check(step(d,true,true)==3,"ordinary VR trigger press no longer fires");
            check(solver.webs()[hand].attached,"ordinary VR grip release/repress no longer shoots");
        }
        // Exact-frame eligible release is the owner's contract, not a delayed
        // landing confirmation. Arm actions above do not change that policy.
        f.cancelGripHeld=false; f.zipPress=f.jumpAsk=false;
        f.state.fighting=false; f.state.grounded=true;
        owner.update(f,20'000'000'000);
        check(owner.domain==hybrid_owner::Domain::anim,
              "ground release failed same-update native handback");
    }
    Gate disabled;
    check(!disabled.update(false,true,true,1,0).cancel,"disabled hybrid consumed ordinary grip");
    Gate both;
    both.update(true,true,true,0,0);
    auto d=both.update(true,true,true,3,0);
    check(d.cancel && d.webBlocked==3,"simultaneous grips not consumed");
    d=both.update(true,false,true,1,0);
    check(d.holdVr,"one remaining held grip released cancellation");
    d=both.update(true,false,true,0,0,0);
    check(d.holdVr,"tracking loss falsely rearmed held grip");
    d=both.update(true,false,true,0,0,3);
    check(!d.holdVr,"tracked release failed to rearm both hands");
    uint32_t seen{};
    check(consumeToken(4,seen) && !consumeToken(4,seen) && !consumeToken(0,seen),
          "persistent cancel command was not idempotent");
    check(consumeToken(6,seen),"coalesced/missed intermediate cancel token lost");
    check(!validAction(9,1,2,10),"copied old shot batch survived generation revoke");
    check(!validAction(9,2,2,10),"late old-input enqueue survived serial floor");
    check(validAction(10,2,2,10) && validAction(11,2,2,10),"new VR actions rejected");
    check(!validAction(0,1,2,10) && validAction(0,2,2,10),"independent gadget lease broken");
    check(animation_cancel::nativeActions(true,false,false,true),"released supported frame allowed original lunge");
    check(!animation_cancel::nativeActions(true,true,true,true),"held override lost original functionality");
    check(animation_cancel::nativeActions(true,true,false,false),"native airborne animation lost contact-only mode");
    check(!animation_cancel::nativeActions(true,false,false,false),"ordinary airborne VR lost original functionality");
    check(!animation_cancel::nativeActions(false,false,false,true),"disabled hybrid changed original behavior");
    game_swing::Command packet; packet.serial=1; packet.focused=1; packet.animationCancel=6;
    check(sizeof(packet)==304 && packet.version==7 && game_swing::valid(packet),"v7 action packet invalid");
    packet.version=6; packet.bytes=game_swing::commandBytesV6;
    check(game_swing::valid(packet),"legacy v6 action packet rejected");
    check(!validPose(8,9,100,50) && !validPose(9,9,100,100) && validPose(9,9,100,99),
          "old-owner/expired pose action accepted");
    std::printf("PASS: cancellation, held/release/repress, owner continuity, momentum and action fencing\n");
}
}
int main() {
    Open world;
    Shooter shooter;
    const Vec3 anchors[] = {{-140, 35, 820}, {285, 91, -507}, {0, 0, 0}};
    // Exercise translated/rotated animation head frames and both mirrored
    // wrists. resolve's output here, like OverlayTracking, is feet-relative
    // in WORLD axes, not an absolute game-world position.
    const Pose heads[] = {{{.1f, 1.6f, -.1f}, Quat::yaw(1.1f) * Quat::around({1,0,0}, -.35f)},
                          {{-.2f, 1.4f, .2f}, Quat::yaw(-2.1f)}};
    unsigned cases{};
    for (const auto feet : anchors) for (const auto head : heads) for (unsigned hand = 0; hand < 2; ++hand) {
        const Pose local{{hand ? -.3f : .3f, -.3f, -.4f}, Quat::yaw(hand ? -.3f : .3f)};
        const Pose grip = animated_view::resolve(animated_view::toNative(head), local);
        Pose aim{};
        check(mapAimToDisplayedGrip(aim, grip, feet, hand, 0.f), "free hand not mapped");
        Pose absoluteGrip = grip;
        absoluteGrip.position += feet;
        const Pose expected = webShooter(absoluteGrip, hand);
        check(length(aim.position - expected.position) < 1e-4f, "feet translation missing/doubled");
        check(web_aim::turnAngle(aim.orientation * expected.orientation.conjugate()) < .001f,
              "world orientation turned twice");
        const auto shot = shooter.aim(static_cast<int>(hand), aim, {}, world);
        check(length(shot.origin - (expected.position + shot.direction * ShooterConfig{}.muzzle)) < 1e-4f,
              "physics shot lost absolute muzzle");
        check(dot(shot.direction, expected.orientation.rotate({0,0,-1})) > .9999f, "shot direction mismatch");
        check(length(normalized(shot.aimPoint - shot.origin) - shot.direction) < 1e-4f,
              "shot aim point and muzzle in different worlds");
        // Forearm correction changes only orientation, retaining absolute origin.
        const Vec3 forearm = expected.orientation.rotate({0,0,-1});
        const auto turned = web_aim::shooterAim(aim, static_cast<int>(hand), {},
                            web_aim::forearmWorldTurn(forearm, aim.orientation, static_cast<int>(hand)));
        check(length(turned.position - expected.position) < 1e-4f, "forearm lost world origin");
        // Existing grip-out semantics and the tracking-rig pose stay intact.
        Pose held{{7,8,9}, Quat::yaw(-.4f)};
        const Pose before = held;
        check(!mapAimToDisplayedGrip(held, grip, feet, hand, .6f), "grip-out remapped");
        check(length(held.position-before.position)<1e-6f, "grip-out position mutated");
        check(web_aim::turnAngle(held.orientation*before.orientation.conjugate())<.001f,
              "grip-out orientation mutated");
        ++cases;
    }
    // Mirror native_body's actual full/level/world anchor construction and
    // model->world publication, including nonzero actor rotation. Compare to
    // the direct rendered actor-root * hand chain, not an already-world input.
    unsigned renderedCases{};
    for (const Vec3 feet : anchors) for (const Pose h : heads)
      for (const auto mode : {animated_view::Frame::full,animated_view::Frame::level,animated_view::Frame::world})
       for (unsigned hand=0;hand<2;++hand) {
        const Quat root=Quat::yaw(.7f)*Quat::around({1,0,0},.18f), inverseRoot=root.conjugate();
        Pose anchor=h;
        if(mode==animated_view::Frame::level) anchor.orientation=inverseRoot*Quat::yaw(-.4f);
        if(mode==animated_view::Frame::world) anchor={inverseRoot.rotate(h.position),inverseRoot*h.orientation};
        const Pose local{{hand ? -.3f:.3f,-.3f,-.4f},Quat::yaw(hand ? -.3f:.3f)};
        const Pose modelGrip=animated_view::resolve(animated_view::toNative(anchor),local);
        const Pose published{root.rotate(modelGrip.position),root*modelGrip.orientation};
        Pose aim;
        check(mapAimToDisplayedGrip(aim,published,feet,hand,0),"rendered anchor not mapped");
        const Pose renderedGrip=compose(Pose{feet,root},compose(anchor,local));
        const Pose expected=webShooter(renderedGrip,hand);
        check(length(aim.position-expected.position)<.0002f,"rendered mode lost/doubled root translation");
        check(web_aim::turnAngle(aim.orientation*expected.orientation.conjugate())<.001f,"rendered mode doubled root turn");
        ++renderedCases;
    }
    Pose untouched{{1,2,3}, {}};
    check(!mapAimToDisplayedGrip(untouched, {}, {NAN,0,0}, 0, 0.f), "invalid feet accepted");
    check(length(untouched.position-Vec3{1,2,3})<1e-6f, "rejection changed pose");
    check(!mapAimToDisplayedGrip(untouched, {{NAN,0,0},{}}, {}, 1, 0.f), "invalid grip accepted");
    check(!mapAimToDisplayedGrip(untouched, {}, {}, 2, 0.f), "invalid hand accepted");
    check(!mapAimToDisplayedGrip(untouched, {{},{0,0,0,0}}, {},0,0),"zero mapped orientation accepted");
    check(!mapAimToDisplayedGrip(untouched, {{},{NAN,0,0,1}}, {},0,0),"nonfinite mapped orientation accepted");
    check(!mapAimToDisplayedGrip(untouched, {}, {},0,NAN),"nonfinite hold accepted");
    check(length(untouched.position-Vec3{1,2,3})<1e-6f,"invalid mapping mutated original aim");
    cancellationTests();
    punchFrameTests();
    punchDiscontinuityTests();
    activationTests();
    std::printf("PASS: %u translated/rotated geometry cases + %u rendered full/level/world chains\n", cases,renderedCases);
}
