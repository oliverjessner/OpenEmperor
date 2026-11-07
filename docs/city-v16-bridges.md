# City-v16 Bridges V1

The separate [OE53 authored source-cell experiment](rendering/oe53-terrain-integration.md)
uses explicit Rule20/policy3/schema36. It retains historical RampLink/Bridge
contracts and keeps native233–244 prototype art and raw-ramp permissions separate.

Baseline: clean HEAD `41af9a171d9ce02e3fe7ebb95e0d70c18ec4c649`.
The explicit versions are **City-v16 rule 5 / MapPermissions policy 3 / save
schema 21**. Rule5 inherits rule 4 economics, production, staffing, demand, Service
targeting, movement, Water, Health, Fire and maintenance. Defaults remain
City-v16 rule 4 and general City-v11 rule 3. Old saves retain their versions;
there is no migration or automatic copy upgrade.

## Construction and shared graph

An owned `BridgeSpan` has an independent typed `BridgeId`, canonical increasing
storage-X/Y banks, ordered interior cells, signed deck height, actual paid cost
and placement tick. V1 admits **1..8 water cells**, at most **64 spans**, with no
overlap or shared banks. Both banks require existing normal owned/original land
roads and equal signed height. Price is **40 + 20 per water cell**, hence 60..200,
versus road 2 and starting Funds 1300. Bridges have **zero upkeep**, workers, goods,
production and tax. Variable price belongs to the complete span validator.

Policy3 retains policy 2 original roads and adds only immutable `bridge_water`:
inside the unchanged Diamond, exact raw terrain **0x4**, objects **0**, and outside
every complete original square, marker, gate and Ferry claim. Mixed/special/
road-water forms, including `0x04000004`, remain excluded. Eligibility is **not
transport**. Raw water and its signed height remain unchanged after construction;
deck level is the explicit equal-bank fact.

`PlaceBridge` validates the whole canonical span before publication. Failed
commands leave funds, sequence, revision, spans, routes and reservations unchanged.
Success buys once, increments road revision once and refreshes existing caches
once. Rule5-only derived cell/bank indexes provide O(1) lookup, never serialized.
`road_cell` remains a normal land road; `transport_cell`/`transport_source` also
recognize the owned deck. The shared World edge predicate admits longitudinal
deck/bank edges only. Building entrances use normal bank roads; lateral water
entry, mid-deck intersections and direct building attachment are forbidden.
Existing BFS, movement, return, reroute, restore and invariants share that graph.
There is no second navigator or per-tick bridge search.

**Whole-span removal is unavailable in V1.** Road removal cannot delete a deck
cell. Ordinary bank-road removal retains occupied/begun-edge protection, genuine
waiting with cargo/reservations and normal paid repair. Reserved banks cannot
become buildings after disconnection. A legitimate disconnected span is saveable.

## Controls and bounded presentation

New Sandbox offers **Enable Bridges V1 (rule 5)** only for City-v16. Direct CLI:
`--sandbox-rules sandbox-city-v16 --sandbox-bridges`. Existing rule 4 cities keep rule 4.
**B Bridge** drags between existing opposite bank roads; MouseUp revalidates the
actual endpoint and publishes one complete command. Green is legal/affordable,
amber valid geometry with insufficient funds, red a named blocker. Preview price
is the authority's actual price. Preview coalesces endpoints/revision/sequence/
funds, with no World copies, commands, ticks, BFS, routes or asset/file work.
When B is armed, a cyan outline identifies a possible starting road; red indicates
that the hovered cell cannot be a starting bank. This cue does not accept a span;
the complete validation controls the green/amber/red drag and payment.
Escape/right-click/focus/layout/resize/UI release cancel. **F3 retains B's existing
diagnostic light control.**

