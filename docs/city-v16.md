# City-v16: maintenance and city budget

Current native input delivery and the executable short human check are centralized in [Native Input Reliability](input-reliability.md) and [input acceptance](testing-input-acceptance.md). Historical observations below retain their original scope; they do not mark the current human checklist PASS or close profile-specific balance/replanning playthroughs.

Current [income and startup budget guidance](economy-start-guidance.md) separates missing infrastructure, actual operations and House supply; it retains historical taxes separately and shows a checked strictly future upkeep projection. Its current validation is tracked there. Construction and simultaneous-full-staffing estimates are not a first-tax promise; all economy, upkeep rates and paid starter authority below remain unchanged.

## Current rule 3: original map roads and gate passages

New City-v16 games use rule **3**, immutable original-map road permissions and fixed GateHouse passages. Schema **19** binds their versioned canonical fingerprint. Economy and 2×2 Well/HealthPost inherit rule 2 exactly. Rules 1/2 and schema 18 remain unchanged without migration; the general menu default remains City-v11 rule 3. See [the contract and isolated test starter](gate-passages.md).

## Historical rule 2: corrected Safety geometry

Rule **2** retains the exact rule-1 economy, maintenance, health, fire and water parameters. Only Well and HealthPost now occupy **2×2**. Central footprint/cell/front/contains APIs take the explicit rule version; placement, all-cell owner picking, perimeter entrances, Manhattan water/desirability distances, previews, selection, demolition and restore use them. Cost/workers/radius/limits remain Well 60/0/5/4 and HealthPost 120/2/road-service/2. The paid 1,280/1,300 dry starter contains neither and remains unchanged.

Schema **18** supports versions 1 and 2 without new fields or automatic migration. Existing City-v16 v1 saves retain 1×1 Well/Post; City-v14/15 remain exact. No optional copy upgrade was added. Built-in exact-pack rule-2 presentation uses identified original base Well 1559 and Herbalist 1580; the old rules retain compatible custom 1×1 visuals or authored fallbacks. Basic roads use the corrected original topology mapping. See [presentation evidence and limits](reverse/original-presentation-correction.md).

The following original maintenance measurements describe rule 1 unless stated otherwise; they are retained rather than rewritten as rule-2 balance measurements.

Starting commit: `a674b0a667190c8919e177e0aa9c237e54e0e677`, clean working tree. The explicit `sandbox-city-v16` / `RulesProfile::CityV16` rule 1 inherits City-v15 rule 1 without changing production, tax rates, operations, demolition, Fire, Health, Water, desirability or Household stages. It is available in CLI and the menu as “City v16 - Maintenance and city budget”, with the description “Recurring building upkeep adds long-term budget pressure.” City-v11 rule 3 remains the default. No automatic migration, commit, push, tag, release or publication.

## Authored ownership costs

| Building | Upkeep per 400 ticks |
| --- | ---: |
| Clay Source | 8 |
| Pottery | 12 |
| Warehouse | 4 |
| Farm | 8 |
| Service Post | 4 |
| Market | 8 |
| Fire Watch | 4 |
| Well | 2 |
| Health Post | 4 |
| Household | 0 |
| Road | 0 |

`maintenance_cost(profile, kind)` is the single explicit rule table. Every earlier profile returns zero. Values are not calculated from workers. This is the cost of owning infrastructure, including zero-worker Wells, and is independent of staffing, operation pause, fire, health or courier state. Pausing one tick before a bill does not avoid payment. Negative Funds never suppress existing production or dispatch.

`Rules::maintenance_interval_ticks` is 400. A building is due when `ticks > placed_tick` and `(ticks - placed_tick) % 400 == 0`. Construction includes the first period: no charge at placement, no proration. Placed at 235 means bills at 635, 1035, etc. Demolition stops future bills, retains the lifetime total and refunds nothing. A rebuilt building has a fresh ID and placement phase. No per-building payment clock, lifetime expense or tombstone authority is stored.

The unchanged dry starter costs 1,280 of 1,300 at tick zero, supplies 24/24 workers, has no Well or Health Post, pays no initial upkeep and installs a rate of 48/400t. The maximum city rate is calculated from the current Rules limits and cost function: 168/400t for 46 buildings and 26 couriers. The theoretical twenty Level-2 Houses could generate 1,200/400t only when their real demand succeeds; this is not a balance or coverage guarantee.

## Tick publication and exact money

Read-only arithmetic validation precedes mutation. The ordinary order is staffing capture, tick increment, production, Household demand and actual tax, maintenance, dispatch, movement/arrivals, Health, Fire, then invariants. Production does not modify House demand stocks; arrivals and health/fire transitions occur later, so preflight can evaluate the exact next-tick demand from existing stocks and next-tick expiry comparisons. The shared tax calculation is used by preflight and demand.

