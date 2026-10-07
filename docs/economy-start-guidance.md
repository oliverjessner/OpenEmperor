# Income and startup budget guidance

This bounded read-only follow-up starts from **`5bc44eec1122b982d3677987787431a593b9ecbd`**, with a clean worktree. It improves the existing `CityStartGuidance`, purchase warning and SandboxView information paths. It changes no economy, World/save authority, rule/map-policy version, sprite/scene rendering or asset authority, or starter defaults. HUD layout and the new income text presentation are intentionally improved.

## Reported start and current facts

The request describes Clay Source, Pottery, Warehouse, Market and three Houses, plus a Farm with 0/4 assigned workers and no Service Post. The reported population/workforce is 16, current personnel demand 22, full-starter estimate 24, Funds approximately 26 and installed upkeep 46/400 ticks. A Service Post costs 100, so 26 Funds leaves a gap of 74 for that building alone, before roads or the separate staffing estimate.

No complete save or command history accompanies the request. These are user-described observations, not an independently reconstructed World. A valid test city must identify its own exact numbers and differences. Missing Food/Market walker artwork remains a separate presentation issue.

The independently established paid Xia control starts at storage origin `(106,113)`: three Houses, Clay Source #4, Pottery #5, Warehouse #6, Market #7, Fire Watch #8, rule-3 2×2 Well #9, then Farm #10, plus 72 paid roads. At tick zero it spends 1,274 and retains 26 Funds. Its population/workforce is **18, not the reported 16**; assigned workers are 18, active/installed demand 22, and the full-staff plan including missing Service needs 24. Farm #10 has 0/4, installed upkeep is 46/400 ticks, and Service Post costs 100 with a 74 gap. Its known Watch/Well/road history is authored test setup, not inferred screenshot history. These values were verified with ordinary commands against the unchanged baseline simulation and the final normal-app run.

A separate paid purchase control has three Houses, population 18, assigned 16, installed demand 20 and upkeep 40/400 ticks. With 102 paid roads it retains 106 Funds. Buying a fourth House for 80 leaves 26: the still-missing Service Post alone then has a 74 gap, and that purchased House eliminates the plan's additional-House estimate. This is a separate warning/control fixture, not a second reconstruction of Oliver's city.

## Infrastructure, operation and household supply

The three supply paths remain Clay → Pottery → Warehouse → Market → House, Farm → Market → House, and Service Post → House. A Market produces neither Food nor Service. The presence of all six starter facilities is an infrastructure finding; `complete_supply_chain` is not evidence that their operations are staffed, their routes are usable or any House has received its goods.

Operation findings use actual building IDs, assigned/required workers, player operation state, fire and existing dispatch/route-cache status. Ready dispatch, buffered goods or an active delivery is not a promised arrival. Incoming reservations remain distinct from stock already in the House.

The House finding uses the same demand conditions as real simulation: actual Pottery stock, profile-required Food, active Service, and absence of actual burning/sickness. At the normal 400-tick demand deadline a successful demand alone pays tax; a failed demand consumes neither ware and pays none. The report includes current supply, remaining Service duration, the next demand interval, the last actual demand result and actual contributed taxes. **Next demand in N ticks is not tax promised in N ticks.** Stocks, expiry, deliveries and incidents can change before then.

Missing Service Post is labelled missing infrastructure for ongoing supply. Delivered Service can survive its source until normal expiry, and Houses may retain goods; an absent source does not retroactively remove those facts. Conversely, an installed Post grants no coverage until actual arrival. Water and desirability affect effective level and supplied tax amount; a dry fully supplied House can still pay Level-0 tax. Unprotected fire/health status alone is not a failed-demand blocker.

Lifetime city and House taxes are history. A past payment neither establishes current coverage nor guarantees a profitable city. The continuing income diagnosis remains available after the first tax, while the existing purchase-confirmation exemption for already-paying cities remains unchanged.

## Workers and construction reserve

Available means current resident workforce; Assigned means workers allocated by the existing complete-requirement priority/ID ordering. Active demand excludes operations currently excluded by pause/fire; Installed demand includes their installed requirements. No automatic priorities, staffing optimizer or population changes are introduced.

The full-staff starter estimate completes the six-facility plan and compares its personnel requirement with current population. Additional Houses use the existing estimate of six initial residents each. The estimate is labelled for simultaneous full staffing, not a proof that these Houses are the minimum possible economic solution. Existing buffers, operation controls, prioritization and subsequent population changes may permit other sequences. The complete calculated count remains visible even when it exceeds remaining House slots.

Construction reserve is split into real costs of missing facilities and the separately labelled fresh-House estimate. Unknown future roads are excluded. Already bought Houses contribute their actual residents without being charged twice; a bought facility is removed from the missing list. Costs prove neither buildable space nor road reachability. Upkeep is separate from this reserve; A+B is not guaranteed enough money until first tax.

