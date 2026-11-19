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
constexpr int cameraSmoothSteps[] = {0, 25, 50, 75, 100, 150, 200, 300, 500, 750, 1000};
constexpr const char* cameraSmoothChoices[] = {"OFF",    "25 MS",  "50 MS",  "75 MS",  "100 MS", "150 MS",
                                               "200 MS", "300 MS", "500 MS", "750 MS", "1000 MS"};
static_assert(std::size(cameraSmoothChoices) == std::size(cameraSmoothSteps));
// STEADY's rows: the same range with STEADY's own 350/300/450 ms starting points on a step.
constexpr int steadySmoothSteps[] = {0, 50, 100, 200, 300, 350, 450, 600, 750, 900, 1000};
constexpr const char* steadySmoothChoices[] = {"OFF",    "50 MS",  "100 MS", "200 MS", "300 MS", "350 MS",
                                               "450 MS", "600 MS", "750 MS", "900 MS", "1000 MS"};
static_assert(std::size(steadySmoothChoices) == std::size(steadySmoothSteps));
constexpr int cameraStabilizations[] = {0, 1, 2, 3};
constexpr const char* cameraStabilizationChoices[] = {"OFF", "LIGHT", "HEAVY", "STEADY"};
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
constexpr int punchViewEases[] = {0, 100, 125, 150, 175, 200, 225, 250, 300, 350, 400};
constexpr const char* punchViewEaseChoices[] = {"OFF (SNAP)", "100 MS", "125 MS", "150 MS", "175 MS", "200 MS",
                                                "225 MS", "250 MS (DEFAULT)", "300 MS", "350 MS", "400 MS"};
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
constexpr int jumpHandoffs[] = {0, 150, 200, 250, 300, 350, 400, 500, 600, 800, 1000};
constexpr const char* jumpHandoffChoices[] = {"STAY TO LANDING", "150 MS", "200 MS", "250 MS (DEFAULT)", "300 MS", "350 MS", "400 MS", "500 MS", "600 MS", "800 MS", "1000 MS"};
static_assert(std::size(jumpHandoffChoices) == std::size(jumpHandoffs));
// AIR VIEW EASE (view_ease.hpp): air_view_ease_ms; 0 is the old snap.
constexpr int airViewEases[] = {0, 100, 125, 150, 175, 200, 225, 250, 300, 350, 400};
constexpr const char* airViewEaseChoices[] = {"OFF (SNAP)", "100 MS", "125 MS", "150 MS", "175 MS", "200 MS (DEFAULT)", "225 MS", "250 MS", "300 MS", "350 MS", "400 MS"};
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
constexpr int animatedTorsoTwistSteps[] = {0, 20, 40, 60, 70, 80, 90};
constexpr const char* animatedTorsoTwistChoices[] = {"OFF",    "20 DEG", "40 DEG (OLD)", "60 DEG",
                                                     "70 DEG", "80 DEG", "90 DEG"};
static_assert(std::size(animatedTorsoTwistChoices) == std::size(animatedTorsoTwistSteps));
constexpr int animForearmFallbackSteps[] = {0, 45, 60, 75, 90};
constexpr const char* animForearmFallbackChoices[] = {"OFF", "45 DEG", "60 DEG", "75 DEG", "90 DEG"};
static_assert(std::size(animForearmFallbackChoices) == std::size(animForearmFallbackSteps));
constexpr int cameraHorizonLevelSteps[] = {0, 25, 50, 75, 100};
constexpr const char* cameraHorizonLevelChoices[] = {"OFF", "25%", "50%", "75%", "100%"};
static_assert(std::size(cameraHorizonLevelChoices) == std::size(cameraHorizonLevelSteps));
// HANDSTAND (handstand.hpp): each list holds the default and the ini's whole range's ends.
constexpr int handstandFallSteps[] = {15, 20, 25, 30, 35, 40, 45, 50, 60};
constexpr const char* handstandFallChoices[] = {"15 DEG", "20 DEG", "25 DEG", "30 DEG", "35 DEG",
                                                "40 DEG", "45 DEG", "50 DEG (DEFAULT)", "60 DEG"};
static_assert(std::size(handstandFallChoices) == std::size(handstandFallSteps));
constexpr int handstandPushGainSteps[] = {100, 150, 200, 220, 250, 300, 400};
constexpr const char* handstandPushGainChoices[] = {"100%", "150%", "200%", "220%", "250%", "300%", "400%"};
static_assert(std::size(handstandPushGainChoices) == std::size(handstandPushGainSteps));
constexpr int handstandPushMaxSteps[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12};
constexpr const char* handstandPushMaxChoices[] = {"2 M/S", "3 M/S", "4 M/S", "5 M/S",  "6 M/S",
                                                   "7 M/S", "8 M/S", "9 M/S", "10 M/S", "12 M/S"};
static_assert(std::size(handstandPushMaxChoices) == std::size(handstandPushMaxSteps));
constexpr int handstandHeightSteps[] = {60, 70, 80, 90, 95, 100, 110, 120, 130, 140};
constexpr const char* handstandHeightChoices[] = {"60 CM",  "70 CM",  "80 CM",  "90 CM",  "95 CM",
                                                  "100 CM", "110 CM", "120 CM", "130 CM", "140 CM"};
static_assert(std::size(handstandHeightChoices) == std::size(handstandHeightSteps));
constexpr int handstandGainSteps[] = {50, 100, 150, 200, 300, 400, 500, 600};
constexpr const char* handstandGainChoices[] = {"50%",  "100%", "150%", "200%",
                                                "300%", "400%", "500%", "600%"};
static_assert(std::size(handstandGainChoices) == std::size(handstandGainSteps));
constexpr int handstandEyeDropSteps[] = {0, 30, 60, 90, 120};
constexpr const char* handstandEyeDropChoices[] = {"0 CM", "30 CM", "60 CM", "90 CM", "120 CM"};
static_assert(std::size(handstandEyeDropChoices) == std::size(handstandEyeDropSteps));
// THE BALANCE PASS: the support's radius (cm), the off-balance time before a fall (ms), the sway's damping (percent).
constexpr int handstandSupportSteps[] = {10, 12, 15, 18, 22, 26, 30};
constexpr const char* handstandSupportChoices[] = {"10 CM", "12 CM (OLD)", "15 CM", "18 CM (DEFAULT)", "22 CM", "26 CM", "30 CM"};
static_assert(std::size(handstandSupportChoices) == std::size(handstandSupportSteps));
constexpr int handstandGraceSteps[] = {100, 120, 250, 500, 800, 1200, 1500};
constexpr const char* handstandGraceChoices[] = {"100 MS", "120 MS (OLD)", "250 MS", "500 MS (DEFAULT)", "800 MS", "1200 MS", "1500 MS"};
static_assert(std::size(handstandGraceChoices) == std::size(handstandGraceSteps));
constexpr int handstandDampingSteps[] = {0, 100, 250, 400, 600};
constexpr const char* handstandDampingChoices[] = {"NONE (OLD)", "100%", "250% (DEFAULT)", "400%", "600%"};
static_assert(std::size(handstandDampingChoices) == std::size(handstandDampingSteps));
// HAND GRAB (hand_grab.hpp): one hand's force in newtons (what it lifts: 7.5% of it in kg), and the hold cost.
constexpr int handGrabForceSteps[] = {400, 600, 800, 1000, 1200, 1600, 2000, 2400, 3000, 4000};
constexpr const char* handGrabForceChoices[] = {"400 N",  "600 N",  "800 N",  "1000 N", "1200 N",
                                                "1600 N", "2000 N", "2400 N", "3000 N", "4000 N"};
static_assert(std::size(handGrabForceChoices) == std::size(handGrabForceSteps));
constexpr int enemyHoldCostSteps[] = {0, 1, 2, 3, 5};
constexpr const char* enemyHoldCostChoices[] = {"NONE", "1 (DEFAULT)", "2", "3", "5"};
static_assert(std::size(enemyHoldCostChoices) == std::size(enemyHoldCostSteps));
// AIR KICKS (air_kick.hpp): how far from the gaze a zone point can be aimed at, and the share of the kick's spin
// reaction the legs keep.
constexpr int kickGazeConeSteps[] = {10, 15, 20, 25, 30, 40, 50};
constexpr const char* kickGazeConeChoices[] = {"10 DEG", "15 DEG", "20 DEG", "25 DEG", "30 DEG", "40 DEG (DEFAULT)", "50 DEG"};
static_assert(std::size(kickGazeConeChoices) == std::size(kickGazeConeSteps));
constexpr int kickSpinSteps[] = {0, 10, 20, 30, 40, 60, 80, 100};
constexpr const char* kickSpinChoices[] = {"NONE", "10%", "20%", "30% (DEFAULT)", "40%", "60%", "80%", "100%"};
static_assert(std::size(kickSpinChoices) == std::size(kickSpinSteps));
// PUNCH AUTO-TARGET (punch_track.hpp): the cone round the targeted enemy a punch must be thrown in, and the range.
constexpr int punchTrackConeSteps[] = {15, 30, 45, 60, 90, 120};
constexpr const char* punchTrackConeChoices[] = {"15 DEG", "30 DEG", "45 DEG", "60 DEG", "90 DEG", "120 DEG (DEFAULT)"};
static_assert(std::size(punchTrackConeChoices) == std::size(punchTrackConeSteps));
constexpr int punchTrackRangeSteps[] = {3, 4, 6, 8, 10};
constexpr const char* punchTrackRangeChoices[] = {"3 M", "4 M", "6 M", "8 M", "10 M (DEFAULT)"};
static_assert(std::size(punchTrackRangeChoices) == std::size(punchTrackRangeSteps));
// JUMP PULL (jump_pull.hpp): the gaze's cone, the stick's cone, the range, and the most a jump adds (tenths of a m/s).
constexpr int jumpPullGazeSteps[] = {10, 15, 20, 30, 45};
constexpr const char* jumpPullGazeChoices[] = {"10 DEG", "15 DEG", "20 DEG (DEFAULT)", "30 DEG", "45 DEG"};
static_assert(std::size(jumpPullGazeChoices) == std::size(jumpPullGazeSteps));
constexpr int jumpPullStickSteps[] = {15, 25, 35, 45};
constexpr const char* jumpPullStickChoices[] = {"15 DEG", "25 DEG (DEFAULT)", "35 DEG", "45 DEG"};
static_assert(std::size(jumpPullStickChoices) == std::size(jumpPullStickSteps));
constexpr int jumpPullRangeSteps[] = {6, 9, 12, 16, 20};
constexpr const char* jumpPullRangeChoices[] = {"6 M", "9 M", "12 M (DEFAULT)", "16 M", "20 M"};
static_assert(std::size(jumpPullRangeChoices) == std::size(jumpPullRangeSteps));
constexpr int jumpPullCapSteps[] = {5, 10, 15, 20, 30};
constexpr const char* jumpPullCapChoices[] = {"0.5 M/S", "1 M/S", "1.5 M/S (DEFAULT)", "2 M/S", "3 M/S"};
static_assert(std::size(jumpPullCapChoices) == std::size(jumpPullCapSteps));
// SHOWTIME (showtime.hpp): its rows' steps.
constexpr int showtimeWhileSteps[] = {0, 1};
constexpr const char* showtimeWhileChoices[] = {"WATCH ONLY (DEFAULT)", "KEEP CONTROL"};
static_assert(std::size(showtimeWhileChoices) == std::size(showtimeWhileSteps));
constexpr int showtimeFadeSteps[] = {0, 100, 150, 250, 400};
constexpr const char* showtimeFadeChoices[] = {"CUT (NO FADE)", "100 MS", "150 MS (DEFAULT)", "250 MS", "400 MS"};
static_assert(std::size(showtimeFadeChoices) == std::size(showtimeFadeSteps));
constexpr int wallJumpSoundSteps[] = {0, 1, 2};
constexpr const char* wallJumpSoundChoices[] = {"OFF", "SOUND A (DEFAULT)", "SOUND B"};
static_assert(std::size(wallJumpSoundChoices) == std::size(wallJumpSoundSteps));
constexpr int showtimeBodySteps[] = {0, 1, 2, 3};
constexpr const char* showtimeBodyChoices[] = {"GAME POSE, HEAD SHOWN (DEFAULT)", "AS IN PLAY (NO HEAD)",
                                             "NATURAL ARMS", "GAME SWING POSE (EXPERIMENTAL)"};
