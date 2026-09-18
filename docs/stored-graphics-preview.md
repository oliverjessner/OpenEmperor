# Stored graphics map preview

Use your own legally obtained game files. The ordinary saved-ID snapshot retains one-cell rendering and shows multi-cell records diagnostically:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Chengdu.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table
```

Add `--multi-tile-preview` to opt into the deliberately narrow 2×2 reconstruction:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Chengdu.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview
```

The existing option continues to mean the isolated-component preview (`--footprint-policy isolated` is an explicit spelling). A separate, reference-derived metadata experiment is available with `--multi-tile-preview --footprint-policy edge-byte`:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Banpo.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview --footprint-policy edge-byte
```

For this second policy only, `MapSubtileMetadata` retains the byte and tentatively reads `part_x=byte&7`, `part_y=(byte>>3)&7`, draw-marker candidate `byte&0x40`, and still-unknown `byte&0x80`. These masks come from the pinned public [Julius edge-grid reference](https://github.com/bvschaik/julius/blob/34d1ecd54befb845c0139b371fa8a0438210dac1/src/map/property.c); they do not by themselves prove Emperor semantics. The separate `bitfields_grid` in that reference is not used. The local Emperor static check supports use of `0x40` in a draw-related path, while the low-bit position meaning remains reference- and data-derived. The marker is diagnostic and never hides a cell. A missing, duplicate, or misplaced marker increments `marker_deviations`; unknown high bits are counted and preserved.

Each eligible 158-wide image cell proposes an origin `(storage_x-part_x, storage_y-part_y)` using signed arithmetic. Only four distinct in-mask cells with exact parts `(0,0),(1,0),(0,1),(1,1)`, identical saved ID and physical AssetId, valid source, and the existing 2×2 image restrictions may form an instance. Groups are validated before ownership. Invalid positions, incomplete/mask-crossing groups, and conflicting claims stay diagnostic; there is no fallback to the isolated policy. Touching groups may share the same image texture. The marker candidate at part `(0,1)` is reported separately from the projected footprint origin `(0,0)`; the same `project(origin) − (width/2,height−80)` image anchor is used for both policies. Every original storage cell remains selectable.

The option requires this view and profile. The saved graphic ID, `candidate_byte`, terrain/object words, logical offsets, off-map bit, and candidate mask are left unchanged. `RuntimeArchiveLayout` resolves the studied version-213 Terrain/Elevation registration to physical SG3 AssetIds; there is no binding file, implicit fallback, or per-frame file read. The snapshot does not establish the original game's first draw.

A single-cell image remains supported when it is Type 30, 78 pixels wide, at least 40 high, has a 3,200-byte base and valid source, and needs no unverified mirroring. It is placed at `project(cell) − (width/2, height−40)`. The opt-in 2×2 rule additionally requires Type 30, width 158, height at least 80, 12,800-byte base, isometric size flag 2, valid source, and no unverified mirror. It finds four **candidate-mask** cells with the same exact saved ID and physical AssetId that form an isolated, complete 2×2 four-neighbor component. The component must have no fifth touching cell with that ID. This is an independently chosen **preview grouping rule**, not a recovered engine anchor or an interpretation of `candidate_byte`. Three-cell groups stay `incomplete_footprint`; touching groups of five or more stay `ambiguous_footprint`. Inconsistent larger layouts stay `unsupported_footprint_size`. A four-cell component touching a matching saved ID just outside the candidate mask stays `anchor_unresolved`; the mask is not used to manufacture isolation. No observed cell is assigned an original anchor flag. No candidate-mask boundary is extended or trimmed to complete a group.

For a qualifying 2×2 square, its minimum storage `(x,y)` is the rear/top point of **our projection**, not an identified stored anchor. The decoded-image anchor is `(width/2, height−80)` and the image origin is `project(rear/top) − anchor`. Thus a rear/top point at `(100,100)` puts 158×80 at `(21,100)` and 158×125 at `(21,55)`. The whole base plus any Omega color overlay is drawn once. Four original storage cells own one stable placed instance, remain individually selectable with their own raw values and offsets, and produce no additional texture draw or diagnostic diamond. Two spatially separate instances may share one decoded/uploaded texture. Clicking selects the ground storage cell; the console also reports its instance ID, footprint, placement rule, and image origin. Pixel-accurate selection of an overhanging image is not implemented.

For these simple noninterlocking footprints, instances and unresolved diagnostic cells are stably sorted by the frontmost ground point, then its projected x position and source order. This is a painter convention, not a complete original-game compositing rule. Culling and fit use the full decoded image rectangle, including tall overlays. The app reports candidate cells, successfully covered cells, exact diagnostic statuses, 1×1 and 2×2 instance counts, decoded assets, uploaded/distinct textures, and first viewport texture/diagnostic draw counts. Metadata recognition is not reported as a successful decode. Per-image errors remain diagnostic. The 2,048 distinct referenced assets, 16 MiB payload/image, and 64 MiB summed logical RGBA budgets and SG3/`.555` path containment checks remain active.

The local read-only check on the user's GOG files gave these post-decode results with the opt-in rule:

| Map | Candidate cells | Covered cells | 1×1 instances | 2×2 instances | Distinct decoded/uploaded textures | Remaining diagnostics |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Chengdu | 14,620 | 14,620 | 14,616 | 1 | 341 | none |
| Xia | 3,612 | 3,612 | 3,588 | 6 | 294 | none |
| Banpo | 6,384 | 6,376 | 6,340 | 9 | 282 | `ambiguous_footprint`: 8 |
| Anyi | 14,620 | 14,620 | 14,560 | 15 | 396 | none |

Chengdu's four cells are `(55,133)`, `(56,133)`, `(55,134)`, `(56,134)` with saved ID `0xc10a`, physical Terrain record 467, 158×95, 12,800-byte base and size flag 2. Banpo's unresolved eight cells form a touching 2×4 component at x=103–104, y=143–146, all ID `0x400a1`; the preview does not partition it. See the [evidence record](reverse/graphics-id.md) for individual Chengdu values and limits. Whole-map and targeted offscreen SDL software captures were viewed locally under ignored `.local/`; these are OpenEmperor renderings, with no interactive desktop run or original-game image comparison. No capture or source game data is distributed.

WASD/arrows pan, wheel zooms, `R` refits, `V` cycles views, and clicking or Return selects a ground cell. Unsupported layouts, ambiguous cells, and decode failures remain purple. The first-draw question, heights, animation, mirroring, and arbitrary/interlocking footprints remain unresolved.

The new policy was checked read-only against `Cities/Chengdu.map`, `Cities/Banpo.map`, `Cities/Xia.map`, and `Cities/Anyi.map`. Its metadata planning counts are:

| Map | Edge-byte 2×2 | Covered cells | Remaining footprint diagnostics | Isolated 2×2 / covered |
| --- | ---: | ---: | --- | --- |
| Chengdu | 1 | 14,620 | none | 1 / 14,620 |
| Banpo | 11 | 6,384 | none | 9 / 6,376, plus 8 ambiguous |
| Xia | 6 | 3,612 | none | 6 / 3,612 |
| Anyi | 15 | 14,620 | none | 15 / 14,620 |

Banpo's x=103–104, y=143–144 and y=145–146 have identical saved ID `0x400a1` and physical Elevation record 362 but repeat `0,1,72,9`; their calculated origins are `(103,143)` and `(103,145)`. All qualifying groups in these four maps have one marker candidate at part `(0,1)` and no unknown high bits. This is a saved-snapshot consistency check, not an original-game first-draw comparison; see [reverse-engineering evidence](reverse/graphics-id.md).

After the shared-asset eligibility was included in the new plan, a headless SDL software run decoded and uploaded all 282 Banpo distinct assets, rendered all 6,384 candidate cells, drew 6,351 texture instances in the full viewport, and drew zero diagnostics. The targeted 3× view drew 93 texture instances and zero diagnostics. The locally ignored `edge-banpo/target.png` and `edge-banpo/full.png` were opened and viewed: the targeted image shows two adjacent rocky/cliff footprints with surrounding grass, and the full view shows a coherent ring of terrain without the prior purple eight-cell gap. These are observations of OpenEmperor output only. They do not substitute for an interactive macOS desktop run or original-game image comparison; captures remain ignored under `.local/` and are not distributed.

## Map browser and compatibility check

`--browse-maps` shows discovered standalone maps, declared sizes, the current saved-graphics profile/policy, and each row's session result. Arrow keys and Page Up/Down move selection; Enter or double-click opens; Escape in a map returns to the remembered row, and Escape in the list quits. A failed map load returns to the list. The small developer UI uses [SDL's ASCII-only debug text](https://wiki.libsdl.org/SDL3/SDL_RenderDebugText), so shown labels are sanitized, never the actual path used to load the map.

`--render-check --report-json` on a direct standalone map uses the normal saved-graphics plan, decoder, texture uploads, and SDL map view. It draws two 1280×720 dummy/software frames (overview and zoom), outputs only JSON on stdout, and exits. The report separates per-stage success, candidate coverage, diagnostic status counts, placed 1×1/2×2 instances, referenced/required/decoded/uploaded assets, logical RGBA bytes, marker/unknown-bit/mask warnings, and draw counts. `snapshot_complete` requires successful frames and full candidate-mask coverage; `snapshot_partial` records any remaining diagnostic cells. These are OpenEmperor saved-snapshot outcomes, not original-game comparisons. The local runner in `tools/verify_map_corpus.py` discovers maps with `--list-maps --report-json`, checks each in an isolated timed process, and atomically preserves every result under `.local/reports/` without pixel data. Source-format errors and unsupported graphics remain visible rather than acquiring new interpretation rules.

A read-only run over the locally supplied GOG standalone maps discovered and checked **167** files. The fixed `edge-byte` snapshot check reported **57 `snapshot_complete`**, **110 `snapshot_partial`**, and no load, render, process, or timeout failures. Across 2,107,780 candidate cells, 2,081,780 were covered and 26,000 remained diagnostic. The diagnostic cell classes were `unregistered_slot` (19,660 cells in 59 maps), `unsupported_footprint_size` (6,132 in 78), `subtile_position_invalid` (203 in 6), and `conflicting_footprint` (5 in 1). Separate warnings were 56 candidate/off-map mask mismatches in 17 maps and 99 marker deviations in 9; unknown metadata high bits were not observed in this run. The four earlier reference maps Xia, Banpo, Chengdu, and Anyi were each `snapshot_complete`. Counts describe only this local corpus and selected experimental policy, not a general Emperor compatibility guarantee. The reproducible per-map technical report is kept in ignored `.local/reports/`.

Local offscreen images were actually viewed for Banpo's known adjacent-footprint excerpt, Anyang's complete overview, and `ASSR-Baoji.map`'s partial overview; the latter showed nine purple unsupported-footprint cells near the upper-right of our rendered map. These are OpenEmperor software-renderer outputs. No interactive macOS desktop session or original-game screenshot comparison was performed, and no captures are distributed.
