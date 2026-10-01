# City-v11-v4 demolition validation

This local candidate starts from `a959d2250e9132ca17bb45ec833c6ff60f495510`. The initial worktree was clean. Display version remains `0.1.0-alpha.2`; demolition requires City-v11 rule version 4. New-game defaults remain v3 pending the native replanning acceptance; v4 testing uses an explicit copy upgrade or restore. Nothing was committed, tagged or published.

## Save decision

Schema 12 already permits variable entity collections, but historical production, consumption and tax contributions were attached to active buildings. Erasing a used building would otherwise invalidate conservation, and reconstructing construction spending from remaining buildings would lose paid demolition costs. Schema 13 therefore persists bounded aggregate `demolition_history` accounting, with no stocks, population, refunds or entity tombstones. Schema 12 remains City-v11-v3 only. Ordinary v1/v2/v3 load preserves its version; the menu's separately confirmed **Enable demolition in a copy** preserves all v3 authority and leaves its source untouched.

## Automated coverage

`SandboxCityV11DemolitionTests.cpp` constructs synthetic Worlds through normal commands and ticks, with one explicitly identified synthetic developed-House restore fixture. It checks:

- All seven supported building kinds, exact 1×1/2×2 footprint release, owned idle-courier deletion, no refund, preserved historical spending and fresh monotone IDs after restore/rebuild.
- All seven role limits, including a funded 38-building/22-courier city. Funding comes from actual supplied Household taxes.
- Outbound and returning couriers, inbound targets/reservations, stocks, a paused recipe at progress zero, normal production/delivery/demand draining and retained lifetime goods/tax accounting.
- Immediate population/workforce loss, fresh House development, Service coverage lasting to its original expiry, an unaffected active trip in another district, and additional actual tax income in that district after demolition.
- Exact schema-13 roundtrip, unchanged v3 source, explicit copy upgrade, malformed history rejection and legacy demolition rejection.
- Two Worlds matching for 20,000 ticks with rebuild commands and periodic restore. The separate 100,000-tick run performs 19 used Service Post pause/return/drain/demolish/rebuild cycles and checks goods, food, economy, service, population, navigation, IDs and cache bounds. It does not claim 19 cycles of every stock-bearing role; the finite controlled-drain test covers those roles.

The SDL software-renderer test drives real SDL events through the production view: inspector selection, Cancel/Escape/focus-loss, modal tick blocking, MouseDown/MouseUp mismatch, confirmation, blocked stock, footprint release, sprite disappearance and shared textures. Cancel preserves a clean view and manual save generation; confirmation marks it dirty. F5 saves the demolished state, and F9 restores that exact state paused after further rebuilding and production. Recovery capture is unsafe while the confirmation is open. The measured demolition performs zero World copies, asset decodes or texture uploads and one route refresh. Existing road preview/batch tests remain part of the suite. Menu and Recovery tests include v3→v4 copy, protected pre-demolition start and a post-demolition checkpoint.

Debug, Release and ASan/UBSan full CTest runs passed **49/49**. Additional focused demolition runs cover the final stricter inventory/zero-progress-recipe assertions. The 100,000-tick run passed in all three configurations. On macOS ASan was run with `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer is unsupported on this host, so this is not a LeakSanitizer result. No compiler warnings were reported.

`tools/package_macos.sh` built the separate Release bundle, ran its tests, checked dependencies/signatures/relocation, and exercised original-data loading and process-restart persistence. Both the CLI and app executable report `Mach-O 64-bit executable arm64`. The dirty-worktree bundle is a local test candidate only. It contains no original data, decoded assets, settings or recovery history.

Reproduce the checks from the repository root (the local runs used installed SDL3 with `OPENEMPEROR_USE_SYSTEM_SDL3=ON`; omit that option for the pinned dependency build):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/openemperor-sandbox-city-v11-demolition-tests --endurance

cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_USE_SYSTEM_SDL3=ON -DOPENEMPEROR_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 ./build-sanitize/openemperor-sandbox-city-v11-demolition-tests --endurance

tools/package_macos.sh
./build-macos-app/openemperor-sandbox-city-v11-demolition-tests --endurance
file build/openemperor dist/OpenEmperor.app/Contents/MacOS/OpenEmperor
```

## Native manual test and remaining acceptance

The real app was launched on the locally supplied Xia map. Rule 4, 1,300 starting funds, a paid House with six residents/workers, and inspector blockers were observed. In a separate native test instance, F5 saved a schema-13 World and, after quitting/restarting the app, F9 loaded its exact tick 461 paused with 1,300 funds. That restart sample is an empty World, not a post-demolition town.

The complete mouse-driven rebuild/replanning acceptance is **still pending**. A separate native test instance with `SDL_EVENT_LOGGING=2` established that the automation tool's coordinate clicks arrived as mouse-button down/up events with `x=0, y=0`, including a click intended for the existing Help button. Keyboard actions worked. This is a limitation of the test input, not evidence of a production mouse-handling defect. That paid prepared-starter instance reached tick 708 and 175 funds through actual supply/taxes, then was paused. No successful native mouse-driven demolition, Warehouse draining/removal or Market relocation is claimed. Those paths passed synthetic core/SDL tests, which do not replace the requested manual playthrough. No screenshots or original pixels are stored here.

Run the actual candidate from the repository root:

```sh
./dist/OpenEmperor.app/Contents/MacOS/OpenEmperor \
  --data "$PWD/.local/gog-extracted/app"
```

Create a City-v11 prepared starter, pause and F5 save. Return to the menu, select that save in **Load Sandbox**, and confirm **Enable demolition in a copy**. The copied session starts paused on rule 4/schema 13 with a separate manual save target. Use Select (5) and the building inspector. In a disposable saved city, cancel once, then demolish a fresh empty misplaced building and rebuild. Remove an empty House and observe residents/workers leave. Verify a stocked Warehouse is blocked, stop upstream production and use ordinary delivery/demand to empty it, then remove it after all trips return. Empty and rebuild a Market at a new location with new roads if needed. F5 save, quit the entire app, restart with the same command and load the v4 copy through **Load Sandbox**: deleted IDs must remain absent, new IDs/stock/treasury must match and the city must resume normally. Never destroy a player's personal city merely to perform this checklist.
