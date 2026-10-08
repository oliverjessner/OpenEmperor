# Service walker presentation

**IMPLEMENTED** for the supported, independently fingerprinted original-data configuration. The ordinary application automatically draws an animated, empty-handed civilian for the existing Service Courier. Its normal departure, House arrival, coverage and return are observed through the real Application/main path. HealthWorker remains an unassigned marker and a separate task.

This pass starts from clean HEAD **`6039912c2dd6e53ac71d74f058b3e32f1e5e9f50`**. At that baseline, `walker_visual_role(CourierRole::Service)` returns `std::nullopt`; rebuilding cannot supply a missing mapping or clip. The new visual role is append-only **Service = 6**, after Clay 0, Pottery 1, Household 2, FireInspector 3, Supplier 4 and Distributor 5. Simulation CourierRole values remain exact.

## Original pixels and curated role

The figure is the purple-robed, empty-handed civilian walk family at physical SprMain base **1417**. It is distinct from the Inspector's 433 family and the Supplier/Distributor's 3605/5585 families. Original pixels and the twelve-phase walking sequence are independently checked. Its original profession is unproved: **curated OpenEmperor Service presentation** describes the assignment, foot points and cadence, rather than an original Emperor Service registration. See [the bounded asset audit](reverse/service-walker-visual-audit.md).

For phase `p = 0…11`, the metadata specifies:

| Storage direction | Physical native source | Display |
| --- | --- | --- |
| `neg_y` | `DATA/SprMain.sg3`, `1417 + 8p` | Native |
| `pos_x` | `DATA/SprMain.sg3`, `1419 + 8p` | Native |
| `neg_x` | The same `1417 + 8p` | Explicit `flip_x: true` |
| `pos_y` | The same `1419 + 8p` | Explicit `flip_x: true` |

Each direction has twelve distinct gait images. The opposite original records are mirror references and remain rejected by the SG3 decoder. The approved display transform uses the existing horizontal-only SDL path on the same native texture; it creates no reflected RGBA image or additional texture. Displayed continuous foot X is `W − ax`, while reflected cached-alpha columns use `W − 1 − i`. Explicit per-frame foot points use metadata X and metadata Y minus eight as a chosen common ground reference. Different canvas sizes retain that ground point; this is not a recovered original pivot.

The existing pose rule chooses `clip[(world_tick / 2) % 12]`. Two World ticks per frame and the global phase origin are authored. Pause freezes the pose; 1×/2×/4× follow ordinary simulated ticks without a second speed multiplier. Direction comes from the actual current path edge. A waiting trip uses a static first frame facing its retained edge, or its idle frame without an edge.

## Optional activation and fallback

Schema **5** adds only the optional `service` role to the existing format. Schemas 1–4 retain their historical parsing, role limits, native/flip behavior and assets. Schema-5 Service requires a bounded clip ID and complete four-direction clips with at least two different prepared native images per direction. Unknown strict frame fields, malformed transforms, unsupported mirror records, invisible/incomplete clips and unsafe paths reject activation.

The eighth bundled metadata JSON, `service-walker.json`, is selected through `optional_service_walkers` after independent SHA-256 verification of its SprMain SG3/555 pair and the existing complete core identity. Automatic preparation uses the existing Service-profile capability (City-v8 through City-v16), while older profiles retain their existing asset counts. It is prepared separately from core, Inspector, Market and flames. A failed Service candidate retains those already prepared roles and names its Service marker fallback. No partial Service clip is published; missing source data, bad metadata, fingerprint mismatch, decode/upload failure and insufficient shared headroom are concrete reasons. Session/save authority stays valid.

An explicit custom Walker profile remains the sole override: no automatic Service, Market or Inspector supplement is added. A valid schema-5 custom Service uses its own complete clip. F2 deliberately displays markers; it is not evidence of a missing build.

