# Agriculture: integration architecture and evidence gates

This is the maintained design for completing civilian agriculture in one
explicit Original-economy generation. **AGRI-01 prepares arithmetic and source
contracts; it does not implement or activate additional agricultural gameplay.**
The recommended Rule18/policy3/schema34 and every existing Rule1–23/schema1–39
retain their own prices, clocks, quantities, identities, limits and save bytes.

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
237–239 plant chains are controls and future integration boundaries.

The new source investigation qualifies field/resource helpers, source factories,
bounded field schedule/state observations, actual hunting/fishing work and return
paths, and the pump/ditch connectivity path. It does **not** establish complete
seasonal yields, real-time units, fertility, ecology or all transport admission.
In particular, the observed Soybean199 selector resolves raw resource1
`BeanCurd`; a new soybean commodity or fruit recipe must not be invented.
Orchard29 is excluded by the examined farm/orchard admission predicate; its model
label is not permission to reuse the existing237–239 producers as a fruit farm.
The observed Farmhouse return path selects one of five resource-tagged bins;
that is neither today's two-column adapter nor a requirement that the complete
Food catalog contain exactly five resources. Raw farm-cart400, hunter-return50,
fish-return100 and conditional100→400 outgoing cargo need a common unit/effect
contract before integration. Clamped source-bin overflow and the cause of the
conditional cargo branch remain explicit accounting gaps.

## Existing authority and safe foundations

Current `OriginalFoodState` contains exactly two authored crop columns. Its
Farm/Market/House stocks, `loaded-delivered-returned` cargo, actual-arrival
witnesses and checked owner/global equations are the sole Food authority.
Warehouses currently have **no Food account**. Six household Goods and five
auxiliary materials are separate inventories and cannot be reinterpreted as Food.

`FoodBatchArithmetic.h` now provides checked volume/history sums, immutable
canonical-prefix portion selection, authored mixed-cart selection and weighted
batch scoring for any nonempty fixed array dimension. Current production calls
these helpers with its unchanged two-column arrays and20/80 scores. Farm checks
iterate the actual array rather than using `1-crop`. No new crop ID, source,
account, cart, resource, timer, serialization field or permission is introduced.

These are arithmetic functions, not a second ledger, production simulator or
resource catalog. Multi-kind tests use independently authored synthetic examples;
they establish arithmetic behavior, not Original rice, hunting or fishing rules.
The existing Wheat-first sink, mixed load choice, original House-quality blend,
80-productive-tick harvest, capacities8/16/8, cart4 and movement5 stay exact.

## One future Agriculture mode

The future user choice is **Agriculture**, based on Rule18's existing Original
economy, needs-aware logistics, services, fire policy and source-bound terrain.
It does not silently combine Rule19 demolition, Rule20 terrain, Rule21 trade or
Rule22/23 venue policy. Non-agricultural behavior stays inherited unless the
final versioned contract explicitly says otherwise.

The proposed entry points are `World::create_original_agriculture`, the matching
strict persistence restore path and CLI `--original-agriculture`, plus one menu
choice named Agriculture. These names describe future integration, not callable
AGRI-01 APIs. Normal loading continues to dispatch by the saved generation.

Rule24/policy3/schema40 and production revision28 are **documentation-only
candidates** for the first integrated contract. No constants, support predicate,
constructor, restore route, menu entry, CLI flag or writer accepts them in
AGRI-01. Recheck allocation before activation. The proposed exact marker is
`authored-integrated-agriculture-v1`; it identifies the eventual bounded adapter,
not a claim that all Original timing has been reproduced.

Policy3 may be retained only if its source admission and permission semantics
stay exact. AGRI-02 must decide whether the new shore/water/ditch context needs
a new policy version; if so, bind that version in the same single Agriculture
contract instead of changing the meaning of historical Policy3.

AGRI-02–06 build and verify components behind internal test construction. They
must not publish per-crop, irrigation-only or fishing-only player modes, nor
silently revise a previously accepted save. AGRI-07 activates the single complete
generation only after its evidence and acceptance gates pass. If a required
mechanism remains unknown, keep activation closed and report the exact blocker.
An authored substitute needs an explicit documented decision and a versioned
contract; missing proof is not a reason to generate stock or time.

