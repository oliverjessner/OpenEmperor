# City-v14: water access and residential infrastructure

Current native input delivery and the executable short human check are centralized in [Native Input Reliability](input-reliability.md) and [input acceptance](testing-input-acceptance.md). Historical observations below retain their original scope; they do not mark the current human checklist PASS or close profile-specific balance/replanning playthroughs.

`sandbox-city-v14`, rule **1**, extends City-v13 rule 1 with proximity-based water access. These are independently authored OpenEmperor rules, **not reconstructed Emperor water mechanics**. Economy, Market distribution, road couriers, Service, fire, desirability, population, workforce controls, demolition and House evolution are inherited. City-v11 rule 3 remains the fresh-settings menu default, and existing preferences, CLI defaults and schemas 1–15 retain their meanings. Starting commit: `1c5782affaf6262028e51792fe2bb2c1f7e5ad18`; the working tree was clean before this milestone. No commit, push, tag or release is performed.

## Well and derived coverage

| Property | City-v14 rule |
| --- | --- |
| Command / kind | `PlaceWell` / `Well` |
| Construction cost | 60 funds, normally paid |
| Footprint / limit | 1×1 / four Wells |
| Water radius | Five logical cells, inclusive |
| Workers / couriers / inventory | None |
| Operations / priorities | Unsupported |
| Fire eligibility / desirability impact | False / zero |
| City limits | 44 buildings / unchanged 24 couriers |

`WorldWater.cpp` implements SDL-free read-only queries: `household_has_water`, `nearest_water_source`, their hypothetical-origin variants, `water_covered_households` and `well_coverage_at`. Distance reuses `footprint_distance`: minimum Manhattan distance between any cells of the 2×2 House and 1×1 Well. Origin distance six can therefore mean footprint distance five and valid coverage. The nearest in-range Well wins by distance, then lower stable Building ID. No roads, route searches, original terrain, staffing, fire or desirability enter this calculation. A disconnected or burning House still has water when in range; water supplies neither goods, Service nor fire protection.

Queries scan bounded placed collections, with at most four Wells and twenty Houses. Coverage is never stored, cached in World, transported, consumed or reserved. There is no Water Good, courier, stock, health system or original-map mutation.

Safe typed demolition removes a Well through the ordinary confirmation/command path. Its 60-fund spending remains in demolition history, with no refund; IDs are monotone and never reused. As an empty infrastructure object it owns no stock, recipe, trip or reservation that could block removal. Normal occupancy/topology changes still use the shared command machinery; coverage queries perform no BFS or route refresh.

## Levels, population, tax and presentation

Effective level is the minimum of historical development, the existing desirability cap, and the water cap. A dry House has water cap **0**; a covered House has cap **2**, leaving Poor/Neutral/Good desirability caps 0/1/2 unchanged. Historical development still requires two/five successful demands and is never reset by losing water.

Actual Pottery + Food + active Service + no fire remain the demand requirements. Water is deliberately **not** another demand condition. A supplied dry House earns **25** tax and builds history; covered Houses earn **25/40/60** according to resulting effective level. Actual per-House tax totals remain authoritative. Capacity is still **6/10/16**. Water loss changes level/capacity immediately but does not truncate residents: an over-cap House loses at most one resident at each ordinary 400-tick deadline. Rebuilding a Well immediately restores any historically earned level permitted by desirability, then ordinary supplied demand can resume growth.

The ten-House/eight-effective-Level-2/population-100 goal is unchanged. Water becomes necessary for effective Level 2 through the shared level query, without a new goal field. Existing curated House stages 1512/1516/1520 follow the effective level, using the already loaded textures; a water change performs no decode or upload.

## Starter and spatial planning

The unchanged paid prepared starter spends **1,280 of 1,300** at tick 0, leaving **20**, with four fresh Houses and **24/24** workers. It starts **Water 0/4**, without goods, Service, protection, historical development, free Wells or hidden ticks. First genuine income can buy the 60-fund Well; money is never injected.

Measured acceptance through normal commands:

