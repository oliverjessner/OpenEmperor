# Curated terrain preview bindings

This is an OpenEmperor preview convention, **not** an Emperor file format or a recovered original map-to-image table. Map positions and raw words come from a user-supplied original `.map`. Coarse category and rule names come from the separate reference-derived diagnostic interpretation. A human selects a few local SG3 images after checking their decode and appearance. The original game's image variants, coast transitions, animation, height, buildings, and coordinate model remain unknown.

The application accepts a local JSON file with `--view textured --terrain-bindings <file>`. Keep real bindings under ignored `.local/terrain-bindings/`. This synthetic example describes the schema only; `tiles.sg3` is a fixture name, not a shipped or original archive:

```json
{
  "schema_version": 1,
  "map_profile": "emperor_map_v1_storage_grid",
  "mode": "curated_preview",
  "assets": {
    "sample_ground": {
      "archive": "tiles.sg3",
      "image_index": 0,
      "provenance": "Synthetic test tile, visually checked"
    }
  },
  "bindings": [
    {
      "id": "sample_fertile_pair",
      "terrain_raw": 128,
      "objects_raw": 0,
      "asset": "sample_ground",
      "expected_category": "fertile_hint",
      "expected_rule": "fertility_bit_without_value",
      "provenance": "Example exact pair; no original engine image mapping claimed"
    }
  ]
}
```

The schema has no wildcards or priority rules. Each exact `(terrain_raw, objects_raw)` pair and each binding ID must be unique; multiple listed pairs may refer to the same asset. JSON order does not affect selection. Expected category and rule must match the current diagnostic interpreter for that exact pair. A `partial` interpretation does not by itself block a binding: `fertile_hint` is partial because no numeric fertility value is known. Unknown bits remain in the cell diagnosis; the binding does not explain them.

Asset paths are relative to `--data`, must end in `.sg3`, and cannot traverse or escape the data directory through symlinks. The derived internal or documented external `.555` source is checked the same way. The file is limited to 256 KiB, 16 asset aliases, and 256 bindings. Preview assets must be supported Type-30 78×40, one-tile footprints with exactly 3,200 base bytes, no overlay/alpha, and zero horizontal mirror offset. Only assets used by map cells are decoded and uploaded; textures are keyed by distinct archive plus image index and limited to 4 MiB total. Missing, malformed, or out-of-bounds referenced assets are explicit `asset_error` failures, never silently replaced by an arbitrary tile.

The candidate mask is the existing reference-derived preview excerpt. Every candidate **storage cell** becomes one instance; the minimap's doubled/clipped pixels are not iterated. With `u=x-border`, `v=y-border`, the preview places its logical top vertex at `((u-v)*40, (u+v)*20)`, then places the 78×40 image at that vertex minus `(39,0)`. The image is not stretched to 80 pixels. Inverse picking applies the camera transform and floors the inverse grid coordinates, then checks storage bounds and candidate membership. Cells outside the candidate mask remain represented as `excluded_by_preview_mask`, not deleted from the parsed map. The off-map raw bit is reported independently. Unbound candidate cells display a conspicuous purple diagnostic diamond and retain `unmapped` status.

The initial ignored local binding chooses `DATA/China_Terrain.sg3` image 655 for the exact Xia pair `(128,0)` only. Its metadata and normal decode were checked; its flat sandy diamond was visually viewed. The pair occurs in the original map's candidate cells and the reference-derived rule is `fertility_bit_without_value`. This assignment is a **curated graphic choice**, not evidence that the original engine selected that image for those cells. Blue candidate images were viewed but not established as water, so water remains diagnostic.

The same ignored binding was run against three local original maps using the SDL dummy driver. Xia reported 3,612 candidate cells, 1,905 bound and 1,707 unmapped; Banpo 6,384 / 4,538 / 1,846; Chengdu 14,620 / 11,725 / 2,895. Each loaded one distinct texture and reported zero asset errors. The shared exact raw pair accounts for all bound cells; other pairs remained unmapped. A temporary offscreen software-render capture of Xia was visually inspected at fit and 2× zoom: beige selected tiles and purple diagnostic fields followed the diamond mask, with no apparent doubled or missing edge cells. The normal macOS video driver could not create a display in this sandbox, so interactive window/picking checks on an actual desktop remain to be done. The temporary capture and decoded pixels were not added to the repository.
