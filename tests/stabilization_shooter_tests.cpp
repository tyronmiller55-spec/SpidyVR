// CAMERA MOTION (camera_motion.hpp), SHOOTERS: LOCKED TO ARM (shooter_lock.hpp)
// and where their rows are (November 11: "I can't find legs or camera
// settings in any of the menus"). Portable: no Windows, no headset.
#include "spidy/camera_motion.hpp"
#include "spidy/realistic_flip.hpp"
#include "spidy/shooter_lock.hpp"
#include "spidy/vr_settings.hpp"
#include "spidy/web_aim.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

using namespace spidy;
namespace {
int passed = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
    ++passed;
}
constexpr float deg = 3.14159265f / 180;
Vec3 forwardOf(Quat q) {
    return q.rotate({0, 0, -1});
}
Vec3 upOf(Quat q) {
    return q.rotate({0, 1, 0});
}
float pitchOf(Quat q) {
    return std::asin(std::clamp(forwardOf(q).y, -1.f, 1.f));
}
float headingOf(Quat q) {
    const Vec3 f = forwardOf(q);
    return std::atan2(-f.x, -f.z);
}
// Roll: the up vector's lean toward the right, about the forward line.
float rollOf(Quat q) {
    return std::atan2(-q.rotate({1, 0, 0}).y, upOf(q).y);
}
bool near(float a, float b, float eps) {
    return std::abs(a - b) <= eps;
}
Quat head(float yaw, float pitch, float roll) {
    return Quat::yaw(yaw) * Quat::around({1, 0, 0}, pitch) * Quat::around({0, 0, -1}, roll);
}

void cameraTilt() {
    using namespace camera_motion;
    const Quat q = head(40 * deg, 20 * deg, 10 * deg);
    check(near(pitchOf(q), 20 * deg, 1e-3f) && near(rollOf(q), 10 * deg, 1e-3f), "test head built as asked");
    Amounts full;
    const Quat same = tilted(q, full);
    check(near(forwardOf(same).x, forwardOf(q).x, 1e-6f) && near(upOf(same).y, upOf(q).y, 1e-6f),
          "100% everything is the animation exactly");
    Amounts level;
    level.pitch = level.roll = 0;
    const Quat flat = tilted(q, level);
    check(near(upOf(flat).y, 1, 1e-4f), "0% pitch and roll is level");
    check(near(headingOf(flat), 40 * deg, 1e-3f), "leveling keeps the animation's heading (not HEAD LEVEL's latch)");
    Amounts half;
    half.pitch = .5f;
    half.roll = 1;
    const Quat h = tilted(q, half);
    check(near(pitchOf(h), 10 * deg, .6f * deg), "PITCH 50% halves the nod");
    check(near(headingOf(h), 40 * deg, .6f * deg), "pitch scaling keeps the heading");
    Amounts noRoll;
    noRoll.roll = 0;
    const Quat r = tilted(q, noRoll);
    check(near(rollOf(r), 0, .6f * deg) && near(pitchOf(r), 20 * deg, .6f * deg), "ROLL 0% keeps the pitch");
    // HEAD ANGLE: above the horizon is up.
    Amounts up10 = level;
    up10.headAngle = 10 * deg;
    check(near(pitchOf(tilted(head(-70 * deg, 0, 0), up10)), 10 * deg, 1e-3f), "HEAD ANGLE UP 10 looks 10 up");
    Amounts down20 = full;
    down20.headAngle = -20 * deg;
    check(!down20.tiltIdentity(), "a head angle is not the identity");
    check(near(pitchOf(tilted(head(0, 5 * deg, 0), down20)), -15 * deg, 1e-3f),
          "HEAD ANGLE adds to the animation's pitch (DOWN 20 from 5 up is 15 down)");
    // A flip: past 150 degrees of tilt the scaling is gone, so upside down is
    // the animation's own, and turning over is continuous.
    const Quat flip = head(0, 170 * deg, 0);
    const Quat f2 = tilted(flip, level);
    check(near(upOf(f2).y, upOf(flip).y, 1e-3f), "a flip's tilt passes through whole near upside down");
    float worst = 0;
    Quat prev = tilted(head(10 * deg, 0, 3 * deg), level);
    for (int a = 1; a <= 360; ++a) {
        const Quat now = tilted(head(10 * deg, static_cast<float>(a) * deg, 3 * deg), level);
        const float step = std::acos(std::clamp(dot(forwardOf(now), forwardOf(prev)), -1.f, 1.f));
        worst = std::max(worst, step);
        prev = now;
    }
    check(worst < 6 * deg, "a full flip under 0% pitch/roll turns smoothly, no pop where up turns over");
}

