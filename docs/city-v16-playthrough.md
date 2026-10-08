# City-v16 rule-3 paid Xia playthrough

The normally paid technical Xia replay first reaches the complete goal at **tick 6,800: ten Houses, eight currently effective Level-2 Houses and 126 residents**. With no further construction it retains the goal at every tick through **9,200**, finishes at 148 residents, and earns **3,360 tax − 576 upkeep = 2,784 additional Funds**. All ten Houses fulfil exactly six further demands, with zero misses in that 2,400-tick window. These are bounded measured outcomes for this layout, not a universal balance or human-playthrough claim.

This integration pass uses `Cities/Xia.map`, `sandbox-city-v16` rule **3**, immutable MapPermissions policy **1** and save schema **19**. The actual starting HEAD is `8d4a35decd8ebb92d0abacc0c87e75b1e0312891`; its only difference from the requested `b88d9b94f2c7eb68023ff4f65e85283d5d7a7b40` is the preceding KNOWN_ISSUES cleanup. The initial check was clean. A subsequent local edit removing the broad human-acceptance bullet from KNOWN_ISSUES is preserved.

The normal paid prepared starter remains tick zero, construction **1,280/1,300**, Funds **20**, four Houses, population/workforce **24/24**, installed upkeep **48/400t**, one FireWatch, no Well or HealthPost, and no delivered goods, coverage, tax or hidden progress. Its production-selected storage origin is `(103,113)`. Every technical expansion uses accepted ordinary paid commands and real ticks; no simulation authority is injected into the main city.

## What the earlier tests establish

| Existing tests | Scope | Limit for this milestone |
| --- | --- | --- |
| SandboxCityV16Tests | Rule-1/2 maintenance, income, four/eight Houses, debt/recovery, 20k determinism and 100k endurance | Synthetic fully buildable 160×32 mask; no full settlement-goal assertion |
| SandboxCityV11BalanceTests | Paid complete goal followed by 2,400 ticks of supply | City-v11 rule 2 on a synthetic fully buildable map |
| AlphaEnduranceTests | Industry-v5 and City-v6–11 100k runs, historical goal retention and goods/persistence invariants | Synthetic maps; no City-v16 goal. This executable is deliberately outside CTest |
| MapPermissions/Gate tests | Authored original-format parsing, immutable policy, protection/heights, gates, begun edges, wait and paid repair | Individual policy/traffic scenarios, not a complete economy |
| Save/Recovery tests | Full snapshots, continuations, restart, rotation, source preservation and child histories | Authored fixtures, not the full earned Xia goal city |

The new on-demand original-data helper closes the combined Xia playthrough gap. It does not add proprietary test fixtures or duplicate the existing subsystem checks.

## Paid expansion recipe

Coordinates are zero-based storage-cell **origins**, not screen pixels. The 2×2 objects occupy the origin plus one cell in each axis; Market, Service, FireWatch and Farm remain 1×1. The starter's road runs along `y=115`, `x=103…117`. Its Clay/Pottery/Warehouse/Farm/Service/Market origins are `(103,113)`, `(103,116)`, `(106,113)`, `(107,116)`, `(108,116)`, `(106,116)`; Houses 7–10 occupy `(109,113)`, `(109,116)`, `(112,113)`, `(112,116)`, and Watch 11 is `(117,116)`.

All listed expansion cells pass the actual original-map policy and full footprint checks. An exploratory Well at `(110,118)` was correctly rejected by buildability; it is absent from the accepted recipe. An exploratory disconnected residential branch lacked Road `(120,115)`; completing that legal connection resolved the setup error. Neither was a code defect.