static_assert(std::size(showtimeBodyChoices) == std::size(showtimeBodySteps));
constexpr int showtimeChordSteps[] = {0, 1};
constexpr const char* showtimeChordChoices[] = {"HOLD X+B (DEFAULT)", "AUTO ONLY"};
static_assert(std::size(showtimeChordChoices) == std::size(showtimeChordSteps));
constexpr int showtimeAutoDodgeSteps[] = {0, 1, 2};
constexpr const char* showtimeAutoDodgeChoices[] = {"OFF", "PERFECT ONLY (DEFAULT)", "ALL"};
static_assert(std::size(showtimeAutoDodgeChoices) == std::size(showtimeAutoDodgeSteps));
constexpr int showtimeDistanceSteps[] = {150, 200, 250, 300, 350, 400};
constexpr const char* showtimeDistanceChoices[] = {"1.5 M", "2 M", "2.5 M (DEFAULT)", "3 M", "3.5 M", "4 M"};
static_assert(std::size(showtimeDistanceChoices) == std::size(showtimeDistanceSteps));
constexpr int showtimeHeightSteps[] = {-50, 0, 30, 60, 100, 150};
constexpr const char* showtimeHeightChoices[] = {"-0.5 M", "0 M", "+0.3 M (DEFAULT)", "+0.6 M", "+1 M", "+1.5 M"};
static_assert(std::size(showtimeHeightChoices) == std::size(showtimeHeightSteps));
constexpr int showtimeAngleSteps[] = {0, 20, 35, 50, 90};
constexpr const char* showtimeAngleChoices[] = {"0 DEGREES", "20 DEGREES", "35 DEGREES (DEFAULT)", "50 DEGREES", "90 DEGREES"};
static_assert(std::size(showtimeAngleChoices) == std::size(showtimeAngleSteps));
constexpr int showtimeTurnSteps[] = {0, 1};
constexpr const char* showtimeTurnChoices[] = {"SMOOTH (DEFAULT)", "SNAP 30 DEGREES"};
static_assert(std::size(showtimeTurnChoices) == std::size(showtimeTurnSteps));
constexpr int showtimeFollowSteps[] = {0, 1, 2};
constexpr const char* showtimeFollowChoices[] = {"SOFT", "NORMAL (DEFAULT)", "TIGHT"};
static_assert(std::size(showtimeFollowChoices) == std::size(showtimeFollowSteps));
constexpr int showtimeVignetteSteps[] = {0, 1, 2};
constexpr const char* showtimeVignetteChoices[] = {"OFF", "LIGHT (DEFAULT)", "STRONG"};
static_assert(std::size(showtimeVignetteChoices) == std::size(showtimeVignetteSteps));
constexpr int showtimeLatchSteps[] = {0, 1};
constexpr const char* showtimeLatchChoices[] = {"OFF (DEFAULT)", "DOUBLE PRESS"};
static_assert(std::size(showtimeLatchChoices) == std::size(showtimeLatchSteps));
constexpr int airCorrectSteps[] = {400, 600, 800, 1000, 1500, 2000};
constexpr const char* airCorrectChoices[] = {"0.4 S", "0.6 S", "0.8 S", "1 S (DEFAULT)", "1.5 S", "2 S"};
static_assert(std::size(airCorrectChoices) == std::size(airCorrectSteps));
constexpr int fallHandoffSteps[] = {0, 50, 75, 100, 125, 150, 175, 200, 250, 300, 400};
constexpr const char* fallHandoffChoices[] = {"WITH THE JUMP TIMER", "50 MS", "75 MS", "100 MS", "125 MS", "150 MS (DEFAULT)",
                                              "175 MS", "200 MS", "250 MS", "300 MS", "400 MS"};
