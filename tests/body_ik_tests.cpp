// BODY IK, the arms (beta polish SA1-SA6): Showtime arms at the hero's own length by bone length, the elbow's bend
// side held against flips, its range, and the forearm's share of the hand's twist.
#include "spidy/body_ik.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace spidy;
static int failures = 0;
static void check(bool b, const char* m) {
    if (!b) {
        std::fprintf(stderr, "FAIL: %s\n", m);
        ++failures;
    }
}
static void must(bool b, const char* m) {
    if (!b) {
        std::fprintf(stderr, "FAIL: %s\n", m);
        std::exit(1);
    }
}
#define check must
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
    must(body::prepare(r,b.rest),"the test rig was rejected");
    return b;
}

#undef check
static float dist(Vec3 a, Vec3 b) { return length(a - b); }

// One armsOnly frame on a fresh rest pose (the game hands the solver its own pose each frame); returns the pose.
struct Frame {
    std::vector<float> data;
    body::Result result;
};
static Frame overlay(const TestBody& b, body::State& state, Vec3 leftHand, float scale, float arms, float hero,
                     float dt = 1.f / 90) {
    Frame f;
    f.data = b.rest;
    body::Pose pose(f.data.data(), static_cast<int>(b.rig.parent.size()));
    body::Targets t;
    t.armsOnly = t.viewMapped = true;
    t.armWeight = {1, 0};
    t.heroArms = hero;
    t.eyes = {0, 1.66f, 0};
    t.facing = Quat::yaw(3.14159265f);
    t.hands[0] = {true, leftHand, Quat{}, 0, 0};
    body::Config config;
    config.hideHead = false;
    config.handOrientation = false;
    f.result = body::solve(pose, b.rig, t, config, state, true, dt, scale, arms);
    return f;
}

static void boneLength() {
    const auto b = testBody();
    const auto& arm = b.rig.arms[0];
    const float upper = dist(b.at(b.rest, arm.upper), b.at(b.rest, arm.lower));
    const float fore = dist(b.at(b.rest, arm.lower), b.at(b.rest, arm.hand));
    const float palm = dist(b.at(b.rest, arm.hand), b.at(b.rest, 13));
    body::State state;
    const auto f = overlay(b, state, {.5f, 1.25f, .3f}, 1.f, 1.2f, 0.f);
    must(f.result.solved, "the arm overlay did not solve");
    check(std::abs(state.armFactor[0] - 1.2f) < .01f, "the overlay arm factor is not the asked 1.2");
    check(std::abs(dist(b.at(f.data, arm.upper), b.at(f.data, arm.lower)) - upper * 1.2f) < .003f,
          "SA1: the upper arm is not 1.2x along its bone");
    check(std::abs(dist(b.at(f.data, arm.lower), b.at(f.data, arm.hand)) - fore * 1.2f) < .003f,
          "SA1: the forearm is not 1.2x along its bone");
    check(std::abs(dist(b.at(f.data, arm.hand), b.at(f.data, 13)) - palm) < .002f,
          "SA1: the hand grew with the arm (a uniform scale, not bone length)");
}

static void heroArms() {
    const auto b = testBody();
    const auto& arm = b.rig.arms[0];
    const float fore = dist(b.at(b.rest, arm.lower), b.at(b.rest, arm.hand));
    body::State state;
    auto f = overlay(b, state, {.5f, 1.25f, .3f}, 1.1f, 1.2f, 1.f);
    must(f.result.solved, "the hero-arms overlay did not solve");
    check(std::abs(state.armFactor[0] - 1.f) < .005f, "D4: from outside the overlay arm is not 1.0x");
    check(std::abs(dist(b.at(f.data, arm.lower), b.at(f.data, arm.hand)) - fore) < .003f,
          "D4: the forearm is not its own length from outside");
    body::State half;
    f = overlay(b, half, {.5f, 1.25f, .3f}, 1.1f, 1.2f, .5f);
    const float wanted = 1.f + (std::clamp(1.1f * 1.2f, .75f, 1.5f) - 1.f) * .5f;
    check(std::abs(half.armFactor[0] - wanted) < .01f, "D4: halfway out the factor is not halfway to 1.0x");
    body::State none;
    f = overlay(b, none, {.5f, 1.25f, .3f}, 1.1f, 1.2f, 0.f);
    check(std::abs(none.armFactor[0] - 1.32f) < .01f, "D4: in the headset the player's reach changed");
}

