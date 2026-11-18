// GROUND MOVES AND CANCELS (the owner, Nov 18, t116u-t119u; plans/ground-moves-and-cancel.md): the web line
// cancel into SpidyVR, intent breaks out of animation jumps, ground flips as jump types, and the ground kick's
// handover.
#include "spidy/hybrid_owner.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace spidy;

namespace {
int passed = 0, failed = 0;
void check(bool ok, const char* what) {
    if (ok) {
        ++passed;
    } else {
        ++failed;
        std::printf("FAIL: %s\n", what);
    }
}
} // namespace

int main() {
    using namespace hybrid_owner;
    const int64_t step = 11'000'000;
    // ---- S4': WEB LINE CANCEL: the safe states ----
    {
        head_probe::StateInputs s;
        s.playing = s.fresh = s.grounded = true;
        check(groundWebRefusal(s, false) == nullptr, "standing: a web may cancel");
        auto t = s;
        t.playing = false;
        check(std::strcmp(groundWebRefusal(t, false), "cutscene") == 0, "a scene: refused");
        t = s, t.fresh = false;
        check(std::strcmp(groundWebRefusal(t, false), "cutscene") == 0, "a stale sample: refused");
        t = s, t.fighting = true;
        check(std::strcmp(groundWebRefusal(t, false), "combat") == 0, "a fight: refused");
        t = s, t.dodging = true;
        check(std::strcmp(groundWebRefusal(t, false), "combat") == 0, "a dodge: refused");
        t = s, t.zipping = true;
        check(std::strcmp(groundWebRefusal(t, false), "zip") == 0, "a zip: refused");
        t = s, t.wall = surfaceWall, t.grounded = false;
        check(std::strcmp(groundWebRefusal(t, false), "wall") == 0, "Spidy's wall: refused (already SpidyVR's)");
        t = s, t.runRt = true, t.wall = surfaceGame;
        check(std::strcmp(groundWebRefusal(t, false), "parkour") == 0, "the game's mantle: refused");
        t = s, t.grounded = false;
        check(std::strcmp(groundWebRefusal(t, false), "air") == 0, "the air: the grip cancel's, not this");
        check(groundWebRefusal(t, true) == nullptr, "a perch: may cancel");
        t = s, t.grounded = false, t.wall = surfaceGame;
        check(groundWebRefusal(t, false) == nullptr, "the game's crawl (supported): may cancel");
    }
    // ---- S4': the owner takes the body on the web, holds it while the line lasts, then the ground's landing ----
    {
        HybridOwner owner;
        Frame f;
        f.state.playing = f.state.fresh = f.state.grounded = true;
        int64_t now = 1'000'000'000;
        for (int i = 0; i < 20; ++i)
            owner.update(f, now += step);
        check(owner.domain == Domain::anim, "ground: ANIM");
        f.groundWeb = true;
        owner.update(f, now += step);
        check(owner.domain == Domain::vr && owner.lastEnterVr == EnterVr::cancel && owner.enterVr,
              "a web on the ground: VR this frame (cancel)");
        f.groundWeb = false;
        f.groundWebHold = true;
        for (int i = 0; i < 40; ++i)
            owner.update(f, now += step);
        check(owner.domain == Domain::vr, "held while the line lasts");
        f.groundWebHold = false;
        owner.update(f, now += step);
        check(owner.domain == Domain::anim && owner.lastEnterAnim == EnterAnim::land, "then the ground's ANIM (land)");
        // Pulled off the ground by the line: the air's rules (VR until the landing).
        f.groundWeb = true;
        owner.update(f, now += step);
        f.groundWeb = false;
        f.state.grounded = false;
        for (int i = 0; i < 20; ++i)
            owner.update(f, now += step);
        check(owner.domain == Domain::vr, "pulled into the air: SpidyVR's air");
        f.state.grounded = true;
        owner.update(f, now += step);
        check(owner.domain == Domain::anim, "and the landing hands back");
        // Unsafe: a fight, a zip, a scene: the animation keeps him.
        for (const int which : {0, 1, 2}) {
            HybridOwner o;
            Frame g;
            g.state.playing = g.state.fresh = g.state.grounded = true;
            int64_t t = 1'000'000'000;
            for (int i = 0; i < 20; ++i)
                o.update(g, t += step);
            if (which == 0)
                g.state.fighting = true;
            if (which == 1)
                g.state.zipping = true;
            if (which == 2)
                g.state.playing = false, g.cutscene = true;
            g.groundWeb = true;
            o.update(g, t += step);
            check(o.domain == Domain::anim, which == 0 ? "a fight keeps the animation" : which == 1 ? "a zip keeps it"
                                                                                                    : "a scene keeps it");
        }
        // The hold alone (no fresh web) never takes the body from the animation.
        HybridOwner o;
        Frame g;
        g.state.playing = g.state.fresh = g.state.grounded = true;
        g.groundWebHold = true;
        int64_t t = 1'000'000'000;
        for (int i = 0; i < 20; ++i)
            o.update(g, t += step);
        check(o.domain == Domain::anim, "a hold without the web: ANIM");
        // A perch: the web takes it too.
        HybridOwner p;
        Frame h;
        h.state.playing = h.state.fresh = true;
        h.perched = true;
        t = 1'000'000'000;
        for (int i = 0; i < 20; ++i)
            p.update(h, t += step);
        check(p.domain == Domain::anim, "a perch: ANIM");
        h.groundWeb = true;
        p.update(h, t += step);
        check(p.domain == Domain::vr, "a web on the perch: VR");
    }
    // ---- S1: INTENT BREAKS: breakForMove takes a midair animation on the frame, never the ground ----
    {
        HybridOwner owner;
        Frame f;
        f.state.playing = f.state.fresh = true;
        int64_t now = 1'000'000'000;
        f.state.grounded = true;
        for (int i = 0; i < 10; ++i)
            owner.update(f, now += step);
        head_probe::StateInputs ground = f.state;
        check(!owner.breakForMove(ground, false, now), "on the ground: no move break");
        f.state.grounded = false; // the game's jump
        f.velY = 5;
        owner.update(f, now += step);
        check(owner.domain == Domain::anim, "a jump: the animation's");
        head_probe::StateInputs air = f.state;
        check(!owner.breakForMove(air, true, now), "a scene: no move break");
        check(owner.breakForMove(air, false, now += step) && owner.domain == Domain::vr, "in the air: VR at once");
        owner.update(f, now += step);
        check(owner.enterVr && owner.lastEnterVr == EnterVr::move, "the frame's switch is the move break's");
        check(std::strcmp(enterVrName(EnterVr::move), "move-break") == 0, "named move-break");
        check(viewEaseMs(EnterVr::move, 250, 200) == 200, "AIR VIEW EASE, as a flip break");
        check(!owner.breakForMove(air, false, now += step), "already VR: nothing");
        head_probe::StateInputs wall = air;
        wall.wall = surfaceWall;
        HybridOwner o2;
        check(!o2.breakForMove(wall, false, now), "Spidy's wall: nothing");
    }
    std::printf("ground moves: %d checks passed, %d failed\n", passed, failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
