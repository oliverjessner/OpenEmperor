# Curated core building previews

`--building-visuals <profile.json>` supplies one optional, session-local display profile for `ClaySource`, `Pottery`, `Warehouse`, `Household`, `Farm`, `ServicePost`, `Market`, and `FireWatch`. The menu uses the same **Building JSON...** picker. Missing roles retain diagnostic markers, so existing one-role, four-role and seven-role schema-1 profiles remain valid. F4 switches all configured building visuals on or off; F2 still controls walkers independently. Neither switch changes the World or a save.

The manifest belongs under ignored `.local/visuals/` and contains only references and preview choices, never pixels:

```json
{
  "schema_version": 1,
  "mode": "curated_building_preview",
  "buildings": {
    "clay_source": {"archive":"DATA/example.sg3","image_index":10,"ground_anchor":[79,76],"evidence":"Local visual choice; identity unverified"},
    "pottery": {"archive":"DATA/example.sg3","image_index":11,"ground_anchor":[79,120],"evidence":"Local visual choice; identity unverified"},
    "warehouse": {"archive":"DATA/example.sg3","image_index":12,"ground_anchor":[79,116],"evidence":"Local visual choice; identity unverified"},
    "household": {"archive":"DATA/example.sg3","image_index":13,"ground_anchor":[79,79],"evidence":"Local visual choice; identity unverified"},
    "farm": {"archive":"DATA/example.sg3","image_index":14,"ground_anchor":[39,41],"evidence":"Local agricultural preview choice"},
    "service_post": {"archive":"DATA/example.sg3","image_index":15,"ground_anchor":[39,28],"evidence":"Generic civic preview; profession unverified"},
    "market": {"archive":"DATA/example.sg3","image_index":16,"ground_anchor":[39,78],"evidence":"Local distribution preview choice"},
    "fire_watch": {"archive":"DATA/example.sg3","image_index":17,"ground_anchor":[39,59],"evidence":"Curated civic/watch-building preview; original building identity unverified"}
  }
}
```

Each `image_index` is a physical SG3 record, with no runtime-table translation. The parser rejects duplicate keys, unknown roles, unsafe SG3 or resolved `.555` paths, unsupported or mirrored layouts, bad decodes, and anchors outside finite ±4096. The manifest limit is 1 MiB and deduplicated RGBA data is capped at 64 MiB. Each distinct selected asset decodes and uploads once per active profile; multiple buildings of one role share its texture. Replacing a live profile publishes the new texture set only after all assets have decoded and uploaded. A failed replacement keeps the current set.

`BuildingState.kind` determines the visual role; `BuildingId` is only a stable instance ID. In City-v10 and City-v11 the four core roles occupy 2×2 and use the front cell `origin+(1,1)` as projected visual ground. Farm, ServicePost, Market and FireWatch remain 1×1. The loader records the verified Type-30 square-base side from the physical record; the new built-in roles all match their active 1×1 rule footprint. The placement formula remains `image_origin = screen(ground) − zoom × ground_anchor`. For a 78×40 Type-30 base at the bottom of an image, its OpenEmperor ground center is `(39, image_height - 20)`. No later renderer offset compensates this position. Thus the base covers `ground_y - 20` through `ground_y + 19` at 1× and the corresponding nearest-neighbor range at other zooms. A valid hover preview draws the image once with partial alpha and outlines the same logical footprint cell. Picking any occupied cell resolves the same entity, and selection outlines the full footprint. Hover creates no building and does not change World state. The shared ground-depth order mixes stored map images, roads, buildings and walkers, with projected x, layer and stable ID tie-breaks.

The locally selected first visual set (all `DATA/China_General.sg3`) is a **preview convention**:

| Sandbox role | Physical record | Type / image | Group | Ground anchor | Visible motif / evidence |
|---|---:|---|---|---|---|
| ClaySource | 2789 | Type 30, 158×92 | 17, `China_Industry_2.bmp` | `[79,76]` | Earth excavation with a lifting structure; plausible raw-material source, moderate visual evidence |
| Pottery | 2810 | Type 30, 158×140 | 17, `China_Industry_2.bmp` | `[79,120]` | Domed kiln and ceramic vessels; visually strong preview, existing choice retained |
| Warehouse | 637 | Type 30, 158×135 | 3, `China_StorNDist.bmp` | `[79,116]` | Roofed store with containers and loading platforms; visual and group-name evidence |
| Household | 1512 | Type 30, 158×96 | 7, `China_Housing.bmp` | `[79,79]` | Small inhabited thatched house; visual and group-name evidence, one static stage |
| Farm | 2415 | Type 30, 78×61 | 15, `China_Fields.bmp` | `[39,41]` | Cultivated green field; strong agricultural motif, generic static Farm preview |
| ServicePost | 2046 | Type 30, 78×48 | 11, `China_Government2.bmp` | `[39,28]` | Freestanding notice sign; plausible generic civic post, no original profession claimed |
| Market | 645 | Type 30, 78×98 | 3, `China_StorNDist.bmp` | `[39,78]` | Staffed two-level stall with displayed goods; strong distribution/market motif |

