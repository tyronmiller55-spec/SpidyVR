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
        check(v.movementMode == 0 && v.rearSnapMs == 0, "defaults: HYBRID, instant rear snap");
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
        check(ini.find("movement_mode=0") != std::string::npos && ini.find("rear_snap_ms=0") != std::string::npos,
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
    // --- The gate: front half runs along the heading, never turns.
    {
        Gate g;
        auto o = g.update(dt, 0, 1);
        check(o.drives && near(o.y, 1, .001f) && o.x == 0 && !o.snapped, "forward: full run along the heading");
        o = g.update(dt, stickX(60), stickY(60));
        check(o.drives && std::abs(o.y - .5f) < .01f && o.x == 0, "60 deg: half speed, still straight ahead");
        o = g.update(dt, stickX(90), stickY(90));
        check(!o.drives && !o.snapped, "pure side: no run, no turn (the game cannot strafe)");
        o = g.update(dt, stickX(95), stickY(95));
        check(!o.drives && !o.snapped, "95 deg: dead band");
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
        check(near(view_yaw_lock::shownYaw(st, wrap(hero + 7 * deg)), wrap(hero + 7 * deg), .5f * deg),
              "at rest the view is the head bone, yaw included");
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
        // A hero that cannot follow (2.5 s): the heading rejoins him.
        in.driving = false;
        bool gave = false;
        for (int i = 0; i < 260 && !gave; ++i)
            gave = h.update(in).gaveUp;
        check(gave && h.hold == Heading::Hold::none, "2.5 s without him: give up, said so");
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
    // --- LANDING VIEW HOLD: a facing hold lasts its setting, quick in a fight or driving; the
    // release eases onto him over the AIR VIEW EASE time instead of snapping.
    {
        auto hold = [&](float holdS, bool quick, bool driving, float easeS, float heroYaw, int& timedOutAt,
                        float& worstStep, float& endYaw, bool& easedFlag) {
            Heading h;
            h.latch(heroYaw);
            h.begin(Heading::Hold::facing, heroYaw + 150 * deg); // the view shown: he stands 150 deg off
            timedOutAt = -1;
            worstStep = 0;
            easedFlag = false;
            float last = h.yaw;
            for (int i = 0; i < 360; ++i) {
                Heading::In in;
                in.dt = dt;
                in.heroOk = true;
                in.hero = heroYaw; // he does not come round (the landing's turn failed)
                in.steerable = true;
                in.driving = driving;
                in.facingHoldSeconds = holdS;
                in.quick = quick;
                in.easeSeconds = easeS;
                const auto o = h.update(in);
                if (o.gaveUp && timedOutAt < 0) {
                    timedOutAt = i;
                    easedFlag = o.eased;
                }
                worstStep = std::max(worstStep, std::abs(wrap(h.yaw - last)));
                last = h.yaw;
            }
            endYaw = h.yaw;
        };
        int at;
        float worst, end;
        bool eased;
        hold(.6f, false, false, .25f, 0.f, at, worst, end, eased);
        check(at >= 52 && at <= 56, "the landing hold lets go at 0.6 s, not 2.5 s");
        check(eased && near(end, 0.f, .1f * deg), "and the heading ends on him");
        check(worst < 10 * deg, "the release eases: no step over 10 deg a frame (a snap was 150)");
        hold(.6f, false, false, 0.f, 0.f, at, worst, end, eased);
        check(at >= 52 && at <= 56 && !eased && worst > 100 * deg, "ease OFF: the old snap, at 0.6 s");
        hold(.6f, true, false, .25f, 0.f, at, worst, end, eased);
        check(at >= 12 && at <= 16, "in a fight (quick): 0.15 s");
        hold(.6f, false, true, .25f, 0.f, at, worst, end, eased);
        check(at >= 52 && at <= 56, "driving alone is cut by game_xr's `quick`, not here");
        hold(.6f, true, true, .25f, 0.f, at, worst, end, eased);
        check(at >= 12 && at <= 16 && !eased,
              "left stick held: 0.15 s, no ease (the heading stays on the view, he runs round)");
        hold(2.5f, false, false, .25f, 0.f, at, worst, end, eased);
        check(at >= 222 && at <= 228, "2500 MS is the old hold");
        hold(0.f, false, false, .25f, 0.f, at, worst, end, eased);
        check(at >= 0 && at <= 2, "NONE lets go at once");
        // A shorter setting never lengthens the quick hold; the longer old one keeps 0.15.
        hold(.1f, true, false, .25f, 0.f, at, worst, end, eased);
        check(at >= 8 && at <= 10, "quick never outlasts the setting");
        // Arriving is unchanged: he gets round, the hold ends on arrival, no ease.
        Heading h;
        h.latch(0);
        h.begin(Heading::Hold::facing, 90 * deg);
        bool arrived = false;
        float hero = 0;
        for (int i = 0; i < 90 && !arrived; ++i) {
            hero = std::min(90 * deg, hero + 250 * deg * dt);
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = hero;
            in.steerable = true;
            in.facingHoldSeconds = .6f;
            in.easeSeconds = .25f;
            const auto o = h.update(in);
            arrived = o.released == Heading::Hold::facing && !o.gaveUp;
        }
        check(arrived, "he comes round inside the hold: arrived, as before");
        // A right-stick or rear-snap hold keeps the 2.5 s give-up (only facing holds are shortened).
        Heading t;
        t.latch(0);
        t.turn(45 * deg);
        int gaveAt = -1;
        for (int i = 0; i < 300 && gaveAt < 0; ++i) {
            Heading::In in;
            in.dt = dt;
            in.heroOk = true;
            in.hero = 0;
            in.steerable = true;
            in.facingHoldSeconds = .6f;
            in.easeSeconds = .25f;
            if (t.update(in).gaveUp)
                gaveAt = i;
        }
        check(gaveAt >= 222 && gaveAt <= 228, "an R-stick turn keeps its 2.5 s");
        // The setting.
        vr_settings::Values v;
        check(v.landingHoldMs == 600, "LANDING VIEW HOLD: 600 ms by default");
        const auto& rows = vr_settings::rows();
        bool row = false;
        for (const auto& r : rows)
            if (r.item == Item::landingHold)
                row = std::string(r.title) == "LANDING VIEW HOLD" && r.choices.size() == 5 &&
                      std::string(r.choices[0]) == "NONE" && std::string(r.choices[2]) == "600 MS" &&
                      std::string(r.choices[4]) == "2500 MS (OLD)";
        check(row, "row labelled: NONE / 300 / 600 / 1000 / 2500 (OLD)");
        check(vr_settings::choose(Item::landingHold, 4, v) && v.landingHoldMs == 2500, "choose OLD");
        check(vr_settings::tabOnlyText(v).find("landing_hold_ms=2500") != std::string::npos,
              "saved as landing_hold_ms");
        check(vr_settings::withTabOnly(vr_settings::Values{}, "landing_hold_ms=1000\n").landingHoldMs == 1000,
              "read back");
        check(vr_settings::withTabOnly(vr_settings::Values{}, "landing_hold_ms=99999\n").landingHoldMs ==
                  2500,
              "sanitized");
        bool listed = false;
        for (Item item :
             vr_settings::screens()[static_cast<size_t>(vr_settings::Category::nativeAnimation)].items)
            listed = listed || item == Item::landingHold;
        check(listed, "on NATIVE ANIMATION");
        v.movementMode = 1;
        check(vr_settings::inactiveReason(Item::landingHold, v) != nullptr, "greyed outside HYBRID");
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
