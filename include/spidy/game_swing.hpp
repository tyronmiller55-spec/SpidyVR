#pragma once
#include "native_movement.hpp"
#include "presentation_gate.hpp"
#include "swing.hpp"
#include "swing_takeoff.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace spidy::game_swing {
// Gravity and air steering are the strengths the October 4-5 test builds
// actually applied. Those builds configured 18 and 12, but steered from
// one-step-old state (see InFlightStep), which applied about a third of each.
// The VR settings' weight sets the gravity, at start and during play
// (Settings), up to maxGravity.
struct Config {
    uint32_t magic = 0x53574346, version = 1, bytes = sizeof(Config), pid{};
    uint64_t base{}, record{}, mover{}, motionModule{};
    uint32_t durationMs = 20000;
    float maxSpeed = 32, gravity = 6;
    // Web grab: what a web may catch (bits 1 << game_targets::Kind; 0 = no
    // grabbing, as before). Only kinds the game adapter can move take effect.
    uint32_t grabKinds{};
};
inline SwingConfig physicsConfig(const Config& c) {
    SwingConfig physics;
    physics.maxSpeed = c.maxSpeed;
    physics.gravity = c.gravity;
    physics.reelSpeed = 16;
    physics.airAcceleration = 4;
    physics.maxZipImpulse = 18;
    physics.zipMultiplier = 4.5f;
    // Walls are the swing's own from the start (Settings::walls).
    physics.walls = true;
    // And webs are ropes (Settings::physical), taut in the air, whose swings
    // come down to the ground no nearer than his centre, a metre over his
    // feet here, 1.4 m over it: his feet 0.4 m (SwingConfig::taut,
    // groundClearance).
    physics.physical = true;
    physics.taut = true;
    physics.groundClearance = 1.4f;
    return physics;
}
// MoverStandard applies a velocity command during the physics step after the
// one Spidy observes: the observed position starts the step already in flight,
// and the observed velocity is the command before last. Steering from those
// made even and odd steps two independent trajectories. Each received gravity
// and steering every other step, and a yank kicked only one of them, so the
// body alternated between two velocities and the view shook until landing.
// Predict from where the step in flight leaves the body instead.
class InFlightStep {
  public:
    struct Sample {
        uint64_t step{}, serial{};   // native step and the command serial it applies
        Vec3 position{}, achieved{}; // start of this step; velocity of the previous one
        float dt{};                  // duration of this step
        bool controlled{};           // this step applies Spidy's command `serial`
    };
    // Body at the start of the step that the next command will govern.
    Body predict(const Sample& s, bool grounded) {
        const Record* flight = s.controlled ? find(s.serial) : nullptr;
        // The command that produced the observed velocity. Without an
        // observation in between, the step in flight repeats the same command.
        const Record* produced = s.step == step_ + 1 ? (controlled_ ? find(serial_) : nullptr) : flight;
        Vec3 velocity = flight ? flight->end : s.achieved;
        // The movement module moves the body as far as stepDelta says: a
        // command it shortened (a step much longer than the one before it,
        // at the speed limit) is no collision, and takes no speed away.
        Vec3 travel = flight ? native_movement::stepDelta(flight->requested, s.dt, flight->distance)
                             : s.achieved * s.dt;
        // The step that produced the observed velocity: the one observed
        // before this one, or an unobserved one about as long as this one.
        const float took = s.step == step_ + 1 && dt_ > 0 ? dt_ : s.dt;
        if (produced && took > 0) {
            // Collision took this part of the command away. Remove motion into
            // that surface; a projection cannot remove one contact twice.
            const Vec3 lost =
                native_movement::stepDelta(produced->requested, took, produced->distance) / took - s.achieved;
            if (length(lost) > .25f) {
                const Vec3 into = normalized(lost);
                velocity -= into * std::max(0.f, dot(velocity, into));
                travel -= into * std::max(0.f, dot(travel, into));
            }
        }
        step_ = s.step;
        serial_ = s.serial;
        controlled_ = s.controlled;
        dt_ = s.dt;
        return {s.position + travel, velocity, grounded};
    }
    // A submitted command: its velocity, the solver's velocity at its end and
    // the farthest its step may go (native_movement::commandDistance).
    void issued(uint64_t serial, Vec3 requested, Vec3 end, float distance = native_movement::farthestStep) {
        records_[next_++ % records_.size()] = {serial, requested, end, distance};
    }
    void reset() {
        *this = {};
    }

  private:
    struct Record {
        uint64_t serial{};
        Vec3 requested{}, end{};
        float distance = native_movement::farthestStep;
    };
    const Record* find(uint64_t serial) const {
        for (const auto& record : records_)
            if (serial && record.serial == serial)
                return &record;
        return nullptr;
    }
    std::array<Record, 8> records_{};
    size_t next_{};
    uint64_t step_{}, serial_{};
    float dt_{}; // length of the step observed last
    bool controlled_{};
};
struct Hand {
    Pose aim{};
    Vec3 relative{};
    uint32_t tracked{};
    float trigger{}, grip{};
};
struct Command {
    uint32_t magic = 0x5357434d, version = 7, bytes = sizeof(Command), focused{};
    uint64_t serial{};
    uint32_t leaseMs = 100, jump{};
    Hand hands[2];
    Vec3 move{};
    float trackingYaw{};
    float sampleSeconds = 1.f / 90.f;
    // Bit 0: the player asks the game for a move of its own now - its LT + RT
    // (a zip to a point), its Y (a web strike; a car's in a chase), a
    // prompt's button - and the swing lets it have the hero once it takes
    // him (AirYield). Zero until October 18, when it was `reserved`.
    // Bit 1: the right thumbstick's click alone, in VR play with WEB TIE or
    // WEB LINES on, when the game does not name its R3 (game_pad::NamedPress)
    // and the pad layer is off: the web tie's and a web line's click, and
    // held, the aim of the zip to a web line (web_line.hpp). No change of
    // layout or version: an older swing module refuses such a command (asks >
    // 1), and both come in one package.
    // Bit 2 (COMBAT RESPONSIVE): the player fights now (Input::fighting). Bit 1
    // was this before WEB TIE took it upstream (0.8.0); an older swing module
    // refuses such a command (asks > 3), and both come in one package.
    // Bit 3 (SPIDY RUN JUMP): A out of the swing's run is its own jump
    // (Input::runJump); bit 2 before the port to 0.8.0.
    // Bit 4 (WEB LINES): of bit 0's moves, the game's zip to a point: the one
    // that takes the player off a web line. Bit 5 (WEB LINES): the dodge's
    // button (B) held: on a web line, a dash along it. Bits 2 and 3 in pull
    // request #79, after #77's since; an older swing module refuses either
    // (asks > 15).
    uint32_t asks{};
    uint64_t sampleTimeNs = 1;
    // Version 3: a flip's tilt of the tracking space (Input::tilt). Version 2
    // (commandBytesV2, the probes') ends before it: level.
    Quat tilt{};
    // Version 4: a charged leap let go of (Input::leap, 0: none), and where
    // the head looks (Input::look; zero: unknown). Version 3
    // (commandBytesV3) ends before them.
    float leap{};
    Vec3 look{};
    // Version 5, hand wall climbing (HAND WALL CLIMBING): where each hand's
    // grip is and how far the controller's own grip is squeezed, whichever
    // button WEB BUTTON gives the webs (HandInput::wallPosition, wallGrip);
    // and 1 while both grips are held for the wall roll (Input::wallRotate).
    // Version 4 (commandBytesV4) ends before them.
    Vec3 wallPositions[2]{};
    float wallGrips[2]{};
    uint32_t wallRotate{}, actionMode{}; // version 5 ended in these 4 bytes of padding
    // Version 6, EYE AIM: where the eyes look (Input::gazeOrigin, gaze; gaze
    // zero: untracked, or EYE AIM off), and eyeAim bit 0: web balls go there
    // (Input::gazeShots). The aim previews say what the gaze meets (AimData::
    // gaze) whenever there is one. Version 5 (commandBytesV5) ends before them.
    Vec3 gazeOrigin{}, gaze{};
    // Version 7 interprets these formerly reserved slots; their offsets keep
    // the v6 prefix intact. Older versions never carry cancellation semantics.
    uint32_t eyeAim{}, animationCancel{};
    // Version 7: physical hand velocity stays measured in the tracking rig,
    // then rotates into the displayed animation frame for contact damage.
    // actionMode: bit0 contact-only animation; bits1/2 valid displayed L/R.
    Quat physicalToContact[2]{};
};
constexpr uint32_t commandBytesV2 = 168, commandBytesV3 = 184, commandBytesV4 = 200, commandBytesV5 = 240, commandBytesV6 = 272;
// attached: 0 none, else bits: 1 a web; 2 both hands hold it (Swing::shared:
// the other hand's web is the same one); 4 this hand took hold of it and did
// not shoot it (Web::taken; it stays when the other hand lets go); 8 this
// hand reels it in now (Swing::reeling; its trigger, reel_haptics.hpp).
struct WebState {
    uint32_t attached{}, bodyId{};
    Vec3 anchor{};
    float length{}, tension{};
};
// Data::wall: what holds the player on a wall or a ceiling now.
enum class Surface : uint32_t {
    none,
    wall,  // the swing's own wall (SwingConfig's walls): wall running, walking
    game,  // the game's wall crawl (its actor turned onto the surface)
    floor, // the game's crawl on a floor: a vent or a duct, too low to stand in
};
struct Data {
    uint32_t magic = 0x53574441, version = 4, bytes = sizeof(Data), status{};
    int64_t sequence{};
    uint64_t qpc{}, steps{}, controlled{}, serial{}, attaches{}, releases{}, zips{}, world{}, sourceStep{};
    Vec3 position{}, velocity{}, requested{};
    float dt{};
    uint32_t owned{}, error{};
    WebState webs[2];
    uint32_t grounded{}, collisionFlags{}, takeoff{}, misses{}, obstructed{}, trackingLost{};
    uint32_t takeoffPhase{}, takeoffAttempts{}, takeoffTimeouts{}, nativeContact{};
    // Version 4: the surface that holds the player (Surface), its normal out
    // of it as the world's rays find it (the game's actor rocks on a wall), and
    // the body's centre's distance from it (the swing's wall; the game's crawl
    // has the feet on it: 0). Then the walls the swing took the player onto
    // so far, and the jumps off them. mount: a standing player is going up
    // onto the swing's wall by the game's jump (Swing::mountBegun), 1 walked
    // at a wall, 2 out of the game's own crawl; the player's stick stays away
    // from the game meanwhile (it walked him on into the wall, and into its
    // crawl).
    uint32_t wall{};
    Vec3 wallNormal{};
    float wallDistance{};
    uint32_t walls{}, wallJumps{}, mount{};
};
static_assert(sizeof(Config) == 64 && sizeof(Hand) == 52 && sizeof(Command) == 304 &&
              offsetof(Command, tilt) == commandBytesV2 && offsetof(Command, leap) == commandBytesV3 &&
              offsetof(Command, wallPositions) == commandBytesV4 && offsetof(Command, wallRotate) == 232 &&
              offsetof(Command, gazeOrigin) == commandBytesV5 &&
              offsetof(Command, actionMode) == 236 && offsetof(Command, animationCancel) == 268 &&
              offsetof(Command, physicalToContact) == commandBytesV6 && sizeof(Quat) == 16);
