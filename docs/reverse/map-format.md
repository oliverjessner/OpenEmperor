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

### Skipped-region diagnostic split, not a verified layout

The pinned public [`EmperorFile::getImage` revision](https://github.com/bvschaik/citybuilding-mappers/blob/bb97d7c7edf7c6608d5e9d3d6d5b63ba3686cda6/emperorfile.cpp) skips `1447 + 5 × 51984` bytes after reading the map-size word at logical offset 84. For a bounded read-only investigation, we split that skipped region as follows. This split is a **hypothesis**; multiple byte layers or another structure are still possible.

| Neutral diagnostic name | Logical range in decompressed selected part | Tentative reading |
| --- | ---: | --- |
| `candidate_word_layer` | `[1535, 209471)` | 51,984 unchanged `uint32_le` values in storage-row order |
| `candidate_byte_layer` | `[209471, 261455)` | 51,984 unchanged bytes in storage-row order |

The separate diagnostic reader uses the existing container's bounded logical `read_range`, requires the supported map profile, and checks exact lengths. At storage `(x,y)`, index is `y*228+x`, word offset is `1535+4*index`, and byte offset is `209471+index`. It does not change the established terrain/object grids. These names do not claim image IDs, coast flags, or variants. `openemperor-map-graphics` tests one explicit **rejected** hypothesis: the unchanged word is a direct image index in the selected SG3 archive (default `DATA/China_Terrain.sg3`). It reports numeric range, nonempty metadata, source bounds, layout support, and selected decode separately; an out-of-range value is never masked or repaired. No graphics-candidate render view was added because the local direct-index evidence fails. The original-map textured view remains an independent curated preview.

## Local read-only validation

The ignored user-supplied GOG tree under `.local/gog-extracted/app/` contained 167 standalone `Cities/*.map` candidates, 31 `Campaigns/*.pak` candidates, and no files with the scanned `.sav`, `.sve`, or `.gam` extensions. This is an inventory of that local installation, not a general claim about every distribution. A deterministic `openemperor-map-inspect --data ... --list --json` pass read all 167 standalone map parts completely through the current parser. All 167 had the outer marker, 58 blocks, the supported map signature, in-bounds terrain and object ranges, and the declared size distribution above (1, 16, 67, 72, 11 respectively). Their compressed file sizes ranged from 79,450 to 192,423 bytes. The same scan recognized 45 map-profile candidate parts in the campaign files, but did **not** fully read every campaign part. One campaign `.pak` was rejected because a declared compressed payload crossed its part boundary; this is reported per file, not silently repaired. Campaign part index is not assumed to have a universal meaning.

Three standalone maps were individually fully read by the inspector: `Cities/Xia.map` (size 84; 23 distinct terrain words, 2 object words), `Cities/Banpo.map` (size 112; 25/2), and `Cities/Chengdu.map` (size 170; 24/3). `Campaigns/1 Xia Dynasty - Tutorials.pak` part 2 was also fully read (size 112; 25/2). These counts are distribution checks, not terrain labels. The SDL view displayed the actual `Xia.map` terrain and object storage grids. Its terrain image showed a bounded, nonuniform central pattern; the object layer had sparser structure. This checks that real values reach the view and that layer switching works, but a plausible pattern alone does not prove orientation or playable bounds. No original game/editor view or reference mapper output was available for pixel-by-pixel comparison. A UI automation click produced `(0,0)` input rather than the requested window coordinate, so visible real-map cell selection could not be verified by that route; synthetic SDL tests cover selection and coordinate conversion.

## Debug-display convention and open questions

`raw_value_color` hashes the entire raw 32-bit value to a deterministic RGBA color; identical words have identical colors. Colors are deliberately unnamed and have no inferred terrain meaning. The view uses one nearest-neighbor 228×228 texture, updates its pixels only when switching between `terrain_raw` and `objects_raw`, and reports selected raw decimal/hex words and logical byte offsets. This is a rectangular **storage-grid** view, not a claim about the game's isometric world.

Still unknown: the meaning of the block's first word and unused multipart table words; the meaning of all other map fields/layers; whether map-size encodes a playable side, border, or another quantity; original-game active-cell/world-coordinate semantics, object identities, and map-to-SG3 graphics. The following section records a **reference-derived diagnostic interpretation** of some terrain conditions, not verification against the original game. Savegames and campaign logic remain unsupported. No original files were changed, and no source bytes, screenshots, or decoded assets were committed.

## Reference-derived terrain rules

The pinned reference's `EmperorFile::getTerrainColours` is an ordered minimap-color decision tree. Our independently written `TerrainInterpretation` retains both raw `uint32_t` words, recognized flag bits, remaining unknown bits, a coarse category, the rule that won, and a `partial` flag. All rule evidence is **reference-derived; original-game behavior is unverified**. We do not copy the reference palette or infer SG3 image IDs. In precedence order:

| Reference condition, tested in this order | Reference reading | Our coarse category / limit |
| --- | --- | --- |
| `terrain & 0x80000` | off map | `off_map`, independent of the diamond candidate mask |
| `(terrain & 0x104) == 0x100` | flood before water/trees | `flood`; presence of `0x100` alone is not sufficient if water bit `0x4` is also present |
| `terrain & 0x1`, with `objects & 0x2` inside this branch | tree versus bamboo | `vegetation` or `bamboo`; object bit is interpreted **only** here |
| `terrain & 0x2`; reference checks `terrain & 0x300002` for copper/iron | rock or ore | `rock_or_ore`; ore identity deliberately omitted |
| `terrain & 0x101000` | ruins, before buildings | coarse `rock_or_ore` with rule `ruins_before_buildings`; individual ruin bits not decoded |
| `terrain & 0x10000000`, then `terrain & 0x8` | monument/canal, then building | coarse `monument` or `structure`; no building type |
| `(terrain & 0x44) == 0x4` **or** `(terrain & 0x104) == 0x104` | water without road, unless flood is present; deep-water subcase uses `0x4000100` | `water`; no deep/shallow distinction |
| `terrain & 0x20000`, then `terrain & 0x200` | quarry before elevation | coarse `rock_or_ore` or `elevation_hint`; quarry depth requires the unread stone layer; height is not reconstructed |
| `0x20`, `0x40`, `0x800`, `0x4000`, `0x10000`, `0x40000`, `0x2000000`, `0x80000000` in order | garden, road, irrigation, wall, beach, marsh, pinnacle, sand | `road` or `other_marked`; details and variation omitted |
| `terrain & 0x80`, otherwise empty | fertile land or default ground | `fertile_hint` or `empty_by_reference`; a fertility value requires an unread byte layer, and unknown-only combinations become `unknown` instead of empty |

The reference also reads `random`, `fertile`, and sometimes `stone` byte layers to vary colors or choose specific fertility/quarry shades. We have not added those layers. A recognized bit can remain visible even when an earlier rule determines the display color; the chosen category never replaces the raw word. Unknown terrain bits and uninterpreted object bits remain explicit. A partial result can mean omitted detail, an unknown bit, or object data outside the one supported bamboo test.

## Diamond candidate, off-map bit, and bitmap coordinates

The pinned `EmperorFile::getImage` uses `half=114`, `border=(228-m)/2`, and `m=declared_map_size`. For each storage row `y` in `[border,border+m)`, it includes storage columns in `[start,end)`, with:

```text
if y < 114: start = border + 114 - y - 1; end = 114 + y + 1 - border
otherwise: start = border + y - 114; end = 342 - y - border
```

This is our **candidate mask**, not a proven active/playable-cell mask. We support only the five declared sizes observed in the local corpus: 84, 112, 140, 170, and 226. Their candidate counts are respectively 3,612; 6,384; 9,940; 14,620; and 25,764 storage cells. Other raw-parser-accepted sizes retain the full storage view but do not acquire this geometry by extrapolation.

The independent second signal is the reference's `terrain & 0x80000` off-map check. Our inspector counts all four combinations of candidate membership and this bit and lists up to 16 mismatch storage coordinates. It never shifts or trims a boundary to force agreement.

`GameFile::getBitmapCoordinates` maps local `(x-border,y-border)` to a **minimap pixel**, not an original-game world coordinate:

```text
pixel_x = m/2 + local_x - local_y - 1
pixel_y = 1 + local_x + local_y - m/2
```

The reference paints two horizontal pixels per selected storage cell. The first pixel always has even `pixel_x + pixel_y` parity (`2*local_x`); its right-hand companion has odd parity. Some border-cell writes are outside the `m×m` output and are clipped in our diagnostic; every in-bounds output pixel maps back to exactly one storage cell. For `m=84`, storage `(113,72)` maps to pixel `(82,0)`, `(72,113)` to `(0,0)`, and `(114,114)` to `(41,43)`. The projected view retains inverse pixel-to-storage lookup and selection, but calls itself `projected_reference` rather than a world or isometric coordinate system. Orientation relative to the original game is unverified.

## Local corpus and visible checks for this layer

A read-only semantic scan of all 167 previously supported standalone maps produced **2,107,780 candidate** and **6,573,548 outside** storage-cell observations. Of these, 2,107,780 are candidate+off-map-bit-clear; 6,573,492 are outside+off-map-bit-set; **56 are outside+bit-clear**; and zero are candidate+bit-set. The 56 differences occur in 17 files; 150 files match exactly. `Xia.map`, `Banpo.map`, and `Chengdu.map` each match exactly. The deviations remain reported (for example, `MP48.map` has 30 outside+clear cells) and are not repaired. These correlations support the diamond as a useful diagnostic candidate but do not prove gameplay boundaries.

Within the candidate cells, the coarse categories include 1,137,615 fertility hints, 239,907 water, 201,555 vegetation, 82,205 rock/ore, 45,240 bamboo, and 23,301 road cells. There are **zero unclassified categories** in this corpus, but that does **not** imply complete semantics: 1,677,964 candidate cells are marked partial, including 77,341 with unknown terrain bits and 323,142 with uninterpreted object bits. Common still-unknown terrain-bit patterns include `0x20400000` and `0x00400000`; they remain raw unknown bits rather than being assigned terrain names. Full-storage and candidate-only counts are reported separately because off-map storage dominates otherwise.

In visible SDL runs of the actual `Cities/Xia.map`, `Cities/Banpo.map`, and `Cities/Chengdu.map`, semantic storage showed coherent, differing spatial structures. Xia had connected blue water and patches of vegetation/rock; Banpo showed a near-circular blue water band; Chengdu showed multiple branching blue water bands. These visual patterns are consistent with the reference-derived water condition but do not independently prove its meaning. On Xia, the comparison view showed the matching green candidate against blue off-map storage, and the projected diagnostic showed the referenced minimap layout. Selecting storage `(114,114)` via the center-selection key displayed raw terrain `0x00000080`, object `0x00000000`, `fertile_hint`, terrain logical offset `365879`, and object logical offset `573815`; the same storage cell remained selected after switching to the projected view. These are direct checks of our file-to-view path and internal coordinates, not a comparison to an original game/editor screenshot or independent mapper output. No screenshots or decoded pixels were committed.
