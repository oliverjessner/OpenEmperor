# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Alpha status

**OpenEmperor 0.1.0-alpha.1 is an experimental sandbox for external testing.** It is not a complete recreation of Emperor. The app uses original game files supplied by the user and never distributes those files or decoded copies. See [Known Issues](KNOWN_ISSUES.md) before testing.

## Requirements

- An Apple Silicon Mac running macOS
- A legally obtained, installed or extracted copy of _Emperor: Rise of the Middle Kingdom_; the GOG offline installer file itself is not a data folder
- For source builds: Xcode Command Line Tools, CMake, and either network access for the pinned SDL3 source or SDL3 3.4.10 or newer installed locally

## Quick start for testers

1. Open `OpenEmperor.app`, or build and run `./build/openemperor`.
2. Choose the installed or extracted Emperor game folder when prompted.
3. Select **New Sandbox** and a supported map.
4. Keep **sandbox-city-v7**, the recommended gameplay profile.
5. Enable **Demo** for a prepared settlement, then select **Start Sandbox**.

For the currently validated GOG-derived data set, OpenEmperor recognizes six original asset files by exact SHA-256 fingerprints and automatically enables its curated walker, building, and road preview. The bundled compatibility package contains metadata only; every displayed pixel is decoded at runtime from the tester’s own files. Other data versions remain usable with subdued fallback graphics. Advanced users can override each visual category with a local JSON profile.

