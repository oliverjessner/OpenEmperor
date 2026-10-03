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