// Forward compatibility (a future handstand / inverted view): with a reference
// up supplied, the result turns with the frame (equivariant), so nothing
// assumes world +Y; HEAD ANGLE is added once, toward that up.
void cameraFrame() {
    using namespace camera_motion;
    const Quat frames[] = {Quat::around({0, 0, 1}, 180 * deg), Quat::around({1, 0, 0}, 180 * deg),
                           Quat::around(normalized(Vec3{1, 2, .5f}), 73 * deg)};
    Amounts base;
    base.pitch = .3f;
    base.roll = 0;
    base.headAngle = 12 * deg;
    base.bobVertical = 0;
    base.bobSide = .5f;
    for (const Quat f : frames) {
        Amounts inFrame = base;
        inFrame.up = f.rotate({0, 1, 0});
        for (const Quat q : {head(40 * deg, 20 * deg, 10 * deg), head(-120 * deg, -35 * deg, 25 * deg)}) {
            const Quat expected = f * tilted(q, base);
            const Quat got = tilted(f * q, inFrame);
            float worst = 0;
            for (const Vec3 axis : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}})
                worst = std::max(worst, length(expected.rotate(axis) - got.rotate(axis)));
            check(worst < 2e-4f, "levelling and HEAD ANGLE turn with a supplied up (an inverted frame works)");
        }
        Bob world, framed;
        float worst = 0;
        for (int i = 0; i < 300; ++i) {
            const float t = static_cast<float>(i) / 90;
            const Vec3 raw{.04f * std::sin(t * 11.f), 1.6f + .05f * std::sin(t * 14.f), .02f * std::cos(t * 9.f)};
            const Vec3 a = f.rotate(world.update(raw, 1.f / 90, base));
            const Vec3 b = framed.update(f.rotate(raw), 1.f / 90, inFrame);
            worst = std::max(worst, length(a - b));
        }
        check(worst < 1e-4f, "the bob's up/down follows the supplied up");
    }
    // Once: HEAD ANGLE on a level head is exactly the angle, not twice it.
    Amounts up20;
    up20.headAngle = 20 * deg;
    check(near(pitchOf(tilted(head(0, 0, 0), up20)), 20 * deg, 1e-4f), "HEAD ANGLE applied exactly once");
}

void cameraBob() {
    using namespace camera_motion;
    Amounts a;
    a.bobVertical = 0;
    a.bobSide = .5f;
    Bob bob;
    float worstY = 0, worstX = 0;
    const float dt = 1.f / 90;
    for (int i = 0; i < 90 * 6; ++i) {
        const float t = static_cast<float>(i) * dt;
        const Vec3 raw{.04f * std::sin(t * 11.f), 1.6f + .05f * std::sin(t * 14.f), 0};
        const Vec3 out = bob.update(raw, dt, a);
        if (t > 3) {
            worstY = std::max(worstY, std::abs(out.y - 1.6f));
            worstX = std::max(worstX, std::abs(out.x));
        }
        check(std::abs(out.y - raw.y) <= maxBobRemovedVertical + 1e-5f, "the eyes stay near the real head");
    }
    check(worstY < .012f, "BOB UP/DOWN 0% takes a run's bob (5 cm) down to under 1.2 cm");
    check(worstX < .03f && worstX > .01f, "BOB SIDE 50% leaves about half the sway");
    // A crouch: the baseline follows, the eyes do not stay up.
    Bob crouch;
    Vec3 out{};
    for (int i = 0; i < 90 * 3; ++i)
        out = crouch.update({0, i < 45 ? 1.6f : 1.1f, 0}, dt, a);
    check(near(out.y, 1.1f, .01f), "a held crouch is followed, only the bob is calmed");
    // 100% is the identity.
    Bob none;
    Amounts full;
    const Vec3 p{.1f, 1.5f, -.2f};
    check(length(none.update(p, dt, full) - p) < 1e-7f, "100% bob is the head exactly");
    // A cut restarts.
    Bob cut;
    cut.update({0, 1.6f, 0}, dt, a);
    out = cut.update({0, 5.f, 0}, dt, a);
    check(near(out.y, 5.f, 1e-5f), "a cut is not smoothed");
}