| Tick | Ordinary purchases, in order | Actual cost | Funds after stage |
| ---: | --- | ---: | ---: |
| 400 | First real House demand revenue; no purchase | 0 | 47 |
| 800 | Well `(114,116)` | 60 | 39 |
| 1,200 | House `(116,113)`; Roads `(118,115)`, `(119,115)` | 84 | 50 |
| 1,600 | House `(119,113)` | 80 | 90 |
| 2,000 | Market `(118,116)`; Road `(120,115)`; Roads `x=121,y=115…119`; Roads `y=119,x=112…124` excluding the already built `(121,119)`; Roads `(117,120)`, `(117,121)` | 180 | 70 |
| 2,400 | Service `(117,122)`; Watch `(111,114)`; Roads `x=105,y=116…122`; Roads `(103,119)`, `(103,122)`, `(104,119)`, `(104,122)` | 202 | 0 |
| 2,800 | Clay `(103,120)` | 120 | 64 |
| 3,200 | Pottery `(103,123)`; Well `(112,120)` | 240 | 0 |
| 3,600 | Well `(122,113)`; House `(119,116)` | 140 | 17 |
| 4,000 | Houses `(122,116)`, `(115,120)` | 160 | 72 |
| 4,400 | House `(118,120)`; Roads `x=114,y=120…122`; HealthPost `(114,123)` | 206 | 56 |
| 4,800 | Well `(114,113)` | 60 | 237 |

The first 400-tick demand pays **75 actual tax** and charges **48 upkeep**, so Funds are only 47; the first 60-fund Well waits until tick 800/Funds 99. Expansion spending is **1,532**: six Houses 480, four Wells 240, 36 new Roads 72, Market 140, Service 100, Watch 80, Clay 120, Pottery 180 and HealthPost 120. Total construction is **2,812**, including the unchanged 1,280 starter. No purchase is accepted without sufficient actual Funds. The two zero-Funds stages remain operational and recover through actual tax.

The first Well covers three starter Houses; the later Wells cover the added residential strip and finally the initially dry House 7. The new Market and Service improve residential Desirability and actual distribution/coverage independently. More residents finance and staff the second production pair: one Pottery cannot sustainably supply ten successful demands per 400 ticks, even at its theoretical production ceiling. Industry stays on the western branch to preserve residential quality. The second Watch provides actual patrols; the paid HealthPost uses actual resident workers. At the goal all **44 installed/active workers are assigned**; residents supply 126, later 148.

The costs and exact cells above are the reproducible purchase instructions. The reason and measured result for each expansion stage are:

| Tick | Bottleneck and intended effect | Actual post-purchase observation |
| ---: | --- | --- |
| 800 | Remove the dry level cap without adding worker demand | Houses 8/9/10 gain Water; 8/9 become effective Level 1. House 7 remains dry |
| 1,200 | Add residents, future revenue and an eastern road connection | Five Houses, population 33; all 24 required workers assigned; the new House has Water |
| 1,600 | Grow workforce before buying more staffed facilities | Six Houses, population 42; the new House initially awaits the later eastern Well |
| 2,000 | Extend distribution and positive Desirability toward the eastern strip | The second Market raises worker demand to 28, all assigned; House 10's score is 16 and the two added Houses score 17/15 |
| 2,400 | Extend real Service and fire patrols; connect the western production and southern residential branches | All 32 workers assigned; Houses 9/10 are effective Level 2, with scores 13/22 |
| 2,800 | Supply the later second Pottery with real Clay | The second Clay Source receives its four workers; all 36 required workers are assigned |
| 3,200 | Increase sustainable Pottery supply and prepare southern Water | All 42 workers assigned. Actual cumulative Pottery rises from 48 to 60 by tick 3,600, versus 41 to 48 in the preceding interval |
| 3,600 | Water the eastern Houses and add another supplied residence | Seven Houses, population 62; Houses 14/22 gain Water; four Houses are currently effective Level 2 |
| 4,000 | Grow toward the ten-House goal on already connected, watered cells | Nine Houses, population 77; both new Houses have Water; all 42 required workers remain assigned |
| 4,400 | Place the tenth House and fund actual Health prevention/cure | Ten Houses, population 86; all 44 workers assigned. The Post dispatches at 4,401 and actually cures House 7 at 4,535 |
| 4,800 | Remove the final dry cap and slow subsequent unprotected Health risk | All ten Houses now have Water; population 91. The completed layout reaches eight current Level-2 Houses and the full goal at 6,800 |

