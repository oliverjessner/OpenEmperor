# OpenEmperor clean-room rules

- This is an independently written open-source reimplementation. Never commit or distribute original Emperor game assets, the GOG installer, extracted game files, or proprietary Sierra/Impressions source code.
- Users must supply their own legally obtained Emperor game files. The first planned distribution to support is the GOG offline installer; this repository does not ship or decode it yet.
- Keep local game files under `.local/` when practical. That directory is ignored by Git. Do not add test fixtures copied from proprietary files.
- Record reverse-engineering observations, provenance, evidence, and unresolved questions in `docs/reverse/`. Keep those notes separate from implementation code.
- Do not guess undocumented file-format semantics. Mark unknown fields explicitly unknown until evidence verifies them. Avoid treating names or interpretations from decompiled code as implementation specifications.
- Write readable, original C++20 code from documented behavior. Do not transliterate or paste decompiled/proprietary code.
- Preserve portability beyond the initial macOS arm64 target: use CMake, SDL3, and standard C++ where possible; isolate platform-specific code in `src/platform/`.
- The reusable `src/assets/` loader may read one documented uncompressed SG3 image and its selected `.555` payload into memory, without SDL or an inspector-tool dependency. The app may render that image directly in SDL3; PNG debugging preview and inspector export remain separate options. Reject unsafe external paths, out-of-bounds ranges, compression, isometric images, alpha masks, and unsupported layouts. Do not import original or decoded assets into the repository or add game logic without a later scoped request.
