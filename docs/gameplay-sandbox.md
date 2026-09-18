# Logistics sandbox (`sandbox-logistics-v1`)

The sandbox is an independent prototype, not a reconstruction of Emperor's economy or building rules. Start an empty world with:

```sh
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map
```

Add `--sandbox-demo` for a deterministic six-cell arrangement: workshop, four roads, warehouse. The demo searches the actual buildable mask in row-major order and uses the same typed placement commands as mouse input. If it cannot find six adjacent suitable cells, it fails without altering the original map. For an offscreen finite check of the same view and simulation, use `--sandbox-check --report-json` in place of the interactive mode; it automatically places the demo and runs 700 ticks with SDL's dummy/software renderer.

The original `.map` and SG3/`.555` files are read only. The background uses the existing `exe-6373328b-v213-slot8-runtime-table` stored-graphics profile and `edge-byte-4x4` preview policy. Those are selected only for sandbox mode; the diagnostic commands retain their existing defaults. The original saved road imagery does not create roads in the sandbox graph. All placed roads, buildings, goods, and courier state live in a separate `simulation::World` and are discarded when the session ends.

`sandbox_buildable_v1` permits a storage cell only when it belongs to the existing candidate mask, has no off-map bit, has exact raw terrain word `0x80` and raw object word `0`, and is covered by a successfully decoded supported 1×1 stored footprint. Unknown, failed, and multi-cell graphics remain blocked. This deliberately conservative local rule is not Emperor's construction test. Workshop and warehouse occupy one sandbox cell each; that size does not claim their original footprint.

The core is SDL-free and has one good, **Goods**. It advances only by explicit integer ticks at 20 ticks/second. A workshop creates one unit after 100 active production ticks, holds at most 8, and pauses progress while full without accumulating a backlog. The warehouse holds 32. One courier carries at most 4 and spends 10 ticks per orthogonal edge. There is no raw-material input, consumption, deletion, or savegame. Every tick checks `total_produced == workshop_stock + courier_cargo + warehouse_stock`.

Tick order is production, dispatch if the courier is at the workshop and warehouse space and route exist, then one movement tick; unloading happens only upon reaching the warehouse. The return trip follows the reverse path edge by edge. The route uses bounded BFS over placed roads, with the workshop and warehouse as endpoints; fixed neighbor order is up, left, right, down. The route is cached by placement revision, so rendering does not run pathfinding. Closing a road gap invalidates that cache. Buildings may be placed without a route; the HUD reports the blockage. There is no removal, so an in-flight route cannot be broken by a command.

The app feeds frame time to a bounded fixed-step accumulator, at most eight ticks per update. 1×, 2×, and 4× affect only that feed. Pausing stops ticks but leaves camera and placement active; `.` executes one tick while paused. A long frame can leave display time behind wall time, but the tick counter never jumps over events.

Use `1` for roads, `2` for workshop, `3` for warehouse, and `4` or right-click to select. Left-click applies the current tool. Space pauses, `.` steps, `+`/`-` changes speed, WASD/arrows pan, the wheel zooms around the pointer, `R` refits, and Escape exits. Green/red hover geometry comes from the same validation used by placement, and clicks in the HUD do not place objects. SDL primitive geometry draws roads, two distinct buildings, the courier, and a cargo indicator over the original background before one `SDL_RenderPresent` per frame. This overlay is a prototype visualization; it does not reproduce original terrain occlusion, original sprites, building sizes, or transport behavior.

Later work may research the original game's economy, traversability, graphic choices, draw ordering, and walker behavior separately. No such assumptions are encoded in this sandbox profile.
