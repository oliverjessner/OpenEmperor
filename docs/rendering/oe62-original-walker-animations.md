# Original walker families and animation

This is the bounded presentation contract for OE62, based on the OE61 merge
`8d4643af7fa4037f2ece84f831f9d8e3f108a3ef`. `World` remains the sole movement,
actor-lifetime, transport, service and save authority. The metadata-only
schema7 visual profile uses the existing SG3 loader, `WalkerVisualProfile`,
`WalkerPose` and `WalkerSpriteSet`. It introduces no gameplay rule, save schema,
actor, delivery, destination selection or migration.

## Actual roles and retained limits

The numeric values below are `CourierRole`, not Original Actor subtypes or
profession IDs. Except where stated, the stable movement identity is the actual
World `CourierId`; the renderer uses its real position and current path edge.

| World role / value | OE61 display | OE62 complete profile | Original profession binding |
|---|---|---|---|
|None /0|No actor|No actor|UNSUPPORTED|
|Clay /1|Curated109 family, four phases|Same native family, twelve phases|AUTHORED Clay assignment; the same family has a separate evidenced Acupuncture208 binding.|
|Pottery /2|Curated217, four phases|Same native family, twelve phases|UNKNOWN; remains curated.|
|Household /3|Curated433, four phases|Same native family, twelve phases|UNKNOWN; remains curated.|
|Food /4|Curated3605 supplier|Same twelve-phase family|UNKNOWN; remains curated.|
|Service /5|Curated1417 civilian|109, actual Acupuncture208 family|EXE_OBSERVED for Original208 sources; classic Service use remains AUTHORED.|
|MarketPotteryInbound /6; MarketFoodInbound /7|Curated3605 supplier|Same twelve-phase family|UNKNOWN; no cargo-specific artwork claim.|
|MarketPotteryDistribution /8; MarketFoodDistribution /9|Curated5585 distributor|Same twelve-phase family|UNKNOWN; remains curated.|
|FireInspector /10|Curated433, twelve phases|Same native family, corrected cardinal columns|UNKNOWN; Inspector is still an OpenEmperor association.|
|HealthWorker /11|Curated2925 civilian|1813, actual Herbalist207 family|EXE_OBSERVED for Original207 sources; classic Health use remains AUTHORED.|
|OriginalHempToWarehouse /12; OriginalFoodToMarket /13|Curated3605 supplier|Same twelve-phase family|UNKNOWN; no invented loaded variant.|
|OriginalFoodToHouse /14|Curated5585 distributor|Same twelve-phase family|UNKNOWN; remains curated.|
|OriginalAncestor /15; OriginalWater /16|Named markers|Same named markers|PARTIAL; no new family admitted.|
|OriginalDaoist /17; OriginalBuddhist /18; OriginalConfucian /19|Named markers|Same named markers|PARTIAL; selectors alone do not admit additional images outside the budget.|
|OriginalMusic /20|Marker|7076 Musician34 family|EXE_OBSERVED family; historical school-to-House policy remains authored.|
|OriginalAcrobat /21|Marker|8541 Acrobat32 family|EXE_OBSERVED family; historical House policy remains authored.|
|OriginalDrama /22|Marker|7205 Actor33 family|EXE_OBSERVED family; historical House policy remains authored.|
|OriginalLuxuryToWarehouse /23; OriginalAuxToWarehouse /24; OriginalAuxSupplier /25|Named markers|Same named markers|PARTIAL; no guessed commodity/profession family.|
|AuthoredLandTrader /26|Curated217 Pottery figure|Same expanded figure|AUTHORED; no Original Trader identity.|
|OriginalMusicPerformer /27|Marker|7076 Musician34 family|EXE_OBSERVED visual; real `PerformerId` is distinct from its fixed school channel.|
|OriginalVenueMusic /28|Marker|9339 Theatre Actor36 family|EXE_OBSERVED visual; real `VenueWalkerId` is distinct from its fixed Theatre channel.|

