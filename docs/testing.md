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

## Agriculture reconfiguration (AGRI-MAC11)

Select **R Reconfigure (36)** / `--agriculture-reconfiguration` for explicit
Rule36/policy3/schema52. The [support/drain/refusal matrix](original-economy/agriculture.md#safe-agriculture-reconfiguration-agri-mac11)
retains the sole Food/Goods/Aux accounts, immutable paid leaf histories and all
lifetime limits. This is authored removal, separate from Rule19.

```sh
ctest --test-dir build --output-on-failure -R 'agriculture-reconfiguration|original-safe-demolition|inspector-layout'
python3 tests/test_agriculture_reconfiguration_replay.py \
  --executable "$PWD/build/openemperor-agriculture-reconfiguration-acceptance" \
  --seed-executable "$PWD/build/openemperor-food-warehouses-acceptance" \
  --data "$PWD/.local/gog-extracted/app"
```

The independent Python case first earns a fresh actual-data seed command plan,
then repurchases its commands into a fresh Rule36 city. It never converts that
seed's Save or imports work/population/stock. Separate creator/replay processes
retain their own 600-second deadlines; the outer CTest case has 1860 seconds.
Outputs use fresh ignored directories. Each actual tick is checked for the
single twelve-kind Food equation, physical cargo/returns, paid budgets and
closed histories. Checkpoints cover idle and worked Field removal/rebuild,
partial/loaded pending Growers, a real Tea pickup/return, Pump/ditch cuts and
wet recovery, plus loaded Hunter/Boat/Food, active Mill and stocked Warehouse
refusals. `initial_work` must equal the real command-time observation and remain
immutable. Creator/replay traces and final canonical bytes must match.

World/Edge cases also earn a fully held32-portion Warehouse, an unrelated actor
crossing a candidate's current/begun footprint, real paid road repair during a
Field drain and normal resumption of paused loaded sources. Save/Recovery cases
cover pending/loaded/returned/closed/rebuilt states, shared labor, known wet
supply intervals, backdated requests/rebuilds, source-blocked closed geometry,
paired nullable closure fields, clipped-work forgery, closed Pump invoices/fire
and actual Inspector commitments. Rejected commands, restores and writes must
preserve live state, funds, IDs and supplied bytes. Historical51 writers/loaders
remain exact and cannot overwrite52.

`agriculture-reconfiguration-presentation` exercises normal R/pointer selection,
shared Drain/confirmation/cancellation, genuine loaded Growers, no-refund paid
rebuild, F5/F9 and pure frames at900/1280 widths. The real-window companion is
`openemperor-agriculture-reconfiguration-view-acceptance`: use `--input` with an
actual schema52 checkpoint and `--target` with its paid ID, fresh `--output`,
`--renderer metal`, and optional36-only `--width 900 --height 600`. It validates
normal selection and visible scrolled cargo witnesses rather than treating JSON
or screenshot existence as visual acceptance. Inspect the PNGs. Cocoa/Metal
render delivery, scripted SDL events and human/OS-input acceptance stay separate.

The frozen macOS arm64 source (891 files) passed all 425 cases in each fresh
Debug, Release and ASan+UBSan build, without skips. Full-data alpha passed
100,000 deterministic ticks and 12 real JSON roundtrips per configuration,
3,000-frame Debug/sanitizer render stress and 100 menu/session cycles. All 74
core comparisons passed against each matching fresh archive set; the numeric
cases and literal public report retain 46 parity, 18 corrected_bug, 6 unverified
and 4 unsupported classifications. This does not widen Original parity.

The actual Rule36 creator and separate replay each ran 24,000 ticks with 20
canonical checkpoints and identical trace/final bytes; their independent
real-data case passed in all three full suites. Fresh historical Rules24–35
comparisons preserved 246 artifacts, 137 paid saves and 28 actual F5/Recovery
byte comparisons. Older rule families retain their registered compatibility
checks. Native Cocoa/Metal passed 17 cases including 900×600, with 188 reviewed
PNGs and 564 pure frames whose 12 counters stayed zero. All 36 native guarded
files and all 15 supplied Original inputs retained their bytes, mode and mtime.
Human/OS-input acceptance remains NOT_RUN until the planned post-MAC14 playtest;
isolated native fire/zero-staff cases are also NOT_RUN, with staffing/fire
behavior covered separately by World tests.

## Food Warehouses (AGRI-MAC10)

Select **V Food Warehouses (35)** / `--food-warehouses` for explicit
Rule35/policy3/schema51. The [contract](original-economy/agriculture.md#food-warehouses-agri-mac10)
keeps the existing Warehouse construction and Goods/Aux account; only this
generation admits its additional bounded compartment in the sole Food ledger.

```sh
ctest --test-dir build --output-on-failure -R 'food-warehouses|food-processing'
./build/openemperor-food-warehouses-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --output "$PWD/.local/agri-mac10/paid-new" --ticks 24000
./build/openemperor-food-warehouses-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --replay "$PWD/.local/agri-mac10/paid-new" \
  --output "$PWD/.local/agri-mac10/replay-new" --ticks 24000
```

Use fresh output directories. `original-food-warehouses-real` independently
audits the creator and separate replay: actual Farm/Quay/Tent/Mill arrivals,
three simultaneously held raw kinds, Warehouse-only input to a fresh Mill and
Market, meal forwarding through a FoodShop and actual House consumption.
The full tick stream binds first receipts to observed accepted arrivals,
one-time work/debits, capacities32/per-kind4, cargo, return room and82/58 limits.
It checks a real loaded cut, paid repair and exact physical return; each child
retains600 seconds and the outer case1260 seconds. No inventory is injected.

World/Edge tests additionally earn a fully held32-portion compartment, genuine
zero staffing, natural fire and Food/Goods/Aux coexistence. Independent paid
Farm buffers avoid confusing a deliberately blocked producer with Warehouse
admission. Save/Recovery tests cover loaded outward/return phases, stock,
House consumption, malformed catalogs, impossible first-hop epochs, reused
transfer keys and protected historical50 targets. The normal UI tests cover
V/contextual legacy V, existing Warehouse construction, narrow scrolling,
actual cargo picking and F5/F9; the native tool is
`openemperor-food-warehouses-view-acceptance`. Inspect native screenshots and
pure-frame counters separately from synthetic SDL and human acceptance.

The frozen native arm64 source passed all 417 cases in each fresh Debug,
Release and ASan+UBSan build, without skips. Full-data alpha passed 100,000
deterministic ticks and 12 real JSON roundtrips per configuration, 3,000-frame
render stress in Debug and sanitizers, and 100 menu/session cycles. All 74 core
observations and public reports matched the merged baseline in every build.
The independent historical comparison preserved 205 artifacts and 110 paid
Rules24–34 saves byte-for-byte, including 15 actual Save/Recovery comparisons.
The actual Chengdu creator and separate replay matched 24,000 ticks each and
11 checkpoints: all four producer classes supplied the Warehouse, three raw
kinds were held simultaneously, and another physical trip supplied a fresh
Mill/Market or FoodShop before real raw/grade3 House consumption. The bounded
paid city used 26 bodies and 32 movement channels. Ten Cocoa/Metal checkpoint
cases passed exact F5/F9/Step and cargo picking, with 97 inspected captures and
291 pure measured frames; all instrumentation deltas were zero. Goods and Aux
rows were also visually checked after ordinary inspector scrolling. Original
inputs retained their bytes, modes and timestamps. Human/OS-input acceptance
and isolated native fire/zero-staff scenarios were not run; technical World
tests cover actual fire and zero staffing. Original reproduction is unproved.

## Food processing (AGRI-MAC09)

Select **Q Food processing (34)** / `--food-processing` for explicit
Rule34/policy3/schema50. The [contract](original-economy/agriculture.md#food-processing-agri-mac09)
uses one Food account for seven actual ingredients and grade-tagged Dinners28.
Test paid5×5 Mill/2×2 Shop footprints, source prices/staff, permanent parent
Market binding, four distinct-input recipes, one-time debits and actual work,
full capacities, zero staffing, pause/fire, loaded road cuts/returns, shared raw
and meal targeting, sole House consumption and strict causal receipt epochs.

```sh
ctest --test-dir build --output-on-failure -R 'food-processing|seasonal-agriculture|physical-food-growers|irrigation-productivity'
./build/openemperor-food-processing-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --output "$PWD/.local/agri-mac09/paid-new" --ticks 12000
./build/openemperor-food-processing-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --replay "$PWD/.local/agri-mac09/paid-new" \
  --output "$PWD/.local/agri-mac09/replay-new" --ticks 12000
```

Use fresh output directories. The registered `original-food-processing-real`
case launches the creator and replay as separate processes, independently audits
raw JSON conservation/recipe work, all seven actual origins and four consumed
meal grades, genuine loaded-route cuts, paid repair and the full12k tick stream.
Each child retains its600-second bound and the case1260 seconds. Original input
bytes, modes and timestamps must remain unchanged. The fixed-size first-input,
first-completion and per-grade Shop/House receipts must reject backdating,
wrong-grade first consumption and contradictory previous quality. Recovery and
cross-generation target protection are part of the Save test.

The normal UI presentation test covers paid pointer placement, menu/CLI
separation, recipe button/key agreement, actual loaded carrier picking and
F5/F9 during work/cargo/House consumption. Native Cocoa/Metal acceptance uses
`openemperor-food-processing-view-acceptance` with an exact schema50 checkpoint,
fresh private output and a declared phase; inspect its screenshots and pure
frame counters. Scripted SDL and native delivery remain separate evidence;
OS-pointer/human play acceptance and Original reproduction are not established.
Full fresh Debug/Release/ASan+UBSan, alpha endurance/session checks, historical
Rules24–33 byte comparison and all74 unchanged core observations remain required.

The native arm64 validation passed all 409 registered cases in each fresh Debug,
Release and ASan+UBSan build, without skips. The alpha matrix passed with real
Original inputs: 100,000 deterministic ticks and 12 JSON roundtrips per build,
3,000-frame render stress in Debug and sanitizers, and 100 menu/session cycles.
The paid 12,000-tick creator and independent replay matched all 11 checkpoints
and the complete trace; all seven ingredients reached the Mill and all four
meal grades reached actual House consumption. Historical comparison preserved
158 artifacts including 79 paid Rules24–33 saves byte-for-byte; all 74 core
observations and the public report matched in every build configuration.
Seven Cocoa/Metal checkpoint cases produced 28 inspected captures and 84 pure
frames with zero instrumentation deltas. Human/OS-input acceptance and separate
native fire/zero-staff scenarios were not run; technical World tests cover
natural fire and actual zero staffing. Original reproduction remains unproved.

## Irrigation productivity (AGRI-MAC08)

Select **X Irrigation yields (33)** or `--irrigation-productivity` for explicit
Rule33/policy3/schema49, marker `authored-irrigation-productivity-v1`, revision1.
The [contract](original-economy/agriculture.md#irrigation-productivity-agri-mac08)
separates actual base-work ticks from earned quarter bonuses and physical
harvest/return. Historical generations and source assets remain exact.

```sh
cmake -S . -B .local/agri-mac08/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=/absolute/path/to/your/game-data
cmake --build .local/agri-mac08/build-release --parallel 2
ctest --test-dir .local/agri-mac08/build-release --output-on-failure \
  -R 'irrigation-productivity|seasonal-agriculture|physical-food-growers|irrigation'
./.local/agri-mac08/build-release/openemperor-irrigation-productivity-acceptance \
  --data /absolute/path/to/your/game-data --map Cities/Chengdu.map \
  --output .local/agri-mac08/creator --ticks 24000
./.local/agri-mac08/build-release/openemperor-irrigation-productivity-acceptance \
  --data /absolute/path/to/your/game-data --map Cities/Chengdu.map \
  --output .local/agri-mac08/replay --replay .local/agri-mac08/creator
```

Use fresh output paths. The registered `original-irrigation-productivity-real`
case independently creates and replays24,000 actual ticks per process. Each
child retains its600-second bound; outer1260 covers both children and source,
JSON and streamed-tick verification. Its sanitizer run is serial, as are the
existing long paid multi-field and seasonal cases. The Python checker verifies
all nine actual crop types, every pre/post tick wet/base/bonus/cap delta, actual
staffing, physical cargo/returns, first readiness, seasonal expiry, all raw
checkpoints, paid fire recovery and phase-delayed network restart. It uses
**pre-tick** water, since phase27 and fire publish later in the existing tick.
Wet masks/providers and a second clock cannot enter the saved work authority.

The World tests cover matched paid dry/wet fields, all nine real return paths,
carry3 through dry/pause/year boundaries, a clipped fourth wet bonus at79,
unchanged five-tick Food harvest work, overlapping providers, actual zero
staffing, exact radius5/6, full Food bins/cart return reservations and blocked
loaded Grower repair. A natural Pump fire loses supply; a paid Watch must earn
an actual Inspector extinguish arrival before the next source water phase.
Save/Recovery cases cover wet/dry/reconnect, ready and Food/Hemp/material cargo,
uint64/counter/identity/epoch mutations, duplicate keys, impossible pre-Pump or
never-paid-ditch wet history, exact continuation and protected historical32
writes. Coherent hostile controls also move all nine Ready epochs before
possible water and mutate all four material witness types, including frozen
same-epoch base/loss drift. Presentation/CLI cases cover X, conflicts with historical selectors,
normal cached inspectors, scrolling, actual cargo picking and F5/F9/Step.
An actual Food return on a Road covers walker-owner inspection precedence;
Road and Ditch inspection cannot hide the selected worker's real cargo.

The unchanged Normal-model Chengdu creator and separate replay in
`.local/agri-mac08/{paid-02,replay-02}` passed24,000 ticks. All nine first ready
cycles used **64 wet base-work ticks versus80 dry**, with actual physical
source returns in both cohorts. The water system supplied23 paid ditch cells.
A real Pump fire at2000 was extinguished by actual Inspector arrival2010 after
paid Watch/road construction; refill occurred at2017. A loaded-state cut at6051
and paid reconnect6150 resumed supply at6199. Operation off6350/restart6400
waited until6403. Funds stayed at least6,686; the final paid city used29 bodies
and30 movement channels. No inventory, residents, model edits, increased
portions or source-admission shortcuts were used.

Terminal wet/dry source returns were Wheat67/61, Millet22/20, Rice19/21,
Cabbage30/30, Soy25/27, Hemp147/119 and each plantation40/40. Shared Farm target
selection and finite downstream bins can redistribute actual output; the
proof requires increased earned increments on the same fully staffed base-work
ticks and genuine returns, rather than universal terminal stock dominance.
The independent raw/stream audit passed80 checkpoints and48,000 paired trace
ticks. Both canonical endpoints reproduced exactly in the separate process:
dry `ca7b38ffd2df05e51a1b9fc4d7a70f566e3d76973a8d386895312a0e4efa0c92`,
wet `a11bbc8dfebe73f8b640c9371406f332d26a08570c36e02fdb7b7d229cde4b81`.
The full47.8MB paired tick stream is byte-identical before/after readiness
witness strengthening (SHA256 `614a4b1c2ec54016362b4e484e729cc73b5904571b6627502ae320d322166d08`).
Original bytes, modes and mtimes stayed unchanged. This finite result does not
claim universal throughput, uninterrupted fire coverage, profit or Original
runtime irrigation/yield parity. Human playtest follows MAC14.

The frozen fresh Release executable was also checked independently with Cocoa
and Metal against12 tightened schema49 checkpoints. All108 crop-inspector
cases, actual Pump/overlay states, F5/F9 and Step replay passed. There were153
pure observations/459 paused frames with every forbidden-work counter zero.
Food and Hemp marker pixel-picking passed on this actual paid layout, including
a loaded Food Grower on a Road during Pump fire. Bound plantation cargo remained
visible through the ordinary plant/source inspector; existing plantation-role
marker picking is explicitly `NOT_RUN`, not implied by that inspector.
The cut retained one supplied upstream ditch while all nine crop recipients
were actually dry. A real next work tick changed Lacquer base3→4, wet3→4,
bonus0→1, growth3→5 and carry3→0, with exact reload continuation.

Twenty-three individual final PNGs were opened, covering all nine types,
provider/fire/recovery, dry cut, cargo and overlays. Normal inspector wheel
scrolling exposes longer details;1440×1000 captures are readable and900×600
scrolling has separate presentation coverage. All861 frozen code/test/resource
files matched their recorded hashes. Fifteen selected native inputs and the
exact Release executable retained their bytes, modes and mtimes. The summary is
`.local/agri-mac08/view-verified-metal-summary-01.json`. Source/model/map inputs
and generated saves remain private under ignored `.local/`. Native OS-pointer
delivery and human acceptance are `NOT_RUN`; no Original parity is claimed.

Final fresh native arm64 validation on2026-11-21 is **PASS/full_data** in
`.local/agri-mac08/alpha-verified-check.json`: **401/401 Debug,401/401 Release
and401/401 ASan+UBSan**, zero failures/skips (157.606s,135.182s,2137.867s).
All three configurations complete100,000 deterministic endurance ticks with12
real JSON roundtrips each;3,000-frame Debug and sanitizer render checks have
zero decode/upload/tick deltas. All100 menu/session lifecycles and resource,
save/resume and input checks pass. Total pipeline walltime was4421.043s.
Debug and sanitizer retain assertions with the isolated `-g1 -O1` toolchain;
macOS LeakSanitizer is disabled and owned texture counters are checked
separately. Packaging was not requested. The15 separately guarded Original
files retain bytes/mode/mtime; the runner's own configured input checks pass.

Fresh Debug paid-save comparison against Git-verified merged MAC07
`9c456bd91bf40ea278e6a6995974be20b52c69ed` passes **138/138 byte-identical
artifacts /69 paid saves** for Rules24–32, including actual ready/work/cargo,
season boundaries, pause/return and exact restore. The preserved baseline
matches851 Git-source files and17 original archives; final current861 sources
and17 Debug archives remain unchanged around the probe. Fresh Debug, Release
and Sanitizer core comparisons each pass **74/74**, with public reports
literally equal to the maintained JSON:46 parity,18 corrected bugs,6 unverified
and4 unsupported. All historical numerical cases/classifications equal merged
main; only reviewed source/API fingerprints change. Reports are in
`.local/agri-mac08/run-01/historical-final-debug/` and
`core-parity-verified-{debug,release,sanitize}/`. These checks establish their
stated compatibility scope, independently of Original runtime or human parity.

## Seasonal Agriculture (AGRI-MAC07)

Select explicit **Rule32/policy3/schema48** with **Z Seasons (32)** or
`--seasonal-agriculture`. The [Agriculture contract](original-economy/agriculture.md#seasonal-agriculture-agri-mac07)
defines all nine windows, real Food harvest work, partial-cycle loss, ready
expiry and Hemp's physical worker. The calendar derives from the sole saved
completed World tick; cargo and inventory have no seasonal expiry.

```sh
cmake -S . -B .local/agri-mac07/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT="$PWD/.local/gog-extracted/app"
cmake --build .local/agri-mac07/build-release --parallel 4
ctest --test-dir .local/agri-mac07/build-release --output-on-failure \
  -R 'seasonal-agriculture'
.local/agri-mac07/build-release/openemperor-seasonal-agriculture-acceptance \
  --data "$PWD/.local/gog-extracted/app" --map Cities/Chengdu.map \
  --output "$PWD/.local/agri-mac07/paid-city-fresh" --ticks 80000
.local/agri-mac07/build-release/openemperor-seasonal-agriculture-acceptance \
  --data "$PWD/.local/gog-extracted/app" --map Cities/Chengdu.map \
  --output "$PWD/.local/agri-mac07/replay-fresh" \
  --replay "$PWD/.local/agri-mac07/paid-city-fresh/seed.save.json" \
  --expected "$PWD/.local/agri-mac07/paid-city-fresh/final.save.json"
```

Destinations must be fresh ignored children of `.local/agri-mac07`, disjoint
from supplied data and explicit replay inputs. The creator buys its city with
ordinary World commands. Reports distinguish earned growth, readiness, actual
harvest/pickup, returned cargo, Warehouse/Market arrivals and House consumption.
The separate process must reproduce its final canonical save exactly. The
80,000-tick supplied-model scenario includes genuine startup and at least five
consecutive complete mixed years afterwards; it does not require immediate
high-tier households. Every annual delta, physical source return and actual
current-row luxury debit is checked independently. Both children retain a
600-second deadline; CTest allows1260 seconds for their pair and verification.
The sanitizer case runs serially to preserve that CPU budget.

World tests use independent literal masks for all nine crops, paid mixed-city
production over60,000 ticks, five later complete years of real receipts and
Food consumption, winter scarcity, staggered fields and the exact first/last
eligible ticks. They cover pending ready crops, expiry, pause/fire/staff/access,
full sources, begun-edge drainage, repair and retained loaded cargo. Save tests
exercise all worker phases, physical carts, boundary/year roundtrips, Recovery,
12,000-tick continuation, hostile ready epochs/harvest clocks and atomic target
protection. Old generations must refuse appended seasonal DTO authority.

Presentation tests use the ordinary Z menu and actual paid construction,
normal cached all-nine inspectors, real Food/Hemp marker selection, F5/F9 and
paused frame purity. These scripted checks remain distinct from native Metal,
native OS pointer delivery and unassisted human acceptance.

The unchanged supplied-model Chengdu creator and separate replay completed
80,000 ticks with canonical final SHA-256
`a74e055816e84df379f7e93a8f731a0753149a1bab806308521d2688996801b4`.
The actual city uses55 paid bodies/56 channels and237 residents. Its minimum
Funds were689 and final Funds4,158; this is a finite positive-balance result,
not a profit or universal city-reliability claim. The80,000-tick duration
includes measured startup; years3–8 are six consecutive complete mixed years.
Each has actual returns from all nine contributors and actual current-row
Tea/Silk/Lacquerware consumption. No inventory, residents, yield, needs or
resource limits were changed to obtain the result.

| Contributor | Actual source returns |
|---|---:|
| Wheat |125|
| Millet |90|
| Rice |30|
| Cabbage |111|
| Soy → Bean Curd |149|
| Hemp |579|
| Tea |338|
| Lacquer |200|
| Mulberry |196|

Actual luxury debits are Tea303, Silk155 and Lacquerware121; material Warehouse
arrivals are579/338/200/196. The four-house plan uses three Common10 consumers
and one Elite15, two physically staffed Hemp sources and two paid Tea Bushes
on the same existing shed/channel. Its ordinary staged operation commands
start water/market access before the Farm, then continuous fishing, Hemp and
later chains only as real population covers the complete model labor. The
copied240-tick geometry probe grants nothing to the real tick0/population0
seed. All issued commands are independently repeated after restore.

Creator/replay reports and all34 exact checkpoints are under
`.local/agri-mac07/{paid-14,replay-14}/`. Independent Python checks validate
annual deltas/current-row events, sole Food balances and separate Hemp/plant
work, picked cargo and accepted-return equations. Stored goods and loaded
cargo survive seasonal boundaries; only unpicked field readiness expires.

The frozen isolated source-model test also passed under ASan+UBSan in
798.80 seconds for its complete creator/replay pair. Each child retained its
600-second deadline. The sanitized creator's canonical final SHA is identical
to the Debug creator above; the separate sanitized replay independently passed
the same exact endpoint, annual and accounting assertions. This preliminary
case is recorded in `.local/agri-mac07/run-01/dev-sanitize-real-80k.log` and
`.local/agri-mac07/positive-replay-_fh2ay6f/`; fresh full-matrix evidence is
reported separately below.

All three fresh primary core comparisons passed74/74 with literal-equal
public results and unchanged46 parity/18 corrected_bug/6 unverified/4 unsupported
classifications. Reports are in
`.local/agri-mac07/run-01/core-parity-verified-{debug,release,sanitize}/`.
The fresh Debug historical probe against Git-verified merged MAC06
`58a07238df08b4c3b5a38bbe8326a9e1a302e8fd` produced108/108 byte-identical
artifacts across54 paid Rules24–31 cases, including12 Rule31 worker/cargo/
cancellation checkpoints. All693 baseline archive files,689 corresponding old
code hashes and17 retained baseline archives were independently checked.
Evidence is `.local/agri-mac07/run-01/historical-final-debug/report.json`.

The frozen Release executable passed six scripted Cocoa/Metal runs: five
successful paid-14 Food/ready/boundary/Hemp/plantation checkpoints plus the
preserved paid-01 Tea-ready expiry. Each uses regular Z and all nine ordinary
inspectors, exact F5/F9/Step continuation and pure frames. Across59 observations/
177 paused frames every instrumented forbidden-work counter stayed0. Twenty
individual key PNGs were opened, including all nine species. The paid-14
boundary8975→8976 changes Cabbage growing true→false with progress/waste/expiry
already0; the additional paid-01 checkpoint actually changes Tea ready80→0,
expired0→1 at8976. No fabricated checkpoint or expiry claim is used.

Food marker pixel selection passed. The actual Hemp checkpoint remains
`OCCLUDED_OR_UNAVAILABLE` at1×/2×/4×; ordinary paid-source selection visibly
shows its real cargo1/1. Existing plantation marker selection is
`NOT_RUN_EXISTING_PLANTATION_ROLE`; bound-source inspection shows its actual
Tea cargo. Original inputs, six protected saves and the frozen executable
retained bytes/size/mode/mtime. Evidence is
`.local/agri-mac07/view-verified-metal-summary-01.json` and
`view-verified-metal-{food,ready,boundary,hemp,plantation,expiry}-01/`.
Native OS pointer delivery and human acceptance remain **NOT_RUN**.

Final fresh `tools/alpha_check.py` result is **PASS/full_data**:
`.local/agri-mac07/alpha-verified-check.json`. Native arm64 Debug, Release and
ASan+UBSan each completed **394/394 tests with zero failures and zero skips**
(147.893s /124.587s /1815.746s). All three100,000-tick deterministic
JSON-roundtrip endurance runs,3,000 renderframes,100 menu/session lifecycles,
resource lifetime checks and configured Original-data checks passed. Debug and
sanitizer retain assertions with the isolated `-g1 -O1` toolchain. macOS
LeakSanitizer remains disabled; project texture/resource counters are checked
separately. Packaging was not requested.

The851-file code/test/resource manifest stayed byteexact throughout the full
matrix and independent comparisons. The15 separately monitored supplied
Original files retained bytes, permissions and modification times; the runner's
own nine-file Original-data check also passed. See
`.local/agri-mac07/run-01/final-source-manifest.json` and
`input-guard-final.json`. These are technical/scripted results, with native
OS pointer and human acceptance still separate.

For native Cocoa/Metal inspection of an earned seasonal checkpoint:

```sh
.local/agri-mac07/build-release/openemperor-seasonal-agriculture-view-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --resources "$PWD/.local/agri-mac07/build-release/resources" \
  --input "$PWD/.local/agri-mac07/paid-city-fresh/grower-loaded.save.json" \
  --output "$PWD/.local/agri-mac07/view-loaded-fresh" \
  --renderer metal --phase food-loaded
```

Other creator checkpoints are `crop-ready.save.json`,
`hemp-grower-loaded.save.json`, `plantation-grower-loaded.save.json` and
`slot-11-before.save.json`; use phases `ready`, `hemp-loaded`, `plant-loaded`
and `boundary` respectively. Software checks use `--renderer software` with
SDL dummy. The script stops on native input/focus takeover. It verifies all
nine ordinary inspectors, exact F5/F9/Step continuation and paused frame
purity, then records marker visibility separately. Existing painter occlusion
can make the supplied Hemp marker unavailable at1×/2×/4×; ordinary source
selection still exposes the real carried portion. Clear-scene Hemp pixel
selection remains required in the synthetic presentation test. Do not report
an occluded actor as a successful native marker pick.

## Physical Food Growers (AGRI-MAC06)

Use explicit **Rule31/policy3/schema47** (`E Physical Growers` or
`--physical-food-growers`). The [Agriculture contract](original-economy/agriculture.md#physical-food-growers-agri-mac06)
defines real field work, carried harvest, reserved Farm return and sole Food
authority. Historical Rule30's direct-credit adapter remains exact.

```sh
cmake -S . -B .local/agri-mac06/build-release -DCMAKE_BUILD_TYPE=Release \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT="$PWD/.local/gog-extracted/app"
cmake --build .local/agri-mac06/build-release --parallel 4
ctest --test-dir .local/agri-mac06/build-release --output-on-failure \
  -R 'physical-food-growers'
.local/agri-mac06/build-release/openemperor-physical-food-growers-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --map Cities/Chengdu.map \
  --output "$PWD/.local/agri-mac06/paid-city-fresh" --ticks 14000
.local/agri-mac06/build-release/openemperor-physical-food-growers-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --map Cities/Chengdu.map \
  --output "$PWD/.local/agri-mac06/replay-fresh" \
  --replay "$PWD/.local/agri-mac06/paid-city-fresh/seed.save.json" \
  --expected "$PWD/.local/agri-mac06/paid-city-fresh/final.save.json"
```

Output must be a fresh ignored child of `.local/agri-mac06`, disjoint from
Original files and explicit replay inputs. The actual source-model creator buys
its city through World commands, records source prices/debits, runs at least
12,000 ticks and independently records every crop's outward/wait/work/carried
return/accepted Farm receipt and later Market/House receipt/consumption. A
separate process restores the exact seed and must reproduce the creator's final
canonical save. Reports distinguish optional paid joint infrastructure from
actual Fish/Meat/material returns; a building alone proves no production.

World tests cover all five physical chains, one worker rotating three crops,
five Farms alongside Fish/Meat/plantations, one-unit labor, finite headroom,
partial work cancellation, staffing/fire/access gates, future land cuts,
begun-edge preservation, repair and retained loaded return. Save tests cover
every actor phase, physical carts, Recovery, twelve-thousand-tick continuation,
hostile identities/crop/work/chronology/routes/conservation, atomic-write failure
and historical writer protection. SDL tests exercise ordinary menu E, Farm/field
inspectors, visible worker picking, F5/F9 and paused frame purity. These are
scripted checks; human play and native OS pointer acceptance remain separate.

The supplied-model Chengdu creator and a separate process each completed
20,000 ticks using the frozen Rule31 implementation. Both final canonical saves
have SHA-256 `942be7dcee141847529a20b00b72595f836f03d83039a7b572f3a24a0e23e898`.
The single Farm rotated all five crops, earned9,760 actual work units, and
spawned/retired122 distinct workers. Its full22-person staffing held for19,721
ticks; the city ended with88 residents,19 paid bodies and26 channels.

| Food | Actual Farm/Quay/Tent returns | Actual House consumption |
|---|---:|---:|
| Wheat | 24 | 24 |
| Millet | 25 | 25 |
| Rice | 25 | 24 |
| Cabbage | 24 | 24 |
| Bean Curd | 24 | 17 |
| Fish | 88 | 70 |
| Meat | 34 | 7 |

The same city recorded eight actual plantation returns. Crop harvest122 equals
Farm returns122 plus zero live Grower cargo; all Food origin244 equals held52
plus one actual Rice cart portion plus consumed191. Work, harvest, carrying,
source receipts, subsequent Food arrivals and House meals were independently
observed; stock/population injection was absent. The creator and replay reports
are `.local/agri-mac06/final-chengdu-creator-02/report.json` and
`.local/agri-mac06/final-chengdu-replay-02/report.json`. The registered supplied-
model test separately earns and replays14,000 ticks, verifies actual joint
Fish/Meat/plantation returns and rejects output reuse, wrong schema, unsafe maps
and a map-mismatched replay without altering supplied inputs.

The frozen-code Rules24–30 comparison produced **84/84 byte-identical** files
(42 canonical JSON saves and42 canonical states), against Git-verified merged
MAC05 source at `29f29deeafe1692ef7a976cac7111914342ed056`. It includes active
Food/boat/Grower cargo, Rule30 partial work and three-crop consumption; none
acquired Rule31 worker authority. Evidence is
`.local/agri-mac06/run-01/historical-final4-byte-equality.json`.

Hostile controls include swapped older receipt lifetime IDs, coherent impossible
spawn/retirement counts, overflowing cart arithmetic, and a coherent forged
four-harvest history requiring376 physical ticks in a375-tick Farm. Both JSON
restore and DTO publication reject these histories; existing targets and live
Worlds remain unchanged. The supplied-data375-tick control is additionally in
`.local/agri-mac06/hostile-source-timeline-01/report.json`.

For a native Cocoa/Metal check of an earned checkpoint:

```sh
.local/agri-mac06/build-release/openemperor-physical-food-growers-view-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --resources "$PWD/.local/agri-mac06/build-release/resources" \
  --input "$PWD/.local/agri-mac06/paid-city-fresh/grower-loaded.save.json" \
  --output "$PWD/.local/agri-mac06/view-loaded-fresh" \
  --renderer metal --phase loaded
```

Match `--phase outward|waiting|working|loaded` to its corresponding creator
checkpoint. Use `--renderer software` for hidden SDL dummy-driver checks.
Native input/focus takeover stops the Metal script immediately. Inputs remain
read-only; F5 writes a separate private target before exact F9/Step replay.

All four phases passed with both renderers against the20,000-tick creator's
checkpoints: outward281, waiting285, working298 and loaded370. Each produced
seven inspected PNGs: Farm/field at1×/2×/4× and actual Grower selection at1×.
All56 real PNGs were opened; the56 frame groups cover168 paused frames.
The two larger-zoom Grower views were honestly recorded as occluded/unavailable,
with no screenshots or visibility claim for them. Native Metal used Cocoa;
software used SDL dummy. Every phase retained exact F5/F9/Step/pause state,
selected the actual worker through submitted pixels at1× and recorded all12
forbidden frame-work counters as zero. Reports/screenshots are in
`.local/agri-mac06/view-verified-{software|metal}-{outward|waiting|working|loaded}-01/`.
Native OS pointer and human acceptance remain **NOT_RUN**; this is scripted
native rendering/input-handler evidence. Grower artwork remains a labelled
authored fallback, not Original Actor26 animation parity.

The initial fresh full pipeline completed387/387 Debug and387/387 Release
cases, then ASan exposed a real next-edge read beyond a restored one-cell
Waiting/Working Grower route. The production position query now returns that
validated actual Field cell directly. Regression checks call it before save
and after fresh restore; sanitized Presentation/World/Save then passed3/3.
The first full report remains `.local/agri-mac06/alpha-check.json`.

That run also exhausted the older Rule30 integration's unchanged240-second
child limit under eight concurrent tests. The identical isolated test passed
its creator and independent14,000-tick replay in400.658 seconds total, with no
input changes. Only that case uses CTest `RUN_SERIAL` in sanitizer builds;
its240-second child limits,600-second case limit,14,000 ticks and assertions
remain unchanged. Final verification uses entirely new build/report paths.

Final full pipeline: `.local/agri-mac06/alpha-verified-check.json` is
**PASS/full_data**, with fresh native arm64 Debug, Release and ASan+UBSan
builds under `alpha-verified-{debug,release,sanitize}`. Each completed
**387/387 tests with zero failures and zero skips** (124.814s /99.135s /
987.979s). The100,000-tick deterministic JSON-roundtrip endurance, render stress,
session lifecycles and Original-data checks also passed. Debug and sanitizer
used assertion-enabled `-g1 -O1`; macOS LeakSanitizer remains disabled, with
project texture/resource lifetime checks reported separately.

All three fresh primary core comparisons passed74/74 with unchanged
46 parity /18 corrected_bug /6 unverified /4 unsupported classifications;
reports are in `.local/agri-mac06/run-01/core-parity-verified-{debug,release,sanitize}/`.
The corrected817-file code/test manifest remained byteexact throughout validation.
All15 monitored supplied files retained bytes, permissions and modification
times; see `.local/agri-mac06/run-01/input-guard-final.json`.

## Multi-field Agriculture (AGRI-MAC05)

Use explicit **Rule30/policy3/schema46** (`C Multi-field Agriculture` or
`--multi-field-agriculture`). Historical Rule29 retains its one-field contract.
The maintained [Agriculture contract](original-economy/agriculture.md#multi-field-agriculture-agri-mac05)
defines shared labor, per-field history and the unchanged sole seven-kind ledger.

```sh
cmake -S . -B .local/agri-mac05/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT="$PWD/.local/gog-extracted/app"
cmake --build .local/agri-mac05/build-release --parallel 4
ctest --test-dir .local/agri-mac05/build-release --output-on-failure \
  -R 'multi-field-agriculture'
.local/agri-mac05/build-release/openemperor-multi-field-agriculture-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --output "$PWD/.local/agri-mac05/paid-city-fresh" --ticks 14000
```

Output must be a fresh ignored child of `.local/agri-mac05`, disjoint from the
read-only source installation. The supplied Xia/Normal scene uses the unchanged
model and actual paid Farm/field/road/House/service commands. Natural immigration
and assigned workers earn all work. A passing report requires at least three
crop kinds whose sole harvested source is the same Farm, actual source departures,
accepted Market/House receipts and House consumption over12,000+ real ticks.
It checks both per-kind Food and physical transfer equations, independently
restored continuation, multiple field phases, real loaded-courier edges and
Recovery. Exit2/PARTIAL is a failed positive acceptance, never a substitute.

Repeat the creator in a separate process with explicitly supplied files:

```sh
.local/agri-mac05/build-release/openemperor-multi-field-agriculture-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --output "$PWD/.local/agri-mac05/replay-fresh" \
  --replay "$PWD/.local/agri-mac05/paid-city-fresh/seed.save.json" \
  --expected "$PWD/.local/agri-mac05/paid-city-fresh/final.save.json"
```

Registered World/Save tests cover all five crop variations, multiple Farms,
per-tick shared quota/fairness, independent field/Farm pause, full inventory,
real workforce competition and natural Farm fire, loaded route cuts, supply
starvation/repair, strict cursor/parent/age/suffix-work bounds, hostile/duplicate
JSON, exact save/recovery and12,000 restored ticks. Historical writers must
preserve both valid and damaged schema46 targets. Presentation tests drive
normal menu/button/keyboard handlers, explicit Farm binding, eight-field panel
scrolling, real field operation controls, per-field growth, F5/F9 and paused
frame purity using synthetic data and scripted SDL. They do not prove native
Metal or unassisted human acceptance. Original source bytes must remain intact.

The supplied-model14,000-tick Xia/Normal run earns four fields at one paid
Farm with22 actual workers. Each receives3,215 shared work units, producing40
portions and retaining15/80 progress; total work12,860 stays within the Farm's
real staffed opportunities. Wheat, Cabbage and BeanCurd have no other Farm
harvest origin. At the endpoint their equations are respectively
`40 = 0 held + 2 cargo + 38 consumed`, `40 = 2 + 2 + 36`, and
`40 = 4 + 2 + 34`. Millet's citywide equation is `203 = 4 + 3 + 196`.
No Fish/Meat origin is earned by this particular city. All actual departures
also equal receipts plus returns plus remaining cargo. Independent-process
creator/replay endpoints are byte-identical. The studied Farm's own loaded
cart checkpoint is tick517; independent-progress checkpoint is203. Private
creator/replay evidence is under `.local/agri-mac05/positive-replay-a_hny_zs/`.

For scripted source-backed rendering, build the excluded view helper and use
that explicitly earned Food-cargo checkpoint:

```sh
cmake --build .local/agri-mac05/build-release --parallel 4 --target \
  openemperor-multi-field-agriculture-view-acceptance
.local/agri-mac05/build-release/openemperor-multi-field-agriculture-view-acceptance \
  --data "$PWD/.local/gog-extracted/app" --resources "$PWD/resources" \
  --input "$PWD/.local/agri-mac05/paid-city-fresh/mid-cargo.save.json" \
  --output "$PWD/.local/agri-mac05/view-fresh" --renderer metal
```

`--renderer software` uses a hidden dummy window; `metal` requires actual
macOS Cocoa/Metal and stops on observed native user input/focus takeover.
The helper verifies Farm/field picking and actual inspector work, F5/F9,
Step/reload and paused1x/2x/4x frames against complete immutable state. Frame
I/O, asset decode/upload, navigation, commands, ticks and tracked snapshot work
must stay zero. Open every capture used for visual claims. Native OS pointer
input and unassisted human acceptance remain separate. At900×600 the inherited
toolbar leaves only one detail row: Rule30 scrolls by text rows and omits the
unsupported demolition footer, preserving fixed operation controls and access
to all eight fields. Larger windows show more of the same bounded document.

The final source-backed software/dummy and native Cocoa/Metal runs both pass
at tick517. Each retains six opened Farm/field1x/2x/4x PNGs under
`.local/agri-mac05/view-software-final/` and `view-metal-final/`. Farm1's Millet
field2 has harvested1 with progress0; selected Wheat field3 and its two peers
are79/80. Farm1's own courier1 carries Millet1 toward Market13 with begun-edge
progress1. F5/F9, Step/reload and pause/resume preserve the exact schema46 World;
all twelve tracked counters remain zero during pure paused frames. Source
pixels become soft at4x and tall artwork can leave the viewport; this is no
Original animation or whole-scene parity claim. Native OS input and human
acceptance are NOT_RUN.

Final fresh native arm64 CTest matrix: **380/380 Debug,380/380 Release and
380/380 ASan+UBSan**, with Original data configured and no skipped cases.
The separate core comparison passes74/74 in all three configurations. The
first full run retained five obsolete unknown-Rule30 expectations; those tests
now explicitly verify30 and continue refusing31/999, preserving every historical
term/default. The first report remains `.local/agri-mac05/alpha-check-01.json`.
The final historical probe against matching libraries preserves35 saveJSON and
35 canonical states byte for byte relative to merged `cda60b20`.

The final full `tools/alpha_check.py` report is
`.local/agri-mac05/alpha-verified-check.json` (**PASS/full_data**), built in fresh
`alpha-verified-debug`, `alpha-verified-release` and `alpha-verified-sanitize`
directories. Every configuration also completes100,000 deterministic generic
endurance ticks and12 real JSON roundtrips. Debug and Sanitizer each complete
3,000 render-stress frames with unavailable loaded source files, zero decode/
upload/order rebuild work and no simulation ticks. All100 session lifecycles
close their owned texture counters; the9 additional Original-file guards pass.
The separate15-file byte/mode/mtime guard remains unchanged. The generic
endurance is distinct from Rule30's earned14,000-tick supplied-model replay.

## High-tier luxury consumption (AGRI-MAC04)

This check uses the unchanged supplied Normal House model and paid
**Rule29/policy3/schema45** gameplay. Existing authored lower-House tests remain
separate controls. Tea first needs Common10, Silk Elite14, and the
Bronzeware/Lacquerware alternative Elite15. Warehouse arrivals alone do not
establish any House consumption.

```sh
cmake -S . -B .local/agri-mac04/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT="$PWD/.local/gog-extracted/app"
cmake --build .local/agri-mac04/build-release --parallel 4 --target \
  openemperor-luxury-progression-acceptance \
  openemperor-luxury-progression-view-acceptance
.local/agri-mac04/build-release/openemperor-luxury-progression-acceptance \
  --data "$PWD/.local/gog-extracted/app" \
  --output "$PWD/.local/agri-mac04/paid-city-fresh" \
  --ticks 30000 --hold 20400
```

Use a configured build and the actual read-only installation. Output must be a
fresh child of `.local/agri-mac04`, disjoint from Original data. The finite Xia
recipe buys roads, bodies and attached plants/fields with normal funds,
workforce and placement checks. It changes no source model or gameplay rule.
`report.json` and `commands.json` retain current/next House needs, physical
routes and commitments, actual transitions and per-good dispatch, arrival and
consumption events. Production, held stock, real cargo and consumed units are
checked independently. Exit2 means **PARTIAL**, with the first actual gate and
the terminal House matrices; it must not be reported as positive acceptance.

The creator restores an independent World from its exact paid seed and compares
complete state throughout continuation. Checkpoints perform canonical JSON
roundtrips; final hostile controls preserve the existing save, and Recovery
must restore the same schema45 World. An explicitly supplied `--input` plus
`--expected` repeats the creator continuation in a separate process. No personal
save directory is searched.

For actual F5/F9, paused Step/reload, House Goods inspection and1x/2x/4x
captures, use an earned `mid-cargo.save.json` from that run:

```sh
.local/agri-mac04/build-release/openemperor-luxury-progression-view-acceptance \
  --data "$PWD/.local/gog-extracted/app" --resources "$PWD/resources" \
  --input "$PWD/.local/agri-mac04/paid-city-fresh/mid-cargo.save.json" \
  --output "$PWD/.local/agri-mac04/view-fresh" --renderer software
```

The optional `--renderer metal` opens a native window and verifies the actual
Metal renderer. Both modes drive scripted production SDL handlers; native OS
mouse delivery and unassisted human acceptance remain separate. Any observed
user input stops the native script. Open all generated PNGs before making
visual claims. Pure paused frames must perform zero tracked I/O, decode/upload,
route, command or tick work and preserve the complete World.

The registered `original-luxury-progression-real` check earns all three goods
within6,000 real ticks and runs an independent process from moving luxury cargo
to the creator endpoint. It requires ordered plantation-return/processor-debit/
Warehouse/Market/House/consumption witnesses, actual paid command receipts,
per-good conservation, meaningful hostile controls and canonical save equality.
`original-luxury-progression-cli-guards` additionally covers14 argument, path,
publication and supplied-source replay refusals. Without Original data it runs
11 independent controls and reports the three source-backed controls NOT_RUN;
the separate positive real-data check returns77 when its inputs are absent.

Measured supplied-model Xia case (Normal, Rule29/schema45):

| Observation | Result |
|---|---|
| Natural occupied House41 sequence | Elite13 at920;14 at3865;15 at4222;16 at4273;17 at4426 |
| First actual luxury consumption | Silk at4120 (Elite14); Tea and Lacquerware at4477 (Elite17) |
| End tick24877 luxury equations | Tea132=81 held+0 cargo+51 consumed; Silk129=77+0+52; Lacquerware128=77+0+51; imports/losses0 |
| Real post-maturity window |20,400 ticks,4477→24877 |
| Target House41 |20,400 Elite17 ticks; minimum22 occupants;0 fires/devolutions;53/400 actual current-need failures |
| Other twelve Houses in that window |51 devolutions;8,090 combined burning House-ticks; per-House durations and need failures retained |
| Window finance | Taxes21,109 minus installed upkeep15,400=+5,709; Funds5,478→11,187; minimum during complete run5,322 |
| Paid resource bounds |62 bodies;46 couriers+12 reserved wildlife channels=58; limits unchanged |
| Exact persistence | Moving cargo at2284, maturity4477 and final24877; canonical roundtrips, independent World continuation, four hostile controls and Recovery |

The finite player plan omits two distant redundant paid school sites to fit the
existing channel budget. A third planned Farm/Field remains unbought because
actual wildlife blocks its footprint; the other two paid Farms supply the city.
The report distinguishes real transient refusals, unresolved purchases and
permanent aborts. No model threshold, actor claim, budget or source-admission
rule is bypassed. These measured shortages/fires are not a claim of uninterrupted
needs or universal citywide fairness. Human acceptance remains NOT_RUN.

| Technical validation | Result |
|---|---|
| Complete Debug / Release / ASan+UBSan suites |374/374 each; zero failures/skips in the final complete runs |
| General-profile alpha endurance |100,000 deterministic ticks and12 real JSON roundtrips per build |
| Alpha render/session stress |3,000 frames each in Debug and sanitizer, zero decode/upload/ticks;100 menu sessions |
| Independent historical core comparison |74/74 each in Debug and Release; existing case observations/classifications unchanged |
| Historical production/resource source identity |347 files byte-identical to merged main0906031; no gameplay or schema change |
| Supplied-data SDL / native Cocoa-Metal | Exact moving-cargo F5/F9, Step/reload and1x/2x/4x House Goods inspection; all final captures opened |
| Supplemental sanitizer View | PASS with zero tracked paused-frame work and no sanitizer diagnostics |
| Original-input protection |15 SHA/size/mode/mtime guards unchanged; alpha additionally verifies its nine used files |

The initial complete sanitizer run passed372/374 and found a use-after-free in
the new QA recipe's workforce-priority loop. Transactional commands replace
World storage, so that loop now retains stable IDs and reacquires copied bodies
before each command. The corrected paid city and independent longplay replay
reproduce the exact canonical endpoint above. All complete suites and the
remaining unchanged alpha-runner components were repeated with fresh visual
output roots; no test deadline or assertion was relaxed. Initial failure reports
remain private, alongside `.local/agri-mac04/alpha-verified-check.json` and the
final creator/replay under `.local/agri-mac04/run-01/`. Debug and sanitizer use
assertion-enabled `-g1 -O1`; macOS LeakSanitizer is disabled as documented above.
Native OS mouse/keyboard delivery, unassisted human play and packaging were not
part of this verification.

## Playable Hunting (AGRI-MAC03)

Select **Y Playable Hunting (rule29)** in the regular menu, or launch:

```sh
build/openemperor --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Chengdu.map --sandbox-rules sandbox-city-v16 \
  --playable-hunting
```

Use the actual supplied installation path. Shift-H selects the paid Hunter's
Tent; ordinary H remains Help. Build Houses, a Well, Ancestor, Market and a
Farm/field first to earn population and workers. A Tent needs a legal2×2 body,
road access and its full source-model staff (Normal15). Click the Tent or a
real Hunter to inspect its live status, routes, target, cargo and seven-kind
inventory. Huntable wildlife uses a labelled authored visual fallback.

For a short manual check, run the city, watch a Hunter walk to an actual animal,
then return carrying Meat. Tent Meat must increase only after return/unload.
Open the Market to see Meat after a courier arrives; open a supplied House and
watch Meat receipts and consumption. Pause the Tent: work/unload stops while
physical cargo is retained. F5 during pursuit, a loaded return or either Food
trip; F9 must restore that same movement/cargo and continue without a second
kill/receipt. Z cycles1x/2x/4x. Try construction using both the toolbar and
Shift-H, including an occupied cell; rejection must leave Funds unchanged.

Run the full native arm64 matrix with fresh destinations:

```sh
python3 tools/alpha_check.py --system-sdl --jobs 2 \
  --build-dir .local/agri-mac03/alpha-debug \
  --release-build-dir .local/agri-mac03/alpha-release \
  --sanitize-build-dir .local/agri-mac03/alpha-sanitize \
  --report .local/agri-mac03/alpha-check.json \
  --data "$PWD/.local/gog-extracted/app"
```

The optional supplied-data acceptance launcher shares the existing paid city
blueprints, production map admission, navigator, capture and exact-save tools:

```sh
cmake --build build --parallel 2 --target openemperor-hunting-native-playtest
build/openemperor-hunting-native-playtest "$PWD/.local/gog-extracted/app" \
  "$PWD/build/OpenEmperor.app/Contents/Resources" Cities/Chengdu.map \
  "$PWD/.local/agri-mac03/actual-data-fresh"
```

It buys a complete city, inherited Farm and Tent through normal commands,
then records actual animal/hunt/cargo/return/unload and Meat Market/House/
consumption witnesses. It modifies no source model, injects no people or stocks
and grants no arrivals. It writes exact schema45 checkpoints during hunting,
loaded Hunter movement and both Food legs, plus three zoom captures. Its
scripted SDL software evidence is distinct from the native Metal application
and from an unassisted human playtest. A bounded planner refusal is not proof a
map cannot support hunting.

Native input diagnostics must record the event before routing as well as the
single `SDL_RenderCoordinatesFromWindow` conversion. Raw `(0,0)` at the SDL
boundary is different from an application coordinate-conversion failure.
Do not infer a DPI/camera offset or manufacture adjusted click positions.
Report native keyboard, mouse construction/picking, status and F5/F9 separately;
when automation cannot supply actual coordinates, use the manual steps above.

`playable-hunting-world` earns all seven foods through paid construction,
normal population/staffing, actual producer work, physical transport and House
consumption. A separate ordinary two-road-component town holds all seven foods
in the same operating Market at once, using source operation controls rather
than stock injection. It covers full footprints, refusal atomicity, exclusive
prey, moving pursuit,60-tick contact,20-tick unload, finite stock, workforce,
pause/fire, route cuts, real depletion/regeneration and12,000-tick replay.
Its strict spawn control depletes a habitat through real hunts, blocks all25
radius3 cells with paid roads and observes1,000 ticks without birth, ID growth
or new Meat; clearing one cell permits exactly one monotone birth. Legal road
movement remains available.

`playable-hunting-save` covers actual pursuit, hunt work, loaded return/unload,
both physical seven-kind delivery legs, exact Recovery rotation/start retention,
12,000-tick continuation, hostile identity/phase/path/cargo/counter/reference
mutations, duplicate JSON keys, atomic-write failure and44/45 isolation.
`playable-hunting-presentation` uses ordinary paid Millet/Fish/Meat production,
actual button coordinates, a selectable procedural canvas Tent, live wildlife
and Hunter picking, real loaded-return F5/F9, actual Meat consumption and seven
Food inspector columns. Correct submitted frames precede sprite picking.
A mismatched mouse Down/Up cancels, the following correct click succeeds,
stale motion cannot replace button coordinates and focus loss cancels the
purchase. Ten unchanged frames perform no authority, I/O or route work.
`playable-hunting-cli` rejects28 invalid/conflicting invocations before data or
SDL startup. The full five-crop/seven-food production acceptance is in the
World/Save tests; the compact UI town keeps Market capacity available.

Supplied-data acceptance on unchanged `Cities/Chengdu.map` uses the actual
Normal source models, compact six-House paid construction and Tent33 at118,43.
Construction costs987 Funds; the Tent requires15 real workers. At tick2029,
four real hunts had removed four animals, four portions had physically unloaded
and reached the Market, four had reached Houses and one Meat plus17 Millet
portions had actually been consumed. Six exact schema45 F5/F9 checkpoints cover
paid-empty0, hunt729, loaded Hunter789, Meat-to-Market879, Meat-to-House975 and
consumption2029. Final1x/2x/4x captures were opened. This is supplied-data
scripted SDL software evidence, independently bounded from native acceptance.

Completion matrix for the final source:

| Check | Result / evidence |
| --- | --- |
| Paid seven-food production and actual House consumption | PASS |
| Seven foods held simultaneously in one Market | PASS, no injected quantities |
| Depletion, exclusive prey, blocked grass/road births and regeneration | PASS |
| Persistence, Recovery, hostile input and12,000-tick replay | PASS |
| Paid SDL controls, canvas roof/actor picking and render purity | PASS |
| Supplied-data Chengdu hunt → return → Market → House → consumption | PASS |
| Complete Debug / Release / ASan+UBSan registered suites | PASS371/371 each, zero failures/skips |
| Native arm64 Metal startup/menu, movement, zoom and F5/F9 | PASS; final strict-spawn continuation checked separately below |
| Native coordinate mouse construction/picking/stock inspector | NOT_VERIFIED: unstable upstream coordinates |
| Unassisted human acceptance | NOT_RUN; manual steps above |
| Original hunting ecology, timing, raw portions and animation parity | Unproved; explicitly AUTHORED successor |

Native input diagnosis observes the raw SDL boundary, not a guessed display
correction. In the first final-task session, one click arrived with Down387,508
but Up1099,563.42; the existing matching-release guard correctly cancelled it.
Another attempt supplied0,566.24 for both events, and a later live-Hunter click
supplied0,0. At native1100×700 output, scale1 and Metal, converted coordinates
matched those raw values. Keyboard/window accessibility controls worked. This
establishes unreliable automated delivery before game conversion, not a proved
application DPI bug or a verified human mouse flow. No input offset/workaround
was added. Scripted SDL verifies both real matching coordinates and these
cancellation boundaries.

The native arm64 Metal app loaded the same earned789 save through the regular
L/Return menu and advanced the real loaded Hunter to792, cargo1 and edge4/5.
A final native continuation after the strict birth-floor change reached4266:
seven actual kills/unloads, five Meat and41 Millet consumed. After two further
paused ticks, native F9/F5 restored the complete4266 World exactly; all three
animal/cargo/Food conservation equations also held in that native save.
The preceding same-UI session observed actual Hunter and wildlife movement,
1x/2x/4x rendering and exact loaded-Hunter F5/F9 at792; the final birth-only
change was followed by a further supplied-data and native runtime check.
The app and its dependencies are native arm64 Mach-O/SDL3/system libraries;
no Windows binary, VM, debugger or Wine is used. Owned test windows were closed.
Private input and current native reports remain under
`.local/agri-mac03/native-ui-01/`, `native-city-01/` and `native-strict-01/`.

Final `tools/alpha_check.py` acceptance is **PASS**: **371/371 Debug**
(142.562s), **371/371 Release** (139.791s) and **371/371 ASan/UBSan** (718.239s),
with no failures or skips. The same unchanged runner also passed general-profile
100,000-tick endurance and12 real JSON roundtrips in each configuration,
3,000-frame Debug/sanitizer render stress,100 menu lifecycles and the actual
Original-data checks. These complement the dedicated Hunting acceptance;
they do not establish Original hunting parity or unassisted human acceptance.
Independent fresh-library Debug/Release comparisons passed74/74 each, with
literal historical cases, classifications and observations unchanged. Eleven
explicit Original-file guards retained SHA-256, size, mode and mtime.

All builds target native arm64. Local Debug/sanitizer use assertion-enabled
`-g1 -O1`; Release retains normal flags. The initial complete builds took
567.8/617.969/1063.598s. The strict empty-grass birth admission and new negative
control were compiled into all three isolated builds before their checks.
The first full Debug suite then found only two stale synthetic assumptions:
known-rule range ended at28, and a compact-menu click used an obsolete literal
position. Tests now retain every historical expectation, explicitly add29 and
unknown30, and click the actual enabled Data Folder control. No gameplay code
changed after that first369/371 run.

The initial failed report, complete Debug JUnit/inventory/log and original
visual bytes are preserved in `.local/agri-mac03/alpha-check.json` and
`alpha-initial-debug/`. The final run uses unchanged public alpha checks, with
a private guard allowing only those exact existing isolated build directories
and a fresh report; every target is reconfigured/rebuilt and every registered
case rerun. Initial visual roots were archived before fresh captures. No test,
assertion or individual deadline was skipped or relaxed. Final results and the
runner/source audit are `alpha-verified-check.json`, `alpha-final-audit.json`
and the49-entry `validation-sources-03.json` under `.local/agri-mac03/`. A final
terminal-blank-line cleanup affects only the optional native-tool support
header; both tools were rebuilt and supplied-data Hunting rerun. Its matching
commit fingerprints are retained in `validation-sources-final.json`.

## Playable Fishing (AGRI-MAC02)

Select **F Playable Fishing (rule28)** in the regular menu, or launch:

```sh
build/openemperor --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Chengdu.map --sandbox-rules sandbox-city-v16 \
  --playable-fishing
```

Use the user's actual supplied installation and map path. This explicit
AUTHORED Rule28/schema44 successor retains all five Playable Agriculture crops
and adds Fish2 through one six-column Food authority. Rule27/schema43 and all
older generations keep their own exact contracts. No Original executable or
Windows runtime is used. The [fishing contract](original-economy/agriculture.md#playable-fishing-agri-mac02)
documents finite ecology, portion units, timing, water admission and equations.

Shift-F selects the paid Fishing Quay; R cycles its four orientations. The
complete outline includes the exterior dock and required eight-water/two-land
context. Build road-connected Houses, Water, Ancestor, a Market and an inherited
Farm/field to earn residents and workers. Full Quay staffing, operation, no fire,
a real road entrance and a reachable safe-water ground are required. Select the
Quay or its actual boat to inspect stock, reservations, work, route and cargo.
Pause/resume it; no Fish may enter its inventory before physical return and
unloading. Inspect Market arrivals and the House Goods section for actual Fish
receipts/consumption. Save with F5 during boat movement and delivery, reload with
F9, then continue. Z cycles three zoom levels. The boat is an explicitly labelled
authored visual fallback; source-qualified static Quay art has authored poses.

Run the complete native arm64 pipeline with fresh paths:

```sh
python3 tools/alpha_check.py --system-sdl --jobs 2 \
  --build-dir .local/agri-mac02/alpha-debug \
  --release-build-dir .local/agri-mac02/alpha-release \
  --sanitize-build-dir .local/agri-mac02/alpha-sanitize \
  --report .local/agri-mac02/alpha-check.json \
  --data "$PWD/.local/gog-extracted/app"
```

`playable-fishing-world` earns paid population/staff, actual outward boat
movement, target work, finite catches, return, unloading, both physical delivery
legs and actual consumption of all six Foods. It covers all four orientations,
atomic refusals, late Quay construction after five Farms, staffing competition,
pause/fire, full storage, rejected/blocked land deliveries, blocked water routes,
begun-edge safety, two-boat competition, depletion/regeneration and deterministic
12,000-tick continuation. No positive setup injects Fish or Food.
`playable-fishing-save` checks schema44 boat/work/cargo and delivery continuations,
full five-cell dock entrances and real Inspector/food returns, Recovery, hostile
and duplicate-key input, source binding, atomic writes and historical isolation.
`playable-fishing-presentation` drives the regular menu, paid Quay construction,
actual boat picking, ten pure frames, mid-edge F5/F9 and later consumption under
scripted SDL. `playable-fishing-cli` checks explicit selection and conflicting
arguments. The existing synthetic/real building-profile tests exercise all four
Quay rasters, shared deduplication, SHA/path/budget refusal and old selectors.

For additional supplied-data evidence, build the isolated technical launcher:

```sh
cmake --build build --parallel 2 --target openemperor-fishing-native-playtest
build/openemperor-fishing-native-playtest "$PWD/.local/gog-extracted/app" \
  "$PWD/build/OpenEmperor.app/Contents/Resources" Cities/Chengdu.map \
  "$PWD/.local/agri-mac02/actual-data-fresh"
```

This requires an app build with its prepared Compatibility metadata. Its fresh
private output contains paid-only saves, actual event witnesses, exact F5/F9
checkpoints and three zoom captures. Planning uses normal World admission and
the existing navigator. It may legitimately refuse a map/layout without suitable
water, complete shoreline geometry or legal roads. It never changes source
models or invents stocks to make a test pass. This is **scripted SDL software**,
separate from running the actual native application and from a human playtest.
Open the captures and verify the earned save in the native app; report each tier
separately. Do not call automation an unassisted human acceptance.

Supplied-data testing on `Cities/Chengdu.map` earned the complete Fish and Millet
chain without model edits or stock/population injection: by tick2040, six boat
allocations had caught18 portions,15 had unloaded/reached the Market,11 had
reached Houses, one Fish and17 Millet portions had been consumed. Five exact
schema44 F5/F9 checkpoints cover paid-empty setup, a moving loaded boat, a moving
Fish courier, first consumption and later movement. All three zoom captures
were opened. The bounded layout planner found no layout on Baoji or connected
layout on Anyi; those are planner limits, not proof those maps cannot host a
city. Xia/Banpo supplied no ground under this conservative water policy.

The actual native arm64 SDL3/Metal app passed regular F selection, empty Baoji
scene startup, earned Chengdu loading, visible boat motion and1x/2x/4x rendering.
Native F5/F9/F5 retained the exact World at tick2043 with cargo3 and edge4/5.
Native continuation to2811 increased dock receipts15→24, Fish consumption1→2
and Millet consumption17→22; the saved state loaded paused again. The native
mouse tier remains **NOT_VERIFIED**: automated coordinate clicks reached SDL
as raw `(0,0)` before application conversion, while keyboard and accessibility
window controls worked. No speculative DPI/offset workaround was added.
Quay placement, boat picking and inventory inspection passed scripted SDL;
this does not claim native mouse or unassisted human acceptance. The private
native report/diagnostics remain under `.local/agri-mac02/native-city-01/`.

The complete registered suite passed **367/367 in each of Debug, Release and
ASan/UBSan**, with zero failures or skips. Independent Debug/Release core parity
passed74/74 with literal-equal case results/classifications and the maintained
public report. Eleven explicit Original-file guards retained SHA-256, size,
permissions and mtime. Debug/sanitizer used assertion-enabled `-g1 -O1`; Release
used normal flags. The native binaries are arm64, with SDL3 and native system/
Homebrew dependencies; no Windows runtime is involved.

The first full Debug run passed364/367. Its three failures were stale test
expectations: the next Object value is now Quay, and regular F now explicitly
selects28 while Advanced F retains11. Tests retain all historical rule/profile/
policy assertions and add the explicit new boundary. No gameplay code changed
after that run. The initial report and complete Debug evidence are preserved
under `.local/agri-mac02/alpha-check.json` and `alpha-initial-debug/`. The same
isolated builds were reconfigured/rebuilt, then all three complete suites rerun
through unchanged alpha runner checks, with a private guard permitting only
those exact existing build paths and a fresh report. Visual diagnostics use
fresh output directories. No individual test deadline or assertion was relaxed.
The compiler-only budget is3600s; the native instrumented build took1924.509s.
The final pipeline report/audit use `alpha-verified-check.json` and
`alpha-resumption-audit.json` under `.local/agri-mac02/`.
All remaining alpha components passed: general-profile100,000-tick endurance
with12 real JSON roundtrips per build,3,000 render frames in Debug and ASan/UBSan,
100 menu lifecycles and unchanged checksums for the nine Original files used by
the runner. These general-profile checks complement the dedicated six-Food
paid-world tests; they do not prove Original fishing parity or human acceptance.

## Playable Agriculture (AGRI-MAC01)

The explicit Rule27/schema43 mode is AUTHORED macOS gameplay. The
Original executable and all Windows runtime analysis remain unexecuted and
are not dependencies of this mode. Historical tests below keep their own
Original evidence boundaries.

Use new isolated arm64 paths and native parallel2 builds:

```sh
cmake -S . -B .local/agri-mac01/build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/agri-mac01/build-release --parallel 2
ctest --test-dir .local/agri-mac01/build-release --output-on-failure
python3 tools/alpha_check.py --system-sdl --jobs 2 \
  --build-dir .local/agri-mac01/alpha-final-debug \
  --release-build-dir .local/agri-mac01/alpha-final-release \
  --sanitize-build-dir .local/agri-mac01/alpha-final-sanitize \
  --report .local/agri-mac01/alpha-final-check.json \
  --data "$PWD/.local/gog-extracted/app"
```

The data argument names the user's own exact supplied installation; change it
explicitly if needed. Never overwrite an existing evidence path. The alpha
runner configures arm64 Debug/Release/ASan-UBSan builds and records actual
registered test results, endurance/JSON roundtrips and rendering/session checks.

`playable-agriculture-world` earns actual residents/staff and all five typed
harvests, Farm/Market/House movement, quality witnesses and consumption in paid
Worlds. It checks crop-specific conservation, finite/full stocks, pause, routes,
staff loss, fire, multiple producers, long continuation and inherited Tea/Silk.
`playable-agriculture-save` checks schema43 moving cargo/progress, deterministic
continuation, Recovery, source/hostile input refusal, atomic writes and frozen
schema40/41/42 separation. `playable-agriculture-presentation` drives regular P
menu/map admission, paid tools, real World ticks, inspectors and F5/F9 under
isolated scripted SDL; it does not itself prove native/human acceptance.
`playable-agriculture-cli` checks explicit selection and invalid/conflicting
arguments without launching gameplay or touching source/settings files.

Native application/Original-asset checks and actually opened screenshots are
reported separately from these authored-data tests. On the local arm64 macOS
host, the native application rendered its regular menu, accepted P and opened
Playable Agriculture setup. The supplied Original directory and
`Cities/ASSR-Baoji.map` passed preflight. Desktop automation stopped when the
computer-use tool reported a user change; native city-scene/human gameplay
acceptance is **NOT_RUN**, not inferred from scripted SDL. The native binary
uses SDL3, system zlib, libcrypto, libc++ and libSystem; no Windows runtime or
external debugger is required. Rice uses the verified static paddy art; no
unqualified growth overlay is enabled. Exact Original harvest, hunting/fishing
and native raw-to-Food processing remain outside this PR.

Local macOS arm64 validation completed **363/363 registered tests in each of
Debug, Release and ASan/UBSan**, with zero failures/skips. The Agriculture
tests include all five crops saved during actual cargo movement on both
delivery legs and a 12,000-tick deterministic paid-city continuation with
Tea/Silk. The additional general-profile alpha checks passed 100,000 ticks
and 12 real JSON roundtrips per build, 3,000 render frames in Debug and
ASan/UBSan, and 100 menu lifecycles. Independent Debug/Release core parity
remains 74/74 with unchanged case results and classifications.
The alpha Original-data check verified nine used files unchanged; the five
explicit source guards also preserved SHA-256, size, mode and mtime.

The private final report is `.local/agri-mac01/alpha-verified-check.json`,
with actual inventories/JUnit under the three `alpha-final-*` builds.
Initial reports are preserved: the sanitizer compiler exceeded the 1800s
build-command limit, and the first Debug suite exposed an incorrectly
configured private visual output root. The same isolated builds were
continued with unchanged alpha runner components, a 3600s compiler allowance
and fresh visual subdirectories under `.local/oe61/agri-mac01-alpha-*`.
The earlier Debug results were archived before rerunning all three suites.
No test deadline or assertion was relaxed. Debug and sanitizer builds used
`-g1 -O1` with assertions enabled; Release used its normal flags. Package
publication/notarization was not requested or validated.

<a id="agriculture-foundations-agri-01"></a>

## Agriculture foundations and material plantations (AGRI-01–03)

Read the [Agriculture gate](original-economy/agriculture.md) and
[source contract](reverse/original-agriculture-source-contract.md). Rule24 has
paid verified construction and five-kind Food/save foundations, with production
explicitly locked until Original tending/harvest timing is fully proved. Tests
must establish that no calendar/staffing/UI/restore path bypasses the lock;
positive growth/harvest/House-quality claims are not supported.

```sh
cmake -S . -B .local/agri-02/build-release -DCMAKE_BUILD_TYPE=Release \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT="$PWD/.local/gog-extracted/app"
cmake --build .local/agri-02/build-release --parallel 4
ctest --test-dir .local/agri-02/build-release --output-on-failure
```

`original-agriculture-save` checks exact schema40/catalog/source identity,
canonical five-kind widths, hostile JSON/duplicate/member refusal, no migration,
Recovery and continuation. Its orchard cases use independently authored raw29
metadata to prove that inspection does not grant a named Production/Agriculture
capability, change26–28 or append Fruit to the five-kind catalog. Hostile sixth
stock columns, crop/catalog relabels and Fruit keys must leave World, funds, IDs,
source bytes and existing save bytes unchanged. These are negative authority
checks, not a fruit growth/harvest implementation.
`original-agriculture-housing` checks paid complete
Farm/Field footprints, range/parent/cost and Rice fertility100, refused demolition,
staff/pause/fire/calendar non-production and hostile production/quality authority.
`original-agriculture-presentation` drives explicit SDL menu/tools/F5/F9 and
verifies the visible production lock and pure rendering. These are automated
checks, not native human acceptance or complete agriculture gameplay.
Its Rice cases exercise prepared eight-neighbor connectivity, source sprites,
placement/cancel, alpha picking, F4 comparison and F5/F9 without production.
The building-profile tests qualify the separate schema6 inactive Rice atlas,
all256 mask selections, native RGBA/anchors, atomic malformed-input refusal and
the unchanged historical schemas1–5 and combined64 MiB budget.
The building-profile suite also explicitly refuses model29 as a World building
body: its menu identity cannot qualify a generic orchard sprite. Existing
`original-industry-world`, `original-industry-world-real` and
`original-industry-save` remain the paid Tea/Lacquer/RawSilk regression controls;
their authored timing is not Original orchard lifecycle proof.

AGRI-03 extends the explicitly selected Agriculture lineage as Rule25/schema41.
The following tests exercise the current contract:

- `original-plantations-world`: paid costs/complete geometry, matching ranged
  multi-plant ownership and32-plant bounds; no-worker/pause/road/fire gates;
  finite readiness, actual land movement, pickup, source return and regrowth;
  full storage, loaded output road cuts and actual Tea/Lacquerware/Silk House
  arrivals plus consumption. It observes completed delivery events before the
  existing bounded latest-trip rows are compacted. Foreign-source planting on
  a grower's actual cell or begun next edge must be refused atomically.
- `original-plantations-save`: strict schema41 markers/catalog, exact saved
  work/readiness/worker cargo/receipts, growth/outward/picked/returned phases,
  deterministic continuation and Recovery, malformed authority refusal,
  historical-target byte preservation and cross-generation rejection.
- `original-plantations-presentation`: scripted actual paid tools, selected
  source and range, inspectors, real worker visibility, saved-growth sprites,
  F4/F5/F9 and pure frame work. `plantation-visual-profile-synthetic` and
  `plantation-visual-profile-real` cover the16-image plantation supplement, native
  source geometry, malformed-input atomicity and unchanged historical budgets.
- `original-plantations-food-adapter`: expressly retained Rule25 authored
  Wheat/Millet80-tick production, one productive field per Farm, paid crop
  identity, actual FoodQuality20/80 arrivals and consumption, pause/full-store/
  natural-fire gates, strict schema41 replay/Recovery and hostile crop relabels.
  Independent controls keep all Rule24 Food and Rule25 Rice/Cabbage/Soy closed.
  A Food-positive authored House model requires actual Food/Water/Ancestor
  input before the three plantation luxury chains reach consumption.

AGRI-02B remains PARTIAL. `original-crop-rules` exercises326 bounded source-rule
checks for the five model/resource/slot identities, the distinct growing-period
query and end-slot growth veto, work-byte boundaries, tending flags, decay's
0xc000 mask, quantity cap, raw base-yield division, runtime-fertility byte
conversion and signed16-bit work waits. It links only the pure
crop-rule library and grants no World production or saved state.
`original-plantations-food-adapter` additionally builds all five field types
within the unchanged three-Farm limit. Both Wheat and Millet reach actual
House consumption while Tea/Silk/Lacquerware remain consumed. The paid,
staffed Rice/Cabbage/Soy Farm retains the published no-origin contract. Its
schema41 write/read and103-tick continuation preserve the complete snapshot.
This is an AUTHORED-model compatibility control. Positive Rice/Cabbage/Soy
Food/Grower and unchanged-native-model progression acceptance remain unavailable.
`original-agriculture-presentation` checks the selected field's parent workers,
actual crop stock/departures, explicit Grower/harvest lock and initial detail
visibility without World mutation. Its private `rice-inspector.png` capture is
available alongside the existing rice topology captures.

```sh
cmake --build .local/agri-01/build-release --parallel 4
ctest --test-dir .local/agri-01/build-release --output-on-failure
ctest --test-dir .local/agri-01/build-debug --output-on-failure \
  -R '^(original-crop-rules|original-agriculture-(save|housing|presentation)|original-plantations-(world|save|food-adapter))$'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir .local/agri-01/build-sanitize --output-on-failure \
  -R '^(original-crop-rules|original-agriculture-(save|housing|presentation)|original-plantations-(world|save|food-adapter))$'
```

AGRI-02B validation passed the337-case Release suite after regenerating the
public parity report from a fresh74-case comparison and rechecking its contract.
The report adds only the two new crop-rule source fingerprints. Its existing
case observations and classifications remain literal. Debug and ASan/UBSan
each passed the seven selected regressions, followed by a rebuild and successful
run of the final326-check pure target. The isolated real-Rice SDL test and both
synthetic/native-asset inspector captures were also checked. These results
cover the implemented arithmetic, preserved contracts and read-only inspection.
Positive new-crop production and native human acceptance remain open.

`PlantationsFixture.h` builds its full228×228 map and model independently.
Every road, source, plant, processor, warehouse, Market and House is paid through
World commands. The earlier authored lower-House Food0/Water0 positive controls
remain separately identified. `PlantationFoodAdapterTests.cpp` requires positive
Food1 plus Water/Ancestor service and earns real Wheat/Millet deliveries before
luxury consumption. No stocks, harvests, residents or successful
arrivals are injected. This is separate from a native-model paid-city result.
Growth80, finite unit yield and five-tick land movement are authored; tests must
not label them Original time/harvest parity. Grower graphics retain their named
fallback until their production visual contract is implemented.

In an existing configured build tree:

```sh
cmake --build build --parallel 2 --target \
  openemperor-plantations-world-tests openemperor-plantations-save-tests \
  openemperor-plantation-presentation-tests openemperor-original-building-visual-profile-tests \
  openemperor-plantation-food-adapter-tests openemperor-plantation-visual-tests
ctest --test-dir build --output-on-failure \
  -R '^(original-(plantations-(world|save|presentation|food-adapter)|building-visual-profile(-real)?)|plantation-visual-profile-(synthetic|real))$'
```

After the corresponding Original test root is configured, the presentation
executable also accepts `--real "$PWD/.local/gog-extracted/app"`. Its captures
stay under ignored `.local/agri-03/plantation-presentation/`; open and inspect
them. Synthetic/direct-render results, scripted SDL, native delivery and human
acceptance must be reported separately. Repeat focused World/save/visual tests
in Debug and ASan/UBSan, then run the appropriate historical regression suite
and literal schema1–40 controls. Current-header/current-library comparisons
must use fresh small private outputs. Do not overwrite protected fixtures or
recreate multi-gigabyte traces. The new source proofs stay under ignored
`.local/agri-03/`; Original files retain hash/size/mode/mtime guards.

The AGRI-03 Release suite passes all336 cases, including targeted reruns of the
updated Rule25 admission matrix and freshly measured parity-report contracts.
The seven focused plantation/visual/legacy-Agriculture cases also pass in Debug
and ASan/UBSan (macOS LeakSanitizer disabled). Fresh current-header/current-library
collectors preserve123 complete historical SaveDocuments,25 DTO controls and
four indexes for schemas1–39:3,184,025 literal bytes against both protected
reference sets. Rule24/schema40 remains separately covered by the unchanged
Agriculture save tests. The refreshed74-case parity report keeps all earlier
classifications and observations; it does not establish plantation clock parity.
The explicit Original-asset SDL presentation run passes and its six captures
were opened and inspected. All1,558 protected Original/earned inputs retain
bytes, size, mode and mtime. Native-model city and human acceptance are NOT_RUN.

After building the corresponding targets in a configured tree, the focused
identity, World, persistence and visual-profile checks are:

```sh
ctest --test-dir .local/agri-02/build-release --output-on-failure \
  -R '^original-(agriculture-(save|housing)|industry-(world(-real)?|save)|building-visual-profile(-real)?)$'
```

`food-batch-arithmetic` covers independently authored1/3/5/16-kind arithmetic,
canonical physical portions, weighted scores and checked overflow. Historical
Food World/save, deadline Food, market, goods, housing, movement, Recovery and
all-generation checks must still pass. Repeat focused checks in Debug and
ASan/UBSan (disable unsupported macOS LeakSanitizer explicitly).

The AGRI-02 foundation baseline passed 330/330 Release tests and eight
focused unique cases in both Debug and ASan/UBSan, including targeted reruns
after correcting a test iterator and adding historical-save overwrite refusal.
LeakSanitizer is disabled on macOS. The unchanged core comparison passes 74/74
classified cases; this is not Agriculture timing parity. Fresh current-header/
current-library collectors match 123 complete SaveDocuments, 25 DTO controls and
four indexes across schemas1–39 (3,184,025 literal bytes) against both protected
reference sets. All 1,558 protected Original/earned inputs retain bytes, size,
mode and mtime. These results establish the closed production boundary and
historical controls; actual new crop production and human acceptance remain open.
The Rice presentation refinement passes ten related Release graphics cases,
the explicit Original-asset SDL scene run and two focused ASan/UBSan cases
(LeakSanitizer disabled). Its five native 2× scene captures were opened and
inspected. This uses authored map/model inputs and fingerprinted Original pixels;
it is separate from a native human playtest. Simulation and persistence code,
the schema5 resource and historical visual selections remain unchanged.

Full CTest is separate from literal historical schema1–39 document comparisons
and paid creator/restore continuations. Use small fresh ignored outputs and
preserve protected originals/earned inputs; do not regenerate multi-gigabyte
traces. Original research uses read-only `.local/gog-extracted/app`; raw windows,
private checkers and diagnostic reports stay ignored under `.local/agri-02/`.
Existing optional OE52/OE61/OE62 visual checks retain their feature-owned output
roots and explicit earned-save inputs below.

## Original walker families and full cycles (OE62)

The [OE62 contract](rendering/oe62-original-walker-animations.md) inventories
every actual CourierRole, source-bound and curated associations, frame/foot
semantics, retained markers and Original timing limits. Build the current app
so its metadata-only `walker-animations.json` is copied beside the executable.
The complete optional profile requires both exact SprMain and SprMain2 pairs;
custom profiles and historical schema1–6 keep their separate behavior.

```sh
ctest --test-dir build --output-on-failure -R 'walker|resource-group|original-(culture|music-venue|venue-house-service)|bridge|ramp|movement|begun|road|save|recovery'
```

`walker-animation-profile` checks compact clips, composed reflection and idle,
complete cycles, direction changes, strict schema7 fields, historical schemas
and unchanged320-alias/256-image/64-MiB bounds using original-free fixtures.
`walker-animation-compatibility-fingerprints` checks complete twelve-role
selection, atomic four-file dependencies, missing/tampered/unsafe resources,
symlinks and custom/historical fallback. Existing walker profile, pose,
software/Metal flip, Inspector, Market, Service and Health tests remain required.

For productive source integration, supply explicit normally paid Rule18 and
Rule23 OpenEmperor saves. Never point F5 at protected evidence or personal saves.
Use a fresh ignored output parent:

```sh
cmake -S . -B .local/oe62/build-release -DCMAKE_BUILD_TYPE=Release \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON \
  -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=/path/to/emperor \
  -DOPENEMPEROR_OE62_RULE18_TEST_SAVE=/explicit/earned-rule18.save.json \
  -DOPENEMPEROR_OE62_RULE23_TEST_SAVE=/explicit/earned-rule23.save.json \
  -DOPENEMPEROR_OE62_VISUAL_OUTPUT_ROOT="$PWD/.local/oe62/visuals"
cmake --build .local/oe62/build-release --parallel 2
ctest --test-dir .local/oe62/build-release --output-on-failure
```

Compare complete World and canonical SaveDocument bytes around actual renders,
pointer probes, pause/resume and F5/F9. Run separate creator/restore processes
and compare full per-tick documents/digests, literal checkpoints and real cargo,
arrival, House Music96 and retirement evidence against the OE61 baseline.
One PNG is insufficient: inspect consecutive frame sequences across complete
cycles and last-to-first transitions at1280×800/1600×900/1920×1080 and1×/2×/4×.
Verify opaque hits, transparent misses, common feet, real current edges, road
and building composition, borders and existing fire/smoke overlays. Keep
source contact sheets, synthetic direct renderer checks and actual productive
software-SDL frames separate. Open every generated image; measure memory and
render time using the same scene/camera/backend rather than treating test
duration as FPS. Recheck all twelve prohibited frame-work counters and the
protected input bytes. Repeat focused Debug and ASan/UBSan checks, historical
schemas and Rule18 Golden City. Native human acceptance remains separate.

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

## Crop dispatch, yield inputs and raw transport (AGRI-02C)

```sh
cmake --build build --parallel 4 --target openemperor-original-crop-dispatch-tests \
  openemperor-original-crop-yield-tests openemperor-original-crop-transport-tests
ctest --test-dir build --output-on-failure \
  -R '^original-crop-(rules|dispatch|yield|transport)$'
```

The additive dispatch tests check independent staffing-tier expectations,
all256 old counter bytes, exact invocation boundaries, caller-supplied quotas
and rejected signed-multiplication overflow. Yield-input tests enumerate the
five typed fields, raw event bytes/states/selectors and every physical
quantity0–10000 through shared integer-base arithmetic. Raw transport tests
check all256 resource bytes, five-bin matching/empty order, explicit capacity,
credit overflow, debit, retained/cleared identities and actual Cart accepted/
residual quantities. The supported nonnegative word-safe API domain is explicit.
Native Farm bin fields are DWORDs and their constructor starts with stock0.

Run complete Release, Debug and ASan/UBSan suites, including persistence,
historical profiles and the existing19,584-tick paid mixed Agriculture city.
That city is a positive control for approved Wheat/Millet/material production
and a zero-origin control for Rice/Cabbage/BeanCurd. Actual new-crop field work,
harvest, FoodQuality, House progression and native Food Grower animations still
depend on the [activation contract](original-economy/agriculture.md#crop-completion-and-decision-boundary-agri-02c).
Keep AUTHORED World fixtures, Original-model/source assertions, scripted SDL
and human acceptance explicit in the result.

AGRI-02C validation on2026-11-15 passed351/351 Release tests,351/351 Debug
tests and351/351 final ASan/UBSan tests, with zero skips. Debug coverage comprises
the complete348-test initial configuration plus the three explicitly enabled
earned-city visual cases. All three final configurations register the same351
test names. ASan/UBSan used Debug assertions, `-g -O1`, leak detection disabled
on macOS and immediate UBSan failure. The initial sanitizer suite passed350/351
with a missing temporary data directory in `original-road-ground-pixels`.
That case then passed20 isolated repetitions and the complete351-test rerun.
Its transient filesystem cause remains unproven. All attempts used the same
gameplay and fixture source.

The new pure checks passed2,790 dispatch,282,074 yield-input and242,123 transport
assertions. Three fresh private receipts passed281 source assertions over57
byte-checked windows. Independent core parity passed74/74 with identical
non-fingerprint public results. Of149 existing simulation/persistence/app paths,
147 remain literal-equal to merged AGRI-06 and only additive `CropRules.h/.cpp`
operations change the other two. Existing World, app and save paths remain exact.
The paid two-cycle city, historical profiles, save/Recovery, Original-model and
earned-city building/walker checks all ran. Meaningful Debug and sanitizer
captures were opened. Scripted SDL inspection establishes its stated rendering
scope. Native human acceptance remains pending. The six guarded Original/earned
inputs retain their hashes, sizes, modes and modification times.

## Further Original Agriculture reconstruction

```sh
cmake --build build --parallel 4 --target openemperor-original-crop-lifecycle-tests \
  openemperor-original-food-rules-tests openemperor-map-tests
ctest --test-dir build --output-on-failure \
  -R '^(original-crop-(rules|lifecycle|dispatch|yield|transport)|original-food-rules|original-goods-rules-(synthetic|real)|emperor-map-reader)$'
```

The lifecycle test checks all65,536 work words for each Food field, literal
end slots/subslots, inactive fields, zero and boundary quantities, retained
assignment/wait intent and completed-old-slot ordering. These are explicit
events, not an independently advancing clock or successful actor/harvest.
The map test reuses synthetic container fixtures to exercise the bounded
header query, signed low-word selectors, unknown values, multipart/profile
refusal, compressed-block boundaries and input immutability. It grants no
complete-map loadability, textual climate identity or World terrain authority.

The Food test covers exact normal House event gates and finite consumption,
all nonnegative signed-WORD populations, raw score boundaries, recipe grade
selection and nine-category Mill arithmetic. Literal ingredient/availability
vectors cover resource order, separate vetoes, Salt-before-Spices clipping,
grade5's actual alternatives,100-unit rounding, partial/zero outputs and
positive opaque native returns. All128 base-veto combinations, supplement
overflow boundaries and a composed positive-availability/empty-request case
exercise the source boundaries without claiming produced Food.

Run the complete Release suite and the affected Debug/ASan/UBSan crop, Food,
goods and map cases. Refresh the independent core-parity report against fresh
matching archives; compare its non-fingerprint results to the protected
baseline. Keep every pre-existing simulation/app/persistence file literal and
retain original source hashes, lengths, modes and modification times. These
pure/read-only changes require no new visual or human acceptance claim.

Validation on2026-11-15 passed353/353 Release CTests with zero skips, plus
9/9 affected cases in each of Debug and ASan/UBSan. The sanitizer configuration
retains Debug assertions with `-g -O1`, disables leak detection on macOS and
halts on UBSan failure. The new lifecycle and Food executables respectively
pass12,791,315 and1,746,768 independent assertions. The independent core
comparison passes74/74; all public results except the four new source-file
fingerprints remain identical to merged AGRI-02C.

Eight fresh private receipts pass590 assertions across154 overlapping
byte-checked windows. All152 pre-existing simulation/app/persistence files
remain literal-equal to baseline `0a3901d`; the separate map query is additive
and leaves the historical parsed-map DTO untouched. The six guarded Original/
earned inputs retain SHA-256, size, mode and modification time. The complete
Release suite includes historical save/Recovery, paid mixed-city and existing
scripted SDL checks. Fresh industry overview and consecutive walker-frame
captures were opened. This validates their stated automated scope, not native
human acceptance or positive production from the three locked crops.

## Original Food Grower source events (AGRI-02D)

```sh
cmake -S . -B build
cmake --build build --parallel 4 --target openemperor-original-crop-selection-tests \
  openemperor-original-crop-grower-tests openemperor-original-crop-harvest-tests \
  openemperor-original-crop-events-tests openemperor-original-crop-lifecycle-tests
ctest --test-dir build --output-on-failure -R '^original-crop-'
```

The selection test checks exact independent gate tables, all physical quantities
and Food work words, zero-quantity mark-clearing order, native cached-queue ties,
the2000-node bound and only post-actual-success initialization/counter effects.
The worker test checks all signed-WORD waits and yield words, movement/source
status bytes, early status mutation, state4, ordinary6–13 callback slices,
counter double-debits, active assignment cleanup and represented reset.
Neither actual entity allocation nor physical movement is fabricated by these
tests; those observations remain explicit API inputs.

The harvest test covers32 literal rounding vectors, an independent8,192-case
exact-rational differential, all PC/RC combinations, physical quantities,
every work word for all five Food models, history-slot effects, one actor WORD,
numeric-to-WORD narrowing, separate carry, repeated callback, expiration and
host-FPU independence. Tending reuses the existing work operation and verifies
complete assignment/counter clearing with quantity/history retained.

The event test composes two distinct targets of the same crop,51/25 work waits,
field growth/decay, actual marked callback effects, supplied route retry/return
results, typed bin credit, full-bin overflow, failed return, cleanup and complete
value replay. Each supplied movement event has its own begin/body/epilogue.
It also exercises late callbacks after expiration and source-inactive body
continuation. Its explicit fertility100/unit-region/PC53 operands and movement
results are synthetic resolved controls, not admitted live city facts. The
source Word retained after receipt is historical memory awaiting reset, not a
second active cargo credit. Source bin/cart bookkeeping is not an accepted-stock
authority.

Run full Release, Debug and ASan/UBSan regression suites against the exact
source tree and fresh matching archives; refresh independent core parity before
its contract check. Preserve the Original/earned input hashes, sizes, modes and
modification times. Existing World tests cover historical paid production,
pause/fire/path behavior, canonical saves and Recovery. No new active Original
Food Grower World mode/save is present: positive physical Farm work/return,
new-worker pause/fire/path admission, active-work or loaded-worker save/load,
hostile new-schema refusal and native human acceptance remain unperformed.
They are listed as activation requirements in the
[AGRI-02D/02E matrix](original-economy/agriculture.md#original-food-grower-reconstruction-agri-02d).

Validation completed on2026-11-16:357/357 Release,357/357 Debug and357/357
ASan/UBSan CTests, with zero skips. All configurations register the same357
names. ASan/UBSan retains Debug assertions with `-g -O1`, macOS leak detection
disabled and immediate UBSan failure. The final supported-domain header-comment
clarification changed no executable behavior; the two affected selection/event
cases and the refreshed parity-contract case then passed in each configuration.
Fresh matching archives pass the independent74/74 core comparison, with all
non-fingerprint public results unchanged from PR55.

Five private source receipts pass524 assertions across65 qualified byte-checked
windows. The harvest test additionally matches its independent8,192-case
exact-rational reference. New executables pass560,787 selection,1,558,299 worker,
1,919,104 harvest and1,931 composed-event assertions; the expanded lifecycle
executable passes12,863,448. Of156 existing simulation/app/persistence files,
154 remain literal-equal to `ea78b07`; the two existing lifecycle files only
append the new growth-driver operation. World/app/save implementations and old
rules/operations remain exact. All six guarded Original/earned inputs retain
SHA-256, size, mode and modification time. Fresh Debug industry/walker captures
were opened. Those are historical scripted SDL controls, not positive Original
Food Grower World or native human acceptance.

## Physical Food Grower integration audit (AGRI-02E)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build build --parallel 4 --target openemperor-original-grower-admission-tests
ctest --test-dir build --output-on-failure -R '^original-grower-admission$'
```

`original-grower-admission` is a **negative World/save control**. Independently
authored map/model fixtures pay for three Farmhouses and all five distinct Food
fields in each of Rules24/25/26. Real World ticks, canonical read/full restore
and deterministic continuation run without supplying synthetic movement8.
The test refuses nonzero Original quantity/work/cycle state for each crop via
full restore and atomic writer, unknown raw Grower/cargo/bin/counter/assignment/
history JSON, and the still unused Rule27/schema43 pair. Rejection preserves
World identities/funds, source bytes, canonical save and hostile overwrite target.
The historical two-kind Food state stays empty beside the sole Agriculture
ledger. There is no settlement in this control: it proves neither staffing nor
physical Original work, harvest, fire/path interruption or Farm-bin credit.

Current World, app, persistence, source rules, assets and tools retain their
exact baseline bytes; only the new test and its CMake registration are executable
changes. Run the complete registered Release, Debug and ASan/UBSan suites,
including historical Save/Recovery and the existing paid Food/material/water
controls. Keep Debug assertions active under sanitizers, with macOS leak
detection disabled and immediate UBSan failure. Reuse the existing build caches
and guarded output roots; serialize suite runs because some SDL controls share
fixed capture destinations. Do not infer an active Grower save from historical
F5/F9 or supplied-result component replay.

The private source checks are separate:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e/spawn/verify_spawn.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e/routing/verify.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e/identity/verify_identity.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e/yield/verify_live.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e/yield/verify_date.py
```

Their five receipts qualify733 assertions over87 bounded byte-checked windows
including overlaps. These are static source observations, not execution of the
Original binary or a positive World Grower test. The Original EXE, models/config
and explicitly supplied earned regression saves must retain hashes, sizes,
modes and modification times. Original files and private disassembly/reports
stay outside Git.

The [completion matrix](original-economy/agriculture.md#physical-food-grower-integration-and-completion-matrix-agri-02e)
lists every still-unimplemented positive World acceptance case. No Original
Actor26 World spawn, physical tending/harvest/return, accepted raw-bin credit,
active/loaded-Grower F5/F9/Recovery or native human acceptance is claimed.
Mill/FoodShop conversion is separate from these raw-chain gaps.

Validation completed on2026-11-16:358/358 Release,358/358 Debug and358/358
ASan/UBSan CTests, with identical registered names and zero skips. The new
five-field/three-generation admission control passes in all configurations.
The fresh independent core comparison passes74/74; its entire public report,
including source fingerprints, remains unchanged. All455 tracked production
source/resource/tool files are byte-identical to baseline `9880bfe`. Seven
guarded Original/earned inputs retain SHA-256, size, mode and modification time.
Fresh historical Debug field and walker captures were opened. These are
historical scripted/render controls, not Original Food Grower World or human
acceptance. AGRI-02E remains PARTIAL under the completion matrix above.

## Bounded World Grower navigation (AGRI-02E2)

```sh
cmake -S . -B .local/agri-02e2/build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/agri-02e2/build-debug --parallel 4 \
  --target openemperor-original-grower-navigation-world-tests
ctest --test-dir .local/agri-02e2/build-debug --output-on-failure \
  -R '^original-grower-navigation-world$'
```

This test uses actual paid Rule26 Worlds with authored map/model fixtures and
real `World::tick` motion through the shared navigator. No movement8 is supplied.
It checks every Food field identity, the literal two-edge41-microstep trace,
actual phase10 cell commits, exact-equality and physical endpoint8, an independent
six-edge Farm-obstacle tie ending at121, paid Road04 cells with atomic current/
begun-edge road-removal refusal, full3x3 Farm exclusion, typed external
return endpoints, unknown source/occupancy/height/boundary refusal, and all
explicit unsupported-interaction requests. The mode17 policy's one-microstep
per World-tick binding is AUTHORED; this does not establish native Actor26 speed.

Current/begun claims refuse overlapping paid fields and irrigation atomically.
Actual future Farm geometry causes9→shared reroute→8, while a newly blocked
return endpoint causes committed-cell→9→10. Unknown future occupancy causes
9→Unsupported11. Source pause during the first or final begun edge preserves
its real cell commit and stops11 instead of granting8. Concurrent disjoint
motions compare complete state against independently restored/ticked Worlds.
Terminal equality tracks fill the shared58-channel budget without gameplay
IDs, stock or field work; overflow and new-courier construction are refused
atomically, and session IDs stay monotone across cleanup.

The transient snapshot guard is intentionally not a historical JSON field.
Active/retained sessions are refused by document creation, writer and restore;
existing save bytes remain unchanged. After terminal cleanup, schema42 bytes
and complete snapshots equal an independently ticked ordinary World. All five
fields retain zero Original work/quantity/harvest/Food authority. Keep
`original-grower-admission` as the unchanged historical24/25/26 no-origin control.
No new persistent Actor26, crop generation or Schema43 is admitted.

Run the complete Release, Debug and ASan/UBSan suites in **separate** task-owned
build/output roots. Use the supplied Original-data regression inputs read-only,
retain Debug assertions under `-g -O1`, and serialize suite runs because some
SDL capture destinations are shared. Refresh the independent74-case core
comparison with fresh matching archives after the reviewed additive World ABI
and new header pins. Confirm all non-fingerprint results remain exact.

The three private source verifiers are additional evidence, not World tests:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e2/predicates/verify.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e2/predicates/verify_region_absence.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02e2/predicates/verify_movement.py
```

Their132 assertions cover28 bounded byte-checked windows including overlaps;
empty-land mapping additionally requires the qualified region-absence context.
An all-zero native height header is not an absence proof. The actual
[World acceptance matrix](original-economy/agriculture.md#bounded-world-grower-navigation-agri-02e2)
separates the implemented motion from unsupported native actor lifetimes,
regions/heights, interactions, autonomous spawn/work, raw return and native
human acceptance. Current menu/CLI games do not enable the C++ acceptance
session automatically. There is no new playable Food-production test command.

Executed on 2026-11-16 in the three task-owned build roots: Release 359/359 and
ASan/UBSan 359/359 passed without skips. The complete Debug run passed 358 cases;
its sole failure was the reviewed-header inventory test's missing new state
header. Adding that reviewed header preserved the drift/missing/symlink guards,
and the targeted contract rerun passed. All 359 unique Debug cases therefore
pass; the extended paid-road navigation case also passed a fresh Debug run.
Sanitizer Debug retained assertions with `-g -O1`, `detect_leaks=0` on macOS and
`UBSAN_OPTIONS=halt_on_error=1`. Fresh Debug and Release independent comparisons
each passed 74/74, with identical public reports and unchanged non-fingerprint
baseline fields. Seven supplied Original/earned-fixture inputs retained exact
SHA-256, size, mode and mtime. Fresh Farm and productive-walker captures were
opened for historical visual regression; they do not establish native Actor26
or human acceptance.

## Autonomous Food Grower World acceptance (AGRI-02E3)

```sh
cmake -S . -B .local/agri-02e3/build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG='-g1 -O1' -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/agri-02e3/build-debug --parallel 4 \
  --target openemperor-autonomous-food-grower-world-tests
ctest --test-dir .local/agri-02e3/build-debug --output-on-failure \
  -R '^original-autonomous-food-grower-world$'
```

This is an actual paid Rule26 World acceptance scene with independent authored
map/model inputs. The shared source reader checks Map5/Header2, context5,
Properties6 and the source entry operands; no Original fixture bytes are copied.
Four paid houses earn24 residents and the Farm receives12 actual workers.
Every tested worker is dispatched by `World::tick` at due source phase31;
the test never calls `start_grower_navigation`, injects movement8 or assigns work.

Separate scenes cover all five Food types in their own growing slots, actual
cell commits, exact wait51, work0x40 and assignment clearing, actual empty
source return and complete actor/channel cleanup. A continuously enabled
controller completes two worker lifetimes. A longer physical return observes
native retainedCounter0/live1 and a real bounded-allocation refusal without
consuming another ID. An independent restored World replays complete actor,
assignment, work and navigation state against a third ordinary control.

Access tests remove/repair the actual unique Farm road; ordinary b7c distance
preparation admits land, so a removed road spur alone is not a failed access
proof. Real future Farm geometry causes retry/shared rerouting. Source pause,
paid workforce-priority loss, natural fire and new field-manager entries stop
work and drain claims safely. Multiple unresolved initial candidates and all
explicit exceptional-interaction requests refuse without invented scores or
actors. Sealed source mismatches, controller-only document/writer/restore guards
and exact post-cleanup schema42 bytes are checked. Historical Wheat/Millet
Food stays equal to the ordinary control; Cabbage controls remain Food0.

The focused Debug test passes. Full validation uses separate task-owned Debug,
Release and ASan/UBSan roots with supplied Original-data inputs read-only and
serialized SDL suites. Debug and sanitizer retain assertions under `-g1 -O1`;
this does not change gameplay. Refresh matching independent core comparisons
after the additive World ABI and source/provider fingerprints. Private source
receipts qualify307 assertions over52 byte-checked windows including overlaps;
they are source evidence, not physical movement or human acceptance.

Executed on 2026-11-16: Release and ASan/UBSan each passed 360/360 cases without
skips. The full Debug run passed 358 cases; its two irrigation visual-profile
failures exposed a timestamp-only fixture directory collision between parallel
processes. The test now reserves its directory atomically and owns only that
directory's cleanup. Both affected cases passed their parallel rerun, so all
360 unique Debug cases pass. This QA fix changes no asset or gameplay code.
Fresh matching Debug and Release independent core comparisons each passed
74/74; their public reports are identical, and all non-fingerprint baseline
fields/classifications remain unchanged. Seven supplied Original/earned-fixture
inputs retained exact SHA-256, size, mode and mtime. Fresh historical Farm and
productive-walker captures were opened and inspected separately from autonomous
Actor26/native/human visual acceptance. Leak detection remains disabled on
macOS; UBSan runs with `halt_on_error=1` and assertions remain active.

The [completion matrix](original-economy/agriculture.md#autonomous-tending-acceptance-agri-02e3)
keeps the unique-candidate/one-live-actor/same-slot context separate from native
concurrency, score/configuration lifetime and positive worker persistence.
Actor callbacks/microsteps per World tick remain AUTHORED. No normal game
enables the controller automatically; there is no harvest/Food-production or
native/human Grower visual acceptance claim.

## Autonomous field-cycle acceptance (AGRI-02E4)

```sh
cmake -S . -B .local/agri-02e4/build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG='-g1 -O1' -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/agri-02e4/build-debug --parallel 4 \
  --target openemperor-autonomous-food-grower-world-tests \
  openemperor-autonomous-food-grower-field-cycle-tests
ctest --test-dir .local/agri-02e4/build-debug --output-on-failure \
  -R '^original-autonomous-food-grower-(world|field-cycle)$'
```

The explicit transient `AutonomousFieldCycleV1` test uses the shared controller,
source reader, World calendar and navigator. Independent authored map/model
fixtures supply paid Farm/field/house commands. Empty World ticks reach the
required slot; only then are the city and fields purchased. Regular immigration
earns24 residents and actual Farm staffing12. No actor, route result, field work,
harvest context, quantity or stock is injected.

Separate Wheat, Millet, Rice, Cabbage and Soy scenes prove quantity0 before real
tending51; literal work64→63 decay; first phase20 quantity157; actual end-slot /
subslot0 marks; due autonomous role2 allocation; actual shared cell movement and
field arrival; and25 subsequent eligible work invocations. The marked callback
then remains **BLOCKED** before mutation. Empty cancellation at the field is
explicitly not Return13, positive cargo or Farm credit. First-block latching
prevents further harvest allocation in that session while the field clock
continues to actual expiration. Counts retain field-quantity conservation;
history and actor cargo remain0. Ordinary historical Food/accounts stay exactly
equal to independent controls, including the unchanged Wheat/Millet adapter.

An independently restored second World replays complete transient state,
routes/cursors/waits/IDs against a third ordinary control. An actual source-road
cut during a begun harvest edge preserves and finishes that section before
cleanup; actor claims reject construction atomically. Source pause, paid
staff-priority loss and natural fire cannot substitute for completed work.
Multiple fields refuse admission, and the original tending policy retains its
same-slot fence. Active positive field quantity is refused by document creation,
writer, restore and Recovery without changing existing bytes/history metadata.
After cleanup, canonical schema42 and Recovery continuation match the ordinary
control exactly. Rule27/schema43 remain unused. Positive raw yield/Return13,
full-bin receipt/overflow and active-worker Save/Load remain **BLOCKED**, not
passing World cases; exact arithmetic/component tests are a separate tier.

Private source verifiers qualify the fresh Farm-WORD40 and single-candidate
harvest-factory trace, field/scheduler order, type/resource-veto distinction,
ordinary-map fertility recompute and dry120-cell irrigation branch. Actual
incoming PC/RC/normal FP state and other runtime numeric owners remain open.
Source evidence is distinct from paid World acceptance and native/human play.
The World tick/actor mapping, enabled-Food World scope and conservative
cancellation retain their explicit AUTHORED classification.

Executed on 2026-11-16 against the final field-cycle code: Debug, Release and
ASan/UBSan each passed361/361 cases without skips (213s,157s and709s). Debug
and sanitizer retain assertions with `-g1 -O1`; macOS leak detection is disabled
and UBSan uses `halt_on_error=1`. Both final independent core comparisons passed
74/74 and produce the same public report; all non-fingerprint historical
results/classifications remain unchanged. Seven supplied Original/earned-save
inputs retain exact SHA-256, size, mode and mtime. The two private source
verifiers qualify201 assertions over53 byte-checked windows including overlaps.
Fresh historical Farm and productive-walker regression captures were opened
and inspected; they do not establish new Actor26/native/human visual acceptance.
The positive raw-harvest definition of done remains BLOCKED/PARTIAL despite the
passing independent component and bounded World tests.

## Live harvest context tests (AGRI-02E5)

```sh
cmake -S . -B .local/agri-02e5/build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG='-g1 -O1' -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/agri-02e5/build-debug --parallel 4 \
  --target openemperor-original-harvest-admission-tests \
  openemperor-autonomous-food-grower-field-cycle-tests \
  openemperor-autonomous-food-grower-world-tests
ctest --test-dir .local/agri-02e5/build-debug --output-on-failure \
  -R '^(original-harvest-context-admission|original-autonomous-food-grower-(world|field-cycle))$'
```

The new admission test uses the existing actual paid scene and the unchanged
autonomous tending/growth/mark/harvest movement pipeline. Each of the five
Food models reaches a real field arrival,24 later worker invocations and wait1,
then the actual25th invocation. Read-only preparation returns immutable
bindings and precise current refusal domains. Repeated inspection leaves
World, field, native actor, movement and IDs exact. The actual refusal audit
survives retirement; field quantity/work/history and cargo receive no harvest
mutation. A later retired-lifetime query cannot acquire another context.

Compile-time checks reject public/default/aggregate construction and assignment
of the sealed result, including construction from a free Pure-Rule context.
Source tests read the actual signed lowWORD of Header+8, including ignored
highWORD and negative controls. Separate real sealed map identities exercise
SHA mismatch, and historical model-source mismatch is refused by restore.
Due work checks model hash/difficulty, Farm/field/crop/assignment, actual
arrived track, Source region, construction fertility, Q/work/C and exact120-cell
dry observations. Missing FP control/state, fertility epoch, region/factor,
date/global/resource and arithmetic domain are reported individually.
Rice/Cabbage's event predicatefalse does not populate invented Event values.

Actual paid dry-query cases place a Ditch at the last radius5 corner, where119
supported cells precede refusal, and outside radius6, where120 cells remain
dry. A source-native800 cell and unsupported source regions3/-1 refuse with
the actual offending coordinate/value. A paid Pump overlay is tested after a
dry control whose intake water lies outside the120-cell neighborhood. Worker
stop at actual wait1 and a newly paid field revoke work before a callback.
Another actual scene cuts its free Farm port after the first natural tending
return and repairs it in end-slot/subslot14. The real subslot15 harvest dispatch
starts its two-edge route before expiration, then arrives after phase50 has
cleared the mark/quantity/assignment. The resulting actual state12/wait0 returns
`MissingHarvestMark`; its next invocation records the refusal without4000,
history or cargo. No callback, movement result, calendar or wait is injected.
Independent replay preserves the complete transient failure audit and exact
historical controls. Document/writer/restore/Recovery rejection retains existing
bytes; after cleanup schema42 and Recovery continuation match the ordinary
World. Existing pure marks/repeated-callback/narrowing/carry/overflow vectors
remain separate from actual World resource acceptance.

The source receipts qualify the complete ordinary native Actor26 prefix after
the last timing call, actual startup/save/restore FP groups, Properties
preservation and later Play reload, date seed/update, fertility and region
owners, and Rice/Cabbage event vetoes. The precise remaining RC/FP capture plan
is maintained in the [source contract](reverse/original-agriculture-source-contract.md#live-harvest-context-admission-agri-02e5).
That hardware-breakpoint plan has not been executed. No Original process,
current FP environment or positive native harvest is inferred from these static
checks. Positive World callback/history/C remain BLOCKED/PARTIAL until actual
live numeric admission is available.

Executed on 2026-11-17: complete Debug, Release and ASan/UBSan suites each
passed362/362 cases without skips (160s,149s,703s). Debug/sanitizer retain
assertions with `-g1 -O1`; sanitizer uses `detect_leaks=0` on macOS and
`UBSAN_OPTIONS=halt_on_error=1`. Final independent Debug and Release core
comparisons each passed74/74, produce identical public reports and retain every
non-fingerprint baseline result/classification. The reviewed additive ABI now
pins15 headers and74 source fingerprints. Seven supplied Original/earned-save
inputs retained exact SHA-256, size, mode and mtime. Static source receipts
qualify238 assertions over86 byte-checked windows including overlaps. Fresh
historical Farm and productive-walker captures were opened and inspected.
Those regressions do not establish native/human Harvest acceptance. The full
positive-context/callback/Cargo definition of done remains BLOCKED/PARTIAL.

## Agriculture calendar and season observations (AGRI-06)

```sh
cmake --build build --parallel 4 --target openemperor-original-season-rules-tests \
  openemperor-agriculture-calendar-world-tests openemperor-season-calendar-presentation-tests
ctest --test-dir build --output-on-failure \
  -R '^original-(season-rules|agriculture-calendar-world|season-calendar-presentation)$'
```

Pure tests independently enumerate the nine reference plants' raw slot masks,
callback suppression and work-byte boundaries, including Hemp's unsuppressed
callback and Lacquer's unmatched end. Food delegates the existing `CropRules`.
World and presentation tests exercise paid construction, multiple elapsed
calendar cycles, boundary restores, pause/speed/step, operation and water facts,
actual retained production, locked crops and deterministic continuation.
Reference slot queries grant no crop work or harvest. Full seasonal production
and native human acceptance remain separate completion conditions.

AGRI-06 validation on2026-11-15 passed348/348 Release CTests with zero skips,
plus13/13 focused checks in each Debug and ASan/UBSan configuration. macOS
LeakSanitizer was excluded. The new pure rules passed4,200,456 checks. The paid
mixed-city test completed19,584 ticks, two full cycles, with actual Food/House
consumption, material Grower returns, water/pause/fire observations and exact
disk/Recovery continuation. The scripted SDL test passed boundaries, input and
pure frames. All nine1280/1600/1920 captures were opened and inspected. Fresh
independent core parity passed74/74 cases with unchanged non-fingerprint public
results. Of95 existing simulation/persistence paths,94 remain literal-equal to
merged AGRI-05. `World.h` adds one include and two const observations. The fresh
private Original-file guard passed66 source assertions across9 windows.

## Production evidence queries (OE55)

### Hunting and fishing components (AGRI-05)

The read-only inspector exposes verified source rules alongside the selected
model's raw columns through separate selectors.

```sh
cmake --build build --parallel 4 --target openemperor-model-inspect \
  openemperor-original-harvest-rules-tests openemperor-original-fishing-rules-tests
build/openemperor-model-inspect /path/to/emperor \
  --harvest-source 31 --harvest-source 33
ctest --test-dir build --output-on-failure \
  -R '^original-(harvest-rules|fishing-rules|model-inspector-cli)$'
```

`OriginalHarvestRulesTests.cpp` checks exact identities, staffing tiers and
capacity headroom, byte/signed-WORD counter order and wrap, supported raw receipt
effects and ordinary cart debit. All65,536 source selector values exercise the
proved initial-prey mapping. `OriginalFishingRulesTests.cpp` checks every Quay
orientation and extra dock claim, coordinates/overflow, all256 navigation bytes,
the distinct origin-flood and boat-distance predicates, exact caller-supplied
attribute planes and perimeter membership. Source planes remain immutable.

The inspector CLI suite uses independent model-only data with deliberately
different costs, employees and raw Resource columns. It verifies atomic sparse
queries, malformed/duplicate selectors, UTF-8 failure, all four Quay shapes,
explicit source evidence and locked gameplay/conversion/clock flags. Existing
default and `--building` JSON have literal expected controls. The new selector
reads model data and reports facts from the documented pinned Original revision.

Positive Hunter/Boat World-to-House acceptance remains blocked by the exact
ecology, source navigation, eligible callback, unit and quality/transport gates
in the [component matrix](original-economy/agriculture.md#hunting-and-fishing-component-matrix-agri-05).
Run the complete Release suite plus focused Debug and ASan/UBSan checks. Keep
their results distinct from real-asset rendering and native human acceptance.

AGRI-05 validation on 2026-11-15 passed345/345 Release CTests with zero skips,
and9/9 focused checks in each Debug and ASan/UBSan configuration. macOS
LeakSanitizer was disabled. The new pure tests passed182,487 harvest checks and
671 fishing checks. The inspector suite passed13 methods, and the real model
query resolved both source types with their independent Fish/Meat bindings.
Fresh independent core parity passed74/74 cases with all existing non-fingerprint
public results literal-equal. A direct comparison with merged AGRI-04 confirmed
121 gameplay, persistence and app source files byte-for-byte equal. Existing
generation/save/irrigation/plantation tests ran in the complete Release suite.
All five private investigation receipts passed their Original-file guards.
Playable new Hunter/Boat chains and native human acceptance remain open in the
completion matrix.

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

<a id="original-building-visuals-and-farmfield-boundaries-oe52oe60oe605"></a>

## Original building visuals and farm/field boundaries (OE52/OE60/OE60.5/OE61)

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
Schema4 has exactly23 roles: the same15 historical rows plus seven existing
Industry identities and Ancestor214. It adds no animation or stock/lifecycle
selector. Check that historical schemas1/2/3 still require exactly10/15/15 rows,
reject later identities and retain their37/39/56-image pools with the current
28-image core/House prefix. The current schema4 pool is64 images/5,534,168 RGBA
bytes. Source registrations, complete pixels, each actual side, atomic failure,
physical deduplication and the unchanged64MiB boundary are checked independently.
Its `-real` companion
uses the explicitly configured `OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT` and the
actual copied application resources. `original-building-selection-wiring`
checks automatic/custom/fingerprint precedence without claiming pixel acceptance.

Set `OPENEMPEROR_OE52_PAID_TEST_SAVE` to an explicitly supplied earned
Rule18/schema34/policy3 Xia save to register
`original-building-paid-production-view`. The complete66-body/46-courier city is
required; there are no original-data fixtures in Git. The test reserves a new
private output for each invocation; set
`OPENEMPEROR_ORIGINAL_VISUAL_OUTPUT_ROOT` to a fresh parent under `.local/oe61/`
to preserve closed earlier evidence. Historical OE52/OE60/OE60.5 output parents
remain supported. The immutable66/46 city now covers18 native bodies and its two
actually bought field kinds:180 size/zoom cases at1280×800/1600×900/1920×1080
and1×/2×/4×. Normal ground-owner selection remains distinct from the explicit F1
full-body/alpha probes. Each newly mapped body must expose an actual visible
source pixel strictly above the complete projected footprint, select its paid
BuildingId, click through a transparent point and display the prepared SG3
record in the Inspector. A fresh submitted F1 frame is required at all nine
size/zoom combinations. Readback uses an owned RGBA target before Present.
Open every generated PNG;
its `report.json` deliberately leaves visual review and native/human acceptance
unconfirmed. Complete World, F5/F9 bytes, historical profiles, F4, failed optional
replacement and all twelve pure-frame counters remain separate assertions.

Set `OPENEMPEROR_OE61_PAID_TEST_SAVE` to an independently preserved, normally
purchased68-body/48-channel extension that also contains Smelter39 and
Bronzeware42. It registers `original-building-industry-production-view`; the
original66/46 fixture is never rewritten. The extended test covers all20 static
body roles plus its two field kinds,198 size/zoom cases. Direct invocation is:

```sh
build/openemperor-original-building-visual-view-tests \
  /path/to/emperor "$PWD" /explicit/paid-68.save.json \
  "$PWD/.local/oe61/manual-view" --extended-paid-city
```

For an actual before/after comparison, compile this same test TU against the
frozen baseline headers/runtime and run it with the baseline metadata and
`--extended-paid-city --baseline`. Preserve the source, binary, libraries and
complete input SaveDocument fingerprints. The new eight roles must then remain
historical authored markers. Compare the matching report rows' complete World,
camera, aperture, selected ID and F1 state before comparing pixels; an F4 or
no-tail run of the new renderer alone is not a baseline binary. The
[OE61 contract](rendering/oe61-original-building-visuals.md) records the current
source/acceptance boundary alongside the historical
[OE52 assessment](rendering/oe52-original-visuals.md) and
[OE60 contract](rendering/oe60-original-farm-visuals.md).

```sh
ctest --test-dir build --output-on-failure -R '^original-(building-(visual-profile(-real)?|selection-wiring|paid-production-view|industry-production-view)|field-paid-growth-view|production-(world|view|save)|industry-(world(-real)?|save)|market-(transport|save)|fire-protection-(world|save|real)|food-(world|save)|hemp-(world|save)|house-visual-production-view)$'
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

`original-production-view` selects paid Warehouses and Markets after physical
Ceramics deliveries. It checks every displayed local Goods/material quantity,
the existing Market Food account, inventory-first ordering and first-row
visibility, and snapshot equality across selection and inspection.

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

## AGRI-04 irrigation graphics and UI

Rule26/schema42 must be selected explicitly. The regular menu offers
**G Agriculture Irrigation (rule26)**. The historical A Agriculture choice and
`--original-agriculture` retain Rule25/schema41. An equivalent explicit launch is:

```sh
build/openemperor --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 \
  --original-irrigation
```

Use the map's actual relative path if the installation uses a different filename.
A fresh Rule26 save has its own generated target; no schema40/41 target may be
silently overwritten or migrated. Test exact42 manual save/load and Recovery,
as well as refusal of old/mismatched contracts.

Build the irrigation targets in the configured Release, Debug and sanitizer
trees, then run their registered checks.

```sh
cmake --build build --parallel 2 --target \
  openemperor-original-irrigation-rules-tests \
  openemperor-irrigation-world-tests openemperor-irrigation-save-tests \
  openemperor-irrigation-visual-tests openemperor-irrigation-presentation-tests
ctest --test-dir build --output-on-failure -R 'irrigation'
```

`IrrigationVisualProfileTests.cpp` uses independently authored SG3/555 pixels.
It checks the complete36-Ditch/four-Pump static atlas, distinct wet/dry identities,
four physical orientation bindings, native geometry and anchors, bad pins and
paths, missing/duplicate/direction errors, budget refusal, physical deduplication
and atomic retention of the prior image pool. Repeated prepared selection must
perform zero file/decode/upload/World/navigation work. Its optional Original-data
check uses the production loader and requires exactly40 unique records totaling
762,008 logical RGBA bytes within the unchanged combined64 MiB budget:

```sh
build/openemperor-irrigation-visual-tests \
  "$PWD/.local/gog-extracted/app" \
  resources/compatibility/gog-derived-2.0.0.2-en-assetset-1/irrigation.json
build/openemperor-irrigation-presentation-tests --real \
  "$PWD/.local/gog-extracted/app"
```

`IrrigationPresentationTests.cpp` drives ordinary SDL actions against independent
source geography and positive paid model inputs. It checks visible action/hit
agreement at800×600,1280×800 and1920×1080; all four shared Pump rotations; actual
paid pointer construction; cancellation and actual-release commit of a bounded
Ditch drag; staffing-dependent supplied water; Pump inspection; wet/dry overlay;
physical Ditch removal and disconnected drying; exact42 F5/F9; and20 repeated
render frames with an unchanged World and zero resource/navigation counters.
Its Original-asset option changes only the two General files used for pixels.
Those read-only inputs are copied into a temporary fixture under ignored
`.local/agri-04/presentation/`, then removed. Captures remain there; **open and
inspect** the dry, wet-overlay and cut images. Source-image selection, scripted
SDL delivery, native delivery and human acceptance are separate results.

For a human playtest: choose the explicit irrigation mode, build a Pump with
Shift-P, rotate with R, and inspect the blue intake-cell outline. A missing
required water cell must refuse construction. Build a connected Ditch with
Shift-D and earn real workers from paid, road-connected Houses with actual
House/Water service routes. Pump supply uses the verified intake, assigned
workers, operation and fire state; check that only supplied sections become
wet after the scheduled update. Shift-V displays water status;
select the Pump or a field for concrete blockers. Pause the Pump, cut a Ditch
with Shift-X, restore the connection and exercise F5/F9. A restored or newly
eligible Pump must respect its scheduled supply boundary. Irrigation must never
unlock Rice/Cabbage/Soy or alter the existing Wheat/Millet and material-plantation
harvest adapters. Yield bonuses remain **PARTIAL / inactive**. Repeat historical
Rule24/25 production/save and earlier schema regressions independently.

### AGRI-04 validated scope, 2026-11-15

| Check | Result |
|---|---|
| Complete Release build and CTest |343/343 passed with configured Original inputs, zero skips |
| Focused Debug |14 distinct tests passed, with final irrigation and version-boundary reruns |
| ASan/UBSan |14 distinct tests passed, with final irrigation and version-boundary reruns; macOS LeakSanitizer disabled |
| Pure irrigation helpers |822 geometry, terrain, crossing and field-range checks |
| Historical schemas1–39 |123 complete SaveDocuments,25 DTO controls and four indexes compared byte-for-byte with protected references, totaling3,184,025 compared bytes |
| Historical schemas40–41 |15 complete SaveDocument pairs compared with frozen main8cee0fe, totaling3,030,744 document bytes; independent creator/restore processes cover growth, actual picked cargo, pause, returns, Wheat/Millet harvest and physical Food delivery |
| Independent core parity |74/74 cases passed; all existing non-fingerprint public fields and classifications remained literal-equal |
| Scripted SDL and Original sprites |Synthetic and real-asset runs passed through normal G/preflight/Loading/Playing startup, paid construction, water distribution, inspectors, overlay, removal and F5/F9; final2× captures opened |
| Original input protection |EXE, model, FarmConfig and both General asset files retained exact SHA-256, size, mode and modification time |
| Native human acceptance |Pending |

The first full run exposed five latest-version assertions that still reserved26
as unknown. Their revised controls admit the explicit26 lineage, keep historical
expectations and reject27. The final complete Release run passed after those
test-boundary updates. Pump presentation uses static verified poses. Immediate
eligibility/cut invalidation is the documented authored World adapter; numerical
crop irrigation effects and Original flood lifecycle remain PARTIAL/inactive.
