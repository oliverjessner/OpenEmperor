# Stored graphics map preview

Use your own legally obtained game files. The ordinary saved-ID snapshot retains one-cell rendering and shows multi-cell records diagnostically:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Chengdu.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table
```

Add `--multi-tile-preview` to opt into the deliberately narrow 2×2 reconstruction:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Chengdu.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table --multi-tile-preview
```

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
