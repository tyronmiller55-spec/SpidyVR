// Off equals the previous build: beta3g's new switches come on once at revision 23,
// a 0 chosen after that stays, rope weight 200% becomes 100%, and Crime Run stays off
// with no revision bump (missing keys take the struct defaults).
#include "spidy/vr_settings.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

using spidy::vr_settings::Values;
using spidy::vr_settings::movementDefaultsRevision;
using spidy::vr_settings::withMovementDefaults;
using spidy::vr_settings::withTabOnly;
using spidy::vr_settings::tabOnlyText;

int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}

int main() {
    check(movementDefaultsRevision == 23, "this tree is not at revision 23");
    check(!Values{}.crimeRun && Values{}.crimeStartScore == 20000 && Values{}.crimeHud && Values{}.crimeRealCrimes,
          "Crime Run's defaults (off, 20,000, HUD, real crimes)");
    const std::string onlyRev = "movement_defaults=23\n";
    const Values fromFile = withMovementDefaults(withTabOnly({}, onlyRev), onlyRev);
    check(!fromFile.crimeRun && fromFile.crimeStartScore == 20000,
          "a revision 23 file without crime_* keys did not stay off");
    const std::string prior = "movement_defaults=22\n";
    check(!withMovementDefaults(withTabOnly({}, prior), prior).crimeRun,
          "an older file turned Crime Run on (it has no migration and must stay off)");

    const std::string rev22 = "movement_defaults=22\nswing_rope_weight_pct=200\n";
    const Values g = withMovementDefaults(withTabOnly({}, rev22), rev22);
    check(g.swingRopeStable && g.swingRopeLoadTaut && g.webWrapKeep,
          "revision 23 did not turn the new rope switches on once");
    check(g.kickTargetRing && g.kickGroundLunge && g.kickLean && g.kickMomentum && g.kickRebound && g.kickRopeAssist &&
              g.kickChamber,
          "revision 23 did not turn the new kick switches on once");
    check(g.kickGroundSpeedCms == 450, "revision 23 did not raise the ground kick speed once");
    check(g.swingRopeWeightPct == 100, "beta3f's 200% weight did not become the new 100%");

    const std::string kept = "swing_rope_stable=0\nkick_chamber=0\nkick_ground_speed_cms=300\nmovement_defaults=23\n";
    const Values chosen = withMovementDefaults(withTabOnly({}, kept), kept);
    check(!chosen.swingRopeStable && !chosen.kickChamber && chosen.kickGroundSpeedCms == 300,
          "a 0 or a tuned value chosen after revision 23 was overwritten");

    // A file that already sat at 23 with the old 200 default was rescaled; a later 80 stays.
    const std::string later = "swing_rope_weight_pct=80\nmovement_defaults=23\n";
    check(withMovementDefaults(withTabOnly({}, later), later).swingRopeWeightPct == 80,
          "a rope weight chosen after revision 23 was rescaled");

    const std::string text = tabOnlyText(Values{});
    check(text.find("crime_run=0\n") != std::string::npos, "the defaults' text did not carry crime_run=0");
    check(text.find("movement_defaults=23\n") != std::string::npos, "the defaults' text did not carry movement_defaults=23");

    // Off of Crime Run matches "no mode": the previous build has no run, no HUD key, no fifth app.
    Values off;
    off.crimeRun = false;
    check(off.crimeRun == Values{}.crimeRun, "crime_run off was not the default");
    std::printf("%d/%d defaults-off checks passed\n", passed, passed);
    return 0;
}