static_assert(std::size(fallHandoffChoices) == std::size(fallHandoffSteps));
// HANDOFF TUNE (Nov 18): finer steps for the handoff and landing timing (in-game rows hold 11 at most).
constexpr int landingBlendSteps[] = {0, 100, 125, 150, 175, 200, 225, 250, 300, 400, 500};
constexpr const char* landingBlendChoices[] = {"AIR VIEW EASE", "100 MS", "125 MS", "150 MS", "175 MS", "200 MS (DEFAULT)", "225 MS", "250 MS", "300 MS", "400 MS", "500 MS"};
static_assert(std::size(landingBlendChoices) == std::size(landingBlendSteps));
constexpr int handoffGlideSpeedSteps[] = {0, 150, 200, 250, 300, 350, 400, 450, 500, 600, 800};
constexpr const char* handoffGlideSpeedChoices[] = {"OFF (BASE TIME)", "150 CM/S", "200 CM/S", "250 CM/S", "300 CM/S", "350 CM/S (DEFAULT)", "400 CM/S", "450 CM/S", "500 CM/S", "600 CM/S", "800 CM/S"};
static_assert(std::size(handoffGlideSpeedChoices) == std::size(handoffGlideSpeedSteps));
constexpr int handoffGlideLongestSteps[] = {150, 200, 250, 300, 350, 400, 450, 500, 600, 700, 1000};
constexpr const char* handoffGlideLongestChoices[] = {"150 MS", "200 MS", "250 MS", "300 MS", "350 MS", "400 MS", "450 MS (DEFAULT)", "500 MS", "600 MS", "700 MS", "1000 MS"};
static_assert(std::size(handoffGlideLongestChoices) == std::size(handoffGlideLongestSteps));
constexpr int handoffPlaceSteps[] = {0, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000};
constexpr const char* handoffPlaceChoices[] = {"ALWAYS GLIDE", "25 CM", "50 CM", "75 CM", "100 CM", "125 CM (DEFAULT)", "150 CM", "200 CM", "300 CM", "500 CM", "1000 CM"};
static_assert(std::size(handoffPlaceChoices) == std::size(handoffPlaceSteps));
constexpr int handoffAngleSteps[] = {0, 100, 150, 200, 250, 300, 350, 400, 500, 700, 1000};
constexpr const char* handoffAngleChoices[] = {"BLEND LENGTH", "100 MS", "150 MS", "200 MS (DEFAULT)", "250 MS", "300 MS", "350 MS", "400 MS", "500 MS", "700 MS", "1000 MS"};
static_assert(std::size(handoffAngleChoices) == std::size(handoffAngleSteps));
constexpr int headSeatGlideSteps[] = {60, 100, 125, 150, 165, 180, 200, 250, 300, 400, 500};
constexpr const char* headSeatGlideChoices[] = {"60 MS", "100 MS", "125 MS", "150 MS", "165 MS (DEFAULT)", "180 MS", "200 MS", "250 MS", "300 MS", "400 MS", "500 MS"};
static_assert(std::size(headSeatGlideChoices) == std::size(headSeatGlideSteps));
constexpr int landingBodyEaseSteps[] = {100, 150, 200, 250, 300, 350, 400, 500, 600, 800, 1000};
constexpr const char* landingBodyEaseChoices[] = {"100 MS", "150 MS", "200 MS", "250 MS", "300 MS", "350 MS", "400 MS", "500 MS (DEFAULT)", "600 MS", "800 MS", "1000 MS"};
static_assert(std::size(landingBodyEaseChoices) == std::size(landingBodyEaseSteps));
// PHYSICAL WEB TETHERS (tether.hpp): the rope's points and its steps a frame.
constexpr int tetherParticleSteps[] = {12, 16, 20, 24, 28, 32};
constexpr const char* tetherParticleChoices[] = {"12", "16", "20", "24 (DEFAULT)", "28", "32"};
static_assert(std::size(tetherParticleChoices) == std::size(tetherParticleSteps));
constexpr int tetherSubstepSteps[] = {4, 6, 8, 10, 12, 16};
constexpr const char* tetherSubstepChoices[] = {"4", "6", "8 (DEFAULT)", "10", "12", "16"};
static_assert(std::size(tetherSubstepChoices) == std::size(tetherSubstepSteps));
constexpr int cameraVrStabilizationSteps[] = {0, 1};
constexpr const char* cameraVrStabilizationChoices[] = {"OFF", "ON"};
static_assert(std::size(cameraVrStabilizationChoices) == std::size(cameraVrStabilizationSteps));
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
    case Item::cameraVrStabilization:
        return cameraVrStabilizationSteps;
    case Item::cameraHorizonLevel:
        return cameraHorizonLevelSteps;
    case Item::handstandFall:
        return handstandFallSteps;
    case Item::handstandPushGain:
        return handstandPushGainSteps;
    case Item::handstandPushMax:
        return handstandPushMaxSteps;
    case Item::handstandHeight:
        return handstandHeightSteps;
    case Item::handstandGain:
        return handstandGainSteps;
    case Item::handstandEyeDrop:
        return handstandEyeDropSteps;
    case Item::handstandSupport:
        return handstandSupportSteps;
    case Item::handstandGrace:
        return handstandGraceSteps;
    case Item::handstandDamping:
        return handstandDampingSteps;
    case Item::handGrabForce:
        return handGrabForceSteps;
    case Item::enemyHoldCost:
        return enemyHoldCostSteps;
    case Item::kickGazeCone:
        return kickGazeConeSteps;
    case Item::punchTrackCone:
        return punchTrackConeSteps;
    case Item::punchTrackRange:
        return punchTrackRangeSteps;
    case Item::jumpPullGaze:
        return jumpPullGazeSteps;
    case Item::jumpPullStick:
        return jumpPullStickSteps;
    case Item::jumpPullRange:
        return jumpPullRangeSteps;
    case Item::jumpPullCap:
        return jumpPullCapSteps;
    case Item::showtimeWhile:
        return showtimeWhileSteps;
    case Item::showtimeFade:
        return showtimeFadeSteps;
    case Item::showtimeBody:
        return showtimeBodySteps;
    case Item::wallJumpSound:
        return wallJumpSoundSteps;
    case Item::showtimeChord:
        return showtimeChordSteps;
    case Item::showtimeAutoDodge:
        return showtimeAutoDodgeSteps;
    case Item::showtimeDistance:
        return showtimeDistanceSteps;
    case Item::showtimeHeight:
        return showtimeHeightSteps;
    case Item::showtimeAngle:
        return showtimeAngleSteps;
    case Item::showtimeTurn:
        return showtimeTurnSteps;
    case Item::showtimeFollow:
        return showtimeFollowSteps;
    case Item::showtimeVignette:
        return showtimeVignetteSteps;
    case Item::showtimeLatch:
        return showtimeLatchSteps;
    case Item::airCorrect:
        return airCorrectSteps;
    case Item::fallHandoff:
        return fallHandoffSteps;
    case Item::landingBlend:
        return landingBlendSteps;
    case Item::handoffGlideSpeed:
        return handoffGlideSpeedSteps;
    case Item::handoffGlideLongest:
        return handoffGlideLongestSteps;
    case Item::handoffPlace:
        return handoffPlaceSteps;
    case Item::handoffAngle:
        return handoffAngleSteps;
    case Item::headSeatGlide:
        return headSeatGlideSteps;
    case Item::landingBodyEase:
        return landingBodyEaseSteps;
    case Item::kickSpin:
        return kickSpinSteps;
    case Item::tetherParticles:
        return tetherParticleSteps;
    case Item::tetherSubsteps:
        return tetherSubstepSteps;
    case Item::steadyAngle:
        return steadySmoothSteps;
    case Item::steadyHorizontal:
        return steadySmoothSteps;
    case Item::steadyVertical:
        return steadySmoothSteps;
    case Item::animForearmFallback:
        return animForearmFallbackSteps;
    case Item::animatedTorsoTwist:
        return animatedTorsoTwistSteps;
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
    case Item::cameraVrStabilization:
        return static_cast<float>(v.cameraVrStabilization);
    case Item::cameraHorizonLevel:
        return static_cast<float>(v.cameraHorizonLevelPct);
    case Item::handstandFall:
        return static_cast<float>(v.handstandFallDegrees);
    case Item::handstandPushGain:
        return static_cast<float>(v.handstandPushGain);
    case Item::handstandPushMax:
        return static_cast<float>(v.handstandPushMax);
    case Item::handstandHeight:
        return static_cast<float>(v.handstandHeightCm);
    case Item::handstandGain:
        return static_cast<float>(v.handstandGainPct);
    case Item::handstandEyeDrop:
        return static_cast<float>(v.handstandEyeDropCm);
    case Item::handstandSupport:
        return static_cast<float>(v.handstandSupportCm);
    case Item::handstandGrace:
        return static_cast<float>(v.handstandGraceMs);
    case Item::handstandDamping:
        return static_cast<float>(v.handstandDampingPct);
    case Item::handGrabForce:
        return static_cast<float>(v.handGrabForce);
    case Item::enemyHoldCost:
        return static_cast<float>(v.enemyHoldCost);
    case Item::kickGazeCone:
        return static_cast<float>(v.kickGazeConeDeg);
    case Item::punchTrackCone:
        return static_cast<float>(v.punchTrackConeDeg);
    case Item::punchTrackRange:
        return static_cast<float>(v.punchTrackRangeM);
    case Item::jumpPullGaze:
        return static_cast<float>(v.jumpPullGazeDeg);
    case Item::jumpPullStick:
        return static_cast<float>(v.jumpPullStickDeg);
    case Item::jumpPullRange:
        return static_cast<float>(v.jumpPullRangeM);
    case Item::jumpPullCap:
        return static_cast<float>(v.jumpPullCapDmps);
    case Item::showtimeWhile:
        return static_cast<float>(v.showtimeWhile);
    case Item::showtimeFade:
        return static_cast<float>(v.showtimeFadeMs);
    case Item::showtimeBody:
        return static_cast<float>(v.showtimeBody);
    case Item::wallJumpSound:
        return static_cast<float>(v.wallJumpSound);
    case Item::showtimeChord:
        return static_cast<float>(v.showtimeChord);
    case Item::showtimeAutoDodge:
        return static_cast<float>(v.showtimeAutoDodge);
    case Item::showtimeDistance:
        return static_cast<float>(v.showtimeDistanceCm);
    case Item::showtimeHeight:
        return static_cast<float>(v.showtimeHeightCm);
    case Item::showtimeAngle:
        return static_cast<float>(v.showtimeAngleDeg);
    case Item::showtimeTurn:
        return static_cast<float>(v.showtimeTurn);
    case Item::showtimeFollow:
        return static_cast<float>(v.showtimeFollow);
    case Item::showtimeVignette:
        return static_cast<float>(v.showtimeVignette);
    case Item::showtimeLatch:
        return static_cast<float>(v.showtimeLatch);
    case Item::airCorrect:
        return static_cast<float>(v.airCorrectMs);
    case Item::fallHandoff:
        return static_cast<float>(v.fallHandoffMs);
    case Item::landingBlend:
        return static_cast<float>(v.landingBlendMs);
    case Item::handoffGlideSpeed:
        return static_cast<float>(v.handoffGlideCmS);
    case Item::handoffGlideLongest:
        return static_cast<float>(v.handoffGlideMaxMs);
    case Item::handoffPlace:
        return static_cast<float>(v.handoffPlaceCm);
    case Item::handoffAngle:
        return static_cast<float>(v.handoffAngleMs);
    case Item::headSeatGlide:
        return static_cast<float>(v.headSeatGlideMs);
    case Item::landingBodyEase:
        return static_cast<float>(v.landingBodyEaseMs);
    case Item::kickSpin:
        return static_cast<float>(v.kickSpinPct);
    case Item::tetherParticles:
        return static_cast<float>(v.tetherParticles);
    case Item::tetherSubsteps:
        return static_cast<float>(v.tetherSubsteps);
    case Item::steadyAngle:
        return static_cast<float>(v.steadyAngleMs);
    case Item::steadyHorizontal:
        return static_cast<float>(v.steadyHorizontalMs);
    case Item::steadyVertical:
        return static_cast<float>(v.steadyVerticalMs);
    case Item::animForearmFallback:
        return static_cast<float>(v.animForearmFallbackDeg);
    case Item::animatedTorsoTwist:
        return static_cast<float>(v.animatedTorsoTwist);
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
    case Item::enemyTypes:
        return &v.enemyTypes;
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
    case Item::swingKickManual:
        return &v.swingKickManual;
    case Item::swingKickAuto:
        return &v.swingKickAuto;
    case Item::handstand:
        return &v.handstand;
    case Item::handstandAssist:
        return &v.handstandAssist;
    case Item::handstandLeap:
        return &v.handstandLeap;
    case Item::wallBrake:
        return &v.wallBrake;
    case Item::gripFling:
        return &v.gripFling;
    case Item::handGrab:
        return &v.handGrab;
    case Item::handGrabClassRules:
        return &v.handGrabClassRules;
    case Item::kicks:
        return &v.kicks;
    case Item::shoulderLimit:
        return &v.shoulderLimit;
    case Item::animHeadRide:
        return &v.animHeadRide;
    case Item::headsetFirst:
        return &v.headsetFirst;
    case Item::punchAutotarget:
        return &v.punchAutotarget;
    case Item::punchViewSnap:
        return &v.punchViewSnap;
    case Item::jumpPull:
        return &v.jumpPull;
    case Item::showtime:
        return &v.showtime;
    case Item::showtimeAutoFinish:
        return &v.showtimeAutoFinish;
    case Item::showtimeSlowMo:
        return &v.showtimeSlowMo;
    case Item::pointJumpBullet:
        return &v.pointJumpBullet;
    case Item::pointJumpSound:
        return &v.pointJumpSound;
    case Item::airLandingBlend:
        return &v.airLandingBlend;
    case Item::airLandingRelease:
        return &v.airLandingRelease;
    case Item::groundKicks:
        return &v.groundKicks;
    case Item::enemyJoints:
        return &v.enemyJoints;
    case Item::enemyLimbZones:
        return &v.enemyLimbZones;
    case Item::physicalTethers:
        return &v.physicalTethers;
    case Item::legAirCrouch:
        return &v.legAirCrouch;
    case Item::legReachSpread:
        return &v.legReachSpread;
    case Item::airPoses:
        return &v.airPoses;
    case Item::directionalFlips:
        return &v.directionalFlips;
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
    v.cameraStabilization = std::clamp(v.cameraStabilization, 0, 3);
    for (int Values::*ms : {&Values::cameraVerticalMs, &Values::cameraHorizontalMs, &Values::cameraAngleMs,
                            &Values::lightVerticalMs, &Values::lightHorizontalMs, &Values::lightAngleMs,
                            &Values::heavyVerticalMs, &Values::heavyHorizontalMs, &Values::heavyAngleMs})
        v.*ms = std::clamp(v.*ms, 0, 1000);
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
    v.cameraVrStabilization = std::clamp(v.cameraVrStabilization, 0, 1);
    v.cameraHorizonLevelPct = std::clamp(v.cameraHorizonLevelPct, 0, 100);
    v.steadyAngleMs = std::clamp(v.steadyAngleMs, 0, 1000);
    v.steadyHorizontalMs = std::clamp(v.steadyHorizontalMs, 0, 1000);
    v.steadyVerticalMs = std::clamp(v.steadyVerticalMs, 0, 1000);
    v.animForearmFallbackDeg = std::clamp(v.animForearmFallbackDeg, 0, 180);
    v.animatedTorsoTwist = std::clamp(v.animatedTorsoTwist, 0, 90);
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
    v.logLevel = std::clamp(v.logLevel, 0, 2);
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
    v.wallBrakeMuPct = std::clamp(v.wallBrakeMuPct, 30, 300);
    v.gripFlingGainPct = std::clamp(v.gripFlingGainPct, 50, 300);
    v.gripFlingMax = std::clamp(v.gripFlingMax, 3, 20);
    v.handGrabForce = std::clamp(v.handGrabForce, 400, 4000);
    v.enemyHoldCost = std::clamp(v.enemyHoldCost, 0, 5);
    v.kickGazeConeDeg = std::clamp(v.kickGazeConeDeg, 10, 50);
    v.punchTrackConeDeg = std::clamp(v.punchTrackConeDeg, 15, 120);
    v.punchTrackRangeM = std::clamp(v.punchTrackRangeM, 3, 10);
    v.jumpPullGazeDeg = std::clamp(v.jumpPullGazeDeg, 10, 45);
    v.jumpPullStickDeg = std::clamp(v.jumpPullStickDeg, 15, 45);
    v.jumpPullRangeM = std::clamp(v.jumpPullRangeM, 6, 20);
    v.jumpPullCapDmps = std::clamp(v.jumpPullCapDmps, 5, 30);
    v.showtimeWhile = std::clamp(v.showtimeWhile, 0, 1);
    v.showtimeFadeMs = std::clamp(v.showtimeFadeMs, 0, 400);
    v.showtimeBody = std::clamp(v.showtimeBody, 0, 3);
    v.wallJumpSound = std::clamp(v.wallJumpSound, 0, 2);
    v.showtimeChord = std::clamp(v.showtimeChord, 0, 1);
    v.showtimeAutoDodge = std::clamp(v.showtimeAutoDodge, 0, 2);
    v.showtimeDistanceCm = std::clamp(v.showtimeDistanceCm, 150, 400);
    v.showtimeHeightCm = std::clamp(v.showtimeHeightCm, -50, 150);
    v.showtimeAngleDeg = std::clamp(v.showtimeAngleDeg, 0, 90);
    v.showtimeTurn = std::clamp(v.showtimeTurn, 0, 1);
    v.showtimeFollow = std::clamp(v.showtimeFollow, 0, 2);
    v.showtimeVignette = std::clamp(v.showtimeVignette, 0, 2);
    v.showtimeLatch = std::clamp(v.showtimeLatch, 0, 1);
    v.airCorrectMs = std::clamp(v.airCorrectMs, 300, 2500);
    v.fallHandoffMs = std::clamp(v.fallHandoffMs, 0, 1000);
    v.kickSpinPct = std::clamp(v.kickSpinPct, 0, 100);
    v.tetherParticles = std::clamp(v.tetherParticles, 8, 32);
    v.tetherSubsteps = std::clamp(v.tetherSubsteps, 2, 16);
    v.handstandFallDegrees = std::clamp(v.handstandFallDegrees, 15, 60);
    v.handstandHeightCm = std::clamp(v.handstandHeightCm, 60, 140);
    v.handstandGainPct = std::clamp(v.handstandGainPct, 50, 600);
    v.handstandEyeDropCm = std::clamp(v.handstandEyeDropCm, 0, 120);
    v.handstandSupportCm = std::clamp(v.handstandSupportCm, 10, 30);
    v.handstandGraceMs = std::clamp(v.handstandGraceMs, 100, 1500);
    v.handstandDampingPct = std::clamp(v.handstandDampingPct, 0, 600);
    v.automaticHandStrength = std::clamp(v.automaticHandStrength, 0, 200);
    v.automaticFalloff = std::clamp(v.automaticFalloff, 500, 3000);
    v.automaticHandThreshold = std::clamp(v.automaticHandThreshold, 50, 300);
    v.handstandCatchHoldCm = std::clamp(v.handstandCatchHoldCm, 5, 60);
    v.handstandReachCm = std::clamp(v.handstandReachCm, 60, 200);
    v.headHeightFadeMs = std::clamp(v.headHeightFadeMs, 0, 1500);
    v.punchCameraLockMs = std::clamp(v.punchCameraLockMs, 0, 1000);
    v.snapTapMs = std::clamp(v.snapTapMs, 60, 400);
    v.landingBodyEaseMs = std::clamp(v.landingBodyEaseMs, 100, 1500);
    v.handoffEaseCurve = std::clamp(v.handoffEaseCurve, 0, 1);
    v.handoffGlideCmS = std::clamp(v.handoffGlideCmS, 0, 1000);
    v.handoffGlideMaxMs = std::clamp(v.handoffGlideMaxMs, 150, 2000);
    v.handoffAngleMs = std::clamp(v.handoffAngleMs, 0, 1500);
    v.landingBlendMs = std::clamp(v.landingBlendMs, 0, 1500);
    v.headSeatGlideMs = std::clamp(v.headSeatGlideMs, 60, 1000);
    v.handstandArmsOverHeadCm = std::clamp(v.handstandArmsOverHeadCm, 0, 60);
    v.markerSwitchHoldMs = std::clamp(v.markerSwitchHoldMs, 0, 500);
    v.markerTurnRateDps = std::clamp(v.markerTurnRateDps, 0, 2000);
    v.handstandTipInReachCm = std::clamp(v.handstandTipInReachCm, 60, 300);
    v.handstandTipInDeg = std::clamp(v.handstandTipInDeg, 100, 180);
    v.handstandGraceAfterMs = std::clamp(v.handstandGraceAfterMs, 100, 1000);
    v.handstandGraceBeforeMs = std::clamp(v.handstandGraceBeforeMs, 0, 1000);
    v.handoffPlaceCm = std::clamp(v.handoffPlaceCm, 0, 2000);
    v.turnPushPct = std::clamp(v.turnPushPct, 20, 100);
    v.targetScanIdleMs = std::clamp(v.targetScanIdleMs, 250, 2000);
    return v;
}
namespace {
constexpr const char* helpChoices[] = {"READ"};
}
const std::array<Row, rowTotal>& rows() {
    // The help fits the game's description column beside the rows.
    static const std::array<Row, settingRowCount> settings{{
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
        {Item::animatedAir, "AIR (ADVANCED)", "Legacy animation in air. Ignored while Hybrid is on.", {}, Place::launcher},
        {Item::animatedOnly,
         "ONLY (ADVANCED)",
         "Legacy animation-only ownership. Ignored while Hybrid is on.",
         {}, Place::launcher},
        {Item::animatedSteering,
         "STEERING (ADVANCED)",
         "Legacy actor-relative steering. Ignored while Hybrid is on.",
         {}, Place::launcher},
        {Item::animatedAirSpin, "AIR SPIN (ADVANCED)", "Legacy animated flips. Ignored while Hybrid is on.",
         animatedAirSpinChoices, Place::launcher},
        {Item::runSpeed, "RUN SPEED", "Scales SpidyVR's own ground run (SPIDY RUN). The game's run is unchanged.",
         runSpeedChoices},
        {Item::slowMotionLength, "SLOW-MO LENGTH", "L3 slow motion on a full meter. INFINITE: until L3 again.",
         slowMotionLengthChoices},
        {Item::softLock, "SOFT LOCK", "Grip webs snap to an enemy or prop this near your aim. Closer wins.",
         softLockChoices},
        {Item::cameraStabilization, "STABILIZATION",
         "Steadies the camera. LIGHT/HEAVY/STEADY use their rows; OFF the CAMERA rows.",
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
        // The launcher's Enemy types, not in the game's tab (its screen is Movement, next to the other combat
        // rows). Before the realistic flips' rows, which come last, so that the numbers of the rows before it
        // stay as they were.
        {Item::enemyTypes,
         "ENEMY TYPES",
         "Enemies that block in the base game block base attacks until webbed or stunned.",
         {},
         Place::launcher},
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
        {Item::animatedTorsoTwist, "TORSO TWIST",
         "Most his chest turns toward you so the arms reach your hands. 40: old cap.",
         animatedTorsoTwistChoices, Place::launcher},
        {Item::animForearmFallback, "ARM AIM FALLBACK",
         "Webs aim along your controller past this forearm angle. OFF: forearm only.",
         animForearmFallbackChoices, Place::launcher},
        {Item::steadyVertical, "STEADY VERTICAL", "Up/down smoothing used by STABILIZATION STEADY.",
         steadySmoothChoices, Place::launcher},
        {Item::steadyHorizontal, "STEADY HORIZONTAL", "Side/forward smoothing used by STABILIZATION STEADY.",
         steadySmoothChoices, Place::launcher},
        {Item::steadyAngle, "STEADY ANGLE", "Turn smoothing used by STABILIZATION STEADY.",
         steadySmoothChoices, Place::launcher},
        {Item::cameraHorizonLevel, "HORIZON LEVEL",
         "Removes this share of roll and pitch tilt, in SpidyVR too. Flips pass whole.",
         cameraHorizonLevelChoices, Place::launcher},
        {Item::cameraVrStabilization, "VR STABILIZATION",
         "Applies CAMERA and STABILIZATION to SpidyVR (swing, air, combat, wall) too.",
         cameraVrStabilizationChoices, Place::launcher},
        {Item::swingKickManual,
         "SWING KICK (EXPERIMENTAL)",
         "Swinging, Y+X, flick the right stick up: kick the ringed enemy within 30 deg.",
         {},
         Place::launcher},
        {Item::swingKickAuto,
         "AUTO DIVE KICK",
         "A fast flight at the ringed enemy dives in feet first. Off: kick by hand only.",
         {},
         Place::launcher},
        {Item::kicks,
         "AIR KICKS (EXPERIMENTAL)",
         "In the air, Y+X, flick the right stick: dive, double dive, roundhouse or a kick.",
         {}},
        {Item::kickGazeCone, "AIR KICK AIM",
         "How far from your gaze a head, chest or leg can be and still be kicked at.",
         kickGazeConeChoices},
        {Item::kickSpin, "AIR KICK SPIN",
         "How much of a kick's spin the legs pass to your body. NONE: no spin.",
         kickSpinChoices, Place::launcher},
        {Item::groundKicks,
         "GROUND KICKS",
         "Standing, Y+X, flick the right stick: front kick, side kick or a leg sweep.",
         {}},
        {Item::enemyJoints,
         "ENEMY JOINTS (EXPERIMENTAL)",
         "Kicks use an enemy's own head and chest joints for zones, not just his height.",
         {},
         Place::launcher},
        {Item::enemyLimbZones,
         "ENEMY LIMB ZONES (EXPERIMENTAL)",
         "Arms and legs from his joints. Run probe_c0_skeleton.py first: untested.",
         {},
         Place::launcher},
        {Item::wallBrake,
         "WALL BRAKE (EXPERIMENTAL)",
         "On a wall, squeeze a grip with your palm on it to brake a slide, then hold on.",
         {},
         Place::launcher},
        {Item::gripFling,
         "GRIP FLING (EXPERIMENTAL)",
         "Grip a surface, pull your hand and let go: you launch the other way (9 m/s max).",
         {},
         Place::launcher},
        {Item::handGrab,
         "HAND GRAB (EXPERIMENTAL)",
         "Squeeze a grip with your palm on an enemy or prop: hold, punch, throw, slam.",
         {},
         Place::launcher},
        {Item::handGrabForce, "HAND GRAB STRENGTH",
         "One hand's force: 7.5% of it in kg is lifted (90 kg at 1200 N, the default).",
         handGrabForceChoices, Place::launcher},
        {Item::enemyHoldCost, "HAND GRAB HOLD COST",
         "How fast holding an enemy tires your hand; a heavy one much faster.",
         enemyHoldCostChoices, Place::launcher},
        {Item::physicalTethers,
         "WEB TETHERS (EXPERIMENTAL)",
         "Whip, spin or throw what a web holds; a trigger-held throw keeps the line.",
         {},
         Place::launcher},
        {Item::tetherParticles, "TETHER POINTS",
         "How many points the rope is simulated with. More is smoother, costs more.",
         tetherParticleChoices, Place::launcher},
        {Item::tetherSubsteps, "TETHER STEPS",
         "How many steps a frame the rope takes. More is stiffer and costs more.",
         tetherSubstepChoices, Place::launcher},
        {Item::headsetFirst,
         "HEADSET FIRST",
         "The stick runs where your headset looks when you press it. Off: the old heading.",
         {},
         Place::launcher},
        {Item::animHeadRide,
         "ANIM HEAD RIDE",
         "In a landing or perch the view rides his head, turn too. Off by default.",
         {},
         Place::launcher},
        {Item::shoulderLimit,
         "SHOULDER LIMIT",
         "Turns the view back when your head is turned past his shoulders. Off by default.",
         {},
         Place::launcher},
        {Item::punchAutotarget,
         "PUNCH AUTO-TARGET",
         "A punch at the enemy you have targeted turns your body and aim onto him.",
         {},
         Place::launcher},
        {Item::punchTrackCone,
         "PUNCH TRACK CONE",
         "How far off the targeted enemy a punch can be thrown and still track him.",
         punchTrackConeChoices},
        {Item::punchTrackRange,
         "PUNCH TRACK RANGE",
         "How far away the targeted enemy can be and still be tracked.",
         punchTrackRangeChoices},
        {Item::punchViewSnap,
         "PUNCH VIEW SNAP",
         "A tracked punch turns your view onto him too. Off: your view stays put.",
         {},
         Place::launcher},
        {Item::jumpPull,
         "JUMP PULL",
         "In your own air a jump toward an enemy is pulled toward him a little. Off: not.",
         {}},
        {Item::jumpPullGaze,
         "JUMP PULL GAZE",
         "How far off the enemy you look and still be pulled toward him.",
         jumpPullGazeChoices, Place::launcher},
        {Item::jumpPullStick,
         "JUMP PULL STICK",
         "How far off the enemy the left stick can point and still pull you toward him.",
         jumpPullStickChoices, Place::launcher},
        {Item::jumpPullRange,
         "JUMP PULL RANGE",
         "How far away an enemy can be and still pull you.",
         jumpPullRangeChoices, Place::launcher},
        {Item::jumpPullCap,
         "JUMP PULL LIMIT",
         "The most speed a jump's pull adds in all.",
         jumpPullCapChoices, Place::launcher},
        {Item::showtime,
         "SHOWTIME (EXPERIMENTAL)",
         "Hold X+B and your view steps outside Spidey. Let go to return.",
         {}},
        {Item::showtimeWhile,
         "WHILE OUT",
         "WATCH ONLY: nothing starts. KEEP CONTROL: walk, jump, dodge.",
         showtimeWhileChoices},
        {Item::showtimeFade,
         "FADE TIME",
         "Fade to black and back at each cut. CUT: no fade.",
         showtimeFadeChoices},
        {Item::showtimeBody,
         "BODY (ADVANCED)",
         "Head shown outside. AS IN PLAY: hidden. NATURAL ARMS: his own arm length.",
         showtimeBodyChoices},
        {Item::showtimeChord,
         "CHORD",
         "HOLD X+B steps outside by hand. AUTO ONLY: no chord, only the automatic moments.",
         showtimeChordChoices},
        {Item::showtimeAutoDodge,
         "AUTO: DODGES",
         "Step outside by itself for a dodge, and back 0.3 s after it ends.",
         showtimeAutoDodgeChoices},
        {Item::showtimeAutoFinish,
         "AUTO: FINISHERS (LOG ONLY)",
         "Not yet: only writes to the log when a finisher scene starts. Off.",
         {}},
        {Item::showtimeDistance,
         "DISTANCE",
         "How far behind him the outside camera sits.",
         showtimeDistanceChoices},
        {Item::showtimeHeight,
         "HEIGHT",
         "How high above his waist it sits. The right stick up and down changes it.",
         showtimeHeightChoices},
        {Item::showtimeAngle,
         "ORBIT ANGLE",
         "Where it starts, off his back: 0 is straight behind, 90 is beside him.",
         showtimeAngleChoices},
        {Item::showtimeTurn,
         "ORBIT TURN",
         "The right stick orbits him: smooth, or in 30 degree steps under a blink.",
         showtimeTurnChoices},
        {Item::showtimeFollow,
         "FOLLOW",
         "How closely the camera keeps up with him. Always smooth.",
         showtimeFollowChoices},
        {Item::showtimeVignette,
         "VIGNETTE",
         "Darkens the edges as the camera moves fast. Never from your head.",
         showtimeVignetteChoices},
        {Item::showtimeSlowMo,
         "SLOW-MO IN SHOWTIME",
         "The left stick click starts slow motion while you are outside. Drains the meter.",
         {}},
        {Item::showtimeLatch,
         "LATCH",
         "DOUBLE PRESS: a second X+B press keeps the camera out. Press again to return.",
         showtimeLatchChoices},
        {Item::handstand,
         "HANDSTAND (EXPERIMENTAL)",
         "Catch a handstand: land inverted, both palms on a floor. Needs REALISTIC FLIPS.",
         {}},
        {Item::handstandAssist,
         "HANDSTAND BALANCE ASSIST",
         "Steadies the balance on your palms as you lean. Off: you balance it yourself.",
         {}},
        {Item::handstandFall, "HANDSTAND FALL ANGLE",
         "Lean past this and you drop to your feet. 50 deg by default.", handstandFallChoices,
         Place::launcher},
        {Item::handstandSupport, "HANDSTAND SUPPORT",
         "How far around your palms you can sway and still hold it. 18 cm by default.",
         handstandSupportChoices, Place::launcher},
        {Item::handstandGrace, "HANDSTAND SWAY TIME",
         "How long you can be off balance before you fall. 500 ms by default.", handstandGraceChoices},
        {Item::handstandDamping, "HANDSTAND DAMPING",
         "How quickly a sway dies down. 250% by default.", handstandDampingChoices, Place::launcher},
        {Item::handstandLeap, "HANDSTAND LEAP",
         "A, a hard stick or a push leaps you out along your view. Off: only a push.",
         {},
         Place::launcher},
        {Item::handstandPushGain, "HANDSTAND PUSH-OFF",
         "How hard a push off the palms launches you, against the palms' speed. 220%.",
         handstandPushGainChoices, Place::launcher},
        {Item::handstandPushMax, "HANDSTAND PUSH-OFF MAX",
         "The fastest a handstand push-off launches you (m/s). 7 by default.", handstandPushMaxChoices,
         Place::launcher},
        {Item::handstandHeight, "HANDSTAND BALANCE HEIGHT",
         "Your centre of mass over the palms, for the balance. 95 cm by default.", handstandHeightChoices,
         Place::launcher},
        {Item::handstandGain, "HANDSTAND BALANCE GAIN",
         "How strongly the balance assist steers (percent). 500% by default.", handstandGainChoices,
         Place::launcher},
        {Item::handstandEyeDrop, "HANDSTAND VIEW DROP",
         "How far your view comes down when you are inverted. 90 cm by default.", handstandEyeDropChoices,
         Place::launcher},
        {Item::airCorrect,
         "AIR UPRIGHT TIME",
         "A in the air rights you smoothly over this long (REALISTIC FLIPS). 1 s default.",
         airCorrectChoices, Place::launcher},
        {Item::pointJumpBullet,
         "POINT JUMP BULLET",
         "A point jump turns your body along your eyes, any angle, like a bullet.",
         {}, Place::launcher},
        {Item::fallHandoff,
         "FALL TO SPIDYVR",
         "A plain fall (no jump) hands to SpidyVR this long into the air. 150 ms.",
         fallHandoffChoices, Place::launcher},
        {Item::airLandingBlend,
         "AIR LANDING BLEND",
         "Body comes upright over the last 0.3 s before landing; the game lands you.",
         {}, Place::launcher},
        {Item::airLandingRelease,
         "AIR LANDING RELEASE (EXPERIMENTAL)",
         "Hands the body to the game 0.2 s before landing. Unverified; needs the blend on.",
         {}, Place::launcher},
        {Item::landingBlend, "LANDING BLEND",
         "A landing: how long the view takes to glide onto the hero's head. 200 ms.", landingBlendChoices},
        {Item::handoffGlideSpeed, "HANDOFF GLIDE SPEED",
         "A long handoff glide is stretched so the view stays under this. Faster: shorter.", handoffGlideSpeedChoices},
        {Item::handoffGlideLongest, "HANDOFF GLIDE LONGEST",
         "The longest HANDOFF GLIDE SPEED may stretch a glide. 450 ms by default.", handoffGlideLongestChoices},
        {Item::handoffPlace, "HANDOFF PLACE",
         "A landing or zip view farther than this from the head is placed, not glided.", handoffPlaceChoices},
        {Item::handoffAngle, "HANDOFF ANGLE TIME",
         "How long the animation head's tilt takes to fade after a switch. 200 ms.", handoffAngleChoices},
        {Item::headSeatGlide, "HEAD SEAT GLIDE",
         "How long the view slides when your head is re-seated on the hero's. 165 ms.", headSeatGlideChoices},
        {Item::landingBodyEase, "LANDING BODY EASE",
         "How long the drawn body takes to ease onto your view after a landing. 500 ms.", landingBodyEaseChoices},
        {Item::directionalFlips, "DIRECTIONAL FLIPS",
         "Standing, hold the left stick a way and tap A: flip that way. Hold A to jump.", {}},
        {Item::airPoses, "MIDAIR POSES",
         "Tuck in a flip, dive looking down, superhero fist up, starfish arms wide.", {}},
        {Item::handGrabClassRules, "HAND GRAB CLASS RULES",
         "Heavies and bosses resist throws unless stunned or webbed up. Off: all alike.", {}, Place::launcher},
        {Item::pointJumpSound, "JUMP SOUNDS",
         "The game's jump grunt on every point jump, ground flip and handstand leap.", {}},
        {Item::wallJumpSound, "WALL JUMP SOUND",
         "Two of the game's sounds to try on a wall jump. Tell us which fits. OFF: none.", wallJumpSoundChoices},
    }};
    static const std::array<Row, rowTotal> all = [] {
        std::array<Row, rowTotal> a{};
        for (size_t r = 0; r < settingRowCount; ++r)
            a[r] = settings[r];
        for (size_t p = 0; p < na_help::pageCount; ++p)
            a[settingRowCount + p] = {Item::helpPage, na_help::pages[p].title, na_help::pages[p].text, helpChoices};
        return a;
    }();
    return all;
}
bool isNativeAnimation(Item item) {
    // The fork's items start at HEAD PROBE; upstream's own that came later (0.8.6's WEB LINES and AIR FISTS,
    // 0.8.7's ENEMY TYPES) sit among them.
    if (item == Item::none || item == Item::webLines || item == Item::airFists || item == Item::enemyTypes)
        return false;
    return static_cast<unsigned>(item) >= static_cast<unsigned>(Item::headProbe) &&
           static_cast<unsigned>(item) <= static_cast<unsigned>(lastItem);
}
std::string helpMarkdown() {
    std::string md =
        "# Native Animation help\n\n"
        "Generated from `include/spidy/na_help.hpp` and the settings catalog (`src/vr_settings.cpp`); in the game "
        "these pages are the NA \xC2\xB7 HELP tab. Do not edit by hand: `spidy_beta_polish_tests` checks it.\n";
    const char* topic = nullptr;
    for (const auto& page : na_help::pages) {
        if (!topic || std::string_view(topic) != page.topic) {
            if (topic)
                md += "\n";
            topic = page.topic;
            md += "\n## ";
            md += topic;
            md += "\n\n";
        } else {
            md += " ";
        }
        md += page.text;
    }
    md += "\n\n## Every NA setting and its default\n";
    for (size_t c = static_cast<size_t>(Category::nativeAnimation); c < categoryCount; ++c) {
        if (static_cast<Category>(c) == Category::help)
            continue;
        const Screen& screen = screens()[c];
        md += "\n### ";
        md += screen.tabTitle;
        md += "\n\n| Setting | Default | Where |\n|---|---|---|\n";
        for (Item item : screen.items) {
            const Row* row = rowFor(item);
            if (!row)
                continue;
            const int def = defaultChoice(item);
            std::string value = row->choices.empty() ? (def ? "ON" : "OFF")
                                : def >= 0 && def < static_cast<int>(row->choices.size()) ? row->choices[def] : "?";
            md += "| ";
            md += row->title;
            md += " | ";
            md += value;
            md += " | ";
            md += row->place == Place::tab ? "game and launcher" : row->place == Place::page ? "pause page" : "launcher";
            md += " |\n";
        }
    }
    return md;
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
        Item::combatVisuals, Item::halfPunchDamage, Item::airFists, Item::enemyTypes, Item::runLogs,
    };
    static constexpr Item flips[] = {
        Item::realisticFlips, Item::flips, Item::controlledFlips, Item::flipSpeed,
        Item::automaticStrength, Item::automaticSpeed, Item::automaticMinimumSpeed, Item::automaticReleaseFlip,
        Item::automaticResponse, Item::automaticHandStrength, Item::automaticFalloff, Item::automaticHandThreshold,
        Item::automaticAirBrake, Item::automaticCombat, Item::dodgeTumble, Item::flipReprojection,
    };
    static constexpr Item feedback[] = {
        Item::haptics, Item::reelHaptics, Item::swingHaptics, Item::bodyHaptics,
        Item::hapticWebs, Item::hapticWind, Item::hapticCombat, Item::hapticMovement,
        Item::blowSounds, Item::pcMedia,
    };
    static constexpr Item display[] = {
        Item::screenSize, Item::screenDepth, Item::hud, Item::steadyHud,
        Item::uiStyle, Item::nearClip, Item::recordingFov, Item::recordingSmoothing,
    };
    // NA · CORE: the mode and its body, then the launcher's deeper rows and the retired ones (hidden in the game,
    // D3; kept in the file so nothing is lost).
    static constexpr Item nativeAnimation[] = {
        Item::nativeAnimation, Item::animatedHead, Item::animatedHeadsetSmooth,
        Item::animatedArms, Item::animatedTorso, Item::animatedModelYaw, Item::animatedHybridSpeed,
        Item::movementMode, Item::jumpHandoff, Item::runSpeed, Item::slowMotionLength, Item::softLock,
        Item::moveHaptics, Item::animatedMoves, Item::animatedHybrid, Item::headProbe, Item::soundTrace,
        Item::animatedTorsoTwist, Item::fallHandoff, Item::airLandingBlend, Item::airLandingRelease,
        Item::viewYawSource, Item::animatedAir, Item::animatedOnly, Item::animatedSteering, Item::animatedAirSpin,
    };
    // CAMERA & STABILIZATION (November 11): how much the animation moves the
    // view (amounts, HEAD ANGLE), then how smoothly (the response times).
    static constexpr Item camera[] = {
        Item::cameraMotion,        Item::cameraPitch,
        Item::cameraRoll,          Item::cameraBobVertical,
        Item::cameraBobSide,       Item::cameraHeadAngle,
        Item::cameraStabilization, Item::punchViewEase,
        Item::airViewEase,         Item::rearSnap, Item::headsetFirst, Item::animHeadRide, Item::shoulderLimit,
        Item::animYawSway,         Item::turnInPlace,
        Item::cameraVertical,      Item::cameraHorizontal,
        Item::cameraAngle,         Item::lightVertical,
        Item::lightHorizontal,     Item::lightAngle,
        Item::heavyVertical,       Item::heavyHorizontal,
        Item::heavyAngle,          Item::steadyVertical,
        Item::steadyHorizontal,    Item::steadyAngle,
        Item::cameraHorizonLevel,  Item::cameraVrStabilization,
    };
    // WEB SHOOTERS (November 11): one choice and its strength, WEB ANGLE on
    // top; the old rows they replace stay in the launcher's file only.
    static constexpr Item shooters[] = {
        Item::shooterMode,   Item::shooterLock,  Item::webAngle,
        Item::webAimSource,  Item::forearmBlend, Item::forearmAimLimit,
        Item::wristShooters, Item::palmShooters, Item::animForearmFallback,
    };
    // NA · BODY & LEGS (LEGS (ADVANCED) since November 11): shown even while unavailable, the help saying why
    // (shownWhenInactive); the flips' NA rows and the handstand with it.
    static constexpr Item legs[] = {
        Item::legControl,      Item::legStiffness,    Item::legDamping,   Item::legCoupling,
        Item::passiveLegs,     Item::passiveCoupling, Item::legAirCrouch, Item::legReachSpread, Item::airPoses,
        Item::directionalFlips, Item::airCorrect,
        Item::handstand, Item::handstandAssist, Item::handstandGrace, Item::handstandFall, Item::handstandSupport,
        Item::handstandDamping, Item::handstandLeap, Item::handstandPushGain,
        Item::handstandPushMax, Item::handstandHeight, Item::handstandGain, Item::handstandEyeDrop,
        Item::wallBrake, Item::gripFling, Item::physicalTethers, Item::tetherParticles, Item::tetherSubsteps,
    };
    // NA · KICKS & COMBAT (beta polish M4): the kicks, the punch and jump assists, the hand grab.
    static constexpr Item kicks[] = {
        Item::kicks, Item::groundKicks, Item::kickGazeCone, Item::swingKickManual, Item::swingKickAuto, Item::kickSpin,
        Item::enemyJoints, Item::enemyLimbZones,
        Item::punchTrackCone, Item::punchTrackRange, Item::jumpPull, Item::punchAutotarget, Item::punchViewSnap,
        Item::jumpPullGaze, Item::jumpPullStick, Item::jumpPullRange, Item::jumpPullCap,
        Item::handGrab, Item::handGrabForce, Item::enemyHoldCost, Item::handGrabClassRules,
    };
    // POINT JUMP (November 11): its switch, trigger and feel.
    static constexpr Item pointJump[] = {
        Item::pointJump, Item::pointJumpTrigger, Item::pointJumpMinSpeed,
        Item::pointJumpMaxSpeed, Item::pointJumpFull, Item::pointJumpPush,
        Item::pointJumpBullet, Item::pointJumpSound, Item::wallJumpSound,
    };
    // SHOWTIME (showtime.hpp; experimental, on by default since revision 14): its switch and its feel.
    static constexpr Item showtime[] = {
        Item::showtime, Item::showtimeWhile, Item::showtimeFade,
        Item::showtimeBody,
        Item::showtimeChord,
        Item::showtimeAutoDodge,
        Item::showtimeAutoFinish,
        Item::showtimeDistance,
        Item::showtimeHeight,
        Item::showtimeAngle,
        Item::showtimeTurn,
        Item::showtimeFollow,
        Item::showtimeVignette,
        Item::showtimeSlowMo,
        Item::showtimeLatch,
    };
    // HANDOFF TIMING (Nov 18, the owner: "give me smaller increments to set"): the handoff and landing glides'
    // times, speed and distance, a tab of their own (NATIVE ANIMATION's holds 20 rows already).
    static constexpr Item handoff[] = {
        Item::landingBlend,
        Item::handoffGlideSpeed,
        Item::handoffGlideLongest,
        Item::handoffPlace,
        Item::handoffAngle,
        Item::headSeatGlide,
        Item::landingBodyEase,
    };
    // NA · HELP (beta polish M5): every page of na_help.hpp (one item, its rows in order).
    static constexpr Item help[] = {Item::helpPage};
    static const std::array<Screen, categoryCount> all{{
        {"Setup", "VR SETUP", setup},
        {"Webs", "VR WEBS", webs},
        {"Swinging", "VR SWINGING", swinging},
        {"Movement", "VR MOVEMENT", movement},
        {"Flips", "VR FLIPS", flips},
        {"Haptics", "VR HAPTICS", feedback},
        {"Display", "VR DISPLAY", display},
        {"Native Animation", SPIDY_NA_TAB_PREFIX "CORE", nativeAnimation},
        {"Camera & Stabilization", SPIDY_NA_TAB_PREFIX "CAMERA", camera},
        {"Handoff Timing", SPIDY_NA_TAB_PREFIX "HANDOFF", handoff},
        {"Body & Legs", SPIDY_NA_TAB_PREFIX "BODY & LEGS", legs},
        {"Kicks & Combat", SPIDY_NA_TAB_PREFIX "KICKS & COMBAT", kicks},
        {"Web Shooters", SPIDY_NA_TAB_PREFIX "WEB SHOOTERS", shooters},
        {"Point Jump", SPIDY_NA_TAB_PREFIX "POINT JUMP", pointJump},
        {"Showtime", SPIDY_NA_TAB_PREFIX "SHOWTIME", showtime},
        {"Help", SPIDY_NA_TAB_PREFIX "HELP", help},
    }};
    return all;
}
const char* inactiveReason(Item item, const Values& v) {
    switch (item) {
    case Item::rearSnap:
        return v.movementMode != 0 ? "Set MOVEMENT MODE to HYBRID to use the rear snap." : nullptr;
    case Item::directionalFlips:
        return !v.realisticFlips && !v.controlledFlips && !v.flips ? "Turn a flip mode on first (the spin is theirs)."
                                                                    : nullptr;
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
    case Item::handstand:
        return !v.realisticFlips ? "Handstands need REALISTIC FLIPS on." : nullptr;
    case Item::handstandAssist:
    case Item::handstandFall:
    case Item::handstandPushGain:
    case Item::handstandPushMax:
    case Item::handstandHeight:
    case Item::handstandGain:
    case Item::handstandEyeDrop:
    case Item::handstandSupport:
    case Item::handstandGrace:
    case Item::handstandDamping:
    case Item::handstandLeap:
        return !v.realisticFlips ? "Handstands need REALISTIC FLIPS on."
               : !v.handstand    ? "Turn HANDSTAND on first."
                                 : nullptr;
    case Item::handGrabForce:
    case Item::enemyHoldCost:
    case Item::handGrabClassRules:
        return !v.handGrab ? "Turn HAND GRAB on first." : nullptr;
    case Item::kickGazeCone:
        return !v.kicks && !v.groundKicks ? "Turn AIR KICKS or GROUND KICKS on first." : nullptr;
    case Item::punchTrackCone:
    case Item::punchTrackRange:
    case Item::punchViewSnap:
        return !v.punchAutotarget ? "Turn PUNCH AUTO-TARGET on first." : nullptr;
    case Item::jumpPullGaze:
    case Item::jumpPullStick:
    case Item::jumpPullRange:
    case Item::jumpPullCap:
        return !v.jumpPull ? "Turn JUMP PULL on first." : nullptr;
    case Item::showtimeWhile:
    case Item::showtimeFade:
    case Item::showtimeDistance:
    case Item::showtimeHeight:
    case Item::showtimeAngle:
    case Item::showtimeTurn:
    case Item::showtimeFollow:
    case Item::showtimeVignette:
    case Item::showtimeSlowMo:
    case Item::showtimeLatch:
    case Item::showtimeChord:
    case Item::showtimeAutoDodge:
    case Item::showtimeAutoFinish:
    case Item::showtimeBody:
        return !v.showtime ? "Turn SHOWTIME on first." : nullptr;
    case Item::kickSpin:
        return !v.kicks ? "Turn AIR KICKS on first." : nullptr;
    case Item::enemyLimbZones:
        return !v.enemyJoints ? "Turn ENEMY JOINTS on first." : nullptr;
    case Item::tetherParticles:
    case Item::tetherSubsteps:
        return !v.physicalTethers ? "Turn WEB TETHERS on first." : nullptr;
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
    case Item::pointJumpBullet:
        return !v.pointJump ? "Turn POINT JUMP on first." : nullptr;
    case Item::airLandingRelease:
        return !v.airLandingBlend ? "Turn AIR LANDING BLEND on first." : nullptr;
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
    case Item::handstand:
    case Item::handstandAssist:
    case Item::handstandFall:
    case Item::handstandPushGain:
    case Item::handstandPushMax:
    case Item::handstandHeight:
    case Item::handstandGain:
    case Item::handstandEyeDrop:
    case Item::handstandSupport:
    case Item::handstandGrace:
    case Item::handstandDamping:
    case Item::handstandLeap:
    case Item::handGrabForce:
    case Item::enemyHoldCost:
    case Item::handGrabClassRules:
    case Item::kickGazeCone:
    case Item::kickSpin:
    case Item::punchTrackCone:
    case Item::punchTrackRange:
    case Item::punchViewSnap:
    case Item::jumpPullGaze:
    case Item::jumpPullStick:
    case Item::jumpPullRange:
    case Item::jumpPullCap:
    case Item::showtimeWhile:
    case Item::showtimeFade:
    case Item::showtimeBody:
    case Item::showtimeChord:
    case Item::showtimeAutoDodge:
    case Item::showtimeAutoFinish:
    case Item::showtimeDistance:
    case Item::showtimeHeight:
    case Item::showtimeAngle:
    case Item::showtimeTurn:
    case Item::showtimeFollow:
    case Item::showtimeVignette:
    case Item::showtimeSlowMo:
    case Item::showtimeLatch:
    case Item::enemyLimbZones:
    case Item::tetherParticles:
    case Item::tetherSubsteps:
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
    case Item::cameraVrStabilization:
        v.cameraVrStabilization = s[index];
        break;
    case Item::cameraHorizonLevel:
        v.cameraHorizonLevelPct = s[index];
        break;
    case Item::handstandFall:
        v.handstandFallDegrees = s[index];
        break;
    case Item::handstandPushGain:
        v.handstandPushGain = s[index];
        break;
    case Item::handstandPushMax:
        v.handstandPushMax = s[index];
        break;
    case Item::handstandHeight:
        v.handstandHeightCm = s[index];
        break;
    case Item::handstandGain:
        v.handstandGainPct = s[index];
        break;
    case Item::handstandSupport:
        v.handstandSupportCm = s[index];
        break;
    case Item::handstandGrace:
        v.handstandGraceMs = s[index];
        break;
    case Item::handstandDamping:
        v.handstandDampingPct = s[index];
        break;
    case Item::handstandEyeDrop:
        v.handstandEyeDropCm = s[index];
        break;
    case Item::handGrabForce:
        v.handGrabForce = s[index];
        break;
    case Item::enemyHoldCost:
        v.enemyHoldCost = s[index];
        break;
    case Item::kickGazeCone:
        v.kickGazeConeDeg = s[index];
        break;
    case Item::punchTrackCone:
        v.punchTrackConeDeg = s[index];
        break;
    case Item::punchTrackRange:
        v.punchTrackRangeM = s[index];
        break;
    case Item::jumpPullGaze:
        v.jumpPullGazeDeg = s[index];
        break;
    case Item::jumpPullStick:
        v.jumpPullStickDeg = s[index];
        break;
    case Item::jumpPullRange:
        v.jumpPullRangeM = s[index];
        break;
    case Item::jumpPullCap:
        v.jumpPullCapDmps = s[index];
        break;
    case Item::showtimeWhile:
        v.showtimeWhile = s[index];
        break;
    case Item::showtimeFade:
        v.showtimeFadeMs = s[index];
        break;
    case Item::wallJumpSound:
        v.wallJumpSound = s[index];
        break;
    case Item::showtimeBody:
        v.showtimeBody = s[index];
        break;
    case Item::showtimeChord:
        v.showtimeChord = s[index];
        break;
    case Item::showtimeAutoDodge:
        v.showtimeAutoDodge = s[index];
        break;
    case Item::showtimeDistance:
        v.showtimeDistanceCm = s[index];
        break;
    case Item::showtimeHeight:
        v.showtimeHeightCm = s[index];
        break;
    case Item::showtimeAngle:
        v.showtimeAngleDeg = s[index];
        break;
    case Item::showtimeTurn:
        v.showtimeTurn = s[index];
        break;
    case Item::showtimeFollow:
        v.showtimeFollow = s[index];
        break;
    case Item::showtimeVignette:
        v.showtimeVignette = s[index];
        break;
    case Item::showtimeLatch:
        v.showtimeLatch = s[index];
        break;
    case Item::airCorrect:
        v.airCorrectMs = s[index];
        break;
    case Item::fallHandoff:
        v.fallHandoffMs = s[index];
        break;
    case Item::landingBlend:
        v.landingBlendMs = s[index];
        break;
    case Item::handoffGlideSpeed:
        v.handoffGlideCmS = s[index];
        break;
    case Item::handoffGlideLongest:
        v.handoffGlideMaxMs = s[index];
        break;
    case Item::handoffPlace:
        v.handoffPlaceCm = s[index];
        break;
    case Item::handoffAngle:
        v.handoffAngleMs = s[index];
        break;
    case Item::headSeatGlide:
        v.headSeatGlideMs = s[index];
        break;
    case Item::landingBodyEase:
        v.landingBodyEaseMs = s[index];
        break;
    case Item::kickSpin:
        v.kickSpinPct = s[index];
        break;
    case Item::tetherParticles:
        v.tetherParticles = s[index];
        break;
    case Item::tetherSubsteps:
        v.tetherSubsteps = s[index];
        break;
    case Item::steadyAngle:
        v.steadyAngleMs = s[index];
        break;
    case Item::steadyHorizontal:
        v.steadyHorizontalMs = s[index];
        break;
    case Item::steadyVertical:
        v.steadyVerticalMs = s[index];
        break;
    case Item::animForearmFallback:
        v.animForearmFallbackDeg = s[index];
        break;
    case Item::animatedTorsoTwist:
        v.animatedTorsoTwist = s[index];
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
    {"enemy_types", &Values::enemyTypes, nullptr},
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
    {"handstand_leap", &Values::handstandLeap, nullptr},
    {"handstand_support_cm", nullptr, &Values::handstandSupportCm},
    {"handstand_grace_ms", nullptr, &Values::handstandGraceMs},
    {"handstand_damping_pct", nullptr, &Values::handstandDampingPct},
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
    {"animated_torso_twist", nullptr, &Values::animatedTorsoTwist},
    {"anim_forearm_fallback_deg", nullptr, &Values::animForearmFallbackDeg},
    {"camera_steady_vertical_ms", nullptr, &Values::steadyVerticalMs},
    {"camera_steady_horizontal_ms", nullptr, &Values::steadyHorizontalMs},
    {"camera_steady_angle_ms", nullptr, &Values::steadyAngleMs},
    {"camera_horizon_level_pct", nullptr, &Values::cameraHorizonLevelPct},
    {"camera_vr_stabilization", nullptr, &Values::cameraVrStabilization},
    {"swing_kick_manual", &Values::swingKickManual, nullptr},
    {"swing_kick_auto", &Values::swingKickAuto, nullptr},
    {"leg_hold_everywhere", &Values::legHoldEverywhere, nullptr},
    {"swing_kick_grunt", &Values::swingKickGrunt, nullptr},
    {"wall_brake", &Values::wallBrake, nullptr},
    {"grip_fling", &Values::gripFling, nullptr},
    {"grip_stop_pct", nullptr, &Values::wallBrakeMuPct}, // was wall_brake_mu_pct (90): a renamed key takes the new default
    {"grip_fling_gain_pct", nullptr, &Values::gripFlingGainPct},
    {"grip_fling_max", nullptr, &Values::gripFlingMax},
    {"hand_grab", &Values::handGrab, nullptr},
    {"hand_grab_force", nullptr, &Values::handGrabForce},
    {"enemy_hold_cost", nullptr, &Values::enemyHoldCost},
    {"hand_grab_class_rules", &Values::handGrabClassRules, nullptr},
    {"kicks", &Values::kicks, nullptr},
    {"kick_gaze_cone_deg", nullptr, &Values::kickGazeConeDeg},
    {"kick_spin_pct", nullptr, &Values::kickSpinPct},
    {"ground_kicks", &Values::groundKicks, nullptr},
    {"enemy_joints", &Values::enemyJoints, nullptr},
    {"enemy_limb_zones", &Values::enemyLimbZones, nullptr},
    {"physical_tethers", &Values::physicalTethers, nullptr},
    {"tether_particles", nullptr, &Values::tetherParticles},
    {"tether_substeps", nullptr, &Values::tetherSubsteps},
    {"leg_air_crouch", &Values::legAirCrouch, nullptr},
    {"leg_reach_spread", &Values::legReachSpread, nullptr},
    {"air_poses", &Values::airPoses, nullptr},
    {"directional_flips", &Values::directionalFlips, nullptr},
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
    {"headset_first", &Values::headsetFirst, nullptr},
    // PUNCH AUTO-TARGET (punch_track.hpp; polish pass, slice 7).
    {"punch_autotarget", &Values::punchAutotarget, nullptr},
    {"punch_track_cone_deg", nullptr, &Values::punchTrackConeDeg},
    {"punch_track_range_m", nullptr, &Values::punchTrackRangeM},
    {"punch_view_snap", &Values::punchViewSnap, nullptr},
    // JUMP PULL (jump_pull.hpp; polish pass, slice 8).
    {"jump_pull", &Values::jumpPull, nullptr},
    {"jump_pull_gaze_deg", nullptr, &Values::jumpPullGazeDeg},
    {"jump_pull_stick_deg", nullptr, &Values::jumpPullStickDeg},
    {"jump_pull_range_m", nullptr, &Values::jumpPullRangeM},
    {"jump_pull_cap_dmps", nullptr, &Values::jumpPullCapDmps},
    {"anim_head_ride", &Values::animHeadRide, nullptr},
    {"shoulder_limit", &Values::shoulderLimit, nullptr},
    {"snap_realign", &Values::snapRealign, nullptr},
    {"showtime", &Values::showtime, nullptr},
    {"showtime_while", nullptr, &Values::showtimeWhile},
    {"showtime_fade_ms", nullptr, &Values::showtimeFadeMs},
    {"showtime_body", nullptr, &Values::showtimeBody},
    {"showtime_chord", nullptr, &Values::showtimeChord},
    {"showtime_auto_dodge", nullptr, &Values::showtimeAutoDodge},
    {"showtime_auto_finish", &Values::showtimeAutoFinish, nullptr},
    {"showtime_distance_cm", nullptr, &Values::showtimeDistanceCm},
    {"showtime_height_cm", nullptr, &Values::showtimeHeightCm},
    {"showtime_angle_deg", nullptr, &Values::showtimeAngleDeg},
    {"showtime_turn", nullptr, &Values::showtimeTurn},
    {"showtime_follow", nullptr, &Values::showtimeFollow},
    {"showtime_vignette", nullptr, &Values::showtimeVignette},
    {"showtime_slow_mo", &Values::showtimeSlowMo, nullptr},
    {"showtime_latch", nullptr, &Values::showtimeLatch},
    {"air_correct_ms", nullptr, &Values::airCorrectMs},
    {"point_jump_bullet", &Values::pointJumpBullet, nullptr},
    {"point_jump_sound", &Values::pointJumpSound, nullptr},
    {"wall_jump_sound", nullptr, &Values::wallJumpSound},
    {"attack_sound", nullptr, &Values::attackSound},
    {"fall_handoff_ms", nullptr, &Values::fallHandoffMs},
    {"air_landing_blend", &Values::airLandingBlend, nullptr},
    {"air_landing_release", &Values::airLandingRelease, nullptr},
    {"handstand_catch_hold_cm", nullptr, &Values::handstandCatchHoldCm},
    {"handstand_reach_cm", nullptr, &Values::handstandReachCm},
    {"takeover_view_ease", &Values::takeoverViewEase, nullptr},
    {"head_height_fade_ms", nullptr, &Values::headHeightFadeMs},
    {"handoff_one_blend", &Values::handoffOneBlend, nullptr},
    {"punch_camera_lock_ms", nullptr, &Values::punchCameraLockMs},
    {"view_relatch_fix", &Values::viewRelatchFix, nullptr},
    {"snap_body_turn", &Values::snapBodyTurn, nullptr},
    {"snap_tap_ms", nullptr, &Values::snapTapMs},
    {"dodge_reface", &Values::dodgeReface, nullptr},
    {"landing_body_to_view", &Values::landingBodyToView, nullptr},
    {"landing_body_ease_ms", nullptr, &Values::landingBodyEaseMs},
    {"landing_rear_turn", &Values::landingRearTurn, nullptr},
    {"handoff_ease_curve", nullptr, &Values::handoffEaseCurve},
    {"handoff_glide_cm_s", nullptr, &Values::handoffGlideCmS},
    {"handoff_glide_max_ms", nullptr, &Values::handoffGlideMaxMs},
    {"handoff_head_leads", &Values::handoffHeadLeads, nullptr},
    {"handoff_angle_ms", nullptr, &Values::handoffAngleMs},
    {"landing_blend_ms", nullptr, &Values::landingBlendMs},
    {"head_seat_glide_ms", nullptr, &Values::headSeatGlideMs},
    {"handstand_arms_over_head_cm", nullptr, &Values::handstandArmsOverHeadCm},
    {"turn_push_pct", nullptr, &Values::turnPushPct},
    {"landing_body_hold_turn", &Values::landingBodyHoldTurn, nullptr},
    {"web_zip_instant", &Values::webZipInstant, nullptr},
    {"point_jump_fire_full", &Values::pointJumpFireFull, nullptr},
    {"target_scan_idle_ms", nullptr, &Values::targetScanIdleMs},
    {"handoff_place_cm", nullptr, &Values::handoffPlaceCm},
    {"punch_track_by_look", &Values::punchTrackByLook, nullptr},
    {"handstand_grace_before_ms", nullptr, &Values::handstandGraceBeforeMs},
    {"handstand_grace_after_ms", nullptr, &Values::handstandGraceAfterMs},
    {"handstand_tip_in", &Values::handstandTipIn, nullptr},
    {"handstand_tip_in_deg", nullptr, &Values::handstandTipInDeg},
    {"handstand_tip_in_reach_cm", nullptr, &Values::handstandTipInReachCm},
    {"marker_twist_steady", &Values::markerTwistSteady, nullptr},
    {"marker_turn_rate_dps", nullptr, &Values::markerTurnRateDps},
    {"marker_switch_hold_ms", nullptr, &Values::markerSwitchHoldMs},
    {"log_level", nullptr, &Values::logLevel},
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
    if (seen < 2 && v.steadyVerticalMs == 350 && v.steadyHorizontalMs == 300 && v.steadyAngleMs == 450) {
        // Revision 2 (polish pass): the STEADY bank answers the head sooner. Saved at the old defaults is
        // untuned; a value the player changed stays.
        const Values fresh;
        v.steadyVerticalMs = fresh.steadyVerticalMs;
        v.steadyHorizontalMs = fresh.steadyHorizontalMs;
        v.steadyAngleMs = fresh.steadyAngleMs;
    }
    if (seen < 3) {
        // Revision 3 (the handstand balance pass): a profile still on the old fall angle (35) and assist gain (300%)
        // is untuned and gets the new ones; a value the player changed stays.
        const Values fresh;
        if (v.handstandFallDegrees == 35)
            v.handstandFallDegrees = fresh.handstandFallDegrees;
        if (v.handstandGainPct == 300)
            v.handstandGainPct = fresh.handstandGainPct;
    }
    if (seen < 4) {
        // Revision 4 (the owner, Nov 16): the point jump's press buffer 150 -> 200 ms (s165800: 20 of 62 refusals
        // were the buffer running out). Still at the old default is untuned; a value the player changed stays.
        const Values fresh;
        if (v.pointJumpBufferMs == 150)
            v.pointJumpBufferMs = fresh.pointJumpBufferMs;
    }
    if (seen < 5) {
        // Revision 5 (the owner, Nov 16): PUNCH TRACK CONE 45 -> 60 degrees and RANGE 6 -> 8 m (s165800: 136 of 176
        // punches found no target, 99 outside the cone, 37 too far). Still at the old defaults is untuned.
        const Values fresh;
        if (v.punchTrackConeDeg == 45)
            v.punchTrackConeDeg = fresh.punchTrackConeDeg;
        if (v.punchTrackRangeM == 6)
            v.punchTrackRangeM = fresh.punchTrackRangeM;
    }
    if (seen < 6) {
        // Revision 6 (the owner, Nov 17): PUNCH TRACK CONE 60 -> 90 degrees and RANGE 8 -> 10 m (s131456: 29 of 49
        // throws outside 60 degrees, 7 too far). Still at the old defaults is untuned; a tuned value stays.
        const Values fresh;
        if (v.punchTrackConeDeg == 60)
            v.punchTrackConeDeg = fresh.punchTrackConeDeg;
        if (v.punchTrackRangeM == 8)
            v.punchTrackRangeM = fresh.punchTrackRangeM;
        // HANDSTAND REACH 130 -> 160 cm (the owner, Nov 17: "loosen up handstands"; the attempts read 1.15-1.41 m).
        if (v.handstandReachCm == 130)
            v.handstandReachCm = fresh.handstandReachCm;
    }
    if (seen < 7) {
        // Revision 7 (the owner, Nov 18): SNAP TAP WINDOW 120 -> 250 ms (s131456: 67 right-stick flicks of 167-375 ms
        // were taken as the smooth turn). Still at the old default is untuned; a tuned value stays.
        const Values fresh;
        if (v.snapTapMs == 120)
            v.snapTapMs = fresh.snapTapMs;
    }
    if (seen < 8) {
        // Revision 8 (the owner, Nov 18: "Change the handoff defaults"): the handoff tune. s131456: every takeover's
        // ease ended with the 500 ms height fade (493 ms on average), and 43 of 85 landing glides ran to the 700 ms cap.
        // Each value still at its old default is untuned and moves; a tuned value stays.
        const Values fresh;
        const auto move = [](int& value, int old, int now) {
            if (value == old)
                value = now;
        };
        move(v.headHeightFadeMs, 500, fresh.headHeightFadeMs);
        move(v.handoffGlideCmS, 200, fresh.handoffGlideCmS);
        move(v.handoffGlideMaxMs, 700, fresh.handoffGlideMaxMs);
        move(v.handoffPlaceCm, 200, fresh.handoffPlaceCm);
        move(v.landingBlendMs, 250, fresh.landingBlendMs);
        move(v.handoffAngleMs, 300, fresh.handoffAngleMs);
        move(v.airViewEaseMs, 250, fresh.airViewEaseMs);
        move(v.headSeatGlideMs, 250, fresh.headSeatGlideMs);
        move(v.jumpHandoffMs, 300, fresh.jumpHandoffMs);
    }
    if (seen < 9) {
        // Revision 9 (the owner, Nov 18, t119u: "All of these are good, let's do them all"): GROUND KICKS on. An ini
        // from before wrote ground_kicks=0 as the old default, so it is untuned and moves.
        v.groundKicks = true;
    }
    if (seen < 10) {
        // Revision 10 (the owner, Nov 18 1:53 PM, the 2 AM log's fixes): KICK GAZE CONE 25 -> 40 degrees (all 25 kicks
        // of s020952 started with target=0: no enemy inside the cone). Still at the old default is untuned.
        const Values fresh;
        if (v.kickGazeConeDeg == 25)
            v.kickGazeConeDeg = fresh.kickGazeConeDeg;
    }
    if (seen < 11) {
        // Revision 11 (the owner, Nov 18 1:53 PM, the 2 AM log's fixes): PUNCH TRACK CONE 90 -> 120 degrees (s020952:
        // 41 throws just outside 90, inside 120). Still at the old default is untuned.
        const Values fresh;
        if (v.punchTrackConeDeg == 90)
            v.punchTrackConeDeg = fresh.punchTrackConeDeg;
    }
    if (seen < 12) {
        // Revision 12 (the owner, Nov 19 6:06 AM: "Yes, manual swing kicks on by default"): MANUAL SWING KICK on. An ini
        // from before wrote swing_kick_manual=0 as the old default, so it is untuned and moves. LEG HOLD EVERYWHERE stays
        // off ("No, keep X and Y instant on the ground").
        v.swingKickManual = true;
    }
    if (seen < 13) {
        // Revision 13 (beta polish, the owner's D7): SOUND TRACE off for the beta. An ini from before wrote
        // sound_trace=1 as the old default, so it is untuned and moves; sound_trace=1 after this brings the logging back.
        v.soundTrace = false;
    }
    if (seen < 14) {
        // Revision 14 (the owner, Nov 19: "leave all features on for further testing"): features that were on
        // in the owner's 0.8.7-na-beta1 session and off by default come on once. An ini from before wrote them
        // as 0 (the old default), so that is untuned and moves; a 0 chosen after this revision stays.
        // Diagnostic logging (sound_trace, head_probe, run_logs) stays off. Wall jump sound is unchanged.
        v.showtime = true;
        v.showtimeAutoFinish = true;
        if (v.showtimeAutoDodge == 0)
            v.showtimeAutoDodge = 1; // PERFECT ONLY, as in the session
        v.handGrab = true;
        v.kicks = true;
        v.physicalTethers = true;
        v.wallBrake = true;
        v.gripFling = true;
        v.airLandingRelease = true;
        v.pointJump = true;
    }
    v.movementDefaults = movementDefaultsRevision;
    return sanitized(v);
}
} // namespace spidy::vr_settings