For a local source build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --parallel
./build/openemperor
```

## What works

- Main-menu setup for user-supplied Emperor data and supported standalone maps
- The original OpenEmperor Industry-v5 sandbox with roads, two clay sources, two potteries, a warehouse, four households, and five couriers
- City-v7 Food production and delivery, dual-supply demand, developing Houses and tiered taxes
- City-v6 treasury, construction costs, deterministic workforce, household taxes, and its preserved four-house goal
- Road placement/removal, production, supply, routing and live rerouting
- OpenEmperor save/load and migration of its older schema versions
- Automatic curated walker, building, and road previews for one exactly fingerprinted GOG-derived data set, with safe fallback for other versions
- Native macOS arm64 app packaging with bundled runtime libraries

## Known limitations

OpenEmperor does not support original Emperor savegames, campaigns, the original economy, or complete map and rendering behavior. Curated visual mappings and the F2/F3/F4/F6/F7 controls are diagnostic previews. The development app is ad-hoc signed and not notarized. The complete current list is in [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Developer and research status

Run `./build/openemperor` to open the main menu. On first start, choose a directory containing your own installed or extracted Emperor files (the GOG installer itself is not such a directory). Choose a supported standalone map and one of the seven OpenEmperor sandbox rule profiles, then start an empty sandbox. City v7 is preselected for new settings; Demo arrangement is off unless selected. Escape or the visible Menu action retains the current World, camera, pause and speed state; Resume continues that same session. F5/Save writes a new OpenEmperor sandbox save under the application preferences folder. Load Save lists those files or opens an external OpenEmperor save through a native file dialog. Replacing a changed session or quitting asks whether to save, discard, or cancel. Direct CLI modes and their defaults remain available below.

On macOS, SDL stores `settings.json` and `saves/` under `~/Library/Application Support/OpenEmperor/OpenEmperor/` (the actual directory comes from `SDL_GetPrefPath("OpenEmperor", "OpenEmperor")`). The settings format is separate from sandbox saves. Original files stay read-only; no original assets or decoded copies are stored with saves. A removed data folder must be selected again. Bad or newer settings require an explicit reset before preferences are rewritten.

This repository contains an SDL3 application shell, read-only asset and map tools, and an interactive prototype sandbox. With `--data <directory> --sandbox Cities/Xia.map`, the app displays the user's original map as an unchanged background. The CLI default `sandbox-logistics-v1` profile offers one workshop, one warehouse, and a courier. Explicit `--sandbox-rules sandbox-production-v2` offers a resource-dependent Clay source → Pottery → Warehouse chain with two couriers, plus protected road removal and live rerouting. `sandbox-household-v3` extends the same simulation with one House and periodic Pottery consumption. `sandbox-settlement-v4` permits up to four independent houses, supplied cyclically by the same single warehouse courier; limited production may leave needs unmet. `sandbox-industry-v5` is the legacy alpha sandbox with a second Clay source and Pottery works. `sandbox-city-v6` preserves the first authored money, workers, taxes and settlement-goal loop. The recommended `sandbox-city-v7` adds one Farm, direct Food delivery, Food-and-Pottery Household demand, developing Houses, tiered taxes and a four-Level-2-House goal. Its Demo is a paid 794-fund two-house starter settlement; without Demo the world starts empty with 1000 funds. All profiles are self-defined prototypes, not Emperor's original economy. Existing `--map-debug`, browser, SG3 preview, and PNG export modes remain available. See [City-v7 rules](docs/city-v7.md), [City-v6 rules](docs/city-v6.md), and [the sandbox guide](docs/gameplay-sandbox.md).

The automatic compatibility preview, or an advanced custom `curated_walker_preview` JSON override, draws Clay, Pottery, and Household couriers with selected original SG3 sprite frames, directly loaded from your own game files. The path and direction still come from the OpenEmperor simulation; CourierRole selects the visual set. Missing roles or direction assignments show diagnostic markers. Schema 1 Clay-only profiles remain valid; schema 2 combines up to three roles and globally shares repeated physical images/textures. Use **Advanced visual previews** in the Sandbox menu or pass `--sandbox-visuals <walker-profile.json>` to override the automatic walker selection for one production-sandbox session. F2 switches all configured roles between sprites and markers without changing the World. The profile is a local preview convention, not a recovered Emperor walker registration or animation speed. See [walker visual profiles](docs/walker-visual-profile.md).

The metadata-only built-in schema-2 profile keeps the verified Clay clips and adds two bounded, visually inspected `DATA/SprMain.sg3` series for Pottery and Household previews. F3 opens an in-sandbox 1×/4× frame inspector; `V` changes role, `Q` direction, and `C`/`E` step frames. The ordinary decoder preserves the figures' exact RGB555 `0x7c00` literals as opaque red. For byte-59 flagged Omega sprites, a separately verified load-time presentation step turns only those literals into destination-darkening shadows; it is neither a global color key nor an alpha-address change. The machine-readable Industry-v5 check distinguishes configured, decoded, and actually drawn directions per role, compares a separate control World, and never claims a manual review. See [the bounded research record](docs/reverse/research-log.md).

The built-in compatibility package, or an advanced custom `curated_building_preview` JSON override, displays locally selected original Type-30 images for ClaySource, Pottery, Warehouse and Household; any omitted role keeps its marker. Use `--building-visuals .local/visuals/buildings.json` or **Building JSON...** under **Advanced visual previews** to override it. F4 switches all configured building images and markers independently of F2, and valid placement previews use a translucent original image. All prototype buildings still occupy one sandbox cell and retain the same simulation and save data, even when their images appear larger. See [building visual profiles](docs/building-visual-profile.md) for the separate manifest, anchors and depth limits.

The built-in compatibility package, or an advanced custom `curated_road_preview` JSON override, displays selected original Type-30 road-like tiles according to a roads-only `RoadNeighborMask`. Adjacent buildings are reported separately as `EntranceMask` and never change the selected road tile. The exact-fingerprint built-in pack covers all 16 masks; custom profiles may remain partial and then use fallback diamonds. Use `--road-visuals .local/visuals/roads.json` or **Road JSON...** under **Advanced visual previews** to override it. F6 independently returns to fallback roads. Presentation mode renders fallbacks in muted earth tones and F1 debug uses the bright yellow diagnostic color. Road placement, removal and drag preview recalculate appearances without changing BFS, commands, revisions or saves. The explicit mask convention and local manifest are documented in [road visual profiles](docs/road-visual-profile.md). This is a visual OpenEmperor mapping, not a recovered original Emperor road-selection rule.

Presentation mode is the normal sandbox default. It uses subdued terrain-derived colors for unresolved saved-map cells and muted but visible marker fallbacks. F1 switches to the brighter technical palette and shows the detected compatibility pack plus automatic/custom/fallback source for walker, building, and road visuals. F1 changes no simulation or save state. The header exposes Help, and the normal status line uses a moderately enlarged SDL debug-text scale without shrinking the map into a sidebar.

The sandbox now linearly merges saved-map images with roads, buildings and walkers by projected ground depth. Tall map images can appear in front of a walker whose ground lies behind them; F7 toggles the former whole-map-first draw order for comparison. Selection and invalid-placement indicators stay visible above the painter. This is an OpenEmperor whole-image preview, not a verified reconstruction of Emperor's object sorting, height or sprite splitting. F7 changes no simulation or save state.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

The textured map preview reads original map cells but uses a small, explicit local binding table to choose original SG3 graphics for exact `(terrain_raw, objects_raw)` pairs. Unbound cells are purple diagnostic diamonds. The map-to-image assignments and the 80×40 isometric placement are preview conventions; the original game's graphic variant logic, coastlines, animation, heights, buildings, and simulation are not reconstructed.

`openemperor-map-graphics` remains a read-only research diagnostic for the saved-word region. A direct index into `DATA/China_Terrain.sg3` failed on the local maps; the later, explicitly selected v213 runtime-table profile resolves saved graphic IDs through Terrain/Elevation registrations. The new `stored-graphics` viewer uses that same resolver to display a snapshot of the **saved** IDs, while the curated preview remains independent. It does not establish which values survive later original-game writes to the first draw.

The opt-in `exe-6373328b-v213-slot8-runtime-table` profile also registers slot 8 as `DATA/China_Mon_Earthen_Greatwall_1.sg3` for maps that reference it. This registration is backed by the same hash-checked v213 resource-manager path and is separate from the unchanged Terrain/Elevation base profile. The separate `--footprint-policy edge-byte-4x4` can preview complete, metadata-consistent 4×4 Type-30 Great Wall images without changing the default or earlier 2×2 policies. Incomplete or unsupported images remain diagnostic. The local evidence and limits are in [graphics-ID notes](docs/reverse/graphics-id.md).

On the local 167-map `edge-byte` check, this profile resolved all 12,416 previously unknown slot-8 cells. Only 96 additional cells were rendered; 12,096 require unsupported footprint placement and 224 have unsupported image layouts. Complete snapshots stayed at 57/167 and no earlier complete map regressed. These are diagnostics from the user's local data, not original-game visual fidelity. See the [per-map comparison](docs/stored-graphics-preview.md#slot-8-profile-comparison-2026-09-18).

With the separate `edge-byte-4x4` policy on the same local corpus, all 12,096 candidate cells in complete 4×4 wall groups became covered through 756 placed instances. The 224 other slot-8 layouts remain unsupported, and complete snapshots stay at 57/167. The original `edge-byte` results remain the baseline; see the [4×4 comparison](docs/stored-graphics-preview.md#opt-in-44-saved-footprint-preview-2026-09-18).

RGB555 decoding now follows the public SGReader's red-high/blue-low channel positions; the published sgfileio pixel table gives the opposite red/blue order. Fixed synthetic vectors cover the correction through decode, PNG export, and SDL software rendering. The previous local terrain-preview choice (image 655) was incorrectly described as sandy before this correction. The ignored local binding now uses a visually checked ochre tile (image 268) for one exact raw pair. Water remains unbound because the viewed blue candidates were not verified as water; no original-game pixel match has been established.

- ✅ C++20/SDL3 application and macOS arm64 build
- ✅ clean-room SG3 metadata parsing and `.555` range checks
- ✅ plain RGB555, Omega sprite, and Type-30 isometric image decoding
- ✅ optional Type-30 Omega overlay over the decoded footprint
- ✅ synthetic tests containing no proprietary game bytes
- ✅ direct SG3/.555 → RGBA → SDL3 preview, plus optional PNG export/debugging preview
- ✅ deterministic metadata-only asset catalog and CLI filters
- ✅ SDL3 visual asset browser with lazy thumbnails and bounded texture cache
- ✅ observed internal version-214 Type-256 alpha addressing and full-image decoding in the local GOG set
- ✅ isometric test scene with camera pan, pointer-centered zoom, ground-cell selection, and grid overlay
- ✅ bounded zlib map-container reader, map inspector, and raw 228×228 storage-grid view of original maps
- ✅ reference-derived terrain flags/categories, separate diamond candidate and off-map-bit masks, and a diagnostic bitmap projection
- ✅ exact-pair curated SG3 terrain preview with distinct unmapped diagnostics and shared textures
- ✅ opt-in stored-graphics map snapshot from saved IDs and the corrected v213 runtime layout
- ✅ separately opted-in placement of isolated complete 2×2 Type-30 footprints (preview convention)
- ✅ separately opted-in, reference-derived edge-byte grouping of touching 2×2 footprints
- ✅ fixed-step, deterministic logistics sandbox with placed roads, production, routed delivery, and return
- ✅ production-sandbox road removal, live rerouting, waiting with reserved cargo, and schema-4/5 save continuation
- ✅ opt-in resource-dependent Clay → Pottery chain with two independent couriers and delivery reservations
- ✅ opt-in Warehouse → Household courier supply, 400-tick Pottery consumption, and persisted unmet demand
- ⚠️ external and other unobserved alpha profiles remain unverified; no pixel-exact game comparison
- ❌ verified horizontal mirroring
- ❌ original-game-verified active cells, world coordinates, or map-to-SG3 graphics mapping
- ❌ original coastline/variant/animation logic, heights, buildings, and original-game simulation

## Build on macOS

For a local arm64 development app, install Xcode Command Line Tools, CMake,
SDL3, OpenSSL 3 and nlohmann/json, then run:

```sh
tools/package_macos.sh
open dist/OpenEmperor.app
```

The script uses a separate `build-macos-app/` Release build and writes the
verified bundle, ZIP, `package-report.json` and `SHA256SUMS` under `dist/`.
The app's normal Finder launch enters the same main menu as the CLI build.
Choose your own legally obtained installed/extracted Emperor data directory
on first launch. The bundle contains no original game files. Settings and
saves continue to use SDL's existing per-user OpenEmperor preference folder.
The ZIP is a local ad-hoc-signed developer preview, without Developer ID
signature or Apple notarization; downloaded copies may trigger Gatekeeper.
See [macOS packaging](docs/macos-packaging.md) for verification and limits.

The ordinary source build and direct `./build/openemperor` commands below
remain available and do not require the bundle option.
For GUI testing with disposable settings, pass `--app-root <temporary-directory>`
to the menu executable; a normal Finder launch continues to use the existing
per-user preference directory.

Install Xcode Command Line Tools, CMake, and OpenSSL (for example `brew install openssl@3`). The map reader also uses the system zlib library through CMake's `find_package(ZLIB REQUIRED)`. The default CMake configuration fetches a fixed SDL3 source commit and the checksum-pinned nlohmann/json 3.12.0 release if no exact system package exists; network access may be needed the first time.

The alpha scope has a separate, fail-closed acceptance runner. It builds and tests Debug, a fresh Release tree, and a fresh ASan/UBSan tree; runs deterministic 100,000-tick Industry-v5 and post-goal City-v6 endurance passes, 3,000 pure render frames, and 100 complete synthetic menu/session test processes; verifies the Release architecture; and writes only a path-scrubbed report to ignored `.local/reports/alpha-check.json`. These longer checks are intentionally outside ordinary CTest.

```sh
python3 tools/alpha_check.py --system-sdl
```

Add `--package` to include the macOS bundle validation. For the exactly fingerprinted data set, the original-data smoke automatically uses the bundled metadata-only profiles; no local visual JSON arguments are needed:

```sh
python3 tools/alpha_check.py --system-sdl --package \
  --data /path/to/emperor
