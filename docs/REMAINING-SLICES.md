# Remaining after feat/animated-hybrid-owner

## Delivered
1. Flesh-out stack (dedicated yaw → softlock → hands-webs) @ `3ddbe36`
2. **Slice 1 — ANIMATED HYBRID** FSM + setting (default Off)
3. **Slices 2+3 — clean G↔T handoff** (this tip): `adoptEyes` re-anchor,
   warm `beginReclaim`, 180 ms eye/yaw blend both ways, delayed arms wipe on
   G→T, delayed MODEL YAW + actor-yaw seed on T→G, no `adoptFeet` on hybrid land.
   MODEL YAW sign fixed (+R turns hero right with −Z actor ahead).

## Next
- **Slice 4+:** OG air immersion (momentum lean, swing-arc legs, web-arm tension)

## Headset check for Hybrid handoff
MOVES on, HYBRID on, AIR / ONLY / AIR SPIN off, HEAD Full, ARMS/TORSO/MODEL YAW on.
- Jump / web attach: no view snap; look for `hybrid: G->T … eyes dPos` (want p90 < 0.05 m)
- Land into run/fight: blend back; `hybrid: T->G (land …) eyeBlend 180 ms`
- R-stick on ground: +X turns the hero right (same sense as OG SpidyVR)

## Tabled
- Legs redesign
- True chest perch eyes offset
