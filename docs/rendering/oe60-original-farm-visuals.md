# Original farm bodies and authored field growth

OE60 restores the original static Hemp Farm192 and Farmhouse193 bodies.
OE60.5 adds six original images for each paid Hemp194, Wheat195 and Millet196
field. Their selection deliberately maps the existing parent Farm progress to
six visual stages. **This is AUTHORED growth using source pixels; original
field-state and calendar reproduction remains PARTIAL.**

No production, crop, quantity, placement, worker, movement, fire, limit, rule,
save schema or default changes. Rule18/schema34 remains recommended;
Rules1–23/schemas1–39 keep their contracts. Original files remain user-supplied
and read-only. Resources contain metadata, never original pixels or binaries.

## Source evidence

The [presentation source contract](../reverse/original-presentation-correction.md#farm-bodies-and-field-state-boundary)
records the qualified factories, publishers, quantity arithmetic, unresolved
lifecycle and separate evidence classes. The pinned installation is unchanged:

| Input | SHA-256 |
|---|---|
| Original EXE | `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e` |
| Building model | `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6` |
| General.sg3 | `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46` |
| General.555 | `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2` |
| Cities/Xia.map | `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433` |

| Model | Role | Group | Selected physical records | Side | Native dimensions |
|---|---|---|---|---|---|
|192|Hemp Farm|`0x4ad`|825|2|158×137|
|193|Farmhouse|`0x43a`|793|3|238×192|
|194|Hemp Field|`0x431`|2458–2463|1|78 wide; per-stage height|
|195|Wheat Field|`0x42e`|2422–2427|1|78 wide; per-stage height|
|196|Millet Field|`0x42c`|2410–2415|1|78 wide; per-stage height|

`EXE_OBSERVED` identifies static farm publication and field group/quantity
selection. `SOURCE_VERIFIED` identifies complete native Type30 records and the
shared decoder's pixels/alpha. All18 field images are canonical bank0, offsets0–5.
The separate source variation byte can choose a second bank; its general meaning
remains unknown. Rice197, Cabbage198, Soybean199 and irrigation202/203 gain no
presentation or gameplay admission.

The original field quantity callback uses signed own-info word`+0x04` and
`0x408ba0` with10000, producing `Q=trunc(own_info_word/100)` toward zero.
In the observed nonnegative range, Hemp chooses `min(5,Q/20)` and Food chooses
zero when `Q==0`, otherwise `min(5,Q/20+1)`. These differ from our deliberately
authored policy below. Original construction quantity, complete work/calendar,
harvest and restore lifecycle remain unresolved. Farm stock0–8 is harvested
inventory, not that quantity word. No original executable was run.

## Authored selection and authority

Building-profile schema3 requires
`field_selection="authored-farm-progress-six-canonical-0-v1"`.
The actual paid field ID must match an existing production ownership row, the
parent's Farm kind and, for Food, its Wheat/Millet crop. Selection reads only
that Farm's validated saved progress0–79:

| Parent Farm progress | Canonical image offset |
|---|---:|
|0–13|0|
|14–26|1|
|27–39|2|
|40–53|3|
|54–66|4|
|67–79|5|

This is `floor(6*progress/80)`, with no wall clock or new mutable field state.
Existing productive ticks, pause, fire and harvest determine the Farm progress;
restore selects the same image from the same saved authority. Missing or
mismatched ownership, invalid progress or unavailable art keeps the named
fallback. A placement ghost may show stage0 as an authored preview; this does
not establish the original game's construction image. The Inspector names the
stage, paid Farm ID, actual progress and authored calendar boundary.

Farm bodies remain static825/793. Their anchors `[79,117]`/`[119,172]` and field
anchors `[39,height−20]` use the existing authored front-cell convention. These
are geometric placement, not recovered runtime pivots. Every stage retains its
complete base/Omega image, own height and alpha mask. Draw, culling, preview,
alpha picking, fire attachment and inspection resolve the same prepared entry.
F4 retains the historical authored comparison.

### Adjacent field ordering

The native field painter qualifies a local correction for a 1×1 field beside
a larger occupied footprint. A shared scalar front-cell key can otherwise draw
the complete 3×3 Farm after its eastern field, hiding crop pixels even though
their paid ground cells are disjoint. Image origins, source anchors and actual
Field/Farm footprints do not move.

Preparation reads only the four direct neighbor owners and their complete
footprints. West/north neighbors bound the field from behind; east/south
neighbors bound it from the front. Where these constraints admit a strictly
ordered slot, the native field follows the rear building at that building's
unchanged flat ground key. A separate 64-bit visual tie namespace keeps actual
32-bit entity IDs intact and retains the existing Building/Walker layer order.
The same key feeds the sorted shared stream and submitted alpha-picking hits.
No special comparator, global layer, image offset or second draw pass is added.

This is a bounded authored correction, not a general footprint compositor.
Conflicting constraints or adjacent field dependencies retain historical order
and report ambiguity; whole-scene parity remains unresolved. The five-entry
cache follows the existing layout/road revision after commands and ticks, with
forced preparation at initial World publication and F9. Neighbor discovery is
outside render/hover work. Only actual native paid fields in productive unified
rendering use the correction. Schema1/2 fallbacks, F4, F7, Full Snapshot and
unpaid placement ghosts retain their previous ordering.

## Preparation and compatibility

Schema1 retains exactly ten native static roles. Schema2 retains twelve native
bodies and three fallback-only fields. Schema3 retains the same static rows,
then admits either an exact complete six-row field table or a strict fallback
row. Static lookup never chooses an implicit field stage. Unknown markers,
wrong order/crop/group/offset/geometry/anchor, unsupported animation, partial
payloads and mixed native/fallback keys reject the optional profile atomically.
A failed replacement preserves active graphics and the complete World.

Preparation reuses `ResourceGroupLookup`, `RuntimeArchiveLayout`, the shared
SG3/555 loader and deduplicated `BuildingSprite` pool. Fingerprints, containment,
metadata and budget validation finish before publication. All stages are decoded
and uploaded eagerly. Existing core/House prefixes and historical replacements
retain their original selection; no parallel parser or renderer is added.

The complete profile has56 physical images and4,770,512 RGBA bytes within the
unchanged64-MiB building budget. Eighteen field entries add17 physical images
and280,176 bytes to OE60's39-image/4,490,336-byte pool: Millet stage5 already
shares core physical2415. Frames/hover perform no file I/O, parsing, decoding,
upload, World copy, command, tick, BFS, route refresh or map scan. Selection
scans only the existing bounded two Hemp/three Food ownership rows. No visual
state enters a SaveDocument.

## OE60.5 baseline verification

The complete acceptance results below describe the OE60.5 implementation
merged as `40903b4ec4b750032a157feb2dc563a688ae4658`, before the adjacent-field
ordering correction. They remain historical evidence; they are not a rerun of
the later painter changes. Those changes have separate focused checks below.

Use the isolated original-data launchers and fresh `.local/oe60-5/` output
paths described in [testing](../testing.md#original-building-visuals-and-farmfield-boundaries-oe52oe60oe605).
The baseline is `ebea562c6fd951268ef7cb18dda5db1370378049`. Fresh Release,
Debug and ASan/UBSan builds provide321/98/98 current configuration/case proofs,
with no unresolved failure or skip. These are composite results: the initial
Sanitizer suite found a use-after-free in the new test's setup, where a borrowed
`buildings()` span survived a command publishing a replacement World. The test
now collects typed IDs before issuing the same commands. Its corrected target
was rebuilt and passed in all three configurations; the other320/97/97 cases,
their binaries, all14 production libraries and resources remained unchanged.
The retained520 executions comprise516 initial passes, that one initial error
and three corrected passes. LeakSanitizer was not run (`detect_leaks=0`).

The earned-city test preserves its66 bodies/46 couriers and actual Hemp/Millet
fields over126 view cases. The separate paid-growth test buys Wheat and worker
Houses through ordinary commands, without rewriting the immutable city. Each
corrected configuration observes all80 real progress values for all three crops,
all18 crop/stage combinations and actual harvests. Its162 size/zoom/stage cases
check source pixels, ordinary pointer selection, F4, Inspector text, pause/resume and F5/F9;
18 representative PNGs per configuration are retained. All701 runtime inputs
match the tested source mirror; all80 simulation/persistence files remain exact
against the baseline. Protected original data, old builds and evidence stayed
byte-identical throughout the full input guard.

The corrected private helper03 independently compares the baseline creator,
final creator and a fresh final restore process over ticks0–3200:3201 complete
authority states per process and identical complete CSV streams. Each process
has17 checkpoint labels at15 distinct ticks and35 full save/roundtrip/working
documents. The audit performs76 actual baseline/restore cross-process literal
comparisons plus38 final-copy integrity checks,105 physical saved-document
audits and51 F5/F9 pairs. Paid construction, staffing, all field stages, harvest,
cargo, actual deliveries/returns, pause500/resume520 and source fire remain
unchanged. Final Funds are8879 =12000−2419 construction−702 upkeep; tax is zero.
Hemp30 =27 stock+3 cargo; Wheat29 =8 stock+18 consumed+3 cargo;
Millet29 =12 stock+17 consumed. This finite city is not a citywide reliability
or profitability proof.

The older OE60 helper and superseded OE60.5 helper02 contain the same borrowed
span setup error. Their files and runs remain historical, unchanged and
superseded as paid evidence; only the fresh corrected helper03 runs support the
current comparison. Historical compatibility separately passes152 literal JSON
roles (123 SaveDocuments,25 legacy DTO controls,4 indices) across Rules1–23 and
schemas1–39. Rule18 Golden City compares93 complete top-level streams,4 recovery
payloads and2 bounded metadata roles. Rules22/23 each pass six creator/restore
processes;18 additional complete Rule23 SaveDocuments match protected references.

The final image inventory covers613 files:591 generated captures,18 decoded
source previews and4 unchanged authored repository-icon paths. There are255
actual individual openings covering252 distinct source/render contents, plus354
explicit transfers to byte-identical SHA-256 aliases; all609 non-icon paths are
covered. Icons are catalogued separately and provide no gameplay evidence.
The six paired render PNGs were individually opened. Native crop images replace
the old green diamonds; high-zoom aperture clipping and existing long menu
footer clipping remain explicit limits. Private receipts and images live under
`.local/oe60-5/`, including `final-technical-result-02.json`; no original pixels
or saves are distributed through Git.

## OE60.5 baseline render measurements

Nine actual camera/aperture/selection pairs use the same immutable paid tick600
save. Each version records20 warm frames and120 measured render/Present samples
per pair (180 warm and1080 measured frames total). Real pointer motion and
`update(0)` are inside the twelve-counter batch but outside the render timer;
readbacks, preparation and input guards remain outside it. Every counter is zero
and full World/F5/F9 bytes remain unchanged. Timings below are milliseconds;
quantiles use sorted-sample index `floor(p*(120−1))`.

| Window | Zoom | Baseline median | Field median | Baseline p95 | Field p95 |
|---|---:|---:|---:|---:|---:|
|1280×800|1×|1.700|1.730|1.754|1.760|
|1280×800|2×|6.657|6.757|6.759|6.849|
|1280×800|4×|10.574|10.979|10.952|11.190|
|1600×900|1×|2.264|2.274|2.292|2.321|
|1600×900|2×|9.338|9.435|9.449|9.519|
|1600×900|4×|12.799|13.144|13.056|13.313|
|1920×1080|1×|3.153|3.190|3.231|3.310|
|1920×1080|2×|13.939|13.993|14.091|14.325|
|1920×1080|4×|18.210|18.322|18.835|18.634|

These finite SDL dummy/software observations retain uncontrolled user/system
load; our other build/test/review jobs were finished and no user app was stopped
or controlled. Median ratios are1.00387–1.03833 and p95 ratios0.98937–1.02449.
They establish no universal frame budget or speed improvement. Session
preparation is separately511.86/521.71ms, not part of the frame samples. The
prepared pool grows from39/4,490,336 to56/4,770,512 RGBA bytes within the unchanged
budget.

Native desktop delivery, human acceptance and clean-Mac acceptance remain
**NOT_RUN**. Original field quantity/variation/calendar, animation, runtime
pivots, original binary-save graphics and whole-scene parity remain **PARTIAL**
or unresolved. The visible growth is the explicitly authored policy above.

## Field-order correction verification

The independent synthetic order cases cover the reported eastern Wheat field,
the mirrored southern edge, front-neighbor protection and contradictory
constraints. The paid-growth regression derives field and Farm screen geometry
from World cells and source heights, independently of renderer diagnostics;
it checks actual overlapping source pixels and picking while preserving the
existing progress, save and pure-frame assertions. Against the unchanged
`40903b4` renderer with only the new test translation unit, the first Wheat
witness (stage 0, 1×) fails with Farm793 RGBA `[231,198,140,255]` instead of the
independently decoded Field2422 RGBA `[148,123,74,255]`.

The corrected frozen source passes all 321 Release tests and the same 12 focused
tests in Debug and ASan/UBSan, with no skips. Each completed growth run retains
all 162 stage/size/zoom cases, all 80 real progress values per crop, actual harvests,
22 full F5/F9 checkpoints and 27 independent Wheat stage0/2/5 overlap cases.
All 27 preserve the parent-Farm control; three representative 1280×800/4× cases
also check the adjacent Road against F4.
Ordinary clicks retain ground-owner selection; explicit F1 diagnostics with a
fresh submitted frame select the visible Field alpha at the same overlap point.
The older reports' ordinary opaque/transparent pointer probes do not establish
that F1 alpha-selection result. All twelve pure-frame counters remain zero.

All 701 runtime inputs match the tested mirror; all 80 simulation/persistence
files remain byte/mode-identical to the baseline. The complete input guard
preserves 80,218 files (33,037,579,734 bytes). The final image inventory covers 369
paths: 51 actual individual openings, 314 explicit transfers to exact SHA-256
aliases and 4 authored repository-icon paths, with no unreviewed capture.
Scene filenames do not prove the selected Inspector; scene/crop review and the
explicit F1 selection assertions remain separate evidence. Existing high-zoom
aperture and long menu-footer clipping remain visible limits.

Private component receipts, retained test/harness failures and the final audit
remain under `.local/oe60-5/field-order-fix/`, including
`final-technical-audit-02.json`. No production code changed after the source
freeze. LeakSanitizer (`detect_leaks=0`), native/human acceptance, new timing,
long paid-gameplay and historical-save replays were not run for this correction;
the earlier broad results above remain historical.
