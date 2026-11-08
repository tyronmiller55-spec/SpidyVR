# Web-ball dispatch observations — local diagnostic prototype

Base: delivered 0.8.5 compatibility commit `78eabb802448078f98061675c616bdaa8f51fa8a`.
This changes diagnostics, not aiming, input mapping, ammo, damage or dispatch policy.
Diagnostic build: use its exact commit and CI results to identify verification.
Not headset-tested; do not confuse it with the delivered compatibility package.

## What the previous evidence establishes
The supplied 0.8.1 session identifies commit `96aed2abb4`, not the later four-slice tip.
Its two mod logs mirror the same diagnostic stream; count once. Final totals are
335 requested / 335 fired, 7 targeted, 7 resolved, 4 webbed, 85 collisions, 119 empty.
Legacy `fired` means successful engine dispatch path, NOT a visible projectile.
The gadget path can return success without a returned projectile pointer. That is
not itself a demonstrated bug. Collision tracking covers the fallback flight list,
not every gadget shot. Webbed counts are not unique-enemy counts. The console's
0xC0000409 exit is a separate investigation; these counters do not establish its cause.
This is not evidence that the user's current 0.8.5 session passes or fails.

## Protocol and labels
Shooter data v5 appends 136 bytes to the unchanged 296-byte v4 prefix (432 total).
The Python reader accepts both schemas and rejects torn/short reads. Legacy field
names/meaning and the existing `web balls:` log line remain unchanged.

One `wb dispatch:` summary per existing one-second reporting interval:
- q: accepted enqueue calls, including probes; full: queue refused enqueue calls.
- exp: expired dequeued requests (no engine call).
- call: requests whose dispatch path returned success, not count of visual balls.
- rej: nonexpired requests whose dispatch path did not succeed, including pre-call rejection.
- obj: successful requests with a nonzero resolved object address; not a lifetime or validity guarantee.
- unk: successful requests without that address; not proof of missing projectile.
- g / f: actual gadget / fallback engine-call attempts (one request can attempt both).
`call = obj + unk`. Queue totals and completion totals need not match during pending
work or cancellation. No pointer is newly dereferenced by these observations.

Up to two additional lines, only when the completed-request ordinal changes:
`wb event` records the latest hand (0 left/1 right), route (0 no call/1 gadget/2 fallback),
outcome (0 none/1 expired/2 rejected/3 call-only/4 object-address returned), final shifted
muzzle and direction. `wb aim` records that request's aim point and target identity.
`last-hit` is the LEGACY latest collision identity, NOT a correlated hit for this request.
These are bounded snapshots, not an exhaustive per-shot history. An expired or
pre-validation-rejected request may have zero muzzle/direction. Target/object IDs
are engine identities, not proof of supported enemy, impact or damage. Each line
fits the existing 240-character slot; coordinates use three significant digits.

## Minimum playtest capture (after a diagnostic Windows build)
1. Record exact package/commit, settings, game version and local start time; retain
   the original mod diagnostic and console/game log, marking mirrored copies.
2. Choose a visible nearby stationary surface, then a supported visible enemy.
   Note distance/occlusion and whether the game currently permits gadget use.
3. With animation owning movement, do a short spaced left-only burst, then right-only;
   repeat after explicit SpidyVR takeover. Keep target, stance and settings stable.
4. For each of those four cases note trigger time/hand, marker position, whether a
   projectile is visibly emitted, origin/direction, impact and enemy reaction separately.
   Leave a quiet second between cases so bounded latest-event snapshots are useful.
5. Compare requested/hand totals to queue/full/expired/rejected/call and g/f deltas;
   then compare obj/unk with the visual observation. Neither pointer category alone
   establishes success/failure. Existing target/resolved/collision/webbed counters
   provide additional scoped evidence, not a universal hit rate.
6. Capture both normal ammo and a clearly observed no-charge condition if available;
   do not change settings mid-case. Record a failed visual example's timestamp.

For a new crash preserve the dump if produced and exact termination time separately.
Do not run the probe concurrently with the four input cases; probes also enqueue.

## Local verification
`python3 tests/shooter_diagnostics_protocol_tests.py` exercises the production decoder
without importing Windows transport modules. `tests/core_tests.cpp` adds pure C++
classification/counter checks; these require the existing Windows/core build route.
The Windows release workflow runs CTest, all protocol suites, and package/import/version
checks. Consult the diagnostic PR and exact CI run rather than assuming they passed.
Headset behavior is separate and remains unverified until an actual playtest.
