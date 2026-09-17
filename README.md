# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Current status

This repository contains an SDL3 application shell, a read-only SG3 metadata inspector, a metadata-only asset catalog, a visual asset browser, and a one-image RGBA/PNG exporter. With `--sg3 <file.sg3> --image <index>`, the app loads one supported image directly from the user's SG3/.555 files and renders it in an SDL3 texture. With `--data <directory> --browse-assets`, it inventories SG3 files recursively and shows lazily decoded thumbnails. Supported color layouts are plain RGB555, Omega sprite streams, and Type-30 isometric footprints (classic 58×30 and Emperor 78×40 tiles, with optional Omega color overlay). A version-214 Omega alpha decoder is implemented, but validation against the available original alpha-bearing files is incomplete; see the limitation below. `--preview <exported.png>` remains available for debugging. There is no map loading or game logic.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

- ✅ C++20/SDL3 application and macOS arm64 build
- ✅ clean-room SG3 metadata parsing and `.555` range checks
- ✅ plain RGB555, Omega sprite, and Type-30 isometric image decoding
- ✅ optional Type-30 Omega overlay over the decoded footprint
- ✅ synthetic tests containing no proprietary game bytes
- ✅ direct SG3/.555 → RGBA → SDL3 preview, plus optional PNG export/debugging preview
- ✅ deterministic metadata-only asset catalog and CLI filters
- ✅ SDL3 visual asset browser with lazy thumbnails and bounded texture cache
- ⚠️ version-214 alpha stream implemented and synthetic-tested; real-data appearance remains unresolved
- ❌ verified horizontal mirroring
- ❌ map format and scene renderer
- ❌ game simulation

## Build on macOS

Install Xcode Command Line Tools and CMake. The default CMake configuration fetches a fixed SDL3 source commit and builds it locally; network access is needed the first time.

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
./build/openemperor --data /path/to/your/game-data --browse-assets
./build/openemperor --data /path/to/your/game-data --browse-assets --ignore-alpha
```

`--data` checks that the directory exists and prints its absolute path. The application's `--sg3` and `--image` options must be supplied together and cannot be combined with `--preview`. The direct preview resolves the documented internal or external `.555` source, validates color and alpha ranges independently against its actual size, reads only those ranges, and renders without writing an intermediary. A nonempty version-214 alpha stream is read from its own `alpha_offset`/`alpha_length`, never inferred to follow the color data. Unsupported image types, out-of-bounds ranges, and malformed streams fail explicitly; the parsed horizontal-mirror offset is not applied. `openemperor-inspect` prints generic file metadata and the first 32 bytes in hex for other files. For a `.sg3` input, it prints structured JSON with the header, index, groups, image records, associated `.555` files, raw unknown fields, and separate color/alpha bounds results. `--summary` produces compact type/kind and alpha counts. Its `--image <index>` export can write headerless RGBA8 with `--rgba <output>` or an RGBA PNG with `--png <output>`. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

The documented alpha algorithm passes synthetic tests for Plain, Sprite, and Type-30 images, but **full compatibility with the available original alpha data is not established**. A read-only addressing audit of the ignored local GOG files found that all 9,059 alpha-bearing records are internal Type-256 Sprites. The current SPEC offset yields 1,141 syntactically valid streams; the diagnostic CONTIGUOUS offset yields 9,059, with more coherent small visual samples. This audit has **not changed the default decoder**. No original external alpha-bearing image was available to validate the legacy `-1` hypothesis. See [the research log](docs/reverse/research-log.md) for counts and limitations.

For diagnosis only, `openemperor-assets --data <directory> --audit-alpha` prints aggregate counts by source, type, and archive plus common offset deltas. Add `--json` for per-record JSON Lines diagnostics. `openemperor --sg3 <file> --image <index> --alpha-addressing spec|contiguous|legacy` previews one image with an explicit addressing hypothesis; `--ignore-alpha` previews color only. The inspector accepts the same diagnostic options when explicitly exporting one image. Keep any such exports outside the repository and do not distribute them.

`openemperor-assets` recursively scans `.sg3` files (case-insensitive extension) without decoding images. It reports archive errors and separate color/alpha range statuses, and supports combined `--kind`, `--type`, `--archive`, `--group`, `--with-alpha`/`--without-alpha`, `--min-width`, and `--min-height` filters. `--json` writes metadata to stdout using relative archive paths as stable IDs; it contains no pixels or absolute local asset paths. Catalog creation does not attempt decodes. Its `color_decoder_supported` field describes the known color layout; `decoder_supported` conservatively excludes alpha-bearing records because their real-data semantics remain unresolved.

The browser initially shows records with an available color range and supported color metadata, excluding alpha-bearing records while the discrepancy remains open. Keys `1`, `2`, and `3` select Plain, Sprite, and Type-30 images; `0` shows all kinds and `4` toggles all candidates, including missing sources and alpha records, whose failed decodes appear as placeholders. Arrow keys or WASD move the selection, Page Up/Down move a page, Enter opens detail, and Escape returns to the grid or quits. The selected asset's metadata appears in the title and detail output. Thumbnails exist only in memory, use nearest-neighbor scaling, and are cached up to 64 textures or 64 MiB (with a 16 MiB per-image browser limit). `--ignore-alpha` is a diagnostic color-only view, not a format correction or default behavior.

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.
