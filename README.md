# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Current status

This repository contains an SDL3 application shell, an SG3 metadata inspector, and a one-image RGBA/PNG exporter. The app opens a window showing “OpenEmperor”, handles Escape/window-close, and accepts a game-data directory without reading its contents. With `--sg3 <file.sg3> --image <index>`, it loads one documented uncompressed regular image directly from the user's SG3/.555 files and renders it in an SDL3 texture, entirely in memory. `--preview <exported.png>` remains available for debugging. The inspector reports SG3 metadata and associated `.555` bounds and can optionally export an image. There is no game logic, map loading, or bulk asset import yet.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

- ✅ native C++20/SDL3-App
- ✅ Apple-Silicon-Build vorgesehen
- ✅ saubere Clean-Room-Regeln
- ✅ SG3-Metadatenparser
- ✅ .555-Range-Validierung
- ✅ RGB555 → RGBA
- ✅ PNG-Export
- ✅ SDL3-Preview
- ✅ synthetische Parser-Tests ohne proprietäre Daten
- ✅ direkte SG3/.555-Vorschau im SDL3-Fenster
- ❌ komprimierte Sprites
- ❌ isometrische Sprites
- ❌ Alpha-Masks
- ❌ Asset-Katalog
- ❌ Maps
- ❌ Simulation

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
./build/openemperor-inspect /path/to/file.sg3 --image 1 --rgba .local/decoded/image-1.rgba
./build/openemperor-inspect /path/to/file.sg3 --image 1 --png .local/decoded/image-1.png
./build/openemperor --preview .local/decoded/image-1.png
./build/openemperor --sg3 /path/to/your/game-data/example.sg3 --image 1
```

`--data` checks that the directory exists and prints its absolute path. The application's `--sg3` and `--image` options must be supplied together and cannot be combined with `--preview`. The direct preview resolves the documented internal or external `.555` source, checks the selected byte range against the actual file size, reads only that payload, and renders the decoded image without writing an intermediary. It rejects compressed, isometric, alpha-mask, and undocumented image layouts explicitly. `openemperor-inspect` prints generic file metadata and the first 32 bytes in hex for other files. For a `.sg3` input, it prints structured JSON with the header, index, groups, image records, associated `.555` files, raw unknown fields, and bounds results. Its `--image <index>` export can still write headerless RGBA8 with `--rgba <output>` or an RGBA PNG with `--png <output>`. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.
