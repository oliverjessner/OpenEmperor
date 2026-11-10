# Roads, Well and Herbalist: original presentation correction

2026-10-03; starting HEAD `4b8d4c4e4ef3f58ae2ca3398a04917ebfcf69319`, clean worktree. Read-only research used the user's local Emperor files. Only observations, addresses, identifiers and metadata are retained here. Original files, disassembly, decoded pixels, atlases, screenshots and review programs remain ignored under `.local/presentation-correction/`. No original executable was run and no proprietary implementation was copied into production code.

## Sources and limits

The EXE is PE32/i386, image base `0x400000`, SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. General/Terrain SG3 and `.555` hashes equal the existing six-file compatibility manifest. `Model/EmperorBuildingModels.txt` SHA-256 is `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`; the local original manual is `b785835967acafeca73162bc21e351ed8bb0ac5fcbe96fab405d8b3fb5174fad`.

The local Model file names Roads **22**, Well **72**, Inspector's Tower **124**, Watchtower **127**, Herbalist's Stall **207**, Acupuncturist's Clinic **208**. It has **no footprint column**: its `SZE` heading describes a desirability step size. The [public Model transcription](https://www.scribd.com/document/506136435/EmperorBuildingModels) likewise does not itself establish footprint. The 1×1/2×2 conclusions below come from the linked original registrations and supported SG3 foundation metadata, rather than interpreting an unrelated Model field as geometry. The manual's Safety Ministry icons (p.95) and descriptions (p.97) distinguish the Well, Herbalist, Acupuncturist, Inspector and Watchtower; the Herbalist is specifically described as a roaming medical service. This supports choosing its artwork for our simplified HealthPost, without importing original health rules.

## Registration evidence

The static archive setup at VA `0x475c2c` (RVA `0x75c2c`, file `0x7502c`) names General at slot **2** and Terrain at slot **3**, using the same graphics manager. The group resolver is VA `0x408170`. Its index-table reduction is consistent with the already studied system-record skip and stable positive-index order; use one-based retained group positions. These group keys are **not physical image indices**, nor saved map words.

The generic model graphic reader at VA `0x414830` (file `0x13c30`) reads the first word of the 24-byte model definition at `0x8235a0 + 24*model_id`, then uses that resolver. The linked definitions are:

| Model / identity | Definition VA / file offset | Group key | Slot / retained group position | Physical record |
| --- | --- | --- | --- | ---: |
| 22 Roads | `0x8237b0` / `0x421db0` | `0x61e` | Terrain 3 / 30 | 782 |
| 72 Well | `0x823c60` / `0x422260` | `0x482` | General 2 / 130 | 1559 |
| 207 Herbalist | `0x824908` / `0x422f08` | `0x484` | General 2 / 132 | 1580 |
| 208 Acupuncturist | `0x824920` / `0x422f20` | `0x485` | General 2 / 133 | 1593 |
| 124 Inspector | `0x824140` | `0x486` | General 2 / 134 | 1618 |
| 127 Watchtower | `0x824188` | `0x488` | General 2 / 136 | 1680 |

Other words in these model-definition records remain uninterpreted; they are not a newly guessed footprint specification. Coincidental references to `0x482` as structure offsets, and a similarly valued unrelated string table, were excluded.

## Safety: complete strict 2×2 set

The previous Well 255-candidate and HealthPost 205-candidate audits were **incorrectly bounded for original building identity**: their one-cell restriction excluded every actual Safety base. Their negative results remain historical, and still explain why no original 2×2 sprite can fit the legacy authored 1×1 rules. They do not show that Emperor lacks these buildings.

Group 8 `China_Safety.bmp` contains physical records 1538–1727. Filtering the entire group for supported unmirrored Type-30, side 2, width 158, base 12,800 bytes, and valid source bounds yields exactly **eight bases**. Every one decoded through the ordinary shared production loader. The complete local atlas exists at 1× and nearest-neighbor 4×, with physical/group/type/base/overlay/mirror/animation labels.

