# City-v12: deterministic fire safety

`sandbox-city-v12`, rule version **1**, is an explicitly selected OpenEmperor-authored profile based on City-v11-v4. It retains the economy, Market distribution, population, operation controls, priorities, footprints and safe demolition. It adds temporary fire incidents and road-based prevention. This is not a reconstruction of Emperor's original fire mechanics. No original fire assets were researched or assigned, no goods were added, and original data remains read-only.

City-v11 stays the New Sandbox default (currently rule 3 pending its separate native v4 acceptance). Existing settings, CLI defaults and schemas 1–13 keep their identities. There is no City-v11-to-v12 migration.

## Rules and tick order

| Rule | Value |
| --- | --- |
| Start funds | 1,300 |
| Risk interval | 20 ticks since placement |
| Risk threshold | 100 |
| Protection after an actual visit | 2,400 ticks |
| Natural incident duration | 600 ticks |
| Fire Watch | 80 funds, 2 workers, 1×1, limit 2 |
| Entity limits | 40 Buildings, 24 Couriers |

Clay Sources, Potteries, Warehouses, Houses, Farms, Service Posts and Markets are fire-eligible. Fire Watch, roads and empty cells are excluded; Fire Watch is fireproof in this profile. Unprotected, non-burning eligible buildings gain one risk point when `(tick - placed_tick) % 20 == 0`, after placement. Player operation pause does not stop structural risk. There is no RNG, wall-clock input or extra saved risk-progress counter.

Each tick captures staffing, increments the tick, produces, evaluates House demand, dispatches, moves/arrives, updates fire, then checks invariants. A fire started in the fire phase affects working buildings on the following tick. An Inspector arriving in the movement phase protects before the risk update. Expiry makes the fire query false at its deadline; tick-start staffing resumes production on the next tick. Risk can increment on the expiry tick if it is a placement-relative risk boundary.

The three authoritative fields are `fire_risk`, `fire_protection_until_tick`, and `fire_until_tick`. Risk is 0–99. At threshold, risk and protection become zero and the fire deadline becomes `tick + 600`. An actual Inspector arrival sets risk to zero, the fire deadline to the current tick and protection to `tick + 2400`. Visits refresh protection rather than stacking it. Expired deadlines remain historical values; queries compare them with the current tick. Arithmetic uses checked future differences, and City-v12 refuses a tick before entering the final 2,400-tick uint64 range to preserve deadline headroom.

## Operation and transport

Burning working buildings receive no workers, produce nothing and start no new owned trip. Installed worker demand still includes them; active demand excludes burning or player-paused operations. The ordinary High/Normal/Low, stable-ID allocation is captured once per tick. Fire never changes `operating_enabled`. After expiry or extinguishing, a formerly Running building resumes and a player-paused building stays paused.

Stocks and active recipes remain intact. Existing trips finish or wait/reroute normally, and inbound deliveries remain valid at burning targets under the existing capacity/reservation rules. A burning Market accepts goods but starts no distribution. A burning Service Post starts no visits; already delivered Service coverage expires normally. There is no automatic destruction, stock loss, rubble, spending, refund or rescue income.

A burning House retains its population as workforce supply. At a demand deadline it records a miss even with all goods and Service present, consumes neither good and earns no tax. The existing population decline and move-in grace apply. No extra evacuation rule is introduced.

Each Fire Watch owns one cargo-free `FireInspector` with `Good::Goods`, zero cargo and zero reservations. It shares existing entrances, BFS, road caches, edge protection, movement and rerouting. `courier_can_target` is the shared target predicate for selection, caches, invariants and restore. Each Watch caches at most 38 eligible targets, refreshed only after topology changes.

Selection first searches reachable burning targets, then other reachable eligible targets, including protected ones. Within each phase it cycles in stable Building-ID order after that Inspector's own last target. There is no global cursor or risk-score scheduler. No path or dispatch grants protection: only physical arrival does. A paused or unstaffed Watch starts no new trip; its existing trip finishes. Disconnected districts require a connected Watch of their own or a road connection to another Watch.

Burning buildings reject demolition with `Cannot demolish: building is on fire.` After fire ends, every ordinary empty-building, recipe, reservation and active-trip blocker still applies. No demolition exemption is introduced.

## Starter and presentation