Population and House history continue only through subsequent real demand ticks. Placing a House, Well, Market or Post does not inject development, goods or coverage.

## Goal and fixed-building stability

| State | Tick | Houses / effective L2 | Population | Funds | Lifetime actual tax | Lifetime upkeep | Installed upkeep |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| First complete goal | 6,800 | 10 / 8 | 126 | 2,282 | 5,080 | 1,286 | 96/400t |
| After stability window | 9,200 | 10 / 8 | 148 | 5,066 | 8,440 | 1,862 | 96/400t |

The last construction is the fourth Well at tick 4,800. The full city has 27 buildings, 15 Couriers and 51 paid Road cells. Goal status is queried at every subsequent tick, with minimum population 126 during stability. Construction, Water, Desirability and installed upkeep stay fixed. Each of eight Good Level-2 Houses pays 360 over six successful demands; the two Neutral Level-1 Houses each pay 240. This reconciles the exact 3,360 tax total, while six complete placement-relative upkeep periods reconcile 576. No building is added to conceal a bottleneck.

| House ID | Origin | Desirability | Historical / effective level | Successful / missed demands at goal | Residents / capacity at goal → final | Actual taxes at goal → final |
| ---: | --- | ---: | --- | --- | --- | --- |
| 7 | `(109,113)` | −6 | 2 / 1 | 14 / 3 | 9/10 → 10/10 | 425 → 665 |
| 8 | `(109,116)` | 0 | 2 / 1 | 15 / 2 | 10/10 → 10/10 | 570 → 810 |
| 9 | `(112,113)` | 13 | 2 / 2 | 16 / 1 | 16/16 → 16/16 | 810 → 1,170 |
| 10 | `(112,116)` | 22 | 2 / 2 | 16 / 1 | 16/16 → 16/16 | 865 → 1,225 |
| 13 | `(116,113)` | 23 | 2 / 2 | 13 / 1 | 16/16 → 16/16 | 685 → 1,045 |
| 14 | `(119,113)` | 17 | 2 / 2 | 13 / 0 | 14/16 → 16/16 | 605 → 965 |
| 22 | `(119,116)` | 22 | 2 / 2 | 8 / 0 | 13/16 → 16/16 | 385 → 745 |
| 23 | `(122,116)` | 13 | 2 / 2 | 6 / 1 | 11/16 → 16/16 | 265 → 625 |
| 24 | `(115,120)` | 20 | 2 / 2 | 6 / 1 | 11/16 → 16/16 | 265 → 625 |
| 25 | `(118,120)` | 23 | 2 / 2 | 5 / 1 | 10/16 → 16/16 | 205 → 565 |

All ten have Water at the goal. Houses 7/8 correctly remain effective Level 1 because their scores are below 10; their historical Level 2 is preserved. `main-facts.jsonl` records every House's stocks/reservations, Service expiry, Water, score, Health/Fire risk and deadlines, successes/misses, both levels, residents/capacity and actual taxes every 100 ticks. Each checkpoint and accepted expansion also records full facts. Full SaveDocuments retain all authoritative fields; diagnostic canonical hashes alone are not used as Snapshot/schema/rule proof.

## Goods under load

Actual phase transitions, cargo/reservations, target and arrival ticks are recorded throughout the 9,200-tick main run. Dispatch is an actual departure, not a read-only Ready finding. The final active trips explain unequal dispatch/arrival/home counts; unarrived goods stay in real cargo and target reservations.

| Logical role | Actual dispatches | Actual arrivals | Actual returns home |
| --- | ---: | ---: | ---: |
| Warehouse → Market Pottery inbound | 114 | 114 | 113 |
| Market → House Pottery distribution | 114 | 113 | 112 |
| Farm → Market Food inbound | 149 | 148 | 148 |
| Market → House Food distribution | 167 | 166 | 165 |
| Service | 161 | 160 | 159 |
| FireInspector | 160 | 159 | 158 |
| HealthWorker | 28 | 27 | 27 |

