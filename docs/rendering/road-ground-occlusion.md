# Walker ground occlusion and cargo overlay

This bounded presentation correction starts from clean HEAD
`22962777dc2df4dc1cffb8312361f0a14f16c400`. Oliver observed original Market
figures walking, Service remaining a marker, and occasional apparent ground
pixels over figures. His video-state save is unavailable: the evidence below
reproduces both technical causes, without claiming his exact frame or city.
Service and HealthWorker remain outside this change; the subsequent [Service pass](../service-walker-visuals.md) implements its separately prepared figure, while HealthWorker stays open.

## Two independently reproduced causes

The old dynamic stream sorted Roads, buildings and walkers by
`(depth, ground_x, layer, stable_id)`. Layer priority only broke equal-depth
ties. An adjoining Road with greater depth could therefore overwrite an
opaque walker leg. This happened in both the productive unified merge and
the legacy map-first/dynamic-shared comparison.

Separately, `draw_courier()` submitted a role-colored cargo rectangle after
the original sprite whenever actual cargo was nonzero. The rectangle was
additional diagnostic artwork, rather than part of that animation. Omitting
only this SDL submission from an otherwise identical frame isolates its
visible contribution.

| Independent baseline witness | Before | Required corrected result |
| --- | --- | --- |
| Authored actual Food trip, tick 106, zoom 1, inner opaque yellow foot around `(353.882,306.202)` | Blue Road `(0,0,255,255)` | Yellow foot `(255,255,0,255)` |
| Same authored trip at tick 100, cargo-only comparison around `(350.605,295.563)` | Extra Food rectangle `(153,132,68,255)` | Underlying Road `(0,0,255,255)` |
| Original Xia ordinary-main Supplier, tick 37, pixel `(319,253)` | Native opaquely decoded leg `(41,49,99,255)` immediately before Road 783; Road overwrites it with `(255,214,140,255)` | Final `(41,49,99,255)` |
| Original Xia ordinary-main cargo-only comparison, tick 35, pixel `(308,262)` | Extra rectangle `(153,132,68,255)` | Identical old-order frame with rectangle omitted and current frame both `(90,73,53,255)` |

The first two witnesses use independently authored SG3 buffers and the real
`SandboxView` translation unit. Both separate baseline invocations fail
against the archived HEAD before production changes. Expected colors and
opaque/transparent regions come from the fixture raster, independently of
the production comparator and layer constants. Original-data traces and
frames remain ignored under `.local/road-ground-occlusion/`.
The final leg oracle uses an interior texel, rather than an outer sprite
raster edge. Its archived old-production invocation independently fails
Blue before the current invocation yields Yellow at the same tick/camera.
For the original scene, the independent pre-fix SDL readback immediately
before/after the specific Road submission and native record 3653/source
`(14,36)` establish the opaque color. An earlier mathematical nearest-sample
estimate is superseded; an alpha-128 shadow candidate is likewise excluded
from the opaque-leg evidence.

## Ground and spatial contract

The productive unified Sandbox path now submits:

1. Existing historical Ground/Base components.
2. Existing old-ground backdrops for valid new replacement-road previews.
3. Explicit Sandbox Road surfaces once, with their existing relative key order.
4. The existing linear merge of historical spatial items, buildings and walkers.
5. Existing selection, hover and invalid-placement diagnostics.

Only actual `Object::Road` dynamic descriptors move to the ground prefix.
The existing reusable instance vector is partitioned in place and its two
subranges sorted with the unchanged comparator. The spatial merge consumes
a span of the remaining suffix; no second Road draw or new map vector is
introduced. This also covers the existing Road fallback presentation.

Historical terrain, sand, riverbank, Rock, Cliff, Pinnacle, wall, GateHouse
and Elevation descriptors keep their current component policy and keys.
No historical image is classified by record, color, coordinate or map name.
Validated Great Wall bodies retain their one spatial Combined draw;
their material/context, alpha, culling, claims and fallback stay exact.
General interpenetrating whole-image composition remains unresolved.

