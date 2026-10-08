# Curated road preview

`--road-visuals .local/visuals/roads.json` or **Road JSON...** in the New/Load Sandbox menu selects an optional, session-only road image set. F6 switches it on and off independently of F2 and F4. The profile is never saved, and World roads, routes, revision values, commands, costs and save fields are unchanged.

## Visual masks

`RoadNeighborMask` is an OpenEmperor preview convention in storage coordinates. It contains only actual sandbox roads; the preview version adds the complete planned drag path without modifying the World.

| Bit | Value | Road-neighbor offset |
|---|---:|---|
| 0 | `0x1` | `neg_y = (0,-1)` |
| 1 | `0x2` | `pos_x = (+1,0)` |
| 2 | `0x4` | `pos_y = (0,+1)` |
| 3 | `0x8` | `neg_x = (-1,0)` |

`EntranceMask` uses the same bit layout but reports directly adjacent placed sandbox buildings. It is diagnostic only. A building beside a left-to-right road therefore leaves `RoadNeighborMask == 0xa` and sets the corresponding bit only in `EntranceMask`. Neither mask changes `World::find_route()`, and neither is stored. F1 shows both masks for a selected road cell.

Every frame derives the selected road tile from `RoadNeighborMask`. A valid drag preview derives hypothetical masks from existing plus planned roads. An invalid plan remains red. A successful placement, removal or load changes visible masks on the next frame without a cache or simulation-rule change.

## Profile format and validation

```json
{
  "schema_version": 1,
  "mode": "curated_road_preview",
  "tiles": {
    "0x5": {
      "archive": "DATA/example.sg3",
      "image_index": 123,
      "ground_anchor": [39, 20],
      "evidence": "Locally reviewed unmirrored Type-30 tile"
    }
  }
}
```

`image_index` is a physical SG3 record, never a runtime-translated ID. Keys use exactly one lower-case hexadecimal digit. Custom profiles may contain a subset; missing masks use the yellow diagnostic diamond or the muted presentation fallback. Duplicate or unknown keys, absolute/traversing archive paths, resolved `.555` paths escaping the game-data root, nonstatic, alpha-bearing, unsupported or mirrored records, failed decodes, nonfinite/out-of-range anchors, and oversized manifests or RGBA data fail before a live set is replaced.

The JSON limit is 1 MiB and deduplicated decoded RGBA is capped at 64 MiB. Each distinct physical `AssetId` is decoded and uploaded once, even when several masks share it. SDL uses straight alpha, nearest scaling and actual image dimensions. Rendering places the image at `screen(world_for(cell)) - zoom * ground_anchor`; image height never changes the ground-depth key. No rotation, mirror, color modulation or inferred SG3 animation offset is applied.

## Historical built-in 16-mask matrix

> **Historical, retired mapping:** The following 782–799 assignment and prior visual acceptance are retained only as research history. They are superseded by the original registration/topology audit below; do not use the table as the current pack.

The historical exact-fingerprint GOG-derived compatibility pack used physical records in `DATA/China_Terrain.sg3`, group 3 `China_Land3.bmp`. The bounded audit covered records 760–900 and identified records 782–799 as one consistent dirt-and-stone family: four alternatives for each straight axis, four corners, four T pieces, a crossing and an isolated tile. The chosen records are Type 30, 78×40, size flag 1, internal, unmirrored, alpha-free and static. Their base is 3,200 bytes. Records 786, 791, 792, 793 and 794 respectively carry 64, 59, 79, 45 and 98 additional color-overlay bytes; the remaining selected records have none.

| Mask | Meaning | Physical record | Evidence level |
|---:|---|---:|---|
| `0x0` | isolated | 799 | visually reviewed family member |
| `0x1` | end `neg_y` | 786 | matching y straight reused; no family end cap found |
| `0x2` | end `pos_x` | 782 | matching x straight reused; no family end cap found |
| `0x3` | corner `neg_y+pos_x` | 790 | visually reviewed orientation |
| `0x4` | end `pos_y` | 786 | matching y straight reused; no family end cap found |
| `0x5` | straight y axis | 786 | visually reviewed orientation |
| `0x6` | corner `pos_x+pos_y` | 791 | visually reviewed orientation |
| `0x7` | T without `neg_x` | 794 | visually reviewed orientation |
| `0x8` | end `neg_x` | 782 | matching x straight reused; no family end cap found |
| `0x9` | corner `neg_y+neg_x` | 793 | visually reviewed orientation |
| `0xa` | straight x axis | 782 | visually reviewed orientation |
| `0xb` | T without `pos_y` | 795 | visually reviewed orientation |
| `0xc` | corner `pos_y+neg_x` | 792 | visually reviewed orientation |
| `0xd` | T without `pos_x` | 796 | visually reviewed orientation |
| `0xe` | T without `neg_y` | 797 | visually reviewed orientation |
| `0xf` | crossing | 798 | visually reviewed family member |

The one-neighbor assignments are a presentation decision: the bounded family contains no material-matched end-cap records, and the straight lets the track reach the connected cell edge. This achieves 16/16 coverage without rotation, mirroring or synthetic pixels. It does not establish Emperor's original handling of road ends. The previous record 875 crossing candidate was rejected during the new contact review because it is a plain rough ground tile outside the coherent family.

