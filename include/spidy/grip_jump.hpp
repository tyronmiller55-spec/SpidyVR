// GRIP LOCKS THE WALL JUMP (0.8.7-na-beta3o, wall_grip_jump_lock; the owner, Nov 26 t147u: "Only point jumps
// should work through the grip. Normal jumps should stay locked."): while a grip holds one of SpidyVR's own walls
// (Surface::wall), a press of A is no jump - neither the swing's wall jump nor the game's A - until it is let go
// of. A+B, the point jump, has its own path (point_jump.hpp's chord, SpidySwingPointJump) and still fires; the
// handstand's grip has held A since beta3n (handstand_grip_hold). Let go of the grip to wall jump, as before.
// Engine independent; no game address.
#pragma once
#include <cmath>
#include <cstdint>

namespace spidy::grip_jump {
// GRIP TO STICK's squeeze (swing.cpp stickInputs: wallGrip over 0.65).
inline constexpr float gripSqueeze = .65f;

struct Hand {
    bool tracked = false;
    float squeeze = 0;
    bool webbed = false; // a web held in that hand: its grip is the web's, not the wall's
};
// A grip holds the wall: on SpidyVR's wall, a tracked hand with no web squeezed past gripSqueeze.
inline bool wallGripHeld(bool onSpidyWall, const Hand& left, const Hand& right) {
    const auto holds = [](const Hand& h) {
        return h.tracked && !h.webbed && std::isfinite(h.squeeze) && h.squeeze > gripSqueeze;
    };
    return onSpidyWall && (holds(left) || holds(right));
}

// A press that begins while locked is swallowed until A is let go of (letting the grip go mid-press does not
// jump); a press that began unlocked goes on as it was.
class Lock {
  public:
    struct Out {
        bool a = false;        // A for the game and the swing
        bool lockedNow = false; // this frame's press was swallowed (its first frame)
    };
    Out update(bool a, bool locked) {
        Out out;
        if (!a) {
            held_ = swallowing_ = false;
            return out;
        }
        if (!held_) {
            held_ = true;
            swallowing_ = locked;
            out.lockedNow = locked;
        }
        out.a = !swallowing_;
        return out;
    }
    bool swallowing() const {
        return swallowing_;
    }

  private:
    bool held_ = false, swallowing_ = false;
};
} // namespace spidy::grip_jump