Taxes are added before maintenance, including their intermediate int64 bound. The complete aggregate bill and uint64 lifetime total are checked before any tick mutation. A threatened signed treasury or unsigned maintenance/tax overflow throws without publishing tick, production, demand, stock or money changes. Billing traverses at most 46 buildings, performs no BFS, map scan, World copy or heap allocation.

City-v16 pays every charge in full even if Funds become negative. No debt limit, interest, loan, arrears, bankruptcy, payment priority or automatic building pause exists. Real taxes can repay debt. Paid roads/buildings require `treasury >= construction_cost`; controls, safe demolition, free road removal and existing-road no-ops retain their ordinary behavior. An all-existing road batch is valid in debt and refreshes nothing.

The identity is:

```
treasury = starting_treasury + taxes_collected_total
           - construction_spent_total - maintenance_spent_total
```

Unsigned credit/debit terms are cancelled before summation; no lifetime counter is cast to int64. INT64_MIN magnitude is formed without negating INT64_MIN. This permits representable results even when raw intermediate credit/debit sums would exceed uint64. Existing actual House tax reconciliation and demolition history remain authoritative.

## Persistence and UI

Schema 18 was introduced for City-v16 rule 1 and now supports City-v16 rules 1 and 2 with the same fields. It adds required nonnegative uint64 `maintenance_spent_total` and uses a separate signed integer parser for treasury. Floats, booleans and unsigned integers above INT64_MAX are rejected; INT64_MIN is accepted when the complete identity is valid. A modified counter without the corresponding treasury fails validated restore. Schemas 1–17 keep their existing fields, profiles and treasury validation; no maintenance authority is backported. Schema/profile swaps are rejected.

Manual save/load and recovery use the existing validated paths. Loads pause at the saved tick. Boundary acceptance saves at 399, steps a paused recovery to 400 and compares the exact bill against a direct control. Negative checkpoints retain their exact debt and future accounting; normal recovery limits and child histories are unchanged.

The top HUD renders signed Funds and installed upkeep, meaning the sum of rates rather than one next global bill. The pure `current_maintenance_rate()` and optional `maintenance_due_in(id)` query have no World mutation or route/asset/file work. Age zero reports 400; exact positive billing ages still return zero, including an already-paid bill. Houses and removed/missing IDs return no deadline. The Inspector converts that current-tick zero into the next strictly future interval for display, retaining the query contract, and shows rate/deadline on every upkeep-bearing building with “Maintenance continues while paused.” Wells retain their radius/coverage and no worker controls. Houses show “Maintenance none” only in debug.

F1 shows lifetime spending, installed rate and buildings due in the next 100 ticks, excluding the current tick's already-paid bill. Help explains separate building cycles, debt, tax repayment, pause, demolition and the absence of interest/bankruptcy. Paid placement previews show construction plus upkeep. Starter reserve warnings remain construction-only; the bounded income panel separately calculates strictly future upkeep for unchanged current buildings and no new income/purchases, without simulated ticks or a first-tax forecast. At INT64_MIN, the display-only missing-House-funds calculation saturates to INT64_MAX rather than overflowing; checked upkeep projections report an unrepresentable boundary. Construction reserve rules remain unchanged. All presentation and road previews are pure and retain coalescing.

## Measured budget acceptance

All figures below use paid commands and actual production, courier deliveries and Household demand. Treasury, goods, tax and population are not injected into these scenarios. Extreme arithmetic and precise -75 persistence fixtures are separate synthetic accounting tests.

| Scenario | Observation |
| --- | --- |
| Dry starter, tick 400 | Actual first tax 75; Funds 20 → 47 after 48 upkeep |
| Dry starter, tick 800 | Funds 99; cumulative actual tax 175; upkeep 96 |
| First water/health purchases | Well at tick 800; Health Post at 1200; Funds 29 after Post |
| Compact healthy four Houses | Rate 54/400t; Funds 4,651 at 16k → 7,171 at 24k |
| Same Houses, duplicate infrastructure | Rate 158/400t; 4,000-tick net +220 versus compact +1,260 |
| Larger supplied eight-House city | Rate 84/400t; measured 4,000-tick net +3,010 |
| Deliberate overbuilding / lost Service | Genuine paid reserve spending and maintenance create debt -127 |
| Safe demolition and actual tax recovery | Duplicates removed: rate 158 → 54; resumed real Service/food/pottery demand repays debt; paid road construction succeeds with Funds 123 remaining |

