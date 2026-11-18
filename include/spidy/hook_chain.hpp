#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Sharing a game function with another mod (docs/MOD_COMPAT.md). Spidy hooks
// a function only where its first bytes are the supported game's; another
// mod that hooked it first has put a jump there instead, and November 18's
// sky mod (a winmm.dll loader in the game's folder) did just that to the UI's
// call wrapper 0x1d1cf30: SPIDY VR's whole tab and every button prompt went
// with it (9301, prompts.invokes 0 from 21,645 the day before), and with the
// prompts the X that sends the game's LB + RB to throw what the game names.
// A function that starts with a jump out of the game's own code is another
// mod's: Spidy's hook goes in front of it and passes every call on through
// it (MinHook copies the jump into Spidy's trampoline), so both mods see the
// call. Anything else at a hook is code Spidy does not know: that hook alone
// stays out. Portable: the decoding takes bytes, not memory.
namespace spidy::hook_chain {
enum class Kind {
    none,
    rel32,   // E9 rel32: a jump within 2 GB (MinHook's, to its relay near the target)
    rel8,    // EB rel8: a short jump (too short to hook over: MinHook needs five bytes)
    pointer, // FF 25 disp32: a jump through the pointer at `dest` (the pointer's own address)
    movRax,  // 48 B8 imm64 / FF E0: mov rax, the detour; jmp rax
};
struct Jump {
    Kind kind = Kind::none;
    // Where it goes; for `pointer`, where the pointer to where it goes is.
    uintptr_t dest{};
    size_t length{};
};
// The jump `bytes` (at least 12 of them, read at `at`) start with, if any.
inline Jump decode(const unsigned char* bytes, uintptr_t at) {
    Jump j;
    if (bytes[0] == 0xe9) {
        int32_t rel;
        std::memcpy(&rel, bytes + 1, 4);
        j = {Kind::rel32, at + 5 + static_cast<intptr_t>(rel), 5};
    } else if (bytes[0] == 0xeb) {
        j = {Kind::rel8, at + 2 + static_cast<int8_t>(bytes[1]), 2};
    } else if (bytes[0] == 0xff && bytes[1] == 0x25) {
        int32_t rel;
        std::memcpy(&rel, bytes + 2, 4);
        j = {Kind::pointer, at + 6 + static_cast<intptr_t>(rel), 6};
    } else if (bytes[0] == 0x48 && bytes[1] == 0xb8 && bytes[10] == 0xff && bytes[11] == 0xe0) {
        uint64_t to;
        std::memcpy(&to, bytes + 2, 8);
        j = {Kind::movRax, static_cast<uintptr_t>(to), 12};
    }
    return j;
}
// What is at a hook: the game's own code, another mod's hook to go in front
// of, or code Spidy does not know (that hook stays out).
enum class Found { original, chained, unknown };
// matches: the first bytes are the supported game's. jump: decode() of them,
// a `pointer` one's dest already read through to where it leads.
// gameStart, gameEnd: the game's image. A jump into the game's own image is
// not a hook (a patched function, a code cave): unknown.
inline Found classify(bool matches, const Jump& jump, uintptr_t gameStart, uintptr_t gameEnd) {
    if (matches)
        return Found::original;
    if (jump.kind == Kind::rel32 || jump.kind == Kind::pointer || jump.kind == Kind::movRax) {
        return jump.dest >= gameStart && jump.dest < gameEnd ? Found::unknown : Found::chained;
    }
    return Found::unknown;
}
// What goes in, of hooks that serve the tab only (`menuOnly`) and hooks the
// prompts need too (the call wrapper, the text lookup, the HUD prompt list's
// show and hide): the tab only with every hook usable; the prompts' each on
// its own.
struct Plan {
    bool menu{};
    uint32_t use{}; // bit i: hook i goes in
};
inline Plan plan(const Found* found, const bool* menuOnly, size_t count) {
    Plan p;
    p.menu = true;
    for (size_t i = 0; i < count; ++i)
        if (found[i] == Found::unknown)
            p.menu = false;
    for (size_t i = 0; i < count && i < 32; ++i)
        if (found[i] != Found::unknown && (p.menu || !menuOnly[i]))
            p.use |= 1u << i;
    return p;
}
} // namespace spidy::hook_chain