static_assert(std::is_standard_layout_v<Command> && std::is_trivially_copyable_v<Command>);
constexpr uint32_t commandBytes(uint32_t version) {
    switch (version) {
    case 2: return commandBytesV2;
    case 3: return commandBytesV3;
    case 4: return commandBytesV4;
    case 5: return commandBytesV5;
    case 6: return commandBytesV6;
    case 7: return sizeof(Command);
    default: return 0;
    }
}
// Let real custom wall velocity drive the momentum pose without also asking
// the stock game to walk. Never suppress ground, vent or stale native input.
inline bool momentumWallDrive(bool requested, bool fresh, const Data& d) {
    return requested && fresh && d.status && d.owned && d.wall == static_cast<uint32_t>(Surface::wall);
}
// NATIVE RUN (game_pad::NativeRun): the game, not the swing, has the player
// on his feet on the ground. A fresh sample (the caller's `fresh`: under 100
// ms old), the swing not owning the body, grounded, on no wall of either kind
// (the game's crawl keeps its own RT, game_xr), no web held by either hand, no
// mount onto a wall, none of the swing's takeoffs, and the mover not in its
// airborne mode (collision flags 0x10). The same states the swing drives in
// (game_swing.cpp's `drive`: owned, attached, airborne) are all excluded: RT
// held off the ground is the game's own web swing.
inline bool gameOwnsGround(bool fresh, const Data& d) {
    return fresh && d.status && !d.owned && d.grounded && d.wall == static_cast<uint32_t>(Surface::none) &&
           !d.webs[0].attached && !d.webs[1].attached && !d.mount && !d.takeoff && !d.takeoffPhase &&
           !(d.collisionFlags & 0x10u);
}
// SpidySwingSettings: what the VR settings (the SPIDY VR tab in the game's
// Settings) change during play.
// grab: webs catch props and thugs (a swing started without them starts
// them here); off lets go of what they hold. maxSpeed: the speed limit, up to
// 65 m/s, which the movement module is started with for that reason.
// airWebs: a web that meets nothing within reach holds in open air there;
// off, it misses (Swing::allowAirAnchors). gravity: the swing's, m/s^2 (the
// VR settings' weight), up to maxGravity. Version 4, walls: a player in the
// air stays on the walls it comes into (Swing::allowWalls); 0, the game's own
// wall crawl takes it as before, and the wall he is on goes with the setting.
// 2 keeps that wall and takes no other (Swing::takeNewWalls), which is what
// indoors asks for: letting go outright drops him - the body is held 0.9 m
// clear of its wall, so a release is a fall straight past it, measured twice
// in the headset - while taking no new ones drops nobody and leaves the next
// wall to the game, where its vents, doors and prompts work. Version 3
// (settingsBytesV3, the probes') ends before it and leaves the walls as they
// are. Version 5,
// physical: webs are ropes (SwingConfig::physical: they bend round edges,
// are held by the hand and pay out on the ground); off, as before October
// 12. Version 4 (settingsBytesV4) ends before it and leaves the webs as they
// are. Version 6, noBlows: the hands (bit 0 the left, 1 the right) whose
// motion lands no blow, no punch and no web strike, as a hand whose web
// carries the player lands none: both while the phone is out (phone.hpp,
// October 19: a hand swung near a thug with the phone out punched him).
// Version 5 (settingsBytesV5) ends before it: both hands fight. Version 7,
// arcade: arcade webs (the VR settings' WEB STYLE: ARCADE; SwingConfig::
// arcade). Version 6 (settingsBytesV6) ends before it and leaves the webs'
// style as it is.
// Version 23 appends webTie (off by default): the web tie's gesture, and a
// webbed enemy who comes only on the yank (WEB TIE; web_tie.hpp).
// Version 31 appends webLines (off by default): web lines (WEB LINES;
// web_line.hpp). Version 24 in pull request #79, after #77's since.
// Version 30 appends webLandingSmooth (off by default) and webLandingImpact
// (m/s, 24 by default): a landing with a web held below that speed is a
// smooth one (Swing::allowWebLandingSmooth, SMOOTH WEB LANDING).
// Version 29 appends spidyRunStanding (off by default): with spidyRun, the
// run also starts on the ground from a stand, the stick held fully
// (Swing::allowGroundRunFromStanding, SPIDY RUN FROM STANDING).
// Version 28 appends spidyRun (off by default): a landing faster than the
// game's sprint is the swing's own run on the ground (Swing::allowGroundRun,
// SPIDY RUN).
// Version 27 appends softLanding (off by default): a fall with no web eases
// to 9 m/s down over its last metres (Swing::allowSoftLanding, SOFT LANDING).
// Version 26 appends momentumLeap (off by default): a charged leap keeps the
// run's speed (Swing::allowMomentumLeap, RUNNING LEAP).
// Version 25 appends webAngle, degrees -45 to 45 (0 as before), and
// wristShooters (off by default): the line each hand's webs leave along
// (web_aim.hpp, WEB ANGLE and WRIST SHOOTERS).
// Version 24 appends airSlingshot (off by default): the slingshot's draw and
// release in the air, from two webs under tension (Swing::allowAirSlingshot).
// Version 22 appends singleHandYankPercent: 50 to 300, how much speed a fast
// one-hand yank adds (Swing::singleHandYankStrength); 100 as before.
// Version 21 appends halfPunchDamage (off by default): punches and web
// strikes deal half their health damage (HALF PUNCH DAMAGE).
// Version 20 appends webSpeed (0 off, a web sticks at once; 1 to 3 it flies
// there) and version 19 webRange, m (100 as before): Swing::webTravel.
// Version 18 appends ropeSteering and version 17 twoHandZip, both off by
// default: the hand gestures of Swing::allowGestures.
// Version 16 appends slingshotSpeed, m/s: 0 the slingshot's launch as before,
// under maxSpeed; 20 to 300 its own (Swing::setSlingshotSpeed).
// Version 15 appends swingAssist (off by default): flying at something,
// speed into it is cut in time (Swing::allowSwingAssist).
// Version 14 appends softRelease: 0 a web lets go at once, as before; 1 to 3
// its last pull fades over 120, 180 or 240 ms (Swing::softRelease).
// Version 13 appends elasticWeb: 0 a hard rope as before, 1 to 3 webs that
// stretch under load, softer and softer (Swing::elasticWeb).
// Version 12 appends smartAnchor (off by default): with airWebs off, a shot
// that just misses catches a nearby real surface (Swing::allowSmartAnchor).
// Version 11 appends combatVisuals: off allows freely directed combat blows.
// Version 10 appends betterClimbing (off by default).
// Version 9 appends webSlingshot (off by default).
// Version 8 appends handClimb without changing the upstream phone or arcade
// fields. Older official settings keep their original layout and meaning.
struct Settings {
    uint32_t magic = 0x53575354, version = 40, bytes = sizeof(Settings), grab = 1;
    float maxSpeed = 32;
    uint32_t airWebs = 1;
    float gravity = 6;
    uint32_t walls = 1;
    uint32_t physical = 1;
    uint32_t noBlows{};
    uint32_t arcade{};
    uint32_t handClimb{};
    uint32_t webSlingshot{};
    uint32_t betterClimbing{};
    uint32_t combatVisuals = 1;
    uint32_t smartAnchor{};
    uint32_t elasticWeb{};
    uint32_t softRelease{};
    uint32_t swingAssist{};
    float slingshotSpeed{};
    uint32_t twoHandZip{};
    uint32_t ropeSteering{};
    float webRange = 100;
    uint32_t webSpeed{};
    uint32_t halfPunchDamage{};
    uint32_t singleHandYankPercent = 100;
    uint32_t webTie{};
    uint32_t airSlingshot{};
    int32_t webAngle{};
    uint32_t wristShooters{};
    uint32_t momentumLeap{};
    uint32_t softLanding{};
    uint32_t spidyRun{};
    uint32_t spidyRunStanding{};
    uint32_t webLandingSmooth{};
    float webLandingImpact = 24;
    uint32_t webLines{}; // Official version 31 prefix.
    // Version 39 appends the community pose controls; v32-v38 fork packets are not accepted.
    // HEAD PROBE, vr_settings::Values::headProbe): the read-only
    // head-bone diagnostic is on. The swing module only accepts it; nothing in
    // the swing reads or changes the body for it (head_probe.hpp).
    uint32_t headProbe{};
    // Version 32 (ANIMATED MOVES, vr_settings::Values::animatedMoves): the game's
    // animation drives the body for ground parkour / vaults / mantles (NATIVE
    // RUN). The swing only accepts it; game_xr / native_body gate the pose
    // (animated_moves.hpp). Off by default.
    uint32_t animatedMoves{};
    // Version 33 (ANIMATED HEAD, vr_settings::Values::animatedHead): while a
    // pose yield is active, Off|Level|Full head follow (0/1/2). The swing only
    // accepts it; game_xr / game_fp gate the eyes (animated_moves.hpp). Off by
    // default.
    uint32_t animatedHead{};
    // Version 34 (ANIMATED AIR, vr_settings::Values::animatedAir): pose yield
    // for freefall / swing / web air. Physics stays with the swing; the module
    // only accepts the switch (animated_moves.hpp). Off by default.
    uint32_t animatedAir{};
    // Version 35 (ANIMATED STEERING): while yielding, L-stick along actor
    // facing. No model-yaw write. Off by default (animated_steering.hpp).
    uint32_t animatedSteering{};
    // Version 36 (ANIMATED ARMS): per-arm controller overlay while yielding.
    // Off by default (animated_arms.hpp).
    uint32_t animatedArms{};
    // Version 37 (ANIMATED TORSO): with ANIMATED ARMS, bound waist→chest toward
    // controller reaches. Off by default (animated_torso.hpp).
    uint32_t animatedTorso{};
    // Version 38 (ANIMATED MODEL YAW): R-stick hero turn via walk facing while
    // yielding. Off by default (animated_model_yaw.hpp).
    uint32_t animatedModelYaw{};
    // Version 40 (RUN SPEED, vr_settings::Values::runSpeedPercent): 50 to 200,
    // the speed SpidyVR's own ground run puts the body at (Swing::
    // setRunSpeedPercent). 100 as before; a v39 packet keeps 100.
    uint32_t runSpeedPercent = 100;
    // Version 40 too (SOFT LOCK, vr_settings::Values::softLockMetres): 0 to
    // 40 metres, GrabConfig::softLock (0 off). A v39 packet keeps it off.
    uint32_t softLockMetres = 15;
};
constexpr uint32_t settingsBytesV3 = 28, settingsBytesV4 = 32, settingsBytesV5 = 36, settingsBytesV6 = 40,
                   settingsBytesV7 = 44, settingsBytesV8 = 48, settingsBytesV9 = 52, settingsBytesV10 = 56,
                   settingsBytesV11 = 60, settingsBytesV12 = 64, settingsBytesV13 = 68, settingsBytesV14 = 72,
                   settingsBytesV15 = 76, settingsBytesV16 = 80, settingsBytesV17 = 84, settingsBytesV18 = 88,
                   settingsBytesV19 = 92, settingsBytesV20 = 96, settingsBytesV21 = 100,
                   settingsBytesV22 = 104, settingsBytesV23 = 108, settingsBytesV24 = 112,
                   settingsBytesV25 = 120, settingsBytesV26 = 124, settingsBytesV27 = 128,
                   settingsBytesV28 = 132, settingsBytesV29 = 136, settingsBytesV30 = 144,
                   settingsBytesV31 = 148, settingsBytesV32 = 152, settingsBytesV33 = 156,
                   settingsBytesV34 = 160, settingsBytesV35 = 164, settingsBytesV36 = 168,
                   settingsBytesV37 = 172, settingsBytesV38 = 176, settingsBytesV39 = 180;
