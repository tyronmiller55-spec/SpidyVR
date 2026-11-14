#include "spidy/vr_settings.hpp"
#include "spidy/camera_motion.hpp"
#include "spidy/shooter_lock.hpp"
#include "spidy/recording_fov.hpp"
#include "spidy/seated.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <span>

namespace spidy::vr_settings {
namespace {
constexpr const char* swingChoices[] = {"10 M/S", "15 M/S", "20 M/S", "25 M/S", "32 M/S",
                                        "40 M/S", "48 M/S", "56 M/S", "65 M/S"};
constexpr const char* snapChoices[] = {"OFF", "15\xC2\xB0", "30\xC2\xB0", "45\xC2\xB0", "60\xC2\xB0", "90\xC2\xB0"};
constexpr const char* smoothChoices[] = {"OFF",          "60\xC2\xB0/S",  "90\xC2\xB0/S",
                                         "120\xC2\xB0/S", "180\xC2\xB0/S", "240\xC2\xB0/S"};
constexpr const char* hapticChoices[] = {"OFF", "25%", "50%", "75%", "100%"};
constexpr const char* weightChoices[] = {"40%", "60%", "80%", "100%", "125%", "150%", "200%", "250%", "300%"};
constexpr const char* screenChoices[] = {"SMALL", "MEDIUM", "LARGE"};
// CALIBRATE BODY, a switch with names of its own: ON RESUME asks for the T-pose
// as soon as play resumes.
constexpr const char* calibrateChoices[] = {"NO", "ON RESUME"};
// WEB BUTTON, a switch named by the button that webs: TRIGGER swaps it with the grip.
constexpr const char* webButtonChoices[] = {"GRIP", "TRIGGER"};
// WEB STYLE, a switch named by how webs behave: ARCADE passes thin things by.
constexpr const char* webStyleChoices[] = {"REALISTIC", "ARCADE"};
// VIEW, a switch named by what the headset shows: GAME SCREEN is the game as
// it is, its own picture on the screen ahead, as menus are, in 3D.
constexpr const char* viewChoices[] = {"VR", "GAME SCREEN"};
constexpr const char* hudChoices[] = {"OFF", "SMALL", "MEDIUM", "LARGE"};
constexpr const char* flipSpeedChoices[] = {"90\xC2\xB0/S",  "120\xC2\xB0/S", "150\xC2\xB0/S", "180\xC2\xB0/S",
                                            "240\xC2\xB0/S", "300\xC2\xB0/S", "360\xC2\xB0/S", "480\xC2\xB0/S"};
constexpr const char* depthChoices[] = {"OFF", "LOW", "MEDIUM", "HIGH"};
constexpr const char* nearClipChoices[] = {"1 CM", "1.5 CM", "2 CM", "3 CM", "5 CM", "7.5 CM", "10 CM"};
static_assert(std::size(nearClipChoices) == std::size(nearClips));
// RECORDING FOV: the least the monitor's view shows across (recording_fov.hpp).
constexpr const char* recordingFovChoices[] = {"OFF",          "70\xC2\xB0",  "90\xC2\xB0",
                                               "110\xC2\xB0", "120\xC2\xB0", "130\xC2\xB0",
                                               "140\xC2\xB0", "150\xC2\xB0", "160\xC2\xB0"};
static_assert(std::size(recordingFovChoices) == std::size(recording_fov::choices));
// UI STYLE, a list (ui_style.hpp's UiStyle in order) so that a style can
// come after them: COMIC, the printed marks, or TECH, the game's own look.
constexpr int uiStyles[] = {0, 1};
constexpr const char* uiStyleChoices[] = {"COMIC", "TECH"};
static_assert(std::size(uiStyleChoices) == std::size(uiStyles));
// BLOW SOUNDS: the game's own melee impacts, Spidy's own tones, or none.
constexpr int blowSoundSteps[] = {0, 1, 2};
constexpr const char* blowSoundChoices[] = {"GAME", "SPIDY", "OFF"};
static_assert(std::size(blowSoundChoices) == std::size(blowSoundSteps));
// EYE AIM: what the eyes aim (Values::eyeAim's bits, eyeAimShots and eyeAimZips).
constexpr int eyeAimSteps[] = {0, eyeAimShots, eyeAimZips, eyeAimShots | eyeAimZips};
constexpr const char* eyeAimChoices[] = {"OFF", "WEB BALLS", "ZIPS", "BOTH"};
static_assert(std::size(eyeAimChoices) == std::size(eyeAimSteps));
// HEIGHT: AUTO, then the player's own.
constexpr const char* heightChoices[] = {"AUTO",   "150 CM", "160 CM", "165 CM", "170 CM",
                                         "175 CM", "180 CM", "185 CM", "190 CM"};
static_assert(std::size(heightChoices) == std::size(heights));
// The realistic flips' rows (RealisticFlipSettings).
constexpr int automaticStrengths[] = {25, 50, 75, 100};
constexpr const char* automaticStrengthChoices[] = {"25%", "50%", "75%", "100%"};
constexpr int automaticSpeeds[] = {90, 120, 180, 240, 360};
constexpr const char* automaticSpeedChoices[] = {"90\xC2\xB0/S", "120\xC2\xB0/S", "180\xC2\xB0/S",
                                                 "240\xC2\xB0/S", "360\xC2\xB0/S"};
constexpr int automaticMinimumSpeeds[] = {3, 5, 8, 12, 16};
constexpr const char* automaticMinimumChoices[] = {"3 M/S", "5 M/S", "8 M/S", "12 M/S", "16 M/S"};
constexpr int automaticResponses[] = {100, 200, 350, 500, 750};
constexpr const char* automaticResponseChoices[] = {"100 MS", "200 MS", "350 MS", "500 MS", "750 MS"};
constexpr int automaticHandStrengths[] = {0, 25, 50, 75, 100, 150, 200};
constexpr const char* automaticHandStrengthChoices[] = {"0%", "25%", "50%", "75%", "100%", "150%", "200%"};
constexpr int automaticFalloffs[] = {500, 1000, 1500, 2000, 3000};
constexpr const char* automaticFalloffChoices[] = {"FAST", "QUICK", "MEDIUM", "SLOW", "VERY SLOW"};
constexpr int automaticHandThresholds[] = {50, 100, 150, 200, 300};
constexpr const char* automaticHandThresholdChoices[] = {"0.5 M/S", "1 M/S", "1.5 M/S", "2 M/S", "3 M/S"};
static_assert(std::size(automaticStrengthChoices) == std::size(automaticStrengths) &&
              std::size(automaticSpeedChoices) == std::size(automaticSpeeds) &&
              std::size(automaticMinimumChoices) == std::size(automaticMinimumSpeeds) &&
              std::size(automaticResponseChoices) == std::size(automaticResponses) &&
              std::size(automaticHandStrengthChoices) == std::size(automaticHandStrengths) &&
              std::size(automaticFalloffChoices) == std::size(automaticFalloffs) &&
              std::size(automaticHandThresholdChoices) == std::size(automaticHandThresholds));
constexpr int screenSizes[] = {0, 1, 2};
constexpr int hudSizes[] = {0, 1, 2, 3};
constexpr int depthSteps[] = {0, 1, 2, 3};
// A level: OFF, then LOW, MEDIUM and HIGH (ELASTIC WEB, SOFT RELEASE).
constexpr int levelSteps[] = {0, 1, 2, 3};
constexpr const char* levelChoices[] = {"OFF", "LOW", "MEDIUM", "HIGH"};
static_assert(std::size(swingChoices) == std::size(swingSpeeds) && std::size(snapChoices) == std::size(snapTurns) &&
              std::size(smoothChoices) == std::size(smoothTurns) &&
              std::size(hapticChoices) == std::size(hapticLevels) && std::size(screenChoices) == std::size(screenSizes) &&
              std::size(weightChoices) == std::size(weights) && std::size(hudChoices) == std::size(hudSizes) &&
              std::size(flipSpeedChoices) == std::size(flipSpeeds) &&
              std::size(depthChoices) == std::size(depthSteps) && std::size(depthSteps) == std::size(screenDepths));
static_assert(std::size(levelChoices) == std::size(levelSteps));
// SLINGSHOT SPEED: 0 keeps the launch under the swing's speed limit.
constexpr int slingshotSpeeds[] = {0, 20, 40, 50, 65, 100, 150, 200, 300};
constexpr const char* slingshotSpeedChoices[] = {"SWING LIMIT", "20 M/S",  "40 M/S",  "50 M/S", "65 M/S",
                                                 "100 M/S",     "150 M/S", "200 M/S", "300 M/S"};
static_assert(std::size(slingshotSpeedChoices) == std::size(slingshotSpeeds));
// RECORDING SMOOTHING: off, then slower and slower turns.
constexpr int recordingSmoothings[] = {0, 10, 20, 30, 40, 50, 60, 80, 100};
constexpr const char* recordingSmoothingChoices[] = {"OFF", "10", "20", "30", "40", "50", "60", "80", "100"};
static_assert(std::size(recordingSmoothingChoices) == std::size(recordingSmoothings));
// WEB RANGE, m, and WEB SPEED (off, then slow to fast).
constexpr int webRanges[] = {10, 25, 50, 100, 150, 200, 300, 500, 1000};
constexpr const char* webRangeChoices[] = {"10 M",  "25 M",  "50 M",  "100 M", "150 M",
                                           "200 M", "300 M", "500 M", "1000 M"};
static_assert(std::size(webRangeChoices) == std::size(webRanges));
constexpr int webSpeeds[] = {0, 1, 2, 3};
constexpr const char* webSpeedChoices[] = {"OFF", "SLOW", "MEDIUM", "FAST"};
static_assert(std::size(webSpeedChoices) == std::size(webSpeeds));
constexpr int singleHandYankStrengths[] = {50, 75, 100, 125, 150, 200, 250, 300};
constexpr const char* singleHandYankStrengthChoices[] = {"50%",  "75%",  "100%", "125%",
                                                         "150%", "200%", "250%", "300%"};
static_assert(std::size(singleHandYankStrengthChoices) == std::size(singleHandYankStrengths));
constexpr int webAngles[] = {-45, -30, -15, 0, 15, 30, 45};
constexpr const char* webAngleChoices[] = {"DOWN 45", "DOWN 30", "DOWN 15", "0", "UP 15", "UP 30", "UP 45"};
static_assert(std::size(webAngleChoices) == std::size(webAngles));
constexpr int dodgeTumbles[] = {0, 1, 2};
constexpr const char* dodgeTumbleChoices[] = {"OFF", "LEAN", "ROLL"};
static_assert(std::size(dodgeTumbleChoices) == std::size(dodgeTumbles));
constexpr int animatedHeads[] = {0, 1, 2};
constexpr const char* animatedHeadChoices[] = {"OFF", "LEVEL", "FULL"};
static_assert(std::size(animatedHeadChoices) == std::size(animatedHeads));
constexpr int animatedHeadsetSmooths[] = {0, 1, 2, 3, 4};
constexpr const char* animatedHeadsetSmoothChoices[] = {"OFF", "LOW", "MEDIUM", "HIGH", "MAX"};
static_assert(std::size(animatedHeadsetSmoothChoices) == std::size(animatedHeadsetSmooths));
// CAMERA SMOOTHING's response times (ms) and STABILIZATION's banks.
constexpr int cameraSmoothSteps[] = {0, 25, 50, 75, 100, 150, 200, 300, 500};
constexpr const char* cameraSmoothChoices[] = {"OFF",    "25 MS",  "50 MS",  "75 MS", "100 MS",
                                               "150 MS", "200 MS", "300 MS", "500 MS"};
static_assert(std::size(cameraSmoothChoices) == std::size(cameraSmoothSteps));
constexpr int cameraStabilizations[] = {0, 1, 2};
constexpr const char* cameraStabilizationChoices[] = {"OFF", "LIGHT", "HEAVY"};
static_assert(std::size(cameraStabilizationChoices) == std::size(cameraStabilizations));
// SHOOTERS (shooter_lock::Mode) and LOCK STRENGTH, percent.
constexpr int shooterModes[] = {0, 1, 2};
constexpr const char* shooterModeChoices[] = {"CONTROLLER", "LOCKED TO ARM", "PALM"};
static_assert(std::size(shooterModeChoices) == std::size(shooterModes));
constexpr int shooterLockSteps[] = {25, 50, 75, 90, 100};
constexpr const char* shooterLockChoices[] = {"25%", "50%", "75%", "90%", "100%"};
static_assert(std::size(shooterLockChoices) == std::size(shooterLockSteps));
// CAMERA MOTION's presets (camera_motion::presets, then CUSTOM), its amounts
// (percent of the animation's motion) and HEAD ANGLE (degrees).
constexpr int cameraMotions[] = {0, 1, 2, 3, 4};
constexpr const char* cameraMotionChoices[] = {"FULL", "LIGHT", "HEAVY", "COMFORT", "CUSTOM"};
static_assert(std::size(cameraMotionChoices) == std::size(cameraMotions) &&
              std::size(cameraMotions) == camera_motion::presetCount + 1);
constexpr int cameraAmountSteps[] = {0, 15, 25, 35, 50, 65, 75, 90, 100};
constexpr const char* cameraAmountChoices[] = {"0% (STILL)", "15%", "25%", "35%", "50%",
                                               "65%",        "75%", "90%", "100% (FULL)"};
static_assert(std::size(cameraAmountChoices) == std::size(cameraAmountSteps));
constexpr int cameraHeadAngles[] = {-30, -20, -10, -5, 0, 5, 10, 20, 30};
constexpr const char* cameraHeadAngleChoices[] = {"DOWN 30", "DOWN 20", "DOWN 10", "DOWN 5", "LEVEL",
                                                  "UP 5",    "UP 10",   "UP 20",   "UP 30"};
static_assert(std::size(cameraHeadAngleChoices) == std::size(cameraHeadAngles));
// LEG CONTROL's springs and coupling, percent.
constexpr int legSpringSteps[] = {25, 50, 75, 100, 150, 200};
constexpr const char* legSpringChoices[] = {"25%", "50%", "75%", "100%", "150%", "200%"};
static_assert(std::size(legSpringChoices) == std::size(legSpringSteps));
constexpr int legCouplingSteps[] = {0, 25, 50, 75, 100};
constexpr const char* legCouplingChoices[] = {"OFF", "25%", "50%", "75%", "100%"};
static_assert(std::size(legCouplingChoices) == std::size(legCouplingSteps));
constexpr int runSpeeds[] = {50, 75, 100, 125, 150, 175, 200};
constexpr const char* runSpeedChoices[] = {"50%", "75%", "100%", "125%", "150%", "175%", "200%"};
static_assert(std::size(runSpeedChoices) == std::size(runSpeeds));
constexpr int slowMotionLengths[] = {6, 12, 20, 30, 45, 60, 75, 90, slowMotionEndless};
constexpr const char* slowMotionLengthChoices[] = {"6 S",  "12 S", "20 S", "30 S",    "45 S",
                                                   "60 S", "75 S", "1:30", "INFINITE"};
static_assert(std::size(slowMotionLengthChoices) == std::size(slowMotionLengths));
constexpr int softLocks[] = {0, 5, 10, 15, 20, 25, 30, 40};
constexpr const char* softLockChoices[] = {"OFF", "5 M", "10 M", "15 M", "20 M", "25 M", "30 M", "40 M"};
static_assert(std::size(softLockChoices) == std::size(softLocks));
constexpr int animatedAirSpins[] = {0, 1, 2};
constexpr const char* animatedAirSpinChoices[] = {"OFF", "FLIPS", "FLIPS + MOMENTUM"};
static_assert(std::size(animatedAirSpinChoices) == std::size(animatedAirSpins));
// ANIMATED HYBRID SPEED: OFF, then the speed hold before OG SpidyVR takes over, ms.
constexpr int animatedHybridSpeeds[] = {0, 5, 10, 25, 50, 100, 200, 300, 500};
constexpr const char* animatedHybridSpeedChoices[] = {"OFF",    "5 MS",   "10 MS",  "25 MS", "50 MS",
                                                      "100 MS", "200 MS", "300 MS", "500 MS"};
static_assert(std::size(animatedHybridSpeedChoices) == std::size(animatedHybridSpeeds));
constexpr int nativeRunPercents[] = {50, 60, 70, 80, 90};
constexpr const char* nativeRunPercentChoices[] = {"50%", "60%", "70%", "80%", "90%"};
static_assert(std::size(nativeRunPercentChoices) == std::size(nativeRunPercents));
constexpr int earlyJumps[] = {0, 1, 2};
constexpr const char* earlyJumpChoices[] = {"OFF", "QUICK TAP", "RUNNING"};
static_assert(std::size(earlyJumpChoices) == std::size(earlyJumps));
constexpr int webAimSources[] = {0, 1};
constexpr const char* webAimSourceChoices[] = {"CONTROLLER", "MODEL FOREARM"};
static_assert(std::size(webAimSourceChoices) == std::size(webAimSources));
constexpr int forearmAimLimits[] = {35, 60, 90, 180};
constexpr const char* forearmAimLimitChoices[] = {"35 DEG", "60 DEG", "90 DEG", "NO LIMIT"};
static_assert(std::size(forearmAimLimitChoices) == std::size(forearmAimLimits));
constexpr int webLandingImpactSteps[] = {12, 16, 20, 24, 28, 32, 40};
constexpr const char* webLandingImpactChoices[] = {"12 M/S", "16 M/S", "20 M/S", "24 M/S", "28 M/S", "32 M/S", "40 M/S"};
static_assert(std::size(webLandingImpactChoices) == std::size(webLandingImpactSteps));
constexpr int forearmBlends[] = {0, 25, 50, 75, 100};
constexpr const char* forearmBlendChoices[] = {"0%", "25%", "50%", "75%", "100%"};
static_assert(std::size(forearmBlendChoices) == std::size(forearmBlends));
// POINT JUMP (point_jump.hpp): its trigger, speeds (m/s), charge time (ms)
// and surface push (percent).
constexpr int pointJumpTriggers[] = {0, 1};
constexpr const char* pointJumpTriggerChoices[] = {"A+B TOGETHER", "HOLD B, THEN A"};
static_assert(std::size(pointJumpTriggerChoices) == std::size(pointJumpTriggers));
constexpr int pointJumpMinSpeeds[] = {6, 7, 8, 9, 10, 12, 14};
constexpr const char* pointJumpMinSpeedChoices[] = {"6 M/S", "7 M/S", "8 M/S", "9 M/S", "10 M/S", "12 M/S", "14 M/S"};
static_assert(std::size(pointJumpMinSpeedChoices) == std::size(pointJumpMinSpeeds));
constexpr int pointJumpMaxSpeeds[] = {14, 16, 18, 20, 22, 26, 30};
constexpr const char* pointJumpMaxSpeedChoices[] = {"14 M/S", "16 M/S", "18 M/S", "20 M/S",
                                                    "22 M/S", "26 M/S", "30 M/S"};
static_assert(std::size(pointJumpMaxSpeedChoices) == std::size(pointJumpMaxSpeeds));
constexpr int pointJumpFulls[] = {500, 750, 1000, 1250, 1500};
constexpr const char* pointJumpFullChoices[] = {"0.5 S", "0.75 S", "1 S", "1.25 S", "1.5 S"};
static_assert(std::size(pointJumpFullChoices) == std::size(pointJumpFulls));
constexpr int pointJumpPushes[] = {0, 10, 25, 40, 60};
constexpr const char* pointJumpPushChoices[] = {"0%", "10%", "25%", "40%", "60%"};
static_assert(std::size(pointJumpPushChoices) == std::size(pointJumpPushes));
// PUNCH VIEW EASE (view_ease.hpp): punch_view_ease_ms; 0 is the old snap.
constexpr int punchViewEases[] = {0, 150, 200, 250, 300, 400};
constexpr const char* punchViewEaseChoices[] = {"OFF (SNAP)", "150 MS", "200 MS", "250 MS", "300 MS", "400 MS"};
static_assert(std::size(punchViewEaseChoices) == std::size(punchViewEases));
// MOVE HAPTICS (move_haptics.hpp): move_haptics_pct, its own strength.
constexpr int moveHapticLevels[] = {0, 25, 50, 75, 100, 125, 150, 175, 200};
constexpr const char* moveHapticChoices[] = {"OFF", "25%", "50%", "75%", "100%", "125%", "150%", "175%", "200%"};
static_assert(std::size(moveHapticChoices) == std::size(moveHapticLevels));
// VIEW YAW SOURCE (stable_yaw.hpp): view_yaw_source.
constexpr int viewYawSources[] = {0, 1};
constexpr const char* viewYawSourceChoices[] = {"BODY (OLD)", "STABLE"};
static_assert(std::size(viewYawSourceChoices) == std::size(viewYawSources));
// JUMP TO SPIDYVR (hybrid_owner.hpp): jump_handoff_ms; 0 stays to the landing.
constexpr int jumpHandoffs[] = {0, 300, 400, 500, 700, 1000};
constexpr const char* jumpHandoffChoices[] = {"STAY TO LANDING", "300 MS", "400 MS", "500 MS", "700 MS", "1000 MS"};
static_assert(std::size(jumpHandoffChoices) == std::size(jumpHandoffs));
// AIR VIEW EASE (view_ease.hpp): air_view_ease_ms; 0 is the old snap.
constexpr int airViewEases[] = {0, 150, 200, 250, 300, 400};
constexpr const char* airViewEaseChoices[] = {"OFF (SNAP)", "150 MS", "200 MS", "250 MS", "300 MS", "400 MS"};
static_assert(std::size(airViewEaseChoices) == std::size(airViewEases));
// ANIM YAW SWAY (view_yaw_lock.hpp): anim_yaw_sway_pct; 100 is the old feel.
constexpr int animYawSways[] = {0, 10, 20, 30, 40, 50, 60, 80, 100};
constexpr const char* animYawSwayChoices[] = {"0% (STEADY)", "10%", "20%", "30%", "40%",
                                              "50%", "60%", "80%", "100% (OLD FEEL)"};
static_assert(std::size(animYawSwayChoices) == std::size(animYawSways));
// TURN-IN-PLACE ANGLE (view_yaw_lock.hpp): turn_in_place_deg; 0 OFF.
constexpr int turnInPlaces[] = {0, 20, 25, 30, 35, 45, 60, 90};
constexpr const char* turnInPlaceChoices[] = {"OFF", "20 DEG", "25 DEG", "30 DEG",
                                              "35 DEG", "45 DEG", "60 DEG", "90 DEG"};
static_assert(std::size(turnInPlaceChoices) == std::size(turnInPlaces));
// PASSIVE LEGS (leg_layers.hpp): passive_legs_pct, the share shown; 0 OFF.
constexpr int passiveLegsSteps[] = {0, 20, 40, 60, 80, 100};
constexpr const char* passiveLegsChoices[] = {"OFF", "20%", "40%", "60%", "80%", "100%"};
static_assert(std::size(passiveLegsChoices) == std::size(passiveLegsSteps));
// PASSIVE COUPLING: passive_coupling_pct, percent of LEG COUPLING.
constexpr int movementModes[] = {0, 1, 2};
constexpr const char* movementModeChoices[] = {"HYBRID", "STABLE", "OLD"};
static_assert(std::size(movementModeChoices) == std::size(movementModes));
constexpr int rearSnaps[] = {0, 100, 150, 200, 300};
constexpr const char* rearSnapChoices[] = {"INSTANT", "100 MS", "150 MS", "200 MS", "300 MS"};
static_assert(std::size(rearSnapChoices) == std::size(rearSnaps));
constexpr int passiveCouplingSteps[] = {0, 5, 10, 15, 25, 50, 100};
constexpr const char* passiveCouplingChoices[] = {"OFF", "5%", "10%", "15%", "25%", "50%", "100%"};
static_assert(std::size(passiveCouplingChoices) == std::size(passiveCouplingSteps));
} // namespace

std::span<const int> steps(Item item) {
    switch (item) {
    case Item::swingSpeed:
        return swingSpeeds;
    case Item::snapTurn:
        return snapTurns;
    case Item::smoothTurn:
        return smoothTurns;
    case Item::haptics:
    case Item::bodyHaptics:
        return hapticLevels;
    case Item::screenSize:
        return screenSizes;
    case Item::weight:
        return weights;
    case Item::hud:
        return hudSizes;
    case Item::flipSpeed:
        return flipSpeeds;
    case Item::screenDepth:
        return depthSteps;
    case Item::elasticWeb:
    case Item::softRelease:
        return levelSteps;
    case Item::slingshotSpeed:
        return slingshotSpeeds;
    case Item::recordingSmoothing:
        return recordingSmoothings;
    case Item::webRange:
        return webRanges;
    case Item::webSpeed:
        return webSpeeds;
    case Item::singleHandYankStrength:
        return singleHandYankStrengths;
    case Item::webAngle:
        return webAngles;
    case Item::dodgeTumble:
        return dodgeTumbles;
    case Item::animatedHead:
        return animatedHeads;
    case Item::animatedHeadsetSmooth:
        return animatedHeadsetSmooths;
    case Item::cameraStabilization:
        return cameraStabilizations;
    case Item::cameraVertical:
    case Item::cameraHorizontal:
    case Item::cameraAngle:
    case Item::lightVertical:
    case Item::lightHorizontal:
    case Item::lightAngle:
    case Item::heavyVertical:
    case Item::heavyHorizontal:
    case Item::heavyAngle:
        return cameraSmoothSteps;
    case Item::legStiffness:
    case Item::legDamping:
        return legSpringSteps;
    case Item::legCoupling:
        return legCouplingSteps;
    case Item::shooterMode:
        return shooterModes;
    case Item::shooterLock:
        return shooterLockSteps;
    case Item::cameraMotion:
        return cameraMotions;
    case Item::cameraPitch:
    case Item::cameraRoll:
    case Item::cameraBobVertical:
    case Item::cameraBobSide:
        return cameraAmountSteps;
    case Item::cameraHeadAngle:
        return cameraHeadAngles;
    case Item::pointJumpTrigger:
        return pointJumpTriggers;
    case Item::punchViewEase:
        return punchViewEases;
    case Item::moveHaptics:
        return moveHapticLevels;
    case Item::viewYawSource:
        return viewYawSources;
    case Item::jumpHandoff:
        return jumpHandoffs;
    case Item::airViewEase:
        return airViewEases;
    case Item::animYawSway:
        return animYawSways;
    case Item::turnInPlace:
        return turnInPlaces;
    case Item::passiveLegs:
        return passiveLegsSteps;
    case Item::passiveCoupling:
        return passiveCouplingSteps;
    case Item::movementMode:
        return movementModes;
    case Item::rearSnap:
        return rearSnaps;
    case Item::pointJumpMinSpeed:
        return pointJumpMinSpeeds;
    case Item::pointJumpMaxSpeed:
        return pointJumpMaxSpeeds;
    case Item::pointJumpFull:
        return pointJumpFulls;
    case Item::pointJumpPush:
        return pointJumpPushes;
    case Item::runSpeed:
        return runSpeeds;
    case Item::slowMotionLength:
        return slowMotionLengths;
    case Item::softLock:
        return softLocks;
    case Item::animatedAirSpin:
        return animatedAirSpins;
    case Item::animatedHybridSpeed:
        return animatedHybridSpeeds;
    case Item::earlyJump:
        return earlyJumps;
    case Item::webAimSource:
        return webAimSources;
    case Item::forearmBlend:
        return forearmBlends;
    case Item::forearmAimLimit:
        return forearmAimLimits;
    case Item::webLandingImpact:
        return webLandingImpactSteps;
    case Item::nativeRunAt:
        return nativeRunPercents;
    case Item::nearClip:
        return nearClips;
    case Item::recordingFov:
        return recording_fov::choices;
    case Item::uiStyle:
        return uiStyles;
    case Item::blowSounds:
        return blowSoundSteps;
    case Item::height:
        return heights;
    case Item::eyeAim:
        return eyeAimSteps;
    case Item::automaticStrength:
        return automaticStrengths;
    case Item::automaticSpeed:
        return automaticSpeeds;
    case Item::automaticMinimumSpeed:
        return automaticMinimumSpeeds;
    case Item::automaticResponse:
        return automaticResponses;
    case Item::automaticHandStrength:
        return automaticHandStrengths;
    case Item::automaticFalloff:
        return automaticFalloffs;
    case Item::automaticHandThreshold:
        return automaticHandThresholds;
    default:
        return {};
    }
}
namespace {
float numberOf(Item item, const Values& v) {
    switch (item) {
    case Item::swingSpeed:
        return v.swingSpeed;
    case Item::snapTurn:
        return static_cast<float>(v.snapTurn);
    case Item::smoothTurn:
        return static_cast<float>(v.smoothTurn);
    case Item::haptics:
        return static_cast<float>(v.haptics);
    case Item::bodyHaptics:
        return static_cast<float>(v.bodyHaptics);
    case Item::screenSize:
        return static_cast<float>(v.screenSize);
    case Item::weight:
        return static_cast<float>(v.weight);
    case Item::hud:
        return static_cast<float>(v.hud);
    case Item::flipSpeed:
        return static_cast<float>(v.flipSpeed);
    case Item::screenDepth:
        return static_cast<float>(v.screenDepth);
    case Item::elasticWeb:
        return static_cast<float>(v.elasticWeb);
    case Item::softRelease:
        return static_cast<float>(v.softRelease);
    case Item::slingshotSpeed:
        return static_cast<float>(v.slingshotSpeed);
    case Item::recordingSmoothing:
        return static_cast<float>(v.recordingSmoothing);
    case Item::webRange:
        return static_cast<float>(v.webRange);
    case Item::webSpeed:
        return static_cast<float>(v.webSpeed);
    case Item::singleHandYankStrength:
        return static_cast<float>(v.singleHandYankPercent);
    case Item::webAngle:
        return static_cast<float>(v.webAngle);
    case Item::dodgeTumble:
        return static_cast<float>(v.dodgeTumble);
    case Item::animatedHead:
        return static_cast<float>(v.animatedHead);
    case Item::animatedHeadsetSmooth:
        return static_cast<float>(v.animatedHeadsetSmooth);
    case Item::cameraStabilization:
        return static_cast<float>(v.cameraStabilization);
    case Item::cameraVertical:
        return static_cast<float>(v.cameraVerticalMs);
    case Item::cameraHorizontal:
        return static_cast<float>(v.cameraHorizontalMs);
    case Item::cameraAngle:
        return static_cast<float>(v.cameraAngleMs);
    case Item::lightVertical:
        return static_cast<float>(v.lightVerticalMs);
    case Item::lightHorizontal:
        return static_cast<float>(v.lightHorizontalMs);
    case Item::lightAngle:
        return static_cast<float>(v.lightAngleMs);
    case Item::heavyVertical:
        return static_cast<float>(v.heavyVerticalMs);
    case Item::heavyHorizontal:
        return static_cast<float>(v.heavyHorizontalMs);
    case Item::heavyAngle:
        return static_cast<float>(v.heavyAngleMs);
    case Item::legStiffness:
        return static_cast<float>(v.legStiffnessPct);
    case Item::legDamping:
        return static_cast<float>(v.legDampingPct);
    case Item::legCoupling:
        return static_cast<float>(v.legCouplingPct);
    case Item::shooterMode:
        return static_cast<float>(shooter_lock::mode(v));
    case Item::shooterLock:
        return static_cast<float>(shooter_lock::strength(v));
    case Item::cameraMotion:
        return static_cast<float>(camera_motion::presetOf(v));
    case Item::cameraPitch:
        return static_cast<float>(v.cameraPitchPct);
    case Item::cameraRoll:
        return static_cast<float>(v.cameraRollPct);
    case Item::cameraBobVertical:
        return static_cast<float>(v.cameraBobVerticalPct);
    case Item::cameraBobSide:
        return static_cast<float>(v.cameraBobSidePct);
    case Item::cameraHeadAngle:
        return static_cast<float>(v.cameraHeadAngleDeg);
    case Item::pointJumpTrigger:
        return static_cast<float>(v.pointJumpTrigger);
    case Item::punchViewEase:
        return static_cast<float>(v.punchViewEaseMs);
    case Item::moveHaptics:
        return static_cast<float>(v.moveHapticsPct);
    case Item::viewYawSource:
        return static_cast<float>(v.viewYawSource);
    case Item::jumpHandoff:
        return static_cast<float>(v.jumpHandoffMs);
    case Item::airViewEase:
        return static_cast<float>(v.airViewEaseMs);
    case Item::animYawSway:
        return static_cast<float>(v.animYawSwayPct);
    case Item::turnInPlace:
        return static_cast<float>(v.turnInPlaceDeg);
    case Item::passiveLegs:
        return static_cast<float>(v.passiveLegsPct);
    case Item::passiveCoupling:
        return static_cast<float>(v.passiveCouplingPct);
    case Item::movementMode:
        return static_cast<float>(v.movementMode);
    case Item::rearSnap:
        return static_cast<float>(v.rearSnapMs);
    case Item::pointJumpMinSpeed:
        return static_cast<float>(v.pointJumpMinSpeed);
    case Item::pointJumpMaxSpeed:
        return static_cast<float>(v.pointJumpMaxSpeed);
    case Item::pointJumpFull:
        return static_cast<float>(v.pointJumpFullMs);
    case Item::pointJumpPush:
        return static_cast<float>(v.pointJumpNormalBias);
    case Item::runSpeed:
        return static_cast<float>(v.runSpeedPercent);
    case Item::slowMotionLength:
        return static_cast<float>(v.slowMotionSeconds);
    case Item::softLock:
        return static_cast<float>(v.softLockMetres);
    case Item::animatedAirSpin:
        return static_cast<float>(v.animatedAirSpin);
    case Item::animatedHybridSpeed:
        return static_cast<float>(v.animatedHybridSpeedMs);
    case Item::earlyJump:
        return static_cast<float>(v.earlyJump);
    case Item::webAimSource:
        return static_cast<float>(v.webAimSource);
    case Item::forearmBlend:
        return static_cast<float>(v.forearmBlend);
    case Item::forearmAimLimit:
        return static_cast<float>(v.forearmAimLimit);
    case Item::webLandingImpact:
        return static_cast<float>(v.webLandingImpact);
    case Item::nativeRunAt:
        return static_cast<float>(v.nativeRunPercent);
    case Item::nearClip:
        return static_cast<float>(v.nearClipMm);
    case Item::recordingFov:
        return static_cast<float>(v.recordingFov);
    case Item::uiStyle:
        return static_cast<float>(v.uiStyle);
    case Item::blowSounds:
        return static_cast<float>(v.blowSounds);
    case Item::height:
        return static_cast<float>(v.heightCm);
    case Item::eyeAim:
        return static_cast<float>(v.eyeAim);
    case Item::automaticStrength:
        return static_cast<float>(v.automaticStrength);
    case Item::automaticSpeed:
        return static_cast<float>(v.automaticSpeed);
    case Item::automaticMinimumSpeed:
        return static_cast<float>(v.automaticMinimumSpeed);
    case Item::automaticResponse:
        return static_cast<float>(v.automaticResponse);
    case Item::automaticHandStrength:
        return static_cast<float>(v.automaticHandStrength);
    case Item::automaticFalloff:
        return static_cast<float>(v.automaticFalloff);
    case Item::automaticHandThreshold:
        return static_cast<float>(v.automaticHandThreshold);
    default:
        return 0;
    }
}
// A switch setting's value (const for const values), or null for a list.
template <class V> auto switchOf(Item item, V& v) -> decltype(&v.aimMarkers) {
    switch (item) {
    case Item::webSlingshot:
        return &v.webSlingshot;
    case Item::betterClimbing:
        return &v.betterClimbing;
    case Item::smartAnchor:
        return &v.smartAnchor;
    case Item::swingAssist:
        return &v.swingAssist;
    case Item::twoHandZip:
        return &v.twoHandZip;
    case Item::ropeSteering:
        return &v.ropeSteering;
    case Item::combatVisuals:
        return &v.combatVisuals;
    case Item::aimMarkers:
        return &v.aimMarkers;
    case Item::airWebs:
        return &v.airWebs;
    case Item::calibrate:
        return &v.calibrate;
    case Item::flips:
        return &v.flips;
    case Item::controlledFlips:
        return &v.controlledFlips;
    case Item::legControl:
        return &v.legControl;
    case Item::webButton:
        return &v.triggerWebs;
    case Item::webStyle:
        return &v.arcadeWebs;
    case Item::handClimb:
        return &v.handClimb;
    case Item::wallRunCameraTilt:
        return &v.wallRunCameraTilt;
    case Item::joystickRunCamera:
        return &v.joystickRunCamera;
    case Item::halfPunchDamage:
        return &v.halfPunchDamage;
    case Item::webTie:
        return &v.webTie;
    case Item::webLines:
        return &v.webLines;
    case Item::airFists:
        return &v.airFists;
    case Item::pointJump:
        return &v.pointJump;
    case Item::view:
        return &v.flatScreen;
    case Item::realisticFlips:
        return &v.realisticFlips;
    case Item::automaticReleaseFlip:
        return &v.automaticReleaseFlip;
    case Item::automaticAirBrake:
        return &v.automaticAirBrake;
    case Item::automaticCombat:
        return &v.automaticCombat;
    case Item::hapticWebs:
        return &v.hapticWebs;
    case Item::hapticWind:
        return &v.hapticWind;
    case Item::hapticCombat:
        return &v.hapticCombat;
    case Item::hapticMovement:
        return &v.hapticMovement;
    case Item::reelHaptics:
        return &v.reelHaptics;
    case Item::swingHaptics:
        return &v.swingHaptics;
    case Item::pcMedia:
        return &v.pcMedia;
    case Item::nativeRun:
        return &v.nativeRun;
    case Item::airSlingshot:
        return &v.airSlingshot;
    case Item::wristShooters:
        return &v.wristShooters;
    case Item::spidyRun:
        return &v.spidyRun;
    case Item::spidyRunFromStanding:
        return &v.spidyRunFromStanding;
    case Item::steadyStates:
        return &v.steadyStates;
    case Item::combatResponsive:
        return &v.combatResponsive;
    case Item::spidyRunJump:
        return &v.spidyRunJump;
    case Item::palmShooters:
        return &v.palmShooters;
    case Item::headProbe:
        return &v.headProbe;
    case Item::soundTrace:
        return &v.soundTrace;
    case Item::legAirCrouch:
        return &v.legAirCrouch;
    case Item::legReachSpread:
        return &v.legReachSpread;
    case Item::animatedMoves:
        return &v.animatedMoves;
    case Item::animatedAir:
        return &v.animatedAir;
    case Item::animatedSteering:
        return &v.animatedSteering;
    case Item::animatedArms:
        return &v.animatedArms;
    case Item::animatedTorso:
        return &v.animatedTorso;
    case Item::animatedModelYaw:
        return &v.animatedModelYaw;
    case Item::animatedOnly:
        return &v.animatedOnly;
    case Item::animatedHybrid:
        return &v.animatedHybrid;
    case Item::webLandingSmooth:
        return &v.webLandingSmooth;
    case Item::softLanding:
        return &v.softLanding;
    case Item::momentumLeap:
        return &v.momentumLeap;
    case Item::runLogs:
        return &v.runLogs;
    case Item::runCurve:
        return &v.runCurve;
    case Item::seated:
        return &v.seated;
    case Item::flipReprojection:
        return &v.flipReprojection;
    case Item::steadyHud:
        return &v.steadyHud;
    default:
        return nullptr;
    }
}
} // namespace

Values sanitized(Values v) {
    v.swingSpeed = std::isfinite(v.swingSpeed) ? std::clamp(v.swingSpeed, 1.f, 65.f) : 32.f;
    v.snapTurn = std::clamp(v.snapTurn, 0, 90);
    v.smoothTurn = std::clamp(v.smoothTurn, 0, 360);
    v.haptics = std::clamp(v.haptics, 0, 100);
    v.bodyHaptics = std::clamp(v.bodyHaptics, 0, 100);
    v.screenSize = std::clamp(v.screenSize, 0, 2);
    v.weight = std::clamp(v.weight, 40, 300);
    v.hud = std::clamp(v.hud, 0, 3);
    v.flipSpeed = std::clamp(v.flipSpeed, 90, 480);
    v.screenDepth = std::clamp(v.screenDepth, 0, 3);
    v.elasticWeb = std::clamp(v.elasticWeb, 0, 3);
    v.softRelease = std::clamp(v.softRelease, 0, 3);
    v.slingshotSpeed = v.slingshotSpeed <= 0 ? 0 : std::clamp(v.slingshotSpeed, 20, 300);
    v.recordingSmoothing = std::clamp(v.recordingSmoothing, 0, 100);
    v.webRange = std::clamp(v.webRange, 10, 1000);
    v.webSpeed = std::clamp(v.webSpeed, 0, 3);
    v.singleHandYankPercent = std::clamp(v.singleHandYankPercent, 50, 300);
    v.webAngle = std::clamp(v.webAngle, -45, 45);
    v.dodgeTumble = std::clamp(v.dodgeTumble, 0, 2);
    v.animatedHead = std::clamp(v.animatedHead, 0, 2);
    v.animatedHeadsetSmooth = std::clamp(v.animatedHeadsetSmooth, 0, 4);
    v.cameraStabilization = std::clamp(v.cameraStabilization, 0, 2);
    for (int Values::*ms : {&Values::cameraVerticalMs, &Values::cameraHorizontalMs, &Values::cameraAngleMs,
                            &Values::lightVerticalMs, &Values::lightHorizontalMs, &Values::lightAngleMs,
                            &Values::heavyVerticalMs, &Values::heavyHorizontalMs, &Values::heavyAngleMs})
        v.*ms = std::clamp(v.*ms, 0, 500);
    v.legStiffnessPct = std::clamp(v.legStiffnessPct, 25, 200);
    v.legDampingPct = std::clamp(v.legDampingPct, 25, 200);
    v.legCouplingPct = std::clamp(v.legCouplingPct, 0, 100);
    for (int Values::*pct : {&Values::cameraPitchPct, &Values::cameraRollPct, &Values::cameraBobVerticalPct,
                             &Values::cameraBobSidePct})
        v.*pct = std::clamp(v.*pct, 0, 100);
    v.cameraHeadAngleDeg = std::clamp(v.cameraHeadAngleDeg, -camera_motion::maxHeadAngle, camera_motion::maxHeadAngle);
    v.legDoubleTapMs = std::clamp(v.legDoubleTapMs, 150, 600);
    v.pointJumpTrigger = std::clamp(v.pointJumpTrigger, 0, 1);
    v.punchViewEaseMs = std::clamp(v.punchViewEaseMs, 0, 1000);
    v.moveHapticsPct = std::clamp(v.moveHapticsPct, 0, 200);
    v.viewYawSource = std::clamp(v.viewYawSource, 0, 1);
    v.jumpHandoffMs = std::clamp(v.jumpHandoffMs, 0, 2000);
    v.airViewEaseMs = std::clamp(v.airViewEaseMs, 0, 1000);
    v.animYawSwayPct = std::clamp(v.animYawSwayPct, 0, 100);
    v.turnInPlaceDeg = std::clamp(v.turnInPlaceDeg, 0, 180);
    v.passiveLegsPct = std::clamp(v.passiveLegsPct, 0, 100);
    v.passiveCouplingPct = std::clamp(v.passiveCouplingPct, 0, 100);
    v.movementMode = std::clamp(v.movementMode, 0, 2);
    v.rearSnapMs = std::clamp(v.rearSnapMs, 0, 1000);
    v.pointJumpChordMs = std::clamp(v.pointJumpChordMs, 20, 150);
    v.pointJumpTapMs = std::clamp(v.pointJumpTapMs, 50, 400);
    v.pointJumpFullMs = std::clamp(v.pointJumpFullMs, 300, 2000);
    v.pointJumpMinSpeed = std::clamp(v.pointJumpMinSpeed, 3, 20);
    v.pointJumpMaxSpeed = std::clamp(v.pointJumpMaxSpeed, std::max(5, v.pointJumpMinSpeed), 40);
    v.pointJumpCurve = std::clamp(v.pointJumpCurve, 0, 1);
    v.pointJumpNormalBias = std::clamp(v.pointJumpNormalBias, 0, 100);
    v.pointJumpMinOutDegrees = std::clamp(v.pointJumpMinOutDegrees, 0, 45);
    v.pointJumpKeep = std::clamp(v.pointJumpKeep, 0, 100);
    // QUICK SIDESTEP (sidestep.hpp).
    v.sidestepDistanceCm = std::clamp(v.sidestepDistanceCm, 100, 300);
    v.sidestepMs = std::clamp(v.sidestepMs, 120, 300);
    v.sidestepCooldownMs = std::clamp(v.sidestepCooldownMs, 0, 2000);
    v.sidestepChain = std::clamp(v.sidestepChain, 1, 10);
    v.sidestepLockoutMs = std::clamp(v.sidestepLockoutMs, 0, 3000);
    v.sidestepFlickFrom = std::clamp(v.sidestepFlickFrom, 20, 60);
    v.sidestepFlickTo = std::clamp(v.sidestepFlickTo, std::max(50, v.sidestepFlickFrom + 20), 95);
    v.sidestepFlickMs = std::clamp(v.sidestepFlickMs, 30, 200);
    v.sidestepAMs = std::clamp(v.sidestepAMs, 40, 250);
    v.pointJumpCoyoteMs = std::clamp(v.pointJumpCoyoteMs, 0, 500);
    v.pointJumpBufferMs = std::clamp(v.pointJumpBufferMs, 0, 500);
    v.pointJumpCooldownMs = std::clamp(v.pointJumpCooldownMs, 0, 2000);
    v.pointJumpZipGuardMs = std::clamp(v.pointJumpZipGuardMs, 0, 2000);
    v.pointJumpDodgeHoldMs = std::clamp(v.pointJumpDodgeHoldMs, 100, 600);
    v.runSpeedPercent = std::clamp(v.runSpeedPercent, 50, 200);
    v.slowMotionSeconds = std::clamp(v.slowMotionSeconds, 3, slowMotionEndless);
    v.softLockMetres = std::clamp(v.softLockMetres, 0, 40);
    v.animatedAirSpin = std::clamp(v.animatedAirSpin, 0, 2);
    v.animatedHybridSpeedMs = v.animatedHybridSpeedMs <= 0 ? 0 : std::clamp(v.animatedHybridSpeedMs, 5, 500);
    v.earlyJump = std::clamp(v.earlyJump, 0, 2);
    v.movementDefaults = std::clamp(v.movementDefaults, 0, movementDefaultsRevision);
    v.webAimSource = std::clamp(v.webAimSource, 0, 1);
    v.forearmBlend = std::clamp(v.forearmBlend, 0, 100);
    v.forearmAimLimit = std::clamp(v.forearmAimLimit, 35, 180);
    v.webLandingImpact = std::clamp(v.webLandingImpact, 12, 40);
    v.nativeRunPercent = std::clamp(v.nativeRunPercent, 50, 90);
    v.nearClipMm = std::clamp(v.nearClipMm, nearClips[0], std::end(nearClips)[-1]);
    v.recordingFov = recording_fov::sanitized(v.recordingFov);
    v.uiStyle = std::clamp(v.uiStyle, uiStyles[0], std::end(uiStyles)[-1]);
    v.blowSounds = std::clamp(v.blowSounds, blowSoundSteps[0], std::end(blowSoundSteps)[-1]);
    v.heightCm = v.heightCm <= 0 ? 0 : std::clamp(v.heightCm, seated::minHeightCm, seated::maxHeightCm);
    v.eyeAim = std::clamp(v.eyeAim, 0, eyeAimShots | eyeAimZips);
    // The realistic flips' ranges, as RealisticFlip takes them.
    v.automaticStrength = std::clamp(v.automaticStrength, 25, 100);
    v.automaticSpeed = std::clamp(v.automaticSpeed, 90, 360);
    v.automaticMinimumSpeed = std::clamp(v.automaticMinimumSpeed, 3, 16);
    v.automaticResponse = std::clamp(v.automaticResponse, 100, 750);
    // HANDSTAND (handstand.hpp).
    v.handstandPushGain = std::clamp(v.handstandPushGain, 100, 400);
    v.handstandPushMax = std::clamp(v.handstandPushMax, 2, 12);
    v.handstandFallDegrees = std::clamp(v.handstandFallDegrees, 15, 60);
    v.handstandHeightCm = std::clamp(v.handstandHeightCm, 60, 140);
    v.handstandGainPct = std::clamp(v.handstandGainPct, 50, 600);
    v.handstandEyeDropCm = std::clamp(v.handstandEyeDropCm, 0, 120);
    v.automaticHandStrength = std::clamp(v.automaticHandStrength, 0, 200);
    v.automaticFalloff = std::clamp(v.automaticFalloff, 500, 3000);
    v.automaticHandThreshold = std::clamp(v.automaticHandThreshold, 50, 300);
    return v;
}
const std::array<Row, 155>& rows() {
    // The help fits the game's description column beside the rows.
    static const std::array<Row, 155> all{{
        {Item::none, "WEBS", nullptr, {}},
        {Item::webStyle, "WEB STYLE",
         "Arcade: webs pass lamp posts, signs and cars, and swings stay above the street.", webStyleChoices},
        {Item::webButton, "WEB BUTTON",
         "Which button shoots and holds a web; the other reels in and shoots web balls.", webButtonChoices},
        {Item::aimMarkers, "AIM MARKERS", "Show where each web would land.", {}},
        {Item::airWebs,
         "WEBS HOLD IN OPEN AIR",
         "With nothing in reach, a web still holds in open air, as far out as WEB RANGE.",
         {}},
        {Item::smartAnchor,
         "SMART ANCHOR",
         "With open-air webs off, a near miss catches a real surface in a small cone.",
         {}},
        {Item::webRange, "WEB RANGE", "How far a web reaches; an open-air web holds this far out.",
         webRangeChoices},
        {Item::webSpeed, "WEB SPEED", "Webs fly to where they stick, then catch you softly. Off: at once.",
         webSpeedChoices},
        {Item::none, "SWINGING", nullptr, {}},
        {Item::swingSpeed, "SWING SPEED LIMIT", "How fast a swing can carry you.", swingChoices},
        {Item::weight, "WEIGHT",
         "How heavy you are while swinging and after letting go. 100% is real gravity.", weightChoices},
        {Item::elasticWeb, "ELASTIC WEB", "Webs stretch under load for a softer catch. Higher is softer.",
         levelChoices},
        {Item::softRelease, "SOFT RELEASE",
         "A web's last pull fades briefly as you let go. Higher lasts longer.", levelChoices},
        {Item::swingAssist,
         "SWING ASSIST",
         "Swinging at a wall, you slow down before it. Your line stays yours.",
         {}},
        {Item::webSlingshot,
         "WEB SLINGSHOT",
         "No auto jump. Draw two fixed webs, hold for vibration, release both to launch.",
         {}},
        {Item::slingshotSpeed, "SLINGSHOT SPEED",
         "A full draw's launch: under the swing speed limit, or at its own speed.", slingshotSpeedChoices},
        {Item::betterClimbing,
         "BETTER CLIMBING",
         "Take one web in both hands, then pull hand over hand to keep climbing it.",
         {}},
        {Item::singleHandYankStrength, "SINGLE HAND YANK STRENGTH",
         "How much speed a fast one-hand yank adds. 100% as before; two-hand zips stay.",
         singleHandYankStrengthChoices},
        {Item::twoHandZip,
         "TWO HAND WEB ZIP",
         "Both webs on one spot: pull both arms back fast to launch at it.",
         {}},
        {Item::ropeSteering,
         "HAND ROPE STEERING",
         "Hold your web with the free hand too and pull sideways to turn your swing.",
         {}},
        {Item::none, "COMFORT", nullptr, {}},
        {Item::snapTurn, "SNAP TURN", "How far the right stick snaps. Used only when SMOOTH TURN is OFF.",
         snapChoices},
        {Item::smoothTurn, "SMOOTH TURN", "Turn steadily while you hold the right stick. Off: it snap turns.",
         smoothChoices},
        {Item::none, "HAPTICS", nullptr, {}},
        {Item::haptics, "CONTROLLER VIBRATION", "How strongly webs, grabs and punches buzz in your hands.",
         hapticChoices},
        {Item::reelHaptics, "REEL HAPTICS", "Reeling a web in ticks in that hand like a winch.", {}},
        {Item::swingHaptics,
         "SWING HAPTICS",
         "Swinging on a taut web ticks softly in that hand, faster as you speed up.",
         {}},
        {Item::blowSounds, "BLOW SOUNDS",
         "What your blows sound like: the game's own hits, Spidy's tones, or none.", blowSoundChoices},
        {Item::pcMedia,
         "PC MEDIA",
         "The phone plays what the PC plays (Spotify, a browser). Off: the Music folder.",
         {}},
        {Item::bodyHaptics, "BODY HAPTICS",
         "How strongly bHaptics vest, sleeves and visor play webs, hits and Spider-Sense.", hapticChoices},
        {Item::hapticWebs,
         "WEB HAPTICS",
         "Body haptics for webs: shots, holds, the swing's pull, zips and grabs.",
         {}},
        {Item::hapticWind,
         "WIND HAPTICS",
         "Body haptics for the wind when you fly or fall free of webs and walls.",
         {}},
        {Item::hapticCombat,
         "COMBAT HAPTICS",
         "Body haptics for punches, hits, Spider-Sense, dodges, heals and slow motion.",
         {}},
        {Item::hapticMovement,
         "MOVEMENT HAPTICS",
         "Body haptics for landings, walls, wall climbing and charged leaps.",
         {}},
        {Item::none, "DISPLAY", nullptr, {}},
        {Item::view, "VR VIEW", "Play in VR, or the game as it is on a 3D screen, with pad controls.",
         viewChoices, Place::page},
        {Item::screenSize, "GAME SCREEN SIZE", "The screen that shows menus, cutscenes and flat mode.",
         screenChoices},
        {Item::screenDepth, "GAME SCREEN 3D",
         "Cutscenes and VIEW: GAME SCREEN get depth. Higher brings them nearer. Off: flat.", depthChoices},
        {Item::hud, "HUD", "How large the game's HUD shows in front of you. Off: no HUD in VR.", hudChoices},
        {Item::uiStyle, "UI STYLE",
         "Spidy's own marks, gadget wheel and meters: comic print, or tech like the HUD.", uiStyleChoices},
        {Item::combatVisuals,
         "COMBAT VISUALS",
         "Off: no combat effects; free uppercuts/smashes. Spider-Sense stays on.",
         {}},
        {Item::nearClip, "NEAR CLIP",
         "How near your eyes things still show. Nearer keeps your hands in view.", nearClipChoices},
        {Item::recordingFov, "RECORDING FOV",
         "A view this wide on the monitor to record. Costs frame rate; no Spidy marks.", recordingFovChoices},
        {Item::recordingSmoothing, "RECORDING SMOOTHING",
         "Softens how the recording view turns, not the headset's. Higher is softer.",
         recordingSmoothingChoices},
        {Item::none, "BODY", nullptr, {}},
        {Item::calibrate, "CALIBRATE BODY",
         "Fit Spider-Man to you: pick ON RESUME, then hold both triggers in a T-pose.", calibrateChoices},
        {Item::height, "HEIGHT",
         "Your height, for Spider-Man's size and SEATED. AUTO measures it in the headset.", heightChoices},
        {Item::seated,
         "SEATED",
         "Play sitting: your view rises to your standing height. Recenter after sitting.",
         {}},
        {Item::none, "EXPERIMENTAL", nullptr, {}},
        {Item::flips,
         "FLIPS",
         "Basic stick flips; with REALISTIC FLIPS, adds left-stick steering instead.",
         {}},
        {Item::controlledFlips,
         "CONTROLLED FLIPS",
         "A toggles left-stick flips. Overrides FLIPS; REALISTIC FLIPS takes priority.",
         {}},
        {Item::flipSpeed, "FLIP SPEED", "How fast a flip turns you over at full stick tilt.",
         flipSpeedChoices},
        {Item::handClimb,
         "HAND WALL CLIMBING",
         "Grip and pull. Reach back near the wall to turn. Left stick: run. A: jump.",
         {}},
        {Item::joystickRunCamera,
         "JOYSTICK WALL RUN CAMERA",
         "On: old wall-floor view. Off: joystick drives the existing momentum wall run.",
         {}},
        {Item::wallRunCameraTilt,
         "WALL RUN CAMERA TILT",
         "Gently bank the view with the body on horizontal momentum wall runs.",
         {}},
        {Item::halfPunchDamage,
         "HALF PUNCH DAMAGE",
         "Punches and web strikes deal 50% damage. Launches and smash force stay the same.",
         {}},
        {Item::webTie,
         "WEB TIE",
         "Web an enemy up, web a wall: hands together, click the right stick. He sticks.",
         {}},
        {Item::webLines,
         "WEB LINES",
         "Webs on two walls, hands together, click right stick: a line. Hold it to zip.",
         {}},
        {Item::eyeAim, "EYE AIM",
         "Web balls and zips go where your eyes look. Needs a headset with eye tracking.", eyeAimChoices},
        {Item::flipReprojection,
         "FLIP REPROJECTION",
         "Flips, spins and smooth turns shown as steadily as head turns.",
         {}},
        {Item::steadyHud,
         "STEADY HUD",
         "The HUD shown by the headset itself: it holds still when the game falls behind.",
         {}},
        {Item::airSlingshot,
         "AIR SLINGSHOT",
         "In the air: two taut webs, draw both arms until the buzz, let go to launch.",
         {}},
        {Item::webAngle, "WEB ANGLE", "Tilts the webs up or down from the controller's line. 0 as before.",
         webAngleChoices},
        {Item::wristShooters,
         "WRIST SHOOTERS (OLD)",
         "Replaced by SHOOTERS: LOCKED TO ARM. Kept so older settings files still load.",
         {},
         Place::launcher},
        {Item::dodgeTumble, "DODGE TUMBLE",
         "The view leans or rolls with the game's dodge, so you see it was one.", dodgeTumbleChoices},
        {Item::runCurve,
         "RUN CURVE",
         "Ground only: full run at 80% of the stick, faster sooner. On by default.",
         {}},
        {Item::nativeRunAt, "NATIVE RUN AT", "The stick's tilt past which NATIVE RUN sprints. 70% as before.",
         nativeRunPercentChoices},
        {Item::runLogs,
         "RUN LOGS",
         "Run, jump and frame logs, in the console and a -diag.log file, to send back.",
         {}},
        {Item::momentumLeap,
         "RUNNING LEAP",
         "Charged leaps keep run speed. EARLY JUMP: RUNNING skips charging in a sprint.",
         {}},
        {Item::earlyJump, "EARLY JUMP",
         "A jumps sooner: a quicker tap, or at once while running. QUICK TAP by default.", earlyJumpChoices},
        {Item::softLanding,
         "SOFT LANDING",
         "A fall with no web eases to 9 m/s down just before the ground. Off as before.",
         {}},
        {Item::spidyRunFromStanding,
         "RUN FROM STANDING",
         "With SPIDY RUN: hold the stick fully to start it from a stand. Off as before.",
         {}},
        {Item::webAimSource, "WEB AIM (OLD)",
         "Replaced by SHOOTERS. Kept so older settings files still load.", webAimSourceChoices,
         Place::launcher},
        {Item::forearmBlend, "FOREARM BLEND (OLD)",
         "Replaced by LOCK STRENGTH. Kept so older settings files still load.", forearmBlendChoices,
         Place::launcher},
        {Item::webLandingSmooth,
         "SMOOTH WEB LANDING",
         "Holding a web, a landing under WEB LANDING IMPACT is soft and keeps its speed.",
         {}},
        {Item::webLandingImpact, "WEB LANDING IMPACT",
         "SMOOTH WEB LANDING: at or over this speed a landing is the game's heavy one.",
         webLandingImpactChoices},
        {Item::steadyStates,
         "STEADY STATES",
         "NATIVE RUN's RT waits out flickers and stays off in fights. Off as before.",
         {}},
        {Item::combatResponsive,
         "COMBAT RESPONSIVE",
         "In a fight: no SPIDY RUN or carry, no RT, A jumps at once. Off as before.",
         {}},
        {Item::spidyRunJump,
         "SPIDY RUN JUMP",
         "A in SPIDY RUN or the landing carry jumps at once, keeping the speed. Off: v5's.",
         {}},
        {Item::forearmAimLimit, "FOREARM AIM LIMIT (RETIRED)",
         "No longer used: LOCKED TO ARM follows the whole arm. Kept for older files.", forearmAimLimitChoices,
         Place::launcher},
        {Item::palmShooters,
         "PALM SHOOTERS (OLD)",
         "Replaced by SHOOTERS: PALM. Kept so older settings files still load.",
         {},
         Place::launcher},
        {Item::headProbe,
         "HEAD PROBE",
         "Diagnostics only: logs Spidey's head against the headset. Changes nothing.",
         {}},
        {Item::none, "NATIVE ANIMATION", nullptr, {}},
        {Item::nativeAnimation,
         "NATIVE ANIMATION",
         "On: animations + tracked arms. Off: classic body; run options stay independent.",
         {}},
        {Item::animatedHead, "HEAD", "FULL follows the animated head; LEVEL keeps the horizon level.",
         animatedHeadChoices},
        {Item::animatedHeadsetSmooth, "SMOOTHING",
         "Calms animation wobble. OFF is as now; headset look and travel stay with you.",
         animatedHeadsetSmoothChoices},
        {Item::animatedArms,
         "ARMS",
         "Your tracked controllers and headset share the animated head's frame.",
         {}},
        {Item::animatedTorso,
         "TORSO",
         "Player spine, head and arms; native root and legs stay animated.",
         {}},
        {Item::animatedModelYaw, "MODEL YAW", "Turn with the right stick while running with the left.", {}},
        {Item::animatedHybridSpeed, "HYBRID SPEED",
         "Time at 15 m/s in air before SpidyVR takes over. OFF disables speed takeover.",
         animatedHybridSpeedChoices},
        {Item::nativeRun,
         "NATIVE RUN",
         "Ground only: stick past 70% holds the game's RT: sprint, vaults, wall runs.",
         {}},
        {Item::spidyRun,
         "SPIDY RUN",
         "Land fast and keep running at that speed, steering, until it eases off.",
         {}},
        {Item::animatedMoves,
         "ANIMATION MOVES (ADVANCED)",
         "Game-driven body animation; the master also enables Hybrid.",
         {}},
        {Item::animatedHybrid,
         "HYBRID OWNER (ADVANCED)",
         "Animations by default; SpidyVR on web, mid-air turn, speed or wall run.",
         {}},
        {Item::animatedAir, "AIR (ADVANCED)", "Legacy animation in air. Ignored while Hybrid is on.", {}},
        {Item::animatedOnly,
         "ONLY (ADVANCED)",
         "Legacy animation-only ownership. Ignored while Hybrid is on.",
         {}},
        {Item::animatedSteering,
         "STEERING (ADVANCED)",
         "Legacy actor-relative steering. Ignored while Hybrid is on.",
         {}},
        {Item::animatedAirSpin, "AIR SPIN (ADVANCED)", "Legacy animated flips. Ignored while Hybrid is on.",
         animatedAirSpinChoices},
        {Item::runSpeed, "RUN SPEED", "Scales SpidyVR's own ground run (SPIDY RUN). The game's run is unchanged.",
         runSpeedChoices},
        {Item::slowMotionLength, "SLOW-MO LENGTH", "L3 slow motion on a full meter. INFINITE: until L3 again.",
         slowMotionLengthChoices},
        {Item::softLock, "SOFT LOCK", "Grip webs snap to an enemy or prop this near your aim. Closer wins.",
         softLockChoices},
        {Item::cameraStabilization, "STABILIZATION",
         "Steadies the animated camera. LIGHT/HEAVY use their rows; OFF the CAMERA rows.",
         cameraStabilizationChoices},
        {Item::cameraVertical, "CAMERA VERTICAL",
         "Animated head up/down bob smoothing (STABILIZATION OFF). Headset stays 1:1.",
         cameraSmoothChoices},
        {Item::cameraHorizontal, "CAMERA HORIZONTAL",
         "Animated head sway smoothing (STABILIZATION OFF). Your feet stay exact.",
         cameraSmoothChoices},
        {Item::cameraAngle, "CAMERA ANGLE",
         "Animated head turn smoothing (STABILIZATION OFF). Headset look stays 1:1.",
         cameraSmoothChoices},
        {Item::lightVertical, "LIGHT VERTICAL", "Up/down smoothing used by STABILIZATION LIGHT.",
         cameraSmoothChoices},
        {Item::lightHorizontal, "LIGHT HORIZONTAL", "Side/forward smoothing used by STABILIZATION LIGHT.",
         cameraSmoothChoices},
        {Item::lightAngle, "LIGHT ANGLE", "Turn smoothing used by STABILIZATION LIGHT.", cameraSmoothChoices},
        {Item::heavyVertical, "HEAVY VERTICAL", "Up/down smoothing used by STABILIZATION HEAVY.",
         cameraSmoothChoices},
        {Item::heavyHorizontal, "HEAVY HORIZONTAL", "Side/forward smoothing used by STABILIZATION HEAVY.",
         cameraSmoothChoices},
        {Item::heavyAngle, "HEAVY ANGLE", "Turn smoothing used by STABILIZATION HEAVY.", cameraSmoothChoices},
        {Item::legControl,
         "LEG CONTROL (ADVANCED)",
         "Hold Y+X midair: right stick poses the legs. With CONTROLLED or REALISTIC FLIPS.",
         {}},
        {Item::legStiffness, "LEG STIFFNESS", "How firmly the legs hold the stick's pose. Lower swings more.",
         legSpringChoices},
        {Item::legDamping, "LEG DAMPING", "How fast the legs' swing settles. Lower keeps them swinging.",
         legSpringChoices},
        {Item::legCoupling, "LEG COUPLING", "How much the legs' swing turns your body in the air.",
         legCouplingChoices},
        {Item::airFists,
         "AIR FISTS",
         "A fast hand closes into a fist with nobody to punch. Off: only near enemies.",
         {}},
        {Item::shooterMode, "SHOOTERS",
         "Where webs leave. LOCKED TO ARM: along the arm you see. CONTROLLER: as before.",
         shooterModeChoices},
        {Item::shooterLock, "LOCK STRENGTH",
         "LOCKED TO ARM: how much of the arm's line webs take. 100% is fully on your arm.",
         shooterLockChoices},
        {Item::cameraMotion, "CAMERA MOTION",
         "How much animation moves your view. Fills the rows below; edit for CUSTOM.",
         cameraMotionChoices},
        {Item::cameraPitch, "PITCH",
         "Animation nodding your view up/down. 0% level, 100% full. Head stays 1:1.",
         cameraAmountChoices},
        {Item::cameraRoll, "ROLL",
         "Animation leaning the horizon. 0% level, 100% full. Flips always turn fully.",
         cameraAmountChoices},
        {Item::cameraBobVertical, "BOB UP/DOWN",
         "Animation head bob up and down. 0% steady, 100% full. Your feet stay exact.",
         cameraAmountChoices},
        {Item::cameraBobSide, "BOB SIDE",
         "Animation head sway side to side and fore-aft. 0% steady, 100% full.",
         cameraAmountChoices},
        {Item::cameraHeadAngle, "HEAD ANGLE",
         "Tilts the animated view below (DOWN) or above (UP) the horizon. LEVEL as before.",
         cameraHeadAngleChoices},
        {Item::pointJump,
         "POINT JUMP (EXPERIMENTAL)",
         "On a surface, A+B charges a jump the way you look. Tap: quick. Hold: bigger.",
         {}},
        {Item::pointJumpTrigger, "POINT JUMP TRIGGER",
         "A+B together, or hold B then A (B alone still dodges on release).", pointJumpTriggerChoices},
        {Item::pointJumpMinSpeed, "TAP SPEED", "A quick tap's launch speed.", pointJumpMinSpeedChoices},
        {Item::pointJumpMaxSpeed, "MAX SPEED", "A full charge's launch speed.", pointJumpMaxSpeedChoices},
        {Item::pointJumpFull, "CHARGE TIME", "How long to hold for the full charge.", pointJumpFullChoices},
        {Item::pointJumpPush, "SURFACE PUSH",
         "How much the jump leans out of the surface you stand on. 25% default.", pointJumpPushChoices},
        // November 12: PUNCH VIEW EASE (view_ease.hpp).
        {Item::punchViewEase, "PUNCH VIEW EASE",
         "A punch's switch to VR: view held, then eased onto your feet. OFF: the old snap.", punchViewEaseChoices},
        // November 12: MOVE HAPTICS (move_haptics.hpp), VIEW YAW SOURCE
        // (stable_yaw.hpp), SOUND TRACE (sound_trace.hpp).
        {Item::moveHaptics, "MOVE HAPTICS",
         "Jump and landing thumps. Own strength: CONTROLLER VIBRATION doesn't scale it.", moveHapticChoices},
        {Item::viewYawSource, "VIEW YAW SOURCE",
         "STABLE: steer by a steady heading (no circling). BODY: old head-bone steering.",
         viewYawSourceChoices, Place::launcher}, // superseded by MOVEMENT MODE (in game)
        {Item::soundTrace, "SOUND TRACE",
         "Diagnostics: logs game sound ids around jumps and landings. Changes nothing.", {}},
        // November 12: JUMP TO SPIDYVR (hybrid_owner.hpp), AIR VIEW EASE (view_ease.hpp).
        {Item::jumpHandoff, "JUMP TO SPIDYVR",
         "Jumps, zip launches, vaults, wall launches: SpidyVR after this. STAY: old way.",
         jumpHandoffChoices},
        {Item::airViewEase, "AIR VIEW EASE",
         "Jump timer, flip and grip breaks: view held, then eased. OFF: the old snap.", airViewEaseChoices},
        // November 12 (Step B): FULL head lock on the stable heading (view_yaw_lock.hpp).
        // November 12 (hybrid movement): hybrid_movement.hpp.
        {Item::movementMode, "MOVEMENT MODE",
         "HYBRID: view on his head, stick back snaps. STABLE: steady heading. OLD.", movementModeChoices},
        {Item::rearSnap, "REAR SNAP TURN",
         "HYBRID: left stick back turns you that way, as a head turn. INSTANT: a cut.", rearSnapChoices},
        {Item::animYawSway, "ANIM YAW SWAY",
         "Full lock, stable yaw: share of the head's yaw sway shown. 100%: the old feel.", animYawSwayChoices},
        {Item::turnInPlace, "TURN-IN-PLACE ANGLE",
         "Standing: he turns to where you look past this angle. OFF: he never does.", turnInPlaceChoices,
         Place::launcher}, // STABLE only since MOVEMENT MODE; the camera tab holds 20
        // November 12 (legs): leg_layers.hpp.
        {Item::passiveLegs, "PASSIVE LEGS",
         "In the air without LEG CONTROL: legs swing with momentum, this much shown.", passiveLegsChoices},
        {Item::passiveCoupling, "PASSIVE COUPLING",
         "How much passive legs turn you, as a share of LEG COUPLING. Swing for more.",
         passiveCouplingChoices},
        {Item::legAirCrouch, "AIR CROUCH",
         "Upright with hang time: the legs fold into the jump's midair crouch.", {}},
        {Item::legReachSpread, "REACH SPREAD",
         "A hand down between your legs: they spread and lift to clear it.", {}},
        {Item::realisticFlips,
         "REALISTIC FLIPS",
         "Momentum rotation from arms and webs. FLIPS also enables left-stick steering.",
         {}},
        {Item::automaticStrength, "ROTATION STRENGTH",
         "How strongly the body follows the web direction. Heading stays free.", automaticStrengthChoices},
        {Item::automaticSpeed, "MAX ROTATION SPEED",
         "Maximum automatic turning speed, including release flips.", automaticSpeedChoices},
        {Item::automaticMinimumSpeed, "MINIMUM FLIGHT SPEED",
         "Start automatic rotation only above this travel speed.", automaticMinimumChoices, Place::launcher},
        {Item::automaticReleaseFlip,
         "SWING MOMENTUM",
         "Carry orbital momentum when the last web is released. No preset flip.",
         {}},
        {Item::automaticResponse, "ROTATION RESPONSE",
         "Longer response gives gentler acceleration and braking.", automaticResponseChoices,
         Place::launcher},
        {Item::automaticHandStrength, "HAND MOMENTUM",
         "How much arm movement changes your angular momentum in the air.", automaticHandStrengthChoices},
        {Item::automaticFalloff, "ANGULAR DRAG", "How quickly rotation slows without more hand or web input.",
         automaticFalloffChoices, Place::launcher},
        {Item::automaticHandThreshold, "MINIMUM HAND SPEED",
         "Ignore hand movements slower than this. Faster gestures add momentum.",
         automaticHandThresholdChoices, Place::launcher},
        {Item::automaticAirBrake,
         "AIR ROTATION BRAKE",
         "Press A in the air to stop momentum and smoothly return upright.",
         {}},
        {Item::automaticCombat,
         "FLIPS IN COMBAT",
         "Off: with enemies near, arms and webs stop turning you and you come upright.",
         {}},
    }};
    return all;
}
const Row* rowFor(Item item) {
    if (item == Item::none)
        return nullptr;
    for (const auto& row : rows())
        if (row.item == item)
            return &row;
    return nullptr;
}
const std::array<Screen, categoryCount>& screens() {
    static constexpr Item setup[] = {
        Item::smoothTurn, Item::snapTurn, Item::calibrate, Item::height,
        Item::seated,
    };
    static constexpr Item webs[] = {
        Item::webStyle, Item::webButton, Item::aimMarkers, Item::airWebs,
        Item::smartAnchor, Item::webRange, Item::webSpeed, Item::eyeAim,
        Item::webTie, Item::webLines,
    };
    static constexpr Item swinging[] = {
        Item::swingSpeed, Item::weight, Item::elasticWeb, Item::softRelease,
        Item::swingAssist, Item::webSlingshot, Item::airSlingshot, Item::slingshotSpeed,
        Item::betterClimbing, Item::singleHandYankStrength, Item::twoHandZip, Item::ropeSteering,
    };
    static constexpr Item movement[] = {
        Item::nativeRun, Item::nativeRunAt, Item::runCurve, Item::steadyStates,
        Item::spidyRun, Item::spidyRunFromStanding, Item::spidyRunJump, Item::momentumLeap,
        Item::earlyJump, Item::softLanding, Item::webLandingSmooth, Item::webLandingImpact,
        Item::handClimb, Item::joystickRunCamera, Item::wallRunCameraTilt, Item::combatResponsive,
        Item::combatVisuals, Item::halfPunchDamage, Item::airFists, Item::runLogs,
    };
    static constexpr Item flips[] = {
        Item::realisticFlips, Item::flips, Item::controlledFlips, Item::flipSpeed,
        Item::automaticStrength, Item::automaticSpeed, Item::automaticMinimumSpeed, Item::automaticReleaseFlip,
        Item::automaticResponse, Item::automaticHandStrength, Item::automaticFalloff, Item::automaticHandThreshold,
        Item::automaticAirBrake, Item::automaticCombat, Item::dodgeTumble, Item::flipReprojection,
    };
    static constexpr Item feedback[] = {
        Item::haptics, Item::moveHaptics, Item::reelHaptics, Item::swingHaptics, Item::bodyHaptics,
        Item::hapticWebs, Item::hapticWind, Item::hapticCombat, Item::hapticMovement,
        Item::blowSounds, Item::pcMedia,
    };
    static constexpr Item display[] = {
        Item::screenSize, Item::screenDepth, Item::hud, Item::steadyHud,
        Item::uiStyle, Item::nearClip, Item::recordingFov, Item::recordingSmoothing,
    };
    static constexpr Item nativeAnimation[] = {
        Item::nativeAnimation, Item::animatedHead, Item::animatedHeadsetSmooth,
        Item::animatedArms, Item::animatedTorso, Item::animatedModelYaw, Item::animatedHybridSpeed,
        Item::animatedMoves, Item::animatedHybrid, Item::animatedAir, Item::animatedOnly,
        Item::animatedSteering, Item::viewYawSource, Item::movementMode, Item::jumpHandoff, Item::animatedAirSpin, Item::headProbe, Item::soundTrace,
        Item::runSpeed,
        Item::slowMotionLength, Item::softLock,
    };
    // CAMERA & STABILIZATION (November 11): how much the animation moves the
    // view (amounts, HEAD ANGLE), then how smoothly (the response times).
    static constexpr Item camera[] = {
        Item::cameraMotion, Item::cameraPitch, Item::cameraRoll, Item::cameraBobVertical,
        Item::cameraBobSide, Item::cameraHeadAngle,
        Item::cameraStabilization, Item::punchViewEase, Item::airViewEase, Item::rearSnap, Item::animYawSway, Item::turnInPlace, Item::cameraVertical, Item::cameraHorizontal, Item::cameraAngle,
        Item::lightVertical, Item::lightHorizontal, Item::lightAngle,
        Item::heavyVertical, Item::heavyHorizontal, Item::heavyAngle,
    };
    // WEB SHOOTERS (November 11): one choice and its strength, WEB ANGLE on
    // top; the old rows they replace stay in the launcher's file only.
    static constexpr Item shooters[] = {
        Item::shooterMode, Item::shooterLock, Item::webAngle,
        Item::webAimSource, Item::forearmBlend, Item::forearmAimLimit, Item::wristShooters,
        Item::palmShooters,
    };
    // LEGS (ADVANCED) (November 11): shown even while unavailable, the help
    // saying why (shownWhenInactive).
    static constexpr Item legs[] = {
        Item::legControl, Item::legStiffness, Item::legDamping, Item::legCoupling,
        Item::passiveLegs, Item::passiveCoupling, Item::legAirCrouch, Item::legReachSpread,
    };
    // POINT JUMP (November 11): its switch, trigger and feel.
    static constexpr Item pointJump[] = {
        Item::pointJump, Item::pointJumpTrigger, Item::pointJumpMinSpeed,
        Item::pointJumpMaxSpeed, Item::pointJumpFull, Item::pointJumpPush,
    };
    static const std::array<Screen, categoryCount> all{{
        {"Setup", "VR SETUP", setup},
        {"Webs", "VR WEBS", webs},
        {"Swinging", "VR SWINGING", swinging},
        {"Movement", "VR MOVEMENT", movement},
        {"Flips", "VR FLIPS", flips},
        {"Haptics", "VR HAPTICS", feedback},
        {"Display", "VR DISPLAY", display},
        {"Native Animation", "VR NATIVE ANIMATION", nativeAnimation},
        {"Camera & Stabilization", "VR CAMERA & STABILIZATION", camera},
        {"Web Shooters", "VR WEB SHOOTERS", shooters},
        {"Legs (Advanced)", "VR LEGS (ADVANCED)", legs},
        {"Point Jump", "VR POINT JUMP", pointJump},
    }};
    return all;
}
const char* inactiveReason(Item item, const Values& v) {
    switch (item) {
    case Item::rearSnap:
        return v.movementMode != 0 ? "Set MOVEMENT MODE to HYBRID to use the rear snap." : nullptr;
    case Item::viewYawSource:
        return "MOVEMENT MODE sets it now (OLD: body; HYBRID, STABLE: stable).";
    case Item::animYawSway:
        return v.movementMode != 1 ? "Set MOVEMENT MODE to STABLE to use it." : nullptr;
    case Item::snapTurn:
        return v.smoothTurn > 0 ? "Turn Smooth turn off to use Snap turn." : nullptr;
    case Item::smartAnchor:
        return v.airWebs ? "Turn Webs hold in open air off to use Smart anchor." : nullptr;
    case Item::shooterLock:
        return shooter_lock::mode(v) != shooter_lock::Mode::arm ? "Set SHOOTERS to LOCKED TO ARM first." : nullptr;
    case Item::wristShooters:
        return v.webAimSource == 1 ? "Web aim already follows the model forearm." : nullptr;
    case Item::forearmBlend:
        return v.webAimSource != 1 ? "Set Web aim to Model forearm to adjust the blend." : nullptr;
    case Item::forearmAimLimit:
        return v.webAimSource != 1 && !v.wristShooters ? "Enable Model forearm or Wrist shooters first." : nullptr;
    case Item::slingshotSpeed:
        return !v.webSlingshot && !v.airSlingshot ? "Enable Web slingshot or Air slingshot first." : nullptr;
    case Item::nativeRunAt:
    case Item::steadyStates:
        return !v.nativeRun ? "Enable Native run first." : nullptr;
    case Item::spidyRunFromStanding:
        return !v.spidyRun ? "Enable Spidy run first." : nullptr;
    case Item::spidyRunJump:
        return !v.spidyRun && !v.webLandingSmooth ? "Enable Spidy run or Smooth web landing first." : nullptr;
    case Item::webLandingImpact:
        return !v.webLandingSmooth ? "Enable Smooth web landing first." : nullptr;
    case Item::controlledFlips:
        return v.realisticFlips ? "Realistic flips takes priority over Controlled flips." : nullptr;
    case Item::legControl:
        return !v.controlledFlips && !v.realisticFlips
                   ? "Needs CONTROLLED or REALISTIC FLIPS. ON here turns CONTROLLED FLIPS on."
                   : nullptr;
    case Item::legStiffness:
    case Item::legDamping:
    case Item::legCoupling:
        return !v.legControl ? "Turn LEG CONTROL on first."
               : !v.controlledFlips && !v.realisticFlips ? "LEG CONTROL needs CONTROLLED or REALISTIC FLIPS on."
                                                         : nullptr;
    case Item::flips:
        return v.controlledFlips && !v.realisticFlips ? "Controlled flips takes priority over basic Flips." : nullptr;
    case Item::flipSpeed:
        return !v.flips && (!v.controlledFlips || v.realisticFlips) ? "Enable stick flips to use Flip speed." : nullptr;
    case Item::automaticStrength:
    case Item::automaticSpeed:
    case Item::automaticMinimumSpeed:
    case Item::automaticReleaseFlip:
    case Item::automaticResponse:
    case Item::automaticHandStrength:
    case Item::automaticFalloff:
    case Item::automaticHandThreshold:
    case Item::automaticAirBrake:
    case Item::automaticCombat:
        return !v.realisticFlips ? "Enable Realistic flips first." : nullptr;
    case Item::reelHaptics:
    case Item::swingHaptics:
        return v.haptics == 0 ? "Raise Controller vibration above zero first." : nullptr;
    case Item::hapticWebs:
    case Item::hapticWind:
    case Item::hapticCombat:
    case Item::hapticMovement:
        return v.bodyHaptics == 0 ? "Raise Body haptics above zero first." : nullptr;
    case Item::steadyHud:
        return v.hud == 0 ? "Enable the HUD first." : nullptr;
    case Item::pointJumpTrigger:
    case Item::pointJumpMinSpeed:
    case Item::pointJumpMaxSpeed:
    case Item::pointJumpFull:
    case Item::pointJumpPush:
        return !v.pointJump ? "Turn POINT JUMP on first." : nullptr;
    case Item::recordingSmoothing:
        return v.recordingFov == 0 ? "Enable Recording FOV first." : nullptr;
    case Item::passiveCoupling:
        return v.passiveLegsPct == 0 ? "Turn PASSIVE LEGS on first."
               : !v.controlledFlips && !v.realisticFlips ? "Turns you only with CONTROLLED or REALISTIC FLIPS."
                                                         : nullptr;
    default:
        return nullptr;
    }
}
bool shownWhenInactive(Item item) {
    switch (item) {
    case Item::legControl:
    case Item::legStiffness:
    case Item::legDamping:
    case Item::legCoupling:
    case Item::shooterLock:
        return true;
    default:
        return false;
    }
}
Values resetCategory(Category category, Values v) {
    const auto index = static_cast<size_t>(category);
    if (index >= categoryCount)
        return v;
    for (Item item : screens()[index].items)
        if (const Row* row = rowFor(item); row && row->place == Place::tab &&
            item != Item::nativeAnimation)
            choose(item, defaultChoice(item), v);
    return v;
}
int choice(Item item, const Values& v) {
    if (item == Item::nativeAnimation)
        return v.animatedMoves && v.animatedHybrid ? 1 : 0;
    if (const bool* on = switchOf(item, v))
        return *on ? 1 : 0;
    const auto s = steps(item);
    if (s.empty())
        return 0;
    const float value = numberOf(item, v);
    int best = 0;
    for (int i = 1; i < static_cast<int>(s.size()); ++i)
        if (std::abs(static_cast<float>(s[i]) - value) < std::abs(static_cast<float>(s[best]) - value))
            best = i;
    return best;
}
bool choose(Item item, int index, Values& v) {
    if (item == Item::nativeAnimation) {
        if (index != 0 && index != 1)
            return false;
        const Values before = v;
        const bool on = index == 1;
        v.animatedMoves = v.animatedHybrid = on;
        v.animatedHead = on ? 2 : 0;
        v.animatedArms = v.animatedTorso = v.animatedModelYaw = on;
        if (on) {
            v.nativeRun = v.spidyRun = true;
        } else {
            // Legacy animation paths must not keep yielding when the mode is off.
            v.animatedAir = v.animatedOnly = v.animatedSteering = false;
            v.animatedAirSpin = 0;
        }
        return !(v == before);
    }
    if (item == Item::legControl && index == 1) {
        // LEG CONTROL ON does what it says: it works with CONTROLLED or
        // REALISTIC FLIPS (November 11 23:35); with neither, CONTROLLED FLIPS
        // comes on. REALISTIC FLIPS is left as the player set it.
        const Values before = v;
        v.legControl = true;
        if (!v.controlledFlips && !v.realisticFlips)
            v.controlledFlips = true;
        return !(v == before);
    }
    if (item == Item::shooterMode || item == Item::shooterLock || item == Item::cameraMotion) {
        const auto s = steps(item);
        if (index < 0 || index >= static_cast<int>(s.size()))
            return false;
        const Values before = v;
        if (item == Item::shooterMode)
            shooter_lock::setMode(v, static_cast<shooter_lock::Mode>(s[index]));
        else if (item == Item::shooterLock)
            shooter_lock::setStrength(v, s[index]);
        else
            camera_motion::applyPreset(v, s[index]);
        return !(v == before);
    }
    if (bool* on = switchOf(item, v)) {
        if (index != 0 && index != 1)
            return false;
        const bool was = *on;
        *on = index == 1;
        return *on != was;
    }
    const auto s = steps(item);
    if (index < 0 || index >= static_cast<int>(s.size()))
        return false;
    const Values before = v;
    switch (item) {
    case Item::swingSpeed:
        v.swingSpeed = static_cast<float>(s[index]);
        break;
    case Item::snapTurn:
        v.snapTurn = s[index];
        break;
    case Item::smoothTurn:
        v.smoothTurn = s[index];
        break;
    case Item::haptics:
        v.haptics = s[index];
        break;
    case Item::bodyHaptics:
        v.bodyHaptics = s[index];
        break;
    case Item::screenSize:
        v.screenSize = s[index];
        break;
    case Item::weight:
        v.weight = s[index];
        break;
    case Item::hud:
        v.hud = s[index];
        break;
    case Item::flipSpeed:
        v.flipSpeed = s[index];
        break;
    case Item::screenDepth:
        v.screenDepth = s[index];
        break;
    case Item::elasticWeb:
        v.elasticWeb = s[index];
        break;
    case Item::softRelease:
        v.softRelease = s[index];
        break;
    case Item::slingshotSpeed:
        v.slingshotSpeed = s[index];
        break;
    case Item::recordingSmoothing:
        v.recordingSmoothing = s[index];
        break;
    case Item::webRange:
        v.webRange = s[index];
        break;
    case Item::webSpeed:
        v.webSpeed = s[index];
        break;
    case Item::singleHandYankStrength:
        v.singleHandYankPercent = s[index];
        break;
    case Item::webAngle:
        v.webAngle = s[index];
        break;
    case Item::dodgeTumble:
        v.dodgeTumble = s[index];
        break;
    case Item::animatedHead:
        v.animatedHead = s[index];
        break;
    case Item::animatedHeadsetSmooth:
        v.animatedHeadsetSmooth = s[index];
        break;
    case Item::cameraStabilization:
        v.cameraStabilization = s[index];
        break;
    case Item::cameraVertical:
        v.cameraVerticalMs = s[index];
        break;
    case Item::cameraHorizontal:
        v.cameraHorizontalMs = s[index];
        break;
    case Item::cameraAngle:
        v.cameraAngleMs = s[index];
        break;
    case Item::lightVertical:
        v.lightVerticalMs = s[index];
        break;
    case Item::lightHorizontal:
        v.lightHorizontalMs = s[index];
        break;
    case Item::lightAngle:
        v.lightAngleMs = s[index];
        break;
    case Item::heavyVertical:
        v.heavyVerticalMs = s[index];
        break;
    case Item::heavyHorizontal:
        v.heavyHorizontalMs = s[index];
        break;
    case Item::heavyAngle:
        v.heavyAngleMs = s[index];
        break;
    case Item::legStiffness:
        v.legStiffnessPct = s[index];
        break;
    case Item::legDamping:
        v.legDampingPct = s[index];
        break;
    case Item::legCoupling:
        v.legCouplingPct = s[index];
        break;
    case Item::cameraPitch:
        v.cameraPitchPct = s[index];
        break;
    case Item::cameraRoll:
        v.cameraRollPct = s[index];
        break;
    case Item::cameraBobVertical:
        v.cameraBobVerticalPct = s[index];
        break;
    case Item::cameraBobSide:
        v.cameraBobSidePct = s[index];
        break;
    case Item::cameraHeadAngle:
        v.cameraHeadAngleDeg = s[index];
        break;
    case Item::pointJumpTrigger:
        v.pointJumpTrigger = s[index];
        break;
    case Item::punchViewEase:
        v.punchViewEaseMs = s[index];
        break;
    case Item::moveHaptics:
        v.moveHapticsPct = s[index];
        break;
    case Item::viewYawSource:
        v.viewYawSource = s[index];
        break;
    case Item::jumpHandoff:
        v.jumpHandoffMs = s[index];
        break;
    case Item::airViewEase:
        v.airViewEaseMs = s[index];
        break;
    case Item::animYawSway:
        v.animYawSwayPct = s[index];
        break;
    case Item::turnInPlace:
        v.turnInPlaceDeg = s[index];
        break;
    case Item::passiveLegs:
        v.passiveLegsPct = s[index];
        break;
    case Item::passiveCoupling:
        v.passiveCouplingPct = s[index];
        break;
    case Item::movementMode:
        v.movementMode = s[index];
        break;
    case Item::rearSnap:
        v.rearSnapMs = s[index];
        break;
    case Item::pointJumpMinSpeed:
        v.pointJumpMinSpeed = s[index];
        v.pointJumpMaxSpeed = std::max(v.pointJumpMaxSpeed, v.pointJumpMinSpeed);
        break;
    case Item::pointJumpMaxSpeed:
        v.pointJumpMaxSpeed = s[index];
        v.pointJumpMinSpeed = std::min(v.pointJumpMinSpeed, v.pointJumpMaxSpeed);
        break;
    case Item::pointJumpFull:
        v.pointJumpFullMs = s[index];
        break;
    case Item::pointJumpPush:
        v.pointJumpNormalBias = s[index];
        break;
    case Item::runSpeed:
        v.runSpeedPercent = s[index];
        break;
    case Item::slowMotionLength:
        v.slowMotionSeconds = s[index];
        break;
    case Item::softLock:
        v.softLockMetres = s[index];
        break;
    case Item::animatedAirSpin:
        v.animatedAirSpin = s[index];
        break;
    case Item::animatedHybridSpeed:
        v.animatedHybridSpeedMs = s[index];
        break;
    case Item::earlyJump:
        v.earlyJump = s[index];
        break;
    case Item::webAimSource:
        v.webAimSource = s[index];
        break;
    case Item::forearmBlend:
        v.forearmBlend = s[index];
        break;
    case Item::forearmAimLimit:
        v.forearmAimLimit = s[index];
        break;
    case Item::webLandingImpact:
        v.webLandingImpact = s[index];
        break;
    case Item::nativeRunAt:
        v.nativeRunPercent = s[index];
        break;
    case Item::nearClip:
        v.nearClipMm = s[index];
        break;
    case Item::recordingFov:
        v.recordingFov = s[index];
        break;
    case Item::uiStyle:
        v.uiStyle = s[index];
        break;
    case Item::blowSounds:
        v.blowSounds = s[index];
        break;
    case Item::height:
        v.heightCm = s[index];
        break;
    case Item::eyeAim:
        v.eyeAim = s[index];
        break;
    case Item::automaticStrength:
        v.automaticStrength = s[index];
        break;
    case Item::automaticSpeed:
        v.automaticSpeed = s[index];
        break;
    case Item::automaticMinimumSpeed:
        v.automaticMinimumSpeed = s[index];
        break;
    case Item::automaticResponse:
        v.automaticResponse = s[index];
        break;
    case Item::automaticHandStrength:
        v.automaticHandStrength = s[index];
        break;
    case Item::automaticFalloff:
        v.automaticFalloff = s[index];
        break;
    case Item::automaticHandThreshold:
        v.automaticHandThreshold = s[index];
        break;
    default:
        return false;
    }
    return !(v == before);
}
int defaultChoice(Item item) {
    return choice(item, Values{});
}
namespace {
// The settings tabOnlyText writes: a switch (flag) or a number.
struct TabOnly {
    std::string_view key;
    bool Values::*flag;
    int Values::*number;
};
constexpr TabOnly tabOnly[] = {
    {"controlled_flips", &Values::controlledFlips, nullptr},
    {"near_clip_mm", nullptr, &Values::nearClipMm},
    {"hand_climb", &Values::handClimb, nullptr},
    {"joystick_wall_run_camera", &Values::joystickRunCamera, nullptr},
    {"wall_run_camera_tilt", &Values::wallRunCameraTilt, nullptr},
    {"half_punch_damage", &Values::halfPunchDamage, nullptr},
    {"realistic_flips", &Values::realisticFlips, nullptr},
    // HANDSTAND (H1, handstand.hpp): spidy_vr.ini only.
    {"handstand", &Values::handstand, nullptr},
    {"handstand_assist", &Values::handstandAssist, nullptr},
    {"handstand_push_gain", nullptr, &Values::handstandPushGain},
    {"handstand_push_max", nullptr, &Values::handstandPushMax},
    {"handstand_fall_degrees", nullptr, &Values::handstandFallDegrees},
    {"handstand_height_cm", nullptr, &Values::handstandHeightCm},
    {"handstand_gain_pct", nullptr, &Values::handstandGainPct},
    {"handstand_eye_drop_cm", nullptr, &Values::handstandEyeDropCm},
    {"rotation_strength", nullptr, &Values::automaticStrength},
    {"rotation_speed", nullptr, &Values::automaticSpeed},
    {"flight_speed_min", nullptr, &Values::automaticMinimumSpeed},
    {"swing_momentum", &Values::automaticReleaseFlip, nullptr},
    {"rotation_response_ms", nullptr, &Values::automaticResponse},
    {"hand_momentum", nullptr, &Values::automaticHandStrength},
    {"angular_drag_ms", nullptr, &Values::automaticFalloff},
    {"hand_speed_min_cm", nullptr, &Values::automaticHandThreshold},
    {"air_rotation_brake", &Values::automaticAirBrake, nullptr},
    {"flips_in_combat", &Values::automaticCombat, nullptr},
    {"body_haptics_webs", &Values::hapticWebs, nullptr},
    {"body_haptics_wind", &Values::hapticWind, nullptr},
    {"body_haptics_combat", &Values::hapticCombat, nullptr},
    {"body_haptics_movement", &Values::hapticMovement, nullptr},
    {"recording_fov", nullptr, &Values::recordingFov},
    {"ui_style", nullptr, &Values::uiStyle},
    {"combat_visuals", &Values::combatVisuals, nullptr},
    {"blow_sounds", nullptr, &Values::blowSounds},
    {"web_slingshot", &Values::webSlingshot, nullptr},
    {"better_climbing", &Values::betterClimbing, nullptr},
    {"height_cm", nullptr, &Values::heightCm},
    {"seated", &Values::seated, nullptr},
    {"eye_aim", nullptr, &Values::eyeAim},
    {"smart_anchor", &Values::smartAnchor, nullptr},
    {"elastic_web", nullptr, &Values::elasticWeb},
    {"soft_release", nullptr, &Values::softRelease},
    {"swing_assist", &Values::swingAssist, nullptr},
    {"slingshot_speed", nullptr, &Values::slingshotSpeed},
    {"recording_smoothing", nullptr, &Values::recordingSmoothing},
    {"two_hand_zip", &Values::twoHandZip, nullptr},
    {"rope_steering", &Values::ropeSteering, nullptr},
    {"web_range_m", nullptr, &Values::webRange},
    {"web_speed", nullptr, &Values::webSpeed},
    {"flip_reprojection", &Values::flipReprojection, nullptr},
    {"steady_hud", &Values::steadyHud, nullptr},
    {"reel_haptics", &Values::reelHaptics, nullptr},
    {"swing_haptics", &Values::swingHaptics, nullptr},
    {"pc_media", &Values::pcMedia, nullptr},
    {"single_hand_yank_percent", nullptr, &Values::singleHandYankPercent},
    {"web_tie", &Values::webTie, nullptr},
    {"native_run", &Values::nativeRun, nullptr},
    {"air_slingshot", &Values::airSlingshot, nullptr},
    {"web_angle_degrees", nullptr, &Values::webAngle},
    {"wrist_shooters", &Values::wristShooters, nullptr},
    {"dodge_tumble", nullptr, &Values::dodgeTumble},
    {"run_curve", &Values::runCurve, nullptr},
    {"native_run_at_percent", nullptr, &Values::nativeRunPercent},
    {"run_logs", &Values::runLogs, nullptr},
    {"running_leap", &Values::momentumLeap, nullptr},
    {"early_jump", nullptr, &Values::earlyJump},
    {"soft_landing", &Values::softLanding, nullptr},
    {"spidy_run", &Values::spidyRun, nullptr},
    {"spidy_run_from_standing", &Values::spidyRunFromStanding, nullptr},
    {"web_aim_source", nullptr, &Values::webAimSource},
    {"forearm_blend", nullptr, &Values::forearmBlend},
    {"web_landing_smooth", &Values::webLandingSmooth, nullptr},
    {"web_landing_impact_speed", nullptr, &Values::webLandingImpact},
    {"steady_states", &Values::steadyStates, nullptr},
    {"combat_responsive", &Values::combatResponsive, nullptr},
    {"spidy_run_jump", &Values::spidyRunJump, nullptr},
    {"forearm_aim_limit", nullptr, &Values::forearmAimLimit},
    {"palm_shooters", &Values::palmShooters, nullptr},
    {"head_probe", &Values::headProbe, nullptr},
    {"animated_moves", &Values::animatedMoves, nullptr},
    {"animated_head", nullptr, &Values::animatedHead},
    {"animated_headset_smooth", nullptr, &Values::animatedHeadsetSmooth},
    {"run_speed_percent", nullptr, &Values::runSpeedPercent},
    {"slow_motion_seconds", nullptr, &Values::slowMotionSeconds},
    {"soft_lock_metres", nullptr, &Values::softLockMetres},
    {"animated_air", &Values::animatedAir, nullptr},
    {"animated_steering", &Values::animatedSteering, nullptr},
    {"animated_arms", &Values::animatedArms, nullptr},
    {"animated_torso", &Values::animatedTorso, nullptr},
    {"animated_model_yaw", &Values::animatedModelYaw, nullptr},
    {"animated_only", &Values::animatedOnly, nullptr},
    {"animated_air_spin", nullptr, &Values::animatedAirSpin},
    {"animated_hybrid", &Values::animatedHybrid, nullptr},
    {"animated_hybrid_speed_ms", nullptr, &Values::animatedHybridSpeedMs},
    {"web_lines", &Values::webLines, nullptr},
    {"animated_full_arms", &Values::animatedFullArms, nullptr},
    {"camera_stabilization", nullptr, &Values::cameraStabilization},
    {"camera_vertical_ms", nullptr, &Values::cameraVerticalMs},
    {"camera_horizontal_ms", nullptr, &Values::cameraHorizontalMs},
    {"camera_angle_ms", nullptr, &Values::cameraAngleMs},
    {"camera_light_vertical_ms", nullptr, &Values::lightVerticalMs},
    {"camera_light_horizontal_ms", nullptr, &Values::lightHorizontalMs},
    {"camera_light_angle_ms", nullptr, &Values::lightAngleMs},
    {"camera_heavy_vertical_ms", nullptr, &Values::heavyVerticalMs},
    {"camera_heavy_horizontal_ms", nullptr, &Values::heavyHorizontalMs},
    {"camera_heavy_angle_ms", nullptr, &Values::heavyAngleMs},
    {"spherical_aim", &Values::sphericalAim, nullptr},
    {"leg_control", &Values::legControl, nullptr},
    {"leg_stiffness_pct", nullptr, &Values::legStiffnessPct},
    {"leg_damping_pct", nullptr, &Values::legDampingPct},
    {"leg_coupling_pct", nullptr, &Values::legCouplingPct},
    {"leg_double_tap_ms", nullptr, &Values::legDoubleTapMs},
    {"punch_view_ease_ms", nullptr, &Values::punchViewEaseMs},
    {"move_haptics_pct", nullptr, &Values::moveHapticsPct},
    {"view_yaw_source", nullptr, &Values::viewYawSource},
    {"sound_trace", &Values::soundTrace, nullptr},
    {"jump_handoff_ms", nullptr, &Values::jumpHandoffMs},
    {"air_view_ease_ms", nullptr, &Values::airViewEaseMs},
    {"anim_yaw_sway_pct", nullptr, &Values::animYawSwayPct},
    {"turn_in_place_deg", nullptr, &Values::turnInPlaceDeg},
    {"passive_legs_pct", nullptr, &Values::passiveLegsPct},
    {"passive_coupling_pct", nullptr, &Values::passiveCouplingPct},
    {"movement_mode", nullptr, &Values::movementMode},
    {"rear_snap_turn_ms", nullptr, &Values::rearSnapMs},
    {"leg_air_crouch", &Values::legAirCrouch, nullptr},
    {"leg_reach_spread", &Values::legReachSpread, nullptr},
    {"air_fists", &Values::airFists, nullptr},
    {"camera_pitch_pct", nullptr, &Values::cameraPitchPct},
    {"camera_roll_pct", nullptr, &Values::cameraRollPct},
    {"camera_bob_vertical_pct", nullptr, &Values::cameraBobVerticalPct},
    {"camera_bob_side_pct", nullptr, &Values::cameraBobSidePct},
    {"camera_head_angle_deg", nullptr, &Values::cameraHeadAngleDeg},
    // QUICK SIDESTEP (November 12, sidestep.hpp): spidy_vr.ini only.
    {"sidestep", &Values::sidestep, nullptr},
    {"sidestep_distance_cm", nullptr, &Values::sidestepDistanceCm},
    {"sidestep_ms", nullptr, &Values::sidestepMs},
    {"sidestep_cooldown_ms", nullptr, &Values::sidestepCooldownMs},
    {"sidestep_chain", nullptr, &Values::sidestepChain},
    {"sidestep_lockout_ms", nullptr, &Values::sidestepLockoutMs},
    {"sidestep_iframes", &Values::sidestepIframes, nullptr},
    {"sidestep_flick_from", nullptr, &Values::sidestepFlickFrom},
    {"sidestep_flick_to", nullptr, &Values::sidestepFlickTo},
    {"sidestep_flick_ms", nullptr, &Values::sidestepFlickMs},
    {"sidestep_a_ms", nullptr, &Values::sidestepAMs},
    {"sidestep_forward", &Values::sidestepForward, nullptr},
    {"point_jump", &Values::pointJump, nullptr},
    {"point_jump_trigger", nullptr, &Values::pointJumpTrigger},
    {"point_jump_chord_ms", nullptr, &Values::pointJumpChordMs},
    {"point_jump_tap_ms", nullptr, &Values::pointJumpTapMs},
    {"point_jump_full_ms", nullptr, &Values::pointJumpFullMs},
    {"point_jump_min_speed", nullptr, &Values::pointJumpMinSpeed},
    {"point_jump_max_speed", nullptr, &Values::pointJumpMaxSpeed},
    {"point_jump_curve", nullptr, &Values::pointJumpCurve},
    {"point_jump_normal_bias", nullptr, &Values::pointJumpNormalBias},
    {"point_jump_min_out_degrees", nullptr, &Values::pointJumpMinOutDegrees},
    {"point_jump_keep", nullptr, &Values::pointJumpKeep},
    {"point_jump_coyote_ms", nullptr, &Values::pointJumpCoyoteMs},
    {"point_jump_buffer_ms", nullptr, &Values::pointJumpBufferMs},
    {"point_jump_cooldown_ms", nullptr, &Values::pointJumpCooldownMs},
    {"point_jump_zip_guard_ms", nullptr, &Values::pointJumpZipGuardMs},
    {"point_jump_dodge_hold_ms", nullptr, &Values::pointJumpDodgeHoldMs},
    {"point_jump_in_combat", &Values::pointJumpInCombat, nullptr},
    // CAMERA MODES (camera_mode.hpp): spidy_vr.ini only.
    {"anim_head_ride", &Values::animHeadRide, nullptr},
    {"shoulder_limit", &Values::shoulderLimit, nullptr},
    {"snap_realign", &Values::snapRealign, nullptr},
    {"movement_defaults", nullptr, &Values::movementDefaults},
};
} // namespace
std::string tabOnlyText(const Values& v) {
    std::string text;
    for (const auto& s : tabOnly) {
        const int value = s.flag ? (v.*s.flag ? 1 : 0) : v.*s.number;
        text.append(s.key).append("=").append(std::to_string(value)).append("\n");
    }
    return text;
}
Values withTabOnly(Values v, std::string_view text) {
    while (!text.empty()) {
        const size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        const size_t equals = line.find('=');
        if (equals == std::string_view::npos)
            continue;
        const std::string_view key = line.substr(0, equals), value = line.substr(equals + 1);
        int number{};
        const auto [rest, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (error != std::errc{} || rest != value.data() + value.size())
            continue;
        for (const auto& s : tabOnly)
            if (key == s.key) {
                if (s.flag)
                    v.*s.flag = number != 0;
                else
                    v.*s.number = number;
            }
    }
    return sanitized(v);
}
Values withMovementDefaults(Values v, std::string_view text) {
    // The revision the text has seen: 0 when it names none (older than the
    // November 12 cleanup, or no file at all).
    int seen = 0;
    while (!text.empty()) {
        const size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        constexpr std::string_view key = "movement_defaults=";
        if (line.substr(0, key.size()) != key)
            continue;
        const std::string_view value = line.substr(key.size());
        int number{};
        const auto [rest, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (error == std::errc{} && rest == value.data() + value.size())
            seen = number;
    }
    if (seen < 1) {
        // Revision 1 (November 12): RUN CURVE on, EARLY JUMP QUICK TAP (RUNNING kept).
        v.runCurve = true;
        v.earlyJump = std::max(v.earlyJump, 1);
    }
    v.movementDefaults = movementDefaultsRevision;
    return sanitized(v);
}
} // namespace spidy::vr_settings
