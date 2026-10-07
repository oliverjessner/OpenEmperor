# Animated fire presentation

This presentation pass starts from **`c107a1bf3464075d1cc05596d23aabf267912b80`**, with a clean worktree. A recognized original `fire1` clip replaces the orange triangle for active sandbox building fires. The normalized, alpha-blended triangle remains the complete fallback. FireWatch artwork and the FireInspector marker are separate, unchanged visuals.

## Original assets and authored presentation

The clip contains all **50 physical records 201–250** from `DATA/destruction.sg3`, backed by `DATA/destruction.555`. Every frame is an unmirrored 80×80 Omega sprite, decoded with the existing version-214 native alpha contract. The named clip, original resource registration, count/stride, static sequential selector and complete decoded sequence support its identity. [The bounded audit](reverse/fire-visual-audit.md) records exact hashes and separates **A: original bytes**, **B: appropriate ordered sequence**, and **C: original timing/positioning**.

A and B are supported. The sandbox timing and owner attachment are **OpenEmperor-authored presentation**, with no original-game parity claim. The common frame anchor is `[40,80]`. At tick `t`, building ID `id`, clip count `n` and duration `d`, selection is:

```text
((t / d) % n + ((id % 7) % n)) % n
```

The built-in duration is **two simulation ticks per frame**, with a 100-tick full loop. At the ordinary 20 ticks/second rate this presents ten frames per second. Both inputs are reduced before addition, so even `UINT64_MAX` tick/ID inputs stay bounded. There is no wall clock, per-building counter, RNG, inferred ignition tick or second multiplication by simulation speed. Pause freezes the frame; Save/Load reproduces it from the existing World tick and BuildingId.

## Small metadata and independent activation

[FireVisualProfile](../src/assets/FireVisualProfile.h) describes one explicit sequence of physical AssetIds, frame anchors, clip identity, evidence and a uniform `ticks_per_frame`. Schema 1 uses `mode: "curated_fire_presentation"`. A frame contains `archive`, `image_index` and `anchor: [x,y]`; it does not import a packed resource ID or courier role. The built-in metadata enumerates every required record instead of treating arbitrary adjacency as a clip.

The parser accepts at most **64 KiB** of metadata, **2–64** frame entries, **64** deduplicated physical assets, **1–256 pixels** per dimension, finite anchors bounded to **±256**, and **1–1000 ticks per frame**. Clip ID/evidence are bounded to 64/2048 bytes. Duplicate/unknown keys, empty or wholly invisible clips, an entirely static decoded clip, invalid aliases, mirrored/unsupported layouts, invalid dimensions and unsafe SG3/derived `.555` paths reject preparation. Repeated explicit frames such as A/B/A are allowed and reuse the same physical texture; the complete clip must include different decoded images. Every referenced asset must belong to the deduplicated prepared set. Existing Building and Walker schemas retain their contracts.

The exact core compatibility pack declares [fire.json](../resources/compatibility/gog-derived-2.0.0.2-en-assetset-1/fire.json) under an independent **`optional_fire`** entry. Its two additional dependencies are hashed separately; the existing six core fingerprints do not stand in for these files. Missing/unsafe/mismatched fire files, missing metadata, unknown revisions, unsupported frames, decode errors and texture/budget failures leave a named fire fallback. They do not disable an otherwise compatible building, road or walker pack, reject a valid save, or prevent the sandbox from loading.

The compatibility metadata is resolved using the normal resource root for builds and packages, including relocated packages. Original graphics stay in the user's selected data folder. Neither an original archive nor a decoded pixel belongs in the repository or app bundle.

F1 exposes **`Fire visual: Original animated clip`** with clip/resource provenance or **`Fire visual: Fallback`** with its concrete reason. A valid clip draws the selected original frame, without an extra triangle. Failed optional preparation discards the entire candidate and releases partial textures; it never drops only the bad frame or borrows a neighboring record.

## Drawing and ownership