static_assert(
    sizeof(Settings) == 188 && offsetof(Settings, runSpeedPercent) == settingsBytesV39 &&
    offsetof(Settings, walls) == settingsBytesV3 &&
    offsetof(Settings, physical) == settingsBytesV4 && offsetof(Settings, noBlows) == settingsBytesV5 &&
    offsetof(Settings, arcade) == settingsBytesV6 && offsetof(Settings, handClimb) == settingsBytesV7 &&
    offsetof(Settings, webSlingshot) == settingsBytesV8 &&
    offsetof(Settings, betterClimbing) == settingsBytesV9 &&
    offsetof(Settings, combatVisuals) == settingsBytesV10 &&
    offsetof(Settings, smartAnchor) == settingsBytesV11 &&
    offsetof(Settings, elasticWeb) == settingsBytesV12 &&
    offsetof(Settings, softRelease) == settingsBytesV13 &&
    offsetof(Settings, swingAssist) == settingsBytesV14 &&
    offsetof(Settings, slingshotSpeed) == settingsBytesV15 &&
    offsetof(Settings, twoHandZip) == settingsBytesV16 &&
    offsetof(Settings, ropeSteering) == settingsBytesV17 &&
    offsetof(Settings, webRange) == settingsBytesV18 && offsetof(Settings, webSpeed) == settingsBytesV19 &&
    offsetof(Settings, halfPunchDamage) == settingsBytesV20 &&
    offsetof(Settings, singleHandYankPercent) == settingsBytesV21 &&
    offsetof(Settings, webTie) == settingsBytesV22 && offsetof(Settings, airSlingshot) == settingsBytesV23 &&
    offsetof(Settings, webAngle) == settingsBytesV24 &&
    offsetof(Settings, momentumLeap) == settingsBytesV25 &&
    offsetof(Settings, softLanding) == settingsBytesV26 && offsetof(Settings, spidyRun) == settingsBytesV27 &&
    offsetof(Settings, spidyRunStanding) == settingsBytesV28 &&
    offsetof(Settings, webLandingSmooth) == settingsBytesV29 &&
    offsetof(Settings, webLines) == settingsBytesV30 && offsetof(Settings, headProbe) == settingsBytesV31 &&
    offsetof(Settings, animatedMoves) == settingsBytesV32 &&
    offsetof(Settings, animatedHead) == settingsBytesV33 &&
    offsetof(Settings, animatedAir) == settingsBytesV34 &&
    offsetof(Settings, animatedSteering) == settingsBytesV35 &&
    offsetof(Settings, animatedArms) == settingsBytesV36 &&
    offsetof(Settings, animatedTorso) == settingsBytesV37 &&
    offsetof(Settings, animatedModelYaw) == settingsBytesV38);
