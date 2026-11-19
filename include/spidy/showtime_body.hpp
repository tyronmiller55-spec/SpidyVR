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
inline constexpr int bodyGamePose = 0, bodyAsInPlay = 1, bodyNatural = 2;
inline constexpr int bodyLast = bodyNatural;
struct BodyIn {
    bool out{};      // the camera is out (fade to fade)
    int body = bodyGamePose;
    Owner owner = Owner::anim;
};
struct BodyOut {
    Body body = Body::firstPerson;
    bool showHead{};        // native_body hideHead off, game_fp's head shrink off
    bool pauseArmOverlay{}; // ANIMATED ARMS off the game's animation while out
    bool naturalArms{};     // body_calibration's arm scale back to 1 (pose.armLength 0)
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
    if (in.body != bodyNatural || in.owner == Owner::frozen) {
        o.body = in.owner == Owner::frozen ? Body::game : Body::asDriven;
        return o;
    }
    o.body = in.owner == Owner::vr ? Body::natural : Body::game;
    o.naturalArms = in.owner == Owner::vr;
    o.pauseArmOverlay = in.owner == Owner::anim;
    return o;
}

} // namespace spidy::showtime
