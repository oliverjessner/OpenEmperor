# Food and Market walker presentation

The subsequent [Walker ground/cargo correction](rendering/road-ground-occlusion.md)
starts from clean `22962777dc2df4dc1cffb8312361f0a14f16c400`. It independently
reproduces adjoining Road writes over opaque legs and the separate diagnostic
cargo rectangle. Productive unified Sandbox Roads now draw once before the
unchanged spatial painter. Original sprites show that extra rectangle and
its hit only in F1; actual Inspector cargo and fallback/F2 diagnostics remain.
All families, native/reflected pixels, foot anchors, transport state and saves
stay exact. F7 remains the historical ordering comparison. The completion
evidence below describes the earlier figure-activation milestone; current
pixel, ordinary-app, performance and suite evidence are recorded in the
linked correction report. The subsequent [Service pass](service-walker-visuals.md) supplies its own schema-5 figure; HealthWorker remains unmapped.

The approved completion starts from clean HEAD **`238741f0b5839e8213e62e03e6b49da241afafe0`**. It adds explicit horizontal display transforms and automatic original Supplier/Distributor activation to the existing Food/Market presentation. The earlier preparation starts from `40eb5da99ac4f8cb2967e60034e62ad5d8fd4cb0`, before the five transport roles had visual mappings. Their existing transports already operate; this task supplies figures without changing a trip or its goods.

Oliver reports successful supply, tax receipts and visible House development, with **203 Funds, 525 received taxes and 35 residents** in his latest screenshot. This is a successful user-reported observation of that city, not a loaded complete save, an independently viewed attachment in this request or proof that all historical transport questions are resolved. His current city is not taken over or overwritten for acceptance.

## Configured families and exact logical roles

| Existing CourierRole | Existing owner → target | Visual family |
| --- | --- | --- |
| `MarketFoodInbound` | Farm → Market | Supplier |
| `MarketPotteryInbound` | Warehouse → Market | Supplier |
| `MarketFoodDistribution` | Market → House | Distributor |
| `MarketPotteryDistribution` | Market → House | Distributor |
| `Food` | Farm → House in its existing legacy profiles | Supplier |

The shared families are chosen by the actual role, never CourierId, BuildingId or collection order. Inbound does not make the Market the owner. Existing Clay/Pottery/Household/FireInspector selections and all simulation enum values remain exact. Food and Pottery receive no artificial recoloring; actual cargo and target remain in the existing diagnostics.

The supplier uses original SprMain2 physical walk base **3605**, a human pushing an empty wheelbarrow. The distributor uses base **5585**, a different human with a back basket. Both have twelve ordered phases. These are original pixels and coherent walking sequences with **curated OpenEmperor** role assignments, feet and timing. Original profession, goods and gameplay registration are unproved. One neutral coherent clip serves real outbound and empty returning trips; no extra loaded/empty action is invented. See [the complete asset and four-direction audit](reverse/market-walker-visual-audit.md).

## Approved four-direction display contract

The current completion pass starts from clean HEAD **`238741f0b5839e8213e62e03e6b49da241afafe0`**. The FINISH FOOD / MARKET WALKERS request explicitly approves the bounded schema-4 horizontal display transform. Earlier preparation at `40eb5da…` remains historical evidence; the approval is now implemented. The original opposite columns are zero-payload mirror-reference records and remain rejected by the shared decoder.

Schema 4 permits an optional boolean `flip_x` only for Supplier/Distributor. Absence means false. Unknown schema-4 frame fields, including misspelled transforms, vertical flip and rotation, reject activation explicitly. Schemas 1–3 retain their exact historical parsing and unflipped drawing. Every selected AssetId is a normally decoded native record. Stored `foot_anchor` already belongs to the displayed image: horizontal reflection uses continuous `W − ax`, while cached-alpha picking reflects the discrete source column with `W − 1 − floor(x)`. Neither the parser nor renderer transforms the stored anchor again. The live hit descriptor carries the same flag; F3 uses the same owner/draw call, reports native versus `flip_x`, and invalidates live inspection until closed and redrawn.

The native path still uses `SDL_RenderTexture`. The flipped path uses `SDL_RenderTextureRotated` with angle zero and `SDL_FLIP_HORIZONTAL` on the same eager texture. No flipped RGBA copy, second texture or per-frame upload is created.
For each family, twelve phases use native base `+0+8p` for `neg_y` and base `+2+8p` for `pos_x`; reflected displays of those same sources provide `neg_x` and `pos_y`. Storage directions are checked against the current projection, not a copied four-column convention. Private full four-by-twelve comparisons show changing gait and stable ground registration, and were viewed by root. They are asset research, not normal Application/main acceptance.

