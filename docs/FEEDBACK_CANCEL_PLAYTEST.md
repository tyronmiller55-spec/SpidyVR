# Native Animation feedback + no-shot cancel follow-up — 0.8.5

This NEW playtest includes the previous action-frame/punch fixes plus two narrow source repairs. Full source SHA is in ACTION_FRAME_BUILD.txt. It does NOT contain the requested full upper-torso rig overhaul; that work is active separately.

## Changed in this slice

- Native head-follow previously reused the cinematic scene flag and hid combat target cues, impacts/KO artwork, Spider-Sense/sight, focus/pad wrist feedback, calibration, gadget wheel and phone UI. Gameplay feedback now has its own gate. True cinematic/flat-screen presentation stays unobstructed; duplicate overlay gloves/ropes remain hidden. Existing UI/aim-marker/combat-artwork settings are respected.
- The combat cue remains an observational target/flow indicator. In animation it does NOT authorize a remote hit, auto teleport, lunge, counter flight or arrival punch; genuine physical contact still determines damage.
- A live cancellation press no longer depends on mover-telemetry freshness. The old predicate could lose that press during a stale sample and let it reach the ordinary web solver. Other existing scope/focus/owner/setting gates remain. This is a demonstrated source defect, NOT yet confirmed as the cause of the user's latest report: the three latest attachments were unavailable for inspection.
- Cancellation diagnostics now include raw web/shot masks, tracking mask and mover freshness.

## Install/test

Close game/VR/old launcher. Back up shared `%APPDATA%\Spidy` (settings and profiles), then extract this WHOLE ZIP into a NEW writable folder outside the game. Start `Spidy Launcher.exe`; do not mix DLLs/tools. Keep your saved profile and Native Animation MOVES + HYBRID + ARMS, AIM MARKERS and COMBAT VISUALS enabled for these observations; keep RUN LOGS / HEAD PROBE on. This does not reset your settings.

1. Native animation: look between enemies. Check the target cue and actual physical punch hit feedback, Spider-Sense including off-screen threats, both hand aim/interaction marks and web-line zip marker. The cue is NOT a guarantee of contact or a teleport destination. Compare OG without changing UI settings.
2. Check gadget wheel, wrist focus/pad status when active. No duplicate glove/rope should appear. Cutscenes and flat-screen menus must not gain gameplay artwork.
3. At ground idle/walk/run and a native airborne pose, press the existing grip/web-hold cancel: NO web line from either hand. Hold it; no delayed line or synthetic shot. Release: eligible ground restores native ownership that update. Independent release/repress in original airborne SpidyVR still shoots normally. Repeat each hand, both hands, grips/triggers together, and during a rendering/movement stutter.
4. Recheck real web balls and swung-hand damage, no passive-animation damage or teleport, momentum/rotation and original air/wallrun.
5. Return newest logs and approximate failure time/hand; do not infer a successful effect from fired/object counters alone.

## Not claimed fixed here

The reported model-arm/upper-torso rig alignment and fast-root-turn drift remain active work, NOT repaired by enabling indicators. Existing mapped-grip versus solved-wrist uncertainty remains. No headset success is claimed. The latest missing logs were NOT inspected or counted. Microsoft VC++ x64/OpenXR prerequisites remain unchanged; MSVCP140 is not bundled. Read ACTION_FRAME_PLAYTEST.txt for inherited behavior and limitations.
