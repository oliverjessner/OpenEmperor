# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Current status

This repository contains an SDL3 application shell, a read-only SG3 metadata inspector, a metadata-only asset catalog, a visual asset browser, a one-image RGBA/PNG exporter, a small isometric test-scene viewer, and a read-only original-map debug view. With `--sg3 <file.sg3> --image <index>`, the app loads one supported image directly from the user's SG3/.555 files and renders it in an SDL3 texture. With `--data <directory> --browse-assets`, it inventories SG3 files recursively and shows lazily decoded thumbnails. With `--data <directory> --scene <scene.json>`, it composes a locally defined still scene from selected images. With `--data <directory> --map-debug <relative.map>`, it reads one supported original map and can show raw storage values, coarse reference-derived terrain categories, a diagnostic bitmap projection, or a curated textured preview. Supported color layouts are plain RGB555, Omega sprite streams, and Type-30 isometric footprints (classic 58×30 and Emperor 78×40 tiles, with optional Omega color overlay). The observed internal version-214 Type-256 alpha profile is supported; other alpha profiles remain unverified. `--preview <exported.png>` remains available for debugging. There is no game simulation.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

The textured map preview reads original map cells but uses a small, explicit local binding table to choose original SG3 graphics for exact `(terrain_raw, objects_raw)` pairs. Unbound cells are purple diagnostic diamonds. The map-to-image assignments and the 80×40 isometric placement are preview conventions; the original game's graphic variant logic, coastlines, animation, heights, buildings, and simulation are not reconstructed.

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
- ⚠️ external and other unobserved alpha profiles remain unverified; no pixel-exact game comparison
- ❌ verified horizontal mirroring
- ❌ original-game-verified active cells, world coordinates, or map-to-SG3 graphics mapping
- ❌ original coastline/variant/animation logic, heights, buildings, and simulation

## Build on macOS

