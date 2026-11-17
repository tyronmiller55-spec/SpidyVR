// AIM WATCH (the owner, Nov 17: "making sure arms line up properly as they handoff ... one of the most common times
// that a player is trying to shoot a web", and "reaching around to fire webs causes the marker to jump"). Pure
// counters for the log: a hand's mapped pose against the controller's own motion between two frames.
//  - Lineup: where the hands are mapped from (the rig, the head lock, the shown view of a handoff blend) and how far
//    a change of that source moved the hand and its line beyond what the controller itself moved.
//  - MarkerJumps: frames where the web line (the marker's) turned more than `degrees` beyond the controller's own
//    turn that frame.
#pragma once
#include "spidy/math.hpp"
#include "spidy/spherical_aim.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::aim_watch {
inline constexpr float toDeg = 180.f / 3.14159265f;

struct Lineup {
    int source = -1;
    bool have{};
    Vec3 pos{}, dir{}, ctrlPos{}, ctrlDir{};
    struct Out {
        bool changed{};
        int from{-1}, to{-1};
        float jumpM{}, jumpDeg{}; // beyond the controller's own move / turn that frame
    };
    // `src` the mapping's source (any id), `pos`/`dir` the mapped hand and its line (world), `ctrlPos`/`ctrlDir` the
    // controller (tracking space).
    Out update(int src, bool valid, Vec3 p, Vec3 d, Vec3 cp, Vec3 cd) {
        Out o;
        if (!valid || !finite(p) || !spherical_aim::unitish(d) || !finite(cp) || !spherical_aim::unitish(cd)) {
            have = false;
            source = src;
            return o;
        }
        if (have && src != source) {
            o.changed = true;
            o.from = source;
            o.to = src;
            o.jumpM = std::max(0.f, length(p - pos) - length(cp - ctrlPos));
            o.jumpDeg = std::max(0.f, (spherical_aim::angleBetween(d, dir) - spherical_aim::angleBetween(cd, ctrlDir)) * toDeg);
        }
        source = src;
        have = true;
        pos = p;
        dir = d;
        ctrlPos = cp;
        ctrlDir = cd;
        return o;
    }
};

struct MarkerJumps {
    bool have{};
    Vec3 line{}, ctrl{};
    uint32_t jumps{}, frames{};
    float worst{};
    // True on a jump this frame.
    bool update(bool valid, Vec3 l, Vec3 c, float degrees) {
        if (!valid || !spherical_aim::unitish(l) || !spherical_aim::unitish(c)) {
            have = false;
            return false;
        }
        bool jump = false;
        if (have) {
            ++frames;
            const float excess =
                (spherical_aim::angleBetween(l, line) - spherical_aim::angleBetween(c, ctrl)) * toDeg;
            if (excess > degrees) {
                ++jumps;
                jump = true;
            }
            worst = std::max(worst, excess);
        }
        have = true;
        line = l;
        ctrl = c;
        return jump;
    }
    void clearWindow() {
        jumps = frames = 0;
        worst = 0;
    }
};
} // namespace spidy::aim_watch
