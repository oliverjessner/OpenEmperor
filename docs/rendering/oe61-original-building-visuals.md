# Original industry and Ancestor building visuals

OE61 adds source-registered static bodies for seven existing paid industries
and Ancestor Shrine214. It does not introduce a building, production chain,
worker, courier, cost, footprint or gameplay rule. The baseline is
`31686ad4558ead7d97470bebff19773e84976054`, on
`feat/oe61-original-building-visuals`. Rule18/policy3/schema34 remains the
recommended Original economy; visual-profile schema4 is a separate asset
metadata version, not a save upgrade.

The source/decoded-image audit and the automated application/regression checks
below have separate evidence. Original graphical parity remains **PARTIAL**:
Market, Warehouse and authored FireWatch still have no qualified equivalent
body; lifecycle animation and native human acceptance are not established.

## Complete Rule18 presentation inventory

The inventory contains all **36 building kinds** in `World::Object` values3–40,
excluding the unavailable legacy Farm7 and Well11. The numbers below are enum
values, not purchased `BuildingId`s or an admission whitelist. Footprints are
the actual Rule18 World footprints. A native image never enlarges one.

`Static / N` means a complete canonical offset0 body is selected; `N` is the
raw animation-sprite count, **not an animation played by the renderer**.
`Preview` means existing Original pixels with an authored World association.
`Marker` means authored geometry without a selected Original image. Missing or
rejected optional art retains the named historical fallback. All listed General
group keys resolve through the shared slot2 resource/layout path; a curated
bitmap-group name alone is not such a key.