- Synthetic starter and native Xia starter both earn their first actual taxes at **tick 400**. A first Well can be purchased at **tick 400**.
- Poor placement covers **one** House; safe demolition and a later paid central Well cover **four**. In native Xia the poor Well is at `(112,109)`, the central replacement at `(111,114)` for starter origin `(103,113)`; these are local examples, not universal map placement advice.
- A supplied, historically Level-2 House with Good desirability earns dry **25** versus covered **60** per successful demand. Removing water at population 16 leaves 16 immediately, then 15 at the next deadline; restoring water permits ordinary growth back to 16.
- Two independently supplied synthetic districts with equal quality/history stay separate: covered Good Houses reach Level 2, dry Good Houses stay Level 0. Building a second paid Well immediately unlocks the second district's historical level. Goods/Service still require each district's roads.

## Controls and read-only preview

The existing-key audit found **I** and **U** unused. W remains camera pan; D, F, F1–F9, Space and every historical action retain their existing meanings. Visible toolbar buttons provide **I Well $60** and **U Water**.

Well hover reports covered Houses and how many are currently dry. House hover reports predicted desirability and Water Yes/No, without promising buildability, goods, Service or fire safety. Pointer changes coalesce by logical cell, tool and World topology/building revision. Load explicitly invalidates the preview even when a different branch has the same revision. Projection uses no World copy, commands, BFS, files or assets.

U shades only placed House footprints: cyan covered, amber dry. It is neither a full-map heatmap nor save state. The House inspector shows availability, nearest Well/distance, historical development, desirability cap, water cap and effective level alongside the existing population/supply/fire/tax details. The Well inspector shows ID, 1×1 footprint, radius five and covered count, with demolition but no worker/operation/priority/courier controls.

## Schema 16 and recovery

Schema **16** belongs only to City-v14 rule 1. It inherits schema 15's authority and stores the ordinary Well entity with zero population, goods, reservations, recipe/demand/production, fire and tax fields, and canonical enabled/Normal non-operation defaults. Unknown kinds and Wells in older profiles fail closed. Water availability, nearest source, level/capacity and overlay are absent from the document. Restore rebuilds occupancy and derives water immediately from positions/kinds, validating the normal invariants before publication. There is no automatic migration from City-v13 or any older profile.

Autosave/Recovery reuses the ordinary schema-16 document, atomic write/read-back and validated load paths. No water-specific recovery metadata or World timer is introduced. Recovery resumes paused through the existing session controller; manual save targeting and original data remain separate.

## Well visual and bounded audit

Schema-1 building profiles accept an optional `well` role through the ordinary shared loader. A Well preview must have a supported, unmirrored Emperor Type-30 **side-one, width-78, 3,200-base-byte** layout, including the deduplicated-asset path. Its conventional ground center is **[39, image_height − 20]**. A 2×2 or classic-width asset is rejected rather than altering gameplay geometry. Older profiles remain valid.

