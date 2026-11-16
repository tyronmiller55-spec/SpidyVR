// VIEW YAW SOURCE: STABLE (stable_yaw.hpp, November 12): a closed loop where
// the old head-bone source runs in circles and STABLE runs straight; the
// heading's latch, freeze and turns; the composeMove turn gate; the gap watch
// and the runaway-turn detector; the setting. Portable.
#include "spidy/hybrid_handoff.hpp"
#include "spidy/hybrid_steer.hpp"
#include "spidy/stable_yaw.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace spidy;
using namespace spidy::stable_yaw;
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
    return std::fabs(wrap(a - b)) <= e;
}
constexpr float dt = 1.f / 90;
constexpr float deg = 3.14159265f / 180;

// The game in a few lines: the hero turns toward the walk direction at up to
// 400 deg/s; his head bone faces where he does (a little behind). The left
// stick is held hard right for `seconds`.
struct Loop {
    float hero = 0, headBone = 0, swept = 0;
    uint32_t runaways = 0, gapOpens = 0;
    float lastGap = 0;
    void run(Source source, float seconds, float stickX = 1, float stickY = 0) {
        hybrid_steer::YawFilter filter;
        Heading heading;
        Runaway runaway;
        GapWatch gaps;
        for (int i = 0; i < static_cast<int>(seconds / dt); ++i) {
            Input in;
            in.dt = dt;
            in.headBoneYaw = headBone;
            in.leftTilt = std::hypot(stickX, stickY);
            float view;
            if (source == Source::stable)
                view = filter.update(heading.update(in), dt, stableTau);
            else
                view = filter.update(hybrid_steer::viewYaw(headBone, 0, 0, 0), dt);
            const Vec3 walk = hybrid_steer::viewMove(stickX, stickY, view);
            const float want = hybrid_handoff::lookYaw(walk);
            const float step = std::clamp(wrap(want - hero), -400 * deg * dt, 400 * deg * dt);
            hero = wrap(hero + step);
            swept += step;
            headBone = wrap(headBone + wrap(hero - headBone) * .5f);
            lastGap = wrap(want - hero);
            // The player's own turns (headset, R stick): none here.
            runaways += runaway.update(dt, hero, 0, true, stickX, stickY, 0).fired;
            gapOpens += gaps.update(dt, true, lastGap).edge == GapWatch::Edge::opened;
        }
    }
};
} // namespace