The effect belongs to one existing burning building and is drawn in that building's ordinary spatial painter position, after its body. Starting from the camera-projected `building_visual_ground()`, attachment rises by `20 * (footprint_side - 1)` plus a bounded roof allowance: `clamp(active_role_ground_anchor_y * 0.3, 12, 36)` logical pixels, or 12 when no prepared role entry exists. These are explicit authored placement choices, using the authoritative footprint, existing signed-height projection and prepared role metadata; they do not infer original fire pivots. The prepared metadata remains the attachment source when F4 hides the building sprite.

One effect per owner preserves proportions; a 2×2 House is one fire instance. A side-1 owner uses a uniform 0.75 scale, larger owners use 1.0, both multiplied by camera zoom. Width and height always scale together, without stretching to building width. The common `[40,80]` canvas attachment keeps the effect position stable as silhouettes change.

The effect's all-frame bounds are prepared once and queried in constant time, followed by the selected raster bounds. An entering flame tip remains drawable even when its owner center is outside the viewport. Frame/draw counters count actual SDL texture submissions whose selected raster bounds intersect the viewport; they do not measure per-pixel alpha visibility after occlusion. Objects later in the existing painter may occlude it; fire is never appended over the entire world or UI. Gate/Great-Wall composition remains unchanged.

Fire adds no logical occupancy, selectable entity or effect hit descriptor. Existing building-body alpha inspection and ground construction picking remain ordinary, so even an opaque flame does not become a separate hit target; transparent effect regions cannot block input. UI hit handling and map-release-over-UI consumption remain unchanged.

[FireSpriteSet](../src/renderer/FireSpriteSet.h) eagerly uploads each unique prepared image once and shares those textures among every burning building. It uses the existing [TextureCompatibility](rendering/texture-compatibility.md): SDL **3.4.14 software** selects STREAMING, other actual runtime/backend combinations select STATIC. Frames use nearest scaling and native decoder color/alpha with ordinary source-over blending. There is no chroma key, additive blend or decoder relaxation; modulation and draw blend state cannot leak into the following world/UI draws.

The supported clip adds **50 textures / 1,280,000 logical RGBA bytes**, shared by all owners, plus one texture draw per visible burning owner (four submissions in the four-fire Xia close view). It consumes the remaining existing **64 MiB** session budget rather than increasing it. Candidate validation, decode and upload are load-time work. The added fire path performs no files, EXE work, decode/upload, pixel readback, World copy/command, BFS or route refresh. It uses the already selected building owner and adds no fire-specific map scan. The existing world renderer retains its pre-existing row-major grid traversal; this pass does not claim that the whole renderer is scan-free. Fire adds no inspection/picking scan.

## Frozen gameplay and evidence boundaries

Fire risk/protection/deadlines, ignition and duration, natural expiry, actual Inspector arrival, staffing/priorities/dispatch, roads/gates/routing, goods/reservations/recipes, maintenance/funds, footprints, rule versions, map policy and Save/Recovery schemas are unchanged. Burning eligibility comes only from existing World queries. Nonburning/excluded buildings and placement previews receive no fire. A Running but unstaffed Watch receives no invented worker or Inspector. Animation rendering cannot advance the simulation.

The original float-color hotfix remains independently tested by [FireOverlayPixelTests](../tests/FireOverlayPixelTests.cpp) on the explicitly absent/forced fallback. Its triangle palette and alpha expectations remain valid. Animated production tests use authored synthetic frames, with distinct silhouettes and independently inspected pixels; an index change alone does not establish animation.

Acceptance records synthetic profile/pixel tests, direct scripted handler delivery, scripted Application event-loop delivery, actual software/Metal backend output and human input separately. Native backend pixels are not native-device input. No human PASS or original-game side-by-side comparison is inferred from automation.

## Validation and test start

The source-frozen production path has passed the focused animated/fallback checks and all final regression matrices. The ordinary Xia Application/main process shows visibly different original frames. Debug, Release, the standard RelWithDebInfo ASan/UBSan suite and package validation have completed.

