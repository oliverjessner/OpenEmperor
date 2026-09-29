# Responsive road-building measurements

This pass starts from commit `6dcf66bfcbcce51fd6823ea0e46136510235b5a7`. It changes presentation and command batching only. It adds no gameplay, rule, save, map, footprint, asset, or routing semantics.

The former pointer path called `plan_road()` for every mouse-motion event. Planning copied the complete `World`, executed every proposed road on that copy, and therefore ran `refresh_routes()` after every new cell. `commit_road()` then planned the path again and repeated every command on another copy. A 30-cell drag in the measured full synthetic City-v10 fixture consequently performed 30 route refreshes, 12,240 BFS calls and 212,400 visited cells before MouseUp. Commit performed another 60 route refreshes, 24,480 BFS calls and 424,800 visits.

The new preview constructs the same X-then-Y cells and uses the SDL-free `World::validate_road_batch()` helper. It validates the complete aggregate cost and counter ranges while leaving `World` untouched. `SandboxView` keeps picking current but coalesces plan construction to the latest pointer state once per frame and reuses it while the start cell, end cell, topology/occupancy revision, command sequence and treasury are unchanged. MouseUp uses its actual event position and performs current validation.

A changed commit validates the whole batch, applies it to one transaction copy without intermediate route refreshes, refreshes routes once for the final topology, and publishes only the complete result. Individual road costs, `command_sequence`, `road_revision`, `roads_placed_total`, no-op behavior and continuation remain identical to sequential commands. A batch made entirely of existing roads makes no copy and performs no refresh.

## Release microbenchmark

Measurements were taken on 2026-09-29 on a MacBook Pro (Mac14,9), Apple M2 Pro, 32 GB, macOS 26.7, Apple clang 21.0.0. Both the starting commit and working tree were compiled directly with C++20 and `-O2`. The same synthetic fixtures and 30-cell drag were used for both. City-v10 contained 34 buildings and 14 couriers; City-v11 rule 3 contained 32 buildings and 14 couriers. Each timing row contains 12 samples. With 12 samples, the reported p95 is the maximum sample. The benchmark excludes rendering, Present, input-device delivery and autosave.

| Fixture and operation | Before median / p95 / max | After median / p95 / max |
|---|---:|---:|
| City-v10 plan, 30 cells | 8.258 / 8.873 / 8.873 ms | 0.000666 / 0.001583 / 0.001583 ms |
| City-v10 commit, 30 cells | 15.533 / 17.448 / 17.448 ms | 0.251 / 0.283 / 0.283 ms |
| City-v11-v3 plan, 30 cells | 5.577 / 6.187 / 6.187 ms | 0.000500 / 0.000667 / 0.000667 ms |
| City-v11-v3 commit, 30 cells | 10.905 / 12.407 / 12.407 ms | 0.165 / 0.170 / 0.170 ms |

| Fixture and phase | World copies | Route refreshes | BFS calls | Visited cells |
|---|---:|---:|---:|---:|
| City-v10 preview before | 1 | 30 | 12,240 | 212,400 |
| City-v10 preview after | 0 | 0 | 0 | 0 |
| City-v10 commit before | 2 | 60 | 24,480 | 424,800 |
| City-v10 commit after | 1 | 1 | 408 | 7,080 |
| City-v11-v3 preview before | 1 | 30 | 8,340 | 133,860 |
| City-v11-v3 preview after | 0 | 0 | 0 | 0 |
| City-v11-v3 commit before | 2 | 60 | 16,680 | 267,720 |
| City-v11-v3 commit after | 1 | 1 | 278 | 4,462 |

No second routing optimization was added: after batching, the only real refresh measured below 0.3 ms in these fixtures. Tests compare complete authoritative snapshots against the former sequential-command procedure and continue both Worlds for equal ticks.

## Runtime diagnostics

Pass `--performance-diagnostics` or set `OPENEMPEROR_PERF_DIAGNOSTICS=1` for a bounded end-of-run summary. It records at most 4,096 samples per timing and reports median, p95 and maximum for event handling, hover/picking, road planning, budget checks, commit, route refresh, simulation update, world/HUD rendering, Present, frame wait, autosave capture and autosave write. Global counters include temporary Worlds: copies, restores, executes, road plans, route refreshes, BFS calls/visits, file operations, decodes, uploads and simulation ticks. There is no per-motion or per-frame console output.

The main loop now sleeps only for the remaining part of a 60 Hz target frame using SDL's monotonic nanosecond clock and `SDL_DelayPrecise()`. Event, update, render and Present work therefore reduce the wait instead of being followed by an unconditional additional 16 ms. The fixed 20 Hz simulation, speed multipliers and bounded catch-up behavior are unchanged.

Automated tests cover the pure-preview counters, same-cell motion coalescing, 1/10/30/256-cell batches, existing/new mixtures, aggregate budget failure, blocked final cells, 2×2 foundations, command-counter overflow, changed Worlds, all-existing batches, City-v11 road reserve projection, City-v10/City-v11 active continuation, cancellation/focus/UI gestures, save/load and deterministic ticks. Autosave remains enabled and separately timed at runtime; this microbenchmark did not cross an autosave checkpoint.

The packaged arm64 app was also run against the local, user-supplied Xia map and original graphics in City-v10. A fresh run was switched to 4× and observed for 30 seconds. The bounded end-of-run report covered the whole session, including its 1× startup/automation period: 3,190 ticks, or 69.531 ticks per wall-clock second overall. It reported simulation-update median/p95/max of 0.019/0.051/0.923 ms, world rendering 0.319/0.819/2.626 ms, Present 0.600/1.585/8.567 ms and frame wait 15.494/15.666/18.541 ms. The rate uses the simulation-counter delta from main-loop entry; setup work can no longer inflate it. A separate packaged road-tool session produced two plans with a 0.002 ms maximum and no World copies; its initial demo construction accounts for the process-wide execute, route and BFS counters. The automation transport could not reliably deliver a held mouse drag to this high-DPI SDL window, so a complete physical pointer-drag review remains **NOT RUN**. No menu autosave checkpoint was crossed in the explicit-CLI sessions. Time measured to the `SDL_RenderPresent()` call is not physical input-to-photon latency.
