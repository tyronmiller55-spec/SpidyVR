# QUICK SIDESTEP (November 12)

The user (Nov 12): "something quicker and upright with no flipping, for fast
lateral movement. The standard dodge is too long" — approved: "Yes on
sidestep". On by default; `sidestep=0` in spidy_vr.ini turns it off.

## What it does

On the ground, under the animation (hybrid, ANIM owns him): **flick the left
stick, then press A within 120 ms**. Spidey bursts about **2.0 m in 0.18 s**
the way the stick was flicked (relative to the view), upright: no flip, no
roll, no camera rotation. The last 0.04 s is a **hard stop** (zero speed
across). Then the ground is the animation's again.

- Cooldown 0.30 s after a burst ends. Three in a row (starts within 0.68 s
  of each other), then a 0.8 s lockout.
- I-frames (`sidestep_iframes=1`): the burst counts as a web's carry for
  the game's own dodge hook (`game_dodge::carry`, WebDodge): a blow the
  game **warns of** that lands within 0.3 s during the 0.14 s move starts the
  game's own kind-5 dodge (its immunity), exactly as on a web. **No new game
  address.** Unwarned hits are not dodged.
- B stays the game's dodge.

## Input rules (sidestep.hpp `Trigger`)

| Input | Result |
|---|---|
| A alone, no flick in the last 120 ms | the game's jump, **at once** (nothing holds A) |
| Stick held, then A (running jump) | jump: the flick was over 120 ms ago |
| Flick forward (within ±35° of ahead), then A | jump (a run started and jumped stays a jump); `sidestep_forward=1` allows it |
| A, then a flick | jump (the flick must come first) |
| B held (A+B point jump chord, hold-B-then-A, the dodge) | no sidestep; A and B as before |
| Point jump chord charging | no sidestep |
| Y+X leg hold engaged | no sidestep |
| In the air, on a wall, crawling, in a vent, on a web line, webs held, a takeoff or a point-jump takeoff under way, a prompt naming A, phone, pad layer, calibration | no sidestep; A as before |
| JUMP TO SPIDYVR (0.5 s handoff) | unaffected: it times midair animations; a sidestep never leaves the ground |

The A that fires a sidestep is held back until let go of. A new A press
during the burst reaches the game when the burst ends (a buffered jump).
The left stick reads zero to the game while the burst lasts.

**Dead zone:** a flick must start under 0.30 (and never under 0.20, above the
0.15 steering dead zone), so stick noise is never a flick.

## Ownership

`EnterVr::sidestep` — an approved ground exception (docs/OWNERSHIP.md). The
fire switches to VR with the position lock; `Frame::sidestepHold` keeps VR
on the ground while the burst lasts; then the ground returns to ANIM
(`land`) with the position lock. Off an edge mid-burst: VR's air rules.

## Swing side

`SpidySwingSidestep` (SidestepAsk, magic 0x53575353, 40 bytes) is taken on
the ground only. The swing drives it in real time as a web strike's flight
(lease velocity), and ends it early at an **edge** (no floor 0.35 m ahead
within 0.5 m down), a **wall** (a body ray 1 m up, 0.75 m along the way),
off the ground, or when a yield, grasp, strike, line or web takes him.
`SpidySidestepLive` reports each end to XR's log.

## Settings (spidy_vr.ini only; no menu row yet)

| key | default | range |
|---|---|---|
| sidestep | 1 | 0/1 |
| sidestep_distance_cm | 200 | 100–300 |
| sidestep_ms | 180 | 120–300 |
| sidestep_cooldown_ms | 300 | 0–2000 |
| sidestep_chain | 3 | 1–10 |
| sidestep_lockout_ms | 800 | 0–3000 |
| sidestep_iframes | 1 | 0/1 |
| sidestep_flick_from | 30 | 20–60 (%) |
| sidestep_flick_to | 80 | 50–95 (%) |
| sidestep_flick_ms | 80 | 30–200 |
| sidestep_a_ms | 120 | 40–250 |
| sidestep_forward | 0 | 0/1 |

## Logs

`sidestep: fire ...`, `sidestep: denied <why> (A went on to the game)`,
`sidestep: the swing refused the burst (N)`, `sidestep: end <time|edge|wall|left
the ground|refused|cancelled> after N ms, X m across`, `sidestep: hold over,
ground back to the animation`, plus the owner's `sidestep` switch.

## TODO

- **Sound / grunt:** marked `TODO(sidestep sound)` in src/game_xr.cpp. No sound
  IDs added; they wait for the SOUND TRACE headset session (1008510). No smoke.
- A menu row (kept ini-only to stay clear of feat/passive-legs' rows).

Not headset-tested.