// SpidySwingWebTurns (WEB AIM: MODEL FOREARM, web_aim::ForearmAim): the turn
// each hand's aim pose takes before WEB ANGLE and WRIST SHOOTERS, in the aim
// pose's own frame, sent by the VR worker each frame while the setting is on
// (and the identity once when it goes off). Only the webs' line takes it - the
// swing, the grab, the web balls and the aim previews - never the fists, the
// walls or the vents. Turns older than webTurnsMaxAgeMs are the identity, so a
// worker that stops sending leaves the controller's line. An optional export:
// a module without it aims as before.
struct WebTurns {
    uint32_t magic = 0x53575754, version = 1, bytes = sizeof(WebTurns), active{};
    Quat turns[2]{};
};
static_assert(sizeof(WebTurns) == 48);
constexpr uint64_t webTurnsMaxAgeMs = 250;
inline bool validWebTurns(const WebTurns& t) {
    if (t.magic != 0x53575754 || t.version != 1 || t.bytes != sizeof(WebTurns) || t.active > 1)
        return false;
    for (const auto& q : t.turns) {
        const float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (!std::isfinite(n) || std::abs(n - 1) > .01f)
            return false;
    }
    return true;
}
// SpidyWebFlightSample: each hand's web in flight (WEB SPEED). phases: 0 no
// web, 1 flying, 2 just arrived and catching softly, 3 holding.
struct WebFlightData {
    uint32_t magic = 0x57464c54, version = 1, bytes = sizeof(WebFlightData), reserved{};
    uint32_t phases[2]{};
    Vec3 tips[2]{};
    float durations[2]{}, progress[2]{1, 1}, coupling[2]{};
};
static_assert(sizeof(WebFlightData) == 72);
// SpidySlingshotSample: charge in [0, 1], zero after cancel or release.
struct SlingshotData {
    uint32_t magic = 0x53534c47, version = 1, bytes = sizeof(SlingshotData);
    float charge{};
};
static_assert(sizeof(SlingshotData) == 16);
// Physical webs (SpidyRopeData): what the ropes did, for the session report.
struct Ropes {
    uint32_t magic = 0x53575250, version = 3, bytes = sizeof(Ropes), physical{};
    int64_t sequence{};
    // Bends made round an edge and taken off again; webs let go because
    // something stayed in the way (no edge found, or too many); steps a web
    // grew while the game walked the player.
    uint64_t wraps{}, unwraps{}, cut{}, paidOut{};
    // Each hand's web now: its bends, the rope beyond the last (m), and all
    // of it from its end (m).
    uint32_t bends[2]{};
    float free[2]{}, whole[2]{};
    // Version 2: grips taken on the other hand's web (EventKind::Join), and
    // whether both hands hold one web now. Version 3, in version 2's spare
    // word: the thin or loose things arcade webs went through (EventKind::
    // Pass: a shot past one, a swing's line coming onto one).
    uint64_t joins{};
    uint32_t shared{}, passed{};
};
static_assert(sizeof(Ropes) == 96);
// Separate telemetry keeps existing swing/probe packets compatible.
struct ClimbData {
    uint32_t magic = 0x53434c44, version = 2, bytes = sizeof(ClimbData), status{};
    int64_t sequence{};
    uint64_t contacts[2]{};
    uint32_t hands{}, reserved{};
    Vec3 crawlMove{}; // world-space stick intent for the game's supported crawl
    uint32_t crawl{};
    uint64_t sampleTimeNs{};
};
static_assert(sizeof(ClimbData) == 72 && offsetof(ClimbData, crawlMove) == 48);
// Only a current outdoor attachment can interrupt the visual momentum run.
// A squeezed grip without contact, a vent hold or stale feedback cannot.
inline bool outdoorClimbGrip(const ClimbData& d, uint64_t nowNs) {
    return d.magic == 0x53434c44 && d.version == 2 && d.bytes == sizeof(ClimbData) && d.status &&
        d.crawl == 0 && d.hands > 0 && d.hands <= 3 && d.sampleTimeNs &&
        nowNs >= d.sampleTimeNs && nowNs - d.sampleTimeNs <= 100'000'000;
}
// Expired feedback, leaving the crawl or a stick press never repeats a pull.
inline Vec3 crawlPull(const ClimbData& d, bool allowed, uint64_t nowNs) {
    if (!allowed || d.version != 2 || d.bytes != sizeof(ClimbData) || !d.status || d.crawl != 1 ||
        !d.hands || d.hands > 3 || !d.sampleTimeNs || nowNs < d.sampleTimeNs ||
        nowNs - d.sampleTimeNs > 100'000'000 || !finite(d.crawlMove)) return {};
    return limited(d.crawlMove, 1.f);
}
// Only measured hand pulls bypass the game's Xbox stick dead zone. Otherwise
// a slow physical pull becomes a stick too small to move the player at all.
// x/z are native right/forward, not tracking-space axes.
inline Vec3 crawlStick(Vec3 pull, Vec3 ahead, Vec3 side) {
    if (!finite(pull) || !finite(ahead) || !finite(side)) return {};
    Vec3 stick{dot(pull, side), 0, dot(pull, ahead)};
    const float amount = length(stick);
    if (amount < .012f) return {}; // under 3 cm/s: stationary controller noise
    return stick * ((.25f + .75f * std::min(amount, 1.f)) / amount);
}
// Contact counters survive an attachment released between headset samples.
class ClimbPulses {
  public:
    unsigned update(const ClimbData& data, bool active) {
        if (!data.status)
            return 0;
        unsigned pulses{};
        for (unsigned i = 0; i < 2; ++i) {
            if (active && data.status && data.contacts[i] > seen_[i])
                pulses |= 1u << i;
            seen_[i] = data.contacts[i];
        }
        return pulses;
    }
  private:
    uint64_t seen_[2]{};
};
// The edges each hand's web bends round (SpidySwingBends), for drawing it
// round them to where it sticks. Data's webs end at the nearest edge, the
// one the swing holds the body from, and the game's rope drawn to it jumped
// from where the web stuck to each new edge (the player, October 15: "the
// rope origin point seem to teleport to the new edge").
struct Bends {
    uint32_t magic = 0x53574245, version = 1, bytes = sizeof(Bends), status{};
    int64_t sequence{};
    struct Hand {
        uint32_t count{};        // edges in `points`; 0: the web lies straight
        Vec3 end{};              // where the web sticks
        Vec3 points[maxBends]{}; // the edges, the one nearest the hand first
    } hands[2];
};
static_assert(sizeof(Bends) == 200);
// Side channel from XR: the visual tuck's inertia (1 = unchanged). Not a
// Settings or Command field, and not versioned with those packets.
struct PoseCoupling {
    uint32_t magic = 0x53575043, version = 1, bytes = sizeof(PoseCoupling);
    float inertia = 1;
};
static_assert(sizeof(PoseCoupling) == 16);
// Side channel from XR (POINT JUMP, point_jump.hpp): launch at `velocity` on
// the next step, from the ground or Spidy's wall (Swing::pointJump). `serial`
// rises with each new launch; the same serial again is ignored. Not a Command
// or Settings field and not versioned with them (an older swing module has no
// SpidySwingPointJump export; XR then never fires). `normal`: the surface's,
// for the diagnostics only.
struct PointJumpAsk {
    uint32_t magic = 0x5357504a, version = 1, bytes = sizeof(PointJumpAsk), serial{};
    Vec3 velocity{}, normal{};
};
static_assert(sizeof(PointJumpAsk) == 40);
inline bool validPointJump(const PointJumpAsk& a) {
    return a.magic == 0x5357504a && a.version == 1 && a.bytes == sizeof(PointJumpAsk) && finite(a.velocity) &&
           length(a.velocity) > .1f && length(a.velocity) <= 80.f && finite(a.normal);
}
// Side channel from XR (QUICK SIDESTEP, sidestep.hpp): a burst across the
// ground at `velocity` (level, real m/s) for moveSeconds, then a hard stop
// (zero speed across) for stopSeconds; the swing drives it while he stays
// on the ground and stops it at an edge or a wall. `serial` rises with each
// burst; the same serial again is ignored. flags bit 0: the game's dodge
// (game_dodge, as a web's carry) answers the blows it warns of during the
// burst. Not a Command or Settings field and not versioned with them (an
// older swing module has no SpidySwingSidestep export; XR then never fires).
struct SidestepAsk {
    uint32_t magic = 0x53575353, version = 1, bytes = sizeof(SidestepAsk), serial{};
    Vec3 velocity{};
    float moveSeconds{}, stopSeconds{};
    uint32_t flags{};
};
static_assert(sizeof(SidestepAsk) == 40);
inline bool validSidestep(const SidestepAsk& a) {
    return a.magic == 0x53575353 && a.version == 1 && a.bytes == sizeof(SidestepAsk) && finite(a.velocity) &&
           std::abs(a.velocity.y) < 1e-3f && length(a.velocity) > 1.f && length(a.velocity) <= 25.f &&
           std::isfinite(a.moveSeconds) && a.moveSeconds > .02f && a.moveSeconds <= .5f &&
           std::isfinite(a.stopSeconds) && a.stopSeconds >= 0 && a.stopSeconds <= .2f;
}
// SpidySidestepLive: the swing's bursts, for XR's log (the swing has none).
// `started` / `ended` count up (ended after the rest is written); for the
// last one: its serial, why it ended (sidestep::Stop), how far it went
// across (m) and for how long (ms), and whether the dodge was on.
struct SidestepLive {
    uint32_t magic = 0x5353544c, version = 1, bytes = sizeof(SidestepLive), serial{};
    uint32_t started{}, ended{}, stop{}, iframes{};
    float metres{}, ms{};
};
static_assert(sizeof(SidestepLive) == 40);
// Side channel from XR (MANUAL SWING KICK, swing_kick.hpp): the settings and, on a flick, the ask. `flags`
// bit 0: swing_kick_manual is on; bit 1: swing_kick_auto is OFF (the auto dive holds back); bit 2: the
// legs are engaged (Y + X) now. `serial` rises with each flick; the same serial again is not a new
// flick (the flags may be sent again with it). coneDegrees and ropeMetres are the kick's own (30 and
// 1.2 by default). Not a Command or Settings field and not versioned with them (an older swing module has
// no SpidySwingKick export; XR then never kicks, and the auto dive is as it was).
struct SwingKickAsk {
    uint32_t magic = 0x5357504b, version = 1, bytes = sizeof(SwingKickAsk), serial{};
    uint32_t flags{};
    float coneDegrees = 30.f, ropeMetres = 1.2f;
    uint32_t reserved{};
};
static_assert(sizeof(SwingKickAsk) == 32);
inline bool validSwingKick(const SwingKickAsk& a) {
    return a.magic == 0x5357504b && a.version == 1 && a.bytes == sizeof(SwingKickAsk) && !(a.flags & ~7u) &&
           std::isfinite(a.coneDegrees) && a.coneDegrees >= 5.f && a.coneDegrees <= 60.f &&
           std::isfinite(a.ropeMetres) && a.ropeMetres >= 0.f && a.ropeMetres <= 3.f;
}
// SpidySwingKickLive: the kick's account for XR's log. `events` counts up last; for the latest event
// (1 armed, 2 hit, 3 miss, 4 refused): its target (an actor record), the cone (degrees off the swing's
// velocity), speed (real m/s) and distance (m) when it came, the rope assist used (m, signed) and the
// steering used (m/s^2), and for a refusal swing_kick::Refusal. `state` is the kick's Phase now (0 idle,
// 1 active, 2 fading); `point` where his middle was.
struct SwingKickLive {
    uint32_t magic = 0x53574b4c, version = 1, bytes = sizeof(SwingKickLive), state{};
    uint32_t asked{}, armed{}, hits{}, misses{}, refused{}, blows{}, events{}, lastEvent{}, refusal{};
    uint64_t target{};
    Vec3 point{};
    float cone{}, speed{}, distance{}, assist{}, steer{};
    uint32_t autoOff{}, reserved{};
};
static_assert(sizeof(SwingKickLive) == 104);
// Side channel from XR (AIR KICKS, air_kick.hpp, K2): the settings, sent whenever they change and again every
// second, and on a flick the ask (`serial` new): `gesture` (air_kick::Gesture, 1 dive, 2 double, 3 roundhouse,
// 4 freeform, 5 sweep: GROUND KICKS, K3), `side` (-1 left, +1 right) and `angle` (degrees from up) of the flick, the eyes' offset from the
// feet and the gaze (both in the world's axes). `flags` bit 0: kicks is on; bit 1: the legs are engaged; bit 2: ground_kicks is on (GROUND KICKS,
// ground_kick.hpp, K3: the same ask, standing and slow); bit 3: enemy_joints is on (R1: the swing reads the
// joints of the enemies a kick may meet, enemy_joints.hpp); bit 4: enemy_limb_zones (the shins and forearms
// too: only after probe_c0_skeleton.py).
// gazeCone is kick_gaze_cone_deg (25). Not a Command or Settings field and not versioned with them (an older
// swing module has no SpidyAirKick export; XR then sends none and no air kick happens).
struct AirKickAsk {
    uint32_t magic = 0x5357414b, version = 1, bytes = sizeof(AirKickAsk), serial{};
    uint32_t flags{}, gesture{};
    int32_t side{};
    float gazeCone = 25.f;
    Vec3 eyeOffset{0, 1.6f, 0}, gaze{0, 0, -1};
    float angle{};
    uint32_t reserved{};
};
static_assert(sizeof(AirKickAsk) == 64);
inline bool validAirKick(const AirKickAsk& a) {
    return a.magic == 0x5357414b && a.version == 1 && a.bytes == sizeof(AirKickAsk) && !(a.flags & ~31u) &&
           a.gesture <= 5 && a.side >= -1 && a.side <= 1 && std::isfinite(a.gazeCone) && a.gazeCone >= 8.f &&
           a.gazeCone <= 60.f && std::isfinite(a.angle) && std::isfinite(a.eyeOffset.x) &&
           std::isfinite(a.eyeOffset.y) && std::isfinite(a.eyeOffset.z) && std::isfinite(a.gaze.x) &&
           std::isfinite(a.gaze.y) && std::isfinite(a.gaze.z) && a.eyeOffset.y > 0.f && a.eyeOffset.y < 3.f &&
           a.gaze.x * a.gaze.x + a.gaze.y * a.gaze.y + a.gaze.z * a.gaze.z > .25f;
}
// SpidyAirKickLive: the kick's account for XR's log, legs and feel. `events` counts up last; for the latest
// event (1 started, 2 hit, 3 miss, 4 refused): its gesture, side, zone (air_kick::Zone), target (an actor
// record), the foot's speed (real m/s) and the blow's strength (0..1), and for a refusal air_kick::Refusal.
// `state` is the kick's Phase now (0 idle, 1 kicking, 2 recovering); `progress` 0..1 through the kick.
struct AirKickLive {
    uint32_t magic = 0x53574b4c, version = 3, bytes = sizeof(AirKickLive), state{};
    uint32_t gesture{}, zone{}, refusal{}, lastEvent{};
    int32_t side{};
    uint32_t asked{}, started{}, hits{}, misses{}, refused{}, flies{}, events{};
    uint64_t target{};
    Vec3 point{};
    float foot{}, strength{}, progress{}, angle{};
    uint32_t reserved{};
    // K3: the kick's foot from the player's feet while it runs (world axes, metres; XR poses a standing kick's
    // leg to it) and the player's speed (m/s) the swing judged it by.
    float footX{}, footY{}, footZ{}, speed{};
    // R1 (ENEMY JOINTS): skeletons read, reads that gave no head and chest (the bands decided), blows whose
    // zone came from the joints, and which roles the last rig read names (enemy_joints::Role bits), with its
    // joint count, why the last failure was (game_joints::Failure) and the roles the last read got.
    uint32_t jointReads{}, jointMisses{}, jointZones{}, jointNamed{}, jointCount{}, jointWhy{}, jointValid{}, jointPad{};
};
static_assert(sizeof(AirKickLive) % 8 == 0);
// Side channel from XR (GRIP ACROSS THE GAME, grip.hpp): the settings, sent whenever they change and again
// every second (a swing module started anew starts from none). `flags` bit 0: wall_brake, bit 1:
// grip_fling. mu: the brake's friction (0.9); gain and vMax: the fling's (1.3, 9 m/s). Not a Command or
// Settings field and not versioned with them (an older swing module has no SpidySwingGrip export; XR then
// sends none and the grip is as it was).
struct GripAsk {
    uint32_t magic = 0x53574750, version = 1, bytes = sizeof(GripAsk), serial{};
    uint32_t flags{};
    float mu = .9f, gain = 1.3f, vMax = 9.f;
};
static_assert(sizeof(GripAsk) == 32);
inline bool validGrip(const GripAsk& a) {
    return a.magic == 0x53574750 && a.version == 1 && a.bytes == sizeof(GripAsk) && !(a.flags & ~3u) &&
           std::isfinite(a.mu) && a.mu >= .3f && a.mu <= 1.5f && std::isfinite(a.gain) && a.gain >= .5f &&
           a.gain <= 3.f && std::isfinite(a.vMax) && a.vMax >= 3.f && a.vMax <= 20.f;
}
// SpidySwingGripLive: the grip's account for XR's feel and log. `brakeHands` (bits) is the hands braking
// now, with the slide speed (m/s) and the braking (m/s^2); `flings` counts up, and the last one's source
// (1 ground, 2 wall, 3 air), speed (m/s) and velocity; `grips` the fling grips begun, `slow` the pulls
// let go of too slowly, `refused` the flings with nowhere to go.
struct GripLive {
    uint32_t magic = 0x5357474c, version = 1, bytes = sizeof(GripLive), brakeHands{};
    float brakeSpeed{}, brakeDecel{};
    uint32_t brakes{}, grips{}, flings{}, groundFlings{}, wallFlings{}, airFlings{}, slow{}, refused{};
    uint32_t source{};
    float flingSpeed{};
    Vec3 flingVelocity{};
    uint32_t reserved{};
};
static_assert(sizeof(GripLive) == 80);
// Side channel from XR (HAND GRAB, hand_grab.hpp, G2): the settings, sent whenever they change and again
// every second. `flags` bit 0: hand_grab. force: hand_grab_force (N per hand, 1200); holdCost:
// enemy_hold_cost (1). Not a Command or Settings field and not versioned with them (an older swing module has
// no SpidySwingHandGrab export; XR then sends none and no hand takes anything).
struct HandGrabAsk {
    uint32_t magic = 0x53574847, version = 1, bytes = sizeof(HandGrabAsk), serial{};
    uint32_t flags{};
    float force = 1200.f, holdCost = 1.f;
    uint32_t reserved{};
};
static_assert(sizeof(HandGrabAsk) == 32);
inline bool validHandGrab(const HandGrabAsk& a) {
    return a.magic == 0x53574847 && a.version == 1 && a.bytes == sizeof(HandGrabAsk) && !(a.flags & ~1u) &&
           std::isfinite(a.force) && a.force >= 400.f && a.force <= 4000.f && std::isfinite(a.holdCost) &&
           a.holdCost >= 0.f && a.holdCost <= 5.f;
}
// SpidySwingHandGrabLive: the hand grab's account for XR's log and feel. `holding` (bits) the hands holding
// by palm now; the counts of bodies taken (`enemies`, `props`), thrown, slammed and only let go of, lost
// (shaken off) and knocked into flight (`knocks`); the latest action (1 hold, 2 throw, 3 slam, 4 let go) on
// what (1 enemy, 2 prop), with its mass (kg) and speed (m/s); `serial` counts up last.
struct HandGrabLive {
    uint32_t magic = 0x5357484c, version = 1, bytes = sizeof(HandGrabLive), holding{};
    uint32_t grabs{}, enemies{}, props{}, throws{}, slams{}, drops{}, lost{}, knocks{};
    uint32_t lastAction{}, lastKind{};
    float lastMass{}, lastSpeed{};
    uint32_t serial{};
};
static_assert(sizeof(HandGrabLive) == 68);
// Side channel from XR (PHYSICAL WEB TETHERS, tether.hpp, W1): the settings, sent whenever they change and again
// every second. `flags` bit 0: physical_tethers. particles: tether_particles (8 to 32, 24); substeps:
// tether_substeps (2 to 16, 8). Not a Command or Settings field and not versioned with them (an older swing
// module has no SpidyTether export; XR then sends none and the webs are as they were).
struct TetherAsk {
    uint32_t magic = 0x53575448, version = 1, bytes = sizeof(TetherAsk), serial{};
    uint32_t flags{}, particles = 24, substeps = 8, reserved{};
};
static_assert(sizeof(TetherAsk) == 32);
inline bool validTether(const TetherAsk& a) {
    return a.magic == 0x53575448 && a.version == 1 && a.bytes == sizeof(TetherAsk) && !(a.flags & ~1u) &&
           a.particles >= 8 && a.particles <= 32 && a.substeps >= 2 && a.substeps <= 16;
}
// SpidyTetherLive: each hand's line to draw and the account, for XR. `lines[i]`: `count` points past the ends
// (at most 6: the game's rope takes 8 in all), `end` where it meets the target, `tension` 0 to 1; `flags` bit 0
// the line is live, 1 the hand is spinning its target, 2 the line was kept after a throw. The counts: whips,
// spins begun, slings, throws that kept the line, lines cut; the latest action (1 whip, 2 spin, 3 sling,
// 4 throw on a tether, 5 cut) with its speed (m/s); `serial` counts up last.
struct TetherLiveLine {
    uint32_t count{}, flags{};
    float tension{};
    Vec3 end{};
    Vec3 points[6]{};
};
struct TetherLive {
    uint32_t magic = 0x5357544c, version = 1, bytes = sizeof(TetherLive), active{};
    uint32_t whips{}, spins{}, slings{}, throws{}, cuts{}, lastAction{};
    float lastSpeed{};
    TetherLiveLine lines[2];
    uint32_t serial{};
};
static_assert(sizeof(TetherLiveLine) == 96 && sizeof(TetherLive) == 240);
// A web's edges as Bends hands them on: its anchor is the edge nearest the
// hand, and each edge records the one before it, back to where it sticks.
inline Bends::Hand edgesOf(const Web& w) {
    Bends::Hand out;
    if (!w.attached)
        return out;
    out.end = w.end();
    out.count = static_cast<uint32_t>(std::clamp(w.bent, 0, maxBends));
    for (int k = 0; k < static_cast<int>(out.count); ++k)
        out.points[k] = k == 0 ? w.anchor : w.bends[static_cast<size_t>(w.bent - k)].from;
    return out;
}
// The movement module's own limit: every speed the VR settings offer.
constexpr float motionSpeedLimit = 65;
// The most gravity a swing takes, m/s^2: every weight the VR settings offer.
constexpr float maxGravity = 30;
inline bool validSettings(const Settings& s) {
    return s.magic == 0x53575354 &&
           ((s.version == 3 && s.bytes == settingsBytesV3) ||
            (s.version == 4 && s.bytes == settingsBytesV4) ||
            (s.version == 5 && s.bytes == settingsBytesV5) ||
            (s.version == 6 && s.bytes == settingsBytesV6) ||
            (s.version == 7 && s.bytes == settingsBytesV7) ||
            (s.version == 8 && s.bytes == settingsBytesV8) ||
            (s.version == 9 && s.bytes == settingsBytesV9) ||
            (s.version == 10 && s.bytes == settingsBytesV10) ||
            (s.version == 11 && s.bytes == settingsBytesV11) ||
            (s.version == 12 && s.bytes == settingsBytesV12) ||
            (s.version == 13 && s.bytes == settingsBytesV13) ||
            (s.version == 14 && s.bytes == settingsBytesV14) ||
            (s.version == 15 && s.bytes == settingsBytesV15) ||
            (s.version == 16 && s.bytes == settingsBytesV16) ||
            (s.version == 17 && s.bytes == settingsBytesV17) ||
            (s.version == 18 && s.bytes == settingsBytesV18) ||
            (s.version == 19 && s.bytes == settingsBytesV19) ||
            (s.version == 20 && s.bytes == settingsBytesV20) ||
            (s.version == 21 && s.bytes == settingsBytesV21) ||
            (s.version == 22 && s.bytes == settingsBytesV22) ||
            (s.version == 23 && s.bytes == settingsBytesV23) ||
            (s.version == 24 && s.bytes == settingsBytesV24) ||
            (s.version == 25 && s.bytes == settingsBytesV25) ||
            (s.version == 26 && s.bytes == settingsBytesV26) ||
            (s.version == 27 && s.bytes == settingsBytesV27) ||
            (s.version == 28 && s.bytes == settingsBytesV28) ||
            (s.version == 29 && s.bytes == settingsBytesV29) ||
            (s.version == 30 && s.bytes == settingsBytesV30) ||
            (s.version == 31 && s.bytes == settingsBytesV31) ||
            (s.version == 39 && s.bytes == settingsBytesV39) ||
            (s.version == 40 && s.bytes == sizeof(Settings))) &&
           s.grab <= 1 && std::isfinite(s.maxSpeed) && s.maxSpeed > 0 && s.maxSpeed <= motionSpeedLimit &&
           s.airWebs <= 1 && std::isfinite(s.gravity) && s.gravity >= 0 && s.gravity <= maxGravity &&
           s.walls <= 4 && s.physical <= 1 && s.noBlows <= 3 && s.arcade <= 1 && s.handClimb <= 1 &&
           s.webSlingshot <= 1 && s.betterClimbing <= 1 && s.combatVisuals <= 1 && s.smartAnchor <= 1 &&
           s.elasticWeb <= 3 && s.softRelease <= 3 && s.swingAssist <= 1 && std::isfinite(s.slingshotSpeed) &&
           (s.slingshotSpeed == 0 || (s.slingshotSpeed >= 20 && s.slingshotSpeed <= 300)) &&
           s.twoHandZip <= 1 && s.ropeSteering <= 1 && std::isfinite(s.webRange) && s.webRange >= 10 &&
           s.webRange <= 1000 && s.webSpeed <= 3 && s.halfPunchDamage <= 1 &&
           (s.version < 22 || (s.singleHandYankPercent >= 50 && s.singleHandYankPercent <= 300)) &&
           s.webTie <= 1 && (s.version < 24 || s.airSlingshot <= 1) &&
           (s.version < 25 || (s.webAngle >= -45 && s.webAngle <= 45 && s.wristShooters <= 2)) &&
           (s.version < 26 || s.momentumLeap <= 1) && (s.version < 27 || s.softLanding <= 1) &&
           (s.version < 28 || s.spidyRun <= 1) && (s.version < 29 || s.spidyRunStanding <= 1) &&
           (s.version < 30 || (s.webLandingSmooth <= 1 && std::isfinite(s.webLandingImpact) &&
                               s.webLandingImpact >= 5 && s.webLandingImpact <= 100)) &&
           (s.version < 39 || s.headProbe <= 1) && (s.version < 39 || s.animatedMoves <= 1) &&
           (s.version < 39 || s.animatedHead <= 2) && (s.version < 39 || s.animatedAir <= 1) &&
           (s.version < 39 || s.animatedSteering <= 1) && (s.version < 39 || s.animatedArms <= 1) &&
           (s.version < 39 || s.animatedTorso <= 1) && (s.version < 39 || s.animatedModelYaw <= 1) &&
           (s.version < 31 || s.webLines <= 1) &&
           (s.version < 40 || (s.runSpeedPercent >= 50 && s.runSpeedPercent <= 200 && s.softLockMetres <= 40));
}
static_assert(sizeof(WebState) == 28 && sizeof(Data) == 272 && offsetof(Data, wall) == 240);
// The player is in the air: Spidy's own flight, or the game's jump or fall,
// where the mover is unsupported and in its airborne mode (collision flags
// 0x10: the steps the game's air state runs). A perch, a wall crawl and a
// wall run are neither, and a sample whose step is over 100 ms older than
// `nowQpc` says nothing (a perch or ledge the game animates without steps).
// A body the swing runs on the ground (SPIDY RUN, SMOOTH WEB LANDING's carry:
// owned and grounded) is not in the air. Until November 11 every owned step
// was: REALISTIC FLIPS turned a player who walked into SPIDY RUN and moved
// the arms ("when walking if you move your arms you start flipping on
// ground", with a video), the wind's body haptics blew there, and the A of a
// jump there never reached the game's jump that game_pad::JumpRelay waits to
// give it (with SPIDY RUN JUMP off). One 0.8.5 session ran 495 such steps,
// all at a standstill up and down and 10 to 14 m/s across.
inline bool airborne(const Data& d, int64_t nowQpc, int64_t qpcFrequency) {
    const bool recent = d.qpc && static_cast<int64_t>(d.qpc) > nowQpc - qpcFrequency / 10;
    return recent && (d.status == 1 || d.status == 2) && !d.grounded &&
           (d.owned || (d.collisionFlags & 0x10u));
}
// What a grip press would do now with each hand (SpidyAimSample), for the
// headset's aim markers: worked out in the world-query callback with the rays
// and target picks the press itself uses, from the latest input. It costs a
// few rays a step, so only while someone samples it within aimLeaseMs.
enum class AimKind : uint32_t {
    none,      // the hand is untracked, or its web holds something already; or nothing is
               // within reach and webs do not hold in open air (Settings::airWebs)
    anchor,    // the web attaches to this surface
    air,       // nothing within reach: the web attaches in the air at maximum reach
    blocked,   // the web misses: what the ray meets cannot hold it, or the body has no clear line
    prop,      // the web catches this prop
    character, // the web catches this character
    strand,    // the hand is on the other hand's web: the press takes hold of that web, at `point` its end
};
struct Aim {
    uint32_t kind{}; // AimKind
    float radius{};  // a target's radius
    Vec3 point{};    // where the web goes: the surface or air point, or the target's centre
    Vec3 normal{};   // the surface's normal (anchor, blocked); zero for none
};
struct AimData {
    uint32_t magic = 0x5357414d, version = 2, bytes = sizeof(AimData), status{};
    int64_t sequence{};
    uint64_t serial{}; // the input command the aims were worked out from
    Aim hands[2];
    // Version 2: what the eyes look at (Command::gaze; EYE AIM): anchor, the
    // first surface on their line within gazeRange, at `point`; air, none,
    // `point` that far out along it; none, no gaze in the command.
    Aim gaze;
};
static_assert(sizeof(Aim) == 32 && sizeof(AimData) == 128);
constexpr float gazeRange = 150;
constexpr uint32_t aimLeaseMs = 250;
inline bool valid(const Command& c) {
    const auto& t = c.tilt;
    const float tilt = t.x * t.x + t.y * t.y + t.z * t.z + t.w * t.w;
    if (c.magic != 0x5357434d ||
        !commandBytes(c.version) || c.bytes != commandBytes(c.version) ||
        !std::isfinite(c.leap) || c.leap < 0 || c.leap > 1 || !finite(c.look) || length(c.look) > 1.01f ||
        !c.serial || c.focused > 1 || c.jump > 1 || c.leaseMs > 250 || (c.focused && !c.leaseMs) ||
        !finite(c.move) || length(c.move) > 1.001f || !std::isfinite(c.trackingYaw) || c.asks > 63 ||
        !std::isfinite(c.sampleSeconds) || c.sampleSeconds <= 0 || c.sampleSeconds > .1f ||
        (c.focused && !c.sampleTimeNs) || !std::isfinite(tilt) || tilt < .99f || tilt > 1.01f)
        return false;
    if (c.version >= 7) {
        if (c.actionMode > 7) return false;
        for (const auto q : c.physicalToContact) {
            const float n = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
            if (!std::isfinite(n) || std::abs(n-1.f) > .01f) return false;
        }
    }
    for (const auto& h : c.hands) {
        const auto q = h.aim.orientation;
        const float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (h.tracked > 1 || !finite(h.aim.position) || !finite(h.relative) || !std::isfinite(norm) ||
            norm < .99f || norm > 1.01f || !std::isfinite(h.trigger) || !std::isfinite(h.grip) ||
            h.trigger < 0 || h.trigger > 1 || h.grip < 0 || h.grip > 1)
            return false;
    }
    for (unsigned i = 0; i < 2; ++i)
        if (!finite(c.wallPositions[i]) || !std::isfinite(c.wallGrips[i]) || c.wallGrips[i] < 0 ||
            c.wallGrips[i] > 1)
            return false;
    // A gaze is a direction or none at all.
    const float gaze = length(c.gaze);
    return c.wallRotate <= 1 && finite(c.gazeOrigin) && finite(c.gaze) &&
           (gaze == 0 || std::abs(gaze - 1) < .01f) && c.eyeAim <= 1;
}
// The actual module ingress. `read` copies a prefix from the submitted pointer
// with its platform's checked read; tests supply a bounded wire buffer. Default
// initialize before copying old packets, and never read beyond their wire size.
template<class Read> inline bool decodeCommand(Command& out, Read read) {
    uint32_t header[3]{};
    if (!read(header, sizeof(header)) || header[0] != 0x5357434d ||
        !commandBytes(header[1]) || header[2] != commandBytes(header[1])) return false;
    Command candidate;
    if (!read(&candidate, header[2]) || candidate.bytes != header[2] || !valid(candidate)) return false;
    if (candidate.version < 7) {
        // v5 padding and v6 reserved bytes were not action policy. Normalize
        // them even when an older producer populated them with nonzero data.
        candidate.actionMode = candidate.animationCancel = 0;
        candidate.physicalToContact[0] = candidate.physicalToContact[1] = {};
    }
    out = candidate;
    return true;
}

