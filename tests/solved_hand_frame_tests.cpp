#include "spidy/body_ik.hpp"
#include "spidy/tracking.hpp"
#include "spidy/web_aim.hpp"
#include "spidy/upper_frame.hpp"
#include "spidy/feedback_frame.hpp"
#include "spidy/animation_cancel.hpp"
#include "spidy/punch.hpp"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace spidy;
void check(bool b,const char* m){if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
// Fixture copied from core_tests.cpp, including noncanonical/mirrored joint frames.
// A small humanoid in the game's joint layout (four rows: x, y, z axes, then
// the position), facing +z with its left on +x, its rest pose a T-pose. With
// oddFrames every joint frame is turned and the left side mirrored, as game
// rigs do, so the solver cannot lean on joint axes. With heroHand its hands
// are Spider-Man's own (four fingers and heroThumbs).
struct TestBody {
    std::vector<float> rest;
    body::Rig rig;
    Vec3 at(const std::vector<float>& pose,int joint) const {return {pose[joint*16+12],pose[joint*16+13],pose[joint*16+14]};}
};
TestBody testBody(bool oddFrames=false,bool heroHand=false) {
    struct J {int parent;Vec3 p;};
    std::vector<J> joints={
        {-1,{0,0,0}},{0,{0,1.f,0}},{1,{0,1.12f,0}},{2,{0,1.25f,0}},{3,{0,1.38f,0}},{4,{0,1.52f,0}},{5,{0,1.62f,0}},
        {6,{.032f,1.7f,.08f}},{6,{-.032f,1.7f,.08f}},
        {4,{.03f,1.47f,.02f}},{9,{.18f,1.46f,0}},{10,{.44f,1.46f,0}},{11,{.74f,1.46f,0}},{12,{.83f,1.46f,0}},
        {12,{.77f,1.45f,.04f}},
        {4,{-.03f,1.47f,.02f}},{15,{-.18f,1.46f,0}},{16,{-.44f,1.46f,0}},{17,{-.74f,1.46f,0}},{18,{-.83f,1.46f,0}},
        {18,{-.77f,1.45f,.04f}},
        {1,{.1f,.95f,0}},{21,{.1f,.5f,.02f}},{22,{.1f,.08f,0}},{23,{.1f,0,.13f}},
        {1,{-.1f,.95f,0}},{25,{-.1f,.5f,.02f}},{26,{-.1f,.08f,0}},{27,{-.1f,0,.13f}},
        // A left middle finger, palm down: base in the palm, knuckle, middle joint, tip, end.
        {12,{.77f,1.46f,0}},{29,{.83f,1.46f,0}},{30,{.875f,1.46f,0}},{31,{.905f,1.46f,0}},{32,{.93f,1.46f,0}},
        // Its thumb, ahead of the palm: base, two joints, end.
        {12,{.76f,1.45f,.03f}},{34,{.78f,1.45f,.06f}},{35,{.8f,1.45f,.085f}},{36,{.82f,1.45f,.1f}},
    };
    if(heroHand){
        // Spider-Man's left hand at rest (the game's rig), turned so its fingers run along +x and the palm a rest
        // pose is assumed to hold (down, across the fingers) is -y. Its own palm faces 30 degrees from there,
        // toward the thumb. Its middle finger and thumb take the places of the small ones (and the hand's
        // finger and thumb joints theirs); its index, ring and little fingers follow, then the right hand, the
        // left's mirror image.
        const Vec3 fingers[4][5]={
            {{.7547f,1.4682f,.0254f},{.8261f,1.4679f,.0223f},{.8739f,1.4709f,.0272f},{.9036f,1.4727f,.0302f},{.931f,1.4743f,.0329f}},
            {{.7521f,1.4625f,.008f},{.8226f,1.46f,0},{.8791f,1.4514f,-.0046f},{.9141f,1.4459f,-.0075f},{.9464f,1.441f,-.0102f}},
            {{.7507f,1.4533f,-.0112f},{.8093f,1.4475f,-.0189f},{.8603f,1.4276f,-.0306f},{.8931f,1.4147f,-.0382f},{.9241f,1.4026f,-.0453f}},
            {{.7469f,1.4398f,-.021f},{.7989f,1.4339f,-.0358f},{.8333f,1.4134f,-.0501f},{.8545f,1.4007f,-.0589f},{.8741f,1.3891f,-.0671f}}};
        const Vec3 thumb[4]={{.7509f,1.4536f,.0366f},{.7913f,1.4578f,.0674f},{.8164f,1.4634f,.0953f},{.8359f,1.4701f,.1244f}};
        joints[13].p=fingers[1][1];joints[14].p=thumb[0];
        for(int k=0;k<5;++k)joints[29+k].p=fingers[1][k];
        for(int k=0;k<4;++k)joints[34+k].p=thumb[k];
        for(const int f:{0,2,3}){
            const int first=static_cast<int>(joints.size());
            for(int k=0;k<5;++k)joints.push_back({k?first+k-1:12,fingers[f][k]});
        }
        const auto mirror=[](Vec3 v){return Vec3{-v.x,v.y,v.z};};
        joints[19].p=mirror(fingers[1][1]);joints[20].p=mirror(thumb[0]);
        for(int f=0;f<4;++f){
            const int first=static_cast<int>(joints.size());
            for(int k=0;k<5;++k)joints.push_back({k?first+k-1:18,mirror(fingers[f][k])});
        }
        const int first=static_cast<int>(joints.size());
        for(int k=0;k<4;++k)joints.push_back({k?first+k-1:18,mirror(thumb[k])});
    }
    TestBody b;
    const int n=static_cast<int>(joints.size());
    b.rest.assign(n*16,0.f);
    for(int j=0;j<n;++j){
        Quat q{};
        if(oddFrames)q=body::unit({std::sin(j*1.3f),std::cos(j*.7f),std::sin(j*.31f+1),1.5f});
        Vec3 axes[3]={q.rotate({1,0,0}),q.rotate({0,1,0}),q.rotate({0,0,1})};
        if(oddFrames&&joints[j].p.x>.01f)axes[0]=axes[0]*-1.f;
        float* m=&b.rest[j*16];
        for(int r=0;r<3;++r){m[r*4]=axes[r].x;m[r*4+1]=axes[r].y;m[r*4+2]=axes[r].z;}
        m[12]=joints[j].p.x;m[13]=joints[j].p.y;m[14]=joints[j].p.z;m[15]=1;
        b.rig.parent.push_back(static_cast<int16_t>(joints[j].parent));
    }
    auto& r=b.rig;
    r.pelvis=1;r.spine={2,3,4};r.neck={5};r.head=6;r.eyes[0]=7;r.eyes[1]=8;
    r.arms[0]={9,10,11,12,13,14};r.arms[1]={15,16,17,18,19,20};
    r.arms[0].fingers={{29,30,31,32,33}};
    r.arms[0].thumbChain={34,35,36,37};
    if(heroHand){
        r.arms[0].fingers={{38,39,40,41,42},{29,30,31,32,33},{43,44,45,46,47},{48,49,50,51,52}};
        r.arms[1].fingers={{53,54,55,56,57},{58,59,60,61,62},{63,64,65,66,67},{68,69,70,71,72}};
        r.arms[1].thumbChain={73,74,75,76};
    }
    r.legs[0]={21,22,23};r.legs[1]={25,26,27};
    check(body::prepare(r,b.rest),"the test rig was rejected");
    return b;
}

void upperBodyCases() {
 check(!upper_frame::hideHead(true,true,true),"head-follow would apply both head shrinkers");
 check(upper_frame::hideHead(true,true,false) && upper_frame::hideHead(true,false,false),
       "World/head-off or original OG head hiding lost");
 check(upper_frame::armWeight(true,true,true,.55f)==1.f,"tracked player arm mixed passive animation");
 check(upper_frame::armWeight(true,false,true,.3f)==.3f,"tracking loss lost existing visual fade");
 check(upper_frame::armWeight(true,true,false,1.f)==0.f,"missing view invented a hand frame");
 check(upper_frame::armWeight(false,true,true,.55f)==.55f,"legacy arms-only weight changed");
 unsigned cases=0; float maxHeadError=0;
 for(bool odd : {false,true})
 for(auto frame : {animated_view::Frame::full,animated_view::Frame::level,animated_view::Frame::world})
 for(Quat rootTurn : {Quat::yaw(.8f), Quat::yaw(-1.4f)*Quat::around({1,0,0},.35f)})
 for(Quat hmd : {Quat::yaw(.45f)*Quat::around({1,0,0},.25f),
                  Quat::yaw(-.55f)*Quat::around({0,0,1},-.2f)}) {
  auto b=testBody(odd,true);auto data=b.rest;
  body::Pose pose(data.data(),static_cast<int>(b.rig.parent.size()));
  // A changing authored chest pose, not just T-pose: real model matrices.
  pose.turn(b.rig.below[b.rig.spine.front()],Quat::yaw(.4f)*Quat::around({1,0,0},.2f),
            pose.position(b.rig.spine.front()));
  const auto original=data;
  Mat4 authored{};check(animated_view::headFrame(pose.position(b.rig.head),
      pose.direction(b.rig.head,b.rig.headForward),pose.direction(b.rig.head,b.rig.headUp),
      b.rig.eyesFromHead,authored),"original author head rejected");
  animated_view::Targets v;v.frame=frame;v.levelWorld=Quat::yaw(.7f);
  const auto originalAnchor=animated_view::fromNative(authored);
  const Vec3 actorRoot{170,35,-460};
  v.worldAnchor={actorRoot+rootTurn.rotate(originalAnchor.position),rootTurn*originalAnchor.orientation};
  v.head={{.025f,-.035f,.02f},hmd};
  for(unsigned h=0;h<2;++h) v.grips[h]={{h? .27f:-.27f,-.28f,-.32f},hmd};
  Mat4 anchor{};check(animated_view::modelAnchor(v,rootTurn,authored,anchor,actorRoot),"production frame map rejected");
  body::Targets t;t.head=t.armsOnly=t.viewMapped=t.torsoAssist=true;t.armWeight={1,1};
  const auto head=animated_view::resolve(anchor,v.head);t.eyes=head.position;t.facing=head.orientation;
  t.up=rootTurn.conjugate().rotate({0,1,0});
  for(unsigned h=0;h<2;++h){const auto grip=animated_view::resolve(anchor,v.grips[h]);
    t.hands[h]={true,grip.position,grip.orientation,0,0};}
  body::State state;body::Config config;config.hideHead=false;
  const auto result=body::solve(pose,b.rig,t,config,state,true,1.f/90,1,1);
  check(result.solved,"upper production pose did not solve");
  maxHeadError=std::max(maxHeadError,result.headError);
  check(std::isfinite(result.headError) && result.headError < .08f,"reachable HMD head translation residual too large");
  check(dot(normalized(pose.direction(b.rig.head,b.rig.headForward)),
            head.orientation.rotate({0,0,-1}))>.999f,"HMD head forward lost");
  check(dot(normalized(pose.direction(b.rig.head,b.rig.headUp)),
            head.orientation.rotate({0,1,0}))>.999f,"HMD head roll/up lost");
  for(unsigned j=0;j<b.rig.parent.size();++j) {
   const auto& upper=b.rig.below[b.rig.spine.front()];
   const bool isUpper=std::find(upper.begin(),upper.end(),j)!=upper.end();
   if(!isUpper)check(std::memcmp(data.data()+j*16,original.data()+j*16,16*sizeof(float))==0,
                    "upper solve changed root/pelvis/lower-body matrix");
   const int parent=b.rig.parent[j];if(parent<0)continue;
   const float before=length(b.at(original,j)-b.at(original,parent));
   const float after=length(pose.position(j)-pose.position(parent));
   check(std::abs(before-after)<.0001f,"upper solve stretched/detached a bone");
  }
  // The old head-pivot assist broke this exact waist attachment.
  check(length(pose.position(b.rig.spine.front())-b.at(original,b.rig.spine.front()))<1e-6f,
        "waist attachment moved");
  for(unsigned hand=0;hand<2;++hand) {
   spidy::Pose actual;check(body::solvedGrip(pose,b.rig,config,hand,actual),"upper actual grip rejected");
   const Quat newer=Quat::yaw(-.6f)*rootTurn;
   const auto job=upper_frame::rebase(actual,rootTurn);
   const auto current=upper_frame::rebase(job,newer*rootTurn.conjugate());
   const Vec3 root{170,35,-460};spidy::Pose aim;
   check(mapAimToDisplayedGrip(aim,current,root,hand,0),"current-root action rejected");
   const auto expected=webShooter(compose({root,newer},actual),hand);
   check(length(expected.position-aim.position)<.0001f,"actual world emitter translation/rotation doubled");
   check(dot(expected.orientation.rotate({0,0,-1}),aim.orientation.rotate({0,0,-1}))>.9999f,
         "rendered/action hand axes diverged");
  }
  ++cases;
 }
 // Source camera anchor has the original matrix, even after rendering a moved
 // HMD head. Current root moves once; lease/instance/serial gates fail closed.
 auto b=testBody(true);upper_frame::Anchor saved;saved.active=true;saved.instance=42;
 saved.serial=7;saved.deadline=200;std::copy_n(b.rest.data()+b.rig.head*16,16,saved.model.begin());
 Mat4 root=animated_view::toNative({{350,20,-700},Quat::yaw(.8f)});
 // toNative is a camera frame; use physical actor basis here.
 root={.6967067f,0,-.7173561f,0,0,1,0,0,.7173561f,0,.6967067f,0,350,20,-700,1};
 Mat4 result{};check(saved.read(42,199,root,result)==1,"fresh author anchor unavailable");
 check(length(Vec3{result[12],result[13],result[14]}-
       (Vec3{350,20,-700}+Quat::yaw(.8f).rotate(b.at(b.rest,b.rig.head))))<.0001f,
       "authored camera anchor composed current actor incorrectly");
 const auto original=result;saved.model[12]+=.1f;
 check(saved.read(42,200,root,result)==-1,"expired camera anchor accepted");
 check(result==original,"expired camera anchor mutated output");
 check(saved.read(43,199,root,result)==0,"different actor used our authored head");
 saved.serial=0;check(saved.read(42,100,root,result)==-1,"unversioned camera anchor accepted");
 saved.active=false;check(saved.read(42,100,root,result)==0,"OG/cinematic anchor was intercepted");
 saved.begin(42);check(saved.read(42,100,root,result)==-1,
    "native activation before first writer read its own modified OG head");
 check(saved.read(43,100,root,result)==0,"pending native anchor hijacked another actor");
 // World/head-off has no game_fp follow-head shrinker: actual player head
 // geometry must still be hidden, without changing head centre or lower rig.
 auto hiddenBody=testBody(true);auto hidden=hiddenBody.rest, visible=hiddenBody.rest;
 body::Targets ht;ht.armsOnly=ht.head=ht.torsoAssist=ht.viewMapped=true;ht.armWeight={1,1};
 ht.eyes={0,1.68f,.08f};ht.facing=Quat::yaw(3.14159265f);
 for(unsigned h=0;h<2;++h)ht.hands[h]={true,{h?-.25f:.25f,1.4f,.3f},{},0,0};
 body::State hs,vs;body::Config hc,vc;hc.hideHead=true;vc.hideHead=false;
 body::Pose hp(hidden.data(),hiddenBody.rig.parent.size()),vp(visible.data(),hiddenBody.rig.parent.size());
 body::solve(hp,hiddenBody.rig,ht,hc,hs,true,.01f,1,1);
 body::solve(vp,hiddenBody.rig,ht,vc,vs,true,.01f,1,1);
 check(length(hp.position(hiddenBody.rig.head)-vp.position(hiddenBody.rig.head))<1e-5f,
       "self-head hiding moved gameplay head");
 check(length(hp.direction(hiddenBody.rig.head,{1,0,0}))<.002f,
       "world native head still fills the player's view");
 check(std::memcmp(hidden.data()+hiddenBody.rig.pelvis*16,visible.data()+hiddenBody.rig.pelvis*16,
       16*sizeof(float))==0,"self-head hiding changed pelvis");
 std::printf("max tracked-head residual %.4fm\n",maxHeadError);
 std::printf("PASS: %u production mapped upper-body cases (Full/Level/World, mirrored rigs, actor/HMD turns); rooted bone/lower matrices and camera lease gates\n",cases);
}

void articulatedPunchCases() {
 for(unsigned hand=0;hand<2;++hand) for(bool physical : {false,true}) {
  auto b=testBody(true,true);body::State state;body::Config config;config.hideHead=false;
  std::vector<PunchHand> samples;
  for(unsigned frame=0;frame<=12;++frame) {
   auto data=b.rest;body::Pose pose(data.data(),static_cast<int>(b.rig.parent.size()));
   // Passive authored torso turns are independent of physical controller speed.
   if(!physical)pose.turn(b.rig.below[b.rig.spine.front()],Quat::yaw(frame*.05f),pose.position(b.rig.spine.front()));
   Mat4 anchor{};check(animated_view::headFrame(pose.position(b.rig.head),
     pose.direction(b.rig.head,b.rig.headForward),pose.direction(b.rig.head,b.rig.headUp),
     b.rig.eyesFromHead,anchor),"punch source head missing");
   body::Targets t;t.head=t.armsOnly=t.viewMapped=t.torsoAssist=true;t.armWeight={1,1};
   const auto head=animated_view::fromNative(anchor);t.eyes=head.position;t.facing=head.orientation;
   const Vec3 raw{hand?.22f:-.22f,-.3f,physical?-.22f-.28f*frame/12.f:-.22f};
   const Quat rawOrientation=Quat::around({1,0,0},1.5707963f);
   const auto target=animated_view::resolve(anchor,{raw,rawOrientation});
   t.hands[hand]={true,target.position,target.orientation,0,0};
   const auto result=body::solve(pose,b.rig,t,config,state,true,1.f/240,1,1);
   check(result.solved,"punch upper pose missing");
   spidy::Pose actual;check(body::solvedGrip(pose,b.rig,config,hand,actual),"punch actual grip missing");
   actual.position+=Vec3{145,20,-735};const auto aim=webShooter(actual,hand);
   samples.push_back({true,aim.position+aim.orientation.rotate({0,0,-.155f}),raw,false,
                     actual.orientation*rawOrientation.conjugate(),true});
  }
  Vec3 feet=samples.back().fist;feet.y=20;feet+=samples.back().physicalToContact.rotate({0,0,-.12f});
  const std::array<PunchTarget,1> targets{{{7,feet,1.8f,.2f}}};
  Punches punches;std::vector<PunchEvent> events;
  for(const auto& sample:samples){std::array<PunchHand,2> hands{};hands[hand]=sample;
   punches.update(1.f/240,hands,targets,events);}
  if(physical)check(!events.empty(),"real swung articulated hand did not enter damage-event path");
  else check(events.empty(),"passive real articulated pose damaged target");
  // Expired/absent pose is untracked in production; reacquisition seeds history.
  events.clear();std::array<PunchHand,2> hands{};punches.update(.01f,hands,targets,events);
  hands[hand]=samples.back();hands[hand].relative.z-=.5f;
  punches.update(.01f,hands,targets,events);check(events.empty(),"pose-gap reacquisition made a stale sweep");
 }
 std::puts("PASS: both-hand actual articulated-pose physical contact events, passive-animation rejection and pose-gap history");
}

void feedbackConsumerCases() {
 unsigned cases{};
 for(auto mode : {animated_view::Frame::full, animated_view::Frame::level, animated_view::Frame::world})
  for(Vec3 root : {Vec3{145,20,-735},Vec3{-350,90,510}})
   for(float yaw : {-.9f,.7f}) {
    const Quat actor=Quat::yaw(yaw)*Quat::around({1,0,0},.12f);
    const spidy::Pose rawHead{{30,2,-40},Quat::yaw(-.4f)};
    const spidy::Pose sourceHead{{0,1.7f,0},Quat::yaw(.6f)*Quat::around({1,0,0},.2f)};
    const auto anchor=animated_view::toNative({{.2f,1.8f,-.1f},actor});
    animated_view::Targets view; view.frame=mode;
    Mat4 mapped{};
    check(animated_view::modelAnchor(view,actor,anchor,mapped,root),"feedback anchor mapping rejected");
    const auto relativeHead=animated_view::resolve(mapped,sourceHead);
    spidy::Pose head;
    check(worldDisplayedPose(head,upper_frame::rebase(relativeHead,actor),root),"mapped feedback head rejected");
    const spidy::Pose gazeInHead{{.01f,.02f,-.03f},Quat::yaw(.18f)};
    const auto rawGaze=compose(rawHead,gazeInHead);
    const auto gaze=feedback_frame::transport(rawHead,head,rawGaze);
    const auto expected=compose(head,gazeInHead);
    check(length(gaze.position-expected.position)<.0002f,"eye gaze root translated twice");
    check(dot(gaze.orientation.rotate({0,0,-1}),expected.orientation.rotate({0,0,-1}))>.9999f,
          "eye-relative gaze did not follow mapped HMD");
    GameMotionFrame basis;basis.headPose=head;basis.gazePose=gaze;
    for(unsigned hand=0;hand<2;++hand) {
     spidy::Pose relative{{hand?.3f:-.3f,1.3f,-.4f},actor};
     spidy::Pose world;
     check(worldDisplayedPose(world,relative,root),"held presentation rejected actual hand");
     basis.grips[hand]=basis.hands[hand]=world;
     for(float hold : {0.f,.5f,.5001f,1.f}) {
      spidy::Pose aim{{9,8,7},{}};const auto original=aim;
      check(mapAimToDisplayedGrip(aim,relative,root,hand,hold)==(hold<=.5f),"held physics aim policy changed");
      if(hold>.5f)check(length(aim.position-original.position)==0,"presentation changed held-web physics");
      else check(length(aim.position-webShooter(world,hand).position)<.0001f,
                 "free action and actual presentation emitter diverged");
     }
    }
    const auto before=basis;
    const spidy::Pose displayed{{root.x+2,root.y+2,root.z-3},actor*Quat::yaw(.25f)};
    const std::array<Mat4,2> eyes{animated_view::toNative(compose(displayed,{{-.032f,0,0},{}})),
                                animated_view::toNative(compose(displayed,{{ .032f,0,0},{}}))};
    feedback_frame::camera(basis,eyes);
    check(length(basis.headPose.position-displayed.position)<.0002f,"native render eye midpoint incorrect");
    const auto pivot=flip_reprojection::eyesMiddle(basis);
    check(length(pivot-displayed.position)<.0002f,"held feedback pivot stayed in raw rig");
    const Quat correction=Quat::yaw(.13f);
    const auto held=flip_reprojection::held(basis,correction);
    for(unsigned hand=0;hand<2;++hand) {
     check(length(basis.grips[hand].position-before.grips[hand].position)==0,
           "camera installation double-transformed actual hand");
     const auto expectedHand=flip_reprojection::about(before.grips[hand],correction,pivot);
     check(length(held.grips[hand].position-expectedHand.position)<.0002f,
           "held actual hand rotated about unrelated raw eyes");
    }
    const spidy::Pose widget=compose(rawHead,{{.2f,-.3f,-.4f},Quat::yaw(.3f)});
    const auto moved=feedback_frame::transport(rawHead,basis.headPose,widget);
    const auto local=compose(inverse(basis.headPose),moved);
    check(length(local.position-Vec3{.2f,-.3f,-.4f})<.0002f,"phone/wheel head-relative placement changed");
    const auto gazeAfter=compose(inverse(basis.headPose),basis.gazePose);
    check(length(gazeAfter.position-gazeInHead.position)<.0003f,"captured native camera/gaze transport drifted");
    check(animation_cancel::validPose(101,100,200,199),"current paired action lease rejected");
    check(animation_cancel::validPose(100,100,200,199),"first current-owner serial rejected");
    check(!animation_cancel::validPose(99,100,200,199),"prior-owner serial accepted");
    check(!animation_cancel::validPose(101,100,200,200),"expired feedback/action lease accepted");
    ++cases;
   }
 for(unsigned mask : {0u,1u,2u,3u}) for(bool native : {false,true}) {
  GameMotionFrame frame;for(auto& hand:frame.swing.hands)hand.tracked=true;
  feedback_frame::hands(frame,native,mask);
  for(unsigned i=0;i<2;++i)check(frame.swing.hands[i].tracked==(!native || bool(mask&(1u<<i))),
      "missing paired action pose exposed raw native feedback or changed OG tracking");
 }
 spidy::Pose unchanged{{1,2,3},{}};
 check(!worldDisplayedPose(unchanged,{{0,0,0},{0,0,0,0}},{}),"invalid presentation quaternion accepted");
 check(length(unchanged.position-Vec3{1,2,3})==0,"invalid presentation modified destination");
 std::printf("PASS: %u production feedback transports (Full/Level/World, both hands, held/free thresholds, translated/rotated roots, native camera pivot, gaze/widgets, generation/lease)\n",cases);
}

int main(){
 feedbackConsumerCases();
 upperBodyCases();
 articulatedPunchCases();
 unsigned count=0;
 for(bool odd : {false,true}) for(float weight : {.55f,1.f})
  for(Quat turn : {Quat::yaw(.6f),Quat::yaw(-1.3f)*Quat::around({1,0,0},.25f)}) {
   auto b=testBody(odd,true);auto data=b.rest;
   body::Pose pose(data.data(),static_cast<int>(b.rig.parent.size()));
   body::Targets t;t.armsOnly=true;t.viewMapped=true;t.armWeight={weight,weight};
   t.eyes=pose.position(b.rig.head);t.facing={};t.up={0,1,0};
   for(unsigned h=0;h<2;++h)t.hands[h]={true,t.eyes+Vec3{h?-.28f:.28f,-.35f,-.3f},turn,0,0};
   body::State state;body::Config config;config.hideHead=false;
   const auto solved=body::solve(pose,b.rig,t,config,state,true,1.f/90,1,1);
   check(solved.solved,"actual pose did not solve");
   for(unsigned h=0;h<2;++h){
    spidy::Pose actual;check(body::solvedGrip(pose,b.rig,config,h,actual),"actual hand axes rejected");
    const Vec3 off{h==0?-config.wristFromGrip.x:config.wristFromGrip.x,
                    config.wristFromGrip.y,config.wristFromGrip.z};
    check(length(actual.position+actual.orientation.rotate(off)-pose.position(b.rig.arms[h].hand))<1e-5f,
          "reconstructed grip is not on rendered wrist");
    if(weight==1){
     check(length(actual.position-t.hands[h].grip)<.001f,"full solved grip lost controller target");
     check(dot(actual.orientation.rotate({0,0,-1}),turn.rotate({0,0,-1}))>.9999f,
           "mirrored hand/pitch inverse changed forward axis");
    }else check(length(actual.position-t.hands[h].grip)>.001f,"partial-weight negative control equals requested grip");
    for(Vec3 feet : {Vec3{145,20,-735},Vec3{-350,90,510}}){
     const spidy::Pose world=compose({feet,Quat::yaw(1.1f)},actual);
     const auto emitter=webShooter(world,h);
     check(finite(emitter.position),"world emitter invalid");
     const Vec3 forearm=Quat::yaw(1.1f).rotate(pose.position(b.rig.arms[h].hand)-pose.position(b.rig.arms[h].lower));
     const auto clamped=web_aim::shooterAim(emitter,h,{},web_aim::forearmWorldTurn(normalized(forearm),emitter.orientation,h));
     check(length(clamped.position-emitter.position)<1e-6f,"forearm clamp moved actual emitter");
    }
    ++count;
   }
 }
 auto b=testBody();body::Pose pose(b.rest.data(),static_cast<int>(b.rig.parent.size()));
 spidy::Pose unchanged{{1,2,3},{}};check(!body::solvedGrip(pose,b.rig,{},2,unchanged),"invalid hand accepted");
 check(length(unchanged.position-Vec3{1,2,3})==0,"rejection mutated output");
 std::printf("PASS: %u actual solved-hand cases, mirrored/calibrated axes, full/partial weight, world emitters and clamp\n",count);
}