The common anchor remains `[39,20]`, the center-bottom reference of the 78×40 footprint. OpenEmperor's logical projection steps by 80×40. At nearest-neighbor 4× review the family retained a common ground position; the two-pixel width difference is the source footprint geometry and is not stretched or patched. Transparent pixels outside the Type-30 diamond continue to reveal the stored terrain underlay.

### City-v10 road-only baseline

The footprint milestone classified the then-current exact-pack road-only matrix separately from buildings: `road_only_coherent = true`. The baseline covers the isolated tile, all four one-neighbor directions, both straights, four corners, four T junctions, the crossing, a long line, L and T compositions, and a small 5×5 network at 1× and 4×. The roads-only mask tests still cover all 16 combinations and composite topology after the footprint change. No road mapping changed, and records 783–785 and 787–789 were not substituted because the existing 782–799 assignments did not show a concrete orientation error. The one-neighbor straight reuse was that historical presentation choice; the corrected pack below uses genuine end records.

The Xia saved-graphics snapshot was also checked as read-only evidence. Its 58 cells classified by the reference-derived terrain layer as roads resolve mainly to physical records 552/553 and singly to 557–563/619 under the studied runtime-table hypothesis. Those decoded records are unrelated terrain/building images, so they do not corroborate the 782–799 selection and reinforce the existing warning that saved IDs may be replaced before first draw. None of the table above is marked map-correlated.

## Bounded atlas

The developer asset browser can open an exact archive and bounded physical-record range:

```sh
./build/openemperor \
  --data /path/to/your/game-data \
  --road-atlas DATA/China_Terrain.sg3 \
  --start 760 \
  --count 160
```

The count is limited to 512. Each grid entry shows physical record, group, dimensions, type, overlay bytes, mirror offset, alpha length and animation count together with the decoded image. Enter opens the existing detail view. The browser reads the user's original files through the shared bounded loader and creates no PNG. Local audit exports, screenshots and decoded pixels stay under ignored `.local/` and are never compatibility resources.

Saved-map items, roads, buildings and walkers share the projected-ground painter, with StoredMap before Road before Building before Walker at the same key. A stored object whose ground sorts farther forward may occlude a road or walker; this whole-image preview does not recover the original game's split-object painter. F7 shows the earlier map-first order for comparison. F6 debug statistics report ON/OFF, configured masks, unique assets and current-frame fallbacks. Machine reports retain `manual_visual_review=false` unless an actual desktop review was performed.

## Corrected original basic-road registration (2026-10-03)

