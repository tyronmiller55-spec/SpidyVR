#pragma once
// ENEMY JOINTS (R1, enemy_joints.hpp): the memory side. Reads the joints an enemy's skeleton names for the zone
// roles, in the world, by the chain the C0 findings verified piece by piece (record -> instance -> info -> rig
// -> getJoint at image + 0x1775a80, the same function game_fp's headWorld calls). Read only: nothing is written
// to the game, no hook is installed. Every read is guarded (a bad pointer is a failed read, not a crash).
//
// One thread reads (the swing module's step); the rig cache is not shared.
#include "enemy_joints.hpp"
#include <cstdint>

namespace spidy::game_joints {
struct Read {
    enemy_joints::Joints joints;
    std::uint32_t named{}; // the roles the rig's joint table has by name (a bit per enemy_joints::Role)
    unsigned count{};      // its joint count
    std::uint64_t asset{}; // the skeleton's asset id (info + 8)
};
// Why a read gave no joints.
enum class Failure : std::uint8_t { none, noGame, badRecord, noRig, noRoles, noHead };
// Reads `record`'s joints (a kick target's id: a pool entry). With `limbs` the forearms and shins are read too
// (ENEMY LIMB ZONES); otherwise only the head and chest. True when the head and chest were read (what a zone
// from joints needs); `out.joints.valid` says what else.
bool read(std::uint64_t record, bool limbs, Read& out, Failure* why = nullptr);
inline const char* failureName(Failure f) {
    switch (f) {
    case Failure::noGame:
        return "no game image";
    case Failure::badRecord:
        return "not an entity";
    case Failure::noRig:
        return "no rig";
    case Failure::noRoles:
        return "no known joint names";
    case Failure::noHead:
        return "no head or chest joint";
    default:
        return "none";
    }
}
} // namespace spidy::game_joints
