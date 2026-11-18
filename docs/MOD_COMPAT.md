# Running Spidy beside other mods

November 18 (pt121u): a sky mod installed as `winmm.dll` in the game's folder
took the SPIDY VR tab out of the game's Settings, and with it X's throw (the
game's LB + RB on a prop). This page says how Spidy gets into the game, where
it can meet another mod, and how to install the two together.

## How Spidy gets into the game

- **Nothing of Spidy's goes into the game's folder.** The launcher starts the
  game (or finds it running) and loads Spidy's libraries into it from Spidy's
  own folder: `spidy_bridge.dll`, `spidy_ray_bridge.dll`,
  `spidy_movement_bridge.dll`, `spidy_stereo_probe.dll` and the render probes
  (`tools/run_game_vr.py`, `SESSION_MODULES`). No other mod has those names,
  and Spidy is no stand-in for a Windows library (`dxgi.dll`, `winmm.dll`,
  `version.dll`, `dinput8.dll` ...), so it never overwrites or replaces
  another mod's file, and another mod's file never replaces Spidy's.
- **Spidy's settings** are its own: the launcher's profile and the session's
  settings file in Spidy's folder. No shared ini.
- **Where two mods can meet** is inside the game's code: two mods hooking the
  same game function. Spidy's in-game tab and the button prompts hook 14
  functions of the game (`src/game_menu.cpp`, `hooks[]`; addresses in
  docs/REFERENCE.md): the UI's call wrapper `0x1d1cf30`, the settings rows'
  answers (`0x72d5f0`-`0x730ee0`, `0x7dc400`, `0x80a220`), the text lookup
  `0x1749970`/`0x1749ae0` and the HUD prompt list's show and hide
  (`0x7793d0`, `0x77a1e0`).

## What the sky mod collided on

The two sessions with the sky mod (November 18, 2:09 AM on 635645e and 1:13 PM
on fdd7b7f) both say:

- `WARNING: another mod or tool loads into the game from its folder (winmm.dll)`;
  November 17's session, without it, has no such line.
- `No SPIDY VR tab in the game's Settings (9301)`: 9301 is hook 0, the UI's call
  wrapper `0x1d1cf30`. Its first bytes were no longer the game's
  (`80 b9 b0 00 00 00 00 75 03`): the sky mod had hooked it first. A menu mod
  hooks that function because every Settings screen goes through it.
- In the session summaries, `prompts.invokes` 21,645 and `prompts.rewrites` 83
  on November 17 (`last_asked` "LB + RB (hold)"), and **0** in both sessions
  since. Spidy read the button prompts through the same hooks, which went in
  all together or not at all, so the game's "LB + RB" was never seen, and X
  (which sends LB + RB only where the game names them, `game_pad::NamedPress`)
  sent nothing. The gadget wheel on a held X still worked (45 gadget shots).

So X was lost to the conflict, not to the Y+X kick chord.

## What Spidy does now

- **Each hook goes in on its own.** A function whose first bytes are a jump out
  of the game's own code (`E9`, `FF 25`, or `mov rax`/`jmp rax`: how MinHook
  and other hooking libraries patch) is another mod's hook: Spidy's hook goes
  in front of it and passes every call on through it, so both mods see every
  call (`include/spidy/hook_chain.hpp`). Both menus can add to the game's
  Settings.
- **Anything else** at a hook (code Spidy does not know) keeps that hook alone
  out. The tab needs every one of its hooks; the button prompts' hooks (the
  call wrapper, the text lookup, the prompt show and hide) go in without the
  tab, so X's throw does not depend on the tab.
- **The diagnostics name it.** With RUN LOGS on, the diag log starts with a
  `mods:` line for every hook found taken: the function, the library whose
  hook it is (or "an unnamed library"), and whether Spidy went in front of it
  or left it alone. The console says whether the tab and the prompts went in.
- **The launcher's warning** about a plugin loader (`winmm.dll`, `version.dll`,
  `dinput8.dll`, `winhttp.dll`, `dsound.dll`) now says Spidy runs beside it.
  Graphics stand-ins (`dxgi.dll`, `d3d11.dll`, `d3d12.dll`, ...) keep the
  stronger warning, and R.E.A.L. VR is still refused (it takes the picture).

## Installing beside other mods

1. Install the other mod as its own instructions say (usually its loader,
   such as `winmm.dll` or `dinput8.dll`, and its plugin into the game's folder
   or a `scripts`/`plugins` folder there).
2. Unzip Spidy into its own folder **outside** the game's folder, as always.
   Do not copy any Spidy file into the game's folder, and do not rename
   Spidy's libraries to a loader's names.
3. Start the game from Spidy's launcher (START VR). The other mod loads with
   the game; Spidy loads after it and goes in front of any function both hook.
4. Check the console: no `No SPIDY VR tab` line means the tab is in. If it is
   there, the line also says whether the button prompts (X's throw) still work.
   Send the diag log (`mods:` lines) with any report.
5. To take the other mod out, move its files out of the game's folder. Verify
   integrity of game files in Steam does not remove them.

## Open

- A mod that hooks with something other than a jump at the function's first
  byte (a short `EB` jump, or code patched in place) still keeps that one
  hook out; the `mods:` line shows its bytes.
- Both menus adding tabs to the Settings list at once has not been seen in a
  headset yet; if the sky mod's own options vanish, say so with the diag log.
- A mod that loads after Spidy and checks the function's bytes before hooking
  may refuse to hook over Spidy's jump. Spidy loads last when started from the
  launcher, so a loader mod sees the game's own code.
