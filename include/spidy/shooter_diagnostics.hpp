#pragma once
#include "math.hpp"
#include <cstdint>
#include <cstddef>
namespace spidy::shooter_diagnostics {
// Observation only: a returned object is not evidence of visibility or damage;
// no returned object is not evidence that the engine drew no projectile.
enum class Outcome : uint32_t { none, expired, rejected, callOnly, objectReturned };
struct Data {
    uint64_t queued{}, queueFull{}, expired{}, callSuccess{}, rejected{}, objectKnown{}, objectUnknown{};
    uint64_t gadgetCalls{}, fallbackCalls{};
    uint64_t target{};
    Vec3 origin{}, direction{}, aimPoint{}; // final shifted muzzle passed to the engine
    uint32_t route{}, outcome{}, hand{}, reserved{}; // route: 0 none, 1 gadget, 2 fallback
};
static_assert(sizeof(Data) == 136 && offsetof(Data, target) == 72 &&
              offsetof(Data, origin) == 80 && offsetof(Data, direction) == 92 &&
              offsetof(Data, aimPoint) == 104 && offsetof(Data, route) == 116 &&
              offsetof(Data, outcome) == 120 && offsetof(Data, hand) == 124);
inline Outcome classify(bool expired, bool callSuccess, bool objectReturned) {
    if (expired) return Outcome::expired;
    if (!callSuccess) return Outcome::rejected;
    return objectReturned ? Outcome::objectReturned : Outcome::callOnly;
}
inline void record(Data& d, bool expired, bool callSuccess, bool objectReturned) {
    const auto out = classify(expired, callSuccess, objectReturned);
    d.outcome = static_cast<uint32_t>(out);
    d.expired += out == Outcome::expired;
    d.rejected += out == Outcome::rejected;
    d.callSuccess += out == Outcome::callOnly || out == Outcome::objectReturned;
    d.objectUnknown += out == Outcome::callOnly;
    d.objectKnown += out == Outcome::objectReturned;
}
} // namespace spidy::shooter_diagnostics
