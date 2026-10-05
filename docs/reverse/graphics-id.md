# Graphics-ID investigation (2026-09-17)

The current [Pass-4 study](#pass-4-great-wall-model-and-selector-evidence-2026-10-04) establishes serialized entities, external Models and the bounded producer/anchor path. Real-map activation remains zero because deserialization recomputes material from unresolved mode/mission/player context. The Pass-3 archive and missing-link statements below retain their historical scope.

## Pass 3: regenerated Rock, Pinnacle, ordinary Wall and Great Wall boundary

**EXE-OBSERVED / VERIFIED REPRODUCTION:** bounded Rock keys `0x606/0x607/0x608` retain their common eight singleton, four 2×2 and two 3×3 variants. Raw-mask selection and origin variation drive fresh row-major occupancy; historical ID groups do not define generated footprints. Pinnacle `0x53fa20` independently requests object-bank keys `0x60d/0x60a/0x609/0x619`, each one static side-5 Type-30 record (Terrain physical 1440–1443 after shared lookup). Ordinary wall `0x4b67b0` uses an eight-neighbor 16-row table and key `0x451`, with generated straight alternation; slot 2 `China_General` resolves local 720 to physical 921. Records 921–938 are the bounded static Type-30 family, while gates and additional Type 256 components 939–953 remain unresolved. Resource keys, packed IDs, runtime locals and physical AssetIds remain separate types/domains.

**RAW MAP FACT / archive metadata:** the complete slot 8 monument group is **42** records, local 0–41 → physical 201–242. The earlier map-observed locals 0–40 did not cover its final record. All records are static/unmirrored; 32 are side-4 Type-30, two are side-2 Type-30, and eight are plain 51×51 Type 1 with unsupported map footprints. There is no side-1 Type-30 family. The read-only 167-map census still finds 12,416 historical slot 8 candidate cells on 18 maps; that number is snapshot provenance.

**EXE-OBSERVED:** Great Wall packed IDs are produced by entity/piece-state helper `0x57bba0`, Earthen registration at `0x57be4d → 0x57c049`, piece/view mapper `0x57c0e0`, and group request **`0x1001`** at `0x57c06b → 0x408170`, followed by mapped variant addition at `0x57c07a`. The choice depends on restored original entity extended type/material/orientation and piece index, not raw `0x4000`, saved slot 8 IDs or a Road-like 16-mask rule. Post-load entity restoration `0x52f030` runs after the ID clear and before landscape setup, reaching virtual placement dispatch at `0x4b1228`; the serialized entity/piece-state link to this producer remains unresolved.

**REFERENCE-DERIVED / OPENEMPEROR PREVIEW:** the saved 4×4 policy and its whole-image anchor remain explicitly diagnostic. Generic rectangle-writer part/marker evidence and local width318 overlay arithmetic do not prove the complete restored Great Wall placement path. **Great Wall regenerated-selector verification and regenerated-instance counts remain zero.** Full original first-draw identity/anchor/composition are unverified; the signed saved-height ×40 behavior is frozen. The complete record audits, raw census and exact unresolved boundary are in the [Pass-3 report](map-first-draw.md#original-map-fidelity-pass-3--mountains-rocks-walls-and-great-wall-2026-10-04).

**First-draw follow-up (2026-10-04):** The [bounded map-first-draw study](map-first-draw.md) found a post-read clear: serializer return → VA `0x43ae05` → `0x53d100` → `0x5355f0`, with all 51,984 saved IDs zeroed at VA `0x53561e` (RVA `0x13561e`). Subsequent candidate-row generation includes terrain/elevation/water selectors before the drawing entry `0x53c930 → 0x46a220`. The older statement below that no relevant post-read clear was found is superseded. Saved-ID resolution remains snapshot provenance; exact regenerated first-draw IDs and complete composition remain unverified. The same study establishes a separate signed height grid and 40-pixel draw operand, with conditional post-load height normalization still open.

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

The renderer-side chain is separately observed: at RVA `0x700e0`, a map-cell index loaded from a view grid selects a 32-bit value from VA `0xfe9880 + 4*cell_index` (the earlier `0x6f0e0` citation was an address error: that window reads the object array). That value is placed in VA `0x10c7388`; at RVA `0x70167` it is pushed to the related graphic lookup at RVA `0x8100`. A second path at RVA `0x709c4` loads the same array and at RVA `0x70a29` passes it to that lookup. Thus there is a static **map-load → per-cell array → graphic lookup** chain for this profile. The exact value at a particular later draw is **not verified**: the array is cleared at RVA `0xb08a8` and other routines, including RVA `0xb39b3`–`0xb39c9`, can replace cells with computed graphic IDs. The sampled file word's identity is preserved in the diagnostic; it must not be presented as an observed runtime draw or original-game visual match. The 4×9 sprite lacks proven map placement; the 78×41 image is not the currently supported flat 78×40 preview layout. No graphics-candidate map texture preview was activated.

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

## Edge-byte adjacent 2×2 preview (2026-09-17)

Starting commit `21077d246c2fdd92fb8c6d744a3f1db35f63e645`. No original data or EXE was changed. The separate opt-in `--multi-tile-preview --footprint-policy edge-byte` uses the unchanged candidate byte and a pinned public [Julius edge-grid reference](https://github.com/bvschaik/julius/blob/34d1ecd54befb845c0139b371fa8a0438210dac1/src/map/property.c). Only 2×2 Type-30 images with width 158, base 12,800 bytes, size flag 2, valid source, and no mirror are considered. The independently implemented profile computes `origin=storage-(raw&7,(raw>>3)&7)` in signed arithmetic. Four exact distinct parts, identical saved ID and physical AssetId, and mask-contained claims are required. The old isolated-component policy remains the default meaning of `--multi-tile-preview`; there is no fallback between policies. `0x40` is reported as a marker candidate, while `0x80` stays unknown.

A bounded static check of the locally available PE32 `Emperor.exe` first verified SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e` and the `.text` mapping (VA `0x401000`, RVA `0x1000`, file offset `0x400`). At VA `0x46f112` the current map index comes from `[0x00adf8e8]`; VA `0x46f12f` tests `0x40` on byte array `0x00fdcd70 + index`, and the clear branch goes to `0x46f3e9`. RVA `0x6f12f`, file offset `0x6e52f`. VA `0x4709f0` loads the same array byte into `ebx`, followed by `test 0x40` at `0x4709fa`; VA `0x46fa5e` likewise tests `0x40` on a value just loaded from that array. These are bounded, draw-related control-flow observations, not a complete reconstruction of selected image or screen placement. A nearby `and 7` at VA `0x46f18a` operates on the *other* array at `0x00f1e780`; it is not evidence for subtile bits here. The low `0x07/0x38` position fields and the effect of `0x80` have not been established from Emperor code, nor has the original image anchor or first-draw state. No full code dump is retained. See [map-format evidence](map-format.md#candidate-byte-subtile-hypothesis-2026-09-17).

Read-only local map planning with this fixed policy yields Chengdu 1 / 14,620 covered, Banpo 11 / 6,384 covered, Xia 6 / 3,612 covered, Anyi 15 / 14,620 covered (2×2 instance count / candidate cells). The isolated policy respectively yields 1 / 14,620, 9 / 6,376, 6 / 3,612, and 15 / 14,620; Banpo's eight remaining isolated cells are `ambiguous_footprint`. The edge-byte plan has zero marker deviations and unknown high bits on these four maps. Planning alone is separate from decode and rendering; visual outcomes and remaining limits are in [preview notes](../stored-graphics-preview.md).

## Focused slot-8 registration extension (2026-09-18)

Starting commit `4e776df2b8164ab96685da0a3b6a971e83ce2ca8` had a clean working tree. The existing local baseline report covers 167 standalone maps: 57 complete and 110 partial saved-ID snapshots. Native per-cell rechecks of the 59 maps with `unregistered_slot` gave:

| Slot | Candidate cells | Maps | Distinct stored IDs | Observed local index range |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 12,416 | 18 | 37 | 0–40 |
| 6 | 4,224 | 8 | 3 | 14–32 |
| 2 | 2,388 | 35 | 116 | 169–1702 |
| 5 | 617 | 16 | 48 | 307–376 |
| 0 | 15 | 6 | 1 | 0 |

These disjoint cell counts sum to the baseline's 19,660 unknown cells; map counts overlap. The rule is maximum candidate-cell count, then lower slot number on a tie. Before examining the archive, `Cities/Badaling.map` was selected as the representative and distinct `Cities/Handan.map` as the countercheck. Their raw example IDs include Badaling `(84,61)=131096` (slot 8/local 24) and Handan `(96,49)=131088` (slot 8/local 16). This is a saved-ID analysis, not proof of the original first draw.

The locally supplied PE32 `Emperor.exe` again hashes to SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. Its image base is `0x400000`; `.text` RVA `0x1000` maps to static VA `0x401000` and PE file offset `0x400`. At static VA **`0x475c7b`** (RVA `0x75c7b`, PE file offset `0x7507b`), the pointer to `China_Mon_Earthen_Greatwall_1` (VA `0x82b100`, `.data` PE file offset `0x429700`) is stored at stack offset `+0x24` after one push. Relative to the array base captured before that push, this is **position 8**, not position 9. The 41-position registration dispatcher at VA `0x5ccdf0` reads the manager's current position at VA `0x5ccdf0`, selects that array entry at `0x5cce02`, checks for a nonempty name, and reaches `0x5ccf70` via `0x5cce12` and `0x5cce70`. The registration routine computes the slot-specific object in the same manager at `0x5ccfcd`, passes the name and slot to the v213 loader at `0x5cd061`, and advances the current position at `0x5cd0b3` on its observed success path. Thus position 8 is on the same resource manager used by the map graphic lookup at VA `0x408150` (and group lookup at `0x408170`), conditional on the preceding registration state, successful file open, and loader success. These static anchors do not prove that every game context loads every entry. No original EXE was executed or modified; no full disassembly is retained.

The matching locally supplied `DATA/China_Mon_Earthen_Greatwall_1.sg3` is v213, capacity 10,000, reported images in use 242, with two bitmap groups. Its first **embedded** bitmap-group filename is `Zeus_system.bmp`, reporting 200 images and physical indices 1–200; the next group is `China_Mon_Earthen_Greatwall_1.bmp`, reporting 42 and indices 201–242. The same loader's literal 200-record branch at VA `0x5cd354`–`0x5cd43b` therefore applies. Together with physical dummy record 0, the verified runtime range is 42 local records: local `i` → physical SG3 record `201+i` for `0≤i<42`. The archive's index word 1 is 201 and yields group local base 0 after the same skip; this group-table path is distinct from the outer SG3 name and from physical `AssetId` numbering. The new profile requires this exact first-group signature and v213; it does not enable other SG3 versions or apply `+201` outside `RuntimeArchiveLayout`.

This evidence supports a **registration and runtime layout**, not a footprint rule by itself. Physical record 201 is Type 30, 318×160, size flag 4, with a 51,200-byte base, and other selected records are similarly large. At the registration milestone, the map renderer accepted only 78-wide one-cell and specifically validated 158-wide 2×2 placements, so these larger cells remained `unsupported_footprint_size` or another precise diagnostic. The later opt-in 4×4 preview is documented below; it did not change the registration. The opt-in profile is `exe-6373328b-v213-slot8-runtime-table`; the base two-slot profile remains unchanged. The selected map and full-corpus outcomes are documented separately in [preview notes](../stored-graphics-preview.md).

The selected Badaling `(84,61)` saved ID `131096` resolves as slot 8/local 24 to physical record **225** (SG3 file offset `40680+64×225=55080`), Type 30, 318×167. Its internal payload is in bounds and a targeted normal-loader decode succeeds. Handan `(96,49)` uses slot 8/local 16 and physical record 217. All referenced slot-8 locals 0–40 map to physical records 201–241, each with a present in-bounds internal color range. Across the 18 affected maps, the earlier extended-profile/`edge-byte` baseline reclassifies 12,416 previously unregistered cells: 12,096 `unsupported_footprint_size`, 224 `unsupported_layout`, and 96 `rendered` after 18 map-local extra successful decoder/uploads. No complete-map count changes (57/167), and no earlier complete map regresses. These are disjoint cell outcomes; class map counts overlap. The local baseline and extended JSON reports remain ignored under `.local/reports/`.

The subsequent 4×4 placement study checked Badaling's original candidate cells `(84..87,61..64)` individually. All 16 retain saved ID `131096`, resolve to the same physical record 225, and carry part bytes by rows `00 01 02 03`, `08 09 0a 0b`, `10 11 12 13`, `58 19 1a 1b`. The `0x40` marker candidate is at part `(0,3)`; no unknown `0x80` bit appears. Their reference-derived positions each propose origin `(84,61)`. The image is 318×167, size flag 4, 51,200-byte base, with a valid source and normal-loader decode. This supports one **OpenEmperor preview** instance under the new explicit `edge-byte-4x4` policy. It does not prove the original game's anchor, first-draw order, or full semantic meaning of the low byte bits. The earlier `edge-byte` baseline continues to reject this size.

## Pass 4: Great Wall Model and selector evidence (2026-10-04)

The earlier 42-record `China_Mon_Earthen_Greatwall_1` registration remains historical layout evidence. It is not the archive selected by every restored Great Wall entity. The pinned EXE and [serialized record sources](map-format.md#pass-4-serialized-original-entities-and-great-wall-state-2026-10-04) now establish the following bounded path, without execution of the original binary.

`0x5635a0` chooses `MonumentPlan` from the entity's signed base type `+14`, checks its signed subindex `+16` against `0x567610`, and selects an 80-byte `SubBuildingInfo` row. Its controller index `+0c` selects the static controller array `0x85b2d0`. `0x563670` invokes controller virtual `+14`: wall `0x57bba0`, gate `0x57cb10`, tower `0x57d2b0`, road `0x57d860`. This is distinct from raw terrain topology or a saved slot-8 image footprint.

**EXE-OBSERVED external Model dependency:** registration `0x564976..0x5649bd → 0x567650 → 0x56b270` associates base type 256 with `Model/Mon_Great_Wall_04_subs.txt`, type 257 with `_05_`, and type 259 with `_07_`. Actual MPWall1 records use **type 256/Model 04**, despite their filename; their layout cannot be selected by a map title. The Model loader's six-column format at `0x85a304` reads signed x/y offsets, type token, elevation, position token and piece token. Writes `0x56b44f..0x56b4a7` establish runtime row offsets `+04/+08/+0c/+14/+18/+1c`; `0x56b6d1..0x56b6dc` derives side `+10` through controller virtual `+1c`. The `SubBuildingInfo` serializer `0x570120` contains no serialization of these six values: they come from Model text, not map bytes.

| Model token | Controller index | Side | Producer | Phase count |
| --- | ---: | ---: | ---: | ---: |
| `SB_GREAT_WALL` | 14 | 4 | `0x57bba0` | 11 |
| `SB_GREAT_WALL_GATE` | 15 | 2 | `0x57cb10` | 2 |
| `SB_GREAT_WALL_TOWER` | 16 | 4 | `0x57d2b0` | 12 |
| `SB_GREAT_WALL_ROAD` | 17 | 1 | `0x57d860` | 3 |

Direction lookup `0x56af60` gives NORTH=0, NE=1, EAST=2, SE=3, SOUTH=4, SW=5, WEST=6, NW=7, CENTER=8. The examined models use position NORTH/EAST and gate piece directions NW/NE/SW/SE. Numeric wall pieces are 0–25, tower pieces 26/27, and road piece 0. These controller domains do **not** admit every archive variant 0–41 as a valid Model piece.

`GreatWallModels` independently parses only the required 04/05/07 files, with complete ordered counts 53/53/51, bounded token/number domains and phase rows. It embeds no original row table. A preparation read is limited to 64 KiB, resolves the selected root and required regular file before opening, rejects an escaping symlink, and increments the existing `FileReads` counter once for the payload. Rendering uses the prepared model and does no Model-file reads.

The wall selector combines Model position, original camera view and saved orientation as `(position - camera + 2*orientation) mod 8`. `0x57c0e0` has explicit even-view mappings: in effective view 0 ordinary pieces retain their numeric variant, while 26→25 and 27→24. The tower's phase-11 branch `0x57d474` instead retains 26/27 in effective 0/4 and swaps them in 2/6; phase 10 still selects the remapped wall base. Construction phase is the numeric serialized `cMonInfo +08`, while material is the separately derived restore context, not the raw saved `+5c`.

Registration branches `0x57be29..0x57c020` select phases 1/4/7/10 into slot 8/group `0x1001`, phases 2/5/8 into slot 9/group `0x1201`, and phases 3/6/9 into slot 10/group `0x1401`. Material 1 names `China_Mon_Greatwall_Ruined`; material 2 names `China_Mon_Earthen_Greatwall_<phase>`; material 3 names `China_Mon_Greatwall_<phase>`. Phase-11 towers use archive phase 10. `0x5ccf70(name,slot)` compares the existing slot's name, retains a matching registration, and replaces a different registration through `0x5cd04a..0x5cd061 → 0x5cd1f0`, passing the slot and `slot<<9`. Slot identity is a mutable resource-manager context; it is never a permanent association with Earthen phase 1.

The gate and road producers make **no archive registration call**. Gate phase 1 with material 2/3 inherits the current slot-8 group; material 1 follows variant 31. With Model position EAST, saved orientation 0 and camera 0, effective view is **2**, and gate NW/NE/SW/SE selects variants **31/28/30/29**. Phase-1 roads select group `0x61e` (slot 3) plus variant 1 for EAST; phase-2 roads inherit group `0x1001` plus variant 40 for EAST. Thus a gate's numeric phase 1 cannot independently select a phase-1 archive. Prepared lookup snapshots must preserve restored entity order and each required registration, then use ordinary `ResourceGroupLookup`/`RuntimeArchiveLayout` for physical records.

Road placement uses a separate singleton path: `0x57d9e3 → 0x5724e0` writes the selected graphic, original entity ID, size bits zero and draw marker `0x40` at the exact entity cell. It also ORs original runtime terrain with `0x48`; that mutation is evidence, not authority for changing OpenEmperor's raw terrain. The producer then returns flags `0x08` at `0x57d9f8`, causing `0x56413c` to skip the generic `0x563dd0` rectangle writer. A side-one static road raster must therefore be described by this singleton source, while wall/tower/gate claims use the rectangle writer.

| Actual map | Model | Walls / phase | Towers / phase | Gates / phase | Roads / phase |
| --- | --- | ---: | ---: | ---: | ---: |
| Badaling | 05 | 39 / 10 | 6 / 11 | 4 / 1 | 4 / 2 |
| Handan | 04 | 40 / 10 | 5 / 11 | 4 / 1 | 4 / 2 |
| MPWall1 | 04 | 40 / 10 | 5 / 11 | 4 / 1 | 4 / 1 |
| MPWall2 | 07 | 38 / 10 | 5 / 11 | 4 / 1 | 4 / 1 |

**RAW ARCHIVE FACT:** actual phase-10 stone and earthen archives each have 42 runtime records after their separately validated system-bitmap skip: 28 side-4 Type-30 variants 0–27, eight side-2 Type-30 variants 28–35, four Type-1 variants 36–39, and two side-1 Type-30 variants 40/41. Every variant selected by these four maps with explicit restore material 2/3 is Type 30. The old Earthen-phase-1 finding of eight unsupported Type-1 records does not describe phase 10. The Ruined archive instead has 40 runtime records: 28 side-4 Type-30, eleven side-1 Type-30 and one side-2 Type-30 at variant 31; variants 40/41 are absent. A material-1 phase-2 road therefore cannot borrow those records from another archive.

The pure `GreatWallSelector` returns required registration when present, slot/group/variant, side, flags and a named unsupported reason. Missing derived material context, odd camera views, unsupported saved orientation or piece domains, invalid kind-specific phases, and the still-unimplemented phase-zero material override fail closed. Its support describes a static selector, not proof that the standalone loader has supplied its real mission/player context or that complete draw composition is verified. No stored graphic ID, terrain word, height or attractive asset choice fills that missing authority.

## Ruined phase-2 road compatibility (2026-10-05)

This bounded recheck starts at clean
`aed9619fe2bd3aaec79118548313fa4772308a46`. Earlier Pass-4 and activation
reports above retain their historical meanings. **Status: LIMITATION
EXPLAINED**, not a repaired original transition or a new rendering PASS.
The technical issue concerns restored monument Road pieces, not sandbox
road commands, topology, costs or navigation.

**EXE-OBSERVED:** the unchanged local executable again hashes to
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The static audit verified 1,586 disassembly-byte lines in 27 bounded windows
against PE-translated bytes, with zero mismatches. No original executable
was run. Code addresses use ImageBase `0x400000`, `.text` RVA `0x1000` /
raw offset `0x400`: physical offset = code VA minus `0x400c00`. Runtime
object offsets, serialized map offsets and physical SG3 indexes remain
separate address spaces.

| Road observation | VA | RVA | Physical EXE offset |
| --- | --- | --- | --- |
| Producer entry | `0x57d860` | `0x17d860` | `0x17cc60` |
| Get entity extended state through virtual `+1ec` | `0x57d888` | `0x17d888` | `0x17cc88` |
| Read signed phase at extended `+08` | `0x57d91c` | `0x17d91c` | `0x17cd1c` |
| Compare signed phase with 2 | `0x57d98a` | `0x17d98a` | `0x17cd8a` |
| Request group `0x61e` | `0x57d991` | `0x17d991` | `0x17cd91` |
| Request group `0x1001` | `0x57d9b8` | `0x17d9b8` | `0x17cdb8` |
| Add decimal 40 to group base | `0x57d9cd` | `0x17d9cd` | `0x17cdcd` |
| Axis-dependent increment | `0x57d9e2` | `0x17d9e2` | `0x17cde2` |
| Call singleton writer | `0x57d9ec` | `0x17d9ec` | `0x17cdec` |
| Return producer flag `0x08` | `0x57d9f8` | `0x17d9f8` | `0x17cdf8` |
| Phase-1 direction table | `0x57da0c` | `0x17da0c` | `0x17ce0c` |
| Phase-2 direction table | `0x57da28` | `0x17da28` | `0x17ce28` |
| Road phase-count virtual | `0x576c40` | `0x176c40` | `0x176040` |
| Creation material helper call | `0x5638a3` | `0x1638a3` | `0x162ca3` |
| Completion phase-count call | `0x563bc4` | `0x163bc4` | `0x162fc4` |
| Completion phase write | `0x563bcc` | `0x163bcc` | `0x162fcc` |

Constructor `0x57d848` installs Road vtable `0x7b99f0`, whose virtual
`+14` points to `0x57d860`. Controller-array entry 17 at `0x85b314` names
runtime singleton `0x12a6ed8`. Immediate dispatch
`0x563670 -> virtual +14` at `0x5636a0` resolves the entity's Model piece
through `0x5635a0`. The placement path
`0x56a139 -> 0x563fd0 -> 0x56411c -> 0x563670` does not normalize its phase
in the inspected windows.

The surrounding caller order needs a narrower label than “one saved record,
one original restore.” Temporary-manager reconstruction `0x52f030` advances
its pointer array by four at `0x52f161`; after placement, `0x52f155` retains
the type in EDI. Its monument check `0x52f0ad..0x52f0b3` skips following
same-type entries. This call supplies creation flag 1 through
`0x4b11f0 -> virtual +100`: `0x56a106` takes
`0x56a124 -> 0x563850`, rather than the direct `0x56a139` branch.
The nested creation path assigns Model subindices in ascending order,
initializes new phases to zero, and calls `0x563fd0` for ascending contiguous
IDs at `0x563ab3`. Its optional completion loop sets controller phase-count
minus one at `0x563bcc`, then redraws ascending IDs at `0x563bdc`.
These are newly created composition states, not a saved phase-2-to-1
normalization rule.

The Road phase-count countercheck is concrete: vtable `0x7b99f0 +0c`
points to `0x576c40`, which returns constant 3 without reading material or
entity state. Completion therefore writes Road phase **2**. At
`0x563b14..0x563b31` completion requires mode `0x88ec38 == 1`, or
predicate `0x53a4e0(type)` plus created material 1. The raw jump tables
`0x53a504`/`0x53a50c` make that predicate true for the examined types
256/257/259. Creation obtains material with `0x563720(type)` at
`0x5638a3` and stores it in each new state's `+5c` at
`0x56394f`/`0x563a52`. For types 253..268 the helper uses the same
mode/current-player goal decision as restore argument -1; its default
returns 1 for a type, while -1 returns 0 before the restore tail converts
it to 1. Thus the inspected material-1 completion path also sets Road
phase 2, not phase 1. This excludes that specific proposed normalization;
it does not establish complete original lifecycle/resource invariants.

The separate loaded-manager redraw `0x4afef0` advances the live pointer
array/ID at `0x4aff62..0x4aff6a`, calling `0x563fd0(id,0)` for each active
monument at `0x4aff5a` (RVA `0xaff5a`, physical `0xaf35a`). A saved-session
caller is `0x534f15` (RVA `0x134f15`, physical `0x134315`). That path and
temporary composition creation must not be conflated. Full equivalence of
every standalone load entry is not established by these bounded windows.

Fresh reads of the four original maps confirm ascending contiguous original
IDs and Model subindices in the serialized monument collection:

| Raw map | Model | Ordered subindices | Final Road original IDs / subindices | Saved Road phase |
| --- | --- | --- | --- | ---: |
| Badaling | 05 | 0..52 | 50..53 / 49..52 | 2 |
| Handan | 04 | 0..52 | 50..53 / 49..52 | 2 |
| MPWall1 | 04 | 0..52 | 50..53 / 49..52 | 1 |
| MPWall2 | 07 | 0..50 | 48..51 / 47..50 | 1 |

The four Roads are last in each corresponding Model. For this corpus, the
preview's preserved record iteration therefore agrees with the relevant
ascending Model/producer order: wall/tower registrations precede the Roads,
with gates making no intervening registration. This checks the concrete
inheritance sequence, not a general original restore algorithm.

The phase belongs to that Road entity's `cMonInfo`, not its Model row or
inherited wall archive. State schema 10 reads runtime `+08` at
`0x562089` (RVA `0x162089`, physical `0x161489`); schema 9 does so at
`0x562229` (RVA `0x162229`, physical `0x161629`). Both serialize the signed
32-bit phase at extended-record offset `+6`. The producer's actual test is
signed **less than 2 / at least 2**. OpenEmperor's admitted Road phases
remain 1/2; this observation does not extend that domain.

The direction tables were read as data, including the shared increment
target `0x57d9e2`. Within the supported even effective views:

| Saved Road phase | Resource group / slot | Effective view 0 or 4 | Effective view 2 or 6 |
| --- | --- | ---: | ---: |
| 1 | `0x61e` / 3 | variant 0 | variant 1 |
| 2 | `0x1001` / 8 | variant 41 | variant 40 |

This matches the existing selector. The Road producer neither reads
material `+5c` nor calls archive registration `0x5ccf70`. Its mode-1 branch
`0x57d923..0x57d986` writes original cell heights/flags, not phase. The
leading virtual `+64 = 0x570da0` reads extended byte `+25`; its placeholder
branch also supplies no phase normalization. That byte is serialized:
schema 10 calls `0x503d50` at `0x5620df` (RVA `0x1620df`, physical
`0x1614df`), and schema 9 at `0x56227f` (RVA `0x16227f`, physical
`0x16167f`). The helper's `0x4c95f0 -> 0x4c9600` reader consumes exactly
one byte and converts nonzero to true. Its serialized source is extended
record offset `+35`, independent of runtime offset `+25`. Fresh original
reads show raw zero/decoded false for Badaling and Handan Road IDs 50..53,
as well as MPWall1 IDs 50..53 and MPWall2 IDs 48..51. Their loaded raw
states therefore do not select the earlier group-`0x612` placeholder
branch. Later lifecycle writers and other source states remain outside this
countercheck. Exact logical/record offsets are retained in the ignored
source-order report. Restore material overwrite
`0x562e2b..0x562e44` and refresh loop `0x5636b0..0x563719` leave `+08`
unchanged in their inspected windows. Higher-level original state invariants
remain open; these findings do not prove that material 1 plus phase 2 is
globally forbidden in the original game.

The singleton writer `0x5724e0` (RVA `0x1724e0`, physical `0x1718e0`)
stores the selected graphic unchanged, owner ID, size low bits zero and
marker `0x40` in one exact entity cell. Producer flag `0x08` skips the
rectangle writer at `0x56413c` (RVA `0x16413c`, physical `0x16353c`). No
archive substitution or variant correction occurs in this write path.

**RAW ARCHIVE FACT:** a fresh metadata read confirms the single admitted
runtime group in each relevant archive. The system prefix is 200 records;
the separate dummy record makes the first physical member 201.

| Registered archive | Reported images in use | Runtime group `[begin,end)` | Valid transition variants |
| --- | ---: | --- | --- |
| `China_Mon_GreatWall_Ruined.sg3` | 240 | `[0,40)` | neither 40 nor 41 |
| `China_Mon_Earthen_GreatWall_10.sg3` | 242 | `[0,42)` | 40 and 41 |
| `China_Mon_GreatWall_10.sg3` | 242 | `[0,42)` | 40 and 41 |

Ruined variant 39 is physical 240 and valid. Requests 40/41 would map to
241/242 outside its validated in-use group; Ruined has no next runtime group
to borrow. Stone/Earthen 40/41 are physical 241/242, and variant 42 is outside.
The runtime skip/dummy translation occurs once. Reserved capacity does not
extend group bounds. `resolve_landscape_variant` correctly retains those
bounds; clamping, modulo or foreign archive records would conceal the issue.

**OPENEMPEROR PREVIEW:** forcing material 1 while preserving saved Road
phase 2 makes the evidenced request incompatible with the inherited Ruined
group. This explains hypothesis E without establishing an original gameplay
invariant. No selector-axis error, incorrect preview registration snapshot or
group-bound defect was found. A phase-2-to-1 rewrite has no support in the
bounded trace. See [the compatibility decision and diagnostic contract](great-wall-restore-context.md#ruined-phase-2-road-compatibility-2026-10-05).

Fresh byte checks, archive metadata and excerpts remain ignored under
`.local/ruined-transitions/trace/`. The user's screenshot has no proven map,
material, storage cell or variant assignment and is not this trace's oracle.
