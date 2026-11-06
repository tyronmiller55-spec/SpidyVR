# Remaining after feat/animated-hybrid-owner

## Delivered
1. Flesh-out stack (dedicated yaw → softlock → hands-webs) @ `3ddbe36`
2. **Slice 1 — ANIMATED HYBRID** (this tip): ground anims / OG SpidyVR air FSM +
   setting (default Off). No clean eye/IK handoff yet.

## Next
- **Slice 2:** `adoptEyes` + warm `beginReclaim` + arm crossfade on G→T
- **Slice 3:** eye blend + yaw carry on T→G (no `adoptFeet` on hybrid land)
- **Slice 4+:** OG air immersion (momentum lean, swing-arc legs, web-arm tension)

## Headset check for Hybrid
MOVES on, HYBRID on, AIR off, ONLY off, AIR SPIN off, HEAD Full, ARMS/TORSO/MODEL YAW on.
- Ground fight/run still animated
- Jump/swing/wall → OG body; look for `hybrid: G->T` / `T->G` / bounce suppressed
- Expect old reclaim pop until Slice 2/3 (baseline)

## Tabled
- Legs redesign
- True chest perch eyes offset