Immigration has no separate current CourierRole or actor. Actual resident
admission changes the paid House through World; drawing an extra immigrant
would invent an entity. It remains **UNSUPPORTED as a visible walker**.
The School performer exists only between successful spawn and real venue
arrival. It retires without a return leg. The Theatre walker has its actual
outward/House/return/retirement lifecycle. Idle school and Theatre channels have
no visible actor or physical position. Composite native Music/Acrobat/Herbalist
artwork is one image for that one actual identity, not additional simulated
people, cargo or service effects.
Classic Clay/Pottery/Household, authored Trader and legacy direct Food retain
their historical idle visibility. Other source channels hide at their actual
idle terminal; rendering does not invent an actor waiting at the source.

## Sources, sequences and directions

The read-only original installation and private source observations stay under
ignored `.local/`. The original EXE is never executed. The
[culture source contract](../reverse/original-culture-source-contract.md) and
the maintained [medical/visitor source contract](../reverse/original-service-spawn-contract.md)
separate actual factory, Actor and source-selector evidence from model labels.
Historical [Inspector](../reverse/fire-inspector-visual-audit.md),
[Market](../reverse/market-walker-visual-audit.md),
[Service](../reverse/service-walker-visual-audit.md) and
[Health](../reverse/health-walker-visual-audit.md) audits retain their old scope;
they do not become profession proofs retroactively.

The shared `RuntimeArchiveLayout` has an explicit
`WalkerSprites38And39` evidence selection for the examined version214
SprMain/slot38 and SprMain2/slot39 registrations. Its ordinary version213 map
admission remains exact. `ResourceGroupLookup` resolves groups through that
same prepared layout; unknown registrations fail closed. Physical record
numbers are never treated as packed runtime resource IDs.

Every admitted family has twelve source phases, stride8 and native columns0/2.
For base `b` and phase `p=0..11`, the ordered native records are `b+8p` and
`b+2+8p`. No phase is skipped or reordered. The full source interval contains
96 records; it is not 96 textures.

| Profile role | Archive | Base / source selector key | Native width × height range | Binding |
|---|---|---|---|---|
|clay; service|SprMain|109 /4c03|29–38 ×47–52|Curated Clay; Acupuncture208 via51be10.|
|pottery|SprMain|217|33–47 ×43–55|Curated.|
|household; fire_inspector|SprMain|433|27–35 ×44–50|Both curated.|
|supplier|SprMain2|3605|63–72 ×46–62|Curated.|
|distributor|SprMain2|5585|37–39 ×48–53|Curated.|
|health_worker|SprMain|1813 /4c1e|60–61 ×47–61|Herbalist207 via51bd50.|
|music|SprMain|7076 /4c67|48–60 ×63–70|Music211/Musician34 via48b000 and actual school actor publication.|
|acrobat|SprMain|8541 /4c94|28–32 ×56–63|Acrobat212/Actor32 via48b1b0 and actual school actor publication.|
|drama|SprMain|7205 /4c6b|26–31 ×47–52|Drama213/Actor33 via48b360 and actual school actor publication.|
|venue_music|SprMain|9339 /4ca0|26–33 ×66–73|Theatre75/Actor36 via48b7c0 and shared visitor publication.|

The pinned source files are SprMain.sg3 SHA256
`3e2817d2644453acc92068d6c1e26532212be213b43b848ebe95ba21654adef8`,
SprMain.555 `d84eb6759e9ce50b0ad75596773b9224f9c1a9b8dd80b691c066c3de7e8df438`,
SprMain2.sg3 `09fc9fccddb9a30266740325b01cf69c53b26ed08016f9ecdd1759e7afb40b39`
and SprMain2.555 `53b1a9b5154316e505ac1a0d46b6796c1f568f4765ec154e313be0aff82daa7e`.
All four fingerprints must match before the optional complete pack is selected.

| Real storage edge | Screen direction | Native source | Display |
|---|---|---|---|
|neg_y|Upper-right|Column0|Native|
|pos_x|Lower-right|Column2|Native|
|neg_x|Upper-left|Column0; original mirror column6|Horizontal reflection|
|pos_y|Lower-left|Column2; original mirror column4|Horizontal reflection|

