# Logistics sandbox (`sandbox-logistics-v1`)

The sandbox is an independent prototype, not a reconstruction of Emperor's economy or building rules. Start an empty world with:

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map
```

Add `--sandbox-demo` for a deterministic six-cell arrangement: workshop, four roads, warehouse. The demo searches the actual buildable mask in row-major order and uses the same typed placement commands as mouse input. If it cannot find six adjacent suitable cells, it fails without altering the original map. For an offscreen finite check of the same view and simulation, use `--sandbox-check --report-json` in place of the interactive mode; it automatically places the demo and runs 700 ticks with SDL's dummy/software renderer.

The original `.map` and SG3/`.555` files are read only. The background uses the existing `exe-6373328b-v213-slot8-runtime-table` stored-graphics profile and `edge-byte-4x4` preview policy. Those are selected only for sandbox mode; the diagnostic commands retain their existing defaults. The original saved road imagery does not create roads in the sandbox graph. All placed roads, buildings, goods, and courier state live in a separate `simulation::World`; they persist only when explicitly saved to an OpenEmperor sandbox file.

`sandbox_buildable_v1` permits a storage cell only when it belongs to the existing candidate mask, has no off-map bit, has exact raw terrain word `0x80` and raw object word `0`, and is covered by a successfully decoded supported 1×1 stored footprint. Unknown, failed, and multi-cell graphics remain blocked. This deliberately conservative local rule is not Emperor's construction test. Workshop and warehouse occupy one sandbox cell each; that size does not claim their original footprint.

The core is SDL-free and has one good, **Goods**. It advances only by explicit integer ticks at 20 ticks/second. A workshop creates one unit after 100 active production ticks, holds at most 8, and pauses progress while full without accumulating a backlog. The warehouse holds 32. One courier carries at most 4 and spends 10 ticks per orthogonal edge. There is no raw-material input, consumption, or deletion. Every tick checks `total_produced == workshop_stock + courier_cargo + warehouse_stock`.

Tick order is production, dispatch if the courier is at the workshop and warehouse space and route exist, then one movement tick; unloading happens only upon reaching the warehouse. The return trip follows the reverse path edge by edge. The route uses bounded BFS over placed roads, with the workshop and warehouse as endpoints; fixed neighbor order is up, left, right, down. The route is cached by placement revision, so rendering does not run pathfinding. Closing a road gap invalidates that cache. Buildings may be placed without a route; the HUD reports the blockage. There is no removal, so an in-flight route cannot be broken by a command.

The app feeds frame time to a bounded fixed-step accumulator, at most eight ticks per update. 1×, 2×, and 4× affect only that feed. Pausing stops ticks but leaves camera and placement active; `.` executes one tick while paused. A long frame can leave display time behind wall time, but the tick counter never jumps over events.

Use `1` for roads, `2` for workshop, `3` for warehouse, and `4` or right-click to select. Left-click applies the current tool. Space pauses, `.` steps, `+`/`-` changes speed, WASD/arrows pan, the wheel zooms around the pointer, `R` refits, and Escape exits. Green/red hover geometry comes from the same validation used by placement, and clicks in the HUD do not place objects. SDL primitive geometry draws roads, two distinct buildings, the courier, and a cargo indicator over the original background before one `SDL_RenderPresent` per frame. This overlay is a prototype visualization; it does not reproduce original terrain occlusion, original sprites, building sizes, or transport behavior.

Later work may research the original game's economy, traversability, graphic choices, draw ordering, and walker behavior separately. No such assumptions are encoded in this sandbox profile.

## Resource-dependent production (`sandbox-production-v2`)

Select the second, explicitly authored profile with `--sandbox-rules sandbox-production-v2`. Without this option, `sandbox-logistics-v1` and its controls remain unchanged. Both profiles use the same read-only original-map background, `sandbox_buildable_v1` mask, simulation `World`, fixed-step `TickDriver`, stored-graphics renderer, camera, and picking. The v2 demo requires seven consecutive suitable cells and places Clay source, two roads, Pottery, two roads, and Warehouse through ordinary commands. It never changes the mask to make a demo fit.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --report-json
```

