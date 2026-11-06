# Remaining after feat/animated-hands-webs

## Delivered (flesh-out stack — not headset-tested)
1. dedicated yaw — MODEL YAW gated midair when AIR SPIN owns R-stick
2. torso softlock — midair effective softlock; ground Full/Level kept
3. **hands+webs (this tip):** softlock/headbone → remapGripToChest; weblines
   from shooters when forearm offs healthy (armsOverlay fallback otherwise)

## Slice 6 — ONLY + AIR SPIN verification (headset script, no new features)
Enable ONLY + AIR SPIN FLIPS+MOMENTUM + HEAD Full + ARMS + MODEL YAW + stack.
- Wall/combat/zip under ONLY → zero `mod body again` except cutscene
- Midair: `head follow softlock`, `model yaw: idle (air spin owns R-stick)`
- `tracking lock: … softlock 1 …` offs trending down vs Full-anim-head dumps
- `forearm aim:` arms-overlay fallback rate drops when offs healthy; webs leave
  shooters
- AIR SPIN lean/bank comfort (provisional maxDive/Bank)

## Still open / tabled
- True chest perch eyes-from-chest offset (softlock uses head place + level axes)
- Legs redesign (tabled)
- Pure zero-translation turn-in-place creep