The prepared starter uses the existing 15×5 City-v11 pattern and normal paid tick-0 commands. A Fire Watch at `origin + (14,3)` accesses the existing road, adding no roads. Cost is **1,280**, remaining funds **20**, population **24**, and installed/assigned workforce **24/24**. All goods, coverage, taxes and risk start at zero. City-v11's 1,200-cost starter is unchanged.

Select **Fire safety and city services** in New Sandbox, or launch explicitly:

```sh
./build/openemperor \
  --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Xia.map \
  --sandbox-rules sandbox-city-v12 \
  --sandbox-demo \
  --sandbox-save "$PWD/.local/saves/city-v12.json"
```

Use your own installed/extracted data folder if that local path does not exist. `F` selects the visible Fire Watch tool. Fire Watch and Inspector use muted authored fallback shapes, with `FW`/`FI` labels in diagnostics. Burning buildings receive a small SDL primitive flame whose phase derives only from World tick. Rendering, camera, zoom and visual toggles neither advance nor edit fire and do not decode/upload assets per frame.

The top status shows protected/eligible counts and burning count. The Inspector shows risk, protection or remaining fire time, and distinguishes **On fire — operation suspended**, **Paused by player**, and staffing. Fire Watch shows operation, priority, workers, Inspector phase/target/status and city protection counts. Starter supply guidance recognizes fire separately from workforce shortage; Fire Watch is not falsely treated as an income-chain requirement.

## Persistence and validation

Schema **14** belongs only to City-v12 rule 1. It extends the scalable snapshot, operation controls and demolition accounting with the three required fire fields per Building. Fire Watch's own fields must be zero. Other profiles have no serialized fire fields and typed snapshots must retain zero; explicit nonzero old-schema fire fields are rejected. Inspector identity, owner, eligible target/cursor, path and zero payload are validated before publishing a World. There are no special fire recovery slots: the ordinary save document and Autosave/Recovery paths carry exact state.

Synthetic tests cover interval/threshold/expiry, protection refresh and expiry, arrival-before-risk ordering, staffing changes on subsequent ticks, burning target priority, independent cyclic cursors, road waiting/reconnection, disconnected districts, paused/unstaffed Watches, supply and tax, recipe/producer freeze, Market inbound/outbound, Service suspension and supplied-House misses. Isolated operational fixtures change only authored fire fields on valid command-built snapshots; natural incidents, districts and endurance use ordinary commands and ticks.

File roundtrips cover ticks 19/20, 1999/2000, 2599/2600, immediately before/after Inspector arrival and protection expiry, followed by exact continuation comparisons. A live burning checkpoint validates the ordinary autosave path. A 20,000-tick paired run includes road break, fire, extinguishing, priority and demolition. The 100,000-tick endurance run uses the full 40/24 collection limits, protected and disconnected districts, repeated natural fires, reconnection and demolition/rebuild. World validates every tick; tests additionally check ID/cache/workforce bounds.

## Milestone validation (2026-10-01)

Starting HEAD was `07460808cbb1ac42ba123d2149ea529e353a6bb0`, with a clean worktree. No commit, push, tag, release or publication was performed. The following results are for the local dirty-workspace candidate.

| Check | Result |
| --- | --- |
| Debug full CTest | 53/53 passed; 268.71 seconds |
| Debug subsequent menu, renderer and road checks | 3/3 passed; 51.65 seconds |
| Release full CTest | 53/53 passed; 16.39 seconds |
| ASan/UBSan full CTest | 53/53 passed; 44.01 seconds |
| Additional malformed-schema/overflow persistence checks | Debug, Release and sanitizer passed |
| 20,000-tick determinism | Passed, snapshots compared every tick |
| 100,000-tick full-city endurance | Passed, including four remote natural incidents and demolition/rebuild |
| Old-profile, City-v11-v4 demolition and autosave regressions | Passed within the full suites |
| City-v12 road preview and reserve warning | Zero copies/restores/commands/BFS/refreshes/I/O/decodes/uploads; batch continuation matches ordinary commands |
| 50 presentation-toggle/camera/zoom iterations during fire | Snapshot unchanged; zero copies/BFS/refreshes/asset decodes/texture uploads |
| Local original-data Xia check, 8,000 ticks | Passed; 11 Buildings/8 Couriers, 10/10 protected, no burning, 4,360 tax collected |
| Xia file save/load and continued state | Exact immediate and continued equality |
| macOS package | Full Release suite, recursive Mach-O verification, staging dependency fixups, ad-hoc signature, relocation/unzip and ordinary menu/save checks passed |
| `file` | `Mach-O 64-bit executable arm64` for CLI and packaged executable |
| Compiler diagnostics | No compiler warnings or errors in Debug, Release or sanitizer builds |
| Native 20–30 minute fire-expansion playtest | **Not passed / incomplete**, as described below |

