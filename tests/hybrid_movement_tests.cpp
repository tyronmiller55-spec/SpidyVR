// MOVEMENT MODE HYBRID (hybrid_movement.hpp; the user, November 12 t75u): the view on
// the head, the left stick's front half along the heading, the rear half's one snap,
// the heading's hold and release, the air facing sync's gate, the settings; and the
// 31.70 / 408.44 landings replayed: the head-locked view must not swing.
#include "spidy/hybrid_movement.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace spidy;
using namespace spidy::hybrid_movement;
using spidy::vr_settings::Item;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float deg = 3.14159265f / 180.f;
constexpr float dt = 1.f / 90;
bool near(float a, float b, float tol) {
    return std::abs(wrap(a - b)) <= tol;
}
// The game turns the hero toward the walk stick, ~250 deg/s.
void heroStep(float& hero, bool push, float toward) {
    if (!push)
        return;
    const float d = wrap(toward - hero), most = 250 * deg * dt;
    hero = wrap(hero + std::clamp(d, -most, most));
}
float stickX(float angleDeg, float tilt = 1) {
    return std::sin(angleDeg * deg) * tilt;
}
float stickY(float angleDeg, float tilt = 1) {
    return std::cos(angleDeg * deg) * tilt;
}
// A landing replayed: the view SpidyVR showed, the game's stored facing, the head bone
// moving from the SpidyVR body (on the view) to the game's head over the body's 180 ms
// fade, the hero brought round by the forced walk-stick turn. Returns the largest
// view swing off the shown view; `hybrid` false is the plain head lock (view = bone).
float landing(float shown, float gameFacing, bool hybrid, float* heroEnd = nullptr, float* viewEnd = nullptr) {
    Heading h;
    h.latch(gameFacing);
    if (hybrid)
        h.begin(Heading::Hold::facing, shown);
    view_yaw_lock::TurnInPlace tip;
    float hero = gameFacing, worst = 0, view = shown;
    for (int f = 0; f < 360; ++f) {
        const float t = f * dt;
        const float fade = std::min(1.f, t / .18f);
        const float bone = wrap(shown + wrap(hero - shown) * fade); // mod head -> game head
        Heading::In in;
        in.dt = dt;
        in.heroOk = true;
        in.hero = hero;
        in.steerable = true;
        const auto o = h.update(in);
        view_yaw_lock::TurnInPlace::In ti;
        ti.dt = dt;
        ti.heroYaw = hero;
        ti.force = hybrid && o.heroTurn;
        ti.forceYaw = h.yaw;
        ti.forceAllowed = t > .05f; // the landing's first frames: not yet standing
        const auto to = tip.update(ti);
        heroStep(hero, to.push, to.pushYaw);
        if (hybrid) {
            const view_yaw_lock::State st{true, h.yaw, h.swayWeight, hero};
            view = view_yaw_lock::shownYaw(st, bone);
        } else {
            view = bone;
        }
        worst = std::max(worst, std::abs(wrap(view - shown)));
    }
    if (heroEnd)
        *heroEnd = hero;
    if (viewEnd)
        *viewEnd = view;
    return worst;
}
} // namespace

