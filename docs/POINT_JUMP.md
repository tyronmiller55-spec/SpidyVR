# POINT JUMP (milestone 1, experimental, off by default)

The user, Nov 11: whenever the player touches a surface (ground, a perch, a wall), he can charge a jump that launches him **the way he is looking**. A tap is a quick jump; holding up to a second gives a bigger leap. It reuses the charged leap's timing and rumble and the swing's takeoff path (the game's own jump to unstick, then the launch velocity, as a charged leap or a gesture zip take off). It never triggers the game's point launch, so there is no smoke. Plan: EngineBot's "SpidyVR - Handstand & Point Jump Plan". Code: `include/spidy/point_jump.hpp`, `src/game_xr.cpp` (`point jump:` diag lines), `Swing::pointJump` and `EventKind::PointJump`, `SpidySwingPointJump`, `hybrid_owner` (`EnterVr::pointJump`). Not headset-tested.

## Trigger

| `point_jump_trigger` | How | B alone | A alone |
|---|---|---|---|
| 0 (default) A+B TOGETHER | both within `point_jump_chord_ms` (50) of each other | the dodge, at most 50 ms late; at once while a spider-sense warning stands | the game's A, at most 50 ms late; a tap inside the window still reaches the game |
| 1 HOLD B, THEN A | hold B, press A | let go with no A: the dodge on the release; held `point_jump_dodge_hold_ms` (250) with no A: the dodge then; a warning: at once | never held back |

Once both are down neither reaches the game until both are let go of; letting go of either fires. A grip (squeeze over 0.6) cancels the charge. The chord can start only while point jump is allowed (below); otherwise A and B pass at once, exactly as before. `ChargeJump::consume()` ends whatever A was doing when the chord closes, so a chord never becomes a charged leap or a late A.

## Charge, direction, velocity

- Tap (held up to `point_jump_tap_ms`, 150): `point_jump_min_speed` (9 m/s). Then `u = (t - 0.15) / (full - 0.15)`, `c = 1 - (1 - u)^2` (`point_jump_curve` 1: `c = u`), speed `= min + (max - min) c`, up to `point_jump_max_speed` (22 m/s) at `point_jump_full_ms` (1000).
- Direction: `d = normalize(look + bias n)` with `bias = point_jump_normal_bias` (25%) and `n` the surface's normal (up on the ground; Spidy's wall normal on a wall). If `d` is less than `point_jump_min_out_degrees` (10) out of the surface it is turned out to exactly that angle, keeping its heading. Straight into the surface is straight out of it.
- Velocity: `v = d speed + keep (v_old - d (d . v_old))`, `keep = point_jump_keep` (25%), with the kept part never into the surface. The look is the actions' head (the same `look` the swing command carries).

## When it is allowed

- Contact now (the ground or a perch as the swing sees it, or Spidy's own wall), or within `point_jump_coyote_ms` (120) of leaving it.
- A release with no contact waits `point_jump_buffer_ms` (150) for one, then is dropped (`denied buffer expired`).
- `point_jump_cooldown_ms` (250) after a launch, and a fresh contact (he has left the surface and touched down again, or stayed on it 0.5 s).
- Not during: out of play, the phone or gadget wheel, the pad layer, the calibration, a prompt naming A or B, a vent, a web line, the game's own crawl (a later milestone), a takeoff under way; in a fight only with `point_jump_in_combat` (1).
- **Zip guard (EngineBot's open Q1):** the takeoff presses the game's jump to unstick. Right after the game's zip lands on a point, that press could be read by the game as its own point launch (with smoke). From the moment a zip is asked for until he has left the surface and landed from it (2 s at most), and `point_jump_zip_guard_ms` (400) after that landing, a chord may charge but its release waits for the guard to pass (`waited for the surface or the zip guard` in the launch line). The 400 ms default is a guess to verify in the headset: if smoke ever shows after a zip, raise it.

## Ownership

ANIM on the ground → VR on the frame the launch is sent (`point-jump`, the approved ground exception), feet locked by the position lock; VR held through the takeoff until he leaves the surface (400 ms at most); the air is VR's; landing on the feet snaps back to ANIM. See [OWNERSHIP.md](OWNERSHIP.md).

## Settings

In game: **VR POINT JUMP** tab. Launcher: Options > **Point Jump**. The player's own (not in shared profiles).

| Row / key | Default | Choices |
|---|---|---|
| POINT JUMP (EXPERIMENTAL) `point_jump` | OFF | OFF, ON |
| POINT JUMP TRIGGER `point_jump_trigger` | A+B TOGETHER (0) | A+B TOGETHER, HOLD B, THEN A |
| TAP SPEED `point_jump_min_speed` | 9 m/s | 6, 7, 8, 9, 10, 12, 14 |
| MAX SPEED `point_jump_max_speed` | 22 m/s | 14, 16, 18, 20, 22, 26, 30 |
| CHARGE TIME `point_jump_full_ms` | 1 s | 0.5, 0.75, 1, 1.25, 1.5 s |
| SURFACE PUSH `point_jump_normal_bias` | 25% | 0, 10, 25, 40, 60% |
| ini only | | `point_jump_chord_ms` 50, `point_jump_tap_ms` 150, `point_jump_curve` 0, `point_jump_min_out_degrees` 10, `point_jump_keep` 25, `point_jump_coyote_ms` 120, `point_jump_buffer_ms` 150, `point_jump_cooldown_ms` 250, `point_jump_zip_guard_ms` 400, `point_jump_dodge_hold_ms` 250, `point_jump_in_combat` 1 |

Not done in milestone 1: launching from the game's own wall crawl, handstands, `point_jump_charge_dip` (a camera dip while charging).

## Diagnostics (`point jump:`)

- `point jump: chord A-first 18 ms (trigger 0, ground)`
- `point jump: charge 0.62 -> 17.9 m/s dir (...) n (...) out-fix 0 deg, held 640 ms, launch (...), from the ground`
- `point jump: off the surface 22 ms after the launch` / `still on the surface 400 ms after the launch`
- `point jump: denied cooldown|no contact|no fresh contact since the last|blocked|prompt|pad layer|vent|line|game crawl|takeoff|combat|zip guard|buffer expired`
- `point jump: charge cancelled (grip) after N ms`, `point jump: the swing refused the launch (N)`
- the owner's `hybrid: anim->vr (point-jump)` and `position lock:` lines.
