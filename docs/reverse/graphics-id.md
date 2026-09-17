# Graphics-ID investigation (2026-09-17)

This is a narrow, read-only static study of the locally supplied `Emperor.exe` (SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`, PE32 Intel 80386). The starting repository commit was `10906083e1697b12782d2dc302c040944513e4d8`. All addresses below are **RVAs** unless marked VA or file offset. The PE image base is `0x400000`; static VAs are base plus RVA. The EXE was never run, patched, or committed. Apple LLVM `objdump` 21.0.0 and the independent read-only `tools/re/pe_string_refs.py` were used. Ghidra/analyzeHeadless was not installed; no decompiler output or Ghidra project was produced.

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

## Corpus check (independent of binary dataflow)

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
```

There is no graphics-candidate texture-preview command yet. The existing hand-curated `--view textured --terrain-bindings ...` preview is independent of this hypothesis.
