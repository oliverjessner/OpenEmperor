# Original farm bodies and the field-state boundary

OE60 restores two original static farm bodies through the existing building
profile and productive `SandboxView`. **Overall status: PARTIAL.** Hemp Farm 192
and Farmhouse 193 have qualified body publishers; Hemp 194, Wheat 195 and Millet 196
fields retain named authored fallbacks. Their original state cannot be inferred
from our authored 80-productive-tick harvest progress.

No production, crop, quantity, placement, worker, movement, fire, limit, rule,
schema or default changes. Rule18/schema34 remains recommended; historical
Rules1–23/schemas1–39 keep their contracts. Original files remain user-supplied
and read-only. The resource contains metadata, never original pixels or binaries.

## Evidence and classification

The fresh audit uses the same pinned installation as the
[presentation source contract](../reverse/original-presentation-correction.md):

| Input | SHA-256 |
|---|---|
| Original EXE | `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e` |
| Building model | `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6` |
| General.sg3 | `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46` |
| General.555 | `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2` |
| Cities/Xia.map | `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433` |

`MODEL_OBSERVED` covers labels and raw Cost/Employees/Resource columns.
`EXE_OBSERVED` covers the bounded static factory and publication paths, without
executing the original game. `SOURCE_VERIFIED` covers physical archive metadata
and shared-decoder output. The front-cell placement convention, paid field
ownership and harvest clock remain `AUTHORED`. Missing runtime state and original
animation cadence remain `UNKNOWN`; selecting native crops without that state
is `UNSUPPORTED`.

| Model | Role | Group | Physical base | Side | Native dimensions | Rendering |
|---|---|---|---|---|---|---|
| 192 | Hemp Farm | `0x4ad` | 825 | 2 | 158×137 | Static native body |
| 193 | Farmhouse | `0x43a` | 793 | 3 | 238×192 | Static native body |
| 194 | Hemp Field | `0x431` | 2458 | 1 | 78×41 | Authored fallback; original state unavailable |
| 195 | Wheat Field | `0x42e` | 2422 | 1 | 78×43 | Authored fallback; original state unavailable |
| 196 | Millet Field | `0x42c` | 2410 | 1 | 78×43 | Authored fallback; original state unavailable |

The field physical bases identify audited source records, **not** a valid active
World selection. Adjacent existing identities197 Rice,198 Cabbage,199 Soybean,
202 Irrigation Ditch and203 Irrigation Pump are also audited in the source
contract. None gains gameplay admission or native presentation here.

## Construction and operation

The concrete factories are 192→`0x4c5ca0`/vtable `0x7af6e0` and
193→`0x4c5b80`/vtable `0x7af1e8`. Both `+0x100` callbacks reach `0x4c5bf0`, which
calls their `+0x264` body writer `0x4c2ac0`. That writer resolves the model's
group, adds its definition `+20` value (zero for both farms) and publishes its
registered square side through `0x4b72b0`. It does not branch on staffing, stock,
field state or authored production progress. Our renderer retains this static
body while authored production, pause, cargo and fire evolve independently.
The proof qualifies the placement callback and later calls of the same body
callback; every original refresh path and active composite remains unresolved.

Both bodies are complete unmirrored native Type30 records with base bytes 12800
and 28800. Their anchors (79,117) and (119,172) use the existing
`[40*side−1, height−20]` front-cell convention. These are `AUTHORED` geometric
anchors, not recovered original runtime pivots. Animation metadata 14/18 does
not authorize a frame clock; no crop or farm animation is invented.

Fields have a separate purchase/publication path. Their writers derive one of
six quantity-dependent records through callback `0x4c1290` (the original field's
signed own-info word `+0x04` passed through `0x408ba0` with 10000), then add six
only when bit 0 of its raw per-cell byte at `0xf1e780` is set. Separate
calendar/work state is also observed; its complete
relationship to that quantity is unresolved. The crop table selects different
groups for Hemp, Wheat and Millet. Our
World owns a paid field ID and farm-level harvest progress, but does not own
that original per-field state. Neither a model label, a source pixel nor
the existence of twelve adjacent records supplies it. Construction and active
crop reproduction remain partial; native field selection fails closed.

