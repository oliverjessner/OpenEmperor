# Scene composition: Great Wall and Elevation

This bounded presentation fix addresses visible Great Wall body pixels that
the old early-base pass exposed to later terrain and Elevation writes. Its
composition contract is **OPENEMPEROR PREVIEW**. Original restore-context
authority, complete first-draw reproduction and the original per-pixel
compositor remain unverified. The baseline and research below are distinct from the implementation and
validation evidence recorded later in this report.

## Research and baseline

The fresh baseline used clean HEAD
`3b834c2bbc1c999f38cf7d54e63ae8dacd1a758f`. Ignored original-data reports,
pixel traces and captures reside under `.local/scene-composition/qa/`.
All six standalone maps read the complete 4,000-record manager. Explicit
Stone preview is caller-selected material presentation, with original
context verification false and reproduced Great Wall counters zero.

| Map | Serialized wall objects | Active preview claims | Preview wall cells | Physical assets | Logical texture bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Badaling | 53 | 53 | 740 | 382 | 56,402,352 |
| MPWall1 | 53 | 53 | 740 | 525 | 65,515,872 |
| MPWall2 | 51 | 51 | 708 | 494 | 47,771,736 |
| Handan | 53 | 53 | 740 | 442 | 61,713,120 |
| Xia | 0 | 0 | 0 | 364 | 18,840,384 |
| Chengdu | 0 | 0 | 0 | 409 | 18,658,080 |

### Concrete overwrite chain

**OPENEMPEROR OBSERVATION:** `Handan-baseline-trace.json` records both the
initial wall-base write and later writes, unlike the previous partial
`Handan-overlap-trace.json`. Camera center is `(96,52)`, viewport
1280×720, zoom 1, offset `(-1120,-1300)`. The selected Stone-10 wall is
original entity ID 1, phase 10, model piece/variant 16, physical record 217
in `DATA/China_Mon_GreatWall_10.sg3`. Its side-4 origin is `(96,49)`, draw
and height marker `(96,52)`, front/depth cell `(99,52)`. The image is
318×238 at screen origin `(601,202)`.

All RGBA values below have alpha 255. The painter key is the existing
OpenEmperor logical `(depth, projected X)` pair; the complete key also
contains layer and stable ID. These keys are not an original-visibility
proof.

| Pixel | Pass/component | Asset and physical record | Origin | Painter key | RGBA after write |
| --- | --- | --- | --- | --- | --- |
| A `(710,330)` | Early wall Base | GreatWall-10 217 | `(96,49)` | `(1880,1880)` | `(82,74,90,255)` |
| A `(710,330)` | Spatial overlay | Terrain 229 | `(94,49)` | `(1720,1800)` | `(115,123,66,255)` |
| A `(710,330)` | Spatial overlay | Elevation 208 | `(95,49)` | `(1740,1840)` | `(123,115,90,255)` |
| A `(710,330)` | Spatial overlay | Elevation 355 | `(95,50)` | `(1760,1800)` | `(214,173,115,255)` |
| A `(752,348)` | Early wall Base | GreatWall-10 217 | `(96,49)` | `(1880,1880)` | `(90,82,90,255)` |
| A `(752,348)` | Spatial overlay | Elevation 208 | `(95,49)` | `(1740,1840)` | `(206,173,115,255)` |
| B `(648,411)` | Early wall Base; final visible pixel | GreatWall-10 216, entity ID 2 | `(96,53)` | `(1960,1720)` | `(57,49,66,255)` |
| C `(790,455)` | Spatial foreground; final visible pixel | Elevation 224 | `(100,52)` | `(1900,1920)` | `(115,90,82,255)` |
| D `(250,400)` | Early terrain Base | Terrain 461 | `(90,56)` | `(1780,1360)` | `(41,33,33,255)` |
| D `(250,400)` | Spatial neighboring overlay; final pixel | Terrain 927 | `(91,57)` | `(1820,1360)` | `(90,90,41,255)` |

A confirms the global-pass failure: the wall body is visible in Type-30
Base and its spatial key follows the overwriting landscape keys, but Base
was drawn before the entire spatial stream. B supplies a visible Base
pixel whose baseline hit result is null. C supplies a foreground
landscape control with a later spatial key; D supplies a neighboring
terrain control with no wall write. These are different roles. The trace
does not show that every occluded wall pixel is wrong or that every
landscape pixel should move behind walls.

