// CRIME RUN (crime_run.hpp; prototype, Nov 22): the score, the ranks, the spawns on the ring and at real crimes,
// the timers, stops and misses, the end of a run, the best, the phone's start and quit, the bridge's checks and the
// drawing. Portable: no Windows, no headset, no game.
#include "spidy/crime_run.hpp"
#include "spidy/phone.hpp"
#include "spidy/vr_settings.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace spidy;
using namespace spidy::crime_run;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
float across(Vec3 a, Vec3 b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}
int count(const std::vector<Event>& e, What w) {
    int n = 0;
    for (const auto& x : e)
        n += x.what == w;
    return n;
}
const Event* last(const std::vector<Event>& e, What w) {
    const Event* found = nullptr;
    for (const auto& x : e)
        if (x.what == w)
            found = &x;
    return found;
}
// Runs `seconds` of frames at 90 Hz.
void play(Run& run, float seconds, Vec3 player, const Census& census, std::vector<Event>& events) {
    for (float t = 0; t < seconds; t += 1.f / 90)
        run.update(1.f / 90, player, census, events);
}
Config quick() {
    Config c;
    c.firstSpawnSeconds = 1;
    c.spawnSeconds = 30;
    c.timerSeconds = 60;
    c.maxSites = 1;
    return c;
}
} // namespace

