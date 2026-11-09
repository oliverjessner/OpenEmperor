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
