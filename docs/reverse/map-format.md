# Read-only Emperor map profile

This note separates public reference observations, checks against the user's local GOG files, and conventions chosen for the OpenEmperor debug display. No original data or code from the reference reader is included here. The independently written implementation is in `src/maps/`.

## Reference and provenance

The public `bvschaik/citybuilding-mappers` sources were inspected at revision [`bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6`](https://github.com/bvschaik/citybuilding-mappers/tree/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6): [`zlibfile.cpp`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/zlibfile.cpp), [`zlibfile.h`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/zlibfile.h), [`emperorfile.cpp`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/emperorfile.cpp), [`emperorfile.h`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/emperorfile.h), [`gamefile.cpp`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/gamefile.cpp), and [`grid.h`](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/grid.h). The reference carries GPL notices. Its behavior is evidence, not an implementation template; no Qt dependency or source translation was used. Decompression uses the normal zlib library, with stream checks guided by the [zlib manual](https://www.zlib.net/manual.html).

## Outer container: reference observations and our checks

| Field or range | Offset domain | Observed/reference form | Reader behavior |
| --- | --- | --- | --- |
| Outer marker | physical file 0 | little-endian `0xFEDCBAAA` | required |
| Block header | physical offset within one part | 12 bytes: unknown `uint32_le`, compressed byte count `uint32_le`, decompressed byte count `uint32_le` | retains unknown word; validates both lengths and part boundary |
| Block payload | physical offset after header | zlib stream | requires `Z_STREAM_END`, exact declared input consumption, and exact declared output count |
| Multipart footer | last 68 physical bytes when marker is present | `0xAAABCDEF`, `0x3C`, then 11 little-endian table words; other bytes are not interpreted | accepts increasing in-range part starts, records unused/non-boundary table words, rejects malformed boundaries |

The first part begins at physical byte 4 after the outer marker. Block `logical_offset` counts decompressed bytes **within its part**, starting at zero. `offset_within_part` counts physical bytes from that part's physical start. Map offsets below are logical offsets in a selected decompressed part; they must never be applied directly to the compressed file. Multipart table words are not all necessarily valid part starts in the local files, and their complete semantics remain unknown. Our reader does not search for alternative zlib starts or repair invalid blocks.

The limits are implementation safety budgets, not claimed format maxima: 64 MiB physical file, 12 parts, 4,096 blocks per part, 65,536 compressed bytes and 32,768 decompressed bytes per block, 64 MiB decompressed per part, and 128 MiB across parts. All additions and ranges are checked before allocation or access. The reader reports part, block, and physical offset on block failures.

## Supported decompressed map profile

The reference map reader checks an eight-byte prefix `05 00 FE CA 00 00 02 00`, reads a little-endian map-size word at logical offset 84, and seeks the following layers using fixed offsets. The local GOG standalone maps match this prefix and the layer bounds. We support this one **storage-grid profile** only. A part with a different signature, implausible size, or missing layer bytes is not reinterpreted as this profile.

| Read value | Logical offset in selected part | Type/count | Ordering and evidence |
| --- | ---: | --- | --- |
| Map signature | 0 | 8 raw bytes | reference header check; confirmed locally |
| Declared map size | 84 | one `uint32_le` | reference field; local values 84, 112, 140, 170, 226 |
| Terrain raw grid | 261,455 | 51,984 `uint32_le` = 207,936 bytes | reference 228×228 grid; `gamefile.cpp`/`grid.h` index rows by `y*228+x`; local complete range and spatial patterns |
| Objects raw grid | 469,391 | 51,984 `uint32_le` = 207,936 bytes | same stored row order; local complete range and spatial patterns |

The terrain grid ends exactly where the object grid begins; the object grid ends at logical offset 677,327. Other bytes and layers are not interpreted. The `uint32_t` words are exposed unchanged, including unknown bits. A terrain word is **not** assumed to be an SG3 image index; an object word is **not** assumed to be a building ID. The map-size word is kept separately from the 228×228 stored dimensions. We do not know the active-cell mask, playable extent, orientation in the original game, or logical/isometric coordinate transform. `active=unknown` applies to every displayed storage cell. A `0x13 00` prefix is identified as a possible savegame part from the reference and rejected for this reader.

## Local read-only validation

The ignored user-supplied GOG tree under `.local/gog-extracted/app/` contained 167 standalone `Cities/*.map` candidates, 31 `Campaigns/*.pak` candidates, and no files with the scanned `.sav`, `.sve`, or `.gam` extensions. This is an inventory of that local installation, not a general claim about every distribution. A deterministic `openemperor-map-inspect --data ... --list --json` pass read all 167 standalone map parts completely through the current parser. All 167 had the outer marker, 58 blocks, the supported map signature, in-bounds terrain and object ranges, and the declared size distribution above (1, 16, 67, 72, 11 respectively). Their compressed file sizes ranged from 79,450 to 192,423 bytes. The same scan recognized 45 map-profile candidate parts in the campaign files, but did **not** fully read every campaign part. One campaign `.pak` was rejected because a declared compressed payload crossed its part boundary; this is reported per file, not silently repaired. Campaign part index is not assumed to have a universal meaning.

Three standalone maps were individually fully read by the inspector: `Cities/Xia.map` (size 84; 23 distinct terrain words, 2 object words), `Cities/Banpo.map` (size 112; 25/2), and `Cities/Chengdu.map` (size 170; 24/3). `Campaigns/1 Xia Dynasty - Tutorials.pak` part 2 was also fully read (size 112; 25/2). These counts are distribution checks, not terrain labels. The SDL view displayed the actual `Xia.map` terrain and object storage grids. Its terrain image showed a bounded, nonuniform central pattern; the object layer had sparser structure. This checks that real values reach the view and that layer switching works, but a plausible pattern alone does not prove orientation or playable bounds. No original game/editor view or reference mapper output was available for pixel-by-pixel comparison. A UI automation click produced `(0,0)` input rather than the requested window coordinate, so visible real-map cell selection could not be verified by that route; synthetic SDL tests cover selection and coordinate conversion.

## Debug-display convention and open questions

`raw_value_color` hashes the entire raw 32-bit value to a deterministic RGBA color; identical words have identical colors. Colors are deliberately unnamed and have no inferred terrain meaning. The view uses one nearest-neighbor 228×228 texture, updates its pixels only when switching between `terrain_raw` and `objects_raw`, and reports selected raw decimal/hex words and logical byte offsets. This is a rectangular **storage-grid** view, not a claim about the game's isometric world.

Still unknown: the meaning of the block's first word and unused multipart table words; the meaning of all other map fields/layers; whether map-size encodes a playable side, border, or another quantity; active-cell mask and map-coordinate mapping; terrain bit meanings, object identities, and map-to-SG3 graphics. Savegames and campaign logic remain unsupported. No original files were changed, and no source bytes, screenshots, or decoded assets were committed.