```

For a sanitizer-only development build, use `-DOPENEMPEROR_ENABLE_SANITIZERS=ON`. This instruments OpenEmperor's own targets with AddressSanitizer and UndefinedBehaviorSanitizer on Clang/GCC-compatible toolchains; normal builds and fetched dependencies are not forced to use those flags.

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENEMPEROR_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

See [alpha readiness](docs/alpha-readiness.md) for the frozen scope, required checks, and known limits.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --parallel
file build/openemperor
./build/openemperor
./build/openemperor
```

For an offline local build with SDL3 3.4.10 or newer already installed, add `-DOPENEMPEROR_USE_SYSTEM_SDL3=ON` to the configure command. The default pinned dependency remains the reproducible build path.

```sh
./build/openemperor --data /absolute/path/to/your/game-data
./build/openemperor-inspect /path/to/a/file
./build/openemperor-inspect /path/to/file.sg3 --summary
./build/openemperor-inspect /path/to/file.sg3 --image 1 --rgba .local/decoded/image-1.rgba
./build/openemperor-inspect /path/to/file.sg3 --image 1 --png .local/decoded/image-1.png
./build/openemperor --preview .local/decoded/image-1.png
./build/openemperor --sg3 /path/to/your/game-data/example.sg3 --image 1
./build/openemperor-assets --data /path/to/your/game-data
./build/openemperor-assets --data /path/to/your/game-data --kind isometric --min-width 78
./build/openemperor-assets --data /path/to/your/game-data --kind sprite --with-alpha --json
./build/openemperor-assets --data /path/to/your/game-data --audit-alpha
./build/openemperor-assets --data /path/to/your/game-data --verify-alpha-decodes
./build/openemperor --data /path/to/your/game-data --browse-assets
./build/openemperor --data /path/to/your/game-data --browse-assets --ignore-alpha
./build/openemperor --data /path/to/your/game-data --road-atlas DATA/China_Terrain.sg3 --start 760 --count 160
./build/openemperor --data /path/to/your/game-data --scene .local/scenes/first-scene.json
./build/openemperor-map-inspect --data /path/to/your/game-data --list --json
./build/openemperor-map-inspect /path/to/your/game-data/Cities/Xia.map --json
./build/openemperor-map-graphics --data /path/to/your/game-data --map Cities/Xia.map --archive DATA/China_Terrain.sg3 --cell 114 114
./build/openemperor-map-graphics --data /path/to/your/game-data --map Cities/Xia.map --profile exe-6373328b-v213-runtime-table --cell 134 93
./build/openemperor-map-graphics --data /path/to/your/game-data --layout-profile exe-6373328b-v213-runtime-table --group-key 0x603 --variants 8 --graphic-id 0xc027 --graphic-id 0xc02a --graphic-id 0xc0c9
./build/openemperor-map-graphics --data /path/to/your/game-data --layout-profile exe-6373328b-v213-runtime-table --group-key 0x603 --variants 8 --map Cities/Xia.map --cell 114 114 --cell 115 114
./build/openemperor-map-graphics --data /path/to/your/game-data --map Cities/Xia.map --terrain-selection-profile exe-6373328b-v213-layout-terrain-probe --cell 114 114 --cell 115 114
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view semantic
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view projected
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view textured --terrain-bindings .local/terrain-bindings/preview.json
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Chengdu.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Banpo.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview --footprint-policy edge-byte
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Badaling.map --view stored-graphics --graphics-profile exe-6373328b-v213-slot8-runtime-table --multi-tile-preview --footprint-policy edge-byte
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Badaling.map --view stored-graphics --graphics-profile exe-6373328b-v213-slot8-runtime-table --multi-tile-preview --footprint-policy edge-byte-4x4
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-routing-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-save .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --load-sandbox .local/saves/quicksave.oesave.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-production-v2 --sandbox-check --sandbox-resume-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-household-v3 --sandbox-check --sandbox-resume-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-settlement-v4 --sandbox-demo --sandbox-check --sandbox-resume-check --report-json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v6 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v7 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-visuals .local/visuals/clay-walker.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-visuals .local/visuals/clay-walker.json --building-visuals .local/visuals/buildings.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-visuals .local/visuals/clay-walker.json --building-visuals .local/visuals/buildings.json --road-visuals .local/visuals/roads.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-visuals .local/visuals/walkers-v2.json --building-visuals .local/visuals/buildings.json --road-visuals .local/visuals/roads.json
./build/openemperor --data /path/to/your/game-data --sandbox Cities/Xia.map --sandbox-rules sandbox-industry-v5 --sandbox-demo --sandbox-check --sandbox-resume-check --report-json
ctest --test-dir build --output-on-failure
```