static void elbowRange() {
    const auto b = testBody();
    const auto& arm = b.rig.arms[0];
    const float a = dist(b.at(b.rest, arm.upper), b.at(b.rest, arm.lower));
    const float c = dist(b.at(b.rest, arm.lower), b.at(b.rest, arm.hand));
    body::State state;
    // Out of reach: straight but not past it (SA3).
    auto f = overlay(b, state, {1.6f, 1.46f, .2f}, 1.f, 1.f, 0.f);
    const float reach = dist(b.at(f.data, arm.upper), b.at(f.data, arm.hand));
    check(reach <= (a + c) * .9991f, "SA3: an out-of-reach hand hyperextended the arm");
    check(state.elbow[0].degrees > 150.f && state.elbow[0].degrees <= 180.f, "SA3: a reaching elbow is not near straight");
    // Onto the shoulder: folded (no fold limit pulls the hand off its controller), finite.
    body::State fold;
    f = overlay(b, fold, b.at(b.rest, arm.upper) + Vec3{0, -.02f, .12f}, 1.f, 1.f, 0.f);
    check(fold.elbow[0].degrees < 60.f, "SA3: a hand at the shoulder did not fold the elbow");
    for (const float v : f.data)
        if (!std::isfinite(v)) {
            check(false, "SA3: a folded arm left a NaN in the pose");
            break;
        }
}

static void elbowHold() {
    const auto b = testBody();
    const auto& arm = b.rig.arms[0];
    const Vec3 hand{.45f, 1.2f, .35f};
    body::State state;
    auto f = overlay(b, state, hand, 1.f, 1.f, 0.f);
    const Vec3 natural = state.elbow[0].bend;
    must(length(natural) > .5f, "no elbow bend side recorded");
    const Vec3 elbowNatural = b.at(f.data, arm.lower);
    // The last frame's bend on the far side: the next frame keeps it (held), for under 80 ms.
    state.elbow[0].bend = natural * -1.f;
    state.elbow[0].away = 0;
    const unsigned heldWas = state.elbow[0].held;
    f = overlay(b, state, hand, 1.f, 1.f, 0.f, .02f);
    check(state.elbow[0].held == heldWas + 1, "SA2: a sudden bend-side change was not held");
    check(dot(state.elbow[0].bend, natural) < 0, "SA2: the held bend side was not kept");
    check(dist(b.at(f.data, arm.lower), elbowNatural) > .05f, "SA2: the elbow did not stay on its held side");
    // Wanted for 80 ms or more: it goes over, counted as a flip.
    const unsigned flipsWas = state.elbow[0].flips;
    for (int i = 0; i < 5; ++i)
        f = overlay(b, state, hand, 1.f, 1.f, 0.f, .02f);
    check(state.elbow[0].flips == flipsWas + 1, "SA2: a lasting bend-side change was not taken (and counted once)");
    check(dot(state.elbow[0].bend, natural) > .9f, "SA2: the elbow did not come back to its pole's side");
    check(dist(b.at(f.data, arm.lower), elbowNatural) < .01f, "SA2: the elbow did not settle where it solves");
    // A small change (under 25 degrees) is never held.
    const unsigned heldNow = state.elbow[0].held;
    f = overlay(b, state, hand + Vec3{0, .03f, 0}, 1.f, 1.f, 0.f);
    check(state.elbow[0].held == heldNow, "SA2: a small bend change was held");
}

static void forearmShare() {
    body::Config c;
    check(std::abs(c.forearmRoll - .3f) < 1e-6f, "SA4: the forearm takes other than 0.3 of the hand's twist");
}

int main() {
    boneLength();
    heroArms();
    elbowRange();
    elbowHold();
    forearmShare();
    if (failures) {
        std::fprintf(stderr, "%d body_ik check(s) failed\n", failures);
        return 1;
    }
    std::puts("body_ik tests passed");
    return 0;
}