Install Xcode Command Line Tools and CMake. The map reader also uses the system zlib library through CMake's `find_package(ZLIB REQUIRED)`. The default CMake configuration fetches a fixed SDL3 source commit and the checksum-pinned nlohmann/json 3.12.0 release if no exact system package exists; network access may be needed the first time.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --parallel
file build/openemperor
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
./build/openemperor --data /path/to/your/game-data --scene .local/scenes/first-scene.json
./build/openemperor-map-inspect --data /path/to/your/game-data --list --json
./build/openemperor-map-inspect /path/to/your/game-data/Cities/Xia.map --json
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view semantic
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view projected
./build/openemperor --data /path/to/your/game-data --map-debug Cities/Xia.map --view textured --terrain-bindings .local/terrain-bindings/preview.json
ctest --test-dir build --output-on-failure
```

`--data` checks that the directory exists and prints its absolute path. The application's `--sg3` and `--image` options must be supplied together and cannot be combined with `--preview`. The direct preview resolves the `.555` source, checks each **effective** range against the actual file size, reads only those ranges, and renders without writing an intermediary. For internal version-214 Type-256 records with nonzero alpha and stored `alpha_offset == data_offset + 2 * data_length`, the effective alpha starts at `data_offset + data_length`. This equality is a conservative observed profile marker; the original meaning of `alpha_offset` remains unknown. Other alpha-bearing profiles fail explicitly in normal loading. `openemperor-inspect` prints generic file metadata for other files and structured SG3 JSON with raw and effective alpha fields. Its `--image <index>` export can write headerless RGBA8 or PNG when requested. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

The local ignored GOG set contains 9,059 alpha-bearing records, all internal version-214 Type-256 Sprites. The normal loader decoded all 9,059 completely with the observed contiguous range; 52 representative RGBA results exactly matched explicit `CONTIGUOUS` diagnostics. Several images were also inspected in the normal SDL browser, including a 236×263 image previously affected by SPEC artifacts and a record whose stored alpha range exceeds EOF. This is **not** a pixel-exact comparison with the original game. No original external alpha-bearing image was available, so no external `-1` rule is used. See [the research log](docs/reverse/research-log.md).

For diagnosis only, `openemperor-assets --data <directory> --audit-alpha` prints aggregate counts by source, type, and archive plus common offset deltas. Add `--json` for per-record JSON Lines diagnostics. `openemperor --sg3 <file> --image <index> --alpha-addressing spec|contiguous|legacy` previews one image with an explicit addressing hypothesis; `--ignore-alpha` previews color only. The inspector accepts the same diagnostic options when explicitly exporting one image. Keep any such exports outside the repository and do not distribute them.

`openemperor-assets` recursively scans `.sg3` files (case-insensitive extension) without decoding images. It reports archive errors and separate color, raw-alpha, and effective-alpha range statuses, and supports combined `--kind`, `--type`, `--archive`, `--group`, `--with-alpha`/`--without-alpha`, `--min-width`, and `--min-height` filters. `--json` writes metadata to stdout using relative archive paths as stable IDs; it contains no pixels or absolute local asset paths. The existing `alpha_offset` field is the stored value; `alpha_bounds` now means the **effective** production range and is also exposed as `effective_alpha_bounds`. New fields `alpha_offset_raw`, `effective_alpha_offset`, `alpha_addressing_policy`, `alpha_profile_supported`, and `raw_alpha_bounds` make the distinction explicit. Catalog creation does not attempt decodes. `decoder_supported` indicates layout support independently of file bounds and actual decode results. The separate `--verify-alpha-decodes` command decodes supported alpha records one at a time without saving images.

The browser initially shows records with supported color metadata and valid color ranges, including supported alpha profiles whose effective alpha range is valid. Unverified profiles remain available through `4` (all candidates), where decode failures become placeholders. Keys `1`, `2`, and `3` select Plain, Sprite, and Type-30 images; `0` shows all kinds. Arrow keys or WASD move the selection, Page Up/Down move a page, Enter opens detail, and Escape returns to the grid or quits. Detail mode reports the selected policy and effective offset. Thumbnails exist only in memory, use nearest-neighbor scaling, and are cached up to 64 textures or 64 MiB (with a 16 MiB per-image limit). `--ignore-alpha` is a separate diagnostic color-only browser mode.

The scene viewer uses a deliberately authored JSON scene, not an Emperor map. The local `.local/scenes/first-scene.json` selects four graphics from the user's GOG data; it is ignored by Git. WASD/arrow keys pan, the wheel zooms around the cursor (0.5×–4×), left-click outlines a ground cell and shows its coordinates in the title, `G` toggles the grid, `R` recenters, and Escape quits. Terrain is drawn before objects, which are sorted by their frontmost footprint point with instance ID as a stable tie-breaker. This is sufficient for the simple, non-interlocking example, not arbitrary overlapping buildings. See [our scene format](docs/scene-format.md) for anchors, bounds, and budgets.

`openemperor-map-inspect` scans `.map`, `.pak`, and common save extensions under a selected data root without following symlinks. Extensions only select candidates: the container and each part are checked by content. A direct file inspection supports `--part <index>` and `--json`; multipart containers require a selected part for a full map read. JSON separates `container_valid` from `map_read_success` and reports physical/logical sizes, block counts, bounded raw and semantic frequencies, and per-file errors. It reports statistics for the full storage raster **separately** from the diamond candidate mask, plus all four combinations of candidate membership and the off-map terrain bit. A valid container alone does not establish a valid map. The app's `--map-debug` path must remain within `--data`; a multipart file additionally requires `--part`. The default `--view storage` keeps the original raw-value palette and `1`/`2` or Tab switches terrain/object layers. `--view semantic` colors coarse terrain categories such as water, vegetation, rock, road, fertility hint, and unknown; `--view projected` applies the referenced bitmap-coordinate transform for the five observed map sizes. `--view textured` applies only explicit local bindings to candidate storage cells. `V` cycles views, `M` cycles full/candidate/off-map/compare mask diagnostics, WASD/arrows pan, the wheel zooms, left-click or Return selects a cell, `R` recenters, and Escape quits. Selection reports the original storage coordinate, raw decimal/hex words, recognized and unknown bits, category/rule, both mask statuses, and logical offsets. The categories and projection are **reference-derived, not verified against the original game**. The 228×228 storage raster and declared map size do not establish a playable rectangular world. Only the textured preview uses selected user-supplied SG3 graphics; it creates no intermediate image file and adds no game logic. See [map-format notes](docs/reverse/map-format.md).

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.

The textured command requires a local binding file (see [terrain binding format](docs/terrain-bindings.md)); no original or decoded assets are bundled. `V` cycles through the available map views. The app prints candidate, bound, unmapped, excluded, loaded-asset, and per-binding counts when it loads a textured map. A missing or damaged referenced asset fails as `asset_error`, never as an unmapped fallback. The binding file under `.local/` is ignored by Git.
