# Original map first-draw evidence

The current bounded study is [Pass 4 — restored Great Wall entities](#pass-4-restored-great-wall-research-and-implementation-boundary-2026-10-04). Its implementation is partial: real-map Great Wall activation remains zero because original restore material depends on unresolved external context. Earlier passes below retain their historical scope.

## Historical original map fidelity pass 1

This bounded study adds landscape provenance, separate Type-30 base/overlay rendering, and an evidenced height operand. It does **not** pass the complete landscape acceptance gate: Xia water has conspicuous cyan diamonds, stone patches still have abrupt seams, and some northern edge fragments remain unexplained. No record substitutions, terrain recoloring, inferred shore tables, arbitrary footprints, original executable execution, or gameplay changes are part of this pass.

The starting revision was `b7b85ddef737c4fc8db71afd2c0adeb461659a0b`, with a clean worktree. All captures, binary windows, review runners and original-data diagnoses are ignored under `.local/map-fidelity-pass1/`. This document contains observations and metadata only. No commit, release or package is implied.

## Evidence levels and scope

| Label | Meaning in this pass |
| --- | --- |
| Raw map fact | Unchanged bytes from the supported standalone map's decompressed logical ranges |
| EXE-observed behavior | Static instructions and call order in the pinned binary; no runtime execution |
| Reference-derived hypothesis | Existing candidate diamond and tentative low-bit part coordinates |
| OpenEmperor preview convention | Existing `edge-byte-4x4` grouping, full-image anchor, stable front-cell painter and preview layer classification |
| Verified reproduction | Signed height operand multiplied by 40, plus the already supported Type-30 base and Omega overlay decoding; not complete original scene placement |

The binary SHA-256 is `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. Addresses below are **virtual addresses**, with PE image base `0x400000`; subtract it for an RVA. Map offsets are decompressed logical offsets, not physical file offsets. Earlier graphics notes use RVAs and describe narrower historical studies.

## Load to first-draw dataflow

The most consequential result is that loaded saved IDs are cleared during post-load setup. The saved snapshot cannot be presented as an unchanged original first draw on this traced standalone-load path.

| Order | Observation |
| --- | --- |
| 1 | Map load at `0x43ad9e` invokes the read branch of serializer `0x52e7c0`. |
| 2 | Read call `0x52e9f1`, destination argument at `0x52e9ea`, fills `0xfe9880`: 51,984 dwords from logical 1,535. |
| 3 | The following reads fill candidate bytes `0xfdcd70`, terrain `0xf6a9e0`, objects `0xf37da0`, draw properties `0xf9d620`, and the separate stored variation-input bytes `0xf1e780`. |
| 4 | After the reads, `0x43ae05` calls `0x53d100`; `0x53d1e4` calls `0x5355f0`. |
| 5 | `0x535612` loads count `0xcb10`, `0x535617` zeroes EAX, `0x535619` selects `0xfe9880`, and **`0x53561e rep stosl` clears all 51,984 loaded IDs**. |
| 6 | Initialization and generation follow, including `0x466c80`, `0x4bc440`, `0x41f400`; subsequent `0x53d1ee` calls `0x53d630`, whose `0x53d689` invokes `0x5403c0(3)`. |
| 7 | `0x5403c0` iterates the candidate row-span tables. Its first pass calls `0x53eaa0`; its second calls selector chain `0x53ec90`. Further setup includes `0x4b67b0` and `0x5251d0`. |
| 8 | The map drawing entry `0x53c930` calls `0x46a220` at `0x53c944`. Per-cell readers such as `0x4700e0` and `0x46fa6f` see the runtime array after setup, not necessarily the loaded words. |

This supersedes the earlier claim that no relevant post-read clear had been found. The pre-read `0x4b08a0`/`0x4b3970` path remains a separate historical observation. Complete generation, all contextual branches and exact first-draw IDs for selected cells were not reproduced. Clearing does not prove that every regenerated numeric ID differs from its saved value.

The first generation pass checks terrain with mask `0x8c008`, conditionally clears a cell ID at `0x53eab9`/`0x53eacc`, and calls `0x53eae0`. The latter probes neighboring heights and has a resource-key `0x601` path at `0x53ec5d`. The second pass tries guarded selectors, including `0x53f240`, `0x540330`, existing-ID nonzero, water `0x53fc30`, vegetation `0x53f520`, rock `0x53f660`, and later ground selectors. These are evidence for regenerated terrain/elevation selection, not a complete implementation specification.

## Height source and projection

| Layer | Logical start | Bytes | Destination/read evidence |
| --- | ---: | ---: | --- |
| Draw properties | 677,327 | 51,984 | `0x52ea35` → `0xf9d620` |
| Separate variation input | 729,311 | 51,984 | `0x52ea46` → `0xf1e780` |
| Height-object header | 989,239 | 36 | `0x52eaba` |
| Height grid | **989,275** | **51,984** | `0x52eacb` → `0xbebf3a` |

The intervening serialized byte grids and small values were counted to establish the offset; their general meanings remain unknown. Getter `0x408cf0` reads `this+2+cell_index`, with `this=0xbebf38`. The getter chain `0x471e20 → 0x471e00 → 0x408cf0` sign-extends the byte, multiplies by five and shifts left three: **signed height × 40 pixels**. Normal map ground drawing repeats this at `0x4700f3`–`0x470103` and subtracts it from Y at `0x4701f9`. The overlay path reads height at `0x46d519` and has a positive-height Y adjustment at `0x46d532`–`0x46d53e`.

The current renderer therefore applies `projected_y - signed_saved_height*40` once. It never derives height from sprite dimensions, terrain bits or draw-properties bits. The draw-properties low bits are used as placement codes in `0x46b010`/`0x46b1c0`; they are a different array.

**Important limit:** `0x5403c0` also has a conditional normalization path controlled by `0x88ec38`; at `0x540511` it calls the height setter `0x474b10` to lower selected heights by one. Thus the saved height grid is a verified source and draw operand, but its complete post-initialization first-draw state is not reproduced. The preview uses the unchanged saved height. Complete original anchors and elevation topology remain unverified.

Candidate-cell saved height histograms:

| Map | h=0 | h=1 | h=2 | h=3 | h=4 | h=5 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Xia | 762 | 2,232 | 356 | 230 | 32 | 0 |
| Banpo | 2,683 | 3,060 | 419 | 161 | 61 | 0 |
| Chengdu | 14,204 | 118 | 199 | 86 | 13 | 0 |
| Anyi | 9,923 | 1,384 | 955 | 2,358 | 0 | 0 |
| Badaling | 7,810 | 2,760 | 2,625 | 953 | 424 | 48 |

The optional bounded layer reads apply only to supported standalone containers long enough to include the complete height grid. Multipart campaign and short historical/synthetic inputs retain the existing flat snapshot path. No height is added to World or save data.

## Original drawing and layer evidence

`0x46a220` uses the existing view-grid/camera state and calls `0x46ff60` for ground before later overlay/object passes, including `0x46cb40`, `0x46cb10`, `0x46e1b0` and a conditional `0x46a810` path. `0x53cae0` and `0x53cbd0` calculate bounds/camera state; they are not the actual draw entry.

Ground helper `0x46f9b0` tests terrain and the candidate byte's `0x40` bit (`0x46fa5e`), then reads `0xfe9880[cell]` at `0x46fa6f`. Normal ground path `0x46ff60` also reads it at `0x4700e0`. Base drawing `0x413960 → 0x5cd8a0` and overlay drawing `0x4169d0 → 0x5ce800` are distinct. The Type-30 overlay skips the base payload and adjusts X by 0/−40/−120 for widths 78/158/318, and Y by −image-height plus 40/80/160. The complete caller-dependent anchor remains open; these local adjustments do not establish the preview's whole-image anchor.

For width 78, Type-30 base code at `0x5ce4b8` reaches `0x44fec0`, which copies the opaque diamond. Other generic image paths have transparency/color-blend behavior; no observed instruction justified treating current water RGB555 `0x46fa` as a transparent key. It remains unchanged. The separate `0x42b640 → 0x5cf350` path is generic object/tree drawing, not an established special water renderer.

There is evidence for separate base and overlays, **not** for drawing an arbitrary additional grass tile under every saved graphic. The new implementation separates the supported image's own Type-30 base from its own Omega overlay. Some decoded base pixels already depict rock, vegetation or cliff-edge detail; “Ground only” is a component inspection mode, not a verified original ground selector.

## Water: bounded selector trace

Water `0x53fc30`, called by `0x53ec90` at `0x53ed0a`, is guarded by raw terrain bit `0x4` at `0x53fc38`. It gathers object context, neighbor state, stored variation input and a resource-group selection. The bound is this routine, its direct neighbor/table helpers and the draw-time update; no shoreline table is copied or reconstructed as guessed production logic.

`0x4b8f70`, called at `0x53fc92`, tests mask `0x4` or contextual `0x104` over eight adjacent terrain words. In row-major storage its observed order is N, NE, E, SE, S, SW, W, NW, with index deltas −228, −227, +1, +229, +228, +227, −1, −229. The booleans occupy `0x101c5b8` onward. `0x4bc140` counts them and `0x4bc100` inverts them. These are terrain neighbors, not sandbox roads or BFS.

At `0x53fcba`, `0x4bbdc0` receives table `0x848d00`, 46 rows and the byte from `0xf1e780`. The helper constructs both presence and absence bits, matches a row's requirements, and uses orientation and per-row variant information. This is more than selecting one blue record for each Water cell. Additional object-context and rotation branches occur for terrain bit `0x10000`.

Observed resource-manager keys include `0x605`, `0x61a`, `0x61b`, `0x61c`. They are **not physical SG3 indices or parser group IDs**. At `0x540055` the `0x605` base receives table-derived offsets from `0x101c538`/`0x101c540`. Several `0x61c` paths use the stored byte modulo 24 or 48, sometimes plus 48 or parity. `0x54007b` writes the selected complete ID into `0xfe9880[cell]`; `0x54008f` clears the draw-properties low nibble and `0x540095` sets candidate bit `0x40`.

The draw helper also updates IDs at `0x46fbe1` when draw-properties low bits are zero, terrain mask `0x104` is nonzero, and `0xadf824` permits the update: a `0x61c` range of 48 advances by two and a subsequent range of 24 advances by one, wrapping within each range. This is evidence of frame cycling. Exact phase and complete shoreline reconstruction remain open.

The local v213 group lookup places key `0x61c` at runtime local 463 / physical Terrain record 664. The reviewed water snapshot contains physical records around 635–705 and edge records around 360–426; that overlap alone does not identify the regenerated variant for any cell. No static replacement family, alpha rule or color correction was enabled. **Water topology/shoreline fidelity is unresolved and its primary visual acceptance fails.**

## Inspector, selection and rendering implementation

F1 shows storage coordinates, candidate membership, off-map bit, raw terrain/object/ID/byte, tentative parts, slot/local/physical/archive/group, complete relevant image metadata, saved height, draw properties, footprint/origin/anchor/ground position and painter tuple. Page Up/Down scroll the wrapped panel. Unresolved cells retain their exact status. Raw outside-candidate values remain inspectable through the pure provenance API; neither off-map nor candidate membership is silently repaired.

A cached overlay-alpha hit test identifies overhanging images before the bounded inverse-height ground picker. A selected tall image identifies its draw-marker/origin cell, while ordinary ground picking retains member-cell selection. The cache is built at load time. Actual UI coordinate clicks could not be reliably targeted in this native automation session; 21 original-data diagnoses were produced by scripted calls to the same provenance API, and pixel-hit behavior was exercised with synthetic SDL inputs. These are not 21 claimed manual native clicks. Optional P-to-file export was not added; inspection performs no file work.

F8 cycles Ground only, Ground + Water, + Elevation, + Decorations, Full stored snapshot. In Sandbox it is available only with F1 debug open. Ground includes all supported Type-30 base planes, including water bases. Water adds overlays associated with raw water bit `0x4`; Elevation adds slot-16 overlays; Decorations adds the remaining overlays. This classifier is an explicit preview convention. Full stored snapshot preserves the prior flat combined-image renderer.

The layered path renders bases first, then merges overlays with existing sandbox road/building/walker draws using the stable logical front-cell painter. It does not introduce new World entity kinds or claim the full original painter. Height moves the displayed ground and associated sandbox presentations; it does not change logical depth, occupancy, road selection, buildability, routes or saves. Existing opaque committed-road replacement and preview alpha paths remain intact. Synthetic pixel checks verify that a cliff overlay covers a building/walker behind it and is covered by one in front.

Each physical asset is read once, decoded/uploaded eagerly as combined, base and overlay images, and deduplicated across placed instances. Three RGBA textures count toward the unchanged 64 MiB aggregate limit; payload/image, asset-count and path-containment limits remain active. Cached alpha is one byte per pixel for combined and overlay images. Per-frame draw, cull, selection and inspection perform no reads, writes, decode, uploads, World copies/commands, BFS or route refresh. Viewport fit/culling includes raised full-image extents. Plan order is built once.

## Xia case diagnoses and footprints

`.local/map-fidelity-pass1/Xia-diagnoses.json` records three entries for each requested category, using production provenance and local 4× context captures. Some cells occur in more than one visual category. “Wrong saved graphic ID interpretation” means the **assumption that the saved ID already is the first-draw result** is false on the traced path; it does not accuse the physical resolver of an off-by-one error.

| Category | Storage cells | Physical records | Assigned causes |
| --- | --- | --- | --- |
| A Floating edge | (73,113), (75,111), (79,106) | Elevation 201, 229, 211 | missing height/elevation offset; wrong saved graphic ID interpretation; unknown original behavior |
| B Cliff pillar | (79,106), (81,104), (105,105) | Elevation 211, 344, 312 | missing height/elevation offset; wrong saved graphic ID interpretation; unknown original behavior |
| C Cliff wall | (77,108), (78,107), (106,104) | Elevation 349, 231, 353 | missing height/elevation offset; wrong saved graphic ID interpretation; unknown original behavior |
| D Water tile | (91,108), (91,107), (141,109) | Terrain 662, 648, 651 | wrong saved graphic ID interpretation; missing transition/variant selection; unknown original behavior |
| E Water edge | (109,110), (110,109), (109,109) | Terrain 385, 411, 417 | wrong saved graphic ID interpretation; missing transition/variant selection; unknown original behavior |
| F Beige ground | (111,112), (112,112), (108,97) | Terrain 429, 430, 430 | wrong saved graphic ID interpretation; missing transition/variant selection; unknown original behavior |
| G Rock/vegetation | (123,103), (105,94), (111,112) | Terrain 432, 439, 429 | wrong saved graphic ID interpretation; unknown original behavior |

No measured case establishes a wrong slot, physical-record translation, unsupported mirror or an arbitrary/interlocking-footprint requirement. Wrong anchors, original painter and border clipping remain questions, not falsely proven diagnoses. Heights explain an observed component of the A/B/C errors; they do not prove every affected record/variant or clip.

Actual Xia candidate-record histogram, grouped by slot / size flag / width / base bytes / parser group ID:

| Slot | Flag | Width | Base bytes | Group ID | Cells |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 16 | 0 | 78 | 3,200 | 1 | 2 |
| 16 | 1 | 78 | 3,200 | 1 | 263 |
| 16 | 1 | 78 | 3,200 | 2 | 89 |
| 16 | 2 | 158 | 12,800 | 1 | 20 |
| 16 | 2 | 158 | 12,800 | 3 | 4 |
| 3 | 1 | 78 | 3,200 | 1 | 2,313 |
| 3 | 1 | 78 | 3,200 | 3 | 921 |

No wider footprint support was introduced. Existing 1×1/2×2/4×4 diagnostic policies remain narrow. Tentative part bits and candidate boundary are unchanged. The observed `0x40` read/write strengthens its draw-related evidence without verifying the low-bit part coordinates or `0x80`. The original map border/background policy, tall-overlay clipping and any extra border tiles remain unknown. No blanket tall-edge clipping was applied.

## Before/after and acceptance

The baseline executable was captured before renderer changes. Xia captures include overview, central area, north border, cliff, water and stone regions at 1×/4×. Banpo, Chengdu, Anyi and Badaling have overview and targeted 1×/4× regions. Final captures also isolate base/water/elevation layers. The major Xia river was separately inspected at (148,119), rather than treating a small cliff pond as the river.

| Map | Snapshot cells decoded / candidates | Distinct physical assets decoded | Visual conclusion |
| --- | ---: | ---: | --- |
| Xia | 3,612 / 3,612 | 294 | Height makes many plateau/cliff connections more plausible; cyan water diamonds, beige seams and some border fragments persist. Full acceptance fails. |
| Banpo | 6,384 / 6,384 | 282 | Height-aware plateau presentation checked; ground/shore selection remains preview-only. |
| Chengdu | 14,620 / 14,620 | 341 | Predominantly flat map remains inspectable; sparse elevations use the same height operand. No original parity claim. |
| Anyi | 14,620 / 14,620 | 396 | Larger stepped terrain checked without map-specific renderer branches; cyan river transitions and some border fragments remain. |
| Badaling | 14,545 / 14,620 | 349 | Larger terrain and existing unsupported cases remain explicit; Great Wall parity is outside this pass. |

Coverage is not identity, placement or composition success. `landscape_fidelity_report` reports `coverage`, `resolved_identity`, `placement_verified`, `composition_verified` separately. Complete original-anchor, original first-draw identity and ground/water/elevation composition verification counts remain **zero**. Available height-transform cells are counted separately as partial placement evidence. The old `snapshot_complete` compatibility status is explicitly scoped to saved-snapshot coverage.

Native keyboard review checks F1, inspector scroll, F8 and the historical snapshot on Xia, and overview/Return cell selection on Banpo, Chengdu, Anyi and Badaling. An ignored prepared City-v16-v2 review scene at tick 4,200 was inspected natively: roads, buildings, Well 1559 and Herbalist 1580 remain aligned at 1×/2×/4×, and tick/funds/population stay unchanged while paused. This is visual review of a prepared production-renderer scene with actual keyboard input, not native gameplay acceptance or an original-game comparison. The 21 diagnoses and most map-region captures are read-only scripted/software-renderer checks.

## Validation and stopping point

Synthetic coverage includes provenance, outside-candidate/off-map distinction, signed negative height, layer separation, inverse picking, alpha hit selection, front/source ordering, culling, invalid metadata and actual SDL pixel occlusion behind/in front of cliff overlays. A repeated draw/inspect/hit sequence measures zero per-frame file reads/writes, decode, upload, World copies/commands, BFS and route refresh. Existing Road responsiveness and simulation suites remain required.

All full CTest suites passed: **Debug 83/83**, **Release 83/83**, **ASan/UBSan 83/83**, in 802.63 / 79.11 / 275.85 seconds respectively. These include City-v16 20k determinism and 100k endurance for both rule versions, operation/maintenance/geometry, persistence, Water/Health and road responsiveness checks. The long suites ran concurrently; elapsed times are not a benchmark comparison. The final Sandbox inspector wrapping adjustment was then rebuilt and checked with the affected view/stored-graphics/road tests in all three configurations. No sanitizer finding or new build warning was reported. `git diff --check` passes.

A separate ignored local runner uses the production Sandbox renderer and Metal, with the paid prepared City-v16-v2 scene paused at tick 4,200. It measures 180 frames with an active read-only road preview and repeated same-cell pointer motion, then 180 historical Snapshot frames on the same scene/camera. Its final World snapshot remains exact. All counters for World copies/restores/executes, route refresh, BFS, asset decodes, texture uploads, file reads/writes and simulation ticks are zero; the layered gesture builds **one** road plan.

| Measurement | Layered road preview median / p95 ms | Historical Snapshot median / p95 ms |
| --- | ---: | ---: |
| World render | 1.617 / 1.745 | 1.282 / 1.408 |
| HUD render | 0.814 / 0.911 | 0.879 / 0.980 |
| Present | 6.216 / 6.399 | 6.188 / 6.340 |
| Hover picking | 0.011 / 0.015 | 0.001 / 0.002 |

The single road-plan sample is 0.003 ms. This bounds observed responsiveness on this local scene/hardware, not every map or device. The extra base pass has measurable render cost; there is no claim of identical frame cost.

Original-data checks are separate: five-map raw identity/resolution/status arrays match the baseline exactly, including Badaling's existing 75 diagnostic cells. This preserves the inputs to the existing buildable-mask rule. The seven previously fingerprinted original inputs have unchanged hashes after review; supplementary map/Elevation hashes are recorded locally without claiming a missing before/after comparison. All 21 diagnosis captures and local reports remain ignored. No original file was written.

Pass 1 stops here. Remaining work is complete post-load selector/height-normalization context, static shoreline/corner selection, original caller anchors and boundary composition. Those unresolved behaviors are not repaired by increasing coverage, choosing attractive replacement records, masking colors or widening footprint rules. City-v16 rules/economy/maintenance/roads/Well/Herbalist/Health/fire/desirability/water/saves/couriers are unchanged.


## Original Map Fidelity Pass 3 — Mountains, Rocks, Walls and Great Wall (2026-10-04)

Pass 3 starts from `9e4684a52419056f53ec1996110b945193d7e130`. The initial HEAD and worktree were checked before edits. This is a bounded presentation/research change. Current Pass-2 Ground and Water selectors and the signed saved-height ×40 displacement are frozen, as are corrected sandbox roads, City rules, economy, maintenance, Well 1559, Herbalist 1580, Health, Fire, desirability, saves and couriers. Earlier Pass-1 visual diagnoses above are historical; they do not supersede the current Pass-2 Ground/Water baseline. No original executable was run and no original data, screenshots, full disassembly or decoded pixels are committed.

### Evidence labels

| Label | Meaning in Pass 3 |
| --- | --- |
| **RAW MAP FACT** | Read-only raw map bytes, coordinates and counts; SG3 record metadata is separately identified as archive metadata. A historical ID remains a historical ID. |
| **EXE-OBSERVED** | Static branches, resource keys, helper arguments, writers and draw operands in the hash-pinned EXE. This does not prove untraced runtime context. |
| **REFERENCE-DERIVED** | The existing candidate diamond and prior saved candidate-byte hypotheses; these retain their own evidence boundary. |
| **OPENEMPEROR PREVIEW** | Conservative eligibility, retained elevation/snapshot fallbacks, whole-image caller anchor and front-cell painter used by our renderer. |
| **VERIFIED REPRODUCTION** | Independently implemented bounded selector/table or ownership behavior established by the observed operands. Complete original first-draw identity, caller placement and scene composition are not implied. |

The pinned EXE SHA-256 remains `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. All addresses in this section are **VAs**, with image base `0x400000`. The ignored evidence directory is `.local/map-fidelity-pass3/`.

### RAW MAP FACT — classify mountains before selecting graphics

The independent production-reader census parsed all **167** supported standalone maps. Counts are over the unchanged candidate diamond only, not all 51,984 storage cells. Conditions overlap and are not a partition of the map.

| Raw condition | Candidate cells | Maps with condition |
| --- | ---: | ---: |
| `(terrain & 0xaffede6f)` equals `2`, `0x100002` or `0x200002` | 66,182 | 167 |
| `terrain & 0x02000000` | 2,225 | 22 |
| `terrain & 0x4000` | 1,320 | 25 |
| Historical packed ID has slot 8 | 12,416 | 18 |

The first row is the bounded Rock selector condition, not the older, broader reference-derived `rock_or_ore` color category. A raised cliff, a rock/ore cell, a packed rock formation, raw Pinnacle terrain, a decorative image and a special monument remain distinct. Height greater than zero never creates a Mountain.

| Map | Rock condition | Raw Pinnacle | Raw ordinary wall | Historical slot 8 |
| --- | ---: | ---: | ---: | ---: |
| Xia | 169 | 0 | 0 | 0 |
| Juchengshi | 415 | 100 | 0 | 0 |
| MP22 | 1,165 | 100 | 0 | 0 |
| MPWall1 | 411 | 25 | 0 | 736 |
| Badaling | 612 | 0 | 0 | 740 |
| Handan | 461 | 0 | 0 | 740 |

Twelve production provenance samples are retained in ignored `mountain-samples.json`: four Xia cliff/ordinary-rock cases, all four Juchengshi Pinnacle banks, and four MP22 Pinnacle cases, including saved heights 0, 1, 2 and 5. Each retains raw terrain/objects, signed height, variation, fertility, candidate byte, historical ID/slot/physical record and the selector result at sample time. The pre-change selector fields show the missing Pinnacle path; they are baseline diagnoses, not final renderer results. MPWall1 was selected independently from the corpus as the Great Wall countermap and also contains a genuine 25-cell Pinnacle. All 167 per-map counts and representative raw samples are in ignored `corpus.json`; final render acceptance is separate below.

### EXE-OBSERVED / VERIFIED REPRODUCTION — Rock packing

`0x53f660` selects ordinary Rock/ore through mask `0xaffede6f` and resource keys `0x606`, `0x607`, `0x608` for values `2`, `0x100002`, `0x200002`. The row-major generation pass at `0x54044f`–`0x540482`, the bounded rectangle test `0x4b8000` and rectangle writer `0x4b72b0` establish **3×3 before 2×2 before singleton**, with unique generated ownership. A matching rectangle rejects flood bit `0x100` and previously generated occupancy. The origin's separate variation byte at logical `729311+index` chooses variant `12+(v&1)` for side 3, `8+(v&3)` for side 2, or `v&7` for side 1. Neither saved graphic IDs nor their historical footprint partition choose the generated Rock geometry.

The independently written load-time packing retains a fresh bounded claim grid. It derives complete instances from raw terrain and variation in row-major order and applies the conservative candidate/elevation-preservation boundary. This reproduces the bounded second-pass packing behavior; **complete original first-pass occupancy is still unresolved**. In particular, a saved slot-16 elevation is preserved as a named preview boundary, not treated as proof that its saved ID survives the original clear.

All selected variants resolve through `ResourceGroupLookup` and `RuntimeArchiveLayout`; no physical record is hardcoded into a selector. Read-only archive metadata gives the complete 14-variant families in `China_Terrain.sg3`. All records below are static, unmirrored Type 30. Each family cell shows physical record / width×height / overlay bytes; the runtime local bases are 257, 271 and 285 respectively.

| Variant | Side | Base bytes | Key 0x606 | Key 0x607 | Key 0x608 |
| ---: | ---: | ---: | --- | --- | --- |
| 0 | 1 | 3,200 | 458 / 78×52 / 1887 | 472 / 78×51 / 2059 | 486 / 78×50 / 1816 |
| 1 | 1 | 3,200 | 459 / 78×54 / 1611 | 473 / 78×45 / 1740 | 487 / 78×55 / 1908 |
| 2 | 1 | 3,200 | 460 / 78×48 / 1221 | 474 / 78×46 / 1308 | 488 / 78×42 / 983 |
| 3 | 1 | 3,200 | 461 / 78×41 / 628 | 475 / 78×51 / 1088 | 489 / 78×48 / 1476 |
| 4 | 1 | 3,200 | 462 / 78×44 / 678 | 476 / 78×49 / 1311 | 490 / 78×51 / 1384 |
| 5 | 1 | 3,200 | 463 / 78×49 / 1321 | 477 / 78×51 / 1927 | 491 / 78×47 / 1286 |
| 6 | 1 | 3,200 | 464 / 78×43 / 943 | 478 / 78×46 / 1526 | 492 / 78×47 / 1262 |
| 7 | 1 | 3,200 | 465 / 78×46 / 1190 | 479 / 78×53 / 1450 | 493 / 78×49 / 1025 |
| 8 | 2 | 12,800 | 466 / 158×100 / 5057 | 480 / 158×95 / 6107 | 494 / 158×102 / 4281 |
| 9 | 2 | 12,800 | 467 / 158×95 / 5793 | 481 / 158×104 / 4299 | 495 / 158×99 / 4465 |
| 10 | 2 | 12,800 | 468 / 158×100 / 4674 | 482 / 158×96 / 6137 | 496 / 158×85 / 3405 |
| 11 | 2 | 12,800 | 469 / 158×100 / 6692 | 483 / 158×93 / 3760 | 497 / 158×94 / 4654 |
| 12 | 3 | 28,800 | 470 / 238×136 / 10129 | 484 / 238×139 / 8356 | 498 / 238×142 / 7033 |
| 13 | 3 | 28,800 | 471 / 238×151 / 9900 | 485 / 238×134 / 8515 | 499 / 238×150 / 14524 |

The generic writer sets draw-properties low bits to `side−1`, stores the selected complete ID on every owned cell, writes `dx|(dy<<3)` from its offset table, and adds candidate bit `0x40` on the view-dependent draw cell. In orientation zero this is part `(0,side−1)`; it is distinct from the diagonal front cell used by the OpenEmperor painter. This writer does not set `0x80`. Its generated byte semantics are **EXE-OBSERVED**; that does not retroactively promote every historical byte/grouping policy or a complete original anchor.

### EXE-OBSERVED / VERIFIED REPRODUCTION — Pinnacle

The later landscape selector `0x53fa20` tests raw terrain bit `0x02000000` and zero current generated ID. It selects an object-state bank in priority order `0x08`, `0x10`, `0x20`, `0x40`, defaulting to the first bank. There is no variation or height operand in this selection.

| Raw object bank | Resource key | Runtime local / physical Terrain record | Type-30 geometry | Base / overlay bytes |
| --- | --- | --- | --- | --- |
| `0x08`, or default | `0x60d` | 1239 / 1440 | side 5, 398×520 | 80,000 / 132,099 |
| `0x10` | `0x60a` | 1240 / 1441 | side 5, 398×544 | 80,000 / 136,733 |
| `0x20` | `0x609` | 1241 / 1442 | side 5, 398×468 | 80,000 / 115,506 |
| `0x40` | `0x619` | 1242 / 1443 | side 5, 398×475 | 80,000 / 116,963 |

All four groups contain one static unmirrored record. Origin helper `0x4b7bd0` reads draw-properties low nibble 1–5 as side 2–6 (otherwise side 1), walks left/up over candidate part fields, and bounds the recovered footprint. At `0x53fb13` the selector invokes `0x4b72b0` for the **5×5** footprint, producing draw-properties `(old&0xe0)|4`, generated part bytes and the orientation-zero marker at `(0,4)`.

The bounded implementation admits a complete 25-cell claim only with every terrain word exactly `0x02000000` or `0x02000080`, canonical low-nibble-4 properties, exact canonical part bytes with one marker, a consistent object bank, candidate containment and orientation zero. All 2,225 raw Pinnacle cells in the real census have terrain `0x02000080`. Mixed Water/vegetation/Rock/Wall/other terrain bits are conservatively rejected to preserve earlier selector precedence; this is a bounded admission, not proof of all possible original combinations. Missing, mixed, out-of-bounds or conflicting claims fail closed. Historical IDs, saved height, fertility and variation do not choose the bank or claim. This is a conservative subset of the observed ownership path, not permission to construct a Pinnacle from any 25 attractive cells. The unchanged signed saved-height ×40 shift applies once to the displayed image. Overlay helper `0x5ce904`–`0x5ce932` observes width-398 component adjustments X−160 and Y−image-height+200; the complete caller anchor remains **OPENEMPEROR PREVIEW**.

### EXE-OBSERVED / VERIFIED REPRODUCTION — ordinary wall terrain

Post-load `0x53d630` calls `0x4b67b0` at `0x53d68e` after the landscape pass. The routine scans raw `0x4000` wall terrain row-major, excluding flood bit `0x100` and road/building mask `0x48`. `0x4b8f70(0x4000)` collects **eight** terrain neighbors in order N, NE, E, SE, S, SW, W, NW. Additional `0x8000` gate context enters `0x4b6290`; it is not treated as a Great Wall slot or an ordinary road mask.

The 16-row semantic topology table at `0x84a8f8`, matched by `0x4bbdc0`, chooses key **`0x451`**. Each row has one variant and its cursor starts at −1, so the bounded static branch needs no saved variation byte. Rotation uses even original view/2. Helper `0x4bee90` alternates only straight variants 0/2 to 1/3 when an already generated cardinal neighbor has the same straight and no neighbor has its alternate. It reads the freshly cleared/regenerated ID grid, never the historical saved IDs. The cross row additionally requires four absent diagonal neighbors; an unmatched dense cross stays explicitly unresolved rather than inheriting a stale global table result.

The EXE slot-2 registration at `0x475c4b` supplies `China_General` to the same loader/runtime model. Key `0x451` resolves to General runtime local 720 / physical 921, with 18 static, unmirrored side-1 Type-30 records **921–938**, width 78, base 3,200 and heights 98–107. A separate component path after the ID write uses `0x4be3e0` and model-pool calls through `0x416a10`; it can add key `0x451+18+component` when the observed global context permits. The remaining group records **939–953** are Type 256 (widths 10–78, heights 19–44), not additional static wall base records. Their original model/component composition and `0x8000` gates remain unresolved and are not guessed.

The independent static selector audit classifies the corpus's 1,320 raw wall cells as **1,123 supported static selections** and **197 gate-context unresolved cells**. Three gate-free counterchecks are Luoyang Tang (75 cells at h0), NavalT-Jiangling (53 at h0), and NavalT-Yen (58 at h1). Their derived static IDs happen to agree with the historical words 75/75, 53/53 and 58/58; agreement is a countercheck, not input authority or original-game visual acceptance.

### RAW MAP FACT / archive metadata — complete Great Wall audit

Ordinary wall terrain is not Great Wall. Badaling's known `(84,61)` example has **terrain `0x88`, objects 0**, historical ID `0x20018`, saved height 4, draw-properties low bits 3 and candidate byte 0. It does not have normal wall bit `0x4000`. Its 16-member saved block `(84..87,61..64)` retains the known row pattern `00 01 02 03 / 08 09 0a 0b / 10 11 12 13 / 58 19 1a 1b`, marker candidate at `(84,64)`, and physical slot-8 record 225 (318×167, side 4, base 51,200, overlay 2,271). These remain saved-snapshot facts.

The archive has 242 reported records after dummy zero: system records 1–200, followed by **all 42** monument-group records 201–242. The old map-observed range 0–40 was not the complete group. The shared runtime layout still maps local `i` to physical `201+i` exactly once. Every record below has animation count 0, mirror offset 0 and an in-bounds internal source. The 8 Type-1 records decode as plain images but have **unsupported map footprint layout**; they are not evidence for eight side-1 Type-30 wall tiles. There are **zero Type-30 side-1 records, two side-2 records and 32 side-4 records**.

| Runtime local | Physical record | Layout / map footprint | Width×height | Base bytes | Overlay bytes |
| ---: | ---: | --- | --- | ---: | ---: |
| 0 | 201 | Type 30 / 4×4 | 318×160 | 51,200 | 2,305 |
| 1 | 202 | Type 30 / 4×4 | 318×169 | 51,200 | 2,487 |
| 2 | 203 | Type 30 / 4×4 | 318×212 | 51,200 | 22,660 |
| 3 | 204 | Type 30 / 4×4 | 318×160 | 51,200 | 2,946 |
| 4 | 205 | Type 30 / 4×4 | 318×167 | 51,200 | 2,185 |
| 5 | 206 | Type 30 / 4×4 | 318×167 | 51,200 | 2,894 |
| 6 | 207 | Type 30 / 4×4 | 318×182 | 51,200 | 2,716 |
| 7 | 208 | Type 30 / 4×4 | 318×165 | 51,200 | 9,530 |
| 8 | 209 | Type 30 / 4×4 | 318×167 | 51,200 | 3,747 |
| 9 | 210 | Type 30 / 4×4 | 318×185 | 51,200 | 1,823 |
| 10 | 211 | Type 30 / 4×4 | 318×168 | 51,200 | 2,576 |
| 11 | 212 | Type 30 / 4×4 | 318×160 | 51,200 | 2,471 |
| 12 | 213 | Type 30 / 4×4 | 318×213 | 51,200 | 24,011 |
| 13 | 214 | Type 30 / 4×4 | 318×167 | 51,200 | 2,378 |
| 14 | 215 | Type 30 / 4×4 | 318×160 | 51,200 | 2,953 |
| 15 | 216 | Type 30 / 4×4 | 318×182 | 51,200 | 2,553 |
| 16 | 217 | Type 30 / 4×4 | 318×167 | 51,200 | 3,397 |
| 17 | 218 | Type 30 / 4×4 | 318×166 | 51,200 | 9,165 |
| 18 | 219 | Type 30 / 4×4 | 318×185 | 51,200 | 1,780 |
| 19 | 220 | Type 30 / 4×4 | 318×167 | 51,200 | 3,843 |
| 20 | 221 | Type 30 / 4×4 | 318×200 | 51,200 | 6,281 |
| 21 | 222 | Type 30 / 4×4 | 318×160 | 51,200 | 0 |
| 22 | 223 | Type 30 / 4×4 | 318×162 | 51,200 | 1,533 |
| 23 | 224 | Type 30 / 4×4 | 318×160 | 51,200 | 1,429 |
| 24 | 225 | Type 30 / 4×4 | 318×167 | 51,200 | 2,271 |
| 25 | 226 | Type 30 / 4×4 | 318×167 | 51,200 | 2,090 |
| 26 | 227 | Type 30 / 4×4 | 318×160 | 51,200 | 0 |
| 27 | 228 | Type 30 / 4×4 | 318×160 | 51,200 | 0 |
| 28 | 229 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 29 | 230 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 30 | 231 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 31 | 232 | Type 30 / 2×2 | 158×80 | 12,800 | 0 |
| 32 | 233 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 33 | 234 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 34 | 235 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 35 | 236 | Type 30 / 2×2 | 158×80 | 12,800 | 0 |
| 36 | 237 | Type 30 / 4×4 | 318×160 | 51,200 | 6,795 |
| 37 | 238 | Type 30 / 4×4 | 318×204 | 51,200 | 22,048 |
| 38 | 239 | Type 30 / 4×4 | 318×160 | 51,200 | 6,930 |
| 39 | 240 | Type 30 / 4×4 | 318×203 | 51,200 | 21,336 |
| 40 | 241 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |
| 41 | 242 | Type 1 / unsupported map footprint | 51×51 | 5,202 | 0 |

All affected maps retain the historical slot-8 counts below. `corpus.json` also records every one of the 167 maps, including zeros and representative raw samples. MPWall1 is an independently selected countermap: 736 historical slot-8 cells, 411 Rock-condition cells and one genuine raw 25-cell Pinnacle. Badaling has 740 historical slot-8 cells, saved wall heights 2/3/4/5 in counts 324/224/176/16; MPWall1 has h4/h5 in counts 480/256. MPWall2 supplies lower saved-height cases: 704 historical slot-8 cells, h0=432 and h1=272. Across all 18 maps the histogram is h0=4,496, h1=896, h2=1,192, h3=1,120, h4=2,984, h5=1,696 and h6=32. These are saved-height facts, not measured regenerated Great Wall placement.

| Map | Historical slot-8 candidate cells |
| --- | ---: |
| `Badaling.map` | 740 |
| `Badaling_S.map` | 740 |
| `Handan.map` | 740 |
| `Handan_S.map` | 740 |
| `Jiayuguan.map` | 516 |
| `Jiayuguan_S.map` | 516 |
| `Juyongguan.map` | 708 |
| `Juyongguan_S.map` | 708 |
| `Liangzhou.map` | 688 |
| `Liangzhou_S.map` | 688 |
| `MPWall1.map` | 736 |
| `MPWall1_S.map` | 736 |
| `MPWall2.map` | 704 |
| `MPWall2_S.map` | 704 |
| `MPWall3.map` | 672 |
| `MPWall3_S.map` | 672 |
| `Shanhaiguan.map` | 704 |
| `Shanhaiguan_S.map` | 704 |

### EXE-OBSERVED — Great Wall producer and exact unresolved boundary

The slot-8 packed-ID producer is an **original entity/piece-state path**, not the ordinary landscape Rock/Wall topology. `0x57bba0` looks up an original entity via `0x8c7634 → 0x47f1b0`, gets extended state through virtual `+0x1ec`, and reads entity coordinates at `+0x0a/+0x0c`. Entity extended type/stage `+0x08`, material/archive state `+0x5c`, piece orientation `+0x18`, piece index `+0x1c`, extended orientation byte `+0x84`, and current view `0x101d0d0` participate. They are not supplied by `objects_raw` alone.

For the observed Earthen Great Wall material branch, `0x57be4d` selects the name at `0x82b100`, registers it at slot 8 through `0x57c049 → 0x5ccf70`, then `0x57c053` calls piece/view mapper `0x57c0e0`. `0x57c06b` requests **resource group `0x1001`**, `0x57c075` calls `0x408170`, and `0x57c07a` adds the mapped piece variant to that packed group base. In view zero, the mapper returns other piece indices unchanged but remaps piece 26→25 and 27→24; view 2/4/6 has additional explicit remapping branches. Related helpers `0x57cb10`, `0x57d2b0` and the single-cell writer `0x5724e0` cover smaller/special pieces. The group is not forced into a Road-like 16-mask system, and straight/corner/T/gate/tower/stair/elevation/end identities are not assigned from appearance.

The exact post-load boundary is `0x53d1e4 → 0x5355f0` (saved-ID clear), **`0x53d1e9 → 0x52f030`** (original entity restoration), then `0x53d1ee → 0x53d630` (landscape setup). `0x52f030` recreates active original entities through `0x42d540` and calls `0x4b11f0`; that routine dispatches entity virtual `+0x100` at `0x4b1228`. The missing link is the supported map serialization and restored entity/piece state leading from that virtual placement to the Great Wall producer and its footprint writer. Raw `0x88`, a saved slot-8 word and similar low-byte patterns cannot replace that state.

The generic rectangle writer's generated part fields and marker are observed, but the **specific restored Great Wall call and caller anchor are not yet established**. Width-318 overlay arithmetic at `0x5ce93e`–`0x5ce954` gives X−120 and Y−image-height+160 only inside the component helper. It does not prove `project(origin)−(width/2,image_height−160)` as the whole image anchor. `0x80` remains unassigned. Further, original monument-owned ground drawing may substitute extended-state height `+0x28` at `0x470158`–`0x470164` for the saved-cell height before its ×40 displacement; `0x57bba0` also has a context-dependent height-write path. Neither is enabled: Pass 3 preserves the existing saved signed-height ×40 presentation and does not normalize or flatten terrain.

Therefore **Great Wall selector verification = 0; regenerated Great Wall instances = 0**. The existing saved 4×4 grouping, marker interpretation, whole-image anchor and painter remain **OPENEMPEROR PREVIEW**. This is an explicit bounded evidence gap, not a successful Great Wall regeneration claim. The old complete group metadata/registration remains valid and the historical preview remains useful.

### OPENEMPEROR PREVIEW — plan, renderer and diagnostics

`LandscapeFamily` separates Ground, Water, Decoration, Rock, Mountain, Wall, GreatWall and Preserved. SDL-free `LandscapeInstances`, `PinnacleSelector` and `WallTopology` build a separate `RegeneratedMapRenderPlan` once at load. Each admitted generated instance retains family/resource, resolved physical asset, origin, side, owned cells, draw cell, and separate placement/composition evidence. New Rock side-1/2/3 and Pinnacle side-5 instances do not inherit saved-ID footprint size or origin. Claims are complete, candidate-contained, nonoverlapping and atomic; unsupported geometry, mirror/animation, source, budget or decode failures retain an explicit historical/diagnostic fallback. A large image is never emitted once per member cell.

Historical raw cells, saved footprint lists, snapshot diagnostics and the buildable mask retain their meanings. Physical assets are deduplicated across old and regenerated plans and eagerly decoded/uploaded with combined/base/overlay RGBA counted against the unchanged 64 MiB budget. The regenerated-only side-3/5 support does not broaden the legacy saved 1×1/2×2/4×4 grouping policies. Original/current images use their own Type-30 base and Omega overlay; there is no new terrain fill or alpha/color-key repair.

F8 exposes Ground, +Water, +Elevation, +Mountains/Rocks, +Walls/Monuments, +Decorations, Regenerated Full, and Historical Snapshot. Partial modes and Regenerated Full use the load-time selected geometries with explicit fallbacks; Historical Snapshot retains the old saved-ID path. F1 distinguishes selector evidence, selected resource/variant/asset, generated origin/owned cells/draw cell, saved provenance, height, anchor and painter. Ground/front-cell order merges overlays with sandbox roads, buildings and walkers; it is a preview painter, not a verified complete original object compositor. Fit/culling and cached alpha picking include full raised tall-image bounds.

`landscape_fidelity_report.regeneration` separately reports `mountain_selector_verified` (cells), `rock_large_instances`, `pinnacle_instances`, `normal_wall_selector_verified` (cells), `great_wall_selector_verified`, `great_wall_instances`, and unresolved Mountain/Wall cells. Asset availability and successful instance decoding are required for renderer counts. Great Wall counters stay zero. Coverage, static selector identity, complete placement and complete composition remain distinct: complete original caller-anchor and first-draw composition verification remain zero.

Frame render/inspect/pick performs no file work, decode/upload, World copies/commands, BFS or route refresh; generation and sorting are load-time work. No gameplay, road selection, City rule/version, save-schema or original-data mutation is authorized by this plan.

### Pass-3 validation and stopping point

The Pass-3 implementation and real-map evidence below are separate from the earlier Pass-1 results and the static census. After the optional-General-bitmap fallback correction, all three final suites passed **87/87: Debug in 642.25 s, Release in 145.41 s, and ASan+UBSan in 729.43 s**. The suite includes independent Rock/Pinnacle/Wall selection and ownership cases, atomic renderer fallback and truthful F1/report counters, plus the existing City-v16 rule-1/rule-2 20,000-tick determinism and 100,000-tick endurance, road, save, autosave, maintenance, Health, Fire and Water checks. A missing or unusable optional `China_General.555` must leave ordinary-wall renderer activation unavailable and retain the named historical/diagnostic path; successful metadata lookup or a decoded member alone must not count an inactive instance as reproduced.

Before captures from baseline `9e4684a` and final captures use the native Metal renderer for **Xia, Juchengshi, MP22, MPWall1, Badaling, Luoyang Tang and NavalT-Yen**, with the same focused views at **1×, 2× and 4×**. Automatic capture runners provide the repeatable comparison; they are distinct from actual desktop input. The final decoded renderer reports record the following bounded static-selector results. Mountain cells include ordinary Rock and admitted Pinnacle ownership; large Rock and Pinnacle columns count complete instances, not member-cell draws.

| Map | Mountain selector cells | Large Rock instances | Pinnacle instances | Ordinary Wall selector cells | Unresolved Wall cells | Combined/base/overlay RGBA bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Xia | 169 | 10 | 0 | 0 | 0 | 18,840,384 |
| Juchengshi | 515 | 54 | 4 | 0 | 0 | 29,703,120 |
| MP22 | 1,265 | 100 | 4 | 0 | 0 | 32,713,776 |
| MPWall1 | 436 | 21 | 1 | 0 | 736 | 42,222,456 |
| Badaling | 612 | 36 | 0 | 0 | 740 | 35,150,496 |
| Luoyang Tang | 353 | 26 | 0 | 75 | 0 | 22,382,760 |
| NavalT-Yen | 189 | 2 | 0 | 58 | 0 | 20,905,896 |

All seven reports have zero unresolved Mountain cells, zero regenerated Great Wall instances/verified Great Wall selections, and zero complete-original-anchor/full-original-composition verification. The MPWall1 and Badaling unresolved Wall counts retain their historical slot-8 cells; these are not ordinary-wall rejection counts. The tallest 5×5 Pinnacles are present as one image per complete claim, while Ground/Water, signed saved-height ×40 and Historical Snapshot retain their existing semantics. Decoded coverage and these selector counts do not establish original first-draw fidelity.

Actual native CUA Return/Tab/F8 input reviewed Juchengshi Pinnacles at 1×/2×/4× and Historical Snapshot, MP22 Rock and Pinnacles at 1×/2×/4×, Xia Rock at 1×/2×/4×, Luoyang Tang ordinary walls at 1×/2×/4×, and Badaling/MPWall1 historical Great Wall previews at 1×/2×/4×. This native capture session writes local review images, so its counters are **not** evidence for a file-free frame path. The Badaling/MPWall1 previews retain visible gaps and diagnostic ground; this is not Great Wall acceptance. Its restored piece state and anchors are unresolved, as are ordinary-wall gates and additional Type-256/model composition.

The separate pure Metal benchmark runs **300 frames in each of Regenerated Full and Historical Snapshot on all seven maps** (4,200 frames), including render, F1 provenance, alpha hit testing and Present. File reads/writes, decode/upload, World copies/restores/commands, BFS and route refreshes are all zero during those loops; each renderer builds its sorted order once at load. Mean frame time including Present/vsync is **8.32–10.35 ms**. The maximum shared combined/base/overlay RGBA allocation is **42,222,456 bytes**, below the unchanged 64 MiB limit. A separate paused City-v16 synthetic road-hover/drag check runs 180 regenerated frames and 180 Historical Snapshot frames: the same forbidden-work counters and simulation ticks are zero, the repeated hover/drag coalesces to one road plan, and the World compares equal before/after. World-render medians are **2.290 ms** and **1.430 ms** respectively; this is synthetic responsiveness/purity evidence, not native user-input acceptance.

A separate comparison confirms **21 frozen authority/profile/visual files are byte-identical to `9e4684a`**, including World/City sources, projection/scene authority, the compatibility pack and building/road visuals. SHA-256 records for **18 original files**—the pinned EXE, model text, manual, four SG3/.555 archive pairs and seven reviewed maps—match before and after the **final verification phase**. Seven of those files also match the retained historical Pass-1 hash baseline. The 18-file set was not recorded at the beginning of Pass 3, so it is not claimed as an 18-file whole-pass before/after audit. Original-data captures, reports, runners and hashes remain ignored local research artifacts.

The bounded stopping point remains: complete original first-pass occupancy/height normalization, caller anchors, ordinary-wall gates/additional model composition, and restored Great Wall entity/piece-state regeneration are unresolved. Existing Ground/Water/Road/City authority stays frozen. No commit, push, tag, release or publication was performed.

## Pass 4: restored Great Wall research and implementation boundary (2026-10-04)

Pass 4 is separately authorized presentation work after `6d43eb671558a083ccdf22c38d2890b123218ebc`. The frozen Ground/Water result, signed saved-height ×40, corrected roads, City/World/save authority and historical snapshot retain their meanings. The preceding Pass-3 counts and stopping point remain historical, not a report of the new pass.

**RAW MAP FACT and EXE-OBSERVED:** the bounded `OriginalMapEntities` reader recovers the original polymorphic building manager at logical 1,093,607, including typed original IDs, exact signed local coordinates, monument type/subindex and source ranges for phase/height/raw material/orientation. [Map-format evidence](map-format.md#pass-4-serialized-original-entities-and-great-wall-state-2026-10-04) distinguishes embedded `cMonInfo` from external `SubBuildingInfo`. [Selector evidence](graphics-id.md#pass-4-great-wall-model-and-selector-evidence-2026-10-04) proves the type-to-Model registration, controller sizes, directional values, phase/material archive selection, special tower remapping and inherited gate/road registration. MPWall1 and Handan use Model 04, Badaling Model 05, MPWall2 Model 07. Their active records contain 53/53/53/51 pieces respectively, with per-kind phases preserved rather than relabeled from the desired appearance.

The source path is now concrete: original restore `0x52f030 → 0x42d540 → 0x4b11f0` dispatches monument virtual placement `+100 = 0x56a0d0`; that reaches `0x563fd0 → 0x563670 → 0x5635a0`, resolves the external Model piece, and invokes its controller producer. The generic writer claims the wall/tower/gate's derived side around the exact restored coordinates. Road pieces instead write one exact entity cell through `0x57d9e3 → 0x5724e0`, return flags `0x08` and skip the generic writer. This closes the earlier serialization-to-producer evidence gap; a saved slot-8 ID or raw `0x88` is still not its replacement.

The actual phase-10 archives also close a misleading asset assumption: their selected walls/towers, gates and roads are all Type 30 under explicit derived material 2/3. Their only Type-1 variants 36–39 are not selected by these four map states. The complete Earthen-phase-1 group audit remains correct for that earlier archive, but cannot justify substituting its short scaffolds or rejecting phase-10 gates/roads. Registration stays contextual and all physical selections go through existing `ResourceGroupLookup`/`RuntimeArchiveLayout`, with no second physical-ID or slot table.

**Unresolved original restore context:** embedded deserialization overwrites raw material `+5c` through `0x563720(-1)`. It consults mode `0x88ec38` and current-player mission goals; these are not established by the bounded standalone map bytes. `GreatWallRestoreContext` therefore requires a separately evidenced derived material. Neither raw saved material 3 nor an assumed editor mode can authorize the finished stone archive. Missing context is named and retains the historical/diagnostic path, with regenerated Great Wall activation/counts unavailable. Pure selector success under an explicitly supplied synthetic context is not real-map renderer acceptance.

`GreatWallModels` reads only required local model files during preparation, validates all admitted tokens and complete row counts, bounds the payload to 64 KiB and resolves file containment before opening. `GreatWallSelector` is SDL-free, does no files or asset work, and distinguishes the optional required registration from inherited gate/road slots. Synthetic tests cover phase/archive changes, camera/saved-orientation combination, tower 26/27 remapping, exact gate parts, both road branches, missing context, invalid domains and escaping model symlinks. Original rows and bytes are absent from synthetic fixtures. The actual 04/05/07 files were independently accepted by the same local parser with 53/53/51 pieces and 22/23/22 phase rows; that verifies the dependency parser, not original material authority.

Complete original height normalization and full first-draw composition remain separate. Ordinary map ground drawing uses signed cell height ×40 in the normal global-height branch; the alternative `cMonInfo +28` height substitution and the producer's mode-dependent height writes do not justify flattening or guessing a vertical wall offset. The camera-zero static Type-30 caller chain is now traced: wall/tower/gate marker `origin+(0,side−1)`, signed marker height, and full-image origin equivalent to front-cell anchor `(40×side−1,H−20)`. Render and alpha picking share that origin, apply ×40 once and use front-cell depth separately. This closes that bounded anchor question; it does not reproduce original post-load heights or the complete compositor. Rock/Pinnacle first-pass ownership, ordinary-wall gates and unrelated model composition retain their earlier limits. No gameplay or original-data modification is part of this bounded pass.

### Pass-4 final serialization and reconstruction results

The actual starting HEAD was `6d43eb671558a083ccdf22c38d2890b123218ebc`, with a clean worktree. A supported standalone map reads manager schema 1/count 4,000 at decompressed logical **1,093,607**. The implemented base schemas are 3/4/5, Monument wrapper 1 with extended schemas 9/10, and the bounded Building/Monument/Fill/Industrial/Ferry classes observed in the reviewed files. Original IDs are separately typed and never enter `World`. Source ranges, widths and signedness are retained for F1; no file pointer or runtime object layout is dereferenced.

Badaling manager entry 1 is original object ID 1, type 257, subindex 0, local `(55,32)` → storage origin `(84,61)`. Its record starts at logical 1,093,808, base at 1,093,827 and extended state at 1,094,010. Signed X/Y come from base +8/+10; extended phase/material/height/orientation come from +6/+87/+38/+123. Its raw state is phase 10, saved material 3, height 4 and orientation 0. Model 05 supplies the piece/controller state independently of the saved graphic word. **The next required original step is unavailable:** deserialization `0x562e2b..0x562e44 → 0x563720(-1)` replaces material using mode `0x88ec38`, or current-player mission-goal queries `0x55f5d0/0x55f5e0` (goal type 2/value 85→2, 86→3; default→1). The standalone loader/header path does not establish those goals or the mode. No raw-material, map-name or editor-mode assumption fills that dependency.

The bounded pure pipeline supports Models 04/05/07 (types 256/257/259), derived materials 1/2/3, wall phases 1–10, tower phases 1–11, gate phase 1 and road phases 1/2, with explicit kind/domain checks. The selector covers evidenced even views; publication geometry is restricted to the existing camera-zero view. Phase-zero material override, Type-1 composition and unrelated monument models remain unsupported. Every complete claim precedes Rock packing; a metadata/decode/composition failure activates no partial ownership and retains complete historical images. Dynamic slot/path registrations are captured in restore order, while physical assets deduplicate independently. The EXE archive spelling `Greatwall` and supplied disk spelling `GreatWall` currently rely on the reviewed Mac's case-insensitive volume; case-sensitive roots can produce a named missing-file fallback. No case-insensitive archive search was added.

### Pass-4 rendering counts and real-map acceptance

Each reviewed Great Wall piece is a separately serialized original object. “Descriptor cells” below are source/model geometry used for diagnostics, **not active renderer claims**. MPWall1/MPWall2 have four singleton road pieces outside their historical slot-8 areas.

| Map | Parsed manager records | Active saved Great Wall objects/pieces | Descriptor cells | Historical slot-8 cells | Renderer-active objects / instances / cells | Great Wall acceptance |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| Badaling | 4,000 | 53 | 740 | 740 | 0 / 0 / 0 | FAILED: restore context unavailable; historical preview remains |
| MPWall1 | 4,000 | 53 | 740 | 736 | 0 / 0 / 0 | FAILED: restore context unavailable; historical preview remains |
| MPWall2 | 4,000 | 51 | 708 | 704 | 0 / 0 / 0 | FAILED: restore context unavailable; historical preview remains |
| Handan | 4,000 | 53 | 740 | 740 | 0 / 0 / 0 | FAILED: restore context unavailable; historical preview remains |
| Xia | 4,000 | 0 | 0 | 0 | 0 / 0 / 0 | No-Great-Wall regression capture unchanged |
| Chengdu | 4,000 | 0 | 0 | 0 | 0 / 0 / 0 | Parser countercase; no Pass-4 visual acceptance claimed |

Normal session loading and the explicit CLI both prepare this source/model state automatically. Missing derived material publishes no new Great Wall assets or claims. F1 and reports separate manager records, saved objects, Model pieces, source descriptors, actual decoded renderer activation, historical IDs and the exact fallback. A successful synthetic explicit-context example reaches group `0x1001`, complete side-4/side-2 instances and shared textures, including saved-ID mutation independence; it cannot authorize these real maps. **The requested first real-map vertical rendering case and visible complete-wall milestone have not been achieved.**

### Pass-4 validation, regression and package

Synthetic parser/selector/claim tests cover bounded/truncated records, references and domains, signed fields, no-wall maps, model symlink containment, archive transitions, inherited registration order, view/tower remapping, complete ownership, conflicts, source-ID independence and Type-1 rejection. Renderer tests additionally verify complete composition rollback, shared physical textures, actual pixel/picking activation and height displacement once.

The full suites each pass **91/91**: Debug **671.62 s**, Release **64.06 s**, ASan+UBSan **747.87 s**, with no sanitizer diagnostics. Final integration fixes were then rebuilt and the nine affected map/lookup/render tests rerun: Debug **9/9 in 7.28 s**, ASan+UBSan **9/9 in 6.68 s**. The final Release suite already contains those fixes. The full suite retains City-v16 rule-1/rule-2 20,000-tick determinism, 100,000-tick endurance with 46 buildings/26 couriers and 20 save/autosave/recovery roundtrips, plus City-v10, road responsiveness and existing Health/Fire/Water/maintenance checks. These are synthetic/scripted tests, not native gameplay acceptance.

**Scripted captures:** the baseline and final same-Release Metal runners use identical cameras at 1×/2×/4× on Badaling, MPWall1, MPWall2, Handan and Xia. All **38/38 PNGs are byte-identical**, including focused Rock/Pinnacle/Great-Wall views and Xia water. This confirms preserved output in those views, including the existing incomplete wall fallback. **Native input:** actual CUA Return/Tab actions review all four Great Wall maps at 1×/2×/4×, plus Badaling F8 Historical Snapshot. The local SDL review app uses the production session/renderer; it is not a main-menu playthrough of the packaged app. The wall views still show old scaffolds/gaps and, in mountain cases, uniform diagnostic regions. No original-game countercapture exists and no original-parity or connected-wall PASS is claimed. Native review writes captures and supplies no frame-purity evidence.

**Release performance:** a separate sequential baseline/final Metal benchmark runs 300 frames per mode (Regenerated Full and Historical Snapshot) on each of five maps: **3,000 frames per build**, with render, F1 inspection, alpha hit testing and Present. Frame file reads/writes, decode/upload, World copies/commands, BFS and route refreshes are all zero; sorted order builds once at load. The unchanged road-responsiveness tests also enforce the zero-work preview contract. Times include Present/vsync, so they do not measure uncapped renderer throughput. Load times include session preparation and eager texture initialization; this is one local sequential sample, not a statistical speedup claim.

| Map | Load before → after (ms) | Regenerated frame before → after (ms) | Snapshot frame before → after (ms) | Shared RGBA bytes (unchanged) |
| --- | --- | --- | --- | ---: |
| Badaling | 207.21 → 214.95 | 8.315 → 8.315 | 8.334 → 8.331 | 35,150,496 |
| MPWall1 | 272.00 → 252.95 | 8.315 → 8.315 | 8.332 → 8.333 | 42,222,456 |
| MPWall2 | 263.16 → 244.11 | 8.318 → 8.318 | 8.329 → 8.332 | 33,461,184 |
| Handan | 217.80 → 225.81 | 8.316 → 8.316 | 8.334 → 8.330 | 38,419,704 |
| Xia | 187.69 → 188.26 | 8.312 → 8.312 | 8.332 → 8.334 | 18,840,384 |

Whole-process maximum RSS from `/usr/bin/time -l` is **233,652,224 → 227,573,760 bytes** across the five-map runs. This noisy process peak includes SDL/driver allocations and is not a per-map memory saving claim. All shared combined/base/overlay textures remain below the existing **64 MiB**, with no asset-budget increase.

**Frozen authority/original data:** the before/after manifest audit verifies **51/51 original files unchanged** and **28/29 frozen files byte-identical to starting HEAD**. The sole intended exception is `LandscapeInstances.cpp`'s optional origin extension; its complete 2,695-byte Rock derivation prefix remains identical. The manifest includes the World/City/save/projection/compatibility/building/road authority files, not every file under the original root. Original hashes, diagnostics, binary windows, review runners and captures remain ignored under `.local/map-fidelity-pass4/`; no original fixture or pixel was added to tracked files.

**Local package:** `tools/package_macos.sh` creates a separate Release arm64 `dist/OpenEmperor.app` and `OpenEmperor-0.1.0-alpha.2-6d43eb671558-macos-arm64.zip`, explicitly marked dirty/local candidate in `BuildInfo.json`. Recursive Mach-O, staged dependencies, ad-hoc signature, relocated/unzipped execution, metadata/compatibility allowlists and bundled dependency notices pass. No original game files, pixels, repository tree or personal histories are staged. This is not Developer-ID/notarization/Gatekeeper or independent-clean-Mac acceptance. The existing absence of a project LICENSE is unchanged; nothing was published.

The pass stops at the bounded partial implementation. No commit, push, tag, release or new gameplay was performed. The exact remaining dependency is original restore mode/current-player mission goals determining material; full post-load height normalization and whole-scene composition remain separately open. The EXE is a research source only and is not a runtime dependency.


## Great Wall restore-context activation follow-up (2026-10-04)

The new [bounded context and activation report](great-wall-restore-context.md) rechecks the pinned original decision, explains the higher-level editor/mission inputs, and records normal-app explicit-preview activation. It starts at clean `91891af8bba00e85223a80773a110edf9f1f7ad0`; the Pass-4 zero-activation results above remain historical. Automatic still has no proven original context. Explicit material previews reuse the existing piece/claim/Type-30 pipeline and have independent active counters; complete original context and scene composition remain open.
