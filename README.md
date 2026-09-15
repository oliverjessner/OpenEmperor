# OpenEmperor

OpenEmperor is a clean-room, open-source reimplementation of _Emperor: Rise of the Middle Kingdom_ for modern systems. The initial target is native macOS on Apple Silicon (arm64), with an architecture intended to support Linux and Windows later.

## Current status

This repository contains an SDL3 application shell, an SG3 metadata inspector, and a one-image RGBA/PNG exporter. The app opens a window showing “OpenEmperor”, handles Escape/window-close, and accepts a game-data directory without reading its contents. With `--preview`, it displays one locally exported PNG in an SDL3 texture. The inspector reads SG3 headers, image groups and image metadata, then checks referenced `.555` file sizes and byte ranges. The exporter supports documented uncompressed regular images only. There is no game logic or asset import yet.

You must provide your own legally obtained original Emperor game data. The first planned source is the GOG offline installer. **No original game assets or proprietary source code are distributed here.** Keep local game files in `.local/`, which Git ignores.

✅ native C++20/SDL3-App
✅ Apple-Silicon-Build vorgesehen
✅ saubere Clean-Room-Regeln
✅ SG3-Metadatenparser
✅ .555-Range-Validierung
✅ RGB555 → RGBA
✅ PNG-Export
✅ SDL3-Preview
✅ synthetische Parser-Tests ohne proprietäre Daten
❌ Engine lädt SG3 noch nicht direkt
❌ komprimierte Sprites
❌ isometrische Sprites
❌ Alpha-Masks
❌ Asset-Katalog
❌ Maps
❌ Simulation

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
```

`--data` checks that the directory exists and prints its absolute path. `openemperor-inspect` prints generic file metadata and the first 32 bytes in hex for other files. For a `.sg3` input, it prints structured JSON with the header, index, groups, image records, associated `.555` files, raw unknown fields, and bounds results. `--image <index>` decodes exactly one supported image; choose `--rgba <output>` for headerless, row-major RGBA8 bytes or `--png <output>` for an RGBA PNG. Dimensions and byte counts are printed as JSON. The PNG encoder uses uncompressed DEFLATE blocks, so files may be larger than PNGs made by optimizing encoders. `--preview <exported.png>` displays that PNG with nearest-neighbor scaling; the current reader supports only the RGBA8, filter-0, stored-DEFLATE subset written by this project, and rejects other PNG features. Keep decoded files under ignored `.local/` and do not distribute original game assets or decoded image copies.

See [architecture](docs/architecture.md) and [reverse-engineering notes](docs/reverse/README.md) for the project boundaries.
