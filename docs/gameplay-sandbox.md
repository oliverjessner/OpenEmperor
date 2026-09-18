# Logistics sandbox (`sandbox-logistics-v1`)

## Start from the menu

`./build/openemperor` opens the normal main menu. Choose a folder containing installed or extracted original Emperor files, then select **New sandbox**, a supported standalone map, and an OpenEmperor rules profile. The menu preselects `sandbox-industry-v5`; the demo arrangement is off by default. The GOG offline installer file alone is not a usable data folder. `--data <directory>` overrides the remembered folder for this launch without changing direct CLI mode semantics. The old `--sandbox` command below still starts directly with its historical v1 default.

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

For the v1–v4 profiles, the UTF-8 JSON document has `format: "openemperor-sandbox-save"` and `schema_version: 4`, separate `rules.id` and `rules.version`, a map-relative path, map part 0, SHA-256 of the required original map, fixed graphics/footprint/buildability profile labels, and SHA-256 of the post-decode buildability mask. The `world` section stores dimensions, tick and command counters, placement revision, total road placements/removals, roads, seven canonical ID-ordered building records and three courier records, stocks, production and demand counters, reservations, active paths, vertices, edge progress, waiting state, last checked road revision, and reroute-attempt counts. Household fields include `placed_tick`, `demand_progress`, `fulfilled_demand`, `missed_demand`, `consumed_total`, and `last_demand_status` (0 none, 1 fulfilled, 2 missed). `next_household_id` and `last_dispatched_household` preserve ID allocation and cyclic selection independently of the supplier's active target. Raster occupancy, owner indices, and future dispatch route caches are rebuilt; in-flight paths are retained. The file contains no map raster, SG3 pixels, absolute installation path, or original game data.

Loading rejects changed original map or buildable mask, unknown versions, unsupported profiles, malformed paths, inconsistent placements, routes, capacities, reservations, or conservation balances. A save is limited to 8 MiB and 32 JSON nesting levels. The save target must be outside `--data`; existing symlink components are rejected. Writing uses a unique temporary file in the target directory, checked write/flush/close, then replacement rename. A failed pre-rename write leaves the previous save in place. Rename is an atomic directory operation on supported local filesystems, but this is not a promise of power-loss durability.

Existing valid `schema_version: 1`, `schema_version: 2`, `schema_version: 3`, and `schema_version: 4` files remain readable under their original profiles. Logistics rules stay at `rules.version: 1`. A known old production schema-1 save with `rules.version: 1` is validated, given zero road-removal and reroute counters, and upgraded **in memory** to production `rules.version: 2`; schema-2 production retains version 2. Neither becomes the household profile. Loading does not advance a tick or rewrite the original save. A subsequent explicit F5 writes schema 4. Unknown schema and rule versions still fail.

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

## Optional Clay walker display

The production-family sandboxes can display their Clay couriers with a locally selected `curated_walker_preview` profile. Choose **Walker JSON...** on the New Sandbox or Load Save screen, or pass `--sandbox-visuals .local/visuals/clay-walker.json` with `--data`, `--sandbox`, and a production-family `--sandbox-rules` value. **No walker visuals** clears the menu selection. F2 toggles sprite preview and ordinary markers in the running sandbox. The selected profile is session-only and is checked again against the selected game-data root. Saves contain the same authoritative World with or without the profile.

The sprite follows `courier_position()` and the currently traversed path edge. A missing direction uses a diagnostic marker; idle and waiting use one static frame. The blue cargo marker reflects real courier cargo. Industry's two Clay couriers share the loaded frame textures, while Pottery and Household couriers keep their existing markers. Animation advances only with World ticks; pause and repeated rendering do not advance it. The image anchors and `ticks_per_frame` are curated preview choices. See [the manifest format](walker-visual-profile.md) and [research observations](reverse/research-log.md).

F3 opens a local visual-inspection panel without changing the World. `Q` cycles `pos_x`, `neg_x`, `pos_y`, `neg_y`; `C`/`E` step only the displayed clip; `X` switches 1×/4×; `B` changes the dark/light background. The panel marks the full frame bounds and selected ground anchor. Normal moving couriers continue to select frames from simulation ticks. With `--sandbox-visuals` on the explicit Industry-v5 `--sandbox-check --sandbox-resume-check --report-json` path, the `walker` object reports configured and moving-drawn directions, decoded physical assets, texture uploads, fallbacks, per-tick equality with a separately advanced control World, and save/resume equality; `manual_visual_review` remains `false` because an automated check cannot attest visual inspection.
