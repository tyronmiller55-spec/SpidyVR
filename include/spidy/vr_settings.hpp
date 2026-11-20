#pragma once
#include "spidy/na_help.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Spidy's VR settings. The launcher (or the command line) gives a session
// the values it starts with; during play the game's own Settings change
// them: pause, Settings, then one of the VR categories after the game's last tab (game_menu).
// The session reports the last ones, and the launcher starts the next
// session with them. The few no launcher.ini key carries the game module
// keeps in a file of its own (tabOnlyText), which the launcher's options
// change too.
namespace spidy::vr_settings {
// How far the eyes keep from a wall you climb by hand, and from a duct's face
// in its close view, centimetres (GameTrackingRig::wallDistance): fixed since
// WALL CAMERA DISTANCE went (October 24).
inline constexpr int fixedWallDistance = 25;
// Wall-run pose selected by the user; retired tuning keys cannot override it.
inline constexpr int fixedRunTorsoAngle = 15;
inline constexpr int fixedRunCameraDistance = 110;
inline constexpr int fixedMomentumCameraTilt = 30;
inline constexpr int fixedMomentumCameraDistance = 90;
inline constexpr int fixedMomentumBodyTilt = 50;
inline constexpr int fixedMomentumForwardLean = 30;
inline constexpr int fixedMomentumStepLength = 100;
inline constexpr int fixedMomentumStepPace = 100;
struct Values {
    bool aimMarkers = true;
    // Webs catching props and thugs, punching and your own body, like the web
    // shooter below: on unless run_game_vr.py's switches turn them off; the
    // tab and the launcher do not offer them.
    bool webGrab = true, punch = true, body = true;
    float swingSpeed = 32; // the swing's speed limit, m/s
    int snapTurn = 30;     // degrees per flick of the right stick; 0: no snap turning
    int haptics = 100;     // controller vibration, percent
    int screenSize = 1;    // the game screen: 0 small, 1 medium, 2 large
    bool airWebs = true;   // a web that meets nothing within reach holds in open air there
    // With airWebs off, a web that just misses (by up to 8 degrees) catches
    // the nearest real surface instead (Swing::shot; pull request #39).
    bool smartAnchor = false;
    bool webShooter = true; // a free hand's trigger shoots web balls
    // Degrees a second the right stick turns you while held over; 0: it snap
    // turns instead.
    int smoothTurn = 0;
    // How heavy you are while webs fly you (swinging, and after letting go
    // until you land), percent of real gravity: the swing's gravity. 100, real
    // gravity, since October 12; 80 from October 9; before, 60 (the 6 m/s^2
    // Spidy flew at from October 5).
    int weight = 100;
    // The game as it is instead of VR (the tab's VIEW: GAME SCREEN): its own
    // camera on the game screen in the headset, in 3D at GAME SCREEN 3D's
    // depth, played as on an Xbox controller, with nothing of Spidy's on the
    // hero. Each session starts in VR: the launcher does not keep it. Until
    // October 12 a click of both thumbsticks switched it; until October 15
    // it showed the game flat, played with a real controller.
    bool flatScreen = false;
    // A T-pose calibration of your body is wanted at the next gameplay (the
    // tab's CALIBRATE BODY: ON RESUME); the XR worker clears it once the
    // calibration starts (body_calibration.hpp).
    bool calibrate = false;
    // Experimental, off unless chosen: airborne basic flips use the right
    // stick, release holds the angle, and A returns upright (FlipMotion).
    // REALISTIC FLIPS retains its own controls and optional left-stick steering.
    bool flips = false;
    bool controlledFlips = false; // airborne A toggles left-stick flip mode
    // The web button (the tab's WEB BUTTON): false the grip, which shoots and
    // holds a hand's web while the trigger reels it in and shoots web balls;
    // true the trigger, the two swapped (GameTrackingRig::triggerWebs).
    bool triggerWebs = false;
    // The game's HUD in the headset (the tab's HUD): 0 off, 1 small, 2 medium,
    // 3 large (native_hud::widths).
    int hud = 2;
    // How fast a flip turns you at full tilt of the left stick, degrees a
    // second (the tab's FLIP SPEED; FlipMotion::speed): 180 since October 9,
    // a quarter slower than the flips' first 240.
    int flipSpeed = 180;
    // Walking a wall or stopped on it, your view turns so that the wall is
    // your floor (GameTrackingRig::standOnWalls); off, you stay upright
    // beside it. On unless run_game_vr.py's --no-stand-on-walls turns it off:
    // the tab and the launcher do not offer it (since October 15).
    bool standOnWalls = true;
    // Experimental physical hand pulls, solid palms and crawling body poses.
    bool handClimb = false;
    bool halfPunchDamage = false; // half health damage, unchanged punch reactions
    // ENEMY TYPES (the launcher's Enemy types; game_guard): the enemies that block in the base game (batons,
    // shields, brutes and swordsmen) block the base attacks (punches and web strikes) until they are webbed
    // or stunned. Off by default: nobody guards, as before Enemy types. First person cutscenes and half punch
    // damage are other settings and do not change with it.
    bool enemyTypes = false;
    // The web tie (the tab's WEB TIE; web_tie.hpp, docs/WEB-TIE.md): a webbed-up
    // enemy stuck to a wall by the hands together and the right stick's click,
    // and a webbed enemy who comes to the player only on the yank
    // (GrabConfig::yankOnlyCharacters). Off by default: both as before.
    bool webTie = false;
    // Web lines (the tab's WEB LINES; web_line.hpp, docs/WEB-LINE.md): a taut
    // web between two surfaces by the hands together and the right stick's
    // click, zipped to, walked on, thugs tied under it. Off by default.
    bool webLines = false;
    // The wall-floor camera transition during HAND WALL CLIMBING's joystick
    // runs (the tab's JOYSTICK WALL RUN CAMERA), as before pull request #38;
    // off, the left stick drives the momentum wall run's view and gait.
    bool joystickRunCamera = true;
    bool wallRunCameraTilt = true; // optional horizontal momentum-run camera bank
    // Scenes the game plays on the game screen (cutscenes, finishers) have
    // depth there (the tab's GAME SCREEN 3D): 0 off, the game's flat frame; 1
    // low, 2 medium, 3 high (screenDepths).
    int screenDepth = 2;
    // Arcade webs (the tab's WEB STYLE: ARCADE; SwingConfig::arcade): webs go
    // through lamp posts, signs, cars and other thin or loose things, and
    // swings stay above the street. Off, REALISTIC: webs are ropes that bend
    // round whatever comes between you and where they stick.
    bool arcadeWebs = false;
    // How near the eyes the headset still shows things, millimetres (the
    // tab's NEAR CLIP, nearClips; near_clip.hpp). The game's own camera clips
    // at 100, which cut off hands held up before the face.
    int nearClipMm = 20;
    // The monitor's view in immersive VR, degrees across (the tab's RECORDING
    // FOV; recording_fov.hpp), 0 for off: the window then mirrors the left
    // eye. An angle renders the game's own view again for the window, which
    // costs frame time and leaves Spidy's drawings out of the window.
    int recordingFov = 0;
    // Realistic flips, experimental (the tab's REALISTIC FLIPS and the ten
    // rows under it; RealisticFlipSettings, which holds what each means): the
    // swing's flight and the arms turn the player in the air, and A there
    // rights them. automaticCombat: FLIPS IN COMBAT, off keeps them out of
    // fights (the player, October 24: "lets add a toggle to disable realistic
    // flips during combat").
    bool realisticFlips = false, automaticReleaseFlip = true;
    int automaticStrength = 50, automaticSpeed = 240, automaticMinimumSpeed = 3;
    int automaticResponse = 500, automaticHandStrength = 100, automaticFalloff = 3000;
    int automaticHandThreshold = 100;
    bool automaticAirBrake = true, automaticCombat = true;
    // HANDSTAND, phase H1 (handstand.hpp; experimental; ON by default from the
    // hybrid-v2 playtest build, the user Nov 13 t86u "Let's move on handstands";
    // handstand=0 turns it off; REALISTIC FLIPS only): come down inverted with both palms on a floor
    // and the hands catch and balance him; a push off them launches, lifting
    // them drops him to his feet. spidy_vr.ini only (handstand* keys, the
    // page's H1 checklist step 9): the balance's assist (DCM steering), the
    // push's gain (percent of the palms' speed) and its cap (m/s), the lean
    // that is a fall (degrees); and the centre of mass's height over the
    // palms (cm), the assist's gain (percent) and how far the inverted eyes
    // come down (cm).
    //
    // THE BALANCE PASS (Nov 14, t67u: "a lot more balance. Make it hard to fall out of it, but easy to leap out of."; the
    // movement-defaults revision 3 moves a saved profile still on the old values): the fall angle 35 -> 50 degrees, the
    // assist's gain 300% -> 500%, and four new ones: the support's radius around the palms (cm; was a fixed 12), how long
    // you can be off balance before you fall (ms; was a fixed 120), the damping of the sway (percent of 1/s; was none) and
    // the leap out (A, the stick held hard or a push leaves at once in a jump along your view).
    bool handstand = true, handstandAssist = true, handstandLeap = true;
    int handstandPushGain = 220, handstandPushMax = 7, handstandFallDegrees = 50;
    int handstandHeightCm = 95, handstandGainPct = 500, handstandEyeDropCm = 90;
    int handstandSupportCm = 18, handstandGraceMs = 500, handstandDampingPct = 250;
    // How strongly a bHaptics vest, sleeves and visor play (the tab's BODY
    // HAPTICS; body_haptics.hpp), percent; 0 off. And which of its groups
    // play (the tab's WEB, WIND, COMBAT and MOVEMENT HAPTICS switches;
    // body_haptics::Group): the player, October 20, after the first session
    // on the devkit, asked for "one switch per effect group".
    int bodyHaptics = 100;
    bool hapticWebs = true, hapticWind = true, hapticCombat = true, hapticMovement = true;
    // How Spidy's own marks in the headset look (the tab's UI STYLE;
    // ui_style.hpp's UiStyle): 0 comic, printed as Spidy's videos are; 1
    // tech, as the game's own HUD and menus are.
    int uiStyle = 0;
    // What Spidy's blows sound like (the tab's BLOW SOUNDS; game_sound.hpp):
    // 0 the game's own melee impacts, 1 Spidy's own tones (impact_sound.hpp),
    // as before October 22, 2 none. Players asked for both (October 13: "pls
    // an option to remove the sfx when punching", 12 reactions).
    int blowSounds = 0;
    bool webSlingshot = false; // two fixed webs drawn from a standstill, then released
    bool betterClimbing = false; // retain hand-over-hand progress along a shared web
    // Your height, centimetres (the tab's HEIGHT; seated.hpp): 0 AUTO, the
    // T-pose calibration's or the headset's; set, it sizes Spider-Man in their
    // place and is the height SEATED raises you to. And seated play (the tab's
    // SEATED): the view and the body rise from where you sit to that height.
    int heightCm = 0;
    bool seated = false;
    bool combatVisuals = true; // combat overlays and automatic combat slow-motion VFX
    // A fast hand closes into a fist with nobody to punch too (the tab's AIR
    // FISTS; hand_pose.hpp). Off: only with an enemy marked or within reach.
    bool airFists = true;
    // Aiming with the eyes (the tab's EYE AIM; eyeAimShots, eyeAimZips): web
    // balls go where the eyes look, from the hand that pulled; a zip goes to
    // the point the eyes look at, the game's camera being turned onto it.
    // Only where the headset tracks the eyes (XrRuntime::eyeTracking); 0, or
    // no eye tracker, the hands aim balls and the head zips as before.
    int eyeAim = 0;
    // How far webs stretch under load (the tab's ELASTIC WEB): 0 off, a hard
    // rope as before; 1 to 3 softer and softer (Swing::elasticWeb; pull
    // request #40).
    int elasticWeb = 0;
    // How long a loaded web's last pull fades after you let go (the tab's
    // SOFT RELEASE): 0 at once, as before; 1 to 3 for 120, 180 or 240 ms
    // (Swing::softRelease; pull request #41).
    int softRelease = 0;
    // While a web carries you and for a moment after, speed into what is
    // ahead is cut so you do not slam into it; your lane stays yours (the
    // tab's SWING ASSIST; Swing::allowSwingAssist; pull request #42).
    bool swingAssist = false;
    // How fast WEB SLINGSHOT launches you at full draw, m/s (the tab's
    // SLINGSHOT SPEED): 0, its launch as before, under the swing's speed
    // limit; 20 to 300 its own (Swing::setSlingshotSpeed; pull request #45).
    int slingshotSpeed = 0;
    // How smoothly RECORDING FOV's window view turns with the head (the tab's
    // RECORDING SMOOTHING): 0 off; 10 to 100, slower and slower. The
    // headset's view is never smoothed (pull request #46).
    int recordingSmoothing = 0;
    // Hand gestures on webs (pull request #47): TWO HAND WEB ZIP, both webs
    // on one spot and both arms pulled back fast launch you at it; HAND ROPE
    // STEERING, the free hand on your web too pulls sideways to turn the
    // swing (Swing::allowGestures).
    bool twoHandZip = false, ropeSteering = false;
    // How far a web reaches, open-air webs too, m (the tab's WEB RANGE; 100
    // as before), and how fast it flies there (WEB SPEED): 0 it sticks at
    // once, as before; 1 to 3 it flies at 60, 150 or 350 m/s, pulls only
    // once it has arrived and catches softly (Swing::webTravel; pull
    // request #48).
    int webRange = 100, webSpeed = 0;
    // The pair the headset shows turned by the player's own turn since it was
    // rendered (the tab's FLIP REPROJECTION; flip_reprojection.hpp): flips,
    // spins and smooth turns as steady as head turns. Off by default, to
    // compare in the headset.
    bool flipReprojection = false;
    // The HUD as a layer of its own that the headset shows at its own rate
    // (the tab's STEADY HUD; steady_hud.hpp), instead of drawn into each eye
    // pair by the game: it holds still when the headset repeats a pair. Off
    // by default, to compare in the headset.
    bool steadyHud = false;
    // The reel felt in the hand (the tab's REEL HAPTICS; reel_haptics.hpp):
    // the reeling hand's controller ticks like a winch. Off by default.
    bool reelHaptics = false;
    // The swing felt in the hand (the tab's SWING HAPTICS; swing_haptics.hpp):
    // a taut web's hand ticks with the body's travel, fainter than the reel.
    // Off by default.
    bool swingHaptics = false;
    // The phone's MUSIC plays what the PC plays (the tab's PC MEDIA;
    // phone_pc_media.hpp): Spotify, a browser. Off by default: the Music folder.
    bool pcMedia = false;
    // NATIVE RUN (the tab's NATIVE RUN; game_pad::NativeRun): on the ground,
    // the left stick past 70% holds the game's RT, its parkour run - sprint,
    // vaults, wall runs - until it falls under 50%. Off by default: the stick
    // jogs as before. Never in the air, on a web or on a wall.
    bool nativeRun = true;
    // AIR SLINGSHOT (the tab's AIR SLINGSHOT; Swing::allowAirSlingshot): the
    // slingshot's draw in the air too - both webs under tension, both grips
    // held and both arms drawn until the hands buzz, then let go: a launch
    // along the two webs. Off by default: in the air a release only lets go.
    bool airSlingshot = false;
    // WEB ANGLE, degrees (the tab's WEB ANGLE; web_aim.hpp): a hand's webs
    // tilt up (positive) or down from the controller's line, -45 to 45. And
    // WRIST SHOOTERS (the tab's WRIST SHOOTERS; redefined in v6): they leave
    // parallel to the forearm, out past the wrist toward the fingers, as the
    // comics' and the films' shooters do: along the body's forearm when the
    // body can say (as MODEL FOREARM), else along the controller's line. The
    // old palm-side quarter turn is PALM SHOOTERS. 0 and off by default:
    // along the controller, as before. The fists keep the controller's.
    int webAngle = 0;
    bool wristShooters = false;
    // WEB AIM (the tab's WEB AIM; web_aim::Source): 0 CONTROLLER, the webs
    // along the controller as before; 1 MODEL FOREARM, along the forearm of
    // the body the player sees (web_aim::ForearmAim), back to the controller's
    // line whenever the body cannot say. WEB ANGLE and WRIST SHOOTERS turn on
    // top of either. CONTROLLER by default. And FOREARM BLEND, percent (the
    // tab's FOREARM BLEND): how much of the forearm's line MODEL FOREARM takes,
    // 0 (the controller's) to 100 (the forearm's), 100 by default.
    int webAimSource = 0;
    int forearmBlend = 100;
    // DODGE TUMBLE (the tab's DODGE TUMBLE; dodge_tumble.hpp): 0 OFF, 1 LEAN,
    // 2 ROLL. The view leans or rolls with a dodge of the game's, so it reads
    // as one. OFF by default: the view stays level, as before.
    int dodgeTumble = 0;
    // RUN CURVE (the tab's RUN CURVE; run_update::curveStick): on the ground
    // the stick reaches the game's full run at 80% of its tilt, more of it
    // early. On by default since the November 12 movement cleanup (the user:
    // "movement fixes will be defaults"); off: the stick as it is.
    bool runCurve = true;
    // NATIVE RUN AT, percent of the stick's tilt (the tab's NATIVE RUN AT;
    // run_update::nativeRunThresholds): past it NATIVE RUN holds the game's
    // RT, and lets go 20 points under it. 70 by default (70 and 50, as before).
    // Always the physical stick's tilt: with RUN CURVE on the thresholds are
    // carried through the curve, so the sprint starts where it always did.
    int nativeRunPercent = 70;
    // RUN LOGS (the tab's RUN LOGS; run_update::RunLog): lines in the
    // session's timeline about running, landing and the mod's ground run, to
    // send back. Off by default. Nothing moves differently either way.
    bool runLogs = false;
    // RUNNING LEAP (the tab's RUNNING LEAP; Swing::allowMomentumLeap,
    // run_update::leapVelocity): a charged leap keeps the run's speed along
    // its way and adds the charge on top; a long one from a fast run goes
    // further and flatter. Off by default: a leap starts from rest, as before.
    bool momentumLeap = false;
    // EARLY JUMP (the tab's EARLY JUMP; game_pad::ChargeJump::earlyJump): 0
    // OFF, a tap of A jumps as it is let go within 0.18 s, as before; 1 QUICK
    // TAP, within 0.1 s; 2 RUNNING, as QUICK TAP, and while NATIVE RUN holds
    // RT the game's A goes out as it is pressed (no charge while running).
    // QUICK TAP by default since the November 12 movement cleanup.
    int earlyJump = 1;
    // MOVEMENT DEFAULTS (November 12 movement cleanup; withMovementDefaults):
    // the revision of the movement defaults a settings file has seen. A file
    // from before it gets RUN CURVE on and EARLY JUMP QUICK TAP once (the
    // user: "movement fixes will be defaults"); after that the player's own
    // choice stands. Not a row. movement_defaults.
    int movementDefaults = 16; // movementDefaultsRevision: a new file has seen them
    // SOFT LANDING (the tab's SOFT LANDING; Swing::allowSoftLanding,
    // run_update::softLandVertical): a fall the swing flies, with no web
    // holding, eases to 9 m/s down over its last 3 m (or 0.35 s), its speed
    // across kept. Off by default: it lands at the speed it falls, as before.
    bool softLanding = false;
    // SPIDY RUN (the tab's SPIDY RUN; Swing::allowGroundRun, run_update::
    // groundRunStep): a landing faster than the game's sprint is the mod's
    // ground run, the wall run's momentum on the ground - its speed kept, the
    // stick steering it - easing to the sprint and handed back to the game.
    // Off by default: the game takes every landing, as before.
    bool spidyRun = true;
    // SPIDY RUN FROM STANDING (the tab's RUN FROM STANDING; Swing::
    // allowGroundRunFromStanding, run_update::standRunStep): with SPIDY RUN
    // on, the stick held fully on the ground for 0.3 s starts the mod's run
    // from a stand or a walk, and it builds past the sprint while held. Off
    // by default: SPIDY RUN starts only from a fast landing, as before.
    bool spidyRunFromStanding = false;
    // SMOOTH WEB LANDING (the tab's SMOOTH WEB LANDING; Swing::
    // allowWebLandingSmooth, web_landing.hpp): a landing with a web held,
    // slower than WEB LANDING IMPACT, eases its fall over the last metres,
    // keeps its speed across the ground for a moment (decaying to a jog) and
    // no longer flips between the game and the swing on a flicker of the
    // ground contact. Off by default: such a landing is the game's at once,
    // as before.
    bool webLandingSmooth = false;
    // WEB LANDING IMPACT, m/s (the tab's WEB LANDING IMPACT): SMOOTH WEB
    // LANDING's threshold; a landing this fast or faster is the game's, heavy
    // as before. 24 by default, 12 to 40.
    int webLandingImpact = 24;
    // STEADY STATES (the tab's STEADY STATES; ground_state.hpp): NATIVE
    // RUN's RT comes back only after everything has allowed it for 0.15 s
    // (a frame's flicker of the ground, a prompt or a vent no longer drops
    // and raises it), and stays off in a fight. Off by default: as before.
    bool steadyStates = false;
    // COMBAT RESPONSIVE (the tab's COMBAT RESPONSIVE; v5): in a fight (the
    // combat camera, or an enemy in reach within 3 s) the swing starts no
    // SPIDY RUN or landing carry and ends one going on, so the game moves
    // the body for its blows and dodges; NATIVE RUN holds no RT; and A is
    // the game's jump at once, no charge. Off by default: as before.
    bool combatResponsive = false;
    // SPIDY RUN JUMP (the tab's SPIDY RUN JUMP; v6; Swing Input::runJump):
    // A while the swing runs the body on the ground (SPIDY RUN, SMOOTH WEB
    // LANDING's carry) with no web is the swing's own jump at once, the
    // run's speed kept. Off: v5's JUMP ALWAYS (the run ends and the game's A
    // waits up to 0.2 s for the game to have the body back). On by default.
    bool spidyRunJump = true;
    // FOREARM AIM LIMIT, degrees (the tab's FOREARM AIM LIMIT; v6): the most
    // MODEL FOREARM (and WRIST SHOOTERS) turns the webs off the controller's
    // line onto the forearm. 180 (no limit) by default; v3-v5 had 35.
    int forearmAimLimit = 180;
    // PALM SHOOTERS (the tab's PALM SHOOTERS; v6): the old WRIST SHOOTERS of
    // v2-v5, a quarter turn out of the palm side, square to the hand. Off by
    // default. WRIST SHOOTERS now shoots parallel to the forearm.
    bool palmShooters = false;
    // HEAD PROBE (the tab's HEAD PROBE; v7): a read-only diagnostic for the
    // head lock to come - "head probe:" lines with the hero's head bone
    // against the headset, per state, in the session's diagnostics
    // (head_probe.hpp). Changes nothing in play. Off by default.
    bool headProbe = false;
    // Native Animation community defaults (saved values always take precedence).
    // ANIMATED MOVES: when on, the game's
    // animation drives the body for ground parkour / vaults / mantles
    // (NATIVE RUN). Swing / web / zip / wall-run stay with the mod. On in this package
    // (animated_moves.hpp).
    bool animatedMoves = true;
    // ANIMATED HEAD (the tab's ANIMATED HEAD; Off|Level|Full, default Full):
    // while a pose yield is active, attach the view to the hero's gameplay head
    // like finishers (anchored 4 Level / 2 Full). Full keeps anim pitch/roll
    // in air too; Level only when chosen (animated_moves.hpp).
    int animatedHead = 2;
    // SMOOTHING (the tab's SMOOTHING; Off|Low|Medium|High|Max, default Off):
    // ease the view toward the headset's look. Translation stays current, so
    // leaning and travel do not trail. Not a Settings / Command field.
    int animatedHeadsetSmooth = 0;
    // CAMERA SMOOTHING (the user, November 10 20:54: "Give me settings for
    // vertical, horizontal, and camera angle smoothing, and give a toggle that
    // applies a heavy stabilization and smoothing to the motion, yet still uses
    // the settings. Two levels, Heavy and Light"). While the animated head
    // carries the view (ANIMATED HEAD), the head the eyes sit on is smoothed:
    // VERTICAL and HORIZONTAL its travel relative to the feet (the bob; the
    // feet themselves, and so where he is, stay exact), ANGLE its turning. The
    // headset's own look and lean stay 1:1 (headset_smoothing.hpp). Response
    // times in milliseconds, 0 off. STABILIZATION picks the bank: OFF the
    // three CAMERA rows, LIGHT the three LIGHT rows, HEAVY the three HEAVY
    // rows (heavier speed damping too), STEADY (3) the three STEADY rows. Off and 0 by default: as before.
    int cameraStabilization = 0;
    int cameraVerticalMs = 0, cameraHorizontalMs = 0, cameraAngleMs = 0;
    int lightVerticalMs = 75, lightHorizontalMs = 50, lightAngleMs = 75;
    int heavyVerticalMs = 200, heavyHorizontalMs = 150, heavyAngleMs = 200;
    // 360 DEGREE SPHERICAL AIM (spidy_vr.ini spherical_aim; spherical_aim.hpp):
    // with WRIST SHOOTERS or MODEL FOREARM, a contorted arm (over or behind
    // the head, down by the legs) aims along the forearm learned on the
    // controller, not the body solve's guess. On by default; 0 is as before.
    // Not on the menu.
    bool sphericalAim = true;
    // LEG CONTROL (ADVANCED; leg_control.hpp): with CONTROLLED FLIPS on and
    // the body airborne or swinging free, the right stick poses the legs
    // (centre straight, forward jackknife, left/right splays, back a rearward
    // splay) and they swing loose with the body's motion and turn it back
    // (angular momentum). A double tap up zips, down heals. Off by default.
    // LEG STIFFNESS, LEG DAMPING: percent of the springs' defaults (1.6 Hz,
    // 0.35); LEG COUPLING: percent of the legs' full turn on the body.
    // leg_double_tap_ms (spidy_vr.ini only): the double tap's window; unused
    // since November 12 (engaged legs never zip or heal), kept so old files
    // read the same. November 12 (t67u, leg_hold.hpp): the legs engage while
    // Y+X are held and stay while the right stick is moved; neutral ends
    // them. No setting (the thumb rest tap and leg_toggle_input are gone; an
    // old leg_toggle_input line is ignored).
    bool legControl = false;
    int legStiffnessPct = 100, legDampingPct = 100, legCouplingPct = 50;
    int legDoubleTapMs = 300;
    // RUN SPEED, percent (the tab's RUN SPEED; Swing::setRunSpeedPercent):
    // scales the speed SpidyVR's OWN ground run (SPIDY RUN and RUN FROM
    // STANDING) puts the body at, 50 to 200. The game's native run/sprint is
    // its own animation and mover: not changed. 100 by default (as before).
    int runSpeedPercent = 100;
    // SLOW-MO LENGTH, seconds (the tab's SLOW-MO LENGTH; SlowMotion::
    // setDrainSeconds): how long a full Focus meter of L3 slow motion lasts,
    // 3 to 90; slowMotionEndless (91) lasts until L3 toggles it off. 12 by
    // default (as before). Refill time is unchanged.
    int slowMotionSeconds = 12;
    // SOFT LOCK, metres (the tab's SOFT LOCK; GrabConfig::softLock): a grip
    // web aimed near an enemy or a throwable prop within this distance snaps
    // to it (marker and web alike); the closer and the nearer the ray, the
    // better. 0 is off; 0 to 40. 15 by default.
    int softLockMetres = 15;
    // ANIMATED AIR (the tab's ANIMATED AIR; off by default): pose yield for
    // freefall / swing / web-hold air. Physics stays with the mod; no cutscene
    // worker. Off leaves air body with the VR IK (animated_moves.hpp).
    bool animatedAir = false;
    // ANIMATED STEERING (tab; off by default): while a pose yield is active,
    // left-stick magnitude runs the hero along his actor facing (read-only
    // transform). MODEL YAW uses the separate existing walk-facing path.
    bool animatedSteering = false;
    // ANIMATED ARMS: controller arm overlay while yielding (animated_arms.hpp).
    bool animatedArms = true;
    // ANIMATED FULL ARMS (spidy_vr.ini animated_full_arms; the user, Nov 9
    // 22:27): while the animation owns, the arms keep every SpidyVR control -
    // punches open web strikes that pull him out of the animation (lunge /
    // teleport) - instead of the contact-only policy. On by default; 0 brings
    // back contact-only arms. Not on the menu.
    bool animatedFullArms = true;
    // ANIMATED TORSO: with ANIMATED ARMS, bound waist→chest toward controller
    // reaches (animated_torso.hpp). On by default; bounded limits.
    bool animatedTorso = true;
    // ANIMATED MODEL YAW: while yielding, right-stick X turns the hero through
    // the game's left-stick walk-facing path (animated_model_yaw.hpp). Not a
    // facing RVA write; not CameraLook. On by default.
    bool animatedModelYaw = true;
    // ANIMATED ONLY: with MOVES/AIR, every gameplay state keeps the pose yield
    // (wall / zip / combat too) — the SpidyVR body never reclaims mid-play, so
    // no adoptFeet / head-follow re-anchor jumps (animated_moves.hpp). Off by
    // default.
    bool animatedOnly = false;
    // ANIMATED AIR SPIN (Off|Flips|Flips + momentum, default Off): while
    // yielding airborne, the flip tilt (and optionally a travel lean/bank) turns
    // the animated skeleton about its head (animated_air_spin.hpp). Visual/view
    // only; physics untouched.
    int animatedAirSpin = 0;
    // ANIMATED HYBRID (on by default): game anims own the body by default
    // (MOVES), head locked to the hero; OG SpidyVR only on player intent (web,
    // mid-air turn, airborne speed, Spidy wall run). Forces AIR / ONLY / AIR SPIN
    // / midair softlock inactive (hybrid_owner.hpp).
    bool animatedHybrid = true;
    // ANIMATED HYBRID SPEED (with HYBRID; unused without it): how long airborne
    // measured horizontal speed must stay at or above the takeover speed
    // (hybrid_owner::vrSpeed) before OG SpidyVR takes over, ms; 0 = off (speed
    // never forces a takeover; webs / mid-air turns / wall runs still do).
    // 5..500, 200 by default.
    int animatedHybridSpeedMs = 200;
    // How much speed a fast one-hand web yank adds, percent (the tab's SINGLE
    // HAND YANK STRENGTH; Swing::singleHandYankStrength): 100 as before.
    int singleHandYankPercent = 100;
    // CAMERA MOTION (the user, November 11 22:59: "Native Animation is moving
    // too much for players, and leveling steals all of the feeling. We need to
    // let the player dial that stuff in"; camera_motion.hpp). While the
    // animated head carries the view (ANIMATED HEAD FULL or LEVEL), how much
    // of the animation's own camera motion reaches the eyes, percent: PITCH
    // and ROLL its tilt (100 all of it, as before; 0 level, like HEAD LEVEL
    // but keeping the animation's heading), BOB UP/DOWN and BOB SIDE its
    // head bob relative to the feet (the feet themselves stay exact). HEAD
    // ANGLE (the user, November 11 23:02: "a setting for head angle, from
    // below to above the horizon"): degrees the animated view is tilted
    // above (positive) or below the horizon. The headset's own look and lean
    // stay 1:1 on top. The CAMERA MOTION row is a preset over the four
    // percentages (no key of its own). 100/100/100/100 and 0 by default: as
    // before.
    int cameraPitchPct = 100, cameraRollPct = 100, cameraBobVerticalPct = 100, cameraBobSidePct = 100;
    int cameraHeadAngleDeg = 0;
    // PUNCH VIEW EASE (view_ease.hpp; the user, November 12 01:13: "hold and
    // ease"): at a punch's ANIM -> VR switch the view stays where the last
    // animated frame showed it and eases onto the position lock over this
    // many ms (ease-out, never slower than a motion-masked bleed). 0: the old
    // snap. 250 by default. punch_view_ease_ms.
    int punchViewEaseMs = 250;
    // MOVE HAPTICS (move_haptics.hpp; the user, November 12 04:56: "haptic
    // feedback for all jumps and landings as well, scaled accordingly"): the
    // controllers' pulses for jumps, wall jumps, zip launches, point jumps (a
    // double thump) and landings, 0..200 percent. Its own strength: CONTROLLER
    // VIBRATION does not scale these. 0 turns them off. move_haptics_pct.
    int moveHapticsPct = 100;
    // VIEW YAW SOURCE (stable_yaw.hpp; the user, November 12: "running sometimes
    // gets stuck with Spiderman running in a circle"): where HYBRID's view-
    // relative steering takes its heading. 1 STABLE (default): a world heading
    // set when the left stick is pushed (or at the handoff), then moved only by
    // the headset's own yaw and the right stick's turn, never by the hero's head
    // bone, so it cannot chase the hero it steers. 0 BODY (OLD): the hero's head
    // bone plus the headset's turn, as before. view_yaw_source.
    int viewYawSource = 1;
    // SOUND TRACE (game_sound.hpp, sound_trace.hpp; the user, November 12: yes
    // to the trace): diagnostics only. Logs the ids of the game's sounds
    // played around takeoffs, landings and point jumps. Plays nothing,
    // changes nothing. Off by default since the beta (decision D7, movement_defaults 13); the hook it uses stays
    // installed either way (it guards the hero cues against double plays). sound_trace.
    bool soundTrace = false;
    // LOG LEVEL (beta polish G4; ini log_level, no menu row): how often the diagnostics repeat their noisiest
    // unchanged lines (diag_log::Throttle::level): 0 QUIET (30 s), 1 (default, 5 s, the head probe and aim family
    // included), 2 VERBOSE (2 s, as before the beta). A changed state is always written at once.
    int logLevel = 1;
    // JUMP TO SPIDYVR (hybrid_owner.hpp; the user, November 12 05:07/05:41:
    // plain jumps, zip launches, vaults and wall launches hand off to SpidyVR
    // after a timer): ms into a midair animation's air; 0 STAY TO LANDING (the
    // November 11 23:39 rule). 500 by default. jump_handoff_ms.
    int jumpHandoffMs = 250; // 300 until the handoff tune (Nov 18)
    // AIR VIEW EASE (view_ease.hpp, the punch's hold and ease): the timed
    // jump handoff, a flip break and the grip cancel hold the view where the
    // animation showed it and ease it onto the feet over this; 0 OFF (SNAP),
    // the old snap. 250 by default. air_view_ease_ms.
    int airViewEaseMs = 200; // 250 until the handoff tune (Nov 18)
    // ANIM YAW SWAY (view_yaw_lock.hpp, rotation desync Step B): with HEAD
    // LOCK FULL and VIEW YAW SOURCE STABLE the eyes' yaw is the stable heading
    // plus this share of the head bone's yaw sway; 100 is the old feel (the eyes
    // on the head bone). 20 by default. anim_yaw_sway_pct.
    int animYawSwayPct = 20;
    // TURN-IN-PLACE ANGLE (view_yaw_lock.hpp): standing, the hero turns to the
    // heading once it is this far from his facing (degrees); 0 OFF. 35 by
    // default. turn_in_place_deg.
    int turnInPlaceDeg = 35;
    // MOVEMENT MODE (hybrid_movement.hpp; the user, November 12 t75u): 0 HYBRID
    // (default: the view locked to the hero's head, yaw included; the left stick's
    // front half runs along the right stick's heading, its rear half snaps hero and
    // view to the stick), 1 STABLE (Step B: full lock on the stable heading), 2 OLD
    // (head-bone steering, VIEW YAW SOURCE BODY). movement_mode.
    int movementMode = 0;
    // REAR SNAP TURN (HYBRID): how long the view takes for the rear snap, ms; 0
    // INSTANT (a frame switch). 200 by default. rear_snap_turn_ms.
    // v2 (t86u "do a head rotation instead of a frame switch"): eased by default; the
    // ini key was renamed from rear_snap_ms so 5061b4a's saved INSTANT does not stick.
    int rearSnapMs = 200;
    // TORSO UPDATE (Nov 13): the most the animation's arm overlay turns the chest toward the player, degrees
    // (0 off, 40 the old cap). animated_torso_twist.
    int animatedTorsoTwist = 80;
    // TORSO UPDATE (Nov 13): in the animation the web line falls back to the controller's once the forearm
    // is this many degrees off it (0 off). anim_forearm_fallback_deg.
    int animForearmFallbackDeg = 60;
    // STEADY (November 14): STABILIZATION's fourth bank, above HEAVY (heavier speed damping again), ms.
    // Polish pass: 200/200/200 (was 350/300/450), so the view answers the head sooner; the bank's speed
    // terms went up with them (headset_smoothing.hpp bankAngleBeta / bankPositionBeta).
    int steadyVerticalMs = 200, steadyHorizontalMs = 200, steadyAngleMs = 200;
    // HORIZON LEVEL (November 14): this share of the view's roll and pitch tilt removed, in the animation and
    // in SpidyVR; flips pass whole. camera_horizon_level_pct.
    int cameraHorizonLevelPct = 0;
    // VR STABILIZATION (November 14): 1 applies the CAMERA rows, HORIZON LEVEL and the bank's ANGLE to
    // SpidyVR's own view tilt (swing, air, combat, wall); 0 leaves it as before. camera_vr_stabilization.
    int cameraVrStabilization = 1;
    // MANUAL SWING KICK (K1, swing_kick.hpp; November 14), EXPERIMENTAL: ON by default (the owner, Nov 19 6:06 AM:
    // "Yes, manual swing kicks on by default"; movement_defaults revision 12 turns it on once). Swinging with the
    // legs engaged (Y + X), a flick of the right stick up kicks at the ringed enemy within 30 degrees of
    // the swing's velocity. swing_kick_manual. And the auto dive (WebStrike::dive, today's) as its own
    // setting, on as before: swing_kick_auto. Both can be on; the dive holds back during a manual kick.
    bool swingKickManual = true, swingKickAuto = true;
    // LEG HOLD EVERYWHERE (EngineBot's K1 series, Nov 19 port; ini leg_hold_everywhere, no menu row):
    // the Y+X chord may start wherever LEG CONTROL is usable, not only where the legs can move. OFF by
    // default (the owner, Nov 19 t127u): on, a lone X or Y on the ground waits up to 80 ms for its partner,
    // which would hold back the X throw.
    bool legHoldEverywhere = false;
    // SWING KICK GRUNT (ini swing_kick_grunt; off by default): Spidey's own jump grunt plays as a manual swing
    // kick arms (from EngineBot's swing kick series).
    bool swingKickGrunt = false;
    // GRIP ACROSS THE GAME (G1, grip.hpp; November 14), EXPERIMENTAL: on by default (the owner, Nov 19,
    // "leave all features on for further testing"; movement_defaults revision 14 turns an old off once). wall_brake: on
    // Spidy's wall a grip closed on a slide slows it as friction does (mu grip_stop_pct, 200: the Nov 14 pass; it was wall_brake_mu_pct, 90, and files that saved that key keep their old default behind it), then holds.
    // grip_fling: a pull let go of while gripping a surface launches the body the other way (gain
    // grip_fling_gain_pct, 130; at most grip_fling_max m/s, 9), the ground included.
    bool wallBrake = true, gripFling = true;
    int wallBrakeMuPct = 200, gripFlingGainPct = 130, gripFlingMax = 9;
    // HAND GRAB (G2, hand_grab.hpp; November 14), EXPERIMENTAL: on by default (the owner, Nov 19; revision 14).
    // hand_grab: a grip squeezed
    // with the palm within 35 cm of an enemy or a prop takes it by hand: held 45 cm ahead of the palm,
    // punched with the free fist, thrown or slammed on letting go. hand_grab_force: one hand's force in
    // newtons (1200; 90 kg lifted). enemy_hold_cost: how fast holding an enemy tires the hand (1; 0 never).
    bool handGrab = true;
    int handGrabForce = 1200, enemyHoldCost = 1;
    // hand_grab_class_rules (G2b, hand_grab::Foe; on; the launcher's HAND GRAB CLASS RULES row, KICKS & COMBAT): heavies (brutes, big
    // capsules) and bosses resist throws unless open for a finisher (stunned or webbed up), shield guards and
    // bosses tire the hand 3x, and an open enemy costs nothing and is fully manipulated. 0: G2 as before.
    bool handGrabClassRules = true;
    // AIR KICKS (K2, air_kick.hpp; November 14), EXPERIMENTAL: on by default (the owner, Nov 19; revision 14).
    // kicks: off the ground with the
    // legs engaged (Y + X), a quick flick of the right stick kicks: up a two-leg dive, up-left or up-right a
    // one-leg dive, a quarter circle from a side to up a roundhouse, any other flick a freeform blow.
    // kick_gaze_cone_deg: how far from your gaze a zone point can be and still be aimed at (25).
    // kick_spin_pct: how much of the kick's spin reaction the legs' coupling keeps (30).
    bool kicks = true;
    int kickGazeConeDeg = 40, kickSpinPct = 30; // cone 25 until the 2 AM log (Nov 18): 25 of 25 kicks found no enemy
    // GROUND KICKS (K3, ground_kick.hpp; November 14): on by default since Nov 18 (the owner, t119u; revision 9
    // moves an untuned off). ground_kicks: standing
    // (slower than 2 m/s) with the legs engaged (Y + X), a quick flick of the right stick kicks: up a front
    // kick, sideways a side kick, a half circle across the bottom sweeps the legs (a knock-down). The Y + X
    // chord is then the kicks' on the ground (no web strike or zip with both held). The kick's leg shows
    // while the animation owns the body (ANIMATED moves); the blow lands either way. kick_gaze_cone_deg aims
    // these too.
    bool groundKicks = true; // false until Nov 18
    // ENEMY JOINTS (R1, enemy_joints.hpp; November 14), EXPERIMENTAL: off until set. enemy_joints: a kick (air
    // or ground) reads the enemy's head and chest joints, so the zone it aims at and the zone it hits are the
    // enemy's own, not a capsule's bands (the bands remain the fallback for any joint a rig lacks).
    // enemy_limb_zones: also the legs from the shins and the arms from the forearms; leave it off until
    // probe_c0_skeleton.py has said READABLE on your game (no enemy's limb joint has been read yet).
    bool enemyJoints = false, enemyLimbZones = false;
    // PHYSICAL WEB TETHERS (W1, tether.hpp; November 14), EXPERIMENTAL: on by default (the owner, Nov 19;
    // revision 14). physical_tethers: a
    // simulated rope on each web (a held line sags and drapes), a whip of the hand jerks what is on it, circling
    // the hand turns it round you, and a throw with the trigger held keeps the line, paid out by the trigger.
    // tether_particles: points in the rope (24). tether_substeps: its steps a frame (8).
    bool physicalTethers = true;
    int tetherParticles = 24, tetherSubsteps = 8;
    // PASSIVE LEGS (leg_layers.hpp; November 12 06:18): in SpidyVR's air with
    // LEG CONTROL not engaged, the legs' spring runs with no stick, so momentum
    // moves them; this share of it shown (0 OFF). 60 by default. passive_legs_pct.
    int passiveLegsPct = 60;
    // PASSIVE COUPLING: the passive legs' turn on the body, percent of LEG
    // COUPLING's. 15 by default. passive_coupling_pct.
    int passiveCouplingPct = 15;
    // AIR CROUCH / REACH SPREAD (leg_layers.hpp's auto poses). leg_air_crouch, leg_reach_spread.
    bool legAirCrouch = true;
    bool legReachSpread = true;
    // MIDAIR POSES (leg_layers.hpp: tuck, dive, superhero, starfish; the owner, Nov 18): their own switch since Nov 18
    // 1:09 PM (they rode AIR CROUCH's before). air_poses.
    bool airPoses = true;
    // DIRECTIONAL FLIPS (ground_flip.hpp's TapTrigger): standing, the left stick held to a side, ahead or back and a
    // quick A tap flips that way. Off, A is never held back. directional_flips.
    bool directionalFlips = true;
    // POINT JUMP (point_jump.hpp; the user, November 11; EngineBot's plan):
    // touching a surface, A+B (or hold B, then A) charges a jump the way you
    // look. A tap is a quick jump at the tap speed; held up to CHARGE TIME a
    // leap rising to MAX SPEED. The tab's VR POINT JUMP rows: POINT JUMP (on
    // by default since revision 14, experimental), TRIGGER, TAP SPEED and MAX SPEED (m/s),
    // CHARGE TIME (ms) and SURFACE PUSH (percent of the surface's normal mixed
    // into the look). The rest are spidy_vr.ini only (point_jump_* keys): the
    // chord window, the tap's length, the curve (0 ease-out, 1 linear), the
    // least angle out of the surface, the speed kept across the launch, the
    // coyote time, the buffer, the cooldown, the guard after a game zip, B's
    // hold before it is the dodge (TRIGGER 1), and whether it works in a fight.
    // The player's own (comfort and controls), not in shared profiles.
    bool pointJump = true, pointJumpInCombat = true;
    int pointJumpTrigger = 0, pointJumpChordMs = 50, pointJumpTapMs = 150, pointJumpFullMs = 1000;
    int pointJumpMinSpeed = 9, pointJumpMaxSpeed = 22, pointJumpCurve = 0, pointJumpNormalBias = 25;
    int pointJumpMinOutDegrees = 10, pointJumpKeep = 25, pointJumpCoyoteMs = 120, pointJumpBufferMs = 200; // buffer 150 until Nov 16 (movement defaults 4)
    int pointJumpCooldownMs = 250, pointJumpZipGuardMs = 400, pointJumpDodgeHoldMs = 250;
    // QUICK SIDESTEP (November 12, sidestep.hpp; approved by the user, on by
    // default): on the ground, a left-stick flick then A within sidestepAMs is
    // a short upright burst (sidestepDistanceCm in sidestepMs, then a hard
    // stop). spidy_vr.ini only (sidestep_* keys), no menu row yet: the
    // player's own, not in shared profiles. sidestep=0 turns it off.
    bool sidestep = true, sidestepIframes = true, sidestepForward = false;
    int sidestepDistanceCm = 200, sidestepMs = 180, sidestepCooldownMs = 300, sidestepChain = 3;
    int sidestepLockoutMs = 800, sidestepFlickFrom = 30, sidestepFlickTo = 80, sidestepFlickMs = 80;
    int sidestepAMs = 120;
    // CAMERA MODES (camera_mode.hpp; the user, November 13 15:42). ANIM HEAD
    // RIDE: in a game animation (a landing, a perch, standing still) the view
    // rides Spider-Man's head, the headset on top; a perch is the animation's
    // as a landing is. SHOULDER LIMIT: the view never turns past his shoulders
    // (it replaced the 90-degree rule). SNAP REALIGN: the right stick's tap is
    // a snap round the full 360 and a realign of the heading, the model and the
    // headset. spidy_vr.ini only, on by default; 0 turns each off.
    bool animHeadRide = false, shoulderLimit = false, snapRealign = true;
    // HEADSET FIRST (the polish pass, November 14; the user, t64u: "Where the player looks takes full precedence, and everything else
    // should be adjusting to it"). On (default): the left stick runs relative to your headset - where you look when you press it,
    // following only your own head and right stick turns after - never relative to the body that is still turning,
    // and (with the view is yours, below) the animation never turns the view. Off: the old HYBRID heading.
    bool headsetFirst = true;
    // PUNCH AUTO-TARGET (the polish pass, slice 7; the user, t65u: "an autotarget for punches that turns your view toward any
    // enemy you're attacking", then: "I only want punches to target enemies that are highlighted and targeted by the
    // player. Punches always do damage, but tracking only happens on targeted enemies. If the player punches toward a
    // targeted enemy, everything should immediately realign in that direction."): a punch thrown within punchTrackConeDeg of
    // the targeted enemy (the one the ring marks), at most punchTrackRangeM away, turns the body, the aim and the icons onto
    // him on that frame. The VIEW turns only with punchViewSnap (off by default: t66u, "it can be a switch"). Punches do the
    // same damage with all of it off. punch_autotarget, punch_track_cone_deg, punch_track_range_m, punch_view_snap.
    bool punchAutotarget = true, punchViewSnap = false;
    // Nov 16 (the owner; s165800: 99 of 136 untracked punches outside 45 degrees, 37 past 6 m): 60 degrees, 8 m.
    // Nov 17 (the owner, "open up punch tracking and lengthen it to 10m"; s131456: 29 of 49 throws outside 60
    // degrees, 17 of them within 90): 90 degrees, 10 m.
    int punchTrackConeDeg = 120, punchTrackRangeM = 10; // cone 90 until the 2 AM log (Nov 18): 41 throws at 90-120
    // JUMP PULL (the polish pass, slice 8; the user, t65u: "a soft pull that gently pulls you toward enemies you jump toward"):
    // in SpidyVR's own air, with no web out, an enemy within jumpPullGazeDeg of where you look (or within jumpPullStickDeg of
    // the left stick's way while it is held) and jumpPullRangeM pulls a little (4 m/s^2, fading at the cone's edge and the
    // range's, off inside 1.5 m) until the jump has added jumpPullCapDmps of speed: the cap is in tenths of a
    // m/s (15 = 1.5 m/s). jump_pull, jump_pull_gaze_deg, jump_pull_stick_deg, jump_pull_range_m, jump_pull_cap_dmps.
    bool jumpPull = true;
    int jumpPullGazeDeg = 20, jumpPullStickDeg = 25, jumpPullRangeM = 12, jumpPullCapDmps = 15;
    // SHOWTIME (showtime.hpp; the plan's Pillar 6, EXPERIMENTAL, on by default — the owner, Nov 19,
    // "leave all features on for further testing"; revision 14 turns an old off once): hold X + B and the view steps outside
    // Spidey. showtimeWhile: 0 WATCH ONLY, 1 KEEP CONTROL; showtimeFadeMs: the whole cut. showtime, showtime_while, showtime_fade_ms.
    bool showtime = true;
    int showtimeWhile = 0, showtimeFadeMs = 150;
    // SHOWTIME's camera (S1): DISTANCE and HEIGHT in cm, ORBIT ANGLE in degrees off his back, ORBIT TURN 0 smooth / 1 snap, FOLLOW 0..2,
    // VIGNETTE 0..2, SLOW-MO IN SHOWTIME (L3), LATCH 0 off / 1 double press.
    int showtimeDistanceCm = 250, showtimeHeightCm = 30, showtimeAngleDeg = 35, showtimeTurn = 0, showtimeFollow = 1, showtimeVignette = 1;
    bool showtimeSlowMo = true;
    int showtimeLatch = 0;
    // SHOWTIME's automatic moments (S2): CHORD 0 hold X+B / 1 auto only; AUTO: DODGES 0 off / 1 perfect only / 2 all; AUTO: FINISHERS
    // (log only in this build; on by default since revision 14, as in the owner's Nov 19 session).
    int showtimeChord = 0, showtimeAutoDodge = 1;
    bool showtimeAutoFinish = true;
    // SHOWTIME's body (S3): 0 game pose with the head shown, 1 as in play (no head).
    int showtimeBody = 0;
    // A IN THE AIR (the user, Nov 14 t71u; air_correct_ms): the upright turn takes this long, smoothly (realistic_flip.hpp
    // RealisticFlipSettings::levelMs). REALISTIC FLIPS' A rights the player; it was a 0.18 s chase with a hard start.
    int airCorrectMs = 1000;
    // POINT JUMP BULLET (the user, Nov 14 t71u, t73s656; point_jump_bullet, default on): the body takes the RAW look on a point
    // jump, any angle, eased over about 120 ms (point_bullet.hpp). The launch velocity is unchanged (10 degree out-of-surface fix).
    bool pointJumpBullet = true;
    // JUMP SOUNDS (beta polish A4, the owner's D6; point_jump_sound, on): the game's jump grunt with the rest
    // of its jump set (sound_guard.hpp) on Spidey at every point jump, ground flip or handstand leap; skipped
    // when the game's own grunt already sounds (300 ms guard). Until beta2 a point jump had the grunt alone,
    // and the owner heard nothing on 0.8.7-na-beta1 (Nov 19): the game added its own a442c255/050553e7 on
    // only 22-37% of them.
    bool pointJumpSound = true;
    // WALL JUMP SOUND (beta polish A5; wall_jump_sound): 0 off (silent as before), 1 d321a6f9 (default), 2 110d202b,
    // the Nov 19 trace's two candidates (sound_guard.hpp) for a headset A/B.
    int wallJumpSound = 1;
    // AIR DODGE (beta2, air_dodge.hpp; the owner, Nov 19 t79u: "Air dodge is fine on B"): in SpidyVR's own
    // air, B is a burst of air_dodge_speed (9 m/s) the way the left stick points from the view's heading (at
    // rest: backward), after waiting air_dodge_chord_ms (50, the point jump's window) for A, X or Y;
    // air_dodge_cooldown_ms (500) between, two an air. The ground's B (dodge, sidestep, point jump) is
    // untouched. On by default (movement_defaults revision 15).
    bool airDodge = true;
    int airDodgeSpeed = 9;
    int airDodgeChordMs = 50;
    int airDodgeCooldownMs = 500;
    // SHOWTIME RAY FAN (beta2, showtime_fan.hpp; the owner, Nov 19 t79u: "ray fan for Showtime is a good call"): while
    // SHOWTIME's camera is out, the head's clearance slots of the native rays carry a fan from his pelvis to the camera's
    // place; the nearest hit limits the distance (fast in, slow out). showtime_speed_lead_pct (60): the share of his
    // velocity the pivot is carried by at 25 m/s (none below 8 m/s), for less lag at swing speed; 0: as before.
    bool showtimeRayFan = true;
    int showtimeSpeedLeadPct = 60;
    // STICKY HANDSTANDS (beta2, handstand.hpp Params::sticky; the owner, Nov 19 t79u: "making handstands \"sticky\" or
    // easy to get into would help"): the arms-up tests 5 cm easier and held 0.08 s, a tracking loss held
    // handstand_sticky_grace_ms (400; was 250), the rig's re-placement taken as a move, not a loss, and the hold's
    // floor line 1.5 x HANDSTAND HOLD with 0.15 s off the floor before it ends. The arms still have to be up.
    bool handstandSticky = true;
    int handstandStickyGraceMs = 400;
    // FULL-LENGTH SWING LINES (beta2, swing_line.hpp; the owner, Nov 20 t81u: "Yes, reel should shorten one-for-one,
    // the line should stay full. Let's add this as a toggle option."): a swing web is drawn as long as it was when it
    // caught, its slack hanging (the combat webs' rope), shortened only by the reel. Visual only: the swing is as
    // before. Off: the straight line from the hand to the anchor.
    bool swingLineFullLength = true;
    // HERO ATTACK SOUND (beta polish A2; attack_sound, spidy_vr.ini only, no menu row): one of the game's sounds played
    // on Spidey as a punch is thrown or a kick starts (at most one a hand every 250 ms, hit_sound.hpp): the id's 32
    // bits as a signed decimal. 0 (the default): none - the game's own whoosh and effort ids are not captured yet
    // (the capture session); nothing is guessed.
    int attackSound = 0;
    // AIR TAKEOVER (fall_handoff_ms, 150): a plain fall (a ledge, a stumble; no jump press, never rose) hands to SpidyVR this long
    // into the air; 0 waits for JUMP TO SPIDYVR's timer (hybrid_owner.hpp Frame::fallHandoffNs).
    int fallHandoffMs = 150;
    // LANDING BLEND-IN (air_landing_blend, default on; landing_blend.hpp): the time to impact in SpidyVR's air from a downward ray,
    // the body coming upright over the last 0.3 s, and a per-landing account of the prediction. The touchdown handoff is as before.
    bool airLandingBlend = true;
    // The animation takes the body 0.2 s BEFORE the touchdown (air_landing_release, on by default since revision 14 for
    // further testing; UNVERIFIED: it needs the game to keep his velocity and pick its normal landing clip when the body
    // is let go in the air). Needs AIR LANDING BLEND.
    bool airLandingRelease = true;
    // [air takeover pass settings]
    // HANDSTAND CATCH HOLD (handstand_catch_hold_cm, 35; 12 was the old floorReach): while a catch settles, palms
    // within this of the floor count as on it, so a catch is not dropped at the 0.6 s settle (the 9790d67 playtest: 3 of 3).
    int handstandCatchHoldCm = 35;
    // HANDSTAND REACH (handstand_reach_cm, 160; 130 until Nov 17, 100 before): an upside-down touchdown counts a palm within
    // this of the floor (and the hands over the head: the gesture is kept). s131456's attempts read 1.15-1.41 m.
    int handstandReachCm = 160;
    // TAKEOVER VIEW EASE (takeover_view_ease, on): every switch to SpidyVR (a point jump, a web, a wall run, a handstand,
    // a sidestep, a swing, an animation's end) holds the view and eases it over AIR VIEW EASE, as the jump, flip and grip
    // cancel always did (the 9790d67 playtest: 60-110 uneased 0.26-0.30 m eye jumps a session). Off: only those.
    bool takeoverViewEase = true;
    // HEAD HEIGHT FADE (head_height_fade_ms, 500; 0: off): at a switch to SpidyVR the animation head's height over
    // where tracking puts the eyes (median 0.27 m seated) fades out over this, on its own, instead of riding the view
    // ease's 250 ms (view_ease.hpp). Only with a view ease.
    int headHeightFadeMs = 275; // 500 until the handoff tune (Nov 18): it set every takeover's end (493 ms)
    // ONE HANDOFF BLEND (handoff_one_blend, on): a switch's eye blend runs as long as AIR VIEW EASE (250 ms; 180 ms when
    // that is off), and when the view ease carries the eyes' place, the blend turns only their angle: one ease of one
    // length instead of a 180 ms blend on top of a 250 ms ease (the 9790d67 playtest). Off: as before.
    bool handoffOneBlend = true;
    // PUNCH CAMERA LOCK (punch_camera_lock_ms, 300; 0: off): a punch thrown on the ground hands him to SpidyVR and,
    // most often, straight back on the landing (45 of 58 short stints in the 9790d67 playtest): the view is held where
    // the animation showed it for this long before PUNCH VIEW EASE starts, so a round trip inside it never moves the camera.
    int punchCameraLockMs = 300;
    // VIEW RELATCH FIX (view_relatch_fix, on): when the head lock's reference is latched afresh (a landing, a takeover
    // back, a recenter, a snap realign), the next frame measures where the player looks against the new reference,
    // not by subtracting the old reference's headset turn: that one wrong frame let a turn hold "arrive" with the hero still
    // far off (snap realigns and landings that never turned him, 9790d67). Off: as before.
    bool viewRelatchFix = true;
    // SNAP BODY TURN (snap_body_turn, on): a snap realign (the right stick's tap) turns the body onto the new heading
    // too - the walk stick brings him round, fights included - and the turn is logged as a "realign" hero turn.
    // Off: the view snaps and the body stays where it was.
    bool snapBodyTurn = true;
    // SNAP TAP WINDOW (snap_tap_ms, 250; 120 Nov 16-18, 250 before): with SMOOTH TURN on, a right-stick push out and
    // back within this many milliseconds is a snap realign; a longer push is the smooth turn. Shorter: fewer snaps by
    // accident. The owner, Nov 18: "Raise the snap tap window to 250 ms" (s131456: 67 flicks of 167-375 ms missed).
    int snapTapMs = 250;
    // DODGE RE-FACE (dodge_reface, on): when a dodge has settled (0.4 s after the button is let go) and the body faces
    // more than 10 degrees off where the player looks, the walk stick turns him back onto the view - a smooth turn,
    // never a snap, fights included; the view is never written. Off: as before (only the turn-in-place angle).
    bool dodgeReface = true;
    // LANDING BODY TO VIEW (landing_body_to_view, on; headset first): after a landing the drawn body is turned about the
    // head onto where the player looks while the walk stick brings the hero round, then eases back onto his real facing -
    // the pose only: nothing is written to the game, and the camera does not move. Off: as before.
    bool landingBodyToView = true;
    // LANDING BODY EASE (landing_body_ease_ms, 500): how long the drawn body takes to ease onto the view after a landing,
    // and to ease back off it.
    int landingBodyEaseMs = 500;
    // LANDING REAR TURN (landing_rear_turn, on): a calm landing with the view more than 120 degrees behind him turns him
    // onto it too (the facing check no longer keeps him facing away). Off: he stays, as before.
    bool landingRearTurn = true;
    // HANDOFF EASE CURVE (handoff_ease_curve, 1; 0: the old quick start): how a takeover's view glide moves. 1 starts
    // and stops at rest (smootherstep, no motion bleed): the camera never jolts into the glide. 0: the cubic ease-out with
    // the motion bleed, which starts at three times the average speed (d591229: median 0.32 m in 250 ms, 3.8 m/s at once).
    int handoffEaseCurve = 1;
    // HANDOFF GLIDE SPEED (handoff_glide_cm_s, 200 = 2 m/s; 0: the fixed lengths): a handoff's camera glide is made
    // long enough that the camera never moves faster than this - a 0.3 m offset still takes the ease's 250 ms, a 0.6 m
    // one about 560 ms (d591229: offsets up to 0.94 m level and 1.31 m in height were crossed in 250 ms). Takeovers'
    // glides and the height fade, and the landing's blend onto the head.
    int handoffGlideCmS = 350; // 200 until the handoff tune (Nov 18)
    // HANDOFF GLIDE LONGEST (handoff_glide_max_ms, 700): the longest a glide is stretched to by HANDOFF GLIDE SPEED.
    int handoffGlideMaxMs = 450; // 700 until the handoff tune (Nov 18)
    // HANDOFF HEAD LEADS (handoff_head_leads, on): through a handoff's blend your head turns move the view 1:1; only
    // the difference between the two views at the switch (the animation's pitch and roll, mostly) fades out. Off: the old
    // blend from the switch's view to the live one, which follows head turns only partly until it ends.
    bool handoffHeadLeads = true;
    // HANDOFF ANGLE TIME (handoff_angle_ms, 300; 0: the blend's own length): how long the pitch and roll the animation's
    // head gave the view take to fade out after a switch (the place glides on its own clock).
    int handoffAngleMs = 200; // 300 until the handoff tune (Nov 18)
    // LANDING BLEND (landing_blend_ms, 250; 0: air_view_ease_ms as before): how long a landing's view takes to glide
    // onto the hero's head when the animation takes over (HANDOFF GLIDE SPEED stretches a long one). Was tied to
    // air_view_ease_ms; now tunable on its own.
    int landingBlendMs = 200; // 250 until the handoff tune (Nov 18)
    // HEAD SEAT GLIDE (head_seat_glide_ms, 250; 180 was the old fixed length): how long the view takes to slide when
    // the mod re-seats your head on the hero's (after a move change, an action, a zip or drift; up to ~0.2 m).
    int headSeatGlideMs = 165; // 250 until the handoff tune (Nov 18)
    // HANDSTAND ARMS OVER HEAD (handstand_arms_over_head_cm, 10; 0: the old test): at a landing, both hands count as up
    // when they are at least this far over your real head (as you hold them), whatever the flip's angle. The old test
    // wanted the palms 20 cm below the head in the flipped world, which hands held over the head only met when he was
    // past about 125 degrees upside down (s165800: refused at 100 and 117).
    int handstandArmsOverHeadCm = 10;
    // TURN SPEED (turn_push_pct, 55; 35 was the old fixed push): how hard the walk stick is pushed when the mod turns
    // the hero onto your view (landing turns, re-faces, turn-in-place). s165800: at 35 the game turned him about 100-140
    // deg/s and a landing's turn took 0.73 s (median), up to 2.6 s. Higher turns faster; a push past ~60 may make him
    // step while turning. The 'still stuck' push stays at least 50.
    int turnPushPct = 55;
    // LANDING BODY HOLDS THE TURN (landing_body_hold_turn, on): after a landing the drawn body stays on your view until
    // the hero has really come round (within 12 deg), not the first frame the walk stick's turn pauses. Off: as before
    // (s165800: 27 of 96 ended 'came round' early, 35 cancelled).
    bool landingBodyHoldTurn = true;
    // INSTANT WEB ZIP (web_zip_instant, on): a web zip or web strike pressed while you swing or run a wall in SpidyVR
    // hands the body to the game's zip on the press, so it starts at once. s165800: from SpidyVR the zip waited a median
    // 0.9 s (up to 2.5 s) for the game to begin it. A press the game does not take (nothing to zip or strike) gives the
    // swing back after 0.4 s. Off: SpidyVR waits for the game to start the zip, as before.
    bool webZipInstant = true;
    // POINT JUMP FIRES AT FULL CHARGE (point_jump_fire_full, on): held to CHARGE TIME the point jump launches by itself at
    // full power, instead of waiting for you to let go (s165800: press to launch median 361 ms, p90 1.2 s, mostly the
    // hold). A tap or a shorter hold still fires on the release. Off: only the release fires, as before.
    bool pointJumpFireFull = true;
    // TARGET SCAN OUTSIDE FIGHTS (the owner, Nov 16; s165800: "things are lagging during heavy action"): how often,
    // milliseconds, the web, grab and punch target lists rescan the game's actors when nothing is fighting (250 to
    // 2000); in a fight, and for 3 s after one, every 250 ms as before. 500: half the scans out on the streets.
    int targetScanIdleMs = 500;
    // HANDOFF PLACE (handoff_place_cm, 200; 0: always glide): a landing or zip handoff whose view has farther than this to go
    // onto the head is placed there at once (the position only; the angle still blends, the headset leads) instead of
    // sliding at many metres a second (b318cb1: 26 of 85 landing glides had over 1.4 m to go, up to 47 m in 700 ms).
    int handoffPlaceCm = 125; // 200 until the handoff tune (Nov 18)
    // PUNCH TRACK BY LOOK (Nov 17; off by default): a throw also tracks when you LOOK within the cone of the
    // targeted enemy, the fist thrown off it (punch_track.hpp). punch_track_by_look.
    bool punchTrackByLook = false;
    // HANDSTAND GRACE (the owner, Nov 17: "give some grace for getting into one"): hands over the head up to this long
    // before an upside-down touchdown count at it, their hold too (handstand_grace_before_ms, 300); and the touchdown can catch for
    // handstand_grace_after_ms (400; 250 before) after SpidyVR's air.
    int handstandGraceBeforeMs = 300;
    int handstandGraceAfterMs = 400;
    // HANDSTAND TIP-IN (the owner, Nov 17: "tip the player into the handstand when it's clear they're attempting one"):
    // inverted at least handstand_tip_in_deg (120), not spinning, the hands over the head (now or in the grace): the catch takes
    // the palms from up to handstand_tip_in_reach_cm (200) over the floor with no hold wait, and sets them down.
    bool handstandTipIn = true;
    int handstandTipInDeg = 120;
    int handstandTipInReachCm = 200;
    // MARKER TWIST STEADY (the owner, Nov 17: "reaching around to fire webs causes the marker to jump ... loosen up that
    // twisting"): the arm line is smoothed in the world, carried with the controller's pointing, so a twist of the hand no longer
    // swings it round (web_aim::ForearmAim::updateWorld); its own changes go at most marker_turn_rate_dps (360; 0 no cap), and the
    // arm/controller fallback switches only after marker_switch_hold_ms (150) of the new answer. marker_twist_steady=0: as before.
    bool markerTwistSteady = true;
    int markerTurnRateDps = 360;
    int markerSwitchHoldMs = 150;
    // [next pass settings]
    bool operator==(const Values&) const = default;
};
inline constexpr int eyeAimShots = 1, eyeAimZips = 2;
// The steps the game's Settings offer. The launcher's slider sets any swing
// speed; the Settings show the step nearest to it.
inline constexpr int swingSpeeds[] = {10, 15, 20, 25, 32, 40, 48, 56, 65};
inline constexpr int snapTurns[] = {0, 15, 30, 45, 60, 90};
inline constexpr int smoothTurns[] = {0, 60, 90, 120, 180, 240};
inline constexpr int hapticLevels[] = {0, 25, 50, 75, 100};
inline constexpr int weights[] = {40, 60, 80, 100, 125, 150, 200, 250, 300};
inline constexpr int flipSpeeds[] = {90, 120, 150, 180, 240, 300, 360, 480};
inline constexpr int nearClips[] = {10, 15, 20, 30, 50, 75, 100};
// HEIGHT's steps, centimetres: AUTO, then 150 to 190 (a row takes nine
// choices at most: five apart where most players are, ten at the ends).
inline constexpr int heights[] = {0, 150, 160, 165, 170, 175, 180, 185, 190};
// Real gravity, m/s^2: a weight of 100% falls at it.
inline constexpr float realGravity = 9.81f;
// The swing's gravity for a weight, m/s^2.
inline float gravity(int weight) {
    return realGravity * static_cast<float>(weight) / 100;
}
// The game screen's width for each size, metres, at screenDistance.
inline constexpr float screenWidths[] = {2.4f, 3.2f, 4.2f};
inline float screenWidth(int size) {
    return screenWidths[size < 0 ? 0 : size > 2 ? 2 : size];
}
// The game screen's distance from the viewer it was placed for, metres
// (presentation_gate's screenAhead).
inline constexpr float screenDistance = 2.5f;
// How much of a scene's depth the game screen shows for each GAME SCREEN 3D
// choice (native_eyes::screenStereo's depth): at 1 everything is as far from
// you as it is from the game's camera, and what is nearer to that camera than
// the screen is to you comes out of the screen; lower, everything lies
// farther back.
inline constexpr float screenDepths[] = {0, .5f, .7f, 1.f};
inline float screenDepth(int choice) {
    return screenDepths[choice < 0 ? 0 : choice > 3 ? 3 : choice];
}
// The values within the session's ranges: swing speed 1-65 m/s (what
// run_game_vr.py accepts), snap turn 0-90 degrees, smooth turn 0-360 degrees
// a second, vibration and body haptics 0-100%, weight 40-300%, HUD 0-3, flip
// speed 90-480 degrees a second, the game screen's depth 0-3, near clip
// 10-100 mm, recording FOV 0 (off) or 130-150 degrees, UI style 0-1, blow
// sounds 0-2, height 0 (AUTO) or 120-220 cm, eye aim 0-3.
Values sanitized(Values);

enum class Item : uint8_t {
    none,
    aimMarkers,
    airWebs,
    swingSpeed,
    snapTurn,
    haptics,
    screenSize,
    smoothTurn,
    weight,
    calibrate,
    flips,
    webButton,
    hud,
    flipSpeed,
    screenDepth,
    view,
    webStyle,
    handClimb,
    nearClip,
    realisticFlips,
    automaticStrength,
    automaticSpeed,
    automaticMinimumSpeed,
    automaticReleaseFlip,
    automaticResponse,
    automaticHandStrength,
    automaticFalloff,
    automaticHandThreshold,
    automaticAirBrake,
    bodyHaptics,
    hapticWebs,
    hapticWind,
    hapticCombat,
    hapticMovement,
    recordingFov,
    uiStyle,
    blowSounds,
    webSlingshot,
    betterClimbing,
    height,
    seated,
    automaticCombat,
    wallRunCameraTilt,
    controlledFlips,
    combatVisuals,
    eyeAim,
    smartAnchor,
    elasticWeb,
    softRelease,
    swingAssist,
    slingshotSpeed,
    recordingSmoothing,
    twoHandZip,
    ropeSteering,
    webRange,
    webSpeed,
    joystickRunCamera,
    halfPunchDamage,
    flipReprojection,
    steadyHud,
    reelHaptics,
    swingHaptics,
    pcMedia,
    singleHandYankStrength,
    webTie,
    nativeRun,
    airSlingshot,
    webAngle,
    wristShooters,
    dodgeTumble,
    runCurve,
    nativeRunAt,
    runLogs,
    momentumLeap,
    earlyJump,
    softLanding,
    spidyRun,
    webAimSource,
    forearmBlend,
    spidyRunFromStanding,
    webLandingSmooth,
    webLandingImpact,
    steadyStates,
    combatResponsive,
    spidyRunJump,
    forearmAimLimit,
    palmShooters,
    headProbe,
    animatedMoves,
    animatedHead,
    animatedAir,
    animatedSteering,
    animatedArms,
    animatedTorso,
    animatedModelYaw,
    animatedOnly,
    animatedAirSpin,
    animatedHybrid,
    animatedHybridSpeed,
    nativeAnimation,
    webLines,
    animatedHeadsetSmooth,
    runSpeed,
    slowMotionLength,
    softLock,
    cameraStabilization,
    cameraVertical,
    cameraHorizontal,
    cameraAngle,
    lightVertical,
    lightHorizontal,
    lightAngle,
    heavyVertical,
    heavyHorizontal,
    heavyAngle,
    legControl,
    legStiffness,
    legDamping,
    legCoupling,
    airFists,
    // November 11: the web shooters' one choice and its strength (views over
    // wrist_shooters, web_aim_source, palm_shooters and forearm_blend:
    // shooter_lock.hpp), and CAMERA MOTION (camera_motion.hpp).
    shooterMode,
    shooterLock,
    cameraMotion,
    cameraPitch,
    cameraRoll,
    cameraBobVertical,
    cameraBobSide,
    cameraHeadAngle,
    // November 11: POINT JUMP (point_jump.hpp), its own tab VR POINT JUMP.
    pointJump,
    pointJumpTrigger,
    pointJumpMinSpeed,
    pointJumpMaxSpeed,
    pointJumpFull,
    pointJumpPush,
    // November 12: PUNCH VIEW EASE (view_ease.hpp), VR CAMERA & STABILIZATION.
    punchViewEase,
    // November 12: MOVE HAPTICS (move_haptics.hpp), HAPTICS; VIEW YAW SOURCE
    // (stable_yaw.hpp) and SOUND TRACE (sound_trace.hpp), NATIVE ANIMATION.
    moveHaptics,
    viewYawSource,
    soundTrace,
    // November 12: JUMP TO SPIDYVR (hybrid_owner.hpp), NATIVE ANIMATION; AIR
    // VIEW EASE (view_ease.hpp), VR CAMERA & STABILIZATION.
    jumpHandoff,
    airViewEase,
    // November 12 (Step B): ANIM YAW SWAY, TURN-IN-PLACE ANGLE (view_yaw_lock.hpp), VR CAMERA & STABILIZATION.
    animYawSway,
    turnInPlace,
    // November 12 (legs): PASSIVE LEGS, PASSIVE COUPLING, AIR CROUCH, REACH SPREAD (leg_layers.hpp), VR LEGS (ADVANCED).
    passiveLegs,
    passiveCoupling,
    legAirCrouch,
    legReachSpread,
    // November 12 (hybrid movement): MOVEMENT MODE, REAR SNAP TURN (hybrid_movement.hpp), VR CAMERA & STABILIZATION.
    movementMode,
    rearSnap,
    animatedTorsoTwist,
    animForearmFallback,
    steadyVertical,
    steadyHorizontal,
    steadyAngle,
    cameraHorizonLevel,
    cameraVrStabilization,
    swingKickManual,
    swingKickAuto,
    wallBrake,
    gripFling,
    handGrab,
    handstand,
    handstandAssist,
    handstandFall,
    handstandPushGain,
    handstandPushMax,
    handstandHeight,
    handstandGain,
    handstandEyeDrop,
    handGrabForce,
    enemyHoldCost,
    kicks,
    kickGazeCone,
    kickSpin,
    groundKicks,
    enemyJoints,
    enemyLimbZones,
    physicalTethers,
    tetherParticles,
    tetherSubsteps,
    headsetFirst,
    animHeadRide,
    shoulderLimit,
    punchAutotarget,
    punchTrackCone,
    punchTrackRange,
    punchViewSnap,
    jumpPull,
    jumpPullGaze,
    jumpPullStick,
    jumpPullRange,
    jumpPullCap,
    handstandSupport,
    handstandGrace,
    handstandDamping,
    handstandLeap,
    showtime,
    showtimeWhile,
    showtimeFade,
    showtimeDistance,
    showtimeHeight,
    showtimeAngle,
    showtimeTurn,
    showtimeFollow,
    showtimeVignette,
    showtimeSlowMo,
    showtimeLatch,
    showtimeChord,
    showtimeAutoDodge,
    showtimeAutoFinish,
    showtimeBody,
    airCorrect,
    pointJumpBullet,
    fallHandoff,
    airLandingBlend,
    airLandingRelease,
    // Handoff tune (Nov 18, the owner: "give me smaller increments to set"): the handoff and landing
    // timing rows, ini-only until now.
    landingBlend,
    handoffGlideSpeed,
    handoffGlideLongest,
    handoffPlace,
    handoffAngle,
    headSeatGlide,
    landingBodyEase,
    // Nov 18 1:09 PM (the owner: "Give poses and side flips their own toggles"): DIRECTIONAL FLIPS (ground_flip.hpp's
    // A taps), FLIPS tab; MIDAIR POSES (leg_layers.hpp's first poses), VR LEGS (ADVANCED).
    directionalFlips,
    airPoses,
    // 0.8.7 (Ilya): ENEMY TYPES, after the fork's items (the launcher's order is the enum's).
    enemyTypes,
    // HAND GRAB CLASS RULES (G2b, the owner's Nov 13 rules; beta polish: a launcher row in KICKS & COMBAT).
    handGrabClassRules,
    // JUMP SOUNDS and WALL JUMP SOUND (beta polish A4/A5, POINT JUMP's tab).
    pointJumpSound,
    wallJumpSound,
    // AIR DODGE (beta2).
    airDodge,
    // SHOWTIME RAY FAN (beta2).
    showtimeRayFan,
    // STICKY HANDSTANDS (beta2).
    handstandSticky,
    // FULL-LENGTH SWING LINES (beta2).
    swingLineFullLength,
    // NA HELP (beta polish M5, na_help.hpp): the NA · HELP tab's pages, one item for all of them (a page has no
    // value: one choice, READ). Its rows are the catalog's last.
    helpPage,
};
inline constexpr Item lastItem = Item::helpPage;
// SLOW-MO LENGTH's last step: no time limit (until L3 toggles it off).
inline constexpr int slowMotionEndless = 91;
// Real seconds a full meter lasts for a SLOW-MO LENGTH value; 0 = endless.
inline float slowMotionDrainSeconds(int value) {
    return value >= slowMotionEndless ? 0.f : static_cast<float>(value < 3 ? 3 : value);
}
// Where a row shows: in the tab; on the pause menu's first page instead,
// under RESUME: VR VIEW (the user, October 15: "move the VIEW: GAME SCREEN to
// the first menu (the one with resume)"); or only in the launcher's options:
// the realistic flips' finest tuning, its thresholds and how a spin builds
// and dies away (the user, October 25: "cleanup the redundant"). The tab's
// RESET ALL leaves the last two alone.
enum class Place : uint8_t { tab, page, launcher };
// The stable row catalog (screens() supplies the display order): a section's heading (item none) or a
// setting. The sections: WEBS, SWINGING, COMFORT, HAPTICS, DISPLAY, BODY and
// EXPERIMENTAL (the player, October 19: "sort the options in spidy vr
// settings ingame into nice sections"; HAPTICS since October 20, the
// controllers' vibration and the bHaptics rows together). A setting without choices is an ON/OFF switch, the game's own;
// one with choices steps through them. Titles are upper case, as the game's
// own rows are; the help shows beside the rows while the setting is selected.
// A row shown elsewhere (Place) stands where it would be in the tab.
struct Row {
    Item item{};
    const char* title{};
    const char* help{};
    std::span<const char* const> choices{};
    Place place{};
};
// Stable row/setting numbers; categories below determine presentation order.
// The catalog: the settings' rows (239), then NA HELP's pages.
inline constexpr size_t settingRowCount = 239;
inline constexpr size_t rowTotal = settingRowCount + na_help::pageCount;
const std::array<Row, rowTotal>& rows();
const Row* rowFor(Item);
// NATIVE ANIMATION'S OWN (beta polish M1): true for every setting the Native Animation fork added (135 at 88cc744),
// false for upstream Spidy's (the 89 settings of v0.8.7's Item, WEB LINES, AIR FISTS and ENEMY TYPES among the fork's
// numbers included). The one source for the launcher's NA highlight and the in-game NA tabs.
bool isNativeAnimation(Item);
// NA HELP (beta polish M5): docs/NA_HELP.md, generated from na_help.hpp and, for its cheat sheet, the catalog's NA
// tabs (every row, where it shows and its default). beta_polish_tests keeps the file equal to this.
std::string helpMarkdown();
// camera, shooters and legs (November 11, the user: "I can't find legs or
// camera settings in any of the menus. Make sure it's clearly labeled"): tabs
// of their own, after the older ones so their numbers stay.
// pointJump (November 11): POINT JUMP's own tab, last so the others keep theirs.
// NA TABS (beta polish M4, the owner's D1/D2): the in-game tabs are the upstream ones, then every setting Native
// Animation added, grouped in tabs whose names start with naTabPrefix (the tab order is this order; a tab is
// numbered by its place only while the menu is open, so reordering needs no migration). KICKS & COMBAT took the
// kicks out of LEGS and the punch / jump assists out of MOVEMENT.
enum class Category : uint8_t { setup, webs, swinging, movement, flips, feedback, display, nativeAnimation,
                                camera, handoff, legs, kicks, shooters, pointJump, showtime, help, count };
// The in-game NA tabs' prefix: "NA" and a middle dot (U+00B7, UTF-8). RE: the game's font is not verified to have
// the dot (the capture-session list); swap this for "NA - " if a headset shows a box.
#define SPIDY_NA_TAB_PREFIX "NA \xC2\xB7 "
inline constexpr const char* naTabPrefix = SPIDY_NA_TAB_PREFIX;
inline constexpr size_t categoryCount = static_cast<size_t>(Category::count);
struct Screen {
    const char* title;
    const char* tabTitle;
    std::span<const Item> items;
};
const std::array<Screen, categoryCount>& screens();
// Null while the control has an effect, otherwise a short explanation. Stored
// preferences are retained when a parent is off or a different mode overrides them.
const char* inactiveReason(Item, const Values&);
// A row that stays selectable while inactiveReason explains why it does
// nothing yet (LEG CONTROL and its tuning: the user could not find them while
// REALISTIC FLIPS was on). Choosing LEG CONTROL ON then also turns CONTROLLED
// FLIPS on and REALISTIC FLIPS off, so the row always does what it says.
bool shownWhenInactive(Item);
// RESET ALL affects only the active category's in-game controls, including inactive ones.
Values resetCategory(Category, Values);
// The most choices a row shows (game_menu's buffers): a row with more would
// lose its last ones.
inline constexpr size_t maxChoices = 11; // 9 until November 14 (the camera rows' 750 and 1000 ms)
// The values a list setting's choices stand for, in their order; none for a
// switch.
std::span<const int> steps(Item);
// The choice a setting shows for these values: a switch 0 (off) or 1 (on), a
// list its step nearest to the value (a launcher value between two steps
// shows the nearer one, the lower on a tie).
int choice(Item, const Values&);
// Puts a choice into the values; false when it is out of range or changes nothing.
bool choose(Item, int choice, Values&);
// What the game's RESET puts back: the choice for Spidy's defaults (Values{}).
int defaultChoice(Item);
// The settings no launcher.ini key or XrConfig field carries: NEAR CLIP,
// RECORDING FOV, UI STYLE, BLOW SOUNDS, HEIGHT, SEATED, WEB SLINGSHOT, BETTER CLIMBING, HAND WALL
// CLIMBING, EYE AIM, FLIP REPROJECTION, STEADY HUD, REEL HAPTICS, SWING HAPTICS, the realistic flips'
// eleven rows and the body haptics' four groups. The game module keeps them as these key=value lines in a
// file of their own, beside the launcher's settings (game_xr.cpp, tabOnlyPath), and reads them back when
// VR starts; the launcher's options read and write the same file.
std::string tabOnlyText(const Values&);
// The values with what such text holds, sanitized. A line with an unknown
// key, or whose value is no whole number, is passed over.
Values withTabOnly(Values, std::string_view text);
// The movement defaults' revision (Values::movementDefaults) this build has.
inline constexpr int movementDefaultsRevision = 16;
// November 12 movement cleanup: a settings file (that text) older than
// movementDefaultsRevision gets the new movement defaults once - RUN CURVE
// on, EARLY JUMP at least QUICK TAP - and is marked as having seen them; a
// newer one is left as it is. Revision 2 (polish pass): STEADY's three times saved at the old defaults
// (350/300/450 ms, never tuned) become the new ones (200/200/200); any other value stays.
// Call after withTabOnly on the same text.
Values withMovementDefaults(Values, std::string_view text);
} // namespace spidy::vr_settings
