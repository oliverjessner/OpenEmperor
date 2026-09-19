# Curated walker previews

`--sandbox-visuals <profile.json>` is an optional display layer for Clay, Pottery, and Household couriers in production, household, settlement, and industry sandboxes. The menu's **Walker JSON...** button selects the same kind of profile for the current app session. Store profiles referring to your legally obtained game data under ignored `.local/visuals/`. No profile or original frame is bundled. F2 switches all configured roles between sprites and ordinary courier markers; the World, save schema, and rule profile do not change. An unconfigured role retains its marker.

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

Schema 1 remains supported exactly as before: its top-level `role` must be `clay`, and it loads into the Clay role only. It is never rewritten. Schema 2 groups independent visual sets under `roles`:

```json
{
  "schema_version": 2,
  "mode": "curated_walker_preview",
  "roles": {
    "clay": {
      "ticks_per_frame": 4,
      "evidence": "Locally inspected preview sequence",
      "frames": [{"alias": "a", "archive": "DATA/your-sprites.sg3", "image_index": 10,
                  "foot_anchor": [12, 45]}],
      "clips": {"pos_x": ["a"]},
      "idle": "a"
    }
  }
}
```

Allowed role keys are exactly `clay`, `pottery`, and `household`. Each role has its own aliases, clip order, idle frame, and tick rate. The runtime chooses a role solely from `CourierState.role`, never CourierId. `None` has no visual role. The same physical SG3 AssetId can be used in several roles; its decoded RGBA and SDL texture are shared globally.

`pos_x`, `neg_x`, `pos_y`, and `neg_y` refer to changes in storage-grid coordinates, not claimed compass directions. Omit an unverified direction. It displays a marker with `W?` while moving on that edge. A stopped or waiting courier displays the static `idle` frame. The current path edge determines direction, including a protected edge still being traversed after a road removal. The animation chooses `clip[(world_tick / ticks_per_frame) % clip.size()]` for the selected role; the tick duration and global cycle origin are preview choices, not recovered original timing. Cargo remains a separate role-colored diagnostic overlay from actual courier state.

The loader accepts at most 1 MiB of JSON, three roles, 256 frame aliases and 256 distinct physical assets **across all roles**, 64 entries per direction clip, and 64 MiB of globally deduplicated RGBA. Each role requires a 1–1000 tick rate, at least one moving clip, and an idle alias belonging to that role. Duplicate JSON keys are rejected. It checks the SG3 and derived `.555` paths against the canonical data root, including symlinks, uses the shared bounds-checked SG3 loader, and uploads each distinct selected frame once before the preview is active. Invalid referenced frames reject activation, leaving a running sandbox intact. All walkers enter the shared ground-depth painter. Its whole-image overlap is a preview convention, not proven original-game occlusion. Neither this manifest nor decoded pixels are included in sandbox saves or the macOS app bundle.

For a selected profile, F3 opens a bounded clip inspection panel over the sandbox. `V` cycles all three roles, showing `UNCONFIGURED` for a missing one. `Q` cycles storage directions, `C`/`E` select the previous/next configured frame without advancing the World, `X` switches 1×/4× nearest-neighbor display, and `B` switches dark/light backgrounds. The panel shows role, archive, physical record, frame size, chosen foot anchor, tick rate, short evidence, full image boundary, and projected ground point. It intentionally shows the decoded color and alpha as loaded, including any unexplained opaque red pixels. F2 remains the separate live sprite/marker toggle. The panel does not change the simulation's tick-bound animation.

The local `DATA/SprMain.sg3` preview currently uses four independently selected four-frame clips: `neg_x` 109/117/125/133, `neg_y` 110/118/126/134, `pos_x` 111/119/127/135, and `pos_y` 112/120/128/136. Their four-phase sequence, foot contacts, and loop were visually checked at 4×; their speed remains the chosen four ticks per frame. The local JSON stays ignored under `.local/visuals/`. These physical IDs identify a related figure in the user's data, not a proven Emperor Clay-courier registration. The red region's source and limits are recorded separately in [the research log](reverse/research-log.md).

A separate ignored local schema-2 profile currently adds Pottery-preview records `217/225/233/241`, `218/226/234/242`, `219/227/235/243`, `220/228/236/244`, and Household-preview records `433/441/449/457`, `434/442/450/458`, `435/443/451/459`, `436/444/452/460`, in `neg_x`, `neg_y`, `pos_x`, `pos_y` order. Both use 4 ticks/frame as an OpenEmperor choice. Their per-frame anchors are recorded in `.local/visuals/walkers-v2.json`; see the [research log](reverse/research-log.md) for evidence and limits. Neither series proves an original Emperor profession assignment.