The original four are version-213, size-flag-2 records with 12,800-byte bases. The three added records are version-213, size-flag-1, 78 pixels wide and have 3,200-byte 1×1 bases. All seven have in-bounds internal color, no alpha payload and zero mirror offset, and all decoded through the normal loader. The three additions were inspected as full images at 1× and nearest-neighbor 4×. Independent synthetic SDL pixel tests use 78×61, 78×48 and 78×98 images with a distinct final 78×40 base and verify normal, translucent placement, panned, 1×, 2× and 4× rendering against logical-cell boundaries. Group names are search evidence, not original building IDs. The mappings and ground anchors are OpenEmperor presentation choices; they do not establish original pivots. Goods, recipes, footprints, rule version and saves are unchanged. See [the research log](reverse/research-log.md) for observations and limits.

A bounded source-value audit found no exact RGB555 `0x7c00` pixels in either the fixed Type-30 base or the Omega overlay of records 2789, 2810, 637, or 1512. Pottery 2810 reports `animation_sprites=0`, offsets `(0,0)`, speed ID 6; ClaySource 2789 reports 20 sprites, offsets `(28,-3)`, speed ID 5. The Warehouse and Household selections report no animation sprites. These fields do not establish frame associations, and no building-animation composition was added. The conspicuous red patches seen beside the buildings in the earlier sandbox capture were carried by overlapping flagged walker sprites, not by these four building payloads.

## Fire Watch presentation (2026-10-02)

The exact-fingerprint built-in pack uses physical record **383** in `DATA/China_General.sg3`, group 1 (`China_Aesthetic.bmp`, description `Aesthetics`). The complete decoded **78×79** image depicts a freestanding red-brick entrance house with a grey tiled roof, substantial side wall and visible dark entrance. It is a **curated civic/watch-building preview; original building identity unverified**. The small entrance/pavilion-like motif is a visual description, not an identification of an original fire station or a recovered asset registration.

Type 30, size flag 1, width 78 and uncompressed base length 3,200 establish its **78×40, one-cell foundation**. Internal color is in bounds, mirror offset and alpha are zero. Anchor **[39,59] = [39,height−20]** uses the decoded base center, with no crop, asset resizing or optical offset. Ordinary scene zoom is unchanged. The seven other role bindings and anchors, compatibility fingerprints, gameplay footprints and renderer are unchanged.

The previous physical record **921**, group 5 `China_Military.bmp`, 78×104/anchor [39,84], remains technically valid and correctly decoded. Its crenellated stone structure reads as a wall/fortification component in the actual city, so it is **rejected for FireWatch presentation**. Its prior audit is retained as historical evidence in [the research log](reverse/research-log.md#2026-10-02-firewatch-presentation-replacement).

A bounded local atlas reviewed 40 strict one-cell candidates in groups 1, 3, 5, 10, 11 and 14; Safety group 8 had no qualifying one-cell record. The strongest candidates 383, 384 and 1900 were compared in the production `SandboxView` on the user's existing Xia City-v13 save, at 1×/2×/4×, with one Watch near Houses and a second normally paid Watch between Pottery and Market. Record 383 has the clearest roof/entrance/side-wall silhouette; 384 shows more of an open passage, while 1900 reads as a low roof/platform and was rejected. Ground selection and road alignment require no correction. Adjacent Market artwork can partially obscure the second Watch under existing whole-image depth sorting; its base remains in the selected cell.

Native Cocoa-window review used a local ignored runner linked to the unchanged application objects, shared loader, compatibility detector and renderer. Keyboard Return cycled predefined camera/profile comparisons; native screenshots verified all three candidates at 1×/2×/4× and both final Watch locations. This is actual desktop visual review of the production city rendering, not an independent human perception study or a manual pointer-placement playthrough. Offscreen readbacks separately cover all 18 candidate/zoom/location combinations, with exactly equal World snapshots and eight shared building textures. Original pixels, atlas pages, screenshots, local profiles and the runner remain under ignored `.local/fire-watch-replacement/`, never in resources or Git.

`fire_watch` is optional in schema 1. Old profiles retain per-role fallback. Both first decode and deduplicated reuse enforce its one-cell foundation; sharing a prior two-cell core record cannot bypass validation. Kind-based selection, common ground-depth ordering, F4, partially transparent placement and one-cell selection use the existing renderer. Two Watch entities share one image and texture; priority and operation affect no visual geometry. Its FireInspector walker deliberately retains the diagnostic marker.

The selected Watch's panel starts with its ID, assigned/required workers, Running/Paused operation, priority, Inspector phase, current target and route status. Protection/burning counts explicitly say **City-wide**; there is no per-Watch visit attribution. Technical asset details require F1. Status and demolition text wrap without ellipsis, with the building list below the Watch status. The ordinary read-only demolition query supplies a derived blocker tag, never persisted. An active owned Inspector shows patrol, return, or interrupted-route guidance; other building blockers retain their exact stock/recipe/reservation/fire reason. Displaying or selecting the Watch never pauses it. Pause/priority/Demolish retain the ordinary command/hit/confirmation paths.
