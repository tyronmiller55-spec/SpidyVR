// POINT JUMP (point_jump.hpp, milestone 1): the chord (A+B together, hold B
// then A), the charge curve, the direction blend, never into the surface, the
// cooldown and fresh contact, the zip guard, ChargeJump::consume, the owner's
// ground exception (hybrid_owner.hpp) and where its settings are. Portable.
#include "spidy/game_pad.hpp"
#include "spidy/game_swing.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/point_jump.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace spidy;
using namespace spidy::point_jump;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
bool near(float a, float b, float e = 1e-3f) {
    return std::fabs(a - b) <= e;
}
constexpr float dt = 1.f / 90;

Chord::Input in(bool a, bool b, int mode = 0, bool allowed = true, bool warning = false) {
    Chord::Input i;
    i.seconds = dt;
    i.a = a;
    i.b = b;
    i.allowed = allowed;
    i.warning = warning;
    i.mode = mode;
    return i;
}

void chordTogether() {
    // Same frame: a chord, neither button reaches the game.
    {
        Chord c;
        auto o = c.update(in(true, true));
        check(o.began && !o.a && !o.b && c.charging(), "A+B in one frame is the chord");
        for (int i = 0; i < 45; ++i) {
            o = c.update(in(true, true));
            check(!o.a && !o.b && !o.released, "held: nothing reaches the game");
        }
        o = c.update(in(false, true));
        check(o.released && near(o.held, 46 * dt, 1e-4f) && !o.a && !o.b, "letting go of A fires, held time");
        o = c.update(in(false, true));
        check(!o.b, "B still held after the fire is swallowed (no late dodge)");
        o = c.update(in(false, false));
        o = c.update(in(false, true));
        check(o.b || c.phase() == Chord::Phase::waitB, "after both let go B works again");
    }
    // A first, B 30 ms later: a chord, A-first; A was held back meanwhile.
    {
        Chord c;
        auto o = c.update(in(true, false));
        check(!o.a && c.phase() == Chord::Phase::waitA, "A waits for B");
        o = c.update(in(true, false));
        o = c.update(in(true, true));
        check(o.began && o.aFirst && near(o.gap, 2 * dt, 1e-4f) && !o.a, "A then B within the window");
    }
    // B first, then A: B-first.
    {
        Chord c;
        c.update(in(false, true));
        auto o = c.update(in(true, true));
        check(o.began && !o.aFirst, "B then A within the window");
    }
    // A alone: held back at most the window, then the game's A as it is held.
    {
        Chord c;
        int waited = 0;
        auto o = c.update(in(true, false));
        while (!o.a && waited < 20) {
            o = c.update(in(true, false));
            ++waited;
        }
        check(o.a && waited * dt <= .05f + dt, "A alone reaches the game within the chord window");
        o = c.update(in(true, true));
        check(!o.began && o.a && o.b, "B after A went on is no chord: both to the game");
    }
    // A tapped inside the window: still the game's tap (one frame late).
    {
        Chord c;
        c.update(in(true, false));
        auto o = c.update(in(false, false));
        check(o.a && !o.began, "a quick A tap still reaches the game");
        o = c.update(in(false, false));
        check(!o.a, "only once");
    }
    // B alone: the dodge after the window; a warning makes it at once.
    {
        Chord c;
        auto o = c.update(in(false, true));
        check(!o.b, "B waits for A");
        int waited = 0;
        while (!o.b && waited < 20) {
            o = c.update(in(false, true));
            ++waited;
        }
        check(o.b && waited * dt <= .05f + dt, "B alone dodges within the window");
        Chord w;
        o = w.update(in(false, true, 0, true, true));
        check(o.b, "spider-sense: B is the dodge at once");
    }
    // Not allowed (no surface, cooldown, a prompt): A and B as they are.
    {
        Chord c;
        auto o = c.update(in(true, true, 0, false));
        check(!o.began && o.a && o.b, "not allowed: both pass at once");
    }
    // A grip cancels the charge: nothing fires, buttons swallowed until let go.
    {
        Chord c;
        c.update(in(true, true));
        c.update(in(true, true));
        c.cancel();
        auto o = c.update(in(true, true));
        check(o.cancelled && !c.charging(), "the grip cancelled");
        o = c.update(in(false, true));
        check(!o.released && !o.b, "a cancelled chord never fires, B swallowed");
    }
}

