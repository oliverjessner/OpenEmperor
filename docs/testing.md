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

## House graphics and paid-starter presentation (OE58/OE59)

Keep the [human report](qa/oe58-human-playtest.md) separate from software-SDL
results. The [OE59 contract](rendering/oe59-original-vacant-houses.md) binds the
pinned fresh Common purchase to group `0x47a`, variant0, physical1508. Its native
158×88 image uses the existing Type30 base12800 and front-cell anchor79,68.
Selection requires the actual2×2 Common body, exact empty Vacant2 state and a
matching Housing row with no first admission; the renderer adds no saved
authority. The
[OE58 authored foundation](rendering/oe58-vacant-house-visual.md) remains the
fallback for unavailable artwork, unsupported lifecycle states and F4.
Ordinary setter group `0x471` and Shelter3's1509 image are not Type2 substitutes.
Use the actual production View, lifecycle and menu tests:

```sh
ctest --test-dir build --output-on-failure -R 'original-(house-visual|house-inspector|starter-availability-menu|player-mode|menu)|native-input|map-rules|map-permissions|save|recovery'
ctest --test-dir build --output-on-failure -R '^original-(house-evolution-(synthetic|real)|housing(-stages)?-(world|world-real|save)|house-inspection(-real)?|building-visual-profile(-real)?|building-paid-production-view)$'
OPENEMPEROR_UI_CAPTURE_DIR="$PWD/.local/oe59/house-captures-01" ctest --test-dir build --output-on-failure -R '^original-house-visual-production-view$'
./build/openemperor-original-menu-session-tests --starter-availability-capture .local/oe58/menu-captures-01
./build/openemperor-original-menu-session-tests --starter-availability-real /path/to/emperor .local/oe58/menu-real-01
```

Configure the real-data tests with
`-DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=/path/to/emperor` and the normal copied
application resources. Choose fresh output roots and preserve the original
inputs. The House profile checks all16 native House entries, strict Type2/Shelter
registration separation, exact RGBA/alpha, eager deduplication, named fallbacks
and unchanged budgets. The complete core/House/Original-building profile now has
56 physical textures and4,770,512 RGBA bytes, including the two OE60 farm bodies
and the OE60.5 field stages; the shared Well and Millet-stage5 image still avoid
duplicate uploads. Historical schema1/2 supplements retain their original
ten-/twelve-body selection and field fallbacks.
The paid View checks the four-house Xia/Normal starter, the actual fifth-House
purchase and preview, real Space-driven admission to Shelter3, complete F5/F9
state/byte equality, rejection of a forged formerly occupied Vacant state,
native alpha picking, foreground/camera clipping and pure render/hover queries.
Record their actual results for the tested source before claiming acceptance.

The authored menu case separates map admission from failed layout placement, retains complete
previous World/files, cancels focus/layout-boundary presses, and verifies that
Empty cannot bypass failed map rules. Its real-data counterpart checks actual
Loulan refusal and explicit Empty startup at1280×800/1600×900/1920×1080, full
F5/F9 equality and the unchanged Xia paid starter. Open every new capture;
PNG creation alone is not visual acceptance. Native human, external-display,
clean-Mac and distribution checks retain separate results. Run the full Release
suite, focused Debug/sanitizers and complete historical save comparisons before
accepting a presentation change.
Original binary-save graphics, original runtime variation and whole-scene
parity remain separate from this fresh-purchase binding; no original EXE
execution or native human acceptance follows from the scripted tests.

## Production evidence queries (OE55)

The existing read-only model inspector accepts repeated sparse Building IDs:

```sh
./build/openemperor-model-inspect /path/to/emperor --building 193 --building 195
ctest --test-dir build --output-on-failure -R '^original-model-inspector-cli$'
```

Queries report raw labels and Cost/Employees/Resource columns with the complete
source fingerprint. They do not infer commodity codes, recipes, footprints,
field ownership, periods or World admission. Missing rows reject the entire
query; invalid/duplicate IDs fail before loading, and unrepresentable UTF-8
observations fail without partial JSON. With no query, the provenance report is
unchanged. The CLI test uses independently authored sparse input only. The
[production gap register](original-economy/oe55-production.md) separates current
paid adapters from unsupported additional chains; research is not paid gameplay
acceptance. Run the complete suite and relevant Debug/sanitizer cases as well.

## Physical Theatre-to-House Music (OE57)

Run `original-venue-house-service-world`, `original-venue-house-service-save`,
`original-venue-house-service-menu`, `original-venue-house-service-view` and
`original-venue-house-service-cli` beside all Rule22 tests, the full Release suite and focused Debug/ASan/UBSan
service, housing, model, input, persistence and historical tests. The authored
fixtures earn residents and staffing through paid commands and ordinary ticks.
Check four eligible full-staff callbacks after real Music input and after real
return, distinct lifetime/channel identities, actual House96 assignment,
current-input expiry/refusal, source pause/staff loss/natural fire, complete
footprints, road cut/begun-edge/wait/paid repair and real return/retirement.