Two ticks per frame is the authored cadence, selected solely from World tick under the existing clip rule. Pause freezes it; simulation speed does not multiply it a second time. Actual position, current path edge, protected begun edge, turns and return determine the existing pose. Interrupted trips stay at their actual position with a static waiting pose; they do not walk in place.

## Live visibility and rendering boundaries

The four Market transport roles must hide `IdleAtWorkshop` consistently before live sprite/marker draw, counters and hit registration. Actual outbound, empty returning and interrupted trips remain visible even if the owner later pauses or loses staff. Cargo zero is not a visibility guard. F2 cannot bring back a hidden idle marker, while F3 may still show an idle frame as a pure asset preview. Legacy direct Food retains its existing idle visibility contract. The corrected FireInspector rule remains separate and exact.

The shared walker painter, ground point and cached-alpha hit path handle each figure. Transparent pixels do not block clicks, foreground buildings/gate roofs retain ordinary occlusion, and a sprite is not also drawn as its large fallback square. UI input boundaries, flames, House stages, building/terrain footprints, scene composition and zoom policy remain unchanged. Original alpha-free flagged shadow composition and the exact runtime/backend `TextureCompatibility` rule are reused.

Preparation is independently atomic for the Market supplement and the existing Inspector supplement. Missing/bad Market files or exhausted aggregate budgets retain a named marker without disabling core walkers, Inspector, flames or a valid save. Removing one supplement retains another supplement's existing textures and shared core images without reuploads. A Fire candidate is decoded and uploaded before any optional walker eviction: missing/malformed metadata or SDL allocation/upload failure preserves the prepared walker supplements even when their original source files are subsequently unavailable. Genuine pressure retains the existing Fire priority, removing Market before Inspector as necessary. Exactly zero remaining headroom at a legal aggregate 64 MiB is distinct from an overflow. A strict replacement's allowance excludes the old walker set rather than adding old bytes to a saturated remainder. An explicit custom Walker profile remains the sole override and receives no hidden automatic additions. Schemas 1–3 and unsupported SG3 mirror-record validation retain their current semantics.

The automatically selected shared families add **96 aliases, 48 native physical assets and 536,512 deduplicated RGBA bytes**. With existing core and Inspector the measured total is **192 aliases, 128 images and 1,043,380 bytes**, beneath unchanged global Walker bounds; remaining aggregate session headroom still governs real activation. Each native texture is prepared/uploaded once and shared across roles/couriers. A display flip adds neither a reflected CPU image nor a second texture. The separately hashed SprMain2 pair must be checked independently of the old dependencies. Bundles contain metadata only, never original or decoded pixels.

Rendering may read the bounded existing Courier state, choose a prepared pose, submit its visible figure and create the ordinary hit descriptor. It adds no files, decodes, uploads, readback, World copies, commands, ticks, BFS, route refreshes or full map scans. Supply, recipes, stocks, reservations, conservation, staffing, movement, arrival, demand/tax/population, Funds, purchase warnings, maintenance, map permissions, gates, rules, defaults and save/recovery authority remain exact.

## Acceptance status and test start

