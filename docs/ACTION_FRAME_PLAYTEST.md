# Native Animation action-frame playtest — 0.8.5

New gameplay-repair test build, not the older diagnostic-only package. The full source commit is in ACTION_FRAME_BUILD.txt and the session build log. No headset outcome is claimed.

## Install without losing settings

1. Stop VR and close the game and old launcher. Extract this WHOLE ZIP into a NEW writable folder outside the game installation. Do not merge files or mix DLLs/tools with previous builds: the host/rays Command v7 pair must match.
2. Keep the previous78eabb8 compatibility and b3a4073 diagnostic builds for comparison. Back up `%APPDATA%\Spidy` (launcher.ini, spidy_vr.ini and profiles) before the playtest. These settings are shared between launcher folders; extraction does not replace them, but changing settings in the launcher does. Export your current profile if convenient.
3. Start `Spidy Launcher.exe`. Keep your saved profile and verify Native Animation MOVES + HYBRID and ARMS are enabled; no profile migration or automatic setting reset is introduced. Keep RUN LOGS and HEAD PROBE on for this playtest. No valid overlay means native free-hand effects are withheld; authored-hand fallback with ARMS off is not implemented.

## Intended behavior to test

- On normal ground, standing/walking/running uses native animation with SpidyVR arm actions.
- Existing grip/web-hold cancel returns to original SpidyVR WITHOUT firing a web. Hold to keep that override; still-held controls do not synthesize new shots. On eligible grounded release, native ownership returns that SAME update, no run rearm or cooldown. Existing visual eye/IK continuity blend and pose-job availability are separate from owner timing.
- Original SpidyVR is available in air and on Spidy wallrunning; this does not force every native airborne animation into OG. Original airborne web/rotation/jump/zip intents remain.
- Physical swung hands can make swept melee contacts in animation; no automatic teleport/lunge/remote strike. Passive authored animation alone must not damage.

## Short acceptance sequence

1. At a safe known target, compare left then right web balls in animation and original airborne SpidyVR; use the same equipped gadget/target and note aim, muzzle, visible flight, impact and actual effect separately. Repeat while turning/translating. A fired engine call or nonzero object alone is not a successful hit.
2. Ground idle, walk and run: grip press must cancel without either hand firing; hold, then release must restore native that update. Repeat both hands and simultaneous grips/triggers. Release/repress in airborne OG must still shoot normally. Confirm momentum, pose and rotation do not snap/reset.
3. Stand in range of a target and physically swing each hand to land damage while native owns. Stand still through passive authored movement: no damage. Confirm no auto teleport/lunge. Repeat after grip handoffs and a fast actor turn.
4. Check original air swing and Spidy wallrunning, native jump/zip, refused zip, touchdown and game crawl. Ground arm webs/combat must not create an artificial native-return delay; a genuinely dispatched takeoff may finish first.
5. Send the newest session logs with approximate test times and which hand/owner was active. If it crashes, include the newest .mdmp if one exists. Never infer hit rate from aggregate fired counters.

## Known validation limits

Corrected missing world translation and added same-job hand/forearm snapshots; snapshots still come from completed pose jobs. Fast root rotation between solve and render can still cause angular drift and needs headset evidence. There may be a brief arm-action availability gap until a valid new-owner solve arrives; ownership itself is not delayed. Already-admitted engine projectiles cannot be retroactively canceled. See WEB_BALL_DIAGNOSTICS.txt for counter caveats.
