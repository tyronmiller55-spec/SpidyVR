// ANIMATED ARMS (vr_settings::Values::animatedArms, off by default): while a
// pose yield is active (ANIMATED MOVES / AIR), overlay controller-driven arms
// on top of the game's torso/legs/root/head animation via native_body's
// armsOverlay path (body::Targets::armsOnly). Full bodyOn is NOT used — that
// would replace the whole figure.
//
// Per-arm weights: while yielding and the hand is tracked, a soft floor keeps
// idle arms on the controllers (not game-anim-only). Motion, fist/shoot, or
// web-button intent raises weight to full; tracking loss clears the latch and
// returns that arm toward the game anim.
//
// Chest-frame grip remap (animated_arms::remapGripToChest): express the
// controller grip relative to the player/HMD eyes in a yaw-level player frame,
// then re-apply that offset in the anim chest/shoulder frame so IRL limbs stay
// matched through model yaw and parkour rotations. Used by body_ik armsOnly.
//
// Thresholds below are PROVISIONAL scaffolding — not headset-validated.
#pragma once
#include "spidy/math.hpp"
#include <algorithm>
#include <cmath>

namespace spidy::animated_arms {

// Provisional (unvalidated) motion / blend tuning.
constexpr float speedOn = .35f;     // m/s of grip vs root → active
constexpr float speedOff = .12f;    // m/s → may release after hold
constexpr float holdSeconds = .35f; // keep full after motion drops
constexpr float blendIn = 12.f;     // weight shares per second (interrupt)
constexpr float blendOut = 6.f;     // return toward floor / anim
constexpr float webBoost = 1.f;     // fist/shoot/web button → full weight
// Soft always-on floor while yielding + tracked (Slice C tracking lock). Idle
// no longer drops to 0 (game-anim-only bones). Intent/motion still reach 1.
constexpr float yieldFloor = .55f;

struct Arm {
    float weight{};
    float hold{};
    Vec3 lastRel{};
    bool lastRelSet{};
};

struct State {
    Arm arms[2]{};
    Vec3 lastRoot{};
    bool rootSet{};
};

struct Sample {
    Vec3 grip{};      // world grip point
    bool tracked{};
    float fist{};     // 0..1
    float shoot{};    // 0..1
    bool webButton{}; // grip/trigger web intent this frame
};

inline float approach(float now, float want, float rate, float dt) {
    if (!std::isfinite(now) || !std::isfinite(want) || !std::isfinite(rate) || !std::isfinite(dt))
        return want;
    const float step = std::max(0.f, rate) * std::max(0.f, dt);
    if (now < want)
        return std::min(want, now + step);
    return std::max(want, now - step);
}

// Orthonormal look/up/left (character left = cross(up, look)).
struct Basis {
    Vec3 look{}, up{}, left{};
    bool ok{};
};

inline Basis makeBasis(Vec3 look, Vec3 upHint) {
    Basis b;
    if (!finite(look) || !finite(upHint))
        return b;
    Vec3 up = upHint;
    if (length(up) < .5f)
        up = {0, 1, 0};
    up = normalized(up);
    look = look - up * dot(look, up);
    if (length(look) < .2f)
        look = Vec3{0, 0, 1} - up * up.z;
    if (length(look) < .2f)
        return b;
    look = normalized(look);
    Vec3 left = cross(up, look);
    if (length(left) < .2f)
        return b;
    left = normalized(left);
    up = normalized(cross(look, left));
    b.look = look;
    b.up = up;
    b.left = left;
    b.ok = true;
    return b;
}

inline Vec3 toLocal(const Basis& b, Vec3 world) {
    return {dot(world, b.left), dot(world, b.up), dot(world, b.look)};
}

inline Vec3 fromLocal(const Basis& b, Vec3 local) {
    return b.left * local.x + b.up * local.y + b.look * local.z;
}

// Horizontal yaw of a look vector (radians); 0 = +Z, as atan2(x, z).
inline float lookYaw(Vec3 look) {
    if (!finite(look))
        return 0.f;
    const float h = std::hypot(look.x, look.z);
    if (h < 1e-4f)
        return 0.f;
    return std::atan2(look.x, look.z);
}

struct Remap {
    Vec3 grip{};
    Quat orientation{};
    bool applied{};
    float playerYaw{};
    float chestYaw{};
};

// Map a controller grip from the player/HMD eye frame into the anim chest
// frame. playerEyes / playerLook come from the XR targets (HMD); chest* and
// animAnchor (usually anim head or chest) come from the game pose this solve.
// Orientation is rotated by the yaw delta (chest − player) so palms stay
// sensible through model yaw; pitch/roll of the controller are preserved in
// the player frame's local axes then rebuilt on the chest yaw.
inline Remap remapGripToChest(Vec3 grip, Quat orientation, Vec3 playerEyes, Vec3 playerLook, Vec3 playerUp,
                              Vec3 animAnchor, Vec3 chestLook, Vec3 chestUp) {
    Remap out;
    out.grip = grip;
    out.orientation = orientation;
    out.playerYaw = lookYaw(playerLook);
    out.chestYaw = lookYaw(chestLook);
    const Basis player = makeBasis(playerLook, playerUp);
    const Basis chest = makeBasis(chestLook, chestUp);
    if (!player.ok || !chest.ok || !finite(grip) || !finite(playerEyes) || !finite(animAnchor))
        return out;
    const Vec3 local = toLocal(player, grip - playerEyes);
    out.grip = animAnchor + fromLocal(chest, local);
    // Rotate orientation by yaw-only change of basis (preserves local pitch/roll).
    const float dyaw = std::remainder(out.chestYaw - out.playerYaw, 6.2831853f);
    if (std::isfinite(dyaw) && std::abs(dyaw) > 1e-5f)
        out.orientation = Quat::yaw(dyaw) * orientation;
    out.applied = finite(out.grip);
    return out;
}

// Update per-arm overlay weights. root = actor feet / tracking anchor (world).
// Subtracts root travel from grip motion so falling/swinging the body is not
// counted as hand activity. Tracked idle hands keep yieldFloor (soft lock).
inline void update(State& s, float dt, bool enabled, Vec3 root, const Sample hands[2]) {
    dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
    if (!enabled) {
        for (auto& a : s.arms)
            a = {};
        s.rootSet = false;
        return;
    }
    if (s.rootSet && finite(root) && finite(s.lastRoot)) {
        // rootDelta reserved for future relative diagnostics; relative grip
        // already excludes travel via lastRel.
    }
    if (finite(root)) {
        s.lastRoot = root;
        s.rootSet = true;
    }
    for (int i = 0; i < 2; ++i) {
        auto& a = s.arms[i];
        const auto& h = hands[i];
        if (!h.tracked || !finite(h.grip) || !finite(root)) {
            a.lastRelSet = false;
            a.hold = 0;
            a.weight = approach(a.weight, 0.f, blendOut, dt);
            continue;
        }
        const Vec3 rel = h.grip - root;
        float speed = 0.f;
        if (a.lastRelSet && dt > 1e-4f) {
            const Vec3 dRel = rel - a.lastRel;
            speed = length(dRel) / dt;
        }
        a.lastRel = rel;
        a.lastRelSet = true;
        const bool intent = h.webButton || h.fist > .2f || h.shoot > .05f;
        const bool moving = speed >= speedOn;
        const bool still = speed <= speedOff;
        if (intent || moving) {
            a.hold = holdSeconds;
        } else if (still) {
            a.hold = std::max(0.f, a.hold - dt);
        }
        // Soft floor while tracked: idle stays on controllers. Intent/hold → 1.
        const float want = intent ? webBoost : (a.hold > 0.f ? 1.f : yieldFloor);
        a.weight = approach(a.weight, want, want > a.weight ? blendIn : blendOut, dt);
    }
}

inline bool anyActive(const State& s) {
    return s.arms[0].weight > .001f || s.arms[1].weight > .001f;
}

} // namespace spidy::animated_arms