| Physical | Whole dimensions | Overlay bytes | Animation sprites / offsets / speed ID | Observation / identity confidence |
| ---: | --- | ---: | --- | --- |
| 1538 | 158×93 | 1,833 | 20 / (45,21) / 4 | Dragon fountain; strong improved/fancy Well candidate, adjacent registered key `0x483`; exact evolution transition not reconstructed |
| **1559** | **158×132** | **6,508** | 20 / (50,69) / 5 | **Base Well**, masonry basin, pottery/tree/plant surrounds; named model 72 registration, Safety membership, motif and manual agree: high confidence |
| **1580** | **158×136** | **12,873** | 12 / (18,45) / 4 | **Herbalist's Stall**, roofed treatment stall, jars, work table/stool and herbs; named model 207 registration plus motif and manual: high confidence |
| 1593 | 158×137 | 13,145 | 24 / (8,42) / 4 | Acupuncturist's Clinic; model 208 registration, treatment platform/table and distinct medical motif |
| 1618 | 158×193 | 16,862 | 30 / (26,17) / 4 | Base Inspector's Tower; model 124 registration |
| 1649 | 158×194 | 18,958 | 30 / (27,16) / 4 | Related ornate Inspector tower candidate, registered key `0x487`; stage transition unverified |
| 1680 | 158×187 | 13,986 | 23 / (21,-16) / 5 | Base Watchtower; model 127 registration |
| 1704 | 158×198 | 21,481 | 23 / (25,-24) / 5 | Related improved Watchtower candidate, key `0x489`; stage transition unverified |

All eight have mirror offset zero, no alpha payload, supported internal color/base/overlay ranges and a 158×80 foundation. Animation metadata is reported, **not used as an inferred frame association**: this milestone draws complete base images, with no new building animation or Well evolution.

The built-in Well uses **1559 / [79,112]**. HealthPost uses **1580 / [79,116]**, rather than Clinic 1593, because the Herbalist analogy matches our road visitor. These are ordinary geometric anchors `[79,height−20]` relative to the existing front-cell ground, not recovered original pivot values. No crop, stretch, optical offset or mirrored image is used. Both remain OpenEmperor-authored services, costs and timing.

## Roads: old mapping retired, family independently confirmed

The previous **782–799 mask assignment is retired/rejected**. It used a curve as a straight, shared that wrong graphic for ends, assigned a four-way image to isolation and a T-shaped image to a crossing. Its visual-only audit and absence of positive saved-map correlation did not justify original parity. That old mapping is never selected by the corrected built-in profile.

New **positive static evidence confirms the physical family**, rather than requiring a different family just because the old mapping failed. Roads model 22 registers key `0x61e`, resolving to base physical **782**. The concrete road refresh branch at VA `0x4b710c` (RVA `0xb710c`, file `0xb650c`) chooses the 17-row table at **VA `0x84aef0` / file `0x4494f0`**, then that same base group and a selected offset. Adjacent branches select upgraded road group `0x61f` (physical 800) or other special terrain graphics; those do not replace the basic road family.

The table has four orientation columns and eight neighbor conditions per row. Its cardinal order is `−228,+1,+228,−1` in the 228-wide storage grid, agreeing with our `neg_y,pos_x,pos_y,neg_x`. Diagonal conditions in this basic road table are don't-care. The mask compiler at VA `0x4bc090` and selection at VA `0x4bbdc0` corroborate this interpretation; the neighbor reader at VA `0x4b8f70` confirms offsets. OpenEmperor uses the independently interpreted **orientation-zero metadata choices**; it does not port the original matcher. The other columns permute physical offsets rather than requesting image mirroring. Original style/terrain-transition and rotated-camera behavior remain outside this fixed-orientation preview.

| Mask | Topology | Offset from 782 | Final physical |
| --- | --- | ---: | ---: |
| 0x0 | isolated | 12 | 794 |
| 0x1 | end neg_y | 8 | 790 |
| 0x2 | end pos_x | 9 | 791 |
| 0x3 | corner neg_y + pos_x | 4 | 786 |
| 0x4 | end pos_y | 10 | 792 |
| 0x5 | straight y | 0 | 782 |
| 0x6 | corner pos_x + pos_y | 5 | 787 |
| 0x7 | T except neg_x | 13 | 795 |
| 0x8 | end neg_x | 11 | 793 |
| 0x9 | corner neg_y + neg_x | 7 | 789 |
| 0xa | straight x | 1 | 783 |
| 0xb | T except pos_y | 16 | 798 |
| 0xc | corner pos_y + neg_x | 6 | 788 |
| 0xd | T except pos_x | 15 | 797 |
| 0xe | T except neg_y | 14 | 796 |
| 0xf | cross | 17 | 799 |

This supplies **16 distinct physical images**, with genuine ends. Records 784/785 are unselected straight alternatives; no random variation was added. Every chosen record is supported, unmirrored, static, Type-30 side 1, 78×40, base 3,200; all decode normally. Anchor `[39,20]` uses source geometry, with no stretching to 80. `RoadNeighborMask` stays roads-only; buildings remain a separate diagnostic `EntranceMask`. No BFS, topology, road cost, batch command or save semantics changed.

The wider search examined **875** matching static 78×40 foundations across all plausible Terrain groups: land1 97, Overlay 19, Land3 324, QuarryTileSet 154, land2 281. All decoded and were assembled into 12 local atlas pages. Other dirt, rock, numbered test, quarry and paved families were considered; only the Model/road-branch-correlated basic family was selected. The old Xia saved graphic-word observations remain negative map correlation, and are not rewritten as positive evidence. The static refresh path provides the new evidence independently.