| Evidence | Result |
| --- | --- |
| Original clip preparation | All 50 required original frames decode through the unchanged loader; 50 distinct RGBA buffers, 50 shared eagerly uploaded textures / 1,280,000 logical bytes |
| Authored profile/frame/pixel checks | `animated-fire-clip-pixels` checks frame boundaries, wrap, maximum arithmetic, stable IDs, native transparency, different actual drawn silhouettes, state preservation, bounds/budget rejection, aliases and texture lifetime |
| Production software and Metal rendering | Existing `fire-overlay-production-pixels` retains all old fallback expectations and also tests the animated production path: pause/Save-Load pixels, no extra triangle, no preview/FireWatch effect, natural expiry at tick 2,600 and actual Inspector arrival |
| Production painter/input/resource checks | An entering tip with owner center 70 pixels outside the viewport, foreground occlusion, ordinary body/ground picking and UI consumption pass; added fire work has zero file/decode/upload/World/route counters and sessions release all owned effect textures |
| Actual normal Xia application, Metal SDL 3.4.14 | Visible ordinary Application/main with scripted `SDL_PollEvent` event delivery: House, Pottery, small Service Post and partly clipped Warehouse show original flames at 1× with F1 off. Twelve consecutive two-tick captures have visibly changing flames, with fixed attachment; all 50 frames are actually submitted during the render stream. Pause, F4, fractional zoom and 1×/2×/4× zoom, F1 provenance and ordinary F5/F9 pass: save at 2,000, step to 2,002, reload returns the same crop/frame at 2,000. A normal running 4× session ends naturally at exactly 2,600 with zero effect draws; 50 uploads occur before activation, none afterward, and all owned effect textures reach zero on shutdown |
| Actual normal Xia application, software SDL 3.4.14 | Same ordinary main/Application path with scripted event delivery: **13 distinct fire crops**, paused crop identical, F9 restores the same phase, actual House-effect widths at 1×/2×/4× are 80/160/320 pixels. All 50 frames are submitted; 50 eager uploads / zero later uploads, and zero owned effect textures at shutdown |
| Exact gate test starter | `sh tools/test_gate_passages.sh .local/gog-extracted/app Xia` builds/launches current code and the ordinary New/Start menu loads its own empty Xia root with **Original animated clip**, 50 prepared textures and zero effect draws because no building is burning. The capture is at actual elapsed tick 5, not an invented tick-0 sample |
| Natural Xia fire and actual Inspector, separate software/Metal production-menu driver | Direct scripted MenuSession delivery and finite ordinary World ticks: a zero-House city naturally has three fires at 2,000 with the Watch unstaffed 0/2 and idle. A normal paid House enables 2/2 staffing, dispatch at 2,001 and actual arrival at **2,065**, which clears the targeted owner, reduces effects 3→2, grants existing protection until 4,465 and starts Returning. Natural expiry separately gives zero fires/draws at 2,600. One hundred paused renders preserve full snapshot/crop with all 12 work counters zero, F9 preserves phase, and shutdown releases all effect textures. This driver is separate from Application/main and human/native input |
| Human native-device fire playthrough | Not run; actual backend output and scripted delivery do not establish human input acceptance |
| Release full suite | **113/113 passed, 66.74 s**, zero warnings/failures/skips/source changes |
| Debug full suite | **113/113 passed, 826.38 s**, zero warnings/failures/skips/source changes |
| ASan+UBSan full suite | **113/113 passed, 150.88 s**, standard RelWithDebInfo configuration, existing macOS `detect_leaks=0` and UBSan halt-on-error; zero warnings/failures/skips/sanitizer diagnostics/source changes |
| Existing additional endurance/resource checks | **PASS:** 100,000 ticks / 12 checkpoints with deterministic save bytes and equal parallel Worlds; 3,000 render frames with source files unavailable, zero simulation ticks/decode/upload/order rebuild; 100 existing menu/session invocations with the StoredGraphicsRenderer owned-texture counter zero. FireSpriteSet cleanup is separately checked by the new production tests; no claim is made that the older counter measures every texture class |
| App package / relocation / ZIP | **PASS:** separate arm64 Release package CTest, 13 existing negative checks and five added fire-metadata negative guards. Relocated and unzipped ordinary Application/main processes both automatically activate the original clip on actual Metal: 13 distinct crop phases, stable pause/reload, 1×/2×/4×, all 50 submitted frames, 50 initial uploads / zero later uploads and zero effect textures at shutdown. Strict signatures pass again after both runs; bundled resources include only metadata, with no original pixels |

