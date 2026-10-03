# Curated core building previews

## City-v16 rule 2: original Well and Herbalist

The exact six-file compatibility pack now supplies `well` **1559 (158×132, anchor [79,112])** and `health_post` **1580 (158×136, anchor [79,116])**, both in `China_Safety.bmp` group 8. Named original Model entries 72/207, traced EXE registrations, complete 2×2 Safety audit, visual motifs and original manual corroborate base Well / Herbalist identities with high confidence. HealthPost remains the simplified authored road service; no original medicine or Well-evolution mechanics are imported. [Audit and candidate table](reverse/original-presentation-correction.md).

Schema 1 optionally accepts `footprint_side: 1|2` on Well/HealthPost entries. Omission keeps the original strict side-1 contract. Both first decode and dedupe reuse require supported unmirrored Emperor Type-30, width `80*side−2`, base `3200*side²`. A declared 2×2 image therefore requires width 158 and base 12,800. Other role contracts remain unchanged. The active World rule footprint selects compatible entries; a mismatched explicit custom profile fails atomically, while mismatched built-in Safety roles use the legacy fallback. City-v14/15 and City-v16 rule 1 never draw these 2×2 sprites over one-cell authority.

All 13 entries / 12 distinct physical images load and upload eagerly, including the whole static Safety base/overlay. There is no per-frame file work, decoding or upload. Normal painter depth, front-cell ground, alpha-128 preview and full footprint selection are shared. Anchors are `[79,height−20]`, with no sprite hack or new pivot. Rule-2 unknown/missing visuals retain the two isolated authored renderers, given the derived footprint side; legacy scale/ground stays exact.

The earlier one-cell audits below are historical: they were incorrectly bounded for original identity and excluded all true 2×2 Safety bases. Their fallback conclusions still apply to legacy one-cell rules. The original Well/Herbalist assets are now identified for rule 2.

## Historical one-cell presentation

City-v15 optionally appends schema-1 `health_post`. It must be supported, unmirrored Emperor Type-30 with side 1, width 78 and a 3,200-byte base, checked on first decode and deduplicated reuse. It uses ordinary eager upload, independent role anchor, front-cell depth, F4 and alpha-128 placement. At that milestone the built-in pack had no Health Post selection: original-like HealthPost remained unresolved after the [bounded audit](reverse/health-post-visual-audit.md). The isolated authored timber/plaster pavilion remains available for legacy one-cell rules when a compatible custom image is absent; Health Worker stays a marker. Illness only adds a small render marker/optional House-footprint overlay; Household stages still select solely through effective World level.

