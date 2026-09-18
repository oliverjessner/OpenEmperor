# Curated road preview

`--road-visuals .local/visuals/roads.json` or **Road JSON...** in the New/Load Sandbox menu selects an optional, session-only road image set. F6 switches it on and off. OFF, an absent mask, or no profile uses the older yellow diamond. F2 and F4 remain independent. The profile is never saved, and all World roads, routes, revision values, commands and save fields are unchanged.

The mask is an **OpenEmperor preview convention** in storage coordinates:

| Bit | Value | Neighbor offset |
|---|---:|---|
| 0 | `0x1` | `neg_y = (0,-1)` |
| 1 | `0x2` | `pos_x = (+1,0)` |
| 2 | `0x4` | `pos_y = (0,+1)` |
| 3 | `0x8` | `neg_x = (-1,0)` |

Each of the 16 values `0x0`–`0xf` is computed read-only from a road cell's four neighbors on every draw. Road-to-road adjacency always counts. A directly adjacent placed sandbox building also counts as a **visual entrance**; this is separate from the World road graph and does not permit transit through buildings. Original map background graphics never contribute. A drag preview computes the mask from existing roads plus its entire planned set, without making a World copy or issuing commands. An invalid plan is shown in red. Accepted PlaceRoad/RemoveRoad commands and save/load therefore update the visible tile on the next frame automatically, without a visual cache or a new simulation rule version.

The local manifest format is:

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

`image_index` is a physical SG3 record, never a runtime-translated ID. Keys use exactly one lower-case hexadecimal digit. Missing masks are valid and use the yellow fallback. Duplicate or unknown keys, absolute/traversing archive paths, resolved `.555` paths escaping the game-data root, nonstatic, alpha-bearing, unsupported or mirrored records, failed decodes, nonfinite/out-of-range anchors, and oversized manifests or RGBA data fail before a live set is replaced. The JSON limit is 1 MiB and deduplicated decoded RGBA is capped at 64 MiB. Each distinct physical `AssetId` is decoded and uploaded once. The SDL texture uses straight alpha, nearest scaling and actual image dimensions. It is drawn at `screen(world_for(cell)) − zoom × ground_anchor`; image height never changes the ground-depth key. No rotation, mirror, color modulation or inferred SG3 animation offset is applied.

The first ignored local `.local/visuals/roads.json` uses `DATA/China_Terrain.sg3`, group 3 `China_Land3.bmp` (description `A new bitmap.`). Records 782, 786, 790–797, 799 and 875 are all unmirrored Type-30 78×40, size-flag-1, 3,200-byte-base tiles with no alpha. The anchor `[39,20]` is a **visual preview choice**. The current mapping is `0x0→799`, `0x5→786`, `0xa→782`, `0x3/0x6/0xc/0x9→790/791/792/793`, `0x7/0xb/0xd/0xe→794/795/796/797`, `0xf→875`. Masks `0x1`, `0x2`, `0x4`, `0x8` have no convincing end-cap selection and retain diamonds. The selected pieces are visibly different; visual continuity and orientation are preliminary. These are not recovered Emperor road IDs, connection bits, variant-selection rules or animation rules. See [the bounded observation log](reverse/research-log.md).

Saved-map items, roads, buildings and walkers share the projected-ground painter, with StoredMap before Road before Building before Walker at the same key. A stored object whose ground sorts farther forward may occlude a road or walker; this whole-image preview does not recover the original game's split-object painter. F7 shows the earlier map-first order for comparison. The F6 debug line reports ON/OFF, configured masks, unique assets and fallbacks in the current frame. The Industry-v5 `--sandbox-check --sandbox-resume-check --report-json` path adds `road_visuals` counts and World equality to a separately ticked control; it always leaves `manual_visual_review` false.