Service adds **48 aliases, 24 native physical images/textures and 160,668 logical RGBA bytes**. The complete core/Inspector/Market/Service profile measures **240 aliases, 152 images and 1,204,048 bytes**. These are logical decoded/texture costs, not measured GPU memory. Global limits remain 256 aliases, 256 physical assets and 64 MiB; the separate aggregate 64 MiB session budget also remains exact. Genuine Fire replacement pressure can remove Service first, then existing Market and Inspector supplements. A Fire candidate must fully decode/upload before eviction; a failed replacement preserves all prior texture identities, including when their source files are now unavailable.

## Live figure and unchanged gameplay

One central visibility rule hides Service at `IdleAtWorkshop` before sprite, F2/fallback, hit and counter registration. Actual outbound, returning and interrupted trips stay visible at their authoritative position. Cargo, stock, coverage expiry, prior tax, staffing and an operation pause do not determine visibility. A stale selected hidden Service is cleared; no hidden live hit remains. F3 may still inspect the prepared idle frame independently.

Service enters the existing corrected Road-ground and spatial painter with the same position, feet, keys and alpha picking. Roads stay below the figure; actual foreground Houses and gate roofs retain occlusion. There is no second marker, always-on-top path or Service Cargo rectangle. F1's existing Courier Inspector reports role, Owner, Target, Phase, family/source and actual clip/record/flip. CLI Walker reports include separate Service activation, fallback, aliases, extra resources and draws.

Only the existing actual House arrival grants 1,200 ticks of Service coverage. The renderer creates no trip, goods, reservation, worker, Funds, tax or coverage. World, commands, dispatch/targeting, routes/gates, timing, economy/maintenance, House demand/population, rules/policies/defaults and Save/Recovery formats are unchanged.

## Technical evidence

The ordinary paid Xia city costs **1,280 of 1,300 Funds**, has **24/24 workers**, a staffed Service Post **2/2**, and four reachable Houses. These are normal paid commands and ordinary ticks, explicitly labelled as a technical fixture. Courier 7 from Post 6 is already travelling to House 7 at **tick 6**, with zero Service coverage. Actual arrival at **tick 15** grants deadline **1215** and starts Returning; it reaches home at **tick 30** and hides. The same sequence occurs in the archived baseline and the current normal application.

All **1,601 complete canonical World rows** for ticks 0–1600 and **eight full SaveDocuments** from the new fixture equal archived HEAD byte for byte. At tick 1600 the unchanged ordinary city has Funds 203, taxes 375, upkeep 192 and population 24; Service cargo and reservations remain zero. The independent scripted Application/main comparison observes 1,602 complete equal states, covering every tick 6–1600, and four actual F5 SaveDocuments with matching F9 state and resumed pose. Positive original-body witnesses cover all four directions, outbound/return, multiple gait phases and native/reflected display. This is instrumented dummy/software ordinary-app evidence, not native human input. A separate actual Load Sandbox → Load selected save → Space run advances through the ordinary frame clock, pauses at tick 82, and performs F5/F9: 84 observed full states (64 distinct ticks) equal the archived controls, and the complete tick-82 save equals a separately generated old-production document. The fixed-tick run additionally checks 1,566 selected tick/phase/record/flip poses. A separate final-source ordinary-main road-cut run matches 124 complete controls: cut at tick 66, finish the begun edge and wait at 70, F5/F9, twelve stationary ticks, paid repair at 82 and resumed return through 182. Both actual full saves match the archived control; the waiting body is stationary and all render counters remain zero.

Focused authored production tests cover strict profile versions and mapping, different gait images, native/reflected alpha and transparent holes, unequal canvases/common feet, clipping/fractional zoom, 1×/2×/4×, pause, F2/hidden home/stale selection, actual trips, interrupted begun edges, static wait, paid repair, save/load and actual arrival. Resource tests exercise all six supplement orders, shared physical dedupe, foreign texture identity retention, unavailable sources, injected create/upload failures, exact alias/headroom bounds, atomic rollback and custom overrides. Existing Road-ground, foreground, Market, Inspector and Fire regressions remain registered.

