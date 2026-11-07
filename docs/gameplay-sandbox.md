# Gameplay sandbox

OpenEmperor's sandbox is an independent, authored simulation over read-only
original map/asset files. It does not load Emperor savegames or claim original
economy, construction, service or mission rules. Saves keep the explicit
OpenEmperor profile, rule version and schema; selecting a newer profile never
rewrites an older save.

## Current city play

**Play Original economy** opens a new-city setup for the recommended existing
Rule18/schema34/policy3 experiment. Selection is explicit and session-only:
review the data root, map, difficulty and **Paid four-house starter** or
**Empty city**, then choose **Start sandbox**. The recommendation changes menu
presentation, not World rules, save formats or stored preferences. The
[player guide](original-economy/player-handoff.md) describes this path and the
[OE49 assessment](qa/oe49-player-mode.md) records its acceptance boundaries.
Original campaigns and Emperor savegames remain unsupported; the finite OE48
city still has five House fires and does not establish complete original parity.

**Advanced / Legacy** retains **N New Sandbox** and all historical profile
choices. Select **City-v16** there for the combined authored city systems. Its
ordinary default is rule 4; fresh general settings still select City-v11 rule 3.
Existing preferences and saves retain their choices. See [City systems](city-systems.md)
for Markets, staffing/operations, demolition, Fire, quality, Water and Health;
[City-v16](city-v16.md) covers maintenance and its versioned map/geometry rules.
The [paid city playthrough](city-v16-playthrough.md),
[startup guidance](economy-start-guidance.md) and
[long-play diagnosis](city-v16-longplay-stability.md) explain real progression
without granting inventory, workforce or hidden ticks.

Optional [Bridges V1](city-v16-bridges.md) requires explicit City-v16 rule 5
(policy 3/schema 21); B drags between equal-height normal bank roads across
1..8 straight water cells. Price is 40 + 20 per water cell, upkeep zero. F3 keeps
B's diagnostic-light meaning. Removal is unavailable in V1. The paid starter is
`sh tools/test_city_v16_bridges.sh DATA --ui`; `--ui-build` starts before construction.

Optional [Ramps V1](city-v16-ramps.md) requires explicit rule 6
(policy 3/schema 22). P links adjacent normal roads differing by exactly one
signed level for 40 funds. It adds no workers/goods/upkeep and has no removal in
V1. The [cliff/ramp visual starter](rendering/cliff-ramp-integration.md) uses
explicitly authored terrain, not verified original-map slopes. Bridge/Ramp
selection, preview and owned links do not migrate older saves or defaults.

In **Advanced / Legacy**, **O Original economy** selects the separate optional
Rule10/schema26 experiment; it retains its existing shortcut from the main menu.
The [player guide](original-economy/player-handoff.md) covers its paid starter,
real deliveries, house development and recovery; its current limits differ from
the authored City systems above. Historical rules7–9 and their schemas remain
frozen; see the [versioned contracts](original-economy/architecture-map.md).

**F Original fire protection**, **S Sustainable economy** and **M Needs-aware
logistics** explicitly select Rule11/schema27, Rule12/schema28 and Rule13/schema29.
Rule13 inherits Rule12's three paid Food chains, 816-tick installed-worker upkeep
and authored fire protection, changing only Warehouse/Market goods selection.
Its direct CLI option is `--original-market-logistics` with `--sandbox`, `--data`
and `--sandbox-rules sandbox-city-v16`. Saved generations and general defaults
remain unchanged; [market diagnosis](original-economy/oe39-market-diagnosis.md)
and [retention assessment](original-economy/oe39-market-retention.md) separate
measured logistics from long-term progression and fire safety.

**W Cardinal-water economy** and **I Inspector coordination** separately select
Rule14/schema30 and the authored Rule15/schema31 experiment. **D Deadline-aware
fire dispatch** selects the separate authored **Rule16/schema32/Policy3**
experiment, also available as `--original-deadline-fire-dispatch` with the explicit
City-v16 sandbox CLI arguments. It inherits the source-bound terrain,
needs-aware market selection and revision27 economy. Its Inspector dispatcher
compares real physical deadlines and travel times across eligible building
classes; only actual arrivals protect buildings. The exact schema32 marker is
`authored-deadline-fire-dispatch-v1`; inherited source/market/cardinal identities
remain required. There are no added resources, passive protection, model, cost,
limit or timing changes. Rules1–15, saved generations and general defaults remain
exact. The [OE45 assessment](original-economy/oe45-fire-coverage.md) records a paid
near-Watch/grid plan with20400 consecutive Elite17 ticks,400/400 current needs,
no target fire/devolution/vacancy and+4951 operating cashflow. Other city
buildings still burn and miss needs; indefinite stability remains unproven.

**T Deadline-aware services** separately selects authored **Rule17/schema33/Policy3**,
also available as `--original-deadline-service-dispatch` with the explicit City-v16
sandbox arguments. It inherits Rule16's paid fire dispatch and economy, and changes
only real service target selection using actual expiry, travel and incoming visits.
Only arrival renews the unchanged service timer. Its exact marker is
`authored-deadline-service-dispatch-v1`; inherited fire/market/cardinal/source
identities stay mandatory. Rules1–16 and their saves do not migrate. The
[OE46 assessment](original-economy/oe46-citywide-reliability.md) records the
separate citywide diagnosis, paid controls and gameplay acceptance.