The package contains no original assets, decoded images or personal histories. Dependency fixup emitted the expected staging signature-invalidation notices; the staged files were subsequently signed and verified. The final bundle is `dist/OpenEmperor.app`, executable `dist/OpenEmperor.app/Contents/MacOS/OpenEmperor`; its ZIP is `dist/OpenEmperor-0.1.0-alpha.2-07460808cbb1-macos-arm64.zip`. It remains a local test candidate, without Developer ID or notarization. The repository's project-license selection remains unresolved.

The synthetic paid starter dispatched at tick **1**, first protected at **75** and first taxed at **400**. A disconnected Service Post placed at 4,000 ignited at **6,000** and a newly built, connected second Watch extinguished it at **6,035**. The road-break scenario ignited at 2,000 and extinguished only after reconnection and arrival at **2,046**. A simple one-target route protected at **45** and expired at **2,445**. These are fixture-specific observations, not universal route timings.

Market incidents retained stock and accepted genuine inbound goods while suppressing new outbound dispatch. Pottery retained its bound two Clay and recipe progress through the incident and resumed on the next staffed tick. Clay/Farm production counters froze; active trips could still remove previously produced output. Service stopped new visits without altering already delivered coverage. A genuinely supplied House missed demand during fire, consumed nothing, earned no tax and followed ordinary population decline. These checks passed in synthetic scenarios.

The native desktop attempt used an isolated app identity and a separate ignored test save, rather than personal preferences/history. The local Xia starter visibly produced tax and reached 10/10 protection. Space pause/resume, `F` tool selection, Help and F5/F9 save/load worked; loading restored the saved tick paused. The coordinate input from the UI automation tool moved the pointer but did not activate toolbar/list clicks, consistent with the earlier v4 automation limitation. Therefore expansion, second-Watch placement, live-fire inspection, extinguishing, and demolition before/after fire were **not** completed as a native playthrough. The session was closed after saving. No automated result is presented as a completed 20–30 minute manual acceptance.

For the six requested playtest questions, layout impact and second-Watch usefulness are demonstrated by the disconnected-district test, road connectivity by waiting/reconnection, and recoverable pressure by temporary operation/House incidents. Player understanding of production interruption and recovery remains unverified in a full native playthrough. In-fire save/load and demolition blockers pass automated tests; native F5/F9 was verified on a protected starter, not during fire. City-v11 therefore remains the menu default.

Build commands used the existing configured trees:

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel 6
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure --parallel 6
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure --parallel 6
tools/package_macos.sh
file build/openemperor dist/OpenEmperor.app/Contents/MacOS/OpenEmperor
```

Debug and Release use macOS arm64 and locally installed SDL3; the repository's pinned SDL dependency configuration remains available and unchanged. The sanitizer tree uses RelWithDebInfo with ASan/UBSan; leak detection was disabled for this macOS run. Full suites were followed by targeted checks for the later menu/road and malformed-persistence test additions. Package construction runs its own full Release suite.

Created files:

- `src/simulation/WorldFire.cpp`
- `tests/SandboxCityV12Tests.cpp`
- `docs/city-v12.md`

Modified files:

- `src/simulation/World.h`, `World.cpp`, `CityStartGuidance.h`, `CityStartGuidance.cpp`
- `src/persistence/SandboxSave.cpp`
- `src/app/MenuSession.cpp`, `MenuStorage.cpp`, `SandboxCheck.cpp`, `SandboxUiLayout.h`, `SandboxUiLayout.cpp`, `SandboxView.cpp`, `WalkerPose.cpp`, `main.cpp`
- `tests/MenuSessionTests.cpp`, `RoadResponsivenessTests.cpp`, `SandboxViewTests.cpp`
- `CMakeLists.txt`, `AGENTS.md`, `README.md`, `KNOWN_ISSUES.md`, `docs/architecture.md`, `docs/gameplay-sandbox.md`

Four new CTest entries use the synthetic City-v12 test executable: `sandbox-city-v12-base`, `sandbox-city-v12-persistence`, `sandbox-city-v12-determinism`, and `sandbox-city-v12-endurance`. Existing menu, road and SDL-view tests gained City-v12 cases. No proprietary fixtures or decoded assets were added.