The bridge is **OpenEmperor-authored timber geometry**, not identified Emperor
artwork: individual weathered planks with grain, shaded beam faces, supporting
pilings and diagonal knees, small capped posts, handrails and half-bank end
sections. Raised decorative bank cubes have been removed. The fixed twelve
axis/end/component templates are prepared before the frame loop, rasterized on
an 80×80 local grid with bounded edge coverage and compressed into colored row
runs. Shared nearest screen boundaries prevent cracks at fractional zoom.
Each bounded component submits one geometry batch; the same prepared runs supply
drawing and picking without textures, material generation or rasterization in a frame.
Deck/back rail and front rail are bounded local
painter components; the front rail follows an actor at the same cell key, without
global bridge-last/elevation sorting. Feet and diagnostic markers share deck
ground. Cached actually drawn geometry supplies visual inspection. F8 hides the
whole structural body in nonstructural comparisons.

Only rule 5's initial/reset view centers its actual first owned span, or its owned
settlement before a span exists, at readable 1x. Ordinary projection/centering is
used; no map-name/coordinate/DPI correction or saved camera. Older camera behavior
is unchanged. Bridge rendering uses **zero textures**, no per-frame decode/upload/
readback/files/navigation. Inspector shows ID, axis, length, banks, signed deck,
paid cost, connectivity and removal limit. Eligible unbuilt water says **blocked
now**, never original traversable bridge. Explicit F1/G captures span provenance
along the authoritative route; cached inspection performs no route scan/BFS.

## Real original-map evidence

Candidate search preferred Xia, Handan and MP34, then other complete maps. It
retains concrete original geometry/protection/budget rejection reasons. The
selected **Cities/Chang-an Sui.map** crossing is storage **(106,155) → (108,155)**,
water **(107,155)**, deck **0**, cost **60**. Coordinates are report output, never
production selection conditions. Normal paid commands build two Households and
Clay on bankA, Pottery on bankB, then normal supply infrastructure. A replay of
the complete paid districts without the bridge proves no alternate actual road
connection. No snapshot goods/workers/cargo/progress or treasury injection.

| Checkpoint | Actual result |
| --- | --- |
| Before bridge | tick 0, Funds 836, separate paid networks |
| Full initial supply district before dispatch | tick 0, Funds 80 |
| Loaded Clay on deck | tick 56, cargo 1/reservation 1 |
| Deposit | tick 71, Clay enters Pottery and reservation releases |
| Return on deck | tick 86, cargo/reservation 0 |
| First completed recipe | tick 285 |
| Main long play | 12,000 ticks; Funds 2295, taxes 4075, upkeep 1580, population 36 |

Main on-deck occupancy is recorded separately from completed trips:

| Courier role | Outbound deck ticks | Returning deck ticks |
| --- | ---: | ---: |
| Clay | 730 | 730 |
| Service | 595 | 595 |
| MarketFoodInbound | 695 | 695 |
| MarketPotteryDistributor | 630 | 630 |
| MarketFoodDistributor | 620 | 620 |
| FireInspector | 425 | 425 |
| HealthWorker | 365 | 361 |

Every on-deck vertex/progress tick is recorded. Main real roles are Clay,
MarketFoodInbound, both Market distributors, Service, FireInspector and HealthWorker,
with outbound and returning crossings. Food uses the inherited market roles;
legacy Food role4 is not backported. A separate paid continuation purchases a
second Warehouse on bankA for real Pottery and MarketPotteryInbound route/movement
controls; it reaches tick 14400 with Funds 2487. These two roles also cross outbound
and return. On-deck tick counts are not claimed to be trip counts, starvation
freedom or Emperor reproduction.

Bank-road removal with loaded Clay on deck yields a real wait: tick 12085 retains
cargo 2/reservation 2 at the same vertex. Paid repair resumes delivery/return;
tick 12585 reaches Funds 2384. No teleport, shortened edge, cargo loss or collision
model is introduced.

## Persistence, regression and evidence labels