City-v14 adds the optional schema-1 `well` role. It is loaded/deduplicated through the same assets layer, but requires an unmirrored supported Emperor Type-30 side-one footprint, width 78 and 3,200 base bytes, including when sharing an already decoded asset. Ground center follows `[39, image_height - 20]`; a 2×2 or classic-width image cannot stand in for the 1×1 Well. Old JSON profiles remain valid. The bounded original audit selected no convincing record, so the then-built-in metadata had eleven roles/ten unique textures and supplied no Well. **Original-like Well identity was unresolved at that milestone.** The current independent SDL fallback draws a stone basin, blue water, timber posts/beam and rope on the one-cell ground, without assets or uploads. F4 and custom profiles do not affect water authority. See [audit and 1×/2×/4× fallback review](city-v14.md#well-visual-and-bounded-audit).

`--building-visuals <profile.json>` supplies one optional, session-local display profile for `ClaySource`, `Pottery`, `Warehouse`, `Household`, `Farm`, `ServicePost`, `Market`, and `FireWatch`, plus optional City-v13 `household_level_0`, `household_level_1` and `household_level_2` entries. The menu uses the same **Building JSON...** picker. Missing roles retain diagnostic markers, so existing one-role, four-role and seven-role schema-1 profiles remain valid. F4 switches all configured building visuals on or off; F2 still controls walkers independently. Neither switch changes the World or a save.

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

Each `image_index` is a physical SG3 record, with no runtime-table translation. The parser rejects duplicate keys, unknown root/entry keys and roles, unsafe SG3 or resolved `.555` paths, unsupported or mirrored layouts, bad decodes, and anchors outside finite ±4096. The manifest limit is 1 MiB and deduplicated RGBA data is capped at 64 MiB. Each distinct selected asset decodes and uploads once per active profile; multiple buildings of one role share its texture. Replacing a live profile publishes the new texture set only after all assets have decoded and uploaded. A failed replacement keeps the current set.

`BuildingState.kind` determines the visual role; `BuildingId` is only a stable instance ID. In City-v10 and City-v11 the four core roles occupy 2×2 and use the front cell `origin+(1,1)` as projected visual ground. Farm, ServicePost, Market and FireWatch remain 1×1. The loader records the verified Type-30 square-base side from the physical record; these three added roles match their active 1×1 rule footprint. The placement formula remains `image_origin = screen(ground) − zoom × ground_anchor`. For a 78×40 Type-30 base at the bottom of an image, its OpenEmperor ground center is `(39, image_height - 20)`. No later renderer offset compensates this position. Thus the base covers `ground_y - 20` through `ground_y + 19` at 1× and the corresponding nearest-neighbor range at other zooms. A valid hover preview draws the image once with partial alpha and outlines the same logical footprint cell. Picking any occupied cell resolves the same entity, and selection outlines the full footprint. Hover creates no building and does not change World state. The shared ground-depth order mixes stored map images, roads, buildings and walkers, with projected x, layer and stable ID tie-breaks.

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

## City-v13 House evolution presentation

Schema **1** retains the legacy `household` entry and adds three optional role keys:

```json
"household_level_0": {"archive":"DATA/China_General.sg3","image_index":1512,"ground_anchor":[79,76],"evidence":"Curated OpenEmperor housing progression preview; original stage mapping unverified"},
"household_level_1": {"archive":"DATA/China_General.sg3","image_index":1516,"ground_anchor":[79,90],"evidence":"Curated OpenEmperor housing progression preview; original stage mapping unverified"},
"household_level_2": {"archive":"DATA/China_General.sg3","image_index":1520,"ground_anchor":[79,114],"evidence":"Curated OpenEmperor housing progression preview; original stage mapping unverified"}
```

For a placed City-v13 House, `SandboxView` queries only `World::household_level(id)` to select its asset. It never reconstructs demand history, spatial caps, capacity or taxes. A configured matching stage wins; otherwise `household` supplies that level, or the ordinary Household marker is drawn when both are absent. Thus legacy-only, partial-stage and stage-only profiles are valid. An explicitly malformed configured stage still fails; atomic replacement preserves the prior profile on failure. Unknown keys fail closed. City-v12 and earlier keep the single legacy role even when stages are configured. A valid fresh City-v13 placement preview uses Level 0 without creating a hypothetical World.

Each stage requires **Type 30, size flag 2, width 158, base length 12,800**: confirmed Emperor 2×2 geometry. Both first decode and deduplicated reuse enforce this constraint; classic 118-wide two-cell and larger foundations are rejected. All configured images are decoded/uploaded before publication. Roles sharing one physical AssetId share one texture while retaining separate anchors. At the housing milestone the built-in profile had eleven configured entries and **ten unique textures**, because legacy Household and Level 0 share record 1512. The current profile adds the two Safety entries, for thirteen roles/twelve textures. Runtime statistics count the selected stage role and actual unique uploads. A level switch adds zero decoding, uploads, file reads, BFS, route refreshes, commands or simulation ticks.

| Effective level | Physical record | Whole image | Ground anchor | Visible progression |
| --- | ---: | --- | --- | --- |
| 0 | 1512 | 158×96 | [79,76] | Simple rounded thatched hut |
| 1 | 1516 | 158×110 | [79,90] | Solid timber/stone house with grey tiled roof |
| 2 | 1520 | 158×134 | [79,114] | Larger plastered three-wing house with raised central room |

All are internal, unmirrored, alpha-free static records in group 7, `China_Housing.bmp`. Their decoded 2×2 base occupies the bottom **80 rows**. The existing visual ground is the front cell `origin+(1,1)`; its pixel anchor is **[79, height−20]**, not the overall base center. At 1× the base therefore occupies `ground_y−60…ground_y+19`, with horizontal extents `ground_x−79…ground_x+78`. The 158-pixel source retains its ordinary inset against the 160-pixel logical footprint. Independent synthetic bases at heights 96/110/134 verify these extents at 1×/2×/4×, after pan and with placement alpha. No crop, resize repair, mirroring or optical offset is applied. The old `household` [79,79] binding is deliberately unchanged for backward compatibility; its Level-0 alias has its own geometric [79,76] anchor.

The bounded Housing audit and native comparisons are recorded in [the research log](reverse/research-log.md#2026-10-02-house-evolution-visuals). The selected sequence is a **curated OpenEmperor housing progression preview**. Original Emperor stage assignment and pivot semantics remain unverified; the effective gameplay level remains OpenEmperor-authored.

Picking all four footprint cells resolves the same House ID at every stage. All stages use the same front-cell `WorldDrawKey` and whole-image depth ordering; taller images receive no special layer. The existing translucent desirability overlay leaves the artwork visible. Only reachable score/level combinations are tested: Poor permits Level 0 only, Neutral at most Level 1, Good any historically earned level. No impossible Poor/Level-2 World is fabricated. Fire overlays the current effective stage, and normal fire/demand may subsequently change it. Safe demolition uses the existing shared World query without a stage branch.

No visual stage is stored. Ordinary manual load and older autosave/recovery checkpoints derive the then-effective stage from their restored World. Synthetic SDL tests cover genuine supplied 0→1→2 development, 100 paid-command-driven reversals, exact snapshot stability while rendering/F4, zero additional render work, all-cell picking, overlay, burning-House safety, legacy/partial/absent-stage fallbacks, and failed replacement. Simulation, schemas, rule versions, economy and original data remain unchanged.
