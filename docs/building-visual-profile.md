# Curated core building previews

`--building-visuals <profile.json>` supplies one optional, session-local display profile for the Industry-v5 `ClaySource`, `Pottery`, `Warehouse`, and `Household` objects. The menu uses the same **Building JSON...** picker. Missing roles retain diagnostic markers. An existing Pottery-only schema-1 profile remains valid. F4 switches all configured building visuals on or off; F2 still controls Clay walkers independently. Neither switch changes the World or a save.

The manifest belongs under ignored `.local/visuals/` and contains only references and preview choices, never pixels:

```json
{
  "schema_version": 1,
  "mode": "curated_building_preview",
  "buildings": {
    "clay_source": {"archive":"DATA/example.sg3","image_index":10,"ground_anchor":[79,76],"evidence":"Local visual choice; identity unverified"},
    "pottery": {"archive":"DATA/example.sg3","image_index":11,"ground_anchor":[79,120],"evidence":"Local visual choice; identity unverified"},
    "warehouse": {"archive":"DATA/example.sg3","image_index":12,"ground_anchor":[79,116],"evidence":"Local visual choice; identity unverified"},
    "household": {"archive":"DATA/example.sg3","image_index":13,"ground_anchor":[79,79],"evidence":"Local visual choice; identity unverified"}
  }
}
```

Each `image_index` is a physical SG3 record, with no runtime-table translation. The parser rejects duplicate keys, unknown roles, unsafe SG3 or resolved `.555` paths, unsupported or mirrored layouts, bad decodes, and anchors outside finite ±4096. The manifest limit is 1 MiB and deduplicated RGBA data is capped at 64 MiB. Each distinct selected asset decodes and uploads once per active profile; multiple buildings of one role share its texture. Replacing a live profile publishes the new texture set only after all assets have decoded and uploaded. A failed replacement keeps the current set.

`BuildingState.kind` determines the visual role; `BuildingId` is only a stable instance ID. All four roles use the same projected ground point `world_for(BuildingState.cell)` and the same formula: `image_origin = screen(ground) − zoom × ground_anchor`. A valid hover preview draws that image at the same anchor with partial alpha and a cell outline; an invalid hover retains the red diagnostic marker. Hover creates no building and does not change World state. Picking and selection stay on the one logical sandbox cell. Images can visibly cover neighboring cells without blocking them. The same ground-depth sort mixes roads, buildings and walkers, with projected x, kind and stable ID tie-breaks. The original `StoredGraphicsRenderer` remains a separate background pass, so tall saved map objects cannot yet occlude these sandbox buildings correctly.

The locally selected first visual set (all `DATA/China_General.sg3`) is a **preview convention**:

| Sandbox role | Physical record | Type / image | Group | Ground anchor | Visible motif / evidence |
|---|---:|---|---|---|---|
| ClaySource | 2789 | Type 30, 158×92 | 17, `China_Industry_2.bmp` | `[79,76]` | Earth excavation with a lifting structure; plausible raw-material source, moderate visual evidence |
| Pottery | 2810 | Type 30, 158×140 | 17, `China_Industry_2.bmp` | `[79,120]` | Domed kiln and ceramic vessels; visually strong preview, existing choice retained |
| Warehouse | 637 | Type 30, 158×135 | 3, `China_StorNDist.bmp` | `[79,116]` | Roofed store with containers and loading platforms; visual and group-name evidence |
| Household | 1512 | Type 30, 158×96 | 7, `China_Housing.bmp` | `[79,79]` | Small inhabited thatched house; visual and group-name evidence, one static stage |

All four are version-213, size-flag-2 records with 12,800-byte base footprints, in-bounds internal color data, no alpha payload and zero mirror offset. Their overlay byte counts are respectively 3,231, 9,482, 13,130, and 3,960. The local Asset Browser showed each full image and the normal decoder succeeded. The group names are search evidence, not original building IDs. Original Emperor identity, building size, stage, animation and pivot remain unproven. The sandbox's one-cell occupancy, goods, routes, recipes, limits and save schema are unchanged. See [the research log](reverse/research-log.md) for observations and limits.
