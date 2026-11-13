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
| Calendar | One World clock. Define a versioned calendar projection only after scheduler/month/day and climate bindings are established. Existing51×16×12 elapsed mapping remains authored and frozen historically. Store non-reconstructible season/work state or deterministically derive it from saved authority; do not store an independently advancing second date. Specify pause, boundary ordering, save/restore, missed work and leap/wrap behavior. |

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

## AGRI-02–07 dependency and acceptance plan

The current task order puts the three material plantations in AGRI-03 and seasons in
AGRI-06. This supersedes the earlier ordering; the AGRI-02 timing blocker remains
an AGRI-02 prerequisite, rather than being deferred to a later milestone.

| Work | Depends on | Concrete deliverable and gate |
|---|---|---|
| AGRI-02: crop chains (not complete) | AGRI-01 evidence, complete Original tending/harvest clock proof | Implemented: typed Food foundation, paid source-verified construction, Rice fertility and exact schema40/Recovery. Blocked: actual Rice/Cabbage/Soy growth/harvest/earned Food delivery. No authored time substitute is authorized for those crops; the separately retained Rule25 Wheat/Millet adapter does not close this gate. |
| AGRI-03: Tea/Lacquer/Mulberry plantations | Verified identities; expressly retained authored work/units and Wheat/Millet Food adapter; shared physical movement and ledgers | Rule25/schema41 implements bounded paid multi-plant readiness, Grower pickup/return, regrowth and existing processor/House paths. Wheat/Millet retain authored80-tick production and real Food delivery/consumption; Rice/Cabbage/Soy stay locked. Validation outcomes must be reported. Original cadence, yield and seasonal/irrigation parity remain AGRI-04/06 evidence gates. |
| AGRI-04: irrigation | AGRI-02 field/work foundations and source-bound terrain | Paid202/203 claims, intake/orientation/height and connectivity, actual pump operation, proven field effects and deterministic reconstruction. Test dry/disconnected/unoperated pumps, network edits, boundary/fertility conflicts, actual output differences and unchanged source bytes. No dependence on an invented fruit chain. |
| AGRI-05: hunting and fishing | AGRI-02 shared work/return foundations; relevant terrain/edge work from AGRI-04 | Paid31/33 geometry, source-bound179/183 origins, real prey/boat lifetimes, work routes, depletion and actual returned Fish/Meat units plus outgoing physical carts. Close ecology, replenishment, shore/return/staffing and target acceptance; no marker-based stock grant. |
| AGRI-06: seasons and calendar | Completed Original work/harvest proofs for each admitted producer; AGRI-04/05 effect predicates | Extend proved seasonal/climate/yield behavior, calendar inspection and multi-season acceptance. Do not defer required AGRI-02 working crop timing to this step. Every activated change needs a new explicit contract; Rule24 stays closed. |
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