class InputSampleClock {
  public:
    float consume(const Command& c) {
        if (c.serial == serial_)
            return 0;
        const float seconds =
            time_ ? (c.sampleTimeNs > time_ ? static_cast<float>((c.sampleTimeNs - time_) * 1e-9)
                                            : std::numeric_limits<float>::quiet_NaN())
                  : c.sampleSeconds;
        serial_ = c.serial;
        time_ = c.sampleTimeNs;
        return seconds;
    }
    void reset() {
        serial_ = time_ = 0;
    }

  private:
    uint64_t serial_{}, time_{};
};
// The game's running time, ms: the time from one of the swing's visits (a
// game frame) to the next, each gap counted up to maxGapMs. A load hitch
// stops the game's whole frame, visits too, and counts as one long frame:
// timed by the wall clock, a hitch over half a second let go of every web
// (the input held for controlHoldMs ran out, or the mover had not stepped
// for that long), the player, November 2: "with load stutters while swinging
// I very often lose the web, as if it were cut or I let go".
class RunClock {
  public:
    static constexpr uint64_t maxGapMs = 100;
    uint64_t update(uint64_t nowMs) {
        if (started_ && nowMs >= lastMs_)
            runMs_ += std::min(nowMs - lastMs_, maxGapMs);
        lastMs_ = nowMs;
        started_ = true;
        return runMs_;
    }

