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

## Built-in 16-mask matrix

The exact-fingerprint GOG-derived compatibility pack uses physical records in `DATA/China_Terrain.sg3`, group 3 `China_Land3.bmp`. The bounded audit covered records 760–900 and identified records 782–799 as one consistent dirt-and-stone family: four alternatives for each straight axis, four corners, four T pieces, a crossing and an isolated tile. The chosen records are Type 30, 78×40, size flag 1, internal, unmirrored, alpha-free and static. Their base is 3,200 bytes. Records 786, 791, 792, 793 and 794 respectively carry 64, 59, 79, 45 and 98 additional color-overlay bytes; the remaining selected records have none.

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
