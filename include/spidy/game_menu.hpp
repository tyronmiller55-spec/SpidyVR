#pragma once
#include "vr_settings.hpp"
#include <cstdint>
#include <string>

// Spidy's VR settings as seven category tabs after the game's own Settings.
// The list wraps: Up from GAME reaches VR DISPLAY. VR VIEW stays
// on the pause menu's own first page, under RESUME, which the game builds from
// the same kind of items (vr_settings::Place::page). The game
// builds the tab's rows with its own option code, from settings items Spidy
// adds beside the ones its config holds, so they look, move and reset like the
// game's own: switches ON/OFF, lists stepped with left and right, X RESET and
// Y RESET ALL for the current category. Spidy answers the game's questions about those items (their
// texts and values) and takes the changes; the game's own settings and save
// never see them. The title screen's Options have no such tab.
namespace spidy::game_menu {
// Hooks the game's settings menu, and the button prompts' functions with it
// (game_prompts reads them through these hooks). 0 when the tab is hooked;
// 9301-9314 when the code at hook N is neither this game's nor another mod's
// hook to go in front of (no tab; the prompts' other hooks still go in),
// 9400-9499 when hooking failed. Each hook goes in on its own, in front of
// another mod's where one has it (docs/MOD_COMPAT.md).
uint32_t install(uintptr_t base);
// What install() found that was not the supported game's own code, a line
// each: another mod's hooks gone in front of, code left alone.
const std::string& report();
// The prompts' hooks are in: X can send the game's LB + RB where it names them.
bool promptsRead();
// Unhooks. A tab still open shows its rows until the game rebuilds it; then
// they are gone, and a change made in them meanwhile does nothing.
void uninstall();
// What the tab shows: the session's values. A change made in the tab and
// not yet taken is kept.
void publish(const vr_settings::Values&);
// A change made in the tab since the last call: true, with `values` holding it.
bool take(vr_settings::Values&);
struct Telemetry {
    // The game built its Settings with all VR categories (each opening, and
    // each refresh after a change), and the settings changed in it (RESET and
    // RESET ALL included).
    uint64_t tabs{}, changes{};
    // installed: hooked. status: 0, or why the tab is missing: 9501 the game's
    // settings config is not loaded (looked for again at the next opening),
    // 9502 it has no item to build rows from, 9503 the game's code faulted
    // building the tab (not tried again), 9504 it faulted adding VR VIEW to
    // the first page.
    uint32_t installed{}, status{};
    // The pause menu's first pages built with VR VIEW on them.
    uint64_t pages{};
};
Telemetry telemetry();
} // namespace spidy::game_menu