int main() {
    // ---- Settings ----
    {
        const Config d;
        check(d.startScore == 20000, "the run did not start at 20,000 (t103u)");
        Config bad;
        bad.radiusMin = 500;
        bad.radiusMax = 100;
        bad.maxSites = 9;
        bad.timerSeconds = NAN;
        bad.winStops = 0;
        const Config s = sanitized(bad);
        check(s.radiusMax >= s.radiusMin + 10, "the ring's outer radius stayed inside its inner one");
        check(s.maxSites == 4 && s.winStops == 1, "the site and stop counts were not kept in range");
        check(s.timerSeconds == d.timerSeconds, "a timer that is no number did not fall back to its default");
    }
    // ---- Ranks ----
    {
        check(rankFor(0, 20) == Rank::rookie, "no stops was not ROOKIE");
        check(stopsFor(Rank::streetLevel, 20) == 2 && stopsFor(Rank::friendlyNeighborhood, 20) == 5 &&
                  stopsFor(Rank::spectacular, 20) == 9 && stopsFor(Rank::sensational, 20) == 14 &&
                  stopsFor(Rank::amazing, 20) == 20,
              "the ranks of a run to 20 were not at 2, 5, 9, 14 and 20 stops");
        check(rankFor(19, 20) == Rank::sensational && rankFor(20, 20) == Rank::amazing, "AMAZING was not the target");
        for (int win = 1; win <= 60; ++win) {
            check(stopsFor(Rank::amazing, win) == win, "AMAZING was not at the target for every target");
            for (int r = 1; r < rankCount; ++r)
                check(stopsFor(static_cast<Rank>(r), win) >= stopsFor(static_cast<Rank>(r - 1), win),
                      "a rank took fewer stops than the one below it");
        }
        check(rankFor(1, 1) == Rank::amazing, "a run to one stop did not end AMAZING");
        check(std::strcmp(rankName(Rank::friendlyNeighborhood), "FRIENDLY NEIGHBORHOOD") == 0, "a rank's name");
    }
    // ---- Points ----
    {
        Config c;
        check(pointsFor(c, 120, 120, 0) == 2000, "a stop at once did not score double (100% speed bonus)");
        check(pointsFor(c, 0, 120, 0) == 1000, "a stop at the last moment did not score the base");
        check(pointsFor(c, 60, 120, 0) == 1500, "a stop half way did not score half the bonus");
        check(pointsFor(c, 60, 120, 3) == 2100, "three enemies did not add 200 each");
        check(pointsFor(c, 200, 120, 0) == 2000, "time left over the timer scored more than the bonus");
        c.speedBonusPct = 0;
        check(pointsFor(c, 120, 120, 0) == 1000, "no speed bonus still scored one");
    }
    // ---- The ring ----
    {
        Rng rng(42);
        const Vec3 centre{100, 20, -50};
        for (int i = 0; i < 2000; ++i) {
            const Vec3 p = ringPoint(centre, 150, 400, rng.unit(), rng.unit());
            const float d = across(p, centre);
            check(d >= 149.9f && d <= 400.1f && p.y == centre.y, "a ring point fell outside 150..400 m");
        }
        Rng a(7), b(7);
        check(a.next() == b.next() && a.unit() == b.unit(), "the same seed did not give the same numbers");
    }
    // ---- A run: start, a beacon spawn on the ring, the timer, a miss ----
    {
        Run run;
        std::vector<Event> ev;
        const Vec3 home{0, 10, 0};
        run.start(quick(), home, 1234, ev);
        check(run.running() && run.score() == 20000 && count(ev, What::started) == 1, "the run did not start");
        play(run, .5f, home, {}, ev);
        check(run.siteCount() == 0, "a crime came before the first spawn's second");
        play(run, .6f, home, {}, ev);
        check(run.siteCount() == 1 && count(ev, What::spawned) == 1, "no crime came at the first spawn");
        const Site s = run.sites()[0];
        check(s.kind == Kind::beacon && !s.grounded, "with no enemies the crime was not a beacon awaiting its ground");
        check(across(s.point, home) >= 150 && across(s.point, home) <= 400, "the beacon was not on the ring");
        run.ground(s.id, 3.5f);
        check(run.sites()[0].grounded && run.sites()[0].point.y == 3.5f, "the ray's ground was not taken");
        // Standing still: the timer runs out, a miss costs 2000.
        play(run, 60.5f, home, {}, ev);
        check(count(ev, What::missed) == 1 && run.misses() == 1, "the timer ran out without a miss");
        check(last(ev, What::missed)->points == -2000 && run.score() == 18000, "a miss did not cost 2000");
        check(run.streak() == 0, "a miss kept the streak");
    }
    // ---- Stopping a beacon: reach it and hold ----
    {
        Run run;
        std::vector<Event> ev;
        Config c = quick();
        c.holdSeconds = 3;
        run.start(c, {}, 99, ev);
        play(run, 1.05f, {}, {}, ev);
        const Site s = run.sites()[0];
        run.ground(s.id, 0);
        // Above it but too high (60 m up, reach height 25): not inside.
        play(run, 4, s.point + Vec3{0, 60, 0}, {}, ev);
        check(run.siteCount() == 1 && count(ev, What::reached) == 0, "a beacon was reached from 60 m above it");
        // At its edge, 14 m across, standing on it.
        const Vec3 at = s.point + Vec3{14, 1, 0};
        play(run, 2, at, {}, ev);
        check(count(ev, What::reached) == 1 && run.siteCount() == 1, "the beacon was not reached, or ended early");
        // Stepping out lets the hold drain; back in it fills to 3 s.
        play(run, .5f, s.point + Vec3{40, 0, 0}, {}, ev);
        check(run.sites()[0].hold > 0 && run.sites()[0].hold < 2, "the hold did not drain outside the zone");
        play(run, 3.1f, at, {}, ev);
        check(count(ev, What::stopped) == 1 && run.stops() == 1 && run.siteCount() == 0, "holding did not stop it");
        const Event* st = last(ev, What::stopped);
        check(st->points > 1000 && st->points <= 2000 && run.score() == 20000 + st->points,
              "the stop's points were not 1000 plus the time bonus");
        check(run.streak() == 1, "a stop did not count in the streak");
    }
    // ---- A real crime: the game's enemies in play become the site; it is stopped once they are down ----
    {
        Run run;
        std::vector<Event> ev;
        Config c = quick();
        run.start(c, {}, 5, ev);
        std::vector<Vec3> thugs = {{90, 2, 10}, {94, 2, 12}, {88, 2, 7}, {300, 2, 300} /* a lone one far off */,
                                   {10, 2, 0} /* too near: under realMin */};
        Census census{thugs.data(), thugs.size(), true};
        play(run, 1.05f, {}, census, ev);
        check(run.siteCount() == 1 && run.sites()[0].kind == Kind::thugs, "the enemies in play did not become the crime");
        check(run.realSpawned() == 1 && run.sites()[0].thugs == 3 && run.sites()[0].grounded,
              "the biggest group (3) was not the crime");
        check(across(run.sites()[0].point, {90.7f, 0, 9.7f}) < 1, "the crime was not at its group's middle");
        // Knocked out from afar does not count: nobody reached it.
        std::vector<Vec3> none;
        Census empty{none.data(), 0, true};
        play(run, 2, {}, empty, ev);
        check(run.siteCount() == 1 && run.sites()[0].kind == Kind::beacon,
              "a crime that emptied before the player came was not left as a beacon");
        check(count(ev, What::stopped) == 0, "a crime emptied from afar counted as a stop");
    }
    {
        Run run;
        std::vector<Event> ev;
        run.start(quick(), {}, 5, ev);
        std::vector<Vec3> thugs = {{90, 2, 10}, {94, 2, 12}};
        play(run, 1.05f, {}, Census{thugs.data(), thugs.size(), true}, ev);
        // The player swings in: reached within 30 m.
        play(run, .5f, {80, 3, 10}, Census{thugs.data(), thugs.size(), true}, ev);
        check(count(ev, What::reached) == 1 && run.sites()[0].thugs == 2, "the crime was not reached");
        // One down: still a crime.
        thugs.pop_back();
        play(run, 2, {85, 2, 10}, Census{thugs.data(), thugs.size(), true}, ev);
        check(run.siteCount() == 1 && run.sites()[0].thugs == 1, "one enemy left did not keep the crime open");
        // The last down: 1.5 s later it is stopped, with 200 for each of the two it had.
        thugs.clear();
        play(run, 1.0f, {85, 2, 10}, Census{thugs.data(), 0, true}, ev);
        check(run.siteCount() == 1, "the crime closed before clearSeconds");
        play(run, .7f, {85, 2, 10}, Census{thugs.data(), 0, true}, ev);
        check(count(ev, What::stopped) == 1, "clearing the enemies did not stop the crime");
        const Event* st = last(ev, What::stopped);
        check(st->kind == Kind::thugs && st->points % 10 == 0 && st->points >= 1400, "a real crime's points lacked its enemies");
    }
    // ---- A stale census: a real crime becomes a beacon ----
    {
        Run run;
        std::vector<Event> ev;
        run.start(quick(), {}, 5, ev);
        std::vector<Vec3> thugs = {{90, 2, 10}};
        play(run, 1.05f, {}, Census{thugs.data(), thugs.size(), true}, ev);
        play(run, 6.5f, {}, Census{}, ev);
        check(run.siteCount() == 1 && run.sites()[0].kind == Kind::beacon, "a crime with no census for 6 s stayed one");
    }
    // ---- Enemies turning up at a beacon make it a real crime ----
    {
        Run run;
        std::vector<Event> ev;
        run.start(quick(), {}, 77, ev);
        play(run, 1.05f, {}, Census{}, ev);
        const Vec3 p = run.sites()[0].point;
        std::vector<Vec3> thugs = {p + Vec3{5, -8, 0}};
        play(run, .1f, {}, Census{thugs.data(), thugs.size(), true}, ev);
        check(run.sites()[0].kind == Kind::thugs && run.sites()[0].grounded &&
                  std::abs(run.sites()[0].point.y - (p.y - 8)) < .01f,
              "an enemy at a beacon did not make it a crime on his ground");
    }
    // ---- REAL CRIMES off: always beacons ----
    {
        Run run;
        std::vector<Event> ev;
        Config c = quick();
        c.realCrimes = false;
        run.start(c, {}, 5, ev);
        std::vector<Vec3> thugs = {{90, 2, 10}, {94, 2, 12}};
        play(run, 1.05f, {}, Census{thugs.data(), thugs.size(), true}, ev);
        check(run.sites()[0].kind == Kind::beacon, "REAL CRIMES off still used the enemies");
        play(run, 1, run.sites()[0].point, Census{thugs.data(), thugs.size(), true}, ev);
        check(run.realSpawned() == 0, "REAL CRIMES off counted a real crime");
    }
    // ---- Spawns: regular, capped at maxSites, apart from each other ----
    {
        Run run;
        std::vector<Event> ev;
        Config c;
        c.firstSpawnSeconds = 0;
        c.spawnSeconds = 10;
        c.timerSeconds = 600;
        c.maxSites = 3;
        run.start(c, {}, 3, ev);
        play(run, 35, {}, {}, ev);
        check(count(ev, What::spawned) == 3 && run.siteCount() == 3, "spawns were not every 10 s up to 3 open");
        play(run, 20, {}, {}, ev);
        check(count(ev, What::spawned) == 3, "a spawn came with every site taken");
        for (size_t i = 0; i < run.siteCount(); ++i)
            for (size_t j = i + 1; j < run.siteCount(); ++j)
                check(across(run.sites()[i].point, run.sites()[j].point) > 1, "two crimes on one spot");
        check(run.mostUrgent() == &run.sites()[0], "the first crime was not the most urgent");
    }
    // ---- Losing at zero ----
    {
        Run run;
        std::vector<Event> ev;
        Config c = quick();
        c.startScore = 3000;
        c.missPenalty = 2000;
        c.timerSeconds = 20;
        c.spawnSeconds = 10;
        run.start(c, {}, 11, ev);
        play(run, 60, {}, {}, ev);
        check(run.phase() == Phase::lost && run.score() == 0 && count(ev, What::lost) == 1, "the run was not lost at 0");
        check(last(ev, What::missed)->points == -1000, "the last miss took more than the score left");
        const size_t before = ev.size();
        play(run, 10, {}, {}, ev);
        check(ev.size() == before, "a lost run went on");
        check(run.summary().find("LOST") != std::string::npos, "the summary did not say LOST");
    }
    // ---- Winning at the target, ranking up on the way ----
    {
        Run run;
        std::vector<Event> ev;
        Config c = quick();
        c.winStops = 3;
        c.holdSeconds = .5f;
        c.spawnSeconds = 10;
        run.start(c, {}, 21, ev);
        for (int k = 0; k < 3; ++k) {
            while (run.siteCount() == 0 && run.running())
                run.update(1.f / 90, {}, {}, ev);
            const Vec3 p = run.sites()[0].point;
            play(run, .7f, p, {}, ev);
        }
        check(run.phase() == Phase::won && run.stops() == 3 && count(ev, What::won) == 1, "three stops did not win");
        check(run.rank() == Rank::amazing && count(ev, What::ranked) >= 1, "the win was not AMAZING, or no rank-up");
        check(run.summary().find("WON") != std::string::npos && run.summary().find("3/3") != std::string::npos,
              "the summary did not say WON 3/3");
        check(better(run.score(), run.stops(), true, 0, 0) && !better(10, 1, false, 99999, 2),
              "a better run was not told from a worse one");
    }
    // ---- Quitting, and a paused game ----
    {
        Run run;
        std::vector<Event> ev;
        run.start(quick(), {}, 1, ev);
        play(run, 2, {}, {}, ev);
        const float left = run.sites()[0].left;
        run.update(0, {}, {}, ev);
        run.update(-1, {}, {}, ev);
        run.update(NAN, {}, {}, ev);
        check(run.sites()[0].left == left, "a paused frame ran the timer");
        run.update(5, {}, {}, ev); // a hitch: at most a quarter second
        check(left - run.sites()[0].left <= .2501f, "a hitch ran the timer by more than a quarter second");
        run.abandon(ev);
        check(run.phase() == Phase::idle && count(ev, What::abandoned) == 1 && run.siteCount() == 0, "quitting");
        check(run.summary().find("QUIT") != std::string::npos, "the summary of a quit run");
    }
    // ---- The phone: A starts, A twice quits ----
    {
        Toggle t;
        check(t.update(.01f, true, false) == Toggle::Ask::start, "A on the app did not start a run");
        check(t.update(.01f, true, true) == Toggle::Ask::confirm && t.confirming(), "the first A in a run quit it");
        check(t.update(1, false, true) == Toggle::Ask::none, "nothing pressed did something");
        check(t.update(.01f, true, true) == Toggle::Ask::quit, "A again within 3 s did not quit");
        check(t.update(.01f, true, true) == Toggle::Ask::confirm, "a quit did not start over");
        t.update(3.1f, false, true);
        check(!t.confirming() && t.update(.01f, true, true) == Toggle::Ask::confirm, "the confirmation did not lapse");
    }
    // ---- A paused frame (dt <= 0) does not run the 3 s quit window down ----
    {
        Toggle t;
        check(t.update(.01f, true, false) == Toggle::Ask::start, "start for the pause test");
        check(t.update(.01f, true, true) == Toggle::Ask::confirm && t.confirming(), "confirm for the pause test");
        check(t.update(0, false, true) == Toggle::Ask::none && t.confirming(), "dt 0 cleared the quit confirm");
        check(t.update(0, false, true) == Toggle::Ask::none && t.confirming(), "another paused frame cleared it");
        check(t.update(.01f, true, true) == Toggle::Ask::quit, "A after a pause did not quit");
    }
    // ---- Clock text ----
    check(clockText(65) == "1:05" && clockText(0) == "0:00" && clockText(-3) == "0:00" && clockText(59.2f) == "1:00",
          "clock text");
    // ---- The bridge ----
    {
        Ask a;
        check(valid(a), "an empty ask was refused");
        a.count = 1;
        a.ids[0] = 3;
        a.x[0] = 10;
        a.z[0] = 20;
        a.top[0] = 300;
        check(valid(a), "a good ask was refused");
        a.ids[0] = 0;
        check(!valid(a), "a site with id 0 was taken");
        a.ids[0] = 3;
        a.top[0] = NAN;
        check(!valid(a), "a ray from no height was taken");
        a.top[0] = 1;
        a.count = 5;
        check(!valid(a), "five sites were taken");
        a.count = 1;
        a.version = 2;
        check(!valid(a), "another version was taken");
        Live l;
        check(valid(l), "an empty account was refused");
        l.count = 17;
        check(!valid(l), "seventeen enemies were taken");
    }
    // ---- Drawing ----
    {
        std::vector<Vertex> out;
        BeaconView b;
        b.point = {0, 0, -200};
        b.grounded = true;
        b.left = 30;
        b.total = 120;
        b.zone = 15;
        appendBeacon(out, b, {0, 1.7f, 0}, .001f);
        check(!out.empty() && out.size() % 3 == 0, "a beacon drew no triangles");
        bool far = false, finiteAll = true;
        for (const auto& v : out) {
            far |= length(v.position - Vec3{0, 1.7f, 0}) > drawReach + 1;
            finiteAll &= finite(v.position) && std::isfinite(v.alpha);
        }
        check(!far && finiteAll, "a beacon 200 m off was drawn past drawReach");
        out.clear();
        b.point = {0, 0, -40};
        appendBeacon(out, b, {0, 1.7f, 0}, .001f);
        float lowest = 1e9f;
        for (const auto& v : out)
            lowest = std::min(lowest, v.position.y);
        check(lowest < 0, "a near beacon did not reach its ground");
        out.clear();
        b.point = {NAN, 0, 0};
        appendBeacon(out, b, {}, .001f);
        check(out.empty(), "a beacon at no point drew");
        HudView h;
        Pose head{{0, 1.7f, 0}, {}};
        appendHud(out, h, head);
        check(out.empty(), "a hidden HUD drew");
        h.shown = true;
        h.phase = Phase::running;
        h.score = 20000;
        h.winStops = 20;
        h.target = true;
        h.targetPoint = {-100, 0, 0};
        h.targetLeft = 45;
        std::snprintf(h.flash, sizeof h.flash, "CRIME STOPPED +1,450");
        h.flashAge = 1;
        appendHud(out, h, head);
        check(!out.empty() && out.size() % 3 == 0, "the HUD drew nothing");
        bool ahead = true;
        for (const auto& v : out)
            ahead &= v.position.z < -1 && v.position.z > -2 && v.position.y < 1.7f;
        check(ahead, "the HUD was not below the gaze, ahead");
        Run run;
        std::vector<Event> ev;
        run.start(quick(), {}, 1, ev);
        play(run, 1.05f, {}, {}, ev);
        const HudView v = hudOf(run, {});
        check(v.shown && v.target && v.score == 20000 && v.winStops == 20, "the HUD's view of a run");
    }
    // ---- The settings: off by default, every key kept through spidy_vr.ini ----
    {
        using namespace spidy::vr_settings;
        const Values d;
        check(!d.crimeRun && d.crimeStartScore == 20000 && d.crimeRealCrimes && d.crimeHud && d.crimeBest == 0,
              "CRIME RUN's defaults (off, 20,000, real crimes, HUD)");
        Values v;
        v.crimeRun = true;
        v.crimeStartScore = 30000;
        v.crimeWinStops = 7;
        v.crimeRadiusMinM = 200;
        v.crimeRadiusMaxM = 120; // under the inner one: kept 10 m past it
        v.crimeRealCrimes = false;
        v.crimeBest = 41230;
        v.crimeBestStops = 9;
        const Values back = withTabOnly(Values{}, tabOnlyText(v));
        check(back.crimeRun && back.crimeStartScore == 30000 && back.crimeWinStops == 7 && !back.crimeRealCrimes &&
                  back.crimeBest == 41230 && back.crimeBestStops == 9,
              "CRIME RUN's settings did not survive spidy_vr.ini");
        check(back.crimeRadiusMaxM >= back.crimeRadiusMinM + 10, "the ring was saved inside out");
        const std::string text = tabOnlyText(Values{});
        check(text.find("crime_run=0\n") != std::string::npos, "the ini did not carry crime_run=0");
    }
    // ---- The phone: a fifth app only while the mode is on ----
    {
        PhoneUi ui;
        ui.reset();
        using E = PhoneControls::Event;
        check(ui.apps() == phoneApps, "the phone had more than its four apps with the mode off");
        for (int i = 0; i < 4; ++i)
            ui.update(E{0, -1}, 0);
        check(ui.app() == 0, "four steps down did not wrap to MUSIC with four apps");
        ui.setCrimeApp(true);
        check(ui.apps() == phoneApps + 1, "the mode on did not add CRIME RUN");
        for (int i = 0; i < 4; ++i)
            ui.update(E{0, -1}, 0);
        check(ui.app() == phoneCrimeApp, "four steps down did not reach CRIME RUN");
        check(ui.update(E{0, 0, true}, 0) == PhoneAction::crimeRun && ui.screen() == PhoneScreen::home,
              "A on CRIME RUN did not ask for the run");
        ui.setCrimeApp(false);
        check(ui.app() == 0 && ui.apps() == phoneApps, "the mode off left CRIME RUN picked");
        // Its card draws.
        PhoneView view;
        view.open = true;
        view.crimeApp = true;
        view.app = phoneCrimeApp;
        std::vector<Vertex> screen;
        appendPhoneScreen(screen, view);
        PhoneView four = view;
        four.crimeApp = false;
        four.app = 0;
        std::vector<Vertex> fewer;
        appendPhoneScreen(fewer, four);
        check(screen.size() > fewer.size(), "the fifth app drew nothing");
    }
    std::printf("%d/%d crime run checks passed\n", passed, passed);
    return 0;
}