void cameraMatrix() {
    using namespace camera_motion;
    Mat4 m{};
    check(animated_view::headFrame({1, 2, 3}, normalized(Vec3{.3f, .4f, -1}), {0, 1, 0}, {}, m), "native head");
    Amounts full;
    const Mat4 same = stabilize(m, full);
    check(std::memcmp(same.data(), m.data(), sizeof(m)) == 0, "the default amounts return the head bit for bit");
    Amounts level;
    level.pitch = level.roll = 0;
    const Mat4 flat = stabilize(m, level);
    // Native rows: right, down, forward, eye.
    check(near(flat[9], 0, 1e-4f) && near(flat[5], -1, 1e-4f), "a leveled native head looks level");
    check(near(flat[12], 1, 1e-5f) && near(flat[13], 2, 1e-5f) && near(flat[14], 3, 1e-5f),
          "tilt never moves the eyes");
    check(near(std::atan2(flat[8], flat[10]), std::atan2(m[8], m[10]), 1e-3f), "native heading kept");
}

void cameraSettings() {
    using namespace vr_settings;
    const Values d{};
    check(camera_motion::fromValues(d).identity(), "defaults change nothing (existing players keep their feel)");
    check(choice(Item::cameraMotion, d) == 0, "the default preset is FULL");
    Values v;
    check(choose(Item::cameraMotion, 3, v) && v.cameraPitchPct == 25 && v.cameraRollPct == 0 &&
              v.cameraBobVerticalPct == 25 && v.cameraBobSidePct == 25,
          "COMFORT fills the four amounts");
    check(choice(Item::cameraMotion, v) == 3, "and shows COMFORT");
    check(choose(Item::cameraRoll, choice(Item::cameraRoll, v) + 1, v) && choice(Item::cameraMotion, v) == 4,
          "editing one amount shows CUSTOM");
    const Values before = v;
    check(!choose(Item::cameraMotion, 4, v) && v == before, "choosing CUSTOM changes nothing");
    check(choose(Item::cameraHeadAngle, 0, v) && v.cameraHeadAngleDeg == -30, "HEAD ANGLE DOWN 30");
    check(choose(Item::cameraHeadAngle, 8, v) && v.cameraHeadAngleDeg == 30, "HEAD ANGLE UP 30");
    const std::string text = tabOnlyText(v);
    for (const char* key : {"camera_pitch_pct=", "camera_roll_pct=", "camera_bob_vertical_pct=",
                            "camera_bob_side_pct=", "camera_head_angle_deg=30"})
        check(text.find(key) != std::string::npos, key);
    const Values back = withTabOnly({}, text);
    check(back.cameraHeadAngleDeg == 30 && back.cameraRollPct == v.cameraRollPct, "spidy_vr.ini round trip");
    const Values clamped = withTabOnly({}, "camera_head_angle_deg=-90\ncamera_pitch_pct=400\n");
    check(clamped.cameraHeadAngleDeg == -30 && clamped.cameraPitchPct == 100, "out-of-range ini values clamped");
    check(resetCategory(Category::camera, v) == Values{} ||
              camera_motion::fromValues(resetCategory(Category::camera, v)).identity(),
          "RESET ALL on the camera tab is as before");
}

