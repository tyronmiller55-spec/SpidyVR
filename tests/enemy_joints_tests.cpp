// ENEMY JOINTS (enemy_joints.hpp, R1; November 14): the game's name hash, the roles found in a joint table, a
// joint's world position, and the zone choice from joints, with the capsule bands as the fallback and the limb
// zones gated. Portable: no Windows, no game, no memory reads (those are game_joints.cpp's).
#include "spidy/air_kick.hpp"
#include "spidy/enemy_joints.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

using namespace spidy;
using namespace spidy::enemy_joints;
using air_kick::Gesture;
using air_kick::Kick;
using air_kick::Zone;
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
bool near(Vec3 a, Vec3 b, float eps = 1e-3f) {
    return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}
// A joint table of `names`, 16 bytes a joint with the hash at +8 (the rest zero, parent -1).
std::vector<std::uint8_t> tableOf(const std::vector<const char*>& names) {
    std::vector<std::uint8_t> t(names.size() * 16, 0);
    for (size_t j = 0; j < names.size(); ++j) {
        const std::int16_t parent = j ? static_cast<std::int16_t>(j - 1) : static_cast<std::int16_t>(-1);
        std::memcpy(&t[j * 16], &parent, 2);
        const std::uint32_t h = nameHash(names[j]);
        std::memcpy(&t[j * 16 + 8], &h, 4);
    }
    return t;
}
// A standing enemy at `feet`, 1.8 m: every joint (the hero's rig names), roughly where a thug's would be.
Joints standing(Vec3 feet) {
    Joints j;
    const auto put = [&](int role, Vec3 at) {
        j.at[static_cast<size_t>(role)] = feet + at;
        j.valid |= 1u << role;
    };
    put(head, {0, 1.65f, 0});
    put(chest, {0, 1.3f, 0});
    put(leftElbow, {-.35f, 1.15f, 0});
    put(leftWrist, {-.4f, .9f, .1f});
    put(rightElbow, {.35f, 1.15f, 0});
    put(rightWrist, {.4f, .9f, .1f});
    put(leftKnee, {-.12f, .5f, 0});
    put(leftFoot, {-.12f, .08f, 0});
    put(rightKnee, {.12f, .5f, 0});
    put(rightFoot, {.12f, .08f, 0});
    return j;
}
} // namespace

