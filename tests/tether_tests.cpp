// PHYSICAL WEB TETHERS (tether.hpp, W1; November 14): the XPBD rope (pins, sag, unilateral stiffness, collision,
// sleep, drawn points), the whip's wave, the spin's detector and orbit, the payout, and WebGrab using them: a
// spun target orbits and is slung, a throw with the trigger held keeps its line. Portable: no Windows, no
// headset, no game.
#include "spidy/tether.hpp"
#include "spidy/web_grab.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace spidy;
using namespace spidy::tether;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
bool near(float a, float b, float eps = 1e-3f) {
    return std::isfinite(a) && std::abs(a - b) <= eps;
}
Config on() {
    Config c;
    c.enabled = true;
    return c;
}
float polyline(const Rope& r) {
    float total = 0;
    const auto p = r.points();
    for (std::size_t i = 0; i + 1 < p.size(); ++i)
        total += length(p[i + 1] - p[i]);
    return total;
}
// A floor at y = 0 (a ray that crosses it).
struct Floor : Collider {
    bool ray(Vec3 from, Vec3 to, Vec3& point, Vec3& normal) const override {
        if ((from.y > 0) == (to.y > 0))
            return false;
        const float t = from.y / (from.y - to.y);
        point = from + (to - from) * t;
        normal = {0, 1, 0};
        return true;
    }
};
// Runs `seconds` of 30 Hz steps with the ends where `hand` and `end` are.
Rope::Result settle(Rope& r, Vec3 hand, Vec3 end, float len, float seconds, const Collider* world = nullptr,
                    std::span<const Capsule> capsules = {}) {
    Rope::Result last;
    for (float t = 0; t < seconds; t += 1.f / 30) {
        Rope::Frame f;
        f.dt = 1.f / 30;
        f.hand = hand;
        f.end = end;
        f.length = len;
        f.world = world;
        f.capsules = capsules;
        last = r.step(f);
    }
    return last;
}
Rope make(const Config& c = on()) {
    Rope r;
    r.configure(c);
    return r;
}

// ---- WebGrab rig ----
struct Open : WorldQueries {
    std::optional<RayHit> raycast(Vec3, Vec3, float) const override {
        return {};
    }
    bool exists(std::uint64_t) const override {
        return false;
    }
};
struct Props : TargetQueries {
    std::vector<GrabTarget> items;
    void add(std::uint64_t id, Vec3 position, float mass) {
        items.push_back({id, TargetKind::Object, position, {}, mass, .3f});
    }
    std::optional<GrabTarget> pick(Vec3, Vec3, float, float) const override {
        if (items.empty())
            return {};
        return items.front();
    }
    mutable unsigned asked{};
    void characters(std::vector<GrabTarget>& out) const override {
        ++asked;
        for (const auto& t : people)
            out.push_back(t);
    }
    std::vector<GrabTarget> people;
    std::optional<GrabTarget> find(std::uint64_t id) const override {
        for (const auto& t : items)
            if (t.id == id)
                return t;
        return {};
    }
    void apply(float dt, const std::vector<TargetCommand>& commands) {
        for (auto& t : items) {
            for (const auto& c : commands)
                if (c.id == t.id)
                    t.velocity = advance(c, t.position, t.velocity, dt, {0, -9.81f, 0});
            if (t.velocity.y != 0 || length(t.velocity) > 0) {
                // free fall for what no command held
            }
            t.position += t.velocity * dt;
        }
    }
};
struct Lab {
    WebGrab grab;
    Open world;
    Props props;
    Body player{{0, 1, 0}, {}, true};
    Input in;
    std::vector<TargetCommand> commands;
    unsigned throwEvents{}, releaseEvents{}, launches{};
    Vec3 launched{};
    Vec3 thrown{};
    std::vector<TargetCommand> last;
    explicit Lab(bool tethers = true) {
        auto c = grab.config().tether;
        c.enabled = tethers;
        grab.tether(c);
        for (int i = 0; i < 2; ++i) {
            in.hands[i].tracked = true;
            in.hands[i].aim.position = {i ? .3f : -.3f, 1.2f, 0};
            in.hands[i].wallPosition = in.hands[i].aim.position;
        }
    }
    void hand(int i, Vec3 at) {
        in.hands[i].aim.position = in.hands[i].wallPosition = at;
    }
    // One 90 Hz sample then three physics steps; `path(t)` is hand 0's place at the end of the sample.
    template <class Path> void run(float seconds, Path path) {
        const float dt = 1.f / 90;
        for (float t = 0; t < seconds - dt / 2; t += dt) {
            hand(0, path(clock + dt));
            grab.claim(dt, in, world, props, player, 0);
            for (const auto& e : grab.events()) {
                throwEvents += e.kind == GrabEventKind::Throw;
                releaseEvents += e.kind == GrabEventKind::Release;
                if (e.kind == GrabEventKind::Throw)
                    thrown = e.velocity;
            }
            for (int s = 0; s < 3; ++s) {
                commands.clear();
                grab.step(dt / 3, world, props, commands);
                props.apply(dt / 3, commands);
                last = commands;
                for (const auto& c : commands)
                    if (c.mode == TargetCommand::Mode::Launch && !c.thrown) {
                        ++launches;
                        launched = c.velocity;
                    }
            }
            clock += dt;
        }
    }
    void hold(float seconds) {
        const Vec3 at = in.hands[0].aim.position;
        run(seconds, [&](float) { return at; });
    }
    float clock = 0;
    const Grab& g() const {
        return grab.grabs()[0];
    }
};
// The left hand shoots a web at a prop `distance` m ahead and (trigger) reels it in to the hand: Held.
void holdProp(Lab& r, float distance, float mass = 20) {
    r.props.add(1, {-.3f, 1.2f, -distance}, mass);
    r.in.hands[0].grip = 1;
    r.hold(.1f);
    check(r.g().phase == GrabPhase::Tethered, "a web is on the prop");
    r.in.hands[0].trigger = 0;
    r.hold(.1f);
    r.in.hands[0].trigger = 1; // released first, then squeezed: reels
    r.hold(3.f);
}
} // namespace

