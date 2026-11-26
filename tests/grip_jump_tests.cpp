// GRIP LOCKS THE WALL JUMP (grip_jump.hpp, beta3o): a grip on SpidyVR's wall holds A back until let go of; a
// press begun unlocked goes on; the switch's default and its revision.
#include "spidy/grip_jump.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy::grip_jump;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
Hand hand(float squeeze, bool webbed = false, bool tracked = true) {
    Hand h;
    h.tracked = tracked;
    h.squeeze = squeeze;
    h.webbed = webbed;
    return h;
}
} // namespace

int main() {
    // What counts as a grip on the wall.
    check(wallGripHeld(true, hand(.9f), hand(0)), "the left grip squeezed on the wall");
    check(wallGripHeld(true, hand(0), hand(.7f)), "the right");
    check(!wallGripHeld(true, hand(.6f), hand(.5f)), "a light squeeze (0.65 or under) is no grip");
    check(!wallGripHeld(false, hand(1), hand(1)), "off SpidyVR's wall: no lock");
    check(!wallGripHeld(true, hand(1, true), hand(0)), "a hand holding a web: its grip is the web's");
    check(!wallGripHeld(true, hand(1, false, false), hand(0)), "an untracked hand");
    check(!wallGripHeld(true, hand(NAN), hand(0)), "a bad squeeze");

    // A press while locked is swallowed until let go of.
    {
        Lock l;
        auto o = l.update(true, true);
        check(!o.a && o.lockedNow, "A with the grip held: no jump");
        o = l.update(true, false);
        check(!o.a && !o.lockedNow, "the grip let go mid-press: still no jump (no jump on the release)");
        o = l.update(false, false);
        check(!o.a && !l.swallowing(), "A let go");
        o = l.update(true, false);
        check(o.a, "a new press with no grip: the wall jump, as before");
    }
    // A press begun unlocked goes on even if a grip closes during it.
    {
        Lock l;
        check(l.update(true, false).a, "unlocked: A");
        check(l.update(true, true).a, "a grip closing mid-press does not cut the press");
        l.update(false, true);
        check(!l.update(true, true).a, "the next press with the grip: held back");
    }
    // Off: the caller never locks; A is A.
    {
        Lock l;
        bool all = true;
        for (int i = 0; i < 10; ++i)
            all = all && l.update(i % 3 != 0, false).a == (i % 3 != 0);
        check(all, "never locked: A passes through");
    }
    check(spidy::vr_settings::Values{}.wallGripJumpLock, "on by default");
    check(!spidy::vr_settings::withTabOnly({}, "wall_grip_jump_lock=0\n").wallGripJumpLock, "the key reads back");
    std::printf("grip jump: %d checks passed\n", passed);
    return 0;
}