Money remains relevant after the opening: duplicate infrastructure both consumes construction capital and reduces the same Houses' ongoing margin by 1,040 over ten periods. A larger supplied city earns more while adding measured upkeep. Debt is visible and reversible when real supply/staffing permits it. These bounded scenarios establish direction, not a claim of perfect balance or recoverability for every depleted city.

## Validation

Final complete suites on the current source: Debug 78/78 (750.53 s), Release 78/78 (57.51 s), ASan/UBSan 78/78 (179.43 s), with no failures. They include all historical profiles, saves, menu, recovery, demolition, Fire, Health, Water, desirability, House presentation and road responsiveness checks. Logs remain ignored under `.local/city-v16/ctest-{debug,release,sanitize}-final.log`.

The final `tools/package_macos.sh` run passed a separate Release 78/78 suite (27.04 s), dependency/license staging, recursive Mach-O and ad-hoc signature verification, relocated and unzipped execution, synthetic menu/save/process-restart checks, read-only original-data smoke checks and forbidden-asset/path negative checks. Original input files were unchanged and no proprietary assets were packaged. The local dirty-workspace arm64 candidate retains display version 0.1.0-alpha.2 and base revision a674b0a66719; it is not a release. ZIP: `dist/OpenEmperor-0.1.0-alpha.2-a674b0a66719-macos-arm64.zip`, 4,075,424 bytes, SHA-256 `bdf6e6eddc470d8a2ae35c87da044d120a4d18a98e1b32cbb108392f770dfed9`. The generated `dist/package-report.json` also records the actual final native keyboard acceptance. Developer ID, notarization and an independent clean-Mac run were not performed; the existing missing project license remains unchanged.

An earlier package invocation failed the timed map-corpus test with “map catalog failed” while long checks were running concurrently. Its isolated recheck and subsequent complete packaging runs passed. No timeout or production behavior was weakened to hide that failure.

Dedicated tests cover the exact table and legacy zero rates; age-100 and independent phase boundaries; pause, unstaffed and fire costs; signed debt and paid/free commands; actual starter order; demolition/rebuild phases; schema 18 and legacy negative rejection; manipulated/missing/wrong-type authority; boundary and negative recovery; INT64_MIN, UINT64 maintenance limits and aggregate tax overflow with unchanged snapshots; City-v15 state equality excluding the new economy; City-v14 Well regression; pure UI/query performance counters. The complete SDL HUD and construction guidance are also exercised at INT64_MIN under ASan/UBSan. The exact -1 command and -75 persistence cases use validated synthetic accounting fixtures, while the budget and debt-recovery scenarios above use actual paid play.

The passing 20k scenario compares complete snapshots after every tick with multiple placement phases, paid reserve spending, debt/tax recovery, Water, Health/illness, Fire, operations and demolition/rebuild. The passing 100k scenario reaches the full 46/26 bounds with staggered construction, a debt period, genuine recovery, safe relocation, Fire/Health/Water and twenty save/autosave/recovery comparisons. It ends with Funds 1,933 and lifetime upkeep 44,132. Lifetime upkeep is independently reconciled against total building exposure, including demolished buildings.

Native keyboard acceptance used the local macOS bundle and the user's read-only original map. Paused single stepping and F5 measured tick 399 at Funds 20 / tax 0 / maintenance 0, followed by tick 400 at Funds 47 / tax 75 / maintenance 48. F9 restored tick 400 exactly and paused. Normal 1x execution reached tick 7,842 at Funds 483, cumulative real taxes 1,375 and upkeep 912, with 23 residents and the unchanged dry starter. Help, signed Funds and installed upkeep were visually inspected. Attempts to select a toolbar building with native mouse coordinates did not select it, consistent with the existing input limitation. A complete native mouse-driven building, expansion and demolition playthrough is therefore still unresolved; the corresponding paid simulation and scripted SDL checks are distinct acceptance evidence.

After the final rebuild, the native City-v16 window loaded tick 400 paused at Funds 47. F1 showed lifetime maintenance 48, installed 48/400t and due next 100t 0, excluding the already-paid bill. The full maintenance Help was readable. Actual F5, single step and F9 returned to tick 400 paused; the saved document was exactly equal to its pre-check control. The test windows were closed after acceptance.

No new Building, Courier, Goods or service type. No SG3 audit, new visuals, original pixels or proprietary files. Existing visual fallbacks and House stages remain unchanged. Package and native checks are recorded separately from synthetic/headless checks; the existing mouse-coordinate limitation is not counted as a completed normal playthrough.

## Presentation correction validation

