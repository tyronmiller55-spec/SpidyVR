# Native Animation package status

Packaged on upstream main `bdd4c9150f` as 0.8.1 with a Native Animation launcher
tab/headset block and recommended fresh defaults. H and S retained. Z (Y-zip
ownership), C (swing-blend polish), legs and known 0xC0000409 crash investigation
remain out of scope. No custom offsets; docs/REFERENCE.md and VALIDATION.md remain
the executable evidence. Packaging merge has not been headset-tested.

---

# Remaining after feat/animated-hybrid-owner

## Delivered
1. Flesh-out stack (dedicated yaw → softlock → hands-webs) @ `3ddbe36`
2. **Slice 1 — ANIMATED HYBRID** FSM + setting (default Off)
3. **Slices 2+3 — clean G↔T handoff** (this tip): `adoptEyes` re-anchor,
   warm `beginReclaim`, 180 ms eye/yaw blend both ways, delayed arms wipe on
   G→T, delayed MODEL YAW + actor-yaw seed on T→G, no `adoptFeet` on hybrid land.
   MODEL YAW sign fixed (+R turns hero right with −Z actor ahead).
4. **Intent owner (Slice A)** — animation is the default owner; OG VR only on
   web attach / airborne web press / mid-air rotate / airborne speed ≥ 15 m/s /
   Spidy wall run. 500 ms VR minimum, 250 ms land confirm. Head follow = owner
   (`animOwns`), no per-frame classify drop. ANIMATED HYBRID SPEED row: speed
   hold OFF or 5–500 ms (200 default).
5. **Parkour steering (Slice B)** — left stick view-relative while the animation
   owns (`hybrid_steer.hpp`); ANIMATED STEERING bypassed under HYBRID; CameraLook
   follows the view unless the R-stick turns.

## Next
- **Slice C:** blend window 180 → ~250 ms once headset confirms the owner stopped chattering
- **Slice 4+:** OG air immersion (momentum lean, swing-arc legs, web-arm tension)

## Headset check for Hybrid handoff
MOVES on, HYBRID on, AIR / ONLY / AIR SPIN off, HEAD Full, ARMS/TORSO/MODEL YAW on.
- Parkour, hops, jumps, game zips, crawls: stay on the animation, head on the body;
  `hybrid: anim->vr` should appear only for web / air-web / rotate / speed / wallrun
- Web attach or mid-air turn: no view snap; look for `hybrid: anim->vr (…) … G->T … eyes dPos`
- Land into run/fight: blend back; `hybrid: vr->anim (land …)` with land ≥ 500 ms
- R-stick on ground: +X turns the hero right (same sense as OG SpidyVR)
- L-stick in a full circle while running and over rooftops: he heads where you push,
  relative to your view; `hybrid steer: … sent L(…)` once a second

## Tabled
- Legs redesign
- True chest perch eyes offset

## Hands / run-and-turn follow-up (2026-11-06 PT)

- H: HYBRID hands share the resolved animated eye frame, continuous tracked
  overlay with existing ramps, same-solve diagnostics/tests. Not headset-tested.
- S: preserve run magnitude while turning; share pad/native ground heading and
  gate hidden rig yaw. Not headset-tested.
- Z (Y-zip immediate animation ownership) and C (exact blend endpoints / slight
  view smoothing) remain separate and unimplemented. Legs remain tabled.
