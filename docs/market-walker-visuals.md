# Food and Market walker presentation

This bounded presentation follow-up starts from clean HEAD **`40eb5da99ac4f8cb2967e60034e62ad5d8fd4cb0`**. At that baseline `WalkerPose` has no visual mapping for `MarketFoodInbound`, `MarketPotteryInbound`, `MarketFoodDistribution`, `MarketPotteryDistribution` or legacy `Food`. Their existing transports already operate; this task supplies figures without changing a trip or its goods.

Oliver reports successful supply, tax receipts and visible House development, with **203 Funds, 525 received taxes and 35 residents** in his latest screenshot. This is a successful user-reported observation of that city, not a loaded complete save, an independently viewed attachment in this request or proof that all historical transport questions are resolved. His current city is not taken over or overwritten for acceptance.

## Proposed families and exact logical roles

| Existing CourierRole | Existing owner → target | Proposed visual family |
| --- | --- | --- |
| `MarketFoodInbound` | Farm → Market | Supplier |
| `MarketPotteryInbound` | Warehouse → Market | Supplier |
| `MarketFoodDistribution` | Market → House | Distributor |
| `MarketPotteryDistribution` | Market → House | Distributor |
| `Food` | Farm → House in its existing legacy profiles | Supplier |

The shared families are chosen by the actual role, never CourierId, BuildingId or collection order. Inbound does not make the Market the owner. Existing Clay/Pottery/Household/FireInspector selections and all simulation enum values remain exact. Food and Pottery receive no artificial recoloring; actual cargo and target remain in the existing diagnostics.

The supplier uses original SprMain2 physical walk base **3605**, a human pushing an empty wheelbarrow. The distributor uses base **5585**, a different human with a back basket. Both have twelve ordered phases. These are original pixels and coherent walking sequences with **curated OpenEmperor** role assignments, feet and timing. Original profession, goods and gameplay registration are unproved. One neutral coherent clip serves real outbound and empty returning trips; no extra loaded/empty action is invented. See [the complete asset and four-direction audit](reverse/market-walker-visual-audit.md).

## Four-direction proposal and scope status

The viewed native columns face upper-right/lower-right; the original opposite columns contain mirror references with zero payload. The current shared decoder and schemas 1–3 do not support those mirror records. The bounded proposal is a **schema-4 explicit horizontal display flip of the validated native source AssetId**, with explicitly reflected feet and the same reflected alpha picking. It does not decode an SG3 mirror record or infer missing pixels. This small new display capability is **pending explicit scope approval**; no public Market metadata resource or automatic activation is asserted yet.

For each family, twelve phases use native base `+0+8p` for `neg_y` and base `+2+8p` for `pos_x`; reflected displays of those same sources provide `neg_x` and `pos_y`. Storage directions are checked against the current projection, not a copied four-column convention. Private full four-by-twelve comparisons show changing gait and stable ground registration, and were viewed by root. They are asset research, not normal Application/main acceptance.

Two ticks per frame is the proposed authored cadence, selected solely from World tick under the existing clip rule. Pause freezes it; simulation speed does not multiply it a second time. Actual position, current path edge, protected begun edge, turns and return determine the existing pose. Interrupted trips stay at their actual position with a static waiting pose; they do not walk in place.

## Live visibility and rendering boundaries

The four Market transport roles must hide `IdleAtWorkshop` consistently before live sprite/marker draw, counters and hit registration. Actual outbound, empty returning and interrupted trips remain visible even if the owner later pauses or loses staff. Cargo zero is not a visibility guard. F2 cannot bring back a hidden idle marker, while F3 may still show an idle frame as a pure asset preview. Legacy direct Food retains its existing idle visibility contract. The corrected FireInspector rule remains separate and exact.

The shared walker painter, ground point and cached-alpha hit path handle each figure. Transparent pixels do not block clicks, foreground buildings/gate roofs retain ordinary occlusion, and a sprite is not also drawn as its large fallback square. UI input boundaries, flames, House stages, building/terrain footprints, scene composition and zoom policy remain unchanged. Original alpha-free flagged shadow composition and the exact runtime/backend `TextureCompatibility` rule are reused.

Preparation is independently atomic for the Market supplement and the existing Inspector supplement. Missing/bad Market files or exhausted aggregate budgets retain a named marker without disabling core walkers, Inspector, flames or a valid save. Removing one supplement retains another supplement's existing textures and shared core images without reuploads. A Fire candidate is decoded and uploaded before any optional walker eviction: missing/malformed metadata or SDL allocation/upload failure preserves the prepared walker supplements even when their original source files are subsequently unavailable. Genuine pressure retains the existing Fire priority, removing Market before Inspector as necessary. Exactly zero remaining headroom at a legal aggregate 64 MiB is distinct from an overflow. A strict replacement's allowance excludes the old walker set rather than adding old bytes to a saturated remainder. An explicit custom Walker profile remains the sole override and receives no hidden automatic additions. Schemas 1–3 and unsupported SG3 mirror-record validation retain their current semantics.

