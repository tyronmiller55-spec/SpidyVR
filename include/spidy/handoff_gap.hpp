// HANDOFF HEAD GAP (beta3, H6; the owner, Nov 21 t86u: "handoffs are glitching; periodically disconnecting
// the head from the body where frames of the body without the neck can be seen"). Pure; no Windows.
//
// For a second after every owner switch (animation <-> SpidyVR) this measures, every frame, how far the view
// the headset shows is from the head of the body drawn under it, and writes one line per switch:
//   handoff head gap: <switch> worst X m at T ms, settled S m, over S+5 cm for N frames (M ms), K frames
//   unmeasured
// "Settled" is the gap at the window's end (the last frames' median): the eyes sit a little off the head bone
// even at rest, so only what a switch adds over that counts as the neck showing. The Nov 21 session
// (game-vr-20261121-080924) had the landings' view glide 1.15-2.6 m onto the head over 450 ms and the
// takeovers' view 0.3-0.6 m off; this line scores each switch directly, so the next session shows what is
// left.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::handoff_gap {

inline constexpr int64_t windowNs = 1'000'000'000; // a switch is watched this long
inline constexpr float overMetres = .05f;          // past settled + this, the neck can show
inline constexpr int maxSamples = 240;             // > 1 s at 120 Hz
inline constexpr int settleSamples = 5;            // the median of the window's last samples

struct Report {
    const char* why = "";
    bool toAnim{};
    float worst{-1.f};   // m (-1: nothing measured)
    float worstMs{};     // ms after the switch
    float settled{-1.f}; // m
    int overFrames{};
    float overMs{};
    int frames{}, unmeasured{};
};

class Watch {
  public:
    // A switch. A window still open is closed first: true, with its report in `closed`.
    bool start(const char* why, bool toAnim, int64_t nowNs, Report& closed) {
        const bool had = active_ && close(closed);
        active_ = true;
        why_ = why ? why : "";
        toAnim_ = toAnim;
        startNs_ = nowNs;
        n_ = 0;
        unmeasured_ = 0;
        return had;
    }
    // A frame: the gap in metres (negative, or not finite: not measured this frame). True once the window is
    // over, with its report.
    bool sample(int64_t nowNs, float gap, Report& out) {
        if (!active_)
            return false;
        if (std::isfinite(gap) && gap >= 0.f) {
            if (n_ < maxSamples)
                samples_[n_++] = {static_cast<float>(nowNs - startNs_) / 1e6f, gap};
        } else {
            ++unmeasured_;
        }
        if (nowNs - startNs_ >= windowNs)
            return close(out);
        return false;
    }
    bool active() const {
        return active_;
    }

  private:
    struct Sample {
        float ms{}, gap{};
    };
    bool close(Report& r) {
        active_ = false;
        r = {};
        r.why = why_;
        r.toAnim = toAnim_;
        r.frames = n_ + unmeasured_;
        r.unmeasured = unmeasured_;
        if (n_ == 0)
            return true;
        const int k = std::min(settleSamples, n_);
        std::array<float, settleSamples> tail{};
        for (int i = 0; i < k; ++i)
            tail[static_cast<size_t>(i)] = samples_[static_cast<size_t>(n_ - k + i)].gap;
        std::sort(tail.begin(), tail.begin() + k);
        r.settled = tail[static_cast<size_t>(k / 2)];
        for (int i = 0; i < n_; ++i) {
            const auto& s = samples_[static_cast<size_t>(i)];
            if (s.gap > r.worst) {
                r.worst = s.gap;
                r.worstMs = s.ms;
            }
            if (s.gap > r.settled + overMetres) {
                ++r.overFrames;
                const float next = i + 1 < n_ ? samples_[static_cast<size_t>(i + 1)].ms : s.ms;
                r.overMs += std::max(0.f, next - s.ms);
            }
        }
        return true;
    }
    std::array<Sample, maxSamples> samples_{};
    int n_{}, unmeasured_{};
    int64_t startNs_{};
    const char* why_ = "";
    bool toAnim_{}, active_{};
};

} // namespace spidy::handoff_gap