The full matrices retain fire-state/Save/Recovery, gate-passage/budget, texture-zoom, scene-composition, road-responsiveness and existing endurance/resource checks. The actual application evidence uses a private before-Present backbuffer observer and scripted events delivered by `SDL_PollEvent` interposition. It exercises the ordinary main/Application loop and real Metal/software renderers; no SDL_PushEvent queue, native-device input or human play is claimed. Twelve consecutive actual application captures were viewed together as a short local animation comparison: flame tongues change while the House-roof attachment stays fixed. A further multi-phase House/Pottery contact sheet also shows stable attachments. Original-frame and actual-application codec previews are ignored diagnostic artifacts, never bundled assets. All **1,464 original files / 819,711,091 bytes** remain byte-identical to the prior complete hash manifest after the application runs. The installed SDL 3.4.14 library hash is unchanged.

The versioned [fire test starter](../tools/test_fire_visuals.sh) builds current application code and a separate technical-fixture writer. It creates a new private app root, places House/Pottery/Warehouse/Service Post/FireWatch using ordinary paid commands (590 spent), and advances unchanged rules to four natural fires at tick 2,000. The disconnected fixture grants no invented visit or protection. This is explicitly a **technical fixture**, not a human build playthrough or a new gameplay rule. At tick 2,000 the save retains City-v16 rule 3 / Map policy 1 / schema 19, four fires, and 590 funds after ordinary maintenance.

The exact starter below was executed: it built current code, generated the isolated four-fire save and launched the ordinary application. A separate technical observation run used the same executable/root with a private observer; the starter itself needs no observer or packaging step. From the repository:

```sh
sh tools/test_fire_visuals.sh .local/gog-extracted/app
```

Choose **Load Sandbox → Load selected save**. The scene starts paused in the whole-map fit view (about 0.18×), so the initial flames are small. Move the pointer over the visible House/Pottery building cluster slightly left of map center and **scroll the mouse wheel there to zoom in**, then press **Space** to animate. Once the cluster is visible, Z cycles zoom around the viewport center; using Z alone from the initial fit view can move the cluster out of view.

F1 reports **Original animated clip** / `original-destruction-fire1`, or a named **Fallback** with orange triangles. The mode is visible with the side panel; longer resource details can truncate, and hiding the panel provides more space. Each invocation uses a fresh `.local/fire-visuals/player-Xia-*` root, preserving personal settings/saves/recovery.

The pre-existing gate starter also builds current code and receives automatic fire activation with the same supported data. It starts an empty city:

```sh
sh tools/test_gate_passages.sh .local/gog-extracted/app Xia
```

The dirty-workspace local package is `.local/fire-visuals/package-dist/OpenEmperor-0.1.0-alpha.2-c107a1bf3464-macos-arm64.zip`, **4,311,860 bytes**, SHA-256 `dee9ea556150207aee5cf190d05379152a9f3dea184ce1a61774fc6a7cbd1b03`. It is a local test candidate, not a release or publication.

**IMPLEMENTED:** the complete original clip visibly animates in the normal supported application and its package, with all required regression checks complete. A static decode or private-only renderer was not used as completion evidence. Timing, owner attachment and uniform footprint scale remain curated presentation; original Emperor timing/pivot parity and the broader human city-playthrough/recovery acceptance remain open.