int main() {
    // The loop: the old source chases itself and circles; STABLE turns 90 and holds.
    {
        Loop old;
        old.run(Source::body, 5);
        check(std::fabs(old.swept) > 720 * deg, "BODY (old): a held side push circles (more than 2 turns in 5 s)");
        check(old.runaways >= 1, "the runaway detector sees the circling");
        check(old.gapOpens >= 1, "the gap watch sees the push open the gap");
        Loop stable;
        stable.run(Source::stable, 5);
        check(near(stable.hero, -90 * deg, 2 * deg), "STABLE: he turns to the push (90 deg right) and runs straight");
        check(std::fabs(stable.swept) < 95 * deg, "STABLE: no circling");
        check(stable.runaways == 0 && stable.gapOpens == 1, "STABLE: no runaway, the gap opens once (the push)");
        check(std::fabs(stable.lastGap) < 1 * deg, "STABLE: the gap closes");
        Loop forward;
        forward.run(Source::stable, 3, 0, 1);
        check(std::fabs(forward.swept) < 1 * deg, "STABLE forward: straight ahead");
        Loop diagonal;
        diagonal.run(Source::stable, 5, .7f, .7f);
        check(near(diagonal.hero, -45 * deg, 2 * deg), "STABLE diagonal: 45 deg and straight");
    }
    // The heading: latched at the first frame, follows the head bone while the
    // left stick is idle, frozen while it is held.
    {
        Heading h;
        Input in;
        in.dt = dt;
        in.headBoneYaw = .5f;
        check(near(h.update(in), .5f), "latched from the head bone");
        in.headBoneYaw = .8f;
        for (int i = 0; i < 30; ++i)
            h.update(in);
        check(near(h.update(in), .8f), "idle: follows the head bone");
        in.leftTilt = 1;
        h.update(in);
        in.headBoneYaw = 2.f;
        check(near(h.update(in), .8f) && h.frozen, "held: the head bone no longer turns it");
        in.hmdDelta = .3f;
        check(near(h.update(in), 1.1f), "held: the headset's turn does");
        in.headTurn = -.2f;
        check(near(h.update(in), .9f), "held: Level's turn does");
        in.leftTilt = 0;
        in.hmdDelta = in.headTurn = 0;
        h.update(in);
        check(near(h.update(in), .8f), "a quick release keeps the heading");
        for (int i = 0; i < 30; ++i)
            h.update(in);
        check(near(h.update(in), 2.f) && !h.frozen, "after 0.2 s idle it follows where he looks again");
        Heading bad;
        Input nan;
        nan.headBoneYaw = NAN;
        check(std::isfinite(bad.update(nan)) && !bad.set, "NaN: not latched, finite");
    }
    // Its own R-stick turn while steering outside Level: smooth deg/s and snaps.
    {
        Heading h;
        Input in;
        in.dt = dt;
        in.leftTilt = 1;
        in.rightX = 1;
        in.smoothDegPerSec = 90;
        float yaw = 0;
        for (int i = 0; i < 90; ++i)
            yaw = h.update(in);
        check(near(yaw, -90 * deg, 2 * deg), "smooth 90 deg/s: a second right is 90 deg right");
        Heading level;
        in.levelTurns = true;
        for (int i = 0; i < 90; ++i)
            yaw = level.update(in);
        check(near(yaw, 0), "Level: animHeadTurn turns it, not this");
        Heading snap;
        Input s;
        s.dt = dt;
        s.leftTilt = 1;
        s.snapDeg = 30;
        s.rightX = .9f;
        for (int i = 0; i < 20; ++i)
            yaw = snap.update(s);
        check(near(yaw, -30 * deg), "a held flick snaps once");
        s.rightX = .2f;
        snap.update(s);
        s.rightX = -.9f;
        yaw = snap.update(s);
        check(near(yaw, 0), "released, then a left flick snaps back");
        Heading idle;
        Input i2;
        i2.dt = dt;
        i2.rightX = 1;
        i2.smoothDegPerSec = 90;
        for (int i = 0; i < 90; ++i)
            yaw = idle.update(i2);
        check(near(yaw, 0), "left stick idle: the R stick turns him (composeMove), not the heading");
    }
    // composeMove's turn: zero in STABLE while running, the stick otherwise.
    {
        check(composeTurn(Source::stable, .8f, 1) == 0, "STABLE running: no second turn");
        check(composeTurn(Source::stable, .8f, 0) == .8f, "STABLE idle: the bounded facing turn stays");
        check(composeTurn(Source::body, .8f, 1) == .8f, "BODY: as before");
        check(composeTurn(Source::stable, NAN, 1) == 0, "NaN: none");
    }
    // The gap watch: 10 lines a second while steering; events at 45 / 40.
    {
        GapWatch g;
        int lines = 0;
        for (int i = 0; i < 90; ++i)
            lines += g.update(dt, true, 0).line;
        check(lines >= 9 && lines <= 11, "10 Hz");
        check(g.update(dt, true, 50 * deg).edge == GapWatch::Edge::opened, "over 45: opened");
        check(g.update(dt, true, 42 * deg).edge == GapWatch::Edge::none, "42: still wide (hysteresis)");
        const auto closed = g.update(dt, true, 30 * deg);
        check(closed.edge == GapWatch::Edge::closed && closed.wideSeconds > 0, "under 40: closed, with how long");
        g.update(dt, true, 90 * deg);
        check(g.update(dt, false, 0).edge == GapWatch::Edge::closed, "steering ends: closed");
        check(g.crossings == 2, "crossings counted");
        int idleLines = 0;
        for (int i = 0; i < 90; ++i)
            idleLines += g.update(dt, false, 0).line;
        check(idleLines == 0, "no lines while not steering");
    }
    // The runaway detector: not on a turn the stick asked for, nor the headset's.
    {
        Runaway r;
        uint32_t fired = 0;
        float hero = 0;
        // A new push right: he turns 90 within 0.3 s - the stick just changed.
        for (int i = 0; i < 90; ++i) {
            hero = std::max(hero - 300 * deg * dt, -90 * deg);
            fired += r.update(dt, hero, 0, true, 1, 0, 0).fired;
        }
        check(fired == 0, "a turn to a new push is no runaway");
        Runaway h;
        float view = 0;
        hero = 0;
        for (int i = 0; i < 300; ++i) {
            view -= 180 * deg * dt;
            hero -= 180 * deg * dt;
            fired += h.update(dt, wrap(hero), wrap(view), true, 0, 1, 0).fired;
        }
        check(fired == 0, "following the headset's own turn is no runaway");
        Runaway rs;
        hero = 0;
        for (int i = 0; i < 300; ++i) {
            hero -= 180 * deg * dt;
            fired += rs.update(dt, wrap(hero), 0, true, 0, 1, .8f).fired;
        }
        check(fired == 0, "turning with the R stick is no runaway");
        Runaway run;
        hero = 0;
        for (int i = 0; i < 300; ++i) {
            hero -= 180 * deg * dt;
            fired += run.update(dt, wrap(hero), 0, true, 0, 1, 0).fired;
        }
        check(fired >= 2 && fired <= 3, "a steady stick and heading, hero spinning: reported, rearmed each second");
        check(std::isfinite(Runaway{}.update(dt, NAN, 0, true, 0, 1, 0).sweep), "NaN: nothing");
    }
    // The setting: STABLE by default, BODY (OLD) the old way, a personal key.
    {
        using namespace vr_settings;
        Values v;
        check(v.viewYawSource == 1 && sourceOf(v.viewYawSource) == Source::stable, "default STABLE");
        const Row* row = rowFor(Item::viewYawSource);
        check(row && row->choices.size() == 2 && std::strcmp(row->choices[0], "BODY (OLD)") == 0 &&
                  std::strcmp(row->choices[1], "STABLE") == 0,
              "choices");
        check(choose(Item::viewYawSource, 0, v) && v.viewYawSource == 0 && sourceOf(v.viewYawSource) == Source::body,
              "BODY (OLD)");
        bool listed = false;
        for (Item item : screens()[static_cast<size_t>(Category::nativeAnimation)].items)
            listed = listed || item == Item::viewYawSource;
        check(listed, "on NATIVE ANIMATION");
        check(tabOnlyText(v).find("view_yaw_source=0") != std::string::npos, "saved as view_yaw_source");
        check(withTabOnly(Values{}, "view_yaw_source=7\n").viewYawSource <= 1, "sanitized");
    }
    std::printf("stable yaw: %d checks passed\n", passed);
    return 0;
}