Schema39 negatives include exact generation/markers, first and current Music
proofs, compacted minimum travel, per-source/global identity/sequence order,
shared simultaneous model workforce, actual fire timelines, forged arrivals,
active routes, all embedded House evaluation histories, funds and live model/map
identity. Refusal preserves the prior World and canonical file bytes. Never
weaken older schema admission or compare only a digest summary.

The new Save binary supports separate `--create OUTPUT [TICKS]` and
`--restore INPUT OUTPUT [TICKS]` processes, only under fresh `.local/oe57/` paths.
Compare whole canonical SaveDocuments at0/103 ticks. The paid Xia integration
must additionally earn at least five occupied Houses, two School→Theatre inputs,
a completed physical Theatre→House→Theatre lifecycle and at least20400 actual
continuation ticks. Preserve independent full Creator/Restore and Recovery/F5/F9
bytes, Rule22's actual-source regression, schemas1–38 and Rule18 Golden City.

Open every new scripted-SDL capture:1280×800,1600×900,1920×1080 at1×/2×/4×,
actual outward/returning actor foot and picking, source and House-Visits Inspectors,
menu controls, scrolling, clipping and explicit authored fallback presentation.
One hundred pure frames must retain the twelve authority/preparation counters
at zero. Software SDL is separate from native human acceptance and complete
Original parity. The [OE57 assessment](original-economy/oe57-venue-house-service.md)
records actual runs, counts and remaining limits; a finite paid sample is not
continuous supply, sustainable profit or Original roaming parity.

## Music performers and Theatre venues (OE56)

Run `original-music-venue-world`, `original-music-venue-save`,
`original-music-venue-menu` and `original-music-venue-view` beside the complete
Release suite and relevant historical culture, service, housing and model tests.
The new tests use authored source inputs and real paid World commands. Check
attempt versus actual spawn, unique Performer IDs, one-way movement/retirement,
actual venue arrival and 32-duration expiry, conservation of money, and the
absence of a House Music96 effect. Keep historical Rule18 Golden City and
schemas 1–37 exact; new markers or culture state must not enter older saves.
Include nonzero signed Music School model-DES fields in complete restoration
checks. The School's inherited physical influence must survive culture
ownership; Theatre75 is not a new DES source or a House Music visit.