The existing replacement mask keeps its meanings: **0** retains ordinary
historical drawing; **1** suppresses a replaced stored singleton entirely;
**2** draws its old image once as an early alpha-preview backdrop and omits
the later singleton. Regenerated and multicell landscape are never removed
by this lookup. F6 off, missing masks and legacy profile opt-out retain
their existing replacement behavior. Road selection, all 16 variants,
entrance/neighbour masks, anchors, integer-zoom origin rounding, nearest
sampling, alpha 128/255 restoration and signed-height projection do not
change. No offsets, stretched paving or new source assets are added.

F7 still selects the old whole-map-first/shared-dynamic comparison. It can
deliberately reproduce the old Road-over-leg pixel, and never changes the
World. The historical Full Snapshot mode also retains its old combined-image
path and shared dynamic ordering. F1 labels productive mode `ground+spatial`,
reports Road-ground items separately, and labels the remaining dynamic count
`spatial`; legacy/Snapshot use `shared`. `PainterStats` and the CLI report
expose `road_ground_pass` and `road_ground_items`. Stored merge visits remain
visits, including replacement no-ops, rather than claimed texture draws.

## Cargo and picking

A loaded original sprite submits its extra cargo rectangle only with F1
open. The corresponding auxiliary hit is registered under the same guard.
Normal sprite alpha and displayed-flip picking remain exact, including
transparent holes and foreground building/landscape selection. Actual good,
amount, target and trip remain available in the Courier Inspector regardless
of F1. F2 and genuine fallback markers retain their existing cargo diagnostic.
Original wheelbarrow/basket pixels are unchanged; no replacement cargo
animation or role is invented.

Roads add no selectable walker hit. Construction ground picking stays
separate. Stored visual picking skips precisely the historical singleton
images suppressed or moved early by replacement states 1/2, within the
existing reverse cached-alpha scan. It continues to the visible eligible
item below, instead of accepting an invisible overlay or rejecting the first
hit and losing a valid lower one. Empty/default masks, multicell images and
regenerated structural images retain their old picking. F6 now invalidates
submitted inspection until redraw, as do the existing layout/F1/F7/UI guards.
Map presses released over UI remain consumed.

## Regression and gameplay evidence

`road-ground-occlusion-production-pixels` runs actual paid City7 commands
and Food movement with a separate full control World. It covers adjoining
Road overwrite, asymmetric feet, transparency, normal/F1 cargo and hits,
high historical foreground, unchanged F7 comparison, zoom 1/1.15/2/4/back,
alpha-128 preview/cancel/paid build/remove, actual single Road submission,
full snapshot/SaveDocument equality and all twelve render counters.

Existing unchanged Road-continuity pixels retain the independently authored
paving-core oracle at 1/2/4 in both painter modes, including preview. Market
and Inspector production tests retain both walking directions/four storage
directions, curves, ends, turns, loaded/empty returns, pause/wait/save-load,
clipping, resize and actual sprites/fallbacks. The extended scene-composition
test guards normal-mode cargo protrusion behind a high structural foreground;
its F1/fallback assertions remain intact. Stored-graphics tests cover masked
singleton picking, visible lower hits, bounds and multicell preservation.
The structural wall/tower/gate/Pinnacle and texture compatibility regressions
retain their independent authored foreground, alpha and zoom oracles.

Ordinary Application/main comparisons use fresh private Xia and Kaifeng
roots, original figure/road/building/gate pixels, actual SDL submission and
scripted SDL event delivery. Positive leg witnesses and original foreground
roof/building controls are compared at the same complete World, tick,
camera and backend. Observer-only omission of the old cargo draw supplies
the separate four-case comparison; no production readback/logger is added.
These instrumented headless software runs are normal-app technical evidence,
not human/native-input acceptance or an uninstrumented GPU benchmark.
Private observer texture copies/readback are outside the production resource
and purity measurements. User-owned processes/settings/saves are untouched.

