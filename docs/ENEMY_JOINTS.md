# Enemy joints (R1)

Off by default, EXPERIMENTAL. `enemy_joints=1` in `spidy_vr.ini` (or **Enemy joints (experimental)** on the launcher's Legs page, with **Enemy limb zones (experimental)** under it) makes the kicks (air kicks K2, ground kicks K3) read an enemy's own joints for the zone they aim at and the zone they hit. This is the plan's R1: **the reader only**. It writes nothing to the game and moves nothing; the overlay of an enemy's pose (R2) is not built and waits on the probes.

## What it does

With `enemy_joints` on, when a kick is asked for (and while it runs) the swing module reads the joints of the nearest six enemies within 3.5 m of the feet, by the chain the C0 findings verified piece by piece (no new offset):

`record` (a kick target's id: a pool entry) -> `instance = [record]` -> `info = [instance + 0x88]` -> `rig = [info + 0x200]` (joint count u16 at +2, the table at the pointer at +8, 16 bytes a joint: parent s16, name hash at +8) -> `getJoint(instance, out, hash)` (image + `0x1775a80`, the function `game_fp::headWorld` already calls) -> the joint's position times the instance's world matrix.

The hash is the game's name hash (CRC-32 steps seeded with `0xedb88320`, no final inversion; `pelvis` is `0x8e59b33f`, `head` `0x37519912`). The roles read, by the hero's names: `chest`, `head`; with the limb zones `LF_loarm`, `LF_wrist`, `RT_loarm`, `RT_wrist` (the forearms) and `LF_loleg`, `LF_foot`, `RT_loleg`, `RT_foot` (the shins). A rig is looked through once for which of those names it has (cached five seconds), so a name it lacks is never asked for.

Then the zones:

- **Aiming.** The zone point a kick bends its foot toward is the head joint, the chest joint (and with the limb zones the nearest point on the shin or forearm nearest the gaze) instead of the capsule's 90 % / 65 % / 20 % heights.
- **The blow's zone.** Where the foot met him: the nearest of the head joint, the chest joint and (limb zones) the forearms' and shins' segments. Without the limb zones the legs are the capsule's band under 35 % of his height (a shin joint is not read), and there is no arms zone.
- **The fallback is the capsule bands**, per enemy and per hit: any enemy whose head or chest name is missing, whose pointer chain fails or whose joint is not finite gets the bands, exactly as with the setting off. The log says which decided (`zone=head (joints)` or `(bands)`).

## The gate: limb zones off until the probe is run

Only the **head** joint has been read on other characters' skeletons (`game_fp`'s first-person cutscenes; the Nov 12 and 13 logs). Whether a thug's rig uses `chest`, `LF_loarm`, `LF_wrist`, `LF_loleg`, `LF_foot` and their right-hand twins as the hero's does is untested, as is the bot's `record -> instance` link on a bot. So:

1. `enemy_joints` on gives the head and chest zones from joints, with the bands behind them. If the chest name is missing the bands decide everything for that enemy.
2. `enemy_limb_zones` (needs `enemy_joints`; also off) adds the legs from the shins and the arms zone from the forearms. **Leave it off until `probe_c0_skeleton.py` has been run** (from the C0 findings; read only, from the repo root, with Spider-Man in free roam and a few thugs near): `reports/c0-skeleton.json` should say `READABLE` with the roles `head`, `chest`, `LF_wrist` and `LF_foot` resolved.

The game log answers the same question without the probe, once a kick has been tried with the setting on: `enemy joints: a rig of N joints names [...] and lacks [...]; read 0x..; (reads, no head+chest, last why)` is printed whenever the roles found change.

## Where it lives

| Piece | File |
| --- | --- |
| Name hash, roles, joint-table parse, world position, the zone geometry | `include/spidy/enemy_joints.hpp` |
| The memory reads and the `getJoint` call (guarded, read only) | `src/game_joints.cpp`, `include/spidy/game_joints.hpp` (in the swing module's DLL) |
| The kick's use: `Kick::Target::joints`, `zonePoint`, `zoneOfHit`, `Config::limbZones`, `Zone::arms` | `include/spidy/air_kick.hpp` |
| Ask flags bits 3 and 4; the reads before a kick; the counters | `game_swing.cpp` (`AirKickAsk` flags; `AirKickLive` version 3 adds the counters and the roles seen; `game_swing::Settings` is untouched) |
| Settings, log | `enemy_joints`, `enemy_limb_zones` in `vr_settings`; `game_xr.cpp` |
| Tests | `tests/enemy_joints_tests.cpp`, a settings check in `tests/core_tests.cpp`, the launcher exemption in `tests/launcher_tests.cpp` |

No new game address: `0x1775a80` (`getJoint`) and the entity layout are the ones `game_fp.cpp` uses.

## Log lines

`enemy joints: a rig of <n> joints names [<roles>] and lacks [<roles>]; read 0x<mask>; (reads <n>, no head+chest <n>, last why: <reason>)` (on a change); the kick's hit line says `zone=<z> (joints|bands)` and counts `zones by joints`.

## Not built

- **R2**, the overlay of an enemy's pose (it needs the pose array's timing relative to the hook: probe 2 of the C0 findings) and **Showtime**.
- **Zones for web strikes and punches**: the reader serves the kicks only; `game_punch`'s strike lock still picks by capsule.
- **Joints for enemies beyond six or 3.5 m**, and a continuous reader: it reads when a kick is asked and while it runs, not every step.
- A zone from the **pelvis, neck or upper arm** joints, and enemy classes with another naming.

## Unverified (needs the game)

- That a thug's rig has the hero's names (the probe answers it); that the instance's matrix and the joint are in the same space as `game_targets`' positions (they are for the head, in `game_fp`).
- That `getJoint` on a bot returns its current animated pose, not the last one: it reads whatever the game holds when the swing's step runs.
- That reading from the swing's thread is safe against the animation job (game_fp's worker already calls it from another thread).
