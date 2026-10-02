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
    "fire_watch": {"archive":"DATA/example.sg3","image_index":17,"ground_anchor":[39,84],"evidence":"Curated watchtower; original function unverified"}
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

The exact-fingerprint built-in pack additionally uses physical record **921** in `DATA/China_General.sg3`, group 5 (`China_Military.bmp`, description `A new bitmap.`). The visibly crenellated stone watchtower is a curated OpenEmperor Fire Watch depiction; its original profession/building identity is unverified. Actual shared decoding produced **78×104 RGBA**, with internal color, zero alpha length/offset and zero mirror offset. Type 30, size flag 1 and the 3,200-byte base establish a **78×40, one-cell foundation**, independently of the total roof/tower height. Its anchor is **[39,84]**, the bottom base center `(39,104-20)` in OpenEmperor coordinates, not a recovered Emperor pivot. No resizing, cropping, renderer compensation or changes to the seven earlier anchors were made.

`fire_watch` is optional in schema 1. Old profiles retain per-role fallback. Both first decode and deduplicated reuse enforce its one-cell foundation; sharing a prior two-cell core record cannot bypass validation. Kind-based selection, common ground-depth ordering, F4, partially transparent placement and one-cell selection use the existing renderer. Two Watch entities share one image and texture; priority and operation affect no visual geometry. Its FireInspector walker deliberately retains the diagnostic marker.

The selected Watch's panel starts with its ID, assigned/required workers, Running/Paused operation, priority, Inspector phase, current target and route status. Protection/burning counts explicitly say **City-wide**; there is no per-Watch visit attribution. Technical asset details require F1. Status and demolition text wrap without ellipsis, with the building list below the Watch status. The ordinary read-only demolition query supplies a derived blocker tag, never persisted. An active owned Inspector shows patrol, return, or interrupted-route guidance; other building blockers retain their exact stock/recipe/reservation/fire reason. Displaying or selecting the Watch never pauses it. Pause/priority/Demolish retain the ordinary command/hit/confirmation paths.