| Object | World identity / Original identity | Footprint | Current group key → physical record | Selection / animation | Evidence and fallback boundary |
|---:|---|---|---|---|---|
|3|Warehouse / functional candidate54|2×2|Unqualified core preview →637|Static preview; no animation|Original pixels; World identity UNKNOWN. Original54 has a side3 callback; its full body is not admitted here.|
|4|ClaySource / functional candidate35|2×2|`0x448` →2789|Static0 /20|Pixels now bound to concrete ClayPit35 publisher. Authored World binding and legacy pivot remain UNKNOWN; keep core preview.|
|5|Pottery / functional candidate43|2×2|`0x449` →2810|Static0 /0|Pixels now bound to concrete Kiln43 publisher. Authored World recipe/lifecycle binding remains UNKNOWN; keep core preview.|
|6|Household / Common House state|2×2|House table below: Type2–10|Canonical0; no building animation|Type2 requires the evidenced never-admitted purchase state. Unsupported vacant history uses authored foundation; other missing stages use historical fallback.|
|8|ServicePost / Acupuncturist208|2×2|`0x485` →1593|Static0 /24|Existing source-registered body. Missing supplement preserves the old2046 ServicePost sign/marker, not a recovered clinic lifecycle.|
|9|Market / functional candidate59|1×1|Unqualified core preview →645|Static preview; no animation|Original pixels, World identity UNKNOWN. Generic59 group0 and its composite descriptor do not establish a complete equivalent1×1 body.|
|10|FireWatch / authored Watch|1×1|Unqualified core preview →383|Static preview; no animation|Original civic pixels, original building identity UNKNOWN. Inspector124/Watchtower127 are separate2×2 objects.|
|12|HealthPost / Herbalist207 adapter|2×2|`0x484` →1580|Static0 /12|Existing source-registered core pixels with footprint check; health visits/illness remain authored World behavior.|
|13|OriginalInfluenceSource / fixed Well72 source|2×2|`0x482` →1559, through core Well role|Static0 /20|Existing source pixels; presentation grants neither Water visits nor new influence authority.|
|14|OriginalHempFarm /192|2×2|`0x4ad` →825|Static0 /14|Existing qualified farm body; no working-state animation.|
|15|OriginalHempField /194|1×1|`0x431` →2458–2463|Offsets0–5; raw animation0|Original images selected by AUTHORED parent-progress mapping; missing/invalid ownership or art keeps field fallback.|
|16|OriginalFoodFarm /193|3×3|`0x43a` →793|Static0 /18|Existing qualified Farmhouse body; Wheat/Millet production remains World-owned.|
|17|OriginalWheatField /195|1×1|`0x42e` →2422–2427|Offsets0–5; raw animation0|Same authored field-selection contract; no Original calendar/quantity claim.|
|18|OriginalMilletField /196|1×1|`0x42c` →2410–2415|Offsets0–5; raw animation0|Same authored field-selection contract; physical2415 deduplicates with the legacy Farm preview.|
|19|OriginalAncestorShrine214 /214|2×2|`0x481` →2232|Static0 /12|New qualified static body. Actual Ancestor service remains an arrival-only World timer.|
|20|OriginalWaterWell72 /72|2×2|`0x482` →1559|Static0 /20|Existing qualified static body; shared physical image with core Well, distinct paid Water authority.|
|21|OriginalDaoistShrine215 /215|2×2|`0x49b` →2245|Static0 /12|Existing source-registered body; no new religious effect.|
|22|OriginalBuddhistShrine217 /217|2×2|`0x49c` →2258|Static0 /12|Existing source-registered body; no new religious effect.|
|23|OriginalConfucianAcademy219 /219|3×3|`0x49f` →2309|Static0 /0|Existing source-registered body; real Paper input and elite-only service eligibility remain unchanged.|
|24|OriginalMusicSchool211 /211|2×2|`0x412` →589|Static0 /12|Existing body. Rule18 targeted House visits are separate from Rule22/23 venues.|
|25|OriginalAcrobatSchool212 /212|3×3|`0x410` →460|Static0 /40|Existing source-registered body; no performer/composite animation added.|
|26|OriginalDramaSchool213 /213|3×3|`0x411` →501|Static0 /87|Existing source-registered body; no performer/composite animation added.|
|27|OriginalLoggingShed38 /38|2×2|`0x43e` →2726|Static0 /14|New qualified body; managed Forestry remains authored, not reconstructed tree harvesting/ecology.|
|28|OriginalBronzeSmelter39 /39|3×3|`0x442` →2697|Static0 /0|New qualified body; managed Ore and Wood recipe remain authored.|
|29|OriginalBronzewareMaker42 /42|2×2|`0x447` →2788|Static0 /0|New qualified body; real Bronze+Clay recipe is unchanged.|
|30|OriginalLacquerwareMaker44 /44|2×2|`0x445` →2750|Static0 /17|New qualified body; real Lacquer+Wood recipe is unchanged.|
|31|OriginalPaperMaker45 /45|2×2|`0x44a` →2811|Static0 /20|Existing native body, not a new OE61 mapping. Hemp→Paper input and actual deliveries remain unchanged.|
|32|OriginalWeaver47 /47|2×2|`0x446` →2768|Static0 /19|Existing native body, not a new OE61 mapping. RawSilk→Silk remains unchanged.|
|33|OriginalTeaCuringShed237 /237|2×2|`0x43b` →840|Static0 /15|New qualified body; separately paid Bush26 and real World production remain required.|
|34|OriginalLacquerRefinery238 /238|2×2|`0x43d` →812|Static0 /12|New qualified body; separately paid Tree27 and real World production remain required.|
|35|OriginalSilkwormShed239 /239|2×2|`0x43c` →856|Static0 /22|New qualified body; separately paid Mulberry28 and real World production remain required.|
|36|OriginalTeaBush26 /26|1×1|Rule25 supplement `0x432` →2470–2473|Four canonical quantity images; no animation|Original publisher is verified;80-tick World growth projection and anchor are authored. Historical modes retain their marker.|
|37|OriginalLacquerTree27 /27|1×1|Rule25 supplement `0x434` →2494–2499|Six canonical quantity images; no animation|Independent plant publisher; no farm-stage substitution. Historical modes retain their marker.|
|38|OriginalMulberryTree28 /28|1×1|Rule25 supplement `0x433` →2482–2487|Six canonical quantity images; no animation|Independent plant publisher; Original time/season/irrigation parity remains unproved.|
|39|OriginalElitePlot / pending Elite family|2×2|No matching native body; Type11's4×4 entry is refused|Authored pending plot|A2×2 reservation cannot display a materialized4×4 House or grant occupants.|
|40|OriginalEliteHouse / materialized Elite family|4×4|House table below: Type11–17|Canonical0; no building animation|Existing House-state lookup with actual4×4 footprint; missing art retains the authored Elite body.|