void chordHoldB() {
    // Mode 1: hold B, then A.
    {
        Chord c;
        auto o = c.update(in(false, true, 1));
        check(!o.b && c.phase() == Chord::Phase::waitB, "B waits for A");
        for (int i = 0; i < 10; ++i)
            o = c.update(in(false, true, 1));
        o = c.update(in(true, true, 1));
        check(o.began && !o.a && !o.b, "B held 110 ms, then A: the chord");
    }
    {
        Chord c;
        c.update(in(false, true, 1));
        c.update(in(false, true, 1));
        auto o = c.update(in(false, false, 1));
        check(o.b && !o.began, "B let go of with no A: the dodge on the release");
    }
    {
        Chord c;
        auto o = c.update(in(false, true, 1));
        int frames = 0;
        while (!o.b && frames < 60) {
            o = c.update(in(false, true, 1));
            ++frames;
        }
        check(o.b && frames * dt >= .24f && frames * dt <= .26f + dt, "B held 250 ms with no A: the dodge then");
        o = c.update(in(true, true, 1));
        check(!o.began && o.a, "A after the dodge went is the game's A");
    }
    {
        Chord c;
        auto o = c.update(in(true, false, 1));
        check(o.a, "mode 1: A alone is never held back");
        o = c.update(in(false, true, 1, true, true));
        check(o.b, "mode 1: spider-sense makes B immediate");
    }
}

void curve() {
    Params p;
    check(charge(0, p) == 0 && charge(.15f, p) == 0, "a tap is no charge");
    check(near(speed(.1f, p), 9), "a tap is 9 m/s");
    check(near(speed(1.f, p), 22) && near(speed(3.f, p), 22), "a full second is 22 m/s, and holds");
    const float u = (.5f - .15f) / .85f;
    check(near(charge(.5f, p), 1 - (1 - u) * (1 - u)), "ease-out c = 1 - (1 - u)^2");
    check(near(speed(.5f, p), 9 + 13 * (1 - (1 - u) * (1 - u))), "v = 9 + 13c");
    float last = 0;
    for (float t = .15f; t <= 1.f; t += .01f) {
        check(charge(t, p) >= last - 1e-6f, "the charge only rises");
        last = charge(t, p);
    }
    Params linear = p;
    linear.curve = 1;
    check(near(charge(.5f, linear), u), "curve 1 is linear");
}

void directions() {
    Params p;
    const Vec3 up{0, 1, 0};
    // Looking level on the ground: the look, lifted by the surface push.
    {
        const auto d = direction({0, 0, -1}, up, p);
        const Vec3 expect = normalized(Vec3{0, .25f, -1});
        check(near(d.way.x, expect.x) && near(d.way.y, expect.y) && near(d.way.z, expect.z) &&
                  d.outFixDegrees == 0,
              "level look + 0.25 n");
    }
    // Looking down into the ground: turned out to 10 degrees, never into it.
    {
        const auto d = direction(normalized(Vec3{0, -1, -1}), up, p);
        check(near(dot(d.way, up), std::sin(10 * 3.14159265f / 180), 1e-4f) && d.outFixDegrees > 0,
              "a downward look leaves the ground at 10 degrees");
        check(d.way.z < 0, "keeping the look's heading");
    }
    // Straight down: straight up.
    {
        const auto d = direction({0, -1, 0}, up, p);
        check(near(d.way.y, 1), "straight into the surface is straight out of it");
    }
    // A wall facing +x: looking along it leaves it a little.
    {
        const Vec3 n{1, 0, 0};
        const auto d = direction({0, 0, -1}, n, p);
        check(dot(d.way, n) >= std::sin(10 * 3.14159265f / 180) - 1e-4f, "off a wall, out of it");
        const auto into = direction({-1, 0, 0}, n, p);
        check(dot(into.way, n) > 0, "looking into the wall never launches into it");
    }
    // Every look, every surface: at least minOut out.
    for (int i = 0; i < 200; ++i) {
        const float a = i * .37f, b = i * .11f;
        const Vec3 look{std::cos(a) * std::cos(b), std::sin(b) - .3f, std::sin(a) * std::cos(b)};
        const Vec3 n = normalized(Vec3{std::sin(i * .7f), 1.2f + std::cos(i * .3f), std::cos(i * .9f)});
        const auto d = direction(look, n, p);
        check(dot(d.way, n) >= std::sin(10 * 3.14159265f / 180) - 1e-3f && near(length(d.way), 1),
              "never less than 10 degrees out of the surface");
    }
    // The velocity: speed along d, a quarter of the rest kept, never into the surface.
    {
        const Vec3 d = normalized(Vec3{0, .25f, -1});
        const Vec3 v = launchVelocity(d, 10, {4, 0, 0}, up, p);
        check(near(dot(v, d), 10, 1e-3f) && near(v.x, 1, 1e-3f), "v = d s + 0.25 (v_old across d)");
        const Vec3 falling = launchVelocity(d, 10, {0, -20, 0}, up, p);
        check(dot(falling, up) >= 10 * d.y - 1e-3f, "a fall kept is never into the surface");
    }
}

