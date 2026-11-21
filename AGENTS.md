# OpenEmperor clean-room rules

## Scope and source handling

- Write independent C++20 from documented behavior. Never copy proprietary or
  decompiled code, original assets, installers, extracted binaries or original-data
  test fixtures into Git or application bundles. Users supply their own game files.
- Treat original game files as read-only. Keep private extracts, captures, original
  saves and diagnostic reports under ignored `.local/`; do not search personal saves
  without an explicit supplied path. Preserve supplied bytes when investigating.
- Record new format/EXE observations and unresolved fields in the relevant existing
  [reverse-engineering contract](docs/reverse/README.md). Distinguish raw file facts,
  EXE observations, reference-derived interpretations, authored previews and verified
  reproduction. Names, pixels, model labels and historical saved IDs are not proof
  of original runtime semantics. Do not guess unknown fields or silently widen support.
- Keep portability through CMake, SDL3 and standard C++; isolate native platform code
  in `src/platform/`. Follow the existing architecture rather than adding parallel
  parsers, admission lists, navigators or simulation engines.

## Authority and compatibility

- `simulation::World` owns paid mutable gameplay. Original map readers, asset loaders,
  presentation, inspection and overlays do not grant construction, transport,
  occupancy, goods, budgets or save authority.
- Preserve every historical profile, rule, policy and save schema without automatic
  migration. New behavior requires an explicitly selected versioned contract.
  General menu defaults remain City-v11 rule3; ordinary City-v16 uses rule4.
  Bridges rule5/policy3/schema21 and Ramps rule6/policy3/schema22 are explicit opt-ins.
  Original economy is independently opt-in: current rule10/policy3/schema26,
  development revision27. Authored fire protection is a separate explicit
  rule11/policy3/schema27 opt-in; it retains the revision27 economy. Historical
  rules7/8/9 retain schemas23/24/25. Sustainable economy is a separate explicit
  rule12/policy3/schema28 opt-in: three paid Food chains and authored installed
  upkeep every 816 ticks; rules10/11 retain their two-chain/400-tick contracts.
  Authored needs-aware market logistics is a separate explicit rule13/policy3/
  schema29 opt-in. It inherits Rule12's economy, fire protection and unchanged
  bounds; only goods selection changes. Require the schema29 contract marker,
  exact generation restores and separate menu/CLI selection; never migrate Rule12. Cardinal-water
  House desirability is a separate explicit rule14/policy3/schema30 opt-in with
  an exact contract marker and immutable full-map-SHA-bound raw terrain context.
  Only actual paid Common2x2/materializedElite4x4 recipients get the conditional
  query bonus; pending plots get none. Rule13/schema29 and older saves stay exact.
  Authored Inspector coordination is a separate explicit rule15/policy3/schema31
  experiment with marker `authored-inspector-coordination-v1`. It inherits Rule14
  and only coalesces actual incoming Inspector commitments and skips unexpired
  real protection for future dispatches. No new saved authority or protection
  grant; all Rules1–14, limits, needs, costs and defaults remain exact. This
  experiment does not establish uninterrupted Elite17 retention.
  Authored deadline-aware fire dispatch is a separate explicit rule16/policy3/
  schema32 experiment with marker `authored-deadline-fire-dispatch-v1` and separate
  create/restore, D menu and CLI selection. It inherits the source-bound terrain,
  market and revision27 economy; Rules1–15 and their schemas stay exact. Rank
  physically reachable targets without a building-class or Elite preference by
  feasible deadline slack, then deadline, round-trip time and the existing cursor;
  use one bounded pivot exchange when a shorter earlier-deadline visit and its
  real return preserve both deadlines while the reverse order misses one;
  coalesce real incoming trips only with a usable physical ETA. No new saved
  authority, passive protection, timer/resource grant, model/cost/limit change or
  automatic migration. Retention and effects on the other homes require the
  separate [OE45 assessment](docs/original-economy/oe45-fire-coverage.md).
  Authored deadline-aware service selection is a separate explicit rule17/policy3/
  schema33 experiment with marker `authored-deadline-service-dispatch-v1`, separate
  create/restore, T menu and CLI selection. It inherits Rule16 fire dispatch and
  the revision27 economy unchanged. Rank eligible occupied House visits using
  actual witness-derived expiry, physical outward/return times and the complete
  canonical House cursor; coalesce only usable actual same-role incoming trips.
  Only arrival renews the existing timer. No new saved authority, workers, timer,
  goods, model, cost or limit changes; Rules1–16/schemas1–32 remain exact without
  migration. Citywide acceptance remains a separate
  [OE46 assessment](docs/original-economy/oe46-citywide-reliability.md).
  Authored deadline-aware Food distribution is a separate explicit rule18/policy3/
  schema34 experiment with marker `authored-deadline-food-dispatch-v1`. It inherits
  Rule17's services, fire and economy. Only Market-to-House Food target selection
  changes; Farm-to-Market selection, portions, capacities, movement and consumption
  remain exact. Use physical stock, usable actual incoming cargo and existing
  prepared routes; retain the complete canonical House cursor for ties. No new
  saved authority or migration; Rules1–17/schemas1–33 remain exact. See the
  [OE47 assessment](docs/original-economy/oe47-food-reliability.md).
