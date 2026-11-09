# Player upper torso — first implementation playtest,0.8.5

Read ACTION_FRAME_BUILD.txt for the exact source. This is a NEW separately compiled unmerged build, not a claim of headset-perfect rigging. Includes delivered79dee84 indicator/cancel changes and8d6b052 action-frame/contact-punch changes. Latest reattached gameplay logs stamp OLD78eabb8024, so they do not accept those newer builds.

## Actual scope

- ANIMATED TORSO now drives a player-controlled, articulated spine/neck/head rooted at the native waist, then clavicle/arm IK. Native actor/pelvis/lower body, locomotion/engine momentum/collisions remain unchanged. The old reach assist rotated spine subtrees about the head and could detach the waist; this path pivots at the real joints and preserves bone lengths.
- In full player-rig mode, fresh tracked arms solve at full weight even idle. The prior .55 floor blended45% passive authored arm pose into stationary controllers. Lost tracking retains only the existing short visual fade, never action/damage permission. ARMS-only/TORSO-off retains its old weighting.
- Gameplay camera uses an original unmodified authored-head anchor, keyed by actor/command lease and composed under the current actor transform. It must not follow its own HMD-driven rendered head a second time. The same pose job caches its calibrated eye axes/offsets too, so a later rig calibration is not mixed into an older anchor. Cutscene paths unchanged.
- Shots/aim marks/web-line emitters and wrist feedback use the actual calibrated solved hand, not just its requested controller target. World/head-off modes hide the self-head and duplicate overlay gloves too, rather than requiring head-follow for native presentation. Read-time current actor root rotation/translation are applied once to both actual hand and forearm snapshot. Raw controllers remain the next IK input (no feedback loop).
- Existing outward0/inward15-degree forearm clamp, no-shot grip cancel, exact-update eligible ground release, original air/wallrun and contact-only animation punch policy retained. Indicators remain presentation only: no teleport/lunge/automated punches.

## Preserved policies and uncertainty

Explicit ANIMATED AIR SPIN retains its established authored full-body/camera spin policy rather than forcing HMD torso articulation during that deliberate flip. Its player arm/action-frame solve still runs. Ordinary native airborne poses use the new torso. Existing180ms visual ownership handoff is unchanged. Existing waist yaw cap (~40deg), OG head lean/neck share remain; unreachable head/hand targets produce measured residuals, not root movement, bone stretching or an invented authored-hand fallback.

Synthetic actual-pose tests cannot establish full engine render alignment. Actor motion AFTER an action snapshot/submission is asynchronous; fast turns, camera transitions, unusual suit rigs and cloth/attachments need headset testing. Fixture max tracked-head residual about5cm is a measured limitation, NOT an accuracy claim. All required Windows/protocol/package checks are recorded with the delivered archive. Microsoft VC++ x64/OpenXR prerequisites unchanged; MSVCP140 is not bundled.

## Safe install

Close game, VR and every old launcher. Back up shared `%APPDATA%\Spidy` settings/profiles. Extract WHOLE ZIP into a NEW writable folder outside the game. Launch `Spidy Launcher.exe` INSIDE uniquely named `Spidy-0.8.5-player-torso-<commit>` folder, not an old shortcut. Never mix DLL/tools from earlier folders. Confirm next log's build stamp matches ACTION_FRAME_BUILD.txt. Existing settings/profiles are not reset.

Enable existing Native Animation MOVES, HYBRID, ARMS and TORSO; keep desired head mode/forearm clamp/settings. Enable AIM MARKERS, COMBAT VISUALS and RUN LOGS/HEAD PROBE for comparison.

## Acceptance

1. Standing/walking/running native: hold both hands still, move independently/across chest, extend/turn palms. Model wrists/forearms should remain player-controlled even idle, not drift toward animation. Turn/lean/tilt head; articulated look should not double camera translation or spin itself. Pelvis/lower locomotion must not be dragged by upper solve.
2. Compare Full, Level and head-off/world modes, both mirrored hands, translated locations, rapid actor/body turns. Follow actual wrist nozzle, web balls, web/zip line origins and aim/interaction/target indicators. No independent floating marker-only correction is intended.
3. Physically swing either hand into enemies: damage only on real contact. Passive running/animation/root movement with stationary hands must not damage. Brief tracking loss/reacquisition should not create a phantom sweep or stale shot.
4. Existing grip cancel from native, each hand and simultaneous: clean OG activation with NO line/ball on press/hold. Eligible idle/walk/run release returns native that update. Independent ordinary airborne OG release/repress still works. Check original wallrun and intended native airborne poses/explicit air-spin behavior.
5. Preserve momentum/rotation/collision, indicator visibility and no teleport/lunge. Return newest logs with exact hand/mode/approximate moment if alignment or cancellation fails. New `player torso:` diagnostics report actual-frame validity, head residual and pose-to-current-root drift; counters alone never prove a visual hit.