void gate() {
    Params p;
    p.enabled = true;
    Controller g;
    Controller::Input i;
    i.seconds = dt;
    i.contact = true;
    check(g.update(i, p), "on the ground: allowed");
    g.release(.3f);
    auto f = g.take(p);
    check(f.now, "fires at once on the ground");
    // Cooldown, then a fresh contact.
    check(!g.update(i, p) && g.deny() == Deny::cooldown, "cooldown right after");
    for (int k = 0; k < 30; ++k)
        g.update(i, p);
    check(g.deny() == Deny::fresh, "after the cooldown, still needs a fresh contact");
    i.contact = false;
    g.update(i, p);
    i.contact = true;
    check(g.update(i, p), "a fresh contact: allowed again");
    // Coyote: 120 ms after leaving the surface still allowed, not after.
    Controller c;
    i.contact = true;
    c.update(i, p);
    i.contact = false;
    for (int k = 0; k < 9; ++k)
        c.update(i, p);
    check(c.deny() == Deny::none, "within coyote time");
    for (int k = 0; k < 4; ++k)
        c.update(i, p);
    check(c.deny() == Deny::noContact, "past coyote time");
    // A release in the air waits 150 ms for the ground (the buffer).
    c.release(.5f);
    f = c.take(p);
    check(!f.now && !f.dropped && c.pending(), "the buffer waits");
    i.contact = true;
    c.update(i, p);
    f = c.take(p);
    check(f.now, "landing within the buffer fires");
    Controller late;
    i.contact = false;
    late.update(i, p);
    for (int k = 0; k < 20; ++k)
        late.update(i, p);
    late.release(.5f);
    for (int k = 0; k < 20 && late.pending(); ++k) {
        late.update(i, p);
        f = late.take(p);
    }
    check(f.dropped && f.why == Deny::expired, "no ground within the buffer: dropped");
    // Blocked and disabled.
    Controller b;
    Controller::Input bi = i;
    bi.contact = true;
    bi.blocked = true;
    check(!b.update(bi, p) && b.deny() == Deny::blocked, "blocked (prompt, pad layer, vent, line)");
    Params off;
    check(!b.update(i, off) && b.deny() == Deny::disabled, "off by default");
    Params noFight = p;
    noFight.inCombat = false;
    bi.blocked = false;
    bi.fighting = true;
    check(!b.update(bi, noFight) && b.deny() == Deny::combat, "point_jump_in_combat 0");
}

