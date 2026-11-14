// ENEMY JOINTS (R1; game_joints.hpp, enemy_joints.hpp): an enemy's joints in the world, read, never written.
//
// Entity layout, as everywhere in this project (game_fp.cpp, native_body.cpp): the object holds its world matrix
// at +0 and an info pointer at +0x88 (the skeleton's asset id at info+8, its rig at info+0x200: the joint count
// as a u16 at +2 and the joint table at the pointer at +8, 16 bytes a joint: parent s16, name hash at +8). A
// pool entry (a kick target's id) holds the object pointer first.
//
// MSVC: functions with __try hold no object that needs a destructor.
#include "spidy/game_joints.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <windows.h>

namespace spidy::game_joints {
namespace {
// The game's own joint reader, as game_fp.cpp's headWorld calls it.
constexpr std::uintptr_t getJointRva = 0x1775a80;
using GetJoint = float*(__fastcall*)(std::uint64_t object, float* out, std::uint32_t hash);
constexpr unsigned maxJoints = 1024;

bool okPointer(std::uint64_t p) {
    return p > 0x10000 && p < 0x00007FFFFFFFFFFFull;
}
struct Chain {
    std::uint64_t instance{}, table{}, asset{};
    unsigned count{};
};
// record -> instance -> info -> rig. 0 a chain, 1 not an entity, 2 no rig.
int walk(std::uint64_t record, Chain& chain) {
    __try {
        if (!okPointer(record))
            return 1;
        const std::uint64_t instance = *reinterpret_cast<volatile std::uint64_t*>(record);
        if (!okPointer(instance))
            return 1;
        const std::uint64_t info = *reinterpret_cast<volatile std::uint64_t*>(instance + 0x88);
        if (!okPointer(info))
            return 1;
        const std::uint64_t rig = *reinterpret_cast<volatile std::uint64_t*>(info + 0x200);
        if (!okPointer(rig))
            return 2;
        const unsigned count = *reinterpret_cast<volatile std::uint16_t*>(rig + 2);
        const std::uint64_t table = *reinterpret_cast<volatile std::uint64_t*>(rig + 8);
        if (!count || count > maxJoints || !okPointer(table))
            return 2;
        chain.instance = instance;
        chain.table = table;
        chain.count = count;
        chain.asset = *reinterpret_cast<volatile std::uint64_t*>(info + 8);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
bool copyTable(std::uint64_t table, unsigned count, std::uint8_t* out) {
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(table), count * 16ull);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// One joint in the world (game_fp's headWorld, the translation only). False with no usable joint.
bool jointWorld(GetJoint getJoint, std::uint64_t instance, std::uint32_t hash, float* position) {
    __try {
        float joint[32]{};
        getJoint(instance, joint, hash);
        if (!enemy_joints::usableJoint(joint))
            return false;
        const float* world = reinterpret_cast<const float*>(instance);
        float matrix[16];
        std::memcpy(matrix, world, sizeof(matrix));
        for (const float v : matrix)
            if (!std::isfinite(v))
                return false;
        const Vec3 p = enemy_joints::worldPosition(joint, matrix);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return false;
        position[0] = p.x;
        position[1] = p.y;
        position[2] = p.z;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The roles each rig names, by its table's address, count and asset: a rig is looked through once, and again
// after a few seconds (a freed table's address may come back as another rig).
struct Cached {
    std::uint64_t table{}, asset{};
    unsigned count{};
    std::uint64_t at{};
    enemy_joints::Found found{};
};
constexpr int cacheSize = 32;
Cached cache[cacheSize];
int cacheNext{};
constexpr std::uint64_t cacheMs = 5000;

const enemy_joints::Found* roles(const Chain& chain) {
    const std::uint64_t now = GetTickCount64();
    for (auto& c : cache)
        if (c.table == chain.table && c.asset == chain.asset && c.count == chain.count && now - c.at < cacheMs)
            return &c.found;
    std::uint8_t bytes[maxJoints * 16];
    if (!copyTable(chain.table, chain.count, bytes))
        return nullptr;
    Cached& slot = cache[cacheNext];
    cacheNext = (cacheNext + 1) % cacheSize;
    slot.table = chain.table;
    slot.asset = chain.asset;
    slot.count = chain.count;
    slot.at = now;
    slot.found = enemy_joints::findRoles(bytes, chain.count);
    return &slot.found;
}
} // namespace

bool read(std::uint64_t record, bool limbs, Read& out, Failure* why) {
    const auto fail = [&](Failure f) {
        if (why)
            *why = f;
        return false;
    };
    out = {};
    static const GetJoint getJoint = [] {
        const auto exe = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return exe ? reinterpret_cast<GetJoint>(exe + getJointRva) : nullptr;
    }();
    if (!getJoint)
        return fail(Failure::noGame);
    Chain chain;
    if (const int walked = walk(record, chain); walked != 0)
        return fail(walked == 1 ? Failure::badRecord : Failure::noRig);
    const enemy_joints::Found* found = roles(chain);
    if (!found)
        return fail(Failure::noRig);
    out.named = found->mask;
    out.count = chain.count;
    out.asset = chain.asset;
    if (!found->mask)
        return fail(Failure::noRoles);
    // The head and chest always; the limbs only when asked for (and only the names the rig has).
    std::uint32_t wanted = (1u << enemy_joints::head) | (1u << enemy_joints::chest);
    if (limbs)
        wanted = enemy_joints::allRoles;
    for (int r = 0; r < enemy_joints::roleCount; ++r) {
        if (!(wanted & (1u << r)) || !(found->mask & (1u << r)))
            continue;
        float p[3];
        if (jointWorld(getJoint, chain.instance, enemy_joints::roleHash(r), p)) {
            out.joints.at[static_cast<size_t>(r)] = {p[0], p[1], p[2]};
            out.joints.valid |= 1u << r;
        }
    }
    if (!out.joints.has(enemy_joints::head) || !out.joints.has(enemy_joints::chest))
        return fail(Failure::noHead);
    if (why)
        *why = Failure::none;
    return true;
}
} // namespace spidy::game_joints