## Shared implementation and compatibility

Schema2 of `original-buildings.json` contains fifteen explicit rows: twelve
native static bodies and three fallback-only fields. The original schema1
ten-body format remains accepted exactly. Enum values and the historical core
and House tables retain their existing identities. Fallback-only rows cannot
smuggle archive, image, group, variant or anchor fields, and unsupported fields
cannot opt into native selection.

Preparation uses `ResourceGroupLookup`, `RuntimeArchiveLayout`, the shared
SG3/555 loader and the existing deduplicated `BuildingSprite` pool. All source
fingerprints, containment, geometry, alpha, budgets and complete rows validate
before atomic publication. Missing or incompatible sources keep the existing
fallback; a failed replacement preserves the active profile and World.

The complete profile uses 39 physical images and 4,490,336 RGBA bytes within the
unchanged 64-MiB building budget. The shared Well remains deduplicated. Native
rendering, culling, preview, alpha picking and inspection use the same prepared
entry and authoritative signed-height footprint. Frames and hover do no file
reading, parsing, decoding, upload, World copy, command, tick or navigation work.
F4 retains the historical authored comparison. No visual state enters a save.

## Verification

Use the current source, an explicit original-data root and fresh private output
paths as described in [testing](../testing.md). The permanent paid-building
View test retains its immutable earned 66-body/46-courier Rule18 city. That city
contains Hemp and Millet fields; a separate fresh paid Rule18 exercise must buy
Wheat normally rather than modify the historical reference.

Fresh Release, Debug and ASan/UBSan builds use the same frozen runtime source;
all 80 simulation/persistence files remain byte-identical to baseline
`c3837ed8f838f82ad460282c1f5c53a1a8f4a4c0`. Full Release CTest passes
320/320 without skips; focused Debug and ASan/UBSan each pass 97/97 without
skips. The four focused building smoke tests also pass. Historical, culture,
paid-flow and bounded rendering/performance checks below are complete.

The additional paid Xia/Normal Rule18 exercise buys a Food Farmhouse, its Wheat
field and twelve worker Houses through normal commands. Its three active farms
cover all models 192–196 without changing the earned historical city. Staffing
produces positive progress at tick 121, harvest and loaded cargo at 200, and
actual Farm-goods arrival at 334. Wheat Farm pause/resume uses normal commands
at 500/520; a natural Farm fire occurs at 2000. Static bodies remain native,
and fields remain authored, throughout these changes.

The final engine, a freshly built previous engine and an independent final
restore process each record ticks 0–3200. Their complete authority-digest CSVs
and literal gameplay timelines are byte-equal. Seventeen checkpoint labels
cover fifteen distinct ticks; all full SaveDocuments and F5/F9 roundtrips match.
There are 35 JSON files per process, including the working save. The independent
physical audit checks all 54 save files (18 per process), rather than claiming
54 different World states. At tick 3200, Funds are
`12000 − 2419 construction − 702 upkeep = 8879`, with zero tax receipts.
Hemp conservation is `30 = 27 stock + 3 cargo`; Wheat is
`29 = 8 stock + 18 consumed + 3 cargo`; Millet is
`29 = 12 stock + 17 consumed`. This finite deliberately unprotected QA city
does not establish citywide stability, profit or original production parity.

The Rule18 Golden City comparison passes all 99 protected reference artifacts:
93 complete streams, four recovery payloads and two bounded metadata records,
including seven checkpoint pairs. Optional full-tick Golden digests are off;
the separate Farm exercise supplies its own 3201-tick digest comparison.

All 152 historical reference artifacts remain byte-identical: 123 complete
SaveDocuments, 25 legacy DTOs and four index files, covering Rules1–23 and
schemas1–39. The old-schema exporter accounts for 132 artifacts; separate
create/restore processes account for the twenty schema35–39 documents. The
aggregate retains the exact expected path set and compares whole bytes, rather
than comparing selected JSON fields. Rule22 and Rule23 each pass six independent
creator/restore processes at offsets 0, 103 and 20400, preserving their complete
venue, performer, House-service and save histories.

