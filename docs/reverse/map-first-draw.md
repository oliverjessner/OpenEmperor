# Original map fidelity pass 1

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
