# Graphics-ID investigation (2026-09-17)

This is a narrow, read-only static study of the locally supplied `Emperor.exe` (SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`, PE32 Intel 80386). The starting repository commit was `10906083e1697b12782d2dc302c040944513e4d8`. All addresses below are **RVAs** unless marked VA or file offset. The PE image base is `0x400000`; static VAs are base plus RVA. The EXE was never run, patched, or committed. Apple LLVM `objdump` 21.0.0 and the independent read-only `tools/re/pe_string_refs.py` were used. Ghidra/analyzeHeadless was not installed; no decompiler output or Ghidra project was produced.

The runtime-record follow-up started at commit `1ed282de020b2387f73d523c853f622d735af06a`; the EXE hash was rechecked unchanged. [PE file offsets, RVAs and static VAs](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) below are distinct coordinate systems. The logical decompressed-map offset and the SG3 file offset are distinct again.

## Registration evidence

| String | PE file offset | RVA | VA | Direct absolute pointer occurrence |
| --- | ---: | ---: | ---: | ---: |
| `China_Terrain` | `0x42977c` | `0x42b17c` | `0x82b17c` | file `0x75057`, RVA `0x75c57` |
| `China_Elevation` | `0x429684` | `0x42b084` | `0x82b084` | file `0x750b7`, RVA `0x75cb7`; one other occurrence |
| `%sChinaMapProj.sg3` | `0x468d0c` | `0x46a70c` | `0x86a70c` | file `0x1cc710`, RVA `0x1cd310` |

The locations above can be independently reproduced with `python3 tools/re/pe_string_refs.py <your-Emperor.exe> --needle China_Terrain --needle China_Elevation --needle '%sChinaMapProj.sg3'`. A raw pointer occurrence is not automatically a code reference; the bounded disassembly was checked around the listed `.text` locations. The `ChinaMapProj` format string has a different reference path and is **not** evidence of a graphics-ID slot.

At RVA `0x75c2c` a routine constructs a 41-entry stack array of name pointers, then passes it to RVA `0x1cbdf0`. Array entry 3 is `China_Terrain` (`0x75c53`); entry 16 is `China_Elevation` (`0x75cb3`). RVA `0x1cbdf0` reads the manager's current entry number, selects that array position, and calls RVA `0x1cbe70` for a nonempty name. That reaches the registration/open routine at RVA `0x1cbf70`; on the observed success path it advances the current entry number at RVA `0x1cc0b3`. A separate call at RVA `0x1cbe80` explicitly requests slot 16 for the elevation name. This establishes positions in a registration sequence, **not** a guarantee that every installation always successfully loads both archives or that the sequence is independent of context. The value `entry * 512` passed at RVA `0x1cc053` is part of that routine's arguments; its role is not yet established and it is not used as the graphic-ID split.

## Generic lookup evidence and limit

At RVA `0x70b60` (wrapped by RVA `0x70b40`), a graphic-resource manager divides its signed input by `0x4000`. For nonnegative values, the quotient selects a per-slot object and the remainder selects a local record. The relevant split occurs at RVAs `0x70b68`–`0x70b84`; RVA `0x70ba9` obtains the selected table's count, RVA `0x70bb5` indexes its record table, and the result is used by a subsequent drawing call. A second bounded routine at RVA `0x8100` uses the same 14-bit split. For the positive sample values only, `0xc027` maps to slot 3 / local 39, `0xc0c9` to slot 3 / local 201, and `0x40014` to slot 16 / local 20. The 12-bit alternative would assign `0xc027` to slot 12 rather than slot 3; the observed shift by 14 resolves this ambiguity for the generic lookup. This is stronger than a coincidental `0x3fff` constant because both split outputs feed a resource-object and record selection. It does **not** prove that `candidate_word_layer` is the input to either routine. The examined direct callers of RVA `0x70b40` include RVA `0x6db86`; its argument is synthesized by another graphic-resource call, not traced back to the map candidate range. The static map-load/store-to-lookup dataflow remains open. The role of `candidate_byte_layer`, possible context selection, high-bit/negative IDs, and whether all entries use the same lookup path remain unknown.

`src/maps/GraphicsIdHypothesis.*` therefore implements **only an opt-in diagnostic** for this EXE hash: reject the high bit, divide a positive raw value by 16384, and consult an explicit registration snapshot for entries 3 and 16. Unknown entries have no fallback. This is an independently written testable model of the observed generic operation and array positions, not an assertion about the map field or an engine renderer rule. The prior direct-index H1 diagnostic remains unchanged.

The preceding paragraph states the **initial study's** dataflow limit. The follow-up below found the map-load edge and the related RVA `0x8100` draw path; the earlier negative statement is historical.

## Runtime local index to physical SG3 record (follow-up)

At RVA `0x8150` (VA `0x408150`), the selected resource object reads its count at object offset `0xcc34` and addresses a 36-byte runtime record as `table_pointer + 36 * local_index`. The table pointer is the first member of that object. RVA `0x1cc1f0` (VA `0x5cd1f0`) supplies it on the observed registration path. Its version-213 branch at RVA `0x1cc3c5` converts each 64-byte physical SG3 image record to a 72-byte intermediate record **in physical order**; no reordering or index table was observed in that loop. For ordinary Terrain/Elevation names, the special `Zeus_system.bmp` branch at RVA `0x1cc34f` is not taken, leaving its skip variable at zero. The routine reads the SG3 header's `reported_images_in_use` from VA `0x1b501d8` at RVA `0x1cc401`, stores that count at object offset `0xcc34`, and copies from intermediate base VA `0x1b5a0f8` at RVA `0x1cc43b`. This source is 72 bytes after the intermediate physical-record-zero base `0x1b5a0b0`: **the dummy record is skipped**. The copied runtime records are then processed in place; the examined loop does not reorder them. The allocation/copy uses the in-use count, not the 10,000-record SG3 capacity. Subsequent runtime count adjustment and mutations have not been proven irrelevant for every context; the resolver therefore applies this rule only to the explicit v213 Terrain/Elevation registration snapshot and bounds `local_index < reported_images_in_use`.

The observed translation is `runtime local i → physical SG3 record i+1`. Our `AssetCatalog.records` and `AssetId.image_index` already use physical positions and remain unchanged. The first local index 0 addresses physical record 1; the last local index of `China_Terrain.sg3` is 1442 and addresses physical 1443; physical 1444 and later are reserved for this runtime table. The local archive has version 213, stride 64, capacity 10,000 and `reported_images_in_use=1443`. `China_Elevation.sg3` has the same version/stride/capacity and `reported_images_in_use=372`. The physical image table starts at SG3 file offset 40,680, so the record offset is `40680 + 64 * physical_index` after range validation. This is **not** an offset into `.555` and not a PE address.

For Xia storage cell `(110,75)`, the unchanged candidate word is decimal 49,191 (`0xC027`) at **decompressed map logical offset** 70,375 (`1535 + 4*(75*228+110)`). *If* this word reaches the generic lookup, it selects observed slot 3 / runtime local 39 / physical `DATA/China_Terrain.sg3` record 40. Its **SG3 file offset** is 43,240 (`0xA8E8`); parser metadata is 4×9, type 276, `.555` color offset 24,318 and length 49. The selected payload is in bounds and the normal decoder succeeds. The original visual match is unverified. Xia's water-classified storage cell `(134,93)` has candidate `0xC0C9`, logical offset 86,887, slot 3/local 201/physical record 202, SG3 offset 53,608, type 30, 78×41, color offset 42,418 and length 3,320. It too decodes; its sand-colored diamond appearance is a reason to avoid claiming a proven water-tile mapping.

## Runtime cell value versus stored candidate word

The map-load edge is now observed in the matching read branch of the bounded map serializer at RVA `0x12d7c0`. The stream-reading routine at RVA `0x37f533` copies the requested byte count into the passed destination (its buffer-to-destination copy is visible at RVA `0x37f55e`); the companion at RVA `0x37f642` writes in the opposite direction. After the map header at RVA `0x12d9e5`, the read branch passes **207,936 bytes** into static VA `0xfe9880` at RVA `0x12d9ea`, then **51,984 bytes** into VA `0xfdcd70` at RVA `0x12d9fb`, then **207,936 bytes** into VA `0xf6a9e0` at RVA `0x12da0c`, then the next 207,936 bytes into VA `0xf37da0` at RVA `0x12da1d`. These are sequential calls on the same stream with no intervening read between the four arrays. The independently parsed map profile places exactly those four ranges at logical offsets 1,535, 209,471, 261,455, and 469,391. The third array is also tested for the terrain off-map bit `0x80000` at RVA `0x6ea87`, corroborating its identity as the independently established terrain layer. Together, the ordering, widths, matching offsets, and separate terrain-bit use connect the **stored candidate word** at `1535 + 4*cell_index` to the initial 32-bit runtime array value at `0xfe9880 + 4*cell_index`. The candidate byte enters the adjacent 51,984-byte array, but its meaning is still unknown.

The renderer-side chain is separately observed: at RVA `0x6f0e0`, a map-cell index loaded from a view grid selects a 32-bit value from VA `0xfe9880 + 4*cell_index`. That value is placed in VA `0x10c7388`; at RVA `0x70167` it is pushed to the related graphic lookup at RVA `0x8100`. A second path at RVA `0x709c4` loads the same array and at RVA `0x70a29` passes it to that lookup. Thus there is a static **map-load → per-cell array → graphic lookup** chain for this profile. The exact value at a particular later draw is **not verified**: the array is cleared at RVA `0xb08a8` and other routines, including RVA `0xb29b3`–`0xb29c9`, can replace cells with computed graphic IDs. The sampled file word's identity is preserved in the diagnostic; it must not be presented as an observed runtime draw or original-game visual match. The 4×9 sprite lacks proven map placement; the 78×41 image is not the currently supported flat 78×40 preview layout. No graphics-candidate map texture preview was activated.

The prior profile output below directly indexed the catalog with the runtime local index. It is retained as history but its record identities and counts are **superseded** by the physical-record translation above.

## Corpus check (independent of binary dataflow)

The corrected physical-record translation was checked first on Xia, then Banpo, Chengdu and Anyi. All counts below cover candidate-mask cells; `decode_candidate` is a metadata/source/layout gate, **not** a successful decode count. The selected two coordinates in each map decoded successfully, but are not identical terrain classes across maps.

| Map | Candidate cells | Decode candidates after physical translation | Empty physical records | Previously reported empty records |
| --- | ---: | ---: | ---: | ---: |
| Xia | 3,612 | 3,578 | 34 | 37 |
| Banpo | 6,384 | 6,347 | 37 | 40 |
| Chengdu | 14,620 | 14,436 | 184 | 238 |
| Anyi | 14,620 | 14,457 | 163 | 211 |

The changed empty counts follow the observed record translation, not a cosmetic adjustment. The remaining empty records remain explicit. A valid payload or selected decode does not validate the map-to-lookup edge. Physical record 40 appears as a tiny dark/transparent sprite; physical 202 appears as a sand-colored diamond. Both were exported only under ignored `.local/re/emperor/` for offscreen examination. Neither is an original-game screenshot or visual-match verification. The external reader's `imageId=i+1` convention is only a numbering comparison and was not used as engine evidence.

`Anyi.map` was chosen as the fourth map **before** the profile was run; Xia and Banpo were inspected first, then Chengdu and Anyi. Counts below include only the existing candidate mask and are metadata gates, not all successful decodes:

| Map | Candidate cells | Nonempty, in-bounds, supported records | Empty records | Successful selected-sample decodes |
| --- | ---: | ---: | ---: | ---: |
| Xia | 3,612 | 3,575 | 37 | 11 |
| Banpo | 6,384 | 6,344 | 40 | 7 |
| Chengdu | 14,620 | 14,382 | 238 | 9 |
| Anyi | 14,620 | 14,409 | 211 | 9 |

The table covers actual local `DATA/China_Terrain.sg3` and `DATA/China_Elevation.sg3` registration candidates. Every selected sample that reached the normal decoder succeeded; other candidate cells were **not** all decoded. The CLI emits raw storage coordinate, terrain/object values, both candidate fields, candidate logical offsets, slot/local index, archive or failure, metadata, color/alpha bounds, and selected-sample decode status. Local JSON and PNG diagnostics were kept only in ignored `.local/re/emperor/`.

Four selected images were exported locally for visual inspection: terrain 39 was a small dark/transparent sprite, terrain 201 a sparse blue footprint-like image from a water-classified cell, terrain 698 a textured diamond from a vegetation-classified cell, and elevation 28 a dark/transparent sprite. This confirms the actual decoder output and also shows that many candidate values select nonflat overlays. Appearance is neither a pixel-exact comparison to the original game nor proof of the map-field mapping. The texture preview was **not** activated because the map-to-lookup dataflow is unproven and many supported records are not flat ground tiles.

To reproduce the bounded diagnostic with your own legally obtained data:

```sh
python3 tools/re/pe_string_refs.py .local/gog-extracted/app/Emperor.exe --needle China_Terrain --needle China_Elevation --needle '%sChinaMapProj.sg3'
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --profile exe-6373328b-14bit-hypothesis --cell 134 93
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --profile exe-6373328b-14bit-hypothesis --cell 110 75 --cell 134 93
./build/openemperor --sg3 .local/gog-extracted/app/DATA/China_Terrain.sg3 --image 40
```

There is no graphics-candidate texture-preview command yet. The existing hand-curated `--view textured --terrain-bindings ...` preview is independent of this hypothesis.
