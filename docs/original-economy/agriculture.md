# Agriculture: integration architecture and evidence gates

This is the maintained design for completing civilian agriculture in one
explicit Original-economy lineage. **Rule24/policy3/schema40 remains frozen with
paid crop construction and Food foundations; crop production is locked.**
The current explicit Agriculture generation is Rule25/policy3/schema41. AGRI-03
adds the three material plantations using retained, expressly authored growth
and real worker pickup/return. Rule25 expressly retains the existing authored
Wheat/Millet80-tick Food adapter; Rice/Cabbage/Soy and the unproved Original
field clock stay closed. The recommended Rule18/policy3/schema34 and all
historical Rules1–24 retain their own prices, quantities, identities and saves.
Current plantation validation is reported separately from implementation.
Rule26/schema42 remains the separately selected irrigation extension. AGRI-06
adds a derived view of the shared World calendar and verified seasonal reference
queries. It adds no rule, schema, independent clock or production activation.

## Evidence and scope

Read the [agricultural source contract](../reverse/original-agriculture-source-contract.md),
[existing production register](oe55-production.md),
[Food operations](../reverse/original-goods-consumption-food-contract.md),
[luxury producers](../reverse/original-luxury-source-contract.md),
[map admission](../map-permissions-compatibility.md) and
[current architecture](architecture-map.md) together. Model labels, raw columns,
map markers, artwork and a successful dispatch do not prove paid production.

| Classification | Use in this design |
|---|---|
| `SOURCE_VERIFIED` | Unmodified source-file structure, named configuration or exact table identity. This alone does not establish an active EXE consumer. |
| `EXE_OBSERVED` | Bounded statically verified factory, selector, state or callback dataflow in the pinned Original. Not execution of the Original or complete lifecycle parity. |
| `MODEL_OBSERVED` | Model row identity and raw fields, not admission, footprint, resource quantity or clock units. |
| `AUTHORED` | An explicit OpenEmperor choice, including current harvest periods, staffing, cart mixing and batch scores. |
| `UNKNOWN` / `UNSUPPORTED` | An unproved mechanism / currently unavailable gameplay. Neither gets a guessed default. |

Required source coverage is Farmhouse193; Rice197, Cabbage198 and Soybean199;
OrchardShed29; FishingQuay31; Hunter33; Ditch202/Pump203; and
FishingPoint179/PreyPoint183. Wheat195, Millet196, Hemp192/194 and the existing
237–239 material plantations are the AGRI-03 integration scope.

The new source investigation qualifies field/resource helpers, source factories,
bounded field schedule/state observations, actual hunting/fishing work and return
paths, and the pump/ditch connectivity path. It does **not** establish complete
seasonal yields, real-time units, fertility, ecology or all transport admission.
In particular, the observed Soybean199 selector resolves raw resource1
`BeanCurd`; a new soybean commodity or fruit recipe must not be invented.
Orchard29 opens submenu30 in the checked UI path; its concrete choices are
Lacquer Refinery238, Silkworm Shed239 and Tea Curing Shed237. Its explicit factory
path creates a generic non-producing Industrial object, with registered side0
and body group0, rather than a Farm/Orchard. The shipped manual's Orchard Sheds
section (pp45–46) describes the same three plant chains. The complete pinned
resource-name table and Trade catalog contain no Fruit commodity. These bounded
facts rule out implementing an Original fruit producer from the29 label.
The observed Farmhouse return path selects one of five resource-tagged bins;
that is neither today's two-column adapter nor a requirement that the complete
Food catalog contain exactly five resources. Raw farm-cart400, hunter-return50,
fish-return100 and conditional100→400 outgoing cargo need a common unit/effect
contract before integration. Clamped source-bin overflow and the cause of the
conditional cargo branch remain explicit accounting gaps.

## Historical authority and shared foundations

Historical `OriginalFoodState` contains exactly two authored crop columns. Its
Farm/Market/House stocks, `loaded-delivered-returned` cargo, actual-arrival
witnesses and checked owner/global equations are the sole Food authority.
Warehouses currently have **no Food account**. Six household Goods and five
auxiliary materials are separate inventories and cannot be reinterpreted as Food.

`FoodBatchArithmetic.h` now provides checked volume/history sums, immutable
canonical-prefix portion selection, authored mixed-cart selection and weighted
batch scoring for any nonempty fixed array dimension. Current production calls
these helpers with its unchanged two-column arrays and20/80 scores. Farm checks
iterate the actual array rather than using `1-crop`. AGRI-01 introduced no new crop ID, source,
account, cart, resource, timer, serialization field or permission. AGRI-02 adds
the separately versioned foundations below. Rule25 reuses those Food
foundations unchanged and adds material plantation work in its own saved state.

These are arithmetic functions, not a second ledger, production simulator or
resource catalog. Multi-kind tests use independently authored synthetic examples;
they establish arithmetic behavior, not Original rice, hunting or fishing rules.
The existing Wheat-first sink, mixed load choice, original House-quality blend,
80-productive-tick harvest, capacities8/16/8, cart4 and movement5 stay exact.

## Agriculture crop foundations and activation gate (AGRI-02)

Historical Rule24/policy3/schema40 has marker `authored-agriculture-v1`, catalog
`wheat7-millet5-rice6-cabbage3-beancurd1-v1`, Agriculture revision28. Shared
Original production metadata remains revision27. It inherits Rule18 economy,
services, deadline-aware Food dispatch, fire and source-bound terrain, without
Rule19 demolition, Rule20 terrain, Rule21 trade or Rule22/23 venues. Defaults
remain unchanged. Its create/restore and existing saves remain exact. Current
**Agriculture** new games and `--original-agriculture` select Rule25 below;
loading an older save retains its saved generation without conversion.

`AgricultureState` is the sole five-kind Food authority. Legacy
`original_production.food` is strictly empty in Rule24. Indices0..4 are Wheat,
Millet, Rice, Cabbage and Soybean; the last represents the source199 output
**BeanCurd resource1**, without an invented raw-soybean processing recipe.
Historical `OriginalFoodState`, its two columns and all schemas1–39 stay exact.
The shared typed Food engine handles both widths; there is no second navigator,
route cache, transport simulation or mirrored aggregate stock.

**Verified construction facts:** Farmhouse193 has a complete3x3 footprint;
fields195–199 are1x1. Prepared model/difficulty arithmetic charges every Farm
and Field separately (pinned source base193:50/22 employees;197:7/0;
198/199:5/0). All ordinary shared map/occupancy/footprint rules still apply.
The source work box is coordinate-wise −3..+5 relative to the Farm origin.
Rule24 binds each field permanently to an existing paid Farm in that box: an
explicit selected Farm must qualify; otherwise nearest Farm center wins, then
BuildingId. This immutable ownership/tie rule is **AUTHORED**; source farm search
does not establish immutable ownership. One Farm can own multiple different
crops. Existing limits remain3 Food Farms,82 total bodies,58 movement channels,
20 Houses; the new crop-field cap is32 across all Farms. Neither Fields nor UI
create workers. New Farm/Field demolition is refused without refund or stock loss.

**Rice admission:** consume the separate serialized fertility byte plane through
the existing map reader and bind it to the full map SHA. Source197 requires
fertility exactly100. Water proximity, a terrain fertility-looking bit, a
renderer overlay or the mere presence of a Pump never substitutes for that
operand. Recreate complete immutable fertility on load and recheck every Rice
cell. A universal Rice-Pump requirement is not established by the checked source
path. Fertility100 admits Rice construction/identity only; all growth/harvest
remains behind the Original work/timing gate below. Irrigation's network,
operation and yield bonus remain AGRI-04-only and grant nothing in Rule24.

**Original work/timing gate:** `agriculture_production_clock_verified` is false.
The source phase20 driver calls the field growth path once per51-phase cycle;
calendar subslots run0..15 and outer slots0..11. Recorded start/end slots do
**not** prove a four-slot growth window: the checked callback suppresses growth
in the configured end slot, otherwise uses actual field work, returning157/79/0.
Actual Grower arrival and its countdown invoke tending; another actual Grower
state/countdown invokes harvesting. Slot entry and full Farm staffing alone do
not prove either transition. Complete actor update/movement/arrival timing,
work decay/reset, harvest admission and yield conversion remain unclosed.
See the exact bounded [source evidence](../reverse/original-agriculture-source-contract.md).

Consequently Rule24 creates **no Food**, grows no field quantity and grants no
House FoodQuality, including when staff, road access and calendar slots are
available. Pause/fire/full stores/year rollover cannot bypass this gate.
Restored nonzero field quantity, work/harvest counters or harvest-cycle state
are refused. No160-step growth,63-event harvest, fixed four-portion yield or
invented Original harvest time is activated. UI/menu/inspectors explicitly say
production is locked. This generation cannot be recommended as a functioning
agricultural economy. Activating actual work later requires a newly selected
version/schema; Rule24 saves must retain their closed contract without migration.

**Transport and consumption:** capacity8 Farm/16 Market/8 House, physical Cart4,
existing five-tick legal edges, actual-arrival credit, source return room and
complete `loaded-delivered-returned` cargo remain shared. The typed transport foundation represents all
five distinct kinds; Warehouses still have no Food account. Mixed loads retain
canonical per-kind quantities. Its shared consumption code preserves one-portion demand
in catalog order; it cannot substitute or relabel a crop. Full recipients and
road cuts preserve cargo/reservations until physical delivery or return.
For each kind, `harvest + received + returned = stock + departed + consumed` at
an owner, and `harvest = stock + outstanding cargo + household consumption`
globally. Checked counters and bounded per-field productive events/quantity/
harvest-cycle witnesses refuse impossible restored histories.

**FoodQuality foundation (Rule24 inactive; Rule25 Wheat/Millet only):** no complete
original five-food scoring rule is claimed.
The explicit authored scores are Wheat20, Millet80, Rice60, Cabbage70, BeanCurd90.
A batch uses its integer quantity-weighted mean; actual House arrival blends it
with previous physical stock using the existing verified integer blend. An
empty House stock resets quality0; consuming a nonfinal portion retains the
last witnessed blend, matching the inherited adapter. Full-width actual-arrival
witnesses and retained House evaluation inputs live only in Agriculture state;
legacy two-column evaluation evidence stays empty. Dispatch/proximity/restore
cannot grant quality or service. Inspection names these choices.

Cabbage/Soy field groups and six quantity-based source stages are prepared in
the separately selected schema5 visual profile. Quantity→stage selection in this
adapter is authored. The separate schema6 supplement adds47 native Rice paddy
bases with verified eight-neighbor topology and canonical variation0. They are
explicit inactive/unworked representatives: Original staffed/seasonal bank
selection and quantity overlays remain unavailable. Drawing supplies no clock,
growth or harvest authority. Schemas1–5 retain their exact selections and Rice
fallback. No original pixels or private selector tables enter the repository.

Save/load, F5/F9 and Recovery require exact schema40, mode/catalog markers,
full source/model identities, five-column records, complete paid identities,
field ownership, calendar bounds, physical trip bindings, conservation and House
witnesses. Unknown members, duplicate keys, relabeled schema34 documents, source
changes and cross-generation authority fail closed before atomic publication.
Agriculture writes also refuse an existing target from any historical generation;
an existing schema34 Wheat/Millet file keeps its exact bytes. Menu new-game
targets remain unique, and a CLI save target must be a separate schema40 file.