Sandbox `F5` saves and `F9` reloads the configured OpenEmperor save. Loading starts paused at the saved tick; Space resumes and `.` steps. In the production, household, settlement, industry, and city profiles, `6` selects road removal; an occupied road or begun edge is protected, and a courier waits with its cargo/reservation if the rest of its route is cut. Household-capable profiles add `7` to place a one-cell House; City-v7 adds `8` for its Farm. OpenEmperor's schema-1 through schema-7 JSON formats retain all seven sandbox profiles; schema 6 remains exclusive to City-v6 and schema 7 to City-v7 Food/economy state. Loading requires the same original map and matching decoded buildability mask. It is **not compatible with original Emperor savegames** and contains no original map or image bytes. Save files belong outside `--data`, such as ignored `.local/saves/`; see [save schema and routing rules](docs/gameplay-sandbox.md#openemperor-sandbox-saves).

The Sandbox has a clickable bottom tool/control bar, a compact status line and a collapsible building inspector on the right. Choose a building tool, then press and release on a buildable map cell. With Road selected, drag from the first to the last cell to preview an orthogonal X-then-Y connection; release on the map to place the entire valid path or reject it without changes. Existing sandbox roads are reused. Right-click or Escape cancels an open gesture; Escape without one exits. Tab toggles the inspector and F1 toggles the small debug readout. Press H or ? for the compact control guide. F2/F3/F4/F6/F7 are advanced visual diagnostics and are never required for normal play. Buttons and their keyboard shortcuts share actions; Save/Load buttons use the same configured path and validation as F5/F9. An unfinished preview and UI selection are never written to a save. The building list and map both select the actual instance, whose stock, progress and courier status appear in the inspector. The map remains available outside the top status bar, bottom bar and optional right panel; wheel zoom is map-only, while wheel over the panel scrolls it. See [sandbox mouse controls](docs/gameplay-sandbox.md) for gesture cancellation and rule limits.

