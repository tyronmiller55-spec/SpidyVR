# Remaining after feat/animated-tracking-lock (Slice C)

## Delivered (this zip — not headset-tested)
- control-fixes (aim/handoff) @ 21a5f81
- animated-torso @ fe5898c
- animated-model-yaw @ 3df38b1: R-stick → game walk-facing turn while yielding
- **animated-tracking-lock (Slice C):** soft arm floor while yielding + chest-frame
  grip remap + dense `tracking lock:` / forearm diag behind ANIMATED ARMS

## Still open / not done
- Midair **spherical** aiming like SpidyVR mode (Slice A) — not implemented
- Momentum transfer (vel/dir/rot) from SpidyVR systems onto animated character (Slice B)
- Headset verification of tracking-lock p50/p95 forearm angles; model yaw in freefall
- Pure zero-translation turn-in-place (game left-stick turn may creep)

## Next zip priority
1. **A** Spherical midair rotation (keep Full; no silent Level)
2. **B** Momentum bridge onto animated character

## Model yaw facts vs hypothesis
**Verified:** left stick turns player matrix (VALIDATION); InteractStep uses that; CameraLook is camera-only; native_movement has no facing field; no REFERENCE facing-write RVA.
**Hypothesis in MODEL YAW build:** same pad path yaws actor root under ANIMATED AIR yield midair (user confirmed air turn worked).
**Rejected substitutes:** CameraLook/trackingYaw as fake model yaw; guessed actor transform writes.
