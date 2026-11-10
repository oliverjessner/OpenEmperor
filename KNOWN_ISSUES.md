# Known issues

OpenEmperor is an alpha. Supported sandbox behavior does not establish
complete original-game compatibility.

- **Further production chains remain research-only.** The existing Wheat/Millet,
  Hemp, Clay/Ceramics, Tea, Silk, Bronzeware, Lacquerware and Paper paths include
  authored harvest periods, recipes or resource management. Additional model
  labels and their raw Cost/Employees/Resource columns do not establish complete
  crop, mining, fishing or seasonal behavior. OE55 adds no new crop, World rule,
  save schema or playable production chain. See the
  [production evidence boundary](docs/original-economy/oe55-production.md).
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
  Missing ownership or incompatible sources retain named authored fallbacks.
  Unassigned service/culture/religion/luxury walker families and pending
  Elite plots retain their named historical fallbacks. Existing Market,
  Warehouse and FireWatch core previews are not newly identified original
  registrations. See the [OE52 evidence and validation](docs/rendering/oe52-original-visuals.md)
  and [OE60 farm/field boundary](docs/rendering/oe60-original-farm-visuals.md).
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
