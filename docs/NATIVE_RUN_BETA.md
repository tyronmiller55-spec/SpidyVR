# SpidyVR 0.8.0 + Native Run Beta (community build)

> **Unofficial community build.** This is [Ilya Mezerowsky's Spidy](https://github.com/IlyaMez/SpidyVR) 0.8.0 plus a set of opt-in movement and web features proposed upstream as PRs. It is **not endorsed by Ilya** unless he says so. **Report bugs on [this fork's Issues](https://github.com/tyronmiller55-spec/SpidyVR/issues), not to Ilya.**
>
> **Beta:** tested by one person, on a Quest 3S via Quest Link. Other headsets and runtimes are untested on this build.

**Download:** [v0.8.0-native-run-beta1 release](https://github.com/tyronmiller55-spec/SpidyVR/releases/tag/v0.8.0-native-run-beta1) → `Spidy-0.8.0-native-run-beta-win64.zip`

Every new feature is **OFF by default** (except SPIDY RUN JUMP, which only does anything while SPIDY RUN is carrying you). With nothing switched on, it plays like Ilya's 0.8.0, including 0.8.0's web tie, load-stutter fix and first-person cutscenes ([CHANGELOG](CHANGELOG.md)).

## What is NATIVE RUN?

On the ground, push the left stick fully (past **70%** by default) and Spidy holds the game's **RT** for you — the game's own parkour run. You get the game's sprint, vaults over cars and rails, and wall runs, as on a gamepad. Ease under 50% and you jog again.

It is never sent in the air, on a web, on a wall, or while a grip or trigger is held, so swings, webs, zips and aiming work as before. **NATIVE RUN AT** sets the threshold (50–90%, default 70%; it lets go 20 points below it). Turn on **STEADY STATES** to stop it flickering at kerbs/prompts and to keep it off in fights.

## Requirements

- Windows (Linux/Proton is experimental upstream; untested on this build — see [LINUX.md](LINUX.md)).
- Marvel's Spider-Man Remastered on Steam (PC).
- A PC VR headset and its OpenXR runtime: Meta Quest Link, Virtual Desktop or SteamVR. (This beta was only tested on Quest Link.)

## Install

1. Download `Spidy-0.8.0-native-run-beta-win64.zip` from the [release](https://github.com/tyronmiller55-spec/SpidyVR/releases/tag/v0.8.0-native-run-beta1). Optionally check its SHA-256 against the release's `SHA256SUMS.txt` (`Get-FileHash -Algorithm SHA256 .\Spidy-0.8.0-native-run-beta-win64.zip`).
2. Extract the whole zip to a **new** folder you can write to (e.g. `Documents\Spidy-beta`). Don't run it from inside the zip.
3. Run **`Spidy Launcher.exe`**. Windows SmartScreen may warn (unsigned): *More info → Run anyway*.
4. Fix anything the launcher marks red, then START VR with the game closed.

**Windows Defender / antivirus:** Spidy injects into the game process and hooks game functions, which antivirus heuristics often flag (false positive). `README-SECURITY.txt` inside the zip explains why, shows how to verify every file against `SHA256SUMS.txt`, and links the GitHub Actions run that built it. Only add an exclusion if all hashes match and you trust the source.

**Updates warning:** the launcher's update check looks at **Ilya's official releases**. Choosing *Update now* replaces this beta with the official build (which doesn't have these features). Choose *Later*, or turn the start-up check off under About > UPDATES, if you want to stay on the beta.

## Turning features on

In the headset: pause (menu button) → **Settings** → **SPIDY VR** (the last tab; Up from GAME gets there in one press). Left stick moves/changes, X resets a setting, Y resets the tab, B goes back. Changes apply at once and are saved for your next session. The launcher's Options tab shows them too.

| Setting | Default | What it does |
|---|---|---|
| NATIVE RUN | OFF | Full stick on the ground holds the game's RT: its sprint, vaults, parkour, wall runs. Never in air / webs / walls / with grip or trigger held. |
| NATIVE RUN AT | 70% | Stick tilt for NATIVE RUN, 50–90%; releases 20 points lower. |
| STEADY STATES | OFF | NATIVE RUN's RT waits until nothing has stopped it for a moment (no flicker at kerbs, prompts, vents) and stays off in a fight. |
| RUN CURVE | OFF | On the ground the stick reaches full run at 80% tilt, with more speed early. |
| RUNNING LEAP | OFF | A charged leap (hold A) keeps your run speed and adds its charge — a long, flat jump from a fast run. |
| EARLY JUMP | OFF | QUICK TAP: a tap of A jumps if released within 0.1 s. RUNNING: also, while running, A jumps the instant you press it (no charged leap while running). |
| SOFT LANDING | OFF | Falling with no web, your fall eases to ~9 m/s over the last 3 m; horizontal speed kept. |
| SPIDY RUN | OFF | Land faster than a sprint (let go of a swing near the street) and keep running at that speed, steering freely; eases to a sprint and the game's run takes over. |
| RUN FROM STANDING | OFF (needs SPIDY RUN) | Hold the stick fully for a moment on the ground to start SPIDY RUN from a stand, building past a sprint. |
| SPIDY RUN JUMP | **ON** | Only acts inside SPIDY RUN / smooth web landing runs: A jumps at once, keeping your speed. |
| SMOOTH WEB LANDING | OFF | Landing while holding a web (slower than WEB LANDING IMPACT, default 24 m/s) eases your fall and carries your speed across the ground. |
| AIR SLINGSHOT | OFF | In the air with both webs taut: hold both grips, draw both arms back until the hands buzz, release both to launch along the webs. |
| WEB ANGLE | 0 | Tilts the web line up/down from the controller's, 15–45°. Aim markers follow. |
| WRIST SHOOTERS | OFF | Webs leave parallel to your forearm, out past the wrist, like Spider-Man's shooters. |
| PALM SHOOTERS | OFF | Webs leave square out of the palm side (the pre-v6 WRIST SHOOTERS). |
| WEB AIM | CONTROLLER | MODEL FOREARM: webs follow the forearm of the body you see (falls back to the controller when it can't). FOREARM BLEND (100%) and FOREARM AIM LIMIT (NO LIMIT) tune it. |
| DODGE TUMBLE | OFF | When the game dodges your view tips (LEAN) or rolls (ROLL) so you can tell. Pick LEAN if ROLL is too much. |
| COMBAT RESPONSIVE | OFF | In fights: no SPIDY RUN/landing carry, NATIVE RUN holds no RT, and A is the game's jump at once. |
| HEAD PROBE | OFF | Read-only diagnostic: logs Spider-Man's head bone vs your headset per state (`head probe:` lines in the diag log). Changes nothing in play. Needed before animated-moves / head-lock work. |
| ANIMATED MOVES | OFF | Game anim drives the body for ground parkour / vaults / mantles (with NATIVE RUN). Swing / web / zip / wall-run stay with the mod. Opt-in play-feel; needs Ilya's yes to merge upstream. |
| ANIMATED HEAD | OFF | While a pose yield is active: view follows the gameplay head. LEVEL = place only (comfort); FULL = turns with anim on ground. Air yields stay LEVEL. Off by default. Yield exit resyncs VR body/tracking (handoff). |
| ANIMATED AIR | OFF | Game anim drives the body in freefall / swing / web air; mod keeps swing physics. Zip/wall/combat stay VR body. Freefall web aim interrupts. Off by default. |
| ANIMATED STEERING | OFF | While yielding: L-stick magnitude runs along Spidey's facing (actor read). R-stick model yaw not implemented (no facing write). Off by default. |
| RUN LOGS | OFF | Writes run/landing/take-off speeds to the diag log for bug reports. Changes nothing in play. |

Full descriptions: [PLAYERS.md](PLAYERS.md) ("VR settings").

### Recommended starter settings

A suggestion, not a tested optimum: **NATIVE RUN ON** (NATIVE RUN AT 70%), **STEADY STATES ON**, **COMBAT RESPONSIVE ON**, **EARLY JUMP: RUNNING**. Then try **SPIDY RUN** + **SMOOTH WEB LANDING** for swing-to-street momentum, and **RUN LOGS ON** while you're testing so your reports are useful. For the head-lock research pass, turn **HEAD PROBE ON**, **ANIMATED MOVES ON**, and try **ANIMATED HEAD: LEVEL** then **FULL** with NATIVE RUN for vaults/mantles/ground parkour; send `reports\game-vr-<time>-diag.log` (look for `animated moves: head follow level|full`).

## Troubleshooting

- **Headset freezes / goes black and nothing answers:** usually the game itself crashed or hung. Check the launcher's session log; the diag log says *"The game crashed (exit code …)"* if it did. For a true hang, Spidy saves a `game-vr-…-freeze.dmp` beside the report; close the game from Task Manager.
- **What to send:** `reports\game-vr-<time>-diag.log` (in the Spidy folder; starts with the build and your settings), plus the game's newest log from `Documents\Marvel's Spider-Man Remastered` if it crashed, and any freeze `.dmp`.
- **Files missing / launcher deleted by antivirus:** verify hashes per `README-SECURITY.txt`, allow the folder, extract again.
- **Picture breaks up:** start the game *from the launcher* with the game closed.
- More in [PLAYERS.md](PLAYERS.md) (troubleshooting section).

## Reporting bugs

Open an issue at <https://github.com/tyronmiller55-spec/SpidyVR/issues> with: what you did, which settings were on, your headset + runtime, the **diag log**, and the launcher console text around the problem. Please **don't** report beta-build bugs to Ilya.

## Upstream status

These features are proposed to Ilya as **open pull requests** (not merged): the combined [#77](https://github.com/IlyaMez/SpidyVR/pull/77) and the 18 stacked PRs [#59](https://github.com/IlyaMez/SpidyVR/pull/59)–[#76](https://github.com/IlyaMez/SpidyVR/pull/76) ([all PRs](https://github.com/IlyaMez/SpidyVR/pulls)).

## Credits & license

Spidy is created by **Ilya Mezerowsky** — <https://github.com/IlyaMez/SpidyVR> (support him: <https://ko-fi.com/ilyamezerowsky>). This community build is a modified version distributed under the **GPL-3.0** ([LICENSE](../LICENSE)); source for exactly this build is the `community-build` branch of this fork.