## Runtime data and ownership

| Component | Concrete design and integration point |
|---|---|
| Immutable catalog | Prepare a canonical, bounded ordered catalog once per session. Stable typed Food IDs, Original raw resource bindings, producer/field identity and supported sinks are distinct fields. An Original model ID, resource ID, pixel index and catalog index are never interchangeable. Require proven bindings; do not reserve a made-up fruit or raw-soy resource. Reuse `ModelData` and checked difficulty preparation, not a parallel model parser/admission list. |
| Versioned production state | Keep the legacy `OriginalProductionState` alternative unchanged. Introduce a generation-tagged Agriculture production alternative, reusing existing non-Food state types and kernels. Exactly one active Food representation exists per World; no authoritative legacy array plus mirrored Agriculture stock. Generation-aware accessors reject the wrong alternative. |
| Food accounts | New generation uses catalog-sized physical amounts and counters with explicit bounded owner roles: admitted producers, eligible stores/Markets and occupied Houses. Extend the existing Food accounting/dispatch integration for any proven Warehouse sink; never also credit six-goods Food or legacy actor cargo. Capacities and units need their own evidence or approved authored contract. |
| Fields and plants | Typed monotone Field/Plant IDs bind an exact paid complete footprint to one paid compatible parent. Store selected crop, owned cells, lifecycle stage and only the measured growth/work/quantity state required by the contract. No crop swap, free attachment, orphan field, double ownership or visual-stage-derived harvest. Original multiple-field ownership is a separate proof gate from today's one adjacent field. |
| Producers and yields | Separate preconditions, work progress, mature quantity, harvested stock and real return. Farm schedules, orchard harvesting and fishing/hunting callbacks may use different state transitions; share checked inventory operations and existing World tick integration, not a fabricated universal80-tick recipe. A due callback is an attempt, not a successful actor or output. |
| Work actors | Each actual successful spawn has a typed monotone lifetime ID, paid source binding and existing bounded movement channel. Work, outbound cargo, return, waiting and retirement are distinct. Only the actual evidenced work/return transition may create credited output; target selection, dispatch or a drawn figure creates none. |
| Goods transport | Reuse World couriers, route caches, incoming/return reservations and the sole per-trip sequence. Preserve begun edges, waiting, cargo and reservations through source pause, fire, staff loss and road cuts. Batch arithmetic operates on the selected catalog; mass credit occurs once at accepted actual arrival. |
| House consumption | Keep one House Food quantity/quality authority with exact per-kind receipt/consumption witnesses. Reuse the evidenced blend only in its proven domain. New food scores, processing/quality rules and consumption priority need explicit evidence or an approved authored contract; do not apply20/80 to additional foods. Embedded housing evaluations must use the new generation's complete catalog witnesses, not truncated legacy arrays. |
| Calendar | One World clock. Define a versioned calendar projection only after scheduler/month/day and climate bindings are established. Existing51×16×12 elapsed mapping remains authored and frozen historically. Store non-reconstructible season/work state or deterministically derive it from saved authority; do not store an independently advancing second date. Specify pause, boundary ordering, save/restore, missed work and leap/wrap behavior. |

All mutable rows belong to `simulation::World`. Presentation, inspectors,
model tools, original map objects and source-fact readers expose immutable
facts or derived views only. They cannot construct, tick, harvest, allocate an
actor, consume, renew, reserve or write an authoritative inventory.

For the first complete contract, AGRI-02 fixes a catalog dimension `N>2` and
canonical typed ID order from the proved products. Use bounded fixed arrays
for amounts/counters and the shared arithmetic; source-specific availability
does not shrink or reorder them. The exact dimension/order and catalog marker
belong to the new strict DTO. A later catalog change requires an explicitly
versioned contract, never reinterpretation of saved indices. Choosing `N` now
from a label count or the Farm's five storage bins would be premature.

