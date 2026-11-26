# Known issues

OpenEmperor is an alpha. Supported sandbox behavior does not establish
complete original-game compatibility.

- **Latest Agriculture city reliability remains partial.** A paid Rule36 city
  consumes all seven raw Foods and meals over100000 further ticks, but records
  deficits and House devolutions. The separate supplied-model Xia V2 plan now
  consumes Tea/Silk/Lacquerware and completes20400 post-maturity ticks with
  nonnegative Funds, but its Elite House devolves8 times and lacks uninterrupted
  FoodQuality70. It does not contain all Food/meal chains. These finite scenarios
  do not establish sustainable citywide supply. The complete Alpha now passes
  437 tests per Debug/Release/sanitizer configuration; its prior timeout and
  stale-fingerprint failures remain retained separately. See the [measured gates](docs/testing.md#final-agriculture-integration-agri-mac14)
  and the [human checklist](HUMAN_PLAYTEST.md); human acceptance remains NOT_RUN.

- **Agriculture animation is authored.** Rule36 displays actual saved growth,
  movement, work and cargo with qualified static/stage art or authored geometry.
  Original Grower/Boat/Hunter/animal animation registration and Mill/FoodShop
  artwork remain unqualified. Rice keeps its static paddy art with actual-state
  overlays. Earlier modes retain their historical visuals. See the
  [provenance matrix](docs/building-visual-profile.md#rule36-agriculture-state-presentation-agri-mac12).

- **Agriculture reconfiguration is bounded.** Explicit Rule36/schema52 supports
  Food-field, plantation-plant and Pump retirement after real drainage. It keeps
  eight pending/closed entries and all lifetime purchase budgets. Farmhouses,
  sheds, Quays, Tents, Mills, FoodShops and Warehouses remain refused, including
  empty owners; their stock/channel/source closure is not implemented. Existing
  Road/Ditch removal remains separate. No refund, cargo cancellation or automatic
  removal is provided. See the [matrix](docs/original-economy/agriculture.md#safe-agriculture-reconfiguration-agri-mac11).

- **Further production chains remain research-only.** The existing Wheat/Millet,
  Hemp, Clay/Ceramics, Tea, Silk, Bronzeware, Lacquerware and Paper paths include
  authored harvest periods, recipes or resource management. Additional model
  labels and their raw Cost/Employees/Resource columns do not establish complete
  crop, mining, fishing or seasonal behavior. OE55 adds no new crop, World rule,
  save schema or playable production chain. See the
  [production evidence boundary](docs/original-economy/oe55-production.md).
- **Playable Agriculture is expressly authored.** The separate Rule27/schema43
  `Playable Agriculture` selection enables Wheat, Millet, Rice, Cabbage and
  Soy-to-BeanCurd through the shared80-productive-tick Food adapter, physical
  Farm/Market/House delivery and consumption. It inherits Rule26 irrigation and
  material plantations; Rice still requires source fertility100 but not a Pump.
  There is no irrigation yield bonus. Rice uses the verified static paddy art;
  an active growth overlay remains unqualified. Native menu selection was
  observed; a manual native city playtest remains outstanding. This gameplay
  does not establish Original
  harvest timing, raw-yield units or x87 runtime parity. The following Original
  research limitations remain applicable to their historical/research contracts,
  not as a lock on Rule27. See [the playable contract](docs/original-economy/agriculture.md#playable-agriculture-agri-mac01).
- **Playable Fishing is a separate authored mode.** Explicit Rule28/schema44
  adds Fish2 to one six-kind Food authority, real boats, finite fishing areas,
  regeneration and physical Quay/Market/House supply. Movement, work, portions,
  ecology and quality are authored; they do not reproduce the unclosed Original
  runtime. Quays need complete flat normal-water shore context. Original Quay
  pixels use curated static pose/anchor selection; boats use a labelled authored
  fallback because their animation registration is not qualified. Historical
  Original hunting/fishing research remains separate. See the
  [fishing contract](docs/original-economy/agriculture.md#playable-fishing-agri-mac02).
- **Playable Hunting uses explicit authored ecology.** Rule29/schema45 adds
  living prey75–77, real Hunter40 pursuit/hunt/return, Meat4 and one seven-kind
  Food authority. Habitat admission, reproduction, movement, timing, portions
  and Meat quality are authored; native Original hunting parity remains open.
  Unqualified Tent/animal/Hunter sprites use labelled authored fallbacks. The
  [hunting contract](docs/original-economy/agriculture.md#playable-hunting-agri-mac03)
  separates these rules from historical Original research and saves.
- **Food processing is a separate authored successor.** Rule34/schema50
  supplies paid Mill53/FoodShop66, finite real ingredient/meal transport,
  staffed recipes and actual House meal consumption in the sole Food account.
  Portion conversion, work, grade policy and shop ownership are authored;
  Original linked assistants/parent-slot behavior and artwork parity remain
  unresolved. No Salt/Spices origin, grade5 or Rule34 Warehouse Food storage exists.
  Rules1–33/schemas1–49 and defaults stay exact. See
  [the contract](docs/original-economy/agriculture.md#food-processing-agri-mac09).
- **Food Warehouses require their own authored successor.** Rule35/schema51
  extends the sole Food account with paid Warehouse owners, a bounded separate
  Food compartment and physical redistribution. Its capacities, extra courier
  and routing policy are authored, with no native Warehouse reproduction claim.
  Warehouse demolition remains refused. Rules1–34/schemas1–50 and defaults stay
  exact. See [the contract](docs/original-economy/agriculture.md#food-warehouses-agri-mac10).
- **Irrigation productivity is a separate authored successor.** Rule33/schema49
  adds bounded+25% earned growth work on actual wet eligible crop ticks for all
  nine crop types. It changes cadence, never the one-portion harvest, physical
  transport or stock authority. Actual network phases, fire, staffing and cuts
  control the effect. Native Original yield/fertility/Flood/x87 parity remains
  open; human playtest follows the final milestone. Historical Rules1–32 retain
  their exact semantics, including inactive numeric effects in32 and older.
  See [the contract](docs/original-economy/agriculture.md#irrigation-productivity-agri-mac08).
- **Seasons are an explicit authored successor.** Rule32/schema48 uses the
  shared51×16×12 World calendar for five Food crops and Hemp/Tea/Lacquer/Mulberry.
  Relative windows are source-observed; work suppression, partial loss, grace,
  expiry and actual five-tick Food harvest work are authored. Hemp now requires
  a real worker return. Picked cargo and stored goods survive winter. Original
  seasonal/harvest parity remains open; irrigation yield effects are inactive.
  Hemp markers can be occluded by existing building art; source inspection
  still shows actual cargo. Historical generations stay exact. See
  [the contract](docs/original-economy/agriculture.md#seasonal-agriculture-agri-mac07).
- **Physical Food Growers are authored.** Explicit Rule31/schema47 replaces
  abstract crop credit with one actual Farm-owned worker, at-field tending,
  typed crop cargo and physical return receipts. Crop timing, yield and worker
  presentation are authored; Original Actor26 harvest/animation parity remains
  separate. Source bounds and82/58 resource limits remain strict. Rule30 and
  historical saves retain their own contracts. See [the contract](docs/original-economy/agriculture.md#physical-food-growers-agri-mac06).
- **Multi-field Agriculture shares authored Farm labor.** Explicit Rule30/schema46
  allows up to eight independently progressing paid fields per Farm with one
  shared work unit/tick and the existing sole seven-kind Food account. Physical
  crop Growers are available separately in Rule31 and seasons in Rule32; irrigation
  yield effects are separately opt-in through Rule33.
  There is no Original field-work or harvest-parity claim. Rice retains static
  qualified paddy art. Historical rules and save generations stay exact. See
  [the contract](docs/original-economy/agriculture.md#multi-field-agriculture-agri-mac05).
- **Luxury consumption and citywide reliability are separate.** The unchanged
  supplied-model Rule29 Xia check earns real Tea/Silk/Lacquerware House
  consumption and20,400 subsequent Elite17 ticks. That House still has53 unmet
  current-need evaluations; the other twelve Houses incur51 devolutions and
  8,090 combined burning House-ticks. This finite integration result does not
  establish uninterrupted supply, citywide stability or Original parity. See
  [the measured scope](docs/testing.md#high-tier-luxury-consumption-agri-mac04).
- **Original Agriculture timing is incomplete.** Frozen Rule24/schema40 provides typed
  Rice/Cabbage/Soy (source output BeanCurd), paid source-verified Farm/Field
  construction, Rice fertility100, Food accounting foundations and exact saves.
  It creates no Food and grants no FoodQuality: the complete Original Grower
  tending/harvest clock and yield transition are unproved. Rule25 expressly
  retains the existing authored80-tick Wheat/Millet adapter with actual physical
  Food deliveries and consumption. Rice/Cabbage/Soy remain locked in25; all Food
  remains locked in24. No Original seasonal production clock is activated.
  AGRI-02B is PARTIAL. Its new bounded rules component covers proved crop
  identities, ordinary zero initialization, relative clock observations and
  work/wait/base-yield arithmetic. Complete post-purchase/restore effects,
  worker tours, runtime yield inputs and
  raw-to-Food units still prevent production activation. Rule26/schema42 adds
  separately selected irrigation infrastructure. AGRI-05 adds tested raw
  hunting/fishing state operations, complete Quay geometry and read-only
  inspection. Original hunting/fishing reconstruction remains blocked by ecology, navigation,
  callback eligibility, raw-to-Food conversion and complete quality/transport
  contracts. The separate authored Rule28/29 successors provide playable fishing
  and hunting. See the [component matrix](docs/original-economy/agriculture.md#hunting-and-fishing-component-matrix-agri-05).
  AGRI-06 adds a derived view of the existing World51×16×12 clock and nine
  verified seasonal reference queries. The one-tick-per-phase mapping and
  elapsed-year epoch remain AUTHORED. Hemp has no direct calendar-growth veto;
  Tea/Mulberry suppress only the last end, while Lacquer suppresses either end.
  These queries do not activate Original seasonal production or convert
  authored80-tick progress into Original work. Complete worker/yield/unit and
  live climate/region inputs remain open. No new rule, schema or migration is
  introduced. See the [seasonal matrix](docs/original-economy/agriculture.md#common-calendar-and-seasonal-reference-matrix-agri-06).
  AGRI-02C adds verified quota/attempt arithmetic, the exact crop-qualified
  doubled-base predicate and raw Farm/Cart bin/partial-receipt operations.
  Complete active-worker lifetimes, live yield inputs/precision and the
  raw-to-World Food contract still gate Rice/Cabbage/BeanCurd production.
  The native Farm caller ignores returned overflow. The World conservation
  policy and any authored successor require an explicit decision. See the
  [crop completion matrix](docs/original-economy/agriculture.md#crop-completion-and-decision-boundary-agri-02c).
  Further Original reconstruction implements field end-slot marking/expiration,
  normal raw House consumption and Mill recipe arithmetic as pure operations.
  Initial53-bit precision, a numeric map-region operand and archived bin capacity
  are now observed. Recipe availability can disagree with rounded ingredient
  requests; retained partial requests and a positive native return do not prove
  successful production. The inspected debit-before-assistant-allocation path
  has unresolved native loss/overcommit conditions. These facts grant no new
  World Food or saved authority. The chosen direction remains Original
  reconstruction; an authored successor is inactive.
  AGRI-02D implements resolved Actor26 selection/work/return/cleanup events,
  exact context-supplied harvest rounding and field/history effects. One native
  actor WORD carries raw yield; counter double-decrements, failed-return10 and
  ignored bin overflow remain explicit. Full live parameter admission,
  movement/interruption, exceptional states and temporal crop homogeneity still
  prevent a complete raw Farm production mode. No new save authority is active.
  See the [AGRI-02D/02E matrix](docs/original-economy/agriculture.md#original-food-grower-reconstruction-agri-02d).
  AGRI-02E remains PARTIAL: actual mode17 routing, native20-phase movement,
  linked State5 interactions and Actor48 carrier-owned suspension13c are now
  qualified, but not bound to the shared World Navigator. Regular field
  deactivation unregisters the field; retained last-field references and native
  slot reuse still require a temporal resource-identity proof. Runtime fertility
  is separately loaded from construction fertility; startup defaults do not
  supply admitted live yield operands. Negative paid World/save controls cover
  all five fields in24/25/26. No physical Original Food Grower, raw Farm stock,
  active/loaded Original Grower save exists. The independent AUTHORED Rule27/schema43 mode uses no such worker authority. See the
  [physical integration matrix](docs/original-economy/agriculture.md#physical-food-grower-integration-and-completion-matrix-agri-02e).
  AGRI-02E2 now integrates a bounded flat mode17 motion policy through the
  shared World navigator, including actual cell commits and8/9/10 outcomes.
  It is an explicit non-production C++ acceptance session: no Actor26 entity,
  work/cargo/receipt or saved generation is activated. One World tick per native
  microstep is AUTHORED; actual speed/cadence and peer/Ferry/link/global/exceptional
  lifetimes remain unsupported. Transient sessions must be detached before
  historical save/restore publication. See the
  [navigation matrix](docs/original-economy/agriculture.md#bounded-world-grower-navigation-agri-02e2).
  AGRI-02E3 adds actual autonomous Actor26 tending in an explicitly selected
  transient C++ World session: paid/staffed Farm dispatch, real field movement,
  51 work callbacks, empty source return and retirement. It is **PARTIAL**:
  multi-candidate runtime-fertility scores, concurrent peer lifetimes, unrestricted
  source histories and positive actor persistence remain unproved. One live
  actor, a unique initial candidate and one raw slot bound the acceptance
  context. Session work never becomes historical field state, harvest or Food;
  no normal menu/CLI game activates it. See the
  [autonomous tending matrix](docs/original-economy/agriculture.md#autonomous-tending-acceptance-agri-02e3).
  AGRI-02E4 adds a separately selected transient single-field cycle: actual
  tending, phase20 growth, end-slot mark, autonomous harvest selection and
  movement through wait25. It remains **PARTIAL**: the actual harvest callback
  is blocked before mutation because the incoming normal FP context and other
  live yield owners remain unbound. Empty cancellation at the field is not a
  harvested return; no cargo or Farm credit is created. Unharvested quantity
  expires through the shared calendar. The ordinary map's post-load runtime
  fertility recompute and a dry-irrigation source branch are now qualified;
  neither supplies a complete live yield context. See the
  [first-harvest matrix](docs/original-economy/agriculture.md#first-physical-harvest-boundary-agri-02e4).
  AGRI-02E5 adds immutable World harvest-context preparation and typed actual
  Wait25 refusals, including source/model/actor/field identities, signed region
  and the full dry120-cell query. Rice/Cabbage's event-result veto is qualified.
  The complete immediate native Actor26 path preserves its incoming FP state;
  actual RC alone still changes a conditional PC53 yield64/65. Native start/date,
  modifier/factor and FP epochs remain unbound. The initial Standalone load
  preserves the pre-load Properties year, while the later Play prelude can
  reload it before the separate date seed. E5 remains **PARTIAL**, with no
  admitted numeric context, marked callback or Raw-cargo grant. See the
  [live-context matrix](docs/original-economy/agriculture.md#live-harvest-context-admission-agri-02e5).
  Menu/inspectors show the lock. See the
  [Agriculture contract](docs/original-economy/agriculture.md).
- **Irrigation yield effects remain PARTIAL.** Explicit Rule26/schema42 has paid
  Ditch202/Pump203 construction, four orientations, source-bound shoreline
  admission, staffed water distribution, wet/dry graphics, field water queries,
  UI and persistence. Numeric crop modifiers, Original fertility/yield unit
  conversion and flood-event lifecycle remain inactive. The inherited authored
  Wheat/Millet and plantation harvest adapters keep their own quantities.
  Pump poses are static because their operating animation cadence is unproved.
  Existing immutable Original ditches and unsupported source objects cannot
  become paid player networks. See the [irrigation matrix](docs/original-economy/agriculture.md#irrigation-infrastructure-and-completion-matrix-agri-04).
- **Material plantations retain authored work rules.** Explicit Agriculture
  Rule25/schema41 adds paid Tea26/237, Lacquer27/238 and Mulberry28/239 plant
  readiness, actual Grower pickup/return and regrowth into the existing typed
  processing/House chains. Growth80, unit yields, source binding and shared land
  movement remain authored; full Original tending cadence, seasonal regeneration,
  yield units and irrigation effects remain unproved or inactive. Food-positive
  consumer controls earn actual Wheat/Millet Food and Water/Ancestor service;
  earlier Food0 controls remain separately identified. Tests lower authored model
  prerequisites; they do not establish native-model city progression or human
  acceptance. Current validation is reported separately. Model29 is a checked
  submenu marker and adds no Fruit commodity. See the
  [completion matrix](docs/original-economy/agriculture.md#material-plantations-and-completion-matrix-agri-03).
- **Land trade is authored, finite and opt-in.** Rule21/schema37 uses classic
  Rule6 and two paid Warehouses, one explicitly bound boundary gateway and one
  real caravan. Only four-unit Pottery imports/exports at authored unit prices 6/3 are
  supported, with 8 units each and a 24-Funds export budget. Original merchant
  routes, partner lists, pricing units, ships/ports and empire trade remain
  unsupported; model labels/markers grant no gateway. No mid-trip cancellation,
  refund, replenishment or bound-Warehouse demolition exists. See [OE54](docs/original-economy/oe54-trade.md).
- **Source-cell ramps are authored and opt-in.** Rule20/schema36 inherits the
  classic Rule6 economy and adds one bounded one-level source-cell surface.
  It does not activate original raw ramps or native233–244 artwork. All four
  freshly checked original maps retain zero legal differing-height normal-land
  pairs. Original normalization, actor rounding, compositor, Ferries and complex
  wall geometry remain unresolved. See the [OE53 scope](docs/rendering/oe53-terrain-integration.md).
- **Original economy is experimental.** **Play Original economy** explicitly
  selects the existing recommended Rule18/schema34 for a new city. It adds no
  World rule, save schema or migration; **Advanced / Legacy** retains all earlier
  choices and shortcuts. City-v11 rule3 and ordinary City-v16 rule4 defaults
  remain exact. Rules10–19 are opt-in and include authored
  immigration, workforce, recipes, harvesting, school-to-house visits and Rule13
  needs-aware goods selection.
  The separate [Rule22 music-venue experiment](docs/original-economy/oe56-culture-venues.md)
  replaces Music211's direct House visit with a paid one-way performer to
  Theatre75 and its own duration. It grants no House Music96 effect in Rule22. The separate
  [Rule23 Theatre-to-House experiment](docs/original-economy/oe57-venue-house-service.md)
  adds actual current-Music House visits through an explicitly authored targeted
  roundtrip. Its paid Xia/Normal five-House city completes two School arrivals,
  a real venue-walker lifetime and 20400 active continuation ticks with at least
  five occupied Houses/population 110 and 74 further Music visits. Its Plain
  stage predates Music, the Watch later burns, and finite positive operations
  do not establish full progression, citywide fire protection or lifetime profit.
  Original Actor36's roaming/RNG clock and binary-save lifecycle,
  other venue chains, festivals, Hero/God AI and illness remain unresolved.
  The bounded chain has automated World, persistence and scripted SDL coverage;
  native/human acceptance is NOT_RUN. Rules10–18 retain
  their House/production demolition refusal; incompatible development saves are refused.
- **Original artwork remains bounded.** OE52 adds ten source-registered static
  bases in Original production, sharing the existing Well image. Static source
  pixels and authored front-cell anchors do not establish original animation
  cadence, runtime pivots or whole-scene composition parity. OE60 adds the
  source-published static Hemp Farm192 and Farmhouse193 bodies. OE60.5 adds
  six original images per Hemp194, Wheat195 and Millet196 field, selected by
  an explicitly authored mapping of the paid parent Farm's existing 80-tick
  progress. Original per-field quantity, variation and calendar/work state
  remain unreconstructed; these images do not establish original growth parity.
  OE61 adds static Logging38, Smelter39, Bronzeware42, Lacquerware44,
  Tea237, Refinery238, Silkworm239 and Ancestor214 bodies matching existing paid
  identities and footprints. No stock/load, paused, burning or understaffed
  image variant is inferred; the existing operation and fire overlay remain
  authoritative. Rule25's separate optional plantation supplement prepares16
  native plant images using an expressly authored80-tick-to-quantity projection;
  Original growth timing, harvest-state animation and whole-scene parity remain
  open. Grower26 keeps a named worker marker; no Original Grower animation/feet
  parity is claimed. Missing plant assets keep named markers.
  Missing ownership or incompatible sources retain named authored fallbacks.
  Unassigned service/religion/luxury walker families and pending
  Elite plots retain their named historical fallbacks. Existing Market,
  Warehouse and FireWatch core previews are not newly identified original
  registrations. Clay/Pottery pixels are source-qualified35/43 images, while
  their generic World identities remain curated previews. See the
  [OE61 inventory and limits](docs/rendering/oe61-original-building-visuals.md),
  [OE52 evidence](docs/rendering/oe52-original-visuals.md) and
  [OE60 farm/field boundary](docs/rendering/oe60-original-farm-visuals.md).
- **Walker parity remains bounded.** The separately fingerprinted OE62 profile
  supplies twelve-phase native walks for twelve visual roles, including the
  actual Acupuncture208, Herbalist207, Musician34, Acrobat32, Actor33 and Theatre
  Actor36 source families. Clay/Pottery/Household, Inspector and Food/Market
  profession associations remain curated. Religion, Water, Ancestor and further
  material/luxury couriers retain named markers; immigration has no separate
  World actor. Feet, static idle, global phase origin and two/four-tick cadence
  are authored. Original load/action/idle variants, timing, roaming and complete
  profession reconstruction remain partial. Existing alias/image/memory limits
  are unchanged; unavailable art retains historical profiles. See the
  [OE62 inventory and evidence boundary](docs/rendering/oe62-original-walker-animations.md).
- **Fresh Common Type2 artwork has a narrow original binding.** The pinned
  normal purchase path publishes group `0x47a`, variant0, physical1508 before
  resident admission; the ordinary setter's `0x471`/physical1509–1510 is a
  different lifecycle context. Native1508 is selected only for an actual2×2
  Common House in Original production with the exact empty Vacant2 state and
  no recorded first admission. Existing validated Housing history supplies that distinction;
  there is no new World or save authority. Missing/incompatible source art and
  F4 retain the authored foundation; unsupported body/lifecycle combinations keep
  named fallbacks. Forged previously occupied Common-to-Vacant restores remain
  refused. Original binary-save graphic identity, runtime variation and
  whole-scene parity are separate limits. The
  [OE59 contract](docs/rendering/oe59-original-vacant-houses.md) distinguishes
  these source findings from the [OE58 user account](docs/qa/oe58-human-playtest.md),
  which has no accompanying screenshot file. New native human visual acceptance
  remains NOT_RUN.
- **Map admission does not guarantee a paid starter fits.** General selected-rule
  checks and legal empty24×13 starter placement are separate. The OE58 user
  reports Loulan passing the former, then refusing the starter with a clipped
  message. OE58 keeps that refusal and adds a fully readable typed dialog with
  explicit Empty City/other-map choices. Scripted real Loulan/Xia controls and
  opened three-size captures pass; placement permissions, selected map,
  retained session and settings remain authoritative. New human acceptance is
  still unrun.
  See the [reported scope and follow-up](docs/qa/oe58-human-playtest.md).
- **Rule19 demolition is deliberately limited.** The separate **V Safe demolition
  economy** opt-in uses schema35 and permits only never-admitted, empty Common2×2
  Houses and paused Clay Sources whose physical production, delivery and return
  have fully settled. Occupied/previously admitted Houses, Elite bodies,
  Warehouse/Market, Farm/Field and other production/service kinds remain refused.
  Retained history can also prevent removal; no goods or people are discarded.
  There is no refund, original demolition-parity claim or migration. Its journal
  has eight retirement slots and preserves the existing lifetime entity budgets.
  See the [OE51 contract and validation scope](docs/gameplay/oe51-demolition.md).
- **Rule10 has no purchasable fire protection.** Natural fires temporarily remove
  occupied Houses from the live goal while `PlaceFireWatch` is refused. The
  [paid progression check](docs/original-economy/paid-progression.md) reaches the goal
  and supplies six Houses in a finite window; its fixed plan still loses Funds over time.
  Separate Rule11 offers the authored paid FireWatch/Inspector contract; this
  does not establish original Inspector's Tower124 identity or profitable economy.
  Its bounded arrival/tax provenance has a finite capacity; see
  [fire protection](docs/original-economy/oe36-fire-protection.md).
  Separate [Rule12](docs/original-economy/oe37-sustainable-economy.md) adds three
  paid Food chains and authored monthly installed upkeep. Existing Rules10/11
  and saves retain their contracts; a cheaper invoice does not guarantee supply
  or profitability for arbitrary cities. Its measured twelve-House plan sustains
  current needs and positive operations for 100000 post-maturity ticks; higher
  stages and unlimited stability remain unproven.
- **Original sprites are incomplete.** Supported House stages use verified art;
  several other buildings, plants and walkers retain fallbacks. Empty Elite plots
  have no matching original body until their larger footprint materializes.
  Building animation, profession mappings and some pivots remain unverified.
- **Rule14 reaches paid Elite17, but stable supply and financing remain open.** The
  [source investigation](docs/original-economy/oe40-desirability-investigation.md)
  records actual Xia/Normal16→17 at tick7333 with raw70+water10, current/next
  needs met and 4455 Funds. Its following 20400 ticks lose Food, medical and
  fire retention and have −3241 operating cashflow; positive final Funds do not
  prove profitable operations. A separate two-additional-Market control still
  misses Tea at the House despite upstream stock. These are finite measured
  recipes, not a global impossibility or fairness proof. Historical Rule12/13's
  DES79 bound remains unchanged. The successful site uses stored`0x80005`
  neighbors outside the shared Diamond: this raw-snapshot adapter result does
  not prove a visible shoreline or original post-load/runtime Elite17.
  Complete original terrain mutation/lifecycle
  parity, newly admitted emitters, Market rectangles and general negative-wall
  shielding remain unsupported.
- **Inspector coordination does not certify Elite17 stability.** Explicit
  Rule15/schema31 coalesces actual incoming Inspector commitments and skips
  unexpired real protection for new dispatches, without changing fire numbers,
  needs, prices, upkeep or historical saves. The best checked paid early-refined
  attempt keeps type17 for20400 ticks and earns+3539, but misses2/400 current
  evaluations and has2070 target fire ticks. The
  [OE44 evidence](docs/original-economy/oe44-elite17-stability.md) separates
  technical compatibility, paid entry, supply, fire, finance and human acceptance.
- **Rule16 has a finite paid Elite17 retention proof.** Separately selected
  Rule16/schema32 uses authored deadline-aware Inspector dispatch with unchanged
  physical travel, protection, prices, limits and needs. The paid near-Watch/grid
  plan reaches type17 at tick5446 and holds it through25846:400/400 current needs,
  no target fire, devolution or vacancy, +4951 operating cashflow and minimum
  Funds5017. The other twelve Houses remain inhabited, but still have Food/service
  deficits and devolutions; city buildings still burn. This is a measured20400-tick
  result, not indefinite stability, all-city fire coverage or original-runtime
  parity. Historical Rules1–15 remain exact. See the
  [OE45 evidence](docs/original-economy/oe45-fire-coverage.md); native human
  acceptance remains open.
- **Rule17 improves the measured whole city but does not establish full stability.**
  Explicit Rule17/schema33 adds authored deadline-aware service selection while
  retaining Rule16 fire dispatch and the paid economy. The selected Ancestor/Music
  swap and south-door Watch plan retains Elite17 over5293→25693 with400/400 needs,
  zero target fire/devolution/vacancy and+5188 operating Funds. Across all13 occupied
  Houses, current failures fall391→95 and service failures328→2; Food misses18→15,
  devolutions25→8 and all-body fire starts164→91. However FoodQuality failures
  increase80→93,14 House fires and7900 burning House ticks remain, and some Houses
  have worse Food gaps. This meets the stated intermediate aggregate thresholds,
  not5200/5200 uninterrupted needs or universal fairness. Historical Rules1–16 and
  schemas1–32 remain separate. See the
  [OE46 assessment](docs/original-economy/oe46-citywide-reliability.md); native human
  acceptance remains unrun.
- **Rule18 meets the finite Food intermediate goal, with residual fires.** Explicit
  Rule18/schema34 changes only authored Market-to-House Food selection. The paid
  south-Market plan holds Elite17 over4834→25234 with400/400 needs and zero target
  fire/devolution/vacancy. All13 Houses remain occupied and complete50 actual Food
  consumptions each: FoodQuality failures93→7, Food misses15→0, service failures
  2→0 and House devolutions8→0. Operating cashflow is+5292, minimum Funds5348;
  all-body/House fire starts fall91/14→67/12. House19 worsens0→7 FoodQuality
  failures after a real fire/refused load; some individual House fire measures
  also worsen. The city therefore has5193/5200 current needs met, with6850 burning
  House ticks. This is an intermediate improvement, not full stability or
  universal starvation freedom. Rules1–17 and schemas1–33 remain exact. See the
  [OE47 assessment](docs/original-economy/oe47-food-reliability.md); native human
  acceptance remains unrun.
- **Legal Rule18 planning meets the finite fire intermediate goal.** The
  [OE48 assessment](docs/original-economy/oe48-citywide-fire-reliability.md)
  retains the unchanged Rule18/schema34 engine and two Inspectors. Earlier paid
  Watch/Food construction plus a third paid Market reaches Elite17 at5395 and
  holds it through25795:400/400 target needs, zero target fire/devolution/vacancy,
  all13 Houses continuously occupied and5200/5200 current needs met, with zero
  Food misses, service failures or House devolutions. House fires fall12→5 and
  burning House ticks6850→2915; operating cashflow is+5180, minimum Funds5391.
  Five residential interruptions and65 all-body ignitions remain, and two House
  sites have worse individual fire measures. This is not full city stability,
  indefinite fairness or a flawless bootstrap:27 Food misses precede the measured
  window. Plans have different real entry ticks, so their window differences do
  not isolate the additional Market's effect. At that OE48 assessment
  Rule19/schema35 was not allocated; the separately selected
  [OE51 contract](docs/gameplay/oe51-demolition.md) now allocates it. That assessment
  did not establish any additional economy generation;
  all historical generations and application defaults remain exact. Native human
  acceptance and complete original-runtime parity remain open.
- **High-tier progression has a bounded Rule12 blocker.** The
  [paid high-tier check](docs/original-economy/oe38-paid-high-tier-progression.md)
  reaches Common10 through actual purchases and deliveries, but does not hold it
  continuously for the following 20400 ticks: Hemp distribution and intermittent
  Ceramics delivery cause devolutions despite positive operating cashflow.
  An exact source/footprint bound gives DES≤79 for a materialized4×4 Elite House;
  the unchanged16→17 entry needs80. This blocks Elite17 in the current Normal
  Rule12 adapter, not in original Emperor. No historical rule or model is changed.
  Separate [Rule13 market logistics](docs/original-economy/oe39-market-retention.md)
  holds paid Common10 for20400 consecutive ticks in the tested near-Pottery plan,
  with no current unmet needs or devolutions across twelve Houses and positive
  operating cashflow. It uses earned Hemp buffers (94→67); indefinite high-tier
  self-sufficiency and universal fairness remain unproven. Other city buildings
  still experience fires. This does not change historical Rule12/13's Elite17/DES
  contract; Rule14's separately selected water-query result is described above.
  The unchanged four-house Xia starter without expansion loses money and misses
  Food demands in the measured long run; it needs active development.
- **Service capacity is limited.** Larger cities can miss Service deadlines
  despite continuous Post activity. Current coverage does not guarantee coverage
  at the next demand; universal fairness and full-city balance remain unverified.
- **Original saves and campaigns are unsupported.** Original residents and NPC
  simulation are not imported. Two of the 167 audited maps still fail conservative
  occupancy validation; other unsupported source states are refused.
- **Road and transport coverage is incomplete.** Original bridges, ramps,
  wall-top roads, mixed water-road terrain and unknown road styles are not fully
  supported. Owned Bridges and Ramps require explicit rule profiles and have
  bounded geometry; safe removal, shipping and Ferry transport are unavailable.
- **Original terrain heights remain unresolved.** The playable source-cell ramp
  prototype uses original sprites, but their surface and anchors do not yet fit
  production RampLinks. Production ramp banks and cliff cuts are authored;
  full original cliff, height and natural-surface fidelity remain open.
- **Some restored structures use previews or fallbacks.** Great Wall context,
  wall/Tower/gate states, quarry depth and shore-landing lifecycle are incomplete.
  Earthen-wall and quarry previews do not reproduce the original restored state.
- **Rendering is approximate.** First-draw anchors, foreground ordering and
  interpenetrating sprites can differ from the original. SG3 mirror references
  are unsupported; sprite shadows are not bit-identical to original RGB555 output.
  Automatic visual profiles recognise only the pinned GOG-derived asset revision.
- **Manual acceptance remains partial.** The [OE58 account](docs/qa/oe58-human-playtest.md)
  supplies concrete **USER_REPORTED_PASS** for paid Xia startup, Road/fifth-House
  construction, inspection and tent appearance after resident entry. No new
  user screenshot or direct native observation accompanies this task. The
  [OE50 assessment](docs/qa/oe50-native-playtest.md) retains its historical
  **HUMAN_NOT_RUN** results; general favorable feedback does not establish all
  pointer/focus/resize subcases, external displays/DPI, complete native Recovery,
  multi-hour play, every historical generation or clean-Mac acceptance. Human
  acceptance of the new changes remains NOT_RUN until specifically reported.
- **macOS distribution is local development only.** The app is ad-hoc signed,
  without Developer ID signing or notarization. Independent clean-Mac and
  Finder/Gatekeeper acceptance remain unverified. The current OE50 dependencies
  require explicitly selecting `--min-os 27.0`; the packaging default remains
  `26.0`, and verification still refuses dependencies newer than the selected
  minimum.

The [original-economy player guide](docs/original-economy/player-handoff.md)
explains the current starter. [Documentation](docs/README.md) links to the
supported rules and technical evidence.