`--data` checks that the directory exists and prints its absolute path. The application's `--sg3` and `--image` options must be supplied together and cannot be combined with `--preview`. The direct preview resolves the `.555` source, checks each **effective** range against the actual file size, reads only those ranges, and renders without writing an intermediary. For internal version-214 Type-256 records with nonzero alpha and stored `alpha_offset == data_offset + 2 * data_length`, the effective alpha starts at `data_offset + data_length`. This equality is a conservative observed profile marker; the original meaning of `alpha_offset` remains unknown. Other alpha-bearing profiles fail explicitly in normal loading. `openemperor-inspect` prints generic file metadata for other files and structured SG3 JSON with raw and effective alpha fields. Its `--image <index>` export can write headerless RGBA8 or PNG when requested. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

The local ignored GOG set contains 9,059 alpha-bearing records, all internal version-214 Type-256 Sprites. The normal loader decoded all 9,059 completely with the observed contiguous range; 52 representative RGBA results exactly matched explicit `CONTIGUOUS` diagnostics. Several images were also inspected in the normal SDL browser, including a 236×263 image previously affected by SPEC artifacts and a record whose stored alpha range exceeds EOF. This is **not** a pixel-exact comparison with the original game. No original external alpha-bearing image was available, so no external `-1` rule is used. See [the research log](docs/reverse/research-log.md).

