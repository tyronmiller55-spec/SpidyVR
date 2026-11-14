#pragma once
// ENEMY JOINTS (R1; the plan's Pillar 5 and the C0 findings, section 2.1; enemy_joints, off until set,
// EXPERIMENTAL): the reader of an enemy's skeleton, so a kick can say which part of him it met.
//
// The chain is the one game_fp and native_body already walk for other characters (no new offset):
//   record (a pool entry, a kick target's id) -> instance = [record] -> info = [instance + 0x88]
//   -> rig = [info + 0x200] (joint count u16 at +2, the joint table at the pointer at +8, 16 bytes a joint:
//   parent s16, name hash at +8, name offset at +0xc) -> getJoint(instance, out, hash) (image + 0x1775a80)
//   -> the joint's matrix times the instance's world matrix = the joint in the world.
// The hash is the game's name hash: CRC-32 steps seeded with 0xedb88320, no final inversion ("pelvis" is
// 0x8e59b33f, "head" 0x37519912).
//
// What is proven and what is not (docs/ENEMY_JOINTS.md): only the head joint has been read on other
// characters' skeletons (game_fp). The hero's other names (chest, LF_/RT_ loarm, wrist, loleg, foot) on an
// enemy's rig are untested until probe_c0_skeleton.py is run (reports/c0-skeleton.json says READABLE and
// resolves the roles head, chest, LF_wrist, LF_foot). So:
//   * Off by default (enemy_joints). The capsule bands (head over 80 % of the capsule, legs under 35 %) are
//     the fallback for any name a rig lacks, and for everything when it is off.
//   * Head and chest zones come from the joints when it is on. The LIMB zones (the legs from the shins, the
//     arms from the forearms) are gated off by a second setting (enemy_limb_zones) until the probe has been
//     run: with it off the legs are the band under 35 % and there is no arms zone.
//
// This is the reader and the zone choice only. It writes nothing to the game and moves nothing (R2, the
// overlay of an enemy's pose, is not built). Engine independent here: the parsing, the roles, the zone
// geometry. The memory reads and the getJoint call are game_joints.cpp's.
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace spidy::enemy_joints {
// The game's name hash (1bb88d0).
inline std::uint32_t nameHash(std::string_view name) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xedb88320u;
    for (const char ch : name)
        c = table[(c ^ static_cast<std::uint8_t>(ch)) & 0xff] ^ (c >> 8);
    return c;
}

// The joints the zones need, by the hero's names (docs/BODY.md; the same roles native_body's identify() uses).
enum Role : int {
    chest,
    head,
    leftElbow,   // LF_loarm
    leftWrist,   // LF_wrist
    rightElbow,  // RT_loarm
    rightWrist,  // RT_wrist
    leftKnee,    // LF_loleg
    leftFoot,    // LF_foot
    rightKnee,   // RT_loleg
    rightFoot,   // RT_foot
    roleCount
};
inline constexpr const char* roleNames[roleCount] = {"chest",   "head",     "LF_loarm", "LF_wrist", "RT_loarm",
                                                     "RT_wrist", "LF_loleg", "LF_foot",  "RT_loleg", "RT_foot"};
inline std::uint32_t roleHash(int role) {
    static const auto hashes = [] {
        std::array<std::uint32_t, roleCount> h{};
        for (int i = 0; i < roleCount; ++i)
            h[i] = nameHash(roleNames[i]);
        return h;
    }();
    return role >= 0 && role < roleCount ? hashes[static_cast<size_t>(role)] : 0u;
}
inline constexpr std::uint32_t allRoles = (1u << roleCount) - 1;
// Bits of the roles each zone needs: the head and chest are points; an arm needs its elbow and wrist, a leg
// its knee and foot.
inline constexpr std::uint32_t armBits[2] = {(1u << leftElbow) | (1u << leftWrist), (1u << rightElbow) | (1u << rightWrist)};
inline constexpr std::uint32_t legBits[2] = {(1u << leftKnee) | (1u << leftFoot), (1u << rightKnee) | (1u << rightFoot)};

// An enemy's joints in the world: `valid` has a bit per Role whose joint was read.
struct Joints {
    std::uint32_t valid{};
    std::array<Vec3, roleCount> at{};
    bool has(int role) const {
        return role >= 0 && role < roleCount && (valid & (1u << role)) != 0;
    }
    bool hasAll(std::uint32_t bits) const {
        return (valid & bits) == bits;
    }
};

