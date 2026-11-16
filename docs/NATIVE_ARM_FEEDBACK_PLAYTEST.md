# Native arm feedback consumer audit / playtest

This unmerged slice extends the delivered 378c44e player torso; it is NOT
headset acceptance of that torso or of the earlier no-shot cancellation repair.
Extract the ENTIRE new launcher archive into a new writable folder. Never mix
host/rays DLLs or Python tools. Back up shared %APPDATA%\Spidy settings/profiles;
this package carries no personal settings. Existing VC++ x64/OpenXR prerequisites
are unchanged. Start Spidy Launcher.exe, retaining your prior native/moves/arms/
torso and wrist/forearm settings; do not invent a new binding for this test.

## Source-demonstrated fixes

* `game_xr.cpp` formerly set actual presentation grips only when free-hand aim
  mapping succeeded. `mapAimToDisplayedGrip` intentionally refuses a held web:
  preserve that physical force/anchor aim, but always consume the valid solved
  grip for native game-rope/zip endpoints and wrist feedback. This is not a new
  web gesture or a change to an already attached web's physics.
* `native_body.cpp::solveHero/actionFrame` now carries the same-job mapped HMD
  target beside actual hands. Current actor rotation rebases it; root translation
  is added once in XR. Look/gaze targeting and spatial sense/haptic feedback no
  longer compare native-world threats/targets with the unrelated raw rig head.
  Eye gaze preserves its physical head-relative offset. Missing native mapping
  withholds gaze targeting/directional placement rather than inventing a head.
* `feedback_frame::camera` installs exact generation/serial-matched native render
  eyes (with travel removed once) BEFORE held reprojection chooses its pivot.
  Actual solved hands/world anchors are never transported through the camera a
  second time. Phone/wheel frames are explicitly transported from their raw
  physical rig head to that captured camera; physical gestures remain unchanged.
* Captured hand feedback rechecks current owner floor/pose lease after the late
  ownership transaction; absent paired solves hide corresponding hand feedback.
  Aim/line markers also obey the existing render feedback gate: an absent or
  wrong-generation native camera is not permission to draw with raw rig eyes.

## Complete remaining-consumer classification

| Consumer | Contract / disposition |
|---|---|
| Raw hand collision rays, wallPositions, wall-climb, arm activity, pose.hands, body IK, dodge/locomotion and ownership handoff inputs | Physical tracking/locomotion; deliberately unchanged. Feeding solved hands back would create a feedback loop or change OG movement. |
| Raw view.grips during calibration; diagnostic hand matrices and forearm angle comparisons | Deliberate measurement/reference; unchanged. Calibration gestures and rings are not a substitute solved-hand action frame. Camera-relative display transport is separate. |
| actionGrips native zip strand/game rope wrists; saved actual wrist meters/pad signs | Actual solved-hand presentation; held-web bypass repaired, paired validity/age checked. |
| Module look/gaze raycasts, line target choice, punch lock/cue; sense side/health feedback | World targeting/feedback; consume mapped head when native. Physical punch damage still requires actual swept contact and independent physical speed, not the cue/lock. |
| Phone/wheel raw tracking gestures | Physical input; unchanged. Render placement alone uses actual native camera transport. |
| Aim/offer/line markers, cue/impact/knockout art, spider-sense | Existing module/world targets; camera-generation feedback gate. No teleport/lunge or automated punching re-enabled. Markers are presentation, not proof of an engine hit. |
| webTimes.update raw shooter wrist | Attach/release history, NOT flight seeding. Used by OG overlay release snapback/shape; native overlay web strands are suppressed in favor of game ropes. Preserved OG behavior rather than blanket replacement. |
| WebFlightVisual | Module flight phases/durations/progress/tips/anchor telemetry, not webTimes wrist. Unchanged; returned/fired counters are not visual outcome proof. |
| Raw head-based locomotion, camera steering outside explicit eye-zip targeting, input calibration, rigTurn/physical tracking turn | Intentional physical/ownership controls; unchanged. Explicit eye-zip fallback gaze stays consistent with its mapped world target. |

No exported packet change: swing Command v7 remains 304 bytes (v6 272); only
private in-process OverlayTracking / NativeEyeFrame fields changed. Snapshot lock
order stays solve -> forearm; no queue lock changes. Head/hand mapping uses the
existing inclusive owner serial floor and strict lease expiry. Ordinary OG
presentation, ground/native ownership, exact-update eligible grip release,
explicit grip-held override, air/wallrun, momentum/collision, the outward0/inward15
clamp, authored air-spin policy and physical-only punch damage are preserved.

## Acceptance steps (headset still required)

1. Standing, walking and running: turn/tilt HMD and both wrists; shoot web balls at
   the same enemy/prop indicated. Compare both arms, wrists, game rope ends and
   markers at several world locations and after large native root turns. Repeat
   existing Full/Level/World options without resetting your profile.
2. Attach an ordinary web while in OG air/wallrun and retain it on native ground
   entry: the rendered rope/zip wrist should meet the actual arm even while held.
   Existing tension/anchor behavior should not acquire a new force direction.
3. Check left/right wrist meter/pad sign, phone, wheel, spider-sense direction and
   optional combat art while turning. With your usual eye-aim setting, look away
   from hand direction and compare preview/projectile aim; no automatic lunge.
4. Cancel with each grip and both across .5: press/hold must ONLY activate OG,
   never fire a line, web ball or punch. Release eligible idle/walk/run on ground:
   native ownership returns that update. A subsequent independent OG air grip
   must retain its original web action. This is mandatory runtime verification,
   not a claim that previous helper tests proved cancellation on your headset.
5. Genuine physical swung hands should damage; passive native animation/root
   turns should not. Briefly lose/reacquire a controller: no stale phantom sweep
   or raw-rig aim feedback. Retest native jump/landing and OG air/wallrunning.

## Verification limits / next gates

Portable tests execute production frame transports, real body solve/solved-hand
reconstruction and punch contact events; Windows builds compile the engine-facing
callers and execute the full portable/protocol suites. Neither runs the game engine or headset.
Actual render alignment, coupled input timing, controller calibration and cue
readability remain unknown. An intermediate test wrongly assumed the owner floor
was exclusive; corrected to its existing inclusive contract, not changed runtime
policy. No fresh 378c44e logs are available; latest uploaded logs stamp 78eabb8.

Next full-plan gates: exact-new-source headset logs for these core behaviors;
crash dump/stack for the older 0xC0000409; procedural-leg direction/physical binding
not decided; genuine normal strafe/backpedal clips and supported decoupled-facing
hook not proven. Do not invent leg controls or synthetic animations to fill time.