int main() {
    // --- Modes and settings.
    {
        check(modeOf(0) == Mode::hybrid && modeOf(1) == Mode::stable && modeOf(2) == Mode::old && modeOf(9) == Mode::hybrid,
              "modes");
        check(viewSource(2) == stable_yaw::Source::body && viewSource(0) == stable_yaw::Source::stable &&
                  viewSource(1) == stable_yaw::Source::stable,
              "OLD steers by the head bone, the rest by the stable heading");
        vr_settings::Values v;
        check(v.movementMode == 0 && v.rearSnapMs == 200, "defaults: HYBRID, rear snap a 200 ms head turn");
        const auto& rows = vr_settings::rows();
        bool mode = false, snap = false;
        for (const auto& r : rows) {
            if (r.item == Item::movementMode)
                mode = std::string(r.title) == "MOVEMENT MODE" && r.choices.size() == 3 &&
                       std::string(r.choices[0]) == "HYBRID" && std::string(r.choices[1]) == "STABLE" &&
                       std::string(r.choices[2]) == "OLD";
            if (r.item == Item::rearSnap)
                snap = std::string(r.title) == "REAR SNAP TURN" && std::string(r.choices[0]) == "INSTANT";
        }
        check(mode && snap, "rows labelled");
        const std::string ini = vr_settings::tabOnlyText(v);
        check(ini.find("movement_mode=0") != std::string::npos && ini.find("rear_snap_turn_ms=200") != std::string::npos,
              "saved keys");
        check(vr_settings::withTabOnly(vr_settings::Values{}, "movement_mode=7\n").movementMode <= 2, "sanitized");
        check(vr_settings::choose(Item::movementMode, 1, v) && v.movementMode == 1, "choose STABLE");
        check(vr_settings::inactiveReason(Item::rearSnap, v) != nullptr, "rear snap greyed outside HYBRID");
        check(vr_settings::inactiveReason(Item::animYawSway, v) == nullptr, "sway live in STABLE");
        v.movementMode = 0;
        check(vr_settings::inactiveReason(Item::rearSnap, v) == nullptr &&
                  vr_settings::inactiveReason(Item::animYawSway, v) != nullptr,
              "HYBRID: rear snap live, sway greyed");
    }
    // --- The stick's yaw: right is - (rig yaw + to the left).
    {
        check(near(stickYaw(0, 1, 0), -90 * deg, .01f), "stick right: 90 deg right");
        check(near(stickYaw(30 * deg, 0, -1), wrap(210 * deg), .01f), "stick back: behind the heading");
    }
    // --- v2 gate: the front half is free movement; the headset steers in the 30-degree cone.
    {
        Gate g;
        auto o = g.update(dt, 0, 1);
        check(o.drives && near(o.y, 1, .001f) && near(o.dir, 0, .001f) && o.zone == Zone::cone && !o.snapped,
              "forward, no gaze: full run along the heading");
        o = g.update(dt, stickX(60), stickY(60));
        check(o.drives && near(o.y, 1, .001f) && near(o.dir, 60 * deg, .01f) && o.zone == Zone::free,
              "60 deg: full speed, he runs 60 deg off the heading");
        o = g.update(dt, stickX(-90), stickY(-90));
        check(o.drives && near(o.dir, -90 * deg, .01f), "pure side: he runs sideways (free movement)");
        o = g.update(dt, stickX(95), stickY(95));
        check(o.drives && near(o.dir, 90 * deg, .01f) && o.zone == Zone::edge && !o.snapped,
              "95 deg: still runs, at 90 (the snap waits for 100)");
        // The headset steers inside the window, clamped to it, low-passed.
        Gate c;
        for (int i = 0; i < 90; ++i)
            o = c.update(dt, 0, 1, 10 * deg);
        check(near(o.dir, 10 * deg, .2f * deg), "cone: looking 10 deg right runs him 10 deg right");
        for (int i = 0; i < 90; ++i)
            o = c.update(dt, stickX(-8), stickY(-8), 60 * deg);
        check(near(o.dir, 15 * deg, .2f * deg), "cone: looking 60 deg right is clamped to the window's 15");
        Gate q;
        q.update(dt, 0, 1, 0);
        o = q.update(dt, 0, 1, 40 * deg);
        check(o.dir < 3 * deg, "a head flick does not jerk the run (gaze low-pass)");
        // No step anywhere across the cone and the blend, with the gaze at the far side.
        float worst = 0, prev = runDirection(0, -15 * deg);
        for (float aDeg = 0; aDeg <= 100; aDeg += .5f) {
            const float d = runDirection(aDeg * deg, -15 * deg);
            worst = std::max(worst, std::abs(d - prev));
            prev = d;
        }
        check(worst < 3.f * deg, "cone -> blend -> free: continuous (no hitch at 15 or 25; under 3 deg per half degree of stick)");
        check(near(runDirection(25 * deg, 15 * deg), 25 * deg, .01f) &&
                  near(runDirection(15 * deg, -15 * deg), -15 * deg, .01f),
              "the blend ends on the stick at 25 and starts on the gaze at 15");
    }
    // --- v2: no feedback loop - the heading never follows the steered run.
    {
        Heading h;
        h.latch(0);
        Gate g;
        float hero = 0, headingMoved = 0;
        for (int i = 0; i < 180; ++i) { // 2 s, stick forward, head held 10 deg right
            const auto o = g.update(dt, 0, 1, 10 * deg);
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.driving = o.drives;
            in.steerable = true;
            h.update(in);
            headingMoved = std::max(headingMoved, std::abs(wrap(h.yaw)));
            heroStep(hero, o.drives, wrap(h.yaw - o.dir));
        }
        check(headingMoved < .01f * deg, "2 s of gaze steering: the heading (the view) never moved");
        check(near(hero, -10 * deg, 1 * deg), "he settles 10 deg right of it and stays (no spiral)");
        // Free run at 70 deg, then let go: he stays, the view stays, no turn-in-place.
        for (int i = 0; i < 90; ++i) {
            const auto o = g.update(dt, stickX(70), stickY(70), 0);
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.driving = o.drives;
            in.steerable = true;
            h.update(in);
            heroStep(hero, o.drives, wrap(h.yaw - o.dir));
        }
        check(near(hero, -70 * deg, 1 * deg) && near(h.yaw, 0, .01f * deg), "free run: body 70 off, view on the heading");
        bool turned = false;
        for (int i = 0; i < 270; ++i) {
            const auto o = g.update(dt, 0, 0, 0);
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.driving = o.drives;
            in.steerable = true;
            turned = turned || h.update(in).heroTurn;
        }
        check(!turned && near(h.yaw, 0, .01f * deg) && near(hero, -70 * deg, 1 * deg),
              "let go: Spidey stays where he is, the view stays (no turn back, no rejoin)");
        // The game turning him (wall, parkour: not steerable) takes the anchor away.
        bool unanchored = false;
        for (int i = 0; i < 40; ++i) {
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.steerable = false;
            unanchored = unanchored || h.update(in).unanchored;
        }
        check(unanchored && !h.anchored, "the game's own turn: the heading follows him again");
    }
    // --- The rear half: one snap past 100, held, re-armed under 80 or neutral.
    {
        Gate g;
        g.update(dt, 0, 0);
        auto o = g.update(dt, stickX(150), stickY(150));
        check(!o.snapped && o.rear, "the first frame past 100 waits for the stick to settle");
        o = g.update(dt, stickX(150), stickY(150));
        check(o.snapped && near(o.snapAngle, 150 * deg, .01f) && o.drives, "snaps to 150 once it holds still");
        for (int i = 0; i < 30; ++i) {
            o = g.update(dt, stickX(150), stickY(150));
            check(!o.snapped && o.drives && near(o.y, 1, .001f), "held back: runs the new way, no second snap");
        }
        o = g.update(dt, stickX(120), stickY(120));
        check(!o.snapped && o.drives, "110-170 while held: still no snap (not re-armed)");
        o = g.update(dt, stickX(85), stickY(85));
        check(!o.snapped && o.drives, "85: still above the re-arm angle");
        o = g.update(dt, stickX(70), stickY(70));
        check(!o.snapped && g.armed, "under 80: re-armed");
        g.update(dt, stickX(170), stickY(170));
        o = g.update(dt, stickX(170), stickY(170));
        check(o.snapped && g.snaps == 2, "re-armed: the next push back snaps again");
        g.update(dt, .1f, -.1f);
        check(g.armed, "neutral re-arms");
        // A fast swing from the side to the back snaps where it lands, not at 100.
        Gate f;
        f.update(dt, stickX(80), stickY(80));
        o = f.update(dt, stickX(105), stickY(105));
        check(!o.snapped, "crossing 100 mid-swing: wait");
        o = f.update(dt, stickX(165), stickY(165));
        check(!o.snapped, "still moving: wait");
        o = f.update(dt, stickX(178), stickY(178));
        o = f.update(dt, stickX(178), stickY(178));
        check(o.snapped && near(o.snapAngle, 178 * deg, .5f * deg), "snaps to where the stick settled");
        // A light push back does not snap.
        Gate l;
        l.update(dt, 0, 0);
        l.update(dt, stickX(180, .4f), stickY(180, .4f));
        check(!l.update(dt, stickX(180, .4f), stickY(180, .4f)).snapped, "under half tilt: no snap");
    }
    // --- Heading: a rear snap holds the view on it, the hero comes round, the head is back.
    {
        Heading h;
        h.latch(0);
        float hero = 0;
        h.begin(Heading::Hold::snap, stickYaw(0, stickX(150), stickY(150)));
        check(near(h.yaw, -150 * deg, .01f), "instant: the heading (the view) is there at once");
        float worstView = 0;
        bool released = false;
        for (int i = 0; i < 200 && !released; ++i) {
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.driving = true; // the stick is held back: he runs that way
            in.steerable = true;
            const auto o = h.update(in);
            released = o.released == Heading::Hold::snap;
            const view_yaw_lock::State st{true, h.yaw, h.swayWeight, hero};
            const float bone = wrap(hero + 5 * deg); // the run's head sway
            worstView = std::max(worstView, std::abs(wrap(view_yaw_lock::shownYaw(st, bone) - h.yaw)));
            heroStep(hero, true, h.yaw);
        }
        check(released && near(hero, -150 * deg, 13 * deg), "the hero arrives, the hold ends");
        check(worstView < 1 * deg, "while he turns the view stays on the snap's heading");
        // Then the sway fades back in: the view ends on the head bone.
        for (int i = 0; i < 60; ++i) {
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.steerable = true;
            h.update(in);
        }
        const view_yaw_lock::State st{true, h.yaw, h.swayWeight, hero};
        // v2: the stick drove him, so the heading is the player's (anchored): the view is the
        // heading plus the head's own sway, within the arrival's 12 deg of the head bone.
        check(h.anchored &&
                  near(view_yaw_lock::shownYaw(st, wrap(hero + 7 * deg)), wrap(h.yaw + 7 * deg), .5f * deg) &&
                  near(view_yaw_lock::shownYaw(st, wrap(hero + 7 * deg)), wrap(hero + 7 * deg), 13 * deg),
              "at rest: the heading plus the head's sway, near the head bone");
    }
    {
        // Eased snap (REAR SNAP TURN 200 MS).
        Heading h;
        h.latch(0);
        h.begin(Heading::Hold::snap, 180 * deg, .2f);
        Heading::In in;
        in.dt = dt;
        in.heroOk = true;
        in.hero = 0;
        in.driving = true;
        in.steerable = true;
        h.update(in);
        check(!near(h.yaw, 180 * deg, 30 * deg), "200 ms: not there on the first frame");
        for (int i = 0; i < 20; ++i)
            h.update(in);
        check(near(h.yaw, 180 * deg, .5f * deg), "200 ms: there after it");
    }
    {
        // His own turns (a parkour move) are followed at once: no lag, no divergence.
        Heading h;
        h.latch(0);
        float worst = 0;
        for (int i = 0; i < 90; ++i) {
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = wrap(i * 4 * deg); // 360 deg/s
            h.update(in);
            worst = std::max(worst, std::abs(wrap(h.yaw - in.hero)));
        }
        check(worst < .01f * deg, "no hold: the heading is his facing every frame");
        // A right-stick turn standing: hold, forced turn, arrive.
        h.latch(0);
        h.turn(45 * deg);
        Heading::In in;
        in.dt = dt;
        in.heroOk = true;
        in.hero = 0;
        in.steerable = true;
        const auto o = h.update(in);
        check(o.heroTurn && h.hold == Heading::Hold::turn && near(h.yaw, 45 * deg, .01f),
              "R stick: the view turns now, the walk stick turns him");
        // Running (driving): no forced turn, the run brings him.
        in.driving = true;
        check(!h.update(in).heroTurn, "running: the run turns him, no stand-still turn");
        // v2: a hero that cannot follow (2.5 s) on his feet: the heading and view stay.
        in.driving = false;
        bool kept = false, gave = false;
        for (int i = 0; i < 260 && !kept; ++i) {
            const auto r = h.update(in);
            kept = r.kept;
            gave = gave || r.gaveUp;
        }
        check(kept && !gave && h.hold == Heading::Hold::none && near(h.yaw, 45 * deg, .01f),
              "2.5 s without him on his feet: the heading stays (no view swing), said so");
        // Off his feet (not steerable) it still rejoins him.
        h.latch(0);
        h.turn(45 * deg);
        in.steerable = false;
        gave = false;
        for (int i = 0; i < 260 && !gave; ++i)
            gave = h.update(in).gaveUp;
        check(gave, "not steerable: give up, the view rejoins him as before");
    }
    // --- The landings replayed (06:37 session).
    {
        float heroEnd = 0, viewEnd = 0;
        const float plain = landing(97 * deg, -80 * deg, false);
        check(plain > 170 * deg, "31.70 replay, plain head lock: the view swings 177 deg");
        const float hybrid = landing(97 * deg, -80 * deg, true, &heroEnd, &viewEnd);
        check(hybrid < 15 * deg, "31.70 replay, HYBRID: the view stays within 15 deg of what was shown");
        check(near(heroEnd, 97 * deg, 13 * deg), "31.70 replay: the hero is turned round to the view");
        check(near(viewEnd, 97 * deg, 13 * deg), "31.70 replay: and the view is the head again, on it");
        std::printf("31.70 replay: plain swing %.0f deg, HYBRID worst %.1f deg\n", plain / deg, hybrid / deg);
        const float plain2 = landing(152 * deg, 24 * deg, false);
        const float hybrid2 = landing(152 * deg, 24 * deg, true, &heroEnd);
        check(plain2 > 120 * deg && hybrid2 < 15 * deg && near(heroEnd, 152 * deg, 13 * deg),
              "408.44 replay: 128 deg plain, HYBRID held and he comes round");
        std::printf("408.44 replay: plain swing %.0f deg, HYBRID worst %.1f deg\n", plain2 / deg, hybrid2 / deg);
        // The facing check agrees in HYBRID: the view is ok (held), the hero is corrected.
        facing_check::Check c;
        c.arm(facing_check::Event::landing, 97 * deg);
        facing_check::Sample s;
        s.viewOk = true;
        s.view = 97 * deg; // the held heading
        s.viewFixable = true;
        s.heroMatters = true;
        s.heroOk = true;
        s.hero = -80 * deg;
        const auto o = c.update(dt, s);
        check(o.view.verdict == facing_check::Verdict::ok && o.heroTurn, "facing check in HYBRID: view ok, hero corrected");
    }
    // --- The air facing sync's gate.
    {
        check(airSyncWanted(true, true, true, false, false, 0, 90 * deg), "air, 90 off: sync");
        check(!airSyncWanted(true, true, true, false, false, 0, 10 * deg), "10 off: no");
        check(!airSyncWanted(false, true, true, false, false, 0, 90 * deg), "not HYBRID: no");
        check(!airSyncWanted(true, true, true, true, false, 0, 90 * deg), "on a web: no");
        check(!airSyncWanted(true, true, false, false, false, 0, 90 * deg), "grounded: no");
        check(!airSyncWanted(true, false, true, false, false, 0, 90 * deg), "the animation's: no");
        // The stick is retired (5061b4a playtest: 170 syncs, the facing did not move); the gate stays.
        check(!airSyncStick, "the sync stick is retired");
    }
    std::printf("hybrid movement tests: %d passed\n", passed);
    return 0;
}