The opposite records have zero payload, signed relative offsets−6/−2, equal
dimensions/Y and reflected X. They are inspected as references and **never
decoded as pixels**. The new compact clips point to the same24 physical aliases,
with a strict boolean transform per direction. This corrects the earlier core
and Inspector use of columns0/1/2/3 as if they were four cardinal directions.
The old schema1–6 profiles preserve that historical interpretation unchanged.

## Feet, cadence, standing and alpha

`SOURCE_VERIFIED` covers original pixels, record dimensions, count/stride and
mirror relations. `EXE_OBSERVED` covers the narrow source/Actor bindings above.
`MODEL_OBSERVED` prices, labels and employees do not prove sprite professions.
Foot interpretation, global phase origin, idle selection and World cadence
remain `AUTHORED`. Unknown Original clocks and full idle/action states remain
`UNKNOWN`; unrelated roles remain named fallbacks rather than assumed parity.

Explicit feet use metadata X and Y−8 for the selected SprMain figures. The
Supplier retains its existing metadata-X/Y choice; Distributor keeps X/Y−8.
Changing canvas sizes does not recenter the actor. Continuous display reflection
uses `width−foot_x`; discrete alpha picking uses `width−1−pixel_column`.
The shared transform helper composes an existing frame transform and the new
clip transform with XOR. Drawing, culling, diagnostic bounds and F1 picking use
the same effective foot and reflection. `idle_flip_x` preserves a static
display orientation without allocating another alias or image.

Core Clay/Pottery/Household retain four World ticks per frame. Inspector,
Market, Service and Health retain two. Newly displayed culture roles reuse
that existing authored two-tick cadence. Raw Original speed ID4 and the
base-plus-phase-times-stride helper do not establish an Original time unit.
Moving phase is `(world_tick / ticks_per_frame) % 12`; World pause freezes it.
Waiting later-role couriers use the first frame facing the actual next edge;
no-edge waiting and historical core waiting use the explicit static idle.
Begun edges still move until World completes them. Rendering never corrects
positions or turns a prepared route into movement, goods or service.

All selected images use the unchanged Type256 Omega decoder and existing
alpha-free byte59 shadow rule. Exactly opaque RGB555 `0x7c00` shadow markers
become black alpha128 once during preparation; skip transparency stays0 and
ordinary pixels255. No new color key, SG3 mirror decoding, inferred load state,
unloading, extinguishing or original idle/action animation is admitted.

## Preparation, compatibility and resource limits

The complete schema7 pack has twelve visual roles, **288 named frame aliases,
240 deduplicated physical images and 2,145,360 logical RGBA bytes**. Clay/Service
and Household/Inspector share their native physical assets. Four clips refer
to two native rows without creating mirrored pixel buffers or hidden aliases.
The existing 320-alias/256-image/64-MiB limits remain unchanged. Historical
schemas1–5 retain256 aliases and schema6 retains320.

The complete optional pack is prepared and published atomically. Missing,
malformed, unsafe, fingerprint-mismatched or over-budget data retains the
historical core and independently qualified optional families. Custom walker
selection suppresses built-in replacement. An explicit custom schema7 remains
custom evidence, not an automatic Original-profession certification.
Preparation additionally resolves the selected families through the shared
runtime group tables and validates all native record identities, ordered
phases, mirror relationships, explicit feet, authored cadence and static idle.
A coherent custom Music clip made from the wrong original profession's images
can remain a generic preview but cannot qualify as the built-in Musician.
Original sessions prepare the existing House/body artwork before attempting
the complete walker replacement. Genuine aggregate pressure retains the
historical Fire priority: an incoming Fire and the complete legacy walker
fallback are staged before either current owner is replaced. The old
Inspector→Market→Service→Health prefix fits within the remaining budget;
failure never publishes a partial schema7 culture pack. Budget enforcement
around an already valid Fire reuses its existing textures.
Normal frames and hover perform no file/SHA/SG3/decode/upload, World copy,
command/tick, BFS, route refresh or full-map scan. All new work is session or
explicit asset-preparation work.

