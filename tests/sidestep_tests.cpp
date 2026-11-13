// QUICK SIDESTEP (sidestep.hpp, November 12): the flick detector, the
// flick-then-A trigger and its input rules (A alone and a running jump go on
// at once; B, the point jump's chord and the leg hold win), the A it holds
// back and the buffered jump, the cooldown and chain lockout, the burst's
// plan (2 m in 0.18 s with a hard stop), its way in the world, the owner's
// ground exception and the settings. Portable.
#include "spidy/game_swing.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/sidestep.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::sidestep;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float dt = 1.f / 90;

struct Pad {
    Trigger t;
    Params p;
    Trigger::Input in;
    Trigger::Out out;
    int fires{}, aFrames{}, aPresses{};
    bool aWas{};
    Deny lastDeny = Deny::none;
    Pad() {
        in.seconds = dt;
        in.stickValid = true;
        in.groundAnim = true;
    }
    void frames(int n, float x, float y, bool a = false) {
        for (int i = 0; i < n; ++i) {
            in.stickX = x;
            in.stickY = y;
            in.a = a;
            out = t.update(in, p);
            fires += out.fire;
            aFrames += out.a;
            aPresses += out.a && !aWas;
            aWas = out.a;
            if (out.denied != Deny::none)
                lastDeny = out.denied;
        }
    }
    // A flick from rest to full right over `n` frames, then A after `wait` frames, held `hold`.
    void flickThenA(float x, float y, int wait = 2, int hold = 6, int n = 2) {
        frames(3, 0, 0);
        for (int i = 1; i <= n; ++i)
            frames(1, x * i / n, y * i / n);
        frames(wait, x, y);
        frames(hold, x, y, true);
        frames(1, x, y, false);
    }
};
} // namespace

