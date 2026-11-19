// GROUND MOVES AND CANCELS (the owner, Nov 18, t116u-t119u; plans/ground-moves-and-cancel.md): the web line
// cancel into SpidyVR, intent breaks out of animation jumps, ground flips as jump types, and the ground kick's
// handover.
#include "spidy/game_tracking.hpp"
#include "spidy/ground_flip.hpp"
#include "spidy/hybrid_owner.hpp"
#include <cmath>
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
        check(groundWebRefusal(t, false) == nullptr, "a zip: cut at once (the owner, Nov 18)");
        t = s, t.wall = surfaceWall, t.grounded = false;
        check(std::strcmp(groundWebRefusal(t, false), "wall") == 0, "Spidy's wall: refused (already SpidyVR's)");
        t = s, t.runRt = true, t.wall = surfaceGame;
        check(std::strncmp(groundWebRefusal(t, false), "parkour", 7) == 0, "the game's mantle, just begun: refused");
        check(groundWebRefusal(t, false, .149f) != nullptr, "149 ms in: still refused");
        check(groundWebRefusal(t, false, .15f) == nullptr, "150 ms in: a web may cut it (the owner, Nov 18)");
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
            if (which == 1)
                check(o.domain == Domain::vr && o.lastEnterVr == EnterVr::cancel, "a zip: the web cuts it at once");
            else
                check(o.domain == Domain::anim, which == 0 ? "a fight keeps the animation" : "a scene keeps it");
        }
        // A zip's pull in the air: the grip cancel cuts it on the frame (immediate, no wait for the pull to end).
        {
            HybridOwner o;
            Frame g;
            g.state.playing = g.state.fresh = g.state.grounded = true;
            int64_t t = 1'000'000'000;
            for (int i = 0; i < 5; ++i)
                o.update(g, t += step);
            g.zipPress = true;
            g.state.grounded = false;
            o.update(g, t += step);
            g.zipPress = false;
            g.state.zipping = true;
            g.velY = 12;
            o.update(g, t += step);
            check(o.domain == Domain::anim, "a zip pulls: the animation's");
            g.cancelGrip = true;
            o.update(g, t += step);
            check(o.domain == Domain::vr && o.lastEnterVr == EnterVr::cancel, "a grip mid-pull: SpidyVR at once");
        }
        // The game's parkour on its crawl: refused in its first 150 ms, cut after.
        {
            HybridOwner o;
            Frame g;
            g.state.playing = g.state.fresh = true;
            g.state.runRt = true;
            g.state.wall = surfaceGame;
            int64_t t = 1'000'000'000;
            for (int i = 0; i < 5; ++i)
                o.update(g, t += step);
            g.groundWeb = true;
            g.parkourSeconds = .05f;
            o.update(g, t += step);
            check(o.domain == Domain::anim, "parkour 50 ms in: the animation keeps him");
            g.parkourSeconds = .2f;
            o.update(g, t += step);
            check(o.domain == Domain::vr && o.lastEnterVr == EnterVr::cancel, "parkour 200 ms in: the web cuts it");
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
    // ---- S2: GROUND FLIPS: the trigger ----
    {
        using namespace ground_flip;
        const Params p;
        const float dt = .011f;
        struct Run {
            Trigger t;
            Trigger::Out last;
            Kind fired = Kind::none;
            int aOut = 0;
            void frame(float x, float y, bool a, bool ground = true, bool legs = false, bool b = false,
                       bool blocked = false) {
                Trigger::Input in;
                in.seconds = .011f;
                in.stickValid = true;
                in.stickX = x, in.stickY = y;
                in.a = a, in.b = b, in.legs = legs, in.ground = ground, in.blocked = blocked;
                last = t.update(in, Params{});
                if (last.fire != Kind::none)
                    fired = last.fire;
                aOut += last.a ? 1 : 0;
            }
        };
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(0, -.9f, false); // flick down
            r.frame(0, -.9f, true);  // A
            check(r.fired == Kind::back && !r.last.a, "down, then A: a backflip, A consumed");
            for (int i = 0; i < 5; ++i)
                r.frame(0, 0, true);
            check(r.aOut == 0, "the firing A stays the flip's while held");
            r.frame(0, 0, false);
            r.frame(0, 0, true);
            check(r.last.a && r.last.fire == Kind::none, "a later plain A is the game's");
        }
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(0, .9f, false);
            r.frame(0, .9f, true);
            check(r.fired == Kind::front, "up, then A: a front flip");
        }
        {
            Run r;
            r.frame(0, 0, true);
            check(r.last.a && r.fired == Kind::none, "A alone: the game's jump, at once");
        }
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(.9f, 0, false); // a snap turn's flick
            r.frame(.9f, 0, true);
            check(r.fired == Kind::none && r.last.a, "a sideways flick (the snap turn) then A: a jump");
            Run d;
            d.frame(0, 0, false);
            d.frame(.6f, -.6f, false); // 45 degrees: outside the 35 degree cone
            d.frame(.6f, -.6f, true);
            check(d.fired == Kind::none, "a diagonal: no flip");
        }
        {
            Run r;
            r.frame(0, 0, false);
            for (int i = 1; i <= 30; ++i)
                r.frame(0, -.03f * i, false); // a slow push down (the heal's)
            r.frame(0, -.9f, true);
            check(r.fired == Kind::none && r.last.a, "a slow push then A: a jump");
        }
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(0, -.9f, false);
            for (int i = 0; i < 16; ++i)
                r.frame(0, -.9f, false); // 176 ms
            r.frame(0, -.9f, true);
            check(r.fired == Kind::none && r.last.a, "A later than 150 ms after the flick: a jump");
        }
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(0, -.9f, false);
            r.frame(0, -.9f, true, false);
            check(r.fired == Kind::none && r.last.a, "in the air: no ground flip");
            Run l;
            l.frame(0, 0, false);
            l.frame(0, -.9f, false);
            l.frame(0, -.9f, true, true, true);
            check(l.fired == Kind::none && l.last.a && l.last.denied == Deny::legs, "the legs engaged: denied, A goes on");
            Run b;
            b.frame(0, 0, false);
            b.frame(0, -.9f, false);
            b.frame(0, -.9f, true, true, false, true);
            check(b.last.denied == Deny::b, "B held: denied");
            Run k;
            k.frame(0, 0, false);
            k.frame(0, -.9f, false);
            k.frame(0, -.9f, true, true, false, false, true);
            check(k.last.denied == Deny::blocked && k.last.a, "blocked: denied, A goes on");
        }
        {
            Run r;
            r.frame(0, 0, false);
            r.frame(0, -.9f, false);
            r.frame(0, -.9f, true);
            r.frame(0, 0, false);
            r.frame(0, -.9f, false);
            r.frame(0, -.9f, true);
            check(r.last.denied == Deny::cooldown, "a second flip inside the cooldown: denied");
            for (int i = 0; i < 60; ++i)
                r.frame(0, 0, false);
            r.fired = Kind::none;
            r.frame(0, -.9f, false);
            r.frame(0, -.9f, true);
            check(r.fired == Kind::back, "after the cooldown: again");
        }
        (void)dt;
    }
    // ---- S2: the spin and the launch ----
    {
        using namespace ground_flip;
        const Vec3 ahead{0, 0, -1};
        const Vec3 back = spin(Kind::back, ahead, 360);
        check(std::abs(back.x - 6.2831853f) < 1e-3f && std::abs(back.y) < 1e-6f && std::abs(back.z) < 1e-6f,
              "a backflip: 360 deg/s about the level right (+X looking -Z), nose up");
        const Vec3 front = spin(Kind::front, ahead, 240);
        check(std::abs(front.x + 4.1887902f) < 1e-3f, "a front flip: the other way, at ROTATION SPEED");
        const Vec3 turned = spin(Kind::back, Vec3{1, -.9f, 0}, 360); // looking +X and down
        check(std::abs(turned.z - 6.2831853f) < 1e-3f && std::abs(turned.x) < 1e-4f,
              "the axis from the level look: looking +X, right is +Z");
        check(length(spin(Kind::back, Vec3{0, -1, 0}, 360)) > 6.f, "looking straight down: still a spin (-Z ahead)");
        check(length(spin(Kind::none, ahead, 360)) == 0, "none: no spin");
        const Params p;
        const Vec3 up = launch(Kind::back, ahead, {}, p);
        check(std::abs(up.y - p.upSpeed) < 1e-5f && std::abs(up.z - p.backSpeed) < 1e-5f, "a backflip launches up and back");
        const Vec3 run = launch(Kind::front, ahead, Vec3{0, -3, -20}, p);
        check(std::abs(run.z + p.keepHorizontal + p.frontSpeed) < 1e-4f && std::abs(run.y - p.upSpeed) < 1e-5f,
              "a front flip out of a run keeps the run (capped) and adds ahead; the fall is not kept");
        check(airSeconds(6.5f) > 1.2f && airSeconds(6.5f) < 1.4f, "about 1.3 s up: one turn at 270-360 deg/s");
        check(std::strcmp(kindName(Kind::back), "backflip") == 0 && std::strcmp(kindName(Kind::front), "front flip") == 0,
              "the names");
    }
    // ---- SIDE FLIPS: a directional A tap ----
    {
        using namespace ground_flip;
        struct Tap {
            TapTrigger t;
            TapTrigger::Out last;
            Kind fired = Kind::none;
            int aOut = 0, late = 0;
            void frame(float x, float y, bool a, float speed = 0, bool ground = true, bool legs = false) {
                TapTrigger::Input in;
                in.seconds = .011f;
                in.stickValid = true;
                in.stickX = x, in.stickY = y;
                in.a = a, in.legs = legs, in.ground = ground, in.speed = speed;
                last = t.update(in, TapParams{});
                if (last.fire != Kind::none)
                    fired = last.fire;
                aOut += last.a ? 1 : 0;
                late += last.late ? 1 : 0;
            }
            void hold(float x, float y, int frames, float speed = 0) {
                for (int i = 0; i < frames; ++i)
                    frame(x, y, false, speed);
            }
        };
        {
            Tap r;
            r.hold(1, 0, 16); // 176 ms to the right
            r.frame(1, 0, true);
            check(r.fired == Kind::none && !r.last.a && r.last.waiting, "the press is held back to see if it is a tap");
            for (int i = 0; i < 6; ++i)
                r.frame(1, 0, true);
            r.frame(1, 0, false);
            check(r.fired == Kind::right && r.aOut == 0, "stick right held, a quick A tap: a side flip right, no jump");
        }
        {
            Tap r;
            r.hold(-.9f, .1f, 16);
            r.frame(-.9f, .1f, true);
            r.frame(-.9f, .1f, false);
            check(r.fired == Kind::left, "to the left: a side flip left");
        }
        {
            Tap r;
            r.hold(1, 0, 16);
            for (int i = 0; i < 30; ++i)
                r.frame(1, 0, true);
            check(r.fired == Kind::none && r.late == 1 && r.last.a, "A held past 180 ms: the game's jump (late)");
        }
        {
            Tap r;
            r.hold(1, 0, 16, 4.5f);
            r.frame(1, 0, true, 4.5f);
            check(r.fired == Kind::none && r.last.a && r.last.denied == Deny::moving,
                  "running or strafing sideways (4.5 m/s): a plain jump at once, never a flip");
            check(std::abs(r.last.speed - 4.5f) < 1e-3f && std::abs(r.last.limit - TapParams{}.maxSpeed) < 1e-4f,
                  "F4: a tap refused for speed reports the speed and the side's limit");
        }
        {
            Tap r;
            r.hold(1, 0, 5); // 55 ms: a fresh push (the sidestep's flick)
            r.frame(1, 0, true);
            check(r.fired == Kind::none && r.last.a, "a stick pushed only just now: the jump (or the sidestep)");
        }
        {
            Tap r;
            r.hold(.6f, 0, 20); // under 0.8
            r.frame(.6f, 0, true);
            check(r.last.a && r.fired == Kind::none, "a half push to the side: a jump");
            Tap f;
            f.hold(0, 1, 20, 4.f); // ahead, running: a running jump
            f.frame(0, 1, true, 4.f);
            check(f.last.a && f.fired == Kind::none && f.last.denied == Deny::moving, "the stick ahead, running: a jump");
            check(std::abs(f.last.limit - TapParams{}.straightMaxSpeed) < 1e-4f,
                  "F4: a straight tap refused for speed reports the straight limit");
            Tap f2;
            f2.hold(0, 1, 20, 2.5f); // ahead at 2.5 m/s: past the straight taps' 2 m/s (the sides' 3 does not apply)
            f2.frame(0, 1, true, 2.5f);
            check(f2.last.a && f2.fired == Kind::none, "the stick ahead at 2.5 m/s: a jump");
            Tap n;
            n.frame(0, 0, true);
            check(n.last.a && !n.last.waiting, "A alone: at once, never held back");
        }
        {
            Tap r;
            r.hold(1, 0, 16);
            r.frame(1, 0, true, 0, false);
            check(r.last.a && r.fired == Kind::none, "in the air: no side flip");
            Tap l;
            l.hold(1, 0, 16);
            l.frame(1, 0, true, 0, true, true);
            check(l.last.a && l.last.denied == Deny::legs, "the legs engaged: denied");
        }
        // FORWARD AND BACK TAPS (the owner, Nov 18 1:09 PM).
        {
            Tap f;
            f.hold(.1f, .95f, 16, 1.f);
            f.frame(.1f, .95f, true, 1.f);
            f.frame(.1f, .95f, false, 1.f);
            check(f.fired == Kind::front && f.aOut == 0, "the stick held ahead, slow, a tap: a front flip");
            Tap fresh;
            fresh.hold(0, 1, 4);
            fresh.frame(0, 1, true);
            check(fresh.last.a && fresh.fired == Kind::none, "the stick only just pushed ahead: a plain jump at once");
            Tap held;
            held.hold(0, 1, 16);
            for (int i = 0; i < 30; ++i)
                held.frame(0, 1, true);
            check(held.fired == Kind::none && held.late == 1, "ahead, A held: the game's jump");
            Tap b;
            b.hold(0, -1, 16);
            b.frame(0, -1, true);
            b.frame(0, -1, false);
            check(b.fired == Kind::back, "STABLE or OLD (no rear snap): the stick held back, a tap: a backflip");
            Tap off;
            for (int i = 0; i < 17; ++i) {
                TapTrigger::Input in;
                in.seconds = .011f, in.stickValid = true, in.stickY = 1, in.ground = true, in.a = i == 16;
                in.straight = false;
                off.last = off.t.update(in, TapParams{});
            }
            check(off.last.a && !off.last.waiting, "forward and back taps off: the jump at once");
        }
        {
            // HYBRID's rear snap: only a held back stick under the snap's tilt, never the snap.
            const auto hybrid = [](float tilt, bool snapHeld) {
                TapTrigger t;
                Kind fired = Kind::none;
                TapTrigger::Out o;
                for (int i = 0; i < 19; ++i) {
                    TapTrigger::Input in;
                    in.seconds = .011f, in.stickValid = true, in.stickY = -tilt, in.ground = true;
                    in.a = i == 16 || i == 17;
                    in.rearSnap = true, in.snapHeld = snapHeld;
                    o = t.update(in, TapParams{});
                    if (o.fire != Kind::none)
                        fired = o.fire;
                }
                return fired;
            };
            check(hybrid(.4f, false) == Kind::back, "HYBRID: the stick held back at 0.4 (no snap, no run), a tap: a backflip");
            check(hybrid(1.f, false) == Kind::none, "HYBRID: the full back stick is the rear snap's, never a flip");
            check(hybrid(.4f, true) == Kind::none, "HYBRID: snapped and holding (running the new way): no flip");
            check(hybrid(.2f, false) == Kind::none, "HYBRID: under 0.3, nothing");
        }
        const Vec3 ahead{0, 0, -1};
        const Vec3 sr = spin(Kind::right, ahead, 360);
        check(std::abs(sr.z + 6.2831853f) < 1e-3f && std::abs(sr.x) < 1e-6f,
              "a side flip right turns about the level forward (-Z), as the stick turned right does");
        check(std::abs(spin(Kind::left, ahead, 360).z - 6.2831853f) < 1e-3f, "left: the other way");
        const Vec3 lr = launch(Kind::right, ahead, {}, Params{});
        check(lr.x > 1.4f && std::abs(lr.y - 6.5f) < 1e-5f, "it launches up and to its side");
        check(std::strcmp(kindName(Kind::left), "side flip left") == 0, "named");
    }
    // ---- GROUND FLIPS in every flip mode: FlipMotion's launched turn (CONTROLLED and basic FLIPS) ----
    {
        for (const bool controlled : {false, true}) {
            FlipMotion flip;
            FlipMotion::Sample sample;
            sample.airborne = true;
            sample.controlled = controlled;
            sample.seconds = 1.f / 90;
            flip.update(sample);
            flip.launch(Vec3{6.2831853f, 0, 0}, 6.2831853f); // a backflip's axis, one turn a second
            check(flip.launching(), "the launch is under way");
            for (int i = 0; i < 22; ++i)
                flip.update(sample);
            const Vec3 nose = flip.tilt().rotate({0, 0, -1});
            check(nose.y > .95f, controlled ? "CONTROLLED: a quarter turn in, nose up (a backflip)"
                                            : "basic: a quarter turn in, nose up (a backflip)");
            for (int i = 0; i < 80; ++i)
                flip.update(sample);
            check(!flip.launching() && std::abs(flip.turned() - 6.2831853f) < .02f, "one full turn, then it stops");
            sample.airborne = false;
            for (int i = 0; i < 200; ++i)
                flip.update(sample);
            check(flip.level(), "the landing uprights it");
        }
        // Cut short by the landing: the turn ends and the tilt comes upright.
        FlipMotion flip;
        FlipMotion::Sample sample;
        sample.airborne = true;
        sample.seconds = 1.f / 90;
        flip.launch(Vec3{0, 0, -4.f}, 6.2831853f);
        for (int i = 0; i < 20; ++i)
            flip.update(sample);
        sample.airborne = false;
        for (int i = 0; i < 300; ++i)
            flip.update(sample);
        check(!flip.launching() && flip.level(), "a landing mid-turn ends it and uprights");
        flip.launch(Vec3{std::nanf(""), 0, 0}, 6.f);
        check(!flip.launching(), "a nan spin is nothing");
    }
    std::printf("ground moves: %d checks passed, %d failed\n", passed, failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
