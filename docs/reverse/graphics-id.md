# Graphics-ID investigation (2026-09-17)

**Current correction:** The [runtime-table fidelity check](#runtime-table-fidelity-correction-2026-09-17) below supersedes this document's earlier `skip=0`, `local+1`, reverse-group-order, `0xc557` and selected-image identities. Earlier sections remain as an audit trail. Their old CLI profile names are intentionally rejected; use the new explicit profile in the correction section.

This is a narrow, read-only static study of the locally supplied `Emperor.exe` (SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`, PE32 Intel 80386). The starting repository commit was `10906083e1697b12782d2dc302c040944513e4d8`. All addresses below are **RVAs** unless marked VA or file offset. The PE image base is `0x400000`; static VAs are base plus RVA. The EXE was never run, patched, or committed. Apple LLVM `objdump` 21.0.0 and the independent read-only `tools/re/pe_string_refs.py` were used. Ghidra/analyzeHeadless was not installed; no decompiler output or Ghidra project was produced.

The runtime-record follow-up started at commit `1ed282de020b2387f73d523c853f622d735af06a`; the EXE hash was rechecked unchanged. [PE file offsets, RVAs and static VAs](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) below are distinct coordinate systems. The logical decompressed-map offset and the SG3 file offset are distinct again. The current study corrected an earlier address-label error: Apple `objdump` displays `.text+offset` relative to the section at VA `0x401000`. That displayed offset is **0x1000 smaller than the PE RVA** for `.text`; it must not be copied as an RVA. The prose below now uses actual RVAs checked against instruction VAs.

## Registration evidence

| String | PE file offset | RVA | VA | Direct absolute pointer occurrence |
| --- | ---: | ---: | ---: | ---: |
| `China_Terrain` | `0x42977c` | `0x42b17c` | `0x82b17c` | file `0x75057`, RVA `0x75c57` |
| `China_Elevation` | `0x429684` | `0x42b084` | `0x82b084` | file `0x750b7`, RVA `0x75cb7`; one other occurrence |
| `%sChinaMapProj.sg3` | `0x468d0c` | `0x46a70c` | `0x86a70c` | file `0x1cc710`, RVA `0x1cd310` |

The locations above can be independently reproduced with `python3 tools/re/pe_string_refs.py <your-Emperor.exe> --needle China_Terrain --needle China_Elevation --needle '%sChinaMapProj.sg3'`. A raw pointer occurrence is not automatically a code reference; the bounded disassembly was checked around the listed `.text` locations. The `ChinaMapProj` format string has a different reference path and is **not** evidence of a graphics-ID slot.

At RVA `0x75c2c` a routine constructs a 41-entry stack array of name pointers, then passes it to RVA `0x1ccdf0`. Array entry 3 is `China_Terrain` (`0x75c53`); entry 16 is `China_Elevation` (`0x75cb3`). RVA `0x1ccdf0` reads the manager's current entry number, selects that array position, and calls RVA `0x1cce70` for a nonempty name. That reaches the registration/open routine at RVA `0x1ccf70`; on the observed success path it advances the current entry number at RVA `0x1cd0b3`. A separate call at RVA `0x1cce80` explicitly requests slot 16 for the elevation name. This establishes positions in a registration sequence, **not** a guarantee that every installation always successfully loads both archives or that the sequence is independent of context. The value `entry * 512` passed at RVA `0x1cd053` is part of that routine's arguments; its role is not yet established and it is not used as the graphic-ID split.

## Generic lookup evidence and limit

At RVA `0x71b60` (wrapped by RVA `0x71b40`), a graphic-resource manager divides its signed input by `0x4000`. For nonnegative values, the quotient selects a per-slot object and the remainder selects a local record. The relevant split occurs at RVAs `0x71b68`–`0x71b84`; RVA `0x71ba9` obtains the selected table's count, RVA `0x71bb5` indexes its record table, and the result is used by a subsequent drawing call. A second bounded routine at RVA `0x8100` uses the same 14-bit split. For the positive sample values only, `0xc027` maps to slot 3 / local 39, `0xc0c9` to slot 3 / local 201, and `0x40014` to slot 16 / local 20. The 12-bit alternative would assign `0xc027` to slot 12 rather than slot 3; the observed shift by 14 resolves this ambiguity for the generic lookup. This is stronger than a coincidental `0x3fff` constant because both split outputs feed a resource-object and record selection. It does **not** prove that `candidate_word_layer` is the input to either routine. The examined direct callers of RVA `0x71b40` include RVA `0x6eb86`; its argument is synthesized by another graphic-resource call, not traced back to the map candidate range. The static map-load/store-to-lookup dataflow remains open. The role of `candidate_byte_layer`, possible context selection, high-bit/negative IDs, and whether all entries use the same lookup path remain unknown.

`src/maps/GraphicsIdHypothesis.*` therefore implements **only an opt-in diagnostic** for this EXE hash: reject the high bit, divide a positive raw value by 16384, and consult an explicit registration snapshot for entries 3 and 16. Unknown entries have no fallback. This is an independently written testable model of the observed generic operation and array positions, not an assertion about the map field or an engine renderer rule. The prior direct-index H1 diagnostic remains unchanged.

The preceding paragraph states the **initial study's** dataflow limit. The follow-up below found the map-load edge and the related RVA `0x8100` draw path; the earlier negative statement is historical.

## Runtime local index to physical SG3 record (historical, superseded)

At RVA `0x8150` (VA `0x408150`), the selected resource object reads its count at object offset `0xcc34` and addresses a 36-byte runtime record as `table_pointer + 36 * local_index`. The table pointer is the first member of that object. RVA `0x1cd1f0` (VA `0x5cd1f0`) supplies it on the observed registration path. Its version-213 branch at RVA `0x1cd3c5` converts each 64-byte physical SG3 image record to a 72-byte intermediate record **in physical order**; no reordering or index table was observed in that loop. For ordinary Terrain/Elevation names, the special `Zeus_system.bmp` branch at RVA `0x1cd34f` is not taken, leaving its skip variable at zero. The routine reads the SG3 header's `reported_images_in_use` from VA `0x1b501d8` at RVA `0x1cd401`, stores that count at object offset `0xcc34`, and copies from intermediate base VA `0x1b5a0f8` at RVA `0x1cd43b`. This source is 72 bytes after the intermediate physical-record-zero base `0x1b5a0b0`: **the dummy record is skipped**. The copied runtime records are then processed in place; the examined loop does not reorder them. The allocation/copy uses the in-use count, not the 10,000-record SG3 capacity. Subsequent runtime count adjustment and mutations have not been proven irrelevant for every context; the resolver therefore applies this rule only to the explicit v213 Terrain/Elevation registration snapshot and bounds `local_index < reported_images_in_use`.

The observed translation is `runtime local i → physical SG3 record i+1`. Our `AssetCatalog.records` and `AssetId.image_index` already use physical positions and remain unchanged. The first local index 0 addresses physical record 1; the last local index of `China_Terrain.sg3` is 1442 and addresses physical 1443; physical 1444 and later are reserved for this runtime table. The local archive has version 213, stride 64, capacity 10,000 and `reported_images_in_use=1443`. `China_Elevation.sg3` has the same version/stride/capacity and `reported_images_in_use=372`. The physical image table starts at SG3 file offset 40,680, so the record offset is `40680 + 64 * physical_index` after range validation. This is **not** an offset into `.555` and not a PE address.

For Xia storage cell `(110,75)`, the unchanged candidate word is decimal 49,191 (`0xC027`) at **decompressed map logical offset** 70,375 (`1535 + 4*(75*228+110)`). *If* this word reaches the generic lookup, it selects observed slot 3 / runtime local 39 / physical `DATA/China_Terrain.sg3` record 40. Its **SG3 file offset** is 43,240 (`0xA8E8`); parser metadata is 4×9, type 276, `.555` color offset 24,318 and length 49. The selected payload is in bounds and the normal decoder succeeds. The original visual match is unverified. Xia's water-classified storage cell `(134,93)` has candidate `0xC0C9`, logical offset 86,887, slot 3/local 201/physical record 202, SG3 offset 53,608, type 30, 78×41, color offset 42,418 and length 3,320. It too decodes; its sand-colored diamond appearance is a reason to avoid claiming a proven water-tile mapping.

## Runtime cell value versus stored candidate word

The map-load edge is now observed in the matching read branch of the bounded map serializer at RVA `0x12e7c0`. The stream-reading routine at RVA `0x380533` copies the requested byte count into the passed destination (its buffer-to-destination copy is visible at RVA `0x38055e`); the companion at RVA `0x380642` writes in the opposite direction. After the map header at RVA `0x12e9e5`, the read branch passes **207,936 bytes** into static VA `0xfe9880` at RVA `0x12e9ea`, then **51,984 bytes** into VA `0xfdcd70` at RVA `0x12e9fb`, then **207,936 bytes** into VA `0xf6a9e0` at RVA `0x12ea0c`, then the next 207,936 bytes into VA `0xf37da0` at RVA `0x12ea1d`. These are sequential calls on the same stream with no intervening read between the four arrays. The independently parsed map profile places exactly those four ranges at logical offsets 1,535, 209,471, 261,455, and 469,391. The third array is also tested for the terrain off-map bit `0x80000` at RVA `0x6ea87`, corroborating its identity as the independently established terrain layer. Together, the ordering, widths, matching offsets, and separate terrain-bit use connect the **stored candidate word** at `1535 + 4*cell_index` to the initial 32-bit runtime array value at `0xfe9880 + 4*cell_index`. The candidate byte enters the adjacent 51,984-byte array, but its meaning is still unknown.

The renderer-side chain is separately observed: at RVA `0x6f0e0`, a map-cell index loaded from a view grid selects a 32-bit value from VA `0xfe9880 + 4*cell_index`. That value is placed in VA `0x10c7388`; at RVA `0x70167` it is pushed to the related graphic lookup at RVA `0x8100`. A second path at RVA `0x709c4` loads the same array and at RVA `0x70a29` passes it to that lookup. Thus there is a static **map-load → per-cell array → graphic lookup** chain for this profile. The exact value at a particular later draw is **not verified**: the array is cleared at RVA `0xb08a8` and other routines, including RVA `0xb39b3`–`0xb39c9`, can replace cells with computed graphic IDs. The sampled file word's identity is preserved in the diagnostic; it must not be presented as an observed runtime draw or original-game visual match. The 4×9 sprite lacks proven map placement; the 78×41 image is not the currently supported flat 78×40 preview layout. No graphics-candidate map texture preview was activated.

The prior profile output below directly indexed the catalog with the runtime local index. It is retained as history but its record identities and counts are **superseded** by the physical-record translation above.

## Corpus check (historical layout, superseded)

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

Historical reproduction commands below used a superseded profile and are retained only as a record of that study; the current CLI rejects them:

```sh
python3 tools/re/pe_string_refs.py .local/gog-extracted/app/Emperor.exe --needle China_Terrain --needle China_Elevation --needle '%sChinaMapProj.sg3'
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --profile exe-6373328b-14bit-hypothesis --cell 134 93
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --profile exe-6373328b-14bit-hypothesis --cell 110 75 --cell 134 93
./build/openemperor --sg3 .local/gog-extracted/app/DATA/China_Terrain.sg3 --image 40
```

There is no graphics-candidate texture-preview command yet. The existing hand-curated `--view textured --terrain-bindings ...` preview is independent of this hypothesis.

## First native terrain-selection probe (historical layout, superseded)

The same local, unexecuted PE32 EXE was rehashed to SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. Apple LLVM `objdump` 21.0.0 was used on bounded address windows and direct call sites, following the [LLVM command reference](https://llvm.org/docs/CommandGuide/llvm-objdump.html). All addresses in this section are RVAs; static VA is RVA plus `0x400000`. No raw EXE bytes or disassembly are stored here.

The routine beginning at RVA `0xb08a0` clears the per-cell graphic array at VA `0xfe9880` with `0xcb10` dwords at RVA `0xb08a8`, along with several other arrays. Its later calls at RVAs `0xb0a58`, `0xb0a5d`, and `0xb0a62` respectively initialize a terrain-related array, fill VA `0xf1e780` one byte per storage cell through a stateful generator call, and invoke the range writer at RVA `0xb3970`. This is a **pre-read generation path**, not evidence that the writer runs before the first draw of a loaded original map. The writer iterates row spans from arrays at VAs `0x101cd18`, `0x101c988`, and `0x101c5f8` (built by RVA `0xb05f0` for the current map dimensions). At RVAs `0xb39a8`–`0xb39c7` it reads `0xf1e780[cell]`, requests key `0x603` from the graphic-resource group lookup at RVA `0x8170`, adds the byte's low three bits, and stores the **complete 32-bit graphic ID** into `0xfe9880[cell]`. If the byte's low bit is set, it also sets bit `0x20` in another per-cell byte array at VA `0xf9d620`. The input `0x603` is divided by 512 in that lookup and addresses a runtime group table; it is **not** a physical SG3 image index. The group table is populated during SG3 registration from the archive's index area (see RVA `0x1cd468` onward). The exact group-table value and generator state needed to independently reproduce this writer's output for a particular generated cell were not established here.

The call order resolves the important ambiguity for the examined map-load branch: RVA `0x3ac34` invokes the clearing/generation routine. Later, RVA `0x3ad9e` invokes the map serializer in its read direction; its calls at RVA `0x12e9ea` and following load the **stored** word and byte ranges over those generated arrays. Then RVA `0x3ae05` calls post-load setup at RVA `0x13d100`; the bounded direct calls examined include a shape/span calculation at RVA `0xb05f0`, but no call to the range writer at RVA `0xb3970`. The only observed **direct** caller of that writer is RVA `0xb0a62` within the pre-read routine. These statements establish static order on this conditional branch, not that the original EXE was executed or that indirect/later writes are absent. Other writers exist; for example, RVA `0xb5e50` conditionally places a resource-derived ID in a cell while also changing terrain flags. That is not a verified `0x80`/`0` ground-selection path. No mutation of the local-index-to-physical-record order was found in the bounded SG3 loader examination; other resource-context replacement before a particular draw remains unverified.

The decompressed-map byte range loaded into `0xf1e780` begins at logical offset `729311` on this read branch, after the independently located terrain and object dword grids and another byte grid. Its use as a low-three-bit input in the **pre-read writer** is observed; a general meaning for the stored byte has not been assigned. The neighboring candidate byte at logical offset `209471 + cell` is a different array at VA `0xfdcd70`. Neither should be called a coast or draw flag from these observations.

The opt-in `--terrain-selection-profile exe-6373328b-first-terrain-probe` compares raw original cells without implementing a native selection rule. It preserves the stored word, reports the existing resolver's **conditional stored-ID result**, and sets `computed_graphic_id=null` with `unresolved_post_read_selection`. The precise missing connection is a verified post-read call/condition showing whether an individual simple ground cell's stored ID is retained or replaced before its first lookup/draw. If it is retained, the role and placement of the selected small image relative to any underlying tile must still be established. An identity function or a `0x603` group base plus byte-low-three-bits function would falsely promote the pre-read generator to a loaded-map selection rule, so `TerrainGraphicSelection.*` was not created.

Two adjacent Xia storage cells `(114,114)` and `(115,114)` both have `terrain_raw=0x80`, `objects_raw=0`, and `candidate_byte=64`. Their stored IDs are respectively `0xc02a` and `0xc027`. The existing v213 diagnostic resolves them to Terrain local indices 42 and 39, physical SG3 records 43 and 40; both payloads decode, but are only 5×9 and 4×9 type-276 images. The table is a **stored-ID diagnostic**, not calculated native selection:

| Map and two simple cells | Stored IDs | Physical Terrain records | Decoded image metadata |
| --- | --- | --- | --- |
| Xia `(114,114)`, `(115,114)` | `0xc02a`, `0xc027` | 43, 40 | type 276, 5×9 and 4×9 |
| Banpo `(114,114)`, `(115,114)` | `0xc028`, `0xc026` | 41, 39 | type 276, 5×9 and 7×9 |
| Chengdu `(113,29)`, `(114,29)` | `0xc017`, `0xc00e` | 24, 15 | type 12, 24×24 each |
| Anyi `(113,29)`, `(114,29)` | `0xc029`, `0xc02a` | 42, 43 | type 276, 4×9 and 5×9 |

Every listed selected payload decoded using the normal loader. Records 43, 41, and 24 were exported only under ignored `.local/re/emperor/` and viewed offscreen; the visible results are tiny dark sprites or a gray 24×24 image. There was no interactive desktop test and no original-game comparison. No sprite was stretched into a tile, no curated binding was used, and no map-texture preview was activated.

```sh
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --terrain-selection-profile exe-6373328b-first-terrain-probe --cell 114 114 --cell 115 114
./build/openemperor --sg3 .local/gog-extracted/app/DATA/China_Terrain.sg3 --image 43
```

## SG3 resource-group lookup for key `0x603` (historical layout, superseded)

The locally supplied PE32 EXE was again checked as SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. Its image base is `0x400000`. The `.text` section has RVA `0x1000`, static VA `0x401000`, and PE file offset `0x400`, so a `.text` instruction's file offset is `RVA - 0xc00`; the SG3 offsets below are in a **different file**. Selected anchors checked against actual instruction VAs:

| Operation | Static VA | PE RVA | PE file offset |
| --- | ---: | ---: | ---: |
| group-key split / resource lookup | `0x408170` | `0x8170` | `0x7570` |
| table entry value read | `0x4081ba` | `0x81ba` | `0x75ba` |
| slot-3 registration calls archive loader | `0x5cd061` | `0x1cd061` | `0x1cc461` |
| signed SG3 index-word read | `0x5cd468` | `0x1cd468` | `0x1cc868` |
| runtime group-table write | `0x5cd519` | `0x1cd519` | `0x1cc919` |
| runtime-record index association | `0x5cd531` | `0x1cd531` | `0x1cc931` |
| range writer supplies `0x603` and manager | `0x4b39b3` | `0xb39b3` | `0xb2db3` |

The group lookup at VA `0x408170` receives the manager in `ECX` and key on the stack. For nonnegative `0x603` (`1539`), quotient by 512 is **slot 3**, remainder is **3**, and the remainder is decremented to runtime group position **2**. It forms the same slot-object address as registration: manager base VA `0x1c42130` plus `0x1000` and slot times the observed object stride `0x264cc`. The registration caller at VA `0x475c36` passes that manager to VA `0x5ccdf0`; the registration routine at VA `0x5ccfcd` computes the same slot-object address, passes that object to the loader at VA `0x5cd061`, and the writer at VA `0x4b39b3` supplies `0x1c42130` to the group lookup. This connects the named Terrain registration, group table, and image lookup to **one manager/object**, rather than merely matching a slot number from another list. Conditional registration success remains a prerequisite; the native diagnostic uses an explicit registration snapshot.

The v213 loader reads the SG3 header and its 300 little-endian index words into the static buffer beginning at VA `0x1b501c8`; index word 0 is at VA `0x1b50218`. At VA `0x5cd468` it sign-extends each word and subtracts the loader's record skip (zero for this ordinary Terrain name). Only strictly positive results enter the temporary list; zero and signed-negative words do not. The list insertion at VA `0x5cff20`/`0x41f730` prepends equal-key entries, and the loop at VA `0x5cd4fc` copies the list head-to-tail into the runtime eight-byte group table. Thus the retained SG3 index words appear in **reverse file order**, not at their original index positions. At VA `0x5cd51b`–`0x5cd524`, the value placed in entry field `+4` is `signed_word - skip - 1`. VA `0x4081d0` addresses eight-byte entries, and VA `0x4081ba` returns this field plus `slot * 16384`; the function returns a **complete packed graphic ID**, not a pointer or physical record index. For a key with remainder zero, the examined lookup returns zero rather than indexing the table; the native resolver treats it as outside the supported group-position profile.

In this local `DATA/China_Terrain.sg3` (v213), 55 of 300 index words are signed-positive. Runtime group position 2 therefore comes from **SG3 index word 53**, not word 2. The existing parser reads that word at **SG3 file offset `80 + 2*53 = 186` (`0xba`)** as raw `1368` (`0x558`). The stored runtime entry value is `1367` (`0x557`); group lookup returns **`0xC557` (50519)**. This is the physical image start only after the separate, already documented v213 translation: packed local 1367 → physical SG3 image record **1368**. No second `+1` is applied. `Sg3Archive::groups[6]` describes the resolved image as `China_land2.bmp` / `A new bitmap.`; that group metadata is unrelated to index-word position 53.

The range writer at VA `0x4b39a8`–`0x4b39c7` adds `(input_byte & 7)` to this returned packed ID. Supplying variants 0–7 explicitly produces the following independently decoded records from the user's local archive. All have in-bounds color payloads, no alpha payload, Type 30, 78×40, SG3 metadata group 6, and successful normal-loader decodes:

| Variant | Packed ID | Runtime local | Physical SG3 record |
| ---: | ---: | ---: | ---: |
| 0 | `0xc557` | 1367 | 1368 |
| 1 | `0xc558` | 1368 | 1369 |
| 2 | `0xc559` | 1369 | 1370 |
| 3 | `0xc55a` | 1370 | 1371 |
| 4 | `0xc55b` | 1371 | 1372 |
| 5 | `0xc55c` | 1372 | 1373 |
| 6 | `0xc55d` | 1373 | 1374 |
| 7 | `0xc55e` | 1374 | 1375 |

All eight PNG exports were generated only under ignored `.local/re/emperor/group-603/` and actually viewed offscreen. They appear as similar teal-gray diamond images with small detailed patches; no image was stretched or committed. This is visual inspection of our decoder, **not** an original-game comparison or evidence that this group composes the ground for a particular loaded map. There was no interactive desktop test.

The map comparison reads the separate byte range at **decompressed map logical offset `729311 + cell_index`**, using the existing container's bounded range API. Its general file-format meaning remains unknown; it is distinct from `candidate_byte_layer` at `209471 + cell_index`. For two exact `(terrain_raw=0x80, objects_raw=0)` cells per map, the unchanged stored IDs are all **below** `0xc557`; signed differences are shown so negative values cannot wrap into a false 0–7 match:

| Map / storage cells | Stored IDs | Input bytes at 729311+index | Stored minus base |
| --- | --- | --- | --- |
| Xia `(114,114)`, `(115,114)` | `0xc02a`, `0xc027` | 194, 254 | -1325, -1328 |
| Banpo `(114,114)`, `(115,114)` | `0xc028`, `0xc026` | 111, 73 | -1327, -1329 |
| Chengdu `(113,29)`, `(114,29)` | `0xc017`, `0xc00e` | 40, 58 | -1344, -1353 |
| Anyi `(113,29)`, `(114,29)` | `0xc029`, `0xc02a` | 184, 131 | -1326, -1325 |

No row equals `group_base + (stored_input_byte & 7)`. That mismatch does not invalidate the **pre-read** group writer: the map serializer subsequently replaces its per-cell graphic values. It does rule out quietly replacing these sampled saved IDs with the group's eight IDs in a loaded-map preview. The SDL-free `ResourceGroupLookup` implements only the bounded, explicit v213 group-table transformation, and `openemperor-map-graphics --group-key 0x603 --variants 8` reports it separately from saved-map correlation. It neither generates unknown byte state nor changes rendering.

```sh
./build/openemperor-map-graphics --data .local/gog-extracted/app --group-key 0x603 --variants 8
./build/openemperor-map-graphics --data .local/gog-extracted/app --group-key 0x603 --variants 8 --map Cities/Xia.map --cell 114 114 --cell 115 114
./build/openemperor --sg3 .local/gog-extracted/app/DATA/China_Terrain.sg3 --image 1368
```

## Runtime-table fidelity correction (2026-09-17)

Starting commit `d09887d7e0335aa5169786970fc96efba51b83d2`; the local EXE SHA-256 was rechecked as `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. The EXE was read for bounded static analysis, never executed or changed. Its `.text` section begins at VA `0x401000`, RVA `0x1000`, PE file offset `0x400`, so an instruction in that section has PE file offset `RVA−0xc00`. SG3 file offsets below refer to a different file.

### System-bitmap branch and image records

At VA `0x5cd346` (RVA `0x1cd346`) the v213 loader first checks that a registration slot exists. At VA `0x5cd354` (RVA `0x1cd354`, PE file offset `0x1cc754`), the first string-compare operand points to VA `0x1b50470`: the **filename of the first SG3 bitmap-group record**, whose SG3 record starts at file offset **680**. The second operand points to the literal `Zeus_system.bmp` at VA `0x86a6fc` (PE file offset `0x468cfc`). It is not the outer `.sg3` filename. On equality, VA `0x5cd38a` stores `0xc8` (200) in the skip local at stack+`0x14`; without equality the skip stays zero. The branch also stores `0x992e` at stack+`0x24` (VA `0x5cd392`), later passed to a bitmap helper at VA `0x5cef60`. Its further meaning is not asserted here.

The existing parser reports both local archives' group 0 as filename `Zeus_system.bmp`, `image_count=200`, `first_image_index=1`, `last_image_index=200`. Both are v213 with capacity 10,000. `China_Terrain.sg3` reports 1,443 images in use and eight bitmap groups; `China_Elevation.sg3` reports 372 and four. Both therefore activate the 200-record branch. At VA `0x5cd406`, the loader subtracts the skip from the reported count; its runtime count is **1,243** for Terrain and **172** for Elevation. At VA `0x5cd43b` it begins the contiguous runtime copy at intermediate physical record `skip+1`. The 64-byte physical SG3 records were converted to 72-byte intermediates in physical order at VA `0x5cd3c5`, and the selected resource object's lookup accesses its resulting 36-byte runtime records. The supported relation is **local `i` → physical SG3 record `201+i`** for these two archives. Physical record 0 is the dummy, records 1–200 are skipped by this branch, and the range is bounded by `reported_images_in_use−200`, not the 10,000-record capacity. This corrects the earlier `i+1` model; `AssetId` and catalog record indices remain physical and unchanged. No claim is made about other versions or registrations.

### Group list keys and order

The v213 loop at VA `0x5cd468` reads each of 300 SG3 little-endian index words, sign-extends it, subtracts the same skip and retains it only when the result is positive. The loop visits SG3 index positions in ascending **file order**. VA `0x5cd45a` initializes `ebx` to zero; VA `0x5cd4a5` passes `ebx` as the list insertion **key**, and VA `0x5cd4b9` increments it at every index position, including a filtered one. The transformed index word is the **value**, not the key. VA `0x5cff20` forwards the key to the node constructor at VA `0x41f710`, which stores it at node+8. VA `0x41f730` compares keys and inserts increasing unique keys at the tail through VA `0x467e90`; equality/prepend behavior is irrelevant because these caller keys are distinct. Equal positive **values** are allowed. VA `0x5cd4fc` traverses from the list head, preserving file order in the final group table. VA `0x5cd519` writes the transformed value (`signed word−skip−1`) into its group entry; VA `0x5cd531` associates groups with runtime image records but does not reorder the contiguous image copy. The group lookup at VA `0x4081ba` adds `slot×16384` to that local base. Thus group key `0x603` splits into slot 3 and one-based group position 3, selecting the **third retained** Terrain index word. The old reverse-order interpretation mistook an increasing insertion key for a constant key.

The local Terrain index at position 3, **SG3 file offset 86**, contains 247. Its transformed local base is `247−200−1=46`, so group `0x603` returns packed base **`0xc02e`**. Variants 0–7 resolve to local 46–53 and physical records **247–254**, not the old `0xc557`/1368–1375. The analogous Elevation key `0x2001` selects index position 1, raw 359, local base 158 and physical record 359; its packed base is `0x4009e`. These are table identities, not a claim that a particular map cell uses the group at draw time.

| Key or saved ID | Previous diagnostic | Corrected v213 result | Runtime local / physical SG3 record | Type and dimensions after correction | Payload / decode |
| --- | --- | --- | --- | --- | --- |
| Terrain group `0x603`, variant 0 | `0xc557`, record 1368, Type-30 78×40 | `0xc02e` | 46 / 247 | Type-30 78×41 | in bounds / success |
| Terrain variants 1–7 | `0xc558`–`0xc55e`, records 1369–1375, Type-30 78×40 | `0xc02f`–`0xc035` | 47–53 / 248–254 | Type-30 78×40 except variant 6: 78×46 | in bounds / all seven successful |
| Terrain saved `0xc027` | record 40, Type-276 4×9 | same ID | 39 / 240 | Type-30 78×46 | in bounds / success |
| Terrain saved `0xc02a` | record 43, Type-276 5×9 | same ID | 42 / 243 | Type-30 78×48 | in bounds / success |
| Terrain saved `0xc0c9` | record 202, Type-30 78×41 | same ID | 201 / 402 | Type-30 78×54 | in bounds / success |
| Elevation group `0x2001`, variant 0 | old reverse/zero-skip model | `0x4009e` | 158 / 359 | Type-30 158×125 | in bounds / success |

Every corrected row uses the same **skip 200 plus dummy 1** rule. The eight Terrain group images and the three selected saved-ID images were decoded with the existing loader and viewed as local offscreen PNGs under ignored `.local/re/emperor/runtime-table/`. The group images look like sandy diamonds with details; the saved-ID images have grassy or vegetation/rock details. The Elevation image was also decoded and viewed there. The direct `openemperor --sg3 ... --image 240` path loaded a 78×46 texture and remained in its SDL event loop under SDL's dummy video/software renderer; the current shell session could not initialize a macOS GUI window. These are qualitative views of OpenEmperor's decoded output and a headless preview-path check, not an interactive first-draw test or original-game pixel comparison.

For the previously selected exact `(terrain_raw=0x80, objects_raw=0)` cells, the map words are unchanged. The difference from the corrected group base `0xc02e` remains outside variants 0–7:

| Map / storage cells | Stored IDs | Byte at logical `729311+cell_index` | Stored minus `0xc02e` |
| --- | --- | --- | --- |
| Xia `(114,114)`, `(115,114)` | `0xc02a`, `0xc027` | 194, 254 | -4, -7 |
| Banpo `(114,114)`, `(115,114)` | `0xc028`, `0xc026` | 111, 73 | -6, -8 |
| Chengdu `(113,29)`, `(114,29)` | `0xc017`, `0xc00e` | 40, 58 | -23, -32 |
| Anyi `(113,29)`, `(114,29)` | `0xc029`, `0xc02a` | 184, 131 | -5, -4 |

The separate candidate byte is at logical `209471+cell_index`; no general meaning is inferred for either byte. The observed group writer runs **before** the saved map arrays are read, so its `0x603` expression cannot be substituted for these loaded words. The corrected saved IDs now resolve to tall Type-30 images, but that alone does not establish their placement, compositing, or retention at first draw. The remaining focused dependency is to follow a selected loaded cell's value through the post-read writers and the VA `0x46f0e0` → `0x470167` → `0x408100` lookup/draw path, including how any overlay relates to an underlying tile. No map texture rule was enabled from this evidence.

`src/maps/RuntimeArchiveLayout.*` is the shared SDL-free v213 model for group and image resolution; it is constructed from parsed user-supplied SG3 metadata at runtime, with no EXE dependency. Synthetic tests distinguish file order from sort/reverse, exercise equal and filtered values, both skip branches, dummy/count bounds, and shared group/image mapping with always-active checks in Debug and Release. Current commands with legally obtained local data:

```sh
./build/openemperor-map-graphics --data .local/gog-extracted/app --layout-profile exe-6373328b-v213-runtime-table --group-key 0x603 --variants 8 --graphic-id 0xc027 --graphic-id 0xc02a --graphic-id 0xc0c9
./build/openemperor-map-graphics --data .local/gog-extracted/app --layout-profile exe-6373328b-v213-runtime-table --group-key 0x2001 --variants 1
./build/openemperor-map-graphics --data .local/gog-extracted/app --map Cities/Xia.map --profile exe-6373328b-v213-runtime-table --cell 114 114 --cell 115 114
./build/openemperor --sg3 .local/gog-extracted/app/DATA/China_Terrain.sg3 --image 240
```

## Stored graphics map snapshot (implementation milestone)

Starting repository commit `31016f17048869f646665125b04a1a553b25a948`. This milestone did not repeat EXE analysis or change the v213 table rule above. It connected the saved map-word layer to the explicit `RuntimeArchiveLayout` diagnostic in a new `--view stored-graphics` mode, without substituting the pre-read group-`0x603` writer or a curated terrain binding. The candidate mask and separate off-map bit are preserved. Only supported one-cell, 78-wide Type-30 images with a 3,200-byte base, height at least 40, valid source, and no unverified mirror are drawn. A full decoded RGBA image, including an Omega overlay, is positioned by the **preview** anchor `(width/2,height−40)` so its lower footprint aligns with the logical cell. This is not evidence of the original game's final placement or first-draw composition.

The local GOG files were read without modification. Debug/headless software-render runs with the same implementation yielded the following final **render statuses**, rather than metadata-only candidate counts:

| Original map | Candidate cells | Rendered cells | Multi-cell placement unverified | Excluded storage cells | Distinct referenced assets | Successfully decoded / uploaded assets | Logical RGBA texture bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Cities/Xia.map` | 3,612 | 3,588 | 24 | 48,372 | 294 | 291 | 4,618,224 |
| `Cities/Banpo.map` | 6,384 | 6,340 | 44 | 45,600 | 282 | 273 | 4,419,168 |
| `Cities/Chengdu.map` | 14,620 | 14,616 | 4 | 37,364 | 341 | 340 | 5,159,232 |
| `Cities/Anyi.map` | 14,620 | 14,560 | 60 | 37,364 | 396 | 393 | 6,043,128 |

Each candidate cell has one final status. The currently examined four maps needed no other failure category, but the plan and renderer retain explicit unsupported-slot, out-of-range, empty, missing-source, unverified-mirror/layout, and decode-failed statuses. The previously selected IDs `0xc027`, `0xc02a`, and `0xc0c9` are resolved by the same layout to physical Terrain records 240, 243, and 402; they are not hardcoded bindings. No archive was decoded wholesale: the upload counts are distinct successful images, with a single texture reused by all referencing cells.

An offscreen SDL software-render capture of the **whole Xia map** and a higher-zoom central excerpt was actually viewed locally under ignored `.local/re/stored-graphics/`. The first showed broad contiguous grassy and sandy areas, blue-gray water-like stretches, cliffs, detailed overlays, and purple diagnostic gaps for unsupported placement. The zoomed excerpt showed uncut taller overlays and aligned lower footprints. These observations describe OpenEmperor output only; there was no interactive desktop run or comparison with original-game pixels. Temporary captures and their helper source remain ignored locally, not in the repository. The exact remaining evidence gap is whether saved per-cell IDs survive later post-read writes to the original first draw, plus original multi-cell placement and compositing.

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Xia.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table
```

## Isolated multi-cell saved-ID preview (2026-09-17)

Starting commit `b23e7e8fb36359db6b525379a4692f96e6f9f366`. This bounded check reused the corrected `RuntimeArchiveLayout`, normal Type-30 decoder, and existing saved-map reader. It did **not** execute or modify the Windows EXE, inspect a new general archive corpus, or establish the original draw path. The four previously unresolved Chengdu cells and their immediate x=53–58, y=131–136 neighborhood were read from the user's ignored GOG files:

| Storage | Saved ID | `candidate_byte` | Terrain / object raw | Physical AssetId | Type / width×height | Base bytes / size flag | Prior status |
| --- | --- | ---: | --- | --- | --- | --- | --- |
| `(55,133)` | `0xc10a` | 0 | `0x200082` / `0` | `DATA/China_Terrain.sg3#467` | 30 / 158×95 | 12,800 / 2 | `multi_tile_placement_unverified` |
| `(56,133)` | `0xc10a` | 1 | `0x200082` / `0` | same | same | same | same |
| `(55,134)` | `0xc10a` | 72 | `0x200082` / `0` | same | same | same | same |
| `(56,134)` | `0xc10a` | 9 | `0x200082` / `0` | same | same | same | same |

The four coordinates form one complete 2×2 candidate-mask block. The neighboring candidate cells have different saved IDs, so this is an isolated component of exact saved-ID references; all four records resolve to the same physical asset, whose color payload is in bounds and decodes with the existing 2×2 Type-30 geometry. No separate part records were found in these four references. The differing bytes `0,1,72,9` are **observations only**: their bit meanings, any original anchor/part-position meaning, and later draw-time use are unknown. The identical terrain/object values also do not identify an anchor.

Our optional `--multi-tile-preview` therefore implements the **independent preview convention** “one isolated, complete four-neighbor 2×2 component of identical saved IDs and matching 158-wide/12,800-byte/size-flag-2 metadata → one placed image.” Its minimum `(x,y)` is merely the rear/top origin of our established 80×40 isometric projection. We use image anchor `(width/2,height−80)`, preserving the full overlay above the 80-pixel base. This rule does not assert which, if any, original cell is the draw anchor. Multiple touching components with the same saved ID are not partitioned. The renderer sorts by frontmost ground depth with projected-x/source-order ties; arbitrary interlocking buildings and original first-draw composition remain unknown.

The same unchanged rule, run after the Chengdu check, gave **actual successful decodes/uploads** and covered cells as follows:

| Map | Candidate | Covered by rendered instances | 1×1 / 2×2 placed | Decoded / uploaded distinct assets | Remaining diagnostics |
| --- | ---: | ---: | ---: | ---: | --- |
| Chengdu | 14,620 | 14,620 | 14,616 / 1 | 341 / 341 | none |
| Xia | 3,612 | 3,612 | 3,588 / 6 | 294 / 294 | none |
| Banpo | 6,384 | 6,376 | 6,340 / 9 | 282 / 282 | 8 `ambiguous_footprint` |
| Anyi | 14,620 | 14,620 | 14,560 / 15 | 396 / 396 | none |

Banpo's eight unresolved cells form x=103–104, y=143–146, all saved ID `0x400a1` and physical `DATA/China_Elevation.sg3#362` (158×127, 12,800-byte base, flag 2). They look like two touching 2×2 squares and their bytes repeat `0,1,72,9`, but no independently verified anchor rule splits this 2×4 connected component, so all eight remain purple diagnostics. This avoids falsely reporting them as two drawn images.

Headless SDL software rendering produced whole-map and targeted, 3× zoom views of each of the four maps; these ignored `.local/re/stored-graphics/multi-*` captures were actually viewed. Chengdu's isolated larger rock/cliff image appeared once and aligned plausibly with surrounding one-cell images. Banpo's ambiguous component stayed visibly purple. Those are observations of **our renderer**, not an original-game pixel comparison. There was no interactive desktop run. The saved-word versus original first-draw question remains open and does not block this diagnostic snapshot.