// The roles a rig's joint table names: the table's index of each (-1 for a name it lacks). `table` holds
// `count` joints of 16 bytes (parent s16 at +0, the name hash u32 at +8).
struct Found {
    std::array<std::int16_t, roleCount> index{};
    std::uint32_t mask{};
};
inline Found findRoles(const std::uint8_t* table, unsigned count) {
    Found f;
    f.index.fill(-1);
    if (!table || !count || count > 1024)
        return f;
    std::array<std::uint32_t, roleCount> hashes{};
    for (int r = 0; r < roleCount; ++r)
        hashes[static_cast<size_t>(r)] = roleHash(r);
    for (unsigned j = 0; j < count; ++j) {
        std::uint32_t hash{};
        std::memcpy(&hash, table + j * 16ull + 8, 4);
        for (int r = 0; r < roleCount; ++r)
            if (hash == hashes[static_cast<size_t>(r)] && f.index[static_cast<size_t>(r)] < 0) {
                f.index[static_cast<size_t>(r)] = static_cast<std::int16_t>(j);
                f.mask |= 1u << r;
            }
    }
    return f;
}

// A getJoint matrix (row-major 4x4, the position in row 3) is a joint: finite, w = 1, axes not collapsed. (A
// shrunk head has axes a thousandth long: still a head, game_fp's rule.)
inline bool usableJoint(const float* m) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]))
            return false;
    return std::abs(m[15] - 1.f) <= .01f && m[0] * m[0] + m[1] * m[1] + m[2] * m[2] >= 1e-8f;
}
// The joint's position in the world: its own position times the instance's world matrix (row vector).
inline Vec3 worldPosition(const float* joint, const float* world) {
    const float x = joint[12], y = joint[13], z = joint[14];
    return {x * world[0] + y * world[4] + z * world[8] + world[12],
            x * world[1] + y * world[5] + z * world[9] + world[13],
            x * world[2] + y * world[6] + z * world[10] + world[14]};
}

// ---- the zones ----
// The parts a hit can be on, with air_kick::Zone's values (head 0, chest 1, legs 2) and the arms (3).
inline constexpr int partHead = 0, partChest = 1, partLegs = 2, partArms = 3;

inline float pointSegmentDistance(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 ab = b - a;
    const float l2 = dot(ab, ab);
    const float t = l2 > 1e-8f ? std::clamp(dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return length(p - (a + ab * t));
}
inline Vec3 pointSegmentNearest(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 ab = b - a;
    const float l2 = dot(ab, ab);
    const float t = l2 > 1e-8f ? std::clamp(dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return a + ab * t;
}
// Whether the joints give the part a place: the head and chest their joint; the arms and legs (the limb
// zones, only when `limbs`) a forearm or a shin.
inline bool hasPart(const Joints& j, int part, bool limbs) {
    switch (part) {
    case partHead:
        return j.has(head);
    case partChest:
        return j.has(chest);
    case partLegs:
        return limbs && (j.hasAll(legBits[0]) || j.hasAll(legBits[1]));
    case partArms:
        return limbs && (j.hasAll(armBits[0]) || j.hasAll(armBits[1]));
    default:
        return false;
    }
}
// The point on the part nearest `ref` (a limb: along the nearer forearm or shin); false with none.
inline bool partPoint(const Joints& j, int part, bool limbs, Vec3 ref, Vec3& out) {
    if (!hasPart(j, part, limbs))
        return false;
    if (part == partHead || part == partChest) {
        out = j.at[part == partHead ? head : chest];
        return finite(out);
    }
    const bool arms = part == partArms;
    float best = 1e30f;
    bool found = false;
    for (int s = 0; s < 2; ++s) {
        if (!j.hasAll(arms ? armBits[s] : legBits[s]))
            continue;
        const Vec3 a = j.at[arms ? (s == 0 ? leftElbow : rightElbow) : (s == 0 ? leftKnee : rightKnee)];
        const Vec3 b = j.at[arms ? (s == 0 ? leftWrist : rightWrist) : (s == 0 ? leftFoot : rightFoot)];
        if (!finite(a) || !finite(b))
            continue;
        const float d = pointSegmentDistance(ref, a, b);
        if (d < best) {
            best = d;
            out = pointSegmentNearest(ref, a, b);
            found = true;
        }
    }
    return found;
}
// Which part a hit at `point` met: the nearest of the head, the chest, and (limbs) the forearms and shins.
// Without the limb zones the legs are not a joint zone: the caller's band decides them. -1 when the joints
// give no head or chest (the capsule bands decide).
inline int partOfHit(const Joints& j, Vec3 point, bool limbs) {
    if (!finite(point) || !j.has(head) || !j.has(chest))
        return -1;
    float best = length(point - j.at[head]);
    int part = partHead;
    if (const float d = length(point - j.at[chest]); d < best) {
        best = d;
        part = partChest;
    }
    if (limbs)
        for (const int p : {partLegs, partArms}) {
            Vec3 on;
            if (partPoint(j, p, true, point, on))
                if (const float d = length(point - on); d < best) {
                    best = d;
                    part = p;
                }
        }
    return part;
}
} // namespace spidy::enemy_joints
