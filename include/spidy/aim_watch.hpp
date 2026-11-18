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
// HAND SOURCE EASE (the 2 AM log of Nov 18, approved 1:53 PM): the mapped hands' source changing (the VR rig, the
// head lock, the shown view of a handoff blend) moved hand, marker and web line in one frame - 2,839 marker jumps,
// the worst 175 degrees, arm line-up steps up to 2.4 m and 140 degrees. The step is now carried as an offset that
// fades over `seconds` (smoothstep), so the hand glides from where it was shown onto its new source; past
// `snapMetres` it is a teleport and snaps. Pure.
class SourceEase {
  public:
    static constexpr float seconds = .15f, snapMetres = 3.f;
    // shownPos/shownOri: what was shown last frame; nowPos/nowOri: the new source's this frame.
    void begin(Vec3 shownPos, Quat shownOri, Vec3 nowPos, Quat nowOri) {
        const Vec3 d = shownPos - nowPos;
        if (!finite(d) || length(d) > snapMetres) {
            active_ = false;
            return;
        }
        dPos_ = d;
        dOri_ = shownOri * nowOri.conjugate();
        if (dOri_.w < 0)
            dOri_ = {-dOri_.x, -dOri_.y, -dOri_.z, -dOri_.w};
        t_ = 0;
        active_ = true;
    }
    // This frame's hand: the source's, with what is left of the step.
    void apply(float dt, Vec3& pos, Quat& ori) {
        if (!active_)
            return;
        t_ += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        const float u = std::clamp(t_ / seconds, 0.f, 1.f);
        const float w = 1.f - u * u * (3.f - 2.f * u);
        if (w <= 0.f) {
            active_ = false;
            return;
        }
        pos = pos + dPos_ * w;
        // nlerp from none to the step, by w.
        Quat q{dOri_.x * w, dOri_.y * w, dOri_.z * w, 1.f - w + dOri_.w * w};
        const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (n > 1e-6f)
            ori = Quat{q.x / n, q.y / n, q.z / n, q.w / n} * ori;
    }
    bool active() const {
        return active_;
    }

  private:
    bool active_{};
    float t_{};
    Vec3 dPos_{};
    Quat dOri_{};
};
} // namespace spidy::aim_watch