- Authored safe demolition is a separate explicit rule19/policy3/schema35
  contract with marker `authored-safe-demolition-v1`. It inherits Rule18 and
  admits only never-occupied Common2x2 Houses with no retained needs/history,
  and paused, physically drained Clay Sources after their real return. Retire
  paid purchase and courier identities in the bounded eight-row journal;
  preserve extracted Clay, completed trips, construction and closed installed
  upkeep. No refund, ID reuse, resident reset, stock discard or budget increase.
  Other production/service/Elite classes remain refused with reasons. The
  historical neutral Well72 path remains exact. Recommended play remains Rule18;
  Rule19 requires its own menu/CLI/create/restore path. Rules1–18/schemas1–34
  do not migrate. See [OE51 demolition](docs/gameplay/oe51-demolition.md).
- Authored source-cell ramps are a separate explicit rule20/policy3/schema36
  contract with marker `authored-source-cell-ramps-v1`. Inherit classic Rule6
  economy and five-tick edges, not Original Rules7–19. Preserve old RampLinks
  and combine both ramp collections under64. World owns three exclusive axial
  claims, paid40 construction, shared flat/slope/flat surface, begun-edge safety
  and exact source-bound restore. Native raw ramps remain refused; original
  sprites233–244 stay isolated. No removal/refund, automatic migration or new
  source-admission exemption. See [OE53](docs/rendering/oe53-terrain-integration.md).
- Authored finite land trade is a separate explicit rule21/policy3/schema37
  contract with marker `authored-land-trade-v1`. It inherits classic Rule6,
  not Original economy or Rule20 terrain. Two paid existing Warehouses bind
  one 40-Funds source-geometry-bound gateway license and one caravan within
  existing limits. fixed four-unit Pottery portions, finite 8/8 offers and a 24-Funds export
  budget: prepaid import reserves owner headroom, loads only at real boundary
  arrival and credits stock only after return; export loads physical stock and
  earns money only at actual boundary arrival. No replenishment, cancel/refund,
  teleport, automatic migration or original gateway/partner inference. Preserve
  real cargo/escrow/reservations on road cuts and permanent bound Warehouse IDs.
  Model/difficulty bind identity only. See [OE54](docs/original-economy/oe54-trade.md).
- Authored Music venues are a separate explicit rule22/policy3/schema38 contract
  with marker `authored-music-venue-v1`. Inherit Rule18, not Rule19 demolition,
  Rule20 terrain or Rule21 trade. Only Music211 changes from targeted House visits
  to real one-way Performer34 arrivals at paid4x4 Theatre75. A fixed school-owned
  movement channel is separate from each monotone PerformerId; idle channels have
  no visible actor or position. Due training attempts are not successful spawns.
  Real accepted arrival alone sets the separate Venue32 timer. Phase35 training
  and unconditional venue countdown are authored clock choices. No House96 grant,
  Venue Actor36, religion/festival effect, automatic migration or default change.
  Two schools/two venues remain within the existing82-body/58-channel budgets.
  Require separately prepared75 model capability and exact source-bound restore.
  All Rules1–21/schemas1–37 and their School-to-House semantics remain exact. See
  [OE56 culture](docs/original-economy/oe56-culture-venues.md).