For diagnosis only, `openemperor-assets --data <directory> --audit-alpha` prints aggregate counts by source, type, and archive plus common offset deltas. Add `--json` for per-record JSON Lines diagnostics. `openemperor --sg3 <file> --image <index> --alpha-addressing spec|contiguous|legacy` previews one image with an explicit addressing hypothesis; `--ignore-alpha` previews color only. The inspector accepts the same diagnostic options when explicitly exporting one image. Keep any such exports outside the repository and do not distribute them.

`openemperor-assets` recursively scans `.sg3` files (case-insensitive extension) without decoding images. It reports archive errors and separate color, raw-alpha, and effective-alpha range statuses, and supports combined `--kind`, `--type`, `--archive`, `--group`, `--with-alpha`/`--without-alpha`, `--min-width`, and `--min-height` filters. `--json` writes metadata to stdout using relative archive paths as stable IDs; it contains no pixels or absolute local asset paths. The existing `alpha_offset` field is the stored value; `alpha_bounds` now means the **effective** production range and is also exposed as `effective_alpha_bounds`. New fields `alpha_offset_raw`, `effective_alpha_offset`, `alpha_addressing_policy`, `alpha_profile_supported`, and `raw_alpha_bounds` make the distinction explicit. Catalog creation does not attempt decodes. `decoder_supported` indicates layout support independently of file bounds and actual decode results. The separate `--verify-alpha-decodes` command decodes supported alpha records one at a time without saving images.

The bounded `--road-atlas <relative.sg3> --start <record> --count <n>` developer view opens at most 512 records from one exact archive in the SDL asset browser. It shows each decoded image with its physical record, group, dimensions, type, overlay-byte count, mirror offset, alpha length, and animation count. It creates no image files; any manual screenshots or exports belong under ignored `.local/`.

The browser initially shows records with supported color metadata and valid color ranges, including supported alpha profiles whose effective alpha range is valid. Unverified profiles remain available through `4` (all candidates), where decode failures become placeholders. Keys `1`, `2`, and `3` select Plain, Sprite, and Type-30 images; `0` shows all kinds. Arrow keys or WASD move the selection, Page Up/Down move a page, Enter opens detail, and Escape returns to the grid or quits. Detail mode reports the selected policy and effective offset. Thumbnails exist only in memory, use nearest-neighbor scaling, and are cached up to 64 textures or 64 MiB (with a 16 MiB per-image limit). `--ignore-alpha` is a separate diagnostic color-only browser mode.

