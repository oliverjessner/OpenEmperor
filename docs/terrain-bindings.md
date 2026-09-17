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

The initial ignored local binding chose `DATA/China_Terrain.sg3` image 655 for exact pair `(128,0)`. Its earlier description as a flat sandy diamond was **invalidated by the RGB555 red/blue correction**: a new local offscreen export shows it as uniformly pale blue. The binding now selects image 268 from the same archive, whose corrected Type-30 decode was viewed as a flat, textured ochre soil diamond. The pair occurs in the original map's candidate cells and the reference-derived rule is `fertility_bit_without_value`. This assignment is a **curated graphic choice**, not evidence that the original engine selected that image for those cells. Nearby pale-blue and darker teal candidate images were viewed, but color and adjacency alone do not establish a water role. Water remains diagnostic and unbound.

The revised ignored binding was run against three local original maps using the SDL dummy driver. Xia reported 3,612 candidate cells, 1,905 bound and 1,707 unmapped; Banpo 6,384 / 4,538 / 1,846; Chengdu 14,620 / 11,725 / 2,895. Each loaded one distinct texture and reported zero asset errors. The shared exact raw pair accounts for all bound cells; other pairs remained unmapped. The previous temporary offscreen software-render capture of Xia, made before the color correction, verified placement and diagnostic fields but is no longer evidence for the selected tile's color. The newly selected source tile was visually checked in an offscreen PNG export; interactive desktop rendering and picking were not checked in this milestone. The temporary exports and decoded pixels were not added to the repository.