Independent original-map direct production checks pass on software and actual hidden Metal: Kaifeng gate 1 at signed height zero and Zhengzhou gate 5 at height one, using ordinary paid House/Post/Road commands (188 spent, 1,112 Funds, 2/2 staffing). Departure is tick 1, actual arrival 40 and home 80; together they exercise all four gate directions. Service-body omission and structural-layer comparison establish 4,235 software / 4,223 Metal colored body pixels correctly covered by original gate foreground, with 1,409 / 1,413 visible opening pixels. Separate actual paid House comparisons find 1,311 software / 1,319 Metal colored body pixels covered by the foreground House, with 6,650 / 6,642 visible body pixels. F1 is closed for every final pixel capture. Covered clicks preserve foreground selection; visible opening/body clicks select Service. Complete per-tick snapshots and schema-19 SaveDocuments equal controls, with zero frame counters and zero owned textures after shutdown. These are direct production targets, separate from normal Application/main and human input.

Per rendered frame, all twelve existing performance-counter deltas are zero, including file reads/bytes, decodes/uploads, World copies/commands/ticks, BFS/cache and route refreshes. Ordinary Xia retains **152 Walker textures and 1,322 eager uploads** through the run, without per-frame uploads; all owned textures reach zero at shutdown. Hidden actual Metal resource/direct-pixel checks are reported separately from the dummy/software ordinary application. Research captures, full controls and runners remain ignored under `.local/service-walker/`; original pixels are not packaged or tracked.

Final Debug, Release and standard project ASan/UBSan each pass all **127 registered tests** on the same **343 frozen source/test/resource/tool/build files**, with no warnings, skips, sanitizer diagnostics or source changes.

| Final configuration | Tests | Test wall time | Configure/build/test total |
| --- | --- | ---: | ---: |
| Debug | 127/127 PASS | 702.15 s | 775.86 s |
| Release | 127/127 PASS | 89.52 s | 195.15 s |
| RelWithDebInfo + project ASan/UBSan | 127/127 PASS | 193.20 s | 401.04 s |

The sanitizer run uses `OPENEMPEROR_ENABLE_SANITIZERS=ON`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, matching the existing project suite. The 123 historical tests remain and four new Service targets cover profile/pose/render, real production View, independent optional resource lifecycle and compatibility fingerprints. Existing bounded Road-ground/foreground/Market/Inspector/Fire tests pass unchanged apart from the role-count, mapping, visibility and next-unknown-schema expectations in three historical test files updated for the append-only role.

A final rehash confirms all 1,464 original files (819,711,091 bytes), system SDL, simulation/navigation/map/persistence and renderer source files are unchanged. The current source hashes equal all three final matrix snapshots and the final normal-app/starter checks. Consolidated ignored evidence is `.local/service-walker/final-validation-report.json`, with `actual-main-report.json`, `assets-evidence-summary.json`, `pipeline/focused-report.json` and `gate-house-{software,metal}/report.json` retaining their distinct provenance. Human playthrough, original profession/timing/pivot parity and complete Emperor composition remain open. No commit, push, tag, release or publication is performed.

## Test the current code

From the repository root:

```sh
sh tools/test_service_walker.sh .local/gog-extracted/app
```

The exact versioned command passes using fresh private root `player-Xia-DMm8mi` on final sources: both actual fixture and Application/main menu observe SDL 3.4.14 dummy/software. That smoke check uses a private passive hint-priority bridge and injects no events or gameplay. The separate scripted normal-main runs above prove loading, Service visits and save/resume; the starter smoke alone does not claim a human graphical playthrough.

The script builds the current Release code and fixture helper, creates a fresh `.local/service-walker/player-Xia-XXXXXX` app root, and starts the normal application using the user's original data. Existing player settings, saves and recovery are not changed. Choose **Load Sandbox → Load selected save → Space**. The selected save is paused during the real tick-6 Service trip; zoom over the Service Post and Houses to observe the visit, coverage at arrival and return. Known supported data select the original/curated clip automatically without a manually supplied manifest. F1 shows the clip or named fallback; F2 compares markers.