The existing validated purchase warning gives the price, Funds afterward, named still-missing infrastructure and its cost/gap, then the House estimate and excluded roads. Cancel is the default and leaves the World unchanged. Build anyway is an explicit decision that revalidates and executes one ordinary purchase. Insufficient current Funds remains normal command rejection. Upkeep advice alone does not become a new mandatory confirmation or a hard purchase prohibition. The paid complete starter's 20 remaining Funds is not labelled hopeless.

## Strictly future upkeep

City-v16 retains the existing ownership rates and independent placement-relative 400-tick cycles. Paused, unstaffed and burning buildings still pay; demolition stops later bills without a refund. Debt grants no money, interest or automatic shutdown, and normal paid construction still requires sufficient Funds.

The bounded arithmetic projection reports installed upkeep, the next strictly future bill, spending for current buildings over the next complete interval, and Funds after that spending. Its assumption is **unchanged buildings, no new income or purchases**. It is neither a first-tax forecast nor a proved bankruptcy date.

The interval is `(current_tick, current_tick + horizon]`. An exact billing tick's already-paid charge is excluded even though the unchanged `maintenance_due_in()` may report zero there. Buildings placed now first pay after their normal interval. Earlier profiles report no invented maintenance. Checked tick, amount and signed-Funds arithmetic must identify an unrepresentable boundary rather than wrap or publish a fabricated number.

## UI and read-only boundaries

The normal interface has a compact Funds/tax/upkeep summary with current House blockers and a concrete staffing/cost finding. **Income (T)** opens the existing information panel's income mode; ordinary selection or toggling returns to building inspection. Labels remain English. Long startup prose is removed from the crowded lower status line and does not replace every selected building's inspector. Wrapping, scrolling and the existing overlay input boundaries retain readable essential lines in the final ordinary-window checks below.

Diagnosis scans only the bounded existing building/courier collections and consumes existing read-only route caches. No hypothetical ticks, BFS, cache mutation, World copies/commands, file/asset work or added forecast World occurs during display or road preview. Concrete building-purchase preflight retains its existing controlled hypothetical, sharing full immutable MapPermissions for rule 3 and using the historical mask path for legacy Worlds. Cancel/failure cannot partly publish a purchase; confirmation revalidates the current state. Command, tick and load changes derive a fresh finding.

## Validation and test start

Implementation, bounded scripted normal-app acceptance and all three complete validation matrices are complete. User observations, authored tests, direct production UI/render checks, scripted normal Application and human input remain separate categories.

| Current evidence | Status |
| --- | --- |
| Shared demand/current-guidance tests | Pass against ordinary actual tax/consumption: retained stock/Service after safe Post removal pays 25 at the next real deadline; later expired coverage remains diagnosed despite historical tax, with startup-modal exemption retained. Healthy/burning/sick combinations and exact next-tick fire/sickness/Service expiry match production demand |
| Upkeep and read-only tests | Placement phases, tick 399, just-paid due-zero, debt, demolition, planned first bill, INT64 bounds and late horizon checks pass. Current House/facility queries and road-cost projection run 100 iterations with all twelve performance counters zero; existing rule-3 controls retain full MapPermissions (`pipeline-tests.log`) |
| Adjacent legacy/maintenance checks | Private Debug 6/6 pass in 74.38 seconds: guidance, City-v6 economy, City-v7 Food, City-v8 Service, City-v16 base/balance (`pipeline-focused-report.json`) |
| Development production UI checks | Economy/gate/maintenance/input focused 4/4 pass; these development runs are not the final complete matrices |
| Current Release | Final 116/116 pass, tests 60.18 seconds/total 106.54 seconds, no skips, compiler warnings or source changes (`release-report.json`) |
| Standard current RelWithDebInfo ASan+UBSan | Final 116/116 pass, tests 180.80 seconds/total 260.30 seconds, no compiler warnings, skips, sanitizer diagnostics or source changes (`sanitizer-report.json`) |
| Current Debug | Final 116/116 pass, tests 718.58 seconds/total 743.86 seconds, no failures, skips, compiler warnings or source changes (`debug-report.json`) |
| Scripted normal Xia header and purchase path | Actual Application/main Metal at 1280×800 shows the valid failed-start header, including Farm #10 0/4, Funds 26/upkeep 46 and Service cost 100/gap 74. At 1100×700 the critical warning shows purchase 80 → Funds 26, Service 100/gap 74, +0 estimated Houses and construction reserve 100, with roads/upkeep separated. Default Enter cancellation and explicit mouse-confirmed exactly-one purchase retain expected complete SaveDocuments; T/select returns preserve the ordinary Inspector |
| Actual paid starter and process restart | The normal-main prepared run matches the frozen old production over ticks 0–800, all 800 observed HUD states and the complete tick-800 F5 SaveDocument. Actual tax/upkeep/Funds are 75/48/47 at 400 and 175/96/99 at 800. A separate dummy/software normal OS process loads the tick-800 save, derives current guidance and retains an equal complete F5 document |
| Income-body pixel readability | Corrected final ordinary-main cases pass: critical/prepared Metal at 1100×700, failed-start Metal at 1280×800 and the separate software restart show readable left-aligned wrapped bodies. Root reviewed the current contacts and the restart upkeep/estimate view. The original clipped body images are superseded. The bounded fix removes only the redundant new clip while retaining vertical row bounds and horizontal wrapping/width limits; an independent direct production glyph oracle fails the old object at 0/330 and passes the corrected object at 330/330 (`IncomeClipBaselineTest.log`/`IncomeClipFixedTest.log`) |
| Human native input | Not run; scripted PollEvent delivery and private captures do not establish human/native-device acceptance |

