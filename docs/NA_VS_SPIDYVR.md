# Native Animation vs SpidyVR: what's different

**SpidyVR** is Ilya Mezerowsky's VR mod for Marvel's Spider-Man Remastered. **Native Animation** (NA) is a
community build on top of SpidyVR 0.8.7. Everything below is about how the two play differently, and where to
find each setting. Everything SpidyVR 0.8.7 does is still in NA unless this page says otherwise.

Where the settings are: in **Spidy Launcher.exe**, open the **Native Animation** tab (between Options and
Controls). In the headset, use the **NA ·** tabs in the menu, and **NA · HELP** explains every topic.

## Your body and the camera

- **Spider-Man's own animations move your body.** In NA, Insomniac's animations play your body on the ground:
  running, vaulting, parkour, landings, dodges. In the air, SpidyVR's physics take over. In plain SpidyVR,
  SpidyVR moves the body everywhere.
- **Your tracked arms ride on top of the animation.** Your hands stay on your controllers while the animation
  plays the rest of the body.
- **You always lead the view.** Only your headset and your turning (right stick, snap turn, recenter) turn the
  view. An animation never turns it for you.
- **HEAD: LEVEL (default), FULL or OFF.** LEVEL keeps the horizon level and smooth, and turns Spider-Man back
  under you after dodges, rolls, turn-arounds and landings. FULL lets the animation's head bob, nod and tilt
  move your view. OFF is the plain SpidyVR camera. Set it under **Head** in the launcher's Native Animation tab
  or **HEAD** on the **NA · CORE** tab.
- **He faces where you look.** On the ground, look well away from Spider-Man and he turns round to face you; where
  the game can't turn him yet, his upper body turns so you never look behind him (**Body follows view**, on).
- **Camera feel controls.** **NA · CAMERA** sets how much of the animation's motion reaches your view (CAMERA
  MOTION, PITCH, ROLL, BOB) and how much it's smoothed (STABILIZATION). Lower is calmer.
- **Smooth hand-offs.** Switching between animation and SpidyVR (landing, jumping, starting a swing) blends
  instead of snapping, and **Smooth view handoff** (on) glides out what's left, never more than 2 cm or 1 degree a
  frame your head didn't make. **NA · HANDOFF** tunes how fast.
- **Taking control mid-animation.** Tilt the left stick or squeeze a grip during a game jump or move to take
  the body over at once. Shooting a web on the ground or a perch does it too.
- **Showtime.** Hold **X + B** for a moment to step outside Spider-Man and watch him from behind. Use the right
  stick to orbit, and let go to come back.

## Movement

- **Point jump.** Press **A + B** together (or hold B, then A) to jump where you're looking. Tap for a hop, or
  hold up to a second for a leap. Plain **A** is still the game's own jump.
- **Flips on A.** Hold the left stick a way and tap A to flip that way. In the air, tap A twice to turn upside
  down.
- **Wall slide and wall run.** Near a wall you slide down it, with a classic wall-slide leg pose. Push the left
  stick toward the wall to blend smoothly into a wall run that way. Let go to slide again.
- **Grip on walls.** Squeeze a grip with your palm on the wall to stick, then crawl as usual. While you grip, a
  pull flings you off the wall whichever way you pull. A normal jump stays locked while you grip, but an
  **A + B** point jump still launches you.
- **Sticky palms.** An open palm against a surface drags on it. Use it to slow a slide, or sweep it to push and
  gain speed on a wall run.
- **Handstands.** Land upside down on both palms and you balance on your hands. Squeeze grip to lock the
  stance. Let go of grip to push off or drop.
- **Air dodge.** In the air, **B** dodges the way the left stick points.

## Webs

- **Full-length webs.** A web keeps its length and the slack hangs, and lines wrap round building edges.
- **Rope physics.** Hanging slack pulls on you, and lines swing and trail. ROPE WEIGHT and ROPE DRAG are
  adjustable.
- **The web is in your fist.** A held line comes out of your closed hand, and the hand closes around it.
- **Reel out.** Double-tap a web's reel trigger and hold it to pay the line out.
- **Web tethers (experimental).** A web on a prop or an enemy sags and drapes like a real rope. Snap your hand
  back to whip it toward you, circle your hand to spin it around you, and let go of grip while it spins to
  sling it.
- **Hand grab.** Put a palm on an enemy or a prop to hold it.

## Combat

- **Kicks.** Hold **Y + X** to use your legs, then flick the right stick: air kicks, swing kicks, and ground
  kicks (front, side, sweep). Kicks lunge at a nearby enemy, and a ring shows who a kick would hit.
- **Mid-air poses.** Y + X with a slow stick push poses the legs: tuck, starfish, dive, superhero.
- **Punch tracking.** Real punches steer onto the enemy in front of you.
- **Crime Run (off by default).** A random-crime challenge mode. Turn it on under Kicks & Combat, then start it
  from the phone's siren app.

## Settings and help

- **Everything in one place.** NA's settings sit on their own **NA ·** tabs in the headset, and on the
  launcher's Native Animation tab, marked NA.
- **Every feature has its own switch.** Don't like something? Turn just that piece off. Off plays like it did
  before that feature.
- **Reset one tab.** Press **Y** (RESET ALL) on an NA tab to reset only that tab.
- **Built-in help.** **NA · HELP** in the headset (and Help in the launcher) explains each feature and what to
  try.
- **Reporting problems.** Press **Copy for Discord** above the launcher's session log and paste it, along with
  the newest three files from the **reports** folder.

## What official SpidyVR has that NA doesn't (yet)

NA is built on SpidyVR **0.8.7**. Ilya's **0.8.8** adds things NA doesn't have:

- **First-time VR setup** from the title screen (play style, web button, turning, seated mode, height,
  cutscenes, comfort vignette, body calibration).
- **Play style presets** (Arcade, Balanced, Pro) you can pick in the headset.
- **Comfort vignette:** darkens the edges of the view during fast movement and turns. NA only has a vignette
  for Showtime.
- **A cutscene switch in the headset** (game screen or first person).
- **VR wording for the game's tutorials and prompts**, so the game names VR buttons instead of gamepad ones.
- **Clearer startup error messages**, such as Linux runtime checks and a mismatched graphics card.

**The camera feels different.** Plain SpidyVR's camera only follows your headset, so it's very steady. NA's
camera can also follow the animation. If NA feels less smooth, keep HEAD on **LEVEL** (the default) or set it
to **OFF**, and lower CAMERA MOTION or raise STABILIZATION on NA · CAMERA.

> ⚠️ **Don't press the launcher's Update button.** It installs official 0.8.8 over Native Animation. To try
> official SpidyVR, put it in its own separate folder.