- Authored venue House service is a separate explicit rule23/policy3/schema39
  contract with marker `authored-venue-house-service-v1`. Inherit Rule22's paid
  School/Performer/Venue input and unchanged Rule18 economy. Each paid Theatre
  owns one existing bounded courier channel; each real spawn owns a distinct
  monotone VenueWalkerId. Full staffing and current Music gate the fourth eligible
  phase35 attempt. Reuse cached House target selection and real outward/return
  movement; the Original actor-centred roaming clock is not reconstructed.
  Only a real eligible occupied-House arrival with current Music and authored
  source operation/staff/fire checks assigns the existing sole Music96 timer.
  Preserve immutable first Music receipt, dispatch/grant Music evidence, actual
  House witnesses, ID/sequence/return/clock bounds and shared workforce/fire
  histories. No dispatch, stock, proximity, rendering or restore grant; no Theatre
  DES source, increased limit, automatic migration or default change. Rules1–22
  and schemas1–38 stay exact; Rule18 remains recommended. See
  [OE57](docs/original-economy/oe57-venue-house-service.md).
- Agriculture construction/Food foundations are an explicit Rule24/policy3/
  schema40 contract with marker `authored-agriculture-v1`, catalog
  `wheat7-millet5-rice6-cabbage3-beancurd1-v1` and separate revision28 state.
  Inherit Rule18 without venues/trade/demolition; keep legacy production Food
  strictly empty in24. Paid193/195–199 geometry, source-bound Rice fertility100,
  separate typed accounting and exact saves do not establish production.
  `agriculture_production_clock_verified` is false: no crop quantity, tending,
  harvest, Food or FoodQuality grant is admitted. Only fully proved Original
  time rules may activate; no authored calendar/160-step/fixed-yield substitute.
  Future activation needs a new explicit version/schema, never reinterpretation
  of24. Rules1–23/schemas1–39 and defaults remain exact. See
  [Agriculture](docs/original-economy/agriculture.md).
- Material plantations extend the explicit Agriculture lineage as Rule25/
  policy3/schema41, marker `authored-agriculture-plantations-v1`, revision1 and
  catalog `tea26-237-lacquer27-238-mulberry28-239-v1`. Paid26/237,27/238 and28/239
  use typed source bindings, range3,32 plants and six sources within unchanged
  82-body/58-channel limits. World owns authored80-tick finite readiness, real
  Grower pickup/cargo/return and regrowth; only actual source return credits
  the existing six-Goods/five-Aux ledgers. Shared five-tick land movement,
  staffing and unit recipes remain authored. No Fruit, independent calendar or
  irrigation effect is activated. Rule25 expressly retains the existing authored
  Wheat/Millet80-tick Food adapter in the sole five-kind ledger: one productive
  Wheat/Millet field per Farm, progress0–79, actual four-portion carts and House
  consumption with scores20/80; no Original quantity/events/cycle authority.
  Rice/Cabbage/Soy stay locked and Rule24 admits no Food origin. Rule24/schema40 and all older
  rules/saves remain exact without migration; a Rule25 writer may overwrite only
  an existing schema41 target. Current Agriculture new games select25; general
  defaults remain unchanged. Original work cadence/yields/seasonal parity and
  native-model city acceptance remain separate evidence gates.