The proposed shared families add **96 aliases, 48 native physical assets and 536,512 deduplicated RGBA bytes**. With existing core and Inspector the measured total is **192 aliases, 128 images and 1,043,380 bytes**, beneath unchanged global Walker bounds; remaining aggregate session headroom still governs real activation. Each native texture is prepared/uploaded once and shared across roles/couriers. A display flip adds neither a reflected CPU image nor a second texture. The separately hashed SprMain2 pair must be checked independently of the old dependencies. Bundles contain metadata only, never original or decoded pixels.

Rendering may read the bounded existing Courier state, choose a prepared pose, submit its visible figure and create the ordinary hit descriptor. It adds no files, decodes, uploads, readback, World copies, commands, ticks, BFS, route refreshes or full map scans. Supply, recipes, stocks, reservations, conservation, staffing, movement, arrival, demand/tax/population, Funds, purchase warnings, maintenance, map permissions, gates, rules, defaults and save/recovery authority remain exact.

## Acceptance status and test start

| Evidence | Current status |
| --- | --- |
| Existing successful player city | User-reported working supply/tax/House evolution, 203/525/35; no loaded save or broader transport diagnosis |
| Original native pixels and coherent phases | Identified and decoded; two separately fingerprinted SprMain2 families |
| Private four-direction gait/feet comparison | Viewed; native-source/reflected-display proposal with exact original alias/anchor relations |
| New explicit display-flip scope | Pending user decision |
| Optional resource lifecycle and aggregate budgets | Release software/dummy CTest PASS (1/1, 0.36 s), hidden actual Metal PASS; authored direct production calls, full snapshots/all 12 render counters checked, both optional texture owners zero at shutdown |
| Profile/custom/pose/production-picking regressions | Completed for the current unmirrored preparation. Authored software and hidden actual Metal View tests cover both families × four directions and gait, all real trips/empty returns, F2/idle, alpha picking, zoom/pause, under-staffing, single-cell wait, begun edge, save/load/paid repair, cached-file deletion and old City7 direct Food |
| Frozen preparation Release | 121/121 pass, no skips, compiler warnings, sanitizer diagnostics or source changes; 34.14 s tests / 63.20 s total. This inventory does not establish automatic original Market activation (`preparation-release-report.json`) |
| Archived production economy/control | All 1,601 canonical complete World rows and 14 full SaveDocuments match the ordinary paid-Xia helper against old HEAD. Simultaneous distribution with real tax at tick 407; at tick 1600 Funds 203, cumulative tax 375, upkeep 192, population 24. These are separate from Oliver’s reported 203/525/35 city |
| Scripted normal original-Xia Food/Pottery deliveries, simultaneous distributors and empty returns | Pending; no atlas-only closeout |
| Curve/gate, waiting save/load/repair and legacy Food | Pending |
| Full Debug/Release/standard ASan/UBSan and package resolution | Pending |
| New versioned delivery-city starter | Tracked helper/script implemented and helper executed in a fresh private root; ordinary original-figure app/script acceptance pending |
| Human native input / original profession parity | Open |

The remaining original-figure normal-app/control comparison must use identical paid commands and ticks and compare full snapshots/SaveDocuments, concrete paths/arrival ticks, every affected stock/reservation, actual demand/taxes, population and Funds. Control Worlds retain complete immutable MapPermissions. Resource counts, extra submissions, constant eager uploads and cleanup are measured separately from simulation equality. Direct target tests, scripted Application/main and human inputs remain distinct.

`tests/MarketWalkerResourceTests.cpp` checks both supplement append/removal orders, exact foreign texture identities, injected SDL create/upload failures, missing sources after successful preparation, real Fire replacement pressure and a historically over-budget schema-2 replacement boundary. The exact-budget fixture decodes/uploads actual independently authored buffers rather than substituting a claimed byte count. Its production renders leave the complete World unchanged and all twelve performance counters zero. Private build/backend results and measured scope are recorded in `.local/market-walkers/resource-report.json`; this does not establish original Market pixel, normal Application/main or human acceptance.

The existing isolated empty-Xia command remains valid:

```sh
sh tools/test_gate_passages.sh .local/gog-extracted/app Xia
```

A new delivery-city starter is documented only after its tracked implementation and exact command are executed. Existing valid saves need no migration or new city. All research and technical acceptance remain private under ignored `.local/market-walkers/` and `.local/food-market-walkers/`; no commit, push, tag, release or publication is performed. Service and HealthWorker figures remain separate tasks.

Current preparation evidence is indexed by `.local/market-walkers/authored-view-report.json`, `development-neutrality-report.json`, `preparation-release-report.json`, `resource-report.json` and `.local/food-market-walkers/integration-audit.json`. The read-only integration audit found no concrete defect. All 360 tracked files outside the explicit presentation changes match HEAD, including 76 simulation/navigation/persistence/map authority sources. The automatic original resource and display flip are absent pending the requested scope decision; Debug/ASan/UBSan, normal original Application/main, curve/gate and local package/starter completion remain open.