Schema21 alone stores canonical spans/next ID with existing city authority.
Policy3 reconstructs from actual map/manager/heights and binds canonical versioned
SHA-256, original map and building-mask identities. Derived indexes/masks, camera,
graphics and inspection never become save authority. Restore validates complete
geometry, IDs, eligibility/deck, nonoverlap, costs/accounting and active paths.

Ten checkpoints cover pre-build, post-build/pre-dispatch, loaded deck, actual
deposit, return, tick 10000, tick 12000, disconnected wait, repair and the additional
role continuation. Each has full
immediate SaveDocument **byte equality in a new OS process**, and exact continued
equality after 100 ticks. Seventeen malformed JSON cases reject schema/rule/policy/
map/hash/mask swaps, invalid cells/axis/length/deck/IDs/cost/extra fields/future tick
and counter exhaustion. Scripted F5/F9 exercises actual view handlers, distinct
from native keyboard delivery.

Production-view 100 frames under dummy/software count **zero World copies/restores/
commands/ticks, BFS, route refresh, asset decodes, texture uploads, file reads/writes**,
with 1709 stable live background textures. BMP capture/readback follows outside
that measurement. Authored production pixel tests cover both axes, signed deck,
tints/alpha, drawing/picking and 1/2/4/fractional zoom. Scripted SDL tests cover
cancellation, UI release, F3/B, fallback-marker alignment, selection, layers and
F5/F9. Fresh complete census remains 167/165/2 and all 990 policy1/2 actual/zero/one
canonical/fingerprint summaries match the frozen baseline.

All **14 historical Xia SaveDocuments** remain byte-identical. Current rule 4
Xia/Handan each retain **eight byte-identical SaveDocuments**, first goal at
**7200/6800** and another **10,000 ticks** with no goal loss. Their gameplay summaries
are identical apart from measured execution time. The immutable transport-edge
predicate and gate-index construction are byte-identical source; together with
the complete permission comparisons this establishes old edge decisions without
claiming a new native input or rendering measurement.

Native preparation used Cocoa/Metal; automation stopped after desktop-focus
takeover as AGENTS requires. **Native bridge construction and human acceptance
remain a short manual check**. No coordinate workaround or native PASS is inferred
from direct-render/scripted evidence. Original bridge identity/traversal, special
water, curves/intersections, ramps, shipping/Ferries, safe span removal and general
interpenetrating original artwork remain outside V1.

## Final validation

The final source, including the explicit menu-choice/load/default test, passed
eight focused Bridge/menu/recovery checks followed by the complete existing suite
in each build configuration. No old negative was removed and no timeout increased.

| Configuration | Complete result | Wall time |
| --- | --- | ---: |
| Debug | 150/150 PASS | 617.07 s |
| Release | 150/150 PASS | 80.48 s |
| ASan + UBSan, RelWithDebInfo | 150/150 PASS; no sanitizer findings | 331.71 s |

The final default starter also passed. Its ignored report root is
`.local/city-v16-bridges/run-2ZTSHj`; source SHA-256 is
`3dcad992f80f3c42e2e14deb262e8b2b9cd656e14a674e8069af088a634ace70`.
Original inventory and source remained unchanged throughout that run. Frozen
baseline comparisons are recorded separately in the ignored
`.local/city-v16-bridges/baseline-city-compatibility.json` and
`edge-predicate-compatibility.json`. The build logs have no compiler warnings.
Native Metal preparation is separate from the software pixel review and does
not close the manual construction/zoom/resize/Save-Load check above.

## Graphics and player construction follow-up

The 2026-10-14 follow-up keeps all rule, policy, save, transport and price authority
unchanged. The actual original-map B/MouseDown/MouseUp path builds from the genuine
pre-construction save: **Funds 836 → 776**, cost 60, and full World equality with
the normally paid command from that same starting state. F5 preserves it; actual
loaded Clay and returning crossings follow. Its 100 preview frames perform no
World copies/commands/ticks, navigation, files, asset decodes or texture uploads.
The ignored current starter report is `.local/city-v16-bridges/run-ZTZBgC`, including
`gameplay/user-build-check.json` and reviewed 1×/4× software captures.