The former mask assignment is retired. New EXE evidence independently confirms the **same physical family**: original Roads model 22 → group `0x61e` → Terrain base 782; the actual road refresh branch selects its 17-row cardinal topology table. The corrected fixed-orientation profile uses 16 distinct records, including genuine ends. It is metadata-only; no mirroring, rotation, stretched pixels, new road authority or entrance-neighbor semantics are introduced. See the [complete provenance, wider 875-record audit and mapping](reverse/original-presentation-correction.md#roads-old-mapping-retired-family-independently-confirmed).

| Mask | Physical | Mask | Physical |
| --- | ---: | --- | ---: |
| 0x0 | 794 | 0x8 | 793 |
| 0x1 | 790 | 0x9 | 789 |
| 0x2 | 791 | 0xa | 783 |
| 0x3 | 786 | 0xb | 798 |
| 0x4 | 792 | 0xc | 788 |
| 0x5 | 782 | 0xd | 797 |
| 0x6 | 787 | 0xe | 796 |
| 0x7 | 795 | 0xf | 799 |

All 16 masks plus long straight, L, T, crossroads and 5×5 were rendered locally at 1×/2×/4× with zero fallbacks. **FAILED / superseded by real user playtest:** the previous native Xia review did not establish connected roads. The real City-v16 playtest shows grass strips between adjacent roads; zero fallbacks and isolated sprite review were insufficient. A continuity correction and full-city pixel regression are required. Anchor `[39,20]` and 78×40 source geometry remain exact. Original rotated views, upgraded styles and terrain-specific transition selection are not implemented. Earlier lack of saved-map correlation remains a historical limitation; the newly traced static road branch is independent positive evidence.

## Road continuity correction (2026-10-03)

The later [Walker ground/cargo correction](rendering/road-ground-occlusion.md)
retains this replacement/geometry contract. Productive unified Sandbox Road
surfaces draw once after existing Ground and alpha-preview backdrops, before
the unchanged historical/building/walker spatial merge. F7/Full Snapshot keep
the historical shared Road ordering. The continuity oracle below remains
required, including both painter modes and alpha-128 previews.

The earlier connected-road acceptance above is **FAILED / superseded by real user playtest**. Correct topology and zero fallbacks did not prevent the complete stored grass image of a front road cell from painting over the road behind it. The full Xia compositor reproduced the user's paving-island failure on both eight-cell straights; isolated `RoadSpriteSet` pairs did not reproduce it.

The exact pack now opts into schema-1 **`"replaces_ground": true`**. A configured road replaces its stored **single-cell image including its old Omega overlay**, rather than adding another image to that cell. Multi-cell stored objects remain in the ordinary painter. F6 off, absent profiles, missing masks and profiles omitting the flag retain their existing additive/fallback behavior. The original refresh writes one selected cell graphic; no separate paving underlay was established. See the [bounded draw/projection audit](reverse/original-presentation-correction.md#road-continuity-follow-up).

Replacement entries require unmirrored static Type-30 side 1, width 78, height 40, base 3,200, anchor `[39,20]`, and an opaque complete base diamond. The loader validates these eagerly, including each reused role's anchor, while retaining physical-asset deduplication. At integer camera zoom, replacement-road texture origins are floored to integer screen pixels, consistent with the original integer draw interface. This translates the whole source raster together; its dimensions, pixels, anchor and 40/20 world steps are unchanged. Other profiles retain their former placement. No stretching, padding, copied edges, generated paving or new original record is used.

Valid new drag cells draw their existing ground once before the painter, then use the same replacement road sprite at alpha 128. The old ground is omitted from the subsequent merge, so another preview cell's grass cannot overwrite earlier paving. Existing roads in that preview remain opaque. Invalid previews retain the old diagnostics. The indexed render projection is read-only and performs no World copies/restore/execute, BFS, route refresh, file reads/writes or asset decode/upload; pointer plan coalescing is unchanged.

Actual production readbacks used the original assets, 16 neighborhoods, all 256 reciprocal mask pairs in four directions, eight-cell x/y lines, L, T, crossroads and a 5×5 grid. The full Xia comparison measures 128 logical contacts at 1×. The table measures **terrain cutting all five sampled central paving lanes** between cell interiors, in screen pixels (horizontal × vertical), not bounding-box separation. Per-lane maxima and exact affected positions/scanlines are retained locally as well.

| 1× pattern | Before: maximum full-core grass cut | After |
|---|---:|---:|
| Eight-cell straight x / 783 | 2 × 1 px | 0 px |
| Eight-cell straight y / 782 | 4 × 2 px | 0 px |
| Corner neighborhood | 4 × 2 px | 0 px |
| T neighborhood | 4 × 2 px | 0 px |
| Cross neighborhood | 4 × 2 px | 0 px |
| Long L / T / cross | 4 × 2 / 2 × 1 / 6 × 3 px | 0 px |
| 5×5 grid | 4 × 2 px | 0 px |

Before correction, individual-lane grass runs reached 4–5 vertical pixels (8–10 horizontal); after correction, a few **single-pixel neighboring grass blades** remain. They do not cut the sampled paving core or separate adjacent road surfaces. This is not a claim that every road-colored pixel is free of neighboring whole-image artwork. Isolated original pair contacts have no transparent gap in any of the 256 combinations. Four local orientation-column comparisons keep the same complete bases; columns 0/2 align the straight curb motifs with this camera, whereas 1/3 place transverse motifs along the test line. Column 0 remains selected.

The new `road-continuity-production-pixels` CTest uses independently authored Type-30 paving and a raised green Omega strip. It first reproduces the additive failure, then asserts every sampled paving-core pixel at exact player zoom presets 1×/2×/4× in both painter modes, and the alpha-128 L preview. Source transparency, bad replacement geometry/anchors, deduplication and legacy opt-out have loader regressions. No original screenshot or source pixels are committed.

The paid Xia rule-2 scene has six Houses, stages 0/1/2, Well 1559, Herbalist 1580, Market, Farm, ServicePost, FireWatch and industry. Scripted Metal captures and the **final-bundle native keyboard review** verify the continuous main roads at true camera 1×, F1 off, with 2×/4× secondary views and F6 off/on. The toolbar's simulation speed label is not camera zoom; actual zoom-preset input was used. The session remained paused at tick 4200, Funds 656, population 53. This closes the scoped road-appearance acceptance, not the older broad mouse-driven playtests. Original-data reports, alternate-column JSONs, runners and images remain ignored in `.local/road-continuity/`.

Validation: complete Debug **83/83** (889.77 s), final Release **83/83** (40.53 s), final ASan/UBSan **83/83** (179.61 s), and the independent bundle Release **83/83** (50.06 s). These include City-v16 rule 1 and rule 2 20k/100k scenarios, Water, Health, maintenance, demolition, old profiles/schemas, recovery and road responsiveness. The final legacy raster opt-out received a further focused Debug road/view run: **5/5** (130.30 s). Packaging verified staged dependencies, notices, recursive Mach-O checks, ad-hoc signing, metadata-only resources and relocated execution. The native review used that bundle, then exited without changing the scene. The local dirty-workspace candidate ZIP is `dist/OpenEmperor-0.1.0-alpha.2-6346c30e32aa-macos-arm64.zip`, SHA-256 `08cc981c26366bc07287cf57e7bd91ef67ccc9a72bc1552fd35e01b74cef3b07`; it is not a published release. Logs and source-fingerprint checks remain ignored with the contact reports.