int main() {
    // The flick: under 0.30 to over 0.80 within 80 ms.
    {
        Params p;
        Flick f;
        f.update(dt, true, 0, 0, p);
        f.update(dt, true, .5f, 0, p);
        check(f.update(dt, true, 1, 0, p).now, "rest to full in 22 ms is a flick");
        Flick slow;
        slow.update(dt, true, 0, 0, p);
        // 0.30 to 0.80 in about 120 ms (a push over 220 ms from rest to full).
        for (int i = 1; i <= 20; ++i)
            check(!slow.update(dt, true, i / 20.f, 0, p).now, "a slow push is no flick");
        Flick held;
        held.update(dt, true, 0, 0, p);
        held.update(dt, true, 1, 0, p);
        for (int i = 0; i < 20; ++i)
            check(!held.update(dt, true, 1, 0, p).now, "one flick per return to rest");
        Flick noisy;
        for (int i = 0; i < 50; ++i)
            check(!noisy.update(dt, true, .1f * (i % 2), .12f, p).now, "dead-zone noise is no flick");
        Flick bad;
        bad.update(dt, true, 0, 0, p);
        check(!bad.update(dt, true, NAN, 1, p).now && !bad.update(dt, false, 1, 0, p).now,
              "a bad or lost stick is no flick");
        // The flick's start stays above the steering dead zone even if set lower.
        Params low;
        low.flickFrom = .1f;
        Flick f2;
        f2.update(dt, true, .18f, 0, low);
        check(f2.update(dt, true, 1, 0, low).now, "from 0.18 (under the clamped 0.20 start) is a flick");
    }
    // Flick right then A: one burst, the A held back, no jump.
    {
        Pad pad;
        pad.flickThenA(1, 0);
        check(pad.fires == 1, "flick then A: a sidestep");
        check(pad.aFrames == 0, "the firing A never reached the game");
    }
    // A alone, and a running jump (stick held steady, then A): A at once.
    {
        Pad pad;
        pad.frames(5, 0, 0);
        pad.frames(1, 0, 0, true);
        check(pad.fires == 0 && pad.out.a, "A alone goes on the same frame");
        Pad run;
        run.frames(3, 0, 0);
        run.frames(1, .5f, 0);
        run.frames(1, 1, 0);  // a flick right ...
        run.frames(15, 1, 0); // ... held 167 ms: running now
        run.frames(1, 1, 0, true);
        check(run.fires == 0 && run.out.a, "A 167 ms after a flick (running) is the jump, at once");
        Pad forward;
        forward.flickThenA(0, 1);
        check(forward.fires == 0 && forward.aPresses == 1 && forward.lastDeny == Deny::forward,
              "a forward flick then A is the running jump (excluded cone)");
        Pad diag;
        diag.flickThenA(.3f, .95f);
        check(diag.fires == 0, "17 degrees off forward is inside the cone");
        // HYBRID v2: sideways only, 55-100 deg off ahead (the rear is the rear snap's).
        Pad back;
        back.flickThenA(0, -1);
        check(back.fires == 0 && back.lastDeny == Deny::notSideways && back.aPresses == 1,
              "a backward flick then A: no back step (rear snap's), the jump at once");
        const auto at = [](float degrees) {
            Pad pad;
            const float r = degrees * 3.14159265f / 180.f;
            pad.flickThenA(std::sin(r), std::cos(r));
            return pad.fires;
        };
        check(at(45) == 0 && at(-45) == 0, "45 deg (a diagonal run starting): no sidestep");
        check(at(60) == 1 && at(-60) == 1 && at(90) == 1 && at(-90) == 1 && at(99) == 1 && at(-99) == 1,
              "60-99 deg either side: sidestep");
        check(at(110) == 0 && at(-110) == 0 && at(150) == 0, "past the snap line (100): no sidestep");
        Pad old;
        old.p.sidewaysOnly = false;
        old.flickThenA(0, -1);
        check(old.fires == 1, "sideways-only off: the back step as before");
        Pad fwdOn;
        fwdOn.p.forward = true;
        fwdOn.flickThenA(0, 1);
        check(fwdOn.fires == 1, "sidestep_forward=1: forward too");
    }
    // A before the flick: no sidestep (the flick must come first).
    {
        Pad pad;
        pad.frames(3, 0, 0);
        pad.frames(1, 0, 0, true);
        pad.frames(1, .5f, 0, true);
        pad.frames(4, 1, 0, true);
        check(pad.fires == 0 && pad.aFrames == 6, "A then a flick: the jump, as before");
    }
    // The 120 ms window.
    {
        Pad in;
        in.flickThenA(1, 0, 9); // A 100 ms after the flick
        check(in.fires == 1, "A 100 ms after the flick fires");
        Pad out;
        out.flickThenA(1, 0, 12); // 133 ms
        check(out.fires == 0 && out.aPresses == 1, "A 133 ms after the flick is a jump");
    }
    // B, the point jump's chord and the leg hold win; the gate's own denials.
    {
        Pad b;
        b.in.b = true;
        b.flickThenA(1, 0);
        check(b.fires == 0 && b.lastDeny == Deny::chord && b.aPresses == 1, "B held: no sidestep, A goes on");
        Pad pj;
        pj.in.pointJump = true;
        pj.flickThenA(1, 0);
        check(pj.fires == 0 && pj.lastDeny == Deny::pointJump, "point jump charging: no sidestep");
        Pad legs;
        legs.in.legs = true;
        legs.flickThenA(1, 0);
        check(legs.fires == 0 && legs.lastDeny == Deny::legs, "Y+X legs: no sidestep");
        Pad air;
        air.in.groundAnim = false;
        air.flickThenA(1, 0);
        check(air.fires == 0 && air.lastDeny == Deny::owner && air.aPresses == 1,
              "not on the animation's ground: A is the game's");
        Pad blocked;
        blocked.in.blocked = true;
        blocked.flickThenA(1, 0);
        check(blocked.fires == 0 && blocked.lastDeny == Deny::blocked, "blocked: no sidestep");
        Pad off;
        off.p.enabled = false;
        off.flickThenA(1, 0);
        check(off.fires == 0 && off.lastDeny == Deny::off && off.aPresses == 1, "sidestep=0: A as before");
    }
    // The burst's time, a buffered jump, the cooldown and the chain lockout.
    {
        Pad pad;
        pad.frames(3, 0, 0);
        pad.frames(1, .5f, 0);
        pad.frames(1, 1, 0);
        pad.frames(1, 1, 0, true); // fire, A held
        check(pad.fires == 1 && pad.t.active(), "fired");
        pad.frames(4, 1, 0, false);
        pad.frames(3, 1, 0, true); // a new A during the burst: held back
        check(pad.aFrames == 0, "an A during the burst waits");
        int n = 0;
        while (pad.t.active() && n < 40) {
            pad.frames(1, 1, 0, true);
            ++n;
        }
        check(n > 0 && n <= 10 && pad.aFrames == 1 && pad.out.a,
              "the waiting A reaches the game as the burst ends");
        check(pad.t.burstSeconds() >= pad.p.seconds - 1e-4f && pad.t.burstSeconds() < pad.p.seconds + 2 * dt,
              "the burst lasts its 0.18 s");
        // Cooldown: a new flick then A within 0.30 s of the end is denied.
        Pad cd;
        cd.flickThenA(1, 0, 0, 1);
        int k = 0;
        while (cd.t.active() && k < 40) {
            cd.frames(1, 1, 0);
            ++k;
        }
        cd.flickThenA(-1, 0, 0, 1);
        check(cd.fires == 1 && cd.lastDeny == Deny::cooldown, "cooldown: denied");
        cd.frames(30, 0, 0);
        cd.flickThenA(-1, 0, 0, 1);
        check(cd.fires == 2, "after the cooldown: again");
        // Three in a row, then the 0.8 s lockout.
        Pad chain;
        chain.p.cooldownSeconds = 0;
        const auto stepOnce = [&](float x) {
            chain.flickThenA(x, 0, 0, 1, 1);
            int m = 0;
            while (chain.t.active() && m < 40) {
                chain.frames(1, x, 0);
                ++m;
            }
        };
        stepOnce(1);
        stepOnce(-1);
        stepOnce(1);
        check(chain.fires == 3 && chain.t.chain() == 3, "three in a row");
        stepOnce(-1);
        check(chain.fires == 3 && chain.lastDeny == Deny::lockout, "the fourth: the lockout");
        chain.frames(75, 0, 0); // 0.83 s
        stepOnce(-1);
        check(chain.fires == 4 && chain.t.chain() == 1, "after 0.8 s: a new chain");
        // The swing refused or ended the burst early: its clock stops, cooldown from then.
        Pad early;
        early.flickThenA(1, 0, 0, 1);
        early.t.stop();
        check(!early.t.active(), "stop() ends the burst");
    }
    // The plan: 2 m in 0.18 s with a 40 ms hard stop; level; its way in the world.
    {
        Params p;
        const auto b = plan({1, .5f, 0}, p);
        check(std::abs(b.moveSeconds + b.stopSeconds - .18f) < 1e-5f &&
                  std::abs(b.stopSeconds - .04f) < 1e-6f,
              "0.14 s across, 0.04 s stopped");
        check(std::abs(length(b.velocity) * b.moveSeconds - 2.f) < 1e-4f && b.velocity.y == 0 &&
                  b.velocity.x > 0,
              "2 m across, level");
        check(length(plan({0, 1, 0}, p).velocity) == 0 && length(plan({NAN, 0, 0}, p).velocity) == 0,
              "no way: no burst");
        const Vec3 right = worldWay(1, 0, 0), ahead = worldWay(0, 1, 0);
        check(std::abs(right.x - 1) < 1e-5f && std::abs(ahead.z + 1) < 1e-5f,
              "stick axes at yaw 0 (-Z ahead)");
        const Vec3 turned = worldWay(0, 1, 3.14159265f / 2);
        check(std::abs(turned.y) < 1e-6f && std::abs(length(turned) - 1) < 1e-5f, "turned view: level, unit");
        check(length(worldWay(1, 0, NAN)) == 0, "no view yaw: no way");
        game_swing::SidestepAsk ask;
        ask.velocity = b.velocity;
        ask.moveSeconds = b.moveSeconds;
        ask.stopSeconds = b.stopSeconds;
        check(game_swing::validSidestep(ask), "the plan is a valid ask");
        ask.velocity.y = 3;
        check(!game_swing::validSidestep(ask), "a burst that lifts is refused");
        ask.velocity = {60, 0, 0};
        check(!game_swing::validSidestep(ask), "too fast is refused");
    }
    // The owner: VR on the fire, held on the ground while it lasts, ANIM (land) after.
    {
        using namespace hybrid_owner;
        HybridOwner owner;
        Frame f;
        f.state.playing = f.state.fresh = f.state.grounded = true;
        int64_t now = 1'000'000'000;
        for (int i = 0; i < 20; ++i)
            owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::anim, "ground: ANIM");
        f.sidestep = f.sidestepHold = true;
        owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::vr && owner.lastEnterVr == EnterVr::sidestep,
              "the fire: VR (sidestep)");
        f.sidestep = false;
        for (int i = 0; i < 15; ++i)
            owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::vr, "held on the ground while the burst lasts");
        f.sidestepHold = false;
        owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::anim && owner.lastEnterAnim == EnterAnim::land,
              "then the ground's ANIM (land)");
        check(std::strcmp(enterVrName(EnterVr::sidestep), "sidestep") == 0, "named sidestep");
        check(viewEaseMs(EnterVr::sidestep, 250, 250) == 0, "no view ease (the position lock alone)");
        // Off the ground mid-burst: the air's rules.
        f.sidestep = f.sidestepHold = true;
        owner.update(f, now += 11'000'000);
        f.sidestep = false;
        f.state.grounded = false;
        owner.update(f, now += 11'000'000);
        check(owner.domain == Domain::vr, "off an edge mid-burst: VR's air");
    }
    // The settings: on by default, the keys, clamps, off.
    {
        using namespace vr_settings;
        const Values v;
        const auto p = paramsOf(v);
        check(p.enabled && std::abs(p.distance - 2) < 1e-6f && std::abs(p.seconds - .18f) < 1e-6f &&
                  std::abs(p.cooldownSeconds - .3f) < 1e-6f && p.chain == 3 &&
                  std::abs(p.lockoutSeconds - .8f) < 1e-6f && p.iframes && !p.forward &&
                  std::abs(p.flickFrom - .3f) < 1e-6f && std::abs(p.flickTo - .8f) < 1e-6f &&
                  std::abs(p.flickSeconds - .08f) < 1e-6f && std::abs(p.aSeconds - .12f) < 1e-6f,
              "the defaults");
        const std::string text = tabOnlyText(v);
        check(text.find("sidestep=1\n") != std::string::npos &&
                  text.find("sidestep_a_ms=120\n") != std::string::npos,
              "spidy_vr.ini keys");
        const Values off = withTabOnly(v, "sidestep=0\nsidestep_distance_cm=900\nsidestep_flick_to=10\n");
        check(!off.sidestep && off.sidestepDistanceCm == 300 &&
                  off.sidestepFlickTo >= off.sidestepFlickFrom + 20,
              "off, and clamped");
    }
    std::printf("%d sidestep checks passed\n", passed);
    return 0;
}