int main() {
    // ---- the game's name hash (the C0 findings re-derived it: pelvis and head) ----
    check(nameHash("pelvis") == 0x8e59b33fu, "pelvis' hash");
    check(nameHash("head") == 0x37519912u, "head's hash");
    check(roleHash(head) == 0x37519912u && roleHash(-1) == 0 && roleHash(roleCount) == 0, "the role hashes");
    check(std::string_view(roleNames[chest]) == "chest" && std::string_view(roleNames[leftWrist]) == "LF_wrist" &&
              std::string_view(roleNames[rightFoot]) == "RT_foot",
          "the hero's joint names");
    for (int a = 0; a < roleCount; ++a)
        for (int b = a + 1; b < roleCount; ++b)
            check(roleHash(a) != roleHash(b), "no two roles share a hash");

    // ---- finding the roles in a joint table ----
    {
        const auto table = tableOf({"a_body", "pelvis", "spine", "chest", "neck", "head", "LF_clavicle", "LF_uparm",
                                    "LF_loarm", "LF_wrist", "RT_loarm", "RT_wrist", "LF_loleg", "LF_foot", "RT_loleg",
                                    "RT_foot", "head"});
        const Found f = findRoles(table.data(), static_cast<unsigned>(table.size() / 16));
        check(f.mask == allRoles, "every role is found");
        check(f.index[chest] == 3 && f.index[head] == 5 && f.index[leftWrist] == 9 && f.index[rightFoot] == 15,
              "by their table index, the first of a name twice");
        const auto bare = tableOf({"a_body", "pelvis", "head", "spine"});
        const Found g = findRoles(bare.data(), 4);
        check(g.mask == (1u << head) && g.index[head] == 2 && g.index[chest] == -1 && g.index[leftFoot] == -1,
              "a rig with only a head names only it");
        check(findRoles(table.data(), 0).mask == 0 && findRoles(nullptr, 5).mask == 0 &&
                  findRoles(table.data(), 5000).mask == 0,
              "no table, no joints, too many: nothing");
        const auto other = tableOf({"root", "hips", "torso", "skull"});
        check(findRoles(other.data(), 4).mask == 0, "another naming gives none: the bands decide");
    }

    // ---- a joint in the world ----
    {
        float joint[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1.5f, 0, 1};
        check(usableJoint(joint), "an ordinary joint");
        float world[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 20, 30, 1};
        check(near(worldPosition(joint, world), {10, 21.5f, 30}), "translated by the instance");
        // Turned a quarter about up (row-major, row vector): x -> -z.
        float turned[16]{0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 5, 0, 0, 1};
        float side[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1};
        check(near(worldPosition(side, turned), {5 + 2 * 0, 0, 2 * -1.f}), "turned by the instance");
        joint[15] = 0;
        check(!usableJoint(joint), "w must be 1");
        joint[15] = 1;
        joint[13] = NAN;
        check(!usableJoint(joint), "not finite");
        joint[13] = 1.5f;
        joint[0] = joint[1] = joint[2] = 0;
        check(!usableJoint(joint), "collapsed axes");
        joint[0] = .001f; // a shrunk head: still a joint
        check(usableJoint(joint), "a shrunk head is a joint");
    }

    // ---- the parts ----
    const Vec3 feet{0, 10, 0};
    {
        const Joints j = standing(feet);
        check(hasPart(j, partHead, false) && hasPart(j, partChest, false), "head and chest without the limbs");
        check(!hasPart(j, partLegs, false) && !hasPart(j, partArms, false), "no limb zones while gated");
        check(hasPart(j, partLegs, true) && hasPart(j, partArms, true), "limb zones with the gate open");
        Vec3 p;
        check(partPoint(j, partHead, false, feet, p) && near(p, feet + Vec3{0, 1.65f, 0}), "the head's point");
        check(partPoint(j, partChest, false, feet, p) && near(p, feet + Vec3{0, 1.3f, 0}), "the chest's point");
        check(!partPoint(j, partLegs, false, feet, p), "no legs while gated");
        // A shin: the nearer leg, the nearest point along it.
        check(partPoint(j, partLegs, true, feet + Vec3{.3f, .3f, 0}, p) && p.x > 0 && near(p.y, feet.y + .3f, .05f),
              "the right shin for a point on the right");
        check(partPoint(j, partLegs, true, feet + Vec3{-.3f, .3f, 0}, p) && p.x < 0, "the left for the left");
        check(partPoint(j, partArms, true, feet + Vec3{.5f, 1.f, 0}, p) && p.x > 0, "the right forearm");
        // One side's arm missing: the other's.
        Joints one = j;
        one.valid &= ~armBits[1];
        check(partPoint(one, partArms, true, feet + Vec3{.5f, 1.f, 0}, p) && p.x < 0, "the arm that was read");
        one.valid &= ~armBits[0];
        check(!hasPart(one, partArms, true) && !partPoint(one, partArms, true, feet, p), "no arm read: no arms");
        Joints noHead = j;
        noHead.valid &= ~(1u << head);
        check(partOfHit(noHead, feet + Vec3{0, 1.6f, 0}, true) == -1, "no head joint: the bands decide");
        Joints noChest = j;
        noChest.valid &= ~(1u << chest);
        check(partOfHit(noChest, feet + Vec3{0, 1.2f, 0}, true) == -1, "no chest joint: the bands decide");
        check(partOfHit({}, feet, true) == -1, "no joints: the bands decide");
        check(partOfHit(j, {NAN, 0, 0}, true) == -1, "a point that is not finite");
    }
    {
        const Joints j = standing(feet);
        const auto at = [&](float x, float y, float z = 0) { return feet + Vec3{x, y, z}; };
        check(partOfHit(j, at(0, 1.7f), false) == partHead && partOfHit(j, at(0, 1.2f), false) == partChest,
              "the head and the chest, nearest");
        // Without the limb zones the shin is no joint zone: a hit there is the chest's (nearest joint) ...
        check(partOfHit(j, at(.12f, .3f), false) == partChest, "gated: a hit on a shin is not a limb");
        // ... and with them it is the legs, an arm is the arms.
        check(partOfHit(j, at(.12f, .3f), true) == partLegs, "open: the shin is the legs");
        check(partOfHit(j, at(.4f, 1.0f, .05f), true) == partArms, "open: the forearm is the arms");
        check(partOfHit(j, at(0, 1.3f), true) == partChest, "open: the torso is still the chest");
    }

    // ---- zoneOfHit: joints, else the bands, and the gate ----
    {
        const Joints j = standing(feet);
        bool by = false;
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{0, 1.7f, 0}, j, false, &by) == Zone::head && by,
              "from the joints: the head");
        // A crouching enemy (joints low, the capsule still 1.8 tall): the bands say chest, his head is there.
        Joints low = standing(feet);
        low.at[head] = feet + Vec3{0, 1.05f, 0};
        low.at[chest] = feet + Vec3{0, .75f, 0};
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{0, 1.1f, 0}, low, false, &by) == Zone::head && by,
              "a crouched enemy's head, where the band says chest");
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{0, 1.1f, 0}, {}, false, &by) == Zone::chest && !by,
              "with no joints the same hit is the band's");
        // Gated: low on the body is the band's legs, not a joint.
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{.1f, .3f, 0}, j, false, &by) == Zone::legs && by,
              "gated: the legs by the band under 35 %");
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{.1f, .3f, 0}, j, true, &by) == Zone::legs && by,
              "open: the legs by the shin");
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{.4f, 1.0f, .05f}, j, false, &by) != Zone::arms,
              "gated: never the arms");
        check(air_kick::zoneOfHit(feet, 1.8f, feet + Vec3{.4f, 1.0f, .05f}, j, true, &by) == Zone::arms && by,
              "open: the arms by the forearm");
        // zonePoint: the joint, else the band.
        check(near(air_kick::zonePoint(feet, 1.8f, Zone::head, j, false, feet, &by), feet + Vec3{0, 1.65f, 0}) && by,
              "the head's point from the joint");
        check(near(air_kick::zonePoint(feet, 1.8f, Zone::head, {}, false, feet, &by),
                   air_kick::zonePoint(feet, 1.8f, Zone::head)) && !by,
              "or the band's");
        check(near(air_kick::zonePoint(feet, 1.8f, Zone::legs, j, false, feet, &by),
                   air_kick::zonePoint(feet, 1.8f, Zone::legs)) && !by,
              "the legs' point is the band's while gated");
        check(air_kick::zonePoint(feet, 1.8f, Zone::legs, j, true, feet + Vec3{.3f, .3f, 0}, &by).x > 0 && by,
              "and the shin's with the gate open");
    }

    // ---- the kick: off by default means unchanged; with joints, the enemy's own zones ----
    {
        const auto target = [&](bool withJoints) {
            Kick::Target t{7, feet + Vec3{0, 0, -1.5f}, 1.8f, .3f};
            if (withJoints)
                t.joints = standing(t.feet);
            return t;
        };
        const auto frame = [&](std::span<const Kick::Target> ts) {
            Kick::Frame f;
            f.feet = feet;
            f.eyeOffset = {0, 1.6f, 0};
            f.gaze = {0, 0, -1};
            f.targets = ts;
            return f;
        };
        std::vector<Kick::Target> none{target(false)}, with{target(true)};
        {
            Kick k;
            const auto o = k.ask(frame(none), {Gesture::doubleDive, 1, 0, 1});
            check(o.event == Kick::Event::started && o.zone == Zone::chest && !o.byJoints,
                  "no joints: the chest by the band, as before");
        }
        {
            Kick k;
            const auto o = k.ask(frame(with), {Gesture::doubleDive, 1, 0, 1});
            check(o.event == Kick::Event::started && o.zone == Zone::chest && o.byJoints && o.target == 7,
                  "with joints: the chest, by the joint");
        }
        {
            // A freeform blow with the limb zones asks for the arms when the gaze is on a forearm.
            air_kick::Config c;
            c.limbZones = true;
            Kick k(c);
            std::vector<Kick::Target> arm{target(true)};
            Kick::Frame f = frame(arm);
            // Look at his right forearm (+.37, 1.02, -1.5 + .05) from the eyes.
            const Vec3 to = Vec3{.37f, 1.02f, -1.45f} - Vec3{0, 1.6f, 0};
            f.gaze = normalized(to);
            const auto o = k.ask(f, {Gesture::freeform, 1, 90, 1});
            check(o.event == Kick::Event::started && o.target == 7 && o.zone == Zone::arms && o.byJoints,
                  "the limb zones on: the gaze on a forearm aims at the arms");
        }
        {
            air_kick::Config c; // the gate shut
            Kick k(c);
            std::vector<Kick::Target> arm{target(true)};
            Kick::Frame f = frame(arm);
            f.gaze = normalized(Vec3{.37f, 1.02f, -1.45f} - Vec3{0, 1.6f, 0});
            const auto o = k.ask(f, {Gesture::freeform, 1, 90, 1});
            check(o.zone != Zone::arms, "the limb zones off: never the arms");
        }
        {
            // The arms with no arm joints read: no arms zone even with the gate open.
            air_kick::Config c;
            c.limbZones = true;
            Kick k(c);
            Kick::Target t = target(true);
            t.joints.valid &= ~(armBits[0] | armBits[1]);
            std::vector<Kick::Target> arm{t};
            Kick::Frame f = frame(arm);
            f.gaze = normalized(Vec3{.37f, 1.02f, -1.45f} - Vec3{0, 1.6f, 0});
            check(k.ask(f, {Gesture::freeform, 1, 90, 1}).zone != Zone::arms, "no forearm read: no arms zone");
        }
        {
            // A hit reports where the joints say: a dive's foot meets him at the chest and says chest, by joints.
            Kick k;
            const Kick::Frame f = [&] {
                auto g = frame(with);
                g.seconds = 1.f / 90.f;
                return g;
            }();
            k.ask(f, {Gesture::doubleDive, 1, 0, 1});
            Kick::Output hit;
            for (int i = 0; i < 90 && k.busy(); ++i) {
                const auto o = k.update(f);
                if (o.event == Kick::Event::hit)
                    hit = o;
            }
            check(hit.event == Kick::Event::hit && hit.byJoints && hit.target == 7, "the blow's zone is by joints");
            check(hit.zone == Zone::chest || hit.zone == Zone::head || hit.zone == Zone::legs, "a zone");
        }
    }
    std::printf("enemy_joints: %d checks passed\n", passed);
    return 0;
}