Observed maxima are Farm output 12, Market Food 16, Market Pottery 7, Warehouse Pottery 6 and House Food/Pottery eight each. Individual cargo/reservation never exceeds four. Aggregate incoming maxima are Clay six, Pottery ten and Food nine, classified by real target kind and Good. Clay reservations at a Pottery are not mislabelled as incoming Pottery.

| Actual production / workforce | At goal, 6,800 | At final, 9,200 | Stability change |
| --- | ---: | ---: | ---: |
| Lifetime Clay extracted | 337 | 487 | +150 |
| Lifetime Pottery completed | 158 | 232 | +74 |
| Lifetime Food produced | 212 | 281 | +69 |
| Assigned / required workers | 44/44 | 44/44 | unchanged |
| Available resident workforce | 126 | 148 | +22 |

The helper also samples each existing Courier's diagnostic status after every tick and its purchases. These are **post-tick Courier observations using current allocation**, not the tick-start staffing used by production/dispatch, predicted events, or necessarily distinct city ticks. Main exposures include Pottery distribution NoStock 5,788, Food distribution NoStock 657, Food TargetFull 807 and Clay TargetFull 941. NoRoad, NoTarget, Unstaffed, Paused and OnFire are explicitly zero in the main branch. Temporary NoStock does not establish failed demand: buffered House supply later fulfils all 60 stability deadlines. Full Market/House stocks demonstrate bounded backpressure, while the separate Road branch deliberately supplies the measured missing-connection/wait condition.

## Real safety visits and separate interruption branches

The main city records actual Service and Inspector arrival/return events and Health prevention. Initially dry House 7 becomes naturally sick at tick 3,400, with expiry 4,600. The paid Post dispatches at 4,401; dispatch does not cure. Its actual arrival at **4,535**, before natural expiry, cures House 7 and protects until **6,935**. During the build-up some real deadlines miss; the later stable window records no misses.

The ordinary goal run produces no natural fire. In a separate earned tick-9,200 city, both Watches are paused through normal operation commands. A House naturally burns at **12,720**; a fire-blocked demand misses at **12,800**, consuming neither ware and paying no tax. Both Watches resume through normal commands, and an actual Inspector arrival extinguishes a still-active incident at **12,865**. This branch injects no incident state and does not alter the main city.

A separate saved transport branch removes future Road `(121,118)` at **4,406** while Food distribution Courier 10 carries/reserves **two Food**. It completes its protected begun edge at **4,409**, then retains the real position, cargo and reservation through 30 waiting ticks. A normally paid **2-fund repair at 4,439** resumes delivery; actual arrival is **4,489**, home is **4,559**. This is specifically a Food road-cut check. Pottery and both inbound/outbound goods paths are measured in the main run; the existing authored road/gate tests retain their separate broader coverage. Goods, economy and navigation invariants pass, and neither intentional disruption changes the main goal snapshot.

No production bug is demonstrated by this layout, and no balance, rule, policy, save, sprite or engine fix is made. The rejected placement, missing exploratory connection, early supply misses and Neutral caps are valid rules/setup findings. The paid recipe demonstrates one viable bounded strategy; it does not certify every layout or depleted city as recoverable.

## Normal Application, persistence and recovery

The unchanged Application/main sources run with an ignored private read-only observer and directly delivered scripted SDL PollEvent structs on dummy/software. A paused CLI prepared starter executes all **52 actual UI purchases**, ordinary Period ticks, seven full checkpoints and F5/F9/F5. The 390 rendered frames take 18.581 seconds; batched stepping means they do not observe every intermediate rendered tick. Every A–G full WorldSnapshot, schema/rule/policy/map context and entire reparsed manual SaveDocument matches the independent paid replay. The separate helper compares full snapshots after all 9,200 ticks and every command.

| Actual F5/F9 checkpoint | Tick | Funds | Lifetime tax | Lifetime upkeep | Population / effective L2 |
| --- | ---: | ---: | ---: | ---: | --- |
| A paid starter, before first tax | 0 | 20 | 0 | 0 | 24 / 0 |
| B first revenue | 400 | 47 | 75 | 48 | 24 / 0 |
| C first added House | 1,200 | 50 | 320 | 146 | 33 / 0 |
| D six Houses | 1,600 | 90 | 490 | 196 | 42 / 0 |
| E active Health transport | 4,406 | 56 | 2,220 | 712 | 86 / 4 |
| F complete goal | 6,800 | 2,282 | 5,080 | 1,286 | 126 / 8 |
| G stable city | 9,200 | 5,066 | 8,440 | 1,862 | 148 / 8 |