Eight independent pre-fix opaque original-leg witnesses, covering all four
Market roles loaded and empty, preserve their exact native pixels after the
fix. Forty original spatial foreground controls across Xia buildings and the
Kaifeng gate retain their old final pixels. The gate roof therefore still
hides a routed figure while its Road is below it. Root viewed the four-case
and gate contact sheets and ordinary-main normal/F1 frames; screenshots alone
are supplementary to the individual pixel/state assertions.

The Xia replay retains 1,608 complete canonical comparisons over 1,601
distinct ticks; Kaifeng retains 506 over 501. All equal their full ordinary
controls. Six F5 SaveDocuments and both F9 tick-400 full states/pose rows
match. Kaifeng observes 236 legal fixed-gate edges. Both families show all
four actual storage directions in Xia, and every Market role has positive
loaded and empty-return original bodies, with zero target-role fallback.
Uploads stay at 1,298 in Xia and 1,553 in Kaifeng; each retains 128 Walker
textures. All twelve render deltas are zero and owned processes/textures
exit cleanly. The frozen tick-407 F7 scene rectangle, with the old cargo
draw independently omitted, is byte-identical to the current F7 scene.

No simulation/navigation/persistence source changes are required. Full
WorldSnapshots, paths, arrivals, goods/reservations, taxes, Funds, residents,
workforce and full SaveDocuments are compared, rather than selected economy
fields. Rendering and preview retain zero files, decode/upload, World copies,
commands, ticks, BFS and route refreshes. Existing eager textures, resource
budgets and capacity reservations are reused.

The new authored target passes on actual dummy/software (0.66 seconds) and
actual Metal with an owned hidden Cocoa window (0.36 seconds). Its baseline
interior Road and Cargo invocations independently fail as above. These are
direct production-render/input calls, separately from ordinary-main tests.

## Measured rendering cost

The direct production benchmark retains the same original Xia complete
World/SaveDocument at tick 407, camera offsets, zoom, renderer/runtime and
passive SDL observer, with 20 warmup and 120 measured frames per case.
There are ten cases per backend: normal 1/1.15/2/4/back, F1, F7 at 2/4,
and F6 off/on. The timer covers CPU `SandboxView::render()` submission;
the target flush is outside it. It measures neither GPU completion nor
whole Application frame pacing. Short sequential samples retain drift and
do not establish zero overhead.

| Productive normal mode | Software median / p95 ms, old → current | Metal median / p95 ms, old → current |
| --- | --- | --- |
| 1× | 0.329 / 0.432 → 0.353 / 0.449 | 0.528 / 0.622 → 0.512 / 0.598 |
| 1.15× | 0.330 / 0.412 → 0.361 / 0.489 | 0.506 / 0.595 → 0.500 / 0.588 |
| 2× | 0.312 / 0.420 → 0.345 / 0.471 | 0.441 / 0.534 → 0.463 / 0.532 |
| 4× | 0.309 / 0.420 → 0.341 / 0.455 | 0.429 / 0.523 → 0.436 / 0.509 |

At F1/2×, Software median/p95 is 0.434/0.572 → 0.497/0.620 ms and
Metal is 0.720/0.852 → 0.757/0.878 ms. The measured small Software cost
and mixed Metal drift remain recorded rather than being called free.

Texture submissions stay exactly **536/432/165/73** at these zooms. Each
of the fifteen Road sprites submits once, seven courier bodies retain their
existing submissions, and normal fill rectangles decrease **35 → 32** by
removing the three loaded-sprite diagnostics. F1 keeps all 35 rectangles.
Painter statistics change **33 shared dynamic items → 15 ground Roads +
18 spatial items**, with 3,723 stored visits and one stored-order build
unchanged. F7 retains all 33 shared items. F6 off retains its fifteen Road
fallback geometry submissions and existing opt-out ground behavior.