Keys `1`–`5` select Road, Clay source, Pottery, Warehouse, and inspection. The existing pause, step, speed, pan, zoom, reset, mouse, and Escape controls still apply. SDL primitive geometry distinguishes the three one-cell buildings, courier A (Clay) and courier B (Pottery), including their loads. The HUD reads source output/progress, pottery input/active recipe/progress/output, both courier states, warehouse stock/reservations, and the two conservation totals from the simulation. It has no timer or inventory of its own.

The v2 rules are deliberately unrelated to established Emperor gameplay. One Clay source creates 1 Clay per 100 active ticks and stores at most 8. One Pottery building accepts at most 8 Clay and holds at most 8 Pottery. A recipe transfers 2 actually delivered Clay into its active work state when it starts, waits 150 further ticks, and creates 1 Pottery. The start tick counts as **zero** processing ticks. Only one recipe can run; it starts only if its future output slot is available. A Warehouse accepts at most 32 Pottery, never Clay. Both couriers carry at most 4 units of one good and traverse each orthogonal edge in 10 ticks. Full outputs pause production without a backlog.

Tick order is stable building IDs (Clay source, Pottery), stable courier IDs (A, B) for dispatch, then both courier movements and arrivals, then invariant checks. Consequently, Clay arriving at the end of a tick can first start a recipe on the next tick. Routes are bounded shortest-path BFS searches through **placed** roads only, with an owning building as start and its designated receiving building as end. A third building is never transit. Each courier caches its own start/goal/revision result. A placed road revises both caches; HUD and render queries never search paths or advance goods. A courier atomically moves source output into cargo and reserves the same amount of destination capacity at dispatch. Only at actual arrival is the reservation released and the good inserted; return is edge-by-edge without cargo. The two couriers may share a road without collision handling.

After every tick, the core checks both balances and all buffer, cargo, and reservation bounds:

```text
clay_extracted_total = clay_in_source + clay_in_transport + clay_in_pottery_input
                     + clay_in_active_recipe + 2 * pottery_completed_total
pottery_completed_total = pottery_in_output + pottery_in_transport + pottery_in_warehouse
```

Reserved space does not count as produced goods. The original v1 Goods balance continues to be checked under its own profile. Headless v2 runs 3,000 ticks through the same `SandboxView` on a software renderer and requires processing, arrival in the warehouse, invariant validity, and frames that draw both couriers. No resource exhaustibility, original building dimensions, walker art, or original production schedule is inferred from map data. Those remain separate future research tasks.

## OpenEmperor sandbox saves

Both rule profiles can save and resume their own simulation state. This is **not** an original Emperor savegame format and cannot load one. The original map and graphics remain necessary and read only.

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-save .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --load-sandbox .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --sandbox-resume-check --report-json
```

`F5` saves at the configured path; `F9` reloads it. The second command also uses the loaded path for later F5/F9. Without a configured path the HUD reports the missing path. A successful load pauses the sandbox, resets the frame accumulator, and leaves it at exactly the saved tick; Space resumes and `.` advances one tick while paused. Camera and tool are local UI state, not simulation state. A failed load leaves the current World unchanged and reports its cause. The old finite `--sandbox-check` remains read only; only the explicit `--sandbox-resume-check` writes a temporary file and deletes it afterward.

The UTF-8 JSON document has `format: "openemperor-sandbox-save"` and `schema_version: 1`, separate `rules.id` and `rules.version`, a map-relative path, map part 0, SHA-256 of the required original map, fixed graphics/footprint/buildability profile labels, and SHA-256 of the post-decode buildability mask. The `world` section stores dimensions, tick and command counters, placement revision, roads, both profiles' building/stock/progress fields, totals, courier identities/cargo/reservations, active paths, vertices, and edge progress. Raster occupancy, owner indices, and future route caches are rebuilt; in-flight paths are retained. The file contains no map raster, SG3 pixels, absolute installation path, or original game data.

Loading rejects changed original map or buildable mask, unknown versions, unsupported profiles, malformed paths, inconsistent placements, routes, capacities, reservations, or conservation balances. A save is limited to 8 MiB and 32 JSON nesting levels. The save target must be outside `--data`; existing symlink components are rejected. Writing uses a unique temporary file in the target directory, checked write/flush/close, then replacement rename. A failed pre-rename write leaves the previous save in place. Rename is an atomic directory operation on supported local filesystems, but this is not a promise of power-loss durability.
