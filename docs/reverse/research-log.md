# Research log

## Initial shell

- Scope: application lifecycle, SDL3 window, user-supplied data-directory validation, generic file inspection.
- Format observations: none.
- Unresolved: installer layout, game-data inventory, and all game-file semantics.

## Read-only SG3 metadata stage

- Public references: [SG file-format wiki](https://github.com/bvschaik/citybuilding-tools/wiki/SG-file-format) and [SG layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md). Their disagreement about image bytes 16–19 remains unresolved.
- Local evidence was read from ignored, user-supplied GOG files in `.local/`; no file bytes or fixtures were copied into the repository.
- Several local SG3 files of both versions have a 14,400-byte suffix after the declared image table. Sampled version-213 suffixes contain nonzero bytes; sampled version-214 suffixes are zero-filled. Its purpose is unknown and the parser leaves it unread.
- At least one local SG3 header's reported file size differs from its actual size. The parser reports both without inferring which value governs any other field.
- Some headers' reported internal `.555` size differs from the associated file's actual size. The inspector reports the comparison without adjusting offsets or lengths.
- Several documented external image offset/length pairs extend one byte beyond the corresponding observed `.555` file size. Some version-214 alpha offset/length pairs also extend beyond the observed internal `.555` size. The parser flags these ranges. Whether any of these fields use another convention is unknown.
- Pixel encoding, mirrored-image data, and all undocumented fields remain outside this stage.

## One-image RGBA export stage

- Public references: the [SG file-format notes](https://github.com/bvschaik/citybuilding-tools/wiki/SG-file-format) for uncompressed pixel order and transparent color, and the [layout specification](https://github.com/hansonry/sgfileio/blob/master/sgSpecification.md) for RGB555 bit positions.
- A single documented uncompressed regular image was selected from ignored, user-supplied GOG data for a local smoke test. The 32 × 32 result is 4,096 bytes of row-major RGBA8 and remains under ignored `.local/decoded/`.
- The exporter checks the actual `.555` range before reading the selected payload. It refuses external ranges outside the observed file size, compressed data, alpha-mask metadata, and image types without a documented uncompressed layout.
- Compression, isometric layout, and version-214 alpha-mask decoding remain unresolved and unimplemented. The previously observed one-byte external range discrepancy is not corrected.

## Direct one-image SG3 preview stage

- Scope: reuse the already documented metadata and uncompressed regular RGB555 decoding in a read-only asset loader. This stage adds no SG3 field interpretations.
- Synthetic on-disk SG3/.555 pairs constructed from the public layout cover internal and external sources, missing and out-of-bounds data, unsafe names and symlink escapes, index rejection, and compressed/unsupported types. No original game bytes are embedded in tests.
- The application obtains only the selected `.555` payload and sends the resulting RGBA pixels directly to SDL3; inspector exports and PNG debugging preview continue as separate commands. Original and decoded local files remain outside the repository in ignored `.local/`.
- A local smoke test selected image index 1 from an ignored, user-supplied SG3/.555 pair. The app reported 32 × 32 pixels, opened its SDL window and exited normally with Escape. The ignored decoded-file inventory was unchanged before and after the app run. Inspector RGBA and PNG exports were checked separately in temporary space and matched the previous local exports byte for byte; those temporary checks were removed.
- Unknown metadata fields, the unparsed SG3 suffix, one-byte external range discrepancies, compression, isometric layout, and alpha-mask behavior remain unresolved; no undocumented adjustment or fallback is made.
