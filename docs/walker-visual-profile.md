# Curated walker previews

`--sandbox-visuals <profile.json>` is an optional display layer for Clay, Pottery and Household couriers; schema 3 also permits FireInspector. The menu's **Walker JSON...** button selects the same kind of profile for the current app session. Store custom profiles referring to your legally obtained game data under ignored `.local/visuals/`. Built-in compatibility resources contain OpenEmperor metadata only; custom profiles and original frames are not bundled. F2 switches all configured roles between sprites and ordinary courier markers; the World, save schema, and rule profile do not change. An unconfigured role retains its marker.

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

Schema-2 role keys are exactly `clay`, `pottery`, and `household`. Schema 3 keeps that structure and adds `fire_inspector`, with a bounded `clip_id` required for that role. Schema 4 additionally admits the `supplier` and `distributor` families, each requiring a bounded clip ID; historical role values and schema limits remain unchanged. The Food/Market follow-up is still in progress: its known-original automatic resource and display-flip decision are pending, as recorded in [the current status](market-walker-visuals.md). Each role has its own aliases, clip order, idle frame, and tick rate. The runtime chooses a role solely from `CourierState.role`, never CourierId. `None` has no visual role. The same physical SG3 AssetId can be used in several roles; its decoded RGBA and SDL texture are shared globally while foot anchors remain per-role/per-frame.

`pos_x`, `neg_x`, `pos_y`, and `neg_y` refer to changes in storage-grid coordinates, not claimed compass directions. Omit an unverified direction in a custom profile. It displays a marker with `W?` while moving on that edge. A stopped or waiting legacy courier displays the static `idle` frame. FireInspector and the Food/Market families may face their retained next edge with that direction's first static frame while waiting; idle/no-edge still uses its idle alias. The current path edge determines direction, including a protected edge still being traversed after a road removal. The animation chooses `clip[(world_tick / ticks_per_frame) % clip.size()]` for the selected role; the tick duration and global cycle origin are preview choices, not recovered original timing. Cargo remains a separate role-colored diagnostic overlay from actual courier state; the existing cargo-free Inspector receives none.

The loader accepts at most 1 MiB of JSON, the historical schema-1/2 role limits or four schema-3/six schema-4 roles, 256 frame aliases and 256 distinct physical assets **across all roles**, 64 entries per direction clip, and 64 MiB of globally deduplicated RGBA. Each role requires a 1–1000 tick rate, at least one moving clip, and an idle alias belonging to that role. A schema-3 Inspector needs visible decoded frames and at least two different prepared images in each configured direction. Duplicate JSON keys are rejected. It checks the SG3 and derived `.555` paths against the canonical data root, including symlinks, uses the shared bounds-checked SG3 loader, and uploads each distinct selected frame once before the preview is active. Invalid explicit custom profiles reject activation, leaving a running sandbox intact. All walkers enter the shared ground-depth painter. Its whole-image overlap is a preview convention, not proven original-game occlusion. No custom manifest or decoded pixel enters sandbox saves or the app bundle; built-in metadata is packaged through normal resource resolution.

Automatic FireInspector activation applies to City-v12 through City-v16 and uses a separate schema-3 supplement containing only that role, with all four complete animated directions. Older production sessions retain their existing asset counts. It shares core physical assets and the unchanged global budgets; failed optional preparation preserves valid core walkers and a named Inspector marker fallback. Explicit custom walker selection disables automatic supplementation, including for old profiles; a declared schema-3 custom Inspector remains available for manual/F3 inspection. See [the Inspector contract, current activation status and evidence](fire-inspector-visuals.md).

For a selected profile, F3 opens a bounded clip inspection panel over the sandbox. `V` cycles all six visual roles, showing `UNCONFIGURED` for a missing one. `Q` cycles storage directions, `C`/`E` select the previous/next configured frame without advancing the World, `X` switches 1×/4× nearest-neighbor display, and `B` switches dark/light backgrounds. The panel shows role, archive, physical record, frame size, chosen foot anchor, tick rate, short evidence, full image boundary, and projected ground point. F2 remains the separate live sprite/marker toggle. The panel does not change the simulation's tick-bound animation.

The ordinary RGB555 decoder still represents source value `0x7c00` as opaque red. A separately verified presentation step applies only to Omega Sprite records whose SG3 byte 59 is nonzero and which have no separate alpha stream. It converts exact decoded `0x7c00` markers to straight RGBA black with alpha 128 once while the profile is loaded. SDL source-over then darkens the destination to approximate the original RGB555 blitter's per-channel floor-half operation. No per-frame pixel scan occurs, nearby red shades stay unchanged, and the rule is not applied to Type-30 images. The RGBA blend can differ by one quantization step from the original 5-bit operation; this is a presentation limitation rather than a file-format reinterpretation.

The local `DATA/SprMain.sg3` preview currently uses four independently selected four-frame clips: `neg_x` 109/117/125/133, `neg_y` 110/118/126/134, `pos_x` 111/119/127/135, and `pos_y` 112/120/128/136. Their four-phase sequence, foot contacts, and loop were visually checked at 4×; their speed remains the chosen four ticks per frame. The local JSON stays ignored under `.local/visuals/`. These physical IDs identify a related figure in the user's data, not a proven Emperor Clay-courier registration. The shadow-marker source, verified composition path, counts, and limits are recorded separately in [the research log](reverse/research-log.md).

A separate ignored local schema-2 profile currently adds Pottery-preview records `217/225/233/241`, `218/226/234/242`, `219/227/235/243`, `220/228/236/244`, and Household-preview records `433/441/449/457`, `434/442/450/458`, `435/443/451/459`, `436/444/452/460`, in `neg_x`, `neg_y`, `pos_x`, `pos_y` order. Both use 4 ticks/frame as an OpenEmperor choice. Their per-frame anchors are recorded in `.local/visuals/walkers-v2.json`; see the [research log](reverse/research-log.md) for evidence and limits. Neither series proves an original Emperor profession assignment.

The separately activated built-in schema-3 Inspector uses all twelve phases of those four SprMain columns, `433 + direction offset + 8×phase`, with its own explicit common-foot anchors and two ticks/frame. Its 48 images share 16 core Household pixels, adding 32 images/193,944 logical RGBA bytes for 80 globally shared walker images/506,868 bytes. Household's older anchor/cadence choices remain exact. Original human pixels and ordered walking are supported; profession, feet and timing are curated. See [the complete A/B/C audit](reverse/fire-inspector-visual-audit.md) and [normal-app/gate acceptance and test starter](fire-inspector-visuals.md).
