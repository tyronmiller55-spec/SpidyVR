// HOOK CHAIN (hook_chain.hpp, November 18, pt121u): another mod's hook at a
// function Spidy hooks is gone in front of, not a reason to drop the SPIDY VR
// tab and the button prompts (the sky mod's winmm.dll at 0x1d1cf30: 9301).
// Jumps decoded, classified against the game's image, and the plan that keeps
// the prompts' hooks when the tab cannot go in. Portable.
#include "spidy/hook_chain.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spidy::hook_chain;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr uintptr_t game = 0x140000000, gameEnd = game + 0x4000000, at = game + 0x1d1cf30;
} // namespace

int main() {
    // The supported game's own bytes at the call wrapper: no jump.
    const unsigned char original[16] = {0x80, 0xb9, 0xb0, 0, 0, 0, 0, 0x75, 0x03};
    check(decode(original, at).kind == Kind::none, "the game's own code is no jump");
    check(classify(true, decode(original, at), game, gameEnd) == Found::original, "matching bytes: original");
    check(classify(false, decode(original, at), game, gameEnd) == Found::unknown, "other bytes, no jump: unknown");

    // MinHook's E9 to its relay, within 2 GB below the game.
    const unsigned char rel32[16] = {0xe9, 0x00, 0x00, 0x00, 0xf0}; // rel -0x10000000
    const auto r = decode(rel32, at);
    check(r.kind == Kind::rel32 && r.length == 5, "E9 decoded");
    check(r.dest == at + 5 - 0x10000000, "E9 destination");
    check(classify(false, r, game, gameEnd) == Found::chained, "E9 out of the game: chained");
    // An E9 into the game's own image: a patch or a cave, not a mod's hook.
    const unsigned char inside[16] = {0xe9, 0x10, 0x00, 0x00, 0x00};
    check(classify(false, decode(inside, at), game, gameEnd) == Found::unknown, "E9 inside the game: unknown");

    // FF 25: through a pointer; the caller reads it through.
    const unsigned char pointer[16] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};
    auto p = decode(pointer, at);
    check(p.kind == Kind::pointer && p.dest == at + 6 && p.length == 6, "FF 25 decoded to its pointer");
    p.dest = 0x7ff800001000; // read through: a library
    check(classify(false, p, game, gameEnd) == Found::chained, "FF 25 to a library: chained");
    p.dest = game + 0x100;
    check(classify(false, p, game, gameEnd) == Found::unknown, "FF 25 into the game: unknown");

    // mov rax, imm64; jmp rax.
    const unsigned char movRax[16] = {0x48, 0xb8, 0x00, 0x10, 0x00, 0x00, 0xf8, 0x7f, 0x00, 0x00, 0xff, 0xe0};
    const auto m = decode(movRax, at);
    check(m.kind == Kind::movRax && m.dest == 0x7ff800001000 && m.length == 12, "mov rax/jmp rax decoded");
    check(classify(false, m, game, gameEnd) == Found::chained, "mov rax/jmp rax: chained");

    // EB: too short to hook over.
    const unsigned char rel8[16] = {0xeb, 0x10};
    check(decode(rel8, at).kind == Kind::rel8, "EB decoded");
    check(classify(false, decode(rel8, at), game, gameEnd) == Found::unknown, "EB: unknown");

    // The plan: hook 0 the call wrapper (prompts), 1 a tab hook, 2 the prompt show.
    const bool menuOnly[3] = {false, true, false};
    {
        const Found f[3] = {Found::original, Found::original, Found::original};
        const auto pl = plan(f, menuOnly, 3);
        check(pl.menu && pl.use == 7, "all original: tab and every hook");
    }
    {
        // November 18: the call wrapper hooked by the sky mod.
        const Found f[3] = {Found::chained, Found::original, Found::original};
        const auto pl = plan(f, menuOnly, 3);
        check(pl.menu && pl.use == 7, "another mod's hook gone in front of: the tab stays");
    }
    {
        const Found f[3] = {Found::original, Found::unknown, Found::original};
        const auto pl = plan(f, menuOnly, 3);
        check(!pl.menu && pl.use == 5, "a tab hook unknown: no tab, the prompts' hooks still in");
    }
    {
        const Found f[3] = {Found::unknown, Found::original, Found::original};
        const auto pl = plan(f, menuOnly, 3);
        check(!pl.menu && pl.use == 4, "the call wrapper unknown: no tab, the prompt show still in");
    }
    std::printf("hook chain: %d checks passed\n", passed);
    return 0;
}