- Irrigation extends that lineage through an explicit Rule26/policy3/schema42
  selection, marker `authored-agriculture-irrigation-v1`, irrigation revision1.
  Inherit Rule25's Food, plantations, workforce, movement and finance. Paid
  Ditch202 overlays and oriented2x2 Pump203 bodies use immutable raw terrain,
  policy3 admission, complete eight-water/two-land intake context and World
  occupancy. The observed phase27 fills cardinal connected paid ditches from
  four oriented Pump seeds. Positive staff, operation and inherited fire gate
  supply; derived wet/supplier caches never enter saves. Construction/restart
  waits for the next phase; loss of eligibility or a cut invalidates immediately
  through the documented authored World adapter. Five-cell perimeter queries
  expose actual water availability. Crop yield effects remain PARTIAL and
  inactive, with Rice/Cabbage/Soy production locked. Schema42 requires exact
  lineage markers, source/model identities and paid history; no migration or
  historical overwrite. Rules1–25/schemas1–41 retain their contracts. The
  separate menu/CLI selection is `--original-irrigation`; ordinary Agriculture
  continues to select25. See [Agriculture](docs/original-economy/agriculture.md).
- Bounded Grower navigation is an explicit non-production C++ World session,
  `GrowerNavigationPolicy::Mode17MicrostepV1`, not Rule27/schema43 or an Actor26
  production grant. It uses the existing shared navigator and58-channel budget,
  qualified region-absent/flat source facts, complete paid footprints and native
  direction/phase semantics. One World tick per microstep is AUTHORED; historical
  5-tick courier/plantation movement stays exact. No field work, raw cargo or Food
  is created. Unknown predicates reject the whole query; unsupported peers,
  suspension/Ferry/links/exceptional/global contexts never become arrival8.
  Preserve current/begun cells. The nonserialized snapshot guard refuses session
  save/restore publication until terminal cleanup; old schemas/defaults do not
  migrate. See [Grower navigation](docs/original-economy/agriculture.md#bounded-world-grower-navigation-agri-02e2).
- Autonomous tending is a separately selected, transient C++ World session,
  `GrowerLifecyclePolicy::AutonomousTendingV1`. Actual pre-tick Farm staffing,
  phase31 source attempts and a sealed SHA/header-bound entry operand gate
  typed monotone Actor26 allocation. Reuse the shared movement channel through
  actual arrival,51 callback invocations, tending, empty return and retirement.
  Keep native retained counters separate from live occupancy. The initial
  candidate must be unique: runtime-fertility scores are not admitted. One live
  actor and one raw slot are conservative context fences, not Original quotas
  or lifetimes. Actor callbacks/microsteps per World tick remain AUTHORED.
  World owns transient field assignments/work; never mutate historical field
  authority or grant growth, harvest, cargo or Food. Save/restore stays blocked
  even for a controller with no actor. Drain begun sections before cleanup;
  preserve historical continuation and monotone lifetime IDs. Rules1–26,
  schemas1–42 and defaults stay exact; this research policy allocates no saved generation. See
  [autonomous tending](docs/original-economy/agriculture.md#autonomous-tending-acceptance-agri-02e3).
- Transient field cycles are a separate `AutonomousFieldCycleV1` selection in
  that same World controller. Admit exactly one complete Food field, one crop,
  one live actor and the bounded fresh native configuration/veto/base-constructor
  context. The enabled-Food resource inputs retain E3's AUTHORED World scope;
  they are not a proved native resource-table import. Real tending alone
  establishes work; shared phase20 marks/grows the
  field and completed-cycle expiration/decay precedes actor work. Actual harvest
  selection/allocation/movement starts wait25. Without admitted live yield and
  normal incoming FP context, stop before the entire harvest callback, then use
  explicit empty transient cancellation. This is no completed harvest, physical
  cargo return or Farm receipt. Keep quantity/history/mark and cargo distinct;
  no raw bin or Food origin, saved generation, default or migration is activated.
  The existing session publication guard applies even with no live actor.
  Rules1–26/schemas1–42 and `AutonomousTendingV1` remain exact. See
  [first-harvest boundary](docs/original-economy/agriculture.md#first-physical-harvest-boundary-agri-02e4).
- Harvest context preparation is a read-only World operation returning an
  immutable, privately constructed `HarvestContextAdmission`. Bind actual
  source/model identities, signed Header+8 region, field/actor/assignment,
  due work and all120 dry-query cells through the shared effective-cell rules.
  Keep construction fertility separate from a live derived epoch. Rice/Cabbage
  have an event-doubling crop veto, without assigning event-object values.
  Current preparation returns typed refusals for unbound live FP/numeric epochs;
  no numeric profile is admitted. The real Wait25 boundary retains these reasons
  before empty cancellation. Startup PC53 and preserved FP state do not establish
  the current RC bits. Initial Standalone Properties preservation and the later
  Play reload/date seed are distinct authorities. See
  [live context admission](docs/original-economy/agriculture.md#live-harvest-context-admission-agri-02e5).
- Playable Agriculture is a separate explicit Rule27/policy3/schema43 mode,
  `authored-playable-agriculture-v1`, revision1, selected by the regular P menu
  and `--playable-agriculture`. It inherits Rule26 and activates all five typed
  Food crops through the shared AUTHORED80-productive-tick adapter. Each of at
  most five paid Farms owns one productive Field, within the same82-body and
  58-channel budgets. Full Farm staffing, operation, fire and road access gate
  work; a paused/burning Field also blocks work. One completed cycle creates
  one portion in the sole five-kind Farm account. Actual cart4 movement,
  reservations, Market/House arrival, quality witnesses and consumption retain
  the existing authority. Soy199 yields BeanCurd1; Rice retains source
  fertility100 construction and has no universal Pump requirement or irrigation
  yield bonus. Timing/yield/scores are expressly authored, not Original raw
  Harvest acceptance. No Original EXE, Windows VM/debugger, Wine or live x87
  values are prerequisites for27. Historical Rules1–26/schemas1–42, defaults,
  Original research sessions and their refusal domains remain exact; no
  automatic migration or cross-schema overwrite. See
  [Playable Agriculture](docs/original-economy/agriculture.md#playable-agriculture-agri-mac01).
- Playable Fishing is a separate explicit Rule28/policy3/schema44 successor,
  `authored-playable-fishing-v1`, revision1, regular F menu and
  `--playable-fishing`. Its sole six-column FishingFoodState extends the same
  typed Food engine with Fish2; old five-column Agriculture and two-column Food
  ledgers are absent/empty. Two paid source-priced/staffed Quay31 bodies retain
  all four oriented2x2+dock five-cell claims. Each owns one courier and one
  reserved boat channel within unchanged82/58 budgets. Shared water traversal,
  actual5-tick boat edges, finite8-portion areas with1/240 regeneration,
  phase31 attempts,40-work-tick portions and20-tick real dock unloads are
  AUTHORED. Marker179 is geography only; normal-water fallback sites are
  explicit and bounded16. Only actual dock return/unload credits Fish stock;
  boat cargo, area depletion, receipts and six-kind Food balances stay distinct.
  Full staff, operation/fire/access, begun-edge safety, capacity/reservations,
  physical Market/House arrivals and actual consumption remain World authority.
  Static Original Quay artwork uses authored pose/anchor selection; unqualified
  Boat49 animation uses a labelled authored fallback. Rules1–27/schemas1–43,
  defaults and Original research stay exact, with no migration/overwrite.
  See [Playable Fishing](docs/original-economy/agriculture.md#playable-fishing-agri-mac02).
- Playable Hunting is a separate explicit Rule29/policy3/schema45 successor,
  `authored-playable-hunting-v1`, revision1, regular Y and
  `--playable-hunting`. It inherits Rule28; its sole seven-kind HuntingFoodState
  adds Meat4 with authored quality85. Paid2×2 source-priced/staffed Tent33 owns
  one Food courier and one Hunter40 slot. Four finite habitats reserve three
  animal channels each within unchanged82/58 budgets. Living75/76/77 identities,
  deterministic roaming40/birth720, actual pursuit,60-tick contact hunt,
  one-portion cargo, physical return and20-tick unload are AUTHORED. Only real
  unload credits Food; exclusive targets, current/begun edges, cargo, histories,
  population and Meat/Food equations survive exact schema45 restore. Prey183
  supplies geography only; qualified normal-land fallback is explicit. Births
  require empty ordinary grass; roads support traversal, never spawning. No
  raw50/100/400 conversion, Original ecology/animation parity, historical
  reinterpretation, automatic migration or cross-generation overwrite.
  See [Playable Hunting](docs/original-economy/agriculture.md#playable-hunting-agri-mac03).
- Multi-field Agriculture is explicit Rule30/policy3/schema46, marker
  `authored-multi-field-agriculture-v1`, revision1, regular C menu and
  `--multi-field-agriculture`. Inherit Rule29 and its sole seven-kind Food ledger.
  At most eight paid fields per Farm share one fully staffed productive work
  unit/tick, canonical Field-ID round robin; field pause/fire skips independently.
  Full reserved source, Farm pause/fire/staff/access freeze work/cursor. Separate
  per-field lifetime work derives progress modulo80 and must equal the existing
  sole harvest count times80 plus progress. Preserve paid-release-time suffix
  quotas, cursor provenance, physical carts/reservations/consumption, five Farms,
  32 fields and82/58 budgets. No physical Grower or Original harvest claim,
  migration, historical reinterpretation or cross-generation overwrite. Rules1–29/
  schemas1–45 remain exact. See [multi-field Agriculture](docs/original-economy/agriculture.md#multi-field-agriculture-agri-mac05).
- Physical Food Growers are explicit Rule31/policy3/schema47, marker
  `authored-physical-food-growers-v1`, revision1, E menu/`--physical-food-growers`.
  Inherit30 with one additional counted Farm Grower channel, shared source staff,
  canonical field assignments, real5-tick land edges,5-tick field wait and at most
  one work unit/Farm/tick. Only at-field work earns an actual one-crop harvest;
  only physical source return credits the sole seven-kind Food account. Keep
  field harvest, live cargo and returned counters distinct; reserve one return
  slot for the whole live trip and settle genuine loaded returns even after
  source pause/fire/staff loss. Empty cancellation drains begun edges and retains
  fractional work; no ID reuse, crop reassignment, stock discard or extra staff.
  Preserve82/58, five Farms/32 fields/eight perFarm, exact source/route/lifetime/
  receipt validation and all Rules1–30/schemas1–46 without migration/overwrite.
  Raw Actor26/x87 research remains separate and unchanged. See
  [physical Growers](docs/original-economy/agriculture.md#physical-food-growers-agri-mac06).
- Seasonal Agriculture is explicit Rule32/policy3/schema48, marker
  `authored-seasonal-agriculture-v1`, revision1, Z/`--seasonal-agriculture`.
  Derive all nine growing windows from the sole completed World tick,51×16×12.
  Source-observed relative windows are distinct from authored suppression,
  80-work readiness, five actual Food harvest-work ticks, partial-cycle loss and
  one end-slot grace/expiry. Food still requires real tending and Farm return;
  material plants still require pickup/source return. Hemp owns one counted
  physical worker channel, distinct from its existing Warehouse cart, and credits
  Goods only after source return. Expiry never discards picked cargo or stock.
  Preserve lifetime work/waste/harvest equations, exact ready epochs, reservations,
  begun edges,82/58 limits and all Rules1–31/schemas1–47 without migration or
  cross-generation overwrite. Water yield effects remain inactive. No Original
  calendar, work cadence, harvest arithmetic or animation parity is claimed. See
  [seasonal Agriculture](docs/original-economy/agriculture.md#seasonal-agriculture-agri-mac07).
- Irrigation productivity is explicit Rule33/policy3/schema49, marker
  `authored-irrigation-productivity-v1`, revision1, X/`--irrigation-productivity`.
  Inherit Rule32 and preserve actual base-work tick histories. Each otherwise
  eligible actual wet growth tick on the five Food fields, Hemp and three
  plantation kinds earns one quarter bonus; every fourth adds one growth unit,
  cap80 with consumed clipped excess. Per-crop carry is wet modulo4 and persists
  without creating work while dry/ineligible. The existing end-of-tick phase27
  water cache alone grants supply; fire/staff/operation/cuts invalidate through
  existing boundaries, restart waits for real refill. No stacked Pumps, harvest
  work bonus, direct stock, changed portions, Rice Pump requirement or extra
  bodies/channels. Physical arrival/return stays sole inventory authority.
  Exact earned growth is base+bonus; wet/4=bonus+clipped. Schema49 binds all crop
  rows, Food dispatch/receipt bonus epochs and immutable five-counter material
  readiness witnesses. Enforce exact same-ready counters and ordinal/physical
  history; known ditch epochs are checked outside normal ticks. Never serialize
  wet caches. The successor leaves Rules1–32/schemas1–48/defaults exact without migration/overwrite.
  See [irrigation productivity](docs/original-economy/agriculture.md#irrigation-productivity-agri-mac08).
- Food processing is explicit Rule34/policy3/schema50, marker
  `authored-food-processing-v1`, revision1, Q/`--food-processing`. Inherit33
  and use separately prepared paid5×5 Mill53/2×2 FoodShop66 within82/58.
  A Shop permanently binds one earlier cardinal-adjacent paid Market; no
  parent/Shop inventory mirror. The sole ProcessedFoodState has seven raw
  ingredients and five grade-tagged lots of the same Dinners28 commodity.
  Grades1–4 consume one of each selected distinct raw kind once at commitment;
  AUTHORED20 staffed ticks per ingredient create the same count of meals only
  at completion. Full shared capacity, active output and real return reservations
  persist. Raw quota2 per kind prevents Mill intake deadlock. Real Mill→Shop→House
  arrivals alone carry meals/quality; the sole House event consumes one portion,
  meals first, without a second need grant. Direct raw arrivals reserve at most4
  of the unchanged8 House slots. Salt/Spices/grade5 and Warehouse Food remain
  unavailable. Preserve recipe/debit/work/completion and immutable first meal
  arrival/consumption provenance. All Rules1–33/schemas1–49/defaults remain
  exact, without migration or cross-schema overwrite; native linked assistants,
  unit conversion, original recipe timing and artwork parity remain unresolved.
  See [Food processing](docs/original-economy/agriculture.md#food-processing-agri-mac09).
- Saves are OpenEmperor JSON, never original Emperor savegames. Validate complete
  identities, versions, source hashes, geometry, accounting, goods, paths and limits
  before atomic publication. Derived indexes/caches do not become saved authority.
  A rejected command or restore must leave state, IDs, funds and source bytes intact.
- Use typed monotone entity IDs, authoritative complete footprints and shared edge
  legality for placement, routes, movement, inspection and restore. Preserve cargo,
  reservations, begun-edge safety and ordinary waiting/repair on road disconnection.
  Do not infer delivery/coverage from proximity, dispatch or a planned visit.
- Map permission support is conservative and load-time. Complete source-manager,
  schema, identity/reference, Diamond/bounds, physical/marker/gate conflicts and
  source/policy fingerprints remain mandatory. Unknown or conflicting states fail
  closed. Never whitelist by map name, source ID, coordinates, terrain appearance or
  a general zero-side/type-range exemption; preserve each evidenced source predicate.
- Original-economy model identity, difficulty arithmetic, lifecycle and phase rules
  are bounded contracts. Keep authored production, immigration, workforce and
  school-to-house behavior distinct from original evidence. Numeric parity does not
  establish original object, illness, venue/performer or full paid progression parity.
  See the [current architecture](docs/original-economy/architecture-map.md).

## Rendering and input

- Prepare source facts, geometry, masks, materials, decode/upload and topology outside
  frame/hover loops. No frame file/parse/decode/upload/readback, World copies/commands,
  ticks, BFS, route refresh, full-map scans or all-pairs graphs. Diagnostic tools must
  reuse production authority and keep instrumentation out of normal frame/tick work.
- Keep existing bounded resource budgets and eager physical-asset deduplication.
  Asset-browser textures stay within 64 textures/64 MiB; Type30 split textures count
  against the existing 64 MiB budget. Authored cliff sides have at most 512 edges,
  4096 runs per state and 8 MiB combined capacity; joined ramps retain 64 links,
  one preview, 65 mask entries and 512 faces/4096 runs per state/8 MiB.
  Do not raise budgets to hide unsupported data.
- Activate metadata-only compatibility packs only after every required SHA-256
  matches atomically. Resolve through the shared resource/layout and payload readers;
  retain bounded offsets, symlink containment, optional-asset diagnostics and complete
  historical fallback. No copied original pixels or private binary tables in resources.
- Preserve signed source heights, shared projection, feet, anchors, culling, cached
  alpha picking and local painter contracts. Historical flat Snapshot and Bridge
  geometry stay exact for scoped ramp/cliff work. General original height
  normalization, interpenetration and whole-scene parity remain unresolved.
- Road art, saved-image fallbacks, shore landings and Great Wall/quarry previews are
  presentation only. Great Wall material requires validated restore context or an
  explicit session preview. `--authored-terrain-sides` is session-only/defaultOFF.
  Original ramp sprites 233–244 remain an isolated source-cell prototype; do not
  substitute their height/anchor semantics for production RampLinks without a contract.
- Texture access uses the existing runtime-bounded compatibility decision: SDL 3.4.14
  software renderer uses STREAMING for eagerly prepared scaled sprites; other
  renderer/runtime pairs retain STATIC. No per-frame recreation or upload workaround.
- UI buttons and keyboard share one action path. Convert positional SDL events once
  with `SDL_RenderCoordinatesFromWindow`, retaining fractional coordinates and
  rejecting outside/nonfinite input. No speculative DPI/camera/global offsets.
  Focus/layout/modal/UI-release boundaries cancel gestures. Road previews remain
  pure/coalesced, bounded to 256 cells, with atomic revalidation on commit.

## Maintained contracts

Read the affected contract before changing its subsystem. Precise source admission,
selector tables, resource limits and historical compatibility remain defined there:

- [Architecture](docs/architecture.md), [City systems](docs/city-systems.md),
  [profile/schema compatibility](docs/gameplay-sandbox.md#legacy-profiles-and-save-compatibility)
  and [City-v16](docs/city-v16.md).
- [Map permissions](docs/map-permissions-compatibility.md),
  [original roads](docs/original-roads.md), [gate passages](docs/gate-passages.md)
  and the source-specific proofs in `docs/reverse/` referenced by `SandboxPlacement.cpp`.
- [Bridges](docs/city-v16-bridges.md), [Ramps](docs/city-v16-ramps.md),
  [authored cliff cuts](docs/rendering/cliff-ramp-integration.md),
  [joined ramp material](docs/rendering/multi-ramp-integration.md) and
  [original ramp prototype](docs/rendering/original-ramp-cell-prototype.md).
- [Scene composition](docs/rendering/scene-composition.md),
  [texture compatibility](docs/rendering/texture-compatibility.md),
  [road](docs/road-visual-profile.md), [building](docs/building-visual-profile.md)
  and [walker](docs/walker-visual-profile.md) profiles.
- [Native input](docs/input-reliability.md),
  [Service coverage](docs/city-v16-service-coverage.md) and
  [long-play diagnosis](docs/city-v16-longplay-stability.md).

## Validation and repository hygiene

- Run checks appropriate to the change using [testing](docs/testing.md). For affected
  original-data integrations use their isolated launchers; preserve all input bytes.
  Test meaningful behavior and historical compatibility, not copies of implementation.
- Separate synthetic/direct-render checks, scripted SDL, native delivery and human
  acceptance. Open screenshots to inspect them. Do not claim manual acceptance from
  automation, universal fairness from finite samples or profit from lifetime tax.
  Stop desktop automation immediately when the user takes over.
- Package only our executable, actual non-system dependencies and their license
  notices. Ad-hoc signing is not notarization or Gatekeeper acceptance; see
  [macOS packaging](docs/macos-packaging.md). No commits, pushes, tags, releases or
  publication without user authorization; existing explicit authorization applies.
- Maintain existing guides and source contracts. Completed task prompts, milestone
  diaries and duplicate validation reports belong in Git history, not new Markdown
  archives. Keep README focused on getting the game running and current limitations.
