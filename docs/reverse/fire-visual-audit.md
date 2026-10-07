# Bounded original flame audit

The animated-fire presentation pass starts from `c107a1bf3464075d1cc05596d23aabf267912b80`, with a clean worktree. Research is read-only and limited to one complete, suitable flame clip. It does not reconstruct the original building/fire lifecycle. Original archives, decoded frames, the contact sheet, codec preview and binary observations remain ignored under `.local/fire-visuals/`.

## A — Original asset identity

The locally supplied **`DATA/destruction.sg3`** contains named `fire1` through `fire5` bitmap-group entries. The selected `fire1` sequence is **physical records 201–250 inclusive**, explicitly listed in the metadata resource. Physical numbering includes the dummy record zero; these numbers are not packed graphic IDs.

| Dependency | Bytes | SHA-256 |
| --- | ---: | --- |
| `DATA/destruction.sg3` | 991,080 | `f5539108b6d585b31fc6f807e4da10f4ac796ee805f0bf065379923f821d035b` |
| `DATA/destruction.555` | 12,935,830 | `0668087731938c626fe01211cc02e102c93022e728e9ea649eb9ed9322cf1a94` |

The archive is SG3 version 214, with 13,000 physical table slots, 1,853 reported images in use and 29 named bitmap groups. The chosen 50 frames are internal, unmirrored **Type 256 Omega sprites**, each with an **80×80** canvas. Every required frame successfully decodes through the existing bounds-checked SG3 loader, without an alpha-address override or format relaxation. All 50 decoded RGBA buffers are different.

Each frame has the supported `internal_v214_type256_contiguous` separate alpha stream. The decoded sequence contains 32 alpha levels from 0 to 255, including genuinely transparent and partially transparent pixels. Transparent pixel counts range from 4,544 to 4,731 of each 6,400-pixel canvas; partially transparent counts range from 1,432 to 1,726. The union of nontransparent bounds is `[14,64) × [5,80)` in canvas coordinates. There are **zero exact decoded opaque red `FF0000` marker pixels** across the sequence. The raw shadow flag is one, but this pass neither imports the walker-only no-alpha shadow transformation nor assigns a new meaning to that flag alongside an alpha stream. It preserves the existing decoder's RGBA output and uses ordinary source-over blending. No black/magenta/red key or additive blend is introduced.

## B — A coherent flame sequence

Physical adjacency alone is not the evidence. The archive's index table position 4, at file offset **88**, contains **201**; its next entry is **251**, delimiting exactly 50 records. Named bitmap entry 4 is `fire1`; its historical `image_count`/first/last fields are zero, so those fields do not independently establish this range. Record 201 declares `animation_sprites=50`, with the signed stride word at record offset `+32` equal to one. Later records have zero animation counts. All frame geometry and source ranges are valid.

The pinned `Emperor.exe` SHA-256 is `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`. Bounded static inspection establishes the following chain:

| Static VA | Observed fact |
| --- | --- |
| `0x475cbb` | The initial resource-name array places `Destruction` at position **17**. The array base is captured before one push; stack operand `+0x48` therefore corresponds to index 17. Position 16 is `China_Elevation`, a distinct registration. |
| `0x5cd39e`–`0x5cd448` | Version 214 skips the version-213 conversion loop. The loader copies the in-use records in physical order starting after dummy zero. The first `dust_clouds_02` name does not take the `Zeus_system.bmp` 200-record skip branch. |
| `0x5cd468`–`0x5cd524` | Positive signed index entries become resource-group starts, with the dummy-record subtraction. |
| `0x408170` | Group-key division by 512 selects the slot; the nonzero remainder minus one selects the group position. Thus **`0x2205`** addresses slot 17 / position 4, local base **200**, packed base **`0x440c8`**, and physical record **201** under this registration. |
| `0x5cfdf0`–`0x5cfe58` | The sequence helper obtains count and stride from the selected base metadata, bounds its frame input, and returns group base plus **frame × stride + bounded secondary index**. For the selected metadata and secondary index zero, this is the linear 201→250 physical sequence. |
| `0x42ac54`–`0x42acfd` | A bounded original object effect draw selects the five keys `0x2205`–`0x2209`, calls `0x5cfdf0` or `0x5cfe60`, reads the selected frame offsets, and passes the result to draw `0x413960`. This connects the named fire families to an actual original drawing path. |

The complete 50-frame decoded contact sheet was inspected. A local 50-frame nearest-neighbor loop preview was also prepared for visual review. The images show a coherent orange/yellow flame with soft dark smoke and changing tongues, not a firework, an oven crop or an abstract warning icon. Together, the exact index range, count/stride, static sequential selector, drawing path and full decoded comparison support using this original `fire1` clip as a curated building-fire effect.

The current terrain/resource-group producer deliberately remains bounded to its existing version-213 registrations. This audit does **not** extend those production readers to version 214 or turn the fire clip into a map entity. The small fire profile uses explicit physical AssetIds and the shared SG3 decoder.

## C — Original timing and attachment remain separate

The original frame metadata contains offsets `[42,90]` and a base speed ID of three. The observed original draw reads frame offsets, but the complete original timing, owner-relative placement, contextual variant selection and lifecycle were not reproduced. No original-game comparison capture is available.

OpenEmperor therefore declares its own common **`[40,80]`** canvas attachment, **two simulation ticks per frame**, a global tick-loop origin and a bounded **`BuildingId % 7` frame offset**. The sandbox's building attachment and roof-height convention are authored presentation. They are not claims of original timing or pivot parity. One complete flame sequence is integrated; smoke, sparks, additional variants and destruction are not separate systems in this pass. The dark pixels already belonging to each original frame remain part of that frame.

See [the fire presentation contract](../fire-visuals.md) for activation, resource limits, rendering and acceptance. A/B original identity and sequence evidence do not by themselves establish normal-application activation; that is verified separately.
