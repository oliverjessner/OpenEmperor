# Test-scene format (schema version 1)

This JSON is an **OpenEmperor test convention**, not evidence about Emperor maps or original asset IDs. Store manifests and original data under ignored `.local/`. The manifest contains no image pixels. The first local example, `.local/scenes/first-scene.json`, is deliberately not tracked because it refers to a local game-data installation.

```json
{
  "schema_version": 1,
  "grid": { "width": 16, "height": 16, "cell_width": 80, "cell_height": 40 },
  "assets": {
    "ground_test": {
      "sg3": "relative/archive.sg3", "image_index": 0,
      "anchor": [39, 0], "footprint": 1
    },
    "sprite_test": {
      "sg3": "relative/sprite.sg3", "image_index": 1,
      "anchor": [20, 65]
    }
  },
  "terrain": {
    "default": "ground_test",
    "overrides": [{ "x": 4, "y": 5, "asset": "ground_test" }]
  },
  "objects": [{ "id": "one", "asset": "sprite_test", "x": 6.5, "y": 7.5 }]
}
```

The archive path is relative to `--data`; `image_index` is its SG3 image-table index. Both the archive and its resolved internal or documented external `.555` source must stay under the canonical data root, including through symlinks. Missing or undecodable referenced assets reject the whole scene with an AssetId error. The renderer decodes only used AssetIds once and shares their SDL texture across aliases and instances. `--scene` requires `--data` and conflicts with preview, single-image, browser, and diagnostic alpha options.

Raster coordinates denote the upper/back corner of a cell. Our flat projection is `world_x=(x−y)*40`, `world_y=(x+y)*20`; the inverse is `x=world_x/80+world_y/40`, `y=world_y/40−world_x/80`. The logical cell is 80×40. A decoded 1×1 Emperor Type-30 image is 78×40 and retains that pixel size; it is not stretched. `anchor` is in decoded image pixels, and `image_origin=project(tile_position)−anchor`. A confirmed Emperor Type-30 footprint of size `n` requires width `80*n−2`, base stream `3200*n*n` bytes, and anchor `(image_width/2, image_height−40*n)`. Thus a flat 78×40 tile uses `(39,0)`; a 158×80 2×2 image uses `(79,0)`, and a taller 158×120 image uses `(79,40)`. This positions the *footprint corner*, independent of image height. Only these unambiguous Emperor geometries are accepted in the scene. Other sprites declare their own chosen image anchor; no SG3 offset or mirror metadata is treated as a pivot. For a sprite intended to stand at a cell center, use `(x+0.5,y+0.5)` as its object position.

Cell selection converts window coordinates to render coordinates through SDL3, then inverts camera and projection and applies `floor`. Exactly on an integer tile boundary, the boundary belongs to the cell on its nonnegative coordinate side. Outside `[0,width)×[0,height)`, no cell is selected. Type-30 object positions must be integer corner coordinates with the full declared footprint inside the grid. Sprite positions may be fractional but must be within the grid; out-of-grid objects are rejected, not clipped by the manifest loader.

Version 1 permits at most 32×32 cells, 64 aliases, 256 objects, 1024 overrides, a 1 MiB manifest, 16 MiB decoded RGBA per image, and 64 MiB total decoded texture budget. Coordinates and anchors must be finite and within 8192. Zoom is clamped to 0.5×–4×. The scene viewer creates one texture per distinct used AssetId, clears and presents once per frame, and culls by the full projected image rectangle so tall sprites remain visible when their anchors leave the viewport. It draws terrain first, then objects ordered by `x+y+2*(footprint−1)`, breaking ties by unique instance ID. This simple order does not solve arbitrary intersecting large buildings. No map loading, animation, or simulation is implied.
