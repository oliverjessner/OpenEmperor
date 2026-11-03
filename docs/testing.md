# Testing OpenEmperor

Use the current source and your own original game files. Record the source
revision, dirty-worktree status, map, profile, rule version and save schema with
each result. A display version alone does not identify the behavior tested.
OpenEmperor saves are JSON documents; original Emperor savegames are unsupported.

## Build and automated checks

From the repository root, with the dependencies described in the
[build guide](../README.md):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For the complete macOS arm64 validation pipeline:

```sh
python3 tools/alpha_check.py --system-sdl --jobs 2
# Include the optional read-only original-data check and package validation:
python3 tools/alpha_check.py --system-sdl --jobs 2 --package --data /path/to/emperor
```

The default build directories are `build/`, `build-alpha-release/` and
`build-sanitize/`. Build, report and package destinations must be new paths;
existing outputs are preserved. Select isolated paths with `--build-dir`,
`--release-build-dir`, `--sanitize-build-dir`, `--report`,
`--package-build-dir` and `--package-dist-dir`. See the
[packaging guide](macos-packaging.md#alpha-acceptance) for a complete command.
The default report is `.local/reports/alpha-check.json`. The runner records the
actual registered-test inventory and JUnit results. It runs the registered tests,
100,000-tick deterministic endurance with real JSON roundtrips, render stress
and repeated menu/session lifecycles. The optional data check verifies the
original files it actually uses remain unchanged. On macOS LeakSanitizer is
disabled; project-owned texture counters provide a separate lifetime check.

Feature-specific original-data checks are additional evidence. Use the runner
documented with the affected feature, for example
[Ramps](city-v16-ramps.md), [Bridges](city-v16-bridges.md),
[Service coverage](city-v16-service-coverage.md) or
[joined ramp rendering](rendering/multi-ramp-integration.md). A finite scripted
scenario does not prove every layout, long-play outcome or original-game parity.

## Rule16, Rule17 and Rule18 dispatch experiments

OE48 adds legal paid planning controls and fire inspection to this QA launcher,
without a new engine generation. Select `--rule 18 --diagnosis oe48 --variant
lean-med --paid-strategy oe48-watch-phase1-third-food-three-markets` for the
measured plan. Every output must be a fresh strict descendant of ignored
`.local/original-economy/oe48/`; preserve the original installation and earlier
proofs read-only. Create and independently resume from the complete seed using
`--expected` for the creator result and `--tick-digests sha256` in both processes.
The full creator digest CSV must be available beside the expected result when
the replay compares its endpoint. Render the earned saves using explicit
`--mode render --rule 18 --diagnosis oe48 --resources ABS`, then actually inspect
the PNGs. The [OE48 assessment](original-economy/oe48-citywide-fire-reliability.md)
separates full-state/byte equality, actual fire/Food events, scripted software
SDL and native human acceptance. Historical test fixtures with source-relative
private paths require a contained authored source mirror when preserving an
earlier proof tree; compile and run there, with an explicit read-only data root.

OE47 adds the separately selected Rule18/schema34 Food contract. Run
`original-deadline-food-world`, `original-deadline-food-save` and
`original-deadline-food-menu` alongside the historical dispatch/save/menu/CLI
tests below. Its actual paid creator/restore/view checks use the same launcher
with explicit `--rule 18 --diagnosis oe47`, a fresh strict descendant of ignored
`.local/original-economy/oe47/`, and the exact selected paid strategy. The
[OE47 assessment](original-economy/oe47-food-reliability.md) records all controls,
actual deliveries, per-House tradeoffs and the distinct acceptance statuses.
Historical modes retain their original output boundaries and generation checks.

OE46 adds the separately selected Rule17/schema33 service contract. Its focused
tests are `original-deadline-service-world`, `original-deadline-service-save`,
`original-deadline-service-menu`, `original-menu-session-flow`,
`original-menu-successor-flow`, `original-economy-cli` and
`original-elite-desirability-guards`, alongside all historical Rule16 tests below.
The [OE46 assessment](original-economy/oe46-citywide-reliability.md) retains the
actual paid windows, complete independent restore/digest checks, conservation,
production SandboxView captures and separately labeled human acceptance.

Rule16/Policy3/Schema32 is an explicit authored experiment, selected through
**D Deadline-aware fire dispatch** or `--original-deadline-fire-dispatch` in the
normal application. It has separate create/restore APIs and exact marker
`authored-deadline-fire-dispatch-v1`; its inherited market/cardinal markers and
live source-bound model/terrain remain mandatory. Rules1–15, general defaults,
physical movement/fire times, model values, costs and limits stay exact. Only
real Inspector arrivals grant protection. The separate paid gameplay check in
the [OE45 assessment](original-economy/oe45-fire-coverage.md) holds Elite17 for
20400ticks with400/400 current needs, zero target fire/devolution/vacancy and
positive operating cashflow; this finite result does not follow from unit tests.

With a configured build, the focused contract/save/menu/CLI checks are:

```sh
cmake --build build --parallel 2 --target \
  openemperor openemperor-elite-desirability-acceptance \
  openemperor-deadline-service-world-tests openemperor-deadline-service-save-tests \
  openemperor-deadline-inspector-world-tests openemperor-deadline-inspector-save-tests \
  openemperor-inspector-coordination-world-tests openemperor-inspector-coordination-save-tests \
  openemperor-original-economy-rule-gate-tests openemperor-original-menu-session-tests
ctest --test-dir build --output-on-failure -R '^(original-deadline-service-world|original-deadline-service-save|original-deadline-service-menu|original-deadline-inspector-world|original-deadline-inspector-save|original-inspector-coordination-world|original-inspector-coordination-save|original-economy-rule-gate|original-menu-session-flow|original-menu-successor-flow|original-economy-cli|original-elite-desirability-guards)$'
```

Repeat the relevant suite in independent Release, Debug and ASan/UBSan builds
and retain their actual source/build identities. New-generation roundtrips do
not replace historical canonical-byte checks, independent creator/restore
processes, road-cut/repair and begun-edge checks, source-fire/pause cases or
hostile save/accounting/route refusals.

The paid source-integration runner requires the pinned Xia/Normal installation
contract; it rejects different source identities instead of substituting data.
Each output below must be a fresh strict descendant of ignored
`.local/original-economy/oe45/`, disjoint from the original installation. Choose
a new example label on every repetition. Commands perform ordinary paid
purchases and World ticks; they never inject a save snapshot or grant stock.

```sh
binary="$PWD/build/openemperor-elite-desirability-acceptance"
data="/path/to/data"
proof="$PWD/.local/original-economy/oe45/example-01"

"$binary" --mode create --rule 16 --diagnosis oe45 --tick-digests sha256 \
  --variant lean-med --paid-strategy oe45-grid-watch-near --search-ticks 40000 \
  --data "$data" --output "$proof/creator"

"$binary" --mode resume --rule 16 --diagnosis oe45 --tick-digests sha256 \
  --variant lean-med --paid-strategy oe45-grid-watch-near \
  --data "$data" --input "$proof/creator/seed.save.json" \
  --expected "$proof/creator/result.save.json" --output "$proof/replay"

"$binary" --mode render --rule 16 --diagnosis oe45 --variant lean-med \
  --data "$data" --resources "$PWD/resources" \
  --input "$proof/creator/result.save.json" --output "$proof/render-final"
```

The creator records an actual paid 16→17 transition only if it occurs; then it
observes exactly 20,400 further ticks and 400 actual phase39 inputs. A failed
search/window stays failed. The independent resume compares every in-range
complete creator checkpoint and final canonical SaveDocument bytes, with
Recovery checked separately.

The optional `--tick-digests sha256` flag shown on create and resume is restricted
to explicit OE45 Rule15/16 simulation. It records the initial state and every
ordinary post-tick state as a SHA-256 digest of complete validated saved authority,
including the full fire journal. Instrumentation stays in the external QA runner;
there is no normal World tick/frame hook. With the flag selected, resume requires
the creator's bounded digest CSV, consecutive ticks and an exact complete CSV
comparison over the inclusive restored-begin through replay-end subrange. A
missing tick, malformed row, mismatch or exhausted digest budget fails the check.
Creator and replay may start at different ticks; every state in the observed
subrange must match. `oe45-state-digests.json` records the enforced comparison
beside `oe45-state-digests.csv`. This per-tick digest gate remains separate from
the full checkpoint/Recovery SaveDocument-byte gates and gameplay outcomes.

The deadline world test also includes a fully paid return-blocking regression:
one earlier-deadline short visit and real return allow the longer visit to
arrive before its fire expires. It observes both actual arrivals and an exact
restored continuation, while historical Rule15 keeps its original order.
Rules13–15 retain their existing contracts; this bounded regression does not
prove stable Elite17 retention or universal scheduling fairness.

Render uses the actual saved state and production SandboxView, including the
bounded sizes/zoom/inspector/F5/F9 matrix. If a genuine
`first-success.save.json` was produced, render that file to another fresh output
to inspect entry separately. Open the PNGs before claiming visual acceptance;
scripted software SDL remains separate from native macOS delivery and human play.

Read `report.json`, `retention-metrics.json`, `city-metrics.json`,
`transport-metrics.json`, `oe45-fire-coverage.json` and `oe45-outcome.json`
together. Keep entry, actual fire duration, current needs, occupancy,
devolutions, goods/Food conservation, operating tax minus all installed upkeep,
minimum Funds and effects on the other twelve homes separate. Dispatches,
reserved cargo and predicted arrival times are not delivered protection,
goods or service. A finite class-neutral selection test does not establish
universal fairness or a successful city retention window.

## Original-map start checks

For a bounded original-map start and paid transport fixtures:

```sh
sh tools/test_map_playability.sh /path/to/emperor "Chang-an Zhou"
sh tools/test_map_playability.sh /path/to/emperor Handan
sh tools/test_map_playability.sh /path/to/emperor Badaling
```

Run one at a time. The launcher builds the current Release app and creates a
fresh ignored test root. New Sandbox uses City-v16 rule 4/policy 2 and Empty
City: scenery with zero Sandbox buildings, residents or walkers is expected.
Its optional A–E technical saves retain historical rule 3/policy 1. Load
`B-paid-courier` and press Space for a real Clay trip, or `C-paid-supply` for
the paid compact starter. An unavailable exact map or legal fixture reports
its preparation error rather than substituting a fabricated map. These finite
checks establish technical behavior, not full map or human acceptance; use
[road diagnostics](road-connectivity-diagnostics.md) to inspect a concrete route.

## Inspector layout regression

The registered `inspector-layout` test checks the shared layout at 1280×800,
1600×900 and 1920×1080 with UI scales 1 and 2, both document orders, large
lists, long wrapped hints and small resize bounds. `sandbox-software-view`
exercises actual production row selection, clipping, scroll limits and press
cancellation with a 34-building World. Run these alongside the existing
`sandbox-input-reliability`, `original-house-inspector-view`, operation/demolition,
modal and save-view checks when changing inspector geometry.

For the optional private OE41 reproduction, build
`openemperor-system-playtest-acceptance` and run its `--inspector-layout` mode
with DATA, RESOURCES, a fresh app root, a fresh capture directory and the explicit
paid checkpoint. Both output paths must be below `.local/oe42/`. Accepted inputs
are the existing `.local/oe41/run-01/journey-create-02/journey.save.json`
(Rule14/Xia/tick901, Market7) and the documented
`.local/original-economy/oe40/run-01/gate-c-release-lean-med-02/first-success.save.json`
(tick7333, 65 paid buildings including FireWatch). The diagnostic loads a working
copy through MenuSession; original input bytes remain protected. For example:

```sh
cmake --build build --target openemperor-system-playtest-acceptance
./build/openemperor-system-playtest-acceptance --inspector-layout \
  /path/to/emperor "$PWD/resources" \
  "$PWD/.local/oe42/check-app" "$PWD/.local/oe42/check-captures" \
  "$PWD/.local/oe41/run-01/journey-create-02/journey.save.json"
```

The matrix uses actual SDL window resizes, camera zooms 1/2/4, ordinary list/body
selection and wheel events in production SandboxView. It checks complete row
reachability and World equality, compares permitted operation clicks with the
same commands on an independent reference World, then restores the full saved
checkpoint. F5/Step/F9/F5 must reproduce canonical document bytes. Open the
captured top/end views and review text, fixed controls and hints before recording
visual acceptance. Scripted SDL/software evidence does not complete the separate
[human input checklist](testing-input-acceptance.md).

## Manual play and persistence

Use a separate test root so the exercise leaves personal settings and saves alone:

```sh
./build/openemperor --data /path/to/emperor --app-root "$PWD/.local/manual-test"
```

1. Create a supported city through the normal menu. Record the chosen profile
   and rules. A prepared starter must spend its normal budget without free goods,
   extra funds or unexplained simulation progress.
2. Observe actual deliveries, Service arrival and fully supplied House demand.
   Check treasury changes against construction, tax and maintenance rather than
   treating a moving walker or current stock count as proof of complete supply.
3. Expand using earned funds. Pause/resume a producer and change a permitted
   workforce priority; inspect workers, stocks, progress and active trips.
4. Save a test copy, remove a future road segment, observe safe waiting or
   rerouting, and repair it. Cargo and reservations must remain coherent.
5. F5 save, quit the whole application, restart the same build and load the save.
   It must open paused with the saved tick, funds, buildings, goods, population,
   operations, identities and active transport intact, then continue normally.

For safe demolition, use a disposable City-v11 rule-4 save. An older rule-3 save
requires **Load Sandbox → Enable demolition in a copy**; ordinary loading must
retain the old rule and leave its source bytes unchanged. Cancel Demolish once,
then remove an empty building and rebuild with a new ID. House removal must
remove its residents/workers. A stocked Warehouse must remain blocked: stop
upstream production, drain it through ordinary delivery/demand and wait for
trips to return before removal. Repeat with a drained Market, then save, fully
restart and verify deleted IDs stay absent. Demolition gives no refund.

The detailed [human input checklist](testing-input-acceptance.md) covers camera,
zoom, picking, drag cancellation, focus/layout/modal boundaries and Service
overlays. Keep its human results separate from queued SDL events and software
pixel checks. Report **NOT RUN**, **PASS** or **FAIL** for the observed scope.

## Packages and failure reports

`tools/package_macos.sh` builds and checks the local development app; see
[macOS packaging](macos-packaging.md) for dependency, relocation, signature and
resource rules. A local package check is not a published release, notarization,
Finder/Gatekeeper acceptance or an independent clean-Mac test.

Report exact steps, expected/actual behavior, the recorded version domains and
terminal errors. An affected OpenEmperor JSON save can make a failure reproducible.
Never attach original maps, SG3/.555 files, installers or decoded game images.
Review [known issues](../KNOWN_ISSUES.md) before interpreting a new result.