The read-only local `China_General.sg3` search visually reviewed **255** strict candidates: 205 from Aesthetic, Government1/2, Guilds and Aesthetic2, plus 50 from StorNDist, Husbandry and Safety. No sufficiently clear freestanding Well/cistern/water-pavilion candidate was selected. Wall/road decoration, Markets, the existing FireWatch record 383 and ambiguous rocky pools 725–728 were excluded. Group labels alone establish no original building identity. Built-in `buildings.json` remains unchanged; atlas pixels and exports stay under ignored `.local/`. See [research log](reverse/research-log.md#city-v14-bounded-well-visual-audit-2026-10-02).

Original-like Well asset remains unresolved. The authored fallback was redesigned for a quieter, period-consistent isometric presentation. It remains independently authored SDL geometry, with no original-pixel sampling, image file, external texture or additional asset registration. Adjacent whole-image House art can still obscure parts through the existing painter, and this is not an original Emperor Well reconstruction.

## Well presentation polish

Starting commit: `570f5f88ad8c047baf9bf7c014591e0df593c8c8`, clean working tree. This follow-up changes presentation only. Well kind/command, cost 60, radius five, four-instance limit, 1×1 footprint, water queries, House levels, taxes, population, desirability, demolition and schema 16 are unchanged. Defaults and original asset bindings are unchanged.

The old ellipse stack, grey rectangular body, bright blue water and axis-aligned timber frame were rejected because they read as a clean technical/debug object. Three temporary, independently drawn variants were compared at **1×/2×/4×** in the same real Xia City-v14 scene, with Houses, Roads, Market, Pottery, Farm, FireWatch and **two paid Wells**:

| Local variant | Scene decision |
| --- | --- |
| StoneRing | Lowest silhouette, but reads as an open basin rather than a clearly recognizable Well |
| TimberWell | Selected: visible opening and thin lifting frame identify a Well without a roof dominating the cell |
| RoofedWell | Rejected: the roof obscures the opening and adds a conspicuous flat canopy above a small structure |

The selected open timber design then received a few fixed chipped stone and wood-grain facets. All temporary selectors and alternate designs were removed from production. Comparison code, scene saves and rendered screenshots remain under ignored `.local/well-polish/`, outside the bundle. The World was constructed with the paid starter, actual supply/tax income and ordinary `PlaceWell` commands: Wells at `(111,114)` / tick 400 and `(111,118)` / tick 800, reviewed at tick 6,800, with zero burning buildings. No funds, history, goods or coverage were fabricated. Camera framing is predefined QA, not a human construction playthrough.

`WellFallbackRenderer` holds one compile-time mesh: **178 vertices / 94 triangles**, one untextured `SDL_RenderGeometry` submission, fixed stack vertices transformed per draw, shared constant indices, no heap-owned geometry or per-frame shape construction. Non-finite coordinates/zoom, non-positive zoom and transforms that could overflow floats fail before submission. The helper restores the caller's draw blend mode and leaves draw color unchanged. No World or asset layer is a dependency.

| Palette element | RGB |
| --- | --- |
| Stone dark / mid / light | `(103,98,78)` / `(133,124,100)` / `(157,146,119)` |
| Mortar / inner opening | `(87,81,63)` / `(51,49,38)` |
| Small interior water | `(55,66,65)` |
| Wood dark / mid / light | `(76,57,38)` / `(102,77,48)` / `(120,93,60)` |
| Rope / subtle ground contact | `(132,116,81)` / `(78,76,53)` |

The low, slightly irregular octagonal rim replaces the cylinder. The supports lean/offset slightly and the narrow beam follows an isometric axis. The opening contains a much smaller dark grey-green water facet; no cyan halo or large axis-aligned base is drawn. Maximum visual bounds relative to the **unchanged** projected `building_visual_ground` are **x [-21,21], y [-31,10]** logical pixels, scaled by zoom. The ground contact remains inside the 80×40 diamond; the top of the lifting beam is elevation above that footprint, not another occupied cell. 1× remains recognizable, 2× exposes the masonry joints, and 4× retains the same bounded faceted shape and small water surface. It remains a geometric fallback alongside detailed original sprites, not a pixel-perfect stylistic match.

A valid I-tool placement now enters the existing building painter even when no original Well role is configured. It calls the same helper with **alpha 128/255** and the ordinary valid one-cell outline. Invalid placement retains the existing red diagnostic. Configured original Well roles still use the ordinary sprite path. U affects House overlays only; Well geometry/colors stay independent. Selection retains the existing white one-cell diamond and no additional halo or normal technical label. The existing post-painter House diagnosis pass is preserved: a neighboring House footprint overlay can cross raised timber, just as it can cross other elevated sprites; it does not assign a coverage tint to the Well. Stone and interior-water pixels in the selected native review remain unchanged with U.

`well-fallback-pixels` renders independent synthetic terrain in an SDL software surface. It checks visible non-terrain pixels, bounded image and ground footprint, smaller/desaturated water, alpha-128 preview against opaque output, 1×/2×/4×, caller blend state and rejection of NaN/Inf/overflow/null renderer without pixels. `sandbox-city-v14-view` additionally proves a visible unplaced Well preview and **100 paused frames** with actual camera offset changes through anchored wheel events, all three zooms, U, Well selection and placement. Its entire authoritative snapshot is unchanged; World copy/restore/execute, tick, BFS/route refresh, decode/upload and file read/write counters stay zero. The existing texture count also stays unchanged.

Follow-up validation (2026-10-02):

| Check | Well polish result |
| --- | --- |
| Debug full CTest | **65/65 passed**, 322.53 seconds |
| Release full CTest | **65/65 passed**, 28.29 seconds |
| ASan/UBSan full CTest | **65/65 passed**, 84.61 seconds; `ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1` |
| Separate package Release CTest | **65/65 passed**, 15.14 seconds |
| City-v14 / building visuals / SandboxView / road responsiveness / desirability / fire / autosave | Passed in all full suites |
| Pixel coverage at 1× / 2× / 4× | 912 / 3,675 / 14,621 non-terrain pixels; water 40 / 161 / 644, stone 493 / 2,070 / 8,416; finite and bounded |
| 100-frame view stress | Exact snapshot, no simulated ticks, zero prohibited counters and unchanged textures |
| Real Xia final native renderer | SDL Metal target readback at 1×/2×/4×, U, selection and valid placement `(111,117)`; snapshot unchanged at tick 6,800 |
| `file build/openemperor` / packaged executable | **Mach-O 64-bit executable arm64** |
| `tools/package_macos.sh` | Recursive arm64/dependency checks, inside-out ad-hoc signing, relocated/unzipped execution and all 13 negative fixtures passed; original files unchanged |

The package is `dist/OpenEmperor.app` plus `dist/OpenEmperor-0.1.0-alpha.2-570f5f88ad8c-macos-arm64.zip`, marked **dirty / local test candidate**, not published. Compiler builds have no errors or warnings. The two expected staging `install_name_tool` signature-invalidation notices are resolved by subsequent signing and verification. QA code, scene saves, comparison images and original data are absent from the bundle.

Native final frames use the production view with a Metal render target so readback occurs before Present, then display that target in the window. They use predefined camera/selection/hover events, not human input. Native UI automation in this follow-up failed to start (`Sky Computer Use native pipe startup failed`); no human playtest is claimed. The existing long manual City-v14 gameplay acceptance remains open. The three-design scene comparison and this bounded presentation review do not reopen or replace the 255-candidate original-asset audit.

## Build and test

From the repository root, using your own existing extracted game files:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --parallel
ctest --test-dir build --output-on-failure --parallel 6
file build/openemperor
./build/openemperor --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v14 --sandbox-demo \
  --sandbox-save "$PWD/.local/saves/city-v14.json"
```

The data path must point at actual game files, not the installer or a placeholder. Save outside that original-data root. The menu also offers **City v14 - Water and residential infrastructure**; selecting it is explicit and does not change the default profile.

Six new CTest cases cover base rules, schema-16 persistence/recovery, independent districts, 20,000-tick determinism, 100,000-tick endurance, and the SDL view. Independent synthetic SG3/.555 fixtures prove visible stage changes, cache invalidation and profile compatibility without proprietary bytes. Normal paid commands construct the World scenarios. The endurance city reaches **44 buildings / 24 couriers**, repeats twenty paid Well relocations and save/autosave/recovery roundtrips, relocates empty industry and checks all invariants. Pure-query/preview/100-overlay-toggle tests preserve exact snapshots and zero World copy/restore/execute, BFS/refresh and asset/file counters. Existing road responsiveness and old-profile tests remain in the full suite.

## Validation on 2026-10-02

| Check | Final result |
| --- | --- |
| Debug full CTest | **64/64 passed**, 539.86 seconds |
| Release full CTest | **64/64 passed**, 27.86 seconds |
| ASan/UBSan full CTest | **64/64 passed**, 190.33 seconds |
| 20,000-tick determinism | Passed in all three configurations |
| 100,000-tick endurance / 44 buildings / 24 couriers | Passed in all three configurations |
| Schema 16, demolition, fire, desirability, autosave/recovery | Passed |
| Water queries / 100 overlay toggles / 200 same-cell motions | Exact snapshots, zero prohibited expensive counters; coalesced preview |
| Existing road responsiveness / old profiles | Passed in full suites |
| macOS app package | Release CTest, recursive arm64 Mach-O/dependency verification, ad-hoc signing, relocated/unzipped execution and all negative fixtures passed |
| Additional packaged City-v14 Xia check | Actual first tax tick 400 / total 75, four dry Houses, exact direct and continued save/resume equality |
| Native fallback 1×/2×/4× | Separate 1×1 structure and ground alignment reviewed; original-like asset remains unresolved |
| Human mouse-driven 20–30 minute playtest | **Open**, coordinate automation blocked |

Builds use Apple clang/C++20 and macOS arm64. Release and sanitizer configurations use the existing local SDL3 setup; sanitizer options are `OPENEMPEROR_ENABLE_SANITIZERS=ON`, `ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`. Final builds contain no compiler errors or warnings. `file build/openemperor` reports **Mach-O 64-bit executable arm64**.

`tools/package_macos.sh` created `dist/OpenEmperor.app` and `dist/OpenEmperor-0.1.0-alpha.2-1c5782affaf6-macos-arm64.zip`. These are **dirty-workspace local test candidates**, not a release. The package is ad-hoc signed, not Developer ID signed/notarized, and this host's bundled dependencies require macOS 26.0. No original files, decoded images, personal settings/saves or QA tools enter the bundle. The package report remains under ignored `dist/`; original-file smoke checks confirm data unchanged.

The staging `install_name_tool` emits the expected two notices that dependency-path fixups invalidate the copied SDL/crypto signatures. Inside-out signing subsequently replaces those signatures; recursive, relocated and post-smoke signature verification all pass. These are packaging-stage notices, not unresolved compiler warnings.

Native keyboard-controlled Xia QA ran approximately 18:21–18:45 UTC, with paused frame review and explicit ordinary tick stepping. Starter first tax and first paid Well purchase were both at tick 400; central replacement at tick 800 covered all four. A second paid starter district at `(120,121)` was built at tick 6,800. At tick 12,800 all four new Houses had genuine fulfilled demands and paid 25 per success while dry; their Good-quality House retained historical Level 2 and population six. A second paid Well at `(128,122)` changed water 4/8→8/8 and the effective-Level-2 count 1→2 at the **same tick**, without population injection. Paused paid Pottery near the first residential quarter reduced its Good House level while water stayed 8/8; safe empty demolition restored it without refund. F5/F9/F5 reproduced the entire schema-16 document exactly at tick 12,800; a new QA process loaded that same World.

Normal operation commands then paused both Watches. At tick 20,000 seven buildings were genuinely burning, including Houses #7/#8 recording fire-blocked demand misses, while water remained 8/8 and both Wells retained zero fire fields. This verifies independent systems; it is not a claim that water extinguishes fire. No treasury, goods, population, demand history or coverage was fabricated. The initial QA run exposed a Well inspector heading using “House”; the final source corrects the heading and the final SDL suite covers its dedicated information/control path. The limited QA does not replace human pointer-placement, perception and replanning acceptance.

## Native acceptance boundary

A local, ignored keyboard-controlled QA runner uses the real Xia map, shared production renderer, ordinary paid commands, normal demolition confirmation and F5/F9. Camera positions and pointer events are predefined by that runner; stepping T/N advances real simulation ticks, not fabricated snapshots. Native keyboard input and frame review work. Direct macOS coordinate clicks fail with `noWindowsAvailable`, so the requested full **20–30 minute human mouse-driven construction/replanning playtest remains open**. Elapsed QA time or screenshots are not counted as that playtest.

For that remaining playtest, start the CLI command above, run until actual income, build a poorly placed Well, compare coverage, safely demolish and rebuild centrally, create a second supplied district, test nearby industry, fire, save/load and U. Confirm that the House inspector explains missing water independently from Service, placement changes coverage, restored water recovers historical levels, and removing a Well does not prevent genuine Level-0 tax income.
