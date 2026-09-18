# Curated Clay walker preview

`--sandbox-visuals <profile.json>` is an optional display layer for the Clay couriers in production, household, settlement, and industry sandboxes. The menu's **Walker JSON...** button selects the same kind of profile for the current app session. Store profiles referring to your legally obtained game data under ignored `.local/visuals/`. No profile or original frame is bundled. F2 switches between the preview and ordinary courier markers; the World, save schema, and rule profile do not change.

The JSON is an OpenEmperor convention, not an Emperor format. Each `image_index` is a **physical SG3 record number**, including record zero in numbering (zero itself is rejected). No runtime-table skip or packed graphic-ID conversion is applied. `archive` is relative to the selected game-data root. A frame's `foot_anchor` is measured in its decoded image pixels from the top-left to the chosen projected ground point. The anchor is chosen by visual inspection; SG3 animation offsets are not treated as verified pivots. Frame dimensions may differ.

```json
{
  "schema_version": 1,
  "mode": "curated_walker_preview",
  "role": "clay",
  "ticks_per_frame": 4,
  "evidence": "Locally inspected sprite sequence; preview speed and foot points chosen by eye",
  "frames": [
    {"alias": "a", "archive": "DATA/your-sprites.sg3", "image_index": 10, "foot_anchor": [12, 45]},
    {"alias": "b", "archive": "DATA/your-sprites.sg3", "image_index": 18, "foot_anchor": [11, 46]}
  ],
  "clips": {"pos_x": ["a", "b"]},
  "idle": "a"
}
```

`pos_x`, `neg_x`, `pos_y`, and `neg_y` refer to changes in storage-grid coordinates, not claimed compass directions. Omit an unverified direction. It displays a marker with `W?` while moving on that edge. A stopped or waiting courier displays the static `idle` frame. The current path edge determines direction, including a protected edge still being traversed after a road removal. The animation chooses `clip[(world_tick / ticks_per_frame) % clip.size()]`; the tick duration and global cycle origin are preview choices, not recovered original timing. Cargo remains a separate blue overlay from actual courier state.

The loader accepts at most 1 MiB of JSON, 256 frame aliases, 64 entries per direction, and 64 MiB of distinct decoded RGBA frames. It checks the SG3 and derived `.555` paths against the canonical data root, including symlinks, uses the shared bounds-checked SG3 loader, and uploads each distinct selected frame once before the preview is active. Invalid referenced frames reject activation, leaving a running sandbox intact. The preview uses the existing map overlay draw order; overlap with tall buildings or other walkers is not a claim of original-game occlusion. Neither this manifest nor decoded pixels are included in sandbox saves or the macOS app bundle.
