# Curated Pottery building preview

`--building-visuals <profile.json>` optionally replaces only the existing sandbox Pottery marker with one selected image from the user's SG3/.555 files. The menu's **Building JSON...** choice is session-only. This JSON is an OpenEmperor display convention, not an Emperor building table or a save format. Keep the real profile under ignored `.local/visuals/`; the repository and app bundle contain no original pixels or private manifest.

```json
{
  "schema_version": 1,
  "mode": "curated_building_preview",
  "buildings": {
    "pottery": {
      "archive": "DATA/your-building-archive.sg3",
      "image_index": 123,
      "ground_anchor": [79, 120],
      "evidence": "Local visual selection; original building ID and pivot unverified"
    }
  }
}
```

`image_index` is a **physical SG3 record**, with no runtime-table translation. Only the `pottery` role and a supported, unmirrored Type-30 record are accepted. The loader checks the canonical data-root-relative SG3 and resolved `.555` paths, metadata, the ordinary decoder result, a 1 MiB manifest limit, finite anchor coordinates within ±4096, and a 16 MiB decoded RGBA limit. It loads and uploads the selected image once per active profile. A bad replacement profile leaves an already running view and texture intact.

The projected reference point is `world_for(BuildingState.cell)`. At zoom `z`, the image origin is `screen(reference) − z × ground_anchor`; no SG3 animation offset, image center, or image edge is interpreted as an original pivot. The image retains its decoded dimensions and straight alpha. The logical Pottery placement remains **one sandbox cell**, even when the visible image covers several tiles. Clicking or selecting a building still uses that one cell and its stable `BuildingId`; no transparent-pixel picking or extra collision footprint is added.

F4 switches only Pottery between the original image preview and its previous diagnostic marker. F2 independently switches the Clay walker display. Neither switch advances simulation, changes the dirty state, or enters a save. The map's `StoredGraphicsRenderer` remains an earlier background pass. Within the sandbox overlay, roads, buildings and walkers are ordered by projected ground depth, then projected x, kind and stable ID. This supports a walker being covered by the building when behind it and appearing in front when closer, but tall objects already drawn by the separate map background cannot occlude the building correctly. No original draw-order fidelity is claimed.

The locally examined `DATA/China_General.sg3` physical record 2810 is a visible domed kiln with vessels: v213, group 17 `China_Industry_2.bmp` (generic description `A new bitmap.`), Type 30, 158×140, size flag 2, 12,800 base bytes plus 9,482 color-overlay bytes, no alpha stream, no mirror. The chosen local ground anchor `[79,120]` is a preview placement by eye. The motif and archive group are search evidence, not proof of the original Emperor Pottery building ID, building footprint, animation state, or pivot. Other plausible industry images were retained as research observations in [the research log](reverse/research-log.md).
