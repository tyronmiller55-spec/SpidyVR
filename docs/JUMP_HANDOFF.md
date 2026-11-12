# JUMP TO SPIDYVR and AIR VIEW EASE (build 2 of the Nov 12 plan)

Source: the user, Nov 12 05:41 PST. Code: `include/spidy/hybrid_owner.hpp` (timer, kinds), `include/spidy/view_ease.hpp`, `src/game_xr.cpp` (wiring, logs, leg chord). Tests: `tests/jump_handoff_tests.cpp`.

## JUMP TO SPIDYVR (`jump_handoff_ms`, NATIVE ANIMATION screen and launcher)
STAY TO LANDING / 300 / 400 / 500 (default) / 700 / 1000 ms.

- Kinds (from the frame before takeoff): `wall launch` (off the game's wall), `vault` (NATIVE RUN held), `jump` (a jump press within 400 ms, or he rose faster than 1 m/s), `fall` (walked off), `zip launch` (timer from the launch's rise; the pull itself never hands over).
- The timer waits while he fights, dodges (combat clear-out 300 ms) or zips.
- Landing before the timer: no switch; each takeoff starts its own timer.
- Unchanged: flip, grip cancel, web, Spidy's wall break on the same frame; landing on the feet returns to ANIM; right-stick turning never breaks; point jump and swing releases are instant.
- STAY TO LANDING = the Nov 11 23:39 rule.
- Log: `jump handoff: <kind> after X ms (timer N ms)`.

## AIR VIEW EASE (`air_view_ease_ms`, VR CAMERA screen and launcher)
OFF (SNAP) / 150 / 200 / 250 (default) / 300 / 400 ms. The punch hold-and-ease (same code path) now also runs on the jump timer, flip break and grip cancel (flip breaks: median 0.50 m view jump in the 03:49 session). The rig placement is offset, so eyes, hands, webs and body move together. Cancels on landing (`cancelled (landed)`), teleport, big turns, recenter, a new handoff. Logs: `jump handoff view ease: start offset X m, dur N ms`, `flip-break view ease: ...`, `grip-cancel view ease: ...`, and `<why> view ease: <end> after N ms`.

## Legs
Y+X can engage the legs during a midair animation (before: zip/gadget menu suppressed only), so a hold made before the timer carries over. The air legs and momentum trail blend in over 0.2 s (no pose pop); the passive state follows the hold rules unchanged.
