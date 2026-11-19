# 0.8.7-na-beta1: headset checklist and zip checklist

The testers' **CURRENT** Native Animation build (branch `feat/087-beta1`, from 966a812), after the owner's
Nov 19 session. What changed: [CHANGELOG.md](CHANGELOG.md). Each line names the log line that scores it
(`game-vr-*-diag.log`). The **NA · HELP** tab's TRY THE BETA pages ask testers to try the current features
and to post the newest reports-folder files on Discord.

## Headset checklist

1. **Menus.** The SPIDY VR menu shows the **NA ·** tabs with the "·" drawn (not a box); every NA setting is in
   one of them; **NA · HELP** scrolls through all its rows (about 51, including TRY THE BETA) and each READ row opens its page; Y (RESET
   ALL) leaves launcher-only settings alone.
2. **Jump sounds.** Point jump, ground flip and handstand leap: one grunt, never two (`jump sounds:`). Wall
   jump with WALL JUMP SOUND on SOUND A, then SOUND B, then OFF: say which fits.
3. **Hits.** A jab sounds lighter than a full swing; a flying kick or finisher the loudest.
4. **Air kicks.** In a fight with the enemy behind or beside you, a kick finds the one your punches are locked
   on (`kick: ... aim=punch lock`); a burst of flicks logs one refusal and a count.
5. **Landings.** Jumps and falls land upright (`landing upright: N% at touchdown (reason)`; the 0% share should
   fall from 51% to under 10%); an inverted fall with the arms down lands upright (`landing: inverted N deg ->
   upright`), with both arms up it still catches a handstand.
6. **Handoffs.** A takeover never hands back inside 120 ms unless it was a hard landing (`hybrid: minimum stint
   held the landing`, `rising touchdown ignored`).
7. **Ground flips.** All three front flips spin, also off a floor crawl and in a fight; a dropped one logs why.
8. **Handstands.** A handstand catch still works; a tracking glitch logs `handstand: absurd arm readings
   ignored` and does not open a catch.
9. **Webs.** With a steady arm the web goes where the controller points (`spherical aim: ... wrist offset N
   deg`); reaching behind the back or overhead the web takes the controller's line (`reach-around`).
10. **Showtime arms (Showtime on, NATURAL ARMS and GAME POSE).** From outside the arms are Spidey's own length
    and the hands normal size (`showtime: arms factor x->1.00`); the elbows do not flip (`showtime arms: ...
    bend flips L/R` should stay low); back in the headset your reach is as before.
11. **Hand grab class rules** (`hand_grab=1`): thug as before; a brute resists throws (`heavy resisted`) unless
    stunned or webbed up (`open`); a shield guard tires the grip about 3x; `hand_grab_class_rules=0` restores
    G2 (`class rules off`); the ground grip fling still works without Y+X.
12. **Memory.** The console's last lines include `commit memory: ...` (flat is good).

## Capture session (batch 3, the owner's)

Punch and kick hit sound ids; whoosh and effort ids (`attack_sound` in spidy_vr.ini, signed decimal, then the
trace); wall jump SOUND A against B; the "·" glyph and the long HELP tab on screen; a handstand catch; a kick on
an enemy; the finisher signal; a NATURAL ARMS Showtime log.

## Zip checklist

- Built by Python (`shutil.make_archive`) from the CI artifact of the commit, never the zip CLI.
- Name `Spidy-0.8.7-na-beta1-win64-<sha7>.zip`; top folder `Spidy-0.8.7-player-torso-<sha7>/`.
- Contents as CI built them: the launcher, the modules, `LICENSE`, `THIRD_PARTY_NOTICES.md`, the docs
  (`MOD_COMPAT.md`, `NA_HELP.md`); no PDBs.
- SHA-256 of the zip in the report.
- The launcher header reads 0.8.7-na-beta1; the diag log's build line names the commit.
- Defaults: sound trace logging off, LOG LEVEL 1, HEAD PROBE off, X and Y instant, `leg_hold_everywhere=0`;
  Showtime, air kicks, hand grab, web tethers, wall brake, grip fling, point jump and the mid-air landing
  handoff on (revision 14; a saved off stays off); `swing_kick_manual=1`, `swing_kick_auto=1`, `ground_kicks=1`;
  a clean profile starts at movement defaults rev 14 and shows the calibration prompt. Wall jump sound is
  unchanged (SOUND A).
