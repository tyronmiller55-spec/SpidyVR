// BETA POLISH PASS (Nov 19, plans/beta-polish-pass.md): the portable pieces of the batch 1 safety and log items and
// the batch 4 feel items that have no suite of their own. Portable.
#include "spidy/diag_log.hpp"
#include "spidy/eye_stall.hpp"
#include "spidy/vr_settings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <string_view>

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
} // namespace

int main(int argc, char** argv) {
    // R1: the eye stall watch forgives a module start on the eye worker, not a real stall.
    {
        using namespace eye_stall;
        Watch w;
        check(judge(10000, 0, w) == Verdict::fine, "nothing shown yet is the first-image deadline's");
        check(judge(12999, 10000, w) == Verdict::fine, "under 3 s is fine");
        check(judge(13000, 10000, w) == Verdict::stalled, "3 s with no start is a stall");
        // 2:17 AM: native swinging took 3682 ms right after a pair was shown.
        w.begin(10010);
        check(w.starting(), "a start under way");
        check(judge(13500, 10000, w) == Verdict::forgivenStart, "during the start: forgiven");
        w.end(13692);
        check(w.lastStartMs() == 3682, "the start's length");
        check(judge(13700, 10000, w) == Verdict::forgivenStart, "just after the start: forgiven");
        check(judge(16691, 10000, w) == Verdict::forgivenStart, "the count restarts at the start's end");
        check(judge(16692, 10000, w) == Verdict::stalled, "3 s after the start with no pair is a stall");
        // a start longer than 10 s still ends VR
        Watch longStart;
        longStart.begin(10000);
        check(judge(19999, 9000, longStart) == Verdict::forgivenStart, "a long start under 10 s");
        check(judge(20000, 9000, longStart) == Verdict::stalled, "a start running 10 s stalls");
        longStart.end(21000);
        check(judge(21100, 9000, longStart) == Verdict::stalled, "a finished 11 s start is not forgiven");
        // a start before the last shown pair forgives nothing
        Watch old;
        old.begin(1000);
        old.end(1500);
        check(judge(13000, 10000, old) == Verdict::stalled, "an older start forgives nothing");
    }
    // G4: LOG LEVEL - the default repeats the noisy kinds once per 5 s and takes in the head probe, forearm aim and
    // player torso; VERBOSE is the old 2 s and leaves those three alone; QUIET is 30 s.
    {
        using diag_log::Throttle;
        check(Throttle::kindOf("head probe: anim live: x") >= 0 && Throttle::kindOf("player torso: active 1") >= 0 &&
                  Throttle::kindOf("forearm aim: left") >= 0,
              "the beta's kinds at the default level");
        check(Throttle::kindOf("head probe: anim live: x", 2) < 0 && Throttle::kindOf("forearm sign: x", 2) == 1,
              "verbose leaves the beta's kinds out");
        auto keptOver60 = [](int level) {
            Throttle t;
            t.level(level);
            int kept = 0;
            for (int i = 0; i < 600; ++i)
                kept += t.allow(("head probe: anim live " + std::to_string(i)).c_str(), i * .1) ? 1 : 0;
            return kept;
        };
        check(keptOver60(1) >= 12 && keptOver60(1) <= 13, "default: once per 5 s");
        check(keptOver60(0) >= 2 && keptOver60(0) <= 3, "quiet: once per 30 s");
        check(keptOver60(2) == 600, "verbose: the head probe is not throttled");
        Throttle t;
        check(t.allow("head probe: state anim -> vr", 0) && t.allow("head probe: owner in vr: body x", .1),
              "a changed shape is kept at once");
        vr_settings::Values v;
        check(v.logLevel == 1, "log_level defaults to 1");
        check(vr_settings::withTabOnly({}, "log_level=7\n").logLevel == 2 &&
                  vr_settings::withTabOnly({}, "log_level=0\n").logLevel == 0,
              "log_level is clamped and read");
    }
    // M1: the catalog - which settings are Native Animation's, and each in-game row in exactly one tab.
    {
        using namespace vr_settings;
        unsigned na = 0, upstream = 0;
        for (unsigned i = 1; i <= static_cast<unsigned>(lastItem); ++i)
            (isNativeAnimation(static_cast<Item>(i)) ? na : upstream) += 1;
        check(upstream == 89, "upstream 0.8.7 has 89 settings (its Item has 90 with none)");
        check(na == 139, "the fork's settings are Native Animation's (135, G2b's class rules, NA HELP's page item, JUMP SOUNDS and WALL JUMP SOUND)");
        check(isNativeAnimation(Item::showtime) && isNativeAnimation(Item::headProbe) &&
                  isNativeAnimation(Item::swingKickManual) && !isNativeAnimation(Item::webLines) &&
                  !isNativeAnimation(Item::airFists) && !isNativeAnimation(Item::enemyTypes) &&
                  !isNativeAnimation(Item::smoothTurn) && !isNativeAnimation(Item::none),
              "the NA flag by example");
        std::array<unsigned, 256> inScreens{};
        for (const auto& screen : screens())
            for (Item item : screen.items)
                ++inScreens[static_cast<size_t>(item)];
        for (const auto& row : rows()) {
            if (row.item == Item::none)
                continue;
            check(row.place == Place::tab || row.place == Place::page || row.place == Place::launcher, "a valid place");
            // The pause page's row (VR VIEW) stands outside the tabs; every other row in exactly one.
            check(inScreens[static_cast<size_t>(row.item)] == (row.place == Place::page ? 0u : 1u), row.title);
        }
    }
    // G7: settings validation - every row's choices round-trip through choose/choice and through spidy_vr.ini
    // (sanitized, so a clamp never moves a menu choice), lists have one step per choice, and the in-game RESET ALL
    // never reaches a launcher-only row.
    {
        using namespace vr_settings;
        int roundTrips = 0;
        for (const auto& row : rows()) {
            if (row.item == Item::none || row.item == Item::nativeAnimation)
                continue; // NATIVE ANIMATION is a preset over many values (its own tests)
            const auto s = steps(row.item);
            const int count = row.choices.empty() ? 2 : static_cast<int>(row.choices.size());
            if (!row.choices.empty())
                check(s.size() == row.choices.size() || s.empty(), row.title);
            for (int i = 0; i < count; ++i) {
                Values v;
                choose(row.item, i, v);
                if (choice(row.item, v) != i)
                    continue; // a choice another one shadows (a preset's nearest step): not a menu value of its own
                const Values back = withTabOnly(v, tabOnlyText(v));
                check(choice(row.item, back) == i, row.title);
                ++roundTrips;
            }
        }
        check(roundTrips > 400, "most choices round-trip");
        for (size_t c = 0; c < categoryCount; ++c) {
            Values v;
            for (const auto& row : rows())
                if (row.item != Item::none && row.place == Place::launcher) {
                    const int def = defaultChoice(row.item);
                    const int count = row.choices.empty() ? 2 : static_cast<int>(row.choices.size());
                    choose(row.item, (def + 1) % count, v);
                }
            const Values reset = resetCategory(static_cast<Category>(c), v);
            for (const auto& row : rows()) {
                // The four "(OLD)" shooter rows are what SHOOTERS writes (shooter_lock::setMode): its reset sets
                // them as the mode says, by design.
                const bool replaced = c == static_cast<size_t>(Category::shooters) &&
                                      std::string_view(row.title).find("(OLD)") != std::string_view::npos;
                if (row.item != Item::none && row.place == Place::launcher && !replaced)
                    check(choice(row.item, reset) == choice(row.item, v), row.title);
            }
        }
    }
    // M5: NA HELP - every page fits the game's help column, the topics the owner asked for are there, the pages are
    // the NA · HELP tab's rows in order, and docs/NA_HELP.md is the generated text (--write-help rewrites it).
    {
        using namespace vr_settings;
        std::set<std::string_view> topics;
        for (const auto& page : na_help::pages) {
            check(page.text && std::strlen(page.text) <= na_help::pageChars, page.title);
            check(page.title && std::strlen(page.title) > 0 && std::strlen(page.title) <= 32, page.topic);
            for (const char* c = page.text; *c; ++c)
                check(static_cast<unsigned char>(*c) < 0x80, "help is plain ASCII");
            topics.insert(page.topic);
        }
        check(topics.size() >= 16, "16 topics or more");
        for (const char* t : {"WHAT IS NATIVE ANIMATION", "SHOWTIME", "SWING KICK", "OTHER MODS", "HAND GRAB",
                              "AIR KICKS", "GROUND KICKS", "HANDSTAND", "TROUBLESHOOTING"})
            check(topics.count(t) == 1, t);
        size_t at = settingRowCount;
        for (const auto& page : na_help::pages) {
            const Row& row = rows()[at++];
            check(row.item == Item::helpPage && row.title == page.title && row.help == page.text &&
                      row.choices.size() == 1 && row.place == Place::tab,
                  "a help page's row");
        }
        const auto& tab = screens()[static_cast<size_t>(Category::help)];
        check(tab.items.size() == 1 && tab.items[0] == Item::helpPage &&
                  std::string(tab.tabTitle) == std::string(naTabPrefix) + "HELP",
              "the NA HELP tab");
        Values v;
        check(!choose(Item::helpPage, 0, v) && choice(Item::helpPage, v) == 0 && v == Values{},
              "a help page changes nothing");
        const std::string md = helpMarkdown();
        const std::string path = std::string(SPIDY_SOURCE_DIR) + "/docs/NA_HELP.md";
        if (argc > 1 && std::string(argv[1]) == "--write-help") {
            std::ofstream(path, std::ios::binary) << md;
            std::printf("wrote %s\n", path.c_str());
        }
        std::ifstream in(path, std::ios::binary);
        std::stringstream file;
        file << in.rdbuf();
        // A Windows checkout may carry CRLF line ends (core.autocrlf): compare without them.
        std::string text = file.str();
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        check(text == md, "docs/NA_HELP.md is the generated help (run spidy_beta_polish_tests --write-help)");
    }
    std::printf("beta polish: %d checks passed\n", passed);
    return 0;
}