The primary process exits. A separate real OS process normally loads the **actual Application F5 E save**, paused at 4,406, and advances 20 ordinary Period ticks to 4,426. Both complete snapshots/context and F5/F9/F5 documents match the control, including active positions/edges, cargo/reservations, stocks, Health/Fire deadlines, levels, population and goal status. It observes 112 frames in 6.501 seconds. Restart uses the existing `--load-sandbox` path, not direct World publication.

A third isolated process uses the **normal menu** to load E and advances 1,200 ordinary ticks. The existing AutosaveController writes its actual periodic schema-19 checkpoint at **5,606**, equal to the complete live state; the old manual target remains unchanged. The ordinary menu's recovery history loads that actual checkpoint paused, gives it a distinct manual target and creates a linked child history. Old protected starts/checkpoints stay unchanged. F5/F9/F5 on the recovered target produces the same entire document as the periodic checkpoint. This branch observes 140 frames in 8.332 seconds and never uses a personal history.

The same normal Application separately reproduces the Food Road interruption with two ordinary UI commands. Waiting at 4,409, delivery at 4,489 and home at 4,559 all retain exact complete Snapshot/context and F5/F9/F5 documents (131 frames, 7.614 seconds). Another real process loads E and observes **4,534 still sick/outbound → one Period → 4,535 actually cured/returning → 4,670 actually home**. All four complete checkpoint comparisons and manual save roundtrips match independent ordinary-load/tick controls (129 frames, 7.813 seconds). Submitted Health clip draws are positive, fallback zero, with schema 6/288 aliases/176 physical assets/1,358,208 logical bytes. This measures the existing figure and arrival, without another walker implementation pass.

Each prepared session retains **176 Walker textures and 1,346 initial eager uploads**. The recovery process prepares two sessions, hence **2,692 total uploads**; these are load-time counts, not frame uploads. Status/Income/Water/Health/Desirability/F1 and ordinary rendering retain all twelve prohibited-work counters at zero; observed trip snapshots and deadlines remain exact through the UI interactions. Each test process reports zero remaining owned textures after shutdown. The private observer's full-state hashing, file reads and validation are measured outside the production-render boundary, before/after separate oracle work.

The first observer attempts exposed two harness errors: its counter span included private checkpoint validation, and a planned Road click was below the map viewport. The private timing boundary and normal anchored wheel framing were corrected, then the complete application replay was rerun from a fresh starter. No production input, coordinates, projection or simulation was patched.

Local application evidence is `.local/city-v16-playthrough/app/{primary-v3,restart-E,recovery-E,road-branch,health-arrival}/`, with reports, frame/query observations, actual manual saves and full comparisons. The five owned processes observe **902 rendered frames** altogether, then exit normally with zero owned textures. The consolidated report is `app/application-validation-report.json`. Primary production render-to-Present-entry CPU timing is median **0.768 ms**, p95 **1.433 ms**, maximum **3.078 ms**, excluding private observer work and SDL Present; it is not real-time gameplay FPS or a native-backend benchmark. Original-data replay controls and detailed events are under `.local/city-v16-playthrough/recipe/frozen/`. Map SHA-256 is `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433`, original buildable-mask SHA-256 is `14b5059ecc0f394d13cee13f9d7210302246fd6f78f62cdb27a69da48e64e55c`, policy-1 SHA-256 is `69e0d2df5b1c9a3f12470f99041d03e2e8b9e8c75f105070e66a4b33bc35bdc4`.

## Rebuild and play

From the repository root, with your own extracted game files:

```sh
sh tools/test_city_v16_playthrough.sh .local/gog-extracted/app
```

