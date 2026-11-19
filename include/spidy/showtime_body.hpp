// SHOWTIME: what is drawn of Spidey while the camera is out (BODY (ADVANCED)).
// Ported from EngineBot's Showtime series (S0, showtime_body.hpp), onto this line's own Showtime
// (showtime.hpp, showtime_rig.hpp): the same decisions, keyed to this line's BODY row and its
// stored values, so a settings file keeps meaning what it meant.
//   0 GAME POSE, HEAD SHOWN (default, as before): the head shown, the body whatever drives him.
//   1 AS IN PLAY (NO HEAD): exactly as first person draws him (EngineBot's RAW (DEBUG)).
//   2 NATURAL ARMS: the head shown; SpidyVR's body with his own arm length (not stretched to the
//     player's measured reach: body_calibration's arm scale back to 1, the size players saw as
//     "too big" from outside), and under the game's animation the arm overlay paused, so the
//     game's own arms are seen.
//   3 GAME SWING POSE (EXPERIMENTAL): as NATURAL ARMS, but while SpidyVR swings him and the game
//     has its own swing animation running (ANIMATION MOVES on, the swing owned), the VR body steps
//     aside for the game's pose (physics stays SpidyVR's) and is reclaimed when it is back.
// Pure; no Windows.
#pragma once
#include <cstdint>

namespace spidy::showtime {

enum class Owner : uint8_t { anim, vr, frozen };
inline const char* ownerName(Owner o) {
    return o == Owner::vr ? "vr" : o == Owner::frozen ? "frozen" : "anim";
}
enum class Body : uint8_t { firstPerson, asDriven, asInPlay, natural, game };
inline const char* bodyName(Body b) {
    switch (b) {
    case Body::asDriven:
        return "game pose, head shown";
    case Body::asInPlay:
        return "as in play";
    case Body::natural:
        return "natural arms";
    case Body::game:
        return "the game's animation";
    default:
        return "first person";
    }
}
// BODY (Values::showtimeBody).
inline constexpr int bodyGamePose = 0, bodyAsInPlay = 1, bodyNatural = 2, bodyGameSwing = 3;
inline constexpr int bodyLast = bodyGameSwing;
struct BodyIn {
    bool out{};      // the camera is out (fade to fade)
    int body = bodyGamePose;
    Owner owner = Owner::anim;
    bool gamePoseAvailable{}; // the game animates what SpidyVR does now (its swing)
};
struct BodyOut {
    Body body = Body::firstPerson;
    bool showHead{};        // native_body hideHead off, game_fp's head shrink off
    bool pauseArmOverlay{}; // ANIMATED ARMS off the game's animation while out
    bool naturalArms{};     // body_calibration's arm scale back to 1 (pose.armLength 0)
    bool yieldPose{};       // native_body bodyOn off: the game's pose is drawn (physics stays VR)
};
inline BodyOut bodyFor(const BodyIn& in) {
    BodyOut o;
    if (!in.out)
        return o;
    if (in.body == bodyAsInPlay) {
        o.body = Body::asInPlay;
        return o;
    }
    o.showHead = true;
    if ((in.body != bodyNatural && in.body != bodyGameSwing) || in.owner == Owner::frozen) {
        o.body = in.owner == Owner::frozen ? Body::game : Body::asDriven;
        return o;
    }
    if (in.body == bodyGameSwing && in.owner == Owner::vr && in.gamePoseAvailable) {
        o.body = Body::game;
        o.yieldPose = o.pauseArmOverlay = true;
        return o;
    }
    o.body = in.owner == Owner::vr ? Body::natural : Body::game;
    o.naturalArms = in.owner == Owner::vr;
    o.pauseArmOverlay = in.owner == Owner::anim;
    return o;
}

} // namespace spidy::showtime
