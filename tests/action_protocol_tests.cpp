#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include "spidy/animation_cancel.hpp"
#include "spidy/game_swing.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <latch>
#include <shared_mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif
using namespace spidy;
namespace {
#ifdef _WIN32
// Windows CI exercises the same SRW operations as the production adapter.
struct TestMutex {
    SRWLOCK lock_ = SRWLOCK_INIT;
    void lock_shared(){AcquireSRWLockShared(&lock_);}
    void unlock_shared(){ReleaseSRWLockShared(&lock_);}
    void lock(){AcquireSRWLockExclusive(&lock_);}
    void unlock(){ReleaseSRWLockExclusive(&lock_);}
    bool try_lock(){return TryAcquireSRWLockExclusive(&lock_)!=0;}
};
#else
using TestMutex=std::shared_mutex;
#endif
void check(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
template<class T> void put(std::vector<unsigned char>& wire,size_t offset,T value) {
    check(offset+sizeof(value)<=wire.size(),"fixture write past boundary");
    std::memcpy(wire.data()+offset,&value,sizeof(value));
}
bool decode(game_swing::Command& out,const std::vector<unsigned char>& wire,size_t* maxRead=nullptr) {
    return game_swing::decodeCommand(out,[&](void* to,size_t bytes){
        if(maxRead) *maxRead=std::max(*maxRead,bytes);
        if(bytes>wire.size()) return false;
        std::memcpy(to,wire.data(),bytes); return true;
    });
}
std::vector<unsigned char> legacy(uint32_t version) {
    // Independently fixed wire offsets, not sizeof/offsetof from the new host.
    const uint32_t bytes[]={0,0,168,184,200,240,272};
    std::vector<unsigned char> w(bytes[version],0);
    put(w,0,uint32_t{0x5357434d}); put(w,4,version);put(w,8,bytes[version]);
    put(w,12,uint32_t{1}); put(w,16,uint64_t{0x1020304050607080});
    put(w,24,uint32_t{100});put(w,28,uint32_t{1});
    put(w,32,123.f);put(w,56,1.f);put(w,60,-.25f);put(w,72,uint32_t{1});put(w,76,.8f);
    put(w,84,-456.f);put(w,108,1.f);put(w,112,.25f);put(w,124,uint32_t{1});put(w,132,.9f);
    put(w,136,.3f);put(w,148,.4f);put(w,152,1.f/90);put(w,156,uint32_t{8});
    put(w,160,uint64_t{987654321});
    if(version>=3)put(w,180,1.f);
    if(version>=4){put(w,184,.7f);put(w,196,-1.f);}
    if(version>=5){put(w,200,17.f);put(w,224,.8f);put(w,232,uint32_t{1});put(w,236,uint32_t{0xfafafafa});}
    if(version>=6){put(w,240,4.f);put(w,260,-1.f);put(w,264,uint32_t{1});put(w,268,uint32_t{0xdeadcafe});}
    return w;
}
void protocol() {
    using game_swing::Command;
    static_assert(sizeof(Command)==304 && offsetof(Command,physicalToContact)==272);
    static_assert(offsetof(Command,hands)==32 && offsetof(Command,tilt)==168 &&
        offsetof(Command,leap)==184 && offsetof(Command,wallPositions)==200 &&
        offsetof(Command,gazeOrigin)==240 && offsetof(Command,eyeAim)==264);
    for(uint32_t version=2;version<=6;++version) {
        auto w=legacy(version);Command out;size_t maxRead{};
        check(decode(out,w,&maxRead),"legacy golden packet rejected");
        check(maxRead==w.size(),"module attempted reading new tail from a legacy sender");
        check(out.serial==0x1020304050607080 && out.jump==1 && out.hands[0].aim.position.x==123.f &&
            out.hands[1].aim.position.x==-456.f && out.hands[0].tracked==1 && out.hands[1].grip==.9f,
            "legacy prefix offsets changed");
        check(out.actionMode==0 && out.animationCancel==0 && out.physicalToContact[0].w==1 &&
            out.physicalToContact[1].w==1,"old padding/reserved bytes acquired new action semantics");
        check((version>=4 ? out.leap==.7f : out.leap==0) &&
            (version>=6 ? out.eyeAim==1 && out.gaze.z==-1 : out.eyeAim==0 && out.gaze.z==0),
            "legacy omitted fields lost defaults or existing fields changed");
        // Every truncated payload, including header truncation, must reject and
        // leave the caller's command unchanged.
        for(size_t size=0;size<w.size();++size) {
            auto shortWire=w;shortWire.resize(size);out.serial=77;
            check(!decode(out,shortWire) && out.serial==77,"truncated legacy packet accepted/mutated output");
        }
    }
    Command host;host.serial=99;host.focused=1;host.animationCancel=17;host.actionMode=7;
    host.hands[0].tracked=host.hands[1].tracked=1;
    host.hands[0].aim={{100,4,-300},Quat::yaw(.4f)};
    host.hands[1].aim={{99,4,-301},Quat::yaw(-.7f)};
    host.physicalToContact[0]=Quat::yaw(1.2f);host.physicalToContact[1]=Quat::yaw(-.8f);
    std::vector<unsigned char> wire(sizeof(host));std::memcpy(wire.data(),&host,sizeof(host));
    check(wire.size()==304 && wire[4]==7 && wire[8]==0x30 && wire[9]==1,"host v7 wire header mismatch");
    Command module;check(decode(module,wire),"actual module decoder rejected host v7");
    check(module.version==7 && module.bytes==304 && module.serial==host.serial &&
        module.animationCancel==17 && module.actionMode==7 &&
        module.physicalToContact[0].y==host.physicalToContact[0].y &&
        module.physicalToContact[1].y==host.physicalToContact[1].y &&
        module.hands[1].aim.position.z==-301,"host/module roundtrip lost action fields or mirrored hand");
    const auto swingInput=game_swing::input(module);
    check(swingInput.hands[0].aim.position.x==100 && swingInput.hands[1].aim.position.z==-301 &&
        swingInput.hands[0].tracked && swingInput.hands[1].tracked,"module Input adapter changed world positions or mirrored hands");
    auto oldModule=[&](const auto& w){uint32_t header[3];std::memcpy(header,w.data(),12);
        return header[1]==6 && header[2]==272;};
    check(!oldModule(wire),"old module unexpectedly accepts new packet; paired modules required");
    for(size_t n=0;n<wire.size();++n){auto bad=wire;bad.resize(n);check(!decode(module,bad),"truncated v7 accepted");}
    for(const auto [version,bytes]:std::array<std::array<uint32_t,2>,5>{{{7,272},{6,304},{8,304},{7,305},{0,304}}}){
        auto bad=wire;put(bad,4,version);put(bad,8,bytes);check(!decode(module,bad),"mismatched version/size accepted");
    }
    auto bad=wire;put(bad,236,uint32_t{8});check(!decode(module,bad),"unknown action bit accepted");
    bad=wire;put(bad,272,float{NAN});check(!decode(module,bad),"nonfinite contact rotation accepted");
    bad=wire;for(size_t offset=272;offset<288;offset+=4)put(bad,offset,0.f);
    check(!decode(module,bad),"zero contact rotation accepted");
    bad=wire;put(bad,0,uint32_t{0});check(!decode(module,bad),"wrong magic accepted");
    // Legacy probes using v2 remain valid; new XR requires paired new rays.
    std::puts("PASS: actual ingress host v7/304 roundtrip; golden v2-v6; all truncations, reserved bytes and malformed packets");
}
void dispatchConcurrency() {
    using namespace animation_cancel;
    TestMutex lease;std::atomic<uint64_t> generation{1},floor{0};
    const auto current=[&]{return ActionEpoch{generation.load(),floor.load()};};
    std::promise<void> inEngine,releaseEngine,cancelAttempt;auto release=releaseEngine.get_future().share();
    std::atomic<unsigned> calls{};
    std::thread dispatch([&]{check(dispatchAction(lease,9,1,current,[&]{
        ++calls;inEngine.set_value();release.wait();return true;
    }),"in-flight callback rejected");});
    inEngine.get_future().wait();
    auto cancel=std::async(std::launch::async,[&]{cancelAttempt.set_value();revokeActions(lease,[&]{floor=10;++generation;});});
    cancelAttempt.get_future().wait();
    check(cancel.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"revocation bypassed an active engine lease");
    releaseEngine.set_value();dispatch.join();cancel.get();
    const auto engine=[&]{++calls;return true;};
    check(!dispatchAction(lease,9,1,current,engine),"copied old-generation batch dispatched after cancel");
    check(!dispatchAction(lease,9,2,current,engine),"late old-input enqueue dispatched with new generation");
    check(!dispatchAction(lease,0,1,current,engine),"old independent gadget command survived cancel");
    check(dispatchAction(lease,10,2,current,engine) && calls==2,"new action or actual engine call count incorrect");
    // RAII must release the real algorithm's lease if a C++ callback throws.
    try {dispatchAction(lease,10,2,current,[]{throw 7;return true;});}catch(int){}
    check(lease.try_lock(),"exception leaked dispatch lease");lease.unlock();
    std::atomic<unsigned> errors{},accepted{};std::latch start(5);
    std::array<std::thread,4> workers;
    for(auto& worker:workers)worker=std::thread([&]{start.arrive_and_wait();for(unsigned n=0;n<3000;++n){
        const auto g=generation.load();const auto serial=floor.load();
        dispatchAction(lease,serial,g,current,[&]{
            if(g!=generation.load() || serial<floor.load())++errors;
            ++accepted;return true;
        });
    }});
    start.arrive_and_wait();
    for(unsigned n=0;n<250;++n)revokeActions(lease,[&]{floor.fetch_add(1);++generation;});
    for(auto& worker:workers)worker.join();
    check(errors==0,"concurrent native-dispatch callback ran with a revoked lease");
    std::printf("PASS: actual dispatch/revoke algorithm, blocked in-flight call, stale batches, exception cleanup; 12000 attempts/250 revocations (%u admitted)\n",accepted.load());
}
}
int main(){protocol();dispatchConcurrency();}