void zipGuard() {
    // EngineBot's Q1: the takeoff's jump press must not fall in the game's
    // point-launch window after a zip lands. A release then waits.
    Params p;
    p.enabled = true;
    Controller g;
    Controller::Input i;
    i.seconds = dt;
    i.contact = true;
    g.update(i, p);
    i.zipping = true;
    g.update(i, p);
    check(g.deny() == Deny::zipGuard, "the zip asked for: guarded");
    i.zipping = false;
    g.update(i, p);
    check(g.deny() == Deny::zipGuard, "still on the ground before the zip took him: guarded");
    i.contact = false;
    for (int k = 0; k < 30; ++k)
        g.update(i, p);
    i.contact = true;
    check(g.update(i, p) && g.deny() == Deny::zipGuard, "landed from the zip: guarded, a chord may start");
    g.release(.2f);
    auto f = g.take(p);
    check(!f.now && !f.dropped, "the release waits for the guard");
    int frames = 1;
    while (!f.now && frames < 100) {
        g.update(i, p);
        f = g.take(p);
        ++frames;
    }
    check(f.now && frames * dt >= .39f && frames * dt <= .45f, "fires once 400 ms have passed since the landing");
}

void consume() {
    using game_pad::ChargeJump;
    ChargeJump c;
    c.update(dt, true, true);
    for (int i = 0; i < 30; ++i)
        c.update(dt, true, true);
    check(c.charging(), "charging a leap");
    c.consume();
    check(!c.charging(), "consumed");
    auto k = c.update(dt, false, true);
    check(!k.jump && k.leap == 0, "a consumed charge never leaps or jumps when let go");
    ChargeJump w;
    w.update(dt, true, true); // waiting for a tap
    w.consume();
    k = w.update(dt, false, true);
    check(!k.jump && k.leap == 0, "a consumed press is no late A");
    k = w.update(dt, true, true);
    k = w.update(dt, false, true);
    check(k.jump, "the next press works as before");
}

void ownership() {
    using namespace spidy::hybrid_owner;
    constexpr int64_t frame = 11'000'000;
    HybridOwner o;
    Frame f{};
    f.state.playing = f.state.fresh = true;
    f.state.grounded = true;
    int64_t t = 0;
    o.update(f, t);
    check(o.domain == Domain::anim, "the ground is the animation's");
    // Fired: VR at once, on the ground (the approved exception).
    f.pointJump = f.pointJumpHold = true;
    check(o.update(f, t += frame) == Domain::vr && o.enterVr && o.lastEnterVr == EnterVr::pointJump &&
              std::strcmp(enterVrName(EnterVr::pointJump), "point-jump") == 0,
          "the point jump takes the ground animation");
    // Still on the ground for the takeoff: held.
    f.pointJump = false;
    check(o.update(f, t += frame) == Domain::vr, "held through the takeoff");
    // Off the ground: the air is VR's.
    f.state.grounded = false;
    f.pointJumpHold = false;
    f.velY = 8;
    for (int i = 0; i < 100; ++i)
        check(o.update(f, t += frame) == Domain::vr, "the air is SpidyVR's");
    // Landing on the feet: back to the animation.
    f.state.grounded = true;
    check(o.update(f, t += frame) == Domain::anim && o.lastEnterAnim == EnterAnim::land, "landing snaps back");
    // Without the hold, a ground VR goes back at once (the old rule stands).
    f.pointJump = true;
    o.update(f, t += frame);
    f.pointJump = false;
    check(o.update(f, t += frame) == Domain::anim, "no hold: the ground returns");
    // Not in a cutscene.
    HybridOwner c;
    Frame g = f;
    g.cutscene = true;
    g.pointJump = true;
    check(c.update(g, 0) == Domain::anim, "a cutscene is never taken");
}

void bridge() {
    game_swing::PointJumpAsk a;
    a.serial = 1;
    a.velocity = {0, 5, -10};
    check(game_swing::validPointJump(a), "a launch");
    a.velocity = {0, 0, 0};
    check(!game_swing::validPointJump(a), "no zero launch");
    a.velocity = {0, NAN, 0};
    check(!game_swing::validPointJump(a), "no NaN launch");
    a.velocity = {0, 100, 0};
    check(!game_swing::validPointJump(a), "no absurd launch");
}

