# Player-controlled upper torso — implementation contract (first implementation playtest)

Based on delivered8d6b052 and feedback/cancel slice79dee84. The feedback ZIP deliberately does not claim this overhaul.

## Ownership / geometry boundary

- Native animation retains actor/root transform, pelvis, every lower-body joint, locomotion, collision, position and momentum. Grip/ground/air/wallrun ownership policies are not changed here. No engine movement command or automatic strike is introduced.
- Player HMD/controller targets are resolved in the SAME Full/Level/World native view frame as the current pose job, exactly once into model space. No chest-space remap of an already mapped grip.
- Upper torso must be articulated, not pasted from a separately solved OG full-body pose: that would disconnect its waist from the native animated pelvis. Keep the native waist attachment, fit the spine/neck chain with actual segment lengths toward tracked head, use OG-style player yaw/lean for chest and mirrored elbow poles, then clavicle/two-bone arm IK and hand orientation. Head look remains articulated independently.
- Native eye anchoring currently reads the model's head bone after solve (game_fp::headWorld/frame). Driving that head without preserving an original authored anchor would feed HMD motion back into the camera a second time. The implementation caches the fresh UNMODIFIED native head by instance/job/lease and let gameplay view anchoring read it under the current actor transform; leave cinematic sources unchanged. Test this boundary explicitly.
- An unreachable head/hand is a measured solver residual, not permission to stretch bones, move lower body or teleport. Missing/stale native pose must withhold the affected action instead of inventing authored-hand control.

## Shared actual action frame

- Keep requested tracking targets for diagnostics. Separately reconstruct actual solved grip pose from the hand matrix's calibrated finger/palm axes, undo configured finger pitch and mirrored wrist-from-grip offset. This uses the rendered wrist/hand, not the requested controller transform.
- Publish actual grip + solved wrist/elbow/palm and original physical mapping from ONE completed pose job with the originating command serial/owner generation/lease. Match actual actor/root rotation at render; do not assume feet translation alone fixes pose-to-render drift.
- Feed actual grip into shot origin/matrix, web line/rope emitter, and hand indicators. Use the render-eye generation/serial cache for world indicators. Do NOT overwrite the raw controller targets fed to the next IK solve (feedback loop/double mapping).
- Preserve forearm outward0/inward15-degree clamp and original OG runtime. Passive authored motion must never qualify as physical punch damage.

## Gates before shipping

Real pose-matrix tests, not only helper comparisons: both mirrored hands, full/level/world view frames, translated/rotated actor roots, torso/head yaw/pitch/roll, partial/absent tracking and owner/grip transitions; exact preservation of pelvis/lower-body matrices, bone lengths and waiver-free no-teleport policies. Compare requested versus actual wrists and render-time actor turns. Run Windows full CTest/protocol/package checks and headset acceptance; no 'full proper rigging' success claim from synthetic tests alone.

## Current implementation state

Implemented locally: rooted articulated spine/neck/HMD look, exact original-head gameplay camera cache, current actor-root rebase and actual solved-hand action consumers. Real production solver/view mapping tests cover24 Full/Level/World actor/HMD cases and16 hand-axis cases; actual IK fists test physical/passive/gap paths. Native Windows compilation/package/headset gates remain until recorded in the living Page. Old78eabb8 logs show L/R p95 wrist residual.056/.015m and large controller-vs-forearm angles but do not isolate torso mapping or validate newer repairs.

## Intentional preserved policies / limits

- Existing explicit ANIMATED AIR SPIN retains its authored full-body/camera spin policy (the new HMD torso articulation is off during that intentional spin). Player arms and actual action-frame publication still run. This avoids feeding a solved/spun head into its own camera or deleting the established flip effect. Ordinary native airborne poses use the new torso. OG air/wallrun solver is untouched.
- Existing waist yaw cap (~40 degrees), OG HMD lean and neck share remain; unreachable HMD targets report residual rather than detaching the waist or stretching bones. Example24-case fixture maximum head residual0.0498m is not a headset alignment guarantee.
- Existing180ms visual OG-to-native full-body handoff is retained before overlay; engine actor/momentum/collisions are never modified by this change.
- Action snapshot rebase reads current actor rotation/root at sample time. Engine movement after submission is still asynchronous; runtime telemetry/playtest is needed for residual fast-motion discrepancies.

- Full player rig uses weight1 for every freshly tracked/mapped arm, including idle. The former .55 idle floor mixed45% authored arm motion into the model and is deliberately no longer used when ANIMATED TORSO is on. Existing tracking-loss visual fade remains, while actions/history are withheld/reset by raw tracking. ARMS-only with TORSO off retains its old weighting.
