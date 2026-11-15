# Web fire from animation (Nov 14, t70u)

> "Sometimes nothing happens, but I can see a switch to SpideyVR happening. We have to get the webs working from animation properly, it's getting hard to fire accurate web lines in combat."

## What happened (game-vr-20261114-104706)

The web press is the grip (the trigger with WEB BUTTON swapped). In a midair animation the same press is the AIR CANCEL ([OWNERSHIP.md](OWNERSHIP.md)): `animation_cancel::Gate` flipped the owner to SpidyVR (`breakForCancel`, the `grip cancel: anim->vr this frame` line) and CONSUMED the press: that hand's web stayed blocked until BOTH hands were let go ("so a cancel is never a web"), and so was the other hand's press during the wait. `swing.cpp` shoots a web on the rising edge of the grip it is given (`shoot = held && !s.held`); with the grip zeroed it never saw one. The player saw the switch and no web. Holding the squeeze, the normal way to shoot, did nothing until he let go and pressed again.

Of the 55 cancels in the session, many were an `anim->vr (grip-cancel)` followed within a frame by `vr->anim (land 0 ms)`: the press was consumed on the ground, while the swing's sample still said the body was not grounded (inferred from the sequence, not read from a field). Nothing at all followed those squeezes.

The 143 of 615 web balls that were dropped were all `rejected` (`wb dispatch: q615 full0 exp0 call472 rej143`): revoked by `invalidatePending` (a consumed cancel token revokes the shooter's batches, `game_swing.cpp`) or refused on the game thread; the log did not say which. That is the other half of "nothing happens" for the trigger.

## The fix

- `web_fire::Queue`: a press the gate blocked is queued, not dropped. The owner flips on that frame as before; the swing gets the press as a fresh rising edge on the next frame (so the owner flip's re-origin has happened and the line is taken from the settled frame), in either owner. A tap is held down 200 ms so it shoots; a squeeze held on keeps the web as long as it is held. Controls lost (a scene, a menu, flat screen) drops the press, and says so.
- The other hand's press during the cancel's wait is queued the same way.
- The line is the controller's current line in the headset-first heading: from the mapped hand of this frame in animation (`kin.aim`, `armView`), from the rig's hand in SpidyVR. The marker's low-pass (`aim_filter`, 2.5 Hz floor) may steady the line but never more than 2.5 degrees off the controller's own line at the squeeze (`web_fire::limitLag`). The cancel and the queue never turn the view: the headset stays the only view-heading authority, and animation turns nothing.
- `web fire:` on every press: `L|R hand, pressed in ANIM|VR (now ...), web attached | no web: missed|dropped|unanswered (reason), aim X deg off the controller's current line (rig line Y, from <source>), queued N ms, #n (attached a, missed m, ...)`, and `web fire: attached D m out`.
- `wb drops:` once a second under `web balls:`: revoked-queued (and the number of revocations), revoked-at-dispatch, refused-at-enqueue, queue-full, expired, and the game's refusals (no-line, gadget, weapon-gone, not-ready, event, call). It reads `SpidyShooterDrops`, a live export of the ray module (magic/version/bytes checked; an older module has none and the line is absent). `game_shooter::Data` stays v5, 432 bytes; `game_swing::Settings` stays v40.

## Not changed (flag if wrong)

- A web press that finds no surface along the aim is a miss, as before: `no web: missed (no surface within reach along the aim)`. That is a different "nothing happens"; the log now says so on that press.
- The trigger's web balls are still revoked by a consumed cancel (a ball is not a web; the cancel's gesture is the grip).

## Not verified

Headset behaviour. The tests are the pure ones (`tests/web_fire_tests.cpp`: the gate and the queue as XR runs them, a squeeze held, a tap, the other hand, an owner that flips back, a lost control, the tracker's outcomes, the lag limit). `docs/VALIDATION.md` has the CI.