**U Deadline-aware food** separately selects authored **Rule18/schema34/Policy3**,
the same existing contract selected by **Play Original economy**,
also available as `--original-deadline-food-dispatch`. It inherits Rule17's paid
services, fire and economy. Only Market-to-House Food target selection changes;
physical arrival still owns delivery and quality. Farm-to-Market selection,
consumption, capacities, movement, prices, installed upkeep and limits stay exact.
Schema34 requires `authored-deadline-food-dispatch-v1` and all inherited markers.
Historical Rules1–17 do not migrate. See the
[OE47 assessment](original-economy/oe47-food-reliability.md) for measured outcomes
and the remaining limits.

**V Safe demolition economy** in Advanced / Legacy separately selects authored
**Rule19/schema35/Policy3**, also available as `--original-safe-demolition`.
It inherits Rule18 and adds only conservative empty-House and settled-Clay
retirement with no refund, fresh paid rebuild IDs and a bounded saved journal.
The exact `safe_demolition` marker is `authored-safe-demolition-v1`.
Occupied/previously admitted Houses, Elite bodies and other production/service
classes remain refused. Recommended play stays Rule18, and old saves never
migrate. See the [OE51 contract](gameplay/oe51-demolition.md).

The separate **Source-cell ramps (rule20)** City-v16 setup uses schema36,
unchanged policy3 and marker `authored-source-cell-ramps-v1`. It inherits classic
Rule6 economy and five-tick edges, not Original Rules7–19. A paid40 source-cell
surface owns three axial roads and shares the existing World navigator; the
combined ramp limit stays64. Native raw ramps remain refused. See the
[OE53 authored contract](rendering/oe53-terrain-integration.md).

The separate **H Land trade (rule21)** Advanced entry and
`--authored-land-trade` option select schema37/policy3 with marker
`authored-land-trade-v1`. It inherits classic Rule6, not Original Rules7–19 or
Rule20 source-cell ramps. Two ordinary paid Warehouses, one 40-Funds boundary
license and a real caravan support finite Pottery imports/exports. Select the
city Warehouse: G binds the other boundary Warehouse, I buys 4 for 24, E sells 4
for 12 only after physical delivery. See the [OE54 player/accounting contract](original-economy/oe54-trade.md).

## Legacy profiles and save compatibility

These profiles remain selectable/readable for compatibility; their milestone
diaries are not separate current instructions. Rules are authored and frozen per
profile/version. All values are integer funds/ticks. Base paid construction from
City-v6 is Road 2, Clay Source 120, Pottery 180, Warehouse 150 and House 80. Road
removal gives no refund; rejected/unchanged commands spend nothing.

| Profile | Save schema | Start funds | Added option/cost | Derived goal |
| --- | --- | ---: | --- | --- |
| City-v6 | 6 | 1000 | Economy/workforce; base costs above | Four Houses, each with three fulfilled Pottery demands |
| City-v7 | 7 | 1000 | Farm 160; Food + Pottery demand | Four historical Level-2 Houses |
| City-v8 | 8 | 1000 | Service Post 100; actual visit coverage required | Four historical Level-2 Houses |
| City-v9 | 9 | 1000 | Resident workforce and 6/10/16 capacity | Four historical Level-2 Houses and population 48 |
| City-v10 | 10 | 1000 | Stable variable collections, scalable limits, core 2×2 footprints | Ten Houses, eight historical Level 2, population 100 |
| City-v11 rules 1/2 | 11 | 1300 | Market 140; rule 2 changes rates/move-in grace | City-v10 goal |
| City-v11 rule 3 | 12 | 1300 | Operation controls/priorities, no construction-price change | City-v10 goal |
| City-v11 rule 4 | 13 | 1300 | Safe demolition without refund | City-v10 goal |
| City-v12 rule 1 | 14 | 1300 | Fire Watch 80 | City-v10 goal |
| City-v13 rule 1 | 15 | 1300 | Desirability/effective-level caps | Ten Houses, eight current effective Level 2, population 100 |
| City-v14 rule 1 | 16 | 1300 | Well 60, proximity Water | City-v13 goal |
| City-v15 rule 1 | 17 | 1300 | Health Post 120 | City-v13 goal |
| City-v16 rules 1/2 | 18 | 1300 | Ownership maintenance; rule 2 uses 2×2 Well/Post | City-v13 goal |
| City-v16 rule 3 | 19 | 1300 | Original map permission policy 1 | City-v13 goal |
| City-v16 rule 4 | 20 | 1300 | Original road/map permission policy 2 | City-v13 goal |
| City-v16 rule 5 | 21 | 1300 | Authored Bridges, policy 3 | City-v13 goal |
| City-v16 rule 6 | 22 | 1300 | Authored Ramps, policy 3 | City-v13 goal |
| City-v16 rule 7 | 23 | 1300 | Frozen original House/model development, policy 3 | No active paid original economy |
| City-v16 rule 8 | 24 | 2000 | Frozen paid Well72 desirability sources, policy 3 | No active paid original economy |
| City-v16 rule 9 | 25 | 2000 | Frozen neutral six-goods ledger, policy 3 | No active paid original economy |
| City-v16 rule 10 | 26 | 12000 | Paid original-model production, House evolution and finance; revision27/policy3 | Six occupied non-burning Houses, Common type5+, population50, collected tax and nonnegative Funds |
| City-v16 rule 11 | 27 | 12000 | Rule10 economy plus authored paid FireWatch/Inspector | Rule10 goal |
| City-v16 rule 12 | 28 | 12000 | Three Food chains; installed-worker upkeep every 816 ticks; Rule11 fire protection | Rule10 goal |
| City-v16 rule 13 | 29 | 12000 | Authored needs-aware Warehouse/Market goods selection; inherits Rule12's unchanged economy and bounds | Rule10 goal |
| City-v16 rule 14 | 30 | 12000 | Source-bound cardinal-water House DES; inherits Rule13 and all economy/bounds | Rule10 goal |
| City-v16 rule 15 | 31 | 12000 | Authored Inspector coordination; exact marker, inherited Rule14 terrain/economy and unchanged bounds | Rule10 goal |
| City-v16 rule 16 | 32 | 12000 | Authored deadline-aware Inspector dispatch; exact marker, inherited source/terrain/market/economy and unchanged costs/limits/times | Rule10 goal |
| City-v16 rule 17 | 33 | 12000 | Authored deadline-aware service selection; exact marker, inherited Rule16 fire/economy and unchanged workers, models, costs, bounds and timers | Rule10 goal |
| City-v16 rule 18 | 34 | 12000 | Authored deadline-aware Market-to-House Food selection; exact marker, inherited Rule17 services/fire/economy and unchanged production, transport, consumption and limits | Rule10 goal |
| City-v16 rule 19 | 35 | 12000 | Authored bounded retirement of never-occupied Common2x2 Houses and physically drained paused Clay Sources; Rule18 economy, no refund, fresh paid rebuild identity | Rule10 goal |
| City-v16 rule 20 | 36 | 1300 | Authored source-cell ramps40; classic Rule6 economy/Bridge/Ramp behavior, unchanged policy3 | City-v13 goal |
| City-v16 rule 21 | 37 | 1300 | Authored finite Pottery land trade, two paid Warehouses/license40, classic Rule6 economy, unchanged policy3/model identity | City-v13 goal |