void shooters() {
    using namespace vr_settings;
    using shooter_lock::Mode;
    struct Case {
        bool wrist;
        int source;
        bool palm;
        int blend;
        Mode mode;
        int strength;
    };
    const Case cases[] = {
        {false, 0, false, 100, Mode::controller, 100}, {true, 0, false, 100, Mode::arm, 100},
        {false, 1, false, 50, Mode::arm, 50},          {true, 1, false, 75, Mode::arm, 75},
        {true, 0, false, 25, Mode::arm, 100}, // WRIST SHOOTERS alone always took all of it
        {false, 0, true, 100, Mode::palm, 100},        {true, 1, true, 100, Mode::palm, 100},
    };
    for (const auto& c : cases) {
        Values v;
        v.wristShooters = c.wrist;
        v.webAimSource = c.source;
        v.palmShooters = c.palm;
        v.forearmBlend = c.blend;
        check(shooter_lock::mode(v) == c.mode, "old keys read as one SHOOTERS choice");
        check(shooter_lock::strength(v) == c.strength, "old keys read as one LOCK STRENGTH");
        check(choice(Item::shooterMode, v) == static_cast<int>(c.mode), "the row shows it");
    }
    // The user's own file (Nov 11 22:38): wrist_shooters=1 web_aim_source=1
    // forearm_blend=100 forearm_aim_limit=35 palm_shooters=0.
    const Values mine = withTabOnly(
        {}, "wrist_shooters=1\nweb_aim_source=1\nforearm_blend=100\nforearm_aim_limit=35\npalm_shooters=0\n");
    check(shooter_lock::mode(mine) == Mode::arm && shooter_lock::strength(mine) == 100,
          "the player's saved settings are LOCKED TO ARM, 100%");
    Values v;
    check(choose(Item::shooterMode, 1, v) && v.wristShooters && v.webAimSource == 1 && !v.palmShooters,
          "LOCKED TO ARM sets the old keys an older build reads the same way");
    check(choose(Item::shooterLock, 1, v) && v.forearmBlend == 50 && shooter_lock::strength(v) == 50,
          "LOCK STRENGTH is FOREARM BLEND");
    check(!inactiveReason(Item::shooterLock, v), "strength usable when locked");
    check(choose(Item::shooterMode, 2, v) && v.palmShooters && !v.wristShooters && v.webAimSource == 0,
          "PALM alone");
    check(inactiveReason(Item::shooterLock, v), "strength explains itself unless locked");
    check(choose(Item::shooterMode, 0, v) && shooter_lock::mode(v) == Mode::controller, "back to CONTROLLER");
    // Locked means exactly on the arm: no 15 degree inward cap.
    const Quat aim = Quat::around({1, 0, 0}, -35 * deg);
    const Vec3 forearm = normalized(Vec3{.4f, .1f, -1});
    const Vec3 web = web_aim::clampedForearmDir(forearm, aim, 0, 0.f);
    check(length(web - forearm) < 1e-5f, "LOCKED TO ARM's line is the forearm exactly");
    const Quat turn = web_aim::forearmWorldTurn(forearm, aim, 1, 0.f);
    check(length(aim.rotate(turn.rotate({0, 0, -1})) - forearm) < 1e-4f, "its turn puts the web on the forearm");
    // The visible arm in animation is used once the SpidyVR arm is on the hand.
    shooter_lock::Arm arm{true, {0, 1.3f, 0}, {0, 1.3f, -.27f}, .01f};
    check(shooter_lock::visibleUsable(arm, true, .9f, true), "the drawn arm in Native Animation is followed");
    check(!shooter_lock::visibleUsable(arm, true, .5f, true), "not while the overlay is still fading in");
    check(shooter_lock::visibleUsable(arm, false, 0, true), "SpidyVR's own arm");
    arm.handError = .2f;
    check(!shooter_lock::visibleUsable(arm, false, 1, true), "a wrist off the hand is not");
}