## Runtime data and ownership

| Component | Concrete design and integration point |
|---|---|
| Immutable catalog | Prepare a canonical, bounded ordered catalog once per session. Stable typed Food IDs, Original raw resource bindings, producer/field identity and supported sinks are distinct fields. An Original model ID, resource ID, pixel index and catalog index are never interchangeable. Require proven bindings; do not reserve a made-up fruit or raw-soy resource. Reuse `ModelData` and checked difficulty preparation, not a parallel model parser/admission list. |
| Versioned production state | Keep the legacy `OriginalProductionState` alternative unchanged. Introduce a generation-tagged Agriculture production alternative, reusing existing non-Food state types and kernels. Exactly one active Food representation exists per World; no authoritative legacy array plus mirrored Agriculture stock. Generation-aware accessors reject the wrong alternative. |
| Food accounts | New generation uses catalog-sized physical amounts and counters with explicit bounded owner roles: admitted producers, eligible stores/Markets and occupied Houses. Extend the existing Food accounting/dispatch integration for any proven Warehouse sink; never also credit six-goods Food or legacy actor cargo. Capacities and units need their own evidence or approved authored contract. |
| Fields and plants | Typed monotone Field/Plant IDs bind complete paid footprints to compatible paid sources. Rule24 fields and Rule25 plants have bounded multiple attachments; historical adjacent single-plant adapters stay exact. Ownership selection is explicitly authored. No free attachment, orphan field, double ownership or visual-stage-derived harvest. |
| Producers and yields | Separate preconditions, work progress, mature quantity, harvested stock and real return. Farm schedules, orchard harvesting and fishing/hunting callbacks may use different state transitions; share checked inventory operations and existing World tick integration, not a fabricated universal80-tick recipe. A due callback is an attempt, not a successful actor or output. |
| Work actors | Each actual successful spawn has a typed monotone lifetime ID, paid source binding and existing bounded movement channel. Work, outbound cargo, return, waiting and retirement are distinct. Only the actual evidenced work/return transition may create credited output; target selection, dispatch or a drawn figure creates none. |
| Goods transport | Reuse World couriers, route caches, incoming/return reservations and the sole per-trip sequence. Preserve begun edges, waiting, cargo and reservations through source pause, fire, staff loss and road cuts. Batch arithmetic operates on the selected catalog; mass credit occurs once at accepted actual arrival. |
| House consumption | Keep one House Food quantity/quality authority with exact per-kind receipt/consumption witnesses. Reuse the evidenced blend only in its proven domain. New food scores, processing/quality rules and consumption priority need explicit evidence or an approved authored contract; do not apply20/80 to additional foods. Embedded housing evaluations must use the new generation's complete catalog witnesses, not truncated legacy arrays. |
| Calendar | One World clock. AGRI-06 derives the existing51×16×12 elapsed position from saved World ticks and retains mapping version1 as AUTHORED. This observation does not establish historical dates, climate or Original work authority. Future non-reconstructible season/work state requires an explicit saved contract; never add an independently advancing second date. Pause, speed and restore retain existing World ordering. |

All mutable rows belong to `simulation::World`. Presentation, inspectors,
model tools, original map objects and source-fact readers expose immutable
facts or derived views only. They cannot construct, tick, harvest, allocate an
actor, consume, renew, reserve or write an authoritative inventory.

Rule24 fixes five typed Food columns and their canonical order; Rule25 inherits
them unchanged. Source availability does not shrink or reorder saved arrays.
A later Food catalog change requires an explicitly versioned contract and its
own producer/unit evidence. The Original Farm's five bins do not define the
complete Original ingredient catalog.

An agricultural product binding must also state its inventory domain. Hemp
remains an existing household Good; Tea, Lacquer and RawSilk follow their existing
Good/auxiliary paths. They are not extra Food columns and receive no second
credit in an Agriculture catalog. Rule25 explicitly retains authored work/units
while adding actual plantation pickup and return. It never retrofits old saves.

## Terrain, irrigation and ecology

Prepare immutable source context through the existing map/resource readers,
complete map SHA, signed heights, Diamond/bounds, source protection and
`MapPermissions`. Extend the existing prepared terrain facts; do not create a
second map admission system. Store the exact source/entity identity for179/183,
not a map name, coordinate whitelist or generic type-range exemption.

Ditch202 has an observed chained terrain path; a raw table side does not make
it an independently purchasable1×1 building. Pump203 has orientation-dependent
geometry. Paid construction therefore needs authoritative full chain/footprint
claims, cost, terrain/height/intake predicates and access. World owns installed
ditch edges, pump operation and wet/dry state. The Original source uses some
image-state queries; our renderer must never become water authority.

Reuse existing cardinal edge legality and bounded prepared topology. Update
irrigation connectivity when admitted construction/operation/restore changes
invalidate it; refresh only explicitly scheduled effects during ticks. No frame
or hover BFS, full-map scan or all-pairs irrigation graph. A graph cache is
derived, never an independently saved water grant. Radius5 source observations
are not a blanket productive bonus until their complete predicates are proved.

Fishing/prey markers establish potential origins, not finite resource amounts,
animal populations, permission or free recurring output. Where proven mutable
ecology exists, World must own depletion/replenishment and typed work claims,
bound to immutable source identities. Define overlap, exhaustion, actor loss,
regeneration and restore equations before admission. A private original save
or a model row cannot supply guessed initial stock.

Land work, prey movement and boats require explicit edge classes and endpoints
in the existing navigator (`find_route`, `find_transport_route`,
`find_building_route`) and shared movement legality. Extend that authority for
evidenced off-road/water work rather than adding a separate hunter/boat
navigator or teleporting between a marker and producer. Landing, quay geometry,
protected edges and real arrival need independent tests and source predicates.

## Conservation, limits and update order

For every typed Food resource, each physical owner's account requires:

```text
harvested + received + returned = stock + departed + consumed + proven_losses
```

Across all owners and actual carrier trips, separately require:

```text
global_production = all_stock + actual_cargo + consumption + proven_losses
departed = received + returned + outstanding_cargo
```

The aggregate `departed` equation is not a per-owner identity: an owner's own
incoming receipts do not describe the recipients of its outgoing trips.
Only admit a loss/spoilage term if its mechanism exists in the final contract;
it defaults to no such operation, not an unexplained mass sink. Ecology has a
separate bounded equation for its actual resource/population state. Reservations
reserve capacity/return room and do not add mass. Validate all arithmetic and
resource/counter headroom before publishing a command, arrival or tick change.
Rejected operations and restore retain IDs, money, state and source bytes.

Declare exactly which real work transition creates a canonical Food unit.
Before that conversion, field quantity and pending raw yield remain typed work
state with their own units. If a worker already carries canonical Food, count
that payload in actual cargo exactly once and preserve its source return room;
do not mirror it into source stock before accepted physical return. Reconcile
this boundary with the source's offered yield, accepted bin amount and overflow.

Retain existing82 bodies/58 movement channels/20 Houses unless a separate
measured final-contract decision justifies different explicit bounds. Catalog
width, field count, terrain claims, ecology rows, work lifetimes, source stock,
history, payload bytes and per-tick traversal all need explicit bounded values.
A five-crop array is not permission for five new Farms or extra workers. Prepare
render resources eagerly within existing budgets; unsupported art stays named.

For future Original Food/ecology activation, canonical units and
accepted-versus-offered harvest must close before activation.
The observed farm helper clamps bin stock and returns overflow that its examined
caller does not use; exact loss handling remains unresolved. Hunter/Fishing
normally debit100 raw stock, but source byte`+0x70` can select400 cargo with the
same100 debit. Prove the complete effect and conversion or exclude that effect
explicitly in the final authored contract. Do not make either branch silently
manufacture mass, erase overflow or borrow the current four-unit cart scale.

Each historical Rule1–23 retains its own exact tick order. In particular, the
inherited Rule18 production path keeps finance→staffing→production→dispatch→
movement→fire→immigration→services/evolution/consumption. The future proved production contract must declare where
calendar, field work, ecology and irrigation effects enter that single World
update. Specify simultaneous harvest/dispatch, arrival/consumption and calendar
boundary cases; never let a UI refresh advance them or catch up by duplicating
production.

## Save and migration boundary

The historical reader/writer/normalization and exact DTO widths remain unchanged.
New Food IDs must not be appended to `OriginalFoodCrop` or serialized into old
two-element arrays, six Goods or five auxiliary slots. The Agriculture schema
has its own complete DTO and create/restore APIs, strict exact fields, positive
typed IDs, canonical catalog order and complete lifetime/counter bounds.

Bind full source map identity, model/difficulty, final catalog/behavior version,
calendar contract and any actually consumed configuration fingerprints. A
FarmConfig file's existence is not proof that it is an active runtime input.
Reconstruct immutable source context and derived indexes on restore; validate
ownership, masks, signed geometry, quantities, work/route/progress, real cargo,
reservations, consumption/housing witnesses, ecology, Funds and update order
before atomic publication. Preserve existing save size/nesting bounds until a
measured explicit contract changes them.

Menu load, CLI, F5/F9 and Recovery select the exact saved generation. Cross-rule
restore, unsupported catalogs, widened old arrays and header-only relabels fail
closed. **No automatic or implicit migration** exists. An eventual manual
conversion would be a separate explicitly requested design with a new output
file, complete validation and preserved input; it is not part of AGRI-01–07.

<a id="orchard-identity-boundary-agri-03"></a>

## Material plantations and completion matrix (AGRI-03)

