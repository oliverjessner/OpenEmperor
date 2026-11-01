# Known issues

OpenEmperor is an alpha. Supported sandbox behavior does not establish
complete original-game compatibility.

- **Original economy is experimental.** Rules10/11/12/13/14/15/16 are opt-in and include authored
  immigration, workforce, recipes, harvesting, school-to-house visits and Rule13
  needs-aware goods selection.
  Original venue/performer chains and illness are missing. Houses and productive
  buildings cannot yet be demolished; incompatible development saves are refused.
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
- **Manual acceptance remains open.** Native pointer/focus/resize behavior,
  external displays, original-map visuals and sustained human play still need
  acceptance. Automated checks do not establish these results.
- **macOS distribution is local development only.** The app is ad-hoc signed,
  without Developer ID signing or notarization. Independent clean-Mac and
  Finder/Gatekeeper acceptance remain unverified.

The [original-economy player guide](docs/original-economy/player-handoff.md)
explains the current starter. [Documentation](docs/README.md) links to the
supported rules and technical evidence.