Starting HEAD `4b8d4c4e4ef3f58ae2ca3398a04917ebfcf69319`. New synthetic checks cover rule-1/rule-2 occupancy, additional-cell overlap/buildability rejection, actual 2×2 Well Manhattan boundaries, far HealthPost perimeter dispatch and arrival, full demolition, schema-18 roundtrip/resume and autosave/recovery. SDL checks cover picking every cell at 1×/2×/4×, preview coalescing, eager texture deduplication, no World/BFS/route/asset work while rendering, zero selected-profile Safety fallbacks, rejected mismatched custom geometry, unknown fallbacks and legacy custom one-cell loading. Existing maintenance, signed arithmetic, fire, health, road responsiveness, old schemas and recovery regression checks remain active.

Both rule 1 and rule 2 have separate **20,000-tick determinism** and **100,000-tick endurance** cases. The v2 cases use ordinarily paid, collision-free 2×2 placements; the full 46-building/26-courier scenario retains the 168/400-tick maximum and checks fire/health/water, debt/recovery, demolition/rebuild and 20 manual/autosave/recovery roundtrips.

Local original-data review uses a new paid Xia rule-2 scene at tick 4200 with simultaneous House stages 0/1/2. Native Metal 1×/2×/4× captures and genuine keyboard zoom input reviewed the original Well/Herbalist style. **Road continuity acceptance FAILED / superseded by real user playtest:** the earlier captures missed grass strips between road cells; see the road-continuity follow-up. The initial scene construction and all-cell input tests are scripted/synthetic; this does not close older broad mouse-driven play acceptance. Original screenshots and review runners remain ignored.


Final full suites for this correction: **Debug 82/82 (481.91 s)**, **Release 82/82 (61.32 s)** and **ASan/UBSan 82/82 (179.58 s)**. All exited successfully. Both rule generations passed their separate 20k determinism and 100k endurance checks. Final logs are ignored under `.local/presentation-correction/final-ctest-debug.log` and `final2-ctest-{release,sanitize}.log`. No final compiler warnings were reported. Two overly narrow bounds in a newly added fallback-pixel assertion were corrected to encompass the existing mesh; no production geometry or timeout was weakened.

The final `tools/package_macos.sh` run passed its own **Release 82/82 (45.65 s)**, staged dependency/license checks, recursive Mach-O/ad-hoc signature verification, relocated and unzipped execution, menu/save/restart checks, unchanged-original-data checks and forbidden-asset negative checks. The local dirty-workspace candidate retains display version 0.1.0-alpha.2 and base revision `4b8d4c4e4ef3`. ZIP `dist/OpenEmperor-0.1.0-alpha.2-4b8d4c4e4ef3-macos-arm64.zip`: **4,076,547 bytes**, SHA-256 `f30e124fdfd2f7d8eb34f7ead0b93a8aeb95853be054f25121cd5e51b1018cc3`. It contains no original pixels, atlases, review runners or saves. The seven Model/EXE/manual/General/Terrain research inputs also match their pre-check SHA-256 values.

Final-bundle desktop acceptance launched `dist/OpenEmperor.app` with the paid rule-2 Xia save. It initially displayed tick 4200 paused, Funds 656, upkeep 54/400t and water 4/6. Actual F1 input showed rule 2, 16/16 road masks and the recognized exact pack; actual F1/Z input displayed the ordinary enlarged city with the original Well and Herbalist, road strips and the other civic/industry roles. **The connected-road conclusion is FAILED / superseded by real user playtest**; this historical input check did not validate pixel continuity. The separate native review window also accepted actual W/H keys for 4× Well/Herbalist detail captures. These desktop observations verify the final rendering and resource lookup; they do not assert a complete manual placement/demolition playthrough.

The existing City-v16 rule-1 native saves at ticks **399, 400 and 7842** were independently loaded through the ordinary save/World/view path with exactly equal World snapshots before and after rendering. Schema 18, one-cell Well/HealthPost authority and legacy visual filtering were retained. No save was migrated or overwritten. HEAD remains the requested starting commit; no commit, push, tag or publication was performed.

### Road continuity follow-up (2026-10-03)

The earlier connected-road acceptance above remains **FAILED / superseded by real user playtest**. The follow-up replaces the old stored single-cell grass graphic for configured complete roads, preserving the evidenced mapping and all Well/Herbalist, geometry, economy and save authority. Original-data production readbacks measure zero disconnecting terrain cuts on 128 contacts at 1×; synthetic adversarial compositor tests cover exact 1×/2×/4× and alpha drag. The final bundle passed actual keyboard F1-off 1× and F6 off/on review of the same six-House paid Xia scene at tick 4200, Funds 656 and population 53. Neighboring individual grass blades and whole-image object occlusion remain possible. See [measurements, native/synthetic distinctions and final regression](road-visual-profile.md#road-continuity-correction-2026-10-03).