## Acceptance and compatibility

New City-v16 Worlds use **rule 2**; only Well and HealthPost change from 1×1 to 2×2. Schema **18** retains the exact existing fields and stores either version. City-v16 rule 1 and City-v14/15 retain their original one-cell semantics, with no migration, upgrade or reflow. A schema-1 optional `footprint_side` of 1 or 2 constrains Well/HealthPost records; absence preserves the old strict one-cell contract. First decode and deduplicated reuse enforce the declared Emperor foundation. Active rules filter mismatched built-in entries; mismatched explicit custom profiles fail visibly before texture publication. All configured entries load eagerly and share physical images/textures, while preserving independent anchors.

The new Xia scene was built by normal paid commands and simulation ticks: Well at tick 800, staffed Herbalist after tick 1200, then an ordinary fifth supplied House and a fresh dry House. At tick 4200 it has simultaneous effective stages 0/1/2, original Well/Herbalist, Market, Farm, FireWatch, ServicePost and industry. **Road acceptance FAILED / superseded by real user playtest:** the earlier normal F1-off Metal captures at 1×/2×/4× were incorrectly accepted as connected. The real City-v16 playtest exposes visible grass strips between adjacent roads. The asset registration and Well/Herbalist findings remain valid; road continuity requires a separate correction. The exact six-file pack supplies **13 roles / 12 textures**, with **zero Well/HealthPost fallbacks** and zero road fallbacks. Unknown packs and legacy rules retain authored fallbacks; in rule 2 their mesh scale/center derives from the active footprint and the same front-cell ground.

A separate ignored runner used the real road profile, shared `RoadSpriteSet`, paid synthetic World roads and ordinary road-mask queries for all 16 neighborhoods and long/L/T/cross/5×5 compositions at 1×/2×/4×. No selected mask fell back; source pixels were never transformed beyond ordinary nearest-neighbor scene zoom. Synthetic committed tests cover the same loader/geometry/render contracts without original bytes. Actual native keyboard zoom review is distinguished from scripted scene construction/captures and automated all-cell picking; broader mouse-driven play acceptance is not inferred from it.