| Evidence | Current status |
| --- | --- |
| Existing successful player city | User-reported working supply/tax/House evolution, 203/525/35; no loaded save or broader transport diagnosis |
| Original native pixels and coherent phases | Both public families selected automatically and independently decoded:96 aliases/48 native images/536,512 bytes; core+Inspector total192/128/1,043,380 |
| Four-direction gait/feet comparison | Existing viewed asset audit retained; native sources and explicit opposite display anchors are now packaged metadata |
| Explicit display-flip scope | Approved by this completion request and implemented; schemas1–3 and mirror-record rejection unchanged |
| Optional resource lifecycle and aggregate budgets | Release software/dummy CTest PASS (1/1, 0.36 s), hidden actual Metal PASS; authored direct production calls, full snapshots/all 12 render counters checked, both optional texture owners zero at shutdown |
| Profile/custom/pose/production-picking regressions | Current native/reflected authored software and hidden actual Metal View tests pass: both families × four directions and distinct gait, first/last columns, transparent holes, off-center feet, unequal canvases, fractional zoom, clipping, F3/stale hits, all real trips/empty returns, F2/idle, pause, under-staffing, single-cell wait, begun edge, save/load/paid repair and old City7 direct Food |
| Frozen preparation Release | 121/121 pass, no skips, compiler warnings, sanitizer diagnostics or source changes; 34.14 s tests / 63.20 s total. This inventory does not establish automatic original Market activation (`preparation-release-report.json`) |
| Archived production economy/control | All 1,601 canonical complete World rows and 14 full SaveDocuments match the ordinary paid-Xia helper against old HEAD. Simultaneous distribution with real tax at tick 407; at tick 1600 Funds 203, cumulative tax 375, upkeep 192, population 24. These are separate from Oliver’s reported 203/525/35 city |
| Scripted normal original-Xia Food/Pottery deliveries, simultaneous distributors and empty returns | PASS: actual Metal before the final zero-Food HUD-row guard, and final-source dummy/software Application/main. All four actual roles have positive original bodies loaded outbound and empty returning; both families cover all four directions, including simultaneous distributors. 1,601 full canonical control comparisons and F5/F9 checkpoints are exact; zero unexplained Food/Market fallback |
| Curve and supported gate | PASS in scripted ordinary-main Metal Kaifeng: 500 ticks, 506 complete control comparisons, 236 legal fixed-gate edge observations; Supplier/Distributor loaded and empty bodies, normal roof occlusion and full saves retained. These are observations of one supported gate fixture, not a map corpus |
| Waiting save/load/paid repair | Final-source dummy/software ordinary main PASS: remove road at 32, finish begun edge and wait at 36, F5/F9, twelve ticks with byte-identical stationary body, normal paid repair at 48, delivery/return by 148; 124 complete control comparisons and both full saves exact |
| Legacy direct Food | Final-source dummy/software ordinary main City7 rule 1/schema 7 PASS: Farm→House loaded and empty return, all four directions with positive original Supplier pixels, ordinary legacy idle visible; 607 complete controls, tick 400/600 F5 and F9 exact |
| Full final Debug/Release/standard ASan/UBSan | Each 122/122 PASS on the same 335 source hashes; no skips, compiler warnings, sanitizer diagnostics or source changes. Tests 817.72/91.81/284.71 seconds respectively; total 846.36/150.64/396.34 seconds |
| Final private macOS package | Package Release 122/122 PASS (88.48 seconds). Strict signatures and all seven metadata files pass; relocated and unzipped headless City16r3 from unrelated CWD activate both families/all four directions with 181 draws and 0 fallback. All 13 historical and 12 new Market negative guards pass |
| Versioned delivery-city starter | Exact final command PASS with fresh private root `player-Xia-R7xIRR`, working save 407 and ordinary-main dummy/software menu startup. Private environment-priority bridge and passive SDL observers are recorded separately; this starter smoke check does not claim a loaded graphical playthrough |
| Human native input / original profession parity | Open |

The completed original-figure ordinary-app/control comparisons use identical paid commands and ticks, full canonical states and SaveDocuments, including paths/arrival ticks, stocks/reservations, demand/taxes, population and Funds. Control Worlds retain complete immutable MapPermissions. Current Xia and road-cut runs retain 128 Walker textures and 1,298 total eager uploads; legacy retains 96 and 1,216, while Kaifeng retains 128 and 1,553. Upload counts stay constant through each run and all owned textures reach zero at shutdown. Direct target tests, scripted Application/main and human inputs remain distinct.

`tests/MarketWalkerResourceTests.cpp` checks both supplement append/removal orders, exact foreign texture identities, injected SDL create/upload failures, missing sources after successful preparation, real Fire replacement pressure and a historically over-budget schema-2 replacement boundary. The exact-budget fixture decodes/uploads actual independently authored buffers rather than substituting a claimed byte count. Its production renders leave the complete World unchanged and all twelve performance counters zero. Private build/backend results and measured scope are recorded in `.local/market-walkers/resource-report.json`; this does not establish original Market pixel, normal Application/main or human acceptance.

The existing isolated empty-Xia command remains valid:

```sh
sh tools/test_gate_passages.sh .local/gog-extracted/app Xia
```

The tracked delivery-city starter is:

```sh
sh tools/test_market_walkers.sh .local/gog-extracted/app
```

Choose **Load Sandbox → Load selected save**, zoom over the city and press **Space**. The helper selects its own paused `working-supply.json` at ordinary tick 407 with actual tax and both distributors travelling. It constructs the normal paid 1,280/1,300 starter with 24/24 workers, then runs ordinary ticks: no free goods, workforce, money or modified gameplay. Each run owns a fresh private app root. The final exact command creates `.local/market-walkers/player-Xia-R7xIRR`; its settings select that root’s paused save. The command’s final audit verifies all 1,601 complete World rows and 14 full SaveDocuments against the ordinary control. Existing valid saves need no migration or new city. All research and technical acceptance remain private under ignored `.local/market-walkers/` and `.local/food-market-walkers/`; no commit, push, tag, release or publication is performed. Service was outside this historical milestone and is now implemented in its [separate pass](service-walker-visuals.md); HealthWorker remains open.

Historical preparation evidence is indexed by `.local/market-walkers/authored-view-report.json`, `development-neutrality-report.json`, `preparation-release-report.json`, `resource-report.json` and `.local/food-market-walkers/integration-audit.json`. The read-only integration audit found no concrete defect. That earlier audit found all 360 protected tracked files equal to its preparation baseline 40eb5da, including 76 simulation/navigation/persistence/map authority sources; the final 238741f0 authority audit below independently checks 366 protected files. The earlier preparation reports retain their historical scope. The approved completion evidence below supersedes their absent-resource/decision notes; a previous 121-test Release preparation is not evidence for the newly integrated original activation.


