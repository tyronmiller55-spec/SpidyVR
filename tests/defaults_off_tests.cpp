// Off equals the previous build: beta3g's new switches come on once at revision 23,
// a 0 chosen after that stays, rope weight 200% becomes 100%, Crime Run stays off
// with no revision bump (missing keys take the struct defaults), and revision 24
// turns WEB GRIP, PUNCH FISTS and WEB IN HAND on once (THICK SWING LINE was removed in beta3n), and
// revision 25 (beta3n) turns WALL RUN PUSH, WALL FLING ANY WAY, STICKY PALMS and
// GRIP HOLDS THE HANDSTAND on once, and revision 26 (beta3o) WALL SLIDE POSE and
// GRIP LOCKS THE WALL JUMP and ROPE GRIP, and revision 27 (beta3p) LEVEL KEEPS HIM WITH YOU and HEAD LEVEL
// as the default (a saved FULL becomes LEVEL once).
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
    check(movementDefaultsRevision == 27, "this tree is not at revision 27");
    check(!Values{}.crimeRun && Values{}.crimeStartScore == 20000 && Values{}.crimeHud && Values{}.crimeRealCrimes,
          "Crime Run's defaults (off, 20,000, HUD, real crimes)");
    const std::string onlyRev = "movement_defaults=23\n";
    const Values fromFile = withMovementDefaults(withTabOnly({}, onlyRev), onlyRev);
    check(!fromFile.crimeRun && fromFile.crimeStartScore == 20000,
          "a revision 23 file without crime_* keys did not stay off");
    check(fromFile.animatedWebGrip && fromFile.animatedPunchFists && fromFile.webHoldInHand,
          "revision 24 did not turn WEB GRIP, PUNCH FISTS and WEB IN HAND on once");
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

    Values already;
    already.animatedWebGrip = already.animatedPunchFists = already.webHoldInHand = false;
    const Values chosen24 = withMovementDefaults(already, "movement_defaults=24\n");
    check(!chosen24.animatedWebGrip && !chosen24.animatedPunchFists && !chosen24.webHoldInHand,
          "a 0 chosen after revision 24 was overwritten");

    const std::string text = tabOnlyText(Values{});
    check(text.find("crime_run=0\n") != std::string::npos, "the defaults' text did not carry crime_run=0");
    check(text.find("movement_defaults=27\n") != std::string::npos, "the defaults' text did not carry movement_defaults=27");

    // Revision 25 (beta3n): the walls' feel comes on once; a 0 chosen after it stays; off is beta3m's wall.
    const std::string rev24 = "movement_defaults=24\nwall_run_push=0\nwall_fling_any_way=0\nsticky_palms=0\nhandstand_grip_hold=0\n";
    const Values n = withMovementDefaults(withTabOnly({}, rev24), rev24);
    check(n.wallRunPush && n.wallRunBlendMs == 250, "revision 25 did not turn WALL RUN PUSH on once");
    check(n.wallFlingAnyWay, "revision 25 did not turn WALL FLING ANY WAY on once");
    check(n.stickyPalms && n.stickyPalmsPct == 100, "revision 25 did not turn STICKY PALMS on once");
    check(n.handstandGripHold, "revision 25 did not turn GRIP HOLDS THE HANDSTAND on once");
    const std::string kept25 = "movement_defaults=25\nwall_run_push=0\nwall_run_blend_ms=0\nwall_fling_any_way=0\nsticky_palms=0\nsticky_palms_pct=30\nhandstand_grip_hold=0\n";
    const Values k = withMovementDefaults(withTabOnly({}, kept25), kept25);
    check(!k.wallRunPush && k.wallRunBlendMs == 0 && !k.wallFlingAnyWay && !k.stickyPalms && k.stickyPalmsPct == 30 && !k.handstandGripHold, "a 0 chosen after revision 25 was overwritten");
    check(withTabOnly({}, "wall_run_blend_ms=5000\n").wallRunBlendMs == 1000, "the blend is not held to 1000 ms");

    // Revision 26 (beta3o): WALL SLIDE POSE and GRIP LOCKS THE WALL JUMP come on once; a 0 chosen after it stays.
    const std::string rev25 = "movement_defaults=25\nwall_slide_pose=0\nwall_grip_jump_lock=0\nweb_rope_grip=0\n";
    const Values r26 = withMovementDefaults(withTabOnly({}, rev25), rev25);
    check(r26.wallSlidePose, "revision 26 did not turn WALL SLIDE POSE on once");
    check(r26.wallGripJumpLock, "revision 26 did not turn GRIP LOCKS THE WALL JUMP on once");
    check(r26.webRopeGrip, "revision 26 did not turn ROPE GRIP on once");
    const std::string kept26 = "movement_defaults=26\nwall_slide_pose=0\nwall_grip_jump_lock=0\nweb_rope_grip=0\n";
    const Values k26 = withMovementDefaults(withTabOnly({}, kept26), kept26);
    check(!k26.wallSlidePose && !k26.wallGripJumpLock && !k26.webRopeGrip, "a 0 chosen after revision 26 was overwritten");

    // Revision 27 (beta3p): LEVEL KEEPS HIM WITH YOU comes on once; a 0 chosen after it stays.
    const std::string rev26 = "movement_defaults=26\nlevel_head_lock=0\n";
    check(withMovementDefaults(withTabOnly({}, rev26), rev26).levelHeadLock,
          "revision 27 did not turn LEVEL KEEPS HIM WITH YOU on once");
    const std::string kept27 = "movement_defaults=27\nlevel_head_lock=0\n";
    check(!withMovementDefaults(withTabOnly({}, kept27), kept27).levelHeadLock,
          "a 0 chosen after revision 27 was overwritten");
    // Revision 27 (beta3p): HEAD LEVEL is the default; a saved FULL becomes LEVEL once, OFF stays, and FULL
    // chosen after it stays.
    check(Values{}.animatedHead == 1, "HEAD's default is not LEVEL");
    const std::string full26 = "movement_defaults=26\nanimated_head=2\n";
    check(withMovementDefaults(withTabOnly({}, full26), full26).animatedHead == 1,
          "revision 27 did not turn a saved FULL into LEVEL");
    const std::string off26 = "movement_defaults=26\nanimated_head=0\n";
    check(withMovementDefaults(withTabOnly({}, off26), off26).animatedHead == 0, "revision 27 changed a saved OFF");
    const std::string full27 = "movement_defaults=27\nanimated_head=2\n";
    check(withMovementDefaults(withTabOnly({}, full27), full27).animatedHead == 2,
          "a FULL chosen after revision 27 was overwritten");

    // Off of Crime Run matches "no mode": the previous build has no run, no HUD key, no fifth app.
    Values off;
    off.crimeRun = false;
    check(off.crimeRun == Values{}.crimeRun, "crime_run off was not the default");
    std::printf("%d/%d defaults-off checks passed\n", passed, passed);
    return 0;
}