void settings() {
    using namespace vr_settings;
    Values v;
    check(!v.pointJump && v.pointJumpTrigger == 0 && v.pointJumpMinSpeed == 9 && v.pointJumpMaxSpeed == 22 &&
              v.pointJumpFullMs == 1000 && v.pointJumpNormalBias == 25 && v.pointJumpTapMs == 150 &&
              v.pointJumpChordMs == 50 && v.pointJumpCoyoteMs == 120 && v.pointJumpBufferMs == 150 &&
              v.pointJumpCooldownMs == 250 && v.pointJumpKeep == 25 && v.pointJumpMinOutDegrees == 10 &&
              v.pointJumpInCombat,
          "the plan's defaults, off by default");
    const auto p = paramsOf(v);
    check(!p.enabled && near(p.minSpeed, 9) && near(p.maxSpeed, 22) && near(p.normalBias, .25f) &&
              near(p.keep, .25f) && near(p.chordSeconds, .05f),
          "params from the values");
    const Item rowsWanted[] = {Item::pointJump, Item::pointJumpTrigger, Item::pointJumpMinSpeed,
                               Item::pointJumpMaxSpeed, Item::pointJumpFull, Item::pointJumpPush};
    const auto& tab = screens()[static_cast<size_t>(Category::pointJump)];
    check(std::strcmp(tab.tabTitle, "VR POINT JUMP") == 0, "its own, clearly named tab");
    for (Item item : rowsWanted) {
        const Row* row = rowFor(item);
        check(row && row->place == Place::tab && row->help && std::strlen(row->help) <= 80, "an in-game row");
        int on = 0;
        for (const auto& s : screens())
            for (Item i : s.items)
                on += i == item;
        check(on == 1, "on exactly one tab");
    }
    check(choose(Item::pointJump, 1, v) && v.pointJump, "POINT JUMP ON");
    check(inactiveReason(Item::pointJumpTrigger, Values{}) && !inactiveReason(Item::pointJumpTrigger, v),
          "the rows say they need POINT JUMP");
    check(choose(Item::pointJumpTrigger, 1, v) && v.pointJumpTrigger == 1, "HOLD B, THEN A");
    check(choose(Item::pointJumpMaxSpeed, 0, v) && v.pointJumpMaxSpeed == 14, "MAX SPEED 14");
    check(choose(Item::pointJumpMinSpeed, 6, v) && v.pointJumpMinSpeed == 14 && v.pointJumpMaxSpeed >= 14,
          "TAP SPEED never above MAX SPEED");
    const std::string text = tabOnlyText(Values{});
    for (const char* key : {"point_jump=0\n", "point_jump_trigger=0\n", "point_jump_chord_ms=50\n",
                            "point_jump_tap_ms=150\n", "point_jump_full_ms=1000\n", "point_jump_min_speed=9\n",
                            "point_jump_max_speed=22\n", "point_jump_curve=0\n", "point_jump_normal_bias=25\n",
                            "point_jump_min_out_degrees=10\n", "point_jump_keep=25\n", "point_jump_coyote_ms=120\n",
                            "point_jump_buffer_ms=150\n", "point_jump_cooldown_ms=250\n",
                            "point_jump_zip_guard_ms=400\n", "point_jump_dodge_hold_ms=250\n",
                            "point_jump_in_combat=1\n"})
        check(text.find(key) != std::string::npos, key);
    const Values read = withTabOnly({}, "point_jump=1\npoint_jump_trigger=1\npoint_jump_max_speed=99\n");
    check(read.pointJump && read.pointJumpTrigger == 1 && read.pointJumpMaxSpeed == 40, "read back, clamped");
#ifdef SPIDY_SOURCE_DIR
    std::ifstream file(std::string(SPIDY_SOURCE_DIR) + "/apps/launcher/ui.cpp");
    std::stringstream ss;
    ss << file.rdbuf();
    const std::string ui = ss.str();
    for (const char* name : {"Item::pointJump)", "Item::pointJumpTrigger", "Item::pointJumpMinSpeed",
                             "Item::pointJumpMaxSpeed", "Item::pointJumpFull", "Item::pointJumpPush"})
        check(ui.find(name) != std::string::npos, name);
    check(ui.find("\"Point jump (experimental)\"") != std::string::npos, "the launcher's section is labelled");
#endif
}
} // namespace

int main() {
    chordTogether();
    chordHoldB();
    curve();
    directions();
    gate();
    zipGuard();
    consume();
    ownership();
    bridge();
    settings();
    std::printf("%d point jump checks passed\n", passed);
    return 0;
}