The scope is Tea26/237, Lacquer27/238 and Mulberry28/239. Model29 is a submenu
marker for these sources on the checked UI path; the source catalogs contain
no Fruit commodity. No edible-fruit plant, resource, generic producer or Food
column is added. See the UI/factory/catalog and shipped-manual
[proof](../reverse/original-agriculture-source-contract.md#model29-is-a-submenu-marker-not-an-orchard-producer).

| Chain | Implemented World path | Automated completion evidence |
|---|---|---|
|Tea26/237|Paid readiness → real Grower pickup/return → existing Tea Good → Warehouse → Market → House|PASS: actual source return, physical deliveries and House consumption in World and Food-positive adapter controls; growth/cargo persistence and Recovery.|
|Lacquer27/238|Paid readiness → real Grower return → auxiliary Lacquer → Maker44 with actual Wood → Lacquerware → Warehouse/Market/House|PASS: returned Lacquer, independently produced Wood, atomic recipe debit, physical deliveries and House consumption; exact save/replay.|
|Mulberry28/239|Paid readiness → real Grower return → auxiliary RawSilk → Weaver47 → Silk → Warehouse/Market/House|PASS: returned RawSilk, actual Weaver debit, physical Silk arrivals and House consumption; exact save/replay.|

**Construction:** each shed is2×2, each separately paid plant1×1. Pinned model
base Cost/Employees are40/12 for each shed,6/0 for Tea Bush and5/0 for
Lacquer/Mulberry. Commands charge prepared difficulty-adjusted costs and validate
complete footprints. Maker44 is2×2 with base65/11; Weaver47 is2×2 with base60/11.
Source-proved range3 covers coordinates−3..+4 from a2×2 source origin on each
axis. A selected source must match; automatic binding requires a unique
qualifying source and refuses ambiguity. Immutable ownership and automatic
choice are **AUTHORED**. Multiple plants share a source;32 plants and at most
two sheds per type, six total, share the unchanged82-body/58-channel budgets.
Source/plant removal remains refused without refund or discarded cargo.

**Work:** `simulation::World` owns
[PlantationsState.h](../../src/simulation/PlantationsState.h). A paid plant
accumulates80 productive ticks under operation, full staffing, road and fire
gates, then holds one finite ready unit. Full stores retain readiness. Each
source owns one movement channel; successful spawns have distinct monotone
`PlantationGrowerId` lifetimes. Shared navigation and five-tick land edges carry
the worker to the plant. Only actual plant arrival picks the unit; only actual
source return credits the existing Goods/Aux ledger and completed counter.
Picked cargo stays on that worker until return. The retained plant starts a new
authored cycle after picking. Growth, dispatch, rendering and load credit no
output. Pause, road cuts and blocked paths preserve begun edges and cargo.

The80-tick period, unit yield, full staffing, binding, land routing and regrowth
are **AUTHORED**, permitted pending AGRI-04/06. Existing buffer8, Warehouse32,
one-unit processor recipes and downstream typed couriers keep their units.
Six House Goods and five auxiliary materials are unchanged. Plant counters
reconcile grown/ready/picked/returned work; the existing ledgers conserve actual
credited output. There is no second Tea/Lacquer/RawSilk inventory.

**Original fidelity:** work-dependent source quantity additions are Tea105/53,
Lacquer125/63 and Mulberry157/79. Raw seasons, work/harvest waits, harvest marks,
source return and a conditional irrigation yield factor are observed. Full actor
cadence, accepted initialization, exact yield units/rounding/global modifiers,
recurring seasonal regeneration and source-to-World time mapping remain unknown.
Rule25 activates none of those numeric growth values or source seasons. No
independent plantation calendar or irrigation engine exists; AGRI-04/06 must
close their shared effect predicates before a future version activates them.

**Presentation:** optional metadata-only `plantations.json` prepares16 verified
native plant images: four Tea and six each Lacquer/Mulberry. Selection
`authored80-source-quantity-canonical-unharvested-v1` projects saved authored
growth onto the evidenced quantity selector with variation0 and unharvested
source context. This projection does not establish Original timing or complete
harvest animation. Missing assets retain named plant markers; Grower26 retains
a named worker fallback without an Original animation/feet/action parity claim.
Preparation stays outside frame/hover loops and within unchanged budgets.
Inspectors expose actual binding, readiness, stocks, worker cargo and blockers.

**Version and saves:** current Agriculture new games select Rule25/policy3/
schema41, `rules.plantations: "authored-agriculture-plantations-v1"`, plantation
revision1 and catalog`tea26-237-lacquer27-238-mulberry28-239-v1`. Inherited
Agriculture revision28 and its five-kind Food marker/catalog stay mandatory.
Exact `create_original_plantations`/`restore_original_plantations`, F5/F9 and
Recovery validate paid identities, sorted rows, lifetime high-water marks,
readiness/cargo/return equations, chronology, paths and live source/model facts
before atomic publication. Unknown/duplicate fields, impossible counters,
invented Fruit authority and cross-generation relabeling are refused. Schema41
can overwrite only an existing schema41 target. No older save is migrated.

**Food and acceptance:** Rule24 still creates no Food or FoodQuality. Rule25
expressly activates the existing **AUTHORED** Wheat/Millet adapter in the sole
five-kind Agriculture ledger. Each Farm may own exactly one productive Wheat
or Millet field; another Wheat/Millet attachment is refused. Additional paid
Rice/Cabbage/Soy fields remain unproductive. Full staffing, operation and road access permit
one portion after80 productive ticks, using the existing Farm progress0–79 and
the selected field's cumulative harvested counter. No Original quantity,
productive-event or harvest-cycle authority is claimed: those fields stay0.
The inherited Farm8/Market16/House8 capacities, four-portion physical carts,
authored scores20/80, real arrival witnesses and actual consumption remain.
No Food is granted on construction, dispatch or restore, and no seasonal clock
is introduced. The user expressly authorized this bounded adapter for Rule25;
the Original Rice/Cabbage/Soy timing gate remains unchanged.

The earlier Food0/Water0 authored lower-House fixture remains a separate luxury
positive control. The new Food-positive control requires Food1 plus actual
Water/Ancestor service and earns Food, real residents, deliveries and all three
luxury consumptions through paid World commands and ticks. Neither synthetic
model establishes native-model city progression, Original plantation parity
or native human acceptance. The current automated checks pass in Release,
Debug and ASan/UBSan; see the [validation scope](../testing.md#agriculture-foundations-agri-01).

## AGRI-02B crop completion status

**PARTIAL.** The additional source investigation closes the ordinary relative
scheduler/Grower call chain and several exact field-work operations. It does
not close the complete production contract required for Rice, Cabbage and
Soybean to supply earned Food. Rule24/schema40 and Rule25/schema41 retain
their published production and save contracts.

The independent `original::CropRules` component implements the proved five
field/resource/calendar identities, the separate growing-period query and
growth callback, bounded quantity addition, tending bit updates, work decay,
raw base-yield division, runtime-fertility byte conversion and signed-word
work waits. The callback's end-slot veto is deliberately
separate from the growing-period query. Work decay retains only flags0xc000
and its decremented low byte. These are resolved-event arithmetic APIs. They
own no actor, field, World clock, inventory or save state.

The source parent invokes the scheduler and eligible actor update in the same
ordinary iteration. The previously approved one-World-tick-per-phase mapping
can therefore remain the inherited **AUTHORED** integration choice. Absolute
Original wall-clock speed is a separate claim. The remaining production gate
concerns post-purchase/restore effects, complete worker scheduling and yield effects, with
the exact evidence in the [AGRI-02B audit](../reverse/original-agriculture-source-contract.md#agri-02b-bounded-clock-and-grower-audit).

The Agriculture inspector now places the selected Farm/Field details before
the building list, lists every bound field, and exposes actual parent workers,
operation/fire state and per-crop Farm stock/departures. Field Growers are
explicitly unavailable in the released generations. The bounded quantity and
harvest counters remain read-only World observations. The existing inactive
Rice bases and quantity-based Cabbage/Soy sprites keep their current source
and lifecycle qualifications.

| Feature | Gameplay | Original fidelity | Tests | Open blockers |
|---|---|---|---|---|
| Wheat195 / Millet196 | Rule25 production, real deliveries, House consumption and luxury support | Existing approved AUTHORED80-tick adapter, without Original field-worker parity | Paid Food-positive five-field city, physical arrivals, both crop consumption, Tea/Silk/Lacquerware consumption and schema41 replay | Original replacement requires the same complete work/yield contract as the other crops |
| Rice197 / resource6 | PARTIAL paid fertility100 construction, binding, real Farm staffing and inactive art. Food origin remains locked | Ordinary constructor zeroes, relative clock, Food work/base-yield arithmetic and base-harvest irrigation branch are EXE_OBSERVED | Original crop-rule boundaries, paid five-field coexistence, negative production, inspector, visual and save/continuation controls | Post-purchase/restore effects, complete worker lifecycle, runtime yield modifiers, rounding and raw-to-Food unit contract |
| Cabbage198 / resource3 | PARTIAL paid construction, binding, real Farm staffing and quantity-based art. Food origin remains locked | Source identity/calendar/work operations independently implemented | Same bounded rules, paid coexistence and historical no-origin controls | Same complete work, yield and unit contract |
| Soybean199 / BeanCurd resource1 | PARTIAL paid construction, binding, real Farm staffing and quantity-based art. Food origin remains locked | BeanCurd identity retained without adding a processing building | Same bounded rules, paid coexistence and historical no-origin controls | Same complete work, yield and unit contract |
| Grower Actor26 | Material plantation Growers remain playable. Food Growers remain unavailable | Actual source/field assignment, wait and return callback observations are bounded | Exact signed-word wait/bit operations plus existing plantation movement/save tests | Source quota configuration, candidate tours, failed destinations and interruption lifecycle |
| Rice irrigation / AGRI-04 | Fertility100 admission retained | The checked false-irrigation branch continues base harvest. Successful Food irrigation adds a0.35 factor | Existing fertility/placement and rice-art controls | Runtime fertility preparation, irrigation topology/global veto and complete yield calculation |

### Successor version boundary

At the AGRI-02B audit, Rule26 and Schema42 were unused. AGRI-04 subsequently
assigned them to the explicit irrigation contract below. AGRI-06 adds no
successor generation or schema. The AGRI-02B investigation added no selectable
successor or new save writer because the outstanding lifecycle and yield
choices determine its saved authority. A second Rule25-equivalent menu entry
would provide no new playable crop behavior. Existing menu/CLI entry points
continue to expose paid Rice/Cabbage/Soy construction with their production
lock, and existing saves restore in their own generation.

The successor needs separate exact create/restore/menu/CLI/Recovery identity,
an explicit marker, and work/cargo records distinct from plantation one-unit
trips. It must inherit the six-Goods/five-Aux economy and sole five-kind Food
ledger. Its field work, pending raw yield, Farm remainder and incoming/return
reservations must reconcile before atomic save publication. The existing
three-Farm,32-field,82-body and58-channel bounds need a complete channel budget
before any new successful worker spawn is admitted. Adding a branch to the
historical exact Rule25 predicate would reinterpret a published contract.

### Decision for an independently authored successor

The user's proof-only crop requirement remains in force. A separate authored
adapter needs an explicit decision covering all of the following choices
before it supplies Rice/Cabbage/BeanCurd Food.

| Decision | Concrete integration choice to review | Required evidence or authorization |
|---|---|---|
| Clock | Retain the existing one-phase-per-World-tick mapping and51×16×12 calendar | Already AUTHORED in the inherited economy. Exceptional actor-only branches remain outside Original parity |
| Workers | Reuse shared land navigation, preserve paid immutable field bindings, and define bounded Farm-owned tending/harvest channels and interruption policy | Original spawn quotas/tours and failed-target lifetime need closure, or this worker policy needs explicit authored approval |
| Yield | Retain ordinary constructor zeroes, resolve complete post-purchase/restore effects, and provide runtime fertility modifiers, crop/region/global multipliers and final x87 rounding | Verified base-yield/fertility operations require explicit resolved inputs. Complete floating-point yield remains open |
| Food units | Specify conversion and rounding from returned raw yield to the existing Food portions, with retained fractional remainder and reserved return space | Display division by100 and a raw400 cart do not independently establish a World portion conversion |
| Conservation | Credit only actual Farm return, then use existing physical Farm→Market→House transfers and actual consumption | Retain existing no-duplication/accounting contracts. Define overflow handling before production |
| Rice | Retain fertility100 placement and admit an unwatered base-yield path when its inputs are fully defined | A universal Pump or water-distance requirement is unsupported. AGRI-04 can later supply the separately proved bonus |

A concrete **AUTHORED candidate**, requiring separate approval, would keep
Rule25 Wheat/Millet unchanged inside the successor and add the following policy
only for Rice/Cabbage/BeanCurd. These proposed values are unimplemented.

- Start paid fields with quantity/work/countdown0. Reserve at most one Food
  Grower channel per Farm inside the existing58-channel bound. Visit eligible
  owned fields in stable field-ID rotation using shared five-tick land edges.
- Admit tending only during the source start-inclusive/end-exclusive slot
  interval. After physical arrival, wait51 eligible work invocations before
  applying the proved tending effect. Run the proved growth callback at phase20
  and decay after phase50, with explicit World operation/fire/staffing gates.
- Mark the crop's end slot for harvest. At least1000 retained quantity admits
  one proposed Food portion. A real Grower waits25 eligible invocations after
  arrival, debits exactly1000 quantity per portion and carries at most four
  portions within reserved Farm headroom. Retain unconverted quantity. Record
  the new origin once at pickup and include the actual Grower cargo in the
  sole Food conservation equation. Only physical return credits Farm stock.
- A pause, fire, staff loss or road cut preserves begun movement and cargo.
  Stop further productive work and use existing waiting/return repair rules.
  Rice retains source fertility100 placement and receives no irrigation bonus
  until AGRI-04 supplies its World-owned effect.

Zero quantity/work/countdown agrees with the ordinary source constructor.
The one-channel policy, candidate rotation, work admission,
1000-quantity conversion and interruption choices are authored decisions.
Approval would authorize a playable approximation with the existing FoodQuality
scores and household logistics. It would not establish Original yield, actor
quota, irrigation or full lifecycle parity. Continuing the proof-only approach
instead requires closing the source blockers above before successor activation.

The new paid coexistence control contains all five field types within three
Farms, earns Wheat and Millet food and all three plantation luxury consumptions,
and verifies exact schema41 restoration and continuation. Its independent
Food-positive model is **AUTHORED**. It establishes preserved household supply
with the released adapter. Rice/Cabbage/Soy production, unchanged-native-model
higher-House progression and positive Food-Grower lifecycle acceptance remain
unfulfilled completion criteria for AGRI-02B.

## Crop completion and decision boundary (AGRI-02C)

**PARTIAL.** Rice197, Cabbage198 and Soybean199/BeanCurd1 still lack an admitted
World production origin. The AGRI-02C investigation adds verified dispatch,
event-qualified integer-base and raw transport operations to the existing
`original::CropRules` component. World, navigation, paid construction, inventories,
UI and save authority retain their released Rule26/schema42 behavior.

### Implemented source operations

- `crop_dispatch_rule` uses explicitly supplied nonnegative configured limits
  and the actual staffing percentage. `advance_crop_dispatch_attempt` preserves
  the old-byte comparison, including2/5/9/17/33 eligible-call cadences from0.
  The pinned Food initializer supplies limits5/2 and field budgets4/10.
  Those retained source counters have incomplete cleanup/census semantics.
- `crop_doubled_base` resolves the enable byte, raw state3 and crop selector.
  Selector0 admits Wheat,1 admits Soy,2/4 admit Millet. Rice and Cabbage use
  ordinary base division in this checked event branch. Existing integer
  `raw_base_yield` and fertility arithmetic remain shared.
- `CropTransportRules` preserves five resource-tagged raw Farm bins, matching
  before empty selection, explicit capacity-clamped credit and overflow,
  floored debit, and actual Cart accepted-count/residual handling. The native
  constructor initializes each bin's resource/stock to0 and capacity to400.
  Later capacity writers remain external inputs. The API admits a documented
  nonnegative word-safe subset of the native DWORD bin fields.

All three operations consume caller-resolved facts and return bounded results.
Their independent tests exercise hostile inputs, integer boundaries, crop
identity, quotas, partial acceptance and complete arithmetic conservation.
The helpers own no advancing worker, World stock, purchase or save state.

### Fourteen production gates

| Gate | Verified Original slice | Remaining integration dependency |
|---|---|---|
| Grower quotas | Default Food limits5/2, per-worker field budgets4/10, staffing tiers and old-counter attempt order | Mutable configuration/load binding, exact retained-counter lifecycle and successful bounded channel allocation |
| Assignment and tours | Several fields can retain the same actual Actor26 identity, with distinct tending/harvest candidate scores | Complete source distance preparation, candidate admissibility, concurrent claims and repeated visits |
| Worker states | Ordinary tending6→7→8→6 and harvest10→11→12→10, with actual eligible waits and return outcomes | Full movement/result inputs, exceptional states and every cleanup caller |
| Failed targets | Exhaustion releases a retained source count and selects empty/cargo return branches | Complete retirement, repair and cargo disposition |
| Interrupted work | Existing World adapters preserve their own paid pause/fire/path contracts | Original active Food Grower operation, fire, workforce and global-veto interactions |
| Actual harvest | End-slot/subslot0 setter marks an active Food field. Completed old end-slot expiration clears field quantity/work/assignment. A completed eligible harvest wait reaches the distinct yield callback | Full readiness/assignment/driver eligibility, actual actor cleanup and single-origin World publication |
| Runtime fertility and climate | Serialized-to-runtime byte operation, default modifier and actual harvest operand | Complete writers, load preparation, climate/region provenance and admitted live values |
| Crop/region factors | Source factor lookup, crop-qualified doubled-base predicate and supported standalone-map header's signed low-word selector | Textual region identity, other profiles, complete later selector writers and mutable factor lifecycle |
| Global yield factors | Conditional multiplier and conversion order | Actual global/year/event inputs and complete later writers |
| x87 arithmetic | Startup selects53-bit precision; conversion temporarily selects truncation and restores the control word | Actual harvest PC/RC, preceding store rounding and reachable environment changes. The same factor gives64 at53/64-bit precision and65 at24-bit precision |
| Raw yield to Food | Grower→Farm→Cart raw identity, normal House debit and Mill recipe arithmetic; a concrete Mill53→Actor25→FoodShop66/Market→House graph | Complete admission/physical arrival/grade blending and a defined mapping to existing authored World Food portions |
| Field/Farm overflow | Bounded quantity helper, initial five400-capacity bins, native archive capacity writer, explicit clamp/debit results | Valid restored capacities, later writers, full overflow disposition and an approved conservation-safe World integration |
| Return and capacity | Actual Farm return offers cargo. Full Cart acceptance clears it, partial acceptance retains the remainder | Source/target reservations, path failure, final retirement and safe return headroom |
| Active-work persistence | Existing schema42 restores actual released Food/material/water authority exactly | A new explicit generation must save typed Food Grower lifetimes, work, claims, cargo and reservations before those states become playable |

The x87 conversion helper alone cannot choose the preceding precision. A fresh
exact arithmetic differential uses the same stored binary32 fertility factor
`0x3f266666` with base100 and obtains distinct64/65 results. The checked Farm
caller also ignores the overflow returned by its capacity-clamped bin credit.
These are concrete boundaries for a faithful and conservation-safe successor.

### Current end-to-end acceptance

The existing paid mixed-city test constructs all five Food fields, Hemp, Tea,
Lacquer, Mulberry, irrigation, Market/Warehouse and20 paid House plots. Actual
residents and staffing arise through subsequent World ticks. The test
runs19,584 actual World ticks, with real Wheat/Millet harvest, physical Food
delivery and House consumption, all three material return chains, water/fire/
pause checks and exact save/Recovery replays. Rice/Cabbage/BeanCurd retain zero
origin, cargo and consumption through both complete calendar cycles.
The separate Food-positive plantation test covers actual FoodQuality and luxury
consumption through the retained approved adapters. Both use explicitly
AUTHORED map/model inputs. Original-model parsing and independent numerical
parity remain separate tests. Positive new-crop production, higher-House
progression from those crops, Food Grower animation and native human acceptance
remain pending. AGRI-05 supplies verified hunting/fishing components with the
same activation boundary described in its matrix.

### Inactive authored alternative

The earlier AUTHORED candidate remains available for separate approval. It
would extend the integrated Rule26 Agriculture lineage through the currently
free Rule27/schema43 pair with an exact new contract marker, separate explicit
selection, create/restore and Recovery. Its saved state would add typed monotone
Food Grower lifetimes, shared movement channels, actual field jobs/work, cargo
and incoming/return reservations. The sole five-kind Agriculture ledger and
existing House receipt/consumption path would remain authoritative.

The candidate deliberately chooses one Food Grower channel per paid Farm,
stable owned-field rotation, the existing authored World clock, eligible
tending/harvest waits51/25, end-slot harvesting, a1000-field-quantity-per-portion
conversion and at most four physical portions per load. Source quota defaults
5/2 and actual raw harvest yields are distinct evidence. The proposed unit
conversion directly debits field quantity and requires an expressly authored
yield policy. Existing Wheat/Millet and material adapters retain their contracts.
Construction and dispatch must reserve their actual channel/headroom needs
within82 bodies/58 channels, including all retained in-flight returns. Only
pickup creates a counted origin, only actual Farm return credits stock, and
actual House arrival controls FoodQuality. Rice retains fertility100 admission
and a dry base path. A numeric irrigation bonus requires its own complete input
contract.

The selected direction is further Original reconstruction. This alternative
has no approval for its worker/time/yield/unit/interruption policy and remains
inactive. Current code activates neither candidate production nor a new rule/schema. The
[source contract](../reverse/original-agriculture-source-contract.md#agri-02c-crop-yield-inputs-and-conversion-boundary)
records the new proofs, and the [testing guide](../testing.md) records actual
validation results.

## Further Original reconstruction

The new components close specific source operations while Rice, Cabbage and
Soybean production remains **PARTIAL**. They reuse the existing Original rules
libraries and shared map/container reader. `simulation::World` retains sole
gameplay authority; no new profile, clock, migration, goods origin or saved
state is introduced. Rule27/schema43 remain unused.

| Component | Verified and implemented boundary | Remaining activation dependency |
|---|---|---|
| Food-field seasonal lifecycle | Active end-slot/subslot0 marking and explicit completed-old-slot expiration. Preserve all other work bits until expiration; report expired field quantity separately from Food/cargo | Full Grower claim, wait, movement, interruption and retirement lifecycle; active source facts and clock binding |
| Yield context | Actual startup53-bit precision, executed parameter initializer, supported map-header region getter and native three-DWORD Farm-bin archive fields | Actual harvest rounding control and reachable environment changes; later parameter writers, restored-value admission and textual region identities |
| Normal House Food event | Status1 and positive FoodQuality requirement consume up to floor(population/4) raw Dinners. An eligible empty result clears quality; nonzero native stock-grant bypass is refused | Native event admission and mapping from raw Dinners to the existing World portions and receipt histories |
| Mill recipe arithmetic | Nine aggregated commodity quantities, separate base vetoes, grade1–5 availability, bounded grade choice and six ingredient-request slots. Mill has eight physical bins | Real bin ownership, source debits, assistant allocation, physical arrivals, exceptional returns and complete quality blending |

Mill base ingredients are resource IDs1–7; Salt8 and Spices9 are supplements.
Grade5 tries four bases plus Spices, then three bases plus both supplements.
It has no four-base-plus-Salt alternative. Availability uses ranked enabled
base stocks and ordered supplement branches. The request builder instead
chooses qualifying bases in resource order, rounds each to100-unit blocks and
does not consult those vetoes. Thus two50-unit base stocks can advertise100
grade2 availability while the selected100-unit request builds no ingredients.
Partial outputs and positive native return values are preserved as observations,
not success or produced-food flags.

The inspected physical graph binds Mill53, Actor25 grade/category payloads,
FoodShop66 and its actual parent Market, followed by a raw House credit/debit.
Common Market59 starts with grade bounds1–3; Grand Market60 starts1–5. Later
policy/load writers and the complete movement/receipt predicates remain open.
This path does not establish that every Farm Cart must visit a Mill. The
assistant helper debits before allocation and has a local failed-allocation
branch without refund; its planner still records the requested incoming amount.
Whether upstream admission excludes that branch remains unproved. A future
World integration needs an explicit conservation contract for those conditions.

The [field/yield source contract](../reverse/original-agriculture-source-contract.md#further-original-reconstruction-food-field-marking-and-expiration),
[Food contract](../reverse/original-goods-consumption-food-contract.md) and
[map contract](../reverse/map-format.md) retain qualified addresses, evidence
domains and unresolved inputs. Private disassembly and source captures stay
under ignored `.local/`. See [validation](../testing.md#further-original-agriculture-reconstruction)
for executed checks and their limits.

## Original Food Grower reconstruction (AGRI-02D)

**PARTIAL.** AGRI-02D implements independently qualified source-event operations
for Wheat195, Millet196, Rice197, Cabbage198 and Soybean199. The complete paid
World chain from Farmhouse through physical Grower work/return to raw Farm stock
is not activated. Current Rule26/schema42 and all historical generations retain
their behavior. Rule27/schema43 were rechecked as unused and remain unused;
there is no additional saved mutable authority or automatic migration.

| Boundary | Implemented verified operation | World activation requirement |
|---|---|---|
| Initial candidate selection | Food-specific gates, zero-quantity mark clearing, distance/work/fertility scores and exact fresh cached-insertion queue ties | Actual prepared distances, source scan/range admission and complete allocation/distribution context |
| Successful spawn | Only a supplied actual factory success increments its retained counter and initializes role/state/stagger | Real bounded entity/channel allocation, field commitments and identity lifetime |
| Ordinary worker | Begin→body→epilogue ordering, states6–13,51/25 waits, actual target results and distinct movement8/9/10 outcomes | Shared Navigator/result binding, all suspension/operation/fire/staffing histories and exceptional states |
| Field events | Full tending effects, terrain/countdown/growth/veto/publication order, existing marking/decay/expiration and complete marked-harvest mutations | Actual eligible caller, source calendar/actor ordering and retained references |
| Raw yield | Exact portable resolved-context PC24/53/64 and all RC modes, both fertility stores, both truncations and crop-qualified modifiers | Admitted live control word/normal FP state and all runtime fertility/region/event/date/global owners |
| Return and cleanup | Outcome8 requests source receipt;10 retires without it. Exact counter debits, every active matching field assignment clear and represented actor reset | Complete source receiver/conservation, missing-target/source histories, generic route/link cleanup and restored identities |
| Persistence | Historical Rule26 and older save/Recovery contracts retained; deterministic resolved-event replay tests | A new explicit World generation before active Grower/claim/cargo/raw-bin saves exist |

`CropSelectionRules`, `CropGrowerRules` and `CropHarvestRules` extend the existing
Original crop-rules library. They reuse `CropRules`, `CropLifecycleRules` and
`CropTransportRules`; they create no second scheduler, route finder, actor
registry or inventory. Their inputs are explicit source observations, not
model-label or rendering-based admission. State4's independently proved WORD
wait arithmetic is separate. State5's shared helper and exceptional14–16 have
qualified branch descriptions but remain outside the supported body API.

The Farm's retained harvest/tending counters are demonstrably not exact live
worker counts: a failed selection decrements its counter, and retirement later
decrements it again. Source inactivity marks the actor retiring **before** its
body, which can still request field work or a source receipt; an invented early
rollback would change this behavior. Complete source/pause/fire/road-cut meanings
cannot be inherited from the existing authored plantation adapter by analogy.

### Resource balance and event order

Field quantity is distinct from numeric raw yield. Numeric yield is narrowed
to a WORD for history and added modulo65536 to the actual actor WORD7c. That
same word travels with the Grower. There is no proved separate pending-stock
to physical-cargo copy and no license to count it twice. Numerical WORD narrowing,
actor-word carry, signed-positive return selection, bin-accepted amount and
overflow remain explicit separate observations. Reset alone proves neither
successful receipt nor cargo loss, because a successful source receipt also
leaves the native actor word retained until reset.

The same-crop resolved-event test composes two tending targets, growth/decay,
end-slot marks, two harvests on one actor, retry/return outcomes, typed bin
acceptance and reset for each of the five fields. With its explicitly supplied
fertility100/unit-region/PC53 context,64 growth events produce field7552 and
each marked callback yields75 raw units; the one actor carries150 and the bin
accepts150. Those are synthetic resolved operands, not admitted native city
parameters or World timing. Full-bin and failed-return branches expose
unaccepted amounts without an invented refund or stock grant. Complete result
replay compares fields, actor reset, counters and typed bins. No Original Food Grower
save/load or physical route acceptance is claimed by this test.

Expiration clears field work/quantity/assignment and the assigned actor's wait,
but does not itself clear that actor's retained field reference or work state.
A later compatible harvest callback is unmarked and cannot produce yield.
A later tending callback can restore work40; the growth callback has only its
last-end-slot veto, not a general growing-period gate. These composed source
events must not be mistaken for a proved reachable ordinary off-season tour.

Initial/repeated selection contains no explicit commodity partition, and Farm
return derives resource identity from the last retained field. Distinct end
slots and expiration may constrain ordinary tours to one crop, but inactive/
reactivated fields, retained assignments and identity reuse still need a full
temporal proof. Existing typed World Food must not accept an unproved pooled
multi-crop amount. The downstream Mill/FoodShop and Food-portion mapping remain
separate work; they are not prerequisites used to excuse an unproved raw Farm
cycle in this task.

### Required AGRI-02E closure

1. Bind complete initial allocation/distribution, repeated target admission,
   prepared distance data and shared movement outcomes to actual Navigator
   routes and typed entities. Close source/target deletion, suspension13c,
   global88ec38, source status, operation, staff and fire relationships.
2. Qualify state5's `4e6280`, exceptional14–16 entry/link histories and shared
   callees, plus every generic route/link cleanup needed by those paths.
3. Admit the actual PC/RC and normal FP environment at `4c1e8d`; bind live
   runtime fertility, factor-table/region, event, date and global operands to
   checked source/load/update owners. Resolved numerical parity alone is not
   this admission.
4. Prove crop homogeneity across active/inactive/expiration/restore/identity
   histories, or keep that context unsupported. Resolve native WORD narrowing,
   failed-return and full-bin dispositions in an explicit resource balance;
   do not infer accepted stock from raw source bookkeeping counters.
5. Only after those gates close, implement one explicit successor with World
   actor/field claims, one carried raw amount, physical source return, typed
   Farm bins and exact create/restore/Recovery. Save every non-reconstructible
   wait/route/reference/counter/reservation and reject inconsistent saves before
   publication. The same field must not also run its authored Food origin while
   producing native raw yield; a second production account or an invented
   raw-to-Food conversion cannot keep households supplied. Existing adapters
   remain exact in Rule26/schema42 and older generations.
6. Exercise paid World work and return for all five crops, full bins, staffing,
   pause/fire/path failures, active-work and loaded-actor save/load, resource
   balance and independent replay. Keep the later Food conversion separate.

Qualified addresses, state tables and concrete exclusions are in the
[source contract](../reverse/original-agriculture-source-contract.md#agri-02d-implementation-boundary).
Executed tests and remaining native/World acceptance are maintained in
[testing](../testing.md#original-food-grower-source-events-agri-02d).

## Physical Food Grower integration and completion matrix (AGRI-02E)

**PARTIAL; no playable Original raw-Farm origin.** The requested chain is paid
Farmhouse193 → actual Actor26 → physical field work → growth/marked harvest →
physical source return → accepted typed raw Farm stock. It is still not proved
end to end for any of the five Food fields. AGRI-02E follows the requested
activation condition: unresolved route, live-yield and identity lifetimes leave
World production closed. Mill/FoodShop/House-quality conversion is separate
work and is not used as a reason to postpone investigation of the raw chain.

The existing six crop-rule components were reviewed without adding another
pure-rule implementation or a parallel simulation. The new C++ change is a
negative World/persistence regression using real paid Farm/field commands for
all five identities in each of Rules24,25,26. It verifies rejection of unproved
raw field state, native-looking cargo/assignment/bin/counter JSON and the still
unused Rule27/schema43 pair, preserving World/save/source bytes on refusal.
Canonical restoration and deterministic continuation remain exact. Its empty
settlement deliberately provides no positive staffing or Grower movement proof.

### New evidence and remaining boundaries

| Boundary | New closure | Still required for active World authority |
|---|---|---|
| Source dispatch | Due dispatcher prepares one cardinal distance plane before harvest then tending; source distance starts at1. Actual operation byte90 and signed current/required-staff percentage are bound | All prepared terrain/occupancy and configuration owners; actual bounded World allocation/distribution; Farm WORD40 meaning |
| Repeated fields | Actual harvest caller restarts the registered-vector scan; no nearest-field queue, mark/calendar/distance or cargo-resource test in that local selector | Full temporal crop homogeneity and retained last-field identity through deletion/rebuild/load |
| Field deactivation | Actual Food cleanup unregisters the field, then clears assignment and assigned actor wait | Actor68 and carried WORD7c survive that local cleanup; returning lookup has no generation check and native slots can be reused |
| Physical routing | Actor26 forces mode17; qualified flood, gradient extraction, route slots/release,20-phase microstep progression (new routes start at20) and actual cell commit at10 | Prepare equivalent source-bound edges through the shared Navigator; prove endpoint/cursor/phase invariants, runtime speed properties and every next-edge predicate |
| Movement result8 | Exhausted valid route yields8 without a separate endpoint check; missing route requires exact coordinate equality | A valid actual World route and cell history, not a supplied result8 or visual proximity |
| State5 | Physical cell commit can enter linked interaction; actual entry saves prior state/peer links and exit restores/clears them | Complete peer/model/RNG/shared effects and interruption/reentry eligibility |
| Suspension13c | Proven writer holds an actual Actor48 carrier ID, follows linked passengers and suppresses behavior plus ordinary retirement; carrier release clears it | Complete carrier/source/link lifetime and failure admission; it is not simply user pause |
| States14–16 / global gate | Direction query and shared physical following clarified; menu branches write88ec38 and parent still calls actors while skipping source scheduler | State14 entry/linked72 histories, all transitive effects and complete global ownership |
| Live yield | Distinct runtime-fertility archives and current event-object/load/update paths qualified; raw date is separately retained | Complete runtime fertility/region/factor/event/date/irrigation owners and actual PC/RC/normal FP context at harvest |
| Raw return | Repeated selector, field removal, naked last-slot lookup, slot reuse and separate Cart handoff qualified; explicit narrowing/carry/acceptance equations | Prove same-resource attribution and choose an explicit conservative supported overflow/wrap domain; never use footer bookkeeping as accepted stock |
| World/save generation | Historical admission, no-overwrite and exact continuation controls added | No new generation until all above gates close; complete active worker/claim/route/wait/history/raw cargo/bin/counter/reservation/return-headroom save authority |

Addresses, qualified callees and source/load distinctions are maintained in the
[AGRI-02E source audit](../reverse/original-agriculture-source-contract.md#agri-02e-physical-world-integration-audit).
The local source-distance plane is separate from mode17's actual route; neither
can be replaced by the material plantation's approved authored five-tick travel
and staffing/fire gates by analogy. There is no second Navigator, inventory,
Food account or scheduler. Rules1–26/schemas1–42 and defaults remain unchanged;
Rule27/schema43 were rechecked and remain unused. Wheat/Millet retain only their
approved processed-Food origin in25/26; no Original raw origin runs beside it.

### Raw resource equations

Keep field quantity Q, numeric raw yield N, narrowed history word H and actor
word C separate. For supported nonnegative resolved operands, with0≤B≤K≤32767 and
0≤O≤32767 for the checked bin API (no signed-DWORD wrapping):

- Field mutations balance their own quantity: prior Q + admitted growth =
  retained Q + harvest-cleared Q + expired Q + veto-removed Q. These are not
  interchangeable raw resource units.
- H = N mod65536; C' = (C + H) mod65536. N−H is numeric narrowing and
  C+H−C' is the separate carry. History is an observation, not another stock.
- Only an actual eligible same-resource return offers O to the bin. With
  stock B and capacity K, accepted A = min(O,K−B), overflow X = O−A and
  B' = B+A. O = A+X; ignored overflow cannot be credited again.
- A successful separate Cart creation debits400 and owns Cart WORD82=400,
  so final bin +400 = B'. Failed creation has no debit or refund. Grower
  WORD7c is not copied into another pending-cargo account.
- Return10 has no checked source receipt. Reset clears retained actor memory.
  After a receipt, A is credited and X unaccepted once; the retained offered
  word must neither be credited nor counted as another loss. Return10 leaves
  unreceived terminal cargo. No unaccepted amount gets an invented refund.

A return word32768..65535 is signed nonpositive and selects the no-receipt
return branch. The resolved bin API supports only nonnegative quantities;
unproved negative offers, mixed-resource words or identity reuse remain
unsupported. Native ignored overflow is observed behavior, not a license to
create a hidden World stock or silently change a resource. A new strict World
balance must expose all accepted, carried and terminal unaccepted amounts.
The historical counters also remain historical: failed selection and retirement
can both debit the same source counter. They must not be repaired into a live
worker census.

### Concrete World binding after the proof gates

This is an implementation dependency map, not an admitted generation or a
placeholder API. The earlier single-Grower AUTHORED proposal is not selected.
Native5/2 defaults and retained counters do not authorize extra channels or
prove an immutable quota. Keep the existing82-body/58-courier budgets.

| Existing owner | Required integration once admitted |
|---|---|
| `World.h` / paid command path | One explicit successor create/restore contract, typed monotone Grower lifetimes and Farm/field references; actual bounded allocation success alone commits source counters/assignments. Keep native retained counters separate from derived channel occupancy |
| Shared Navigator / `WorldPlantations.cpp` patterns | Reuse movement infrastructure, legal-edge/cache preparation and begun-edge preservation; provide the proved mode17 edge/direction/cursor/microstep semantics rather than borrowing plantation timing or creating another navigator |
| `WorldOriginalFood.cpp` / `AgricultureState.h` | In the successor only, disable the authored Wheat/Millet origin before admitting raw work for any field. Keep raw Farm bins/cargo distinct from processed Food units; no automatic400→4 conversion and no second Food ledger |
| Shared Agriculture calendar / terrain/irrigation queries | Feed actual admitted phase/order, rawdate and live context owners; derived seasonal observations and serialized construction fertility never create mutable quantity or yield |
| Existing six `Crop*Rules` components | Invoke selection, wait, growth, harvest, receipt and retirement only from actual eligible World actor events with checked live inputs. A supplied movement-result enum cannot certify arrival |
| Existing SandboxSave / `RecoveryStore` | Exact new schema/marker/source identities; save active state/waits, typed assignments, routes/phases, histories, single raw word, bins, retained counters, reservations/headroom and all indispensable context. Reject inconsistent/unknown authority before atomic publication; retain independent replay |
| Existing Agriculture inspector / walker presentation | Render actual World cells, actor states and inventory receipts; show unsupported status explicitly. Neither rendering nor an inspector can assign work or stock |

Rule27/schema43 are provisional candidates, still unallocated. Before a future
implementation uses them, recheck availability and define the complete marker,
source profile, FP/context domain, terminal accounting and explicit menu/CLI
selection together. Rules1–26, old save targets and the existing general defaults
must remain exact, with no migration. All new-save/Recovery guarantees require
positive active/loaded-Grower tests after this binding exists.

### World acceptance status and next implementation order

| World check | Wheat195 | Millet196 | Rice197 | Cabbage198 | Soy199→BeanCurd1 |
|---|---|---|---|---|---|
| Paid typed Farm/field; negative raw/save admission in24/25/26 | PASS | PASS | PASS | PASS | PASS |
| Actual Original Actor26 spawn, physical outward route and completed tending | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED |
| Live-context growth/mark/harvest and physical Farm-bin credit | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED | NOT IMPLEMENTED |
| Repeated cycles, blocked paths, pause/fire/staff loss and full raw bins | NOT RUN: no active raw mode | NOT RUN | NOT RUN | NOT RUN | NOT RUN |
| Active/loaded Original Grower F5/F9, Recovery and independent World replay | NOT RUN: no raw schema | NOT RUN | NOT RUN | NOT RUN | NOT RUN |

Existing authored plantation/Food/irrigation and historical Save/Recovery tests
remain controls for their own contracts. Supplied movement-result component
replay is not promoted to World acceptance. No new Original Grower native or
human playtest is claimed; executed checks are recorded in the
[testing guide](../testing.md#physical-food-grower-integration-audit-agri-02e).

The next dependency order is concrete:

1. Close prepared passability/occupancy and runtime actor-property load/update
   provenance; bind mode17 route, cell-commit and microstep invariants to the
   existing shared Navigator without changing historical movement contracts.
2. Close State5, carrier13c/linked66, exceptional14/linked72 and global88ec38
   lifetimes, including actual source operation/staff/fire interactions.
3. Close live harvest context: independently loaded/updated fertility, region
   factors, events/date/global modifiers, irrigation query and entry FP state.
4. Prove homogeneous tour/last-field identity through field removal, native slot
   reuse and restore; close capacity/load/wrap/overflow dispositions without
   refunds, extra stocks or resource retyping.
5. Define one explicit successor only after1–4; replace the authored Wheat/Millet
   origin there, save all non-reconstructible work/route/cargo state, and then
   implement the15 requested positive World scenarios across all five crops.
   Raw-to-Mill/FoodShop conversion follows independently.

## Bounded World Grower navigation (AGRI-02E2)

The flat mode17 movement kernel is now integrated into the **existing shared
World navigator**, with actual World tick-driven cell movement. This is an
explicit non-production acceptance session, not an autonomous Food Grower or
new gameplay/save generation. Raw harvest, Mill/FoodShop and FoodQuality are
outside this task; no new crop work, inventory or resource credit is activated.

| Acceptance boundary | Status | Evidence / limit |
|---|---|---|
| A. Original routing source | PARTIAL | Prepared WORD predicates, ordinary region absence, cardinal flood/eight-way gradient, ties and20-phase ordering qualified for the flat ordinary subset. Full source/actor/speed/exceptional contexts remain outside admission |
| B. Shared Navigator integration | PASS | One existing `World::find_road_route` queue/storage, an explicit policy branch, original direction/tie order and499-direction maximum; historical branch retained |
| C. Actual World cell movement | PASS | Paid3x3 Farm and all five typed1x1 fields; actual commits at microsteps11/31, endpoint at41. Literal Farm-obstacle tie takes six real edges and finishes at121 |
| D. Endpoint/retry/failure | PASS | Actual endpoint8, future obstruction9 with shared re-preparation, proved blocked return endpoint10 and unsupported-context11. No injected result8 |
| E. Unsupported actor interactions | BLOCKED | Peer/state5, suspension13c, Ferry48, linked66/72,14–16 and global88ec38 are explicit refusals; no successful fallback or pause substitution |

`GrowerNavigationPolicy::Mode17MicrostepV1` is selected explicitly through the
World API. It advances **one native microstep per World tick**, an AUTHORED
clock binding. It does not claim Original actor speed, eligible-call cadence or
source/statistical body side effects. Existing5-tick couriers and plantation
movement keep their contracts. The actual requests identify paid Farm/field
bodies and caller-explicit external Farm endpoints; no native spawn or WORD2a/2c
return identity is inferred.

Initial admission requires Rule26's complete immutable raw-source/boundary/SHA
context, signed height0, and no raw10000 region marker anywhere. This stronger
restriction prepares the proved ordinary region-absent counterpart once; no
native region operation is admitted. Known empty/road/Food cells are combined
with World's complete paid footprints, ownership and existing policy. The Farm
body is blocked, including its origin. Unknown predicates refuse the whole
route query instead of masquerading as walls. Native saved/runtime regions,
height transitions, bridges, ramps, ditches and other body classes stay outside
the initial supported domain. See the exact
[source binding](../reverse/original-agriculture-source-contract.md#world-mode17-motion-binding-agri-02e2).

The session shares the existing58-channel bound with ordinary couriers. Current
cells and begun segments are protected from overlapping construction, road or
irrigation changes and Elite expansion. Future geometry is rechecked at the
next boundary, with real9→reroute→8 or9→10/11 histories. Unsupported context
arising on the final edge cannot become8 after the real cell commit.

No Rule27/schema43 is allocated. Session state has no Actor26/Crop authority and
is not serialized; a transient default-false snapshot guard blocks save/restore
publication until terminal cleanup. Historical snapshots, gameplay IDs, source
bytes and schema42 JSON then match an independently ticked control exactly.
Rules1–26/schemas1–42 retain their ordinary semantics and defaults. The earlier
[raw Grower completion matrix](#physical-food-grower-integration-and-completion-matrix-agri-02e)
still applies to autonomous spawn, tending, live harvest and Farm-bin credit.

Remaining integration order: admit native speed/cadence and source/target lifetime;
close peer/Ferry/link/exceptional/global histories; widen proved terrain/region
contexts; only then bind real Actor26 work/cargo and a complete new saved
contract. Native/human and unrestricted Original-map movement acceptance remain
separate from the actual authored-World motion tests. Executed validation is in
[the testing guide](../testing.md#bounded-world-grower-navigation-agri-02e2).

## Irrigation infrastructure and completion matrix (AGRI-04)

Rule26/policy3/schema42 is an explicitly selected successor of Rule25. Its
marker is `authored-agriculture-irrigation-v1`, irrigation revision1. Menu G and
`--original-irrigation` select it. Ordinary Agriculture A and
`--original-agriculture` retain25. It inherits the sole five-kind Food ledger,
Wheat/Millet adapter, Tea/Lacquer/Mulberry work, physical transport, household
consumption, workforce, services, fire and finance. Rules1–25/schemas1–41 keep
their own create/restore paths and canonical saves. Automatic migration is absent.

### Installed authority and update boundaries

`IrrigationState` records sorted paid ditch cells, their placement tick/command
sequence, lifetime placed/removed counts, exact construction spending, and typed
Pump BuildingIds with orientation and eligibility epoch. Ditches are terrain
overlays. Pumps are ordinary paid production bodies within the existing82-body
budget and own no movement channel. Shared workforce and installed upkeep apply.
The prepared model/difficulty prices each new cell and each Pump. A gesture of
1–256 cells commits atomically, charging only its new cells. Ditch removal is
free, has no refund, and retains lifetime payment/revision history.

The immutable terrain context is prepared once through the shared map parser,
full source-map SHA and policy3. It retains raw terrain and a source Diamond
margin3 mask. Pump bodies claim all four2x2 cells, including their two water
cells. Eight oriented water cells and two land cells form the intake context;
the six external water cells remain context rather than occupancy. Existing
policy3 water/source-manager/gate/marker checks remain mandatory. Unsupported
Original objects and pre-existing Original ditch bits cannot acquire player
payment authority. The source paths checked here contain no height predicate;
the Pump-specific shoreline admission therefore permits different legal signed
source heights without applying the ordinary flat-land-building shortcut.

The shared road revision invalidates placement/navigation caches after complete
transactions. Ditch/road edits revalidate perpendicular crossings, including
neighboring topology, before publication. Existing navigator, edge safety and
worker routes remain shared. Topology is computed outside rendering and hover.
Only phase/eligibility/topology changes rebuild the bounded water cache.

### Water and agricultural effects

`EXE_OBSERVED` source phase27 clears and fills cardinal connected ditch cells
from four orientation-specific seeds of eligible Pumps. The dispatcher decrements
raw phase before its jump-table lookup. Raw27 therefore uses table slot26 and
maps to completed World ticks28,79,130 and subsequent51-tick intervals. Dry
networks have no implicit river or proximity source. Water traverses admitted
perpendicular road crossings.

The existing World allocator assigns full-or-zero staffing. Pump supply requires
positive assigned workers, operation enabled, a valid intake and the inherited
fire gate. Translating those existing operation/fire/workforce states into the
source eligibility check is an explicit `AUTHORED` integration boundary. Loss of
eligibility or a cut invalidates water immediately. New construction and restart
wait for a later irrigation phase. The source phase is preserved, while exact
Original status/fire latency remains `UNKNOWN`. No Pump-specific road-access
requirement is inferred from the checked source path.

The sole derived wet/supplier cache records the lowest eligible PumpId for a
connected component. A field query visits the source square perimeters1–5,
excluding the center, and reports actual supplied water and its Pump. This is
water availability. The observed global active-Flood veto and numeric yield
branches also exist in the Original. OpenEmperor has no corresponding Flood
lifecycle. Fertility combination, complete runtime inputs and raw-yield-to-Food
units remain unclosed. Every crop's `yield_effect_active` is false. Wheat, Millet,
Tea, Lacquer and Mulberry keep their approved authored harvest quantities.
Rice retains fertility100 construction admission and stays production-locked,
as do Cabbage and Soy. Irrigation introduces no Food origin or crop substitution.

### Persistence and presentation

Schema42 requires exact inherited Agriculture/plantation markers, its own marker,
model/difficulty and full source identity. Restore checks complete Pump geometry,
source intake, staffing/operation/fire eligibility epochs, bounded chronology,
unique paid cells/sequences, counters, accounting and road crossings before
publication. Wet masks, supplier arrays and image selections are rebuilt and
cannot be serialized as authority. Unknown keys, duplicate JSON keys, mismatched
generations, spoofed goods/couriers, malformed orientation and altered source
context are refused. Schema42 writes can replace only a schema42 target. Recovery
retains exact generation and performs the same source-bound validation.

The optional metadata-only compatibility supplement resolves18 wet and18 dry
ditch poses plus four Pump orientations through existing source readers and the
shared sprite pool. It adds762008 logical RGBA bytes before shared physical
deduplication within the existing64MiB limit. Static Pump poses are verified;
operating animation cadence remains `UNKNOWN`. Anchors follow the documented
`AUTHORED` presentation convention. Toolbar, keys, rotations, cancelled/paid
Ditch drag, inspectors and overlay consume actual World state. Sprites and the
overlay create no wetness or field effect.

| Feature | Gameplay | Original fidelity | Tests | Open blockers |
|---|---|---|---|---|
| Paid Ditch202 construction/removal | Implemented atomic gestures, unique cells, exact prepared charges and retained spending | Observed terrain masks, payment per accepted new cell, margin and crossing predicates; World transaction/removal history is AUTHORED | Rules, World, persistence, scripted SDL | Unsupported source objects and immutable Original ditches remain protected |
| Pump203 body/intake/orientation | Implemented four orientations, full footprint and source-bound shore context | Observed2x2 body, eight water/two land context, four seed sets, model cost10/staff10; existing World workforce/fire adapter is AUTHORED | Four directions, missing water, height, occupation, workers, pause, fire | Exact Original status/fire latency and operating animation cadence |
| Connected water distribution | Implemented supplied/dry sections, crossings and cut/restart behavior | Observed cardinal fill at phase27; immediate invalidation/restart epoch is AUTHORED | Connectivity, multiple Pumps, cuts, crossings, deterministic replay | Complete Original scheduler/environment lifecycle parity |
| Field water availability | Implemented bounded World query with actual supplier | Observed square perimeter1–5; fresh World has no active Flood simulation | Center/range boundaries, all crop kinds, save continuation | Original Flood events and effect-input reconstruction |
| Crop yield modifiers | PARTIAL, inactive for every crop | Bounded source branches documented separately | Unchanged adapter production and locked Rice/Cabbage/Soy controls | Fertility combination, complete yield inputs and common units |
| Graphics and UI | Implemented wet/dry topology, four Pump poses, shared build actions, inspectors and overlay | Verified source selectors; static poses and authored anchors | Synthetic/real profile tests, scripted SDL and inspected captures | Original operating animation timing; native human acceptance |
| Save/Load/Recovery | Implemented exact schema42, source-bound reconstruction and hostile-save rejection | OpenEmperor persistence contract is AUTHORED | Creator/restore/replay/Recovery, mutation and historical compatibility controls | Original Emperor save format is outside this contract |

Validation results are recorded in the maintained testing guide after the
Release, Debug, sanitizer, literal-history and independent parity runs. Scripted
SDL and private captures remain separate from native/human acceptance.

## Hunting and fishing component matrix (AGRI-05)

AGRI-05 is **PARTIAL**. `HarvestRules` and `FishingRules` implement bounded
Original state operations and complete Quay geometry. The read-only model
inspector consumes these components through `--harvest-source 31` and
`--harvest-source 33` observations.
Playable Hunter/Quay production requires the remaining source contracts below.
The current gameplay generation is Rule26/schema42. Rule27/schema43 were checked
as available. This change exposes read-only source components. Paid Agriculture,
irrigation and typed Food accounts use their existing generation contracts.

The two named source models are FishingQuay31 and HuntersTent33. Their raw model
Cost/Employees are50/10 and32/15 in the pinned data. Executable selectors identify
Fish2 and Meat4 independently of the raw model Resource column. The inspector
reports both facts, their separate evidence classification and the pinned EXE
identity. Modified synthetic model values exercise this distinction in tests.

### Implemented source operations

Quay geometry reuses the proved Pump shore geometry and adds its actual exterior
dock. Four orientations retain all five occupied cells, eight water-context
cells, two land cells and the complete eight-cell perimeter. Coordinate overflow
and incomplete caller-supplied attribute planes are rejected. Source fishing
origin flood excludes navigation bytesFE/FF. Boat distance preparation instead
admitsFE with an extra raw4 and a separate deferred expansion, whileFF is blocked.
These independent predicates consume explicitly resolved source operands. Full
navigation-plane classification and the saved-header coordinate-to-map reader
binding remain required before World routing or construction can use them.

Hunter staffing tiers expose the observed attempt thresholds and active limits,
with quota capped by `floor((400-stock)/100)`. The byte throttle increments before
its strict comparison. Boat search and work helpers preserve signed-WORD wrap
and their distinct comparison order. Dock comparison204 completes from zero on
its205th eligible call. Every count refers to a resolved eligible Original
callback. Exceptional scheduler states and actor suspension still qualify the
relationship to current World execution.

The source prey selector maps raw0 to Antelope77,1 to Pheasant76 and other WORD
values to WildPig75. Seven initial attempts belong to each eligible source point.
Positive coordinates, successful group allocation, a zero initialization-bypass
flag, valid spawn placement and actual actor creation are separate prerequisites.
The helper exposes selector facts and an attempt budget.
Source replacement attempts have newly proved phase5/29 and missing-count
counter behavior, while group lifetime/count ownership, RNG and recurring live
state progression remain incomplete.

Raw receipt operations preserve the actual difference between Hunter addition50
and Quay assignment100. The Quay result exposes overwritten stock explicitly.
The standalone Hunter helper supports nonnegative, capacity-safe operands and
rejects unsupported over-capacity histories. Original performs uncapped WORD addition. The helper's admitted domain covers
capacity-safe inputs. Ordinary successful cart debit moves100 raw units
and requires the resolved external-effect byte to be zero. The nonzero branch
can carry400 while debiting100 and retains an unresolved accounting contract.

### Completion matrix

| Feature | Gameplay | Original fidelity | Tests | Open blockers |
|---|---|---|---|---|
| Models, resource and worker identities | Read-only inspection implemented | Raw model facts and EXE selectors independently qualified | Named schema, CLI, modified synthetic values and real model query | Full paid admission and integrated producer capability |
| Quay shore/dock shape | Complete pure geometry implemented | Verified five occupied cells and four orientations | All orientations, bounds, overflow and source-context immutability | Resolved source navigation plane, complete shore admission and World claims |
| Fishing-origin and distance predicates | Pure resolved-input operations implemented | FE/FF domain distinction and raw extra4 verified | All256 byte values and perimeter limits | Header-to-map binding, full classification/rebuild and physical water routing |
| Hunter quota and Boat work | Raw callback operations implemented | Exact quota, threshold and signed counter order | Staffing boundaries, stock headroom, counter wrap and completion boundaries | Actor lifetime, suspension, physical pursuit/return and full World-time binding |
| Prey initialization and regeneration | Selector and attempt-budget components implemented | Conditional initial seven attempts and separate replacement path qualified | All65,536 selectors and explicit species identities | Group/count lifecycle, successful spawn, RNG, depletion and recurrence |
| Raw receipts/cart debit | Bounded component implemented | Hunter ADD50, Quay assignment100 and ordinary debit100 | Capacity-safe raw conservation, replacement reporting and invalid-effect refusal | Live work/return admission, stock replacement conservation,400-effect and overflow histories |
| Fish/Meat Farm/Market/House integration | UNSUPPORTED | Raw Actor19/Market/Actor25 boundaries documented | Existing Food and irrigation regressions, inactive-inspector assertions | Shared World unit conversion, complete FoodShop recipe/grade and actual delivery witnesses |
| Buildings, workers and animal graphics | UNSUPPORTED for new World actors | Actor/model identities qualified | Existing visual profile regressions | Actual World lifetimes and verified animation/state bindings |
| Save/Load/Recovery | Historical generation contracts preserved | Exact Rule26/schema42 and historical contracts | Full existing persistence and historical rule suite | Positive Hunter/Boat/cargo restore needs the remaining production contract |

Complete source details and checked instruction sites are maintained in the
[agricultural source contract](../reverse/original-agriculture-source-contract.md#agri-05-bounded-harvesting-rules-and-remaining-activation-gates).
The remaining conversion and ecology gates prevent a complete playable chain.
An authored replacement requires a concrete separately approved contract.

## Common calendar and seasonal reference matrix (AGRI-06)

AGRI-06 is **PARTIAL** at the Original seasonal-production boundary. The common
calendar observation and seasonal query components are implemented. Original
crop work, final yields and recurring harvest lifecycles remain activation gates.
No new rule or save schema is needed for these read-only derived observations.
Rules1–26 and schemas1–42 keep their existing clocks, production and restore
contracts. General defaults and Agriculture/irrigation selections remain exact.

### One calendar, two evidence domains

`World::agriculture_calendar()` derives `AgricultureCalendarStatus` from the
actual completed World ticks and the existing `OriginalScheduler` mapping.
It exposes phase, subslot, outer slot, elapsed year and mapping version1, plus
the next reference phase20 growth opportunity when representable. The next
phase27 irrigation opportunity is present only for the irrigation generation.
The51×16×12 counts have bounded Original source observations. Advancing one
phase per World tick and counting elapsed years from0 remain **AUTHORED**.
Raw slots0–11 are displayed without invented month names or historical dates.
There is no additional advancing clock, saved calendar epoch or mutable season
state. Pausing advances no World tick. Speed changes how quickly ticks run.
Save/restore reconstructs the same derived position from existing authority.
A next opportunity does not invoke a callback, deliver goods or authorize
production.

`World::agriculture_season_status()` joins that observation with the selected
paid plant/source's actual World state. Reference growing-period and quantity
suppression results are separate from operation, fire, staffing, roads, water,
authored productive progress/readiness, current Grower activity and retained
harvest/return accounts. The inspector's blocker remains explicit. An authored
80-tick progress value is not passed off as an Original work word. An authored
ready plant is not an Original harvest mark.

### Verified seasonal queries

`original::SeasonRules` supplies nine typed plant descriptors with bounded
four-entry raw start/end arrays, sentinel−1 and independent counts. These are
the pinned default EXE configuration, not proof that a selected model context
or arbitrary runtime configuration has those calendar values. Food195–199
delegates the existing `CropRules` period/increment arithmetic unchanged.

The Original growing-period query pairs positions and accepts
`start<=slot<end` only when both entries exist. Quantity callbacks have separate
predicates and inspect the actual low work byte in the three ranges0,1–32 and33–255. Neither a
calendar match nor staffing alone substitutes for that work operand.

| Plant | Reference growing intervals, raw slots | Quantity callback suppression | Raw increment, high/low work |
|---|---|---|---|
|Wheat195|[2,6)|End6|157 /79|
|Millet196|[6,10)|End10|157 /79|
|Rice197|[5,9)|End9|157 /79|
|Cabbage198|[7,11)|End11|157 /79|
|Soybean199 / BeanCurd1|[4,8)|End8|157 /79|
|Hemp194|[3,8)|No calendar suppression in its concrete callback|125 /63|
|Tea26|[2,4),[5,7),[8,10)|Last end10 only|105 /53|
|Lacquer27|[1,6)|Both ends6 and7|125 /63|
|Mulberry28|[3,5),[6,8)|Last end8 only|157 /79|

All callbacks return0 with low work byte0. Lacquer's extra end7 does not create
a second growing interval. Hemp end8, Tea ends4/7 and Mulberry end5 illustrate
why a growing-period result cannot be substituted for the concrete callback's
quantity suppression. The complete shared terrain/countdown/veto driver and
actual worker lifecycle remain separate prerequisites.

### Completion matrix

| Feature | Implemented scope | Original fidelity | Validation scope | Open activation gates |
|---|---|---|---|---|
| Common calendar | Derived World tick/phase/subslot/slot/year view and next opportunities | Observed relative counts. World tick mapping and epoch AUTHORED | Complete cycles, year boundaries, pause/speed and exact save continuation | Original wall-clock/date and exceptional event ordering |
| Seasonal configuration | Nine typed reference descriptors with exact counts/sentinels | Pinned default source tables and concrete callback bindings | Independent12-slot masks and invalid models/slots | Live climate/region configuration and runtime table effects |
| Work-conditioned quantity queries | Pure period, suppression and increment queries. Historical Food delegation | Hemp has no calendar veto. Distinct Tea/Mulberry/Lacquer end predicates | Work0/1/32/33/255, high flags and all65,536 Food work words | Actual Original work ownership and full shared-driver eligibility |
| Paid Wheat/Millet, Hemp and plantations | Existing approved80-tick adapters and physical transport remain active | Adapter timing, units and readiness remain AUTHORED | Existing paid production, delivery, inventory and persistence regressions | Original Grower tours, yield inputs/rounding/units and recurring seasonal replacement |
| Rice/Cabbage/Soy | Existing paid construction, inspector and lock retained | Bounded source identities/queries do not grant Food | Historical no-origin and existing construction/save controls | Complete worker, harvest, overflow and raw-to-Food contracts |
| Irrigation/yields and hunting/fishing | Existing Rule26 water infrastructure and AGRI-05 source components retained | No additional yield or ecology authority | Irrigation, harvest helper and historical regressions | Complete fertility/Flood/global yield effects. Source units, ecology and transport |
| Presentation | Slot/reference conditions and actual World status kept separate | Existing sprite mappings retain their original evidence limits | Inspector assertions and existing visual regressions | Native/human acceptance and Original seasonal animation bindings |

The fresh static source receipt passes66 checks across9 byte-checked windows.
Full check outcomes and automated versus native/human acceptance are recorded
in the [maintained testing guide](../testing.md#agriculture-calendar-and-season-observations-agri-06).
Exact addresses and outstanding proof gates remain in the
[AGRI-06 source contract](../reverse/original-agriculture-source-contract.md#agri-06-seasonal-reference-rules-and-common-calendar).

AGRI-06 does not activate an Original seasonal production cycle, reinterpret
saved authored progress, introduce new goods, or change current harvest amounts.
The next production generation still needs complete Grower/quantity/yield/unit
authority and explicit save boundaries. No automatic migration is permitted.

## AGRI-02–07 dependency and acceptance plan

The current task order puts the three material plantations in AGRI-03 and seasons in
AGRI-06. This supersedes the earlier ordering; the AGRI-02 timing blocker remains
an AGRI-02 prerequisite, rather than being deferred to a later milestone.

| Work | Depends on | Concrete deliverable and gate |
|---|---|---|
| AGRI-02: crop chains (not complete) | AGRI-01 evidence, complete Original tending/harvest clock proof | Implemented: typed Food foundation, paid source-verified construction, Rice fertility and exact schema40/Recovery. Blocked: actual Rice/Cabbage/Soy growth/harvest/earned Food delivery. No authored time substitute is authorized for those crops; the separately retained Rule25 Wheat/Millet adapter does not close this gate. |
| AGRI-03: Tea/Lacquer/Mulberry plantations | Verified identities; expressly retained authored work/units and Wheat/Millet Food adapter; shared physical movement and ledgers | Rule25/schema41 implements bounded paid multi-plant readiness, Grower pickup/return, regrowth and existing processor/House paths. Wheat/Millet retain authored80-tick production and real Food delivery/consumption; Rice/Cabbage/Soy stay locked. Validation outcomes must be reported. Original cadence, yield and seasonal/irrigation parity remain AGRI-04/06 evidence gates. |
| AGRI-04: irrigation | AGRI-02 field/work foundations and source-bound terrain | Rule26/schema42 implements paid202/203 infrastructure, oriented source intake, signed-height admission, actual staffed water networks, field water queries, graphics/UI and deterministic persistence. Crop yield modifiers remain PARTIAL and inactive pending complete fertility/Flood/runtime-input and unit proofs. Existing harvest quantities remain exact. See the irrigation completion matrix above. |
| AGRI-05: hunting and fishing | AGRI-02 shared work/return foundations; relevant terrain/edge work from AGRI-04 | Paid31/33 geometry, source-bound179/183 origins, real prey/boat lifetimes, work routes, depletion and actual returned Fish/Meat units plus outgoing physical carts. Close ecology, replenishment, shore/return/staffing and target acceptance; no marker-based stock grant. |
| AGRI-06: seasons and calendar (PARTIAL) | Existing common scheduler, CropRules and AGRI-04/05 source predicates | Implemented derived World calendar/status and nine verified reference descriptors/queries with historical adapters unchanged. Original seasonal production, live climate/yield inputs and complete multi-season Original acceptance remain blocked. See the completion matrix above; future activation requires an explicit contract and Rule24 stays closed. |
| AGRI-07: integrated Agriculture activation | AGRI-02–06 complete evidence/implementation gates | One explicit generation with complete producer→store/Market→House paths, exact schema/create/restore/menu/CLI/Recovery, catalog-aware needs/consumption witnesses and production inspection/rendering. Paid multi-season multi-source city, independent creator/restore processes, complete-document/cargo/accounting equality, hostile-save refusal, historical Rule1–23/schema1–39 controls and measured budgets. Extend the one Agriculture lineage only after all additional required chains close; native/human acceptance stays separately reported. |

The plan orders evidence and integration, not an unconditional promise to fill
unknown mechanics with authored guesses. Required remaining decisions include
raw yield/cargo units, original calendar mapping, climate/fertility/pump predicates,
Food storage/processing/quality policy, ecology
regeneration, clamped harvest overflow and unexplained source effects, and the
final measured budget. Record each as `EXE_OBSERVED`, an
approved `AUTHORED` choice, or an explicit activation blocker.

## Validation

Use [the Agriculture checks](../testing.md#agriculture-foundations-agri-01).
New arithmetic tests exercise more than two synthetic kinds, checked bounds,
canonical allocation, batch scores and rejection without input mutation. Existing
paid Food, transport, production, housing, save/Recovery, deadline dispatch and
all generation tests remain required. Compare complete historical canonical
documents and active paid continuations against the protected pre-AGRI baseline;
helper tests alone do not establish historical gameplay compatibility.

Raw Original files, executable windows, private scripts and reports stay under
ignored `.local/`. Hash/size/mode/mtime guards bracket source research. Do not
store original binaries, table dumps, proprietary fixtures or disassembly in
Git. Automated crop checks have no human native acceptance to claim, and its research does
not establish full Original agricultural reproduction.