Focused simulation/UI evidence remains under `.local/economy-start-guidance/`; the final ordinary original-Xia report/index is `.local/economy-start/actual-main-report.json`/`evidence-index.json`. Current contact sheets are `income-body-final-contact.png`, `critical-main-final-contact.png` and `prepared-income-main-final-contact.png`. Private observers capture the unchanged normal default target before Present and deliver explicit SDL structs through PollEvent; no production observer/readback or World injection is introduced. All four completed current cases keep their 1,250 initial uploads unchanged and report zero live textures after shutdown, with no native takeover. Normal-window pixels, direct target tests and human input retain distinct labels.

The taller HUD exposed a framing problem in the existing 4× fire-pixel fixture: all 76 independent opaque witnesses were above the new map viewport. Its test-only normal wheel zoom now centers on the independently calculated attachment and matches 76/76 witnesses. Production sprite/scene offsets, triangle/fallback checks, all zooms and color assertions are retained.

The final integrity audit (`authority-audit-final.json`) confirms 356 nonapproved tracked files match HEAD, including 187 protected source files and six compatibility resources. Five entity/snapshot structures and existing rates, tax, billing and balance contracts remain exact. World demand changes only factor the shared boolean finding and add pure queries/checked projection guards. All 1,464 original files/819,711,091 bytes and the SDL 3.4.14 library remain unchanged.

Required bounded checks include the user-like depleted start; installed but undelivered supply; absent Post with active House coverage/stocks; actual staffing distribution; current blockers after historic tax; healthy/burning/sick demand equivalence; cancel/exactly-one confirmed purchase; road-only rule-3 cells and complete MapPermissions; independent maintenance phases/current paid bill/debt/extreme arithmetic; legacy zero rates; unchanged paid starter with actual demand/tax; save/load and process restart. UI/render purity remains measured separately from fixture setup and accepted commands.

The executed economy-specific isolated test starter is:

```sh
sh tools/test_economy_start.sh .local/gog-extracted/app
```

It builds the current normal app plus the on-demand technical helper and creates a fresh private settings/save/recovery root. Choose **Load Sandbox**, then **Load selected save**: `critical-before` starts paused at tick zero with 106 Funds. **Income / T** opens the details; T or normal selection returns to the Inspector. Zoom with the mouse wheel over the building cluster before placing another House. A fourth House costs 80 and opens the concrete warning: Funds 26 afterward, still-missing Service Post 100 and gap 74. Enter/Escape cancels; **Build anyway (Y)** or its matching mouse button explicitly commits once. The separate `failed-start` save shows Funds 26, Farm #10 0/4 and upkeep 46/400; `prepared-starter` is the unchanged paid full starter. The exact script was repeated after the final body fix, launching a normal menu without observer or input automation under `.local/economy-start/player-Xia-XK2Ljc` and leaving that window available. The earlier plain root was left untouched.

The existing empty Xia/gate starter also remains:

```sh
sh tools/test_gate_passages.sh .local/gog-extracted/app Xia
```

Both scripts use the user's original files and isolated roots. Their technical scenes do not use personal saves or add Funds, stocks, population, coverage or hidden ticks.

The unchanged City-v16 paid starter contract is tick zero, construction 1,280/1,300, 24/24 workers, no supplied goods/coverage/hidden ticks, and installed upkeep 48/400 ticks. The current original-Xia baseline and final normal-app comparison independently produce actual tax 75, upkeep 48 and Funds 47 at tick 400, then cumulative tax 175, upkeep 96 and Funds 99 at tick 800. These agree with earlier [City-v16 measurements](city-v16.md#measured-budget-acceptance). The already-paid tick-800 bill is excluded from the displayed next future bill: 48 in 400 ticks.

Broader human city-playthrough/recovery and original Emperor economy parity remain open. No commit, push, tag, release or publication is performed.