Preparation stays **1,298 eager texture creations/uploads**, including 128
Walker, sixteen Road, twelve building and fifty Fire textures; both versions
destroy all 1,298 at shutdown. Walker native-image bytes and optional Market
536,512-byte/48-image supplement are unchanged. Each checked frame has zero
texture creation/upload/destruction and all twelve asset/World/navigation
counters zero. Reports `perf-{software,metal}-{before,after}.json` retain
complete per-case counters, resource counts, medians/p95 and provenance under
the ignored QA root. The normal-app observer comparisons remain separately
instrumented evidence.

## Final validation

| Final frozen-source suite | Passed / registered | Test time |
| --- | ---: | ---: |
| Debug | 123 / 123 | 737.54 s |
| Release | 123 / 123 | 65.59 s |
| Project ASan/UBSan, RelWithDebInfo | 123 / 123 | 202.02 s |

All three use the same **336 source/resource/tool/build files** with no
changes during configure/build/test, skips, compiler warnings or sanitizer
diagnostics. They include UnifiedPainter, SceneComposition, StoredGraphics,
Road visuals/continuity/responsiveness/topology, Market/Inspector, texture
pixels/zoom, input, save/recovery and full City determinism/endurance.
The project sanitizer configuration remains unchanged.

The tracked baseline contains 390 files. All **376 outside the fourteen
authorized existing code/test/build/documentation edits** remain identical;
the only new files are this report and the authored pixel regression.
All **1,464 original files / 819,711,091 bytes** and the actual system SDL
hash remain unchanged. In particular World, courier state, navigation,
permissions, readers/selectors/decoders, resource metadata, save/recovery,
input and zoom policy are untouched.

The private evidence index is
`.local/road-ground-occlusion/final-validation-report.json`. Its constituent
`pixel-regression-report.json`, `actual-main-report.json`,
`assets-evidence-summary.json`, `exact-starter-report.json`,
`final-{debug,release,sanitizer}-report.json` and `final-authority-report.json`
retain baseline/current sources, original opaque and authored pixels,
full state/save equality, draw/resource counts, timing and evidence limits.
The ordinary-main observer's texture copies/draw wrappers/readback impose
substantial QA overhead; its absolute times are deliberately excluded from
the product performance comparison above. No graphical human playthrough,
native input PASS, exact personal-video state or whole Emperor compositor
parity is claimed.

## Test the current version

```sh
sh tools/test_market_walkers.sh .local/gog-extracted/app
```

The exact unchanged script was rerun with final sources in fresh private
root `.local/market-walkers/player-Xia-H7TLch`: paid 1,280/1,300 starter,
24/24 workers, working save tick 407/schema 19/policy 1. All 1,601 canonical
helper states and fourteen full saves equal the ordinary control. Its
dummy/software ordinary-main menu startup passes and its owned process exits
cleanly. This smoke invocation uses a private shell/fixture-only SDL
hint-priority bridge to preserve the same dummy backend, plus passive real
SDL initialization observations; the tracked script and production source
are unchanged. It does not exercise Load/Space or claim a graphical/human
playthrough. The separate scripted ordinary-main replays supply loaded-game
render evidence. `exact-starter-report.json` records that boundary.

Choose **Load Sandbox → Load selected save → Space**. The helper creates
its own paused `working-supply.json` at ordinary tick 407 with actual tax
and both distributors travelling. Look at corners, adjoining diagonal Road
tiles and building entrances during Farm/Warehouse→Market→House trips and
empty returns. Normal original figures should have no extra colored cargo
rectangle, Roads should remain below feet, and foreground buildings/gate
roofs should still hide figures. F1 deliberately restores the diagnostic;
F7 deliberately retains the old ordering comparison. Optional Service fallback/Health markers
remain expected. Human native play acceptance and complete Emperor renderer
parity remain open. No commit, push, tag, release or publication is performed.