At baseline, `StoredGraphicsRenderer::render()` draws admitted regenerated
bases in an early pass and overlays in a later stream. `SandboxView` uses
the same early pass before its merged road/building/walker painter.
Regenerated alpha picking sees only the overlay. The base path also
omits the regenerated semantic-layer visibility check used by the overlay
path, allowing a hidden wall Base to remain in F8 views.

A Type-30 Base is a file payload of diamond-tile color samples. The file
format does not establish that these samples depict only flat scene
ground. The Handan Stone-10 piece contains visible masonry in that
payload. Format component and scene role therefore need separate
contracts; a pixel-color heuristic or decoder change is not justified.

### Independent hypotheses

| Hypothesis | Bounded finding | Consequence |
| --- | --- | --- |
| A: global component pass | Fresh baseline records wall Base before the entire spatial stream, then Terrain/Elevation writes with keys preceding the wall. | Emit admitted structural body at its spatial item. |
| B: wrong wall reference point | The established marker is `(96,52)` and separate front/depth cell `(99,52)`. Both nearby Elevation items already precede the wall in the late stream. | Changing only the key cannot protect an early Base. Preserve marker, anchor and front cell for this fix. |
| C: distinct height sources confused | Original code conditionally substitutes embedded monument height `+0x28` for signed cell height. Here ID 1 raw monument height 4 equals saved marker height 4; adjacent Elevation cells are 2. | Preserve wall displacement 160 and adjacent Elevation displacement 80. No offset or global-height adjustment is justified by this case. |
| D: historical Landscape components invalid after restore | Exact original eligibility of historical Elevation overlays after entity restoration remains unproved. | Preserve both records and complete historical/generated ownership and atomic fallback. No map/record-specific suppression. |
| E: some occlusion legitimate | A later write establishes order, not legitimacy. Foreground landscape/buildings/walkers must be able to hide walls. Whole sprites can need finer splits when geometry interpenetrates. | Preserve deterministic foreground wins. No universal wall-last or Elevation-first rule; general interpenetration remains open. |

The bounded **OPENEMPEROR PREVIEW** contract follows these findings:
admitted structural wall body participates once in the existing spatial
merge, at the established key, with its effective visible alpha available
to object inspection/picking. Unrelated ground handling remains unchanged.
Components must not be drawn twice through Combined plus Base/Overlay;
their original decoding, alpha and blend behavior stay intact. Layer
visibility and complete-image culling must apply to body and overlay
together. Ground-cell construction picking remains a separate operation.
Independent synthetic pixels and real foreground/control scenes are
needed to validate the implementation; this is not original compositor
parity.

### Bounded original draw-chain recheck

