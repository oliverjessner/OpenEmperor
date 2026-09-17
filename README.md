# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Current status

This repository contains an SDL3 application shell, a read-only SG3 metadata inspector, a metadata-only asset catalog, a visual asset browser, and a one-image RGBA/PNG exporter. With `--sg3 <file.sg3> --image <index>`, the app loads one supported image directly from the user's SG3/.555 files and renders it in an SDL3 texture. With `--data <directory> --browse-assets`, it inventories SG3 files recursively and shows lazily decoded thumbnails. Supported color layouts are plain RGB555, Omega sprite streams, and Type-30 isometric footprints (classic 58×30 and Emperor 78×40 tiles, with optional Omega color overlay). The observed internal version-214 Type-256 alpha profile is supported; other alpha profiles remain unverified. `--preview <exported.png>` remains available for debugging. There is no map loading or game logic.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

- ✅ C++20/SDL3 application and macOS arm64 build
- ✅ clean-room SG3 metadata parsing and `.555` range checks
- ✅ plain RGB555, Omega sprite, and Type-30 isometric image decoding
- ✅ optional Type-30 Omega overlay over the decoded footprint
- ✅ synthetic tests containing no proprietary game bytes
- ✅ direct SG3/.555 → RGBA → SDL3 preview, plus optional PNG export/debugging preview
- ✅ deterministic metadata-only asset catalog and CLI filters
- ✅ SDL3 visual asset browser with lazy thumbnails and bounded texture cache
- ✅ observed internal version-214 Type-256 alpha addressing and full-image decoding in the local GOG set
- ⚠️ external and other unobserved alpha profiles remain unverified; no pixel-exact game comparison
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
./build/openemperor-assets --data /path/to/your/game-data --verify-alpha-decodes
./build/openemperor --data /path/to/your/game-data --browse-assets
./build/openemperor --data /path/to/your/game-data --browse-assets --ignore-alpha
```

`--data` checks that the directory exists and prints its absolute path. The application's `--sg3` and `--image` options must be supplied together and cannot be combined with `--preview`. The direct preview resolves the `.555` source, checks each **effective** range against the actual file size, reads only those ranges, and renders without writing an intermediary. For internal version-214 Type-256 records with nonzero alpha and stored `alpha_offset == data_offset + 2 * data_length`, the effective alpha starts at `data_offset + data_length`. This equality is a conservative observed profile marker; the original meaning of `alpha_offset` remains unknown. Other alpha-bearing profiles fail explicitly in normal loading. `openemperor-inspect` prints generic file metadata for other files and structured SG3 JSON with raw and effective alpha fields. Its `--image <index>` export can write headerless RGBA8 or PNG when requested. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

The local ignored GOG set contains 9,059 alpha-bearing records, all internal version-214 Type-256 Sprites. The normal loader decoded all 9,059 completely with the observed contiguous range; 52 representative RGBA results exactly matched explicit `CONTIGUOUS` diagnostics. Several images were also inspected in the normal SDL browser, including a 236×263 image previously affected by SPEC artifacts and a record whose stored alpha range exceeds EOF. This is **not** a pixel-exact comparison with the original game. No original external alpha-bearing image was available, so no external `-1` rule is used. See [the research log](docs/reverse/research-log.md).

For diagnosis only, `openemperor-assets --data <directory> --audit-alpha` prints aggregate counts by source, type, and archive plus common offset deltas. Add `--json` for per-record JSON Lines diagnostics. `openemperor --sg3 <file> --image <index> --alpha-addressing spec|contiguous|legacy` previews one image with an explicit addressing hypothesis; `--ignore-alpha` previews color only. The inspector accepts the same diagnostic options when explicitly exporting one image. Keep any such exports outside the repository and do not distribute them.

`openemperor-assets` recursively scans `.sg3` files (case-insensitive extension) without decoding images. It reports archive errors and separate color, raw-alpha, and effective-alpha range statuses, and supports combined `--kind`, `--type`, `--archive`, `--group`, `--with-alpha`/`--without-alpha`, `--min-width`, and `--min-height` filters. `--json` writes metadata to stdout using relative archive paths as stable IDs; it contains no pixels or absolute local asset paths. The existing `alpha_offset` field is the stored value; `alpha_bounds` now means the **effective** production range and is also exposed as `effective_alpha_bounds`. New fields `alpha_offset_raw`, `effective_alpha_offset`, `alpha_addressing_policy`, `alpha_profile_supported`, and `raw_alpha_bounds` make the distinction explicit. Catalog creation does not attempt decodes. `decoder_supported` indicates layout support independently of file bounds and actual decode results. The separate `--verify-alpha-decodes` command decodes supported alpha records one at a time without saving images.

The browser initially shows records with supported color metadata and valid color ranges, including supported alpha profiles whose effective alpha range is valid. Unverified profiles remain available through `4` (all candidates), where decode failures become placeholders. Keys `1`, `2`, and `3` select Plain, Sprite, and Type-30 images; `0` shows all kinds. Arrow keys or WASD move the selection, Page Up/Down move a page, Enter opens detail, and Escape returns to the grid or quits. Detail mode reports the selected policy and effective offset. Thumbnails exist only in memory, use nearest-neighbor scaling, and are cached up to 64 textures or 64 MiB (with a 16 MiB per-image limit). `--ignore-alpha` is a separate diagnostic color-only browser mode.

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.