Validation commands and final results are recorded in [City-v16](../city-v16.md#presentation-correction-validation). No original pixels or local review tools enter the package or repository.


Final regression: Debug, Release and ASan/UBSan **82/82 each**, including both generations' 20k/100k scenarios. Packaging passed its independent Release 82/82 and relocated/signed resource checks. Final-bundle native F1/Z review loaded the paid rule-2 scene and displayed the recognized pack and original sprites. Existing native rule-1 saves at ticks 399/400/7842 retain exact snapshots and one-cell authority. All seven source-file SHA-256 values were rechecked unchanged. Detailed timing, package hash and native/synthetic distinctions are in the linked City-v16 validation record.

## Road continuity follow-up

2026-10-03; starting HEAD `6346c30e32aaa5ce606c97fafd7a8cf2ab2999a0`, initially clean. The connected-road acceptance above is **FAILED / superseded by real user playtest**. Well 1559, Herbalist 1580 and City-v16 rule-2 2×2 authority are frozen. This follow-up adds only road presentation and read-only research. All seven previously recorded source fingerprints were rechecked unchanged. Original disassembly, payloads, decoded pixels, contact reports, variants and captures remain under ignored `.local/road-continuity/`.

### Four causes distinguished

| Hypothesis | Evidence and result |
|---|---|
| A: mask/orientation selection | All 16 configured records decode. The 782/783 curb motifs follow y/x respectively in the fixed projection. Four original-column mappings were rendered on the same eight-cell roads: 0/2 match these straight axes, 1/3 make transverse curb bands. All columns have complete bases and isolated contacts; swapping columns cannot cure grass overpainting. Keep column 0. |
| B: anchor/projection | Original integer map setup and raster geometry corroborate 80-wide rows and 20-high half rows, not a 39-wide storage step. Our 40/20 diagonal steps and `[39,20]` anchor give zero transparent gap in all 256 reciprocal pair tests at 1×. Fractional screen origins separately caused tiny nearest-sampling cracks at integer magnification in the software renderer; integer alignment of the whole replacement raster removes them. No width/anchor/global projection change. |
| C: extra underlay/composition | Every basic record already has all 1,600 opaque base pixels and its beige paving core. No independent road-base/underlay record was identified. The actual defect is retaining the old grass **and** drawing a complete road for the same cell. Replacing the old singleton graphic removes the disconnecting grass strips. |
| D: original modification of selected graphics | Refresh stores a selected graphic ID, and a bounded post-selection branch can replace it with group `0x623` after neighboring raw-terrain comparisons. This is a different selected cell graphic, not edge-pixel synthesis or an additional base draw. Optional Omega detail belongs to that selected image. Special terrain transitions remain unimplemented; no evidence supports stretching or generated edges. |

### Refresh, map draw and projection observations

The previous phrase “road draw at `0x4b710c`” was imprecise: this is a **refresh/selection/store** path. Call order is style classification at `0x4b70d7` → `0x4bdef0`; style 1 table selection at `0x4b7113` → `0x4bbdc0` using table `0x84aef0`; neighbor normalization at `0x4b711b` → `0x4bc120`; group resolution at `0x4b712a` → `0x408170` for `0x61e`; selected offsets added; **one cell graphic written at `0x4b7145` to the `0xfe9880` array**. The matcher publishes selected offsets; `0x4bc120` normalizes neighbor state, rather than drawing or adding a second surface. Post-selection call `0x4b7190` → `0x4b6e20` can write a replacement group-`0x623` graphic at `0x4b6f3a`; its adjacent raw terrain comparisons include exact `0x80` and `4`. These observations do not expand sandbox terrain authority.

The normal map draw helper at `0x46b1c0` reads this selected cell graphic (`0x46b1f9`, ordinary branch `0x46b4b3`). Its ordinary branches call `0x413960` at `0x46b4ef` or `0x4169d0` at `0x46b4d3`, according to the caller's draw-pass flag. The first wrapper forwards to `0x5cd8a0`, whose Type-30 dispatch is at `0x5ce4b8`; the latter forwards to `0x5ce800`. In that optional-overlay path, `0x5ce8ae` advances the selected image pointer by its base byte count; the width-78 case uses the supplied x and the image-height/40 y relation, and the RGB555 Omega path reaches `0x41c170`. This distinguishes base data from the same image's Omega detail. It does **not** establish a separate retained old grass image beneath each selected road, an extra beige ground record, or a pixel modification of the basic family. The complete original multi-pass object painter has not been reconstructed.

The Emperor-size setup caller at `0x53ca93` supplies height 40 and width 78 to `0x469820` (`0x53cab3`). The setup retains width 78 at `0xadf8b8` and half height 20 at `0xadf8ac`; the draw-grid loop advances horizontally by **width + 2** (`0x46aa1a`) and vertically by **half height** (`0x46aa44`). Thus width 78 is the source raster width, not a 78-wide logical row. Neighbor storage cells have the corresponding diagonal 40/20 relation. Draw coordinates and wrapper arguments are integer-valued; no road-specific optical offset was found. OpenEmperor only aligns the complete replacement texture's origin to pixels at integer camera zoom. It preserves source size, source anchor, World coordinates and every building's placement.

The topology matcher at `0x4bbdc1` reads the orientation state `0x101d0d0`, dividing it by two to select the column. Rotation helpers around `0x406bb0` corroborate that it is view state. “Orientation 0” here means the first cardinal view selection, not a newly verified compass label. Pixel review independently matches its x/y curb directions to OpenEmperor's fixed camera. Other view orientations are local comparisons only; no production rotation/mirroring or alternate-column selector was added.

### Basic-road physical structure

All records below are static internal unmirrored Type 30, size flag 1, 78×40, base 3,200 bytes, no separate alpha stream. Every base contains **zero `0xf81f` transparency sentinels** and exactly 1,600 opaque RGB555 paving pixels. The decoded nontransparent bounding box is `[0,0,78,40]` for all. Transparent rectangular padding is ordinary diamond shape, not a missing paving underlay. Omega bytes, when present, add minor edge details within the same image.

| Physical record | Omega color bytes | Total opaque RGBA pixels |
|---|---:|---:|
| 782 | 0 | 1600 |
| 783 | 0 | 1600 |
| 784 | 0 | 1600 |
| 785 | 0 | 1600 |
| 786 | 64 | 1606 |
| 787 | 0 | 1600 |
| 788 | 0 | 1600 |
| 789 | 43 | 1601 |
| 790 | 0 | 1600 |
| 791 | 59 | 1605 |
| 792 | 79 | 1609 |
| 793 | 45 | 1602 |
| 794 | 98 | 1614 |
| 795 | 0 | 1600 |
| 796 | 0 | 1600 |
| 797 | 0 | 1600 |
| 798 | 0 | 1600 |
| 799 | 0 | 1600 |

Both straight records 782/783 have **no Omega payload**; the road surface and curb stones are already in their bases. Decoded RGBA inspection and production readback agree. The recorded “beige fraction” is a local RGB heuristic for measuring paving, not a new file-format or gameplay interpretation.

### Actual contacts and limits

The ignored read-only pair diagnostic uses the ordinary `RoadSpriteSet`, source anchor and the same `terrain_ground` 40/20 formula as `SandboxView::world_for`. It tests all 256 reciprocal mask combinations in `neg_y,pos_x,pos_y,neg_x` at 1×, sampling rendered RGBA across the central shared edge: **0 transparent/background pixels, 0 minimum and maximum transparent gap**. Complete bases already tessellate; 78→80 stretching would obscure the real compositor defect.

A separate production `SandboxView` readback over Xia reproduces the prior grass strips. It covers 16 neighborhoods, eight-cell straights, L, T, cross and 5×5 at exact player zoom presets 1×/2×/4×. At 1×, all **128** logical contacts retain a paving core after correction; maximum full-core grass cut is **0 px**, compared with 2–6 horizontal / 1–3 vertical pixels before. Individual-lane maxima were 8–10 horizontal / 4–5 vertical pixels before; after, occasional one-pixel grass blades from neighboring non-road images remain without cutting the road. Exact positions, cut scanlines, per-lane maxima and map-first controls are local JSON reports. See the [pattern-by-pattern measurements and regression contract](../road-visual-profile.md#road-continuity-correction-2026-10-03). Do not reinterpret 0 disconnecting gap as absence of every blade or other object's occlusion.

The paid six-House Xia scene includes House stages 0/1/2 and all requested existing roles at tick 4200. Final-bundle actual keyboard F1, zoom-preset and F6 input verified normal 1× continuity and the independent fallback toggle; 2×/4× are secondary. On the local QWERTZ layout the automation's physical Y key generates the application's Z shortcut. Scripted scene construction/readbacks and native input are reported separately. Well/Herbalist IDs, geometry, anchors and source images remain unchanged. Road adjacency to Houses, Market, Farm, Well, Herbalist, ServicePost and FireWatch was reviewed without inventing entrance paths. No additional entrance-spur graphic was established in this bounded basic-road path; this does not prove none exists elsewhere in Emperor. `EntranceMask` stays diagnostic.

The exact pack's optional complete-ground flag applies only to configured singleton roads and valid drag cells. Alpha preview preserves its existing terrain as an early backdrop; opaque roads replace the old cell image. Other stored objects keep their painter keys and images. No original bytes, World terrain mutations, new save/rule fields, costs, BFS, buildings, goods or service mechanics were introduced.

## Original-production static building bases

The OE52 source audit starts from `cc53e7037dac4ebd4da6b68d382a6e898890716b`.
The EXE, Model file and General archive pair have the exact SHA-256 values
recorded above. Their bytes, sizes, modes and modification times were checked
before and after the read-only audit. No original executable was run. Raw
inspection reports, source-field checks, the small shared-API resolver, decoded
images and review artifacts remain ignored under `.local/oe52/source-audit-01/`.

The following ten static selections have separate identity and geometry
evidence. Well72 and Acupuncturist208 reuse the generic graphic-reader evidence
above. The [culture source contract](original-culture-source-contract.md) binds
211–213 to their concrete constructors, initial sides and registrations; the
[religion source contract](original-religion-source-contract.md) does the same
for 215, 217 and 219. The [industry source contract](original-luxury-source-contract.md)
binds the maker bodywriter `0x5f1ee0` through selector `0x4f13f0` to each actual
model registration. Fresh bounded reads of the pinned EXE confirm the initial
side byte at `0x823598 + 24*model` and group word at `0x8235a0 + 24*model`,
including Paper45/key`0x44a` and Weaver47/key`0x446`.

Every group was independently resolved through the production
`RuntimeArchiveLayout` with `TerrainElevationAndGeneral2` and
`ResourceGroupLookup`; there is no second SG3 parser or physical-index
translation. All ten complete images decoded through the existing
`Sg3ImageLoader` via the asset inspector and were actually opened and inspected.
The native bases, roof/body details and transparent rectangular padding are
coherent. Names and visible motifs corroborate these selected frames; they do
not independently establish a runtime selector or animation.

| Model / source identity | General group key | Physical record | Native size | Initial side | Geometric front-cell anchor |
|---|---:|---:|---:|---:|---|
|72 Well|`0x482`|1559|158×132|2|`[79,112]`|
|208 Acupuncturist's Clinic|`0x485`|1593|158×137|2|`[79,117]`|
|211 Music School|`0x412`|589|158×137|2|`[79,117]`|
|212 Acrobat School|`0x410`|460|238×162|3|`[119,142]`|
|213 Drama School|`0x411`|501|238×176|3|`[119,156]`|
|215 Daoist Shrine|`0x49b`|2245|158×134|2|`[79,114]`|
|217 Buddhist Shrine|`0x49c`|2258|158×142|2|`[79,122]`|
|219 Confucian Academy|`0x49f`|2309|238×201|3|`[119,181]`|
|45 Paper Maker|`0x44a`|2811|158×102|2|`[79,82]`|
|47 Weaver|`0x446`|2768|158×119|2|`[79,99]`|

All are internal, unmirrored Type30 records with no separate alpha payload and
in-bounds color payloads. Their exact base lengths are 12,800 bytes for side2
and 28,800 for side3; native width is `80*side−2`. The selected geometric anchor
is `[40*side−1,height−20]` relative to the existing authoritative front cell.
For these ten selections this is an **authored geometric placement convention**,
not a recovered per-model original runtime pivot. Source heights, projection,
footprints and whole-image painter rules are separate and remain unchanged.

| Physical record | Animation sprites | Raw animation offset | Raw speed ID |
|---:|---:|---|---:|
|1559|20|`(50,69)`|5|
|1593|24|`(8,42)`|4|
|589|12|`(38,34)`|4|
|460|40|`(49,41)`|4|
|501|87|`(95,56)`|5|
|2245|12|`(5,28)`|5|
|2258|12|`(34,21)`|5|
|2309|0|`(0,0)`|0|
|2811|20|`(2,0)`|4|
|2768|19|`(2,5)`|5|

These raw fields do **not** establish associated overlay frames, state
selection, a World-tick duration or original cadence. Only the complete static
base is selected. There is no inferred building animation, loaded/empty variant,
religious effect, culture performer or production-state graphic. The ten images
contain 1,083,824 logical RGBA bytes. Excluding the existing Well1559 physical
image leaves nine additional images totaling 1,000,400 bytes. The final combined
pool must retain the unchanged 64 MiB budget and shared physical deduplication.

The metadata-only `original-buildings.json` keeps these ten typed selections
separate from the thirteen legacy role entries and the original House-stage
table. In original production, `ServicePost` corresponds to the paid
Acupuncturist208 source; the historical one-cell notice-sign preview remains a
different contract. Model59's generic registration word is zero and does not
justify a new Market graphic. Ancestor214 remains an inventory candidate outside
this selected set. The separate [farm and field audit below](#farm-bodies-and-field-state-boundary)
qualifies Farmhouse193 and HempFarm192; it does not change these ten historical
selections or infer field states from nearby records or model names.

This source audit establishes the ten selected static frames, source bindings
and geometric compatibility. Final application, capture, fallback, resource and
historical-save validation are separate from source research. It does not claim
original animation, profession, full scene, native interaction or human play
acceptance, and introduces no World, rule or save authority.

## Farm bodies and field-state boundary

The farm audit uses the same read-only PE32/i386 EXE, image base `0x400000`,
SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The Model SHA-256 remains
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`;
General.sg3 is
`c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`,
and General.555 is
`3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`.
Bounded static batches passed the OE60 and OE60.5 guards before and after their
reads, preserving protected bytes, sizes, modes, modification times and tree
membership. The original executable was not run; no personal saves were sought.
Private disassembly, helper programs, raw metadata and decoded candidates stay
under `.local/oe60/` and `.local/oe60-5/` research directories.

The evidence classes have separate meanings. **MODEL_OBSERVED** identifies the
named rows; names do not establish a factory or graphic. **EXE_OBSERVED** below
means static qualified calls, object callbacks and table reads, not original
execution. **SOURCE_VERIFIED** describes SG3/555 metadata and ordinary decoder
output. **AUTHORED** identifies our placement convention and production clock.
**UNKNOWN** retains an incomplete original lifecycle; **UNSUPPORTED** means it
does not authorize runtime presentation or gameplay in this integration.

### Concrete bodies and publication

**EXE_OBSERVED:** manager creation `0x42d540` selects the actual object through
`0x42d360`, then invokes its `+0x94` initializer. Industrial factory
`0x51c660` first rejects the field predicate `0x4c11b0` for192/193, then accepts
farm predicate `0x4c2930`. Its exact jump-table entries select:

| Model / MODEL_OBSERVED identity | Concrete constructor | Installed vtable | Initial side | Qualified body group |
|---|---|---|---:|---|
|192 Hemp Farm|`0x4c5ca0`|`0x7af6e0`|2|`0x4ad`|
|193 Farmhouse|`0x4c5b80`|`0x7af1e8`|3|`0x43a`|

Both tables select `+0x94→0x428aa0`. This writes the requested runtime type and
the initial side byte from `0x823598+24*type`, then invokes
`+0x90→0x4c30f0`. Both concrete placement callbacks select
`+0x100→0x4c5bf0→+0x264→0x4c2ac0`.
The complete checked bodywriter `0x4c2ac0–0x4c2b17` resolves the type's
registered group with `0x408170`, adds the definition's final graphic offset
(zero for both types), and passes the registered square side, coordinates and
object ID to `0x4b72b0`. That publisher writes the selected graphic into the
per-cell image array at `0xfe9880` and the object IDs into `0xfc3750`.
The checked bodywriter contains no stock, productive-progress, staffing or
crop-state branch. This qualifies the static body for construction and later
calls of this same body callback; it does not reconstruct every original
refresh path or operational animation.

**SOURCE_VERIFIED:** the existing General2 registration/layout rules applied
to production-inspector metadata resolve these two group bases to825 and793.
The shared decoder reads the complete native Type30 images, without cropping
or stretching. Both are internal, unmirrored records with no separate alpha
stream. Native foundation padding and Omega detail belong to those images.

| Model / physical | Native dimensions | Base / Omega bytes | Raw animation count / offset / speed | AUTHORED geometric front-cell anchor |
|---|---|---|---|---|
|192 /825|158×137|12,800 /13,202|14 /`(15,36)` /4|`[79,117]`|
|193 /793|238×192|28,800 /19,406|18 /`(56,−2)` /4|`[119,172]`|

The static body offset is0; the animation counts are **not** body variants.
Associated animation records, activation, original cadence and the complete
animated scene remain **UNKNOWN**. The anchors use the existing convention
`[40*side−1,height−20]`; source dimensions and foundations are verified, but
these anchors are **AUTHORED geometry**, not recovered original per-model
pivots. Shared signed height, projection, picking and painter rules remain
separate contracts.

### Fields and actual adjacent registrations

**EXE_OBSERVED:** `0x4c0600` accepts195–199; `0x4c0630` accepts194. Through the
first Industrial factory branch,195–199 select
`0x4c0660→0x4c2220`, vtable`0x7aea40`;194 selects
`0x4c0660→0x4c25e0`, vtable`0x7aecc8`. All six have initial side1, inherited
initializer `0x428aa0` and field initialization callback `0x4c1860`.
Field construction has a distinct path: the purchase dispatcher qualifies
`0x4c11b0` at `0x4b14d8`, and `0x4c1320` creates accepted cells through the
real manager before calling each field's `+0x260` publisher at `0x4c1495`.
The generic Farm placement callback is not evidence for a field's initial
graphic.

The table records group **bases**, not automatically selected live frames.
Side bytes and group registrations are EXE observations; physical records,
dimensions and payload sizes are source resource facts. Labels are Model
observations.202/203 were bounded neighboring candidates, not additional
supported sources.

| Model / identity | Group base | Physical base | Initial side | Base image dimensions | Base / Omega bytes | Qualified runtime scope |
|---|---|---:|---:|---|---|---|
|194 Hemp Field|`0x431`|2458|1|78×41|3,200 /246|State-dependent field callback|
|195 Wheat Field|`0x42e`|2422|1|78×43|3,200 /396|State-dependent field callback|
|196 Millet Field|`0x42c`|2410|1|78×43|3,200 /366|State-dependent field callback|
|197 Rice Paddy|`0x42d`|2506|1|78×42|3,200 /594|Separate rice topology branch; gameplay UNSUPPORTED|
|198 Cabbage Field|`0x42f`|2434|1|78×40|3,200 /0|State-dependent field callback; gameplay UNSUPPORTED|
|199 Soybean Field|`0x430`|2446|1|78×41|3,200 /201|State-dependent field callback; gameplay UNSUPPORTED|
|202 Irrigation Ditch|`0x435`|672|1|78×41|3,200 /139|Generic definition only; factory/lifecycle UNKNOWN|
|203 Irrigation Pump|`0x439`|761|2|158×110|12,800 /2,994|Generic definition only; factory/lifecycle UNKNOWN|

All listed base records are internal, unmirrored Type30 images without a
separate alpha stream. The field base records have animation count0. This
does not make their complete live selection state-independent.

For194, `+0x260→0x4c2650`; for195/196/198/199,
`+0x260→0x4c2360`. The actual type-to-crop helper `0x4c1050/0x4c1060`
selects the crop group table at `0x7ae2bc`:194→index5,195→0,196→1,
197→2,198→3,199→4. Except for197's separate topology branch, publication
uses a quantity-dependent offset bounded to six choices, plus6 when bit0 of
the original per-cell byte at `0xf1e780` is set. The quantity callback
`0x4c1290` reads signed own-info word`+0x04` and calls `0x408ba0` with10000.
The complete helper `0x408ba0–0x408bbb` returns0 for a zero denominator;
otherwise it computes signed integer `trunc(100*numerator/denominator)` toward
zero, without floating point or a clamp. The field caller sign-extends its
own-info word and supplies denominator10000, so its result is
`Q=trunc(own_info_word/100)`. The signed16-bit field input cannot overflow the
helper's multiplication by100. In the observed nonnegative0–10000 quantity
range, Hemp selects `min(5,Q/20)`, whereas Food selects0 when `Q<=0`, otherwise
`min(5,Q/20+1)`, using integer division. The source does not supply a general
lower-bound clamp for arbitrary invalid Hemp quantities.

Both publishers then add6 when the separate source variation byte's bit0 is
set, and publish an actual1×1 footprint through `0x4b72b0`. This byte comes from
the separately retained `0xf1e780` array, not the terrain dword or candidate
byte. Its general semantic meaning is not established. Thus the twelve
neighboring records are six quantity choices in two source-selected banks,
not twelve elapsed-time stages, random alternatives or a shared progress
formula. These arithmetic observations do not reconstruct the original
quantity's complete growth, harvest or restore lifecycle.

Separate code at `0x4c1a90/0x4c1a20` compares crop schedule entries at
`0x7ae19c` with global`0xc82eec`, gated by`0xc82ef0`, and changes own-info
`+0x6a` flags. Reset `0x4c1720` clears own-info`+0x04` and`+0x6a`;
work callbacks `0x4c1d20/0x4c1bc0` change retained work/countdown bits.
These are bounded evidence of distinct crop state and calendar dependencies,
not a complete reconstructed growth, harvest, fertility or season contract.
The checked initializer `0x4c1860` clears own-info bytes`+0x06..+0x1d` and
`+0x5c`; it does not directly clear the quantity word`+0x04`. A reset that clears
that word is therefore not by itself a proof of the complete purchase path's
initial quantity. The exact complete frame at every original field purchase
or restored field state remains **UNKNOWN**.

### Current authority and supported boundary

**AUTHORED:** current World production owns the parent farm's bounded
progress0–79, actual harvest counter, physical stock and real cargo. A separately
paid field has an identity, ownership link and placement tick. These do not
bind the original field's own-info quantity/work state, calendar or raw cell
bit. Rendering cannot reconstruct those missing original states from elapsed
ticks, progress80, stock, a dispatch or a crop label, and cannot create new
authority to fill them. In particular, already-harvested Farm stock0–8 is not
the original field's separate quantity word.

The two static farm bodies have qualified original body callbacks. Historical
building-profile schema2 retains its explicit field fallbacks; schema1 retains
its ten-body contract. Schema3 can display the registered field images under
the deliberately authored policy below. Original194/195/196 growth-state
reproduction remains **UNSUPPORTED**.197/198/199/202/203 gain no gameplay
admission. Full original animation, original-save restoration, camera rotation
and whole-scene parity remain **PARTIAL**, separately from application and
regression results in
[the current rendering assessment](../rendering/oe60-original-farm-visuals.md).
No World, rule, cost, limit, productive clock, ownership or save schema is
changed by these source observations.

### Canonical field images with authored progress

**EXE_OBSERVED / SOURCE_VERIFIED:** the qualified crop groups and shared resource
resolver identify these canonical bank0 record families:

| Paid field model | Group | Canonical offsets | Physical General records |
|---|---|---|---|
|194 Hemp|`0x431`|0–5|2458–2463|
|195 Wheat|`0x42e`|0–5|2422–2427|
|196 Millet|`0x42c`|0–5|2410–2415|

Each prepared image retains its own decoded dimensions and alpha mask and must
validate as a side1 Type30 record. The shared front-cell anchor
`[39,height−20]` remains **AUTHORED geometry**, not an original per-frame pivot.
The six offsets fit the existing bounded resource-group variant path; no
global variant-limit increase is needed. The second source-selected bank is
not activated by this policy.

**AUTHORED:** building-profile schema3 explicitly names
`authored-farm-progress-six-canonical-0-v1`. For a real paid field bound to its
actual parent Farm, the presentation selects
`stage=floor(6*parent_farm.progress/80)` only for saved progress0–79:

| Authored productive progress | Source-family offset |
|---|---:|
|0–13|0|
|14–26|1|
|27–39|2|
|40–53|3|
|54–66|4|
|67–79|5|

Invalid progress or missing/mismatched paid ownership has no admitted sprite
and retains a diagnostic fallback. This is a pure presentation of existing
validated World authority. It does not use elapsed wall time, assign field
quantity, grant a harvest or alter staffing, production, stock, cargo or saves.
Existing pause, fire and harvest behavior governs the retained progress;
there is no separate presentation growth clock. Restore recomputes the same
selection from the saved parent/field identities and progress.

The policy intentionally chooses canonical bank0 and does not reproduce the
original per-cell variation bit, `Q` formula, calendar or crop-state lifecycle.
An authored stage0 is not a claim that every original purchase starts in that
state. Original images are source facts; their assignment to OpenEmperor's
productive progress is an explicit authored choice, not original-state parity.