F2 retains the marker comparison. F3 shows the selected family, physical
record, phase, effective foot/flip and evidence. Real walker inspection
distinguishes a curated association from a source-bound family with authored
feet/timing. A source-qualified figure does not establish Original World
movement, service policy, actor roaming, original saves or whole-scene parity.

## Validation scope

Use the isolated commands in [testing](../testing.md). Source contact sheets
show all four directions and twelve consecutive source phases, including the
last-to-first boundary. Eight new-family sheets were individually opened by
both researcher and integration review; Original composite imagery remains
one bitmap per actual actor. These are asset checks, separate from productive
World frames, software-SDL picking and native human acceptance.

Native human delivery/playtest, external-display behavior, Clean Mac acceptance
and complete Original gameplay/animation timing parity remain **NOT_RUN** or
**PARTIAL** as applicable. A generated screenshot or one passing image cannot
establish a running animation or a complete profession reconstruction.

The independent OE61/OE62 history comparison covers schemas 1–39: 123 complete
SaveDocuments, 25 complete DTO restore controls and four indexes, all literally
equal to the protected references. The Rule18 Golden City comparison covers
93 complete streams, four Recovery payloads and two histories with only their
documented local metadata differences. Its seven complete World/SaveDocument
checkpoints are ticks 0, 5395, 8160, 15595, 16320, 24480 and 25795. Normally paid
Xia Rule22 and Rule23 each pass six separate creator/restore processes at
0, 103 and 20400 continued ticks, including complete Recovery payloads and
Rule23's paused controls. All 80 simulation/persistence authority files remain
byte- and mode-identical to OE61; all 5300 protected source/reference files
retain their hashes, sizes, modes and mtimes.

The actual-data initialization test also checks positive headroom and exactly
64 MiB of prepared texture demand. With headroom, all 240 walker images and
64 building images load. At the exact limit, all 176 historical walker images
and all 64 building images remain prepared, including the paid starter and
initial-save restore. Complete World, canonical F5/F9 bytes and paused UI
messages match. Debug and ASan/UBSan pass; a private control removing only the
preparation-order fix fails the building-priority assertion. LeakSanitizer is
disabled on macOS and is not claimed as tested.

## Finite render and memory comparison

The paired Release measurement uses SDL 3.4.14's software renderer, the same
paid Rule18 World at tick 5395, the same actual Courier18 position, camera and
map viewport. Each case warms 20 frames and measures 100 actual
hover/update-zero/render/Present frames. Readback, saves and snapshot comparison
remain outside the timed batch. All twelve prohibited-work counters stay zero;
the complete World, camera and three canonical F5/F9 checkpoint pairs match.

| Resolution | Zoom | OE61 median / p95 ms | OE62 median / p95 ms |
|---|---:|---:|---:|
|1280×800|1×|1.928 / 1.971|1.958 / 2.052|
|1280×800|2×|9.429 / 9.531|9.555 / 9.951|
|1280×800|4×|25.874 / 27.083|26.191 / 27.782|
|1600×900|1×|2.455 / 2.495|2.523 / 3.066|
|1600×900|2×|11.294 / 12.071|11.465 / 12.301|
|1600×900|4×|27.954 / 28.278|27.919 / 28.676|
|1920×1080|1×|3.180 / 3.466|3.255 / 3.307|
|1920×1080|2×|15.170 / 16.753|15.591 / 15.902|
|1920×1080|4×|30.919 / 31.585|31.710 / 32.727|

The median differences span −0.12% to +2.78%. Walker storage grows from
176 images / 1,358,208 logical RGBA bytes to 240 / 2,145,360, an increase of
787,152 bytes with the same 288 named aliases and unchanged limits. Peak whole
process RSS is 231,817,216 versus 239,960,064 bytes; it includes preparation and
the test harness, so it is not the texture budget. Measured initialization is
536.482 versus 629.322 ms. This is one finite pair on the local Mac with user
browser activity in the background; it does not establish an FPS guarantee,
statistical significance or another renderer/device's performance.