This is an inventory of admissible kinds, not a claim that one city contains
all36. Original files and the complete paid World determine what can actually
be purchased. The [model candidate register](../../src/simulation/original/OriginalModelMapping.h)
also retains conservative functional/mechanics classifications; it is not the
renderer’s native-art admission table.

### Rule25 plantation supplement

Rule25 also reuses the existing six-image Wheat/Millet bank and authored
Farm-progress quantization for its explicitly authorized80-tick adapter. The
actual paid parent Farm's `progress` selects the frame; actual field
`harvested` is shown separately. No stored Original quantity, season, crop
calendar or Original timing is inferred. Rice/Cabbage/Soy remain locked, and
Rule24 preserves its exact zero-quantity/production-locked presentation.

The independent metadata-only `plantations.json` schema1 supplement prepares
exactly16 native plant images through the shared General2 registration,
payload reader and decoder. It leaves all historical building schemas1–6 and
Rules1–24 selections exact. Both General SHA256 fingerprints must match before
atomic publication; containment, complete canonical banks, physical identity,
native Type30/Omega geometry and the existing combined64MiB budget remain
mandatory. Repeated physical assets share the existing eager pool.

The source constructor `4c2710` publishes vtable`7aef50`, whose `+260`
callback `4c2820` selects plant images. For canonical variation0 and the
harvest-done flag false, percent is integer quantity/100: Tea selects
`min(3,3*percent/32)`, Lacquer `min(5,percent/20)`, Mulberry
`min(5,2*percent/20)`. Rule25 explicitly projects its authored0–80 productive
ticks to quantity0–10000. These images follow actual saved plant growth;
they do not establish Original growth rates, season slots, tending or harvest
animation. Preview uses the initial image and grants no plant or quantity.
The independent source harvest flag and variation bank are not synthesized.

All36 checked source bank records are width78, Type30, base3200 bytes,
without animation, mirror or separate alpha. Lacquer offset4/physical2498
has the observed size hint0; only this exact plant/offset with full78×86
geometry is admitted by the new supplement. Its1×1 footprint is independently
proved. The shared decoder already derives this geometry from the complete
base; no general zero-side admission or historical decoder change is added.
Front-cell anchors `[39,height−20]` are authored projection conventions.

Selection, culling, picking and previews reuse the prepared shared images.
A bounded painter cache adds the actual32 plantation plants to the existing
field rows at geometry revision boundaries; no neighbor discovery, source
read, decode or upload runs inside frames/hover. F4 retains the explicitly
labelled authored comparison. Growers are real World workers rendered as
named markers; their budget boundary is recorded in the
[walker contract](oe62-original-walker-animations.md).

## House-state selections

These existing House rows are unchanged. Every row selects static offset0;
the second Original variation is not selected from the raw map byte. Types2–10
require an actual2×2 Common body; Types11–17 require an actual4×4 Elite body.

| House type | General key | Physical record | Footprint |
|---:|---|---:|---|
|2|`0x47a`|1508|2×2, restricted fresh purchase|
|3|`0x471`|1509|2×2|
|4|`0x472`|1511|2×2|
|5|`0x473`|1513|2×2|
|6|`0x474`|1515|2×2|
|7|`0x475`|1517|2×2|
|8|`0x476`|1519|2×2|
|9|`0x477`|1521|2×2|
|10|`0x478`|1523|2×2|
|11|`0x479`|1525|4×4|
|12|`0x47b`|1526|4×4|
|13|`0x47c`|1528|4×4|
|14|`0x4ac`|1530|4×4|
|15|`0x47e`|1532|4×4|
|16|`0x47f`|1534|4×4|
|17|`0x480`|1536|4×4|

Type2 additionally requires `Household`, the exact empty Vacant House state,
an existing validated housing-history row and no `first_admission_tick`.
Previously occupied, devolved or otherwise unsupported empty states do not
inherit the special fresh-purchase graphic. Preview selection is explicitly a
fresh-purchase illustration. See [the Type2 contract](oe59-original-vacant-houses.md).
No rendering query mutates that history, occupants or House type.

### Roads and excluded kinds

Road is Object1, a separately paid1×1 transport cell, not one of the36 building
kinds. Its existing source key is `0x61e`, base physical782 in
`China_Terrain.sg3`, with the unchanged cardinal topology table:

| Cardinal mask |0|1|2|3|4|5|6|7|8|9|a|b|c|d|e|f|
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Physical record |794|790|791|786|792|782|787|795|793|789|783|798|788|797|796|799|

These are static topology choices, not a new random variation or road
animation. Shared projection and anchor39,20 remain unchanged. Source roads,
landings, gates, bridges and ramps retain their own permission/geometry
contracts; artwork grants no transport edge.

| Object | Why it is outside the Rule18 building inventory |
|---|---|
|Empty0|An empty cell, not a purchasable building.|
|Workshop2|Historical logistics object; unavailable in Original production.|
|Farm7|Legacy one-cell Farm command is refused. Rule18 buys192/193 and their separately owned fields. Legacy2415 remains a compatibility image only.|
|Well11|Legacy Well command is refused in Original production. The typed influence source and paid WaterWell72 remain distinct admitted objects.|
|OriginalTheatre75, Object41|Requires explicit Rule22/23; not admitted by Rule18. Its authored4×4 venue art and Music behavior are separate contracts.|

## New static source evidence

The maintained [presentation correction contract](../reverse/original-presentation-correction.md#existing-industry-and-ancestor-static-bodies-oe61)
contains the exact factory, initializer, concrete-vtable, body-callback and
selector observations. The [luxury source contract](../reverse/original-luxury-source-contract.md)
separately establishes paid source/material identities. Neither names nor image
motifs alone establish a mapping.

The qualified manufacturer classes select body callback `0x5f1ee0`; its actual
virtual selector `+0x25c→0x4f13f0` reads the runtime type registration. Logging38,
Smelter39, Bronzeware42 and Lacquerware44 have separately checked concrete
constructors/vtables. Orchard237–239 use their concrete class's
`+0x100→0x4c5bf0→+0x264→0x4c2ac0`, which reads the registration, definition
offset0, side, coordinates and ID. Ancestor214's exact factory passes literal
`0x481`; its constructor retains that key at`+0x158`, read by body`0x5abef0`.
All resolve through`0x408170` and publish through`0x4b72b0`.

The reviewed complete static publishers do not inspect stock, production
progress, staff, fire, calendar or terrain variation. This qualifies a
canonical static body, not every transitive refresh/restore path or associated
working animation.

| Model / physical | Whole image | AUTHORED front-cell anchor | Raw animation count / offset / speed |
|---|---|---|---|
|38 /2726|158×122|79,102|14 /11,−5 /4|
|39 /2697|238×208|119,188|0 /0,0 /0|
|42 /2788|158×132|79,112|0 /0,0 /6|
|44 /2750|158×120|79,100|17 /18,14 /5|
|237 /840|158×121|79,101|15 /6,35 /5|
|238 /812|158×131|79,111|12 /0,34 /5|
|239 /856|158×120|79,100|22 /2,33 /5|
|214 /2232|158×149|79,129|12 /37,49 /5|

All eight are internal, unmirrored Type30 records with no separate alpha
payload or shadow-marker flag. The complete shared-loader RGBA results have
only alpha0/255, transparent exterior padding and no visible full-red pixels.
Both native base and Omega are retained; none is a cropped replacement or an
animation overlay. Every newly exported image was individually opened.

Native width is`80*side−2`; base bytes are12,800 for side2 and28,800 for side3.
The anchor`[40*side−1,height−20]` is the existing **AUTHORED geometric
front-cell convention**, not a recovered model pivot. Tall Smelter art does
not change its3×3 ground claim. Culling, cached alpha picking, selection,
preview alpha and fire attachment use the same prepared whole image.

### Preserved uncertain previews

ClayPit35/key`0x448` and Kiln43/key`0x449` now resolve to the exact existing
core2789/2810 pixels through concrete source publishers. This improves their
**source-image identity**, not the authored World-to-Original object or recipe
equivalence. Clay2789 is158×92; its existing core anchor79,76 is preserved,
despite the separate geometric convention yielding79,72. Pottery2810 remains
158×140/anchor79,120. No silent pivot correction or strict native-role
promotion is made.

Warehouse54 has an actual side3 callback, whereas the World Warehouse is2×2.
Its group`0x424` resolves a78×93 one-cell record644, not a complete side3
Warehouse. This component does not replace curated637. Market59 has a
special-shape/composite path and generic registration0; observed4×7/7×4
influence bounds are not physical occupancy. They do not identify preview645
as an equivalent complete1×1 market. FireWatch remains the authored1×1 Watch
with curated383; the separate Inspector124 and Watchtower127 are2×2. No
resizing, original-name substitution or geometry exemption closes these gaps.

## Preparation, compatibility and authority

The shipped `original-buildings.json` uses visual schema4 with exactly23 typed
roles: **20 canonical static bodies and three explicit six-stage field roles**.
The first15 rows remain exactly the prior schema3 selections. The eight new
roles append to the existing enum/table; old enum values, source keys and
selection behavior remain stable.

| Visual schema | Exact role set | Combined physical pool with the unchanged current core/House prefix |
|---:|---|---:|
|Core + House prefix|12 unique core images +16 House images|28|
|1|Original ten static roles; no farm/field additions|37|
|2|Same ten +two static Farms +three fallback-only fields|39|
|3|Same15 roles; explicit canonical six-stage fields|56|
|4|Same15 +seven industries +Ancestor214|64|

The37/39/56 figures describe those historical schemas with today's unchanged
28-image core/House prefix; they do not rewrite earlier OE52 measurements made
before the later Type2 addition. Well1559 is shared with the core. Millet2415
is shared with the legacy Farm preview. Eight new physical images add
763,656 RGBA bytes to the measured56-image/4,770,512-byte baseline, giving
**64 images /5,534,168 logical RGBA bytes** before any per-placement instance.
This source-verified pool calculation is separate from final View upload
acceptance. The existing combined64MiB budget is unchanged; the asset-browser's
separate64-texture limit is not a new building-profile limit.

Schemas1/2/3 retain exact10/15/15 row counts, keys, exclusions and refusal
behavior. A new role cannot leak into an old schema. Schema4 keeps explicit
native/fallback selection: fallback-only rows cannot carry native source keys;
a native field requires all six correctly ordered stages and the existing
`authored-farm-progress-six-canonical-0-v1` marker. Static lookup never invents
a field stage. Field images still use the validated parent/crop/field binding
and `floor(6*progress/80)`; Original quantity/calendar/raw-variation parity
remains PARTIAL. See [the farm/field contract](oe60-original-farm-visuals.md).

The same `ResourceGroupLookup`, `RuntimeArchiveLayout`, SG3/555 payload reader,
complete decoder, physical-image deduplication and eager texture path serve
every row. No second parser or runtime admission list is added. Both General
source SHA values must match before activation. Strict source containment,
group/offset/side/geometry/anchor checks and the complete resource budget are
atomic. Invalid metadata or payloads do not partially publish a new pool;
failed replacement preserves the prior working profile. Explicit custom core
selection and missing optional art preserve their historical precedence and
fallbacks. F4/F7/Full Snapshot retain their existing comparison/order contracts.

Render, hover and picking select prepared entries only: no file access,
decoding, upload, readback, World copy/command/tick, BFS, route refresh, full-map
scan or all-pairs graph is introduced. Existing scene and local field-order
limitations remain; this work does not establish general interpenetration or
whole-scene parity.

World remains the sole paid authority. All Rules1–23, policies, save schemas1–39,
source permissions, costs, employment, recipes, progress, inventory, cargo,
reservations, routes, occupancy, fire and services retain their contracts.
Saves remain OpenEmperor JSON, not Original Emperor saves. No presentation
choice grants a body, resource, visit, protection or saved authority. No
Original executable is run and no Original pixels, binaries, private fixtures
or screenshots enter Git/application resources.

## Verification status and reproducibility

Bounded source/asset research uses these exact inputs:

| Input | SHA-256 |
|---|---|
|Original EXE|`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`|
|Building model|`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`|
|General.sg3|`c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`|
|General.555|`3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`|
|Paid city Cities/Xia.map|`871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433`|

The private qualified findings are
`.local/oe61/research-industry/industry-qualified-findings-04.json`, SHA-256
`f5b2cb18623f15fcb23a06ddcf66b2913ab31b047f1f1c97cd7094575b2ca8d1`.
They bind four completed guarded batches, concrete callback windows, a freshly
linked shared-API helper, direct before/after checks of838 baseline source
files and14 archives/toolchain inputs, and13 source-record comparisons. The
eight actual individual source-image openings are separately pinned by
`source-image-review-01.json`, SHA-256
`cf9d086797c89bd0178c265a4fc70e99d6cd572fae0c88e4af7a464b14e64d26`.
These are decoder/source observations, not application screenshots or human
playtest results.

| Gate | Current bounded result |
|---|---|
|Factory/body/group linkage +shared decode +eight source-image openings|PASS within the static-body scope above; independently reviewed.|
|Final OE61 builds and regression|PASS: Release322/322, Debug119/119 and ASan/UBSan119/119; zero skipped tests.|
|Paid Rule18 baseline/final/independent restore and conservation|PASS within the actual six-source continuation below; the new Bronze chain remains PARTIAL/NOT_OBSERVED.|
|Scripted SDL size/zoom, picking, F4 and F5/F9|PASS within the matrices and independent before/after comparison below; physical image review is recorded separately.|
|Original animation, loaded states, full lifecycle and whole-scene parity|PARTIAL/UNKNOWN; not established by static bodies.|
|Native desktop/human playtest and Clean Mac delivery|NOT_RUN.|

### Frozen code, resource budgets and technical regressions

The tested runtime is Candidate02; the final test union03 changes only the
synthetic Production View assertion to the explicit unconfigured-art fallback
and strengthens its diagnostic checks. The first candidate had removed useful
Field Inspector information; the existing field test caught it and the tested
runtime restores the Farm/progress/stage and authored-calendar lines. Retained
failed attempts are not successful suite results.

`final-source-binding-03.json`, SHA-256
`d5cafcb23b4b0798e47b45e371dbe5c089aad168157385aeb6f63efa891740f8`,
binds the frozen source mirror and actual build inputs. All701 runtime files
match the reviewed working tree; all80 simulation/persistence files retain
their baseline bytes and modes. Later documentation-only edits do not alter
those tested inputs. Test binaries display the baseline revision plus `dirty`;
their exact binary/source fingerprints, rather than that display string,
identify the tested implementation.

The full baseline Release suite passed321/321. Final Release passed322/322 in
454.348s; the focused Debug and ASan/UBSan suites each passed119/119 in2628.163s
and1128.480s. The independent audit checks all560 actual test names/JUnit
results, runtime equality, authority preservation and sanitizer compile/link
flags on96 selected targets:
`.local/oe61/completed-ctest-independent-audit-01.json`, SHA-256
`e65218eea81115006127e074a3cd2ff1a64f1c73e6e42545fc3b3bc6a9ca7170`.
The external SDL dependency is not sanitizer-instrumented; LeakSanitizer was
not run on macOS. The matrix includes House/farm/field, production, industry,
market, fire, scene, alpha selection, recovery and the historical profiles.

The combined prepared pool has64 images and5,534,168 logical RGBA bytes,
versus56/4,770,512 in the baseline: eight eager images and763,656 extra bytes.
The64MiB limit is unchanged. Schemas1/2/3 retain37/39/56 images independently;
explicit custom core/no-tail/failed replacement paths retain their historical
behavior. One hundred pure renders leave all12 measured prohibited-work
counters at zero and the complete World unchanged. Frame-time/FPS were
**NOT_MEASURED**; image counts and logical pixel capacity are not latency or
resident-memory measurements.

### Paid visual city and an actual baseline renderer

The immutable earned66-body/46-courier Rule18/policy3/schema34 Xia/Normal city
at tick5395 has full SaveDocument SHA-256
`d0c0eddeb5024554c354340a25a89f6b5aa70c11b18da8c6d2178bc27053f424`.
The unchanged baseline World buys Smelter67 and Bronzeware68 for80+60 Funds,
producing a separate68-body/48-channel seed with SHA-256
`74b90881dc0405eae5d788296fd134352522e34c7738976116cd8ec14bb1cd5d`.
Funds fall5391→5251; no tick, resident, stock, road or pre-existing body is
edited. This initial city deliberately exposes the ordinary workforce shortage
(320 supplied/380 required, Maker68 at0/12). The visual inspection also found
that Smelter67 initially has no adjacent paid road. Its placement and empty
Inspector are valid visual cases, but are not evidence of a working chain.
Three explicitly tested west/north/east connector plans are refused by the
unchanged production road-admission API. No refused road is purchased, no
terrain exception is added and no claim is made about every possible route.

The completed continuation uses three separate processes: the old renderer,
the final creator and a final independent restore from a reference copy frozen
after creator completion. Each executes3200 ordinary Steps, tick5395→8595,
with3201 complete canonical F5-document hashes and complete World digests.
Eleven ordinary zero-cost commands set all nine industry priorities High and
pause/resume Logging at6195/6235; construction5487 and all roads remain exact.
There are17 checkpoint labels at14 distinct ticks,51 full F5/F9 byte-equal
pairs and105 complete saved-document physical-balance audits. The174 listed
file comparisons comprise116 cross-process comparisons and58 creator-copy
controls, not174 independent comparisons. All six goods/five auxiliary
balances, reservations, actual cargo, managed Ore and installed816-tick
maintenance invoices are independently checked. The bounded treasury trace is
5251→5926 =3629 received taxes −2954 upkeep; this is no lifetime-profit claim.

Actual additional completions are Logging32, Silkworm40, Weaver35, Refinery40,
Lacquerware26 and Tea40. Real Warehouse receipts include32 Wood,40 RawSilk,
40 Lacquer and finished Tea40/Silk34/Lacquerware26; Weaver actually receives38
RawSilk and consumes35, while Lacquerware receives28 Wood/23 Lacquer and
consumes26 of each using its retained opening stock. Dispatch is not counted
as arrival. Paper remains paused with progress47 and no new completion.
Smelter67 produces no Bronze; Maker68 receives eight actual Clay and reaches
12 assigned workers, but receives no Bronze and completes nothing. Their
new-body empty/understaffed cases are tested, while the full new Bronze chain
is explicitly **PARTIAL/NOT_OBSERVED** in this city. Warehouse empty/filled
checks concern actual individual goods accounts and positive stock; the
historical paid Rule10 Production View also checks zero Ceramics before Steps
and a later actual positive receipt. No all-inventory-empty native Warehouse
matrix or recovered loaded-state variant is claimed.

The independent continuation report is
`.local/oe61/paid-continuation-independent-audit-06.json`, SHA-256
`b8d7aff946bd5fa7168f21fbb12b8dbdd9ffd4cd25048cc9c1c928afa2824d54`.
Its closed outer guard preserves126,707 protected files/44,609,239,613 bytes.
The actual per-tick observations include natural industry-fire intervals
Logging56 [6420,7020) and Smelter67 [7395,7995). No fire is injected and no
Watch pause is needed to obtain these observations.

The66/46 matrix covers18 static roles plus its two field kinds (180 cases);
the68/48 matrix covers all20 static roles plus those two field kinds (198
cases). Both use1280×800,1600×900,1920×1080 at1×,2×,4× with explicit fresh F1
frames. Each of the eight new bodies has nine actual opaque source-pixel probes
strictly above the complete projected footprint, exact paid BuildingId
selection, transparent click-through and the expected prepared SG3 Inspector.
These checks are separate from ordinary ground selection and category-named
captures, whose final alpha-picked neighbor can differ from the filename.
Complete World/F5/F9 equality, culling, scene order and failed optional-art
replacement are checked without granting production or service authority.

For BEFORE, the same View test is compiled against the actual frozen baseline
headers,22 View objects and13 baseline archives, using baseline metadata.
AFTER uses the final runtime and the same full68/48 SaveDocument. All221 common
readback rows match camera offset/zoom, viewport/aperture, selected ID, F1 state,
output size and tick; full authority and F5/F9 bytes remain equal. All72 new-body
pixel witnesses (eight bodies ×nine combinations) match the selected source RGBA
in AFTER and differ from BEFORE. This is not an F4-only comparison.
`view-before-after-independent-audit-02.json`, SHA-256
`e6345a56440c694996c2b55fa938855addac77a71c644e2f7f07f83ffe9befce`,
binds the comparison. Private captures are below
`.local/oe61/baseline-view-overlay-01/repo/.local/oe61/paired-views/` (109 BEFORE
PNGs) and `source-final-02/.local/oe61/` (the corresponding117 AFTER PNGs,
including eight additional F4 controls). They are intentionally absent from Git.

Ancestor214 has an additional three-resolution check using the actual panel
wheel and a fresh F1 frame; its complete wrapped SG3/#2232 lines are visibly
onscreen. Small Inspectors otherwise require scrolling. Dense F1 HUD text,
4× scene-aperture clipping, baseline900-wide menu-footer clipping and local
neighbor occlusion remain bounded presentation limits, not whole-scene parity.

The closed base capture inventory has1986 physical PNG/BMP files:1982 generated
files individually opened and four unchanged authored repository icons. Its545
distinct generated SHA contents do not substitute for those1982 physical
openings. All25 review ledgers and2020 dependencies are rechecked against the
complete actual membership, SHA/bytes/mode/exact mtime and source/producer pins:
`all-physical-image-review-final-01.json`, SHA-256
`f53418ed2741adaffd427570bc0c2abe7e2e1a86fc36f05b2aa4b76fb08fba7c`.
This includes retained failed-attempt captures; opening those does not turn the
failed attempt into a successful regression run.

Six additional final captures restore the actual Logging56 fire checkpoint at
tick6420, full SaveDocument SHA-256
`21711d26e71766c9e54845328b0d12eb9da9473134e7965a95216d2cf7cd66af`.
Normal/F1 views at all three resolutions show the native2726 body and the real
existing fire overlay with no fallback, no additional Step/World command and
exact complete World/F5/F9 bytes. The scrolled complete SG3/#2726 row is also
checked through actual submitted text pixels. The six physical PNGs were each
opened; Root review SHA-256
`37b48fbc2157213d1c05180c8f84752e3bde6e97f73d5bb21f95222a81c1ff62`.
`logging-fire-inspector-result-04.json`, SHA-256
`df01fdd0ab4b75b45b26d90ab3960a8d7608c717207d661332e66b9b7e56b7ed`,
binds this final fire check. It does not select an original burning-building
body or infer an original lifecycle animation.

The final additive inventory is1994 PNG/BMP files: **1990 generated physical
files individually opened**, plus the same four repository icons. The eight
additions include the six final fire captures and two separately opened
retained-attempt fire captures. All551 distinct generated SHA contents and all
physical paths are accounted for; zero generated files lack an opening witness.
`all-physical-image-review-final-02.json`, SHA-256
`8ab93699513ef5fe52185cd2f1eed299291c09434c67d6dd653dbedc2ec5a79a`,
binds the exact final membership and full file/producer/review pins. The base
inventory and earlier evidence remain unchanged.

### Historical saves, Golden City and culture

The historical corpus comparison preserves152 literal JSON files across save
schemas1–39:123 top-level SaveDocuments,25 DTO restore controls and four case
indexes. These are not152 distinct SaveDocument generations. It includes132
historical files and20 independently exported schema35–39 files. The Rule18
Golden comparison has99 files:93 stream files, four recovery files and two
metadata files, at the seven named checkpoints seed/first-success/8160/midpoint/
16320/24480/result. This is not an every-tick Golden comparison.

Rule22 and Rule23 culture each pass six fresh independent processes with exact
full-save/recovery comparisons. Their40/31 comparisons each include one seed
self-check; another18 complete protected Rule23 documents compare exactly.
`history-golden-culture-results-02.json`, SHA-256
`c5ff22a021bbbd94bf9dcf55b18d990520c3ca6aa871aefbc03afcba416d5d6d`,
binds the closed history/Golden/culture runs and protected-input checks. No
historical rule or schema is migrated.

Use the isolated launchers and focused commands in
[testing](../testing.md#original-building-visuals-and-farmfield-boundaries-oe52oe60oe605).
All research, original-data runs and closed evidence use full before/after
SHA/bytes/mode/mtime/membership/symlink checks; no stat-cache shortcut or human
playtest-save search is used. Scripted SDL evidence remains separate from
native desktop delivery, human playtesting and Clean Mac acceptance, all
**NOT_RUN**. Static publisher evidence does not reconstruct the original
animation cadence, loaded state, full lifecycle or calendar.