The script builds Release and creates a fresh `.local/city-v16-playthrough/player-Xia-XXXXXX` app root outside the original-data directory. It starts the ordinary application. Choose **Load Sandbox → Load selected save** to load **A-paid-starter** paused at tick zero. **F-goal** and **G-stability** are separate technical replay checkpoints, not the starting city. Their buildings, taxes, goods and residents must be earned by the logged normal commands.

That exact command was executed against the final helper/source, produced the passing report and all checkpoint saves under `player-Xia-nQFCLB`, and launched the ordinary uninstrumented app with that isolated root. The standalone CLI app cannot be bound by the available native Computer Use app inventory; no native pointer/keyboard or human PASS is inferred from this passive launch. The scripted normal-main checks above prove loading, input actions and save paths separately.

Space runs/pauses; `.` advances one tick; F5 saves and F9 reloads paused. Original sprites activate through the existing fingerprinted profiles. No personal settings, saves or recovery histories are used.

## Final validation

All final builds use the project's system-SDL configuration. The normal **131-test CTest inventory is unchanged**, and every registered test runs without failure or skip. This includes City-v16 economy, House evolution/Desirability, Water/Health/Fire, workforce, Road/Gate routing, Save/Recovery, Renderer/Walker and road responsiveness. AlphaEndurance is run separately because it is not registered in CTest. Each configuration also builds the excluded original-data helper explicitly and runs the complete earned Xia replay in a fresh root.

| Final configuration | CTest | Separate AlphaEndurance | Original Xia goal/stability helper |
| --- | --- | --- | --- |
| Debug | 131/131 PASS, 894.67 s | PASS, 582.35 s | PASS, 195.78 s |
| Release | 131/131 PASS, 67.75 s | PASS, 31.55 s | PASS, 4.24 s |
| ASan/UBSan, RelWithDebInfo | 131/131 PASS, 235.14 s | PASS, 131.81 s | PASS, 59.34 s |

The sanitizer run uses `OPENEMPEROR_ENABLE_SANITIZERS=ON`, `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, matching the existing project setup. All test, Endurance and helper logs contain zero ASan/UBSan findings; compilation contains zero warnings.

All **14 complete SaveDocuments**, including A–G and the earned road/fire branches, are byte-identical across Debug, Release, ASan/UBSan and the exact tested player command. Their complete `main-facts.jsonl` traces are also byte-identical. The helper additionally requires full Snapshot equality after every command and tick; the normal Application comparisons described above independently exercise the UI, persistence and recovery paths.

The **352 source/build/tool files** remain identical before/after and across all three final matrices. All **314 existing tracked files under `src/`, `tests/` and `resources/`** match the baseline, and all **1,464 original files / 819,711,091 bytes** retain exact bytes and inventory. HEAD is unchanged; existing local edits and personal roots are preserved. The only new code is the excluded integration/helper target and isolated test script. No production defect requiring a regression/fix or a balance change was demonstrated.

Consolidated ignored evidence is `.local/city-v16-playthrough/final-validation-report.json`, with `final-{debug,release,sanitizer}-report.json`, stage logs and JUnit records; `app/application-validation-report.json` keeps scripted normal-Application evidence separate. No original data or generated checkpoints enter the repository fixtures or a distribution bundle.

## Evidence boundaries

The authoritative goal query requires all three conditions simultaneously: at least ten Houses, eight **currently effective** Level-2 Houses and 100 residents. Historical development alone does not satisfy the level condition. Water and Desirability caps, actual goods/Service supply, incidents, workers, real tax and placement-relative upkeep remain unchanged.

Technical simulation, scripted ordinary Application, native input and human observation are separate evidence. On 2026-10-08 Oliver confirms normal-session HealthWorker visibility: **User-reported Health visibility PASS**, alongside the prior Service confirmation. This does not establish a complete human goal/stability/recovery playthrough. The remaining manual check is to rebuild the paid expansion, observe supply and safety visits, save/restart/recover, and assess placement, Inspector clarity and stability in ordinary play.

No new goods, buildings, walkers, asset research, rule/policy/save versions, commit, push, tag, release or publication.