An agricultural product binding must also state its inventory domain. Hemp
remains an existing household Good; Tea, Lacquer and RawSilk follow their existing
Good/auxiliary paths. They are not extra Food columns and receive no second
credit in an Agriculture catalog. The future contract must explicitly identify
which existing authored producer behavior it inherits and which newly proved
lifecycle replaces it in that generation. This never retrofits old saves.

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

Canonical units and accepted-versus-offered harvest must close before activation.
The observed farm helper clamps bin stock and returns overflow that its examined
caller does not use; exact loss handling remains unresolved. Hunter/Fishing
normally debit100 raw stock, but source byte`+0x70` can select400 cargo with the
same100 debit. Prove the complete effect and conversion or exclude that effect
explicitly in the final authored contract. Do not make either branch silently
manufacture mass, erase overflow or borrow the current four-unit cart scale.

Each historical Rule1–23 retains its own exact tick order. In particular, the
inherited Rule18 production path keeps finance→staffing→production→dispatch→
movement→fire→immigration→services/evolution/consumption. The future mode declares where
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

## AGRI-02–07 dependency and acceptance plan

| Work | Depends on | Concrete deliverable and gate |
|---|---|---|
| AGRI-02: catalog, source context and persistence design | AGRI-01 evidence | Final typed resource bindings, immutable catalog/source context, bounds/workforce accounting, new generation DTO and fail-closed validation design. Close Soybean/BeanCurd semantics and determine whether29 is a real producer, alias or unused path. Unknown types stay unavailable. Prove legacy bytes and synthetic multi-kind conservation; no player activation. |
| AGRI-03: crops and calendar | AGRI-02 | Paid193/fields195–199 ownership, complete geometry/admission, growth/tending/harvest quantities and one explicitly versioned calendar. Independent tests cross actual boundaries, staffing/fire/pause/full buffers and multiple harvests; verify all proposed original-to-World units. No assumed80-tick additional crops. |
| AGRI-04: irrigation | AGRI-02, AGRI-03 | Paid202/203 claims, intake/orientation/height and connectivity, actual pump operation, proven field effects and deterministic reconstruction. Test dry/disconnected/unoperated pumps, network edits, boundary/fertility conflicts, actual output differences and unchanged source bytes. |
| AGRI-05: orchard identity and physical harvesting | AGRI-02, shared field/work/calendar foundations in AGRI-03 | Resolve29's actual UI/factory/runtime role first; if not a standalone admitted producer, document that negative result and implement only proven applicable plant chains. Real harvesting work/return, output identity/units and proven depletion/regeneration. Do not invent a fruit resource to satisfy a label. Reuse existing237–239 ownership without changing their legacy contracts. |
| AGRI-06: hunting and fishing | AGRI-02, shared work/return foundations from AGRI-05; relevant terrain/edge work from AGRI-04 | Paid31/33 geometry, source-bound179/183 origins, real prey/boat lifetimes, work routes, depletion and actual returned Fish/Meat units plus outgoing physical carts. Close ecology, replenishment, shore/return/staffing and target acceptance; no marker-based stock grant. |
| AGRI-07: integrated Agriculture activation | AGRI-02–06 complete evidence/implementation gates | One explicit generation with complete producer→store/Market→House paths, exact schema/create/restore/menu/CLI/Recovery, catalog-aware needs/consumption witnesses and production inspection/rendering. Paid multi-season multi-source city, independent creator/restore processes, complete-document/cargo/accounting equality, hostile-save refusal, historical Rule1–23/schema1–39 controls and measured budgets. Activate only after all required chains close; native/human acceptance stays separately reported. |

The plan orders evidence and integration, not an unconditional promise to fill
unknown mechanics with authored guesses. Required remaining decisions include
the admitted29 role, raw yield/cargo units, original calendar mapping, climate/
fertility/pump predicates, Food storage/processing/quality policy, ecology
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
Git. AGRI-01 has no new-game native acceptance to claim, and its research does
not establish full Original agricultural reproduction.