// LEG CONTROL with REALISTIC FLIPS (November 11 23:35): the legs' spread sets
// the body's inertia inside RealisticFlip (angular momentum kept), and their
// swing turns the body without adding spin.
void realisticLegs() {
    RealisticFlipSettings settings;
    settings.enabled = true;
    const float dt = 1.f / 90;
    const auto spun = [&](RealisticFlip& flip) {
        RealisticFlight flight;
        flight.valid = true;
        flight.velocity = {0, 0, -6};
        RealisticControls spin;
        spin.manualActive = true;
        spin.manualAngular = {-4, 0, 0};
        for (int i = 0; i < 60; ++i)
            flip.update(flight, true, false, 0, dt, settings, spin);
        return flight;
    };
    RealisticFlip same, tucked;
    RealisticFlight flight = spun(same);
    spun(tucked);
    same.update(flight, true, false, 0, dt, settings);
    RealisticFlight tuck = flight;
    tuck.legInertia = 1 / 1.5f; // a jackknife: spinScale 1.5
    tucked.update(tuck, true, false, 0, dt, settings);
    const float ratio = length(tucked.angularVelocity()) / length(same.angularVelocity());
    check(ratio > 1.4f && ratio < 1.6f, "tucking the legs speeds the realistic spin by the inertia (momentum kept)");
    // Extending again: back to the first rate.
    tucked.update(flight, true, false, 0, dt, settings);
    same.update(flight, true, false, 0, dt, settings);
    check(std::abs(length(tucked.angularVelocity()) / length(same.angularVelocity()) - 1) < .05f,
          "extending slows it back");
    // The swing's counter-turn: the orientation turns, the spin does not.
    RealisticFlip a, b;
    spun(a);
    spun(b);
    RealisticFlight swing = flight;
    swing.legTurn = legTurnVector(Quat{}, .05f, 0);
    a.update(flight, true, false, 0, dt, settings);
    b.update(swing, true, false, 0, dt, settings);
    check(length(a.angularVelocity() - b.angularVelocity()) < 1e-5f, "the legs' turn adds no spin");
    const float apart = std::acos(std::clamp(dot(a.orientation().rotate({0, 1, 0}), b.orientation().rotate({0, 1, 0})), -1.f, 1.f));
    check(std::abs(apart - .05f) < .005f, "the legs' turn turns the body by it");
    // On the ground nothing.
    RealisticFlip ground;
    RealisticFlight still;
    still.valid = true;
    still.legTurn = {.05f, 0, 0};
    ground.update(still, false, false, 0, dt, settings);
    check(ground.level(), "no leg turn on the ground");
    check(legTurnVector(Quat{}, .2f, 0).x == .1f, "a frame's leg turn is capped as for controlled flips");
}

