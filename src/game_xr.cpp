#include "spidy/animation_cancel.hpp"
// Bounded first-person OpenXR integration using the game's two native views.
#include "spidy/native_appearance.hpp"
#include "spidy/native_body.hpp"
#include "spidy/wall_crawl_body.hpp"
#include "spidy/wall_run_body.hpp"
// The input bridge remains a separately verified, separately stoppable module.
#include "spidy/bhaptics_output.hpp"
#include "spidy/body_calibration.hpp"
#include "spidy/body_haptics.hpp"
#include "spidy/crawl_hands.hpp"
#include "spidy/crawl_stick.hpp"
#include "spidy/eye_resolution.hpp"
#include "spidy/monitor_rate.hpp"
#include "spidy/eye_snapshot.hpp"
#include "spidy/game_bridge_protocol.hpp"
#include "spidy/dodge_tumble.hpp"
#include "spidy/game_dodge.hpp"
#include "spidy/point_jump.hpp"
#include "spidy/game_fp.hpp"
#include "spidy/web_aim.hpp"
#include "spidy/game_grab.hpp"
#include "spidy/game_lines.hpp"
#include "spidy/game_menu.hpp"
#include "spidy/game_offers.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/game_prompts.hpp"
#include "spidy/game_player.hpp"
#include "spidy/game_punch.hpp"
#include "spidy/game_sense.hpp"
#include "spidy/gadget_wheel.hpp"
#include "spidy/phone.hpp"
#include "spidy/reel_haptics.hpp"
#include "spidy/swing_haptics.hpp"
#include "spidy/phone_music.hpp"
#include "spidy/phone_pc_media.hpp"
#include "spidy/game_shooter.hpp"
#include "spidy/game_sound.hpp"
#include "spidy/game_steam.hpp"
#include "spidy/game_swing.hpp"
#include "spidy/swing_coupling.hpp"
#include "spidy/game_time.hpp"
#include "spidy/game_tracking.hpp"
#include "spidy/feedback_frame.hpp"
#include "spidy/game_warp.hpp"
#include "spidy/hand_pose.hpp"
#include "spidy/headset_smoothing.hpp"
#include "spidy/camera_motion.hpp"
#include "spidy/shooter_lock.hpp"
#include "spidy/spherical_aim.hpp"
#include "spidy/leg_control.hpp"
#include "spidy/leg_hold.hpp"
#include "spidy/aim_filter.hpp"
#include "spidy/arm_frame.hpp"
#include "spidy/kinematics.hpp"
#include "spidy/impact_sound.hpp"
#include "spidy/native_eye_frame.hpp"
#include "spidy/native_eye_gpu.hpp"
#include "spidy/native_eye_history.hpp"
#include "spidy/native_hud.hpp"
#include "spidy/native_webs.hpp"
#include "spidy/near_clip.hpp"
#include "spidy/offer_pointing.hpp"
#include "spidy/presentation_gate.hpp"
#include "spidy/recording_fov.hpp"
#include "spidy/run_update.hpp"
#include "spidy/diag_log.hpp"
#include "spidy/ground_state.hpp"
#include "spidy/head_probe.hpp"
#include "spidy/head_seat.hpp"
#include "spidy/animated_moves.hpp"
#include "spidy/animated_steering.hpp"
#include "spidy/animated_model_yaw.hpp"
#include "spidy/animated_air_spin.hpp"
#include "spidy/hybrid_owner.hpp"
#include "spidy/hybrid_handoff.hpp"
#include "spidy/hybrid_steer.hpp"
#include "spidy/move_haptics.hpp"
#include "spidy/sound_trace.hpp"
#include "spidy/stable_yaw.hpp"
#include "spidy/view_yaw_lock.hpp"
#include "spidy/leg_layers.hpp"
#include "spidy/facing_check.hpp"
#include "spidy/hybrid_movement.hpp"
#include "spidy/animated_arms.hpp"
#include "spidy/frame_stats.hpp"
#include "spidy/seated.hpp"
#include "spidy/slow_motion.hpp"
#include "spidy/steady_hud.hpp"
#include "spidy/throw_aim.hpp"
#include "spidy/vr_settings.hpp"
#include "spidy/web_pull.hpp"
#include "spidy/xr_layers.hpp"
#include "spidy/xr_session.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>
#include <shlobj.h>
using namespace spidy;
struct XrConfig {
    uint32_t magic = 0x53585243, version = 24, bytes = sizeof(XrConfig), pid{};
    // record and mover are no longer used: VR starts with the game, before
    // there is a player, and finds each new player itself (game_player).
    uint64_t base{}, queue{}, bridgeModule{}, rayModule{}, motionModule{}, record{}, mover{};
    uint32_t durationMs = 20000, eyeSize = defaultEyeSize;
    float swingSpeed = 32;
    // bit 0: CPU eye capture; bit 1: Spidy's overlay webs instead of the game's;
    // bit 2: keep the stock camera as the game's active (monitor) view in VR;
    // bit 3: no web grab (webs never catch props or thugs);
    // bit 4: no eye occlusion (each eye draws everything in its view, hidden or not);
    // bit 5: no body (the hero stays hidden in VR, the overlay draws gloves);
    // bit 6: no punching (fists pass through thugs);
    // bit 7: aim markers start hidden (the SPIDY VR tab shows them);
    // bit 8: no webs in open air (a web that meets nothing within reach misses);
    // bit 9: no web shooter (a free hand's trigger shoots nothing);
    // bit 10: no T-pose calibration at the first gameplay of a session without
    // one (the player skipped it before; the SPIDY VR tab still offers it);
    // bit 11: flips, experimental (A and the left stick in the air flip the player; FlipMotion);
    // bit 12: the trigger webs and the grip reels (WEB BUTTON: TRIGGER);
    // bit 13: the view stays upright on walls (STAND ON WALLS: OFF);
    // bit 14: walls are the game's (its wall crawl takes a player who flies
    // into one, as before the swing's own walls);
    // bit 15: no anti-aliasing for the eyes (each eye is its scene as rendered,
    // at the eye's size, as before October 10's eighth build: upscale is not used);
    // bit 16: the game renders its own view on the head in immersive VR too,
    // for the monitor (as before October 10's eighth build; without it the monitor shows the
    // left eye);
    // bit 17: each eye makes its own set of the key light's shadow cascades,
    // as before October 11 (without it both use one set, fitted to a lens
    // covering both: SpidyEyeShadows);
    // bit 18: no targets for screen-space reflections in the eyes;
    // bit 19: the eyes keep the distances their own wide lenses give
    // (--eye-lens-distances; until October 15 it was webs as before October
    // 12: PHYSICAL WEBS off);
    // bit 20: the game's own button prompts are left alone (neither read
    // for the button they ask for nor named with the VR control, and the
    // interact button is the game's Y as before; the pad layer stays);
    // bit 21: arcade webs (WEB STYLE: ARCADE; vr_settings::Values::arcadeWebs);
    // bit 22: first-person cutscenes (cutscenes and story moments play in 3D around the player, from
    // inside the hero's head, instead of on the game screen: game_fp.hpp, game_fp.cpp);
    // bit 23: with bit 22, the scene's own camera aims the view, not the player's head;
    // bit 24: no theatre screen: every scene the game plays is shown round the player, from the
    // scene's own camera without bit 22 (game_fp::sceneCamera)
    uint32_t options{};
    // OpenXR runtime manifest the launcher chose; empty: Virtual Desktop's if
    // installed, else Windows' active runtime.
    wchar_t runtime[260]{};
    // The rest of the VR settings a session starts from (options bits 3, 5-9
    // and 11-13, swingSpeed, weight, hud and flipSpeed give the others):
    // degrees per snap turn (0: none), controller vibration in percent, the
    // game screen's size (0-2), degrees a second of smooth turning (0: the
    // stick snap turns).
    uint32_t snapTurn = 30, haptics = 100, screenSize = 1, smoothTurn{};
    // Eye resolution in percent of the runtime's recommendation, per side
    // (eye_resolution.hpp; with eyeSize set, 100), and the player's weight
    // while webs fly them, in percent of real gravity (40-300).
    uint32_t renderScale = 100, weight = 100;
    // The player's T-pose calibration (body_calibration.hpp): the eye height
    // and arm length their body is sized to, millimetres. Both 0: none yet.
    uint32_t eyeHeightMm{}, armLengthMm{};
    // The game's HUD in the headset: 0 off, 1 small, 2 medium, 3 large
    // (native_hud.hpp); how fast a flip turns the player at full tilt of the
    // left stick, degrees a second (90-480; FlipMotion::speed).
    uint32_t hud = 2, flipSpeed = 180;
    // Percent of the eye's size, per side, the eyes' scene is rendered at
    // (50-100; eye_resolution.hpp): the engine's temporal anti-aliasing brings
    // it to the eye's size. And how often the game's window gets the left eye
    // while the game renders no view of its own: presents a second (1-240), 0
    // every frame (monitor_rate.hpp).
    uint32_t upscale = defaultEyeUpscale, monitorRate = defaultMonitorRate;
    // Version 23: how much depth scenes have on the game screen: 0 none (the
    // game's flat frame), 1 low, 2 medium, 3 high (vr_settings::screenDepths).
    // Version 24, in version 23's spare word: how strongly a bHaptics vest,
    // sleeves and visor play (BODY HAPTICS, percent; 0 off: body_haptics.hpp).
    uint32_t screenDepth = 2, bodyHaptics = 100;
};
static_assert(sizeof(XrConfig) == 664);
// Why the last frame had no gameplay (XrData::gate bits).
enum GateReason : uint32_t {
    gateNoPlayer = 1,     // no save loaded, or a level change in progress
    gateBridge = 2,       // the input bridge is not running
    gateNoCommit = 4,     // no camera commit for 100 ms: paused, loading, a cutscene
    gateOtherCamera = 8,  // the camera is not the player's follow or combat camera
    gateTracking = 16,    // the headset's pose or timing was not usable
};
struct XrData {
    uint32_t magic = 0x53585244, version = 43, bytes = sizeof(XrData), status{};
    int64_t sequence{};
    uint64_t frames{}, tracked{}, submitted{}, dropped{}, leftHands{}, rightHands{}, serial{}, generation{};
    uint32_t nativeKeys{}, error{};
    float head[16]{}, hands[2][16]{};
    char message[256]{};
    uint64_t uniqueSubmitted{}, reusedSubmitted{};
    uint32_t flatScreen{}, toggles{}, eyeWidth{}, eyeHeight{};
    // What the last frame showed (Presentation); B presses that reached the
    // game as its B (the dodge; 0 from builds before it, where this was
    // reserved); and frames that showed the game's presented frame on the
    // screen because gameplay was unavailable.
    uint32_t presentation{}, dodges{};
    uint64_t screenSubmitted{};
    // GateReason bits of the last frame, and the vtable (image offset) of the
    // camera mover that last committed: which camera a scene without VR used.
    uint32_t gate{}, cameraMover{};
    // Players found so far (each loaded save, respawn or character switch),
    // and the current one's actor record.
    uint64_t players{}, playerRecord{};
    // Xbox buttons the VR controllers hold for the game, and the game's reads
    // of that virtual controller.
    uint32_t padButtons{}, padInstalled{};
    uint64_t padReads{};
    // The input bridge's camera commits, and those on the player by an
    // accepted camera. Both rising while the gate says other_camera: two
    // cameras commit each frame and the other one commits last.
    uint64_t cameraCommits{}, playerCommits{};
    // Y presses that reached the game as its Y (interact, web strike; B
    // presses until October 11); whether the aim markers show now; markers
    // drawn so far, one per hand per headset frame that shows one.
    uint32_t interacts{}, aimMarkers{};
    uint64_t markers{};
    // The VR settings now: the SPIDY VR tab in the game's Settings changes
    // them during play. settings bits: 1 web grab,
    // 2 punch, 4 body, 8 webs in open air, 16 web shooter, 32 flips, 64 the
    // trigger webs (WEB BUTTON: TRIGGER; the grip reels), 128 STAND ON WALLS,
    // 256 PHYSICAL WEBS (until October 15), 512 arcade webs (WEB STYLE:
    // ARCADE).
    // Then the changes
    // made in that tab so far, the times the game built its Settings with it,
    // whether its hooks are in, and why it is missing (game_menu: 93xx-94xx
    // not hooked, 95xx not built).
    uint32_t settings{}, snapTurn{}, haptics{}, screenSize{};
    float swingSpeed{};
    uint32_t settingChanges{};
    uint64_t menuTabs{};
    uint32_t menuInstalled{}, menuStatus{};
    // Degrees a second of smooth turning now (0: the stick snap turns).
    uint32_t smoothTurn{};
    // Walls and ceilings the game held the player on (GameTrackingRig): the
    // stretches and frames of play on one, the player's up (its actor's)
    // now, the head's stand-off now (m), and on the last frame on one the
    // head's height over it from the feet, before and after the stand-off.
    uint32_t surfaceEntries{};
    uint64_t surfaceFrames{};
    float heroUp[3]{};
    float standOff{}, surfaceHeight{}, surfaceClearance{};
    // The weight now, percent of real gravity (the swing's gravity while webs
    // fly the player), and the HUD row now (0 off, 1-3 small to large).
    uint32_t weight{}, hud{};
    // The T-pose calibration (body_calibration.hpp): its phase now and what its
    // panel asks for (Phase, Hint), how far the hold is (0-1); the eye height
    // and arm length the body is sized to (mm; 0: the headset's running height
    // and the hero's own arms); calibrations finished and skipped this session;
    // calibrationFlags bit 0: a session without one asks for none at its first
    // gameplay (skipped); each arm's reach at the last one (m, 0 the left).
    uint32_t calibrationPhase{}, calibrationHint{};
    float calibrationProgress{};
    uint32_t eyeHeightMm{}, armLengthMm{}, calibrations{}, calibrationSkips{}, calibrationFlags{};
    float calibrationReach[2]{};
    // The flip speed now, degrees a second at full tilt (FLIP SPEED), and how
    // far the view leans from the world's up now, degrees: 0 level, 90 while
    // the player stands on a wall (GameMotionFrame::viewTilt).
    uint32_t flipSpeed{};
    float viewTilt{};
    // Version 21: the walks a held Y began (game_pad::InteractStep). Version
    // 22: the GAME SCREEN 3D row now (0 off, 1-3 low to high), and the frames
    // that showed the game screen in stereo (a scene's own two eye images;
    // screenSubmitted counts them too).
    uint32_t interactSteps{}, screenDepth{};
    uint64_t stereoSubmitted{};
    // Version 23: pushes of the right stick that reached the game as its
    // D-pad down (heal) and as its LT + RT (the zip to a point).
    uint32_t heals{}, zips{};
    // Version 24: the game's own camera against where the head looks
    // (game_pad::CameraLook): the angle between them now, degrees (the head to
    // the camera's right: more than 0; 0 while the camera is not steered), and
    // the times the right stick began to turn the camera.
    float lookError{};
    uint32_t lookTurns{};
    // Version 25: the scans a trigger pulled at the temple began (the game's
    // R3: game_pad::TemplePress), and the Charge Jumps a held A began (its
    // RT + A: game_pad::ChargeJump).
    uint32_t scans{}, chargeJumps{};
    // Version 26: the layout the runtime uses for each hand's controller now
    // (ControllerKind, controller_layout.hpp; left, right: 0 none yet,
    // 1 Touch, 2 Index, 3 Steam Frame, 4 one of the runtime's own), the
    // layouts the runtime took Spidy's controls for (bit 1 << ControllerKind)
    // and a spare word. A Steam Frame whose hands say Touch plays SteamVR's
    // translation of that layout: its B, X and Y are all B. Version 38 in
    // that spare word: the zips sent as a hold of the stick was let go in the
    // game's aim mode (game_pad::ZipPress; `zips` counts them too).
    uint32_t controllers[2]{}, controllerLayouts{}, zipReleases{};
    // Version 27: the game's own prompts, read through game_menu's hooks
    // (game_prompts.hpp). Its calls into Flash and the prompt and QTE
    // interface's among them (`promptCalls` at 0 with `promptInvokes` rising
    // says the names no longer match the game), prompts raised and taken
    // away, texts that named a button and texts rewritten for a VR player;
    // what the game is asking for now and the last thing it asked for
    // (prompt_buttons::pack), the asks VR play could not have sent on its own
    // (what the interact button now carries) and texts that came with the
    // keyboard's icons, which name no pad button at all.
    uint64_t promptInvokes{}, promptCalls{}, promptsRaised{}, promptsCleared{};
    uint64_t promptTexts{}, promptRewrites{};
    uint32_t promptDemand{}, promptLastDemand{}, promptBeyondPlay{}, promptKeyboardTexts{};
    // Presses that went to a button the game named, from the VR control of
    // that name or a thrown fist (game_pad::NamedPress, since October 20;
    // before it the interact button sending a prompt's demand, which never
    // ran with the reading off), the times the pad layer was latched on and
    // whether it is on now (game_pad::PadLayer), and a spare word.
    // Then the presses of Y that went out while the player stood on a wall,
    // which `playing` withheld until October 13 (the closed vent).
    uint32_t contextPresses{}, padEntries{}, padLayerNow{}, wallInteracts{};
    // Version 28: the VR controllers' own buttons as the runtime gives them
    // (ControllerButton bits: 1 A, 2 B, 4 X, 8 Y, 16 menu, 32 the Steam
    // Frame's View, 64-512 its D-pad) - held on the last frame, and every bit
    // that went down at all in VR play. A report then says what the player
    // pressed as well as what reached the game, which is what two October 13
    // sessions could not tell apart: the game's Y never reached it in either
    // (0 of 4230 and 0 of 3432 samples) while its B, from the other hand,
    // reached it 30 times, and nothing said whether Y had been pressed.
    uint32_t vrButtons{}, vrButtonsPlayed{};
    // Version 29: presses of the interact that handed the player back to the
    // game's own wall crawl, because the swing held him on a wall of its own
    // and the game was offering nothing to press (game_pad::WallHandover).
    // Then those of them the game answered by offering an interact again,
    // which is the hand-over having worked: the two apart say whether it is
    // worth its moment.
    uint32_t wallHandovers{}, wallHandoversOffered{};
    // Version 37 in version 30's slots (the room around the player, which
    // decided nothing since October 15): the eyes held out of what the world
    // is built of (HeadClearance) - frames they were moved more than a
    // centimetre, and the most they ever were, millimetres.
    uint32_t headPushFrames{}, headPushMostMm{};
    // Version 31: the game's own wall crawl, now that the view turns onto it
    // as it does onto a wall of the swing's (SurfaceHold::crawl). Frames the
    // crawl held the player, and of those the frames the view had turned.
    //
    // Then what the game did with the stick there (CrawlStick): the mean turn
    // from the direction sent to the direction taken, in the surface's own
    // axes, and how alike those turns were - near 1 a frame fixed against the
    // surface, which the stick wants rotating by that turn; near 0 no fixed
    // frame at all, and the mover wants driving instead, as the swing drives
    // it on a wall of its own. Then the mean speed taken. The crawl is the
    // only state the game calls supported on a wall, so it is the only one
    // whose interacts work there: what it does with the stick is the last
    // thing in the way of a vent that behaves like the ground.
    uint32_t crawlFrames{}, crawlStood{}, crawlStickSamples{};
    float crawlStickTurn{}, crawlStickAgreement{}, crawlStickSpeed{};
    // Frames the head was brought down to keep it under the surface over the
    // player, and the least room there ever was over his feet (metres; zero
    // where no ray ever found one). Inside the vent the head stood at the
    // player's own eye height over its floor and so out through its roof.
    uint32_t headDroppedFrames{};
    float headroomLeast{};
    // Version 32: handing the player from the swing's own wall to the game's,
    // indoors (WallToGame). Tries, those the game answered by taking him -
    // which is a vent that works - those it did not and the swing took the
    // wall straight back, and the frames he was being driven into the wall.
    // The three apart say whether the game catches a player let go of at a
    // wall, which nothing could rig headlessly.
    uint32_t wallToGameTries{}, wallToGameHanded{}, wallToGameRecovered{}, wallToGamePushFrames{};
    // Version 33: the game's own crawl under the player at rest on the
    // swing's wall (game_warp): crawls asked for, those the game's state
    // machine took, refused, faulted; how the hand-overs ended - the crawl
    // took him, or the swing had him back (the game did not begin or finish,
    // or the body dropped). Then frames of the game's own wall run (its RT
    // with the stick up the wall, `climbing`) and frames the view held its
    // surface through a gap in the swing's reports (`holdGrace`).
    uint32_t crawlAsked{}, crawlApplied{}, crawlRefused{}, crawlFaulted{};
    uint32_t crawlTaken{}, crawlDeclined{}, gameRunFrames{}, holdGraceFrames{};
    // Version 34: frames of play the rig could not place (the gate's
    // `tracking`), and every reason seen for them (GameTrackingRig::Inactive
    // bits); one shorter than blipSeconds keeps the view standing where it
    // stood. Then frames the game crawled the player on a floor (a vent, a
    // duct: game_swing::Surface::floor), of those the frames the head was
    // held at the crawl's own height (crawlRoom), and frames the body knelt
    // under a held head. Then the game camera's look and up at the latest
    // frame: the frame the game's crawl takes its stick in.
    uint32_t trackingGapFrames{}, trackingGapReasons{}, floorCrawlFrames{}, crawlCapFrames{};
    // Version 35 in the slot after: the game_offers hooks (game_offers::Data
    // status - 0 off, 1 hooked, 3 failed - and its error above the low byte).
    uint32_t kneelFrames{}, offersState{};
    float cameraForward[3]{}, cameraUp[3]{};
    // Version 35: the game's interactions on Spidy's wall (game_offers,
    // offer_pointing): frames a hand pointed at one the game would let the
    // player use, and at one he had to come closer to; presses of the
    // interact that asked for a use, those the game refused at once, those
    // that came to the game's interact and of those the ones the game had
    // picked the offer for first, and those given up (its crawl did not take
    // him); the most offers the game gathered near him at once.
    uint32_t offerPointFrames{}, offerReachFrames{}, offerAsks{}, offerRefusals{};
    uint32_t offerPresses{}, offerPicked{}, offerFailures{}, offersSeen{};
    // Version 36: game_offers' own counts at the latest frame - the player's
    // offer updates run with the crawl's mask, the offers checked against the
    // crawl's pose, the masked updates whose gather checked any offer and the
    // most it checked at once, the spot searches for an aimed offer out of
    // reach and those that found one, and the offers near the player now.
    uint32_t offersMasked{}, offersPosed{}, offersGathered{}, offersMostChecked{};
    // And the presses made again because the game still held the offer.
    uint32_t offersSearches{}, offersSpots{}, offersNear{}, offerRetries{};
    // Version 39: Spider-Sense in the headset (spider_sense.hpp). Warnings
    // that showed (an attacker's mark from its start), headset frames that
    // drew a mark over an attacker and frames that drew one in the sightline
    // for an attacker out of view, tingles sent to the controllers, marks a
    // counter broke, and the game's dodge (B) pressed while a warning stood.
    uint32_t senseShown{}, senseFrames{}, senseSightFrames{}, senseTingles{}, senseBroken{}, senseDodges{};
    // Version 40: suit powers asked for with both hands at the chest
    // (game_pad::SuitPowerPress), dodges made by moving (MoveDodge), dives
    // (WebStrike::dive) and the blows whose sound the headset played itself,
    // the game's own not being there (game_sound.hpp).
    uint32_t suitPowers{}, moveDodges{}, dives{}, ownSounds{};
    // Version 41: seated play (seated.hpp): SEATED on (1) and a seated eye
    // height measured (2); HEIGHT, centimetres (0 AUTO); the seated eye height
    // last measured and how far the tracking space is raised now, millimetres;
    // and how many seated eye heights were measured.
    uint32_t seated{}, heightCm{}, seatedEyeMm{}, seatedLiftMm{}, seatedMeasures{}, reserved41{};
    // Version 42: the phone drawn from a back pocket (PocketDraw::Stats):
    // grip presses that may have been for it, draws begun, phones out, draws
    // too slow and let go of on the way, and presses that began none, by
    // PocketDraw::Miss. Then the latest miss: its Miss + 1 (0 none yet), the hand
    // from the neck in the body's level axes (millimetres: x right, y up, z
    // ahead), how long the headset had lost it (milliseconds) and how far the
    // head looked from the body's way (tenths of a degree, left positive).
    uint32_t pocketPresses{}, pocketBegun{}, pocketDraws{}, pocketSlow{}, pocketLetGo{}, pocketMissed[6]{};
    uint32_t pocketLastMiss{};
    int32_t pocketLastHand[3]{};
    uint32_t pocketLastLostMs{};
    int32_t pocketLastLook{}, reserved42{};
    // Version 43: EYE AIM. eyeGaze: the headset's eye tracking + 1 (0 not
    // known yet; 1 the runtime offers none, 2 it does but not for this
    // headset, 3 an eye tracker: XrRuntime::EyeTracking) and the EYE AIM row
    // in bits 8-9 (1 web balls, 2 zips). Then frames of play with EYE AIM on
    // and of those the frames the eyes were tracked; web balls the eyes aimed;
    // zips the eyes aimed that went out with the game's camera on their point,
    // and those that went at EyeZip::waitSeconds without.
    uint32_t eyeGaze{}, eyeAimFrames{}, gazeFrames{}, eyeShots{}, eyeZips{}, eyeZipsLate{};
};
static_assert(sizeof(XrData) == 1312);
static_assert(PocketDraw::misses == 6);
// The game's own prompt and QTE calls, for one play session to say what their
// arguments hold (game_prompts.hpp). The eight names matched the game on
// October 13 - 107 of 630 calls into Flash in a 224 s session - but no prompt
// text reached the localized lookup (`texts` 0), so the text is not where a
// session can read what the game is asking for, and these arguments are.
struct PromptData {
    uint32_t magic = 0x53505244, version = 4, bytes = sizeof(PromptData), status{};
    int64_t sequence{};
    uint64_t invokes{}, calls{}, raised{}, cleared{}, texts{}, rewrites{};
    uint32_t demand{}, lastDemand{}, beyondPlay{}, keyboardTexts{};
    // The newest of those calls, one a line with its arguments, oldest first;
    // how many lines are filled; and how often each of the eight names came,
    // in the order of game_prompts.cpp's table (ShowPlaceablePrompt,
    // HidePlaceablePrompt, SetPlaceablePromptProgress, UpdateInWorldPrompt,
    // POIInteractAvailable, AddQTE, UpdateQTE, ClearQTE, ShowText, HideText).
    uint32_t lines{}, reserved{}, perName[game_prompts::callNames]{};
    char line[game_prompts::callLines][game_prompts::callChars]{};
    // Then the census: every distinct name the game called, in the order
    // first seen, how often each came, and one call of each with its
    // arguments. This is what names the call a prompt's text and its button
    // arrive in, without anyone having to guess which it is.
    uint32_t names{}, spare{}, censusCalls[game_prompts::censusNames]{};
    char example[game_prompts::censusNames][game_prompts::censusChars]{};
};
static_assert(sizeof(PromptData) == 12112);
// Slow motion (slow_motion.hpp, game_time.hpp): what the player did with it
// and what the game's time did. status: 1 before VR starts, then 0 with the
// game's time hooked, else why it is not (game_time::install) and slow
// motion does nothing.
struct SlowMotionData {
    uint32_t magic = 0x574f4c53, version = 2, bytes = sizeof(SlowMotionData), status = 1;
    int64_t sequence{};
    // Presses that started it, ended it, or came with too little focus; the
    // times focus ran out and play stopped it (a menu, the game screen, the
    // flat screen); whether it is on now.
    uint32_t starts{}, stops{}, refused{}, emptied{}, interrupted{}, active{};
    // Real seconds in it so far; focus (0-1) and how far into it (0-1) now;
    // the time scale asked of the game.
    float seconds{}, focus = 1, blend{}, wanted = 1;
    // The game's side in the last update of its time system: its own time
    // scale, its world's (the smaller of that and Spidy's), Havok's step (s)
    // and whether the game scales physics itself; its updates so far, and
    // those Spidy slowed.
    float gameScale = 1, worldScale = 1, physicsStep{};
    uint32_t physicsScaled{};
    uint64_t updates{}, slowedUpdates{};
    // Version 2, moments (slow_motion.hpp's SlowMoments): those begun so far
    // by kind (a heavy blow, a called blow, a thug pulled in the air, a
    // perfect dodge), whether one is on now, and the time scale it asks for.
    uint32_t moments[4]{};
    uint32_t momentActive{};
    float momentScale = 1;
};
static_assert(sizeof(SlowMotionData) == 120);
// Body haptics on bHaptics devices (body_haptics.hpp, bhaptics_output.hpp):
// what the bHaptics Player and its devices did. status: bhaptics::Status (0
// off: BODY HAPTICS has not been on; 4 no_player: no bHaptics Player installed,
// as for most players). level: BODY HAPTICS now, percent. Devices on now and
// at any time (bit 1 << body_haptics::Device: 1 the vest, 2 and 4 the left and
// right sleeve, 8 the visor) and the times the Player was connected. Events
// posted, frames sent to a device and those the SDK did not send. Blows the
// player took (a fall in his health) and of them those placed by the attacker
// the game had warned of, and landings. Version 2: ticks of VR play with the
// Player connected (25 a second), and of them each device's with something
// playing and the sum of its strongest motor over those (percent) - how much
// of play a device was on, and how strongly. Then the events by Kind (posted,
// whether their group played them or not), and the groups the tab's switches
// leave on (bit 1 << body_haptics::Group: 1 webs, 2 wind, 4 combat, 8 movement).
struct HapticsData {
    uint32_t magic = 0x53485044, version = 2, bytes = sizeof(HapticsData), status{};
    int64_t sequence{};
    uint32_t level{}, devices{}, devicesSeen{}, connects{};
    uint64_t events{}, frames{}, refused{};
    uint32_t hits{}, aimedHits{}, landings{}, liveTicks{};
    uint32_t onTicks[body_haptics::deviceCount]{}, peakSum[body_haptics::deviceCount]{};
    uint32_t kinds[body_haptics::kindCount]{};
    uint32_t groups{};
};
static_assert(sizeof(HapticsData) == 116 + 4 * body_haptics::kindCount && body_haptics::kindCount == 35);
extern "C" {
__declspec(dllexport) XrData SpidyXrData;
__declspec(dllexport) PromptData SpidyPromptData;
// SESSION DIAGNOSTICS (diag_log.hpp), for run_game_vr.py's console and diagnostics file.
__declspec(dllexport) diag_log::Data SpidyDiagData;
__declspec(dllexport) SlowMotionData SpidySlowMotionData;
__declspec(dllexport) HapticsData SpidyHapticsData;
__declspec(dllexport) XrTimingData SpidyXrTimingData;
__declspec(dllexport) EyeSnapshot SpidyXrSnapshot;
__declspec(dllexport) flip_reprojection::Data SpidyReprojectData;
DWORD WINAPI SpidyStart(void*);
DWORD WINAPI SpidyEyeTemporal(void*);
DWORD WINAPI SpidyEyeShadows(void*);
DWORD WINAPI SpidyMonitorRate(void*);
DWORD WINAPI SpidyStop(void*);
DWORD WINAPI SpidySetEyes(void*);
float SpidyFlatAspect();
}
namespace {
using BridgeCall = DWORD(WINAPI*)(void*);
// FACING CHECK lines: 'facing check: <event> <view|hero> expected X got Y (verdict)'.
void logFacing(const facing_check::Check& c, const facing_check::Out& o) {
    constexpr float toDeg = 180.f / 3.14159265f;
    const char* ev = facing_check::eventName(c.event);
    if (o.view.verdict != facing_check::Verdict::none)
        diag_log::writef("facing check: %s view expected %.0f got %.0f (%s)%s", ev, o.view.expected * toDeg,
                         o.view.got * toDeg, facing_check::verdictName(o.view.verdict),
                         o.view.verdict == facing_check::Verdict::corrected ? ", view turned back" : "");
    if (o.hero.verdict != facing_check::Verdict::none)
        diag_log::writef("facing check: %s hero expected %.0f got %.0f (%s)%s", ev, o.hero.expected * toDeg,
                         o.hero.got * toDeg, facing_check::verdictName(o.hero.verdict),
                         o.hero.verdict == facing_check::Verdict::corrected ? ", walk stick turns him" : "");
}
XrConfig config;
BridgeCall submitInput{}, sampleBridge{}, stopBridge{}, retargetBridge{};
BridgeCall startRays{}, submitRays{}, stopRays{};
// The world rays' results, read where they are written: the bridge exports the
// struct itself, and the session is in the same process as it.
const native_rays::Data* rayResults{};
// The game's own blow sounds (game_sound.hpp), read in place; null from an
// older ray module, whose blows sound as the headset makes them.
const game_sound::Data* soundResults{};
// The game's own dodges for a player a web carries (game_dodge.hpp), read in
// place; null from an older ray module, which has none.
const game_dodge::Data* dodgeResults{};
// The hero's dodge as the game has it each frame (game_dodge.hpp's Live), for
// DODGE TUMBLE; null from an older ray module, which has none.
const game_dodge::Live* dodgeLive{};
// The tab's BLOW SOUNDS to the ray module (SpidySoundMode; optional).
DWORD(WINAPI* soundMode)(void*){};
// SOUND TRACE (sound_trace.hpp): the game's sound calls, read in place, and
// the setting to the ray module; null from an older ray module.
const sound_trace::Ring* soundTrace{};
DWORD(WINAPI* soundTraceMode)(void*){};
BridgeCall startSwing{}, submitSwing{}, sampleSwing{}, stopSwing{}, retargetSwing{}, sampleGrab{}, sampleAim{};
// Optional: pose coupling from the visual tuck (an older ray module has none).
BridgeCall setSwingPoseCoupling{};
// POINT JUMP's launch for the swing's next step (SpidySwingPointJump; none in an older module).
BridgeCall swingPointJump{};
// Optional: the web tie's telemetry (game_grab::TieData; an older ray module has none).
BridgeCall sampleTie{};
// Optional: web lines (game_lines::LineData; an older ray module has none).
BridgeCall sampleLine{};
// Each web's edges (game_swing::Bends): optional, without it a web round an
// edge is drawn to that edge.
BridgeCall sampleBends{}, sampleWebFlight{};
BridgeCall sampleClimb{};
BridgeCall startPunch{}, samplePunch{}, stopPunch{}, swingSettings{}, sampleStrike{}, sampleSlingshot{};
// Spider-Sense (game_sense, in the same module as the punches): optional, an
// older module draws no warnings.
BridgeCall sampleSense{};
// The game's throw aimed by the free hand (game_throw, same module): optional,
// an older module throws the camera's way.
BridgeCall aimGameThrow{};
BridgeCall startShooter{}, sampleShooter{}, stopShooter{}, wheelShooter{};
// WEB AIM: MODEL FOREARM: optional (an older ray module aims as before).
BridgeCall swingWebTurns{};
// The game's own crawl under the player (game_warp, in the same module):
// optional, for the session report.
BridgeCall sampleWarp{};
// The game's interactions on Spidy's wall (game_offers, in the same module):
// what it gathered near the player, and the use of the one he points at.
BridgeCall sampleOffers{}, useOffer{}, aimOffer{};
std::mutex lifecycle, telemetry;
HANDLE worker{};
// STARTUP STAGES and the watchdog (diag_log::Stages). diagPublish: the worker
// and the watchdog both copy the diagnostics out; one at a time keeps the
// sequence's odd/even meaning for run_game_vr.py.
diag_log::Stages startupStages;
std::mutex diagPublish;
void publishDiag() {
    std::lock_guard lock(diagPublish);
    auto& g = SpidyDiagData;
    if (!g.build[0])
        strncpy_s(g.build, diag_log::build(), _TRUNCATE);
    InterlockedIncrement64(&g.sequence);
    diag_log::copyOut(g);
    InterlockedIncrement64(&g.sequence);
}
void stageLine(int stage, const char* detail = nullptr) {
    diag_log::writef("stage: %s%s%s", diag_log::Stages::name(stage), detail ? " - " : "", detail ? detail : "");
}
// Once a second while the worker runs: a "watchdog:" line when it is due, and
// the diagnostics published, as the stuck worker would not.
DWORD WINAPI watchdog(void* handle) {
    char line[diag_log::lineChars];
    while (WaitForSingleObject(static_cast<HANDLE>(handle), 1000) == WAIT_TIMEOUT) {
        if (diag_log::enabled() && startupStages.check(GetTickCount64(), line, sizeof(line))) {
            diag_log::write(line);
            publishDiag();
        }
    }
    CloseHandle(static_cast<HANDLE>(handle));
    return 0;
}
std::atomic<bool> stopRequested{};
std::atomic<uint64_t> keepAliveDeadline{};
void message(uint32_t status, uint32_t error, const char* text) {
    std::lock_guard lock(telemetry);
    InterlockedIncrement64(&SpidyXrData.sequence);
    SpidyXrData.status = status;
    SpidyXrData.error = error;
    strncpy_s(SpidyXrData.message, text, _TRUNCATE);
    InterlockedIncrement64(&SpidyXrData.sequence);
}
bool read(uintptr_t p, void* out, size_t bytes) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(p), bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void check(DWORD code, const char* operation) {
    if (code)
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(code));
}
// Publishes a completed eye copy, or withdraws the previous one before its
// pixels are released.
void publishSnapshot(EyeSnapshot shot, const D3D12Renderer::Captured* pixels) {
    std::lock_guard lock(telemetry);
    auto& d = SpidyXrSnapshot;
    InterlockedIncrement64(&d.sequence);
    const auto sequence = d.sequence;
    const auto count = d.count + (pixels != nullptr);
    d = shot;
    d.sequence = sequence;
    d.count = count;
    if (pixels) {
        d.pixels = reinterpret_cast<uint64_t>(pixels->pixels);
        d.width = pixels->width;
        d.height = pixels->height;
        d.rowPitch = pixels->rowPitch;
    }
    InterlockedIncrement64(&d.sequence);
}
struct RuntimeSelection {
    std::wstring previous;
    bool present{};
    explicit RuntimeSelection(const wchar_t* requested) {
        const DWORD size = GetEnvironmentVariableW(L"XR_RUNTIME_JSON", nullptr, 0);
        present = size != 0;
        if (size) {
            previous.resize(size);
            GetEnvironmentVariableW(L"XR_RUNTIME_JSON", previous.data(), size);
            previous.resize(size - 1);
        }
        constexpr auto virtualDesktop =
            L"C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json";
        const wchar_t* manifest = *requested ? requested : virtualDesktop;
        if (GetFileAttributesW(manifest) == INVALID_FILE_ATTRIBUTES) {
            if (*requested)
                throw std::runtime_error("The chosen OpenXR runtime is not installed");
            return; // no Virtual Desktop: the loader uses Windows' active runtime
        }
        if (!SetEnvironmentVariableW(L"XR_RUNTIME_JSON", manifest))
            throw std::runtime_error("Could not select the OpenXR runtime for this process");
    }
    ~RuntimeSelection() {
        SetEnvironmentVariableW(L"XR_RUNTIME_JSON", present ? previous.c_str() : nullptr);
    }
};
// OpenXR add-ons Spidy cannot run with (xr_layers.hpp), switched off in this
// process while the loader reads them: as it creates the instance. Each by
// the variable its own manifest names; one the player has set stays as it is.
struct LayerSwitches {
    std::vector<std::string> variables;
    // The layers switched off, for the player.
    std::string names;
    explicit LayerSwitches(std::span<const xr_layers::Layer> layers = xr_layers::unsupported) {
        for (const HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            HKEY key{};
            if (RegOpenKeyExW(root, L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit", 0, KEY_READ, &key) !=
                ERROR_SUCCESS)
                continue;
            // Each value: a manifest's path, and 0 while its layer is on.
            for (DWORD index = 0; index < 256; ++index) {
                wchar_t path[1024];
                DWORD chars = static_cast<DWORD>(std::size(path)), type{}, off{}, bytes = sizeof(off);
                const auto code = RegEnumValueW(key, index, path, &chars, nullptr, &type,
                                                reinterpret_cast<BYTE*>(&off), &bytes);
                if (code == ERROR_NO_MORE_ITEMS)
                    break;
                if (code != ERROR_SUCCESS || type != REG_DWORD || off)
                    continue;
                const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                nullptr, OPEN_EXISTING, 0, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                    continue;
                std::string text(65536, '\0');
                DWORD size{};
                const bool loaded =
                    ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr);
                CloseHandle(file);
                if (!loaded)
                    continue;
                text.resize(size);
                const auto layer = xr_layers::switchOff(text, layers);
                if (layer.variable.empty() || GetEnvironmentVariableA(layer.variable.c_str(), nullptr, 0) ||
                    !SetEnvironmentVariableA(layer.variable.c_str(), "1"))
                    continue;
                variables.push_back(layer.variable);
                names += (names.empty() ? "" : ", ") + std::string(layer.name);
            }
            RegCloseKey(key);
        }
    }
    ~LayerSwitches() {
        for (const auto& variable : variables)
            SetEnvironmentVariableA(variable.c_str(), nullptr);
    }
};
// Where the settings only the SPIDY VR tab offers are kept
// (vr_settings::tabOnlyText): beside the launcher's launcher.ini, so every
// Spidy folder and every build finds them (this module runs from a copy
// staged per build).
std::wstring tabOnlyPath() {
    PWSTR folder{};
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder)) && folder)
        path = std::wstring(folder) + L"\\Spidy\\spidy_vr.ini";
    CoTaskMemFree(folder);
    return path;
}
std::string readTabOnly() {
    const auto path = tabOnlyPath();
    if (path.empty())
        return {};
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    std::string text(4096, '\0');
    DWORD size{};
    if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr))
        size = 0;
    CloseHandle(file);
    text.resize(size);
    return text;
}
// Written whole and then put in place, so a crash mid-write keeps the last.
bool writeTabOnly(const std::string& text) {
    const auto path = tabOnlyPath();
    if (path.empty())
        return false;
    CreateDirectoryW(path.substr(0, path.find_last_of(L'\\')).c_str(), nullptr);
    const auto written = path + L".new";
    const HANDLE file = CreateFileW(written.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD size{};
    const bool whole = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr) &&
                       size == text.size();
    CloseHandle(file);
    return whole &&
           MoveFileExW(written.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
// REALISTIC FLIPS and the rows under it, as the rig takes them.
RealisticFlipSettings realisticFlips(const vr_settings::Values& v) {
    return {v.realisticFlips,         v.automaticReleaseFlip,  v.automaticStrength,
            v.automaticSpeed,         v.automaticMinimumSpeed, v.automaticResponse,
            v.automaticHandStrength,  v.automaticFalloff,      v.automaticHandThreshold,
            v.automaticAirBrake,      v.automaticCombat};
}
DWORD WINAPI run(void*) {
    bool nativeStarted = false, gpuStarted = false, raysStarted = false, swingStarted = false, websStarted = false,
         bodyStarted = false;
    uint64_t serial = 1, nativeDeadline{};
    // lastPresentedMs: an image was on show. lastNewImageMs: the game delivered a newer one.
    uint64_t firstImageDeadline{}, lastPresentedMs{}, lastNewImageMs{};
    bool presented{}, wasGameplay{};
    // The last frame's predicted display time; held images age against it.
    int64_t displayTime{};
    Presentation shownKind{};
    uint32_t error{};
    std::string failure;
    // VR starts with the game, before there is a player; each loaded save,
    // respawn or character switch brings a new one.
    game_player::Watch players;
    bool padInstalled{};
    try {
        auto queue = reinterpret_cast<ID3D12CommandQueue*>(config.queue);
        ComPtr<ID3D12Device> device;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))
            throw std::runtime_error("Game graphics device unavailable");
        XrRuntime runtime;
        // The connection's lines (xr_connect.hpp): on the launcher's console while VR starts (its log
        // keeps every status line), and in the report's timeline and RUN LOGS throughout, so a start
        // or a drop names the OpenXR call, its result and the session states before it.
        bool connecting = true;
        runtime.log = [&connecting](const std::string& line) {
            game_prompts::note(line.c_str());
            if (connecting)
                message(1, 0, line.c_str());
        };
        // A headset the runtime calls unavailable is waited for (November 11: Meta Quest Link said so
        // five starts in a row, seconds after the launcher's probe had found it), up to two minutes,
        // telling the player; stopping VR ends the wait.
        runtime.headsetWait = xr_connect::Wait{};
        // One status line each 5 s of the wait, not each attempt (the launcher prints every change).
        int waitShown = -1;
        runtime.waitingForHeadset = [&runtime, &waitShown](double seconds, unsigned) {
            if (static_cast<int>(seconds / 5) != waitShown) {
                waitShown = static_cast<int>(seconds / 5);
                const auto line = xr_connect::waitingLine(runtime.runtimeName().c_str(), seconds,
                                                          runtime.headsetWait.limitSeconds);
                message(1, 0, line.c_str());
            }
            return !stopRequested.load();
        };
        {
            RuntimeSelection selected(config.runtime);
            LayerSwitches layers;
            if (!layers.names.empty()) {
                const auto text = layers.names + " is switched off for the game: Spidy does not run with it. "
                                                 "It stays on for your other games";
                message(1, 0, text.c_str());
            }
            runtime.initialize(device.Get(), queue, config.eyeSize, config.renderScale);
        }
        connecting = false;
        D3D12Renderer overlay;
        overlay.initialize(device.Get(), queue);
        // The session report's copies of the left eye live in the overlay's
        // readback buffer; withdraw the last one before the overlay goes.
        struct WithdrawSnapshot {
            ~WithdrawSnapshot() {
                publishSnapshot({}, nullptr);
            }
        } withdrawSnapshot;
        EyeSnapshotSchedule snapshots;
        EyeSnapshot pendingSnapshot{};
        uint64_t snapshotTicket{}, gapGeneration{};
        message(2, 0, "Waiting for the headset");
        players.start(config.base);
        game_player::Player player{};
        uint64_t playerChanges{}, playersFound{};
        // Who the player is (game_fp::otherCharacter), from the skeleton last
        // read off his entity: Spider-Man until a body says otherwise.
        uint64_t playerBody{};
        bool spiderMan = true;
        // Menus are played with a virtual Xbox controller (game_pad).
        padInstalled = !game_pad::install();
        uint64_t padRetryMs = GetTickCount64() + 1000;
        // The VR settings are a tab of the game's own Settings (game_menu).
        const uint32_t menuCode = game_menu::install(config.base);
        if (menuCode) {
            const auto text = "No SPIDY VR tab in the game's Settings (" + std::to_string(menuCode) +
                              "); the launcher's settings apply";
            message(3, 0, text.c_str());
        }
        // The game's request for a Steam ticket, on its way back to the main
        // menu, closed every VR session that quit to it (game_steam). The
        // guard goes in once the game holds its Steam interface.
        bool steamGuarded{}, steamWarned{};
        uint64_t steamRetryMs{};
        // Slow motion slows the game's own time (game_time).
        const uint32_t timeCode = game_time::install(config.base);
        if (timeCode) {
            const auto text =
                "No slow motion: the game's time could not be hooked (" + std::to_string(timeCode) + ")";
            message(3, 0, text.c_str());
        }
        {
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidySlowMotionData.sequence);
            SpidySlowMotionData.status = timeCode;
            InterlockedIncrement64(&SpidySlowMotionData.sequence);
        }
        // The last frame showed the game screen; A held from there (Resume,
        // Continue) is not a jump once gameplay is back.
        bool wasScreen{}, jumpFromScreen{};
        // A pressed in the air is not the game's web zip.
        AirJumpFilter airJump;
        // Where B, X and Y went down: on the game screen they are the game's
        // own, in VR B dodges, Y interacts and X opens the gadget wheel
        // (game_pad::PressOrigins).
        game_pad::PressOrigins origins;
        // Y held walks the player up to what they look at: the game takes
        // its Y only within about a metre of the thing.
        game_pad::InteractStep interactStep;
        uint32_t interactStepsBefore{};
        // The game's own camera, turned to where the player looks: the game
        // takes its Y for what that camera has in view.
        game_pad::CameraLook cameraLook;
        // The game's LB + RB (its web pull): both web buttons on a thing no
        // web holds on, then a yank (WebPull); or X where the game shows the
        // two (NamedPress; the right thumbstick's click until October 24).
        // aimed: what each hand's web button would do, markers or not.
        WebPull webPull;
        game_swing::AimData aimed{};
        uint64_t pullsBefore{};
        // The right thumbstick pushed down is the game's D-pad down (heal),
        // pushed up its LT + RT (the zip to a point, a vent).
        game_pad::StickPush stickPush;
        // LEG CONTROL (leg_control.hpp): with CONTROLLED FLIPS on and the body
        // airborne or swinging free, the right stick poses the legs while they
        // are engaged (LEG HOLD: Y+X, then the stick moved). legStickOwns: the legs'
        // block below found the mode on this frame; legStickActive: last
        // frame's, latched where the stick is read (the rig, the zip/heal).
        leg_control::Simulation legSim;
        bool legStickOwns{}, legStickActive{}, legModeLogged{};
        // LEG HOLD (leg_hold.hpp; the user, November 12 01:33, t67u): Y+X held
        // engage the legs, the right stick moved keeps them, neutral ends them.
        leg_hold::Hold legHold;
        leg_hold::Hold::Output legHoldOut;
        bool legBaseLast{};
        // JUMP TO SPIDYVR: last frame was a midair animation under HYBRID. LEG
        // HOLD's Y+X may start there too, so a hold made before the timed handoff
        // carries across it (the legs move from the switch, or a right-stick
        // pitch breaks at once as the flip break's legs rule says).
        bool legChordAirAnimLast{};
        float legStickX{}, legStickY{};
        Vec3 legPrevVelocity{}, legAccel{};
        bool legPrevVelocitySet{};
        Quat legPrevBody{};
        bool legPrevBodySet{};
        // The legs' layers (leg_layers.hpp; November 12 06:18): ACTIVE (the stick),
        // FLAIL (Y+X + B in the air), AUTO POSES (AIR CROUCH, REACH SPREAD) and
        // PASSIVE LEGS, one spring. legsAirLast: last frame's legs-may-move air
        // (the flail's B gate runs before this frame's is known).
        leg_layers::Mixer legMixer;
        leg_layers::FlailGate flailGate;
        bool legsAirLast{}, flailNow{};
        bool reachHand[2]{};
        game_pad::ZipPress zipPress;
        game_pad::FlipZipPriority flipZipPriority;
        // EYE AIM: the zip waits for the game's camera to come onto what the
        // eyes look at (EyeZip); frames of play with EYE AIM on and, of those,
        // with the eyes tracked; web balls the eyes aimed; and whether the
        // player was told the headset gives no gaze.
        game_pad::EyeZip eyeZip;
        uint32_t eyeAimFrames{}, gazeFrames{}, eyeShots{};
        bool toldNoGaze{};
        bool interacting{}, dodging{}, healing{}, zipping{};
        bool zipBegan{}; // a zip press began this frame (MOVE HAPTICS)
        // The player asked the game for a move of its own while the swing carried him
        // in the air (game_swing::AirYield), for the next swing command.
        bool askingGame{};
        bool askingZip{};
        uint32_t interacts{}, dodges{}, heals{}, zips{};
        // The whole pad, latched by clicking both thumbsticks, for the
        // controls VR play has no room for; and the buttons the game names in
        // a prompt, sent by the VR controls of those names (game_pad.hpp,
        // game_prompts.hpp): what they send this frame, and whether the swing
        // was last told that fists are the game's X meanwhile.
        game_pad::PadLayer padLayer;
        game_pad::NamedPress namedPress;
        game_pad::State namedPad{};
        bool namedFists{}, fistsBlows{};
        uint32_t namedPressesBefore{}, slamsBefore{};
        // The gadget wheel (X held; gadget_wheel.hpp): the player's gadgets
        // as the shooter last listed them, and whether the free hand's trigger
        // fires the equipped one (off: web balls, the game's own Web Shooter's
        // while it is the one equipped: game_shooter.hpp).
        GadgetWheel gadgetWheel;
        game_shooter::Data listed{};
        bool gadgetFire{};
        // The gadgets in the wheel's places, the web shooter's at the top, and
        // the place equipped (the web shooter's while the free hand's trigger
        // fires web balls): the wheel's, and the phone's GEAR in its order.
        const auto wheelPlaces = [&](GadgetWheelView& view) {
            view.places[0] = Gadget::webShooter;
            view.equipped = 0;
            view.bare = true;
            for (uint32_t g = 0; g < std::min(listed.gadgetCount, 8u); ++g) {
                const auto& o = listed.gadgets[g];
                if (!o.kind || o.place >= gadgetPlaces)
                    continue;
                view.places[o.place] = static_cast<Gadget>(o.kind);
                view.ammo[o.place] = o.ammo;
                view.most[o.place] = o.most;
                view.bare = view.bare && o.place == 0;
                if (gadgetFire && o.weapon == listed.equipped)
                    view.equipped = o.place;
            }
        };
        // The phone (phone.hpp): open from a back-pocket draw until B, live
        // while VR shows play; its music plays on with it away, on a thread
        // of its own (phone_music.hpp). phoneShown: what its screen shows,
        // the newest, which every image draws. phoneBlows: the swing was last
        // told the phone has the hands (Settings::noBlows).
        Phone phone;
        PocketDraw pocketDraw;
        PhoneMusic phoneMusic;
        // PC MEDIA's: what the PC plays (phone_pc_media.hpp), from the first
        // draw with it on.
        std::unique_ptr<PcMedia> pcMedia;
        PhoneView phoneShown;
        bool phoneBlows{};
        // The phone's MENU: the game's Back, which opens its menu (the map,
        // suits and skills), sent this long. A tap of X sent it until October
        // 23, when the menu went onto the phone.
        constexpr float menuTapSeconds = .1f;
        float menuTap{};
        // The phone's CAMERA: the game's D-pad up, which takes its camera out
        // (and puts it away again), sent this long.
        constexpr float cameraTapSeconds = .2f;
        float cameraTap{};
        // The game played on its screen (VIEW: GAME SCREEN): Back and the
        // D-pad, which the VR controllers lack, besides the Xbox controller's.
        game_pad::ScreenControls screenControls;
        // Both thumbsticks held in: the game's Start, where the runtime keeps
        // the menu button (SteamVR's dashboard on a Quest's left controller).
        game_pad::PausePress pausePress;
        uint32_t padEntriesBefore{}, wallInteracts{}, pausesBefore{};
        // The headset has given a tracked head pose at least once.
        bool headSeen{};
        // The swing settings exactly as they were last sent, so the interact's
        // hand-over can put the walls back the way the player set them, and
        // never turn them on for a player who asked for them off.
        game_swing::Settings sentSwingSettings;
        bool swingSettingsSent{}, wallsHandedBack{}, toldHandoverRefused{};
        game_pad::WallHandover wallHandover;
        // What the game's own crawl does with the stick Spidy sends it, which
        // is the last unmeasured step between a wall and a vent that behaves
        // like the ground.
        CrawlStick crawlStick;
        uint32_t crawlFrames{}, crawlStood{};
        // The game's own crawl at rest and its wall run (XrData version 33).
        uint32_t gameRunFrames{}, holdGraceFrames{};
        // Version 34: frames of play the rig could not place and why; the
        // game's crawl on a floor, and the head held at its height there
        // (under a duct's roof: FloorCrawlRoof); the body kneeling under a
        // held head.
        uint32_t trackingGapFrames{}, trackingGapReasons{}, floorCrawlFrames{}, crawlCapFrames{};
        uint32_t kneelFrames{};
        FloorCrawlRoof floorRoof;
        CrawlCameraGate crawlCameraGate;
        game_warp::Data warp{};
        // The game's interactions on Spidy's wall (version 35): what the game
        // gathered near the player, what each hand points at, and the
        // interact's use of it.
        game_offers::Data offers{};
        std::array<std::optional<offer_pointing::Pointed>, 2> pointing{};
        std::array<uint32_t, 2> pointedBefore{};
        offer_pointing::Use offerUse;
        uint32_t offerPointFrames{}, offerReachFrames{}, offersSeen{};
        uint32_t offerAsksBefore{}, offerFailuresBefore{}, offerAimed{};
        // The surface over the player's feet, as the last finished ray batch
        // found it: the head stops short of it, so it never goes through the
        // roof of a vent. Zero where nothing is over him.
        float headroomSeen{}, headroomLeast{};
        uint32_t headDroppedFrames{};
        bool pulling_{};
        // The game's throw aimed by the free hand: seconds the aim still goes
        // out after X is let go (the throw may still be reading it).
        float throwAimTail{};
        bool padLayerWas{};
        // The prompt calls are copied out only when one more has come: the
        // lines are 3 kB and nothing changes between calls.
        uint64_t promptCallsBefore = ~0ull;
        uint32_t promptRefresh{}, vrButtonsPlayed{};
        float slingshotPulseSeconds{};
        // A held on the ground charges a leap of the swing's own (the game's
        // Charge Jump is a skill a save may lack), and a trigger pulled with
        // its hand at the temple is the game's R3, its scan. leapOut: the
        // leap let go of, for the swing's next commands.
        game_pad::ChargeJump chargeJump;
        // JUMP ALWAYS (v5): the game's A held for the moment the swing hands
        // back a body it ran on the ground (game_pad::JumpRelay), and a jump:
        // line in the diagnostics for every press: where it went, how late.
        game_pad::JumpRelay jumpRelay;
        bool runJumpOpen{};
        float runJumpAge{};
        bool jumpRawBefore{}, jumpOutBefore{}, jumpPressOpen{};
        uint32_t jumpPressCharges{};
        float jumpPressAge{};
        char jumpPressWhere[96]{};
        game_pad::NativeRun nativeRun; // NATIVE RUN's RT on the ground
        run_update::RunLog runLog;     // RUN LOGS' lines
        // WEB AIM: MODEL FOREARM (web_aim::ForearmAim): each hand's forearm
        // turn, smoothed, and the turn sent to the swing this frame (the aim
        // markers take the same one); the body's latest forearms' solve count
        // and the frames it has not moved; the log's clock and each hand's
        // largest step of the forearm's turn from frame to frame since the
        // last line (its jitter). webTurnsOn: turns are being sent.
        std::array<web_aim::ForearmAim, 2> forearmAims;
        // SPHERICAL AIM: each hand's learned forearm (controller frame) and
        // the eased trust in the solve (spherical_aim.hpp). The model lives
        // for the session: owner switches keep it.
        std::array<spherical_aim::ForearmModel, 2> forearmModels;
        std::array<spherical_aim::Blend, 2> forearmBlends;
        std::array<Quat, 2> webTurnsNow{}, forearmTargets{};
        bool webTurnsOn{};
        uint64_t forearmJob{};
        uint32_t forearmStaleFrames{};
        float forearmLogSeconds{};
        float shooterLogSeconds{};
        diag_log::ChangeOnly forearmDiag[2];
        // STEADY STATES' RT (ground_state::RiseDelay), and the state changes'
        // lines in the diagnostics (ground_state::StateLog).
        ground_state::RiseDelay runSteady;
        ground_state::StateLog stateLog;
        // perf: lines (frame_stats.hpp).
        frame_stats::FrameStats frameStats;
        // HEAD PROBE (off by default; head_probe.hpp): read only. Its numbers
        // (some 15 KB) on the heap once, its lines in a buffer reused each frame.
        const auto headProbe = std::make_unique<head_probe::Probe>();
        head_probe::Probe::Lines headProbeLines;
        bool headProbeWas{};
        animated_moves::Handoff animatedMovesHandoff{};
        // ANIMATED ONLY (off by default): cutscene/stale grace and state diag.
        animated_moves::StayLatch animatedOnlyLatch{};
        animation_cancel::Gate gripCancelGate;
        uint32_t gripCancelToken{};
        uint64_t actionFrameFloor{};
        uint64_t cancellationSerialFloor{};
        bool animatedOnlyHeldWas{};
        int animatedOnlyStateWas = -1;
        // ANIMATED AIR SPIN (off by default): smoothed spherical / momentum turn.
        animated_air_spin::Tracker airSpin{};
        int64_t airSpinDiagAt{};
        bool airSpinWas{};
        // ANIMATED HYBRID (off by default): animation-default / intent-VR owner FSM.
        hybrid_owner::HybridOwner hybridOwner{};
        bool hybridWebWas{};
        // ANIMATED HYBRID web zips (hybrid_owner::Frame::zipPress): pressed this
        // frame, for the owner's update further down the same frame.
        bool hybridZipPress{};
        bool hybridYWas{};
        // ANIMATED HYBRID jump / air adopt (hybrid_owner::Frame::jumpAsk): a real jump or leap
        // asked for on the ground this frame, or any other mid-air takeover. hybridLeapAir: the
        // body was adopted in the air (its legs keep that pose, body::Targets::airborne). The
        // adopt's first-frame diag waits on native_body::jumpAdopt's count.
        bool hybridJumpAsk{};
        int64_t positionLockCheckNs{-1}; // POSITION LOCK residual check time
        unsigned punchEaseEndsLogged{}; // PUNCH VIEW EASE: ends already logged
        // Which switch the view ease now running (or last) began at: "punch",
        // "jump handoff", "flip-break", "grip-cancel" (AIR VIEW EASE).
        const char* easeWhy = "punch";
        std::array<aim_filter::Direction, 2> aimSteady; // WRIST MARKER SMOOTHING (shot = marker)
        bool hybridLeapAir{};
        bool hybridAdoptWaiting{};
        uint64_t hybridAdoptSeen{};
        // Horizontal measured (feet position-delta) speed for the owner's speed intent.
        Vec3 hybridFeetWas{};
        bool hybridFeetWasSet{};
        float hybridSpeedH{};
        // Parkour steering under HYBRID: the head-locked view's yaw, low-passed.
        hybrid_steer::YawFilter hybridViewYaw{};
        int64_t hybridSteerDiagAt{};
        // VIEW YAW SOURCE: STABLE's heading (stable_yaw.hpp), the hero-vs-steer
        // watch and the runaway-turn detector.
        stable_yaw::Heading stableHeading{};
        stable_yaw::GapWatch yawGap{};
        stable_yaw::Runaway yawRunaway{};
        // FULL head lock on the stable heading (view_yaw_lock.hpp, Step B): the
        // lock's heading, TURN-IN-PLACE, the divergence metric and the R-stick
        // turn watch. viewLockActive: published this frame (eyes, hands, arms);
        // fullLockGate: the R stick turns it (Full on the ground, not softlock).
        view_yaw_lock::Lock viewLock{};
        view_yaw_lock::TurnInPlace turnInPlace{};
        // FACING CHECK (facing_check.hpp): armed on each ownership event, a forced
        // hero turn waiting for the next frame's walk stick.
        facing_check::Check facingCheck{};
        bool facingHeroPending = false;
        float facingHeroYaw = 0;
        facing_check::Event facingTurnEvent = facing_check::Event::landing;
        uint32_t facingArmSeen = 0, facingFrames = 0;
        // MOVEMENT MODE HYBRID (hybrid_movement.hpp): the left stick's gate, the heading
        // the view holds while the hero is brought onto it, the air facing sync.
        hybrid_movement::Gate hybridGate{};
        hybrid_movement::Heading hybridHeading{};
        hybrid_movement::Gate::Out hybridStick{};
        bool hybridHeroTurn = false, hybridSteerable = false, hybridAirSyncOn = false;
        // PARKOUR RT diagnostic (t86u): frames with the stick at the run on the ground.
        uint32_t parkourRtFrames = 0, parkourRtHeld = 0, parkourRtRising = 0;
        float parkourRtSeconds = 0;
        std::array<uint32_t, static_cast<int>(game_pad::RunBlock::count)> parkourRtBlocked{};
        float hybridAirSyncSec = 0, hybridHoldLoggedAge = 0;
        uint32_t hybridAirSyncStarts = 0;
        view_yaw_lock::Divergence viewDivergence{};
        view_yaw_lock::TurnWatch viewTurnWatch{};
        bool viewLockActive{}, fullLockGate{}, viewLockSnapped{};
        float viewLockDelta{};
        float viewLockHeading{}; // the lock + the headset's turn (where the player looks)
        bool viewLockHeadingOk{};
        int64_t viewYawDiagAt{}, viewDivergenceAt{};
        uint32_t viewLockTurnsLogged{};
        // MOVE HAPTICS (move_haptics.hpp) and SOUND TRACE's events (sound_trace.hpp).
        move_haptics::Detector moveDetector{};
        move_haptics::Scheduler moveScheduler{};
        sound_trace::Watch soundWatch{};
        uint32_t moveHapticPulses{};
        // Slices 2–3: 180 ms eye/yaw blend + delayed arms wipe on G↔T edges.
        struct HybridBlend {
            bool active{};
            bool toTraversal{}; // true = G→T reclaim; false = T→G land
            int64_t startNs{};
            int64_t landConfirmNs{}; // grounded clock at enterAnim (diag)
            Vec3 fromEyes{};
            float fromYaw{};
            Vec3 fromEyePos[2]{};
            Quat fromEyeOri[2]{};
            bool fromValid{};
            bool delayArmsClear{};
            bool delayModelYaw{};
            float enterEyeDPos{};
            float enterEyeDYaw{};
            float landSpeed{};
            bool clearLook{};
            // After an owner edge: refresh sticks and hold CameraLook off
            // through the blend and a held run, so leftover look does not
            // orbit the walk camera until a jump.
            bool recheckMove{};
            // Slice C: the source eyes' player position; they travel with him from it.
            Vec3 fromAnchor{};
        } hybridBlend{};
        // Slice C: the view the headset showed last frame (blend source on an edge).
        hybrid_handoff::Shown hybridShown{};
        int64_t modelYawDiagAt{};
        animated_arms::State animatedArmsState{};
        Pose animatedLastGrip[2]{}, animatedLastAim[2]{};
        bool animatedHandSeen[2]{};
        // ARMS IN ANIMATION (arm_frame.hpp): the hand view's frame last sent
        // (the per-frame map uses the same), the hands' relative velocity for
        // HAND PREDICTION, and BODY KINEMATICS (kinematics.hpp).
        animated_view::Frame armFrameWas = animated_view::Frame::none;
        Quat armLevelWas{};
        Pose armWorldEyesWas{};
        // HAND PREDICTION (review H1/L1): velocities in tracking space; a new
        // epoch (snap turn, re-latch, recenter, reseat, owner or frame-mode
        // change) restarts them. The lead is the pose job's measured wait.
        arm_frame::HandVelocity armVelocity[2];
        uint32_t armEpoch{};
        float armLead{};
        // M1: the Level R-stick turn runs before the hands are mapped, so this
        // frame's webs use this frame's turn (the gate is last frame's mode).
        bool levelTurnGate{};
        kinematics::Tracker kinematicsTracker;
        native_body::Forearms kinematicsArms{};
        // FULL ARMS (review M3/M4): this frame's strike sample and its hold.
        hybrid_owner::StrikeHold strikeHold;
        bool strikeRead{}, strikeUnderWay{};
        int64_t strikeSequence{};
        int64_t trackingLockDiagAt{};
        int64_t animatedMovesDiagAt{};
        bool animatedHeadFollowWas{};
        bool animatedSoftlockWas{};
        int animatedHeadModeWas = -1;
        Vec3 animatedMovesYieldFeet{}; // actor feet when yield began (diag: actor vs latch)
        Pose animHeadRef{};
        bool animHeadRefSet{};
        headset_smoothing::Filter animHeadSmooth;
        camera_motion::Bob animHeadBob;
        Vec3 cameraBobShift{};
        // The check that keeps that reference seated on the neck over every switch (head_seat.hpp).
        head_seat::Glide animHeadSeat{};
        int animSeatStateWas = -1;
        uint32_t animSeatActionWas = 0xffffffffu;
        uint32_t animSeatCount{};
        int64_t animSeatDiagAt{};
        float animHeadTurn{};
        bool animHeadFlick{};
        std::array<float, 2> forearmJitter{};
        game_pad::TemplePress templePress;
        uint32_t chargesBefore{}, leapsBefore{}, scansBefore{};
        // Both hands at the chest with both triggers pulled: the game's suit
        // power (L3 + R3).
        game_pad::SuitPowerPress suitPress;
        uint32_t suitPowersBefore{};
        float leapOut{};
        // POINT JUMP (point_jump.hpp): the A+B chord, the surface gate, the
        // launches sent and refused, the look and surface they aim from, the
        // takeoff held for the owner (Frame::pointJumpHold) and this frame's fire.
        point_jump::Chord pointChord;
        point_jump::Controller pointJumpGate;
        uint32_t pointJumpSerial{}, pointJumps{}, pointJumpRefused{}, pointJumpDenied{};
        Vec3 pointJumpLook{}, pointJumpNormal{0, 1, 0};
        bool pointJumpFired{}, pointJumpHold{}, pointJumpCharged{}, pointJumpBothWas{}, pointJumpOffLogged{};
        float pointJumpHoldAge{};
        const char* pointJumpSurface = "ground";
        // The game's camera follows the head's pitch too: it offers a thing
        // at the player's feet or overhead only with that camera on it.
        game_pad::CameraPitch cameraPitch;
        // Thugs the web brought to the player's hand so far (an air yank's
        // arrival among them): in the air, time slows as one arrives.
        uint64_t catchesBefore{};
        // A dodge the game itself slows time for is its perfect dodge: for a
        // moment after B went down with the game's time at full speed. The
        // side its whoosh starts on (body_haptics' azimuth: + the right).
        float dodgeWatch{}, dodgeFrom{};
        // DODGE TUMBLE (dodge_tumble.hpp): the view's lean or roll with a
        // dodge the game took, started from the game's own dodge start
        // (dodgeLive) within half a second of the player's ask for one (B, or
        // a dodge by moving). The way he asked, how long ago, whether that
        // ask is still waiting for the game's dodge, the game's starts seen,
        // and whether play stepped the tumble this frame (if not, it ends).
        dodge_tumble::Tumble dodgeTumble;
        dodge_tumble::Way tumbleWay{{0, 0, -1}, 1, false};
        float tumbleAskAge = 1e9f;
        bool tumbleAskWaiting{}, tumbleStepped{};
        uint64_t tumbleStartsSeen = UINT64_MAX;
        // A dodge by moving (spider_sense.hpp's MoveDodge): the head out of a
        // warned attack's way is the game's B, the way it went.
        MoveDodge moveDodge;
        // A dive (WebStrike::dive): the legs kick out at the one it flies at,
        // from its start until a moment after its blow. And the dives and the
        // blows whose sound the headset played itself, for the report.
        Vec3 diveAt{};
        int64_t kickUntil{};
        uint32_t dives{}, ownSounds{};
        // The VR settings: the launch options, then the SPIDY VR tab in the
        // game's Settings during play.
        vr_settings::Values values;
        values.aimMarkers = !(config.options & 128);
        values.webGrab = !(config.options & 8);
        values.body = !(config.options & 32);
        values.punch = !(config.options & 64);
        values.airWebs = !(config.options & 256);
        values.webShooter = !(config.options & 512);
        values.flips = config.options & 2048;
        values.triggerWebs = config.options & 4096;
        values.standOnWalls = !(config.options & 8192);
        values.arcadeWebs = config.options & 0x200000;
        values.swingSpeed = config.swingSpeed;
        values.snapTurn = static_cast<int>(config.snapTurn);
        values.smoothTurn = static_cast<int>(config.smoothTurn);
        values.haptics = static_cast<int>(config.haptics);
        values.screenSize = static_cast<int>(config.screenSize);
        values.weight = static_cast<int>(config.weight);
        values.hud = static_cast<int>(config.hud);
        values.flipSpeed = static_cast<int>(config.flipSpeed);
        values.screenDepth = static_cast<int>(config.screenDepth);
        values.bodyHaptics = static_cast<int>(config.bodyHaptics);
        // And those only the tab offers, as it left them (tabOnlyPath); all sanitized.
        values = vr_settings::withTabOnly(values, readTabOnly());
        std::string tabOnlySaved = vr_settings::tabOnlyText(values);
        game_menu::publish(values);
        {
            // BODY HAPTICS as the session starts: written before the first frame,
            // so a report never reads the zero of a struct not yet written.
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidyHapticsData.sequence);
            SpidyHapticsData.level = static_cast<uint32_t>(values.bodyHaptics);
            InterlockedIncrement64(&SpidyHapticsData.sequence);
        }
        uint32_t settingChanges{};
        // aims are each hand's latest preview.
        uint64_t markersDrawn{};
        game_swing::AimData aims{};
        // Each hand's aim marker, steadied from eye image to eye image.
        std::array<AimMarkerMotion, 2> aimMotion;
        GameTrackingRig rig;
        GameMotionFrame motion;
        NativeEyeHistory history;
        // Where the HUD panel faces in the room: it follows the head lazily
        // (native_hud::Follow), and starts in front of it again with play,
        // the immersive view and a recentred room.
        native_hud::Follow hudFollow;
        // STEADY HUD: where the panel goes this headset frame (the tracked
        // head, and the way Follow turned it), and the panel copy the layer's
        // image last got.
        Vec3 hudHead{};
        Quat hudFacing{};
        bool hudPlaced{};
        uint64_t hudLayerCopies{};
        bool hudLayerFilled{};
        GameScreen gameScreen;
        // Whether the game is playing a scene on the screen (its time runs),
        // and whether that scene's stereo eyes have the HUD's panel.
        SceneClock sceneClock;
        ScenePanel scenePanel;
        // First-person cutscenes (options bit 22): the tracking-space pose the view of a scene is measured
        // from (the headset where the scene began, turned to its heading), whether it is taken, and the last
        // line of the worker that finds the scene's character the log has.
        Pose sceneRef{};
        bool sceneRefSet{}, wasFpScene{}, wasCameraView{};
        uint64_t fpLastWanted{};
        // The right thumbstick's click in a first-person scene puts the theatre screen back, and again the
        // first-person view (fpTheatre); with the theater screen disabled (options bit 24) it puts the
        // scene's own camera there instead of the screen; it stays until clicked again in a scene (in play
        // the click is the game's R3, or WEB TIE's). fpClickTaken: that click toggled the view, it is not
        // slow motion.
        bool fpTheatre{}, wasFpRightClick{}, fpClickTaken{}, wasFpX{};
        // Head tracking off: the right stick turns the view (your snap or smooth turn setting), radians, and
        // whether the stick is held past a flick.
        float sceneTurn{};
        // The heading the first-person scene's view last had, world yaw, and when: play that follows a scene
        // faces there (GameTrackingRig::sceneFacing).
        float fpFacing{};
        uint64_t fpFacingMs{};
        // When play (the gameplay camera) last began / the game's own camera last began, for the first-person
        // view's quick entry and its hold across a blink of play inside one scene (a finisher's shots).
        uint64_t fpPlayStart{}, fpIdleSince{}, fpWantStart{};
        bool sceneFlick{};
        uint32_t fpNoteSeen{};
        bool flatScreen{};
        Pose screenPose{};
        const auto dimensions = runtime.eyeDimensions();
        // Aim markers are drawn so many pixels wide: above the headset's own
        // resolution they keep the size they have at it.
        const auto recommended = runtime.recommendedDimensions();
        const float markerPixels =
            recommended[0] ? std::max(1.f, static_cast<float>(dimensions[0]) / static_cast<float>(recommended[0])) : 1.f;
        const auto moduleDuration = config.durationMs ? config.durationMs + 3000 : 0;
        {
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidyXrData.sequence);
            SpidyXrData.eyeWidth = dimensions[0];
            SpidyXrData.eyeHeight = dimensions[1];
            InterlockedIncrement64(&SpidyXrData.sequence);
        }
        game_swing::Data swingState;
        game_swing::Bends swingBends;
        game_swing::WebFlightData webFlight;
        WebFlightVisual flightVisual[2];
        bool webArrivedBefore[2]{};
        game_swing::ClimbData climbState;
        game_swing::ClimbPulses climbPulses;
        unsigned collisionRayPhase{};
        WallCrawlBody wallCrawlBody;
        WallRunBody wallRunBody;
        bool ventCrawlBefore{};
        // What each hand's web holds, when it caught a thug instead of a wall.
        game_grab::Data grabState;
        uint32_t grabPhases[2]{};
        // The web tie: its webs, and the gesture's state and ties made before
        // this frame (a tick in both hands when it is ready, a pulse on a tie).
        game_grab::TieData tieState;
        bool tieArmedBefore{};
        uint64_t tiesMadeBefore{};
        // Web lines: theirs, and the gesture's state and lines made before
        // this frame (a tick in both hands when it is ready, a pulse on a line).
        game_lines::LineData lineState;
        // Each hung thug's strand's rope, among the ropes handed to the game
        // this frame (0xff: none; the overlay draws it).
        uint8_t strandRope[16]{};
        bool lineArmedBefore{}, linesSeen{};
        uint64_t linesMadeBefore{};
        // A web line's zip marker came into view (the right stick's click held
        // to aim): a tick in the right hand.
        bool lineMarkedBefore{};
        // The dodge's dash on a web line: B as it was, the dash's way along the
        // line, and how long its legs still run.
        bool lineDashDown{};
        Vec3 lineDashWay{};
        float lineDashLeft{};
        // A grab's event pulse plays this long before the web's tension
        // takes the hand's haptics again (predicted display time, ns).
        int64_t pulseUntil[2]{};
        WebTimeline webTimes;
        // The reel's ticks in each hand (reel_haptics.hpp).
        reel_haptics::Ticks reelTicks[2];
        // The swing's ticks in each hand (swing_haptics.hpp).
        swing_haptics::Ticks swingTicks[2];
        bool attachedBefore[2]{};
        // Which of the game's two ropes draws which hand's web (one web in
        // both hands has one rope), and when a free hand last came onto the
        // other hand's web (a tick says a press there takes hold of it).
        WebRopes webRopes;
        bool onStrandBefore[2]{};
        int64_t strandTickAt[2]{};
        uint64_t zipBefore{};
        // Walls the swing took the player onto, and jumps off them, by the
        // previous frame.
        uint32_t wallsBefore{}, wallJumpsBefore{};
        // Punches each hand had landed by the previous frame.
        bool punchStarted{};
        uint64_t punchesBefore[2]{};
        // Fighting across the room (web_strike.hpp): the blows landed and the
        // web strikes opened by the previous frame; the game's time around a
        // blow; the bursts still showing where the latest blows landed, each
        // since its display time; and the sounds, made once.
        uint64_t blowsBefore{}, strikesBefore{};
        HitStop hitStop;
        // The called blow's arrow over the thug it is on (web_strike.hpp's
        // Cues), since the display time it showed.
        CueView cue{};
        uint64_t cueTarget{};
        uint32_t cueWay{};
        bool cueLocked{};
        int64_t cueSince{};
        // The player's knockouts by the previous frame, and the lettering of
        // the latest, each since its display time.
        uint64_t knockoutsBefore{};
        struct Knockout {
            KnockoutView view;
            int64_t since{};
        };
        std::array<Knockout, 2> knockoutMarks{};
        size_t nextKnockout{};
        struct Burst {
            ImpactView view;
            int64_t since{};
        };
        std::array<Burst, 6> bursts{};
        size_t nextBurst{};
        // Spider-Sense's marks (spider_sense.hpp), one for each attacker the
        // game warned the player of: since the display time it showed, the
        // top of his head, what it asks for, its time left, when a counter
        // broke it, and what the sightline showed for him on the image before.
        // Then the counters made by the previous frame, and what the session
        // report counts.
        struct SenseMark {
            uint64_t attacker{};
            int64_t since{}, brokenAt{};
            Vec3 head{};
            SenseAnswer answer = SenseAnswer::Counter;
            float left = 1;
            bool timed{}, live{};
            SenseSight sight{};
        };
        std::array<SenseMark, 6> senseMarks{};
        uint64_t countersBefore{}, webDodgesBefore = UINT64_MAX;
        uint32_t senseShown{}, senseFrames{}, senseSightFrames{}, senseTingles{};
        uint32_t senseBroken{}, senseDodges{};
        // (They outlive the session: one may still be playing as it ends.)
        static const std::vector<uint8_t> sounds[5] = {
            impactWave(ImpactSound::Light), impactWave(ImpactSound::Firm), impactWave(ImpactSound::Heavy),
            impactWave(ImpactSound::Finisher), impactWave(ImpactSound::Rush)};
        // One at a time, the newest cutting the one before short: a strike's
        // rush ends in its blow.
        auto play = [&](ImpactSound sound) {
            const auto& wave = sounds[static_cast<size_t>(sound)];
            PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
        };
        // Web-shooter shots each hand had fired by the previous frame.
        bool shooterStarted{};
        uint64_t shotsBefore[2]{};
        uint64_t submittedFrames{};
        uint64_t previousPresentedGeneration{};
        // The pair FLIP REPROJECTION last turned, to count a pair shown again.
        uint64_t reprojectedGeneration{};
        // The player's standing eye height above the floor: it rises at once
        // to a higher head and sinks a centimetre a second, so crouching
        // does not shrink the body.
        float standingEye{};
        // Seated play (SEATED, seated.hpp): how far the tracking space is
        // raised from where the player sits, and the seated eye heights
        // measured by the last frame.
        seated::Lift seatLift;
        uint32_t seatMeasuresBefore{};
        // Each hand's fingers (HandPoser): the web-shooting hand as its web
        // button goes down and while the shot is on its way, else a fist,
        // closed by the grip, by a web held or by a hand moving fast relative
        // to the head (a punch); and where each grip was relative to the
        // head. What the swing's frame says of each hand for it: a web ball
        // left it, it holds the other hand's web (taken hold of, not shot),
        // its own web holds (attachedBefore); and an enemy is marked or
        // within a fist's reach.
        HandPoser fingers[2];
        Vec3 gripsBefore[2]{};
        bool gripsSeen[2]{};
        bool ballShot[2]{}, webTaken[2]{}, fightNear{};
        // When enemies were last within a strike's reach or a fist's
        // (performance counter; 0: never): FLIPS IN COMBAT.
        int64_t fightAt{};
        // Stretches and frames of play the game held the player on a wall
        // or a ceiling (the head stood off it).
        uint32_t surfaceEntries{};
        uint64_t surfaceFrames{};
        bool wasOnSurface{};
        // The surface the view last stood the player on, and for how long
        // since no hold has been reported: kept a moment while the game turns
        // the hero onto the wall or off it (`hold` below).
        SurfaceHold lastHold{};
        float holdGapSeconds{};
        // The player's size for the body: their T-pose calibration (the
        // launch's, or one made in this session), else the headset's running
        // height and the hero's own arms. A session with the body and without
        // a calibration asks for one at its first gameplay unless the player
        // skipped it before; the SPIDY VR tab asks again (CALIBRATE BODY: ON
        // RESUME).
        float calibratedEye = static_cast<float>(config.eyeHeightMm) / 1000;
        float calibratedArm = static_cast<float>(config.armLengthMm) / 1000;
        bool calibrationPrompt = !(config.options & 1024);
        // Reading the game's prompts and naming the VR control in them.
        const bool promptHelp = !(config.options & 0x100000);
        bool calibrationWanted = calibrationPrompt && !config.eyeHeightMm && values.body;
        body_calibration::Calibration calibration;
        body_calibration::Measurements lastCalibration{};
        // The panel's place in the tracking space, ahead of where the player
        // faced as it appeared; since when VR has shown play; when the result
        // appeared.
        Pose calibrationPanel{};
        uint64_t immersiveSince{}, calibrationShownAt{};
        // From the calibration's start until each is let go after it, the
        // triggers, grips, A and B do nothing in the game (B skips it).
        bool calibrationInput{}, skipHeld{};
        uint32_t calibrations{}, calibrationSkips{};
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        // Body haptics (body_haptics.hpp): a bHaptics vest, sleeves and visor,
        // played on a thread of their own (bhaptics_output.hpp), started the
        // first time BODY HAPTICS is on. What happens is posted where it
        // happens, beside the controllers' pulses (feel); what goes on, once a
        // frame. A fall in the hero's health is a blow, from where the attack
        // the game last warned of came (blowFrom, since the display time
        // blowSeen); landings come from the swing's samples.
        bhaptics::Output haptics;
        // feel: an event in its own place (a dodge's azimuth names only its
        // side); feelFrom: one that comes from a world direction, placed on the
        // vest by where the torso faces (Facing: it follows the head, the
        // room's turns at once), which the frame loop updates every frame.
        body_haptics::Facing facing;
        body_haptics::Body torso;
        const auto feel = [&](body_haptics::Kind kind, int hand = -1, float strength = 1, float azimuth = 0) {
            haptics.post({kind, static_cast<int8_t>(hand), strength, azimuth, .4f, false});
        };
        const auto feelFrom = [&](body_haptics::Kind kind, int hand, float strength, Vec3 from) {
            const bool placed = finite(from) && length(from) > 1e-3f;
            haptics.post({kind, static_cast<int8_t>(hand), strength, placed ? torso.azimuth(from) : 0.f,
                          placed ? torso.height(from) : .4f, placed});
        };
        // The groups the tab's WEB, WIND, COMBAT and MOVEMENT HAPTICS switches
        // leave on (body_haptics::State::groups).
        const auto hapticGroups = [&] {
            using body_haptics::Group;
            const auto bit = [](bool on, Group g) { return on ? 1u << static_cast<unsigned>(g) : 0u; };
            return bit(values.hapticWebs, Group::webs) | bit(values.hapticWind, Group::wind) |
                   bit(values.hapticCombat, Group::combat) | bit(values.hapticMovement, Group::movement);
        };
        body_haptics::LandingWatch landingWatch;
        body_haptics::HealthWatch heroHealth;
        Vec3 blowFrom{};
        int64_t blowSeen{};
        uint32_t hits{}, aimedHits{}, landings{};
        bhaptics::Status hapticsTold{};
        uint32_t devicesTold{};
        uint64_t hapticsSince{};
        // Slow motion: the left thumbstick's click (StickPress keeps it apart
        // from clicking both), its focus and the game's eased time
        // (SlowMotion). Each step sets the game's time, gives the controllers
        // a pulse at a change and reports it; without headset frames for a
        // while the loop below steps it to its end.
        SlowMotion slowMotion;
        // And the moments it slows by itself (a finisher, a called blow, a
        // thug pulled in the air, a perfect dodge).
        SlowMoments moments;
        StickPress slowPress;
        SlowMotionData slowCounts;
        uint64_t slowSteppedMs = GetTickCount64();
        auto stepSlowMotion = [&](float seconds, bool pressed, bool allowed) {
            using Event = SlowMotion::Event;
            const bool before = slowMotion.active();
            // SLOW-MO LENGTH: the menu may change it mid-session.
            slowMotion.setDrainSeconds(vr_settings::slowMotionDrainSeconds(values.slowMotionSeconds));
            const auto event = slowMotion.update(seconds, pressed, allowed);
            // A landed blow stops the game's time for a moment, in slow motion too.
            hitStop.update(seconds);
            if (!allowed)
                hitStop.reset();
            moments.update(seconds, allowed);
            game_time::slow(std::min({slowMotion.timeScale(), hitStop.timeScale(), moments.timeScale()}));
            slowSteppedMs = GetTickCount64();
            if (before && std::isfinite(seconds))
                slowCounts.seconds += std::clamp(seconds, 0.f, .1f);
            switch (event) {
            case Event::started:
                ++slowCounts.starts;
                runtime.haptic(0, .55f);
                runtime.haptic(1, .55f);
                feel(body_haptics::Kind::slowStart);
                break;
            case Event::stopped:
                ++slowCounts.stops;
                runtime.haptic(0, .3f);
                runtime.haptic(1, .3f);
                feel(body_haptics::Kind::slowStop);
                break;
            case Event::emptied:
                ++slowCounts.emptied;
                runtime.haptic(0, .8f);
                feel(body_haptics::Kind::slowStop);
                break;
            case Event::refused:
                ++slowCounts.refused;
                runtime.haptic(0, .6f);
                break;
            case Event::interrupted:
                ++slowCounts.interrupted;
                break;
            case Event::none:
                break;
            }
            const auto time = game_time::telemetry();
            std::lock_guard lock(telemetry);
            auto& d = SpidySlowMotionData;
            InterlockedIncrement64(&d.sequence);
            d.starts = slowCounts.starts;
            d.stops = slowCounts.stops;
            d.refused = slowCounts.refused;
            d.emptied = slowCounts.emptied;
            d.interrupted = slowCounts.interrupted;
            d.active = slowMotion.active();
            d.seconds = slowCounts.seconds;
            d.focus = slowMotion.focus();
            d.blend = slowMotion.blend();
            d.wanted = slowMotion.timeScale();
            d.gameScale = time.own;
            d.worldScale = time.world;
            d.physicsStep = time.physicsStep;
            d.physicsScaled = time.physicsScaled;
            d.updates = time.updates;
            d.slowedUpdates = time.slowed;
            for (unsigned i = 0; i < std::size(d.moments); ++i)
                d.moments[i] = moments.count(static_cast<Moment>(i));
            d.momentActive = moments.active();
            d.momentScale = moments.timeScale();
            InterlockedIncrement64(&d.sequence);
        };
        // Puts the settings into effect: at once, or when the module they
        // belong to starts (each start reads `values`).
        // What the swing is told during play: the settings, a hand-over's
        // walls, and the hands that land no blow: the phone's while it is out,
        // and both while the game names its X, whose press a thrown fist is
        // then (NamedPress) - a blow of Spidy's on top would hit as well.
        auto sendSwingSettings = [&] {
            // BLOW SOUNDS: the game's own impacts only on GAME.
            if (soundMode)
                soundMode(reinterpret_cast<void*>(static_cast<uintptr_t>(values.blowSounds)));
            // SOUND TRACE: diagnostics only, the hook on or off.
            if (soundTraceMode) {
                const DWORD traceError = soundTraceMode(values.soundTrace ? reinterpret_cast<void*>(1) : nullptr);
                diag_log::writef("sound trace: %s%s", values.soundTrace ? "on" : "off",
                                 traceError ? (" (error " + std::to_string(traceError) + ")").c_str() : "");
            }
            if (!swingStarted || !swingSettings)
                return;
            game_swing::Settings s;
            s.grab = values.webGrab && sampleGrab;
            s.maxSpeed = values.swingSpeed;
            s.airWebs = values.airWebs;
            s.gravity = vr_settings::gravity(values.weight);
            s.walls = !(config.options & 16384);
            s.arcade = values.arcadeWebs;
            s.handClimb = values.handClimb;
            s.webSlingshot = values.webSlingshot;
            s.betterClimbing = values.betterClimbing;
            s.combatVisuals = values.combatVisuals;
            s.smartAnchor = values.smartAnchor;
            s.elasticWeb = static_cast<uint32_t>(values.elasticWeb);
            s.softRelease = static_cast<uint32_t>(values.softRelease);
            s.swingAssist = values.swingAssist;
            s.slingshotSpeed = static_cast<float>(values.slingshotSpeed);
            s.twoHandZip = values.twoHandZip;
            s.ropeSteering = values.ropeSteering;
            s.webRange = static_cast<float>(values.webRange);
            s.webSpeed = static_cast<uint32_t>(values.webSpeed);
            s.halfPunchDamage = values.halfPunchDamage;
            s.singleHandYankPercent = static_cast<uint32_t>(values.singleHandYankPercent);
            s.webTie = values.webTie;
            s.airSlingshot = values.airSlingshot;
            s.momentumLeap = values.momentumLeap;
            s.softLanding = values.softLanding;
            s.spidyRun = values.spidyRun;
            // SPIDY RUN FROM STANDING: only with SPIDY RUN itself.
            s.spidyRunStanding = values.spidyRun && values.spidyRunFromStanding;
            s.runSpeedPercent = static_cast<uint32_t>(std::clamp(values.runSpeedPercent, 50, 200));
            s.softLockMetres = static_cast<uint32_t>(std::clamp(values.softLockMetres, 0, 40));
            s.webLandingSmooth = values.webLandingSmooth;
            s.webLines = values.webLines;
            s.headProbe = values.headProbe;
            s.animatedMoves = values.animatedMoves;
            s.animatedHead = static_cast<uint32_t>(values.animatedHead);
            s.animatedAir = values.animatedAir;
            s.animatedSteering = values.animatedSteering;
            s.animatedArms = values.animatedArms;
            s.animatedTorso = values.animatedTorso;
            s.animatedModelYaw = values.animatedModelYaw;
            s.webLandingImpact = static_cast<float>(values.webLandingImpact);
            s.webAngle = values.webAngle;
            s.wristShooters = values.palmShooters ? 2u : values.wristShooters ? 1u : 0u;
            sentSwingSettings = s;
            swingSettingsSent = true;
            // A hand-over owns the walls while it lasts; the player's own
            // setting is kept above and goes back when it ends.
            if (wallsHandedBack)
                s.walls = 0;
            phoneBlows = phone.live();
            fistsBlows = namedFists;
            s.noBlows = phoneBlows || fistsBlows ? 3u : 0u;
            if (const auto code = swingSettings(&s)) {
                const auto text = "VR settings: the swing did not take them (" + std::to_string(code) + ")";
                message(3, 0, text.c_str());
            }
        };
        // SESSION DIAGNOSTICS (diag_log.hpp): the build, and the feature
        // settings as the session starts and whenever they change, in the
        // console and the diagnostics file, so a log says what was tested.
        std::string diagSettings;
        // The watchdog (diag_log::Stages): started with RUN LOGS, once a session.
        bool watchdogStarted{};
        auto logSettings = [&] {
            if (!diag_log::enabled()) {
                diagSettings.clear();
                return;
            }
            std::string text = vr_settings::tabOnlyText(values);
            for (auto& c : text)
                if (c == '\n')
                    c = ' ';
            text += "swing_speed=" + std::to_string(values.swingSpeed) +
                    " weight=" + std::to_string(values.weight);
            if (text == diagSettings)
                return;
            const bool first = diagSettings.empty();
            diagSettings = text;
            if (first)
                diag_log::writef("build: %s", diag_log::build());
            // In pieces a line holds, each ending at a setting's end.
            constexpr size_t piece = 200;
            unsigned part = 0;
            for (size_t at = 0; at < text.size(); ++part) {
                size_t end = std::min(text.size(), at + piece);
                if (end < text.size()) {
                    const size_t space = text.rfind(' ', end);
                    if (space != std::string::npos && space > at)
                        end = space;
                }
                diag_log::writef("settings %s %u: %s", first ? "at start" : "changed", part + 1,
                                 text.substr(at, end - at).c_str());
                at = end + (end < text.size() && text[end] == ' ' ? 1 : 0);
            }
        };
        auto applySettings = [&] {
            // SESSION DIAGNOSTICS only with RUN LOGS on: off, no line is kept or
            // published and no watchdog runs (the review of pull request #77).
            diag_log::enable(values.runLogs);
            if (values.runLogs && !watchdogStarted) {
                // The watchdog holds its own handle to this worker and ends with it.
                HANDLE watched{};
                if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &watched,
                                    SYNCHRONIZE, FALSE, 0)) {
                    if (HANDLE dog = CreateThread(nullptr, 0, watchdog, watched, 0, nullptr)) {
                        CloseHandle(dog);
                        watchdogStarted = true;
                    } else {
                        CloseHandle(watched);
                    }
                }
            }
            logSettings();
            runtime.hapticStrength(static_cast<float>(values.haptics) / 100);
            // The eyes are read only while something aims with them.
            runtime.wantGaze(values.eyeAim != 0);
            // The bHaptics library loads, on the haptics' own thread, the first
            // time BODY HAPTICS is on; at 0 the devices go quiet.
            if (values.bodyHaptics > 0 && !haptics.started()) {
                bhaptics::Config felt;
                felt.library = bhaptics::libraryBeside();
                felt.appId = bhaptics::builtAppId();
                felt.apiKey = bhaptics::builtApiKey();
                haptics.start(felt);
                hapticsSince = GetTickCount64();
            }
            haptics.level(values.bodyHaptics);
            rig.snapTurn(static_cast<float>(values.snapTurn) * 3.14159265f / 180);
            rig.smoothTurn(static_cast<float>(values.smoothTurn) * 3.14159265f / 180);
            rig.flips(values.flips);
            rig.controlledFlips(values.controlledFlips);
            rig.flipSpeed(static_cast<float>(values.flipSpeed) * 3.14159265f / 180);
            rig.triggerWebs(values.triggerWebs);
            rig.standOnWalls(values.standOnWalls);
            rig.handClimb(values.handClimb);
            rig.joystickWallRunCamera(values.joystickRunCamera);
            rig.wallRunCameraTilt(values.wallRunCameraTilt);
            rig.wallRunCameraDistance(vr_settings::fixedRunCameraDistance * .01f);
            rig.momentumWallRun(vr_settings::fixedMomentumCameraDistance * .01f,
                vr_settings::fixedMomentumCameraTilt * .0174532925f,
                std::tan(vr_settings::fixedMomentumBodyTilt * .0174532925f),
                vr_settings::fixedMomentumForwardLean * .0174532925f);
            rig.wallDistance(static_cast<float>(vr_settings::fixedWallDistance) / 100.f);
            rig.automaticFlips(realisticFlips(values));
            native_hud::setSize(values.hud);
            native_gpu::wantHudLayer(values.steadyHud && values.hud > 0);
            near_clip::set(values.nearClipMm);
            recording_fov::set(values.recordingFov);
            recording_fov::smoothing(values.recordingSmoothing);
            sendSwingSettings();
            if (swingStarted && startPunch && samplePunch) {
                if (values.punch && !punchStarted) {
                    game_punch::Config punch;
                    punch.pid = config.pid;
                    punch.base = config.base;
                    punchStarted = !startPunch(&punch);
                } else if (!values.punch && punchStarted && stopPunch) {
                    stopPunch(nullptr);
                    punchStarted = false;
                }
            }
            if (swingStarted && startShooter && sampleShooter && stopShooter) {
                if (values.webShooter && !shooterStarted) {
                    game_shooter::Config shooter;
                    shooter.pid = config.pid;
                    shooter.base = config.base;
                    shooterStarted = !startShooter(&shooter);
                } else if (!values.webShooter && shooterStarted) {
                    stopShooter(nullptr);
                    shooterStarted = false;
                }
            }
            // Turned off, the body blends back to the game's pose (Command
            // flags); it is started only once.
            if (nativeStarted && values.body && !bodyStarted)
                bodyStarted = !native_body::start(config.base);
        };
        applySettings();
        auto controls = [&](uint32_t keys) {
            bridge::Control c;
            c.modes = bridge::Mode::input;
            c.serial = ++serial;
            c.leaseMs = 250;
            c.keys = keys;
            check(submitInput(&c), "Controller input");
        };
        // The game replaced its player or has none: every module that acts on
        // the player follows. A player gone again before this reaches the
        // modules is reported by the watch once more.
        auto attach = [&](const game_player::Player& next) {
            const auto follow = [&](const game_player::Player& p) {
                bridge::Config target;
                target.pid = config.pid;
                target.imageBase = config.base;
                target.hero = p.hero;
                target.actorRecord = p.record;
                if (retargetBridge(&target))
                    return false;
                if (!swingStarted)
                    return true;
                game_swing::Config swing;
                swing.pid = config.pid;
                swing.base = config.base;
                swing.record = p.record;
                swing.mover = p.mover;
                return !retargetSwing(&swing);
            };
            player = follow(next) ? next : game_player::Player{};
            if (!player.hero)
                follow({});
            native_appearance::setPlayerRecord(player.record);
            native_body::retarget(player.record);
            if (websStarted)
                native_webs::retarget(player.record);
            playersFound += player.hero != 0;
            // A respawn or another hero: his health is his own.
            heroHealth.reset();
            // The tracking space aligns with the new body's camera again.
            rig.reset();
            history.clear();
        };
        while (!stopRequested && (!nativeDeadline || GetTickCount64() < nativeDeadline)) {
            startupStages.beat(GetTickCount64());
            if (!config.durationMs && GetTickCount64() >= keepAliveDeadline)
                break;
            double copyMs{}, overlayMs{};
            Presentation drawn{};
            // This frame shows the game's presented frame on the screen, or
            // (stereoScene) the scene's own two eye images once there are some.
            bool showScreen{}, stereoScene{}, fpScene{}, fpPlayBlip{}, fpPending{}, levelView{};
            if (!steamGuarded && GetTickCount64() >= steamRetryMs) {
                const uint32_t code = game_steam::install(config.base);
                steamGuarded = !code;
                steamRetryMs = GetTickCount64() + 2000;
                if (code && code != 9801 && !steamWarned) {
                    steamWarned = true;
                    const auto text = "Quitting to the game's main menu may close the game: its Steam ticket "
                                      "call could not be guarded (" + std::to_string(code) + ")";
                    message(3, 0, text.c_str());
                }
            }
            if (!padInstalled && GetTickCount64() >= padRetryMs) {
                padInstalled = !game_pad::install();
                padRetryMs = GetTickCount64() + 1000;
            }
            // VR play has eye images; the game played on its screen has the
            // screen's, which these do not count. The last pair shown again
            // counts too: around a menu closing the game can make no new pair
            // for a few seconds while the headset shows the last one, which
            // ended VR (pull request #36). Only nothing shown at all does.
            const bool eyesWanted = motion.active && !flatScreen;
            if (eyesWanted && firstImageDeadline && !lastPresentedMs && GetTickCount64() >= firstImageDeadline)
                throw std::runtime_error("No native eye images reached OpenXR within 3 seconds");
            if (eyesWanted && lastPresentedMs && GetTickCount64() - lastPresentedMs >= 3000)
                throw std::runtime_error("Native eye presentation stalled for 3 seconds");
            // The tracking space's turn in the world for this frame's display
            // time, once the frame placed it (FLIP REPROJECTION).
            flip_reprojection::Turn turnNow{};
            const bool active = runtime.frameStereo(
                [&](const XrFrame& trackedFrame) {
                    XrFrame frame = trackedFrame;
                    LARGE_INTEGER workStart{};
                    QueryPerformanceCounter(&workStart);
                    // The phone takes its controls first (Phone::take, PhoneHold):
                    // while it is live the stick picks a button, A presses it, B
                    // puts it away and the triggers step the volume, and none of
                    // its controls reaches the swing or the game.
                    {
                        const auto use = phone.take(frame, phoneShown.gearCount);
                        const auto& e = use.controls;
                        if (e.x || e.y)
                            runtime.haptic(use.stick, .08f);
                        // PC MEDIA on: the buttons and the volume are the PC's player's.
                        PcMedia* const pc = values.pcMedia ? pcMedia.get() : nullptr;
                        if (e.volume) {
                            if (pc)
                                pc->setVolume(pc->status().volume + e.volume * phoneVolumeStep);
                            else if (!values.pcMedia)
                                phoneMusic.setVolume(phoneMusic.status().volume + e.volume * phoneVolumeStep);
                            runtime.haptic(e.volume < 0 ? 0 : 1, .1f);
                        }
                        switch (use.action) {
                        case PhoneAction::previous:
                            if (pc)
                                pc->previous();
                            else if (!values.pcMedia)
                                phoneMusic.previous();
                            break;
                        case PhoneAction::play:
                            if (pc)
                                pc->toggle();
                            else if (!values.pcMedia)
                                phoneMusic.toggle();
                            break;
                        case PhoneAction::next:
                            if (pc)
                                pc->next();
                            else if (!values.pcMedia)
                                phoneMusic.next();
                            break;
                        case PhoneAction::autoplay:
                            // PC MEDIA's button only names it: the PC's player
                            // plays on as it is set.
                            if (!values.pcMedia)
                                phoneMusic.setAutoplay(!phoneMusic.status().autoplay);
                            break;
                        case PhoneAction::camera:
                            cameraTap = cameraTapSeconds;
                            break;
                        case PhoneAction::menu:
                            menuTap = menuTapSeconds;
                            break;
                        case PhoneAction::none:
                        case PhoneAction::close:
                            break;
                        }
                        if (e.pressed || (e.closed && use.action != PhoneAction::close))
                            runtime.haptic(1, .2f);
                        if (use.action == PhoneAction::close) {
                            runtime.haptic(1, .15f);
                            message(3, 0, "Phone away - each control is the game's again once let go of");
                        } else if (use.action == PhoneAction::camera) {
                            message(3, 0, "The game's camera - left trigger aims, right trigger takes the photo, "
                                          "right stick up puts it away");
                        } else if (use.action == PhoneAction::menu) {
                            message(3, 0, "The game's menu (map, suits, skills) - B closes it");
                        }
                    }
                    uint64_t changes{};
                    if (const auto found = players.current(changes); changes != playerChanges) {
                        playerChanges = changes;
                        attach(found);
                    }
                    // Spider-Man, or a character the game gives the player in his
                    // place (Peter out of the suit, MJ, Miles): told by the skeleton
                    // of the player's entity, kept while there is none to read (a
                    // load, a spawn). That character plays with the game's own
                    // controls: no web, web ball, grab, yank, web strike, punch,
                    // wall, leap, gadget wheel or slow motion of Spidy's (November
                    // 5: MJ webbed up passers-by in her own mission).
                    if (uintptr_t entity{}; player.hero && read(player.record, &entity, sizeof(entity))) {
                        uint64_t asset{};
                        unsigned joints{};
                        if (game_fp::skeleton(entity, asset, joints) && asset != playerBody) {
                            playerBody = asset;
                            spiderMan = !game_fp::otherCharacter(asset);
                            char line[160];
                            std::snprintf(line, sizeof(line),
                                          "Player character: body %016llX (%u joints), %s",
                                          static_cast<unsigned long long>(asset), joints,
                                          spiderMan ? "Spider-Man" : "not Spider-Man: the game's controls");
                            message(3, 0, line);
                        }
                    }
                    bridge::Data game{};
                    check(sampleBridge(&game), "Gameplay sample");
                    LARGE_INTEGER now{};
                    QueryPerformanceCounter(&now);
                    // A fight goes on 3 s past the last enemy in reach, so
                    // the realistic flips that FLIPS IN COMBAT keeps out of
                    // it do not come back between blows.
                    const bool fightLately = fightAt && now.QuadPart - fightAt < frequency.QuadPart * 3;
                    Mat4 body{}, camera{};
                    std::memcpy(body.data(), game.player, 64);
                    std::memcpy(camera.data(), game.before, 64);
                    // Gameplay: the player's own follow or combat camera
                    // committed within 100 ms (game_bridge.cpp).
                    const bool bridgeRunning = game.state == 1;
                    const bool committed = game.qpc && now.QuadPart >= static_cast<int64_t>(game.qpc) &&
                                           static_cast<double>(now.QuadPart - game.qpc) / frequency.QuadPart < .1;
                    const bool followsPlayer = native_view::validPose(body) && native_view::validPose(camera);
                    const bool gameplay = player.hero && bridgeRunning && committed && followsPlayer;
                    uint64_t moverTable{};
                    read(game.mover, &moverTable, sizeof(moverTable));
                    // The headset is worn, focused and tracked: an image can be shown.
                    const bool viewing = frame.focused && frame.valid;
                    displayTime = frame.predictedDisplayTime;
                    // The game as it is, on its screen, instead of VR: the
                    // SPIDY VR tab's VIEW (a click of both thumbsticks until
                    // October 12). Its gameplay is the game screen's, in 3D
                    // as scenes are there, the controllers an Xbox
                    // controller's, and nothing of Spidy's on the hero.
                    if (values.flatScreen != flatScreen && viewing) {
                        flatScreen = values.flatScreen;
                        rig.reset();
                        history.clear();
                        lastPresentedMs = lastNewImageMs = 0;
                        presented = false;
                        firstImageDeadline = GetTickCount64() + 3000;
                        // Chosen in the pause menu, the game's screen is the
                        // menu's own, where it stands.
                        if (!wasScreen)
                            screenPose = screenAhead(frame.head);
                        std::lock_guard lock(telemetry);
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        ++SpidyXrData.toggles;
                        SpidyXrData.flatScreen = flatScreen;
                        InterlockedIncrement64(&SpidyXrData.sequence);
                    }
                    // POINT JUMP (point_jump.hpp; the user, November 11): touching a
                    // surface, A+B together (or hold B, then A) charges a jump the way
                    // he looks. Here, before anything reads A or B this frame: what of
                    // them reaches the game and the rest of the mod (a button waiting
                    // for the other at most the chord window; both swallowed while the
                    // chord is held), the charge's rumble, and the launch sent to the
                    // swing (SpidySwingPointJump). Off (the default), A and B pass as
                    // they are.
                    // FLAIL (leg_layers.hpp): in the air with Y+X physically held, B is
                    // the legs' - taken here, before POINT JUMP's A+B, the dodge and the
                    // tuck read it - until it is let go of.
                    {
                        const bool flailAllowed = values.legControl &&
                                                  (values.controlledFlips || values.realisticFlips) && values.body &&
                                                  !flatScreen && viewing && gameplay && motion.active && spiderMan &&
                                                  !calibrationInput && !phone.open() && !padLayer.latched();
                        const auto flail = flailGate.update(flailAllowed, legsAirLast, (frame.buttons & buttonX) != 0,
                                                            (frame.buttons & buttonY) != 0,
                                                            (frame.buttons & buttonB) != 0);
                        if (flail.swallowB)
                            frame.buttons &= ~static_cast<uint32_t>(buttonB);
                        flailNow = flail.flail;
                        if (flail.started)
                            diag_log::write("legs: flail B taken (Y+X held in the air): no dodge, tuck or point jump");
                        if (flail.ended)
                            diag_log::write("legs: flail B let go of");
                    }
                    pointJumpFired = false;
                    {
                        const auto pj = point_jump::paramsOf(values);
                        const bool pjFresh = swingState.status && swingState.qpc &&
                                             static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10;
                        const bool pjSpidyWall = swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall);
                        const bool pjContact = pjFresh && (swingState.grounded || pjSpidyWall);
                        if (pjContact) {
                            pointJumpNormal = pjSpidyWall && finite(swingState.wallNormal) &&
                                                      length(swingState.wallNormal) > .5f
                                                  ? normalized(swingState.wallNormal)
                                                  : Vec3{0, 1, 0};
                            pointJumpSurface = pjSpidyWall ? "wall" : "ground";
                        }
                        // Not while something else has A or B: out of play, the phone,
                        // the gadget wheel, the pad layer, the calibration, a prompt
                        // naming A or B, a vent, a web line, the game's own crawl (a later
                        // milestone), a takeoff under way.
                        const char* blockedBy = nullptr;
                        if (!swingPointJump)
                            blockedBy = "no swing export";
                        else if (!(viewing && gameplay && motion.active && !flatScreen && spiderMan &&
                                   recentPresentation(lastNewImageMs, GetTickCount64())))
                            blockedBy = "not in play";
                        else if (phone.open() || gadgetWheel.open())
                            blockedBy = "phone or wheel";
                        else if (padLayer.latched())
                            blockedBy = "pad layer";
                        else if (calibrationInput)
                            blockedBy = "calibration";
                        else if (namedPress.claimed() &
                                 (game_pad::NamedPress::controlA | game_pad::NamedPress::controlB))
                            blockedBy = "prompt";
                        else if (motion.ventCrawl)
                            blockedBy = "vent";
                        else if (values.webLines && lineState.phase == 2 && lineState.onLine)
                            blockedBy = "line";
                        else if (pjFresh && (swingState.wall == static_cast<uint32_t>(game_swing::Surface::game) ||
                                             swingState.wall == static_cast<uint32_t>(game_swing::Surface::floor)))
                            blockedBy = "game crawl";
                        else if (pjFresh && (swingState.takeoff || swingState.takeoffPhase))
                            blockedBy = "takeoff";
                        point_jump::Controller::Input gateIn;
                        gateIn.seconds = frame.seconds;
                        gateIn.contact = pjContact;
                        gateIn.blocked = blockedBy != nullptr;
                        gateIn.fighting = fightLately;
                        gateIn.zipping = zipping;
                        const bool allowed = pointJumpGate.update(gateIn, pj);
                        const bool warning = std::any_of(senseMarks.begin(), senseMarks.end(),
                                                         [](const SenseMark& m) { return m.live && !m.brokenAt; });
                        const bool rawA = frame.jump, rawB = (frame.buttons & buttonB) != 0;
                        // A grip cancels a charge.
                        if (pointChord.charging() &&
                            ((frame.hands[0].valid && frame.hands[0].squeeze > .6f) ||
                             (frame.hands[1].valid && frame.hands[1].squeeze > .6f)))
                            pointChord.cancel();
                        point_jump::Chord::Input chordIn;
                        chordIn.seconds = frame.seconds;
                        chordIn.a = rawA;
                        chordIn.b = rawB;
                        chordIn.allowed = allowed;
                        chordIn.warning = warning;
                        chordIn.mode = pj.trigger;
                        chordIn.window = pj.chordSeconds;
                        chordIn.dodgeHold = pj.dodgeHoldSeconds;
                        const auto chord = pointChord.update(chordIn);
                        frame.jump = chord.a;
                        frame.buttons = chord.b ? (frame.buttons | buttonB) : (frame.buttons & ~uint32_t(buttonB));
                        // Both pressed with the jump not allowed: why (the buttons went on as they were).
                        const bool both = rawA && rawB;
                        if (pj.enabled && both && !pointJumpBothWas && !allowed && !chord.began &&
                            !pointChord.charging()) {
                            ++pointJumpDenied;
                            diag_log::writef("point jump: denied %s (A and B went on to the game)",
                                             blockedBy ? blockedBy : point_jump::denyName(pointJumpGate.deny()));
                        }
                        pointJumpBothWas = both;
                        if (chord.began) {
                            // The chord took the press: whatever A was doing ends unsent.
                            chargeJump.consume();
                            pointJumpCharged = false;
                            diag_log::writef("point jump: chord %s %d ms (trigger %d, %s%s)",
                                             pj.trigger == 1 ? "B-then-A" : chord.aFirst ? "A-first" : "B-first",
                                             static_cast<int>(chord.gap * 1000 + .5f), pj.trigger, pointJumpSurface,
                                             pointJumpGate.deny() == point_jump::Deny::zipGuard ? ", zip guard" : "");
                        }
                        if (pointChord.charging()) {
                            // The charged leap's rumble: a pulse past the tap, then it builds and holds.
                            const float held = pointChord.held();
                            if (held > pj.tapSeconds) {
                                const float c = point_jump::charge(held, pj);
                                // MOVE HAPTICS' strength, not CONTROLLER VIBRATION's.
                                const float moveScale = static_cast<float>(values.moveHapticsPct) / 100;
                                const auto buzz = [&](int hand, float amplitude) {
                                    const auto p = move_haptics::scaled({hand, amplitude, .025f}, moveScale);
                                    if (p.amplitude > 0)
                                        runtime.hapticUnscaled(hand, p.amplitude, p.seconds);
                                };
                                if (!pointJumpCharged) {
                                    pointJumpCharged = true;
                                    buzz(1, .3f);
                                    feel(body_haptics::Kind::leapCharge);
                                } else {
                                    buzz(0, .08f + .3f * c);
                                    buzz(1, .1f + .4f * c);
                                }
                            }
                        }
                        if (chord.cancelled)
                            diag_log::writef("point jump: charge cancelled (grip) after %d ms",
                                             static_cast<int>(pointChord.held() * 1000 + .5f));
                        if (chord.released)
                            pointJumpGate.release(chord.held);
                        const auto fire = pointJumpGate.take(pj);
                        if (fire.now) {
                            Vec3 look = pointJumpLook;
                            if (!finite(look) || length(look) < .5f)
                                look = motion.headPose.orientation.rotate({0, 0, -1});
                            const auto way = point_jump::direction(look, pointJumpNormal, pj);
                            const float speedNow = point_jump::speed(fire.held, pj);
                            const Vec3 before = pjFresh && finite(swingState.velocity) ? swingState.velocity : Vec3{};
                            const Vec3 velocity =
                                point_jump::launchVelocity(way.way, speedNow, before, pointJumpNormal, pj);
                            game_swing::PointJumpAsk ask;
                            ask.serial = ++pointJumpSerial;
                            ask.velocity = velocity;
                            ask.normal = pointJumpNormal;
                            const DWORD result = swingPointJump(&ask);
                            if (result == 0) {
                                ++pointJumps;
                                pointJumpFired = pointJumpHold = true;
                                pointJumpHoldAge = 0;
                                pointJumpOffLogged = false;
                                // MOVE HAPTICS: the point jump's double thump, at its
                                // own strength (the second thump 110 ms on).
                                {
                                    const float moveScale = static_cast<float>(values.moveHapticsPct) / 100;
                                    const auto first = move_haptics::scaled(move_haptics::pointFirst(), moveScale);
                                    if (first.amplitude > 0) {
                                        runtime.hapticUnscaled(0, first.amplitude, first.seconds);
                                        runtime.hapticUnscaled(1, first.amplitude, first.seconds);
                                        moveScheduler.add(move_haptics::pointGap,
                                                          move_haptics::scaled(move_haptics::pointSecond(), moveScale));
                                        ++moveHapticPulses;
                                    }
                                    moveDetector.pointJump();
                                    diag_log::writef("move haptics: point jump %.1f m/s -> %.2f x2 (move haptics %d%%)",
                                                     speedNow, first.amplitude, values.moveHapticsPct);
                                }
                                feel(body_haptics::Kind::leap);
                                diag_log::writef(
                                    "point jump: charge %.2f -> %.1f m/s dir (%.2f, %.2f, %.2f) n (%.2f, %.2f, %.2f) "
                                    "out-fix %.0f deg, held %d ms%s, launch (%.1f, %.1f, %.1f), from the %s",
                                    point_jump::charge(fire.held, pj), speedNow, way.way.x, way.way.y, way.way.z,
                                    pointJumpNormal.x, pointJumpNormal.y, pointJumpNormal.z, way.outFixDegrees,
                                    static_cast<int>(fire.held * 1000 + .5f),
                                    fire.waited > .001f ? ", waited for the surface or the zip guard" : "",
                                    velocity.x, velocity.y, velocity.z, pointJumpSurface);
                                // FACING CHECK: what the headset showed, and the launch way
                                // when the player launched roughly where they looked.
                                {
                                    float before = NAN;
                                    if (!hybridShown.valid || !hybrid_handoff::shownYaw(hybridShown, before))
                                        before = NAN;
                                    if (!std::isfinite(before) && motion.active)
                                        before = hybrid_handoff::lookYaw(
                                            motion.headPose.orientation.rotate({0, 0, -1}));
                                    const float launchYaw =
                                        std::hypot(velocity.x, velocity.z) > 1.f
                                            ? hybrid_handoff::lookYaw({velocity.x, 0, velocity.z})
                                            : NAN;
                                    facingCheck.arm(facing_check::Event::pointLaunch, before,
                                                    facing_check::launchExpected(before, launchYaw));
                                }
                            } else {
                                ++pointJumpRefused;
                                diag_log::writef("point jump: the swing refused the launch (%lu)",
                                                 static_cast<unsigned long>(result));
                            }
                        } else if (fire.dropped) {
                            ++pointJumpDenied;
                            diag_log::writef("point jump: denied %s (let go of after %d ms)",
                                             point_jump::denyName(fire.why),
                                             static_cast<int>(fire.held * 1000 + .5f));
                        }
                        // The takeoff: held for the owner until he is off the surface.
                        if (pointJumpHold && !pointJumpFired) {
                            pointJumpHoldAge += std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            const bool off = pjFresh && !swingState.grounded &&
                                             swingState.wall == static_cast<uint32_t>(game_swing::Surface::none);
                            if (off && !pointJumpOffLogged) {
                                pointJumpOffLogged = true;
                                diag_log::writef("point jump: off the surface %d ms after the launch",
                                                 static_cast<int>(pointJumpHoldAge * 1000 + .5f));
                            }
                            if (off || pointJumpHoldAge > .4f) {
                                if (!off)
                                    diag_log::write("point jump: still on the surface 400 ms after the launch "
                                                    "(the game kept him there)");
                                pointJumpHold = false;
                            }
                        }
                    }
                    // LEG HOLD (leg_hold.hpp; the user, November 12 01:33, t67u): Y+X
                    // held engage LEG CONTROL's legs, before anything reads X, Y or the
                    // right stick this frame. From the chord until both are let go of,
                    // neither reaches the game or the mod (no web strike / zip, no gadget
                    // wheel); one alone waits up to 80 ms for the other. Let go, the legs
                    // stay while the right stick is moved; neutral ends them on that
                    // frame. Never the left stick's click: that is slow motion.
                    {
                        const bool master = values.legControl && (values.controlledFlips || values.realisticFlips) &&
                                            values.body && !flatScreen && viewing && gameplay && motion.active &&
                                            spiderMan && !calibrationInput && !phone.open() && !padLayer.latched();
                        leg_hold::Hold::Input in;
                        in.seconds = frame.seconds;
                        in.x = (frame.buttons & buttonX) != 0;
                        in.y = (frame.buttons & buttonY) != 0;
                        in.stickX = frame.hands[1].stickX;
                        in.stickY = frame.hands[1].stickY;
                        in.master = master;
                        // A chord may start where the legs could move (last frame's finding),
                        // with no prompt naming X or Y and the wheel shut.
                        in.allowed = master && (legBaseLast || legChordAirAnimLast) && !gadgetWheel.open() &&
                                     !(namedPress.claimed() &
                                       (game_pad::NamedPress::controlX | game_pad::NamedPress::controlY));
                        legHoldOut = legHold.update(in);
                        frame.buttons = (frame.buttons & ~static_cast<uint32_t>(buttonX | buttonY)) |
                                        (legHoldOut.x ? static_cast<uint32_t>(buttonX) : 0u) |
                                        (legHoldOut.y ? static_cast<uint32_t>(buttonY) : 0u);
                        frame.reset = legHoldOut.y;
                        if (legHoldOut.engagedNow) {
                            runtime.haptic(0, .2f);
                            diag_log::writef("legs: engaged (%s)", leg_hold::whyName(legHoldOut.why));
                        }
                        if (legHoldOut.disengagedNow)
                            diag_log::writef("legs: disengaged (%s)", leg_hold::whyName(legHoldOut.why));
                    }
                    if (!frame.jump)
                        jumpFromScreen = false;
                    else if (wasScreen)
                        jumpFromScreen = true;
                    XrFrame controller = frame;
                    // The T-pose calibration has A as it has the other buttons.
                    controller.jump = frame.jump && !jumpFromScreen && !calibrationInput;
                    // Seated play: how far everything the game's world takes from
                    // the tracking space is raised - the rig's head, eyes and hands,
                    // the body's height, a T-pose calibration - measured from where
                    // the player sits in VR play, again after a recenter.
                    const float seatStanding = seated::standingEye(values.heightCm, calibratedEye, seated::defaultEye);
                    const float seatRaise =
                        seatLift.update(frame.seconds, values.seated,
                                        viewing && gameplay && !flatScreen && validTrackedPose(frame.head),
                                        frame.head.position.y, frame.recentered, seatStanding);
                    if (seatLift.measures() != seatMeasuresBefore) {
                        seatMeasuresBefore = seatLift.measures();
                        char line[128];
                        sprintf_s(line, "Seated: your eyes are %.2f m up; raised %.2f m to %.2f m (HEIGHT %s)",
                                  seatLift.seatedEye(),
                                  std::clamp(seatStanding - seatLift.seatedEye(), 0.f, seated::Lift::maxLift),
                                  seatStanding, values.heightCm ? "set" : "AUTO");
                        message(3, 0, line);
                    }
                    // Each hand's grip pose, and whether the headset has it; and
                    // whether the hand gestures play VR: gameplay in the headset,
                    // the controllers no calibration's.
                    const Pose grips[2]{frame.hands[0].grip, frame.hands[1].grip};
                    const bool gripsTracked[2]{frame.hands[0].valid && validTrackedPose(frame.hands[0].grip),
                                               frame.hands[1].valid && validTrackedPose(frame.hands[1].grip)};
                    const bool gesturesPlay = viewing && gameplay && !flatScreen && !calibrationInput &&
                                              validTrackedPose(frame.head);
                    // A trigger pulled with its hand at the temple is the game's
                    // scan, and no web ball or reel meanwhile.
                    {
                        const float triggers[2]{frame.hands[0].trigger, frame.hands[1].trigger};
                        templePress.update(frame.seconds, gesturesPlay, frame.head, grips, gripsTracked,
                                           triggers);
                        for (unsigned i = 0; i < 2; ++i)
                            if (templePress.hands() & (1u << i))
                                controller.hands[i].trigger = 0;
                        if (templePress.scans() != scansBefore) {
                            scansBefore = templePress.scans();
                            runtime.haptic(0, .3f);
                            runtime.haptic(1, .3f);
                            feel(body_haptics::Kind::scan);
                        }
                    }
                    // Both hands at the chest with both triggers pulled is the
                    // game's suit power (its L3 + R3, which VR play had no way
                    // to), and those triggers shoot nothing meanwhile. A hand
                    // whose web is out reels with its trigger instead.
                    {
                        const float triggers[2]{frame.hands[0].trigger, frame.hands[1].trigger};
                        const bool webs[2]{swingState.webs[0].attached != 0,
                                           swingState.webs[1].attached != 0};
                        suitPress.update(frame.seconds, gesturesPlay, frame.head, grips, gripsTracked,
                                         triggers, webs);
                        for (unsigned i = 0; i < 2; ++i)
                            if (suitPress.hands() & (1u << i))
                                controller.hands[i].trigger = 0;
                        if (suitPress.powers() != suitPowersBefore) {
                            suitPowersBefore = suitPress.powers();
                            runtime.haptic(0, .9f, .12f);
                            runtime.haptic(1, .9f, .12f);
                            feel(body_haptics::Kind::leap);
                            game_prompts::note("both hands at the chest, both triggers -> the game's L3 + R3 (its suit "
                                               "power)");
                        }
                    }
                    // A grip squeezed at a back pocket draws the phone, and shoots
                    // no web until it is let go. Not with the phone out. In the pad
                    // layer too, where the grips are the game's bumpers: that grip
                    // is no bumper (October 19: a draw sent the game LB or RB until
                    // the phone came out), and the phone drawn ends the layer
                    // (PadLayer::leave), so the phone is always the way back to
                    // play's controls.
                    int phoneDrawnWith = -1;
                    {
                        const float squeezes[2]{frame.hands[0].squeeze, frame.hands[1].squeeze};
                        const bool drawPlay = gesturesPlay && !phone.open();
                        pocketDraw.seated(values.seated);
                        if (pocketDraw.update(frame.seconds, drawPlay, frame.head, grips, gripsTracked,
                                              squeezes))
                            phoneDrawnWith = pocketDraw.drewWith();
                        if (phoneDrawnWith >= 0 && padLayer.latched()) {
                            padLayer.leave();
                            // "Phone out" says it; "Pad layer off" would hide it.
                            padLayerWas = false;
                        }
                        for (unsigned i = 0; i < 2; ++i)
                            if (pocketDraw.hands() & (1u << i))
                                controller.hands[i].squeeze = 0;
                    }
                    // The buttons the game names in a prompt, pressed with the
                    // controls of those names (game_pad::NamedPress). A control
                    // it claims is the game's until it is let go, and does
                    // nothing of Spidy's meanwhile: no web ball or reel from that
                    // hand's web-ball button, no jump or flip from A, no turn
                    // from the right stick pushed for the game's D-pad.
                    {
                        const auto asked = game_prompts::demand();
                        game_pad::NamedPress::Input named;
                        named.buttons = frame.buttons;
                        // The temple's scan has a trigger, the pocket's draw a grip.
                        const uint32_t gestures =
                            values.triggerWebs ? pocketDraw.hands() : templePress.hands() | suitPress.hands();
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            named.balls[i] = values.triggerWebs ? hand.squeeze : hand.trigger;
                            named.ballsTaken[i] = (gestures & (1u << i)) != 0;
                            named.clicks[i] = hand.stickClick;
                            named.hands[i] = hand.grip.position - frame.head.position;
                            named.tracked[i] = gripsTracked[i] && validTrackedPose(frame.head);
                            named.busy[i] = swingState.webs[i].attached != 0;
                        }
                        named.stickX = frame.hands[1].stickX;
                        named.stickY = frame.hands[1].stickY;
                        named.airborne = game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart);
                        named.slams = fightLately && (!values.realisticFlips || !values.automaticCombat);
                        const bool namedPlay =
                            gesturesPlay && !padLayer.latched() && !phone.open() && !gadgetWheel.open();
                        namedPad = namedPress.update(frame.seconds, namedPlay, named, asked);
                        const uint32_t claimed = namedPress.claimed();
                        if (claimed & game_pad::NamedPress::controlA) {
                            controller.jump = false;
                            controller.buttons &= ~static_cast<uint32_t>(buttonA);
                        }
                        for (unsigned i = 0; i < 2; ++i) {
                            auto& hand = controller.hands[i];
                            if (claimed & game_pad::NamedPress::ball(i))
                                (values.triggerWebs ? hand.squeeze : hand.trigger) = 0;
                        }
                        if (claimed & (game_pad::NamedPress::stickLeft | game_pad::NamedPress::stickRight))
                            controller.hands[1].stickX = 0;
                        if (values.flips && !values.controlledFlips && !values.realisticFlips &&
                            (claimed & game_pad::NamedPress::stickUp))
                            controller.hands[1].stickY = 0;
                        // While the game names its X a thrown fist is that X, and
                        // the swing lands no blow of its own (sendSwingSettings).
                        namedFists = namedPlay && (asked.buttons & game_pad::x) != 0;
                    }
                    // The player's up (its actor's second row): a wall's normal
                    // while the game holds it on one. Spidy's own flight is on none.
                    const Vec3 heroUp{body[4], body[5], body[6]};
                    // The wall that holds the player, as the latest swing sample
                    // has it (no older than a tenth of a second): the swing's own
                    // wall, its floor on the wall beside the body's centre, or
                    // the game's own crawl, whose actor has the feet on the
                    // surface itself.
                    //
                    // Both of them, since October 14. The crawl was held back
                    // while how it takes the stick went unmeasured, and that
                    // left the one state whose interacts work on a wall - the
                    // only one the game calls supported there - looking nothing
                    // like the wall walking the player asked to keep, so a vent
                    // cost him the view. The rig's own crawl path was built and
                    // tested the whole time; only this passed it nothing.
                    const Vec3 feet{body[12], body[13], body[14]};
                    SurfaceHold hold;
                    const bool swingFresh =
                        swingState.status && swingState.qpc &&
                        static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10;
                    if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall)) {
                        // The wall's face under the feet, in the wall's own frame
                        // (its distance is the body's centre's, a metre over the
                        // feet): the same floor the game's crawl stands the hero
                        // on, so going from the one hold to the other moves no one.
                        hold.normal = swingState.wallNormal;
                        hold.anchor = feet - hold.normal * (swingState.wallDistance - hold.normal.y);
                        hold.speed = length(swingState.velocity);
                    } else if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::game)) {
                        // The crawl's feet are on the surface, so they are the
                        // floor under the player, and its normal comes from a
                        // ray where the swing has one. The actor's own up is
                        // that surface while it rocks on it (up to 14 degrees
                        // off over a few frames, reports/wall-crawl.json) and
                        // serves where the ray found nothing. In the game's own
                        // wall run the feet are off the wall by the distance
                        // the swing measured: the floor is the wall under them.
                        hold.normal = finite(swingState.wallNormal) && length(swingState.wallNormal) > .5f
                                          ? normalized(swingState.wallNormal)
                                          : normalized(heroUp);
                        hold.anchor = feet - hold.normal * (std::isfinite(swingState.wallDistance)
                                                                ? std::clamp(swingState.wallDistance, 0.f, 1.5f)
                                                                : 0.f);
                        hold.speed = length(swingState.velocity);
                        hold.crawl = true;
                        ++crawlFrames;
                    } else if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::floor)) {
                        // The game's crawl on a floor: a vent, a duct. The
                        // floor is under the feet and level, whatever the
                        // crawling actor rocks to: the view stands as it
                        // stands, the body plants its feet on it (the game's
                        // crawl laid the legs out in front of the player in
                        // the first mission's duct, October 14 19:57), and
                        // the head stays under its roof (below).
                        hold.normal = {0, 1, 0};
                        hold.anchor = feet;
                        hold.speed = length(swingState.velocity);
                        hold.crawl = true;
                        ++floorCrawlFrames;
                    }
                    hold.vent = hold.crawl &&
                        (climbState.status && climbState.crawl == 1 && climbState.sampleTimeNs &&
                         frame.predictedDisplayTime > 0 &&
                         static_cast<uint64_t>(frame.predictedDisplayTime) >= climbState.sampleTimeNs &&
                         static_cast<uint64_t>(frame.predictedDisplayTime) - climbState.sampleTimeNs <=
                             100'000'000);
                    // The game's crawl on a floor keeps the head under the
                    // roof (below) whether or not the vent view takes it.
                    const bool gameFloorCrawl = hold.crawl && hold.normal.y > .99f;
                    // The gate keeps transient floor crawls out of the vent
                    // view: with vents off for now (ventHandCrawl) there is none.
                    const bool ventView = ventHandCrawl && gameplay && values.handClimb;
                    hold = crawlCameraGate.update(frame.seconds, ventView,
                        (origins.play() & buttonB) != 0, swingFresh && swingState.grounded, hold);
                    const bool crawlsFloor = hold.crawl && hold.normal.y > .99f;
                    const float crawlCap = floorRoof.update(gameFloorCrawl, frame.seconds, headroomSeen);
                    // Between the swing's wall and the game's crawl (game_warp:
                    // the game turning the hero onto the wall at rest, or off it
                    // as he walks on) the swing reports neither for a moment.
                    // The view keeps the surface it stood him on while the body
                    // stays at its floor, slower along it than a walk: dropped,
                    // the view turned back toward level and onto the wall again
                    // (48 degrees in a tenth of a second, October 14 16:48). A
                    // jump or a run off the wall leaves it at once.
                    {
                        constexpr float holdGrace = .4f, holdGraceSpeed = .8f;
                        const float seconds =
                            std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                        const Vec3 n = lastHold.normal;
                        const Vec3 v = finite(swingState.velocity) ? swingState.velocity : Vec3{};
                        const bool held = length(hold.normal) > .5f;
                        holdGapSeconds = held ? 0.f : holdGapSeconds + seconds;
                        if (held) {
                            lastHold = hold;
                        } else if ((motion.standing || motion.ventCrawl) && length(n) > .5f &&
                                   holdGapSeconds < holdGrace &&
                                   std::abs(dot(feet - lastHold.anchor, n)) < 1.5f &&
                                   length(v - n * dot(v, n)) < (motion.ventCrawl ? 3.f : holdGraceSpeed)) {
                            // On the plane it stood on, with the feet: kept
                            // where it was while the game took a player over
                            // a shaft's lip at 1.3 m/s, the view stayed on
                            // the wall a third of a second after he had left
                            // it and then jumped to him (October 14, 86 s).
                            // Only for a player at rest: one on his way off
                            // the surface is on his way off it.
                            hold = lastHold;
                            hold.anchor = feet - n * dot(feet - lastHold.anchor, n);
                            ++holdGraceFrames;
                        } else {
                            lastHold = {};
                        }
                    }
                    // A or the left stick in the air flips the player (FlipMotion),
                    // in the air as the latest swing sample has it.
                    // In the game's crawl in a duct the head goes no higher
                    // than a crouch, whatever a ray finds over it: part of the
                    // first mission's duct has a roof no ray hits, and the
                    // head stood out through it at the player's own height.
                    // A duct is the crawl under a duct's roof (FloorCrawlRoof):
                    // the game crawls a hero ducking under a beam the same way.
                    float over = headroomSeen;
                    if (crawlCap > 0) {
                        over = over > 0 ? std::min(over, crawlCap) : crawlCap;
                        ++crawlCapFrames;
                    }
                    rig.headroom(over);
                    if (headroomSeen > 0 && (headroomLeast == 0 || headroomSeen < headroomLeast))
                        headroomLeast = headroomSeen;
                    // The eyes out of what the world is built of (HeadClearance): from a point in
                    // the body, where no wall is - half a metre off the wall that holds him, or
                    // a metre up from his feet along his up (the world's while Spidy's swing
                    // has him, which the game keeps upright in its air state) - and the
                    // surfaces the head's rays met in the batch the bridge last finished,
                    // facing them and of what the world is built of (a car, a crate or a
                    // passer-by moves, and is no wall).
                    {
                        const Vec3 held = normalized(hold.normal);
                        const Vec3 bodyUp = swingState.owned ? Vec3{0, 1, 0} : normalized(heroUp);
                        const bool holds = finite(held) && length(held) > .5f && finite(hold.anchor);
                        // A thick corner pillar can cover the old half-metre
                        // point. Start climbing camera rays inside the actual
                        // native capsule so its front cannot be ignored.
                        rig.bodyCentre(
                            headCollisionOrigin(feet, bodyUp, hold, values.handClimb && swingState.owned));
                        if (holds)
                            rig.surfaceFound({hold.anchor, held}, true);
                        if (rayResults) {
                            // The bridge publishes on another thread. Hand
                            // contacts consume each complete, fresh batch once.
                            const auto sequence = rayResults->sequence;
                            if (!(sequence & 1)) {
                                MemoryBarrier();
                                const native_rays::Data sample = *rayResults;
                                MemoryBarrier();
                                if (rayResults->sequence == sequence)
                                    rig.handFeedback(sample, now.QuadPart, frequency.QuadPart);
                            }
                        }
                        if (rayResults)
                            for (unsigned i = 0; i < native_rays::capacity && i < rayResults->count; ++i) {
                                const auto& hit = rayResults->hits[i];
                                const Vec3 n = normalized(hit.normal);
                                if (hit.ray.tag >= 2 && hit.ray.tag <= 6 && native_rays::fixedSurface(hit) &&
                                    std::isfinite(hit.fraction) && hit.fraction < 1 && finite(n) &&
                                    dot(n, hit.ray.direction) < -.2f)
                                    rig.surfaceFound({hit.position, n});
                            }
                    }
                    // REALISTIC FLIPS turn with the swing's flight: its
                    // velocity, and while it has the player each web's pull
                    // toward where it holds, by its tension.
                    RealisticFlight flight;
                    if (values.realisticFlips) {
                        flight.fighting = fightLately;
                        const int64_t fresh = now.QuadPart - frequency.QuadPart / 10;
                        flight.valid = swingState.qpc && static_cast<int64_t>(swingState.qpc) > fresh &&
                                       !swingState.error &&
                                       (swingState.status == 1 || swingState.status == 2);
                        flight.velocity = swingState.velocity;
                        float ropes{};
                        unsigned attached{};
                        float coupling{};
                        if (flight.valid && swingState.owned)
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& web = swingState.webs[i];
                                if (webFlight.phases[i] == 1)
                                    continue;
                                const Vec3 toward = web.anchor - feet;
                                if (!web.attached || !finite(web.anchor) || !std::isfinite(web.tension) ||
                                    length(toward) < .1f)
                                    continue;
                                flight.attached = true;
                                ropes += length(toward);
                                ++attached;
                                const float gain = webFlight.phases[i] == 0 ? 1.f : webFlight.coupling[i];
                                flight.pull +=
                                    normalized(toward) * std::clamp(web.tension, .1f, 100.f) * gain;
                                coupling = std::max(coupling, gain);
                            }
                        if (attached) {
                            flight.coupling = coupling;
                            flight.ropeLength = ropes / static_cast<float>(attached);
                        }
                        const auto pose = swing_coupling::scales(native_body::tuckWeight());
                        flight.poseInertia = pose.inertia;
                        flight.poseDrag = pose.angDrag;
                    }
                    rig.flight(flight);
                    rig.wallHands(climbState.status ? climbState.hands : 0u);
                    rig.outwardCrawl(values.body && !flatScreen ? native_body::crawlOutwardWeight() : 0.f);
                    const bool runPalm = game_swing::outdoorClimbGrip(climbState,
                        frame.predictedDisplayTime > 0 ? frame.predictedDisplayTime : 0);
                    rig.wallRunBody(values.body && values.handClimb && !flatScreen && swingFresh,
                        runPalm ? climbState.hands : 0, swingState.velocity);
                    // On a web line (standing on it or jumping off it: game_lines::LineData
                    // phase 2 or 3) the player is not in the air for the flips: the
                    // swing holds him over the line, and FLIPS spun him there without
                    // end (November 3, 17:04, with a video).
                    const bool onLine = lineState.phase == 2 || lineState.phase == 3;
                    const bool flipAir =
                        game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart) && !onLine;
                    const bool flipZipPlay = gesturesPlay && !padLayer.latched() && !phone.open() &&
                                             !gadgetWheel.open() && values.flips && !values.controlledFlips &&
                                             !values.realisticFlips && flipAir && length(hold.normal) < .5f;
                    const auto& zipHand = controller.hands[1];
                    const bool zipPriority = flipZipPriority.update(flipZipPlay, zipHand.valid,
                        zipHand.stickX, zipHand.stickY);
                    if (zipPriority || (flipZipPlay && zipPress.active())) {
                        controller.hands[1].stickX = controller.hands[1].stickY = 0;
                    }
                    // FLIP BREAK (hybrid_owner::flipBreak; the user, November 11 23:39 /
                    // 23:41): a flip's pitch or tilt breaks a midair animation on the
                    // frame it is read, before this frame's tracking, hands and body
                    // use the owner (review M1 for this path). The right stick's up /
                    // down here, before the legs take the stick; the left stick's
                    // after the rig has read the controlled flip's toggle (below).
                    const bool flipBreakHybrid = values.animatedHybrid && values.animatedMoves;
                    const bool flipBreakLegs = values.legControl &&
                                               (values.controlledFlips || values.realisticFlips) &&
                                               values.body && !flatScreen;
                    head_probe::StateInputs flipBreakState;
                    flipBreakState.playing = gesturesPlay && !padLayer.latched() && !phone.open() &&
                                             !gadgetWheel.open();
                    flipBreakState.fresh = swingFresh;
                    flipBreakState.grounded = swingState.grounded != 0;
                    flipBreakState.wall = swingState.wall;
                    const auto tryFlipBreak = [&](const hybrid_owner::FlipInput& in) {
                        if (!flipBreakHybrid || !flipAir || hybridOwner.domain != hybrid_owner::Domain::anim)
                            return false;
                        const auto why = hybrid_owner::flipBreak(in);
                        if (why == hybrid_owner::FlipBreak::none)
                            return false;
                        const int64_t at = frame.predictedDisplayTime > 0
                                               ? frame.predictedDisplayTime
                                               : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                        if (!hybridOwner.breakForFlip(flipBreakState,
                                                      !flipBreakState.playing || !flipBreakState.fresh, at))
                            return false;
                        diag_log::writef("flip break: %s (R %.2f,%.2f L %.2f,%.2f), anim->vr this frame "
                                         "(display %lld ns), position lock on the feet follows",
                                         hybrid_owner::flipBreakName(why), in.rightX, in.rightY, in.leftX,
                                         in.leftY, static_cast<long long>(at));
                        return true;
                    };
                    {
                        hybrid_owner::FlipInput in;
                        in.rightValid = controller.hands[1].valid;
                        in.rightX = controller.hands[1].stickX;
                        in.rightY = controller.hands[1].stickY;
                        in.basicFlips = values.flips && !values.controlledFlips && !values.realisticFlips;
                        // Toggled off, the right stick is no leg's: its up/down zips
                        // and heals (no pitch, no break), as without LEG CONTROL.
                        in.legs = flipBreakLegs && legHoldOut.engaged;
                        in.zipPriority = zipPriority;
                        // With LEG CONTROL the push that broke is the legs' from this
                        // frame (a double tap's first push counts toward the zip/heal).
                        if (tryFlipBreak(in) && in.legs)
                            legStickOwns = true;
                    }
                    // LEG CONTROL owns the right stick (last frame's finding, in
                    // play): it turns nothing and goes to the game as rest.
                    // LEG TOGGLE on this frame: off, the stick is the turn's at once;
                    // on where the legs could move last frame, the legs' at once.
                    legStickActive = gesturesPlay && legHoldOut.engaged &&
                                     (legStickOwns || (legHoldOut.engagedNow && legBaseLast));
                    legStickOwns = false;
                    rig.legStick(legStickActive);
                    if (legStickActive)
                        controller.hands[1].stickX = controller.hands[1].stickY = 0;
                    // Whether the game's time runs, for the rig to tell a scene's end from a menu's (after a
                    // scene the view faces where the game's camera does). Only with first-person cutscenes or
                    // the theater screen disabled (options bits 22, 24): without them play resumes after a
                    // scene facing as it did before it, as it always has.
                    {
                        const auto sceneTime = game_time::telemetry();
                        rig.scene((config.options & 0x1400000) && !flatScreen &&
                                  sceneClock.running(sceneTime.installed && sceneTime.gameFrame > 0,
                                                     GetTickCount64()));
                        // The heading the first-person scene's view had at its last frame, for 4 s after it
                        // and only while play has not come back (a menu later is no scene's end).
                        rig.sceneFacing(fpFacingMs && GetTickCount64() - fpFacingMs < 4000 && !motion.active,
                                        fpFacing);
                    }
                    // DODGE TUMBLE: the eyes take the tumble's turn from the
                    // last frame of play; with none (a menu, a scene), it ends.
                    if (!tumbleStepped)
                        dodgeTumble.reset();
                    tumbleStepped = false;
                    rig.viewTurn(dodgeTumble.turn());
                    // ANIMATED MOVES: stable exit after the body block owns adoptFeet
                    // (debounced). Speculative early adoptFeet on web-intent flicker
                    // was snapping tracking every frame under Full head midair.

                    head_probe::StateInputs hybridInputState;
                    hybridInputState.playing = gesturesPlay && !padLayer.latched() && !phone.open() &&
                                               !gadgetWheel.open() && !dodging;
                    hybridInputState.fresh = swingFresh;
                    const bool hybridInputAnim =
                        values.animatedHybrid && hybridOwner.animOwns(values.animatedMoves, hybridInputState);
                    const bool hybridInputLocked = hybridInputAnim && values.animatedHead != 0 && !flatScreen;
                    const bool hybridInputCrawl =
                        swingState.wall == static_cast<uint32_t>(game_swing::Surface::game) ||
                        swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall);
                    rig.animatedYawOwner(
                        hybrid_steer::ownsYaw(hybridInputLocked, values.animatedModelYaw, hybridInputCrawl));
                    motion = rig.update(seated::raised(controller, seatRaise), feet,
                                        {camera[8], camera[9], camera[10]}, gameplay,
                                        swingState.owned ? Vec3{0, 1, 0} : heroUp, flipAir, hold,
                                        {camera[4], camera[5], camera[6]});
                    // PUNCH / AIR VIEW EASE: how each ease ended, once.
                    if (rig.viewEase().ends() != punchEaseEndsLogged) {
                        punchEaseEndsLogged = rig.viewEase().ends();
                        diag_log::writef("%s view ease: %s after %d ms", easeWhy,
                                         view_ease::endName(rig.viewEase().lastEnd()),
                                         static_cast<int>(rig.viewEase().elapsed() * 1000 + .5f));
                    }
                    {
                        // FLIP BREAK, the left stick: the axes CONTROLLED FLIPS (toggled
                        // on in the air) or REALISTIC FLIPS' steering read.
                        hybrid_owner::FlipInput in;
                        in.leftValid = frame.hands[0].valid;
                        in.leftX = frame.hands[0].stickX;
                        in.leftY = frame.hands[0].stickY;
                        in.controlledActive = motion.controlledFlipControls;
                        in.realisticSteering = values.realisticFlips && values.flips;
                        tryFlipBreak(in);
                    }
                    bool hybridInputViewOk = false;
                    float hybridInputViewYaw = 0.f;
                    // FULL head lock on the stable heading steers from the lock (Step B).
                    const bool hybridMode =
                        hybrid_movement::modeOf(values.movementMode) == hybrid_movement::Mode::hybrid;
                    const bool viewLockSteer =
                        viewLockActive && viewLock.set &&
                        hybrid_movement::modeOf(values.movementMode) == hybrid_movement::Mode::stable &&
                        animated_moves::headFollow(values.animatedHead) == animated_moves::HeadFollow::full;
                    viewLockHeadingOk = false;
                    // The player's own turn (headset since the latch + R-stick turns), for
                    // the runaway-turn detector (stable_yaw.hpp).
                    float hybridInputIntentYaw = 0.f;
                    Vec3 hybridInputMove{};
                    const float hybridInputL =
                        frame.hands[0].valid ? std::hypot(frame.hands[0].stickX, frame.hands[0].stickY) : 0.f;
                    const int64_t hybridInputNs = frame.predictedDisplayTime > 0
                                                      ? frame.predictedDisplayTime
                                                      : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                    const bool hybridInputYawHold =
                        hybridBlend.active && !hybridBlend.toTraversal && hybridBlend.delayModelYaw &&
                        !hybrid_handoff::blendDone(hybridInputNs - hybridBlend.startNs);
                    const float hybridInputR = values.animatedModelYaw && !motion.basicFlipControls &&
                                                       !legStickActive &&
                                                       !hybridInputYawHold && frame.hands[1].valid
                                                   ? frame.hands[1].stickX
                                                   : 0.f;
                    if (hybridInputLocked && !hybridInputCrawl && !motion.ventCrawl && !motion.onSurface &&
                        motion.active) {
                        Mat4 headMat{};
                        const bool levelHead = animated_moves::headFollow(values.animatedHead) ==
                                               animated_moves::HeadFollow::level;
                        // The head bone as animated (never the lock's turn): heroHead.
                        if (levelHead ? game_fp::frameLevel(headMat) : game_fp::heroHead(headMat)) {
                            const Vec3 ahead{headMat[8], headMat[9], headMat[10]};
                            if (finite(ahead) && std::hypot(ahead.x, ahead.z) > .2f) {
                                float trackedYaw = 0.f, refYaw = 0.f;
                                if (animHeadRefSet && validTrackedPose(frame.head)) {
                                    trackedYaw = hybrid_handoff::lookYaw(
                                        screenAhead(frame.head, 0).orientation.rotate({0, 0, -1}));
                                    refYaw =
                                        hybrid_handoff::lookYaw(animHeadRef.orientation.rotate({0, 0, -1}));
                                }
                                const float headBoneYaw = hybrid_handoff::lookYaw(ahead);
                                const float headTurnNow = animHeadRefSet ? animHeadTurn : 0.f;
                                hybridInputIntentYaw = stable_yaw::wrap(trackedYaw - refYaw + headTurnNow);
                                if (hybridMode && viewLockActive && hybridHeading.set && !levelHead) {
                                    // HYBRID: he runs along the heading (the right stick's);
                                    // the headset's own turn never steers.
                                    stableHeading.reset();
                                    hybridInputIntentYaw = hybridHeading.yaw;
                                    hybridInputViewYaw = hybridHeading.yaw;
                                    hybridViewYaw.update(hybridHeading.yaw, frame.seconds, stable_yaw::stableTau);
                                } else if (viewLockSteer && !levelHead) {
                                    // FULL head lock (view_yaw_lock.hpp): where the player looks
                                    // is the lock's heading + the headset's turn; the R stick
                                    // turned the lock at the top of the frame. No relatch to the
                                    // head bone, no steering own turn: the hero never turns it.
                                    const float target = stable_yaw::wrap(viewLock.yaw + trackedYaw - refYaw);
                                    stableHeading.reset();
                                    hybridInputIntentYaw = target;
                                    viewLockHeading = target;
                                    viewLockHeadingOk = true;
                                    hybridInputViewYaw =
                                        hybridViewYaw.update(target, frame.seconds, stable_yaw::stableTau);
                                } else if (hybrid_movement::viewSource(values.movementMode) == stable_yaw::Source::stable) {
                                    // VIEW YAW SOURCE: STABLE - a heading of its own, never
                                    // turned by the head bone while he runs (stable_yaw.hpp).
                                    stable_yaw::Input in;
                                    in.dt = frame.seconds;
                                    in.headBoneYaw = headBoneYaw;
                                    in.hmdDelta = trackedYaw - refYaw;
                                    in.headTurn = headTurnNow;
                                    in.leftTilt = hybridInputL;
                                    in.rightX = hybridInputR;
                                    in.levelTurns = levelHead;
                                    in.smoothDegPerSec = values.smoothTurn;
                                    in.snapDeg = values.snapTurn;
                                    const float target = stableHeading.update(in);
                                    hybridInputIntentYaw = stable_yaw::wrap(hybridInputIntentYaw + stableHeading.ownTurn);
                                    hybridInputViewYaw =
                                        hybridViewYaw.update(target, frame.seconds, stable_yaw::stableTau);
                                } else {
                                    const float target =
                                        hybrid_steer::viewYaw(headBoneYaw, trackedYaw, refYaw, headTurnNow);
                                    hybridInputViewYaw = hybridViewYaw.update(target, frame.seconds);
                                }
                                hybridInputViewOk = true;
                            }
                        }
                        if (!hybridInputViewOk && hybridViewYaw.set) {
                            hybridInputViewYaw = hybridViewYaw.yaw;
                            hybridInputViewOk = true;
                        }
                        // HYBRID's left stick: front half along the heading, the rear
                        // half one snap of the heading (and the view) to the stick.
                        const bool hybridGround = hybridMode && viewLockActive && hybridHeading.set &&
                                                  hybridOwner.domain != hybrid_owner::Domain::vr && swingFresh &&
                                                  swingState.grounded && !swingState.webs[0].attached &&
                                                  !swingState.webs[1].attached;
                        if (hybridGround && frame.hands[0].valid) {
                            // v2: the headset's yaw off the heading (the view is heading +
                            // the headset's turn since the head latched), + right.
                            float hybridGaze = NAN;
                            if (animHeadRefSet && validTrackedPose(frame.head)) {
                                const float tracked = hybrid_handoff::lookYaw(
                                    screenAhead(frame.head, 0).orientation.rotate({0, 0, -1}));
                                const float ref =
                                    hybrid_handoff::lookYaw(animHeadRef.orientation.rotate({0, 0, -1}));
                                if (std::isfinite(tracked) && std::isfinite(ref))
                                    hybridGaze = -stable_yaw::wrap(tracked - ref);
                            }
                            const auto zoneWas = hybridStick.zone;
                            hybridStick =
                                hybridGate.update(frame.seconds, frame.hands[0].stickX, frame.hands[0].stickY, hybridGaze);
                            if (hybridStick.zone != zoneWas && hybridStick.zone != hybrid_movement::Zone::none) {
                                constexpr float toDeg = 180.f / 3.14159265f;
                                diag_log::writef("hybrid stick: %s (stick %+.0f deg, gaze %+.0f, run %+.0f off the "
                                                 "heading)",
                                                 hybrid_movement::zoneName(hybridStick.zone),
                                                 hybrid_movement::stickAngle(frame.hands[0].stickX,
                                                                             frame.hands[0].stickY) * toDeg,
                                                 std::isfinite(hybridGaze) ? hybridGaze * toDeg : 0.f,
                                                 hybridStick.dir * toDeg);
                            }
                            if (hybridStick.snapped) {
                                constexpr float toDeg = 180.f / 3.14159265f;
                                const float from = hybridHeading.yaw;
                                const float to =
                                    hybrid_movement::stickYaw(from, frame.hands[0].stickX, frame.hands[0].stickY);
                                const float heroNow = std::hypot(body[8], body[10]) > .2f
                                                          ? hybrid_handoff::actorLookYaw({body[8], body[9], body[10]})
                                                          : NAN;
                                hybridHeading.begin(hybrid_movement::Heading::Hold::snap, to,
                                                    static_cast<float>(values.rearSnapMs) * .001f);
                                if (values.rearSnapMs <= 0)
                                    ++armEpoch; // an instant snap is a jump of the frame, not a hand's motion
                                diag_log::writef("rear snap: stick %+.0f deg -> facing %.0f (from %.0f, hero %.0f, "
                                                 "view %s, #%u)",
                                                 hybridStick.snapAngle * toDeg, to * toDeg, from * toDeg,
                                                 std::isfinite(heroNow) ? heroNow * toDeg : -999.f,
                                                 values.rearSnapMs <= 0 ? "instant" : "eased", hybridGate.snaps);
                            }
                        } else {
                            hybridGate.reset();
                            hybridStick = {};
                        }
                        if (hybridInputViewOk) {
                            const Vec3 walk =
                                hybridGround
                                    ? hybrid_steer::viewForward(stable_yaw::wrap(hybridHeading.yaw - hybridStick.dir)) *
                                          hybridStick.y
                                : frame.hands[0].valid
                                    ? hybrid_steer::viewMove(frame.hands[0].stickX, frame.hands[0].stickY,
                                                             hybridInputViewYaw)
                                    : Vec3{};
                            // STABLE: the R stick turns the heading itself while
                            // the left stick runs (no second turn request).
                            // FULL head lock: the R stick only ever turns the lock's
                            // heading; the hero follows by steering or TURN-IN-PLACE.
                            hybridInputMove = animated_model_yaw::composeMove(
                                walk,
                                viewLockSteer || hybridGround ? 0.f
                                              : stable_yaw::composeTurn(hybrid_movement::viewSource(values.movementMode),
                                                                        hybridInputR, hybridInputL),
                                {body[8], body[9], body[10]});
                            // Only ground locomotion consumes the shared vector. Never
                            // renormalize or redirect web/wall/air physical velocities.
                            if (swingFresh && swingState.grounded && !hybridInputCrawl &&
                                !swingState.webs[0].attached && !swingState.webs[1].attached &&
                                hybridInputL >= animated_model_yaw::deadzone) {
                                motion.swing.move = hybridInputMove;
                            }
                        }
                    } else {
                        hybridViewYaw.reset();
                        stableHeading.reset();
                        hybridGate.reset();
                        hybridStick = {};
                    }
                    if (values.eyeAim && motion.active && !flatScreen) {
                        ++eyeAimFrames;
                        gazeFrames += motion.gazeValid ? 1u : 0u;
                        // An eye tracker that gave no gaze in three seconds of
                        // play: over Quest Link and Virtual Desktop the eyes reach
                        // PC games only where the player switched that on.
                        if (!toldNoGaze && !gazeFrames && eyeAimFrames >= 270 &&
                            runtime.eyeTracking() == XrRuntime::EyeTracking::tracker) {
                            toldNoGaze = true;
                            message(3, 0,
                                    "EYE AIM: the headset has an eye tracker but sends no gaze. Quest Pro: turn on "
                                    "eye tracking for PC apps (Quest Link: Settings > Beta; Virtual Desktop: its "
                                    "eye tracking forwarding). Until then hands and head aim as before.");
                        }
                    }
                    if (motion.headDrop > .02f)
                        ++headDroppedFrames;
                    if (motion.active && motion.onSurface) {
                        surfaceEntries += !wasOnSurface;
                        ++surfaceFrames;
                    }
                    wasOnSurface = motion.active && motion.onSurface;
                    // Without VR play (menus, hint cards, cutscenes, animated
                    // cameras, loading, and the game played on its screen,
                    // VIEW: GAME SCREEN) the game's own camera goes on the
                    // screen.
                    const bool vrPlay = motion.active && !flatScreen;
                    bool screen{};
                    if (viewing)
                        screen = gameScreen.update(vrPlay, GetTickCount64());
                    else
                        gameScreen.reset();
                    wasScreen = screen;
                    // A scene the game plays there (a cutscene, a finisher, a
                    // death) gets its own two eyes: the game renders its
                    // camera once for each of the viewer's. Not a pause menu
                    // or a hint card (the game's frames take no game time),
                    // nor without a player (the title screen, a load), nor a
                    // scene whose eyes get no HUD panel for its subtitles.
                    const auto clock = game_time::telemetry();
                    const bool timeRuns =
                        sceneClock.running(clock.installed && clock.gameFrame > 0, GetTickCount64());
                    // FIRST-PERSON CUTSCENES: the scene is shown from inside the head of the character it
                    // plays, round the viewer, instead of on the screen (game_fp.hpp). Wanted while a scene
                    // plays (the same conditions, but the screen's 3D depth and its HUD panel); it is shown
                    // once the character's head has been found, the screen's stereo scene until then.
                    const bool sceneBase = screen && player.hero && bridgeRunning && timeRuns;
                    // A cut from one scene to the next, a loading frame, a moment with no hero: the
                    // first-person view is held up to 1.5 s (never into play, which is `vrPlay`), so the next
                    // scene replaces it in place.
                    const uint64_t fpNow = GetTickCount64();
                    const bool fpBase = (config.options & 0x400000) && !flatScreen;
                    // DISABLE THEATER SCREEN ENTIRELY (options bit 24): every scene the game plays is
                    // rendered around the viewer, from inside the character's head with first-person
                    // cutscenes on, from the scene's own camera without them or where no head is found
                    // (game_fp::sceneCamera). Menus, the pause screen and loading have no scene to put around
                    // you: they stay on the screen.
                    const bool noTheater = (config.options & 0x1000000) && !flatScreen;
                    const bool sceneAny = fpBase || noTheater;
                    // The first-person view needs none of the screen's 250 ms wait (screenDelayMs, there to
                    // keep a late camera sample in play from flashing the screen): 60 ms of the game's own
                    // camera is enough, and a blink of play (under 250 ms) inside a scene, as between a
                    // finisher's shots, keeps the view.
                    if (vrPlay) {
                        fpIdleSince = 0;
                        if (!fpPlayStart)
                            fpPlayStart = fpNow;
                    } else {
                        fpPlayStart = 0;
                        if (!fpIdleSince)
                            fpIdleSince = fpNow;
                    }
                    fpPlayBlip = vrPlay && wasFpScene && fpPlayStart && fpNow - fpPlayStart < 250;
                    const bool fpOwn =
                        !vrPlay && fpIdleSince && fpNow - fpIdleSince >= (wasFpScene ? 0u : 60u);
                    const bool fpSceneBase =
                        viewing && player.hero && bridgeRunning && timeRuns && (fpOwn || fpPlayBlip);
                    // The right thumbstick's click (not with the left: both held are the game's pause)
                    // switches between the first-person view and the theatre screen, for a scene that
                    // glitches in first person.
                    const bool fpClick = frame.hands[1].stickClick && !frame.hands[0].stickClick;
                    // Only while a scene plays: in play the click is the game's R3, or WEB TIE's.
                    if (fpClick && !wasFpRightClick && fpBase && (sceneBase || fpSceneBase)) {
                        fpTheatre = !fpTheatre;
                        fpClickTaken = true;
                        message(3, 0,
                                fpTheatre
                                    ? (noTheater
                                           ? "First-person cutscene: the scene's own camera (right stick)"
                                           : "First-person cutscene: theatre screen (right stick)")
                                    : "First-person cutscene: first-person view (right stick)");
                    }
                    wasFpRightClick = fpClick;
                    // X on the left controller (the equivalent on the others: buttonX) swaps whose eyes the
                    // view is in, Spider-Man, Miles, MJ, while a first-person scene has Miles or MJ in it;
                    // then the game does not get that X (its Back).
                    {
                        const bool x = (frame.buttons & buttonX) != 0;
                        if (fpBase && (sceneBase || fpSceneBase) && !fpTheatre &&
                            game_fp::alternatives() > 0) {
                            if (x && !wasFpX)
                                game_fp::cycleCharacter();
                            frame.buttons &= ~static_cast<uint32_t>(buttonX);
                        }
                        wasFpX = x;
                    }
                    if (!frame.hands[1].stickClick)
                        fpClickTaken = false;
                    // Both sticks held is the game's pause: its menu is drawn only into the game's own frame,
                    // so the theatre screen is wanted at once, and no cut-hold keeps the first-person view
                    // over it.
                    const bool fpPausing = frame.hands[0].stickClick && frame.hands[1].stickClick;
                    // The click's alternative view: the theatre screen, or with the theater screen disabled
                    // the scene's own camera (cameraChosen: also what a session without first-person
                    // cutscenes always has).
                    const bool theaterChosen = fpTheatre && !noTheater;
                    const bool cameraChosen = noTheater && (!fpBase || fpTheatre);
                    bool fpWanted = sceneAny && (sceneBase || fpSceneBase) && !theaterChosen && !fpPausing;
                    if (fpWanted)
                        fpLastWanted = fpNow;
                    else if (sceneAny && !theaterChosen && !fpPausing && !vrPlay && wasFpScene &&
                             fpNow - fpLastWanted < 1500)
                        fpWanted = true;
                    game_fp::sceneCamera(noTheater ? (cameraChosen ? 2 : 1) : 0);
                    if (!fpWanted)
                        fpWantStart = 0;
                    else if (!fpWantStart)
                        fpWantStart = fpNow;
                    game_fp::want(fpWanted);
                    fpScene = fpWanted && game_fp::available();
                    // Wanted, the character's head not found yet: the last image stays up (the eye images'
                    // own hold) for up to 0.7 s, not the theatre screen; a scene with no head to be found
                    // gets the theatre then.
                    fpPending = fpWanted && !fpScene && fpNow - fpWantStart < 700;
                    // One worker line every 120 ms, not one a frame: message() is a single slot that the
                    // session log samples every 50 to 100 ms, so lines sent faster than that overwrite each
                    // other and only the last of a burst reaches the log (the start of every listing was lost
                    // this way).
                    if (static ULONGLONG lastNoteAt{}; GetTickCount64() - lastNoteAt >= 120) {
                        char line[200];
                        if (game_fp::note(fpNoteSeen, line, sizeof(line))) {
                            message(3, 0, line);
                            lastNoteAt = GetTickCount64();
                        }
                    }
                    if (const bool onCamera = fpScene && game_fp::onCamera(); onCamera != wasCameraView) {
                        message(3, 0,
                                onCamera ? "First-person cutscene: the scene's own camera is the view"
                                         : "First-person cutscene: a head is the view");
                        wasCameraView = onCamera;
                    }
                    if (fpScene != wasFpScene) {
                        char text[96];
                        std::snprintf(text, sizeof(text), "First-person cutscene: %s (at %llu ms)",
                                      fpScene ? "started" : "ended",
                                      static_cast<unsigned long long>(fpNow % 100000000ull));
                        message(3, 0, text);
                        wasFpScene = fpScene;
                    }
                    if (!fpScene || frame.recentered)
                        sceneRefSet = false;
                    // Head tracking off, or the scene's own camera the view by choice: a level frame at the
                    // character's (or the camera's) place, and the right stick turns the view.
                    levelView = (config.options & 0x800000) != 0 || (noTheater && cameraChosen);
                    if (!fpScene || !levelView) {
                        sceneTurn = 0;
                        sceneFlick = false;
                    }
                    // (While a first-person view is pending, no stereo-screen scene is asked for either: its
                    // images would show the theatre screen the moment they arrive.)
                    const bool panelScene =
                        scenePanel.wanted(sceneBase && values.screenDepth > 0,
                                          native_hud::onScreen(sceneHoldMs), GetTickCount64());
                    const bool scene = fpScene || (panelScene && !fpPending);
                    // On the screen the controllers are the game's Xbox
                    // controller; in VR it gets Start (menu), Back (X) and
                    // the native walking, jumping, dodge and interact
                    // (submitted below).
                    // The pad layer is latched in the input block below, where the
                    // swing's state is known, so this reads the last frame's: the
                    // chord takes chordSeconds to latch, so a frame is nothing.
                    // Nothing reaches the game until the headset has tracked
                    // once: before that the player presses blind, and the title
                    // screen's QUIT GAME is two moves from the top.
                    headSeen = headSeen || validTrackedPose(frame.head);
                    const auto mapping =
                        game_pad::mappingFor(viewing, headSeen, screen, padLayer.latched());
                    const bool inPlay =
                        mapping == game_pad::Mapping::gameplay || mapping == game_pad::Mapping::pad;
                    origins.update(frame.buttons, inPlay);
                    // What the controllers themselves sent, beside what reached the
                    // game: a button missing from both was never pressed. The
                    // phone's presses are the controllers' too.
                    if (inPlay)
                        vrButtonsPlayed |= trackedFrame.buttons;
                    // The prompts are read wherever the game's UI builds one: what
                    // they name is what the controls of those names send in VR
                    // play (NamedPress). Off from October 13 to 20, because with
                    // the reading on CONTINUE at the main menu killed the game
                    // every time - the process simply gone, no crash dump. The
                    // death was the reading's own: the timeline's line for a
                    // prompt call went through sprintf_s into a buffer a long
                    // text overflowed, and sprintf_s ends the process for that
                    // (game_prompts.cpp, keepLine). The wording stays the game's
                    // but for its LB + RB in VR play (setRewriting): X sends them
                    // there (NamedPress), so the game's prompt says X (October 24).
                    // On the pad layer the grips are its bumpers again.
                    game_prompts::setObserving(promptHelp);
                    game_prompts::setReading(promptHelp);
                    game_prompts::setRewriting(promptHelp && mapping == game_pad::Mapping::gameplay);
                    // A change in the SPIDY VR tab takes effect at once.
                    if (game_menu::take(values)) {
                        ++settingChanges;
                        applySettings();
                        if (auto text = vr_settings::tabOnlyText(values); text != tabOnlySaved) {
                            if (!writeTabOnly(text))
                                message(3, 0, "VR settings: the SPIDY VR tab's own settings were not saved");
                            tabOnlySaved = std::move(text);
                        }
                    }
                    game_menu::publish(values);
                    const uint32_t gate =
                        (player.hero ? 0 : gateNoPlayer) | (bridgeRunning ? 0 : gateBridge) |
                        (committed ? 0 : gateNoCommit) | (followsPlayer ? 0 : gateOtherCamera) |
                        (gameplay && !motion.active ? gateTracking : 0);
                    if (gameplay && !motion.active) {
                        ++trackingGapFrames;
                        trackingGapReasons |= motion.inactive;
                    }
                    if ((gameScreen.entered() && !flatScreen) || ((flatScreen || screen) && frame.recentered))
                        screenPose = screenAhead(frame.head, vr_settings::screenDistance);
                    // An image keeps the tracking-space poses it was rendered
                    // for, valid until the tracking space itself changes.
                    if (!viewing || frame.recentered)
                        history.clear();
                    if (!viewing || frame.recentered || !motion.active || flatScreen)
                        hudFollow.reset();
                    if (!viewing) {
                        lastPresentedMs = lastNewImageMs = firstImageDeadline = 0;
                        presented = false;
                    } else if (vrPlay && !wasGameplay) {
                        // Gameplay counts its own images: the screen before it
                        // may have had none for a while (loading). The screen's
                        // own frames counted as shown, so a play that never
                        // shows a pair still says so after 3 seconds.
                        lastPresentedMs = lastNewImageMs = 0;
                        presented = false;
                        firstImageDeadline = nativeStarted ? GetTickCount64() + 3000 : 0;
                    }
                    wasGameplay = vrPlay;
                    const bool controlsVisible = recentPresentation(lastNewImageMs, GetTickCount64());
                    // The T-pose calibration: at the first gameplay of a session
                    // without one, or when the SPIDY VR tab asks for it, once VR
                    // has shown play for a moment and no web flies the player. It
                    // goes on only while VR shows play (a pause or the flat
                    // screen holds it), and B skips it.
                    using CalibrationPhase = body_calibration::Phase;
                    const bool immersive = viewing && motion.active && !flatScreen && controlsVisible;
                    const uint64_t nowMs = GetTickCount64();
                    if (!immersive)
                        immersiveSince = 0;
                    else if (!immersiveSince)
                        immersiveSince = nowMs;
                    const bool skipPressed = (frame.buttons & buttonB) && !skipHeld;
                    skipHeld = (frame.buttons & buttonB) != 0;
                    if (calibration.phase() == CalibrationPhase::idle && (calibrationWanted || values.calibrate) &&
                        immersive && nowMs - immersiveSince >= 1500 && !swingState.owned) {
                        calibration.start();
                        calibrationPanel = body_calibration::panelPose(frame.head);
                        calibrationInput = true;
                        values.calibrate = false; // the tab shows NO again
                        runtime.haptic(0, .3f);
                        runtime.haptic(1, .3f);
                        message(3, 0, "Body calibration: stand in a T-pose and hold both triggers (B skips)");
                    } else if (calibration.phase() == CalibrationPhase::done) {
                        if (nowMs - calibrationShownAt >= 3500)
                            calibration.stop();
                    } else if (calibration.phase() != CalibrationPhase::idle && immersive && skipPressed) {
                        calibration.stop();
                        ++calibrationSkips;
                        // Nor at the next sessions' first gameplay (the launcher keeps it).
                        calibrationWanted = calibrationPrompt = false;
                        runtime.haptic(1, .25f);
                        message(3, 0, "Body calibration skipped - Settings > SPIDY VR > CALIBRATE BODY does it later");
                    } else if (calibration.phase() != CalibrationPhase::idle && immersive) {
                        body_calibration::Sample sample;
                        // Seated, the player as the game's world has them: raised
                        // (seated.hpp), so the eyes measure as standing ones.
                        const XrFrame placed = seated::raised(frame, seatRaise);
                        sample.headTracked = validTrackedPose(placed.head);
                        sample.head = placed.head;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = placed.hands[i];
                            sample.handTracked[i] = hand.valid && validTrackedPose(hand.grip);
                            sample.grips[i] = hand.grip;
                            sample.triggers[i] = hand.trigger;
                        }
                        sample.seconds = frame.seconds;
                        // The hero's rig once the body has turned it, else Spider-Man's measured one.
                        body_calibration::Proportions sizes;
                        native_body::proportions(sizes);
                        const auto before = calibration.phase();
                        if (calibration.update(sample, sizes)) {
                            lastCalibration = calibration.result();
                            calibratedEye = std::clamp(lastCalibration.eyeHeight,
                                                       body_calibration::minEyeHeightMm / 1000.f,
                                                       body_calibration::maxEyeHeightMm / 1000.f);
                            calibratedArm = std::clamp(lastCalibration.armLength,
                                                       body_calibration::minArmLengthMm / 1000.f,
                                                       body_calibration::maxArmLengthMm / 1000.f);
                            ++calibrations;
                            calibrationWanted = false;
                            calibrationShownAt = nowMs;
                            runtime.haptic(0, .7f);
                            runtime.haptic(1, .7f);
                            feel(body_haptics::Kind::calibrated);
                            char text[160];
                            sprintf_s(text, "Body calibrated: height %.2f m (eyes %.2f m), arm length %.2f m (left %.2f, "
                                      "right %.2f)",
                                      body_calibration::height(calibratedEye), calibratedEye, calibratedArm,
                                      lastCalibration.reach[0], lastCalibration.reach[1]);
                            message(3, 0, text);
                        } else if (before == CalibrationPhase::waiting &&
                                   calibration.phase() == CalibrationPhase::holding) {
                            // The pose counts: a light tick in both hands.
                            runtime.haptic(0, .15f);
                            runtime.haptic(1, .15f);
                        }
                    }
                    // The phone, which a back-pocket draw opens in the hand. It is
                    // live while VR shows play and no calibration has the
                    // controllers, which it leaves to a calibration (October 19:
                    // one begun under it could be neither finished nor skipped).
                    {
                        const bool phonePlay = immersive && calibration.phase() == CalibrationPhase::idle;
                        if (phone.update(frame.seconds, phonePlay, frame.head, grips, gripsTracked,
                                         phoneDrawnWith, pocketDraw.hand())) {
                            if (!values.pcMedia)
                                phoneMusic.load();
                            else if (!pcMedia)
                                pcMedia = std::make_unique<PcMedia>();
                            runtime.haptic(0, .2f);
                            runtime.haptic(1, .2f);
                            feel(body_haptics::Kind::phone, pocketDraw.drewWith());
                            message(3, 0, "Phone out - the stick, A, B and the triggers are the phone's "
                                          "until B puts it away");
                        }
                    }
                    // While it is out its hands land no blow (Settings::noBlows),
                    // nor while the game names its X (NamedPress, above).
                    if (phone.live() != phoneBlows || namedFists != fistsBlows)
                        sendSwingSettings();
                    if (calibrationInput && calibration.phase() == CalibrationPhase::idle) {
                        bool held = (frame.buttons & buttonB) || frame.jump;
                        for (const auto& hand : frame.hands)
                            held = held || hand.trigger > .2f || hand.squeeze > .2f;
                        calibrationInput = held;
                    }
                    if (calibrationInput) {
                        // The calibration has the controllers: no web, web ball or jump.
                        motion.swing.jump = false;
                        motion.nativeKeys &= ~swingJumpKey;
                        for (auto& hand : motion.swing.hands)
                            hand.trigger = hand.grip = 0;
                    }
                    if (padLayer.latched()) {
                        // The pad layer has them too: the grips are the game's bumpers
                        // there and the triggers its triggers, so neither shoots a web.
                        // A is the game's own A straight from the controller, so the
                        // swing's leap goes with them.
                        motion.swing.jump = false;
                        motion.nativeKeys &= ~swingJumpKey;
                        for (auto& hand : motion.swing.hands)
                            hand.trigger = hand.grip = 0;
                    }
                    // Slow motion: the left thumbstick's click, in VR play outside the calibration.
                    const bool slowAllowed =
                        immersive && calibration.phase() == CalibrationPhase::idle && spiderMan;
                    // A click the game names (its L3, its R3) is not slow motion.
                    const uint32_t clicksNamed = namedPress.claimed();
                    const bool slowLeft =
                        frame.hands[0].stickClick && !(clicksNamed & game_pad::NamedPress::leftClick);
                    const bool slowRight =
                        frame.hands[1].stickClick && !fpClickTaken &&
                        !(clicksNamed & game_pad::NamedPress::rightClick);
                    const bool slowPressed =
                        slowPress.update(frame.seconds, slowAllowed, slowLeft, slowRight);
                    stepSlowMotion(frame.seconds, slowPressed, slowAllowed);
                    if (motion.active && !raysStarted) {
                        native_rays::Config rays;
                        rays.pid = config.pid;
                        rays.base = config.base;
                        rays.durationMs = moduleDuration;
                        check(startRays(&rays), "Start controller world rays");
                        raysStarted = true;
                    }
                    if (raysStarted) {
                        auto input = motion.swing;
                        input.focused = motion.active && !flatScreen;
                        auto aim = controllerAimRays(input, ++serial);
                        // The head's rays, in the slots the hands leave free
                        // (HeadClearance): from the body to past the eyes, and
                        // from the eyes ahead, to each side and up. Until
                        // October 18 these five read the room around the head
                        // for the report alone (Enclosure).
                        if (motion.active && validTrackedPose(motion.headPose)) {
                            const Quat turn = motion.headPose.orientation;
                            const Vec3 ahead = turn.rotate({0, 0, -1}), aside = turn.rotate({1, 0, 0}),
                                       overhead = turn.rotate({0, 1, 0});
                            const auto head = rig.clearanceRays(ahead, aside, overhead);
                            const auto hands = rig.handCollisionRays(motion.headPose.position);
                            // And the room over his feet, along the surface he
                            // stands on: how far the head can go before it is
                            // through the roof of a vent or a duct. From the
                            // FEET, where the other five are from the head - a
                            // head already through that roof would cast from
                            // the far side of it and find nothing.
                            //
                            // Along the up the head is actually placed on: the
                            // surface's normal while the view stands on it, the
                            // world's up otherwise, because that is the axis the
                            // rig caps. The actor's own up is neither while
                            // Spidy holds him on a wall - the game has him
                            // upright in its air state there - and casting along
                            // it found the shaft's ceiling 0.16 m over his feet
                            // and brought his head down to a crouch for 22
                            // frames (reports/game-vr-20261014-124656.json).
                            const Vec3 up = motion.standing && finite(hold.normal) && length(hold.normal) > .5f
                                                ? normalized(hold.normal)
                                                : Vec3{0, 1, 0};
                            const native_rays::Ray roof{feet + up * .1f, headroomReach, up, 7u};
                            appendTrackingRays(aim, head, hands, roof, collisionRayPhase++);
                        }
                        check(submitRays(&aim), "Controller world rays");
                        // What the roof's ray found, from the batch the bridge
                        // last finished (the head's go to the rig before its
                        // update, above).
                        if (rayResults) {
                            for (unsigned i = 0; i < native_rays::capacity && i < rayResults->count; ++i) {
                                const auto& hit = rayResults->hits[i];
                                if (hit.ray.tag != 7)
                                    continue;
                                // The surface over the feet, from where the ray
                                // started a tenth of a metre up. A reading
                                // tighter than a crouch is not a roof - a step,
                                // a strut, a lip at his feet - and the head is
                                // left alone for it: the rule is for a space a
                                // player is inside, not for whatever his feet
                                // are against.
                                const float found = native_rays::fixedSurface(hit) && std::isfinite(hit.fraction)
                                                        ? hit.fraction * hit.ray.distance + .1f
                                                        : 0.f;
                                headroomSeen = found >= leastHeadroom + headClearance ? found : 0.f;
                            }
                        }
                    }
                    if (motion.active && !flatScreen && controlsVisible && !swingStarted) {
                        game_swing::Config swing;
                        swing.pid = config.pid;
                        swing.base = config.base;
                        swing.record = player.record;
                        swing.mover = player.mover;
                        swing.motionModule = config.motionModule;
                        swing.durationMs = moduleDuration;
                        swing.maxSpeed = values.swingSpeed;
                        swing.gravity = vr_settings::gravity(values.weight);
                        swing.grabKinds = !values.webGrab || !sampleGrab ? 0 : game_grab::movableKinds;
                        check(startSwing(&swing), "Start native swinging");
                        swingStarted = true;
                        // Fists and the web shooter take the swing's input samples;
                        // both are optional and, like the grab, can be switched on later.
                        applySettings();
                    }
                    // Raw motion.grips remains the next IK input. Action emitters
                    // use a separate completed-job pose; never feed solved hands back.
                    std::array<Pose,2> actionGrips = motion.grips;
                    uint32_t actionGripMask{};
                    Pose actionHead{};
                    bool actionHeadValid{}, nativeActionSample{};
                    uint64_t actionPoseSerial{}, actionPoseDeadline{};
                    const Pose physicalGaze = motion.gazePose;
                    const bool physicalGazeValid = motion.gazeValid;
                    // ARMS IN ANIMATION: the hands in the game THIS frame, through the
                    // eyes' own map (arm_frame.hpp) - not the last pose job's solved
                    // bones (about 41 a second, contorted by the animation) and not
                    // the SpidyVR tracking rig (whose yaw never sees the actor's
                    // R-stick turn). Built when the animation owns with ANIMATED ARMS.
                    arm_frame::ViewMap armView{};
                    strikeRead = strikeUnderWay = false; // the strike read below says again this frame
                    if (frame.recentered)
                        ++armEpoch;
                    // M1: Level's R-stick turn (as in cutscenes with head tracking off)
                    // for THIS frame, before the hands are mapped; eyes and hands use it.
                    if (levelTurnGate && animHeadRefSet && !legStickActive) {
                        const float x = frame.hands[1].stickX;
                        const float degrees = 3.14159265f / 180;
                        if (values.smoothTurn > 0) {
                            if (std::abs(x) > .15f)
                                animHeadTurn -= x * static_cast<float>(values.smoothTurn) * degrees * frame.seconds;
                        } else if (values.snapTurn > 0) {
                            if (!animHeadFlick && std::abs(x) > .7f) {
                                animHeadFlick = true;
                                animHeadTurn -= (x > 0 ? 1.f : -1.f) * static_cast<float>(values.snapTurn) * degrees;
                                ++armEpoch; // a snap is a jump of the frame, not a hand's motion
                            } else if (animHeadFlick && std::abs(x) < .4f)
                                animHeadFlick = false;
                        }
                    }
                    levelTurnGate = false; // the eyes, below, say again for the next frame
                    // FULL head lock on the stable heading: the R stick turns the lock's
                    // heading (smooth deg/s or snap) for THIS frame, before the hands are
                    // mapped; eyes, hands and arms all take the published lock.
                    viewLockDelta = 0;
                    viewLockSnapped = false;
                    if (fullLockGate && viewLockActive && viewLock.set && !legStickActive &&
                        !motion.basicFlipControls && frame.hands[1].valid) {
                        const uint32_t snapsBefore = viewLock.snaps;
                        viewLockDelta = viewLock.turn(frame.seconds, frame.hands[1].stickX, values.smoothTurn,
                                                      values.snapTurn);
                        viewLockSnapped = viewLock.snaps != snapsBefore;
                        if (viewLockSnapped)
                            ++armEpoch; // a snap is a jump of the frame, not a hand's motion
                    } else {
                        viewLock.turn(frame.seconds, 0.f, values.smoothTurn, values.snapTurn);
                    }
                    // HYBRID: the right stick turned the heading; the hero is brought onto it
                    // (walk stick), the view holds it meanwhile and is the head again after.
                    hybridHeroTurn = false;
                    if (hybrid_movement::modeOf(values.movementMode) == hybrid_movement::Mode::hybrid &&
                        viewLockActive && hybridHeading.set) {
                        constexpr float toDeg = 180.f / 3.14159265f;
                        hybridHeading.turn(viewLockDelta);
                        const bool heroOk = std::hypot(body[8], body[10]) > .2f;
                        hybrid_movement::Heading::In hin;
                        hin.dt = frame.seconds;
                        hin.heroOk = heroOk;
                        hin.hero = heroOk ? hybrid_handoff::actorLookYaw({body[8], body[9], body[10]}) : 0.f;
                        hin.driving = hybridStick.drives;
                        hin.steerable = hybridOwner.domain != hybrid_owner::Domain::vr && swingFresh &&
                                        swingState.grounded && !motion.onSurface;
                        hybridSteerable = hin.steerable;
                        const auto ho = hybridHeading.update(hin);
                        hybridHeroTurn = ho.heroTurn;
                        if (ho.released != hybrid_movement::Heading::Hold::none)
                            diag_log::writef("hybrid heading: %s hold %s after %d ms, heading %.0f hero %.0f",
                                             hybrid_movement::holdName(ho.released),
                                             ho.gaveUp ? "GAVE UP (view rejoins the head)"
                                             : ho.kept ? "timed out on his feet (heading and view stay; he stays)"
                                                       : "arrived",
                                             static_cast<int>(hybridHeading.age * 1000 + .5f),
                                             hybridHeading.yaw * toDeg, hin.hero * toDeg);
                        if (ho.unanchored)
                            diag_log::writef("hybrid heading: the game turns him (parkour/wall/air); it follows him "
                                             "again (heading %.0f hero %.0f)", hybridHeading.yaw * toDeg, hin.hero * toDeg);
                        viewLock.yaw = hybridHeading.yaw;
                    }
                    {
                        const bool hybridNow =
                            hybrid_movement::modeOf(values.movementMode) == hybrid_movement::Mode::hybrid;
                        view_yaw_lock::State lock;
                        lock.on = viewLockActive && viewLock.set;
                        lock.yaw = viewLock.yaw;
                        lock.sway = hybridNow ? hybridHeading.swayWeight : view_yaw_lock::swayOf(values.animYawSwayPct);
                        lock.heroYaw = std::hypot(body[8], body[10]) > .2f
                                           ? hybrid_handoff::actorLookYaw({body[8], body[9], body[10]})
                                           : NAN; // no facing: no sway term

                        view_yaw_lock::publish(lock);
                    }
                    // H1: the hands' velocities, in tracking space.
                    for (unsigned i = 0; i < 2; ++i)
                        armVelocity[i].update(frame.hands[i].grip.position,
                                              frame.hands[i].valid && validTrackedPose(frame.hands[i].grip),
                                              frame.predictedDisplayTime, armEpoch);
                    armLead = arm_frame::leadSeconds(native_body::handLeadWait());
                    if (bodyStarted && values.animatedHybrid && values.animatedArms &&
                        animatedMovesHandoff.yielding && animHeadRefSet && validTrackedPose(frame.head) &&
                        armFrameWas != animated_view::Frame::none && armFrameWas != animated_view::Frame::missing) {
                        Mat4 headWorld{};
                        Quat level = armLevelWas;
                        if (armFrameWas == animated_view::Frame::level) {
                            float levelYaw = 0;
                            if (game_fp::sceneYaw(true, levelYaw))
                                level = Quat::yaw(levelYaw); // M1: this frame's heading
                        }
                        if (native_body::viewAnchorWorld(headWorld) == 1 &&
                            arm_frame::worldAnchor(armFrameWas, headWorld, level, armWorldEyesWas,
                                                   armView.anchor)) {
                            // FULL head lock on the stable heading: turned as the eyes are.
                            if (armFrameWas == animated_view::Frame::full)
                                view_yaw_lock::turnHead(armView.anchor, view_yaw_lock::live());
                            // CAMERA MOTION: the hands map onto the head the eyes are on
                            // (its tilt, HEAD ANGLE, the bob's shift), so they stay where
                            // the player's hands are in the view.
                            if (armFrameWas != animated_view::Frame::world) {
                                // The amounts in force (published below last frame:
                                // the identity unless the eyes are on the animated head).
                                armView.anchor = camera_motion::stabilize(armView.anchor, camera_motion::live());
                                armView.anchor[12] += cameraBobShift.x;
                                armView.anchor[13] += cameraBobShift.y;
                                armView.anchor[14] += cameraBobShift.z;
                            }
                            const bool world = armFrameWas == animated_view::Frame::world;
                            armView.reference = world ? frame.head : animHeadRef;
                            armView.turn = world ? 0.f : animHeadTurn;
                            armView.valid = true;
                        }
                    }
                    // BODY KINEMATICS: this frame's snapshot (the single source the
                    // web, ball, marker and punch aims below are taken from in animation).
                    kinematics::Snapshot kin;
                    kin.ns = frame.predictedDisplayTime > 0 ? frame.predictedDisplayTime
                                                            : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                    kin.owner = hybridOwner.domain == hybrid_owner::Domain::vr ? 1u : 0u;
                    kin.epoch = armEpoch;
                    if (armView.valid) {
                        kin.head = arm_frame::toGame(armView, frame.head);
                        kin.headValid = true;
                        kin.viewYaw = arm_frame::trackingYaw(armView, frame.head);
                    } else if (motion.active) {
                        kin.head = motion.headPose;
                        kin.headValid = finite(kin.head.position);
                        kin.viewYaw = motion.swing.trackingYaw;
                    }
                    for (unsigned i = 0; i < 2; ++i) {
                        if (armView.valid && frame.hands[i].valid && validTrackedPose(frame.hands[i].grip)) {
                            // M2: led by the SAME lead and velocity the glove gets in the
                            // pose job, so glove, marker and line start agree.
                            Pose led = frame.hands[i].grip;
                            led.position = led.position + arm_frame::clampSpeed(armVelocity[i].value()) * armLead;
                            kin.grip[i] = arm_frame::toGame(armView, led);
                            kin.aim[i] = webShooter(kin.grip[i], i);
                            kin.handValid[i] = true;
                        } else if (motion.active && motion.swing.hands[i].tracked) {
                            kin.grip[i] = motion.grips[i];
                            kin.aim[i] = motion.swing.hands[i].aim;
                            kin.handValid[i] = finite(kin.grip[i].position);
                        }
                    }
                    animation_cancel::Decision gripCancel;
                    if (swingStarted) {
                        unsigned webButtons{}, shotButtons{}, trackedHands{};
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& h = motion.swing.hands[i];
                            webButtons |= (h.grip > .5f ? 1u : 0u) << i;
                            shotButtons |= (h.trigger > .5f ? 1u : 0u) << i;
                            trackedHands |= (h.tracked ? 1u : 0u) << i;
                        }
                        // OWNERSHIP UPDATE: grip cancels only a midair animation; on
                        // the ground (or the game's crawl) grip is the arms' web.
                        const bool cancelSupported = swingFresh &&
                            hybrid_owner::supported(head_probe::StateInputs{
                                true, true, swingState.grounded != 0, false, false, false, false,
                                false, false, swingState.wall});
                        gripCancel = gripCancelGate.update(
                            animation_cancel::enabled(bodyStarted, values.animatedHybrid,
                                                      values.animatedMoves, calibrationInput),
                            hybrid_owner::cancelArmed(hybridOwner.domain == hybrid_owner::Domain::anim,
                                                      cancelSupported),
                            motion.active && !flatScreen && controlsVisible && spiderMan,
                            webButtons, shotButtons, trackedHands);
                        if (gripCancel.cancel) {
                            // The cancel is SpidyVR's on this frame (review M1 for this
                            // path), not at the owner's update further down.
                            if (values.animatedHybrid && values.animatedMoves &&
                                hybridOwner.breakForCancel(flipBreakState,
                                                           !flipBreakState.playing || !flipBreakState.fresh,
                                                           frame.predictedDisplayTime > 0
                                                               ? frame.predictedDisplayTime
                                                               : static_cast<int64_t>(GetTickCount64()) *
                                                                     1'000'000ll))
                                diag_log::writef("grip cancel: anim->vr this frame (display %lld ns)",
                                                 static_cast<long long>(frame.predictedDisplayTime));
                            if (!++gripCancelToken) ++gripCancelToken;
                            hybridZipPress = hybridJumpAsk = false;
                            stickPush.cancel();
                            zipPress.cancel();
                            diag_log::writef("hybrid: grip cancel consumed (no action), token %u fresh %u web %u shot %u tracked %u",
                                             gripCancelToken, swingFresh ? 1u : 0u, webButtons,
                                             shotButtons, trackedHands);
                        }
                        // Suppress before submitSwing: ownership is evaluated later in
                        // this loop, too late to prevent an outgoing grip web otherwise.
                        animation_cancel::suppress(motion.swing.hands, gripCancel);
                        if (setSwingPoseCoupling) {
                            game_swing::PoseCoupling pose;
                            pose.inertia = swing_coupling::scales(native_body::tuckWeight()).inertia;
                            setSwingPoseCoupling(&pose);
                        }
                        game_swing::Command input;
                        input.wallRotate = motion.wallRotate;
                        input.serial = ++serial;
                        input.animationCancel = gripCancelToken;
                        if (gripCancel.cancel) cancellationSerialFloor = input.serial;
                        input.focused = motion.active && !flatScreen && controlsVisible && spiderMan;
                        input.sampleTimeNs = frame.predictedDisplayTime > 0 ? frame.predictedDisplayTime : 1;
                        input.sampleSeconds =
                            std::isfinite(frame.seconds) && frame.seconds > 0 && frame.seconds <= .1f
                                ? frame.seconds
                                : 1.f / 90.f;
                        input.jump = motion.swing.jump;
                        input.move = motion.swing.move;
                        input.trackingYaw = motion.swing.trackingYaw;
                        input.tilt = motion.swing.tilt;
                        input.leap = input.focused ? leapOut : 0.f;
                        input.asks = input.focused && askingGame ? 1u : 0u;
                        // WEB LINES: the game's zip from a line (bit 4) and the dodge's
                        // button (B) for a dodge on a line (bit 5).
                        if (values.webLines && input.focused && askingZip)
                            input.asks |= 16u;
                        if (values.webLines && input.focused && (frame.buttons & buttonB) &&
                            !padLayer.latched() && !calibrationInput)
                            input.asks |= 32u;
                        // The click of the web tie and the web lines (WEB TIE, WEB LINES):
                        // the right stick alone, not half of clicking both (the pad layer,
                        // the pause), not an R3 the game asks for.
                        if ((values.webTie || values.webLines) && input.focused &&
                            frame.hands[1].stickClick && !frame.hands[0].stickClick && !padLayer.latched() &&
                            !(namedPress.claimed() & game_pad::NamedPress::rightClick))
                            input.asks |= 2u;
                        // COMBAT RESPONSIVE (off by default): in a fight the swing
                        // runs nothing of its own on the ground (Input::fighting).
                        if (input.focused && values.combatResponsive &&
                            (fightLately || moverTable == config.base + bridge::combatCamera))
                            input.asks |= 4u;
                        // SPIDY RUN JUMP (on by default): A out of the swing's run is
                        // the swing's own jump, at once (Input::runJump).
                        if (input.focused && values.spidyRunJump)
                            input.asks |= 8u;
                        if (const Vec3 look = motion.headPose.orientation.rotate({0, 0, -1});
                            input.focused && finite(look) && length(look) > .5f)
                            input.look = normalized(look);
                        // EYE AIM: where the eyes look, for the web balls they aim
                        // and for what the aim previews say they meet (the zip's
                        // camera is turned onto that).
                        if (const Vec3 gaze = motion.gazePose.orientation.rotate({0, 0, -1});
                            values.eyeAim && input.focused && motion.gazeValid && finite(gaze) &&
                            length(gaze) > .5f) {
                            input.gazeOrigin = motion.gazePose.position;
                            input.gaze = normalized(gaze);
                            input.eyeAim = (values.eyeAim & vr_settings::eyeAimShots) ? 1u : 0u;
                        }
                        const Input climbInput = wallClimbInput(motion);
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = climbInput.hands[i];
                            input.hands[i] = {hand.aim, hand.gripRelativeToHead, hand.tracked, hand.trigger,
                                              hand.grip};
                            input.wallPositions[i] = motion.grips[i].position;
                            input.wallGrips[i] = hand.tracked &&
                                !((gripCancel.webBlocked | gripCancel.shotBlocked) & (1u << i))
                                    ? frame.hands[i].squeeze : 0.f;
                        }
                        const bool nativeGroundNext = swingFresh &&
                            (swingState.grounded || swingState.wall == hybrid_owner::surfaceGame) &&
                            swingState.wall != hybrid_owner::surfaceWall && !gripCancel.holdVr;
                        const bool animationActions = animation_cancel::nativeActions(
                            values.animatedHybrid && values.animatedMoves,
                            animatedMovesHandoff.yielding, gripCancel.holdVr, nativeGroundNext);
                        // ANIMATED FULL ARMS (default on): the arms keep every SpidyVR
                        // control in animation - punches may open a web strike (and
                        // its lunge) instead of contact-only blows.
                        input.actionMode =
                            animationActions && !strikeHold.fullArms(values.animatedFullArms) ? 1u : 0u;
                        // Release restores ground animation later in THIS loop. Do not
                        // allow a one-sample original-VR lunge/rig shot before that
                        // ownership transaction has a matching displayed hand solve.
                        if (animationActions && (!animatedMovesHandoff.yielding || !values.animatedArms)) {
                            for (unsigned i = 0; i < 2; ++i) if (input.hands[i].grip <= .5f) {
                                input.hands[i].tracked = 0;
                                motion.swing.hands[i].tracked = false;
                            }
                        }
                        if (gripCancel.webBlocked || gripCancel.shotBlocked) input.wallRotate = 0;
                        if (gripCancel.cancel) {
                            input.asks = input.jump = 0;
                            input.leap = 0;
                        }
                        native_body::ActionFrame handFrame;
                        bool handFrameRead{};
                        const auto sampleHandFrame = [&]() -> const native_body::ActionFrame& {
                            if (!handFrameRead) {
                                handFrame = native_body::actionFrame();
                                handFrameRead = true;
                            }
                            return handFrame;
                        };
                        // Native Animation: gloves sit in the head-bone frame while the
                        // animation owns. Aim web balls and markers from those same
                        // gloves. Grip cancel is already consumed above; it never maps
                        // or dispatches a web. An existing attached air web remains an
                        // arm action when returning to native ground movement.
                        if (animationActions && values.animatedArms && animatedMovesHandoff.yielding) {
                            nativeActionSample = true;
                            const auto& overlay = sampleHandFrame().tracking;
                            const bool current = overlay.targetFrame != animated_view::Frame::none &&
                                animation_cancel::validPose(overlay.serial, actionFrameFloor,
                                                           overlay.deadlineMs, GetTickCount64());
                            // Look/gaze are world-space targeting inputs, not raw IK.
                            // Use the same pose job/root and owner lease as the hands.
                            actionPoseSerial = overlay.serial;
                            actionPoseDeadline = overlay.deadlineMs;
                            // The head the actions use: the per-frame mapped head when the
                            // arms are mapped (ARMS IN ANIMATION), else the pose job's.
                            if (armView.valid && kin.headValid) {
                                actionHead = kin.head;
                                actionHeadValid = true;
                            } else {
                                actionHeadValid = current && overlay.headValid &&
                                    worldDisplayedPose(actionHead, overlay.mappedHead, overlay.root);
                            }
                            // Review H2: the physical gaze is transported ONCE onto that
                            // head, and gaze, its origin and EYE AIM are rebuilt from it.
                            input.look = {};
                            input.eyeAim = 0;
                            input.gaze = {};
                            input.gazeOrigin = {};
                            if (actionHeadValid) {
                                if (input.focused)
                                    input.look = actionHead.orientation.rotate({0,0,-1});
                                motion.gazePose = feedback_frame::transport(motion.headPose, actionHead, physicalGaze);
                                motion.gazeValid = physicalGazeValid;
                                if (const Vec3 gaze = motion.gazePose.orientation.rotate({0, 0, -1});
                                    values.eyeAim && input.focused && physicalGazeValid && finite(gaze) &&
                                    finite(motion.gazePose.position) && length(gaze) > .5f) {
                                    input.gazeOrigin = motion.gazePose.position;
                                    input.gaze = normalized(gaze);
                                    input.eyeAim = (values.eyeAim & vr_settings::eyeAimShots) ? 1u : 0u;
                                }
                            } else {
                                motion.gazePose = physicalGaze;
                                motion.gazeValid = false;
                            }
                            // Review M5: contact-only arms (FULL ARMS off) need the glove to
                            // be on the hand, as before: a contact is the drawn glove's.
                            const bool contactOnly = !strikeHold.fullArms(values.animatedFullArms);
                            for (unsigned i = 0; i < 2; ++i)
                                kin.handResidual[i] = current && overlay.valid[i] && overlay.actualValid[i]
                                                          ? overlay.residual[i]
                                                          : -1.f; // L2
                            if (armView.valid) {
                                // ARMS IN ANIMATION: the tracked hands, mapped this frame
                                // (kinematics snapshot): webs - held ones too -, balls,
                                // markers and punches leave from where the player's hand
                                // is in the game. The glove is solved onto the very same
                                // pose; the line starts there.
                                input.trackingYaw = kin.viewYaw;
                                for (unsigned i = 0; i < 2; ++i) {
                                    if (!input.hands[i].tracked)
                                        continue;
                                    if (!kin.handValid[i]) {
                                        if (input.hands[i].grip <= .5f && !gripCancel.cancel) {
                                            input.hands[i].tracked = 0;
                                            motion.swing.hands[i].tracked = false;
                                        }
                                        continue;
                                    }
                                    actionGrips[i] = kin.grip[i];
                                    actionGripMask |= 1u << i;
                                    input.hands[i].aim = kin.aim[i];
                                    motion.swing.hands[i].aim = kin.aim[i];
                                    input.physicalToContact[i] = {}; // the tracked hand is the contact
                                    const bool gloveOn = !contactOnly ||
                                        (current && overlay.valid[i] && overlay.actualValid[i] &&
                                         overlay.residual[i] >= 0 && overlay.residual[i] <= web_aim::forearmMiss &&
                                         overlay.armWeight[i] > .85f);
                                    if (gloveOn)
                                        input.actionMode |= animationActions ? (2u << i) : 0u;
                                }
                            } else {
                                for (unsigned i = 0; i < 2; ++i) {
                                    if (!input.hands[i].tracked) continue;
                                    if (!current || !overlay.valid[i] || !overlay.actualValid[i]) {
                                        // Never dispatch a free-hand animation action using a
                                        // prior owner's pose or the unrelated tracking rig.
                                        if (input.hands[i].grip <= .5f && !gripCancel.cancel) {
                                            input.hands[i].tracked = 0;
                                            motion.swing.hands[i].tracked = false;
                                        }
                                        continue;
                                    }
                                    // Held-web force/anchor aim stays unchanged, but its
                                    // rendered endpoint must still meet the actual hand.
                                    if (worldDisplayedPose(actionGrips[i], overlay.actualGrip[i], overlay.root))
                                        actionGripMask |= 1u << i;
                                    if (mapAimToDisplayedGrip(input.hands[i].aim,
                                                              overlay.actualGrip[i],
                                                              overlay.root, i, input.hands[i].grip)) {
                                        motion.swing.hands[i].aim = input.hands[i].aim;
                                        input.physicalToContact[i] = overlay.actualGrip[i].orientation *
                                            overlay.sourceGripOri[i].conjugate();
                                        if (overlay.residual[i] >= 0 && overlay.residual[i] <= web_aim::forearmMiss &&
                                            overlay.armWeight[i] > .85f)
                                            input.actionMode |= animationActions ? (2u << i) : 0u;
                                    } else if (input.hands[i].grip <= .5f) {
                                        // Rejected world anchor/orientation is not permission
                                        // to fire the unrelated original tracking-rig pose.
                                        input.hands[i].tracked = 0;
                                        motion.swing.hands[i].tracked = false;
                                    }
                                }
                            }
                        }
                        // WEB AIM: MODEL FOREARM (off by default): each hand's webs turn
                        // from the controller's line onto the body's forearm, carried on
                        // the controller's pose now (web_aim::forearmTurn), smoothed, and
                        // back to the controller's line whenever the body cannot say. The
                        // swing puts the turn on the webs' line alone (WebTurns).
                        // WRIST SHOOTERS (v6): parallel to the forearm, the same turns.
                        // SHOOTERS: LOCKED TO ARM (shooter_lock.hpp; November 11: "my model
                        // forearm wrist shooters aren't working at all"): the old WEB AIM /
                        // WRIST SHOOTERS keys read as one choice, the visible arm used in
                        // Native Animation too, the line exactly on the forearm.
                        if (shooter_lock::mode(values) == shooter_lock::Mode::arm && swingWebTurns) {
                            using web_aim::Fallback;
                            const auto& snapshot = sampleHandFrame();
                            const auto& arms = snapshot.forearms;
                            const auto& overlay = snapshot.tracking;
                            kinematicsArms = arms;
                            forearmStaleFrames = arms.job != forearmJob ? 0u : forearmStaleFrames + 1;
                            forearmJob = arms.job;
                            const float dt = input.sampleSeconds;
                            forearmLogSeconds += dt;
                            const float forearmLogEvery =
                                animatedMovesHandoff.yielding && values.animatedArms ? .2f : 2.f;
                            const bool logNow = forearmLogSeconds >= forearmLogEvery;
                            game_swing::WebTurns turns;
                            turns.active = 1;
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& f = arms.arms[i];
                                Fallback why = Fallback::none;
                                if (!values.body || flatScreen)
                                    why = Fallback::off;
                                else if (!input.focused)
                                    why = Fallback::gameCamera;
                                else if (!input.hands[i].tracked)
                                    why = Fallback::untracked;
                                // ARMS IN ANIMATION: before November 11 the web was refused
                                // the arm here whenever armView was valid (a contorted pose
                                // of the ANIMATION's forearm bent it on Nov 9 22:27), so in
                                // Native Animation LOCKED TO ARM never followed the arm: 2972
                                // of 5768 "spherical aim:" lines on Nov 11 22:38 said "arms
                                // overlay" with trust 0. The arm drawn now is the SpidyVR arm
                                // solved onto the tracked hand (the overlay, faded in below),
                                // so it is the one to follow; the wrist miss and the overlay's
                                // weight still guard it.
                                else if (!native_body::drawn() || !f.valid)
                                    why = Fallback::notSolved;
                                else if (forearmStaleFrames > web_aim::forearmStaleFrames ||
                                         (values.animatedHybrid &&
                                          !animation_cancel::validPose(overlay.serial, actionFrameFloor,
                                                                     overlay.deadlineMs, GetTickCount64())))
                                    why = Fallback::stale;
                                else if (!(f.handError >= 0 && f.handError <= web_aim::forearmMiss))
                                    why = Fallback::wristMiss;
                                // A flip's tilt is no fallback since v6: the solve turns
                                // the whole body back with it (body_ik step 7) and the
                                // wrist miss check still guards the sample. V5 fell back
                                // in 492 of 1134 forearm aim: lines for it.
                                const Quat aimNow = input.hands[i].aim.orientation;
                                const Quat solvedAim =
                                    overlay.targetFrame != animated_view::Frame::none && overlay.valid[i]
                                        ? overlay.aim[i]
                                        : aimNow;
                                Quat target{};
                                float alignDeg = -1.f;
                                float signedDeg = 0.f;
                                if (why == Fallback::none || f.valid) {
                                    const Vec3 forearmDir = normalized(f.wrist - f.elbow);
                                    alignDeg =
                                        std::acos(std::clamp(dot(forearmDir, solvedAim.rotate({0, 0, -1})),
                                                             -1.f, 1.f)) *
                                        57.29578f;
                                    signedDeg =
                                        web_aim::signedPalmAngle(forearmDir, aimNow, static_cast<int>(i)) *
                                        57.29578f;
                                }
                                // A large forearm-vs-controller angle is no longer a
                                // fallback: the world-frame clamp keeps the web on the
                                // forearm instead of snapping to the controller.
                                // ANIMATED ARMS yield: wait only until the overlay
                                // weight is up; do not require the old 45° align check.
                                if (why == Fallback::none && animatedMovesHandoff.yielding &&
                                    values.animatedArms) {
                                    if (animatedArmsState.arms[i].weight <= .85f)
                                        why = Fallback::armsOverlay;
                                }
                                // 360 DEGREE SPHERICAL AIM (spherical_aim.hpp): the solve's
                                // forearm where it is trusted, else the forearm learned in
                                // the controller's own frame, carried on the controller now
                                // - good behind, above and below - eased between the two.
                                // Where the solve cannot say (stale, not solved, out of
                                // reach, the animation's arms) the model keeps the web on
                                // the same forearm line rather than jumping to the
                                // controller's. Body off, the game's camera and an
                                // untracked hand fall back as before.
                                const bool solvedOk = why == Fallback::none;
                                const bool modelAllowed = values.sphericalAim &&
                                    (solvedOk || why == Fallback::notSolved || why == Fallback::stale ||
                                     why == Fallback::wristMiss || why == Fallback::armsOverlay ||
                                     why == Fallback::misaligned);
                                const Vec3 solvedDir = solvedOk ? normalized(f.wrist - f.elbow) : Vec3{};
                                const bool modelReady = forearmModels[i].ready();
                                const float offModel = modelReady && solvedOk
                                    ? spherical_aim::angleBetween(solvedDir, forearmModels[i].world(solvedAim)) *
                                          57.29578f
                                    : -1.f;
                                const float trustNow =
                                    solvedOk ? spherical_aim::trust(f.handError, offModel, modelReady) : 0.f;
                                // Learned in animation too: the arm is the tracked hand's.
                                if (values.sphericalAim && solvedOk &&
                                    trustNow >= spherical_aim::learnTrust)
                                    forearmModels[i].learn(solvedDir, solvedAim, dt);
                                Vec3 webLine = solvedDir;
                                if (modelAllowed)
                                    webLine = forearmBlends[i].update(
                                        solvedDir, forearmModels[i].world(aimNow), trustNow, dt);
                                else
                                    forearmBlends[i].reset();
                                const bool haveLine = spherical_aim::unitish(webLine);
                                if (haveLine) {
                                    // Exactly on the forearm: no inward wrist cap (0 degrees).
                                    target = web_aim::forearmWorldTurn(webLine, aimNow, static_cast<int>(i), 0.f);
                                    if (forearmAims[i].primed)
                                        forearmJitter[i] = std::max(
                                            forearmJitter[i],
                                            web_aim::turnAngle(forearmTargets[i].conjugate() * target) *
                                                57.29578f);
                                    forearmTargets[i] = target;
                                }
                                const float limit = 0.f; // LOCKED TO ARM: no wrist cap
                                const bool clamped = false;
                                // 180: do not pull the turn back toward the controller.
                                forearmAims[i].update(haveLine, target, dt, 180.f);
                                // WRIST SHOOTERS without MODEL FOREARM takes all of it.
                                webTurnsNow[i] = forearmAims[i].turn(shooter_lock::strength(values));
                                turns.turns[i] = webTurnsNow[i];
                                // In the diagnostics only as it changes
                                // (diag_log::ChangeOnly); the timeline as before.
                                const bool diagNow = forearmDiag[i].update(
                                    dt, static_cast<int>(why), web_aim::turnAngle(webTurnsNow[i]) * 57.29578f);
                                if (logNow || diagNow) {
                                    // The unknowns, to send back: the forearm against the
                                    // controller's line in the same world (a frame mismatch
                                    // shows as a large or yaw-only angle), the grip the solve
                                    // had against the grip now (the solve's lag), which side
                                    // the palm faces (+ toward the aim's +X; the left palm is
                                    // expected +, the right -), and the jitter.
                                    const Vec3 forearm = normalized(f.wrist - f.elbow);
                                    const float off =
                                        std::acos(std::clamp(dot(forearm, solvedAim.rotate({0, 0, -1})), -1.f,
                                                             1.f)) *
                                        57.29578f;
                                    const float lag =
                                        web_aim::turnAngle(f.grip.conjugate() * motion.grips[i].orientation) *
                                        57.29578f;
                                    web_aim::ForearmAimLine fields;
                                    fields.right = i != 0;
                                    fields.reason = web_aim::fallbackName(why);
                                    fields.active = haveLine;
                                    fields.modelForearm =
                                        values.webAimSource == static_cast<int>(web_aim::Source::forearm);
                                    fields.wristShooters = values.wristShooters != 0;
                                    fields.turnDegrees = web_aim::turnAngle(webTurnsNow[i]) * 57.29578f;
                                    fields.targetDegrees = web_aim::turnAngle(target) * 57.29578f;
                                    fields.limitDegrees = limit;
                                    fields.clamped = clamped;
                                    fields.weight = forearmAims[i].weight;
                                    fields.blend = values.forearmBlend;
                                    fields.handError = f.handError;
                                    fields.stale = forearmStaleFrames;
                                    fields.offDegrees = f.valid ? off : -1.f;
                                    fields.lagDegrees = f.valid ? lag : -1.f;
                                    fields.palmX = dot(f.palm, aimNow.rotate({1, 0, 0}));
                                    fields.jitterDegrees = forearmJitter[i];
                                    fields.tilted = arms.tilted;
                                    char line[480];
                                    web_aim::formatForearmAimLine(line, sizeof(line), fields);
                                    if (logNow)
                                        game_prompts::note(line, false);
                                    if (diagNow)
                                        diag_log::write(line);
                                    if (logNow || diagNow)
                                        diag_log::writef(
                                            "forearm sign: %s %+.1f deg (positive toward palm, treated as "
                                            "out; inward cap %.0f)%s",
                                            i ? "R" : "L", static_cast<double>(signedDeg),
                                            static_cast<double>(limit),
                                            clamped ? ", inward capped" : "");
                                    if (logNow || diagNow)
                                        diag_log::writef(
                                            "spherical aim: %s %s, trust %.2f (target %.2f), model %s "
                                            "(%.2f s), solve off model %.0f deg, line off controller %.0f deg",
                                            i ? "R" : "L", web_aim::fallbackName(why),
                                            static_cast<double>(forearmBlends[i].trust),
                                            static_cast<double>(trustNow),
                                            !values.sphericalAim ? "off"
                                            : modelReady         ? "ready"
                                                                 : "learning",
                                            static_cast<double>(forearmModels[i].learned),
                                            static_cast<double>(offModel),
                                            haveLine ? static_cast<double>(
                                                           spherical_aim::angleBetween(
                                                               webLine, aimNow.rotate({0, 0, -1})) *
                                                           57.29578f)
                                                     : -1.0);
                                    if (logNow || diagNow)
                                        diag_log::writef(
                                            "shooter lock: %s %s, %s, strength %d%%, line off controller %.0f deg",
                                            i ? "R" : "L", shooter_lock::modeName(shooter_lock::mode(values)),
                                            !haveLine ? shooter_lock::sourceName(shooter_lock::Source::none)
                                            : solvedOk && forearmBlends[i].trust > .5f
                                                ? shooter_lock::sourceName(shooter_lock::Source::visibleArm)
                                                : shooter_lock::sourceName(shooter_lock::Source::learnedArm),
                                            shooter_lock::strength(values),
                                            haveLine ? static_cast<double>(spherical_aim::angleBetween(
                                                           webLine, aimNow.rotate({0, 0, -1})) *
                                                       57.29578f)
                                                     : -1.0);
                                    if (logNow)
                                        forearmJitter[i] = 0;
                                }
                            }
                            if (logNow)
                                forearmLogSeconds = 0;
                            swingWebTurns(&turns);
                            webTurnsOn = true;
                        } else if (webTurnsOn) {
                            // Off again: the controller's line at once.
                            for (auto& a : forearmAims)
                                a.reset();
                            for (auto& b : forearmBlends)
                                b.reset();
                            webTurnsNow = {};
                            game_swing::WebTurns turns;
                            if (swingWebTurns)
                                swingWebTurns(&turns);
                            webTurnsOn = false;
                        }
                        // WRIST MARKER SMOOTHING: the final aim of each free hand (OG rig or
                        // the animation's gloves, whichever was written above) is steadied
                        // here, once, so the swing's previews (the markers) and its shots
                        // and web balls come from the same steadied line. A held web keeps
                        // its raw aim (its pull is force, not a shot).
                        {
                            const Quat aimTurn = Quat::yaw(input.trackingYaw) * input.tilt;
                            for (unsigned i = 0; i < 2; ++i) {
                                if (!input.focused || !input.hands[i].tracked) {
                                    aimSteady[i].reset();
                                    continue;
                                }
                                const Vec3 steadied = aimSteady[i].update(
                                    input.hands[i].aim.orientation.rotate({0, 0, -1}), aimTurn,
                                    input.sampleSeconds);
                                if (swingState.webs[i].attached)
                                    continue;
                                input.hands[i].aim = aim_filter::steer(input.hands[i].aim, steadied);
                                motion.swing.hands[i].aim = input.hands[i].aim;
                            }
                        }
                        // POINT JUMP aims where the actions' head looks (next frame's fire).
                        pointJumpLook = input.look;
                        check(submitSwing(&input), "Tracked swing input");
                        check(sampleSwing(&swingState), "Native swing feedback");
                        game_swing::SlingshotData slingshot;
                        slingshotPulseSeconds += frame.seconds;
                        if ((values.webSlingshot || values.airSlingshot) && input.focused &&
                            sampleSlingshot && !sampleSlingshot(&slingshot) &&
                            slingshot.magic == 0x53534c47 && slingshot.version == 1 &&
                            slingshot.bytes == sizeof(slingshot) && slingshot.charge > 0 &&
                            slingshot.charge <= 1) {
                            if (slingshotPulseSeconds >= .06f) {
                                for (unsigned i = 0; i < 2; ++i)
                                    runtime.haptic(i, .15f + .65f * slingshot.charge, .045f);
                                slingshotPulseSeconds = 0;
                            }
                        } else {
                            slingshotPulseSeconds = .06f;
                        }
                        if (!sampleClimb || sampleClimb(&climbState) || climbState.magic != 0x53434c44 ||
                            climbState.version != 2 || climbState.bytes != sizeof(climbState))
                            climbState = {};
                        const unsigned wallPulses =
                            climbPulses.update(climbState, input.focused && values.handClimb);
                        for (unsigned i = 0; i < 2; ++i)
                            if (wallPulses & (1u << i)) {
                                runtime.haptic(i, .45f, .045f);
                                feel(body_haptics::Kind::wallGrip, static_cast<int>(i));
                            }
                        if (!sampleWebFlight || sampleWebFlight(&webFlight)) {
                            webFlight = {};
                            for (unsigned i = 0; i < 2; ++i) {
                                webFlight.phases[i] = swingState.webs[i].attached ? 3 : 0;
                                webFlight.coupling[i] = 1;
                            }
                        }
                        if (!sampleBends || sampleBends(&swingBends))
                            swingBends = {};
                        // Nothing here changes who holds the player on a wall
                        // behind his back. Every way of doing it from the room
                        // around him dropped him (October 14: switching the
                        // walls off, refusing the next wall of a climb, and a
                        // push into the wall with the walls off, each a fall of
                        // up to 7.5 m), and the player put the reason plainly:
                        // "the shaft shouldnt affect anything, the vent in the
                        // top is the target, there are similiar events in the
                        // game with vents outside". The room is still read, for
                        // the report.
                        check(swingState.error, "Native swinging");
                        if (!sampleGrab || sampleGrab(&grabState))
                            grabState = {};
                        {
                            game_grab::TieData sample;
                            const game_grab::TieData expected;
                            if (values.webTie && sampleTie && !sampleTie(&sample) &&
                                sample.magic == expected.magic && sample.version == expected.version &&
                                sample.bytes == expected.bytes)
                                tieState = sample;
                            else
                                tieState = {};
                            const bool armedNow = input.focused && tieState.armed != 0;
                            if (armedNow && !tieArmedBefore) {
                                runtime.haptic(0, .25f);
                                runtime.haptic(1, .25f);
                            }
                            if (tieState.made > tiesMadeBefore && tiesMadeBefore) {
                                runtime.haptic(0, .9f);
                                runtime.haptic(1, .9f);
                            }
                            tieArmedBefore = armedNow;
                            if (tieState.made)
                                tiesMadeBefore = tieState.made;
                        }
                        {
                            game_lines::LineData sample;
                            const game_lines::LineData expected;
                            if (values.webLines && sampleLine && !sampleLine(&sample) &&
                                sample.magic == expected.magic && sample.version == expected.version &&
                                sample.bytes == expected.bytes)
                                lineState = sample;
                            else
                                lineState = {};
                            const bool armedNow = input.focused && lineState.armed != 0;
                            if (armedNow && !lineArmedBefore) {
                                runtime.haptic(0, .25f);
                                runtime.haptic(1, .25f);
                            }
                            if (linesSeen && lineState.made > linesMadeBefore) {
                                runtime.haptic(0, .9f);
                                runtime.haptic(1, .9f);
                            }
                            lineArmedBefore = armedNow;
                            const bool markedNow = input.focused && lineState.targetId != 0;
                            if (markedNow && !lineMarkedBefore)
                                runtime.haptic(1, .2f);
                            lineMarkedBefore = markedNow;
                            if (lineState.status) {
                                linesSeen = true;
                                linesMadeBefore = lineState.made;
                            }
                        }
                        // What each free hand's web button would do: the aim markers.
                        aims = aimed = {};
                        if (sampleAim && input.focused) {
                            game_swing::AimData sample;
                            const game_swing::AimData expected;
                            if (!sampleAim(&sample) && sample.magic == expected.magic &&
                                sample.version == expected.version && sample.bytes == expected.bytes &&
                                sample.status == 2)
                                aimed = sample;
                        }
                        if (values.aimMarkers)
                            aims = aimed;
                        // A free hand come onto the other hand's web: a light
                        // tick, a press here takes hold of that web.
                        for (unsigned i = 0; i < 2; ++i) {
                            const bool on =
                                aimed.hands[i].kind == static_cast<uint32_t>(game_swing::AimKind::strand);
                            if (on && !onStrandBefore[i] &&
                                frame.predictedDisplayTime - strandTickAt[i] > 400'000'000) {
                                runtime.haptic(i, .2f);
                                strandTickAt[i] = frame.predictedDisplayTime;
                            }
                            onStrandBefore[i] = on;
                        }
                        for (unsigned i = 0; i < 2; ++i)
                            flightVisual[i].update(frame.seconds, webFlight.phases[i], webFlight.durations[i],
                                                   webFlight.progress[i], webFlight.tips[i],
                                                   swingState.webs[i].anchor);
                        bool trails[2]{};
                        Vec3 trailEnds[2]{};
                        // The hands whose webs a zip pulled: one zip on the body, in the arm of
                        // each (both: a web held in both hands).
                        unsigned zipHands{};
                        for (unsigned i = 0; i < 2; ++i) {
                            // A web that caught a thug ends on him, wherever he goes.
                            const auto& held = grabState.hands[i];
                            const bool grabbing = input.focused && held.phase != 0;
                            const bool wasGrabbing = grabPhases[i] != 0;
                            const int64_t displayAt = frame.predictedDisplayTime;
                            if (grabbing) {
                                swingState.webs[i] = {1, 0, held.end, held.length, held.taut ? 1.f : 0.f};
                                // Caught, yanked, at the hand: each its own pulse.
                                const auto phase = static_cast<GrabPhase>(held.phase);
                                if (held.phase != grabPhases[i]) {
                                    runtime.haptic(i, phase == GrabPhase::Yanked ? 1.f
                                                      : phase == GrabPhase::Held ? .4f
                                                                                 : .55f);
                                    pulseUntil[i] = displayAt + 30'000'000;
                                    feel(phase == GrabPhase::Yanked ? body_haptics::Kind::grabYanked
                                         : phase == GrabPhase::Held ? body_haptics::Kind::grabHeld
                                                                    : body_haptics::Kind::grabCaught,
                                         static_cast<int>(i));
                                } else if (held.tension > .05f && displayAt >= pulseUntil[i]) {
                                    // Between pulses the hand feels the web pull: a
                                    // hanging trash can faintly, one swung hard or too
                                    // heavy to lift strongly.
                                    runtime.haptic(i, std::min(.6f, .6f * held.tension));
                                }
                            } else if (wasGrabbing) {
                                runtime.haptic(i, .6f); // thrown or let go
                                feel(body_haptics::Kind::grabLetGo, static_cast<int>(i));
                            }
                            grabPhases[i] = grabbing ? held.phase : 0;
                            // The web let go of trails what it held as it dissolves.
                            trails[i] = input.focused && !grabbing && held.trailing;
                            trailEnds[i] = held.end;
                            const bool attached =
                                input.focused && swingState.status && swingState.webs[i].attached;
                            // A web taken hold of, not shot: the hand closes on it,
                            // a softer pulse than a shot's.
                            const bool taken = attached && !grabbing && (swingState.webs[i].attached & 4);
                            // A web that flew (WEB SPEED) pulses as it arrives, not as it is shot.
                            const bool arrived = attached && !grabbing && webFlight.phases[i] >= 2;
                            if (arrived && !webArrivedBefore[i] && attachedBefore[i] && !taken) {
                                runtime.haptic(i, .8f);
                                pulseUntil[i] = displayAt + 60'000'000;
                                feel(body_haptics::Kind::webAttach, static_cast<int>(i));
                            }
                            webArrivedBefore[i] = arrived;
                            if (attached != attachedBefore[i] && !grabbing && !wasGrabbing) {
                                runtime.haptic(i, !attached                  ? .2f
                                                  : taken                    ? .4f
                                                  : webFlight.phases[i] == 1 ? .15f
                                                                             : .65f);
                                feel(!attached ? body_haptics::Kind::webRelease
                                     : taken   ? body_haptics::Kind::webTaken
                                               : body_haptics::Kind::webAttach,
                                     static_cast<int>(i));
                            }
                            if (attached && swingState.zips > zipBefore && !grabbing) {
                                runtime.haptic(i, 1);
                                zipHands |= 1u << i;
                            }
                            // Reeling in, the hand ticks like a winch (REEL HAPTICS, reel_haptics.hpp).
                            if (const auto reel =
                                    reelTicks[i].update(values.reelHaptics && attached && !grabbing &&
                                                            (swingState.webs[i].attached & 8),
                                                        swingState.webs[i].length);
                                reel && displayAt >= pulseUntil[i])
                                runtime.haptic(i, reel.strength, reel.seconds);
                            // Swinging on it taut, the hand ticks with the body's travel
                            // (SWING HAPTICS, swing_haptics.hpp); a reel's ticks go instead.
                            if (const auto swing = swingTicks[i].update(
                                    values.swingHaptics && attached && !grabbing &&
                                        !(swingState.webs[i].attached & 8) && !swingState.grounded &&
                                        swingState.webs[i].tension > .5f,
                                    length(swingState.velocity), frame.seconds);
                                swing && displayAt >= pulseUntil[i])
                                runtime.haptic(i, swing.strength, swing.seconds);
                            attachedBefore[i] = attached;
                            webTaken[i] = taken;
                            webTimes.update(i, attached, swingState.webs[i].anchor,
                                            webShooter(motion.grips[i], i).position,
                                            frame.predictedDisplayTime);
                        }
                        zipBefore = swingState.zips;
                        if (zipHands) {
                            // On the body toward where the zipped webs pull it.
                            Vec3 toward{};
                            for (unsigned i = 0; i < 2; ++i)
                                if (zipHands & (1u << i))
                                    toward += swingState.webs[i].anchor - swingState.position;
                            feelFrom(body_haptics::Kind::zip, zipHands == 3 ? -1 : zipHands == 2 ? 1 : 0, 1, toward);
                        }
                        {
                            // One web in both hands is one of the game's ropes,
                            // from the hand that holds it nearer its end.
                            const bool shared = attachedBefore[0] && attachedBefore[1] &&
                                                (swingState.webs[0].attached & 2) &&
                                                (swingState.webs[1].attached & 2) && !grabPhases[0] && !grabPhases[1];
                            std::array<WebRopes::Hand, 2> held{};
                            for (unsigned i = 0; i < 2; ++i)
                                held[i] = {attachedBefore[i], webTaken[i], shared, swingState.webs[i].length,
                                           webTimes[i].attachedAt};
                            webRopes.update(held);
                        }
                        // A thug the web brought to a player in the air has
                        // arrived within reach: time slows, to hit him there.
                        // (Until the fifth build of October 12 it slowed as the
                        // yank began, with the thug still far off.)
                        if (grabState.catches > catchesBefore && input.focused &&
                            game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart)) {
                            moments.start(Moment::airPull);
                            runtime.haptic(0, .45f);
                            runtime.haptic(1, .45f);
                            feel(body_haptics::Kind::airPull);
                        }
                        catchesBefore = grabState.catches;
                        // Onto a wall, and off it with a jump: both hands feel it.
                        if (input.focused && swingState.status &&
                            (swingState.walls > wallsBefore || swingState.wallJumps > wallJumpsBefore)) {
                            const float pulse = swingState.wallJumps > wallJumpsBefore ? .6f : .45f;
                            runtime.haptic(0, pulse);
                            runtime.haptic(1, pulse);
                            // On the body's side toward the wall (behind its normal).
                            feelFrom(swingState.wallJumps > wallJumpsBefore ? body_haptics::Kind::wallJump
                                                                            : body_haptics::Kind::wallLand,
                                     -1, 1, -swingState.wallNormal);
                        }
                        wallsBefore = swingState.walls;
                        wallJumpsBefore = swingState.wallJumps;
                        // Every landed blow, a fist's or a web strike's: the hand
                        // that threw it kicks, as hard and as long as the blow was
                        // (a jab 60 ms, a blow that sends him flying 180 ms and the
                        // other hand with it); the game's time stops for a moment;
                        // a burst shows where it landed, with its count in the
                        // flow; and it sounds. Until October 11 a landed punch was
                        // one 25 ms pulse at half strength and nothing else.
                        game_punch::Strikes strikes{};
                        const game_punch::Strikes expectedStrikes;
                        const bool fighting =
                            punchStarted && sampleStrike && !sampleStrike(&strikes) &&
                            strikes.magic == expectedStrikes.magic && strikes.version == expectedStrikes.version &&
                            strikes.bytes == expectedStrikes.bytes;
                        strikeRead = fighting;
                        strikeUnderWay = fighting && strikes.phase != static_cast<uint32_t>(WebStrike::Phase::Idle);
                        strikeSequence = fighting ? strikes.sequence : strikeSequence;
                        if (fighting) {
                            // Punching switched off and on again counts from nothing.
                            blowsBefore = std::min(blowsBefore, strikes.blows);
                            strikesBefore = std::min(strikesBefore, strikes.opened);
                            knockoutsBefore = std::min(knockoutsBefore, strikes.knockouts);
                            const bool diving =
                                strikes.phase == static_cast<uint32_t>(WebStrike::Phase::Flying) &&
                                strikes.hand == static_cast<uint32_t>(WebStrike::diveHand);
                            if (strikes.opened > strikesBefore && input.focused) {
                                // Off to him: a tick in both hands, and the rush. A
                                // dive's is harder, and the last of its way slows.
                                runtime.haptic(0, diving ? .7f : .35f, diving ? .08f : .05f);
                                runtime.haptic(1, diving ? .7f : .35f, diving ? .08f : .05f);
                                feel(body_haptics::Kind::strikeRush);
                                // BLOW SOUNDS: OFF silences the rush as well.
                                if (values.blowSounds != 2)
                                    play(ImpactSound::Rush);
                                if (diving) {
                                    moments.start(Moment::dive);
                                    ++dives;
                                }
                            }
                            // The dive's legs kick out at the one it flies at (his
                            // chest, where the lock's mark is), and a moment after.
                            if (diving) {
                                diveAt = strikes.cueTarget == strikes.target && finite(strikes.cuePoint)
                                             ? strikes.cuePoint
                                             : strikes.destination;
                                kickUntil = frame.predictedDisplayTime + 250'000'000;
                            }
                            if (strikes.blows > blowsBefore && input.focused) {
                                const auto kind = static_cast<BlowKind>(strikes.lastKind);
                                const bool called = strikes.lastCalled != 0;
                                const bool finisher = kind == BlowKind::Finisher,
                                           heavy = kind != BlowKind::Hit || called;
                                // Time slows for a moment at the blows worth a look.
                                if (called)
                                    moments.start(Moment::called);
                                else if (finisher || kind == BlowKind::Launch || kind == BlowKind::Slam)
                                    moments.start(Moment::blow);
                                const unsigned hand = strikes.lastHand & 1;
                                const float strength =
                                    heavy ? 1.f
                                          : std::isfinite(strikes.lastStrength)
                                                ? std::clamp(strikes.lastStrength, 0.f, 1.f)
                                                : 0.f;
                                const float kick = finisher ? .32f : heavy ? .18f : .06f + .08f * strength;
                                runtime.haptic(hand, heavy ? 1.f : .6f + .4f * strength, kick);
                                if (heavy)
                                    runtime.haptic(1 - hand, finisher ? 1.f : .5f, finisher ? kick : .1f);
                                feel(finisher ? body_haptics::Kind::finisher
                                     : heavy  ? body_haptics::Kind::heavyBlow
                                              : body_haptics::Kind::punch,
                                     static_cast<int>(hand), strength);
                                for (unsigned i = 0; i < 2; ++i)
                                    if (i == hand || heavy)
                                        pulseUntil[i] = frame.predictedDisplayTime +
                                                        static_cast<int64_t>(kick * 1e9f) + 20'000'000;
                                hitStop.hit(strength, finisher);
                                if (finite(strikes.lastPoint)) {
                                    auto& burst = bursts[nextBurst++ % bursts.size()];
                                    burst.view = {strikes.lastPoint, strength, kind,
                                                  static_cast<int>(std::min(strikes.lastCount, 999u)), 0, called};
                                    burst.since = frame.predictedDisplayTime;
                                }
                                // BLOW SOUNDS: the game's own impact plays on him
                                // while it can (game_sound.hpp), then none of the
                                // headset's; SPIDY, the headset's; OFF, none.
                                const bool gameSounds =
                                    values.blowSounds == 0 && soundResults &&
                                    *static_cast<const volatile uint32_t*>(&soundResults->live) == 1;
                                if (values.blowSounds != 2 && !gameSounds) {
                                    play(finisher ? ImpactSound::Finisher
                                         : heavy ? ImpactSound::Heavy
                                         : strength >= .35f ? ImpactSound::Firm
                                                            : ImpactSound::Light);
                                    ++ownSounds;
                                }
                            }
                            // A knockout of the player's: time eases for an instant, both
                            // hands feel it, and it is lettered where he stood. This one is
                            // done.
                            if (strikes.knockouts > knockoutsBefore && input.focused &&
                                finite(strikes.knockoutPoint)) {
                                moments.start(Moment::knockout);
                                runtime.haptic(0, .8f, .09f);
                                runtime.haptic(1, .8f, .09f);
                                feel(body_haptics::Kind::knockout);
                                auto& mark = knockoutMarks[nextKnockout++ % knockoutMarks.size()];
                                mark.view = {strikes.knockoutPoint, 0};
                                mark.since = frame.predictedDisplayTime;
                            }
                            blowsBefore = strikes.blows;
                            strikesBefore = strikes.opened;
                            knockoutsBefore = strikes.knockouts;
                        }
                        // The mark over the thug the next punch would go to, with
                        // the arrow of the blow the flow calls. A new call ticks
                        // in both hands.
                        if (fighting && input.focused && strikes.cueTarget && finite(strikes.cuePoint)) {
                            const bool locked = strikes.cueLocked != 0;
                            if (strikes.cueTarget != cueTarget || strikes.cueWay != cueWay || locked != cueLocked)
                                cueSince = frame.predictedDisplayTime;
                            cueLocked = locked;
                            if (strikes.cueWay && strikes.cueWay != cueWay) {
                                runtime.haptic(0, .12f);
                                runtime.haptic(1, .12f);
                            }
                            cueTarget = strikes.cueTarget;
                            cueWay = strikes.cueWay;
                            cue = {strikes.cuePoint, static_cast<CueWay>(strikes.cueWay), strikes.cueFinisher != 0,
                                   strikes.cueLeft, 0, locked};
                        } else {
                            cue = {};
                            cueTarget = 0;
                            cueWay = 0;
                            cueLocked = false;
                        }
                        // Spider-Sense (spider_sense.hpp): a mark for each attacker the
                        // game warned the player of, from the display time it showed.
                        // A new one tingles in the hand on his side (both for one
                        // ahead or behind), a dodge's harder. A counter breaks its
                        // mark; time slows for a moment and both hands feel it.
                        game_sense::Data senseNow{};
                        const game_sense::Data expectedSense;
                        const bool sensing = punchStarted && sampleSense && !sampleSense(&senseNow) &&
                                             senseNow.magic == expectedSense.magic &&
                                             senseNow.version == expectedSense.version &&
                                             senseNow.bytes == expectedSense.bytes && senseNow.status == 2;
                        for (auto& mark : senseMarks)
                            mark.live = false;
                        if (sensing && input.focused) {
                            const Pose& eyes = actionHeadValid ? actionHead : motion.headPose;
                            for (uint32_t i = 0; i < std::min<uint32_t>(senseNow.count, 6); ++i) {
                                const auto& t = senseNow.threats[i];
                                if (!t.attacker || !finite(t.head) || !t.answer || t.answer > 3)
                                    continue;
                                SenseMark* mark{};
                                for (auto& m : senseMarks)
                                    if (m.attacker == t.attacker)
                                        mark = &m;
                                if (!mark) {
                                    for (auto& m : senseMarks)
                                        if (!m.attacker && !mark)
                                            mark = &m;
                                    if (!mark)
                                        continue;
                                    *mark = {};
                                    mark->attacker = t.attacker;
                                    mark->since = frame.predictedDisplayTime;
                                    ++senseShown;
                                    const Vec3 to = t.head - eyes.position;
                                    const Vec3 right = eyes.orientation.rotate({1, 0, 0});
                                    const bool placed = length(to) > 1e-3f && validTrackedPose(eyes) &&
                                                        (!nativeActionSample || actionHeadValid);
                                    const float side = placed ? dot(normalized(to), right) : 0.f;
                                    const bool dodge = t.answer == static_cast<uint8_t>(SenseAnswer::Dodge);
                                    const int64_t until = frame.predictedDisplayTime + 90'000'000;
                                    for (unsigned h = 0; h < 2; ++h)
                                        if ((h == 0 && side < .35f) || (h == 1 && side > -.35f)) {
                                            runtime.haptic(h, dodge ? .6f : .35f, dodge ? .07f : .045f);
                                            pulseUntil[h] = std::max(pulseUntil[h], until);
                                        }
                                    ++senseTingles;
                                    // In the head, from his side.
                                    feelFrom(dodge ? body_haptics::Kind::senseDodge : body_haptics::Kind::spiderSense,
                                             -1, 1, placed ? to : Vec3{});
                                }
                                mark->live = true;
                                mark->head = t.head;
                                mark->answer = static_cast<SenseAnswer>(t.answer);
                                mark->left = std::isfinite(t.left) ? std::clamp(t.left, 0.f, 1.f) : 1.f;
                                mark->timed = t.timed != 0;
                                if (t.countered && !mark->brokenAt) {
                                    mark->brokenAt = frame.predictedDisplayTime;
                                    ++senseBroken;
                                }
                            }
                            // The attack to land first, timed ones before the rest: a
                            // blow the player takes next comes from him (the hits below).
                            const game_sense::Threat* first{};
                            for (uint32_t i = 0; i < std::min<uint32_t>(senseNow.count, 6); ++i) {
                                const auto& t = senseNow.threats[i];
                                if (t.attacker && finite(t.middle) &&
                                    (!first || (t.timed && (!first->timed || t.toImpact < first->toImpact))))
                                    first = &t;
                            }
                            if (first) {
                                blowFrom = first->middle;
                                blowSeen = frame.predictedDisplayTime;
                            }
                            // A counter made: its attacker's mark breaks, even one the
                            // game has taken off already.
                            const uint64_t counters = senseNow.thrown + senseNow.landed;
                            countersBefore = std::min(countersBefore, counters);
                            if (counters > countersBefore) {
                                for (auto& m : senseMarks)
                                    if (m.attacker && m.attacker == senseNow.lastCounter && !m.brokenAt) {
                                        m.brokenAt = frame.predictedDisplayTime;
                                        ++senseBroken;
                                    }
                                moments.start(Moment::called);
                                const int64_t until = frame.predictedDisplayTime + 100'000'000;
                                for (unsigned h = 0; h < 2; ++h) {
                                    runtime.haptic(h, .8f, .08f);
                                    pulseUntil[h] = std::max(pulseUntil[h], until);
                                }
                                feel(body_haptics::Kind::counter);
                            }
                            countersBefore = counters;
                        }
                        // A dodge on a web the game took and that answered a warning
                        // (game_dodge): the whoosh across the body as a dodge's, from
                        // the side the web carries the player away from, and a breath
                        // in both hands. It is a dodge while a warning stands.
                        if (dodgeResults) {
                            const auto* answers = static_cast<const volatile uint64_t*>(dodgeResults->answers);
                            const uint64_t answered = answers[1] + answers[2] + answers[3];
                            // Those of an earlier session in this game are not this one's.
                            if (webDodgesBefore == UINT64_MAX)
                                webDodgesBefore = answered;
                            if (answered > webDodgesBefore && input.focused) {
                                const volatile float* way = &dodgeResults->lastWay.x;
                                const float right =
                                    way[0] * motion.walkRightAxis.x + way[2] * motion.walkRightAxis.z;
                                feel(body_haptics::Kind::dodge, -1, 1, right > 0 ? -1.f : 1.f);
                                const int64_t until = frame.predictedDisplayTime + 60'000'000;
                                for (unsigned h = 0; h < 2; ++h) {
                                    runtime.haptic(h, .3f, .05f);
                                    pulseUntil[h] = std::max(pulseUntil[h], until);
                                }
                                ++senseDodges;
                            }
                            webDodgesBefore = answered;
                        }
                        // A warning gone (its blow landed, or the game took it off) is
                        // gone at once; a broken mark plays out its poses first.
                        for (auto& m : senseMarks) {
                            const double sinceBroken =
                                static_cast<double>(frame.predictedDisplayTime - m.brokenAt) * 1e-9;
                            const bool breaking = m.brokenAt && sinceBroken < SenseView::brokenSeconds;
                            if (m.attacker && !m.live && !breaking)
                                m = {};
                        }
                        game_punch::Data punch{};
                        if (punchStarted && !samplePunch(&punch))
                            for (unsigned i = 0; i < 2; ++i) {
                                // With an older ray module: a landed punch knocks back
                                // into the hand, as hard as it was; the web's rumble
                                // waits for it.
                                if (!fighting && punch.hands[i].punches > punchesBefore[i] && input.focused) {
                                    runtime.haptic(i, .5f + .5f * punch.hands[i].lastStrength);
                                    pulseUntil[i] = frame.predictedDisplayTime + 40'000'000;
                                    feel(body_haptics::Kind::punch, static_cast<int>(i), punch.hands[i].lastStrength);
                                }
                                punchesBefore[i] = punch.hands[i].punches;
                            }
                        game_shooter::Data shots{};
                        if (shooterStarted && !sampleShooter(&shots)) {
                            listed = shots;
                            for (unsigned i = 0; i < 2; ++i) {
                                // A web ball leaving the wrist: a short, crisp tick.
                                if (shots.hands[i].shots > shotsBefore[i] && input.focused) {
                                    runtime.haptic(i, .35f);
                                    pulseUntil[i] = frame.predictedDisplayTime + 25'000'000;
                                    ballShot[i] = true;
                                    feel(body_haptics::Kind::webShot, static_cast<int>(i));
                                    eyeShots += input.eyeAim != 0;
                                }
                                shotsBefore[i] = shots.hands[i].shots;
                            }
                            shooterLogSeconds +=
                                std::isfinite(frame.seconds) && frame.seconds > 0 ? frame.seconds : 0;
                            if (shooterLogSeconds >= 1.f) {
                                shooterLogSeconds = 0;
                                diag_log::writef(
                                    "web balls: requested %llu fired %llu targeted %llu resolved %llu "
                                    "webbed %llu collisions %llu pushed %llu dropped %llu empty %llu "
                                    "(L %llu R %llu)",
                                    static_cast<unsigned long long>(listed.requested),
                                    static_cast<unsigned long long>(listed.fired),
                                    static_cast<unsigned long long>(listed.targeted),
                                    static_cast<unsigned long long>(listed.resolved),
                                    static_cast<unsigned long long>(listed.webbed),
                                    static_cast<unsigned long long>(listed.collisions),
                                    static_cast<unsigned long long>(listed.pushed),
                                    static_cast<unsigned long long>(listed.dropped),
                                    static_cast<unsigned long long>(listed.emptyPulls),
                                    static_cast<unsigned long long>(listed.hands[0].shots),
                                    static_cast<unsigned long long>(listed.hands[1].shots));
                                const auto& d = listed.diagnostic;
                                diag_log::writef(
                                    "wb dispatch: q%llu full%llu exp%llu call%llu rej%llu obj%llu unk%llu g%llu f%llu",
                                    (unsigned long long)d.queued, (unsigned long long)d.queueFull,
                                    (unsigned long long)d.expired, (unsigned long long)d.callSuccess,
                                    (unsigned long long)d.rejected, (unsigned long long)d.objectKnown,
                                    (unsigned long long)d.objectUnknown, (unsigned long long)d.gadgetCalls,
                                    (unsigned long long)d.fallbackCalls);
                                // Snapshot, not a record of every shot: bounded by the existing one-second cadence.
                                const auto event = d.expired + d.callSuccess + d.rejected;
                                static uint64_t lastDispatchEvent{};
                                if (event && event != lastDispatchEvent) {
                                    lastDispatchEvent = event;
                                    diag_log::writef(
                                        "wb event %llu: hand%u route%u outcome%u muzzle %.3g %.3g %.3g dir %.3g %.3g %.3g",
                                        (unsigned long long)event, d.hand, d.route, d.outcome,
                                        d.origin.x, d.origin.y, d.origin.z, d.direction.x, d.direction.y, d.direction.z);
                                    diag_log::writef(
                                        "wb aim %llu: point %.3g %.3g %.3g target %llx last-hit %llx (not hit/visibility proof)",
                                        (unsigned long long)event, d.aimPoint.x, d.aimPoint.y, d.aimPoint.z,
                                        (unsigned long long)d.target, (unsigned long long)listed.lastHit);
                                }
                            }
                        }
                        fightNear = input.focused && ((fighting && strikes.cueTarget != 0) || punch.bots != 0);
                        if (input.focused && ((fighting && strikes.enemies != 0) || punch.bots != 0))
                            fightAt = now.QuadPart;
                        if (websStarted && motion.active && !flatScreen) {
                            // The game draws these webs from the tracked web shooters.
                            native_webs::Request webs;
                            native_webs::Bent bent[2]{};
                            webs.feet = motion.anchor;
                            for (unsigned i = 0; i < 2; ++i) {
                                auto& hand = webs.hands[i];
                                // Each rope draws the web WebRopes gave it, from the
                                // web shooter of the hand that holds that web nearer
                                // its end.
                                const auto& rope = webRopes.rope(i);
                                const auto from =
                                    static_cast<unsigned>(rope.hands ? rope.from : rope.last >= 0 ? rope.last : i);
                                // A web let go of a thrown prop goes with it as it dissolves.
                                const bool trail = !rope.hands && rope.last >= 0 && !attachedBefore[from] && trails[from];
                                hand.attached = rope.hands ? 1 : trail ? 2 : 0;
                                hand.tracked = motion.swing.hands[from].tracked;
                                hand.attachedAt = rope.key;
                                hand.anchor = trail ? trailEnds[from] : swingState.webs[from].anchor;
                                hand.wrist = webShooter(actionGrips[from], from).position;
                                if (hand.attached == 1 && flightVisual[from].flying())
                                    hand.anchor = flightVisual[from].tip();
                                if (hand.attached == 1 && flightVisual[from].flying() &&
                                    native_webs::drawsHeld()) {
                                    bent[i].count = native_webs::maxRopeBends;
                                    const float progress = flightVisual[from].progress();
                                    const float age = flightVisual[from].age();
                                    for (uint32_t k = 0; k < bent[i].count; ++k)
                                        bent[i].points[k] = flyingWebPoint(
                                            hand.wrist, hand.anchor, float(k + 1) / float(bent[i].count + 1),
                                            age, progress, flightVisual[from].direction());
                                }
                                // A web round edges: to where it sticks, round each of them. The
                                // swing's anchor is the nearest edge, and drawn to it the web's end
                                // jumped to each new one. Where Spidy cannot lay the rope's points
                                // (no hand points hook) it stays drawn to that edge, not straight
                                // through the building to its end.
                                if (const auto& edges = swingBends.hands[from];
                                    hand.attached == 1 && !grabPhases[from] && edges.count &&
                                    finite(edges.end) && native_webs::drawsHeld()) {
                                    hand.anchor = edges.end;
                                    bent[i].count =
                                        std::min<uint32_t>(edges.count, native_webs::maxRopeBends);
                                    for (uint32_t k = 0; k < bent[i].count; ++k)
                                        bent[i].points[k] = edges.points[k];
                                }
                            }
                            // A zip to a web line: its web from the hand that shot it to
                            // where it holds on the line, in the game's own material, on a
                            // rope with none of the hands' own (November 4: without it a far
                            // zip "pulled me in place to the other line").
                            if (values.webLines && lineState.phase == 1 && lineState.zipKey &&
                                finite(lineState.zipTo)) {
                                const unsigned hand = lineState.zipHand > 1 ? 1u : lineState.zipHand;
                                for (uint32_t i = 0; i < 2; ++i)
                                    if (!webs.hands[i].attached && motion.swing.hands[hand].tracked) {
                                        auto& rope = webs.hands[i];
                                        rope.attached = 1;
                                        rope.tracked = 1;
                                        rope.attachedAt = static_cast<int64_t>(lineState.zipKey);
                                        rope.anchor = lineState.zipTo;
                                        rope.wrist = webShooter(actionGrips[hand], hand).position;
                                        bent[i].count = 0;
                                        break;
                                    }
                            }
                            // One web in both hands: its rope comes to the upper
                            // wrist from the lower one, in the game's own material.
                            if (const int upper = webRopes.upper(); upper >= 0) {
                                const auto lower = static_cast<unsigned>(1 - upper);
                                const int slot = webRopes.slot(lower);
                                if (slot >= 0 && motion.swing.hands[lower].tracked) {
                                    const Vec3 wrist = webShooter(actionGrips[lower], lower).position;
                                    webs.held.rope = static_cast<uint32_t>(slot) + 1;
                                    webs.held.wrist = wrist;
                                    webs.held.slack =
                                        std::max(swingState.webs[lower].length - swingState.webs[upper].length -
                                                     length(webs.hands[slot].wrist - wrist),
                                                 0.f);
                                }
                            }
                            native_webs::submit(webs, 200, bent);
                            // The web tie's webs, in the game's own rope material.
                            native_webs::TieRope tieRopes[native_webs::maxTieRopes]{};
                            uint32_t tieRopeCount{};
                            for (uint32_t k = 0;
                                 k < tieState.count && tieRopeCount < native_webs::maxTieRopes; ++k)
                                tieRopes[tieRopeCount++] = {tieState.lines[k].anchor, tieState.lines[k].end};
                            native_webs::submitTies(tieRopes, tieRopeCount, 200);
                            // Web lines, in the game's own rope material, then the strands of
                            // the thugs hung from them (game_grab::TieData::hangs), each its own
                            // rope while there are ropes (native_webs::maxLineRopes).
                            native_webs::LineRope lineRopes[native_webs::maxLineRopes]{};
                            uint32_t lineRopeCount{};
                            for (uint32_t k = 0;
                                 k < lineState.count && lineRopeCount < native_webs::maxLineRopes; ++k)
                                lineRopes[lineRopeCount++] = {lineState.lines[k].start,
                                                              lineState.lines[k].end, lineState.lines[k].id};
                            // Each strand its own rope while they last; past that, two
                            // neighbours on a line share one (native_webs::LineRope::via),
                            // as many pairs as it takes.
                            {
                                const uint32_t hangs = std::min<uint32_t>(tieState.hangCount, 16);
                                for (auto& r : strandRope)
                                    r = 0xff;
                                uint32_t order[16]{};
                                uint32_t lineOf[16]{};
                                float alongOf[16]{};
                                for (uint32_t h = 0; h < hangs; ++h) {
                                    order[h] = h;
                                    std::memcpy(&lineOf[h], &tieState.hangs[h].distance, sizeof(uint32_t));
                                    alongOf[h] = 0;
                                    for (uint32_t k = 0; k < lineState.count; ++k)
                                        if (lineState.lines[k].id == lineOf[h]) {
                                            const Vec3 ab = lineState.lines[k].end - lineState.lines[k].start;
                                            alongOf[h] =
                                                dot(tieState.hangs[h].anchor - lineState.lines[k].start, ab);
                                        }
                                }
                                std::sort(order, order + hangs, [&](uint32_t a, uint32_t b) {
                                    return lineOf[a] != lineOf[b] ? lineOf[a] < lineOf[b]
                                                                  : alongOf[a] < alongOf[b];
                                });
                                const uint32_t room = native_webs::maxLineRopes - lineRopeCount;
                                uint32_t pairs = hangs > room ? hangs - room : 0;
                                for (uint32_t i = 0; i < hangs && lineRopeCount < native_webs::maxLineRopes;
                                     ++i) {
                                    const auto& first = tieState.hangs[order[i]];
                                    auto& rope = lineRopes[lineRopeCount];
                                    if (pairs && i + 1 < hangs && lineOf[order[i]] == lineOf[order[i + 1]]) {
                                        const auto& second = tieState.hangs[order[i + 1]];
                                        rope = {second.anchor,
                                                second.end,
                                                0x40000000u | (first.flags & 0x3fffffffu),
                                                2,
                                                {first.end, first.anchor}};
                                        strandRope[order[i]] = strandRope[order[i + 1]] =
                                            static_cast<uint8_t>(lineRopeCount);
                                        ++i;
                                        --pairs;
                                    } else {
                                        rope = {first.anchor, first.end,
                                                0x40000000u | (first.flags & 0x3fffffffu)};
                                        strandRope[order[i]] = static_cast<uint8_t>(lineRopeCount);
                                    }
                                    ++lineRopeCount;
                                }
                            }
                            native_webs::submitLines(lineRopes, lineRopeCount, 200);
                        }
                    }
                    const bool steering = motion.active && !flatScreen && controlsVisible;
                    motion.nativeKeys = airJump.update(
                        motion.nativeKeys, game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart));
                    // Going up onto a wall (game_swing::Data::mount), the game
                    // gets the jump alone: the stick walked the player on into
                    // the wall, and into the game's own crawl.
                    const bool mounting =
                        swingState.status && swingState.mount && swingState.qpc &&
                        static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10;
                    // Standing on a web line, the game walks the player itself (its
                    // own legs: game_swing's GameWalk), so it gets its walk as it
                    // does on the ground, though the swing holds him there: until
                    // November 4 (19:07) the stick never reached it ("now I cannot
                    // walk at all"), the swing owning him.
                    const bool lineFoot = values.webLines && lineState.phase == 2 && lineState.onLine;
                    const bool swingHolds = swingState.owned != 0 && !lineFoot;
                    auto keys = swingNativeKeys(steering, swingHolds || mounting, motion.nativeKeys,
                                                static_cast<SwingTakeoff::Phase>(swingState.takeoffPhase),
                                                swingState.takeoff != 0);
                    // A held on the ground charges a leap, the swing's own, let
                    // go of with A. A tap is the jump as before, made as it is
                    // let go. The swing's own jumps (a takeoff, a wall, the
                    // leap's) are the game's A as they are.
                    const bool standing = steering && padInstalled && swingState.status &&
                                          swingState.grounded && !swingState.owned && !mounting &&
                                          !swingState.takeoff && !swingState.takeoffPhase &&
                                          !swingState.webs[0].attached && !swingState.webs[1].attached &&
                                          !calibrationInput && spiderMan;
                    // EARLY JUMP (OFF by default): a quicker tap, and while
                    // NATIVE RUN holds RT the game's A at once.
                    chargeJump.earlyJump(values.earlyJump);
                    // EARLY JUMP's RUNNING (2) counts the stick's run on the
                    // game's ground too, not only NATIVE RUN's RT (JUMP ALWAYS,
                    // v5: the November 3 session ran with NATIVE RUN 0 at every
                    // take-off, so a press while running still waited or charged).
                    // COMBAT RESPONSIVE (off by default): in a fight A is the
                    // game's jump at once, no charge.
                    const bool runningJump =
                        nativeRun.held() ||
                        (values.earlyJump == 2 && std::hypot(motion.walkRight, motion.walkForward) >= .5f);
                    const bool fightJump = values.combatResponsive &&
                                           (fightLately || moverTable == config.base + bridge::combatCamera);
                    const auto leap =
                        chargeJump.update(frame.seconds, (keys & swingJumpKey) && !swingState.takeoff,
                                          standing, runningJump, fightJump);
                    if (!swingState.takeoff)
                        keys = (keys & ~swingJumpKey) | (leap.jump ? swingJumpKey : 0u);
                    // JUMP ALWAYS (v5, always on): while the swing runs the body on
                    // the ground (SPIDY RUN, the landing carry) the press ends that
                    // run (Swing::jumpCancels) and the game's A waits for the game to
                    // have the body back, 0.2 s at most (game_pad::JumpRelay).
                    {
                        // Only with SPIDY RUN or SMOOTH WEB LANDING on, the runs it is for
                        // (the review of pull request #77): the swing can own a body on the
                        // ground with no web for a moment after any landing it flew, and the
                        // relay held or swallowed an A pressed then.
                        const bool swingGround =
                            (values.spidyRun || values.webLandingSmooth) && steering && swingFresh &&
                            swingState.status && swingState.owned && swingState.grounded &&
                            swingState.wall == static_cast<uint32_t>(game_swing::Surface::none) &&
                            !swingState.webs[0].attached && !swingState.webs[1].attached && !mounting &&
                            !swingState.takeoff && !swingState.takeoffPhase;
                        // SPIDY RUN JUMP (on by default): with no web the swing jumps
                        // at once, the run's speed kept; the game gets no A for that press.
                        const bool runJumpNow = values.spidyRunJump && swingGround;
                        const uint32_t swallowedBefore = jumpRelay.swallowed();
                        const bool relayed = jumpRelay.update(frame.seconds, (keys & swingJumpKey) != 0,
                                                              swingGround, values.spidyRunJump);
                        if (runJumpNow && jumpRelay.swallowed() != swallowedBefore)
                            runJumpOpen = true, runJumpAge = 0;
                        if (!swingState.takeoff)
                            keys = (keys & ~swingJumpKey) | (relayed ? swingJumpKey : 0u);
                        // jump: lines (diagnostics only): each press of A in play, and
                        // the moment the game's A went out, or that it never did.
                        const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                        const bool raw = steering && frame.jump;
                        const bool out = (keys & swingJumpKey) != 0;
                        char line[200];
                        if (raw && !jumpRawBefore) {
                            if (jumpPressOpen)
                                diag_log::write("jump: a press the game never got as A (another came)");
                            jumpPressOpen = true;
                            jumpPressAge = 0;
                            jumpPressCharges = chargeJump.charges();
                            sprintf_s(jumpPressWhere, "%s%s%s%s%s, fight %d",
                                      swingGround ? "the swing running on the ground"
                                      : standing  ? "standing on the game's ground"
                                      : swingState.grounded ? "on the ground"
                                                            : "off the ground",
                                      swingState.owned ? ", owned" : "",
                                      runningJump ? ", running" : "",
                                      swingState.wall ? ", on a wall" : "",
                                      (swingState.webs[0].attached || swingState.webs[1].attached) ? ", webbed" : "",
                                      fightLately ? 1 : 0);
                        } else if (jumpPressOpen)
                            jumpPressAge += dt;
                        if (jumpPressOpen && out && !jumpOutBefore) {
                            sprintf_s(line, "jump: A to the game %d ms after the press (%s%s)",
                                      static_cast<int>(jumpPressAge * 1000 + .5f), jumpPressWhere,
                                      jumpRelay.relayed() && jumpRelay.waited() > 0 && jumpPressAge > 0 &&
                                              jumpRelay.waited() >= jumpPressAge - .001f
                                          ? "; held for the swing's hand-back"
                                          : "");
                            diag_log::write(line);
                            jumpPressOpen = false;
                        } else if (jumpPressOpen && !raw && !out && jumpPressAge > .5f) {
                            sprintf_s(line, "jump: a press the game never got as A (%s%s)", jumpPressWhere,
                                      chargeJump.charges() != jumpPressCharges ? "; held into a charged leap"
                                      : !swingState.grounded            ? "; in the air A waits for a press from the ground"
                                                                        : "");
                            diag_log::write(line);
                            jumpPressOpen = false;
                        }
                        // SPIDY RUN JUMP: the swing's own jump, press to off the ground.
                        if (runJumpOpen) {
                            if (!swingState.grounded) {
                                sprintf_s(line,
                                          "jump: SPIDY RUN JUMP off the ground %d ms after the press (the swing's "
                                          "own jump, the run's speed kept; no wait for the game; fight %d)",
                                          static_cast<int>(runJumpAge * 1000 + .5f), fightLately ? 1 : 0);
                                diag_log::write(line);
                                runJumpOpen = jumpPressOpen = false;
                            } else if ((runJumpAge += dt) > .5f) {
                                diag_log::write("jump: SPIDY RUN JUMP never left the ground within 500 ms "
                                                "(the game kept the body down)");
                                runJumpOpen = jumpPressOpen = false;
                            }
                        }
                        // ANIMATED HYBRID jump adopt: the player's own jump from the ground - A
                        // pressed, the game's A sent, SPIDY RUN JUMP's own jump. (A charged leap
                        // let go of is below.) The owner adopts it on the frame he leaves the ground.
                        if (steering && !calibrationInput && swingState.grounded &&
                            ((raw && !jumpRawBefore) || (out && !jumpOutBefore) || runJumpOpen))
                            hybridJumpAsk = true;
                        jumpRawBefore = raw;
                        jumpOutBefore = out;
                    }
                    leapOut = leap.leap;
                    if (chargeJump.charges() != chargesBefore) {
                        chargesBefore = chargeJump.charges();
                        runtime.haptic(1, .3f);
                        feel(body_haptics::Kind::leapCharge);
                    } else if (chargeJump.charging()) {
                        // It builds in both hands, and holds at full.
                        const float charge = chargeJump.charge();
                        runtime.haptic(0, .08f + .3f * charge);
                        runtime.haptic(1, .1f + .4f * charge);
                    }
                    if (chargeJump.leaps() != leapsBefore) {
                        leapsBefore = chargeJump.leaps();
                        // A charged leap let go of (Command::leap): a real leap, from the ground.
                        hybridJumpAsk = true;
                        runtime.haptic(0, .8f, .12f);
                        runtime.haptic(1, 1.f, .12f);
                        feel(body_haptics::Kind::leap);
                    }
                    controls(keys);
                    askingGame = false;
                    askingZip = false;
                    if (padInstalled) {
                        // The same walking and jumping on the virtual Xbox
                        // controller: after the VR menus, the game plays the
                        // player with it and ignores the bridge's keys.
                        game_pad::Walk walk;
                        const bool playing = steering && !swingHolds;
                        // Reaching for a thing is not walking: a player standing on a
                        // wall does it as much as one on the ground. The swing owns
                        // him there (it has taken the walls since October 10), so
                        // `playing` was false for the whole climb and the game's Y
                        // never went out. The user, October 13, at a closed vent
                        // reached by walking up the wall it is on: "its right in from
                        // of me and its closed and i cant iteract with it". That
                        // session's report has the swing owning the player in every
                        // one of its 1106 samples on a wall, none of them grounded,
                        // and the game's Y in 2 of 4230 samples.
                        const bool onWall = swingState.wall != 0;
                        const bool reaching =
                            steering && (!swingState.owned || onWall) && !calibrationInput;
                        // And in flight, the swing carrying him: the game's zip to a
                        // point (the stick up) and its web strike (Y, a car's in a
                        // chase) go out there too since October 18, and the swing
                        // lets the game have him once its move has begun
                        // (game_swing::AirYield). Held back until then: a hand-over
                        // in mid-flight dropped players at the game's fall speed.
                        const bool inFlight = steering && swingState.owned && !onWall && !calibrationInput;
                        // Clicking both thumbsticks latches the whole pad, for the
                        // controls play has no room for (the game's X, its L3, each
                        // bumper and trigger on its own, three ways of its D-pad).
                        // Neither stick's own press is half of the chord (PullPress,
                        // slow_motion's StickPress both drop one), so nothing else
                        // answers it. A press of any kind keeps the layer open.
                        const bool layerBusy =
                            frame.buttons != 0 || frame.hands[0].squeeze > .6f ||
                            frame.hands[1].squeeze > .6f || frame.hands[0].trigger > .6f ||
                            frame.hands[1].trigger > .6f ||
                            std::hypot(frame.hands[1].stickX, frame.hands[1].stickY) > .5f;
                        const bool layerOn =
                            padLayer.update(frame.seconds, playing && !calibrationInput,
                                            frame.hands[0].stickClick, frame.hands[1].stickClick, layerBusy);
                        if (padLayer.entries() != padEntriesBefore) {
                            padEntriesBefore = padLayer.entries();
                            runtime.haptic(0, .5f);
                            runtime.haptic(1, .5f);
                            message(3, 0, "Pad layer on - your grips are the game's LB and RB, your triggers its "
                                          "LT and RT, X its X, the right stick its D-pad; click both sticks "
                                          "again to leave it");
                        } else if (padLayerWas && !layerOn) {
                            runtime.haptic(0, .25f);
                            runtime.haptic(1, .25f);
                            message(3, 0, "Pad layer off - the webs are back on your grips and triggers");
                        }
                        padLayerWas = layerOn;
                        {
                            // HYBRID AIR FACING SYNC's gate (hybrid_movement.hpp), with hysteresis:
                            // on past 20 degrees, off under 8.
                            constexpr float toDeg = 180.f / 3.14159265f;
                            const bool vrOwned = hybridOwner.domain == hybrid_owner::Domain::vr;
                            const float actorNow = std::hypot(body[8], body[10]) > .2f
                                                       ? hybrid_handoff::actorLookYaw({body[8], body[9], body[10]})
                                                       : NAN;
                            const float lookNow = motion.active && validTrackedPose(motion.headPose)
                                                      ? hybrid_handoff::lookYaw(
                                                            motion.headPose.orientation.rotate({0, 0, -1}))
                                                      : NAN;
                            const bool hybridAir =
                                hybrid_movement::modeOf(values.movementMode) == hybrid_movement::Mode::hybrid;
                            const bool base = hybridAir && vrOwned && swingFresh && swingState.owned &&
                                              !swingState.grounded && !swingState.webs[0].attached &&
                                              !swingState.webs[1].attached &&
                                              swingState.wall == static_cast<uint32_t>(game_swing::Surface::none) &&
                                              std::isfinite(actorNow) && std::isfinite(lookNow);
                            const float gap = base ? std::abs(stable_yaw::wrap(lookNow - actorNow)) : 0.f;
                            const bool was = hybridAirSyncOn;
                            hybridAirSyncOn =
                                !playing && base &&
                                (hybridAirSyncOn ? gap > 8.f * (3.14159265f / 180.f)
                                                 : hybrid_movement::airSyncWanted(true, true, true, false, false,
                                                                                  actorNow, lookNow));
                            if (hybridAirSyncOn) {
                                hybridAirSyncSec += frame.seconds;
                                if (!was) {
                                    ++hybridAirSyncStarts;
                                    diag_log::writef("hybrid air facing sync: on, game facing %.0f view %.0f (gap %.0f), #%u",
                                                     actorNow * toDeg, lookNow * toDeg, gap * toDeg,
                                                     hybridAirSyncStarts);
                                }
                            } else if (was) {
                                diag_log::writef("hybrid air facing sync: off, gap %.0f deg", gap * toDeg);
                            }
                        }
                        if (playing) {
                            const bool momentumDrive = game_swing::momentumWallDrive(
                                motion.joystickWallRun && !values.joystickRunCamera, swingFresh, swingState);
                            walk.right = mounting || momentumDrive ? 0 : motion.walkRight;
                            walk.forward = mounting || momentumDrive ? 0 : motion.walkForward;
                            // Hand pulls become ordinary native crawl input. The
                            // game keeps vent collision and its supported state.
                            const bool handCrawl = motion.ventCrawl && values.handClimb && motion.active &&
                                !flatScreen && !calibrationInput && !layerOn && !mounting &&
                                std::hypot(motion.walkRight, motion.walkForward) <= .2f;
                            const Vec3 pull =
                                game_swing::crawlPull(climbState, handCrawl, frame.predictedDisplayTime);
                            if (handCrawl && climbState.crawl && climbState.hands) {
                                const Vec3 stick =
                                    game_swing::crawlStick(pull, motion.walkForwardAxis,
                                                          motion.walkRightAxis);
                                walk.right = stick.x;
                                walk.forward = stick.z;
                            }
                            if (motion.ventCrawl != ventCrawlBefore) {
                                ventCrawlBefore = motion.ventCrawl;
                                message(1, 0, ventCrawlBefore ?
                                                  "Vent hand crawling active (native crawl, close view)" :
                                                  "Vent hand crawling ended");
                            }
                            // On the game's own crawl, with the view turned
                            // onto the wall, what the game makes of that stick
                            // is the one thing no session has measured.
                            if (hold.crawl && motion.standing) {
                                ++crawlStood;
                                crawlStick.update(hold.normal, motion.walkForwardAxis, walk.right, walk.forward,
                                                  swingState.velocity);
                            }
                            // Y is the game's Y: interact, or web strike in a fight.
                            // B is its B: the dodge, the way the stick points. Not
                            // while a web carries the player, nor while B skips the
                            // calibration.
                            walk.dodge = (origins.play() & buttonB) && !calibrationInput;
                        } else if (hybridAirSyncOn && !layerOn && !calibrationInput) {
                            // HYBRID AIR FACING SYNC: in SpidyVR's air the game gets a light
                            // walk stick toward where the player looks (as SOFT LANDING hands
                            // it the stick: the swing still moves the body), so the game's
                            // facing - the one a landing plays on - stays with the view.
                            const auto stick = animated_model_yaw::worldToStick(
                                hybrid_steer::viewForward(hybrid_handoff::lookYaw(
                                    motion.headPose.orientation.rotate({0, 0, -1}))),
                                motion.walkForwardAxis, motion.walkRightAxis, hybrid_movement::airSyncTilt);
                            walk.right = stick.right;
                            walk.forward = stick.forward;
                        } else if (steering && !layerOn && !calibrationInput && swingFresh &&
                                   swingState.owned &&
                                   swingState.wall == static_cast<uint32_t>(game_swing::Surface::none) &&
                                   !swingState.webs[0].attached && !swingState.webs[1].attached &&
                                   ((values.softLanding && !swingState.grounded &&
                                     swingState.velocity.y < -4) ||
                                    (values.spidyRun && swingState.grounded))) {
                            // SPIDY RUN (off by default): on the ground the swing runs
                            // the body, and the game gets the stick for its run's look;
                            // no RT (NATIVE RUN's gate is the game's ground), so none of
                            // its vaults fight the swing.
                            // SOFT LANDING (off by default): falling with no web, the
                            // game has the stick already, so its first step on the
                            // ground is the run asked for. Only the stick: the swing
                            // moves the body until it lands, and RT in the air is the
                            // game's web swing.
                            walk.right = motion.walkRight;
                            walk.forward = motion.walkForward;
                        } else if (values.webLandingSmooth && steering && !layerOn && !calibrationInput &&
                                   swingFresh && swingState.owned && swingState.grounded &&
                                   swingState.wall == static_cast<uint32_t>(game_swing::Surface::none)) {
                            // SMOOTH WEB LANDING (off by default): the swing carries a
                            // landing with a web across the ground; the game has the
                            // stick for its run's look, so its first step when it takes
                            // the body back is the run asked for. Only the stick, no RT.
                            walk.right = motion.walkRight;
                            walk.forward = motion.walkForward;
                        }
                        // What each hand points at among the game's interactions near
                        // the player, on Spidy's wall only: off it the game offers its
                        // own, and its interact uses them. Read every frame all the
                        // same: a use goes on once the game's crawl has him.
                        const bool onOwnWall =
                            swingState.status && swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall);
                        const bool offersRead = sampleOffers && !sampleOffers(&offers) && offers.status == 1;
                        std::array<offer_pointing::Offer, game_offers::maxOffers> candidates{};
                        unsigned candidateCount{};
                        if (offersRead && onOwnWall)
                            for (uint32_t k = 0; k < offers.offers && k < game_offers::maxOffers; ++k) {
                                const auto& o = offers.nearby[k];
                                candidates[candidateCount++] = offer_pointing::fromGame(
                                    o.handle, o.position, o.reasons, o.enabled != 0, o.reachable != 0);
                            }
                        offersSeen = std::max(offersSeen, candidateCount);
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = motion.swing.hands[i];
                            pointing[i] = std::nullopt;
                            if (steering && !calibrationInput && candidateCount)
                                pointing[i] = offer_pointing::pick(
                                    {hand.tracked, hand.aim.position, hand.aim.orientation.rotate({0, 0, -1})},
                                    candidates.data(), candidateCount);
                            // A tick in the hand as it comes onto one it can use.
                            const uint32_t usable = pointing[i] && pointing[i]->usable ? pointing[i]->handle : 0;
                            if (usable && usable != pointedBefore[i])
                                runtime.haptic(static_cast<int>(i), .25f);
                            pointedBefore[i] = usable;
                        }
                        const auto pointed = offer_pointing::either(pointing[0], pointing[1]);
                        // The game tries the wall round the one pointed at for a
                        // spot within reach (game_offers::aim).
                        if (uint32_t aim = pointed ? pointed->handle : 0; aimOffer && aim != offerAimed) {
                            offerAimed = aim;
                            aimOffer(&aim);
                        }
                        offerPointFrames += pointed && pointed->usable;
                        offerReachFrames += pointed && !pointed->usable;
                        // Y reaches for a thing wherever the player stands, a wall
                        // included; the step up to it below stays on the ground. On
                        // Spidy's wall at one of the game's interactions a hand points
                        // at, it is that one's use: the game's crawl under the player
                        // there, then the game's interact for him.
                        // A Y the game names goes out as it is (NamedPress), with no
                        // turn or step of the interact's.
                        const uint32_t named = namedPress.claimed();
                        walk.interact = offerUse.update(
                            frame.seconds,
                            (reaching || inFlight) && (origins.play() & buttonY) != 0 &&
                                !(named & game_pad::NamedPress::controlY),
                            onOwnWall, pointed,
                            {offers.usesTaken, offers.usesDeclined, offers.best},
                            [&](uint32_t handle) { return useOffer && !useOffer(&handle); });
                        if (offerUse.asks() != offerAsksBefore) {
                            offerAsksBefore = offerUse.asks();
                            runtime.haptic(0, .4f);
                            runtime.haptic(1, .4f);
                        }
                        if (offerUse.failures() != offerFailuresBefore) {
                            offerFailuresBefore = offerUse.failures();
                            message(3, 0, "The game would not take that from here - come closer to it and press "
                                          "again");
                        }
                        // A press of Y turns the hero to where the player looks; held
                        // on, he walks there, until the game takes the press. The
                        // player's own stick goes first.
                        const bool fight = moverTable == config.base + bridge::combatCamera;
                        const float stepIn = interactStep.update(
                            frame.seconds, walk.interact,
                            playing && !mounting && swingState.grounded && !fight && !motion.standing,
                            std::hypot(swingState.velocity.x, swingState.velocity.z));
                        if (interactStep.steps() != interactStepsBefore) {
                            interactStepsBefore = interactStep.steps();
                            runtime.haptic(0, .2f);
                        }
                        // The game's Y: the player's, and after a tap the time the
                        // hero takes to turn to what they look at.
                        walk.interact = interactStep.pressed();
                        // What the game names now, for a pull (below). Not in the pad
                        // layer: there the player presses the game's own buttons.
                        const auto asked = layerOn ? prompt_buttons::Demand{} : game_prompts::demand();
                        // The game's throw of a prop where the hand X is not on
                        // points (throw_aim.hpp, game_throw): while X sends the
                        // game's LB + RB, at the enemy that hand's aim marker is
                        // on, or the way from the player to what it points at.
                        // The user, October 24: "really hard or not possible to
                        // aim where the throw will go".
                        {
                            const bool pair = (asked.buttons & game_pad::bumpers) == game_pad::bumpers;
                            const bool holding = playing && pair && (named & game_pad::NamedPress::controlX);
                            const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            throwAimTail = holding ? 1.f : std::max(0.f, throwAimTail - dt);
                            if (aimGameThrow && throwAimTail > 0) {
                                const unsigned xHand =
                                    runtime.controllers()[1] == ControllerKind::frame ? 1u : 0u;
                                const unsigned hand = 1 - xHand;
                                auto request = throw_aim::fromHand(
                                    aimed.hands[hand], motion.swing.hands[hand].aim.orientation.rotate({0, 0, -1}),
                                    swingState.position);
                                aimGameThrow(&request);
                            }
                        }
                        // The game takes its own interact away the moment the
                        // player leaves the surface, and the swing's wall is
                        // the player off it: hand him back for the press. Only
                        // where the game is offering nothing, which is the one
                        // case the press does nothing at all today.
                        const bool handing = wallHandover.update(
                            frame.seconds, walk.interact,
                            swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall),
                            game_prompts::live().demand != 0);
                        // The walls are NOT let go of yet, and October 13 is why:
                        // they were, and the player fell. Leaving the wall takes
                        // the swing's hold off a hero the game still has in its
                        // air state, and the game's own crawl does not pick him
                        // up where he stands - it takes him when a stick pushes
                        // him into a wall, which is the half this does not do.
                        // Until that half is built and measured, the rule only
                        // counts: `wall_handovers` says how often a player asked
                        // to interact on a wall of the swing's with the game
                        // offering nothing, which is the size of the thing to
                        // fix. At a vent today the answer is wall running off.
                        wallsHandedBack = handing;
                        (void)swingSettingsSent;
                        (void)toldHandoverRefused;
                        // Not on a wall the player stands on: the game's camera
                        // there is not the follow camera's level turn.
                        const bool cameraSteered = steering && !motion.onSurface;
                        // EYE AIM's zips (game_pad::EyeZip): from the stick going up
                        // for a zip to the zip's end, the camera is brought closely
                        // onto what the eyes look at, from where it stands - the point
                        // the game zips to. Not while basic flips have the stick.
                        const auto& zipStick = frame.hands[1];
                        const bool zipWanted =
                            zipPress.active() || (!legStickActive && zipStick.stickY >= game_pad::StickPush::pushed &&
                                                  std::abs(zipStick.stickX) <= game_pad::StickPush::aside);
                        const Vec3 gazeLook = motion.gazePose.orientation.rotate({0, 0, -1});
                        const bool eyesZip = (values.eyeAim & vr_settings::eyeAimZips) && motion.gazeValid &&
                                             finite(gazeLook) && length(gazeLook) > .5f && zipWanted &&
                                             (!motion.basicFlipControls || zipPriority || zipPress.active());
                        const auto& seen = aimed.gaze;
                        const Vec3 cameraLookTo =
                            eyesZip ? game_pad::cameraLookAt(
                                          {camera[12], camera[13], camera[14]}, seen.point,
                                          seen.kind == static_cast<uint32_t>(game_swing::AimKind::anchor) ||
                                              seen.kind == static_cast<uint32_t>(game_swing::AimKind::air),
                                          normalized(gazeLook))
                                    : motion.headPose.orientation.rotate({0, 0, -1});
                        walk.look = cameraLook.update(cameraLookTo, {camera[8], camera[9], camera[10]}, cameraSteered,
                                                      eyesZip);
                        walk.lookUp = cameraPitch.update(cameraLookTo, {camera[8], camera[9], camera[10]},
                                                         cameraSteered, eyesZip);
                        if (stepIn > 0 && std::hypot(walk.right, walk.forward) < .1f) {
                            Vec3 look = motion.headPose.orientation.rotate({0, 0, -1});
                            Vec3 ahead{camera[8], 0, camera[10]};
                            look.y = 0;
                            // Looking straight down or up says no way to go.
                            if (length(look) > .3f && length(ahead) > .3f) {
                                look = normalized(look);
                                ahead = normalized(ahead);
                                walk.forward = stepIn * dot(look, ahead);
                                walk.right = stepIn * dot(look, cross(ahead, {0, 1, 0}));
                            }
                        }
                        // On a web line the game walks him itself (its own legs:
                        // game_swing's GameWalk), so its stick goes along the line:
                        // the stick's part along it, none across.
                        if (values.webLines && lineState.phase == 2 && lineState.onLine) {
                            for (uint32_t k = 0; k < lineState.count && k < std::size(lineState.lines); ++k) {
                                if (lineState.lines[k].id != lineState.onLine)
                                    continue;
                                Vec3 along = lineState.lines[k].end - lineState.lines[k].start;
                                along.y = 0;
                                const Vec3 ahead = motion.walkForwardAxis, side = motion.walkRightAxis;
                                if (length(along) > .1f && finite(ahead) && finite(side)) {
                                    along = normalized(along);
                                    const Vec3 meant = ahead * walk.forward + side * walk.right;
                                    Vec3 kept = along * dot(meant, along);
                                    // And back toward the line's middle as he walks: the
                                    // game walked him beside it, on air, and the line's
                                    // holds took him back with a jolt (November 4, 19:35).
                                    const Vec3 a = lineState.lines[k].start, b = lineState.lines[k].end;
                                    const Vec3 ab = b - a;
                                    const Vec3 at = swingState.position;
                                    if (length(kept) > .05f && finite(at) && dot(ab, ab) > 1e-4f) {
                                        const Vec3 on =
                                            a + ab * std::clamp(dot(at - a, ab) / dot(ab, ab), 0.f, 1.f);
                                        Vec3 off = on - at;
                                        off.y = 0;
                                        off = off - along * dot(off, along);
                                        // 4 a metre, 0.8 of the stick at most: at 2.5 and 0.6 he
                                        // still walked off its middle on some lines (20:37).
                                        const float pull = std::min(length(off) * 4.f, .8f);
                                        if (length(off) > .02f)
                                            kept = kept + normalized(off) * pull;
                                    }
                                    walk.forward = std::clamp(dot(kept, ahead), -1.f, 1.f);
                                    walk.right = std::clamp(dot(kept, side), -1.f, 1.f);
                                }
                            }
                        }
                        // While a use is under way the game's crawl is where the
                        // offer was checked: the stick does not walk him off it.
                        if (offerUse.busy())
                            walk.right = walk.forward = 0;
                        // The game's LB + RB, its web pull where it shows the two
                        // (a door, a grate): both web buttons on the thing, which
                        // no web holds on, and a yank. X sends them too, where
                        // the game names them (NamedPress, above).
                        std::array<WebPullHand, 2> pulling{};
                        bool free = swingStarted && swingState.grounded && !calibrationInput;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = motion.swing.hands[i];
                            const auto& aim = aimed.hands[i];
                            free = free && !swingState.webs[i].attached && !grabPhases[i] &&
                                   !(motion.ventCrawl && climbState.status && (climbState.hands & (1u << i)));
                            pulling[i] = {hand.tracked && hand.grip > .5f,
                                          aim.kind == static_cast<uint32_t>(game_swing::AimKind::blocked), aim.point,
                                          hand.aim.position, hand.gripRelativeToHead};
                        }
                        const bool yanked =
                            webPull.update(frame.seconds, playing && free, trackingTurn(motion.swing), pulling);
                        if (webPull.pulls() != pullsBefore) {
                            pullsBefore = webPull.pulls();
                            runtime.haptic(0, 1);
                            runtime.haptic(1, 1);
                            feel(body_haptics::Kind::webPull);
                            message(3, 0, "Web pull - the game's LB + RB, for as long as you hold both web buttons");
                        }
                        const bool pulled = yanked && playing;
                        // A yank at a thing the game is prompting both triggers for
                        // sends those (its LT + RT for a vent as readily as its
                        // LB + RB for a door): the gesture is the player's first way
                        // in. With nothing of the kind named it is the pull it was.
                        if (pulled && asked.leftTrigger && asked.rightTrigger)
                            walk.demand = {0, true, true};
                        else
                            walk.pull = pulled;
                        if (walk.pull && !pulling_)
                            game_prompts::note(onWall ? "the player's pull -> the game's LB + RB, on a wall"
                                                      : "the player's pull -> the game's LB + RB");
                        pulling_ = walk.pull;
                        // Outside airborne basic flips, the right stick down heals,
                        // the ground; pushed up it is the game's LT + RT, while
                        // a web carries the player too (inFlight). Left and
                        // right turn.
                        // The stick as it is, whether or not the controller's pose is tracked: a pose
                        // lost for a moment (the controller out of the cameras' sight, as it is held
                        // down by the hip) took the stick to rest and spent a heal under the thumb.
                        const auto& turning = frame.hands[1];
                        // Not a push the game names a D-pad way for (NamedPress).
                        const bool flipZip = motion.basicFlipControls && (zipPriority || zipPress.active());
                        if (flipZipPriority.began()) stickPush.update(0, true, 0, 0);
                        const bool sticking =
                            steering && !calibrationInput && !layerOn &&
                            (!motion.basicFlipControls || flipZip) &&
                            !(named & (game_pad::NamedPress::stickUp | game_pad::NamedPress::stickLeft |
                                       game_pad::NamedPress::stickRight));
                        // LEG HOLD (the user, November 12 01:33): while the legs own the
                        // stick it poses them only: no turn, no zip, no heal (the November 10
                        // double tap is gone). Off that mode StickPush is as it was, and must
                        // see neutral first.
                        game_pad::StickPush::Way push = game_pad::StickPush::Way::none;
                        if (legStickActive) {
                            legStickX = sticking ? turning.stickX : 0.f;
                            legStickY = sticking ? turning.stickY : 0.f;
                            if (!std::isfinite(legStickX) || !std::isfinite(legStickY))
                                legStickX = legStickY = 0;
                            stickPush.cancel();
                        } else {
                            legStickX = legStickY = 0;
                            push = motion.basicFlipControls && !flipZip ?
                                game_pad::StickPush::Way::none :
                                stickPush.update(frame.seconds, sticking, turning.stickX, turning.stickY);
                        }
                        if (motion.basicFlipControls && !flipZip) {
                            stickPush.cancel();
                            zipPress.cancel();
                        }
                        walk.heal = !motion.basicFlipControls && push == game_pad::StickPush::Way::down;
                        // Held on into the game's aim mode (its own time slowed), LT stays down alone,
                        // and letting go zips to the point its aim holds (ZipPress).
                        // With EYE AIM's zips, not before the game's camera is on the
                        // eyes' point (EyeZip; a moment at most).
                        const bool zipUp = eyeZip.update(
                            frame.seconds, (reaching || inFlight) && push == game_pad::StickPush::Way::up,
                            eyesZip && cameraSteered, cameraLook.error(), cameraPitch.error());
                        const auto gameTime = game_time::telemetry();
                        const bool slow = gameTime.installed && gameTime.own < game_pad::ZipPress::slowBelow;
                        const auto zipKeys = zipPress.update(frame.seconds, zipUp, sticking, slow);
                        walk.zip = zipKeys.zip;
                        walk.aim = zipKeys.aim;
                        if ((walk.heal && !healing) || (walk.zip && !zipping))
                            runtime.haptic(1, .2f);
                        if (walk.heal && !healing)
                            feel(body_haptics::Kind::heal);
                        if (walk.zip && !zipping)
                            feel(body_haptics::Kind::zip);
                        // The swing hears of it with the next command: the zip from its own wall
                        // too, where Y is the wall's interactions' (offerUse).
                        // And of any button the game names, pressed in flight: the
                        // dive it asks for, its web swing, its Ground Strike.
                        askingGame = steering && swingState.owned && !calibrationInput &&
                                     (walk.zip ||
                                      (inFlight && (walk.interact || walk.demand.any() || named != 0)));
                        // Of those, the game's zip to a point (Command::asks bit 4), and
                        // its aim mode held on into from one: only it takes the player off
                        // a web line, which holds him till the game's move begins.
                        // Its web strike (Y) too, from a line (20:37: "no web strike from the
                        // line, it bugs").
                        askingZip = steering && swingState.owned && !calibrationInput &&
                                    (walk.zip || walk.aim || walk.interact);
                        // ANIMATED HYBRID: a web zip asked of the game - its zip to a point
                        // (the stick up, LT + RT) or its Y in the air (a web strike; a car's
                        // in a chase) - is noted this press frame (hybrid_owner zip hold).
                        // The owner leaves SpidyVR only if the game then begins that zip.
                        // Y on the ground or on Spidy's wall is interact, not a zip.
                        {
                            const bool yDown = steering && (origins.play() & buttonY) != 0;
                            // The floor-crawl flag lights in rooftop parkour: not a wall here.
                            const bool yWall =
                                swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall) ||
                                swingState.wall == static_cast<uint32_t>(game_swing::Surface::game);
                            const bool airY = yDown && !hybridYWas && !calibrationInput && !yWall &&
                                              (inFlight || (reaching && !swingState.grounded));
                            hybridYWas = yDown;
                            if ((walk.zip && !zipping) || airY)
                                hybridZipPress = true;
                        }
                        walk.jump = (keys & swingJumpKey) != 0;
                        // A jump on the game's own crawl - the player's, a web's
                        // takeoff, or the mount's that walks him on from it onto
                        // Spidy's wall - goes out as its RT + A, the only one
                        // that leaves the wall; A alone leaps along it. Only
                        // while the crawl holds him: RT held on in the air is
                        // the game's own web swing.
                        //
                        // From the moment a web holds, too, so the RT is down
                        // before the takeoff's A: the same RT + A pressed at
                        // once just after a web took hold was ignored the first
                        // time, and only the takeoff's retry 0.6 s later left
                        // the wall (October 14). RT alone on the crawl does
                        // nothing, so holding it early costs nothing.
                        //
                        // No longer up the wall with the stick: that was the
                        // game's own wall run (October 14), and since October 15
                        // the walls are Spidy's alone - the stick on the game's
                        // crawl walks the player on, onto them.
                        {
                            const bool onCrawl = swingState.status &&
                                                 swingState.wall == static_cast<uint32_t>(game_swing::Surface::game);
                            const bool webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                            walk.parkour = onCrawl && (walk.jump || (!motion.ventCrawl && webbed));
                            // NATIVE RUN (off by default): on the ground, the game's
                            // own, RT is held too while the stick runs, its sprint,
                            // vaults and wall runs (game_pad::NativeRun). Never with
                            // the crawl's RT above (gameOwnsGround is on no wall),
                            // never in the air, on a web, mounting or taking off
                            // (there RT is the game's web swing), and not while a
                            // grip or trigger is in use (webs, web balls, grabs) nor
                            // with the game's A, Y, zip, aim, pull or a prompt's
                            // buttons, which RT would turn into something else.
                            // MJ, Miles and Peter out of the suit play with the game's own
                            // controls (spiderMan): no RT for them, nor RUN CURVE.
                            game_pad::NativeRunGate runGate;
                            runGate.setting = values.nativeRun;
                            runGate.playing = playing && spiderMan;
                            runGate.calibrating = calibrationInput;
                            runGate.padLayer = layerOn;
                            runGate.mounting = mounting;
                            runGate.ventCrawl = motion.ventCrawl;
                            runGate.onSurface = motion.onSurface;
                            runGate.onCrawl = onCrawl;
                            runGate.gameOwnsGround = game_swing::gameOwnsGround(swingFresh, swingState);
                            runGate.grabbing = grabPhases[0] || grabPhases[1];
                            // STEADY STATES (off by default): no RT in a fight.
                            // COMBAT RESPONSIVE (off by default): none either.
                            runGate.steady = values.steadyStates || values.combatResponsive;
                            runGate.fighting =
                                fightLately || moverTable == config.base + bridge::combatCamera;
                            for (unsigned i = 0; i < 2; ++i) {
                                runGate.grips[i] = frame.hands[i].squeeze;
                                runGate.triggers[i] = frame.hands[i].trigger;
                            }
                            // RUN CURVE (off by default): on the game's own ground the
                            // stick reaches the full run sooner (run_update::curveStick);
                            // NATIVE RUN AT moves where its RT holds. Off and 70%: as before.
                            const float stickTilt = std::hypot(walk.right, walk.forward);
                            if (runGate.playing && !runGate.calibrating && !runGate.padLayer &&
                                !runGate.mounting && !runGate.ventCrawl && !runGate.onSurface &&
                                !runGate.onCrawl && runGate.gameOwnsGround)
                                run_update::curveStick(walk.right, walk.forward, values.runCurve);
                            const auto runAt = run_update::nativeRunThresholds(values.nativeRunPercent);
                            nativeRun.thresholds(runAt.onAbove, runAt.offBelow);
                            bool runAllowed = game_pad::nativeRunAllowed(runGate, walk);
                            // PARKOUR RT diagnostic (t86u): on the ground with the stick at
                            // the run (NATIVE RUN AT or more), why RT - the game's sprint and
                            // parkour - was not held; one line each 5 s of such frames.
                            {
                                const bool wantsRun = runGate.playing && swingFresh && swingState.grounded &&
                                                      stickTilt >= static_cast<float>(values.nativeRunPercent) * .01f;
                                if (wantsRun) {
                                    const auto why = game_pad::nativeRunBlock(runGate, walk);
                                    ++parkourRtFrames;
                                    parkourRtSeconds += frame.seconds;
                                    if (why == game_pad::RunBlock::none && !nativeRun.held())
                                        ++parkourRtRising; // allowed, RT not yet back (STEADY STATES' rise, hysteresis)
                                    else if (why != game_pad::RunBlock::none)
                                        ++parkourRtBlocked[static_cast<int>(why)];
                                    if (nativeRun.held())
                                        ++parkourRtHeld;
                                }
                                if (parkourRtSeconds >= 5.f) {
                                    char reasons[400]{};
                                    int at = 0;
                                    for (int r = 1; r < static_cast<int>(game_pad::RunBlock::count); ++r)
                                        if (parkourRtBlocked[r] && at < static_cast<int>(sizeof reasons) - 80)
                                            at += std::snprintf(reasons + at, sizeof reasons - at, "%s%s %u%%",
                                                                at ? ", " : "",
                                                                game_pad::runBlockName(static_cast<game_pad::RunBlock>(r)),
                                                                parkourRtBlocked[r] * 100u / std::max(parkourRtFrames, 1u));
                                    diag_log::writef("parkour RT: %u frames with the stick at the run on the ground, RT "
                                                     "held %u%%; off: %s%s rising %u%%",
                                                     parkourRtFrames, parkourRtHeld * 100u / std::max(parkourRtFrames, 1u),
                                                     at ? reasons : "-", at ? ";" : "",
                                                     parkourRtRising * 100u / std::max(parkourRtFrames, 1u));
                                    parkourRtFrames = parkourRtHeld = parkourRtRising = 0;
                                    parkourRtSeconds = 0;
                                    parkourRtBlocked = {};
                                }
                            }
                            // STEADY STATES (off by default): RT comes back only once
                            // everything has allowed it for a moment (ground_state::
                            // RiseDelay); it still goes at once.
                            {
                                const bool steadied = runSteady.update(frame.seconds, runAllowed);
                                if (values.steadyStates)
                                    runAllowed = steadied;
                            }
                            nativeRun.update(runAllowed, walk.right, walk.forward);
                            if (nativeRun.held())
                                walk.parkour = true;
                            // RUN LOGS (off by default): what running did, in the
                            // session's timeline (run_update::RunLog). Changes nothing.
                            if (values.runLogs) {
                                run_update::RunSample sample;
                                sample.seconds = frame.seconds;
                                sample.fresh = swingFresh && steering && !calibrationInput;
                                sample.grounded = swingState.grounded != 0;
                                sample.owned = swingState.owned != 0;
                                sample.webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                                sample.nativeRun = nativeRun.held();
                                sample.position = swingState.position;
                                sample.velocity = swingState.velocity;
                                sample.requested = swingState.requested;
                                sample.stick = stickTilt;
                                sample.sent = std::hypot(walk.right, walk.forward);
                                if (const char* line = runLog.update(sample))
                                    game_prompts::note(line);
                            }
                            // The state changes on and off the ground, in and out of a
                            // fight, to the session's diagnostics only (ground_state::
                            // StateLog), with RUN LOGS on. Changes nothing.
                            if (diag_log::enabled()) {
                                ground_state::Sample state;
                                state.fresh = swingFresh && steering && !calibrationInput;
                                state.grounded = swingState.grounded != 0;
                                state.owned = swingState.owned != 0;
                                state.webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                                state.wall = swingState.wall;
                                state.mount = swingState.mount;
                                state.takeoff = swingState.takeoffPhase;
                                state.gameGround = runGate.gameOwnsGround;
                                state.runRt = nativeRun.held();
                                state.fighting = runGate.fighting;
                                state.dodging = walk.dodge;
                                state.velocity = swingState.velocity;
                                if (const char* line = stateLog.update(frame.seconds, state))
                                    diag_log::write(line);
                                // HEAD PROBE (off by default; head_probe.hpp): the hero's
                                // head bone against the headset, per state, to the
                                // diagnostics' own "head probe:" ring. Read only: it
                                // changes nothing in play.
                                native_body::setHeadProbe(values.headProbe);
                                if (values.headProbe) {
                                    if (!headProbeWas)
                                        diag_log::write("head probe: on (read only; changes nothing in play)");
                                    head_probe::StateInputs probeState;
                                    probeState.playing = steering && !calibrationInput;
                                    probeState.fresh = swingFresh;
                                    probeState.grounded = state.grounded;
                                    probeState.owned = state.owned;
                                    probeState.webbed = state.webbed;
                                    probeState.zipping = walk.zip;
                                    probeState.fighting = state.fighting;
                                    probeState.dodging = state.dodging;
                                    probeState.runRt = state.runRt;
                                    probeState.wall = state.wall;
                                    headProbe->update(frame.seconds, head_probe::classify(probeState), state.owned,
                                                      native_body::headSample(), headProbeLines);
                                    for (unsigned i = 0; i < headProbeLines.count; ++i)
                                        diag_log::write(headProbeLines.text[i]);
                                } else if (headProbeWas) {
                                    diag_log::write("head probe: off");
                                }
                                headProbeWas = values.headProbe;
                                // perf: lines (always; changes nothing): the VR loop's
                                // frames and the mod's own work in them so far this frame.
                                LARGE_INTEGER workEnd{};
                                QueryPerformanceCounter(&workEnd);
                                const float work = static_cast<float>(workEnd.QuadPart - workStart.QuadPart) /
                                                   static_cast<float>(frequency.QuadPart);
                                if (const char* line =
                                        frameStats.update(frame.seconds, work, runGate.fighting))
                                    diag_log::write(line);
                            }
                        }
                        walk.scan = templePress.held() && steering;
                        walk.suit = suitPress.held() && steering;
                        walk.menu = menuTap > 0;
                        // Out of a warned attack's way by moving: a quick step or
                        // lean across the attacker's line, a step back or a duck is
                        // the game's B, the way the head went (a duck: the game's
                        // own way). Where B may go out, as B itself.
                        {
                            std::array<DodgeThreat, 6> warned{};
                            size_t count{};
                            for (const auto& m : senseMarks)
                                if (m.attacker && m.live && !m.brokenAt && count < warned.size())
                                    warned[count++] = {m.attacker, m.head, m.answer};
                            const Quat roomToWorld = motion.headPose.orientation * frame.head.orientation.conjugate();
                            const bool dodgePlay = playing && !calibrationInput && validTrackedPose(frame.head);
                            if (moveDodge.update(frame.seconds, dodgePlay, motion.headPose.position, frame.head.position,
                                                 roomToWorld, {warned.data(), count})) {
                                const Vec3 way = moveDodge.way();
                                walk.dodge = true;
                                walk.right = dot(way, motion.walkRightAxis);
                                walk.forward = dot(way, motion.walkForwardAxis);
                            }
                        }
                        // A perfect dodge: the game slows its own time within a
                        // moment of a B that went down with it at full speed.
                        if (walk.dodge && !dodging) {
                            dodgeWatch = game_time::telemetry().own > .95f ? .7f : 0.f;
                            // A whoosh across the body the way the stick sends him.
                            dodgeFrom = walk.right > 0 ? -1.f : 1.f;
                            feel(body_haptics::Kind::dodge, -1, 1, dodgeFrom);
                        }
                        // A dodge while a warning stands: the answer a red mark asks for.
                        if (walk.dodge && !dodging &&
                            std::any_of(senseMarks.begin(), senseMarks.end(),
                                        [](const SenseMark& m) { return m.live && !m.brokenAt; }))
                            ++senseDodges;
                        if (dodgeWatch > 0) {
                            dodgeWatch -= std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                            if (const auto time = game_time::telemetry(); time.installed && time.own < .9f) {
                                dodgeWatch = 0;
                                moments.start(Moment::dodge);
                                runtime.haptic(0, .5f);
                                runtime.haptic(1, .5f);
                                feel(body_haptics::Kind::perfectDodge, -1, 1, dodgeFrom);
                            }
                        }
                        auto buttons = origins.filtered(frame, mapping);
                        // On the pad layer a grip pressed at a back pocket is the
                        // phone's draw, not the game's bumper.
                        if (mapping == game_pad::Mapping::pad)
                            for (unsigned i = 0; i < 2; ++i)
                                if (pocketDraw.hands() & (1u << i))
                                    buttons.hands[i].squeeze = 0;
                        // An X the game names is its X: no wheel (NamedPress).
                        if (named & game_pad::NamedPress::controlX)
                            buttons.buttons &= ~static_cast<uint32_t>(buttonX);
                        // The gadget wheel: X held opens it in front of the hand
                        // X is on (the left on Touch and Index, the right on the
                        // Steam Frame); X tapped does nothing (the game menu until
                        // October 23: the phone's MENU now). Chosen, a gadget is
                        // equipped in the game and the free hand's trigger fires
                        // it; the web shooter at the top has it fire web balls
                        // again. A game that offers no gadget yet (a new game's
                        // first missions: the user's second profile, 2% in, on
                        // October 24) still opens it, the web shooter alone and
                        // NO GADGETS YET in the middle: until then X held did
                        // nothing at all, and that read as a broken wheel.
                        {
                            const unsigned hand = runtime.controllers()[1] == ControllerKind::frame ? 1u : 0u;
                            const uint32_t owned = std::min(listed.gadgetCount, 8u);
                            uint8_t places = 1;
                            for (uint32_t g = 0; g < owned; ++g)
                                if (listed.gadgets[g].place < gadgetPlaces)
                                    places |= static_cast<uint8_t>(1u << listed.gadgets[g].place);
                            const bool offered = wheelShooter && shooterStarted && !listed.gadgetError &&
                                                 mapping == game_pad::Mapping::gameplay && spiderMan;
                            const bool x = (buttons.buttons & buttonX) != 0;
                            if (offered)
                                buttons.buttons &= ~static_cast<uint32_t>(buttonX);
                            const auto chosen =
                                gadgetWheel.update(frame.seconds, x, offered && steering && !calibrationInput,
                                                   frame.hands[hand].grip.position, frame.head.position, places);
                            if (chosen.opened) {
                                runtime.haptic(hand, .3f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .3f);
                            }
                            if (chosen.lit) {
                                runtime.haptic(hand, .12f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .15f);
                            }
                            if (chosen.chosen >= 0) {
                                runtime.haptic(hand, .5f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .5f);
                                game_shooter::GadgetCommand command;
                                for (uint32_t g = 0; g < owned; ++g)
                                    if (listed.gadgets[g].place == chosen.chosen)
                                        command.equip = listed.gadgets[g].weapon;
                                gadgetFire = command.equip != 0;
                                command.fire = gadgetFire ? 1u : 0u;
                                wheelShooter(&command);
                            }
                        }
                        // ANIMATED STEERING / MODEL YAW (off by default): while pose
                        // yield is active, L-stick magnitude can run along actor
                        // facing (read from game.player). R-stick model yaw, when
                        // ANIMATED MODEL YAW is on, asks the game to turn the hero
                        // through left-stick walk facing (VALIDATION / InteractStep)
                        // — not a facing RVA write, and not CameraLook view yaw.
                        // Basic flips that own the right stick keep that ownership.
                        if ((values.animatedSteering || values.animatedModelYaw ||
                             (values.animatedHybrid && values.animatedMoves)) &&
                            !walk.dodge && !calibrationInput && mapping == game_pad::Mapping::gameplay) {
                            head_probe::StateInputs steerState;
                            steerState.playing = steering && !calibrationInput;
                            steerState.fresh = swingFresh;
                            steerState.grounded = swingState.grounded != 0;
                            steerState.owned = swingState.owned != 0;
                            steerState.webbed =
                                swingState.webs[0].attached || swingState.webs[1].attached;
                            steerState.zipping = zipping;
                            steerState.fighting = fightLately;
                            steerState.dodging = dodging;
                            steerState.runRt = nativeRun.held();
                            steerState.wall = swingState.wall;
                            // The web-hold button (grip by default; trigger if WEB BUTTON
                            // is swapped). Web balls use the other button and never set this.
                            const bool webAimIntent =
                                values.triggerWebs
                                    ? (frame.hands[0].trigger > .5f || frame.hands[1].trigger > .5f)
                                    : (frame.hands[0].squeeze > .5f || frame.hands[1].squeeze > .5f);
                            // Under HYBRID the owner decides (animOwns: no per-frame
                            // classify), and AIR / ONLY / AIR SPIN are ignored.
                            const bool hybridSteer = values.animatedHybrid && values.animatedMoves;
                            const animated_moves::Gate steerGate{values.animatedMoves,
                                                                 hybridSteer ? false : values.animatedAir,
                                                                 hybridSteer ? false : values.animatedOnly};
                            const bool yieldingPose =
                                hybridSteer ? hybridOwner.animOwns(values.animatedMoves, steerState)
                                            : animated_moves::yieldsBody(steerGate, steerState, webAimIntent,
                                                                         values.animatedArms);
                            const Vec3 actorAhead{body[8], body[9], body[10]};
                            // HYBRID parkour steering: while the animation owns and the
                            // view is head-locked, aim the left stick at what the player
                            // sees (headbone yaw + HMD turn since the view latched + the
                            // Level R-stick turn), low-passed, then into the game camera's
                            // walk axes. ANIMATED STEERING is bypassed (it keeps only the
                            // stick's magnitude). On a surface the crawl stick stays.
                            const bool hybridViewSteer =
                                hybrid_steer::usesView(hybridSteer, yieldingPose,
                                                       animated_moves::headFollow(values.animatedHead) !=
                                                               animated_moves::HeadFollow::off &&
                                                           !flatScreen) &&
                                !motion.onSurface;
                            const bool hybridViewOk = hybridViewSteer && hybridInputViewOk;
                            const float hybridViewYawNow = hybridInputViewYaw;
                            if (hybridViewSteer) {
                                const auto& moveHand = frame.hands[0];
                                const float walkMag = std::hypot(walk.right, walk.forward);
                                const bool hybridWalk = hybridMode && viewLockActive && hybridHeading.set &&
                                                        swingFresh && swingState.grounded;
                                if (hybridWalk && moveHand.valid && walkMag > 1e-3f && !hybridStick.drives) {
                                    // HYBRID: the rear dead band / settling: no run.
                                    walk.right = walk.forward = 0;
                                } else if (hybridViewOk && moveHand.valid && walkMag > 1e-3f) {
                                    // Direction from the raw stick, magnitude from the
                                    // vanilla walk (its run curve / zeroing kept). HYBRID v2:
                                    // the gate's run direction off the heading (free front
                                    // half, the headset's 30-degree window), full magnitude.
                                    const float tiltNow = std::max(std::hypot(moveHand.stickX, moveHand.stickY), 1e-3f);
                                    const auto viewed =
                                        hybridWalk
                                            ? hybrid_steer::viewRelativeStick(
                                                  std::sin(hybridStick.dir), std::cos(hybridStick.dir),
                                                  hybridHeading.yaw, motion.walkForwardAxis, motion.walkRightAxis,
                                                  std::min(walkMag, 1.f) * std::clamp(hybridStick.y / tiltNow, 0.f, 1.f))
                                            : hybrid_steer::viewRelativeStick(
                                                  moveHand.stickX, moveHand.stickY, hybridViewYawNow,
                                                  motion.walkForwardAxis, motion.walkRightAxis, std::min(walkMag, 1.f));
                                    if (frame.predictedDisplayTime - hybridSteerDiagAt > 1'000'000'000) {
                                        hybridSteerDiagAt = frame.predictedDisplayTime;
                                        diag_log::writef(
                                            "hybrid steer: stick(%.2f,%.2f) viewYaw %.0f actorYaw %.0f sent "
                                            "L(%.2f,%.2f)",
                                            moveHand.stickX, moveHand.stickY,
                                            hybridViewYawNow * (180.f / 3.14159265f),
                                            hybrid_handoff::actorLookYaw(actorAhead) * (180.f / 3.14159265f),
                                            viewed.right, viewed.forward);
                                    }
                                    walk.right = viewed.right;
                                    walk.forward = viewed.forward;
                                }
                                // Hero vs steer yaw (10 Hz while steering, an event
                                // when the gap crosses 45 degrees) and runaway turns.
                                {
                                    constexpr float toDeg = 180.f / 3.14159265f;
                                    const bool steeringNow = hybridViewOk && moveHand.valid &&
                                                             hybridInputL >= hybrid_steer::deadzone;
                                    const bool heroOk = finite(actorAhead) &&
                                                        std::hypot(actorAhead.x, actorAhead.z) > .2f;
                                    const float heroYaw = heroOk ? hybrid_handoff::actorLookYaw(actorAhead) : NAN;
                                    const bool steerOk = length(hybridInputMove) > .05f;
                                    const float steerYaw = steerOk ? hybrid_handoff::lookYaw(hybridInputMove) : NAN;
                                    const float gap = stable_yaw::wrap(steerYaw - heroYaw);
                                    const float rRaw = frame.hands[1].valid ? frame.hands[1].stickX : 0.f;
                                    const char* src =
                                        stable_yaw::sourceName(hybrid_movement::viewSource(values.movementMode));
                                    const auto watched =
                                        yawGap.update(frame.seconds, steeringNow && heroOk && steerOk, gap);
                                    if (watched.line)
                                        diag_log::writef(
                                            "yaw sync: hero %.0f view %.0f steer %.0f gap %.0f L %.2f R %.2f src %s",
                                            heroYaw * toDeg, hybridViewYawNow * toDeg, steerYaw * toDeg,
                                            gap * toDeg, hybridInputL, rRaw, src);
                                    if (watched.edge == stable_yaw::GapWatch::Edge::opened)
                                        diag_log::writef("yaw sync: gap over 45 (#%u) hero %.0f steer %.0f gap %.0f src %s",
                                                         yawGap.crossings, heroYaw * toDeg, steerYaw * toDeg,
                                                         gap * toDeg, src);
                                    else if (watched.edge == stable_yaw::GapWatch::Edge::closed)
                                        diag_log::writef("yaw sync: gap back under 40 after %d ms src %s",
                                                         static_cast<int>(watched.wideSeconds * 1000 + .5f), src);
                                    const auto runaway = yawRunaway.update(
                                        frame.seconds, heroYaw, hybridInputIntentYaw, steeringNow && heroOk,
                                        moveHand.valid ? moveHand.stickX : 0.f, moveHand.valid ? moveHand.stickY : 0.f,
                                        rRaw);
                                    if (runaway.fired)
                                        diag_log::writef(
                                            "yaw sync: RUNAWAY TURN #%u hero swept %+.0f deg in %d ms, own turn "
                                            "%+.0f, stick (%.2f,%.2f) R %.2f src %s",
                                            yawRunaway.reports, runaway.sweep * toDeg,
                                            static_cast<int>(runaway.seconds * 1000 + .5f),
                                            runaway.headingMove * toDeg, moveHand.stickX, moveHand.stickY, rRaw, src);
                                }
                            } else {
                                hybridViewYaw.reset();
                                stableHeading.reset();
                                yawGap.update(frame.seconds, false, 0.f);
                                yawRunaway.clear();
                            }
                            if (hybrid_steer::steeringApplies(values.animatedSteering, yieldingPose,
                                                              hybridViewSteer)) {
                                const auto steered = animated_steering::actorRelativeForward(
                                    walk.right, walk.forward, actorAhead, motion.walkForwardAxis,
                                    motion.walkRightAxis);
                                walk.right = steered.right;
                                walk.forward = steered.forward;
                            }
                            // AIR SPIN midair owns R-stick for SpidyVR-like 6DOF
                            // (RealisticFlip / takeHeading → trackingYaw + worldFlip).
                            // Gate MODEL YAW off so walk-facing cannot fight that yaw.
                            const bool airSpinOwnsStick = animated_air_spin::ownsRightStick(
                                hybridSteer ? 0 : values.animatedAirSpin, yieldingPose, steerState);
                            // Blend clock: hybridOwner updates later this frame; the
                            // hold uses state latched on a prior enterAnim edge.
                            const int64_t blendNowNs =
                                frame.predictedDisplayTime > 0
                                    ? frame.predictedDisplayTime
                                    : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                            const bool hybridYawHold =
                                hybridSteer && hybridBlend.active && !hybridBlend.toTraversal &&
                                hybridBlend.delayModelYaw &&
                                !hybrid_handoff::blendDone(blendNowNs - hybridBlend.startNs);
                            // OWNERSHIP UPDATE: a midair native animation (jump, point
                            // launch, zip) plays as in the main game: the R stick turns
                            // the camera there, never the hero (no walk-stick turn).
                            const bool midairAnimation =
                                hybrid_owner::midairAnimation(hybridSteer && yieldingPose, steerState);
                            const bool modelYawOwns =
                                animated_model_yaw::ownsStick(values.animatedModelYaw, yieldingPose,
                                                              airSpinOwnsStick) &&
                                !motion.basicFlipControls && !legStickActive && !hybridYawHold &&
                                !midairAnimation &&
                                // FULL head lock: the R stick turns the lock's heading only.
                                // HYBRID the same: the heading, the walk stick brings him.
                                !viewLockSteer && !(hybridMode && viewLockActive && hybridHeading.set);
                            // HYBRID view steering: MODEL YAW frees CameraLook only while
                            // the R-stick turns; otherwise the game camera follows the
                            // view so its parkour assists see a sensible camera.
                            const float hybridLookBefore = walk.look;
                            const bool modelYawClears =
                                hybridViewSteer && hybridViewOk
                                    ? hybrid_steer::clearsLook(
                                          modelYawOwns,
                                          // STABLE: while the left stick runs, the R stick
                                          // turns the heading the camera follows.
                                          stable_yaw::composeTurn(
                                              hybrid_movement::viewSource(values.movementMode),
                                              frame.hands[1].valid ? frame.hands[1].stickX : 0.f, hybridInputL))
                                    : modelYawOwns;
                            const bool blendHold =
                                hybridBlend.active &&
                                !hybrid_handoff::blendDone(blendNowNs - hybridBlend.startNs);
                            if (hybridBlend.recheckMove &&
                                hybrid_steer::doneRecheck(blendHold, hybridInputL)) {
                                cameraLook.reset();
                                cameraPitch.reset();
                                hybridViewYaw.reset();
                                stableHeading.reset();
                                hybridBlend.recheckMove = false;
                            }
                            if (modelYawClears || airSpinOwnsStick || hybridYawHold ||
                                hybridBlend.clearLook || hybridBlend.recheckMove ||
                                (viewLockSteer && !(hybridViewSteer && hybridViewOk))) {
                                // Free the right stick from CameraLook so it does not
                                // also yaw the follow camera (HMD free look stays XR;
                                // tracking yaw / AIR SPIN keep the stick). After a
                                // handoff, keep it off through the blend and until the
                                // run stick is released, so CameraLook cannot orbit a
                                // held forward under the turning walk camera.
                                walk.look = 0;
                                hybridBlend.clearLook = false;
                            } else if (hybridViewSteer && hybridViewOk) {
                                walk.look =
                                    cameraLook.update(hybrid_steer::viewForward(hybridViewYawNow),
                                                      {camera[8], camera[9], camera[10]}, cameraSteered);
                            }
                            // OWNERSHIP UPDATE: nothing of the mod's is steered into the
                            // game's own animation - no facing seed after a handoff, no
                            // launch facing toward momentum (both removed).
                            if (modelYawOwns) {
                                const float rStickX =
                                    frame.hands[1].valid && std::isfinite(frame.hands[1].stickX)
                                        ? frame.hands[1].stickX
                                        : 0.f;
                                auto yawed =
                                    animated_model_yaw::apply(walk.right, walk.forward, rStickX, actorAhead,
                                                              motion.walkForwardAxis, motion.walkRightAxis);
                                if (hybridViewOk &&
                                    (hybridInputL >= animated_model_yaw::deadzone ||
                                     std::abs(hybridInputR) >= animated_model_yaw::deadzone)) {
                                    const float magnitude = hybridInputL >= animated_model_yaw::deadzone
                                                                ? std::hypot(walk.right, walk.forward)
                                                                : length(hybridInputMove);
                                    yawed = animated_model_yaw::worldToStick(normalized(hybridInputMove),
                                                                             motion.walkForwardAxis,
                                                                             motion.walkRightAxis, magnitude);
                                }
                                walk.right = yawed.right;
                                walk.forward = yawed.forward;
                                if (frame.predictedDisplayTime - modelYawDiagAt > 250'000'000) {
                                    modelYawDiagAt = frame.predictedDisplayTime;
                                    const float actorYaw =
                                        std::isfinite(actorAhead.x) && std::isfinite(actorAhead.z)
                                            ? hybrid_handoff::actorLookYaw(actorAhead) * (180.f / 3.14159265f)
                                            : -1.f;
                                    diag_log::writef(
                                        "model yaw: stickX %.2f → L(%.2f,%.2f) actorYaw %.0f state %s",
                                        rStickX, yawed.right, yawed.forward, actorYaw,
                                        animated_moves::yieldName(steerState));
                                    if (hybridViewOk)
                                        diag_log::writef(
                                            "hybrid run turn: owner anim rawL %.2f R %.2f desired %.2f/%.2f "
                                            "native %.2f/%.2f finalL %.2f/%.2f look %.2f->%.2f",
                                            hybridInputL, rStickX, hybridInputMove.x, hybridInputMove.z,
                                            motion.swing.move.x, motion.swing.move.z, walk.right,
                                            walk.forward, hybridLookBefore, walk.look);
                                }
                            } else if (airSpinOwnsStick && values.animatedModelYaw &&
                                       frame.predictedDisplayTime - modelYawDiagAt > 1'000'000'000) {
                                // Assert midair: model-yaw path idle while AIR SPIN owns stick.
                                modelYawDiagAt = frame.predictedDisplayTime;
                                diag_log::writef(
                                    "model yaw: idle (air spin owns R-stick, state %s)",
                                    animated_moves::yieldName(steerState));
                            }
                            // TURN-IN-PLACE (view_yaw_lock.hpp): standing with the left stick
                            // idle, the walk stick points at the lock's heading until the hero
                            // faces it (closed loop on his facing; no facing write).
                            {
                                constexpr float toDeg = 180.f / 3.14159265f;
                                const bool heroOk =
                                    finite(actorAhead) && std::hypot(actorAhead.x, actorAhead.z) > .2f;
                                const float heroYaw = heroOk ? hybrid_handoff::actorLookYaw(actorAhead) : NAN;
                                view_yaw_lock::TurnInPlace::In tin;
                                tin.dt = frame.seconds;
                                tin.allowed = viewLockSteer && viewLockHeadingOk && yieldingPose && hybridViewOk &&
                                              heroOk && swingFresh && swingState.grounded &&
                                              hybridInputL < stable_yaw::steerZone && !midairAnimation &&
                                              !fightLately && !dodging && !zipping && !legStickActive &&
                                              !motion.onSurface && !motion.basicFlipControls;
                                tin.heading = viewLockHeading;
                                tin.heroYaw = heroYaw;
                                tin.angle = view_yaw_lock::angleOf(values.turnInPlaceDeg);
                                tin.rTurning = frame.hands[1].valid &&
                                               viewLock.turning(frame.hands[1].stickX, values.smoothTurn);
                                // FACING CHECK's hero turn: any angle, right after the event,
                                // lock or no lock, the landing's frames allowed.
                                tin.force = facingHeroPending;
                                tin.forceYaw = facingHeroYaw;
                                if (hybridHeroTurn) { // HYBRID: bring him onto the heading (any angle)
                                    tin.force = true;
                                    tin.forceYaw = hybridHeading.yaw;
                                }
                                tin.forceAllowed = yieldingPose && heroOk && swingFresh && swingState.grounded &&
                                                   hybridInputL < stable_yaw::steerZone && !fightLately &&
                                                   !dodging && !zipping && !legStickActive;
                                facingHeroPending = false;
                                const auto turned = turnInPlace.update(tin);
                                if (turned.dropped)
                                    diag_log::writef("facing check: %s hero turn dropped (no standing frame within "
                                                     "400 ms: moving, fighting or the left stick held)",
                                                     facing_check::eventName(facingTurnEvent));
                                if (turned.push) {
                                    const auto stick = animated_model_yaw::worldToStick(
                                        hybrid_steer::viewForward(turned.pushYaw), motion.walkForwardAxis,
                                        motion.walkRightAxis, turned.tilt);
                                    walk.right = stick.right;
                                    walk.forward = stick.forward;
                                }
                                if (turned.started && turned.forced)
                                    diag_log::writef("facing check: %s hero turn start, gap %+.0f deg, hero %.0f to %.0f",
                                                     facing_check::eventName(facingTurnEvent), turned.startGap * toDeg,
                                                     heroYaw * toDeg, facingHeroYaw * toDeg);
                                else if (turned.started)
                                    diag_log::writef(
                                        "turn in place: start (#%u, %s) gap %+.0f deg, angle %d, hero %.0f heading %.0f",
                                        turnInPlace.starts, turned.byStick ? "R stick" : "look", turned.startGap * toDeg,
                                        values.turnInPlaceDeg, heroYaw * toDeg, viewLockHeading * toDeg);
                                if (turned.ended != view_yaw_lock::TurnInPlace::End::none && turned.forced)
                                    diag_log::writef("facing check: %s hero turn %s after %d ms, turned %+.0f of %+.0f, "
                                                     "left %+.0f deg",
                                                     facing_check::eventName(facingTurnEvent),
                                                     view_yaw_lock::endName(turned.ended),
                                                     static_cast<int>(turned.seconds * 1000 + .5f),
                                                     turned.swept * toDeg, turned.startGap * toDeg, turned.gap * toDeg);
                                else if (turned.ended != view_yaw_lock::TurnInPlace::End::none)
                                    diag_log::writef(
                                        "turn in place: %s after %d ms, hero turned %+.0f of %+.0f deg (%.0f deg/s), "
                                        "left %+.0f%s; %u done, %u gave up, %u interrupted, %u wrong way",
                                        view_yaw_lock::endName(turned.ended),
                                        static_cast<int>(turned.seconds * 1000 + .5f), turned.swept * toDeg,
                                        turned.startGap * toDeg,
                                        turned.seconds > 0 ? std::abs(turned.swept) * toDeg / turned.seconds : 0.f,
                                        turned.gap * toDeg, turned.wrongWay ? " WRONG WAY" : "", turnInPlace.done,
                                        turnInPlace.gaveUp, turnInPlace.interrupted, turnInPlace.wrongWays);
                            }
                        }
                        // A dodge on a web line is the line's dash (game_swing's LineZip::dash):
                        // the game's own dodge left his feet in the air in its pose after it
                        // (20:14, 20:37). The game gets no B there; for the dash its stick
                        // goes all the way along the line, the dash's way, so its legs run.
                        if (values.webLines && lineState.phase == 2 && lineState.onLine) {
                            const bool dodgeDown = (frame.buttons & buttonB) != 0;
                            for (uint32_t k = 0; k < lineState.count && k < std::size(lineState.lines); ++k) {
                                if (lineState.lines[k].id != lineState.onLine)
                                    continue;
                                Vec3 along = lineState.lines[k].end - lineState.lines[k].start;
                                along.y = 0;
                                if (length(along) < .1f)
                                    break;
                                along = normalized(along);
                                if (dodgeDown && !lineDashDown) {
                                    const Vec3 stick = motion.walkForwardAxis * motion.walkForward +
                                                       motion.walkRightAxis * motion.walkRight;
                                    Vec3 look = motion.headPose.orientation.rotate({0, 0, -1});
                                    look.y = 0;
                                    const Vec3 way = length(stick) > .3f ? stick : look;
                                    lineDashWay = along * (dot(way, along) >= 0 ? 1.f : -1.f);
                                    lineDashLeft = .3f;
                                }
                                if (lineDashLeft > 0) {
                                    walk.forward =
                                        std::clamp(dot(lineDashWay, motion.walkForwardAxis), -1.f, 1.f);
                                    walk.right =
                                        std::clamp(dot(lineDashWay, motion.walkRightAxis), -1.f, 1.f);
                                }
                            }
                            walk.dodge = false;
                            lineDashDown = dodgeDown;
                        } else {
                            lineDashDown = false;
                            lineDashLeft = 0;
                        }
                        if (lineDashLeft > 0)
                            lineDashLeft -=
                                std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                        auto pad = game_pad::fromControllers(buttons, mapping, walk);
                        // What the controls of the names the game shows send it.
                        if (mapping == game_pad::Mapping::gameplay) {
                            pad.buttons |= namedPad.buttons;
                            pad.leftTrigger = std::max(pad.leftTrigger, namedPad.leftTrigger);
                            pad.rightTrigger = std::max(pad.rightTrigger, namedPad.rightTrigger);
                        }
                        if (namedPress.presses() != namedPressesBefore) {
                            namedPressesBefore = namedPress.presses();
                            const bool slam = namedPress.slams() != slamsBefore;
                            slamsBefore = namedPress.slams();
                            char line[96];
                            sprintf_s(line, "the player's press of a button the game names -> 0x%04x%s%s%s",
                                      namedPad.buttons, namedPad.leftTrigger ? " LT" : "",
                                      namedPad.rightTrigger ? " RT" : "",
                                      slam ? ", both fists slammed down"
                                      : named & game_pad::NamedPress::fist ? ", a thrown fist"
                                                                           : "");
                            game_prompts::note(line);
                        }
                        if (menuTap > 0)
                            menuTap -= std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                        // The phone's CAMERA, as its own D-pad up would: the
                        // game's camera out (its overlay up, the triggers then
                        // its aim and shutter: prompt_buttons::cameraDemand).
                        if (cameraTap > 0) {
                            pad.buttons |= game_pad::dpadUp;
                            cameraTap -= std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                        }
                        screenControls.apply(frame.seconds,
                                             flatScreen && motion.active && mapping == game_pad::Mapping::menus,
                                             buttons, pad);
                        // Both thumbsticks held in pause the game in every mapping, the menu button as
                        // before: under SteamVR a Quest's menu button opens SteamVR's dashboard instead.
                        if (pausePress.update(frame.seconds, mapping != game_pad::Mapping::none,
                                              frame.hands[0].stickClick, frame.hands[1].stickClick))
                            pad.buttons |= game_pad::start;
                        if (pausePress.spent())
                            pad.buttons &=
                                static_cast<uint16_t>(~(game_pad::leftThumb | game_pad::rightThumb));
                        if (pausePress.pauses() != pausesBefore) {
                            pausesBefore = pausePress.pauses();
                            runtime.haptic(0, .4f);
                            runtime.haptic(1, .4f);
                        }
                        game_pad::submit(pad, 200);
                        if (walk.interact && !interacting) {
                            ++interacts;
                            wallInteracts += onWall;
                            char line[96];
                            sprintf_s(line, "the player's Y -> the game%s%s", onWall ? ", on a wall" : "",
                                      walk.demand.any() ? ", carrying a prompt's demand" : "");
                            game_prompts::note(line);
                        }
                        interacting = walk.interact;
                        // DODGE TUMBLE (off by default): the view leans or rolls
                        // with a dodge, from the game's own start of one
                        // (dodgeLive: its ground dodge, kind 4, or its dodge on
                        // the move, 5; never Spidy's on a web) within half a
                        // second of the player's ask, and only while the game,
                        // not the swing, moves him and no web holds him. Each
                        // start and each ask the game did not answer goes in
                        // the session's timeline (game_prompts::note).
                        {
                            using dodge_tumble::Style;
                            const auto style = static_cast<Style>(std::clamp(values.dodgeTumble, 0, 2));
                            const float dt =
                                std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            const bool webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                            const bool allowed = style != Style::off && motion.active && !flatScreen &&
                                                 !calibrationInput && swingFresh && !swingState.owned &&
                                                 !webbed;
                            tumbleAskAge += dt;
                            if (walk.dodge && !dodging) {
                                Vec3 ahead = motion.headPose.orientation.rotate({0, 0, -1});
                                ahead.y = 0;
                                ahead = length(ahead) > .1f ? normalized(ahead) : motion.walkForwardAxis;
                                tumbleWay = dodge_tumble::way(walk.right, walk.forward, ahead,
                                                              normalized(cross(ahead, {0, 1, 0})));
                                tumbleAskAge = 0;
                                tumbleAskWaiting = style != Style::off;
                            }
                            char line[128];
                            if (dodgeLive) {
                                const volatile game_dodge::Live* live = dodgeLive;
                                const uint64_t starts = live->starts;
                                MemoryBarrier();
                                const uint32_t kind = live->lastKind, ours = live->ours;
                                if (tumbleStartsSeen == UINT64_MAX)
                                    tumbleStartsSeen = starts;
                                if (starts != tumbleStartsSeen) {
                                    tumbleStartsSeen = starts;
                                    if (style != Style::off) {
                                        const bool tumbleAsked = tumbleAskWaiting && tumbleAskAge <= .5f;
                                        const bool tumbles =
                                            allowed && tumbleAsked && dodge_tumble::tumbles(kind) && !ours;
                                        if (tumbles)
                                            dodgeTumble.start(
                                                tumbleWay.pitch && style == Style::roll ? Style::lean : style,
                                                tumbleWay.axis, tumbleWay.side);
                                        sprintf_s(
                                            line,
                                            "dodge tumble: the game's dodge began (kind %u%s), %d ms after "
                                            "the ask%s -> %s",
                                            kind, ours ? ", Spidy's web dodge" : "",
                                            tumbleAsked ? static_cast<int>(tumbleAskAge * 1000) : -1,
                                            tumbleAsked ? "" : " (none asked)",
                                            tumbles ? (tumbleWay.pitch ? "a flip's lean" : "a roll's tumble")
                                                    : "no tumble");
                                        game_prompts::note(line);
                                        tumbleAskWaiting = false;
                                    }
                                }
                            } else if (allowed && walk.dodge && !dodging) {
                                // An older ray module: the ask alone, unconfirmed.
                                dodgeTumble.start(tumbleWay.pitch && style == Style::roll ? Style::lean
                                                                                          : style,
                                                  tumbleWay.axis, tumbleWay.side);
                                game_prompts::note(
                                    "dodge tumble: no SpidyDodgeLive, tumbling on the ask alone");
                                tumbleAskWaiting = false;
                            }
                            if (tumbleAskWaiting && tumbleAskAge > .5f) {
                                game_prompts::note(
                                    "dodge tumble: a dodge asked for, and none of the game's began "
                                    "within 500 ms");
                                tumbleAskWaiting = false;
                            }
                            dodgeTumble.update(dt, allowed);
                            tumbleStepped = true;
                        }
                        dodges += walk.dodge && !dodging;
                        dodging = walk.dodge;
                        heals += walk.heal && !healing;
                        healing = walk.heal;
                        zips += walk.zip && !zipping;
                        zipBegan = walk.zip && !zipping;
                        zipping = walk.zip;
                    }
                    // Body haptics: what goes on, every frame, in VR play only (a
                    // menu, a scene, the game screen: all quiet).
                    {
                        body_haptics::State felt;
                        felt.live = immersive;
                        felt.groups = hapticGroups();
                        // The room's turn into the world: the world's head over the room's.
                        const Pose& feedbackHead = actionHeadValid ? actionHead : motion.headPose;
                        const bool feedbackHeadValid = !nativeActionSample || actionHeadValid;
                        if (feedbackHeadValid && validTrackedPose(frame.head) && validTrackedPose(feedbackHead))
                            torso = facing.update(frame.head.orientation,
                                                  feedbackHead.orientation * frame.head.orientation.conjugate(),
                                                  frame.seconds);
                        const bool fresh = swingStarted && swingState.status && swingState.qpc &&
                                           static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10 &&
                                           finite(swingState.velocity);
                        felt.airborne = fresh && game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart);
                        felt.onWall = fresh && swingState.wall != 0;
                        if (fresh)
                            felt.velocity = torso.local(swingState.velocity);
                        for (unsigned i = 0; i < 2; ++i) {
                            const bool grabbing = grabPhases[i] != 0;
                            felt.webTension[i] = fresh && attachedBefore[i] && !grabbing ? swingState.webs[i].tension : 0.f;
                            felt.grabTension[i] = grabbing ? grabState.hands[i].tension : 0.f;
                        }
                        felt.charge = chargeJump.charging() ? chargeJump.charge() : 0.f;
                        felt.slowMotion = slowMotion.active();
                        // A fall in the hero's hit points is a blow: from the attack
                        // the game warned of last, within a second, else on the chest
                        // and the back at once.
                        float hitPoints = std::numeric_limits<float>::quiet_NaN();
                        if (const auto h = players.health(); h && player.hero)
                            read(h + game_player::hitPointsAt, &hitPoints, sizeof(hitPoints));
                        const float lost = heroHealth.update(hitPoints);
                        felt.health = heroHealth.fraction();
                        if (lost > 0 && immersive) {
                            ++hits;
                            const float strength = std::clamp(lost * 4, 0.f, 1.f);
                            const Vec3 to = blowFrom - (feedbackHead.position - Vec3{0, .4f, 0});
                            if (feedbackHeadValid && blowSeen && frame.predictedDisplayTime - blowSeen < 1'000'000'000 && length(to) > .1f) {
                                ++aimedHits;
                                feelFrom(body_haptics::Kind::hit, -1, strength, to);
                            } else {
                                feel(body_haptics::Kind::hit, -1, strength);
                            }
                        }
                        // MOVE HAPTICS: jumps, wall jumps, zip launches and landings
                        // on the controllers, at MOVE HAPTICS' own strength; and the
                        // same events for SOUND TRACE.
                        {
                            move_haptics::Input moved;
                            moved.dt = frame.seconds;
                            moved.live = immersive;
                            moved.fresh = fresh;
                            moved.airborne = felt.airborne;
                            moved.onWall = felt.onWall;
                            moved.webbed = fresh && (swingState.webs[0].attached || swingState.webs[1].attached);
                            moved.zipPressed = zipBegan;
                            moved.velocity = fresh ? swingState.velocity : Vec3{};
                            const auto ev = moveDetector.update(moved);
                            zipBegan = false;
                            const float moveScale = static_cast<float>(values.moveHapticsPct) / 100;
                            const auto play = [&](const move_haptics::Pulse& p) {
                                if (!(p.amplitude > 0))
                                    return;
                                for (int hand = 0; hand < 2; ++hand)
                                    if (p.hand < 0 || p.hand == hand)
                                        runtime.hapticUnscaled(hand, p.amplitude, p.seconds);
                            };
                            moveScheduler.update(frame.seconds, play);
                            if (ev.kind != move_haptics::Kind::none) {
                                const auto p = move_haptics::scaled(ev.pulse, moveScale);
                                play(p);
                                moveHapticPulses += p.amplitude > 0;
                                diag_log::writef("move haptics: %s %.1f m/s -> %.2f %d ms (move haptics %d%%)",
                                                 move_haptics::kindName(ev.kind), ev.speed, p.amplitude,
                                                 static_cast<int>(p.seconds * 1000 + .5f), values.moveHapticsPct);
                                if (values.soundTrace && soundTrace) {
                                    char what[48];
                                    std::snprintf(what, sizeof(what), "%s %.1f m/s", move_haptics::kindName(ev.kind),
                                                  ev.speed);
                                    soundWatch.add(now.QuadPart, what);
                                }
                            }
                            if (pointJumpFired && values.soundTrace && soundTrace)
                                soundWatch.add(now.QuadPart, "point jump");
                            if (values.soundTrace && soundTrace)
                                soundWatch.update(soundTrace, now.QuadPart, frequency.QuadPart,
                                                  [](const std::string& line) { diag_log::writef("%s", line.c_str()); });
                            else
                                soundWatch.clear();
                        }
                        const float landed =
                            landingWatch.update(frame.seconds, felt.airborne, fresh ? -swingState.velocity.y : 0.f);
                        if (landed > 0 && immersive) {
                            ++landings;
                            feel(body_haptics::Kind::landing, -1, landed);
                        }
                        haptics.set(felt);
                        // The player hears once what the devices do: on, and which; or
                        // why not. Without a Player installed (most players), nothing.
                        const auto told = haptics.counters();
                        if (told.status != hapticsTold ||
                            (told.status == bhaptics::Status::playing && told.devices != devicesTold)) {
                            std::string text;
                            switch (told.status) {
                            case bhaptics::Status::playing:
                                text = "Body haptics on: " + bhaptics::deviceNames(told.devices);
                                break;
                            case bhaptics::Status::connected:
                                text = "Body haptics: the bHaptics Player has no device on - pair them in it";
                                break;
                            case bhaptics::Status::noLibrary:
                                text = "Body haptics: bhaptics_library.dll is missing beside Spidy's modules";
                                break;
                            case bhaptics::Status::noApp:
                                text = "Body haptics: this build of Spidy has no bHaptics app";
                                break;
                            case bhaptics::Status::failed:
                                text = "Body haptics: the bHaptics SDK would not start";
                                break;
                            case bhaptics::Status::rejected:
                                text = "Body haptics: the bHaptics Player did not take this build's bHaptics app";
                                break;
                            default:
                                break;
                            }
                            if (!text.empty())
                                message(3, 0, text.c_str());
                            hapticsTold = told.status;
                            devicesTold = told.devices;
                        }
                        // Installed but not answering for ten seconds: not running.
                        if (told.status == bhaptics::Status::waiting && hapticsSince &&
                            GetTickCount64() - hapticsSince > 10000) {
                            hapticsSince = 0;
                            message(3, 0, "Body haptics: start the bHaptics Player to feel webs, blows and "
                                          "Spider-Sense on your vest - it connects by itself");
                        }
                    }
                    {
                        std::lock_guard lock(telemetry);
                        auto& d = SpidyXrData;
                        InterlockedIncrement64(&d.sequence);
                        ++d.frames;
                        d.tracked += motion.active;
                        d.leftHands += frame.hands[0].valid;
                        d.rightHands += frame.hands[1].valid;
                        d.nativeKeys = keys;
                        d.gate = gate;
                        d.cameraMover = moverTable > config.base && moverTable - config.base < 0x10000000
                                            ? static_cast<uint32_t>(moverTable - config.base)
                                            : 0;
                        d.players = playersFound;
                        d.playerRecord = player.record;
                        const auto pad = game_pad::telemetry();
                        d.padButtons = pad.buttons;
                        d.padInstalled = pad.installed;
                        d.padReads = pad.reads;
                        d.cameraCommits = game.cameraCalls;
                        d.playerCommits = game.matched;
                        d.interacts = interacts;
                        d.dodges = dodges;
                        d.heals = heals;
                        d.zips = zips;
                        d.zipReleases = zipPress.releases();
                        d.aimMarkers = values.aimMarkers;
                        d.settings = (values.webGrab ? 1u : 0u) | (values.punch ? 2u : 0u) | (values.body ? 4u : 0u) |
                                     (values.airWebs ? 8u : 0u) | (values.webShooter ? 16u : 0u) |
                                     (values.flips ? 32u : 0u) | (values.triggerWebs ? 64u : 0u) |
                                     (values.standOnWalls ? 128u : 0u) | (values.arcadeWebs ? 512u : 0u);
                        d.scans = templePress.scans();
                        d.chargeJumps = chargeJump.charges();
                        const auto prompts = game_prompts::live();
                        d.promptInvokes = prompts.invokes;
                        d.promptCalls = prompts.calls;
                        d.promptsRaised = prompts.raised;
                        d.promptsCleared = prompts.cleared;
                        d.promptTexts = prompts.named;
                        d.promptRewrites = prompts.rewritten;
                        d.promptDemand = prompts.demand;
                        d.promptLastDemand = prompts.lastDemand;
                        d.promptBeyondPlay = prompts.beyondPlay;
                        d.promptKeyboardTexts = prompts.keyboardTexts;
                        d.contextPresses = namedPress.presses();
                        d.padEntries = padLayer.entries();
                        d.padLayerNow = padLayer.latched() ? 1u : 0u;
                        d.wallInteracts = wallInteracts;
                        d.wallHandovers = wallHandover.handovers();
                        d.wallHandoversOffered = wallHandover.offered();
                        d.headPushFrames = rig.clearance().pushedFrames();
                        d.headPushMostMm = static_cast<uint32_t>(std::lround(rig.clearance().most() * 1000));
                        d.crawlFrames = crawlFrames;
                        d.crawlStood = crawlStood;
                        d.crawlStickSamples = crawlStick.samples();
                        d.crawlStickTurn = crawlStick.turn();
                        d.crawlStickAgreement = crawlStick.agreement();
                        d.crawlStickSpeed = crawlStick.speed();
                        d.headDroppedFrames = headDroppedFrames;
                        d.headroomLeast = headroomLeast;
                        if (sampleWarp && !sampleWarp(&warp)) {
                            d.crawlAsked = static_cast<uint32_t>(warp.asked);
                            d.crawlApplied = static_cast<uint32_t>(warp.applied);
                            d.crawlRefused = static_cast<uint32_t>(warp.refused);
                            d.crawlFaulted = static_cast<uint32_t>(warp.faulted);
                            d.crawlTaken = static_cast<uint32_t>(warp.taken);
                            d.crawlDeclined = static_cast<uint32_t>(warp.declined);
                            gameRunFrames += (warp.swing & 32u) != 0;
                        }
                        d.gameRunFrames = gameRunFrames;
                        d.holdGraceFrames = holdGraceFrames;
                        d.trackingGapFrames = trackingGapFrames;
                        d.trackingGapReasons = trackingGapReasons;
                        d.floorCrawlFrames = floorCrawlFrames;
                        d.crawlCapFrames = crawlCapFrames;
                        d.kneelFrames = kneelFrames;
                        d.offerPointFrames = offerPointFrames;
                        d.offerReachFrames = offerReachFrames;
                        d.offerAsks = offerUse.asks();
                        d.offerRefusals = offerUse.refusals();
                        d.offerPresses = offerUse.presses();
                        d.offerPicked = offerUse.picked();
                        d.offerFailures = offerUse.failures();
                        d.offersSeen = offersSeen;
                        d.offersState = offers.status | offers.error << 8;
                        d.offersMasked = static_cast<uint32_t>(offers.masked);
                        d.offersPosed = static_cast<uint32_t>(offers.posed);
                        d.offersGathered = offers.gathered;
                        d.offersMostChecked = offers.mostChecked;
                        d.offersSearches = offers.searches;
                        d.offersSpots = offers.spotsFound;
                        d.offersNear = offers.offers;
                        d.offerRetries = offerUse.retries();
                        d.senseShown = senseShown;
                        d.senseFrames = senseFrames;
                        d.senseSightFrames = senseSightFrames;
                        d.senseTingles = senseTingles;
                        d.senseBroken = senseBroken;
                        d.senseDodges = senseDodges;
                        d.suitPowers = suitPress.powers();
                        d.moveDodges = moveDodge.dodges();
                        d.dives = dives;
                        d.ownSounds = ownSounds;
                        d.seated = (values.seated ? 1u : 0u) | (seatLift.measures() ? 2u : 0u);
                        d.heightCm = static_cast<uint32_t>(values.heightCm);
                        d.seatedEyeMm = static_cast<uint32_t>(std::lround(seatLift.seatedEye() * 1000));
                        d.seatedLiftMm = static_cast<uint32_t>(std::lround(seatLift.lift() * 1000));
                        d.seatedMeasures = seatLift.measures();
                        {
                            const auto& p = pocketDraw.stats();
                            d.pocketPresses = p.presses;
                            d.pocketBegun = p.begun;
                            d.pocketDraws = p.draws;
                            d.pocketSlow = p.tooSlow;
                            d.pocketLetGo = p.letGo;
                            for (unsigned m = 0; m < PocketDraw::misses; ++m)
                                d.pocketMissed[m] = p.missed[m];
                            d.pocketLastMiss = p.lastMiss < PocketDraw::misses ? p.lastMiss + 1 : 0;
                            const float hand[3]{p.lastHand.x, p.lastHand.y, p.lastHand.z};
                            for (unsigned a = 0; a < 3; ++a)
                                d.pocketLastHand[a] =
                                    static_cast<int32_t>(std::lround(std::clamp(hand[a], -9.f, 9.f) * 1000));
                            d.pocketLastLostMs = static_cast<uint32_t>(std::lround(p.lastLost * 1000));
                            d.pocketLastLook = static_cast<int32_t>(std::lround(p.lastLook * 1800 / 3.14159265f));
                        }
                        d.eyeGaze = (static_cast<uint32_t>(runtime.eyeTracking()) + 1) |
                                    static_cast<uint32_t>(values.eyeAim) << 8;
                        d.eyeAimFrames = eyeAimFrames;
                        d.gazeFrames = gazeFrames;
                        d.eyeShots = eyeShots;
                        d.eyeZips = eyeZip.settled();
                        d.eyeZipsLate = eyeZip.late();
                        for (unsigned i = 0; i < 3; ++i) {
                            d.cameraForward[i] = camera[8 + i];
                            d.cameraUp[i] = camera[4 + i];
                        }

                        d.vrButtons = trackedFrame.buttons;
                        d.vrButtonsPlayed = vrButtonsPlayed;
                        // The session's diagnostics, as they come (diag_log.hpp), with RUN LOGS on.
                        if (diag_log::enabled())
                            publishDiag();
                        auto& p = SpidyPromptData;
                        InterlockedIncrement64(&p.sequence);
                        p.invokes = prompts.invokes;
                        p.calls = prompts.calls;
                        p.raised = prompts.raised;
                        p.cleared = prompts.cleared;
                        p.texts = prompts.named;
                        p.rewrites = prompts.rewritten;
                        p.demand = prompts.demand;
                        p.lastDemand = prompts.lastDemand;
                        p.beyondPlay = prompts.beyondPlay;
                        p.keyboardTexts = prompts.keyboardTexts;
                        // A name can join the census with no prompt call of its own, so
                        // the lists are copied out every so often as well as on a call.
                        if (prompts.calls != promptCallsBefore || ++promptRefresh % 64 == 0) {
                            promptCallsBefore = prompts.calls;
                            game_prompts::Calls seen;
                            game_prompts::recentCalls(seen);
                            p.lines = seen.count;
                            std::memcpy(p.perName, seen.perName, sizeof(p.perName));
                            std::memcpy(p.line, seen.line, sizeof(p.line));
                            p.names = seen.names;
                            std::memcpy(p.censusCalls, seen.calls, sizeof(p.censusCalls));
                            std::memcpy(p.example, seen.example, sizeof(p.example));
                        }
                        InterlockedIncrement64(&p.sequence);
                        {
                            const auto felt = haptics.counters();
                            auto& h = SpidyHapticsData;
                            InterlockedIncrement64(&h.sequence);
                            h.status = static_cast<uint32_t>(felt.status);
                            h.level = static_cast<uint32_t>(values.bodyHaptics);
                            h.devices = felt.devices;
                            h.devicesSeen = felt.devicesSeen;
                            h.connects = felt.connects;
                            h.events = felt.events;
                            h.frames = felt.frames;
                            h.refused = felt.refused;
                            h.hits = hits;
                            h.aimedHits = aimedHits;
                            h.landings = landings;
                            h.liveTicks = felt.liveTicks;
                            std::copy(felt.onTicks.begin(), felt.onTicks.end(), h.onTicks);
                            std::copy(felt.peakSum.begin(), felt.peakSum.end(), h.peakSum);
                            std::copy(felt.kinds.begin(), felt.kinds.end(), h.kinds);
                            h.groups = hapticGroups();
                            InterlockedIncrement64(&h.sequence);
                        }
                        const auto controllers = runtime.controllers();
                        d.controllers[0] = static_cast<uint32_t>(controllers[0]);
                        d.controllers[1] = static_cast<uint32_t>(controllers[1]);
                        d.controllerLayouts = runtime.layoutsTaken();
                        d.snapTurn = static_cast<uint32_t>(values.snapTurn);
                        d.smoothTurn = static_cast<uint32_t>(values.smoothTurn);
                        d.haptics = static_cast<uint32_t>(values.haptics);
                        d.screenSize = static_cast<uint32_t>(values.screenSize);
                        d.swingSpeed = values.swingSpeed;
                        d.weight = static_cast<uint32_t>(values.weight);
                        d.hud = static_cast<uint32_t>(values.hud);
                        d.flipSpeed = static_cast<uint32_t>(values.flipSpeed);
                        d.viewTilt = motion.active ? motion.viewTilt * 180 / 3.14159265f : 0.f;
                        d.interactSteps = interactStep.steps();
                        d.lookError = cameraLook.error();
                        d.lookTurns = cameraLook.turns();
                        d.screenDepth = static_cast<uint32_t>(values.screenDepth);
                        d.settingChanges = settingChanges;
                        const auto menu = game_menu::telemetry();
                        d.menuTabs = menu.tabs;
                        d.menuInstalled = menu.installed;
                        d.menuStatus = menuCode ? menuCode : menu.status;
                        d.surfaceEntries = surfaceEntries;
                        d.surfaceFrames = surfaceFrames;
                        std::memcpy(d.heroUp, body.data() + 4, 12);
                        d.standOff = length(motion.standOff);
                        if (motion.active && motion.onSurface) {
                            d.surfaceHeight = motion.surfaceHeight;
                            d.surfaceClearance = motion.surfaceClearance;
                        }
                        const bool calibrating = calibration.phase() != body_calibration::Phase::idle;
                        d.calibrationPhase = static_cast<uint32_t>(calibration.phase());
                        d.calibrationHint = calibrating ? static_cast<uint32_t>(calibration.hint()) : 0u;
                        d.calibrationProgress = calibrating ? calibration.progress() : 0.f;
                        d.eyeHeightMm = static_cast<uint32_t>(std::lround(calibratedEye * 1000));
                        d.armLengthMm = static_cast<uint32_t>(std::lround(calibratedArm * 1000));
                        d.calibrations = calibrations;
                        d.calibrationSkips = calibrationSkips;
                        d.calibrationFlags = calibrationPrompt ? 0u : 1u;
                        d.calibrationReach[0] = lastCalibration.reach[0];
                        d.calibrationReach[1] = lastCalibration.reach[1];
                        std::memcpy(d.head, motion.head.data(), 64);
                        const Mat4 basis = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto pose = native_view::relativePose(basis, motion.hands[i]);
                            std::memcpy(d.hands[i], pose.data(), 64);
                        }
                        InterlockedIncrement64(&d.sequence);
                    }
                    // Copies of the game's presented frame, for the screen.
                    if (gpuStarted)
                        native_gpu::wantScreen(viewing && !vrPlay && !fpScene);
                    showScreen = screen && !fpScene && !fpPending;
                    if (!viewing)
                        return false;
                    if (!gpuStarted) {
                        // From here the headset shows the game: its screen until
                        // gameplay, then the eye views.
                        native_gpu::Config gpu;
                        gpu.pid = config.pid;
                        gpu.base = config.base;
                        gpu.queue = config.queue;
                        gpu.durationMs = moduleDuration;
                        gpu.copyToXr = 2;
                        gpu.captureImages = config.options & 1;
                        check(native_gpu::start(&gpu), "Start native GPU bridge");
                        gpuStarted = true;
                        nativeDeadline =
                            config.durationMs ? GetTickCount64() + config.durationMs : UINT64_MAX;
                        message(3, 0, "Headset active");
                        if (startupStages.mark(diag_log::Stages::headset, GetTickCount64()))
                            stageLine(diag_log::Stages::headset);
                        return false; // initialization can exceed one predicted display interval
                    }
                    // Without gameplay the screen shows the game's presented
                    // frame; until it is up, the last image stays. The eye
                    // views get no command and stop rendering meanwhile.
                    if (!vrPlay && !scene)
                        return screen || gameScreen.holding();
                    if (!nativeStarted) {
                        struct NativeConfig {
                            uint32_t magic, version, bytes, pid;
                            uint64_t base;
                            uint32_t duration, flags, width, height;
                        };
                        // Flag 8: while immersive, the game's active view follows the
                        // head, so work the game does only for that view fits the eyes.
                        // Flag 16: each eye culls what its own previous frame hid, as the
                        // game's views do; without it the render thread did 2-4 times the
                        // work and held VR at 40-65 frames a second.
                        // Flag 32: the game's own view, on the head for that work, is not
                        // rendered as well; it cost as much as an eye and only the monitor
                        // showed it, which gets the left eye instead.
                        // Flag 64 (options 0x80000, --eye-lens-distances): the eyes keep the
                        // distances their own wide lenses give, about half the game's.
                        const uint32_t flags = ((config.options & 4) ? 5 : 13) | ((config.options & 16) ? 0 : 16) |
                                               ((config.options & (4 | 0x10000)) ? 0 : 32) |
                                               ((config.options & 0x80000) ? 64 : 0);
                        // The eyes' scene at `upscale` percent of their size, with a
                        // jittered lens and the engine's temporal anti-aliasing (bits 1,
                        // 2, 4): taken when the views are made.
                        const bool antialiased = !(config.options & 0x8000);
                        // Bit 8: no targets for screen-space reflections (options
                        // 0x40000: the game has them off, or --no-eye-reflections).
                        const uint32_t reflections = (config.options & 0x40000) ? 8u << 8 : 0;
                        check(SpidyEyeTemporal(reinterpret_cast<void*>(static_cast<uintptr_t>(
                                  (antialiased ? config.upscale | 7u << 8 : 100u) | reflections))),
                              "Eye anti-aliasing");
                        check(SpidyEyeShadows(reinterpret_cast<void*>(
                                  static_cast<uintptr_t>((config.options & 0x20000) ? 0 : 1))),
                              "Eye shadows");
                        check(SpidyMonitorRate(reinterpret_cast<void*>(static_cast<uintptr_t>(config.monitorRate))),
                              "Monitor rate");
                        NativeConfig views{
                            0x53534346, 1, 40, config.pid, config.base, moduleDuration, flags, dimensions[0],
                            dimensions[1]};
                        check(SpidyStart(&views), "Start native views");
                        nativeStarted = true;
                        // Game-drawn webs are optional: without them the overlay draws webs.
                        websStarted = !(config.options & 2) && !native_webs::start(config.base, player.record);
                        // The player's body is optional too: without it the hero stays
                        // hidden and the overlay draws gloves, as before.
                        bodyStarted = values.body && !native_body::start(config.base);
                        firstImageDeadline = GetTickCount64() + 3000;
                        message(3, 0, vrPlay ? "Gameplay found; waiting for the first eye images"
                                             : "A scene found; waiting for its first eye images");
                        if (startupStages.mark(diag_log::Stages::views, GetTickCount64()))
                            stageLine(diag_log::Stages::views, vrPlay ? "gameplay" : "a scene");
                        // Initialization can exceed one predicted display
                        // interval. A scene's screen keeps the game's frame.
                        return !vrPlay;
                    }
                    if ((!vrPlay || fpPlayBlip) && fpScene) {
                        headset_smoothing::liveLevel.store(0, std::memory_order_relaxed);
                        headset_smoothing::publish({});
                        camera_motion::publish({});
                        // A first-person scene: the headset's own two eyes, inside the head of the character
                        // the scene plays. The eyes are sent relative to that head (anchored 2); the native
                        // views put them on the head of the frame they render (stereo_probe.cpp,
                        // resolveScene).
                        if (!sceneRefSet) {
                            // The view starts where the character looks, from where the headset is.
                            sceneRef = {frame.head.position, screenAhead(frame.head, 0).orientation};
                            sceneRefSet = validTrackedPose(frame.head);
                        }
                        const bool follow =
                            !levelView; // CUTSCENE HEAD TRACKING (off, or the scene's own camera: level)
                        const Pose toRef = inverse(sceneRef);
                        const Mat4 axes = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                        native_eyes::Command eyes;
                        eyes.enabled = 1;
                        eyes.serial = ++serial;
                        eyes.leaseMs = 250;
                        // Head tracking on: the view turns with the character's head as well as with yours.
                        // Off: only his place follows him, on a level frame (anchored 4); you look about
                        // freely from there.
                        eyes.anchored = follow ? 2 : 4;
                        if (!follow) {
                            // The right stick turns the view, as it turns you in play: smooth while held, or
                            // a snap a flick (the launcher's turn settings).
                            const float x = frame.hands[1].stickX;
                            const float degrees = 3.14159265f / 180;
                            if (values.smoothTurn > 0) {
                                if (std::abs(x) > .15f)
                                    sceneTurn -=
                                        x * static_cast<float>(values.smoothTurn) * degrees * frame.seconds;
                            } else if (values.snapTurn > 0) {
                                if (!sceneFlick && std::abs(x) > .7f) {
                                    sceneFlick = true;
                                    sceneTurn -=
                                        (x > 0 ? 1.f : -1.f) * static_cast<float>(values.snapTurn) * degrees;
                                } else if (sceneFlick && std::abs(x) < .4f)
                                    sceneFlick = false;
                            }
                        }
                        const Pose turned{{}, Quat::yaw(sceneTurn)};
                        {
                            // The heading the view has now: the character's head (or the level frame, head
                            // tracking off) turned by how far your head has turned from where it was when the
                            // scene began.
                            float base{};
                            const Vec3 headLook = frame.head.orientation.rotate({0, 0, -1});
                            const Vec3 refLook = sceneRef.orientation.rotate({0, 0, -1});
                            if (game_fp::sceneYaw(!follow, base) &&
                                std::hypot(headLook.x, headLook.z) > .3f &&
                                std::hypot(refLook.x, refLook.z) > .3f) {
                                const float turnedBy = std::remainder(std::atan2(-headLook.x, -headLook.z) -
                                                                          std::atan2(-refLook.x, -refLook.z),
                                                                      6.2831853f);
                                fpFacing = base + turnedBy + (follow ? 0.f : sceneTurn);
                                fpFacingMs = GetTickCount64();
                            }
                        }
                        for (unsigned i = 0; i < 2; ++i) {
                            Pose eye = compose(turned, compose(toRef, frame.eyes[i].pose));
                            eyes.eyes[i].world = native_view::relativePose(axes, eye);
                            const auto& f = frame.eyes[i].fov;
                            eyes.eyes[i].fov[0] = f.left;
                            eyes.eyes[i].fov[1] = f.right;
                            eyes.eyes[i].fov[2] = f.down;
                            eyes.eyes[i].fov[3] = f.up;
                        }
                        // The HUD panel (the scene's subtitles) goes in front of the head, as in play,
                        // and STEADY HUD's layer with it.
                        hudFacing = hudFollow.update(frame.head.orientation, frame.seconds);
                        hudHead = frame.head.position;
                        hudPlaced = true;
                        native_hud::follow(eyes.serial, frame.head.orientation.conjugate() * hudFacing);
                        NativeEyeFrame saved;
                        saved.serial = eyes.serial;
                        saved.motion.predictedDisplayTime = frame.predictedDisplayTime;
                        saved.trackingEyes = frame.eyes;
                        saved.scene = true;
                        saved.uiStyle = values.uiStyle == 1 ? UiStyle::tech : UiStyle::comic;
                        saved.screenAspect = SpidyFlatAspect();
                        history.remember(saved);
                        const auto code = SpidySetEyes(&eyes);
                        if (code && code != 4003)
                            check(code, "First-person scene eye command");
                        return !code;
                    }
                    if (!vrPlay) {
                        // The scene's two eyes: the game's own camera, set apart
                        // for this viewer and this screen (native_eyes::Stereo).
                        native_eyes::Command eyes;
                        eyes.enabled = 3;
                        eyes.serial = ++serial;
                        eyes.leaseMs = 250;
                        native_eyes::setStereo(
                            eyes, native_eyes::screenStereo(
                                      eyeDistance(frame.eyes[0].pose.position, frame.eyes[1].pose.position),
                                      vr_settings::screenWidth(values.screenSize), vr_settings::screenDistance,
                                      vr_settings::screenDepth(values.screenDepth)));
                        NativeEyeFrame saved;
                        saved.serial = eyes.serial;
                        // A held pair ages by the time it was asked for.
                        saved.motion.predictedDisplayTime = frame.predictedDisplayTime;
                        saved.flatScreen = saved.stereoScreen = true;
                        saved.screenPose = screenPose;
                        saved.screenAspect = SpidyFlatAspect();
                        history.remember(saved);
                        const auto code = SpidySetEyes(&eyes);
                        if (code && code != 4003)
                            check(code, "Scene eye command");
                        stereoScene = !code;
                        return true;
                    }
                    if (const float h = frame.head.position.y + seatRaise; std::isfinite(h) && h > .8f && h < 2.4f)
                        standingEye = standingEye <= 0 ? h
                                      : h > standingEye
                                          ? standingEye + (h - standingEye) * std::min(1.f, frame.seconds * 4)
                                          : standingEye - std::min(standingEye - h, frame.seconds * .01f);
                    bool yieldAnim = false;
                    bool animHeadFollowNow = false;
                    // A switch this frame that puts the head-locked view back on the neck (head_seat).
                    auto seatWhy = head_seat::Why::none;
                    auto animHeadMode = animated_moves::HeadFollow::off;
                    native_body::Command pose;
                    if (bodyStarted) {
                        // The hero's body follows the headset and the controllers,
                        // placed from the same feet as the eyes.
                        pose.serial = serial;
                        // Under a roof that holds the head below the player's
                        // own: the legs kneel rather than fold up in front of
                        // his eyes.
                        const bool kneels = (motion.standing || crawlsFloor) && motion.headDrop > .05f;
                        kneelFrames += kneels;
                        // Standing on a wall or a ceiling - Spidy's own or the
                        // game's crawl - the body stands on it with the player,
                        // never in the air the swing keeps the hero in there.
                        // ANIMATED MOVES / ANIMATED AIR (off by default; animated_moves.hpp):
                        // pose yield clears bodyOn so the game's animation drives the
                        // body. MOVES: ground / vault-mantle. AIR: freefall / swing /
                        // web-hold. Zip / wall-run / combat stay with the mod. Physics
                        // ownership (swing owned) is independent — no root-motion
                        // extraction. Freefall web aim interrupts immediately.
                        // ANIMATED HEAD: while yielding, finisher eye path; air clamps
                        // Full keeps anim pitch/roll (no air Level force).
                        head_probe::StateInputs animState;
                        animState.playing = steering && !calibrationInput;
                        animState.fresh = swingFresh;
                        animState.grounded = swingState.grounded != 0;
                        animState.owned = swingState.owned != 0;
                        animState.webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                        animState.zipping = zipping;
                        animState.fighting = fightLately;
                        animState.dodging = dodging;
                        animState.runRt = nativeRun.held();
                        animState.wall = swingState.wall;
                        // The web-hold button (grip by default; trigger if WEB BUTTON
                        // is swapped). Web balls use the other button and never set this.
                        const bool webAimIntent =
                            values.triggerWebs
                                ? (frame.hands[0].trigger > .5f || frame.hands[1].trigger > .5f)
                                : (frame.hands[0].squeeze > .5f || frame.hands[1].squeeze > .5f);
                        // ANIMATED ONLY (off by default): every gameplay state keeps
                        // the yield (wall / zip / combat too) so the OG SpidyVR body
                        // never reclaims mid-play; a cutscene/stale classify keeps it
                        // for StayLatch's grace before a real exit (animated_moves.hpp).
                        const bool hybridOn = values.animatedHybrid && values.animatedMoves;
                        // Hybrid forces AIR off for the gate; AIR SPIN / softlock stay inactive
                        // because midair never yields under hybrid (hybrid_owner.hpp).
                        const animated_moves::Gate animGate{values.animatedMoves,
                                                            hybridOn ? false : values.animatedAir,
                                                            hybridOn ? false : values.animatedOnly};
                        const bool animOnly = !hybridOn && animated_moves::stays(animGate);
                        const int64_t handoffNs = frame.predictedDisplayTime > 0
                                                      ? frame.predictedDisplayTime
                                                      : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                        const bool webNow = animState.webbed;
                        // Intent inputs (existing signals only): measured horizontal
                        // speed from the actor's feet (not the swing's velocity, which
                        // reads high at takeoff), R-stick x, tracked HMD yaw, the web
                        // button, and the wall kind already in animState.wall.
                        {
                            const Vec3 feetNow{body[12], body[13], body[14]};
                            const float dt = std::isfinite(frame.seconds) ? frame.seconds : 0.f;
                            if (finite(feetNow) && hybridFeetWasSet && dt > 1e-4f && dt < .1f) {
                                const Vec3 d = feetNow - hybridFeetWas;
                                const float v = std::hypot(d.x, d.z) / dt;
                                // A teleport / respawn is not speed; ~0.1 s low-pass.
                                if (v < 80.f)
                                    hybridSpeedH += (v - hybridSpeedH) * std::min(1.f, dt / .1f);
                                else
                                    hybridSpeedH = 0.f;
                            } else if (!finite(feetNow)) {
                                hybridSpeedH = 0.f;
                            }
                            hybridFeetWas = feetNow;
                            hybridFeetWasSet = finite(feetNow);
                        }
                        hybrid_owner::Frame hybridFrame;
                        hybridFrame.state = animState;
                        hybridFrame.webRising = webNow && !hybridWebWas;
                        hybridFrame.webIntent = !gripCancel.holdVr && webAimIntent;
                        hybridFrame.cancelGrip = gripCancel.cancel;
                        hybridFrame.cancelGripHeld = gripCancel.holdVr;
                        hybridFrame.groundAction = swingFresh && swingState.takeoff &&
                            swingState.serial >= cancellationSerialFloor;
                        // POINT JUMP: the launch takes the body (the ground exception)
                        // and holds it through the takeoff.
                        hybridFrame.pointJump = pointJumpFired;
                        hybridFrame.pointJumpHold = pointJumpHold;
                        hybridFrame.speedH = hybridSpeedH;
                        hybridFrame.speedHoldNs =
                            static_cast<int64_t>(values.animatedHybridSpeedMs) * 1'000'000ll;
                        hybridFrame.rStickX = frame.hands[1].valid && std::isfinite(frame.hands[1].stickX)
                                                  ? frame.hands[1].stickX
                                                  : 0.f;
                        hybridFrame.hmdYawValid = validTrackedPose(frame.head);
                        if (hybridFrame.hmdYawValid)
                            hybridFrame.hmdYaw = hybrid_handoff::lookYaw(
                                screenAhead(frame.head, 0).orientation.rotate({0, 0, -1}));
                        hybridFrame.cutscene = !animState.playing || !animState.fresh;
                        hybridFrame.zipPress = hybridZipPress;
                        hybridFrame.jumpAsk = hybridJumpAsk;
                        {
                            // Review M3/M4: a fresh, capped, setting-frozen strike hold.
                            const auto held = strikeHold.update(strikeRead, strikeUnderWay, strikeSequence,
                                                                values.animatedFullArms, handoffNs);
                            hybridFrame.strike = held.hold;
                            if (held.event != hybrid_owner::StrikeHold::Event::none && diag_log::enabled()) {
                                const char* owner = hybridOwner.domain == hybrid_owner::Domain::vr ? "vr" : "anim";
                                if (held.event == hybrid_owner::StrikeHold::Event::enter)
                                    diag_log::writef("full arms strike: enter (owner %s)", owner);
                                else
                                    diag_log::writef("full arms strike: leave %s after %.0f ms (owner %s)",
                                                     hybrid_owner::StrikeHold::eventName(held.event),
                                                     held.heldNs / 1e6, owner);
                            }
                        }
                        hybridFrame.velY = swingFresh && std::isfinite(swingState.velocity.y)
                                               ? swingState.velocity.y
                                               : NAN;
                        // JUMP TO SPIDYVR: the midair animation's timer (0 stays to the landing).
                        hybridFrame.jumpHandoffNs = static_cast<int64_t>(values.jumpHandoffMs) * 1'000'000ll;
                        hybridFrame.moveStick = frame.hands[0].valid && std::isfinite(frame.hands[0].stickX) &&
                                                        std::isfinite(frame.hands[0].stickY)
                                                    ? std::hypot(frame.hands[0].stickX, frame.hands[0].stickY)
                                                    : -1.f;
                        legChordAirAnimLast = false;
                        if (hybridOn) {
                            hybridOwner.update(hybridFrame, handoffNs);
                            legChordAirAnimLast = hybridOwner.domain == hybrid_owner::Domain::anim &&
                                                  hybrid_owner::midairAnimation(true, animState) &&
                                                  !hybridFrame.cutscene;
                            if (hybridOwner.enterVr || hybridOwner.enterAnim) {
                                ++armEpoch; // another owner's hands: velocities start afresh
                                actionFrameFloor = pose.serial;
                                // Never carry an animation-relative correction into the
                                // first original-VR action (or the opposite handoff).
                                for (auto& aim : forearmAims) aim.reset();
                                forearmTargets = {}; forearmJitter = {}; webTurnsNow = {};
                                // SPHERICAL AIM: the learned forearm means the same line in
                                // either owner, so a switch keeps the web on it at once
                                // instead of easing in from the controller's line.
                                for (unsigned k = 0; k < 2; ++k) {
                                    forearmBlends[k].reset();
                                    if (values.sphericalAim && forearmModels[k].ready())
                                        forearmAims[k].weight = 1;
                                }
                            }
                            if (hybridFrame.zipPress)
                                diag_log::writef("hybrid: web zip pressed, owner %s (%s), held for the zip",
                                                 hybrid_owner::domainName(hybridOwner.domain),
                                                 hybridOwner.enterAnim ? "vr->anim this frame"
                                                                       : "already anim");
                            if (hybridOwner.enterVr && hybridOwner.lastEnterVr == hybrid_owner::EnterVr::timed)
                                diag_log::writef("jump handoff: %s after %d ms (timer %d ms)",
                                                 hybrid_owner::jumpKindName(hybridOwner.lastJumpKind),
                                                 static_cast<int>(hybridOwner.lastJumpAirNs / 1'000'000),
                                                 values.jumpHandoffMs);
                            if (hybridOwner.enterVr) {
                                // Slice 2 G→T (anim→vr): capture the view the player is seeing,
                                // re-anchor tracking onto it, warm-reclaim IK, start
                                // the 180 ms pos+rot blend. Do this BEFORE followHero
                                // flips off so we do not publish a raw snap frame.
                                Vec3 worldEyes{};
                                float worldYaw = 0.f;
                                bool eyesOk = false;
                                Mat4 headMat{};
                                const bool levelHead =
                                    animated_moves::headFollow(values.animatedHead) ==
                                        animated_moves::HeadFollow::level;
                                if (levelHead ? game_fp::frameLevel(headMat) : game_fp::frame(headMat)) {
                                    worldEyes = {headMat[12], headMat[13], headMat[14]};
                                    worldYaw = hybrid_handoff::lookYaw({headMat[8], headMat[9], headMat[10]});
                                    eyesOk = finite(worldEyes);
                                }
                                if (!eyesOk && motion.active) {
                                    worldEyes = {(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                 (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                 (motion.eyes[0][14] + motion.eyes[1][14]) * .5f};
                                    worldYaw = hybrid_handoff::lookYaw(
                                        motion.headPose.orientation.rotate({0, 0, -1}));
                                    eyesOk = finite(worldEyes);
                                }
                                // Prefer actor look-yaw (MODEL YAW ownership while
                                // yielding) so torso/hands match after reclaim.
                                const Vec3 actorAhead{body[8], body[9], body[10]};
                                if (finite(actorAhead) &&
                                    length(Vec3{actorAhead.x, 0, actorAhead.z}) > .2f)
                                    worldYaw = hybrid_handoff::actorLookYaw(actorAhead);
                                // Slice C: what the headset last showed, carried with the
                                // player since, is both the blend's source and the view
                                // tracking adopts (place and look yaw): the takeover starts
                                // where the player was looking, and the tracked head and
                                // hands go on from the head-locked frame they were in. Only
                                // the head lock's pitch/roll is left for the blend.
                                const bool shownOk = hybrid_handoff::validShown(hybridShown, handoffNs);
                                float shownLook = 0.f;
                                if (shownOk && hybrid_handoff::shownYaw(hybridShown, shownLook)) {
                                    const Vec3 shownEyes =
                                        hybrid_handoff::shownCentre(hybridShown) +
                                        hybrid_handoff::travel(hybridShown.anchor, motion.anchor);
                                    if (finite(shownEyes)) {
                                        worldEyes = shownEyes;
                                        worldYaw = shownLook;
                                        eyesOk = true;
                                    }
                                }
                                float dPos = 0.f, dYaw = 0.f;
                                if (eyesOk) {
                                    const Vec3 before =
                                        motion.active
                                            ? Vec3{(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                   (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                   (motion.eyes[0][14] + motion.eyes[1][14]) * .5f}
                                            : worldEyes;
                                    if (finite(before))
                                        dPos = length(before - worldEyes);
                                    const float preYaw = hybrid_handoff::lookYaw(
                                        motion.headPose.orientation.rotate({0, 0, -1}));
                                    dYaw = hybrid_handoff::wrapPi(worldYaw - preYaw) * (180.f / 3.14159265f);
                                }
                                // POSITION LOCK: one authority, the actor's feet. The tracking
                                // origin goes over them (look yaw kept); the anim head's offset
                                // is not baked into the VR stint. The eye blend covers the move.
                                if (validTrackedPose(frame.head)) {
                                    const Vec3 lockFeet{body[12], body[13], body[14]};
                                    if (finite(lockFeet)) {
                                        const float discarded = rig.lockToFeet(
                                            lockFeet, eyesOk ? worldEyes : Vec3{NAN, NAN, NAN},
                                            eyesOk ? worldYaw : hybrid_handoff::lookYaw(
                                                motion.headPose.orientation.rotate({0, 0, -1})),
                                            frame.head);
                                        positionLockCheckNs = handoffNs + 1'000'000'000ll;
                                        diag_log::writef("position lock: G->T origin on actor feet (%.1f, %.1f, "
                                                         "%.1f), anim-view offset discarded %.2f m",
                                                         lockFeet.x, lockFeet.y, lockFeet.z, discarded);
                                        // PUNCH VIEW EASE (view_ease.hpp; the user, November 12
                                        // 01:13): a punch's switch holds the view where the last
                                        // animated frame showed it (worldEyes) and eases onto the
                                        // lock; eyes, hands, webs and body move together (the
                                        // rig's placement), the lock under them unchanged. Punch
                                        // only; 0 is the old snap. Any other switch ends one.
                                        // AIR VIEW EASE (November 12 05:41): the same hold and
                                        // ease for the timed jump handoff, a flip break and the
                                        // grip cancel (hybrid_owner::viewEaseMs); 0 the snap.
                                        const int easeMs = hybrid_owner::viewEaseMs(
                                            hybridOwner.lastEnterVr, values.punchViewEaseMs, values.airViewEaseMs);
                                        const auto why = hybridOwner.lastEnterVr;
                                        const char* easeName = why == hybrid_owner::EnterVr::punch  ? "punch"
                                                               : why == hybrid_owner::EnterVr::timed ? "jump handoff"
                                                               : why == hybrid_owner::EnterVr::flip  ? "flip-break"
                                                                                                     : "grip-cancel";
                                        // Log the previous ease's end under its own name first.
                                        if (rig.viewEase().active()) {
                                            rig.cancelViewEase(view_ease::End::handoff);
                                            punchEaseEndsLogged = rig.viewEase().ends();
                                            diag_log::writef("%s view ease: %s after %d ms", easeWhy,
                                                             view_ease::endName(rig.viewEase().lastEnd()),
                                                             static_cast<int>(rig.viewEase().elapsed() * 1000 + .5f));
                                        }
                                        if (easeMs > 0 && eyesOk &&
                                            rig.beginViewEase(worldEyes, frame.head, static_cast<float>(easeMs) * .001f)) {
                                            easeWhy = easeName;
                                            punchEaseEndsLogged = rig.viewEase().ends();
                                            diag_log::writef("%s view ease: start offset %.2f m, dur %d ms", easeName,
                                                             rig.viewEase().startOffset(), easeMs);
                                        } else {
                                            if (easeMs > 0 && eyesOk)
                                                diag_log::writef("%s view ease: none (offset %.2f m outside 0.01-3 m)",
                                                                 easeName, length(worldEyes - (lockFeet + Vec3{0, frame.head.position.y, 0})));
                                            else if (why == hybrid_owner::EnterVr::timed ||
                                                     why == hybrid_owner::EnterVr::flip ||
                                                     why == hybrid_owner::EnterVr::cancel)
                                                diag_log::writef("%s view ease: off (snap)", easeName);
                                            rig.cancelViewEase(view_ease::End::handoff);
                                            punchEaseEndsLogged = rig.viewEase().ends();
                                        }
                                    } else if (eyesOk) {
                                        rig.adoptEyes(worldEyes, worldYaw, frame.head);
                                        diag_log::write("position lock: no actor feet; anim view adopted");
                                    }
                                }
                                // A real jump or leap, or any other takeover while the hero is in
                                // the air (web, grip, rotate, speed, swing, wallrun): snapshot the
                                // pose the animation left and seed SpidyVR from it on the next pose
                                // job, arms on the controllers at once, legs kept airborne. A
                                // grounded takeover still reclaims without a seed.
                                const bool airAdopt =
                                    hybrid_owner::adoptsFromAir(hybridOwner.lastEnterVr, animState.grounded);
                                if (airAdopt) {
                                    hybridAdoptSeen = native_body::jumpAdopt().count;
                                    native_body::beginJumpAdopt();
                                } else {
                                    native_body::beginReclaim();
                                }
                                hybridLeapAir = airAdopt;
                                hybridAdoptWaiting = airAdopt;
                                hybridBlend = {};
                                hybridBlend.active = true;
                                hybridBlend.toTraversal = true;
                                hybridBlend.startNs = handoffNs;
                                hybridBlend.fromEyes = worldEyes;
                                hybridBlend.fromYaw = worldYaw;
                                hybridBlend.fromValid = eyesOk;
                                // Prefer anim-head orientation (game_fp) so the
                                // blend settles pitch/roll out of Full head follow.
                                Quat animOri = Quat::yaw(worldYaw);
                                if (eyesOk) {
                                    const Vec3 right{headMat[0], headMat[1], headMat[2]};
                                    const Vec3 down{headMat[4], headMat[5], headMat[6]};
                                    const Vec3 forward{headMat[8], headMat[9], headMat[10]};
                                    if (length(right) > .2f && length(down) > .2f && length(forward) > .2f) {
                                        // Same layout as worldPose: images of +X, −Y, −Z.
                                        animOri = Quat::yaw(worldYaw); // level yaw carry
                                        // Approximate full orientation from basis via
                                        // looking along forward with down as −up.
                                        const Vec3 up = normalized(Vec3{-down.x, -down.y, -down.z});
                                        const Vec3 look = normalized(forward);
                                        const Vec3 r = normalized(cross(look, up));
                                        const Vec3 u = cross(r, look);
                                        // Build quat that rotates −Z→look, +Y→u (approx yaw*pitch*roll).
                                        // Use fromAxes-free path: store look as yaw+pitch via two turns.
                                        const float yaw = hybrid_handoff::lookYaw(look);
                                        const float pitch = std::asin(std::clamp(look.y, -1.f, 1.f));
                                        animOri = Quat::yaw(yaw) * Quat::around({1, 0, 0}, -pitch);
                                        (void)r;
                                        (void)u;
                                    }
                                }
                                if (motion.active) {
                                    for (unsigned i = 0; i < 2; ++i) {
                                        hybridBlend.fromEyePos[i] = {motion.eyes[i][12], motion.eyes[i][13],
                                                                     motion.eyes[i][14]};
                                        // Re-centre stereo around the anim eyes we captured.
                                        if (eyesOk) {
                                            const Vec3 centre{(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                              (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                              (motion.eyes[0][14] + motion.eyes[1][14]) * .5f};
                                            hybridBlend.fromEyePos[i] =
                                                worldEyes + (hybridBlend.fromEyePos[i] - centre);
                                        }
                                        hybridBlend.fromEyeOri[i] = animOri;
                                    }
                                } else if (eyesOk) {
                                    hybridBlend.fromEyePos[0] = hybridBlend.fromEyePos[1] = worldEyes;
                                    hybridBlend.fromEyeOri[0] = hybridBlend.fromEyeOri[1] = animOri;
                                }
                                hybridBlend.fromAnchor = motion.anchor;
                                if (shownOk) {
                                    for (unsigned i = 0; i < 2; ++i) {
                                        hybridBlend.fromEyePos[i] = hybridShown.eyes[i].position;
                                        hybridBlend.fromEyeOri[i] = hybridShown.eyes[i].orientation;
                                    }
                                    hybridBlend.fromAnchor = hybridShown.anchor;
                                    hybridBlend.fromValid = true;
                                }
                                hybridBlend.delayArmsClear = true;
                                hybridBlend.delayModelYaw = false; // OG stick from frame 0
                                hybridBlend.enterEyeDPos = dPos >= 0.f ? dPos : 0.f;
                                hybridBlend.enterEyeDYaw = dYaw;
                                hybridBlend.clearLook = true; // clear Walk::look next pad frame
                                hybridBlend.recheckMove = true;
                                hybridViewYaw.reset();
                                cameraLook.reset();
                                cameraPitch.reset();
                                game_fp::relatchHeadFrame();
                                diag_log::writef(
                                    "hybrid: anim->vr (%s) speed %.1f rot %.0f, G->T (%s) eyes dPos %.2f m "
                                    "dYaw %.0f, ikWarm 1, overlay->body %.0f ms",
                                    hybrid_owner::enterVrName(hybridOwner.lastEnterVr), hybridFrame.speedH,
                                    hybridOwner.lastSpin * (180.f / 3.14159265f),
                                    hybrid_owner::enterVrName(hybridOwner.lastEnterVr),
                                    hybridBlend.enterEyeDPos, hybridBlend.enterEyeDYaw,
                                    hybrid_handoff::handoffSec * 1000.f);
                                // HYBRID: SpidyVR owns the view now; a landing starts the heading again.
                                hybridHeading.reset();
                                hybridGate.reset();
                                // FACING CHECK: the view the player saw is what SpidyVR's view
                                // must start on (a point launch armed at its launch keeps its own).
                                if (!(facingCheck.armed && facingCheck.event == facing_check::Event::pointLaunch))
                                    facingCheck.arm(hybridOwner.lastEnterVr == hybrid_owner::EnterVr::pointJump
                                                        ? facing_check::Event::pointLaunch
                                                    : hybridOwner.lastEnterVr == hybrid_owner::EnterVr::timed
                                                        ? facing_check::Event::jumpHandoff
                                                        : facing_check::Event::takeover,
                                                    eyesOk ? worldYaw : NAN);
                            }
                            // Landing on the feet ends an ease at once (the animation's
                            // view takes over); any other switch back too.
                            if (hybridOwner.enterAnim)
                                rig.cancelViewEase(hybridOwner.lastEnterAnim == hybrid_owner::EnterAnim::land
                                                       ? view_ease::End::landed
                                                       : view_ease::End::handoff);
                            if (hybridOwner.enterAnim) {
                                if (hybridAdoptWaiting)
                                    diag_log::write(
                                        "hybrid: adopt never seeded (no SpidyVR body frame before "
                                        "the animation took him back)");
                                hybridLeapAir = hybridAdoptWaiting = false;
                                // Slice 3 T→G (vr→anim): blend eyes back to the headbone; keep
                                // IK state warm; delay MODEL YAW until the blend ends;
                                // seed actor facing from the rig look yaw. No adoptFeet.
                                Vec3 fromEyes{};
                                float fromYaw = 0.f;
                                bool fromOk = false;
                                if (motion.active) {
                                    fromEyes = {(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                (motion.eyes[0][14] + motion.eyes[1][14]) * .5f};
                                    fromYaw = hybrid_handoff::lookYaw(
                                        motion.headPose.orientation.rotate({0, 0, -1}));
                                    fromOk = finite(fromEyes);
                                }
                                float headDPos = -1.f, headDAng = 0.f;
                                Mat4 headMat{};
                                if (game_fp::frame(headMat)) {
                                    const Vec3 hb{headMat[12], headMat[13], headMat[14]};
                                    if (fromOk && finite(hb))
                                        headDPos = length(fromEyes - hb);
                                    const float hbYaw =
                                        hybrid_handoff::lookYaw({headMat[8], headMat[9], headMat[10]});
                                    if (fromOk)
                                        headDAng = std::abs(hybrid_handoff::wrapPi(hbYaw - fromYaw)) *
                                                   (180.f / 3.14159265f);
                                }
                                // The VR stint just ended (the owner's sinceNs is already this edge's).
                                const float landMs = static_cast<float>(hybridOwner.lastVrNs) / 1e6f;
                                const float landSpeed =
                                    swingFresh && finite(swingState.velocity)
                                        ? length(Vec3{swingState.velocity.x, 0, swingState.velocity.z})
                                        : 0.f;
                                hybridBlend = {};
                                hybridBlend.active = true;
                                hybridBlend.toTraversal = false;
                                hybridBlend.startNs = handoffNs;
                                hybridBlend.fromEyes = fromEyes;
                                hybridBlend.fromYaw = fromYaw;
                                hybridBlend.fromValid = fromOk;
                                if (motion.active) {
                                    for (unsigned i = 0; i < 2; ++i) {
                                        hybridBlend.fromEyePos[i] = {motion.eyes[i][12], motion.eyes[i][13],
                                                                     motion.eyes[i][14]};
                                        hybridBlend.fromEyeOri[i] = motion.headPose.orientation;
                                    }
                                }
                                // Slice C: from exactly what the headset last showed (a blend
                                // still under way included), travelling with the player.
                                hybridBlend.fromAnchor = motion.anchor;
                                if (hybrid_handoff::validShown(hybridShown, handoffNs)) {
                                    for (unsigned i = 0; i < 2; ++i) {
                                        hybridBlend.fromEyePos[i] = hybridShown.eyes[i].position;
                                        hybridBlend.fromEyeOri[i] = hybridShown.eyes[i].orientation;
                                    }
                                    hybridBlend.fromAnchor = hybridShown.anchor;
                                    hybridBlend.fromValid = true;
                                }
                                hybridBlend.delayArmsClear = false;
                                hybridBlend.delayModelYaw = true; // MODEL YAW after H
                                hybridBlend.landSpeed = landSpeed;
                                hybridBlend.clearLook = true;
                                hybridBlend.recheckMove = true;
                                hybridViewYaw.reset();
                                cameraLook.reset();
                                cameraPitch.reset();
                                game_fp::relatchHeadFrame();
                                const char* why = hybrid_owner::enterAnimName(hybridOwner.lastEnterAnim);
                                // The head is often not sampled yet on the edge frame (head follow
                                // turns on below): say so rather than log a 0.00 m that never was.
                                if (headDPos >= 0.f)
                                    diag_log::writef("hybrid: vr->anim (%s %.0f ms), T->G (%s %.0f ms, speed "
                                                     "%.1f) eyeBlend %.0f ms, headbone dPos %.2f m dAng %.0f",
                                                     why, landMs, why, landMs, landSpeed,
                                                     hybrid_handoff::handoffSec * 1000.f, headDPos, headDAng);
                                else
                                    diag_log::writef("hybrid: vr->anim (%s %.0f ms), T->G (%s %.0f ms, speed "
                                                     "%.1f) eyeBlend %.0f ms, headbone n/a",
                                                     why, landMs, why, landMs, landSpeed,
                                                     hybrid_handoff::handoffSec * 1000.f);
                                // FACING CHECK: the view SpidyVR showed last is the facing the
                                // player expects on the animation's side, view and hero.
                                {
                                    float before = NAN;
                                    if (!hybrid_handoff::validShown(hybridShown, handoffNs) ||
                                        !hybrid_handoff::shownYaw(hybridShown, before))
                                        before = fromOk ? fromYaw : NAN;
                                    facingCheck.arm(hybridOwner.lastEnterAnim == hybrid_owner::EnterAnim::land
                                                        ? facing_check::Event::landing
                                                        : facing_check::Event::animReentry,
                                                    before);
                                    // HYBRID: the heading is the view SpidyVR showed, held from the
                                    // edge frame (before the animation's first frame turns him onto
                                    // the game's facing), so the head-locked view cannot swing.
                                    if (hybrid_movement::modeOf(values.movementMode) ==
                                            hybrid_movement::Mode::hybrid &&
                                        std::isfinite(before)) {
                                        constexpr float toDeg = 180.f / 3.14159265f;
                                        const float actorNow = hybrid_handoff::actorLookYaw({body[8], body[9], body[10]});
                                        hybridHeading.begin(hybrid_movement::Heading::Hold::facing, before);
                                        viewLock.set = true;
                                        viewLock.yaw = hybridHeading.yaw;
                                        diag_log::writef("hybrid landing: view %.0f, game facing %.0f (gap %+.0f), air "
                                                         "facing sync %d ms this stint",
                                                         before * toDeg, actorNow * toDeg,
                                                         stable_yaw::wrap(before - actorNow) * toDeg,
                                                         static_cast<int>(hybridAirSyncSec * 1000 + .5f));
                                    }
                                    hybridAirSyncSec = 0;
                                }
                            }
                        } else {
                            hybridOwner = {};
                            actionFrameFloor = 0;
                            hybridBlend = {};
                            hybridLeapAir = hybridAdoptWaiting = false;
                        }
                        // BODY KINEMATICS: velocities, checks; a line only at an owner or
                        // state change, a new mismatch, or a 5 s heartbeat while one lasts.
                        {
                            kin.owner = hybridOwner.domain == hybrid_owner::Domain::vr ? 1u : 0u;
                            kin.state = static_cast<uint32_t>(head_probe::classify(animState));
                            const Vec3 feetNow{body[12], body[13], body[14]};
                            kin.feetValid = finite(feetNow);
                            kin.feet = feetNow;
                            kin.actorYaw = hybrid_handoff::actorLookYaw({body[8], body[9], body[10]});
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& arm = kinematicsArms.arms[i];
                                kin.forearmValid[i] = arm.valid && kin.feetValid && finite(arm.elbow) &&
                                                      finite(arm.wrist) && kin.owner == 1u;
                                kin.elbow[i] = arm.elbow;
                                kin.wrist[i] = arm.wrist;
                            }
                            kinematicsTracker.derive(kin);
                            const uint32_t checked = kinematics::validate(kin);
                            if (kinematicsTracker.shouldLog(kin, checked) && diag_log::enabled()) {
                                const uint32_t issues = kinematicsTracker.active();
                                const Vec3 h = kin.head.position - kin.feet;
                                diag_log::writef(
                                    "kinematics: owner %s state %s issues 0x%x head %.2f/%.2f/%.2f from feet "
                                    "v %.1f m/s spin %.0f deg/s, L %u v %.1f, R %u v %.1f, actorYaw %.0f viewYaw %.0f, "
                                    "arm map %s",
                                    kin.owner ? "vr" : "anim", head_probe::stateName(static_cast<head_probe::State>(kin.state)),
                                    issues, h.x, h.y, h.z, length(kin.headVelocity), kin.headSpin * 57.29578f,
                                    kin.handValid[0] ? 1u : 0u, length(kin.handVelocity[0]), kin.handValid[1] ? 1u : 0u,
                                    length(kin.handVelocity[1]), kin.actorYaw * 57.29578f, kin.viewYaw * 57.29578f,
                                    armView.valid ? "per-frame" : "none");
                            }
                        }
                        // FACING CHECK, SpidyVR's side: the view after a takeover or a
                        // point launch against the view before it; the rig's yaw turns it back
                        // (about the eyes). Not on the edge's own frame (the rig adopts then).
                        {
                            if (facingCheck.events != facingArmSeen) {
                                facingArmSeen = facingCheck.events;
                                facingFrames = 0;
                            }
                            if (facingCheck.armed) {
                                ++facingFrames;
                                if (hybridOwner.domain == hybrid_owner::Domain::vr) {
                                    facing_check::Sample fs;
                                    const bool headOk = motion.active && validTrackedPose(motion.headPose);
                                    fs.viewOk = headOk && facingFrames >= 2;
                                    fs.view = headOk ? hybrid_handoff::lookYaw(
                                                           motion.headPose.orientation.rotate({0, 0, -1}))
                                                     : 0.f;
                                    fs.viewFixable = validTrackedPose(frame.head);
                                    fs.heroMatters = false;
                                    const auto fc = facingCheck.update(frame.seconds, fs);
                                    if (fc.viewFix != 0.f) {
                                        const Vec3 eyesNow{(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                           (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                           (motion.eyes[0][14] + motion.eyes[1][14]) * .5f};
                                        rig.adoptEyes(eyesNow, stable_yaw::wrap(fs.view + fc.viewFix), frame.head);
                                    }
                                    logFacing(facingCheck, fc);
                                } else if (facingFrames > 45) {
                                    // The animation's side never sampled it (no head lock this
                                    // stint): let it run out and say so.
                                    logFacing(facingCheck, facingCheck.update(frame.seconds, {}));
                                }
                            }
                        }
                        // Jump adopt's first frame, once the pose job has seeded it.
                        if (hybridAdoptWaiting) {
                            const auto adopt = native_body::jumpAdopt();
                            if (adopt.count != hybridAdoptSeen) {
                                hybridAdoptSeen = adopt.count;
                                hybridAdoptWaiting = false;
                                diag_log::writef(
                                    "hybrid: %s adopt seeded from the live pose (%u joints): first "
                                    "frame hips %.3f m legs %.3f m off it, body %.2f in, wrists %.3f "
                                    "/ %.3f m from the controllers",
                                    hybridOwner.lastEnterVr == hybrid_owner::EnterVr::jump ? "jump"
                                                                                           : "air",
                                    adopt.joints, adopt.hips, adopt.legs, adopt.weight, adopt.handError[0],
                                    adopt.handError[1]);
                            }
                        }
                        // An adopted jump's legs keep its shape only in the air.
                        if (swingState.grounded)
                            hybridLeapAir = false;
                        // POSITION LOCK residual: 1 s into a VR stint, and as it ends.
                        // Across-ground head floor point vs actor feet; it includes
                        // the player's own physical steps/leans since the switch.
                        if (hybridOn && validTrackedPose(frame.head) &&
                            ((positionLockCheckNs > 0 && handoffNs >= positionLockCheckNs &&
                              hybridOwner.domain == hybrid_owner::Domain::vr) ||
                             hybridOwner.enterAnim)) {
                            const Vec3 feetNow{body[12], body[13], body[14]};
                            diag_log::writef("position lock: residual %.2f m %s",
                                             rig.residualAcross(frame.head, feetNow),
                                             hybridOwner.enterAnim ? "at VR->anim" : "1 s into VR");
                            positionLockCheckNs = -1;
                        }
                        hybridZipPress = false;
                        hybridJumpAsk = false;
                        hybridWebWas = webNow;
                        // HYBRID: yield (and so head follow) changes only on an owner
                        // edge, which always runs the blend above.
                        const bool rawYield =
                            hybridOn ? hybridOwner.animOwns(values.animatedMoves, animState)
                                     : animated_moves::yieldsBody(animGate, animState, webAimIntent,
                                                                  values.animatedArms);
                        yieldAnim = animOnly ? animatedOnlyLatch.update(rawYield, animatedMovesHandoff.yielding,
                                                                        handoffNs)
                                             : rawYield;
                        const bool onlyHeld = animOnly && animatedOnlyLatch.held;
                        if (!animOnly) {
                            animatedOnlyLatch = {};
                            animatedOnlyStateWas = -1;
                        }
                        if (onlyHeld != animatedOnlyHeldWas) {
                            if (onlyHeld)
                                diag_log::writef("animated only: kept yield through %s (grace %.1f s, no reclaim)",
                                                 animated_moves::yieldName(animState),
                                                 static_cast<double>(animated_moves::stayGraceNs) / 1e9);
                            else
                                diag_log::writef("animated only: grace ended (%s, yield %s)",
                                                 animated_moves::yieldName(animState),
                                                 yieldAnim ? "kept" : "released after grace");
                            animatedOnlyHeldWas = onlyHeld;
                        }
                        if (animOnly && yieldAnim) {
                            const int st = static_cast<int>(head_probe::classify(animState));
                            if (st != animatedOnlyStateWas && animatedOnlyStateWas >= 0)
                                diag_log::writef(
                                    "animated only: %s -> %s, game anim keeps body (no reclaim, no adoptFeet)",
                                    head_probe::stateName(static_cast<head_probe::State>(animatedOnlyStateWas)),
                                    animated_moves::yieldName(animState));
                            animatedOnlyStateWas = st;
                        }
                        // Hybrid: no midair softlock — the user's HEAD setting (Full) in
                        // the air too, while the animation owns.
                        animHeadMode = hybridOn ? animated_moves::headFollow(values.animatedHead)
                                                : animated_moves::headFollowForYield(values.animatedHead,
                                                                                     animState);
                        // In the grace window keep following the anim head through a
                        // stale sample, but not over a menu / the game's camera.
                        const bool headFollow =
                            animated_moves::followsHead(values.animatedHead, yieldAnim, animState) &&
                            !flatScreen && !(onlyHeld && !animState.playing);
                        animHeadFollowNow = headFollow;
                        // Always character-held camera while HEAD ≠ Off (ground
                        // headbone or midair softlock proxy); shrinkHead stays on.
                        game_fp::followHero(headFollow);
                        const Vec3 nowFeet = finite(Vec3{body[12], body[13], body[14]})
                                                 ? Vec3{body[12], body[13], body[14]}
                                                 : motion.anchor;
                        animatedMovesHandoff.update(yieldAnim);
                        if (animatedMovesHandoff.exit) {
                            // Hybrid G→T: keep overlay state alive for H so arms
                            // crossfade into bodyOn instead of popping off.
                            if (!(hybridOn && hybridBlend.active && hybridBlend.toTraversal &&
                                  hybridBlend.delayArmsClear))
                                animatedArmsState = {};
                        }
                        const bool cancelledPending = animatedMovesHandoff.enter && animatedMovesHandoff.pendingExit;
                        if (animatedMovesHandoff.enter) {
                            animatedMovesYieldFeet = nowFeet;
                            // Hybrid T→G: do not wipe IK — beginReclaim already
                            // warmed the opposite edge; a quick re-jump stays warm.
                            if (!cancelledPending && !(hybridOn && hybridBlend.active && !hybridBlend.toTraversal))
                                native_body::resyncAfterYield();
                            diag_log::writef(
                                "animated moves: game anim drives body (%s) actor (%.1f, %.1f, %.1f)%s",
                                animated_moves::yieldName(animState), nowFeet.x, nowFeet.y, nowFeet.z,
                                cancelledPending ? " (brief interrupt)" : "");
                            animatedMovesDiagAt = frame.predictedDisplayTime;
                        }
                        const bool stableExit =
                            animatedMovesHandoff.consumeStableExit(handoffNs, nowFeet);
                        if (stableExit) {
                            const Vec3 feet =
                                finite(nowFeet) ? nowFeet : animatedMovesHandoff.pendingExitFeet;
                            const Vec3 delta = feet - animatedMovesYieldFeet;
                            const float moved = finite(delta) ? length(delta) : -1.f;
                            if (hybridOn) {
                                // Slices 2–3 already ran adoptEyes + beginReclaim on
                                // enterVr. Skipping adoptFeet / state={} keeps the
                                // warm handoff; origin already tracked feet in yield.
                                diag_log::writef(
                                    "animated moves: mod body again (%s) actor (%.1f, %.1f, %.1f) "
                                    "moved %.2f m since yield, head-follow was %s, hybrid handoff",
                                    animated_moves::yieldName(animState), feet.x, feet.y, feet.z, moved,
                                    animatedHeadFollowWas ? animated_moves::headFollowName(animHeadMode)
                                                          : "off");
                            } else {
                                native_body::resyncAfterYield();
                                if (finite(feet) && validTrackedPose(frame.head))
                                    rig.adoptFeet(feet, frame.head);
                                diag_log::writef(
                                    "animated moves: mod body again (%s) actor (%.1f, %.1f, %.1f) "
                                    "moved %.2f m since yield, head-follow was %s, adopt+ik resync",
                                    animated_moves::yieldName(animState), feet.x, feet.y, feet.z, moved,
                                    animatedHeadFollowWas ? animated_moves::headFollowName(animHeadMode)
                                                          : "off");
                            }
                            animatedMovesDiagAt = frame.predictedDisplayTime;
                        } else if (!animatedMovesHandoff.enter && yieldAnim &&
                                   frame.predictedDisplayTime - animatedMovesDiagAt > 2'000'000'000) {
                            diag_log::writef("animated moves: still yielding (%s)",
                                             animated_moves::yieldName(animState));
                            animatedMovesDiagAt = frame.predictedDisplayTime;
                        }
                        const bool softlockNow =
                            headFollow && animHeadMode == animated_moves::HeadFollow::softlock;
                        if (softlockNow && !animatedSoftlockWas) {
                            game_fp::relatchHeadFrame();
                            diag_log::writef(
                                "view softlock: midair chest|headbone (%s) offHeadPitch — dYaw —",
                                animated_moves::yieldName(animState));
                        }
                        if (!softlockNow && animatedSoftlockWas) {
                            game_fp::relatchHeadFrame(); // settle back onto ground headbone frame
                        }
                        animatedSoftlockWas = softlockNow;
                        // Head seat (head_seat.hpp): every switch while the view is on the hero's
                        // head puts it back on the neck, so the room's steps and leans since the
                        // lock began cannot pile up into a head off its neck. An owner edge or a
                        // recenter starts the lock afresh; the others glide back onto the head.
                        if (headFollow && hybridOn) {
                            const int stateNow = static_cast<int>(head_probe::classify(animState));
                            uint32_t actionNow = 0;
                            actionNow |= animState.fighting ? 1u : 0u;
                            actionNow |= animState.dodging ? 2u : 0u;
                            actionNow |= animState.zipping ? 4u : 0u;
                            actionNow |= (static_cast<uint32_t>(animState.wall) & 7u) << 3;
                            actionNow |= phone.open() ? (1u << 6) : 0u;
                            actionNow |= gadgetWheel.open() ? (1u << 7) : 0u;
                            actionNow |= padLayer.latched() ? (1u << 8) : 0u;
                            actionNow |= (namedPress.claimed() & 0x3fu) << 9;
                            actionNow |= (static_cast<uint32_t>(listed.equipped) & 0xffu) << 16;
                            if (hybridOwner.enterAnim || hybridOwner.enterVr)
                                seatWhy = head_seat::Why::owner;
                            else if (frame.recentered)
                                seatWhy = head_seat::Why::recenter;
                            else if (hybridFrame.zipPress)
                                seatWhy = head_seat::Why::zip;
                            else if (animatedHeadFollowWas &&
                                     static_cast<int>(animHeadMode) != animatedHeadModeWas)
                                seatWhy = head_seat::Why::mode;
                            else if (animSeatStateWas >= 0 && stateNow != animSeatStateWas)
                                seatWhy = head_seat::Why::state;
                            else if (animSeatActionWas != 0xffffffffu && actionNow != animSeatActionWas)
                                seatWhy = head_seat::Why::action;
                            animSeatStateWas = stateNow;
                            animSeatActionWas = actionNow;
                        } else {
                            animSeatStateWas = -1;
                            animSeatActionWas = 0xffffffffu;
                        }
                        const int modeNow = headFollow ? static_cast<int>(animHeadMode) : -1;
                        if (headFollow != animatedHeadFollowWas || modeNow != animatedHeadModeWas) {
                            // softlock|full|level|off — never silent "level" for Full midair.
                            diag_log::writef(
                                "animated moves: head follow %s (%s)",
                                headFollow ? animated_moves::headFollowName(animHeadMode) : "off",
                                animated_moves::yieldName(animState));
                            animatedHeadFollowWas = headFollow;
                            animatedHeadModeWas = modeNow;
                            if (!headFollow) {
                                animHeadRefSet = false;
                                animHeadTurn = 0;
                                animHeadFlick = false;
                            }
                        }
                        // ANIMATED ARMS: per-arm overlay while yielding. Full bodyOn stays
                        // off so the game keeps torso/legs/head. Motion vs actor root;
                        // web/aim intent interrupts immediately.
                        animated_arms::Sample armSamples[2]{};
                        const float armDt =
                            std::isfinite(frame.seconds) && frame.seconds > 0 ? frame.seconds : 1.f / 90;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            const bool tracked = hand.valid && validTrackedPose(hand.grip);
                            armSamples[i].grip = motion.grips[i].position;
                            armSamples[i].tracked = tracked;
                            armSamples[i].fist = hand.squeeze;
                            armSamples[i].shoot = 0.f; // filled after fingers below; intent uses web too
                            armSamples[i].webButton = motion.swing.hands[i].grip > .5f ||
                                                      motion.swing.hands[i].trigger > .5f;
                        }
                        const bool hybridArmFade =
                            hybridOn && hybridBlend.active && hybridBlend.toTraversal &&
                            hybridBlend.delayArmsClear &&
                            !hybrid_handoff::blendDone(handoffNs - hybridBlend.startNs);
                        const bool armsWanted =
                            values.animatedArms && yieldAnim && !flatScreen && values.body;
                        const bool armsOverlayLive =
                            armsWanted || (hybridArmFade && animated_arms::anyActive(animatedArmsState));
                        pose.flags =
                            (flatScreen || !values.body || yieldAnim ? 0u : native_body::bodyOn) |
                            native_body::hideHead | native_body::handTurn |
                            ((swingState.owned || hybridLeapAir) && !motion.standing ? native_body::airborne
                                                                                     : 0u) |
                            (calibratedEye > 0 || values.heightCm ? native_body::calibrated : 0u) |
                            (motion.standing || crawlsFloor ? native_body::standing : 0u) |
                            (kneels ? native_body::kneel : 0u);
                        pose.standUp = motion.standUp;
                        pose.floor = motion.floor;
                        // A dive: the legs kick out at him (body::Targets::kick).
                        if (kickUntil > frame.predictedDisplayTime && finite(diveAt)) {
                            pose.flags |= native_body::kick;
                            pose.kick = diveAt - motion.anchor;
                        }
                        // PROCEDURAL LEGS ONLY IN THE AIR (the user, November 10 21:03:
                        // "procedural legs are only in midair or during swinging"): the
                        // trail and the tuck need the body truly off everything - a
                        // swing or fall the swing owns with no ground, no wall of its
                        // own (wall run, wall hold), no mount, no landing carry, or a
                        // hybrid leap - never the ground, a wall run, a wall or vent
                        // crawl, parkour or a landing. body_ik blends them out
                        // (airLegBlendSeconds) on landing or wall contact.
                        const bool legsAirOnly =
                            (pose.flags & native_body::airborne) && !(pose.flags & native_body::kick) &&
                            !motion.standing && !motion.ventCrawl && !crawlsFloor && !kneels &&
                            (hybridLeapAir ||
                             (swingState.owned && !swingState.grounded && !swingState.mount &&
                              swingState.wall == static_cast<uint32_t>(game_swing::Surface::none)));
                        // Slice L: trail air legs from swing / leap velocity. No Command bump.
                        if (legsAirOnly && finite(swingState.velocity) && length(swingState.velocity) > .3f)
                            native_body::setAirMomentum(swingState.velocity);
                        else
                            native_body::setAirMomentum({});
                        // LEG CONTROL (ADVANCED; leg_control.hpp; the user, November 10:
                        // "procedural leg control on the right stick, but only when
                        // Controlled Flips is on"): only where the legs may move in the
                        // air (legsAirOnly). The legs swing with the body's specific
                        // force and turn, and their swing turns the body back
                        // (rig.legCoupling, applied by the flip on the next update).
                        {
                            // November 11 23:35: with REALISTIC FLIPS too (the rig feeds the
                            // legs' turn and inertia into RealisticFlip's momentum model).
                            // LEG TOGGLE: toggled on, the legs own the right stick here;
                            // just toggled off they spring back to straight (critically
                            // damped, stick centred, Toggle::easeSeconds) before body_ik
                            // blends them out, and the stick is already the turn's.
                            const bool legBase = values.legControl &&
                                                 (values.controlledFlips || values.realisticFlips) &&
                                                 values.body && !flatScreen &&
                                                 legsAirOnly && !calibrationInput;
                            const bool legEasing = legBase && !legHoldOut.engaged && legHoldOut.easing;
                            legStickOwns = legBase && legHoldOut.engaged;
                            legBaseLast = legBase;
                            // The legs' layers (leg_layers.hpp): PASSIVE LEGS needs no LEG
                            // CONTROL (nor a flip mode to be seen; its turn on the body is
                            // the flips' to apply). One spring for every layer.
                            const bool legAir = values.body && !flatScreen && legsAirOnly && !calibrationInput;
                            legsAirLast = legAir;
                            const float legDt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            const Quat body = Quat::yaw(motion.swing.trackingYaw) * motion.swing.tilt;
                            Vec3 look = frame.head.orientation.rotate({0, 0, -1});
                            look.y = 0;
                            const Vec3 levelLook = length(look) > .1f ? normalized(look) : Vec3{0, 0, -1};
                            const Vec3 fwd = body.rotate(levelLook), upB = body.rotate({0, 1, 0});
                            const Vec3 leftB = cross(upB, fwd);
                            Vec3 bodySpin{};
                            if (legAir && legPrevBodySet && legDt > 1e-4f) {
                                Quat d = body * legPrevBody.conjugate();
                                if (d.w < 0)
                                    d = {-d.x, -d.y, -d.z, -d.w};
                                bodySpin = Vec3{d.x, d.y, d.z} * (2 / legDt);
                            }
                            // REACH SPREAD: each tracked hand against the player's pelvis, in
                            // the level frame he faces (tracking space: his own body).
                            {
                                const Vec3 pelvis = frame.head.position -
                                                    Vec3{0, leg_layers::pelvisDrop(seatStanding), 0};
                                const Vec3 leftT = cross(Vec3{0, 1, 0}, levelLook);
                                for (int i = 0; i < 2; ++i) {
                                    const bool tracked = frame.hands[i].valid && validTrackedPose(frame.hands[i].grip) &&
                                                         validTrackedPose(frame.head);
                                    const Vec3 d = frame.hands[i].grip.position - pelvis;
                                    reachHand[i] = legAir && tracked &&
                                                   leg_layers::handBetween({dot(d, levelLook), dot(d, leftT), d.y},
                                                                           reachHand[i]);
                                }
                            }
                            leg_layers::Input layerIn;
                            layerIn.dt = legDt;
                            layerIn.air = legAir;
                            layerIn.active = legBase && legHoldOut.engaged;
                            layerIn.easing = legEasing;
                            layerIn.stickX = legStickX;
                            layerIn.stickY = legStickY;
                            layerIn.flail = flailNow && legAir;
                            layerIn.crouch.upDot = upB.y;
                            layerIn.crouch.vy = finite(swingState.velocity) ? swingState.velocity.y : 0.f;
                            layerIn.crouch.spin = length(bodySpin);
                            layerIn.crouch.webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                            layerIn.handBetween[0] = reachHand[0];
                            layerIn.handBetween[1] = reachHand[1];
                            leg_layers::Settings layerSet;
                            layerSet.passivePct = values.passiveLegsPct;
                            layerSet.passiveCouplingPct = values.passiveCouplingPct;
                            layerSet.crouch = values.legAirCrouch;
                            layerSet.reach = values.legReachSpread;
                            const auto layered = legMixer.update(layerIn, layerSet);
                            const bool legMode = layered.run;
                            const Vec3 vel = swingState.velocity;
                            if (legMode && finite(vel) && legPrevVelocitySet && legDt > 1e-4f) {
                                const Vec3 raw = limited((vel - legPrevVelocity) / legDt, 60.f);
                                legAccel = legAccel + (raw - legAccel) * (1 - std::exp(-legDt / .06f));
                            } else if (!legMode) {
                                legAccel = {};
                            }
                            legPrevVelocity = finite(vel) ? vel : Vec3{};
                            legPrevVelocitySet = legMode && finite(vel);
                            leg_control::Motion legMotion;
                            if (legMode && legPrevVelocitySet) {
                                const Vec3 force = legAccel + Vec3{0, 9.81f, 0};
                                legMotion.specificForce = {dot(force, fwd), dot(force, leftB), dot(force, upB)};
                            }
                            if (legMode && legPrevBodySet && legDt > 1e-4f)
                                legMotion.angularVelocity = {dot(bodySpin, fwd), dot(bodySpin, leftB), dot(bodySpin, upB)};
                            legPrevBody = body;
                            legPrevBodySet = legAir;
                            auto params = leg_control::fromPercent(values.legStiffnessPct, values.legDampingPct,
                                                                   values.legCouplingPct);
                            params.coupling *= layered.couplingScale;
                            if (layered.easing) {
                                params.frequencyHz = std::max(params.frequencyHz, 2.5f);
                                params.dampingRatio = 1.f;
                                params.looseness = 0.f;
                            }
                            const auto legOut =
                                layered.useStick
                                    ? legSim.update(legDt, legMode, layered.easing ? 0.f : legStickX,
                                                    layered.easing ? 0.f : legStickY, legMotion, params)
                                    : legSim.updateTarget(legDt, legMode, layered.pose, legStickX, legStickY, legMotion,
                                                          params);
                            std::array<body::Targets::LegAngles, 2> angles{};
                            for (int i = 0; i < 2; ++i)
                                angles[i] = {legOut.legs[i].pitch, legOut.legs[i].side, legOut.legs[i].knee};
                            native_body::setLegControl(legMode, angles, layered.amount);
                            // The legs' turn on the body: the flips' momentum models take it
                            // (CONTROLLED: FlipMotion; REALISTIC: RealisticFlip).
                            if (legMode && (values.controlledFlips || values.realisticFlips))
                                rig.legCoupling(legOut.reactionPitch, legOut.reactionRoll, legOut.spinScale);
                            if (layered.layerChanged) {
                                legModeLogged = legMode;
                                diag_log::writef("legs: layer %s (shown %.0f%%, coupling %.0f%% of LEG COUPLING)",
                                                 leg_layers::layerName(layered.layer), layered.amount * 100.f,
                                                 layered.couplingScale * 100.f);
                            }
                            if (layered.crouchChanged)
                                diag_log::writef("legs: air crouch %s (up %.0f deg, air %.2f s, vy %.1f m/s, spin %.1f rad/s%s)",
                                                 layered.crouchOn ? "on" : "off",
                                                 std::acos(std::clamp(upB.y, -1.f, 1.f)) * (180.f / 3.14159265f),
                                                 legMixer.airSeconds(), layerIn.crouch.vy, layerIn.crouch.spin,
                                                 layerIn.crouch.webbed ? ", webbed" : "");
                            if (layered.reachChanged)
                                diag_log::writef("legs: reach spread %s (%s)", layered.reachOn ? "on" : "off",
                                                 reachHand[0] && reachHand[1] ? "both hands"
                                                 : reachHand[0]               ? "left hand"
                                                 : reachHand[1]               ? "right hand"
                                                                              : "no hand");
                        }
                        // Slice T: hold B (dodge) to tuck. Triggers and grips stay
                        // web. Not while the pad layer is latched, or during a kick.
                        // Isolated here so a later remap can point at another button.
                        native_body::setTuck(legsAirOnly &&
                                             (frame.buttons & buttonB) && !calibrationInput &&
                                             !padLayer.latched());
                        const bool wallGrip = game_swing::outdoorClimbGrip(
                            climbState, frame.predictedDisplayTime > 0 ? frame.predictedDisplayTime : 0);
                        const bool runningPose = wallRunBody.update(motion, hold, swingState.velocity,
                            swingFresh && values.handClimb && values.body && !flatScreen &&
                            !(pose.flags & native_body::kick), swingState.grounded != 0, wallGrip ? climbState.hands : 0, frame.seconds,
                            vr_settings::fixedMomentumStepPace * .01f);
                        if (motion.ventCrawl && values.handClimb && values.body && !flatScreen) {
                            pose.flags |= native_body::ventHands |
                                          native_body::crawlTouchFlags(motion.wallTouches);
                            const auto sampleTime = frame.predictedDisplayTime > 0 ?
                                static_cast<uint64_t>(frame.predictedDisplayTime) : 0;
                            const bool fresh = climbState.status && climbState.crawl == 1 && sampleTime &&
                                climbState.sampleTimeNs && sampleTime >= climbState.sampleTimeNs &&
                                sampleTime - climbState.sampleTimeNs <= 100'000'000;
                            pose.flags |= native_body::crawlHandFlags(fresh ? climbState.hands : 0);
                            pose.crawlNormal = hold.normal;
                            pose.floor = hold.anchor - motion.anchor;
                        }
                        if (wallCrawlBody.update(motion, hold,
                            values.handClimb && values.body && !flatScreen && !runningPose,
                            frame.seconds, wallGrip ? climbState.hands : 0)) {
                            pose.flags |= native_body::crawl | native_body::crawlPlanted |
                                          native_body::crawlHandSync;
                            pose.flags |= native_body::crawlHandFlags(wallGrip ? climbState.hands : 0);
                            pose.flags |= native_body::crawlTouchFlags(motion.wallTouches) |
                                          native_body::crawlTurnAway;
                            pose.floor = wallCrawlBody.floor();
                            pose.crawlNormal = wallCrawlBody.normal();
                            pose.crawlAhead = wallCrawlBody.ahead();
                            pose.crawlStep = wallCrawlBody.step();
                            pose.crawlPhase = wallCrawlBody.phase();
                            pose.crawlMotion = wallCrawlBody.motion();
                            pose.crawlOrigin = motion.anchor;
                            pose.crawlStrokes[0] = wallCrawlBody.handSteps()[0];
                            pose.crawlStrokes[1] = wallCrawlBody.handSteps()[1];
                            pose.crawlDistance = static_cast<float>(vr_settings::fixedWallDistance) / 100.f;
                        }
                        if (runningPose) {
                            pose.flags &= ~(native_body::standing | native_body::kneel);
                            pose.flags |= native_body::wallRun | native_body::airborne;
                            pose.floor = wallRunBody.floor();
                            pose.crawlNormal = wallRunBody.normal();
                            pose.crawlAhead = wallRunBody.ahead();
                            pose.crawlPhase = wallRunBody.phase();
                            pose.crawlMotion = wallRunBody.speed();
                            pose.crawlDistance = vr_settings::fixedRunTorsoAngle * .0174532925f;
                            pose.runBank = std::tan(vr_settings::fixedMomentumBodyTilt * .0174532925f);
                            pose.runLean = vr_settings::fixedMomentumForwardLean * .0174532925f;
                            pose.runStride = vr_settings::fixedMomentumStepLength * .01f;
                        }
                        pose.eyes = Vec3{(motion.eyes[0][12] + motion.eyes[1][12]) / 2,
                                         (motion.eyes[0][13] + motion.eyes[1][13]) / 2,
                                         (motion.eyes[0][14] + motion.eyes[1][14]) / 2} -
                                    motion.anchor;
                        pose.facing = motion.headPose.orientation;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            const bool tracked = hand.valid && validTrackedPose(hand.grip);
                            const Vec3 relative = hand.grip.position - frame.head.position;
                            const float seconds =
                                std::isfinite(frame.seconds) && frame.seconds > 0 ? frame.seconds : 1.f / 90;
                            const float speed =
                                tracked && gripsSeen[i] ? length(relative - gripsBefore[i]) / seconds : 0.f;
                            gripsBefore[i] = relative;
                            gripsSeen[i] = tracked;
                            HandPoseInput doing;
                            doing.tracked = tracked;
                            doing.wallContact = motion.active && !flatScreen && values.handClimb &&
                                ((motion.wallTouches & (1u << i)) ||
                                 (climbState.status && (climbState.hands & (1u << i))));
                            doing.web = motion.swing.hands[i].grip;
                            doing.squeeze = hand.squeeze;
                            doing.speed = speed;
                            doing.gripping = onStrandBefore[i] || webTaken[i];
                            doing.holding = attachedBefore[i];
                            doing.shot = ballShot[i];
                            doing.fighting = fightNear;
                            doing.airFists = values.airFists;
                            ballShot[i] = false;
                            const auto fingered = fingers[i].update(seconds, doing);
                            pose.hands[i] = {motion.grips[i].position - motion.anchor, motion.grips[i].orientation,
                                             tracked ? 1u : 0u, fingered.fist};
                            pose.shoot[i] = fingered.shoot;
                            armSamples[i].fist = fingered.fist;
                            armSamples[i].shoot = fingered.shoot;
                        }
                        // Hybrid G→T: enabled=false ramps overlay 1→0 over ~blendOut;
                        // keep calling update + armsOverlay so the crossfade reaches bodyOn.
                        animated_arms::update(animatedArmsState, armDt, armsWanted, motion.anchor, armSamples,
                                              hybridOn && values.animatedHead != 0);
                        if (armsWanted) {
                            // Flag while yielding so native_body can blend arm
                            // weights out smoothly when idle; soft floor keeps
                            // tracked arms off game-anim-only bones (Slice C).
                            pose.flags |= native_body::armsOverlay;
                            native_body::setArmOverlay(animatedArmsState.arms[0].weight,
                                                       animatedArmsState.arms[1].weight);
                            native_body::setTorsoAssist(values.animatedTorso);
                        } else if (armsOverlayLive) {
                            pose.flags |= native_body::armsOverlay;
                            // Force target toward 0 while fading into body IK.
                            native_body::setArmOverlay(animatedArmsState.arms[0].weight,
                                                       animatedArmsState.arms[1].weight);
                            native_body::setTorsoAssist(false);
                        } else {
                            native_body::setArmOverlay(0.f, 0.f);
                            native_body::setTorsoAssist(false);
                        }
                        // Softlock / headbone view frame → remapGripToChest player eyes.
                        native_body::setViewSoftlock(animHeadFollowNow && values.animatedArms);
                        // Dense tracking-lock diag (~10 Hz) while ARMS yield:
                        // overlay weights, forearm vs controller, actor/chest
                        // yaw delta, HMD vs anim head. Full never silent Level.
                        if (armsWanted &&
                            frame.predictedDisplayTime - trackingLockDiagAt > 100'000'000) {
                            trackingLockDiagAt = frame.predictedDisplayTime;
                            const auto diagnosticFrame = native_body::actionFrame();
                            const auto& ot = diagnosticFrame.tracking;
                            const auto& arms = diagnosticFrame.forearms;
                            const auto hs = native_body::headSample();
                            float offL = -1.f, offR = -1.f;
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& f = arms.arms[i];
                                const bool mapped = ot.targetFrame != animated_view::Frame::none;
                                if (mapped ? !ot.valid[i] : (!f.valid || !frame.hands[i].valid))
                                    continue;
                                const Vec3 forearm = mapped ? normalized(ot.solvedWrist[i] - ot.elbow[i])
                                                            : normalized(f.wrist - f.elbow);
                                const Vec3 aim = (mapped ? ot.aim[i] : motion.swing.hands[i].aim.orientation)
                                                     .rotate({0, 0, -1});
                                if (length(forearm) < .2f || length(aim) < .2f)
                                    continue;
                                const float off =
                                    std::acos(std::clamp(dot(normalized(forearm), normalized(aim)), -1.f,
                                                         1.f)) *
                                    57.29578f;
                                (i == 0 ? offL : offR) = off;
                            }
                            const Vec3 actorAhead{body[8], body[9], body[10]};
                            const bool actorOk = finite(actorAhead) && length(Vec3{actorAhead.x, 0, actorAhead.z}) > .2f;
                            const float actorYaw =
                                actorOk ? animated_arms::lookYaw(actorAhead) * 57.29578f : -1.f;
                            const float chestYaw = ot.chestYaw * 57.29578f;
                            const float playerYaw = ot.playerYaw * 57.29578f;
                            float hmdY = 0, hmdP = 0, hmdR = 0;
                            if (validTrackedPose(frame.head)) {
                                const Vec3 look = frame.head.orientation.rotate({0, 0, -1});
                                const Vec3 up = frame.head.orientation.rotate({0, 1, 0});
                                hmdY = animated_arms::lookYaw(look) * 57.29578f;
                                hmdP = std::asin(std::clamp(look.y, -1.f, 1.f)) * 57.29578f;
                                hmdR = std::atan2(up.x, up.y) * 57.29578f;
                            }
                            float animY = 0, animP = 0, animR = 0;
                            if (hs.job && hs.head >= 0 && finite(hs.gameForward) &&
                                length(hs.gameForward) > .2f) {
                                animY = animated_arms::lookYaw(hs.gameForward) * 57.29578f;
                                animP =
                                    std::asin(std::clamp(hs.gameForward.y, -1.f, 1.f)) * 57.29578f;
                                if (finite(hs.gameUp) && length(hs.gameUp) > .2f)
                                    animR = std::atan2(hs.gameUp.x, hs.gameUp.y) * 57.29578f;
                            }
                            diag_log::writef(
                                "tracking lock: Lw %.2f Rw %.2f floor %.2f offL %.1f offR %.1f "
                                "actorYaw %.0f chestYaw %.0f playerYaw %.0f dYaw %.0f "
                                "hmd ypr %.0f/%.0f/%.0f anim ypr %.0f/%.0f/%.0f remap %u softlock %u head %s",
                                animatedArmsState.arms[0].weight, animatedArmsState.arms[1].weight,
                                animated_arms::yieldFloor, offL, offR, actorYaw, chestYaw, playerYaw,
                                chestYaw - playerYaw, hmdY, hmdP, hmdR, animY, animP, animR, ot.remapped,
                                ot.softlock, animated_moves::headFollowName(animHeadMode));
                            diag_log::writef("player torso: active %u serial %llu job %llu head residual %.3f m root drift %.2f deg actual hands %u/%u",
                                ot.playerTorso, static_cast<unsigned long long>(ot.serial),
                                static_cast<unsigned long long>(ot.job), ot.headError,
                                ot.rootDrift * 57.29578f, ot.actualValid[0], ot.actualValid[1]);
                            if (ot.targetFrame != animated_view::Frame::none) {
                                for (unsigned i = 0; i < 2; ++i) {
                                    diag_log::writef(
                                        "animated hand: owner anim serial %llu job %llu frame %u %c valid %u "
                                        "overlay %.2f solver %.2f clamp %u residual %.3f m %.1f deg",
                                        static_cast<unsigned long long>(ot.serial),
                                        static_cast<unsigned long long>(ot.job),
                                        static_cast<unsigned>(ot.targetFrame), i ? 'R' : 'L', ot.valid[i],
                                        ot.requestedWeight[i], ot.armWeight[i], ot.reachClamped[i],
                                        ot.residual[i], ot.angle[i]);
                                    diag_log::writef(
                                        "animated hand points: serial %llu job %llu %c grip %.3f/%.3f/%.3f "
                                        "target %.3f/%.3f/%.3f actual %.3f/%.3f/%.3f (feet-relative world "
                                        "axes)",
                                        static_cast<unsigned long long>(ot.serial),
                                        static_cast<unsigned long long>(ot.job), i ? 'R' : 'L', ot.grip[i].x,
                                        ot.grip[i].y, ot.grip[i].z, ot.wrist[i].x, ot.wrist[i].y,
                                        ot.wrist[i].z, ot.solvedWrist[i].x, ot.solvedWrist[i].y,
                                        ot.solvedWrist[i].z);
                                }
                            }
                        }
                        // The player's size: HEIGHT when set, else the T-pose
                        // calibration's, else the headset's running height.
                        pose.height = values.heightCm ? seated::eyeHeight(values.heightCm)
                                      : calibratedEye > 0 ? calibratedEye
                                                          : standingEye;
                        pose.armLength = calibratedArm;
                        pose.trackingYaw = motion.swing.trackingYaw;
                        // A flip's tilt in world axes: the body turns with the player.
                        // Standing on a surface, the body already stands along it:
                        // only a flip turns its planted pose further (none on a wall).
                        // JOYSTICK WALL RUN CAMERA on (the default): the running body
                        // follows the wall-standing view about its eyes, residual
                        // surface rotation too. Off, joystick and momentum runs both
                        // take the travel-driven pose.
                        const Quat bodyTilt = runningPose && values.joystickRunCamera
                                                  ? (motion.viewTilt > .0001f ? motion.swing.tilt : Quat{})
                                                  : motion.bodyTilt(runningPose);
                        if (const Quat tilt = bodyTilt;
                            tilt.x != 0 || tilt.y != 0 || tilt.z != 0) {
                            const Quat yaw = Quat::yaw(motion.swing.trackingYaw);
                            pose.tilt = yaw * tilt * yaw.conjugate();
                        }
                        // ANIMATED AIR SPIN (off by default; animated_air_spin.hpp):
                        // the flip tilt (+ momentum lean) turns the yielded anim
                        // skeleton about its head — SpidyVR-style spherical midair on
                        // the game's pose. Visual/view only; physics untouched.
                        if ((values.animatedAirSpin && !hybridOn) || airSpinWas) {
                            const auto spinMode = animated_air_spin::mode(hybridOn ? 0 : values.animatedAirSpin);
                            const bool spinActive =
                                !hybridOn &&
                                animated_air_spin::active(values.animatedAirSpin, yieldAnim, animState) &&
                                !flatScreen && values.body;
                            const Quat flipWorld =
                                animated_air_spin::worldFlip(motion.bodyTilt(false), motion.swing.trackingYaw);
                            animated_air_spin::Inputs spinIn;
                            spinIn.active = spinActive;
                            spinIn.momentum = spinMode == animated_air_spin::Mode::momentum;
                            spinIn.flipWorld = flipWorld;
                            spinIn.velocity = swingState.velocity;
                            spinIn.velocityFresh = swingFresh;
                            spinIn.actorForward = Vec3{body[8], body[9], body[10]};
                            spinIn.dt = armDt;
                            if (!yieldAnim)
                                airSpin.reset();
                            const auto spun = airSpin.update(spinIn);
                            native_body::setYieldTilt(yieldAnim ? spun.body : Quat{},
                                                      yieldAnim ? flipWorld : Quat{});
                            airSpinWas = values.animatedAirSpin != 0 || !airSpin.resting();
                            if ((spinActive || spun.bodyDeg > .5f) &&
                                frame.predictedDisplayTime - airSpinDiagAt > 250'000'000) {
                                airSpinDiagAt = frame.predictedDisplayTime;
                                diag_log::writef(
                                    "air spin: %s %s (%s) flip %.0f lean %.0f (pitch %.0f bank %.0f) body %.0f deg "
                                    "v %.1f m/s head %s",
                                    animated_air_spin::modeName(spinMode), spinActive ? "on" : "settling",
                                    animated_moves::yieldName(animState), spun.flipDeg, spun.leanDeg,
                                    spun.pitchDeg, spun.bankDeg, spun.bodyDeg, length(swingState.velocity),
                                    animated_moves::headFollowName(animHeadMode));
                            }
                        }
                        // Submitted together with the finalized eye-frame mapping below.
                    } else {
                        game_fp::followHero(false);
                        if (animatedHeadFollowWas) {
                            diag_log::writef("animated moves: head follow off (no body)");
                            animatedHeadFollowWas = false;
                            animHeadRefSet = false;
                            animHeadTurn = 0;
                            animHeadFlick = false;
                        }
                    }
                    // Keep publishing tracking while the engine renders earlier
                    // commands. The presentation loop no longer waits for this pose.
                    // ANIMATED HEAD while yielding: eyes relative to the hero's gameplay
                    // head (anchored 2 Full / 4 Level), resolved on the render thread
                    // like finishers (stereo_probe::resolveScene → game_fp::frame).
                    // animHeadMode is the user choice (Full may include air pitch/roll).
                    const bool headFollowEyes =
                        animHeadFollowNow && animHeadMode != animated_moves::HeadFollow::off && !flatScreen;
                    const int64_t eyeBlendNow =
                        frame.predictedDisplayTime > 0
                            ? frame.predictedDisplayTime
                            : static_cast<int64_t>(GetTickCount64()) * 1'000'000ll;
                    const int64_t eyeBlendElapsed =
                        hybridBlend.active ? (eyeBlendNow - hybridBlend.startNs)
                                           : hybrid_handoff::handoffNs;
                    const float eyeBlendW =
                        hybridBlend.active ? hybrid_handoff::blendAlpha(eyeBlendElapsed) : 1.f;
                    const bool hybridEyeBlend =
                        hybridBlend.active && hybridBlend.fromValid && eyeBlendW < 1.f - 1e-4f && !flatScreen;
                    // SMOOTHING filters the animated head the view sits on, not the
                    // headset. The render thread reads liveLevel in resolveScene.
                    // CAMERA SMOOTHING / STABILIZATION (headset_smoothing::fromValues):
                    // the same params on both threads, so hands and eyes share the head.
                    const headset_smoothing::Params cameraSmooth =
                        headFollowEyes ? headset_smoothing::fromValues(values) : headset_smoothing::Params{};
                    headset_smoothing::liveLevel.store(
                        headFollowEyes ? values.animatedHeadsetSmooth : 0, std::memory_order_relaxed);
                    headset_smoothing::publish(cameraSmooth);
                    // CAMERA MOTION (camera_motion.hpp): the amounts in force, on the
                    // render thread's eyes and on this thread's head and hands alike.
                    const camera_motion::Amounts cameraAmounts =
                        headFollowEyes ? camera_motion::fromValues(values) : camera_motion::Amounts{};
                    camera_motion::publish(cameraAmounts);
                    if (!headFollowEyes)
                        animHeadBob.reset(), cameraBobShift = {};
                    if (!headFollowEyes)
                        animHeadSmooth.reset();
                    native_eyes::Command eyes;
                    eyes.enabled = flatScreen ? 2 : 1;
                    eyes.serial = ++serial;
                    eyes.leaseMs = 250;
                    eyes.anchor = motion.anchor;
                    // Slice C: during a blend into the head lock, the head-locked eyes
                    // are built (reference latched at its first frame) as the blend's
                    // destination, so the blend ends on exactly the view that follows.
                    bool viewLockRelatch = false;
                    if (headFollowEyes) {
                        // An owner switch or a recenter: the lock starts afresh, on the neck.
                        if (seatWhy == head_seat::Why::owner || seatWhy == head_seat::Why::recenter)
                            animHeadRefSet = false;
                        if (!animHeadRefSet) {
                            viewLockRelatch = true; // the yaw lock starts afresh with it
                            animHeadRef = {frame.head.position, screenAhead(frame.head, 0).orientation};
                            animHeadRefSet = validTrackedPose(frame.head);
                            animHeadTurn = 0;
                            ++armEpoch; // a new reference: hand velocities start afresh
                            animHeadFlick = false;
                            animHeadSeat = {};
                            animSeatCount = 0;
                        } else if (validTrackedPose(frame.head)) {
                            // Any other switch, or farther off the head than a lean: glide the
                            // reference to the headset now (its heading, the player's look, kept).
                            if (seatWhy == head_seat::Why::none && values.animatedHybrid &&
                                values.animatedMoves &&
                                head_seat::overDrift(animHeadSeat, animHeadRef, frame.head.position))
                                seatWhy = head_seat::Why::drift;
                            float off = 0.f;
                            if (seatWhy != head_seat::Why::none &&
                                head_seat::reseat(animHeadSeat, animHeadRef, frame.head.position, eyeBlendNow,
                                                  &off)) {
                                ++animSeatCount;
                                ++armEpoch; // a reseat glides the frame, not the hands
                                if (eyeBlendNow - animSeatDiagAt > 250'000'000) {
                                    animSeatDiagAt = eyeBlendNow;
                                    diag_log::writef(
                                        "head seat: back on the neck (%s, %s) off %.2f m, glide %.0f ms, "
                                        "%u this lock",
                                        head_seat::whyName(seatWhy),
                                        animated_moves::headFollowName(animHeadMode), off,
                                        head_seat::glideNs / 1e6, animSeatCount);
                                }
                            }
                        }
                        // The reference this frame (a glide under way included): eyes and hands alike.
                        head_seat::step(animHeadSeat, animHeadRef, eyeBlendNow);
                        // Full (ground): anchored 2 follows anim head pitch/roll.
                        // Level (ground) / softlock (midair): anchored 4 level frame,
                        // free HMD. Softlock is deliberate midair policy — not a
                        // silent Full→Level settings rewrite (diag says softlock).
                        const bool followFull = animHeadMode == animated_moves::HeadFollow::full;
                        const bool followSoftlock =
                            animHeadMode == animated_moves::HeadFollow::softlock;
                        eyes.anchored = followFull ? 2u : 4u;
                        // Level only: right stick turns the view, as in cutscenes with
                        // head tracking off (softlock leaves R-stick to AIR SPIN /
                        // tracking yaw). The turn itself runs at the top of the next
                        // frame, before the hands are mapped (review M1).
                        levelTurnGate = !followFull && !followSoftlock;
                        // FULL head lock on the stable heading (view_yaw_lock.hpp, Step B):
                        // latched from the head bone with the head lock, kept through the
                        // midair softlock, published for the render thread's eyes, the hands'
                        // map and the arms' IK.
                        {
                            constexpr float toDeg = 180.f / 3.14159265f;
                            const auto moveMode = hybrid_movement::modeOf(values.movementMode);
                            const bool hybridLock = moveMode == hybrid_movement::Mode::hybrid;
                            // HYBRID: the same published lock carries the heading the view holds
                            // while the hero is brought onto it; at rest it is the head bone.
                            const bool lockWanted =
                                (hybridLock ||
                                 (moveMode == hybrid_movement::Mode::stable &&
                                  view_yaw_lock::lockOn(animated_moves::headFollow(values.animatedHead) ==
                                                            animated_moves::HeadFollow::full,
                                                        true, values.animYawSwayPct))) &&
                                (followFull || followSoftlock) && animHeadRefSet;
                            const float sway = hybridLock ? hybridHeading.swayWeight
                                                          : view_yaw_lock::swayOf(values.animYawSwayPct);
                            const float heroYawNow = std::hypot(body[8], body[10]) > .2f
                                                         ? hybrid_handoff::actorLookYaw({body[8], body[9], body[10]})
                                                         : NAN;
                            Mat4 rawHead{};
                            float boneYaw = 0;
                            const bool boneOk = lockWanted && game_fp::heroHead(rawHead) &&
                                                view_yaw_lock::nativeYaw(rawHead, boneYaw);
                            if (lockWanted) {
                                if ((!viewLock.set || viewLockRelatch) && boneOk && hybridLock) {
                                    // HYBRID: a facing hold (a landing) keeps its heading; else
                                    // the heading is his facing and the view the head bone.
                                    if (hybridHeading.hold == hybrid_movement::Heading::Hold::none ||
                                        !hybridHeading.set)
                                        hybridHeading.latch(std::isfinite(heroYawNow) ? heroYawNow : boneYaw);
                                    viewLock.set = true;
                                    viewLock.yaw = hybridHeading.yaw;
                                    ++viewLock.latches;
                                    diag_log::writef("view yaw lock: HYBRID latched at %.0f deg (head bone %.0f, hero "
                                                     "%.0f, hold %s), #%u",
                                                     viewLock.yaw * toDeg, boneYaw * toDeg,
                                                     std::isfinite(heroYawNow) ? heroYawNow * toDeg : -999.f,
                                                     hybrid_movement::holdName(hybridHeading.hold), viewLock.latches);
                                } else if ((!viewLock.set || viewLockRelatch) && boneOk) {
                                    viewLock.latch(boneYaw, heroYawNow, sway);
                                    turnInPlace.resetLook();
                                    diag_log::writef("view yaw lock: latched at %.0f deg (head bone %.0f, hero %.0f), "
                                                     "sway %d%%, turn-in-place %d deg, #%u",
                                                     viewLock.yaw * toDeg, boneYaw * toDeg,
                                                     std::isfinite(heroYawNow) ? heroYawNow * toDeg : -999.f,
                                                     values.animYawSwayPct, values.turnInPlaceDeg, viewLock.latches);
                                }
                            } else if (viewLock.set) {
                                diag_log::writef("view yaw lock: off (%s)",
                                                 moveMode == hybrid_movement::Mode::old ? "movement mode OLD"
                                                 : values.animYawSwayPct >= 100 && !hybridLock ? "sway 100%, old feel"
                                                                                                : "head lock not Full");
                                viewLock.reset();
                                if (!hybridLock)
                                    hybridHeading.reset();
                                turnInPlace.reset();
                            }
                            viewLockActive = lockWanted && viewLock.set;
                            fullLockGate = viewLockActive && followFull;
                            // FACING CHECK, the animation's side: the view (the lock + sway +
                            // headset, or the head bone + headset without the lock) and the
                            // hero's facing against what SpidyVR showed / the view before.
                            if (facingCheck.armed && hybridOwner.domain != hybrid_owner::Domain::vr) {
                                float checkBone = boneYaw;
                                bool checkBoneOk = boneOk;
                                if (!checkBoneOk) {
                                    Mat4 h{};
                                    checkBoneOk = game_fp::heroHead(h) && view_yaw_lock::nativeYaw(h, checkBone);
                                }
                                float hmdNow = 0;
                                if (validTrackedPose(frame.head))
                                    hmdNow = stable_yaw::wrap(
                                        hybrid_handoff::lookYaw(screenAhead(frame.head, 0).orientation.rotate({0, 0, -1})) -
                                        hybrid_handoff::lookYaw(animHeadRef.orientation.rotate({0, 0, -1})));
                                view_yaw_lock::State probeLock;
                                probeLock.on = viewLockActive;
                                probeLock.yaw = viewLock.yaw;
                                probeLock.sway = sway;
                                probeLock.heroYaw = heroYawNow;
                                facing_check::Sample fs;
                                fs.viewOk = checkBoneOk && (!lockWanted || viewLockActive);
                                fs.view = stable_yaw::wrap(view_yaw_lock::shownYaw(probeLock, checkBone) + hmdNow);
                                fs.viewFixable = viewLockActive;
                                fs.heroMatters = true;
                                fs.heroOk = std::isfinite(heroYawNow);
                                fs.hero = heroYawNow;
                                const auto fc = facingCheck.update(frame.seconds, fs);
                                if (fc.viewFix != 0.f) {
                                    viewLock.yaw = stable_yaw::wrap(viewLock.yaw + fc.viewFix);
                                    if (hybridLock)
                                        hybridHeading.begin(hybrid_movement::Heading::Hold::facing, viewLock.yaw);
                                }
                                if (fc.heroTurn && hybridLock) {
                                    // HYBRID: the heading holds the view on what was shown while
                                    // the walk stick brings him round (the forced turn).
                                    hybridHeading.begin(hybrid_movement::Heading::Hold::facing, fc.heroYaw);
                                    viewLock.yaw = hybridHeading.yaw;
                                    facingTurnEvent = facingCheck.event;
                                } else if (fc.heroTurn) {
                                    facingHeroPending = true;
                                    facingHeroYaw = fc.heroYaw;
                                    facingTurnEvent = facingCheck.event;
                                }
                                logFacing(facingCheck, fc);
                            }
                            view_yaw_lock::State lock;
                            lock.on = viewLockActive;
                            lock.yaw = viewLock.yaw;
                            lock.sway = sway;
                            lock.heroYaw = heroYawNow;
                            view_yaw_lock::publish(lock);
                            if (viewLockActive && boneOk) {
                                // Divergence: the view (lock + sway + headset) against the
                                // hero's facing and against the old view (head bone + headset).
                                float hmd = 0;
                                if (validTrackedPose(frame.head))
                                    hmd = stable_yaw::wrap(
                                        hybrid_handoff::lookYaw(screenAhead(frame.head, 0).orientation.rotate({0, 0, -1})) -
                                        hybrid_handoff::lookYaw(animHeadRef.orientation.rotate({0, 0, -1})));
                                const float shown = view_yaw_lock::shownYaw(lock, boneYaw);
                                const float viewYaw = stable_yaw::wrap(shown + hmd);
                                viewDivergence.add(frame.seconds, viewYaw - heroYawNow, shown - boneYaw);
                                const float rX = frame.hands[1].valid ? frame.hands[1].stickX : 0.f;
                                const auto watched =
                                    viewTurnWatch.update(frame.seconds, fullLockGate && viewLock.turning(rX, values.smoothTurn),
                                                         viewLockDelta, heroYawNow, viewLockSnapped);
                                if (watched.ended)
                                    diag_log::writef(
                                        "view turn: %s, %d ms, view %+.0f deg (%.0f deg/s, %u snaps), hero %+.0f deg, "
                                        "#%u",
                                        values.smoothTurn > 0 ? "smooth" : "snap",
                                        static_cast<int>(watched.seconds * 1000 + .5f), watched.viewDeg,
                                        watched.viewRate, watched.snaps, watched.heroDeg, ++viewLockTurnsLogged);
                                if (eyeBlendNow - viewYawDiagAt > 500'000'000) {
                                    viewYawDiagAt = eyeBlendNow;
                                    diag_log::writef(
                                        "view yaw: lock %.0f bone %.0f hero %.0f shown %.0f hmd %+.0f view %.0f "
                                        "view-hero %+.0f shown-bone %+.0f sway %d%% %s",
                                        viewLock.yaw * toDeg, boneYaw * toDeg, heroYawNow * toDeg, shown * toDeg,
                                        hmd * toDeg, viewYaw * toDeg, stable_yaw::wrap(viewYaw - heroYawNow) * toDeg,
                                        stable_yaw::wrap(shown - boneYaw) * toDeg, values.animYawSwayPct,
                                        followFull ? "full" : "softlock");
                                }
                                if (!viewDivergenceAt)
                                    viewDivergenceAt = eyeBlendNow;
                                if (eyeBlendNow - viewDivergenceAt > 10'000'000'000ll) {
                                    viewDivergenceAt = eyeBlendNow;
                                    const auto d = viewDivergence.summary();
                                    diag_log::writef(
                                        "view yaw divergence: %u samples, view-hero mean %.1f p95 %.0f max %.0f deg, "
                                        "over 45 %.1f%%; view-headbone mean %.1f p95 %.0f max %.0f deg; sway %d%%; "
                                        "turn in place %u (%u done, %u gave up, %u wrong way); mode %s, rear snaps %u, "
                                        "holds arrived %u gave up %u",
                                        d.samples, d.meanHero, d.p95Hero, d.maxHero, d.over45 * 100.f, d.meanBone,
                                        d.p95Bone, d.maxBone, values.animYawSwayPct, turnInPlace.starts,
                                        turnInPlace.done, turnInPlace.gaveUp, turnInPlace.wrongWays,
                                        hybrid_movement::modeName(moveMode), hybridGate.snaps, hybridHeading.arrived,
                                        hybridHeading.gaveUp);
                                    viewDivergence.reset();
                                }
                            }
                        }
                        const Mat4 axes = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                        for (unsigned i = 0; i < 2; ++i) {
                            Pose raw = frame.eyes[i].pose;
                            Pose eye = animated_view::relative(raw, animHeadRef, animHeadTurn);
                            eyes.eyes[i].world = native_view::relativePose(axes, eye);
                            const auto& f = frame.eyes[i].fov;
                            eyes.eyes[i].fov[0] = f.left;
                            eyes.eyes[i].fov[1] = f.right;
                            eyes.eyes[i].fov[2] = f.down;
                            eyes.eyes[i].fov[3] = f.up;
                        }
                    } else {
                        animHeadRefSet = false;
                        if (viewLock.set)
                            diag_log::writef("view yaw lock: off (head lock off)");
                        viewLock.reset();
                        turnInPlace.reset();
                        viewLockActive = fullLockGate = false;
                        view_yaw_lock::publish({});
                        eyes.anchored = 1;
                        for (unsigned i = 0; i < 2; ++i) {
                            eyes.eyes[i].world = motion.eyes[i];
                            const auto& f = motion.fovs[i];
                            eyes.eyes[i].fov[0] = f.left;
                            eyes.eyes[i].fov[1] = f.right;
                            eyes.eyes[i].fov[2] = f.down;
                            eyes.eyes[i].fov[3] = f.up;
                        }
                    }
                    Mat4 animHeadNow{};
                    bool haveAnimHead = false;
                    if (headFollowEyes && (eyes.anchored == 2 || eyes.anchored == 4)) {
                        if (eyes.anchored == 4 ? game_fp::frameLevel(animHeadNow)
                                               : game_fp::frame(animHeadNow)) {
                            // CAMERA MOTION first (how much), then CAMERA SMOOTHING (how
                            // smoothly), as the render thread does. The bob's shift is
                            // kept for the hands' anchor next frame.
                            const Vec3 rawAt{animHeadNow[12], animHeadNow[13], animHeadNow[14]};
                            animHeadNow = camera_motion::stabilize(
                                animHeadNow, cameraAmounts, &animHeadBob,
                                finite(motion.anchor) ? &motion.anchor : nullptr, frame.seconds);
                            cameraBobShift = Vec3{animHeadNow[12], animHeadNow[13], animHeadNow[14]} - rawAt;
                            if (!finite(cameraBobShift))
                                cameraBobShift = {};
                            animHeadNow = animHeadSmooth.update(
                                animHeadNow, frame.seconds, cameraSmooth,
                                finite(motion.anchor) ? &motion.anchor : nullptr);
                            haveAnimHead = true;
                        }
                    }
                    // Hybrid 180 ms pos+rot blend: slerp published eyes from the
                    // edge capture toward the live owner (tracking on G→T,
                    // headbone-centred on T→G). adoptEyes already re-anchored
                    // the rig on G→T; this covers residual gap and pitch/roll
                    // settle so reclaim is not a hard cut.
                    if (hybridEyeBlend) {
                        // Slice C destination: what the new owner shows once the blend is
                        // over. Into the head lock: the head-locked eyes just built, put on
                        // the head as the render thread will (native_eyes::onHead).
                        Pose to[2]{};
                        bool toHead = false;
                        if (headFollowEyes && (eyes.anchored == 2 || eyes.anchored == 4) &&
                            haveAnimHead) {
                            toHead = true;
                            for (unsigned i = 0; i < 2; ++i) {
                                to[i] = animated_view::fromNative(
                                    native_eyes::onHead(eyes.eyes[i].world, animHeadNow));
                                toHead = toHead && finite(to[i].position);
                            }
                        }
                        if (!toHead) {
                            // Tracking (G→T), or no head this frame: the tracked eyes, on
                            // T→G moved onto the headbone as before.
                            Vec3 landDelta{};
                            bool landDeltaOk = false;
                            if (!hybridBlend.toTraversal) {
                                Mat4 hb{};
                                if (game_fp::frame(hb) && motion.active) {
                                    const Vec3 hbPos{hb[12], hb[13], hb[14]};
                                    const Vec3 centre{(motion.eyes[0][12] + motion.eyes[1][12]) * .5f,
                                                      (motion.eyes[0][13] + motion.eyes[1][13]) * .5f,
                                                      (motion.eyes[0][14] + motion.eyes[1][14]) * .5f};
                                    landDelta = hbPos - centre;
                                    landDeltaOk = finite(landDelta);
                                }
                            }
                            for (unsigned i = 0; i < 2; ++i) {
                                if (motion.active) {
                                    to[i] = animated_view::fromNative(motion.eyes[i]);
                                    if (!hybridBlend.toTraversal && landDeltaOk)
                                        to[i].position = to[i].position + landDelta;
                                } else {
                                    to[i] = {hybridBlend.fromEyePos[i], hybridBlend.fromEyeOri[i]};
                                }
                                eyes.eyes[i].fov[0] = motion.fovs[i].left;
                                eyes.eyes[i].fov[1] = motion.fovs[i].right;
                                eyes.eyes[i].fov[2] = motion.fovs[i].down;
                                eyes.eyes[i].fov[3] = motion.fovs[i].up;
                            }
                            // The head lock latches afresh after this blend.
                            animHeadRefSet = false;
                        }
                        const Vec3 carry = hybrid_handoff::travel(hybridBlend.fromAnchor, motion.anchor);
                        for (unsigned i = 0; i < 2; ++i) {
                            const Pose blended = hybrid_handoff::blendEye(
                                {hybridBlend.fromEyePos[i], hybridBlend.fromEyeOri[i]}, carry, to[i],
                                eyeBlendW);
                            eyes.eyes[i].world =
                                hybrid_handoff::eyeMatFromPose(blended.position, blended.orientation);
                        }
                        eyes.anchored = 1;
                    }
                    if (hybridBlend.active && hybrid_handoff::blendDone(eyeBlendElapsed)) {
                        if (hybridBlend.delayArmsClear)
                            animatedArmsState = {};
                        hybridBlend.delayArmsClear = false;
                        hybridBlend.delayModelYaw = false;
                        hybridBlend.active = false;
                    }
                    // Slice C: keep what this command shows, in the world, as the next
                    // edge's blend source (head-locked eyes put on this frame's head).
                    hybridShown.valid = false;
                    if (values.animatedHybrid && values.animatedMoves && !flatScreen && eyes.enabled == 1) {
                        const bool onHeadEyes = eyes.anchored == 2 || eyes.anchored == 4;
                        if (!onHeadEyes || haveAnimHead) {
                            for (unsigned i = 0; i < 2; ++i)
                                hybridShown.eyes[i] = animated_view::fromNative(
                                    onHeadEyes ? native_eyes::onHead(eyes.eyes[i].world, animHeadNow)
                                               : eyes.eyes[i].world);
                            hybridShown.anchor = eyes.anchor;
                            hybridShown.ns = eyeBlendNow;
                            hybridShown.valid = true;
                            hybridShown.valid = hybrid_handoff::validShown(hybridShown, eyeBlendNow);
                        }
                    }
                    // HYBRID hands use the very same reference/turn as the eyes.
                    // Publish with the body command under commandLock; the pose job
                    // resolves Full on its fresh, unmodified head (not this thread's sample).
                    animated_view::Targets handView;
                    if (bodyStarted && values.animatedHybrid && yieldAnim &&
                        values.animatedArms && (pose.flags & native_body::armsOverlay)) {
                        const bool anchoredHead = eyes.anchored == 2 || eyes.anchored == 4;
                        Pose reference = animHeadRef;
                        float turn = animHeadTurn;
                        handView.frame =
                            eyes.anchored == 2 ? animated_view::Frame::full : animated_view::Frame::level;
                        bool frameOk = anchoredHead && animHeadRefSet;
                        if (eyes.anchored == 4) {
                            float levelYaw = 0;
                            frameOk = frameOk && game_fp::sceneYaw(true, levelYaw);
                            handView.levelWorld = Quat::yaw(levelYaw);
                        } else if (!anchoredHead && validTrackedPose(frame.head)) {
                            // Existing world-space eye blend: map hands around its
                            // displayed centre, without changing blend endpoints/timing.
                            handView.frame = animated_view::Frame::world;
                            handView.worldAnchor = animated_view::fromNative(eyes.eyes[0].world);
                            handView.worldAnchor.position =
                                Vec3{(eyes.eyes[0].world[12] + eyes.eyes[1].world[12]) * .5f,
                                     (eyes.eyes[0].world[13] + eyes.eyes[1].world[13]) * .5f,
                                     (eyes.eyes[0].world[14] + eyes.eyes[1].world[14]) * .5f};
                            reference = frame.head;
                            turn = 0;
                            frameOk = true;
                        }
                        if (frameOk) {
                            // FULL head lock on the stable heading: the arms' anchor is
                            // turned in the pose job as the eyes are (view_yaw_lock.hpp).
                            if (handView.frame == animated_view::Frame::full) {
                                const auto lock = view_yaw_lock::live();
                                handView.yawLock = lock.on;
                                handView.lockYaw = lock.yaw;
                                handView.lockSway = lock.sway;
                                handView.lockHero = lock.heroYaw;
                            }
                            handView.head = animated_view::relative(frame.head, reference, turn);
                            for (unsigned i = 0; i < 2; ++i) {
                                if (frame.hands[i].valid && validTrackedPose(frame.hands[i].grip) &&
                                    validTrackedPose(frame.hands[i].aim)) {
                                    animatedLastGrip[i] = frame.hands[i].grip;
                                    animatedLastAim[i] = frame.hands[i].aim;
                                    animatedHandSeen[i] = true;
                                }
                                handView.valid[i] = animatedHandSeen[i];
                                handView.grips[i] =
                                    animated_view::relative(animatedLastGrip[i], reference, turn);
                                handView.aims[i] =
                                    animated_view::relative(animatedLastAim[i], reference, turn);
                                // HAND PREDICTION (review H1/M2): the tracking-space
                                // velocity turned into this frame's relative space, and
                                // the same lead the actions' hand used (kinematics).
                                handView.gripVelocity[i] = arm_frame::relativeVelocity(
                                    armVelocity[i].value(), reference, turn);
                            }
                            handView.leadSeconds = armLead;
                            LARGE_INTEGER ticks{};
                            QueryPerformanceCounter(&ticks);
                            handView.sampledTicks = ticks.QuadPart;
                        } else {
                            handView = {};
                            handView.frame = animated_view::Frame::missing;
                        }
                    } else {
                        animatedHandSeen[0] = animatedHandSeen[1] = false;
                    }
                    // The frame the hands go out in: the per-frame map takes it next loop.
                    if (handView.frame != armFrameWas)
                        ++armEpoch; // another frame mode: velocities start afresh
                    armFrameWas = handView.frame;
                    armLevelWas = handView.levelWorld;
                    armWorldEyesWas = handView.worldAnchor;
                    if (bodyStarted)
                        native_body::submit(pose, handView);
                    // The HUD panel for this command's frame, as the head sees it:
                    // fixed in the room, so a reused image the headset turns to a
                    // newer head shows it where it was.
                    hudFacing = hudFollow.update(frame.head.orientation, frame.seconds);
                    hudHead = frame.head.position;
                    hudPlaced = true;
                    native_hud::follow(eyes.serial, frame.head.orientation.conjugate() * hudFacing);
                    NativeEyeFrame saved;
                    saved.serial = eyes.serial;
                    saved.motion = motion;
                    saved.animation = headFollowEyes || nativeTrackedPresentation(bodyStarted && values.body,
                        values.animatedHybrid, yieldAnim, values.animatedArms);
                    saved.feedbackRigHead = motion.headPose;
                    const bool captureActionFrame = saved.animation && actionHeadValid &&
                        animation_cancel::validPose(actionPoseSerial, actionFrameFloor,
                                                   actionPoseDeadline, GetTickCount64());
                    if (captureActionFrame) saved.motion.headPose = actionHead;
                    else {
                        // Ownership may have changed since the early input sample.
                        saved.motion.gazePose = physicalGaze;
                        saved.motion.gazeValid = physicalGazeValid;
                    }
                    for (unsigned i=0;i<2;++i) if(captureActionFrame && (actionGripMask & (1u<<i))) {
                        saved.motion.grips[i] = actionGrips[i];
                        saved.motion.hands[i] = actionGrips[i];
                    }
                    feedback_frame::hands(saved.motion, saved.animation,
                                          captureActionFrame ? actionGripMask : 0u);
                    saved.trackingEyes = frame.eyes;
                    // Same as finishers: placeEyes re-resolves the head each frame
                    // (stereo_probe) instead of keeping a latched gameplay head.
                    saved.scene = headFollowEyes || saved.animation;
                    if (motion.active)
                        saved.rigTurn =
                            flip_reprojection::trackingTurn(motion.headPose.orientation, frame.head.orientation);
                    turnNow = saved.rigTurn;
                    saved.flatScreen = flatScreen;
                    saved.uiStyle = values.uiStyle == 1 ? UiStyle::tech : UiStyle::comic;
                    saved.combatVisuals = values.combatVisuals;
                    saved.screenPose = screenPose;
                    saved.screenAspect = SpidyFlatAspect();
                    saved.slow = values.combatVisuals ? moments.view(slowMotion.view()) : slowMotion.view();
                    saved.padLayer = padLayer.latched();
                    if (gadgetWheel.open()) {
                        // The gadget wheel, from tracking space into this frame's
                        // world, with the gadgets in their places.
                        const Pose toWorld = compose(motion.headPose, inverse(frame.head));
                        const auto& f = gadgetWheel.frame();
                        auto& view = saved.gadgetWheel;
                        view.open = true;
                        view.age = gadgetWheel.age();
                        view.litAge = gadgetWheel.litAge();
                        view.lit = gadgetWheel.lit();
                        view.frame = {compose(toWorld, Pose{f.centre, {}}).position, toWorld.orientation.rotate(f.right),
                                      toWorld.orientation.rotate(f.up), toWorld.orientation.rotate(f.out)};
                        wheelPlaces(view);
                    }
                    if (phone.live()) {
                        // The phone: this image's way from tracking space into its
                        // world, and the screen as it is now, which every image
                        // shows (phoneShown).
                        saved.phone = {true, compose(motion.headPose, inverse(frame.head))};
                        auto& view = phoneShown;
                        view.screen = phone.ui().screen();
                        view.app = phone.ui().app();
                        view.lit = phone.ui().button();
                        // GEAR: the gadget wheel's places in its order, the web
                        // shooter's first, and the one equipped as the wheel has it.
                        GadgetWheelView wheel;
                        wheelPlaces(wheel);
                        view.gearCount = 0;
                        for (int p = 0; p < gadgetPlaces; ++p) {
                            if (wheel.places[p] == Gadget::none)
                                continue;
                            auto& row = view.gear[view.gearCount++];
                            strncpy_s(row.name, gadgetName(wheel.places[p]), _TRUNCATE);
                            row.ammo = wheel.ammo[p];
                            row.most = wheel.most[p];
                            row.equipped = wheel.equipped == p;
                        }
                        view.gearTop = phone.ui().gearTop();
                        view.pcMedia = values.pcMedia;
                        if (values.pcMedia) {
                            // What the PC plays: one song or none, its app, its volume.
                            const auto media = pcMedia ? pcMedia->status() : PcMedia::Status{};
                            view.playing = media.playing;
                            view.failed = false;
                            view.track = 0;
                            view.tracks = media.session ? 1 : 0;
                            view.volume = media.volume;
                            view.noVolume = !media.volumeKnown;
                            strncpy_s(view.song, media.song, _TRUNCATE);
                            strncpy_s(view.source, media.source, _TRUNCATE);
                        } else {
                            const auto music = phoneMusic.status();
                            view.playing = music.playing;
                            view.autoplay = music.autoplay;
                            view.failed = music.failed;
                            view.track = music.track;
                            view.tracks = music.tracks;
                            view.volume = music.volume;
                            view.noVolume = false;
                            strncpy_s(view.song, music.song, _TRUNCATE);
                        }
                    }
                    for (unsigned i = 0; i < 2; ++i) {
                        saved.webs[i] = swingState.webs[i];
                        saved.webTimes[i] = webTimes[i];
                        saved.aims[i] = aims.hands[i];
                        saved.offers[i] = pointing[i] ? *pointing[i] : offer_pointing::Pointed{};
                    }
                    saved.webSlots = {webRopes.slot(0), webRopes.slot(1)};
                    saved.strandUpper = webRopes.upper();
                    saved.tiesByGame = native_webs::tiesDrawn();
                    saved.linesByGame = native_webs::linesDrawn();
                    saved.lineTarget = lineState.target;
                    saved.lineTargetId = lineState.targetId;
                    saved.lineCount = 0;
                    for (uint32_t i = 0; i < lineState.count && saved.lineCount < saved.lines.size(); ++i)
                        saved.lines[saved.lineCount++] = {lineState.lines[i].start, lineState.lines[i].end};
                    // Hung thugs' strands, each with its rope's place in what was handed
                    // to the game (strandRope; 0xff: none, the overlay draws it).
                    saved.strandCount = 0;
                    for (uint32_t h = 0; h < tieState.hangCount && h < std::size(tieState.hangs) &&
                                         saved.strandCount < saved.strands.size();
                         ++h) {
                        saved.strandLine[saved.strandCount] =
                            h < std::size(strandRope) ? strandRope[h] : uint8_t{0xff};
                        saved.strands[saved.strandCount++] = {tieState.hangs[h].anchor,
                                                              tieState.hangs[h].end};
                    }
                    saved.tieCount =
                        std::min<uint32_t>(tieState.count, static_cast<uint32_t>(saved.ties.size()));
                    for (uint32_t i = 0; i < saved.tieCount; ++i)
                        saved.ties[i] = {tieState.lines[i].anchor, tieState.lines[i].end};
                    saved.webFlight = webFlight;
                    for (unsigned i = 0; i < 2; ++i)
                        saved.flightVisual[i] = flightVisual[i];
                    if (calibration.phase() != body_calibration::Phase::idle) {
                        // The panel stays where it appeared in the room; the tracking
                        // space is where this frame's head puts it in the world.
                        auto& view = saved.calibration;
                        view.phase = calibration.phase();
                        view.hint = calibration.hint();
                        view.progress = calibration.progress();
                        view.ready = calibration.armsReady();
                        view.result = calibration.result();
                        view.result.eyeHeight = calibratedEye;
                        view.result.armLength = calibratedArm;
                        view.panel = compose(compose(motion.headPose, inverse(frame.head)), calibrationPanel);
                        for (unsigned i = 0; i < 2; ++i) {
                            view.handTracked[i] = frame.hands[i].valid && validTrackedPose(frame.hands[i].grip);
                            view.grips[i] = motion.grips[i];
                        }
                    }
                    history.remember(saved);
                    const auto code = SpidySetEyes(&eyes);
                    if (code && code != 4003)
                        check(code, "Tracked eye command");
                    return !code;
                },
                [&](std::array<XrRuntime::EyeTarget, 2>& targets) {
                    const auto copyStart = std::chrono::steady_clock::now();
                    if (showScreen && stereoScene && native_hud::onScreen(sceneHoldMs)) {
                        // The scene's newest pair, once the game has rendered
                        // one with the HUD's panel on its screen; until then
                        // the game's own frame below.
                        uint64_t pairSerial{}, pairGeneration{};
                        const bool copied = native_gpu::copyLatest(targets[0].texture, targets[1].texture,
                                                                   pairSerial, pairGeneration);
                        check(native_gpu::error(), "GPU eye bridge");
                        const auto* saved = copied ? history.find(pairSerial, displayTime) : nullptr;
                        if (saved && saved->stereoScreen) {
                            copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                               copyStart)
                                         .count();
                            runtime.presentation(true, screenPose, saved->screenAspect,
                                                 vr_settings::screenWidth(values.screenSize), true);
                            drawn = Presentation::stereoScreen;
                            D3D12Renderer::Captured pixels;
                            if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                                publishSnapshot(pendingSnapshot, &pixels);
                                snapshotTicket = 0;
                            }
                            if (!snapshotTicket && snapshots.due(GetTickCount64(), 0, false) &&
                                (snapshotTicket = overlay.capture(targets[0].texture)) != 0) {
                                pendingSnapshot = {};
                                pendingSnapshot.flatScreen = 1;
                                pendingSnapshot.generation = pairGeneration;
                                pendingSnapshot.serial = pairSerial;
                            }
                            std::lock_guard lock(telemetry);
                            InterlockedIncrement64(&SpidyXrData.sequence);
                            SpidyXrData.serial = pairSerial;
                            SpidyXrData.generation = pairGeneration;
                            InterlockedIncrement64(&SpidyXrData.sequence);
                            return true;
                        }
                    }
                    native_gpu::Screen image;
                    if (showScreen && native_gpu::latestScreen(image, imageHoldMs)) {
                        // The game's presented frame, scaled into the image the screen shows.
                        const auto& left = targets[0];
                        overlay.blit(image.texture.Get(), image.view, image.linear,
                                     {left.texture, left.format, left.width, left.height, {}});
                        copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                           copyStart)
                                     .count();
                        runtime.presentation(true, screenPose,
                                             static_cast<float>(image.width) / static_cast<float>(image.height),
                                             vr_settings::screenWidth(values.screenSize));
                        drawn = Presentation::gameScreen;
                        D3D12Renderer::Captured pixels;
                        if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                            publishSnapshot(pendingSnapshot, &pixels);
                            snapshotTicket = 0;
                        }
                        if (!snapshotTicket && snapshots.due(GetTickCount64(), 0, false) &&
                            (snapshotTicket = overlay.capture(left.texture)) != 0) {
                            pendingSnapshot = {};
                            pendingSnapshot.flatScreen = 1;
                        }
                        std::lock_guard lock(telemetry);
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        SpidyXrData.serial = SpidyXrData.generation = 0;
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        return true;
                    }
                    uint64_t imageSerial{}, imageGeneration{};
                    bool copied = native_gpu::copyLatest(targets[0].texture, targets[1].texture, imageSerial,
                                                         imageGeneration);
                    check(native_gpu::error(), "GPU eye bridge");
                    const auto* saved = copied ? history.find(imageSerial, displayTime) : nullptr;
                    copied = copied && saved;
                    copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                       copyStart)
                                 .count();
                    if (copied) {
                        // A scene's pair stays on its screen until play's first image.
                        drawn = saved->stereoScreen ? Presentation::stereoScreen
                                : saved->flatScreen ? Presentation::flat
                                                    : Presentation::immersive;
                        runtime.presentation(saved->flatScreen, saved->screenPose, saved->screenAspect,
                                             vr_settings::screenWidth(values.screenSize), saved->stereoScreen);
                        runtime.headLocked(saved->headLocked);
                        // A first-person scene gets none of Spidy's own marks, webs or hands over it.
                        const bool bare = saved->flatScreen || saved->scene;
                        bool feedback = gameplayFeedback(saved->flatScreen, saved->scene,
                                                               saved->animation);
                        // STEADY HUD: the panel as a layer of its own, placed for this headset frame; the
                        // eyes leave it out while it shows (native_hud::layered).
                        uint32_t hudWidth{}, hudHeight{};
                        uint64_t hudCopies{}, hudCopiedMs{};
                        if (drawn == Presentation::immersive && values.steadyHud && values.hud > 0 &&
                            hudPlaced && native_gpu::latestHud(hudWidth, hudHeight, hudCopies, hudCopiedMs) &&
                            steady_hud::fresh(GetTickCount64(), hudCopiedMs, true)) {
                            if (hudCopies != hudLayerCopies) {
                                hudLayerFilled =
                                    runtime.fillHud(hudWidth, hudHeight, [](ID3D12Resource* image) {
                                        return native_gpu::fillHudLayer(image);
                                    });
                                hudLayerCopies = hudCopies;
                            }
                            const auto quad =
                                steady_hud::quad(hudHead, hudFacing, native_hud::halfWidth(values.hud),
                                                 native_hud::Shape{}.distance, hudWidth, hudHeight);
                            if (hudLayerFilled && quad.valid) {
                                runtime.hudQuad(quad.pose, quad.width, quad.height);
                                native_hud::layered();
                                native_gpu::hudLayerShown();
                            }
                        }
                        // FLIP REPROJECTION: the pair turned by the player's own turn
                        // since it was rendered, about the head (flip_reprojection.hpp).
                        const bool reproject =
                            values.flipReprojection && !saved->flatScreen && !saved->stereoScreen;
                        const auto reprojection = reproject
                                                      ? flip_reprojection::correction(saved->rigTurn, turnNow)
                                                      : flip_reprojection::Correction{};
                        std::array<Pose, 2> layerEyes{saved->trackingEyes[0].pose, saved->trackingEyes[1].pose};
                        if (reprojection.applied)
                            layerEyes = flip_reprojection::layerEyes(layerEyes, reprojection.layer);
                        // What the player holds stays with the head: turned the other
                        // way in the world, about the eyes' middle, before it is drawn.
                        // The eyes' own views stay as rendered, as the image is.
                        GameMotionFrame basis = saved->motion;
                        std::array<Mat4,2> nativeFeedbackEyes{};
                        const bool nativeFeedback = saved->animation &&
                            native_eyes::renderEyes(imageGeneration, imageSerial, nativeFeedbackEyes);
                        if (saved->animation && !nativeFeedback) feedback = false;
                        Vec3 travel{};
                        native_eyes::renderOffset(imageGeneration, travel);
                        if (nativeFeedback)
                            feedback_frame::camera(basis, native_eyes::overlayEyes(nativeFeedbackEyes, travel));
                        const Vec3 heldAbout = flip_reprojection::eyesMiddle(basis);
                        GameMotionFrame rendered = reprojection.applied
                            ? flip_reprojection::held(basis, reprojection.world) : basis;
                        const auto widgetPose = [&](Pose p) {
                            return nativeFeedback ? feedback_frame::transport(
                                saved->feedbackRigHead, basis.headPose, p) : p;
                        };
                        const auto heldPose = [&](Pose p) {
                            p = widgetPose(p);
                            return reprojection.applied
                                       ? flip_reprojection::about(p, reprojection.world, heldAbout)
                                       : p;
                        };
                        {
                            std::lock_guard lock(telemetry);
                            auto& r = SpidyReprojectData;
                            InterlockedIncrement64(&r.sequence);
                            r.features = values.flipReprojection ? 1u : 0u;
                            if (reproject) {
                                const float degrees = reprojection.angle * 57.29578f;
                                r.lastDegrees = degrees;
                                if (!reprojection.applied) {
                                    ++r.passed;
                                } else if (degrees < .05f) {
                                    ++r.still;
                                } else {
                                    ++r.turned;
                                    r.reusedTurned += imageGeneration == reprojectedGeneration;
                                    r.mostDegrees = std::max(r.mostDegrees, degrees);
                                    r.sumDegrees += degrees;
                                }
                                reprojectedGeneration = imageGeneration;
                            }
                            InterlockedIncrement64(&r.sequence);
                        }
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& pose = layerEyes[eye];
                            const auto& f = saved->trackingEyes[eye].fov;
                            targets[eye].view.pose = {{pose.orientation.x, pose.orientation.y,
                                                       pose.orientation.z, pose.orientation.w},
                                                      {pose.position.x, pose.position.y, pose.position.z}};
                            targets[eye].view.fov = {f.left, f.right, f.up, f.down};
                        }
                        const auto overlayStart = std::chrono::steady_clock::now();
                        // The native views moved this command's eyes with the
                        // player to the rendered frame. Hands move with them, so
                        // world-space web anchors line up with the rendered city.
                        const Vec3 viewer =
                            Vec3{(rendered.eyes[0][12] + rendered.eyes[1][12]) / 2,
                                 (rendered.eyes[0][13] + rendered.eyes[1][13]) / 2,
                                 (rendered.eyes[0][14] + rendered.eyes[1][14]) / 2} +
                            travel;
                        const auto& lens = rendered.fovs[0];
                        const float pixelAngle =
                            (std::tan(lens.right) - std::tan(lens.left)) / static_cast<float>(targets[0].width);
                        const auto shown = saved->motion.predictedDisplayTime;
                        std::vector<Vertex> vertices;
                        // Where the overlay puts each tracked web shooter, and
                        // where the game's rope for this image's frame starts.
                        Vec3 shooters[2]{}, ropeStarts[2]{};
                        uint32_t shooterHands{}, ropeHands{};
                        float webGap = -1;
                        for (unsigned hand = 0; !bare && hand < 2; ++hand) {
                            const auto& web = saved->webs[hand];
                            const auto& timing = saved->webTimes[hand];
                            // The game's rope is part of the scene image already: the
                            // rope that draws this hand's web, which may be the other
                            // side's (one web in both hands).
                            const int slot = saved->webSlots[hand];
                            const bool gameWeb = native_webs::drawing(slot >= 0 ? static_cast<unsigned>(slot) : hand);
                            // One web in both hands: the strand comes to the other hand
                            // first, and this hand's part runs from wrist to wrist, as
                            // slack as the rope between the two holds is long.
                            const int upper = saved->strandUpper;
                            const bool lower = web.attached && upper >= 0 && upper != static_cast<int>(hand);
                            WebLine line;
                            const auto between = [&](Vec3 start) {
                                auto other = rendered.grips[static_cast<size_t>(upper)];
                                other.position += travel;
                                line.start = start;
                                line.end = webShooter(other, static_cast<unsigned>(upper)).position;
                                line.slack = std::max(
                                    web.length - saved->webs[static_cast<size_t>(upper)].length -
                                        length(line.end - line.start),
                                    0.f);
                                line.extended = 1;
                                line.splat = false;
                            };
                            // The timeline keeps the anchor through the release, so
                            // the splat keeps its shape while the web snaps back.
                            line.seed =
                                static_cast<float>(hand) * 1.7f + timing.anchor.x * .37f + timing.anchor.z * .11f;
                            if (!rendered.swing.hands[hand].tracked) {
                                if (!gameWeb && WebAnimation::released((shown - timing.releasedAt) * 1e-9f,
                                                                       timing.wrist + travel, timing.anchor, line))
                                    appendWeb(vertices, line, viewer, pixelAngle);
                                continue;
                            }
                            auto grip = rendered.grips[hand];
                            grip.position += travel;
                            const Vec3 shooter = webShooter(grip, hand).position;
                            shooters[hand] = shooter;
                            shooterHands |= 1u << hand;
                            if (gameWeb) {
                                // The game draws the piece between the hands
                                // itself where it can (native_webs::Held).
                                if (lower && !native_webs::drawsHeld()) {
                                    between(shooter);
                                    appendWeb(vertices, line, viewer, pixelAngle);
                                } else if (web.attached && slot == static_cast<int>(hand) &&
                                           native_eyes::renderWebStart(imageGeneration, hand, ropeStarts[hand])) {
                                    ropeHands |= 1u << hand;
                                    webGap = std::max(webGap, length(shooters[hand] - ropeStarts[hand]));
                                }
                                continue;
                            } else if (lower) {
                                between(shooter);
                                appendWeb(vertices, line, viewer, pixelAngle);
                            } else if (web.attached) {
                                line.start = shooter;
                                line.end = web.anchor;
                                line.slack = web.tension > 0 ? 0.f
                                                             : std::max(web.length - length(line.end - line.start), 0.f);
                                if (saved->webFlight.durations[hand] > 0) {
                                    line.extended = 1;
                                    if (saved->flightVisual[hand].flying()) {
                                        line.end = saved->flightVisual[hand].tip();
                                        line.splat = false;
                                        line.slack = 0;
                                        line.flightProgress = saved->flightVisual[hand].progress();
                                        line.flightAge = saved->flightVisual[hand].age();
                                        line.flightDirection = saved->flightVisual[hand].direction();
                                    }
                                } else
                                    line.extended =
                                        WebAnimation::extended((shown - timing.attachedAt) * 1e-9f);
                                appendWeb(vertices, line, viewer, pixelAngle);
                            } else if (WebAnimation::released((shown - timing.releasedAt) * 1e-9f, shooter,
                                                              timing.anchor, line)) {
                                appendWeb(vertices, line, viewer, pixelAngle);
                            }
                        }
                        // The web tie's webs: from the surface to the enemy held on it,
                        // taut, with the splat where they stick.
                        // Only those the game does not draw itself (both hands have webs of
                        // their own, or a tie beyond the game's ropes for them): as thin as
                        // a game web, not the overlay's swing strand.
                        WebLook tieLook;
                        tieLook.radius = .0018f;
                        tieLook.minPixels = 1.f;
                        for (uint32_t i = 0; !bare && i < saved->tieCount; ++i) {
                            const auto& strand = saved->ties[i];
                            if ((saved->tiesByGame >> i) & 1u)
                                continue;
                            if (!finite(strand.anchor) || !finite(strand.end))
                                continue;
                            WebLine line;
                            line.start = strand.end;
                            line.end = strand.anchor;
                            line.slack = 0;
                            line.extended = 1;
                            line.seed = 3.1f + static_cast<float>(i) * .9f + strand.anchor.x * .23f;
                            appendWeb(vertices, line, viewer, pixelAngle, tieLook);
                        }
                        // Web lines and the strands of thugs hung from them: only ever
                        // in the game's own rope (native_webs). The overlay drew a thin white
                        // strand for one the game had not drawn yet - each new line's first
                        // frame or two, the first line's longest ("only clean web, never that
                        // white thread", November 5) - and draws none for them now.
                        // Aim markers: where each free hand's web button would send its
                        // web, on the line that hand points along in this image, that
                        // line steadied from image to image. Translucent, the farthest
                        // drawn first.
                        std::vector<AimMarker> marks;
                        for (unsigned hand = 0; hand < 2; ++hand) {
                            auto& steady = aimMotion[hand];
                            const auto& input = rendered.swing.hands[hand];
                            // Flat screen stays bare. Native Animation head-follow still
                            // hides overlay webs and gloves (saved.scene), but the aim
                            // markers stay up so a shot or a grip is visible.
                            if (!feedback || saved->flatScreen) {
                                steady.reset();
                                continue;
                            }
                            steady.begin(shown);
                            if (!input.tracked) {
                                steady.markers(nullptr, {}, {}, marks);
                                continue;
                            }
                            const auto& aim = saved->aims[hand];
                            const auto kind = static_cast<game_swing::AimKind>(aim.kind);
                            // The hand moved with the player to this image, as its glove does.
                            const Vec3 origin = input.aim.position + travel;
                            // Along the line its webs leave (WEB ANGLE, WRIST
                            // SHOOTERS; web_aim.hpp), as the swing's aim previews.
                            // WEB AIM: MODEL FOREARM: the turn the swing was sent.
                            const web_aim::Shooters webShooters{values.webAngle, values.palmShooters};
                            const Pose webLine =
                                webTurnsOn
                                    ? web_aim::shooterAim(input.aim, static_cast<int>(hand), webShooters,
                                                          webTurnsNow[hand])
                                    : web_aim::shooterAim(input.aim, static_cast<int>(hand), webShooters);
                            const Vec3 direction =
                                steady.aim(normalized(webLine.orientation.rotate({0, 0, -1})),
                                           trackingTurn(rendered.swing));
                            const float reach = length(aim.point - input.aim.position);
                            AimMarker marker;
                            marker.squeeze = input.grip;
                            marker.hand = hand;
                            // A hand pointed at one of the game's interactions on
                            // Spidy's wall shows that instead: a turning ring round
                            // one the interact would use, a faint one round one to
                            // come closer to.
                            if (const auto& offer = saved->offers[hand]; offer.handle) {
                                marker.kind = offer.usable ? AimMark::target : AimMark::air;
                                marker.point = offer.position;
                                marker.radius = offer_pointing::markRadius;
                                steady.markers(&marker, origin, direction, marks);
                                ++markersDrawn;
                                continue;
                            }
                            switch (kind) {
                            case game_swing::AimKind::prop:
                            case game_swing::AimKind::character:
                                marker.kind = AimMark::target;
                                marker.point = aim.point;
                                marker.radius = aim.radius;
                                break;
                            case game_swing::AimKind::anchor:
                                marker.point = onAimLine(aim.point, aim.normal, origin, direction);
                                break;
                            case game_swing::AimKind::air:
                                marker.kind = AimMark::air;
                                marker.point = origin + direction * reach;
                                break;
                            default:
                                marker.kind = AimMark::blocked;
                                marker.point = length(aim.normal) > .5f
                                                   ? onAimLine(aim.point, aim.normal, origin, direction)
                                                   : origin + direction * reach;
                                break;
                            }
                            // Not on the floor a lowered hand points at, by the player's feet.
                            const float away = length(marker.point - origin);
                            const float nearest = marker.kind == AimMark::target    ? .5f
                                                  : marker.kind == AimMark::blocked ? 3.f
                                                                                    : 2.f;
                            const bool wanted = kind != game_swing::AimKind::none &&
                                                kind <= game_swing::AimKind::character &&
                                                !saved->webs[hand].attached && away >= nearest;
                            steady.markers(wanted ? &marker : nullptr, origin, direction, marks);
                            markersDrawn += wanted;
                        }
                        // A web line's zip marker: a turning ring where the right stick up
                        // would zip to.
                        if (feedback && !saved->flatScreen && saved->lineTargetId && finite(saved->lineTarget)) {
                            AimMarker marker;
                            marker.kind = AimMark::target;
                            marker.point = saved->lineTarget;
                            marker.radius = .3f;
                            marker.hand = 1;
                            marker.spin =
                                static_cast<float>(std::fmod(static_cast<double>(shown) * 1e-9, 6.283185307));
                            marks.push_back(marker);
                        }
                        std::sort(marks.begin(), marks.end(), [&](const AimMarker& a, const AimMarker& b) {
                            return length(a.point - viewer) > length(b.point - viewer);
                        });
                        std::vector<Vertex> translucent;
                        for (const auto& mark : marks)
                            appendAimMarker(translucent, mark, viewer, pixelAngle * markerPixels,
                                            saved->uiStyle);
                        // Slow motion's focus meter over the back of the left wrist, moved
                        // with the player to this image as the glove is.
                        if (feedback && saved->slow.meter > 0 && rendered.swing.hands[0].tracked) {
                            auto wrist = rendered.hands[0];
                            wrist.position += travel;
                            const double clock = std::fmod(static_cast<double>(shown) * 1e-9, 3600.);
                            appendSlowMotionMeter(translucent, saved->slow, wrist, viewer, pixelAngle,
                                                  static_cast<float>(clock), saved->uiStyle);
                        }
                        // The pad layer's sign over the back of the right wrist while
                        // it is on: until October 25 only a buzz said so.
                        if (feedback && saved->padLayer && rendered.swing.hands[1].tracked) {
                            auto wrist = rendered.hands[1];
                            wrist.position += travel;
                            appendPadLayerSign(translucent, wrist, viewer, pixelAngle, saved->uiStyle);
                        }
                        // Hide combat artwork only; targeting, damage, counters and haptics still update.
                        const bool combatVisuals = values.combatVisuals && saved->combatVisuals && feedback;
                        // The bursts where the latest blows landed: fixed in the world.
                        if (combatVisuals)
                            for (const auto& burst : bursts) {
                                if (!burst.since)
                                    continue;
                                auto view = burst.view;
                                view.age = static_cast<float>(static_cast<double>(shown - burst.since) * 1e-9);
                                appendImpact(translucent, view, viewer, pixelAngle, saved->uiStyle);
                            }
                        // The called blow's arrow, over its thug.
                        if (combatVisuals && cue.left > 0) {
                            auto view = cue;
                            view.age = static_cast<float>(static_cast<double>(shown - cueSince) * 1e-9);
                            if (view.age >= 0)
                                appendCue(translucent, view, viewer, pixelAngle, saved->uiStyle);
                        }
                        // A knockout's lettering, where he stood.
                        if (combatVisuals)
                            for (const auto& mark : knockoutMarks) {
                                if (!mark.since)
                                    continue;
                                auto view = mark.view;
                                view.age = static_cast<float>(static_cast<double>(shown - mark.since) * 1e-9);
                                appendKnockout(translucent, view, viewer, pixelAngle, saved->uiStyle);
                            }
                        // Spider-Sense's marks: over each attacker's head, and for one out of
                        // view in the sightline as well, moved with the player as the eyes are.
                        // Attack warnings remain visible independently of optional combat artwork.
                        if (feedback && validTrackedPose(rendered.headPose)) {
                            Pose eyes = rendered.headPose;
                            eyes.position += travel;
                            bool marked{}, sighted{};
                            const auto since = [&](int64_t at) {
                                return static_cast<float>(static_cast<double>(shown - at) * 1e-9);
                            };
                            for (auto& mark : senseMarks) {
                                if (!mark.attacker)
                                    continue;
                                const float age = since(mark.since);
                                const float broken = mark.brokenAt ? since(mark.brokenAt) : -1.f;
                                if (age < 0)
                                    continue;
                                SenseView view{mark.head, mark.answer, mark.left, mark.timed, age, broken};
                                appendSense(translucent, view, viewer, pixelAngle, saved->uiStyle);
                                mark.sight = senseSight(eyes, mark.head, mark.sight);
                                if (mark.sight.shown) {
                                    const SenseSightView sight{mark.answer, mark.sight, mark.left,
                                                               mark.timed,  age,        broken};
                                    appendSenseSight(translucent, sight, eyes, pixelAngle, {},
                                                     saved->uiStyle);
                                }
                                marked = true;
                                sighted |= mark.sight.shown;
                            }
                            senseFrames += marked;
                            senseSightFrames += sighted;
                        }
                        // The T-pose calibration's panel and controller rings, moved with
                        // the player to this image as the hands are. The tech style's
                        // panel is translucent, as the game's menus are.
                        if (feedback && saved->calibration.phase != body_calibration::Phase::idle) {
                            auto view = saved->calibration;
                            view.panel = heldPose(view.panel);
                            view.panel.position += travel;
                            for (auto& grip : view.grips) {
                                grip = heldPose(grip);
                                grip.position += travel;
                            }
                            auto& panel = saved->uiStyle == UiStyle::tech ? translucent : vertices;
                            body_calibration::appendView(panel, view, viewer, saved->uiStyle);
                        }
                        // The gadget wheel in front of its hand, moved with the
                        // player to this image as the hands are: printed, so
                        // laid down in order over all the rest.
                        if (feedback && saved->gadgetWheel.open) {
                            auto view = saved->gadgetWheel;
                            if (nativeFeedback) {
                                auto& w = view.frame;
                                const Quat turn = basis.headPose.orientation * saved->feedbackRigHead.orientation.conjugate();
                                w.centre = widgetPose({w.centre, {}}).position;
                                w.right = turn.rotate(w.right);
                                w.up = turn.rotate(w.up);
                                w.out = turn.rotate(w.out);
                            }
                            if (reprojection.applied) {
                                auto& w = view.frame;
                                w.centre = flip_reprojection::about(w.centre, reprojection.world, heldAbout);
                                w.right = reprojection.world.rotate(w.right);
                                w.up = reprojection.world.rotate(w.up);
                                w.out = reprojection.world.rotate(w.out);
                            }
                            view.frame.centre += travel;
                            appendGadgetWheel(translucent, view, viewer, pixelAngle, saved->uiStyle);
                        }
                        // From here on the overlay is drawn about the eyes' middle,
                        // not the world's origin. The city is kilometres across, and
                        // a vertex there taken through the eye's view-projection in
                        // single floats was off by a tenth of a millimetre or so,
                        // each by its own amount and anew every frame: the phone's
                        // screen, 0.3 mm over its glass, lost to the glass at a
                        // fifth of its points 1158 m out (the user, October 24:
                        // "still z-fighting between screen and its body"). About
                        // the eyes the error is a thousandth of that.
                        const Vec3 about = viewer;
                        for (auto& v : vertices)
                            v.position = v.position - about;
                        for (auto& v : translucent)
                            v.position = v.position - about;
                        // The phone where it is now - in the hand, where the hand is
                        // now - through this image's own way into its world, and
                        // moved with the player to this image as the wheel is: its
                        // body lit as it turns, its screen a picture of its own
                        // (D3D12Renderer::Picture) on the glass. Built about the
                        // eyes from the start, its parts a tenth of a millimetre
                        // apart stay so.
                        std::vector<Vertex> phoneScreen;
                        D3D12Renderer::Picture phonePicture;
                        if (feedback && saved->phone.open) {
                            auto view = phoneShown;
                            const Pose toWorld = heldPose(saved->phone.toWorld);
                            const auto& f = phone.place();
                            view.open = true;
                            view.age = phone.age();
                            const Quat turn = toWorld.orientation;
                            view.frame = {compose(toWorld, Pose{f.centre, {}}).position + travel - about,
                                          turn.rotate(f.right), turn.rotate(f.up), turn.rotate(f.out)};
                            appendPhone(vertices, view, viewer - about);
                            appendPhoneScreen(phoneScreen, view);
                            phonePicture = {phoneScreen, phoneScreenHalfWidth, phoneScreenHalfHeight,
                                            phoneScreenCorners(view), phoneScreenGlass()};
                        }
                        std::array<D3D12Renderer::ViewTarget, 2> overlayViews;
                        // Slow motion recolours the image under the overlay, about each eye's lens.
                        D3D12Renderer::Grade grade;
                        grade.amount = saved->slow.blend;
                        grade.ripple = saved->slow.ripple;
                        grade.rippleStrength = saved->slow.entering ? 1.f : .7f;
                        grade.entering = saved->slow.entering;
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& f = rendered.fovs[eye];
                            grade.tangents[eye] = {std::tan(f.left), std::tan(f.right), std::tan(f.up),
                                                   std::tan(f.down)};
                        }
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& target = targets[eye];
                            const auto& f = rendered.fovs[eye];
                            // The eye about the eyes' middle, as the overlay is.
                            auto pose = rendered.eyes[eye];
                            pose[12] += travel.x - about.x;
                            pose[13] += travel.y - about.y;
                            pose[14] += travel.z - about.z;
                            const auto vp = multiply(projection(f.left, f.right, f.down, f.up, .03f, 200.f),
                                                     native_view::view(pose));
                            overlayViews[eye] = {target.texture, target.format, target.width, target.height,
                                                 vp};
                        }
                        if (!bare)
                            overlay.renderViews(overlayViews, vertices, true, false, translucent, &grade,
                                                &phonePicture);
                        else if (feedback && (!vertices.empty() || !translucent.empty() || !phoneScreen.empty()))
                            overlay.renderViews(overlayViews, vertices, true, false, translucent, nullptr,
                                                &phonePicture);
                        overlayMs = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - overlayStart)
                                        .count();
                        // The game's window mirrors the left eye as it is now, the
                        // drawings above on it; copied only when the window took the last.
                        native_gpu::keepShownEye(targets[0].texture);
                        if (webGap >= 0 && imageGeneration != gapGeneration) {
                            native_eyes::reportWebGap(webGap);
                            gapGeneration = imageGeneration;
                        }
                        // Session report: publish a finished copy of the left
                        // eye, then queue the next when one is due.
                        D3D12Renderer::Captured pixels;
                        if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                            publishSnapshot(pendingSnapshot, &pixels);
                            snapshotTicket = 0;
                        }
                        const float speed = swingState.status ? length(swingState.velocity) : 0;
                        if (!snapshotTicket && snapshots.due(GetTickCount64(), speed, ropeHands != 0) &&
                            (snapshotTicket = overlay.capture(targets[0].texture)) != 0) {
                            pendingSnapshot = {};
                            pendingSnapshot.flags = ropeHands;
                            pendingSnapshot.generation = imageGeneration;
                            pendingSnapshot.serial = imageSerial;
                            pendingSnapshot.flatScreen = saved->flatScreen;
                            pendingSnapshot.speed = speed;
                            pendingSnapshot.webGap = webGap;
                            const auto& eye = overlayViews[0];
                            for (unsigned hand = 0; hand < 2; ++hand) {
                                if (shooterHands & (1u << hand))
                                    eyePixel(eye.viewProjection, shooters[hand], eye.width, eye.height,
                                             pendingSnapshot.wrist[hand][0], pendingSnapshot.wrist[hand][1]);
                                if (ropeHands & (1u << hand))
                                    eyePixel(eye.viewProjection, ropeStarts[hand], eye.width, eye.height,
                                             pendingSnapshot.ropeStart[hand][0],
                                             pendingSnapshot.ropeStart[hand][1]);
                            }
                        }
                    }
                    std::lock_guard lock(telemetry);
                    auto& d = SpidyXrData;
                    InterlockedIncrement64(&d.sequence);
                    d.dropped += !copied;
                    d.serial = imageSerial;
                    d.generation = imageGeneration;
                    d.markers = markersDrawn;
                    InterlockedIncrement64(&d.sequence);
                    return copied;
                });
            // No headset frame for a while: slow motion ends on its own, in real time.
            if (const auto now = GetTickCount64(); now - slowSteppedMs >= 200)
                stepSlowMotion(static_cast<float>(now - slowSteppedMs) / 1000, false, false);
            const auto& timing = runtime.lastFrameTiming();
            if (submittedFrames >= 30 && timing.period > 0) {
                const double stages[] = {timing.total,   timing.wait,  timing.tracking, timing.prepare,
                                         timing.acquire, copyMs,       overlayMs,       timing.release,
                                         timing.end,     timing.period};
                std::lock_guard lock(telemetry);
                InterlockedIncrement64(&SpidyXrTimingData.sequence);
                for (unsigned i = 0; i < 10; ++i)
                    SpidyXrTimingData.stages[i].add(stages[i]);
                InterlockedIncrement64(&SpidyXrTimingData.sequence);
            }
            // Count only a successful xrEndFrame containing a projection layer.
            // A GPU copy alone does not confirm that OpenXR accepted the frame.
            const bool submitted = runtime.lastFrameSubmitted();
            if (submitted) {
                ++submittedFrames;
                if (const int stage = startupStages.submitted(GetTickCount64()))
                    stageLine(stage, drawn == Presentation::gameScreen     ? "the game's screen"
                                     : drawn == Presentation::stereoScreen ? "the game's screen in 3D"
                                     : drawn == Presentation::flat         ? "flat"
                                                                           : "the eye images");
                lastPresentedMs = GetTickCount64();
                if (!presented || drawn != shownKind) {
                    presented = true;
                    shownKind = drawn;
                    // The game played on its screen shows it there, flat or in 3D.
                    const bool screenPlay =
                        flatScreen && (drawn == Presentation::gameScreen || drawn == Presentation::stereoScreen);
                    message(3, 0,
                            screenPlay
                                ? "The game on its screen: the controllers work as an Xbox controller; hold the "
                                  "menu button for its Back (the map), hold the left stick's click and push the "
                                  "right stick for its D-pad - pause, SWITCH TO VR goes back to VR"
                            : drawn == Presentation::gameScreen
                                ? "Game screen (menus, loading, cutscenes): the controllers work as an Xbox "
                                  "controller, VR settings are in Settings > SPIDY VR - VR resumes with gameplay"
                            : drawn == Presentation::stereoScreen
                                ? "Game screen in 3D (a cutscene or another scene the game plays): the "
                                  "controllers work as an Xbox controller - Settings > SPIDY VR > GAME SCREEN 3D "
                                  "sets its depth"
                            : drawn == Presentation::flat
                                ? "Flat screen - pause, SWITCH TO VR goes back to VR"
                                // 255 characters, all the message holds.
                                : "VR active - A jumps (hold: Charge Jump), B dodges, Y interacts, X: LB + RB "
                                  "where shown (hold: gadget wheel), menu or both sticks held: pause, left "
                                  "stick click: slow time, trigger at temple: scan; right stick down: heal, up: "
                                  "zip (hold to aim, let go)");
                }
            }
            {
                std::lock_guard lock(telemetry);
                auto& d = SpidyXrData;
                InterlockedIncrement64(&d.sequence);
                d.presentation = static_cast<uint32_t>(submitted ? drawn : Presentation::none);
                if (submitted) {
                    ++d.submitted;
                    // Game-screen frames are not eye images: neither new nor reused ones.
                    if (drawn == Presentation::gameScreen || drawn == Presentation::stereoScreen) {
                        ++d.screenSubmitted;
                        d.stereoSubmitted += drawn == Presentation::stereoScreen;
                    } else if (d.generation != previousPresentedGeneration) {
                        ++d.uniqueSubmitted;
                        lastNewImageMs = lastPresentedMs;
                        previousPresentedGeneration = d.generation;
                    } else {
                        ++d.reusedSubmitted;
                    }
                }
                InterlockedIncrement64(&d.sequence);
            }
            if (!active)
                break;
        }
        // HEAD PROBE: the session's numbers, one line per state, before VR stops.
        if (headProbeWas) {
            char session[head_probe::stateCount][head_probe::lineChars];
            const unsigned n = headProbe->session(session, head_probe::stateCount);
            for (unsigned i = 0; i < n; ++i) {
                diag_log::write(session[i]);
                game_prompts::note(session[i], false);
            }
            if (!n)
                diag_log::write("head probe: session: no samples (the body never posed the hero's head)");
            native_body::setHeadProbe(false);
            publishDiag();
        }
        if (!runtime.endReason().empty()) {
            const auto text = runtime.endReason() + ". Stopping VR and restoring game hooks";
            game_prompts::note(text.c_str());
            message(6, 0, text.c_str());
        } else
            message(6, 0, "Stopping VR and restoring game hooks");
    } catch (const std::exception& e) {
        error = 1;
        failure = e.what();
    }
    players.stop();
    if (padInstalled)
        game_pad::uninstall();
    game_menu::uninstall();
    // The game's own time back, and no blow still sounding. The ticket's
    // guard stays for as long as the game runs: it is what keeps a quit to
    // the main menu after VR from closing the game.
    game_time::uninstall();
    PlaySoundW(nullptr, nullptr, 0);
    bridge::Control release;
    release.serial = ++serial;
    release.leaseMs = 0;
    if (submitInput)
        submitInput(&release);
    if (swingStarted) {
        const auto code = stopSwing(nullptr);
        if (code) {
            error = code;
            failure = "Native swinging did not stop cleanly";
        }
    }
    if (websStarted) {
        const auto code = native_webs::stop();
        if (code) {
            error = code;
            failure = "Game web lines did not stop cleanly";
        }
    }
    if (bodyStarted) {
        const auto code = native_body::stop();
        if (code) {
            error = code;
            failure = "The player's body did not stop cleanly";
        }
    }
    if (nativeStarted) {
        const auto code = SpidyStop(nullptr);
        if (code) {
            error = code;
            failure = "Native views did not retire cleanly";
        }
    }
    if (gpuStarted) {
        const auto code = native_gpu::stop();
        if (code) {
            error = code;
            failure = "Native GPU bridge did not stop cleanly";
        }
    }
    if (raysStarted) {
        const auto code = stopRays(nullptr);
        if (code) {
            error = code;
            failure = "Controller world rays did not stop cleanly";
        }
    }
    if (stopBridge) {
        const auto code = stopBridge(nullptr);
        if (code) {
            error = code;
            failure = "Input bridge did not stop cleanly";
        }
    }
    message(error ? 5 : 4, error, error ? failure.c_str() : "VR session finished");
    return error;
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrStart(void* input) {
    std::lock_guard life(lifecycle);
    if (worker)
        return 1000; // one bounded XR session per process during validation
    if (!read(reinterpret_cast<uintptr_t>(input), &config, sizeof(config)) || config.magic != 0x53585243 ||
        config.version != 24 || config.bytes != sizeof(config) || config.pid != GetCurrentProcessId() ||
        config.runtime[std::size(config.runtime) - 1] ||
        config.base != reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr)) ||
        !GetModuleHandleW(L"Spider-Man.exe") || !config.queue || !config.bridgeModule || !config.rayModule ||
        !config.motionModule || config.options > 0x1ffffff || config.snapTurn > 90 || config.haptics > 100 ||
        !validEyeUpscale(config.upscale) || !validMonitorRate(config.monitorRate) ||
        config.screenSize > 2 || config.screenDepth > 3 || config.bodyHaptics > 100 || config.smoothTurn > 360 ||
        config.hud > 3 || config.flipSpeed < 90 ||
        config.flipSpeed > 480 || !std::isfinite(config.swingSpeed) || config.swingSpeed < 1 ||
        config.swingSpeed > 65 || (config.durationMs && config.durationMs < 2000) || config.durationMs > 25000 ||
        (config.eyeSize && !validEyeSize(config.eyeSize)) || !validRenderScale(config.renderScale) ||
        (config.eyeSize && config.renderScale != 100) || config.weight < 40 || config.weight > 300)
        return 1001;
    // A calibration is both measurements or neither.
    if ((config.eyeHeightMm == 0) != (config.armLengthMm == 0) ||
        (config.eyeHeightMm &&
         (config.eyeHeightMm < body_calibration::minEyeHeightMm || config.eyeHeightMm > body_calibration::maxEyeHeightMm ||
          config.armLengthMm < body_calibration::minArmLengthMm || config.armLengthMm > body_calibration::maxArmLengthMm)))
        return 1001;
    auto module = reinterpret_cast<HMODULE>(config.bridgeModule);
    submitInput = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidySubmit"));
    sampleBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyBridgeSample"));
    stopBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyStop"));
    retargetBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyRetarget"));
    auto rays = reinterpret_cast<HMODULE>(config.rayModule);
    startRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRayStart"));
    submitRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRaySubmit"));
    stopRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRayStop"));
    startSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingStart"));
    submitSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSubmit"));
    sampleSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSample"));
    sampleSlingshot = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySlingshotSample"));
    sampleBends = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingBends"));
    sampleWebFlight = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyWebFlightSample"));
    sampleClimb = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyClimbSample"));
    rayResults = reinterpret_cast<const native_rays::Data*>(GetProcAddress(rays, "SpidyRayData"));
    stopSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingStop"));
    retargetSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingRetarget"));
    // Optional: an older ray module has no web grab, nor punching, nor aim previews.
    sampleGrab = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyGrabSample"));
    sampleTie = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyTieSample"));
    sampleLine = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyLineSample"));
    sampleAim = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyAimSample"));
    startPunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchStart"));
    samplePunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchSample"));
    // Optional too: without them the VR settings' grab, speed and punch
    // switches take effect only at the next session.
    stopPunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchStop"));
    // Optional: an older ray module has no web strike, and a punch only its short pulse.
    sampleStrike = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyStrikeSample"));
    sampleSense = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySenseSample"));
    aimGameThrow = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyThrowAim"));
    soundResults = reinterpret_cast<const game_sound::Data*>(GetProcAddress(rays, "SpidySoundData"));
    soundMode = reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(rays, "SpidySoundMode"));
    soundTrace = reinterpret_cast<const sound_trace::Ring*>(GetProcAddress(rays, "SpidySoundTrace"));
    soundTraceMode = reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(rays, "SpidySoundTraceMode"));
    if (const sound_trace::Ring* expected = soundTrace;
        expected && (expected->magic != 0x53535443 || expected->version != 1 || expected->bytes != sizeof(sound_trace::Ring)))
        soundTrace = nullptr;
    if (!soundTrace)
        soundTraceMode = nullptr;
    if (const game_sound::Data expected; soundResults && (soundResults->magic != expected.magic ||
                                                          soundResults->version != expected.version ||
                                                          soundResults->bytes != expected.bytes))
        soundResults = nullptr;
    dodgeLive = reinterpret_cast<const game_dodge::Live*>(GetProcAddress(rays, "SpidyDodgeLive"));
    if (const game_dodge::Live expected;
        dodgeLive && (dodgeLive->magic != expected.magic || dodgeLive->version != expected.version ||
                      dodgeLive->bytes != expected.bytes))
        dodgeLive = nullptr;
    dodgeResults = reinterpret_cast<const game_dodge::Data*>(GetProcAddress(rays, "SpidyDodgeData"));
    if (const game_dodge::Data expected; dodgeResults && (dodgeResults->magic != expected.magic ||
                                                          dodgeResults->version != expected.version ||
                                                          dodgeResults->bytes != expected.bytes))
        dodgeResults = nullptr;
    swingSettings = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSettings"));
    setSwingPoseCoupling = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingPoseCoupling"));
    swingPointJump = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingPointJump"));
    swingWebTurns = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingWebTurns"));
    // Optional: an older ray module has no web shooter.
    startShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterStart"));
    sampleShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterSample"));
    sampleWarp = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyWarpSample"));
    sampleOffers = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersSample"));
    useOffer = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersUse"));
    aimOffer = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersAim"));
    stopShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterStop"));
    wheelShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterGadgets"));
    if (!submitInput || !sampleBridge || !stopBridge || !retargetBridge || !startRays || !submitRays ||
        !stopRays || !startSwing || !submitSwing || !sampleSwing || !stopSwing || !retargetSwing)
        return 1002;
    stopRequested = false;
    // The worker attaches each player it finds; none yet.
    native_appearance::setPlayerRecord(0);
    keepAliveDeadline = GetTickCount64() + 10000;
    message(1, 0, "Starting OpenXR");
    // The startup stages anew; the worker starts the watchdog with RUN LOGS on.
    startupStages.reset();
    worker = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    if (worker) {
        // The watchdog holds its own handle to the worker and ends with it.
        HANDLE watched{};
        if (DuplicateHandle(GetCurrentProcess(), worker, GetCurrentProcess(), &watched, SYNCHRONIZE, FALSE, 0)) {
            if (HANDLE dog = CreateThread(nullptr, 0, watchdog, watched, 0, nullptr))
                CloseHandle(dog);
            else
                CloseHandle(watched);
        }
    }
    return worker ? 0 : 1003;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrKeepAlive(void*) {
    if (stopRequested)
        return 1102;
    keepAliveDeadline = GetTickCount64() + 5000;
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrStop(void*) {
    std::lock_guard life(lifecycle);
    stopRequested = true;
    if (!worker)
        return 0;
    const auto status = WaitForSingleObject(worker, 15000);
    return status == WAIT_OBJECT_0 ? 0 : 1101;
}