  private:
    uint64_t lastMs_{}, runMs_{};
    bool started_{};
};
// The input the swing acts on. A focused sample stays in use for
// controlHoldMs after input stops coming or turns unfocused, as if no new
// sample had arrived: a game frame of over 100 ms closes the XR worker's
// gameplay gate. The time is the game's running time (RunClock), so a load
// hitch of any length is a stutter here. Letting go there handed a swinging player to the game's own
// fall, which counts the time airborne through Spidy's flight and starts at
// 36-48 m/s down (October 6-7 headset reports).
class InputHold {
  public:
    // `live`: c is focused and within its lease. Whether c, replaced by the
    // held sample while holding, is input to act on.
    bool update(Command& c, bool live, uint64_t nowMs) {
        if (live) {
            held_ = c;
            heldMs_ = nowMs;
            holding_ = true;
            return true;
        }
        if (holding_ && nowMs >= heldMs_ && nowMs - heldMs_ <= controlHoldMs) {
            c = held_;
            return true;
        }
        holding_ = false;
        return false;
    }
    void reset() {
        holding_ = false;
    }

  private:
    Command held_{};
    uint64_t heldMs_{};
    bool holding_{};
};
// The game's own move out of Spidy's flight, asked for (Command::asks): its
// zip to a point (LT + RT), its web strike (Y; a car's in a chase among
// them), a prompt's. "Is there anyway to web zip-to-point or web strike while
// moving? ... this makes it hard to do things like the car chases" (October
// 17): until October 18 both were held back while a web carried the player,
// because handing him to the game in mid-flight dropped him at the game's
// fall speed - and a hand-over before the game's move begins (a press it
// refuses, no point in sight) would do the same.
//
// So the swing waits for the game to begin. Through Spidy's flight the hero
// stays in the game's air states (its Jump and Fall), whose handler runs
// about twice a mover step (native_movement's airEvents: never once missing
// from a driven step in the sessions of October 15), and none of the game's
// moves is one of them. Asked within askMs, and the handler quiet for
// quietSteps steps (not in the game's wall grasp, which has a latch of its
// own): the game has him, and the swing lets go - its webs too, the move has
// its own - until he lands, the handler runs again (back in the game's air,
// the move done), or yieldMs pass. A press the game does not take changes
// nothing: the handler goes on, and so does the swing.
class AirYield {
  public:
    static constexpr uint64_t askMs = 400, yieldMs = 3000, quietSteps = 2;
    // nowMs: a clock in milliseconds. asked: the player asks now. owned: the
    // swing has the hero. airborne: he is in the air. grasp: the game's wall
    // grasp (mover flag 0x80). steps, airEvents: the player's mover steps and
    // the game's air handler's calls so far. True while the swing must not
    // drive.
    bool update(uint64_t nowMs, bool asked, bool owned, bool airborne, bool grasp, uint64_t steps,
                uint64_t airEvents) {
        began_ = false;
        if (!seen_ || airEvents != air_ || steps < airStep_) {
            air_ = airEvents;
            airStep_ = steps;
            seen_ = true;
        }
        const bool quiet = steps >= airStep_ + quietSteps;
        if (asked)
            askedMs_ = nowMs;
        const bool recent = askedMs_ && nowMs >= askedMs_ && nowMs - askedMs_ < askMs;
        if (yielded_) {
            yielded_ = airborne && quiet && nowMs >= since_ && nowMs - since_ < yieldMs;
        } else if (owned && airborne && !grasp && quiet && recent) {
            yielded_ = began_ = true;
            since_ = nowMs;
            ++yields_;
        }
        return yielded_;
    }
    bool yielded() const {
        return yielded_;
    }
    // It let go in the last update (the swing drops its webs then).
    bool began() const {
        return began_;
    }
    uint32_t yields() const {
        return yields_;
    }
    void reset() {
        *this = {};
    }