Schemas 1–9 keep their historical fixed collection/one-cell semantics. City-v7
has ten building/six courier entries; City-v8/v9 have eleven/seven. City-v6–8
supply eight workers per House; City-v9 and later use actual residents. City-v6
requires only Pottery, City-v7 adds Food, City-v8 adds Service. From City-v10,
entities are sorted by stable monotone IDs, core foundations are 2×2 and each
courier has its own cyclic target cursor. No historical profile acquires a later
feature on ordinary load.

Original-economy schemas23–34 require their exact explicit rule generation.
Schema29 additionally requires the immutable
`rules.market_logistics: "authored-needs-aware-market-v1"` identity; it stores no
new demand authority and does not convert schema28 cities. Manual F5/F9 and
Recovery preserve the selected generation.

The two explicitly confirmed City-v11 copy upgrades are rule 2→3 (schema 11→12,
initializing Running/Normal controls) and rule 3→4 (schema 12→13, adding demolition
history). Both preserve the source and exact saved tick. Other City upgrades,
including City-v16 geometry/Bridge/Ramp versions, have no implicit migration.
[City systems](city-systems.md#rule-versions-and-demand) records rates and demand;
[versioned Water geometry](city-systems.md#water) retains the older 1×1 Well/Post.
The production schema-1 compatibility adapter described below is a separate
historical exception.

City-v6's schema-6 treasury identity is `1000 + taxes_collected_total -
construction_spent_total`; upkeep is absent before City-v16. Its fixed layout has
nine building/five courier entries, extended to ten/six in City-v7 and eleven/seven
in City-v8/v9. The legacy goal conditions and fields remain independently checked.

## Start from the menu

`./build/openemperor` opens the normal main menu. Choose a folder containing
installed or extracted original Emperor files. **Play Original economy** creates
a new city using the recommended experimental Rule18; it never resumes or
upgrades an existing save. **Advanced / Legacy → N New Sandbox** opens the
historical profile selector. Its fresh settings still preselect
`sandbox-city-v11` rule3 and the paid prepared starter; an existing stored profile
and starter choice are preserved. Ordinary City-v16 remains rule4, with Bridge
rule5 and Ramp rule6 as explicit options. O/F/S/M/W/I/D/T/U and N remain main-menu
keyboard shortcuts, including while the advanced list is closed.

The GOG offline installer file alone is not a usable data folder.
`--data <directory>` overrides the remembered folder for this launch without
changing direct CLI mode semantics. The old `--sandbox` command below still
starts directly with its historical v1 default. Missing/unreadable data require
a valid data folder; unsupported or unchecked maps cannot start the selected
rules. Start also validates model, graphics and paid placement. A failed start
keeps the retained city intact.

The menu creates a unique save target for each new session but writes no save until the Save button or F5 is used. Escape first cancels an active road or UI gesture; otherwise Escape or **Menu** retains the session without advancing simulation. **Resume session** continues the same World and camera, with its previous pause/speed state. **Load save** shows bounded, validated OpenEmperor save entries and can open an external OpenEmperor save file. It checks the original map hash and decoded buildability mask before replacing any existing World. Loading starts paused at the saved tick. A changed World prompts Save / Without saving / Cancel before replacement or exit; failed writes keep the session. Window close uses the same prompt. Direct `--sandbox` retains its existing Escape-to-exit behavior.

SDL's user preference directory holds `settings.json` and `saves/`; on macOS the usual location is `~/Library/Application Support/OpenEmperor/OpenEmperor/`. `SDL_GetPrefPath("OpenEmperor", "OpenEmperor")` determines the actual path. Neither saves nor settings belong in the original data directory. These files contain no original game bytes. Settings version 1 stores the chosen data root, map, rules profile and most recent successful save path; it does not change the sandbox save schema.

The sandbox is an independent prototype, not a reconstruction of Emperor's economy or building rules. Start an empty world with:

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map
```

Add `--sandbox-demo` for a deterministic six-cell arrangement: workshop, four roads, warehouse. The demo searches the actual buildable mask in row-major order and uses the same typed placement commands as mouse input. If it cannot find six adjacent suitable cells, it fails without altering the original map. For an offscreen finite check of the same view and simulation, use `--sandbox-check --report-json` in place of the interactive mode; it automatically places the demo and runs 700 ticks with SDL's dummy/software renderer.

The original `.map` and SG3/`.555` files are read only. The background uses the existing `exe-6373328b-v213-slot8-runtime-table` stored-graphics profile and `edge-byte-4x4` preview policy. Those are selected only for sandbox mode; the diagnostic commands retain their existing defaults. The original saved road imagery does not create roads in the sandbox graph. All placed roads, buildings, goods, and courier state live in a separate `simulation::World`; they persist only when explicitly saved to an OpenEmperor sandbox file.

`sandbox_buildable_v1` permits a storage cell only when it belongs to the existing candidate mask, has no off-map bit, has exact raw terrain word `0x80` and raw object word `0`, and is covered by a successfully decoded supported 1×1 stored footprint. Unknown, failed, and multi-cell graphics remain blocked. This deliberately conservative local rule is not Emperor's construction test. Workshop and warehouse occupy one sandbox cell each; that size does not claim their original footprint.

The core is SDL-free and has one good, **Goods**. It advances only by explicit integer ticks at 20 ticks/second. A workshop creates one unit after 100 active production ticks, holds at most 8, and pauses progress while full without accumulating a backlog. The warehouse holds 32. One courier carries at most 4 and spends 10 ticks per orthogonal edge. There is no raw-material input, consumption, or deletion. Every tick checks `total_produced == workshop_stock + courier_cargo + warehouse_stock`.

Tick order is production, dispatch if the courier is at the workshop and warehouse space and route exist, then one movement tick; unloading happens only upon reaching the warehouse. The return trip follows the reverse path edge by edge. The route uses bounded BFS over placed roads, with the workshop and warehouse as endpoints; fixed neighbor order is up, left, right, down. The route is cached by placement revision, so rendering does not run pathfinding. Closing a road gap invalidates that cache. Buildings may be placed without a route; the building inspector reports relevant status. The v1 profile does not permit road removal, so its in-flight route cannot be broken by a command.

The app feeds frame time to a bounded fixed-step accumulator, at most eight ticks per update. 1×, 2×, and 4× affect only that feed. Pausing stops ticks but leaves camera and placement active; `.` executes one tick while paused. A long frame can leave display time behind wall time, but the tick counter never jumps over events.

The clickable bottom bar offers the available tools, Pause/Continue, Step, 1×/2×/4×, Reset, Save and Load. Keys `1`–`4` still select Road, Workshop, Warehouse and Select; Space pauses, `.` steps, `+`/`-` changes speed, WASD/arrows pan, the wheel zooms over the map, `R` refits, and Escape exits when no gesture is active. A building or road-removal command happens on a complete map press/release. Road building shows an X-then-Y orthogonal drag preview and commits at most 256 cells atomically on release. Existing roads are reused without new commands; an invalid cell rejects the entire gesture. Right-click, Escape, focus loss, resize, tool switch, Save or Load cancel a pending gesture. Buttons and the information panel block clicks through to the map; a press begun on either surface never turns into a map command. Choose Select with its button or `4`. SDL primitive geometry draws roads, buildings and couriers over the unchanged original background. The overlay is a prototype visualization; it does not reproduce original terrain occlusion, sprites, building sizes or transport behavior.

Later work may research the original game's economy, traversability, graphic choices, draw ordering, and walker behavior separately. No such assumptions are encoded in this sandbox profile.

## Resource-dependent production (`sandbox-production-v2`)

Select the second, explicitly authored profile with `--sandbox-rules sandbox-production-v2`. Without this option, `sandbox-logistics-v1` and its controls remain unchanged. Both profiles use the same read-only original-map background, `sandbox_buildable_v1` mask, simulation `World`, fixed-step `TickDriver`, stored-graphics renderer, camera, and picking. The v2 demo requires seven consecutive suitable cells and places Clay source, two roads, Pottery, two roads, and Warehouse through ordinary commands. It never changes the mask to make a demo fit.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --report-json
```

Keys `1`–`6` select Road, Clay source, Pottery, Warehouse, Select and Remove road. The same actions are clickable in the bottom bar; the shared mouse and cancellation rules above apply. SDL primitive geometry distinguishes the three one-cell buildings and couriers A (Clay) and B (Pottery), including their loads. A waiting courier is orange while its cargo indicator remains visible. The right information panel reads the selected instance's stock, progress, recipe, reservation and courier state; it has no timer or inventory of its own. `F1` opens a small debug readout and Tab collapses the panel.

The v2 rules are deliberately unrelated to established Emperor gameplay. One Clay source creates 1 Clay per 100 active ticks and stores at most 8. One Pottery building accepts at most 8 Clay and holds at most 8 Pottery. A recipe transfers 2 actually delivered Clay into its active work state when it starts, waits 150 further ticks, and creates 1 Pottery. The start tick counts as **zero** processing ticks. Only one recipe can run; it starts only if its future output slot is available. A Warehouse accepts at most 32 Pottery, never Clay. Both couriers carry at most 4 units of one good and traverse each orthogonal edge in 10 ticks. Full outputs pause production without a backlog.

Tick order is stable building IDs (Clay source, Pottery), stable courier IDs (A, B) for dispatch, then both courier movements and arrivals, then invariant checks. Consequently, Clay arriving at the end of a tick can first start a recipe on the next tick. Routes are bounded shortest-path BFS searches through **placed** roads only, with an owning building as start and its designated receiving building as end. A third building is never transit. Each courier caches its own start/goal/revision result. A placed road revises both caches; inspector and render queries never search paths or advance goods. A courier atomically moves source output into cargo and reserves the same amount of destination capacity at dispatch. Only at actual arrival is the reservation released and the good inserted; return is edge-by-edge without cargo. The two couriers may share a road without collision handling.

After every tick, the core checks both balances and all buffer, cargo, and reservation bounds:

```text
clay_extracted_total = clay_in_source + clay_in_transport + clay_in_pottery_input
                     + clay_in_active_recipe + 2 * pottery_completed_total
pottery_completed_total = pottery_in_output + pottery_in_transport + pottery_in_warehouse
```

Reserved space does not count as produced goods. The original v1 Goods balance continues to be checked under its own profile. Headless v2 runs 3,000 ticks through the same `SandboxView` on a software renderer and requires processing, arrival in the warehouse, invariant validity, and frames that draw both couriers. No resource exhaustibility, original building dimensions, walker art, or original production schedule is inferred from map data. Those remain separate future research tasks.

## OpenEmperor sandbox saves

All sandbox rule profiles can save and resume their own simulation state. This is **not** an original Emperor savegame format and cannot load one. The original map and graphics remain necessary and read only.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-save .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --load-sandbox .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --sandbox-resume-check --report-json
```

`F5` or Save writes at the configured path; `F9` or Load reloads it. The second command also uses the loaded path for later F5/F9. Without a configured path the status line reports the missing path and the buttons are disabled. A successful load pauses the sandbox, resets the frame accumulator, and leaves it at exactly the saved tick; Space resumes and `.` advances one tick while paused. Camera, tool, selection, panel scroll and unfinished road preview are local UI state, not simulation state. Load discards an open gesture and rechecks the selection against the restored World. A failed load leaves the current World unchanged and reports its cause. The old finite `--sandbox-check` remains read only; only the explicit `--sandbox-resume-check` writes a temporary file and deletes it afterward.

Normal menu sessions additionally have an app-level recovery history, enabled by default. Its start point is captured after the fully initialized candidate session is accepted and before it can tick. Every 1,200 subsequent simulated ticks the app can write the current safe state; it does not write during pause, presentation-only interaction, an open road drag, a budget decision, load, migration, or session replacement. One history retains the start plus its five newest periodic points. Restoring a point uses the same parser and restore validation described below, starts paused, allocates a new F5 target, and creates a child history. The original branch and its later checkpoints remain present. Autosave captures do not change the manual saved tick/command markers or clear the unsaved-progress prompt.

Recovery administration lives outside the save schema under the app preference directory. Metadata summaries are checked against each save rather than used as World authority. The aggregate store is capped at 256 MiB, and a failure delays the next retry by another interval instead of retrying every frame. A new checkpoint is written and read back before metadata publication; rotation happens afterward. An interrupted publication may leave an unreferenced managed file, but it cannot make a prior referenced checkpoint disappear. The load screen separates manual saves from recovery points and requires confirmation before deleting one checked history. This design does not promise survival of every storage or power failure.

For the v1–v4 profiles, the UTF-8 JSON document has `format: "openemperor-sandbox-save"` and `schema_version: 4`, separate `rules.id` and `rules.version`, a map-relative path, map part 0, SHA-256 of the required original map, fixed graphics/footprint/buildability profile labels, and SHA-256 of the post-decode buildability mask. The `world` section stores dimensions, tick and command counters, placement revision, total road placements/removals, roads, seven canonical ID-ordered building records and three courier records, stocks, production and demand counters, reservations, active paths, vertices, edge progress, waiting state, last checked road revision, and reroute-attempt counts. Household fields include `placed_tick`, `demand_progress`, `fulfilled_demand`, `missed_demand`, `consumed_total`, and `last_demand_status` (0 none, 1 fulfilled, 2 missed). `next_household_id` and `last_dispatched_household` preserve ID allocation and cyclic selection independently of the supplier's active target. Later profile-specific schemas 5–9 extend only their named profiles: Industry, city economy, Food, Service, and City-v9 population respectively. Schema 10 belongs only to City-v10 and stores sorted variable-length entities, monotonic next IDs and each courier's cyclic target cursor. Schema 11 belongs only to City-v11 v1/v2 and admits Market records plus the four appended Market courier roles while reusing the existing stock and reservation fields. Schema 12 belongs only to City-v11 v3 and requires `operating_enabled` and `workforce_priority` on every building record, with default-only values on Houses. A confirmed load-menu action creates a separate v3/schema-12 copy of a v2 save without advancing it; the source file remains unchanged. Raster occupancy, owner indices, future dispatch route caches, population capacities, staffing and goal state are rebuilt or derived; in-flight paths are retained. The file contains no map raster, SG3 pixels, absolute installation path, or original game data.

Loading rejects changed original map or buildable mask, unknown versions, unsupported profiles, malformed paths, inconsistent placements, routes, capacities, reservations, or conservation balances. A save is limited to 8 MiB and 32 JSON nesting levels. The save target must be outside `--data`; existing symlink components are rejected. Writing uses a unique temporary file in the target directory, checked write/flush/close, then replacement rename. A failed pre-rename write leaves the previous save in place. Rename is an atomic directory operation on supported local filesystems, but this is not a promise of power-loss durability.

Existing valid schemas 1–9 remain readable under their original profiles; no save is automatically promoted to another rules profile. Logistics rules stay at `rules.version: 1`. A known old production schema-1 save with `rules.version: 1` is validated, given zero road-removal and reroute counters, and upgraded **in memory** to production `rules.version: 2`; schema-2 production retains version 2. Neither becomes the household profile. Loading does not advance a tick or rewrite the original save. A subsequent explicit F5 writes the schema belonging to that World's profile. Unknown schema and rule versions still fail.

## Production sandbox road removal and rerouting

The `RemoveRoad` command (`6`, then left-click) applies only to roads placed in our v2 sandbox. It rejects empty cells, buildings, out-of-grid cells, and roads under either courier. At a waypoint with zero edge progress only the occupied road is protected; once an edge begins, both its endpoints are protected until that edge completes. Roads farther along the planned route and roads already left behind can be removed. Validation and hover preview use the same rules, including while paused. A successful placement or removal increments the topology revision once; rejected commands and duplicate-road no-ops do not.

If removal breaks a future segment, the courier finishes any protected current edge, then searches from the road or building it actually reached. The shared bounded BFS uses orthogonal placed roads, one fixed destination building, and up/left/right/down tie-breaking. A courier already on a road may use a two-point remaining route to its adjacent goal; a new dispatch still requires a road between its buildings. A valid active route is not replaced merely because another road was built. With no route, the courier waits at its real waypoint, retaining its phase, load, and target reservation. A failed search is not repeated every frame or tick at the same road revision; a topology change enables another attempt. A returner waits empty and cannot teleport to its source. Arrival releases and deposits a shipment exactly once. These are authored OpenEmperor sandbox rules, **not** verified Emperor demolition or walker behavior.

The synthetic routing test exercises a true detour, a later disconnection, JSON save/load while waiting, repair, and at least 1,000 identical continuation ticks. For an explicit offscreen check against a locally available original map, use:

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-routing-check --report-json
```

This check uses only the demo row if the real buildability mask permits it: it rejects removal under an active edge, removes a later road, verifies waiting and reservations, saves temporarily, reloads into a fresh World, repairs the road, and verifies delivery, return, balances, frame rendering, and per-tick equality. The ordinary `--sandbox-check` behavior is unchanged. The full detour test uses only synthetic cells, because a suitable alternate branch is not assumed to exist on an original map.

## Household supply (`sandbox-household-v3`)

This optional authored profile keeps the v2 Clay/Pottery production rates and protected road-removal behavior, then adds one Warehouse-to-Household supplier and one 1×1 household placeholder. It does not model population, housing development, or original Emperor needs. The demo looks for ten consecutive cells that pass the unchanged conservative buildability mask; if none exist it fails without altering the original map.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3 --sandbox-check --sandbox-resume-check --report-json
```

Keys `1`–`6` keep the v2 tools; `7` places the only Household. The green household diamond and third supplier marker are distinct from existing buildings and couriers. The inspector reports house stock (capacity 8), incoming reservation, demand-clock progress, last demand result, fulfilled and missed dates, total Pottery consumed and supplier state. Pause and single-step use the same fixed tick driver; F5/F9 save and restore the complete household clock and any in-flight or waiting delivery.

The supplier carries up to 4 Pottery from the **existing warehouse stock** to the household over placed roads at 10 ticks per edge. Dispatch removes only available warehouse stock and reserves the same amount of household capacity. Incoming warehouse reservations are not stock available to the supplier. Only arrival releases the household reservation and adds the cargo to its stock. The warehouse may receive and dispatch Pottery in the same period without duplicating it. A cut future road leaves the supplier waiting with its cargo and reservation; the household keeps consuming already stored Pottery until empty, then records missed needs. Repair triggers a route search from the real waiting cell. Empty returners obey the same rule.

The household clock starts at zero on placement. Every 400 **subsequent** simulation ticks it evaluates one need: if stock is positive it consumes exactly one Pottery and records a fulfilled date; otherwise it records a missed date. A missed date creates no debt. Delivery does not reset the clock or retroactively fill a missed need. The v3 order is production, demand against existing household stock, courier dispatch in ID order, courier movement/arrival, then invariants. Thus cargo arriving at the end of a deadline tick is available only for later needs. No household means no demand.

The v3 Pottery conservation identity is `completed = pottery output + pottery courier cargo + warehouse stock + supplier cargo + household stock + consumed total`. The Clay identity remains unchanged. Both inbound reservations are separate from goods; the household's fulfilled count equals its consumed total, and fulfilled plus missed dates equals elapsed ticks since placement divided by 400. The synthetic tests cover interruption, JSON roundtrip while waiting, 1,200 equal continuation ticks, deadline ordering, and a 30,000-tick run with production after full-buffer backpressure. These are OpenEmperor prototype rates and routing rules, not reconstructed Emperor housing behavior.

## Multiple households (`sandbox-settlement-v4`)

This optional OpenEmperor sandbox profile permits at most four independently placed households, with stable IDs 4–7. Keys `1`–`6` retain the production tools; repeatedly press `7` and click to place another house. A rejected fifth placement does not consume an ID. The three existing couriers remain: one carries Clay, one carries Pottery to the warehouse, and **one shared supplier** carries Pottery from the warehouse to exactly one household per trip. No original Emperor market or walker rule is inferred.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4 --sandbox-demo --sandbox-check --sandbox-resume-check --report-json
```

The demo searches a fixed 11×3 branched arrangement in the actual conservative buildability mask, then places one production chain and three houses through ordinary commands. If no such shape fits, it reports that clearly; the empty interactive sandbox still works. Every house begins empty with its own 400-tick demand clock and capacity 8. Dispatch scans house IDs cyclically after the last successful dispatch, skipping full or unreachable destinations. A successful trip removes at most four Pottery from the warehouse, reserves only its concrete target, and advances the cursor. The target remains fixed during rerouting and waiting. A new house or a failed query cannot redirect an active shipment. The next destination is chosen only when the supplier is idle at the warehouse. One blocked loaded supplier can therefore delay every other house.

The building list in the right panel selects each house by stable ID; its details show stock, reservation, demand progress, results and supplier state. Houses remain labeled on the map. The unchanged production rate can leave three or four houses with missed demands; the round-robin rule prevents persistent preference among houses that remain eligible, but does not promise equal amounts or uninterrupted supply. The Pottery balance includes output, both Pottery couriers, warehouse stock, every house's stock, and every house's consumed total. Schema 4 saves stable IDs, clocks, target and cursor. Schema-3 household saves retain their single house as ID 4 and remain on v3. The offscreen check requires actual arrivals and consumption in multiple houses, three rendered couriers, and optional tick-exact JSON continuation. Synthetic tests include a disconnected house, road interruption, wait/save/repair, idle cursor save, and 30,000 ticks.

## Scalable production (`sandbox-industry-v5`)

This optional OpenEmperor rule version 1 keeps the v4 household demand and cyclic shared supplier, and permits two Clay sources and two Pottery works feeding one warehouse. Keys `2` and `3` repeatedly place distinct instances until each two-building limit; `7` still places up to four houses. Every Clay source has its own Clay courier, every Pottery work has its own Pottery courier, and the warehouse has one household supplier: five couriers at full buildout. Each source extracts one Clay per 100 active ticks; each Pottery work needs two actually delivered Clay and 150 subsequent processing ticks per recipe. More buildings do not alter these rates or create raw material. A new Pottery work remains idle until Clay reaches its own input.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-check --sandbox-resume-check --report-json
```

The demo searches one fixed 11×5 branch pattern in the unchanged conservative buildability mask. It places both production pairs, warehouse and three houses through ordinary commands, or explains when no suitable patch exists. The core processes Clay sources by stable ID, Pottery works by stable ID, household demand, dispatch decisions in courier ID order, movement/arrival, then conservation and navigation checks. Each Clay courier scans reachable Pottery targets cyclically after its own last successful dispatch. An outbound shipment has one fixed target; an interrupted courier waits with its cargo and reservation. Multiple inbound couriers reserve the same target's capacity cumulatively. The second dispatch sees the first reservation. On arrival only that shipment's reservation is removed. The warehouse supplier may dispatch stored Pottery in a tick when a Pottery courier also arrives; the newly arrived Pottery becomes available on the following tick.

The Clay balance adds every source output, Clay courier cargo, Pottery input, bound recipe Clay and twice completed Pottery. The Pottery balance adds every Pottery output, Pottery courier cargo, warehouse stock, supplier cargo, all household stocks and consumed Pottery. Source extraction and completed recipes are counted per instance as well as globally. Limited courier throughput, road access and full buffers can still prevent a second works from helping. The UI labels each instance, lists its own buffers/progress and courier target, and shows all five agents. Schema 5 writes nine ID-ordered building slots and five couriers with explicit roles, owners, per-Clay target cursors, next ID states and active paths. Earlier schema-1–4 saves load under their original rules and are never converted to v5 automatically. Synthetic tests compare a control against a real connected expansion, exercise shared reservations and waiting, save/restore two active transports with a recipe, and run 30,000 ticks. These rules are not reconstructed original Emperor industry behavior.

## Optional multi-role walker display

The production-family sandboxes can display their Clay, Pottery, and Household couriers with a locally selected `curated_walker_preview` profile. Choose **Walker JSON...** on the New Sandbox or Load Save screen, or pass `--sandbox-visuals .local/visuals/walkers-v2.json` with `--data`, `--sandbox`, and a production-family `--sandbox-rules` value. The earlier Clay-only schema-1 profile remains valid. **No walker visuals** clears the menu selection. F2 toggles all configured roles between sprite preview and ordinary markers in the running sandbox. Missing roles retain markers. The selected profile is session-only and is checked again against the selected game-data root. Saves contain the same authoritative World with or without the profile.

The sprite follows `courier_position()` and the currently traversed path edge. The configured visual set is selected by `CourierState.role`, never by CourierId. A missing direction uses a diagnostic marker; idle and waiting use one static frame. The role-colored cargo marker reflects real courier cargo. Both Industry Clay couriers share their role's loaded frame textures, as do both Pottery couriers; the Household supplier uses its own set. Any repeated physical AssetId across roles is decoded and uploaded only once. Animation advances only with World ticks; pause and repeated rendering do not advance it. All five couriers enter the unified ground-depth painter. Image anchors and `ticks_per_frame` are curated preview choices. See [the manifest format](walker-visual-profile.md) and [research observations](reverse/research-log.md).

F3 opens a local visual-inspection panel without changing the World. `V` cycles Clay, Pottery, and Household (including a clear unconfigured state); `Q` cycles `pos_x`, `neg_x`, `pos_y`, `neg_y`; `C`/`E` step only the displayed clip; `X` switches 1×/4×; `B` changes the dark/light background. The panel marks the full frame bounds and selected ground anchor, and names the role, archive, physical record, tick rate, and short evidence. Normal moving couriers continue to select frames from simulation ticks. With `--sandbox-visuals` on the explicit Industry-v5 `--sandbox-check --sandbox-resume-check --report-json` path, `walker_visuals` reports configured roles, per-role configured and actually drawn directions, draws and fallbacks, globally unique decoded assets, texture uploads, per-tick equality with a separately advanced control World, and save/resume equality. The earlier `walker` field remains for Clay-only tooling. `manual_visual_review` remains `false` because an automated check cannot attest visual inspection.

## Optional core building display

The production-family sandboxes can display optional selected original Type-30 images for the existing one-cell ClaySource, Pottery, Warehouse and Household prototypes. Choose **Building JSON...** on the New Sandbox or Load Save screen, or pass `--building-visuals .local/visuals/buildings.json` alongside the sandbox CLI arguments. The existing Pottery-only profile remains valid. F4 independently restores all building markers for diagnostics; a missing role uses its marker even with F4 on. Without a profile, F4 reports that no building visuals are loaded. The Inspector retains the authoritative BuildingId, stock, reservations, recipe, progress and courier, and adds only the selected role, SG3 record, image dimensions, anchor and curated-preview status. Picking and the white selection diamond still address the one simulation cell.

Stored map graphics, sandbox roads, buildings and walkers share a deterministic projected-ground painter order. An entire image is drawn once, with no per-frame decode or inferred foreground mask. A valid hover placement draws the same anchored image translucently in the painter; invalid hover and selection remain visible overlays afterward. This preview does not create a BuildingState or mark a save dirty. A tall stored image can cover a sandbox image only when its front-cell ground sorts later; complex foreground/background splitting is not supported. The four core size-flag-2 roles in City-v10 and City-v11 use their authored 2×2 footprint for occupancy, selection and depth; legacy profiles and other visuals retain their established geometry. With `--building-visuals` in the explicit Industry-v5 JSON sandbox check, `building_visuals` reports configured roles, unique decoded assets, texture uploads, instance draws and marker fallbacks per role, and equality with a separate control World; `manual_visual_review` remains false. See [the manifest convention](building-visual-profile.md) and [the bounded local observations](reverse/research-log.md).

An optional [curated road profile](road-visual-profile.md) replaces yellow road diamonds with locally selected Type-30 images. F6 toggles road pictures independently of F2 walkers and F4 buildings; missing custom masks keep diamonds. `RoadNeighborMask` is the four-bit storage-neighbor preview convention and counts roads only. `EntranceMask` separately reports adjacent placed sandbox buildings for diagnostics; it never selects the road tile. `World::find_route()`, road placement/removal validation, `road_revision`, command sequence, courier travel, and saves use their existing rules. During a valid RoadDrag, planned cells and adjacent existing roads show their hypothetical road-only masks in the shared painter; an invalid plan shows red diagnostic diamonds after it. No road variant or entrance mask is stored. F7 switches between the default unified painter and the former map-first order for visual comparison, without entering save data or changing simulation state.

For visual inspection, `Z` cycles the sandbox camera through 1×, 2× and 4× around the map viewport center. This changes only the camera; world positions, picking cells and save data stay unchanged.
# Frozen alpha simulation scope

The first alpha froze the authored `sandbox-industry-v5` scope: two Clay sources, two Pottery works, one warehouse, four households, five couriers, roads with live rerouting/removal, and OpenEmperor schema-5 save/load. City-v6 layers its schema-6 economy rules over that stable simulation without changing Industry-v5. City-v7 adds only its schema-7 Food and House-development rules without changing City-v6. Neither profile claims compatibility with Emperor's simulation or savegames.

The long acceptance test uses ordinary commands and the production `World::tick()` path for 100,000 ticks. Two independently constructed Worlds receive the same fixed command schedule, including road removals and replacements, and are compared throughout. Checkpoints cover direct snapshots, immediate `World::restore`, real JSON serialization, exact pre-tick equality, deterministic save bytes, and continued execution. Existing conservation and navigation validation is supplemented in the driver with bounded stock, reservations, IDs, courier roles, paths, positions, phases, and target checks. See [automated checks](testing.md#build-and-automated-checks).