The scene viewer uses a deliberately authored JSON scene, not an Emperor map. The local `.local/scenes/first-scene.json` selects four graphics from the user's GOG data; it is ignored by Git. WASD/arrow keys pan, the wheel zooms around the cursor (0.5×–4×), left-click outlines a ground cell and shows its coordinates in the title, `G` toggles the grid, `R` recenters, and Escape quits. Terrain is drawn before objects, which are sorted by their frontmost footprint point with instance ID as a stable tie-breaker. This is sufficient for the simple, non-interlocking example, not arbitrary overlapping buildings. See [our scene format](docs/scene-format.md) for anchors, bounds, and budgets.

`openemperor-map-inspect` scans `.map`, `.pak`, and common save extensions under a selected data root without following symlinks. Extensions only select candidates: the container and each part are checked by content. A direct file inspection supports `--part <index>` and `--json`; multipart containers require a selected part for a full map read. JSON separates `container_valid` from `map_read_success` and reports physical/logical sizes, block counts, bounded raw and semantic frequencies, and per-file errors. It reports statistics for the full storage raster **separately** from the diamond candidate mask, plus all four combinations of candidate membership and the off-map terrain bit. A valid container alone does not establish a valid map. The app's `--map-debug` path must remain within `--data`; a multipart file additionally requires `--part`. The default `--view storage` keeps the original raw-value palette and `1`/`2` or Tab switches terrain/object layers. `--view semantic` colors coarse terrain categories such as water, vegetation, rock, road, fertility hint, and unknown; `--view projected` applies the referenced bitmap-coordinate transform for the five observed map sizes. `--view textured` applies only explicit local bindings; `--view stored-graphics` uses the separately selected saved-ID profile. `V` cycles views; `M` cycles full/candidate/off-map/compare mask diagnostics in raster views. WASD/arrows pan, the wheel zooms, left-click or Return selects a cell, `R` recenters, and Escape quits. Selection reports the original storage coordinate, raw decimal/hex words, recognized and unknown bits, category/rule, both mask statuses, and logical offsets. The categories and projection are **reference-derived, not verified against the original game**. The 228×228 storage raster and declared map size do not establish a playable rectangular world. Both texture modes use user-supplied SG3 graphics, create no intermediate image file, and add no game logic. See [map-format notes](docs/reverse/map-format.md).

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.

The textured command requires a local binding file (see [terrain binding format](docs/terrain-bindings.md)); no original or decoded assets are bundled. `V` cycles through the available map views. The app prints candidate, bound, unmapped, excluded, loaded-asset, and per-binding counts when it loads a textured map. A missing or damaged referenced asset fails as `asset_error`, never as an unmapped fallback. The binding file under `.local/` is ignored by Git.

The separate `--view stored-graphics` command requires an explicit `--graphics-profile` and no binding file. The base `exe-6373328b-v213-runtime-table` profile reads unchanged saved words at candidate storage cells and resolves only the studied v213 Terrain/Elevation slots; the separately named slot-8 profile adds the evidenced Great Wall registration. By default it draws supported one-cell Type-30 images, with full decoded height and preview origin `world − (width/2,height−40)`; unsupported sizes remain diagnostic. Add `--multi-tile-preview` to place only isolated, complete 2×2 clusters with matching saved IDs and confirmed 158-wide/12,800-byte Type-30 geometry. This explicitly chosen preview rule draws one image for four retained, individually selectable cells at `project(rear/top cell) − (width/2,height−80)`. It does not identify an original-game anchor or interpret `candidate_byte`. Touching same-ID clusters and incomplete/mismatched footprints remain purple diagnostic cells. On the local GOG samples it places one 2×2 instance in Chengdu, six in Xia, nine in Banpo, and fifteen in Anyi; Banpo retains eight ambiguous cells. The view checks source paths under `--data`, limits distinct referenced assets to 2,048, payload/image size to 16 MiB, and total logical RGBA textures to 64 MiB. Each distinct supported asset is decoded and uploaded once. The first viewport's texture/diagnostic draws and separate cell/instance/asset counters are printed. Selection reports original cell values and offsets plus any owning footprint. This is a saved-state diagnostic snapshot, not a reconstruction of the original first draw. See [stored graphics preview](docs/stored-graphics-preview.md).