  private:
    uint64_t air_{}, airStep_{}, askedMs_{}, since_{};
    uint32_t yields_{};
    bool seen_{}, yielded_{}, began_{};
};
inline Input input(const Command& c) {
    Input out;
    out.focused = c.focused != 0;
    out.jump = c.jump != 0;
    // Bit 2 of asks (COMBAT RESPONSIVE): the player fights now.
    out.fighting = (c.asks & 4u) != 0;
    // Bit 3 (SPIDY RUN JUMP): A out of the swing's run is its own jump.
    out.runJump = (c.asks & 8u) != 0;
    out.move = c.move;
    out.trackingYaw = c.trackingYaw;
    const float n =
        std::sqrt(c.tilt.x * c.tilt.x + c.tilt.y * c.tilt.y + c.tilt.z * c.tilt.z + c.tilt.w * c.tilt.w);
    if (c.version >= 3 && n > .5f)
        out.tilt = {c.tilt.x / n, c.tilt.y / n, c.tilt.z / n, c.tilt.w / n};
    if (c.version >= 4) {
        out.leap = c.leap;
        out.look = c.look;
    }
    for (unsigned i = 0; i < 2; ++i)
        out.hands[i] = {c.hands[i].aim, c.hands[i].relative, c.hands[i].tracked != 0, c.hands[i].trigger,
                        c.hands[i].grip};
    if (c.version >= 5) {
        for (unsigned i = 0; i < 2; ++i) {
            out.hands[i].wallPosition = c.wallPositions[i];
            out.hands[i].wallGrip = c.wallGrips[i];
        }
        out.wallRotate = c.wallRotate != 0;
    }
    if (c.version >= 6 && length(c.gaze) > .5f) {
        out.gazeOrigin = c.gazeOrigin;
        out.gaze = c.gaze;
        out.gazeShots = (c.eyeAim & 1) != 0;
    }
    return out;
}
} // namespace spidy::game_swing
