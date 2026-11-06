# Remaining after feat/animated-dedicated-yaw

## Delivered (this zip — not headset-tested)
- Prior stack through animated-only-sphere @ 4e4c3d5
- **dedicated yaw @ (this tip):** MODEL YAW gated off midair when AIR SPIN owns
  R-stick 6DOF; CameraLook cleared; ground MODEL YAW sign kept (SpidyVR parity);
  diag `model yaw:` apply / idle lines.

## Still open / not done
- Headset verify: ground +R → model turns right; midair AIR SPIN 6DOF no opposite yaw;
  MODEL YAW diag idle midair while spin active
- Midair torso softlock (Slices 2–3) — next zip
- Tracking-lock 1:1 under softlock + weblines from shooters (Slices 4–5)
- ONLY + AIR SPIN headset verification (Slice 6)
- Legs redesign (tabled)

## View policy (user 2026-11-05; softlock not in this zip yet)
- Ground/parkour/perch: headbone follow (Full/Level per setting; never silent Level)
- Midair: torso softlock (perch-like) — lands in torso-softlock zip
- Outside yield: SpidyVR tracking path
