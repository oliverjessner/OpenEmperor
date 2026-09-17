# Research log

## Initial shell

- Scope: application lifecycle, SDL3 window, user-supplied data-directory validation, generic file inspection.
- Format observations: none.
- Unresolved: installer layout, game-data inventory, and all game-file semantics.

## Read-only SG3 metadata stage

- Public references: [SG file-format wiki](https://github.com/bvschaik/citybuilding-tools/wiki/SG-file-format) and [SG layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md). The sources disagree about image bytes 16–19 and byte 51; the later metadata audit below records the interpretation now used by the parser.
- Local evidence was read from ignored, user-supplied GOG files in `.local/`; no file bytes or fixtures were copied into the repository.
- Several local SG3 files of both versions have a 14,400-byte suffix after the declared image table. Sampled version-213 suffixes contain nonzero bytes; sampled version-214 suffixes are zero-filled. Its purpose is unknown and the parser leaves it unread.
- At least one local SG3 header's reported file size differs from its actual size. The parser reports both without inferring which value governs any other field.
- Some headers' reported internal `.555` size differs from the associated file's actual size. The inspector reports the comparison without adjusting offsets or lengths.
- Several documented external image offset/length pairs extend one byte beyond the corresponding observed `.555` file size. Some version-214 alpha offset/length pairs also extend beyond the observed internal `.555` size. The parser flags these ranges. Whether any of these fields use another convention is unknown.
- Pixel encoding, mirrored-image data, and all undocumented fields were outside this metadata-only stage.

## One-image RGBA export stage

- Public references: the [SG file-format notes](https://github.com/bvschaik/citybuilding-tools/wiki/SG-file-format) for uncompressed pixel order and transparent color, and the [layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md) for RGB555 bit positions.
- A single documented uncompressed regular image was selected from ignored, user-supplied GOG data for a local smoke test. The 32 × 32 result is 4,096 bytes of row-major RGBA8 and remains under ignored `.local/decoded/`.
- The exporter checks the actual `.555` range before reading the selected payload. It refuses external ranges outside the observed file size, alpha-mask metadata, and image types without a supported layout.
- At this earlier stage, Omega and isometric decoding were not yet implemented. The previously observed one-byte external range discrepancy has not been corrected.

## Direct one-image SG3 preview stage

- Scope: reuse the already documented metadata and plain RGB555 decoding in a read-only asset loader. This stage added no SG3 field interpretations.
- Synthetic on-disk SG3/.555 pairs constructed from the public layout cover internal and external sources, missing and out-of-bounds data, unsafe names and symlink escapes, and index rejection. No original game bytes are embedded in tests.
- The application obtains only the selected `.555` payload and sends the resulting RGBA pixels directly to SDL3; inspector exports and PNG debugging preview continue as separate commands. Original and decoded local files remain outside the repository in ignored `.local/`.
- A local smoke test selected image index 1 from an ignored, user-supplied SG3/.555 pair. The app reported 32 × 32 pixels, opened its SDL window and exited normally with Escape. The ignored decoded-file inventory was unchanged before and after the app run. Inspector RGBA and PNG exports were checked separately in temporary space and matched the previous local exports byte for byte; those temporary checks were removed.
- Unknown metadata fields, the unparsed SG3 suffix, one-byte external range discrepancies, and alpha-mask behavior remain unresolved; no undocumented adjustment or fallback is made.

## Image metadata audit and Omega Sprite stage

- The [published SG layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md) identifies bytes 50–51 as a little-endian 16-bit image type, bytes 16–19 as a four-byte horizontal-mirror offset, and bytes 8–11 as uncompressed length. An earlier parser treated byte 51 as a compression flag and byte 53 as another compression indicator. The parser now reads the type as 16 bits, preserves the mirror offset without applying it, and leaves bytes 53–54 unknown. This choice differs from parts of the older [SG wiki](https://github.com/bvschaik/citybuilding-tools/wiki/SG-file-format); it is not evidence of additional flag semantics.
- Independently authored Omega color decoding supports documented Sprite image types 256, 257, and 276. Synthetic tests exercise literals, transparent skips, row transitions, and malformed/truncated commands. No alpha stream is decoded.
- A local smoke test using ignored GOG data decoded a 32×32 Type-256 Sprite and displayed it in SDL3; Escape closed the window normally. The temporary visual check was deleted. No proprietary bytes or derived image files were added to the repository.
- A local inventory of 58 ignored SG3 files reported 63,709 Sprite-kind records and 4,114 Type-30 records. These are metadata record counts, not a claim that every payload is decodable; per-image bounds and unsupported alpha still govern loading.

## Type-30 isometric stage

- The [published SG layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md) guided an independently written decoder. Type 30 uses `uncompressed_length` as the exact split between the RGB555 diamond-tile base and optional Omega color overlay. The decoder supports classic 58×30/1,800-byte and Emperor 78×40/3,200-byte tile geometries, validates the size hint against dimensions and complete base length, and keeps skipped overlay pixels already drawn in the base. Synthetic tiles and temporary synthetic SG3/.555 pairs test geometry, 2×2 assembly, vertical placement, overlay preservation, bad lengths, alpha rejection, and file-range bounds.
- One local ignored archive has 376 Type-30 records. Its 78×40 single Emperor tile (derived footprint N=1, no overlay) and 158×80 four-tile footprint (derived N=2, no overlay) visibly rendered. Each app window exited normally with Escape. A 78×41 image with nonempty overlay also decoded through the application loading path; that check did not open a display in the sandbox. Temporary visual checks were deleted.
- At this stage, alpha-mask decoding was not yet implemented; applying the parsed mirror offset, the SG3 suffix, and observed out-of-bounds source ranges remained unresolved. No offset correction, alpha interpretation, map logic, or game simulation was introduced.

## Version-214 alpha stream stage

- The [public SG layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md) documents separate alpha offset/length metadata at record bytes 64–71, one byte per literal alpha value (low five bits), and Omega skip/literal commands. An independently written decoder applies the exact documented five-to-eight-bit expansion to A only. The loader validates and reads the alpha range independently from the color range, including offset zero, then applies it after Plain, Sprite, or Type-30 color decoding. Synthetic streams and synthetic SG3/.555 pairs cover all three kinds, nonadjacent ranges, skipped existing alpha, truncation, and bounds rejection.
- Across 58 ignored local GOG SG3 archives, 9,059 records have nonzero `alpha_length`: 9,059 Sprite, zero Plain, and zero Type-30. Of these, 9,042 alpha ranges fit their actual `.555` file and 17 are out of bounds. None of the 17 is clamped, shifted, padded, or read.
- A read-only diagnostic applying the documented alpha command rules to the 9,042 in-bounds streams found 1,141 that are syntactically complete and 7,901 that fail a command or image-bound check. Syntactic completion does not establish correct visual semantics. Two real Sprite records from different archives passed the decoder, opened in SDL3, and exited normally with Escape, but both showed visible bands/holes. For one, a temporary build with alpha application disabled displayed a coherent color-only sprite, isolating the visible difference to the alpha path. That diagnostic change was reverted, and temporary window captures were removed. These observations do not justify an undocumented offset correction or a claim of full real-data alpha compatibility.
- No available Type-30 or Plain record has a nonempty alpha range, so real-data smoke tests for those combinations could not be run. Their support is synthetic-tested only. The meaning of the real-data alpha discrepancy, as well as previously observed one-byte out-of-bounds ranges, remains unresolved. Mirroring, maps, animation systems, and game logic remain outside scope.