Current Release passes **150/150**; the eleven affected Bridge, input, original-road,
save, composition and ground-occlusion tests pass in **Debug and ASan/UBSan** with
no sanitizer findings. The complete starter retains 167/165/2, identical before/
after policy summaries, original bytes and ten process-restart continuations.
Oliver confirmed the walker crossing in his previous opened test session:
**User-reported walker PASS**, limited to that crossing. The new appearance and
manual B build are separate human checks; scripted success does not infer them.

## Visual rejection and revised material

Oliver rejected the previous smooth, blocky appearance on 2026-10-14. Its technical
PASS does not establish visual acceptance. The replacement darkens and details
the timber, reduces post density to land thresholds/water-cell centres, relocates
supports under the water-cell centres, narrows the deck edge and removes the cubic
bank pads. The authoritative deck, projection, bank endpoints, World and saves
remain unchanged; no decorative geometry establishes transport or height.

Both axes, 1×/2×/4×/fractional zoom, preview tints, visible supports, material color
variation, caller blend restoration and shared picking pass production pixel tests.
The revised actual original-map build/cargo/return/F5/100-pure-frame checks pass;
private reviewed captures are in `.local/city-v16-bridges/pixel-bridge-final`.
Current Release passes 150/150, with eight affected Debug and ASan/UBSan tests
passing separately. The new native appearance still awaits Oliver's review;
these checks are not an original-Emperor artwork or human-style PASS.

## Test starter

```sh
sh tools/test_city_v16_bridges.sh .local/gog-extracted/app
sh tools/test_city_v16_bridges.sh .local/gog-extracted/app --ui
sh tools/test_city_v16_bridges.sh .local/gog-extracted/app --ui-build
```

Default `--all` builds current Release, verifies original/source identity, runs
bridge tests, full before/after corpus, water/sprite-metadata audit, paid gameplay,
JSON negatives, process restarts and historical/current city controls. `--audit`
omits original gameplay; `--gameplay` omits old city playthroughs; `--ui` directly
loads the paid outbound save in the normal current Release app after diagnosis.
It opens paused at tick 56 with rule 5/policy 3/schema 21, the bridge centered and
a loaded Clay courier on its deck. It does not depend on navigating the menu or
starting a new city, whose default deliberately remains rule 4. Generated reports, settings,
saves and original-data captures stay ignored in `.local/city-v16-bridges/run-*`;
personal histories and original inputs are unchanged. No publication or asset
distribution.

Manual check: run `--ui`, confirm **Bridges 1 / Policy 3 / Save 21**, then click
**Continue** and watch Clay cross/return; zoom/pan/resize, select the bridge, F5/F9.
The CLI load uses only that isolated test save, without personal preferences or
recovery histories. `L` opens the save list only from the main menu; it has no
load-list action in the playing view. **`--ui-build` loads the pre-construction
save directly**, with existing paid bank roads and enough funds for the 60-fund
span. Press B or click B Bridge, drag between the roads on opposite sides of the
small stream, and release the green preview. The paid bridge is immediately
visible; Continue starts the actual Clay delivery. F5/F9 preserves a user-built
span. The same empty-span checkpoint is also printed as `01-build-bridge.json`.
Prebuilt returning and running-city saves also remain in
the isolated root. The direct CLI view exits with Escape.

## Explicit Rule6 inheritance

[Authored land Ramp V1](city-v16-ramps.md) inherits Bridge V1 under explicit rule6/policy3/schema22. Rule5/schema21 prices, spans, heights, IDs, rendering and saves remain exact. Ramp edges join normal roads before/after a bridge, never water/deck/side edges or unequal banks. Original bridge/slope import remains unresolved.