void menus() {
    using namespace vr_settings;
    // Each of these is reachable: a row on exactly one in-game tab, shown
    // there (Place::tab), on a tab short enough to see.
    const Item wanted[] = {Item::cameraMotion,     Item::cameraPitch,      Item::cameraRoll,
                           Item::cameraBobVertical, Item::cameraBobSide,   Item::cameraHeadAngle,
                           Item::cameraStabilization, Item::cameraVertical, Item::cameraHorizontal,
                           Item::cameraAngle,      Item::lightVertical,    Item::lightHorizontal,
                           Item::lightAngle,       Item::heavyVertical,    Item::heavyHorizontal,
                           Item::heavyAngle,       Item::legControl,       Item::legStiffness,
                           Item::legDamping,       Item::legCoupling,      Item::shooterMode,
                           Item::shooterLock,      Item::webAngle};
    for (Item item : wanted) {
        const Row* row = rowFor(item);
        check(row && row->place == Place::tab, "a new row is on the in-game tab");
        check(row->help && std::strlen(row->help) <= 80, "help fits the game's column");
        int screensWith = 0;
        for (const auto& screen : screens())
            for (Item i : screen.items)
                screensWith += i == item;
        check(screensWith == 1, "on exactly one tab");
    }
    for (const auto& screen : screens()) {
        size_t inGame = 0;
        for (Item i : screen.items)
            inGame += rowFor(i) && rowFor(i)->place == Place::tab;
        check(inGame > 0 && inGame <= 20, "every tab short enough to see");
    }
    check(std::strcmp(screens()[static_cast<size_t>(Category::camera)].tabTitle, "VR CAMERA & STABILIZATION") == 0 &&
              std::strcmp(screens()[static_cast<size_t>(Category::legs)].tabTitle, "VR LEGS (ADVANCED)") == 0 &&
              std::strcmp(screens()[static_cast<size_t>(Category::shooters)].tabTitle, "VR WEB SHOOTERS") == 0,
          "the sections are named for what they hold");
    for (Item old : {Item::webAimSource, Item::forearmBlend, Item::forearmAimLimit, Item::wristShooters,
                     Item::palmShooters})
        check(rowFor(old) && rowFor(old)->place == Place::launcher, "the old wrist rows are off the in-game tab");
    // LEGS: never hidden; the note says what it needs, and ON makes it work.
    // November 11 23:35: with REALISTIC FLIPS too, left on.
    Values v;
    v.realisticFlips = true;
    check(!inactiveReason(Item::legControl, v), "LEG CONTROL usable with REALISTIC FLIPS");
    check(choose(Item::legControl, 1, v) && v.legControl && v.realisticFlips && !v.controlledFlips,
          "LEG CONTROL ON keeps REALISTIC FLIPS as it was");
    check(!inactiveReason(Item::legCoupling, v), "leg tuning usable with REALISTIC FLIPS");
    Values none;
    check(inactiveReason(Item::legControl, none) && shownWhenInactive(Item::legControl),
          "with no stick flips mode, LEG CONTROL stays visible and says why");
    check(choose(Item::legControl, 1, none) && none.legControl && none.controlledFlips && !none.realisticFlips,
          "with neither, LEG CONTROL ON turns CONTROLLED FLIPS on");
    check(!inactiveReason(Item::legControl, none) && !inactiveReason(Item::legCoupling, none), "and then works");
    for (Item item : {Item::legStiffness, Item::legDamping, Item::legCoupling})
        check(shownWhenInactive(item), "leg tuning stays visible");
    // The launcher has a control for each (the launcher never had camera or
    // leg controls before November 11).
#ifdef SPIDY_SOURCE_DIR
    std::ifstream file(std::string(SPIDY_SOURCE_DIR) + "/apps/launcher/ui.cpp");
    std::stringstream text;
    text << file.rdbuf();
    const std::string ui = text.str();
    check(!ui.empty(), "the launcher's source");
    const char* names[] = {"Item::cameraMotion",      "Item::cameraPitch",      "Item::cameraRoll",
                           "Item::cameraBobVertical", "Item::cameraBobSide",    "Item::cameraHeadAngle",
                           "Item::cameraStabilization", "Item::cameraVertical", "Item::cameraHorizontal",
                           "Item::cameraAngle",       "Item::lightVertical",    "Item::lightHorizontal",
                           "Item::lightAngle",        "Item::heavyVertical",    "Item::heavyHorizontal",
                           "Item::heavyAngle",        "Item::legControl",       "Item::legStiffness",
                           "Item::legDamping",        "Item::legCoupling",      "Item::shooterMode",
                           "Item::shooterLock"};
    for (const char* name : names)
        check(ui.find(std::string("tabCombo(\"") ) != std::string::npos &&
                  (ui.find(std::string(", ") + name + ")") != std::string::npos),
              name);
    check(ui.find("\"Camera motion (how much)\"") != std::string::npos &&
              ui.find("\"Legs (advanced)\"") != std::string::npos &&
              ui.find("\"Web shooters\"") != std::string::npos,
          "the launcher's sections are labelled");
#endif
}
} // namespace

int main() {
    cameraTilt();
    cameraFrame();
    cameraBob();
    cameraMatrix();
    cameraSettings();
    shooters();
    realisticLegs();
    menus();
    std::printf("%d stabilization/shooter checks passed\n", passed);
    return 0;
}
