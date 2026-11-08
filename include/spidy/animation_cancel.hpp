#pragma once
#include <cstdint>
namespace spidy::animation_cancel {
struct Decision {
    bool cancel{}, holdVr{};
    unsigned webBlocked{}, shotBlocked{};
};
// Consume the existing web-hold gesture before any swing/grab/shot command.
// A cancel is not a web press. Both hands release before ordinary webs rearm.
struct Gate {
    unsigned was{}, blockedWeb{}, blockedShot{};
    bool holding{};
    Decision update(bool enabled, bool animOwns, bool focused, unsigned web, unsigned shot,
                    unsigned tracked = 3) {
        web &= 3; shot &= 3; tracked &= 3;
        if (!focused) { was = web; return {false, holding, blockedWeb, blockedShot}; }
        const unsigned rising = web & ~was & tracked;
        was = (was & ~tracked) | (web & tracked);
        blockedWeb &= web | (3 & ~tracked);
        blockedShot &= shot | (3 & ~tracked);
        holding = holding && blockedWeb != 0;
        const bool cancel = enabled && animOwns && rising && !holding;
        if (cancel) { holding = true; blockedShot |= shot; }
        if (holding) {
            blockedWeb |= web; // another hand pressed during cancel also waits for release
        }
        return {cancel, holding, blockedWeb, cancel ? 3u : blockedShot};
    }
};
// Contact-only policy is decided before the later body-owner transaction.
// In particular a supported release must not get one original-VR lunge sample.
inline bool nativeActions(bool enabled, bool yielding, bool cancelHeld, bool supported) {
    return enabled && !cancelHeld && (yielding || supported);
}
template<class Hands> inline void suppress(Hands& hands, Decision decision) {
    for (unsigned i = 0; i < 2; ++i) {
        if (decision.webBlocked & (1u << i)) hands[i].grip = 0;
        if (decision.shotBlocked & (1u << i)) hands[i].trigger = 0;
    }
}
inline bool validPose(uint64_t serial, uint64_t ownerFloor, uint64_t deadline, uint64_t now) {
    return serial >= ownerFloor && serial != 0 && now < deadline;
}
// Revoke copied dispatch batches and late enqueues from an earlier input.
// Serial 0 is an independent gadget command: still fenced by generation.
inline bool validAction(uint64_t serial, uint64_t generation, uint64_t current, uint64_t floor) {
    return generation == current && (!serial || serial >= floor);
}
// The dispatch algorithm is shared by the native SRW adapter and portable
// concurrency tests. A copied batch must acquire this lease and validate at
// dispatch, not rely on its earlier enqueue check. No queue/output lock is held
// on entry; revocation takes the same lock exclusively before clearing queues.
struct ActionEpoch { uint64_t generation{}, floor{}; };
template<class Mutex, bool Shared> struct ActionLease {
    Mutex& mutex;
    explicit ActionLease(Mutex& m) : mutex(m) {
        if constexpr (Shared) mutex.lock_shared(); else mutex.lock();
    }
    ~ActionLease() {
        if constexpr (Shared) mutex.unlock_shared(); else mutex.unlock();
    }
    ActionLease(const ActionLease&) = delete;
    ActionLease& operator=(const ActionLease&) = delete;
};
template<class Mutex, class Current, class Dispatch>
inline bool dispatchAction(Mutex& mutex, uint64_t serial, uint64_t generation,
                           Current current, Dispatch dispatch) {
    ActionLease<Mutex, true> lease(mutex);
    const auto epoch = current();
    return validAction(serial, generation, epoch.generation, epoch.floor) && dispatch();
}
template<class Mutex, class Revoke> inline void revokeActions(Mutex& mutex, Revoke revoke) {
    ActionLease<Mutex, false> lease(mutex);
    revoke();
}
// Commands repeat until sampled; the token is persistent, not a one-frame bit.
inline bool consumeToken(uint32_t token, uint32_t& seen) {
    if (!token || token == seen) return false;
    seen = token;
    return true;
}
} // namespace spidy::animation_cancel