The Save target supports independent `--create OUTPUT [TICKS]` and
`--restore INPUT OUTPUT [TICKS]` processes with fresh `.local/oe56/` paths.
Compare the entire canonical documents at zero and 103 continuation ticks.
The registered Save test separately covers active, arrived and expired state,
source binding and Recovery.
The [culture/venue contract](original-economy/oe56-culture-venues.md#reusable-validation)
gives commands and refusal expectations. Open the MenuSession/SandboxView PNGs:
inspect the actual actor foot/direction and selected identity, complete 4×4
claims, aperture/Inspector clipping and authored fallback labels at the tested
window sizes and 1×/2×/4×. An owned RGBA target is read before Present. Confirm
pure frames keep all twelve preparation/authority counters at zero. Scripted
SDL, native delivery, human acceptance and complete Original parity remain
separate gates; the new native/human checks are NOT_RUN. Repeat focused Debug
and ASan/UBSan checks without changing old timeouts or tolerances.

## Authored finite land trade (OE54)

Run `authored-land-trade-world`, `authored-land-trade-save`,
`authored-land-trade-menu` and `authored-land-trade-view` alongside the complete
suite. Fixtures buy roads, two Warehouses and production using actual commands;
they do not inject stocks or grants. The World test audits every transaction
phase, paid own-production exports, prepaid imports, finite offers, headroom,
road cuts/repair and hostile restores. Save CLI `--create OUTPUT [TICKS]` and
`--restore INPUT OUTPUT [TICKS]` use fresh ignored `.local/oe54/` outputs for
independent complete-document comparisons. Menu/View write owned software
captures; open them and distinguish scripted SDL from native/human acceptance.
See the [OE54 contract](original-economy/oe54-trade.md).

## Authored source-cell ramps (OE53)

Run `source-cell-ramp-world`, `source-cell-ramp-save`, `source-cell-ramp-menu`,
`source-cell-ramp-rendering` and `source-cell-ramp-view` beside the full suite.
The World/Save tests use paid authored inputs; the actual SandboxView test
prepares four signed/oriented original-format authored maps and reserves a new
private `.local/oe53/view-*` directory. It writes36 software PNGs through an
owned RGBA target before Present. Open those captures; synthetic pixel checks
and SDL events do not establish native or human acceptance. The Save test
provides separate `--create OUTPUT [TICKS]` and `--restore INPUT OUTPUT [TICKS]` modes
under fresh `.local/oe53/` paths for full-document continuation comparisons.
See the [OE53 contract](rendering/oe53-terrain-integration.md).

## Original building visuals and farm/field boundaries (OE52/OE60/OE60.5)

`original-building-visual-profile` checks authored inputs, atomic source/metadata
refusal, native Type30 geometry and deduplicated budgets. Schema1 retains the
historical ten native roles; schema2 explicitly selects twelve native bodies
and three fallback-only fields. Schema3 adds the exact authored selector
`authored-farm-progress-six-canonical-0-v1` and six ordered original images per
field role. Check all stage boundaries, invalid progress, unknown schemas,
missing/duplicate roles, mixed native/fallback keys, historical native-field
refusal, wrong crop/offset/geometry, animated payload refusal, incomplete last
payload, the two farm registrations, shared physical-image reuse and the exact
combined budget boundary. Static lookup never implicitly selects a field stage.
Its `-real` companion
uses the explicitly configured `OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT` and the
actual copied application resources. `original-building-selection-wiring`
checks automatic/custom/fingerprint precedence without claiming pixel acceptance.

Set `OPENEMPEROR_OE52_PAID_TEST_SAVE` to an explicitly supplied earned
Rule18/schema34/policy3 Xia save to register
`original-building-paid-production-view`. The complete66-body/46-courier city is
required; there are no original-data fixtures in Git. The test reserves a new
private output below `.local/oe60-5/` for each invocation (explicit historical
`.local/oe60/` and `.local/oe52/` output parents remain accepted), checks full F5/F9 bytes,
126 size/zoom cases, ordinary pointer selection, F4 and resource/authority guards.
The matrix covers twelve native bodies and the two field kinds actually bought
in that immutable city, at1280×800/1600×900/1920×1080 and1×/2×/4×. It captures37
PNGs with owned-target readback before Present. Open the images;
its `report.json` deliberately leaves visual review and native/human acceptance
unconfirmed. See the historical [OE52 assessment](rendering/oe52-original-visuals.md)
and the current [OE60 contract](rendering/oe60-original-farm-visuals.md).

```sh
ctest --test-dir build --output-on-failure -R '^original-(building-(visual-profile(-real)?|selection-wiring|paid-production-view)|field-paid-growth-view|production-(world|view|save)|food-(world|save)|hemp-(world|save)|house-visual-production-view)$'
```

`original-field-paid-growth-view` needs only the configured original-data root,
not the earned-city fixture. It retains the normally paid Xia/Normal Rule18
starter, buys a second Food Farmhouse, Wheat field and worker Houses through
normal commands, and observes all80 existing parent progress values for all
three crops. Its162 size/zoom/stage cases use owned-target readback;18 representative
PNGs show every crop/stage at1280×800/4× with the actual authored-stage Inspector
details. Check exact source pixels, ordinary opaque/transparent pointer probes, F4, ten real
ticks of a normally paused Wheat Farm, resumption, actual harvest, complete
F5/F9 equality and all twelve pure-frame counters. Open the images separately.
The original per-field quantity, variation and calendar/work state remain
outside this explicitly authored six-stage mapping.

Adjacent-field order checks must derive projected positions independently from
the actual paid Field cell, source height and complete Farm footprint. Require
real source-alpha overlap witnesses that the old front-cell painter overwrote,
then verify the Field pixel at1×/2×/4×. Ordinary clicks must retain ground-owner
selection; explicit F1 diagnostics with a fresh submitted frame must pick the
visible Field alpha at the same overlap point. Earlier ordinary pointer probes
alone do not establish that F1 result. Do not derive expected
positions from `original_building_visual_diagnostic().image_origin` or merely
search for any still-visible pixel. The synthetic building-depth cases also
protect front neighbors and preserve historical keys when all four neighbor
constraints cannot be satisfied. Check that preparation follows placement,
layout revision and F9 without per-frame neighbor scans or new asset work.

Additionally compare the existing longer paid Farm exercise, including loading,
arrival and natural source fire. Farm bodies must remain825/793, while fields
must use only their actual paid parent's progress. Compare full Funds, IDs,
workers, progress, stock, cargo, reservations,
conservation and SaveDocument bytes against a freshly built previous engine.
Use actual F5/F9 and a separately launched restore process. Full Release,
focused Debug/ASan/UBSan, historical Rules1–23/schemas1–39, Rule18 Golden City
and Rule22/23 culture checks remain required. Native/human acceptance is separate.

## Rule19 safe demolition

The explicit Rule19/schema35 slice has focused tests
`original-safe-demolition-world`, `original-safe-demolition-save`,
`original-safe-demolition-menu` and `original-safe-demolition-view`. Run them
alongside the complete historical suite; Rule18 saves remain schema34 and never
acquire a retirement journal. The view test accepts `--capture NEW_DIR` for
actual scripted SDL PNGs at 1×/2×/4×; open the generated images. Scripted event
delivery does not establish native or human acceptance.

`openemperor-original-safe-demolition-save-tests --create
.local/oe51/NEW/creator.json` earns a paid, drained Clay removal and an empty
House removal. In separate processes use `--restore INPUT NEW_OUTPUT [TICKS]`
to check complete canonical bytes at zero ticks and independent continuation.
Inputs and outputs must be explicit private paths; existing outputs are refused.
See the [OE51 contract and measured evidence](gameplay/oe51-demolition.md) for
the bounded type matrix, refusal cases and required long-window comparison.

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