The optional `openemperor-map-graphics --profile exe-6373328b-v213-runtime-table` diagnoses a version-specific 14-bit graphics-ID interpretation from bounded static analysis of a locally supplied EXE. A shared, SDL-free runtime layout resolves both image IDs and group keys for the studied v213 Terrain/Elevation registration. The loader compares the **first SG3 bitmap-group filename** with `Zeus_system.bmp`; both examined archives match. It skips 200 system records plus physical dummy record 0, so runtime local image `i` selects physical SG3 record `201+i`, with a runtime count of `reported_images_in_use−200`. Physical catalog IDs do not change. The map-load path copies stored candidate words into a per-cell array used by a related lookup, but later routines can replace them; the value and composition at first draw remain unverified. The new opt-in snapshot displays the saved values only. A local single-image preview is `./build/openemperor --sg3 /path/to/China_Terrain.sg3 --image 240` for the conditional resolution of `0xc027`. See [graphics-ID evidence](docs/reverse/graphics-id.md) for addresses and limits.

The separate `--terrain-selection-profile exe-6373328b-v213-layout-terrain-probe` compares selected simple-ground cells with their stored graphic IDs and conditional SG3 decode. It deliberately reports no computed ID: the observed range writer runs before the stored map graphic array is read, and the path from those saved values to the first draw is not yet complete. It does not alter the curated preview or imply a recovered terrain rule.

The explicit `--layout-profile exe-6373328b-v213-runtime-table --group-key 0x603 --variants 8` query uses the same layout as image resolution. Retained signed-positive index words stay in **file order**; the Terrain word at SG3 offset 86 is 247, yielding packed base `0xc02e`. Variants 0–7 decode physical Terrain records 247–254. `--graphic-id` adds a full selected-ID resolution; optional `--map`/`--cell` compares untouched saved values. The sampled Xia, Banpo, Chengdu, and Anyi cells do not match the group's 0–7 range. The older profile names are rejected as superseded. This query does not establish loaded-map drawing behavior or reproduce the generator state. See [graphics-ID evidence](docs/reverse/graphics-id.md).

For touching 2×2 saved-image instances, `--multi-tile-preview --footprint-policy edge-byte` selects a separate, reference-derived grouping rule. It preserves the raw byte and groups only validated four-part, same-ID and same-AssetId footprints; the upper bit remains unknown and the draw marker is diagnostic. On the locally checked Banpo map this yields eleven 2×2 instances and covers the eight cells that remain ambiguous under the default isolated policy. This does not establish the original game's first draw. See [preview details](docs/stored-graphics-preview.md) and [evidence](docs/reverse/graphics-id.md).

`--multi-tile-preview --footprint-policy edge-byte-4x4` retains those 1×1/2×2 rules and additionally groups only complete, nonconflicting 16-part Type-30 images with 318-pixel width, a 51,200-byte base, size flag 4, valid source, and no unverified mirroring. The full image, including any Omega overlay, becomes one SDL texture draw per instance while all 16 saved cells remain selectable. This is an opt-in snapshot convention, not the original game's wall construction or first-draw composition.

## Standalone map browser and compatibility check

Use your own GOG game-data tree. The map browser discovers standalone `.map` files by case-insensitive extension, checks their container/map profile without decoding images, and starts every row as `not_checked`. It keeps the existing `stored-graphics` profile and an explicit footprint policy; Escape returns from an opened map to the list, while Escape in the list quits. SDL's built-in debug font shows ASCII only, so a non-ASCII display label may be simplified while the underlying relative path remains unchanged.

```sh
./build/openemperor --data .local/gog-extracted/app --browse-maps --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview --footprint-policy edge-byte
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Banpo.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview --footprint-policy edge-byte
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Banpo.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview --footprint-policy edge-byte --render-check --report-json
python3 tools/verify_map_corpus.py --binary build/openemperor --data .local/gog-extracted/app --report .local/reports/map-compatibility.json --timeout 30 --build-type Debug --footprint-policy edge-byte
```

The noninteractive check uses a bounded 1280×720 SDL dummy/software renderer, the same map plan, SG3 decoder, texture uploads, and map view as the browser. It draws an overview and a zoomed frame, writes one JSON object to stdout, and exits. `--list-maps --report-json` exposes the shared catalog to the local runner. The runner launches each map in its own timed subprocess and saves results after every file under ignored `.local/reports/`. `snapshot_complete` means both frames succeeded and every cell in the selected **candidate mask** was covered; `snapshot_partial` means frames succeeded but at least one candidate remains diagnostic. `unsupported_profile`, `load_failed`, `render_failed`, `timeout`, and `process_failed` remain distinct. Neither status asserts playable boundaries, visual fidelity to the original game, or first-draw composition.
