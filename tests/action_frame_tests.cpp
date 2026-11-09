#include "spidy/animated_view.hpp"
#include "spidy/animation_cancel.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/game_swing.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/run_update.hpp"
#include "spidy/punch.hpp"
#include "spidy/presentation_gate.hpp"
#include "spidy/native_eye_frame.hpp"
#include <cstring>
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
// acos(q.w) assumes an exactly unit quaternion. Even q * conjugate(q)
// can have w slightly below 1 after MSVC float rounding, producing a false
// milliradian mismatch for identical poses. Compare the actual basis instead.
bool sameOrientation(Quat a, Quat b) {
    for (const Vec3 axis : {Vec3{1,0,0}, Vec3{0,1,0}, Vec3{0,0,1}})
        if (!(length(a.rotate(axis)-b.rotate(axis)) < 1e-5f)) return false;
    return true;
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
void feedbackAndCancelPipelineTests() {
    check(nativeTrackedPresentation(true,true,true,true),"world/head-off native body lost native presentation");
    check(!nativeTrackedPresentation(true,true,false,true) &&
          !nativeTrackedPresentation(false,true,true,true) &&
          !nativeTrackedPresentation(true,true,true,false),"OG/body-off/arms-off presentation changed");
    for(const Vec3 root : {Vec3{145,20,-735},Vec3{-350,90,510}})
      for(const Quat turn : {Quat::yaw(1.3f),Quat::yaw(-2.2f)*Quat::around({1,0,0},.4f)}) {
        std::array<Mat4,2> actual;
        const auto head=animated_view::toNative({root,turn});
        for(unsigned i=0;i<2;++i)
            actual[i]=native_eyes::onHead(animated_view::toNative({{i?.032f:-.032f,0,0},{}}),head);
        const Vec3 travel{.3f,-.2f,.5f};
        const auto overlay=native_eyes::overlayEyes(actual,travel);
        for(unsigned i=0;i<2;++i) {
            const auto a=animated_view::fromNative(actual[i]),b=animated_view::fromNative(overlay[i]);
            check(length(b.position+travel-a.position)<.0001f,"native indicator camera translated twice");
            check(sameOrientation(a.orientation,b.orientation),"native indicator camera lost render rotation");
        }
      }
    check(gameplayFeedback(false,false,false), "OG feedback hidden");
    check(gameplayFeedback(false,true,true), "native feedback hidden with duplicate geometry");
    check(!gameplayFeedback(false,true,false), "cinematic feedback leaked");
    for(bool scene : {false,true}) for(bool animation : {false,true})
        check(!gameplayFeedback(true,scene,animation), "flat-screen feedback leaked");
    check(!animation_cancel::enabled(false,true,true,false), "absent body enabled cancel");
    check(!animation_cancel::enabled(true,false,true,false), "nonhybrid cancel changed");
    check(!animation_cancel::enabled(true,true,false,false), "MOVES-off cancel changed");
    check(!animation_cancel::enabled(true,true,true,true), "calibration enabled cancel");
    animation_cancel::Gate unfocused;
    check(!unfocused.update(true,true,false,3,3).cancel,"unfocused input canceled ownership");
    for(unsigned mask : {1u,2u,3u}) {
        // Negative control: the previous XR predicate disabled the Gate when
        // mover freshness was false although focused controller input was live.
        animation_cancel::Gate old;
        Input escaped;escaped.focused=true;
        for(unsigned i=0;i<2;++i) {
            escaped.hands[i].tracked=true;escaped.hands[i].aim={{i?.2f:-.2f,2,0},{}};
            escaped.hands[i].grip=(mask>>i)&1;
        }
        const auto missed=old.update(false,true,true,mask,0);
        animation_cancel::suppress(escaped.hands,missed);
        Swing oldSwing;OpenMotion oldWorld;oldSwing.reset({{0,2,0},{},false});
        oldSwing.update(.011f,escaped,oldWorld);
        for(unsigned i=0;i<2;++i) check(oldSwing.webs()[i].attached==bool(mask&(1u<<i)),
                                      "stale cancellation bypass negative control did not emit web");
        check(!old.update(true,true,true,mask,0).cancel,"old gate unexpectedly recovered held cancel edge");
        animation_cancel::Gate gate;
        Swing swing;OpenMotion world;swing.reset({{0,2,0},{},false});
        game_swing::InputHold hold;
        unsigned token=0,seen=0;uint64_t serial=0;
        const auto sample=[&](bool fresh,bool anim,unsigned web,unsigned shot) {
            // Production XR policy deliberately does not use mover freshness.
            // It may freeze the later owner FSM, never the input consumption.
            const auto decision=gate.update(animation_cancel::enabled(true,true,true,false),
                                             anim,true,web,shot);
            Input raw;raw.focused=true;
            for(unsigned i=0;i<2;++i) {
                raw.hands[i].tracked=true;raw.hands[i].aim={{i?.2f:-.2f,2,0},{}};
                raw.hands[i].grip=(web>>i)&1;raw.hands[i].trigger=(shot>>i)&1;
            }
            animation_cancel::suppress(raw.hands,decision);
            game_swing::Command sent;sent.focused=1;sent.serial=++serial;sent.sampleTimeNs=serial;
            if(decision.cancel) ++token;
            sent.animationCancel=token;
            for(unsigned i=0;i<2;++i) {
                const auto& h=raw.hands[i];
                sent.hands[i]={h.aim,h.gripRelativeToHead,1,h.trigger,h.grip};
            }
            game_swing::Command received;
            check(game_swing::decodeCommand(received,[&](void* dst,size_t n){
                if(n>sizeof(sent)) return false;std::memcpy(dst,&sent,n);return true;
            }), "cancel producer packet rejected by real decoder");
            check(hold.update(received,true,serial*11), "cancel command not retained");
            if(animation_cancel::consumeToken(received.animationCancel,seen)) swing.releaseAll();
            const auto input=game_swing::input(received);
            swing.update(.011f,input,world);
            (void)fresh; // source-derived stale-mover case: owner update is later
            return decision;
        };
        check(sample(false,true,mask,mask).cancel, "stale mover swallowed native cancel edge");
        check(!swing.webs()[0].attached&&!swing.webs()[1].attached,"stale cancel fired through command pipeline");
        check(sample(true,false,mask,mask).holdVr,"held cancel override lost after freshness restored");
        check(!swing.webs()[0].attached&&!swing.webs()[1].attached,"held cancel fired through receiver");
        check(!sample(true,false,0,0).holdVr,"release did not end cancel hold");
        // Independent repress in airborne OG must still use the original solver.
        sample(true,false,mask,0);
        for(unsigned i=0;i<2;++i) check(swing.webs()[i].attached==bool(mask&(1u<<i)),
                                      "ordinary OG web no longer works after cancel release");
    }
    std::puts("PASS: presentation parity and stale-mover cancel producer/decoder/hold/input/actual-swing pipeline");
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
        // GRIP CANCEL STAYS IN SPIDYVR (November 8 21:51): idle, walking and
        // running on the ground keep SpidyVR after release; no handback.
        owner.update(f,4);
        animated_moves::Handoff handoff;
        handoff.update(false);
        handoff.update(owner.animOwns(true,f.state));
        check(!handoff.enter && !handoff.yielding && owner.domain==hybrid_owner::Domain::vr && !owner.enterAnim,
              "ground release after cancel handed back to animation");
        owner.update(f,5);
        check(!owner.enterVr && !owner.enterAnim,"cancel stay synthesized an edge");
        f.state.fighting=true; // the game's own fight starts: the handback
        owner.update(f,6);
        handoff.update(owner.animOwns(true,f.state));
        check(owner.enterAnim && owner.lastEnterAnim==hybrid_owner::EnterAnim::native,
              "native start did not hand back");
        check(handoff.enter && handoff.yielding,"owner returned but production handoff deferred native ownership");
        owner.update(f,7);
        check(!owner.enterVr && !owner.enterAnim,"native handback synthesized another edge");
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
    check(owner.domain==hybrid_owner::Domain::vr,"cleared action ended the cancel stay");
    f.state.wall=hybrid_owner::surfaceGame; // the game's crawl starts
    owner.update(f,4);
    check(owner.enterAnim && owner.lastEnterAnim==hybrid_owner::EnterAnim::native,"crawl start did not hand back");
    f.state.wall=0;
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
    std::printf("PASS: cancel stay through idle/walk/run until a native start, actual-action deferral and air/wall policy\n");
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
        // GRIP CANCEL STAYS IN SPIDYVR (the user, November 8 21:51): release
        // no longer hands back on the ground; full OG play, web lines on the
        // ground included, until another native animation starts on its own.
        check(owner.domain==hybrid_owner::Domain::vr && !owner.enterAnim && owner.cancelSticky,
              "release after cancel left SpidyVR");
        d=gate.update(true,false,true,web,3);
        check(!d.cancel && !d.webBlocked && !d.shotBlocked,"press after cancel still intercepted");
        check(step(d,true,true)==3,"trigger press after cancel no longer fires");
        check(solver.webs()[hand].attached,"grip after cancel shot no web (lingering suppression)");
        f.cancelGripHeld=false; f.zipPress=f.jumpAsk=false;
        f.state.fighting=false; f.state.grounded=true;
        for (unsigned i=0;i<5;++i) {
            owner.update(f,20'000'000'000+static_cast<int64_t>(i)*1'000'000'000);
            check(owner.domain==hybrid_owner::Domain::vr,"standing still/walking ended the cancel stay");
        }
        f.state.dodging=true; // the game's own dodge starts: the handback
        owner.update(f,26'000'000'000);
        check(owner.domain==hybrid_owner::Domain::anim && owner.enterAnim &&
              owner.lastEnterAnim==hybrid_owner::EnterAnim::native && !owner.cancelSticky,
              "native start did not end the cancel stay");
        f.state.dodging=false;
        owner.update(f,27'000'000'000);
        check(owner.domain==hybrid_owner::Domain::anim,"handback did not hold (ground native)");
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
    feedbackAndCancelPipelineTests();
    Open world;
    check(sameOrientation({}, {0,0,0,.9999998f}), "roundoff identity rejected");
    check(!sameOrientation({}, Quat::yaw(.01f)), "real orientation error accepted");
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
        check(sameOrientation(aim.orientation, expected.orientation),
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
        check(sameOrientation(held.orientation, before.orientation),
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
        check(sameOrientation(aim.orientation, expected.orientation),"rendered mode doubled root turn");
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