**EXE-OBSERVED:** pinned SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`
was verified read-only. The EXE was not executed. ImageBase is `0x400000`;
`.text` RVA `0x1000` maps to physical `0x400`, so the following code
offsets are `VA - 0x400c00`. Runtime globals, embedded-object offsets and
decompressed map offsets remain separate address spaces.

| Observation | VA | RVA | Physical EXE offset |
| --- | --- | --- | --- |
| Drawing entry calls `0x46ff60` | `0x46a2b9` | `0x6a2b9` | `0x696b9` |
| Ground traversal reads saved cell height | `0x4700f3` | `0x700f3` | `0x6f4f3` |
| Sign extension and times-40 displacement | `0x4700f8`, `0x470103` | `0x700f8`, `0x70103` | `0x6f4f8`, `0x6f503` |
| Conditional monument height substitution | `0x470158`..`0x470164` | `0x70158`..`0x70164` | `0x6f558`..`0x6f564` |
| Applies selected displacement to Y | `0x4701f9` | `0x701f9` | `0x6f5f9` |
| Same function starts another view-grid traversal | `0x47027c` | `0x7027c` | `0x6f67c` |
| Marker/owner-dependent late virtual calls | `0x4709a5`, `0x470a21`, `0x470a90` | `0x709a5`, `0x70a21`, `0x70a90` | `0x6fda5`, `0x6fe21`, `0x6fe90` |
| Ground helper tests marker `0x40` | `0x46fa5e` | `0x6fa5e` | `0x6ee5e` |
| Ground helper tests property low nibble | `0x46fb21` | `0x6fb21` | `0x6ef21` |
| Conditional base and overlay calls | `0x46fd5f`, `0x46fdde` | `0x6fd5f`, `0x6fdde` | `0x6f15f`, `0x6f1de` |
| Another conditional path calls base then overlay together | `0x46d0fc`, `0x46d104` | `0x6d0fc`, `0x6d104` | `0x6c4fc`, `0x6c504` |
| Base wrapper calls `0x5cd8a0` | `0x413983` | `0x13983` | `0x12d83` |
| Overlay wrapper calls `0x5ce800` | `0x4169e4` | `0x169e4` | `0x15de4` |
| Overlay advances past the base payload | `0x5ce8ae` | `0x1ce8ae` | `0x1cdcae` |
| Width-318 overlay: local X minus 120; Y minus H plus 160 | `0x5ce93e`..`0x5ce960` | `0x1ce93e`..`0x1ce960` | `0x1cdd3e`..`0x1cdd60` |

`0x46ff60` contains more than an unconditional draw of every base. Its
first traversal selects graphics and height, including the owner-sensitive
monument branch, then continues into a second traversal with marker,
terrain and virtual-owner predicates. Separate helpers conditionally
dispatch Base/Overlay and sometimes dispatch both together. These
observations reject inferring a universal scene-ground role from the
Type-30 Base field. They do not specify the complete original per-pixel
compositor or establish that OpenEmperor should copy any one branch.

The alternate height path requires positive draw-mode state, a valid cell
owner, the monument-type predicate and embedded state through virtual
`+0x1ec`, before substituting `+0x28`. Another draw path at
`0x46d519`..`0x46d53e` applies a saved-cell times-40 adjustment only for a
positive value. These distinct branches do not authorize broadening the
current camera-zero saved signed-height policy.

The `cMonumentBldg` vtable at `0x7b887c` was re-read from the pinned PE;
relevant slots map `+0x104 → 0x568db0`, `+0x118 → 0x4290e0`,
`+0x120 → 0x4298c0` and `+0x1ec → 0x416b50`. Late owner calls therefore
cannot safely be replaced with a terrain-only inference. Remaining
context-specific component/state behavior is outside this fix.

### Evidence and preservation scope

Ignored `static-composition-evidence.json` retains 34 instruction anchors
with matching encoded bytes and proper VA/RVA/physical translations, plus
nine translated virtual slots. `recheck-static-evidence.py` independently
reverifies these without executing the original program.

`Handan-height-source-check.json` independently reads bounded original
container blocks, validates zlib lengths and retains field widths/logical
offsets: marker `(96,52)` at 1,001,227, Elevation `(95,49)` at 1,000,542,
Elevation `(95,50)` at 1,000,770, all signed bytes; ID 1 embedded raw height
at 1,094,050 is signed 32-bit. The report retains the exact map SHA-256.
Original post-load height normalization remains unresolved.

Fresh manifests retain 51 original source hashes, four additional used
Stone-10/Ruined SG3/555 hashes and 38 frozen HEAD files. All 51 match the
prior milestone's source baseline; all 38 frozen files matched HEAD when
recorded. Simulation, persistence, compatibility resources, readers,
selectors, dependency resolution and decoders are in that frozen set.
`WorldDrawOrder.h` is explicitly excluded because this milestone permits
composition changes there. Final before/after verification is separate
from the initial baseline.

Original material authority, source records/models, archive identity,
heights, buildability and logical occupancy remain unchanged. Original
post-load heights, exact historical-overlay eligibility, special Type-1
composition and general interpenetrating-sprite ordering remain open.
Improved pixels are not evidence of complete Emperor reproduction.

## Implemented composition contract

`RegeneratedMapRenderPlan` assigns the policy once while loading, solely
from the validated original model piece kind. Wall, Tower and Gate use
`SpatialCombined`; Road retains `EarlyBaseSpatialOverlay`. Other landscape
families retain the existing split convention. There are no map-name,
coordinate, record-ID, color, height or material-specific exceptions.

The structural early-base call is empty. Its spatial call draws the
already cached Combined texture once at the existing key; separate Base
and Overlay calls are omitted for that instance. Existing eager uploads,
physical deduplication and complete atomic readiness remain required.
Unknown policies fail readiness; failed members retain complete fallback.
The same renderer calls serve MapDebug and the Sandbox linear merge.
No comparator, general projection, marker, front cell, anchor or decoder
changed. Historical Snapshot keeps its old combined-image path.

Full Combined alpha owns visible structural body pixels; empty texels
permit a later hit on an earlier visible image. F8 applies to the whole
structural image, and culling uses the unchanged full-image rectangle.
F1 separates file Base/Overlay from policy/drawn components and retains
front key, marker, height source and anchor provenance.

Sandbox inspection records bounded dynamic descriptors in actual draw
order and compares the stored hit's key with the foreground dynamic hit.
It reuses eagerly loaded sprite pixels and the exact authored Well/Health
Post mesh triangles. Cargo markers have the owner's key. The inspected
frame retains its submitted camera and painter mode; profile/layer/order
changes invalidate descriptors until another frame is drawn. Landscape
and walker selection are view-owned, so a wall click is not reinterpreted
as occupancy behind it. Logical ground picking for construction is
unchanged, and UI input still blocks map inspection. Diagnostic text and
UI annotations are not general image masks.

A map press released over the panel/toolbar is consumed before visual
sampling. The production-event regression starts on the map, releases
over an opaque scene pixel covered by the panel, and verifies unchanged
selection and complete World snapshot; this guards against UI click-through.
Viewport/layout changes invalidate the submitted cache until redraw.
A frame with Help, budget warning or demolition confirmation also leaves
visual inspection invalid until an unobstructed frame is submitted. Thus
closing a panel or modal before redraw cannot expose alpha that was still
hidden by UI in the last submitted image.

### Independently authored pixel regressions

`scene-composition-production-pixels` uses independently constructed
Type-30 Base/Omega payloads: red structural body, green overlay and blue
rear/front landscape. Expected screen pixels are calculated separately
from the composition helper. The retained split policy reproduces the
old wrong blue body pixel; structural Combined produces red, while a
later foreground image still wins. Supported alpha-128 shadow samples
blend to red 127 exactly once. No original pixels enter the test fixture.

The test exercises actual MapDebug input and Sandbox render/input paths,
1×/2×/4× with reverse insertion, repeated zoom and identical frame hashes,
empty alpha holes, paid buildings behind/in front, an actually routed
moving walker and sprite/fallback cargo protrusions. F7 legacy order and
F1/F4/F7 frame invalidation use the submitted painter state. F8 hides and
restores the entire body. Adjacent side-4/4/2 structural claims exercise
wall/tower/gate-sized geometry; an authored complete 5×5 Pinnacle provides
rear/front countercases. Body-only viewport intersections include
fractional camera translations. These are composition fixtures, not new
original gate registration or original-game visual acceptance.

Mesh inspection reuses the original Well/Health Post triangles. An
independent raster oracle checks 345,011 opaque interior and 248,316 empty
interior samples for both families, side 1/2 and zoom 1×/2×/4×. Original
mesh constants, geometry and draw code remain byte-identical; only pure
hit helpers are added.

Twelve idle render/inspection frames retain all eight normal performance
counters at zero and the full World snapshot. A held new-road drag during
actual 4× simulation is compared with an ordinary reference World at the
same ticks. Preview remains coalesced and adds no commands, revision,
copies, files, decode/upload or route refresh; cancellation leaves both
Worlds equal. Ordinary simulation-tick navigation is outside the render
counters, rather than falsely reported as absent from simulation.

## Real-map renderer comparison

Fresh final captures use exactly the baseline source files, camera,
1280×720 software viewport, material choice and selected piece. Ignored
`Handan-final-trace.json` establishes the corrected A chain: at `(710,330)`
Terrain 229 and Elevation 208/355 draw before the wall's spatial Combined
write. Final RGBA is `(82,74,90,255)`; at `(752,348)` it is
`(90,82,90,255)`. Both select marker `(96,52)`. Visible B keeps its exact
`(57,49,66,255)` while now selecting piece 2 marker `(96,56)`.
The neighboring D pixel retains `(90,90,41,255)` and hit `(91,57)`.

A separate read-only decode/rectangle intersection confirms real
foreground countercases: Elevation 224 at `(100,52)`, key `(1900,1920)`,
intersects 250 opaque pixels of the first wall; the same record at
`(100,53)`, key `(1920,1880)`, intersects another 45. At screen
`(762,438)` the wall first writes `(222,189,132,255)`, then those two
foreground items write `(214,181,123,255)` and `(148,165,57,255)`.
The final hit is `(100,53)`. At `(760,439)` foreground also wins.
This is an actual opaque wall/landscape intersection, unlike the earlier
C control `(790,455)`, which is outside the first wall's rectangle.

All 24 six-map × Automatic/Stone/Earthen/Ruined runs retain exactly the
baseline assets, uploads, logical texture bytes, complete claims, height
checks and unique owned cells. Each mode/map captures an overview and
1×/2×/4× primary area in Regenerated and Snapshot modes. The wall-bearing
Stone views show connected supported bodies in Handan, Badaling, MPWall1
and MPWall2; MPWall1 includes the Pinnacle overview, and Chengdu supplies
a Rock control. No claim of original-game visual parity follows.

For the complete four-mode matrix, 192 captures were compared: all 144
unaffected controls are byte-identical (all Automatic, all Snapshot and
both zero-wall maps). The other 48 captures are the intended regenerated
wall-bearing changes. Earthen and Ruined retain their material and missing
piece/fallback boundaries; the phase-2 Road variant 40 remains unresolved.

### Measured static renderer cost

Each primary Regenerated view runs 300 render/inspect/pick frames. The
normal frame counters remain zero for files, asset decodes, texture uploads,
World copies/executes, BFS and route refresh. Static order builds remain 1.
The following are observed local software-renderer timings, not an
isolated benchmark or a native GPU speed claim.

| Stone primary map | Baseline texture draws | Final texture draws | Baseline ms/frame | Final ms/frame |
| --- | ---: | ---: | ---: | ---: |
| Badaling | 571 | 565 | 1.224 | 1.324 |
| MPWall1 | 612 | 605 | 1.182 | 1.315 |
| MPWall2 | 632 | 626 | 1.466 | 1.714 |
| Handan | 574 | 567 | 1.147 | 1.340 |
| Xia | 491 | 491 | 0.495 | 0.526 |
| Chengdu | 1152 | 1152 | 1.219 | 1.294 |

Structural Combined replaces two draws with one when an overlay exists;
it adds no new image, upload or alpha cache. Logical texture bytes remain
exactly the six baseline values above, at most 65,515,872 within 64 MiB.
These are RGBA budgets, not GPU resident-memory measurements. Sandbox
adds a retained bounded descriptor vector rather than a scene graph;
arm64 compiler layout measures 128 bytes per descriptor. Its requested
capacity is bounded by three times `(buildings + couriers + 2)`, at most
222 records/28,416 logical bytes for the existing 46/26 limits, excluding
allocator overhead. No complete-map pixel readback or new map scan runs
in inspection; the pixel traces are local opt-in programs only.

The final run includes the bounded software-backend compatibility below.
Observed CPU times increase despite fewer structural texture draws; the
measurements do not establish a performance improvement. A separate
single-process `/usr/bin/time -lp` pair over the same complete six-map
Stone workload measured baseline/final maximum RSS of
219,365,376/196,198,400 bytes and peak process footprint of
207,192,832/184,599,272 bytes. These are observational whole-process peaks,
not per-texture/GPU residency or an isolated memory benchmark. Logical
RGBA budgets and eager upload counts remain identical.

### Bounded SDL software compatibility

Repeated native software texture submissions exposed an independent
SDL 3.4.14 failure: an unchanged STATIC mixed-alpha wall draws at 1× and
the first 2× frame, then subsequent scaled copies stop writing pixels;
deferred execution reports `Parameter 'src' is invalid`. A standalone
SDL-only reproduction, without World, map planning or OpenEmperor's
comparator, reproduces this. The same wall created as STREAMING passes
all twelve sequential 1×/2×/4×/1× frames, translations and alpha-hole
checks while the opaque control stays STATIC.

The installed headers/runtime identify SDL 3.4.14, revision
`SDL-release-3.4.14-0-g147a8ee32`. Its
[official software-renderer source](https://raw.githubusercontent.com/libsdl-org/SDL/release-3.4.14/src/render/software/SDL_render_sw.c)
enables surface RLE for STATIC textures and temporarily changes source
blend state during a clipped scaled copy. RLE interaction is the bounded
diagnostic explanation; the reproduction proves the access-mode effect,
not complete causality inside SDL. No upstream implementation was copied.

Only eagerly loaded Combined textures used by admitted `SpatialCombined`
instances on the `software` renderer now use STREAMING access. They still
receive exactly one existing `SDL_UpdateTexture` during initialization,
with no per-frame upload. All other assets, Base/Overlay components and
native Metal textures retain STATIC access. Texture count, deduplication,
readiness, alpha, decoder and the existing byte budget are unchanged.

A separate pure SDL probe also reproduced an existing STATIC dynamic
sprite failure after fractional zoom 1× → 1.125× → 1×. This milestone does
not broaden the compatibility change to Building/Walker/Road renderers or
other stored components. Integer sequential zoom, structural culling at
fractional camera translations and native package acceptance are separate
checks; they do not claim every software-renderer fractional-zoom sequence
is repaired. Ignored probe sources and logs retain the reproduction.

## Final validation (2026-10-05)

All runs below use the final source, including submitted-viewport and
blocking-overlay inspection invalidation. Earlier interrupted runs and a
completed sanitizer run preceding those fixes are explicitly archived as
superseded and do not establish final acceptance.

| Configuration | Result | CTest wall time |
| --- | --- | ---: |
| Debug, native arm64, sanitizers off, 6 jobs | 93/93 passed | 487.51 s |
| Release, native arm64, 6 jobs | 93/93 passed | 28.48 s |
| RelWithDebInfo `-O2 -g`, arm64, ASan + UBSan, 4 jobs | 93/93 passed; zero skips/diagnostics | 169.66 s |
| Final macOS package Release build | 93/93 passed; resource/signature/relocation/ZIP/process-restart checks passed | recorded in ignored package log |

Sanitizers use frame pointers, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; this is not a leak
sanitizer claim. Suites include City-v10, both City-v16 rule versions,
determinism/endurance, save/load, recovery, road responsiveness/continuity,
asset/resources and scene composition. Independent ignored controls fail
when either layout invalidation or modal frame invalidation is removed,
at their corresponding UI-covered-pixel assertions.

Final before/after World review uses the frozen baseline executable and
the same authoritative commands/ticks: City-v10 rule 1 and City-v16 rules
1/2 each run 20,000 ticks. Four checkpoints per profile, at ticks
800/1,200/8,000/20,000, produce twelve byte-identical complete save documents;
every final restore equals its source World snapshot. The public City-v10
8,000-tick and City-v16 rule-2 1,200-tick check/resume JSONs also match
exactly after supplying both executables the same unchanged compatibility
resources. The initially relocated baseline executable had no adjacent
resource pack; that earlier fallback-profile report remains retained,
rather than being conflated with the matched-resource comparison.

Final hashes preserve all 51 original source files, four additional
original assets and 38 frozen authority files. The existing Well/Health
Post mesh/palette/draw source is byte-identical after removing only the
authorized pure-hit helpers. All files in the final Release/sanitizer directory fingerprints remain
unchanged; Debug records a separately enumerated 258-file source
fingerprint, including CMake support files. The pinned EXE's 34 encoded
instruction anchors and nine virtual slots were independently reverified
read-only. No original program was executed and no original source pixels
were added to tracked files.

### Native package acceptance and remaining boundary

The final ad-hoc-signed arm64 `0.1.0-alpha.2` package, dirty revision
`3b834c2bbc1c`, resides only in ignored
`.local/scene-composition/final-dist/`. Its validator passes relocation,
unzipping, bundled resource/dependency checks, original/synthetic ordinary
save/resume/process restart, and all thirteen negative cases. This is a
local test candidate, not a release/publication. No independent clean-Mac
or notarization acceptance is claimed.

**ACTUAL NATIVE INPUT:** the final packaged executable was started with
public `--data`/`--app-root`, an isolated test preference root, an Empty
city test preset and the Metal renderer hint. The normal New Sandbox UI
was used to select Handan and explicit Stone preview, then load it. The
connected supported wall run is visible in the ordinary Sandbox overview.
Keyboard pause/F1 and public zoom input were actually delivered and
observed. The public default remains City-v11 rule 3; no default or
personal preference was changed. Test settings and recovery histories
remain under the ignored isolated root.

**NATIVE POINTER ACCEPTANCE OPEN:** the current native automation's
requested clicks and drags do not establish the requested SDL positions:
the unmodified package's opt-in SDL event log records button coordinates
`(0,0)` and then stale `(781.715,699)` for requested wall pixels. Wheel and
brief arrow inputs likewise did not establish the target camera movement.
Fresh binding, explicit clicks, drag, focus/menu checks and a reset of the
automation session did not resolve this. Therefore no native mouse-hit,
target-area panning, or complete native 1×/2×/4× wall acceptance is marked
PASS. Physical-user pointer acceptance remains open; the running paused
Handan sandbox is available for that check. This input observation does
not establish whether the defect belongs to the automation or SDL.

The exact 1×/2×/4× body/picking, camera/culling, foreground, F8 and
busy-road acceptance above is **SCRIPTED PRODUCTION-RENDERER** or
**INDEPENDENTLY AUTHORED SYNTHETIC** evidence, distinct from native input.
No original-game side-by-side visual comparison was performed. The
Handan global-pass overwrite is corrected under the bounded OpenEmperor
contract; complete original compositor parity, interpenetrating whole
sprites, height normalization, historical-overlay eligibility and the
separate Ruined phase-2 Road variant 40 remain open.