The permanent earned-city test covers 126 size/zoom cases and 147 owned-target
readbacks, writing 35 PNGs per invocation. Its twelve native-body categories
include the two farms; its two actual field kinds remain authored. The fresh
paid Farm exercise adds 45 targeted cases across all five models, plus F4 and
seven milestone captures. Another 27 actual selected-field captures use normal
panel-wheel input to show the complete rendered visual-role and fallback reason
at every size/zoom combination. These close the Inspector evidence gap left by
the ordinary building-list captures. Six additional paired performance PNGs
show the previous and final engines at 1280×800, 1×/2×/4×. New capture contents
were individually opened; byte-identical aliases are transferred only with
matching SHA-256 and file pins. Images remain private under `.local/oe60/`.
The final inventory has 462 PNG paths and 252 distinct SHA-256 contents:
449 render captures, ten decoded source previews and three authored icon copies.
There are 155 actual current-path openings; all 459 capture/preview paths are
covered by an opening or an exact-SHA review transfer, with no unreviewed content.
Icons are classified separately rather than counted as gameplay frames.
At high zoom the finite map aperture can crop a tall body; this is not proof
of the original scene's runtime pivot or whole-scene layering. Existing long
footer clipping in some small menu captures remains outside this change.

The paired software/dummy SDL measurement uses the same paid tick600 save and
nine exact camera/aperture pairs. Each case has 20 warm frames and 120 ordinary
render/Present samples. Real pointer motion and `update(0)` are inside the
instrumented counter batch but outside the render timer; diagnostic readbacks
are outside that batch. All twelve counters remain zero in both versions:
file reads/writes, asset decodes, texture uploads, World copies/executes/restores,
simulation ticks, BFS calls/visited cells, road plans and route refreshes. Full
World and F5/F9 bytes remain exact. The measurement does not run 3201 simulation
steps; those belong to the separate paid-flow exercise above.

| Window | Zoom | Previous mean ms | Final mean ms | Previous p95 ms | Final p95 ms |
|---|---|---|---|---|---|
| 1280×800 | 1× | 1.863 | 1.829 | 2.205 | 1.869 |
| 1280×800 | 2× | 6.839 | 7.108 | 7.828 | 7.461 |
| 1280×800 | 4× | 10.796 | 12.013 | 12.362 | 17.518 |
| 1600×900 | 1× | 2.397 | 2.411 | 2.767 | 2.468 |
| 1600×900 | 2× | 9.918 | 9.634 | 11.446 | 10.161 |
| 1600×900 | 4× | 12.881 | 14.334 | 13.491 | 20.311 |
| 1920×1080 | 1× | 3.634 | 3.393 | 4.314 | 3.669 |
| 1920×1080 | 2× | 15.520 | 14.933 | 21.329 | 16.635 |
| 1920×1080 | 4× | 17.105 | 18.828 | 17.736 | 19.793 |

These are one finite local run, with p95 at sorted index `floor(.95*(120−1))`.
The 4× mean cost rises by about 10–11%; several other means and tails vary in
either direction. This is not a universal speed gain or 60-fps/frame-budget
acceptance. Session preflight/projection/copy/initialize/restore time is
647.605→540.138 ms, excluding an independently reloaded merged pool; it is not
an isolated asset-upload benchmark. The eagerly prepared RGBA increase is
269,368 bytes, with no raised budget or per-frame upload workaround.

Private source decisions, source bindings, guards, literal comparisons and
image-review ledgers are retained under `.local/oe60/`. The paired measurement
report is `farm-paired-measurement-result-01.json`, SHA-256
`875d03f26d0786510f8da08ceccafd41871479882c53285f7362d1a4b52e85b3`.
Original sources and protected earlier evidence remain unchanged; no original
pixels, binaries, private saves or research dumps enter Git or app resources.

Native desktop delivery and human playtest acceptance are **NOT_RUN**. Static
body reproduction does not establish original animation, runtime pivots,
seasonal agriculture, original binary-save graphics or whole-scene parity.