int main() {
    // ---- config ----
    {
        Config c;
        check(!c.enabled && c.particles == 24 && c.substeps == 8 && near(c.compliance, 1e-7f, 1e-9f),
              "off by default: 24 points, 8 steps, compliance 1e-7");
        check(near(c.spinMax, 18) && near(c.whipMax, 14) && near(c.whipCooldown, .4f) && near(c.spinRate, 3),
              "whip at most 14 m/s, 0.4 s apart; spin from 3 rad/s, at most 18 m/s");
        check(valid(c), "the default is valid");
        Config bad = c;
        bad.particles = 4;
        check(!valid(bad), "too few points");
        bad = c;
        bad.particles = 33;
        check(!valid(bad), "too many points");
        bad = c;
        bad.substeps = 1;
        check(!valid(bad), "too few steps");
        bad = c;
        bad.compliance = std::nanf("");
        check(!valid(bad), "NaN compliance");
        bad = c;
        bad.lockTrigger = bad.payoutTrigger;
        check(!valid(bad), "a lock at the cut");
        Rope r;
        bad = c;
        bad.substeps = 99;
        r.configure(bad);
        check(r.config().substeps == 8, "an invalid config is refused");
        check(near(smoothstep(0, 1, .5f), .5f) && smoothstep(0, 1, -1) == 0 && smoothstep(0, 1, 2) == 1 &&
                  smoothstep(1, 1, 2) == 1 && smoothstep(1, 1, 0) == 0,
              "smoothstep");
    }
    // ---- the drawn points ----
    {
        Vec3 pts[24];
        for (int i = 0; i < 24; ++i)
            pts[i] = {static_cast<float>(i), 0, 0};
        Vec3 out[6];
        check(simplify(pts, 6, .02f, out) == 0, "a straight line has no bends");
        for (int i = 12; i < 24; ++i)
            pts[i] = {11.f, static_cast<float>(i - 11), 0}; // an L, the corner at 11
        check(simplify(pts, 6, .02f, out) >= 1 && near(out[0].x, 11) && near(out[0].y, 0),
              "an L's corner is the first bend");
        for (int i = 0; i < 24; ++i)
            pts[i] = {static_cast<float>(i), std::sin(static_cast<float>(i) * .7f), 0};
        const int n = simplify(pts, 6, .001f, out);
        check(n == 6, "a wavy line fills the budget, no more");
        check(simplify(pts, 3, .001f, out) == 3 && simplify(pts, 0, .001f, out) == 0, "and obeys a smaller one");
        const int again = simplify(pts, 6, .001f, out);
        bool ordered = again == n;
        for (int i = 1; i < n; ++i)
            ordered = ordered && out[i].x > out[i - 1].x;
        check(ordered, "in order along the rope");
        check(simplify(std::span<const Vec3>(pts, 2), 6, .01f, out) == 0 && simplify(pts, 6, .01f, nullptr) == 0,
              "too short, and no output");
    }
    // ---- the rope ----
    {
        // Slack: a hanging line sags below the ends, shorter than its length, ends pinned.
        Rope r = make();
        const Vec3 hand{0, 2, 0}, end{3, 2, 0};
        const auto res = settle(r, hand, end, 4.f, 3.f);
        check(res.active && !res.taut && near(res.tension, 0), "slack: not taut");
        const auto p = r.points();
        check(p.size() == 24 && length(p.front() - hand) < 1e-4f && length(p.back() - end) < 1e-4f,
              "both ends are pinned to the hand and the target");
        float lowest = 9;
        for (const auto& q : p)
            lowest = std::min(lowest, q.y);
        check(lowest < 1.4f && lowest > -1.f, "the line sags");
        check(polyline(r) <= 4.f * 1.05f, "and does not stretch past its length");
        Vec3 out[6];
        check(r.draw(out) >= 1 && r.draw(out, 2) <= 2, "the sag is drawn as bends, as many as asked");
        // No particle above the chord: a rope pulls, it does not push.
        bool above = false;
        for (const auto& q : p)
            above = above || q.y > 2.0f + .01f;
        check(!above, "nothing above the ends");
        // Taut: the ends further than the length: a straight line, tension 1.
        Rope t = make();
        const auto tr = settle(t, hand, {5, 2, 0}, 4.f, 1.f);
        float off = 0;
        for (const auto& q : t.points())
            off = std::max(off, std::abs(q.y - 2.f));
        check(tr.taut && near(tr.tension, 1) && off < .02f, "taut: straight, tension 1");
        Vec3 none[6];
        check(t.draw(none) == 0, "a straight line has nothing to draw");
        // Just taut: tension between.
        Rope m = make();
        const auto mr = settle(m, hand, {4.2f, 2, 0}, 4.f, 1.f);
        check(mr.tension > 0 && mr.tension < 1, "a little past its length: some tension");
        // Compliance: the more compliant, the longer the stretched line.
        Config soft = on();
        soft.compliance = 1e-3f;
        Rope s = make(soft);
        settle(s, hand, {4.f, 2, 0}, 4.f, 1.f);
        check(polyline(s) >= 3.99f, "a line pulled to its length is that long");
        // The sleeping.
        Rope z = make();
        settle(z, hand, end, 4.f, 6.f);
        check(z.asleep(), "asleep once still");
        const auto asleep = settle(z, hand, end, 4.f, .2f);
        check(asleep.asleep && asleep.active, "and says so");
        const auto woke = settle(z, hand, end + Vec3{0, .5f, 0}, 4.f, .1f);
        check(!woke.asleep && !z.asleep(), "a moved end wakes it");
        // A reel shortens the line: the particles close up.
        Rope w = make();
        settle(w, hand, end, 4.f, 2.f);
        settle(w, hand, end, 3.2f, 2.f);
        check(polyline(w) <= 3.2f * 1.06f, "a shorter web gives a shorter line");
        // A teleport re-lays it, without a thousand-metre line.
        settle(w, hand, {200, 2, 0}, 4.f, .1f);
        check(polyline(w) < 300.f, "far past its length it is laid again");
    }
    // ---- bad input ----
    {
        Rope r = make();
        settle(r, {0, 2, 0}, {3, 2, 0}, 4.f, 1.f);
        const auto before = r.points()[10];
        Rope::Frame f;
        f.dt = 1.f / 30;
        f.hand = {std::nanf(""), 0, 0};
        f.end = {1, 1, 1};
        f.length = 4;
        check(!r.step(f).active && length(r.points()[10] - before) == 0, "a NaN hand does nothing");
        f.hand = {0, 2, 0};
        f.length = std::nanf("");
        check(!r.step(f).active, "a NaN length does nothing");
        f.length = 4;
        f.dt = 0;
        check(!r.step(f).active, "no time does nothing");
        // A random walk of the ends for ten seconds stays finite and bounded.
        Rope walk = make();
        unsigned seed = 7;
        const auto rnd = [&] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1 << 24) - .5f;
        };
        Vec3 hand{0, 2, 0}, end{3, 2, 0};
        bool fine = true;
        for (int i = 0; i < 300; ++i) {
            hand += Vec3{rnd(), rnd(), rnd()} * .6f;
            end += Vec3{rnd(), rnd(), rnd()} * .4f;
            Rope::Frame g;
            g.dt = 1.f / 30;
            g.hand = hand;
            g.end = end;
            g.length = 4;
            walk.step(g);
            for (const auto& q : walk.points())
                fine = fine && finite(q) && length(q) < 1e4f;
        }
        check(fine, "ten seconds of wild ends stay finite");
    }
    // ---- collision ----
    {
        const Floor floor;
        Rope r = make();
        settle(r, {0, .3f, 0}, {3, .3f, 0}, 4.f, 3.f, &floor);
        float lowest = 9;
        for (const auto& q : r.points())
            lowest = std::min(lowest, q.y);
        check(lowest > -.05f, "a line laid on the floor stays on it");
        check(!r.planes().empty() && r.planes().size() <= 12, "from cached planes, no more than the limit");
        Rope open = make();
        settle(open, {0, .3f, 0}, {3, .3f, 0}, 4.f, 3.f);
        float low2 = 9;
        for (const auto& q : open.points())
            low2 = std::min(low2, q.y);
        check(low2 < -.2f, "and with no world it goes through");
        // The planes age out.
        Rope::Frame f;
        f.dt = .1f;
        f.hand = {0, 5, 0};
        f.end = {3, 5, 0};
        f.length = 4;
        for (int i = 0; i < 12; ++i)
            r.step(f);
        check(r.planes().empty(), "planes die when the rope is away from them");
        // A capsule: an enemy the line is pushed round.
        const Capsule enemy{{1.5f, 0, 0}, 1.8f, .3f};
        const std::array<Capsule, 1> list{enemy};
        Rope c = make();
        settle(c, {0, 1, 0}, {3, 1, 0}, 3.3f, 2.f, nullptr, list);
        float nearest = 9;
        for (const auto& q : c.points())
            nearest = std::min(nearest, std::hypot(q.x - 1.5f, q.z));
        check(nearest > .3f - .06f, "the line goes round an enemy, not through");
    }
    // ---- the whip ----
    {
        // A line with a metre of slack: the hand jerks away along it.
        Rope r = make();
        const Vec3 end{0, 2, 0};
        Vec3 hand{0, 2, 3.f};
        settle(r, hand, end, 4.f, 2.f);
        int whips = 0;
        float biggest = 0;
        float firstAt = -1;
        float t = 0;
        // Still hand: no whip however long.
        for (; t < 1.f; t += 1.f / 30) {
            Rope::Frame f;
            f.dt = 1.f / 30;
            f.hand = hand;
            f.end = end;
            f.length = 4;
            const auto res = r.step(f);
            whips += res.whip;
        }
        check(whips == 0, "no whip from a still hand");
        // The snap: 1.4 m in 0.15 s (9 m/s).
        for (int i = 0; i < 40; ++i) {
            Rope::Frame f;
            f.dt = 1.f / 30;
            if (i < 5)
                hand.z += 9.f / 30;
            f.hand = hand;
            f.handVelocity = {0, 0, i < 5 ? 9.f : 0.f};
            f.end = end;
            f.length = 4;
            const auto res = r.step(f);
            if (res.whip) {
                ++whips;
                biggest = std::max(biggest, res.whipSpeed);
                if (firstAt < 0)
                    firstAt = static_cast<float>(i) / 30;
            }
        }
        check(whips >= 1, "a snap of the hand whips");
        check(biggest > 0 && biggest <= 14.f + 1e-3f, "the jerk is at most 14 m/s");
        check(firstAt >= 0 && firstAt < .6f, "within a fraction of a second");
        check(whips <= 2, "and the cooldown keeps it to a crack or two");
        // A slow pull of the same distance: nothing.
        Rope slow = make();
        hand = {0, 2, 3.f};
        settle(slow, hand, end, 4.f, 2.f);
        int slowWhips = 0;
        for (int i = 0; i < 90; ++i) {
            Rope::Frame f;
            f.dt = 1.f / 30;
            hand.z += 1.4f / 90;
            f.hand = hand;
            f.handVelocity = {0, 0, 1.4f / 3};
            f.end = end;
            f.length = 4;
            slowWhips += slow.step(f).whip;
        }
        check(slowWhips == 0, "a slow pull of the same distance does not");
        // The jerk's limits: a heavy target gets less, and none from nothing.
        const Vec3 jerk = whipJerk({0, 0, 1}, 10, 20, 2400, Config{});
        check(near(length(jerk), 10, 1e-3f) && near(jerk.z, 10, 1e-3f), "a light target takes the wave's speed");
        check(length(whipJerk({0, 0, 1}, 10, 400, 2400, Config{})) < 1.f, "a heavy one, little");
        check(near(length(whipJerk({0, 0, 1}, 50, 5, 2400, Config{})), 14, 1e-3f), "never more than 14 m/s");
        check(length(whipJerk({0, 0, 0}, 10, 20, 2400, Config{})) == 0 && length(whipJerk({0, 0, 1}, 0, 20, 2400)) == 0,
              "no direction or no speed: nothing");
    }
    // ---- the spin's detector and orbit ----
    {
        const Config c = on();
        const float dt = 1.f / 30;
        // Circling at 8 rad/s, radius .3 m (2.4 m/s).
        Spinner s;
        float seconds = 0;
        Spinner::State st;
        bool everBefore = false;
        float startedAt = -1;
        for (int i = 0; i < 60; ++i) {
            const float a = 8.f * seconds;
            const Vec3 v{-std::sin(a) * 2.4f, 0, std::cos(a) * 2.4f};
            st = s.update(dt, v, c);
            if (st.active && startedAt < 0)
                startedAt = seconds;
            everBefore = everBefore || (st.active && seconds < .25f);
            seconds += dt;
        }
        check(st.active && startedAt >= .29f && startedAt < .7f && !everBefore, "circling takes hold after a third of a second");
        check(std::abs(st.axis.y) > .95f && near(st.rate, 8, 1.2f), "about the circle's axis, at its rate");
        check(near(st.gain, 1), "the orbit has built up");
        // The reverse direction flips the axis, still detected.
        Spinner back;
        Spinner::State bst;
        for (int i = 0; i < 60; ++i) {
            const float a = -8.f * dt * static_cast<float>(i);
            bst = back.update(dt, {-std::sin(a) * 2.4f, 0, std::cos(a) * 2.4f}, c);
        }
        check(bst.active && bst.axis.y * st.axis.y < 0, "the other way round, the other axis");
        // Stops: inactive within a moment, and the gain with it.
        for (int i = 0; i < 12; ++i)
            st = s.update(dt, {0, 0, 0}, c);
        check(!st.active && st.gain == 0, "a hand that stops lets go of the orbit");
        // Straight, slow and small never count.
        Spinner straight;
        bool any = false;
        for (int i = 0; i < 90; ++i)
            any = any || straight.update(dt, {3, 0, 0}, c).active;
        check(!any, "a straight swing is not a spin");
        Spinner slowSpin;
        any = false;
        for (int i = 0; i < 150; ++i) {
            const float a = 1.5f * dt * static_cast<float>(i);
            any = any || slowSpin.update(dt, {-std::sin(a) * 2.4f, 0, std::cos(a) * 2.4f}, c).active;
        }
        check(!any, "a slow turn (1.5 rad/s) is not a spin");
        Spinner tiny;
        any = false;
        for (int i = 0; i < 90; ++i) {
            const float a = 8.f * dt * static_cast<float>(i);
            any = any || tiny.update(dt, {-std::sin(a) * .5f, 0, std::cos(a) * .5f}, c).active;
        }
        check(!any, "a tiny circle (0.5 m/s) is not a spin");
        check(!Spinner{}.update(std::nanf(""), {1, 0, 0}, c).active && !Spinner{}.update(dt, {std::nanf(""), 0, 0}, c).active,
              "bad input is nothing");
        // The orbit: tangential to the offset about the axis, at the hand's rate, at most 18 m/s.
        const Vec3 hand{0, 1, 0}, at{3, 1, 0}, axis{0, 1, 0};
        const Vec3 v = orbitVelocity(hand, {}, at, 3.f, axis, 2.f, 1.f, c);
        check(near(length(v), 6, .05f) && std::abs(dot(normalized(v), {1, 0, 0})) < .05f, "tangential, rate times radius");
        check(near(length(orbitVelocity(hand, {}, at, 3.f, axis, 20.f, 1.f, c)), 18, .05f), "at most 18 m/s");
        check(length(orbitVelocity(hand, {}, at, 3.f, axis, 2.f, 0.f, c)) < .01f, "no gain, no orbit");
        const Vec3 carried = orbitVelocity(hand, {1, 0, 0}, at, 3.f, axis, 2.f, .5f, c);
        check(near(carried.x, 1, .05f) && near(length(carried - Vec3{1, 0, 0}), 3, .05f), "the hand's own velocity carries it");
        const Vec3 pulled = orbitVelocity(hand, {}, {4, 1, 0}, 3.f, axis, 0.f, 1.f, c);
        check(pulled.x < 0 && near(pulled.x, -6, .05f), "a stretched radius is drawn back in, at most 6 m/s");
        check(near(length(orbitVelocity(hand, {1, 2, 3}, hand, 3.f, axis, 2.f, 1.f, c)), length(Vec3{1, 2, 3}), 1e-4f),
              "on the hand: the hand's velocity");
    }
    // ---- the payout ----
    {
        const Config c;
        check(payout(0.f, 2.f, 5.f, c).cut && payout(.09f, 2.f, 5.f, c).cut, "the trigger let go of cuts the line");
        check(payout(std::nanf(""), 2.f, 5.f, c).cut, "and so does nothing");
        const auto free = payout(.1f, 2.f, 5.f, c);
        check(!free.cut && near(free.length, 5.f, 1e-3f) && near(free.brake, 0) && near(free.grip, 0),
              "a light trigger pays out freely");
        const auto locked = payout(.95f, 2.f, 5.f, c);
        check(!locked.cut && near(locked.length, 2.f) && near(locked.brake, 1) && near(locked.grip, 1),
              "a full trigger locks the length and holds with the web's whole force");
        check(near(payout(1.f, 2.f, 5.f, c).length, 2.f), "and so does a full squeeze");
        const auto half = payout(.5f, 2.f, 5.f, c);
        check(near(half.length, 5.f) && half.grip > 0 && half.grip < .35f, "between, it runs out against a friction");
        check(payout(.4f, 2.f, 5.f, c).grip < payout(.7f, 2.f, 5.f, c).grip &&
                  payout(.7f, 2.f, 5.f, c).grip < payout(.94f, 2.f, 5.f, c).grip,
              "the harder the trigger, the harder the hold");
        check(near(payout(.5f, 4.f, 3.f, c).length, 4.f), "a line that is not taut pays out nothing");
    }
    // ---- WebGrab: off is as it was ----
    {
        Lab off(false);
        holdProp(off, 3.f);
        check(off.g().phase == GrabPhase::Held, "reeled in, the prop is held (tethers off)");
        check(!off.grab.tetherView(0).active && off.grab.tetherReport().serial == 0, "nothing is simulated");
        check(off.props.asked == 0, "and no one is asked about");
        off.in.hands[0].grip = 0;
        off.in.hands[0].trigger = .6f;
        off.hold(.2f);
        check(off.g().phase == GrabPhase::None && off.throwEvents == 1, "let go with the trigger held: thrown, as before");
    }
    // ---- WebGrab: the rope is drawn and the line is kept after a throw ----
    {
        Lab r;
        holdProp(r, 3.f);
        check(r.g().phase == GrabPhase::Held, "reeled in, the prop is held");
        check(r.grab.tetherView(0).active && near(length(r.grab.tetherView(0).end - r.props.items[0].position), 0, .5f),
              "the line is live and ends at the prop");
        check(r.props.asked > 0, "the characters near the line are asked about, for the line to go round");
        // Let go of the grip with the trigger held half way.
        r.in.hands[0].grip = 0;
        r.in.hands[0].trigger = .5f;
        const float before = r.g().length;
        r.hold(.02f);
        check(r.throwEvents == 1, "the throw is reported once");
        check(r.g().phase == GrabPhase::Tethered && r.grab.tetherView(0).kept, "the line is kept");
        // Fling the prop away: it flies, the line pays out behind it.
        r.props.items[0].velocity = {0, 3, -8};
        r.hold(.5f);
        check(r.g().phase == GrabPhase::Tethered && r.g().length > before + 1.f, "the line pays out after the prop");
        check(r.grab.tetherReport().throws == 1 && r.grab.tetherReport().lastAction == 4, "counted as a throw on a tether");
        check(r.grab.tetherView(0).kept, "still kept");
        // The trigger squeezed full: the line locks, the prop swings on it.
        r.in.hands[0].trigger = 1.f;
        const float locked = r.g().length;
        r.hold(1.f);
        check(r.g().phase == GrabPhase::Tethered && near(r.g().length, locked, .05f), "a full trigger locks the length");
        check(length(r.props.items[0].position - r.in.hands[0].aim.position) < locked + 1.f,
              "and the prop stays within it");
        // A pull while it is kept yanks the prop back (the yank gesture): no, with no head samples this is the
        // trigger's: letting go cuts.
        r.in.hands[0].trigger = 0;
        r.hold(.05f);
        check(r.g().phase == GrabPhase::None && r.grab.tetherReport().cuts == 1 && r.grab.tetherReport().lastAction == 5,
              "the trigger let go of cuts the line");
        check(!r.grab.tetherView(0).active, "and nothing is drawn");
    }
    {
        // Squeezed hard at the throw: the line is locked at once, a short leash.
        Lab r;
        holdProp(r, 3.f);
        r.in.hands[0].grip = 0;
        r.in.hands[0].trigger = 1.f;
        r.hold(.02f);
        const float leash = r.g().length;
        r.props.items[0].velocity = {0, 0, -10};
        r.hold(.6f);
        check(r.g().phase == GrabPhase::Tethered && r.g().length < leash + .3f, "a full trigger at the throw keeps a short leash");
        // A new squeeze lets go of the kept line and shoots anew.
        r.in.hands[0].grip = 1;
        r.hold(.05f);
        check(r.releaseEvents >= 1, "a new grip press lets go of the kept line");
        // Turning the tethers off lets a kept line go.
        Lab q;
        holdProp(q, 3.f);
        q.in.hands[0].grip = 0;
        q.in.hands[0].trigger = .5f;
        q.hold(.05f);
        check(q.g().phase == GrabPhase::Tethered, "kept");
        auto cfg = q.grab.config().tether;
        cfg.enabled = false;
        check(q.grab.tether(cfg) && q.g().phase == GrabPhase::None && !q.grab.tetherView(0).active,
              "turned off, the kept line is let go of");
        cfg.particles = 3;
        check(!q.grab.tether(cfg), "an invalid config is refused");
    }
    {
        // A kept line has no reel: the trigger is the brake, not a winch.
        Lab r;
        holdProp(r, 3.f);
        r.in.hands[0].grip = 0;
        r.in.hands[0].trigger = .7f;
        r.hold(.02f);
        r.props.items[0].position = {-.3f, 1.2f, -6.f};
        r.props.items[0].velocity = {};
        r.hold(.3f);
        check(r.g().length >= 1.f, "the kept line does not reel the prop in");
    }
    // ---- WebGrab: spin and sling ----
    {
        Lab r;
        r.props.add(1, {-.3f, 1.2f, -3.f}, 10);
        r.in.hands[0].grip = 1;
        const Vec3 centre{-.3f, 1.2f, 0};
        // Let the web take hold, then circle the hand at 8 rad/s, radius .25 m, in the horizontal plane.
        r.hold(.2f);
        check(r.g().phase == GrabPhase::Tethered, "a web is on the prop");
        r.props.items[0].velocity = {};
        float seconds = 0;
        bool orbit = false;
        r.run(1.5f, [&](float t) {
            seconds = t;
            const float a = 8.f * t;
            return centre + Vec3{std::cos(a), 0, std::sin(a)} * .25f;
        });
        for (const auto& c : r.last)
            orbit = orbit || (c.mode == TargetCommand::Mode::Follow && c.id == 1);
        check(r.grab.tetherReport().spins == 1 && r.grab.tetherReport().lastAction == 2, "circling the hand starts a spin");
        check(orbit, "the target is steered (Follow) while it spins");
        check(r.grab.tetherView(0).spinning, "and the view says so");
        const float speed = length(r.props.items[0].velocity);
        check(speed > 2.f && speed < 40.f, "the target moves on its orbit");
        // Let go of the grip with the trigger off: slung.
        r.in.hands[0].grip = 0;
        r.in.hands[0].trigger = 0;
        r.run(.05f, [&](float t) {
            const float a = 8.f * t;
            return centre + Vec3{std::cos(a), 0, std::sin(a)} * .25f;
        });
        check(r.g().phase == GrabPhase::None && r.throwEvents == 1 && r.grab.tetherReport().slings == 1,
              "let go while it spins slings it (a throw)");
        check(length(r.thrown) > 2.f && length(r.thrown) <= 25.1f, "at the throw's speed, at most 25 m/s");
        // A hand that only swings straight never spins the target.
        Lab q;
        q.props.add(1, {-.3f, 1.2f, -3.f}, 10);
        q.in.hands[0].grip = 1;
        q.hold(.2f);
        q.run(1.5f, [&](float t) { return centre + Vec3{std::sin(t * 6.f) * .3f, 0, 0}; });
        check(q.grab.tetherReport().spins == 0, "a hand swung to and fro does not spin it");
        // Off: circling does nothing special.
        Lab z(false);
        z.props.add(1, {-.3f, 1.2f, -3.f}, 10);
        z.in.hands[0].grip = 1;
        z.hold(.2f);
        z.run(1.5f, [&](float t) {
            const float a = 8.f * t;
            return centre + Vec3{std::cos(a), 0, std::sin(a)} * .25f;
        });
        check(z.grab.tetherReport().spins == 0, "with tethers off, circling spins nothing");
    }
    // ---- WebGrab: a whip ----
    {
        const Vec3 at{-.3f, 1.2f, 0};
        const auto trial = [&](float metresPerSecond, bool tethers) {
            Lab r(tethers);
            r.props.add(1, {-.3f, 1.2f, -4.f}, 10);
            r.in.hands[0].grip = 1;
            r.hold(.2f);
            r.props.items[0].velocity = {};
            r.hold(1.f);
            const float t0 = r.clock;
            r.run(.6f, [&](float t) {
                const float travelled = std::min(metresPerSecond * (t - t0), 1.5f);
                return at + Vec3{0, 0, travelled};
            });
            return r.grab.tetherReport();
        };
        const auto fast = trial(10.f, true);
        check(fast.whips >= 1 && fast.lastAction >= 1, "snapping the hand back along the line whips");
        check(fast.whips <= 2, "no more than a crack or two");
        const auto slow = trial(1.f, true);
        check(slow.whips == 0, "a slow pull of the hand does not");
        const auto off = trial(10.f, false);
        check(off.whips == 0 && off.serial == 0, "and with tethers off, nothing");
        // The jerk goes toward the hand.
        Lab r;
        r.props.add(1, {-.3f, 1.2f, -4.f}, 10);
        r.in.hands[0].grip = 1;
        r.hold(.2f);
        r.props.items[0].velocity = {};
        r.hold(1.f);
        const float t0 = r.clock;
        r.run(.6f, [&](float t) { return at + Vec3{0, 0, std::min(10.f * (t - t0), 1.5f)}; });
        check(r.launches >= 1, "a Launch (not a throw) carries the jerk");
        check(r.launched.z > 1.f && length(r.launched) <= 16.f, "toward the hand, within the jerk's limit");
    }
    std::printf("tether tests passed: %d checks\n", passed);
    return 0;
}