## Completion diagnostics and evidence

Oliver reports a figure with a stick, another human and three remaining markers. Their roles are not established by appearance or count; his full save is unavailable, and no exact personal-city reproduction is claimed. F1 plus a selected live-walker Inspector now identify CourierId, logical role, actual owner/target, phase/cargo/good, current edge/storage direction, expected family, profile source and selected native record/flip or named fallback. The pure bounded `walker_diagnostics()` query also exposes last submitted body bounds, whether it is a sprite or marker, and hidden/culling separation for a private technical export. No permanent logger or production frame readback is added.

Logical roles, live submitted instances and configured families are separate counts. Hidden Market Idle and offscreen instances are not positive figure evidence. Service and HealthWorker were unassigned outside this pass; current Service status is recorded in its separate schema-5 pass, while HealthWorker remains unassigned. F2 deliberately forces marker comparison. Other named causes are an unconfigured optional profile, incompatible fingerprints, missing/invalid metadata, absent clips/directions, preparation failure or exhausted budgets. No unassigned role is disguised as Supplier.

The public `market-walkers.json` resource and `optional_market_walkers` registration pin both SprMain2 dependencies separately. CLI sibling resources, the normal fixture/starter and macOS bundle use the same metadata; an explicit Custom Walker profile still suppresses supplementation. Repository/package resources contain seven metadata JSON files and no original pixels.

The bounded Food/Market figure gap is complete. The final private evidence index is `.local/market-walkers-finish/final-validation-report.json`:

- `.local/market-walkers/finish/actual-main-report.json` and `EVIDENCE.txt` retain exact source/binary provenance for earlier actual Metal Xia/Kaifeng and final-source scripted dummy/software Xia, road-cut and legacy replays. The only intervening production edit gates the extra zero-Food F1 count row; the earlier Metal hashes are not rewritten. Root viewed all final contact sheets and the earlier Metal/gate sheets. Positive bodies are checked against actual native-frame pixels, independently of idle/culling counts.
- `.local/market-walkers-finish/economy-neutrality.json` verifies all 1,601 complete helper states and 14 full SaveDocuments against archived production 40eb5da commands. The final exact starter independently matches that current control.
- `final-debug-report.json`, `final-release-report.json`, `final-sanitizer-report.json` and `final-matrix-summary.json` record 122/122 each, with all 335 source hashes unchanged/current. ASan/UBSan use the project’s normal sanitizer configuration. The earlier 121-test preparation and superseded failed intermediate runs remain historical records.
- `.local/market-walkers/flip-audit/report.json` and `resource-audit.json` retain direct authored software/Metal pixels, baseline negative proof and fresh independent original-source/resource measurements. All twelve render performance counters stay zero; no production readback is added.
- `package-dist/package-report.json`, `package-extra-report.json` and `pipeline/package-guard-report.json` record strict signatures, seven metadata files, unrelated-CWD relocation/ZIP activation, complete control/resume and 13 existing plus 12 new negative guards. The private local dirty-workspace ZIP is 4,379,868 bytes, SHA256 `86674e4e6aa8a1bee87e1aedbcd29e03939b8f28282bda78bffbc03af8331100`; it is a local test candidate with no original assets or publication.
- `exact-starter-report.json` records the exact tracked `sh tools/test_market_walkers.sh .local/gog-extracted/app` workflow. To keep it nonvisible after takeover, a private Homebrew POSIX shell preserves a fixture-only SDL same-dummy hint-priority override, and passive observers confirm actual SDL 3.4.14 dummy/software startup. These private audit hooks do not edit the tracked starter, resources or the normal Application/main source and loop. The earlier external-environment and private observer failures are archived as harness failures, not activation or gameplay failures. This verifies fixture generation and menu startup; loaded-city graphics are established by the separate ordinary-main replays.
- `authority-final.json` records unchanged HEAD 238741f0, 366 protected tracked files, all 1,464 original files/819,711,091 bytes and the unchanged system SDL 3.4.14 hash. The shared decoder, World/navigation/persistence/map authority, old manifest sections and SDL installation remain exact.

The native road-cut attempt stopped immediately when real input arrived, before a Sandbox render; that process/window was left untouched. All remaining application/packaging/starter checks used isolated nonvisible dummy/software processes. The completed headless road-cut evidence closes the scripted application case, without retroactively claiming a native road-cut PASS. Direct authored pixels, scripted instrumented Application/main, actual Metal output, headless package resolution and human input retain distinct provenance. Original profession/pivot/timing parity and a new human playthrough remain open. Oliver’s personal screenshot/save is not reconstructed, and HealthWorker, optional Service fallback and deliberate F2 markers remain separately named.
