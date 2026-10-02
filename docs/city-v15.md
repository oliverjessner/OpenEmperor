# City-v15 — Health and disease

`sandbox-city-v15`, rule 1, is an opt-in OpenEmperor-authored extension of City-v14 rule 1. It adds deterministic Household health and a road service, without RNG, new goods, medicine inventory or water transport. It makes no claim about original Emperor health mechanics. City-v11 remains the menu/CLI default; profiles and schemas 1–16 retain their existing behavior, with no City-v14 migration.

## Rules and authority

Each placed House owns `health_risk`, `health_protection_until_tick` and `sick_until_tick`. Their defaults are zero; every non-House and every older profile must keep them zero. `WorldHealth.cpp` and the five read-only Household queries own the rules. Rendering and inspectors never calculate disease independently.

| Parameter | City-v15 rule 1 |
|---|---:|
| Placement-relative risk step | 100 ticks |
| Water-covered, unprotected healthy House | +1 risk per step |
| Dry, unprotected healthy House | +3 risk per step |
| Illness threshold | 100 |
| Untreated illness | 1,200 ticks / 60 simulated seconds |
| Protection after actual Health Worker arrival | 2,400 ticks / 120 simulated seconds |
| Health Post cost / workers | 120 funds / 2 |
| Health Post footprint / limit | 1×1 / 2 |
| Building / courier collection bounds | 46 / 26 |

Risk does not grow while sick or protected. At threshold, risk and protection reset to zero and illness starts. Protection and illness use strict future deadlines; they expire at the exact deadline. On a qualifying risk step at expiry, growth resumes immediately. There is no extra progress clock: use `ticks - placed_tick`, checked before subtraction. Future deadline headroom is checked before tick mutation and when restoring City-v15.

Water only slows risk and retains its independent effective-level cap. A Well does not cure, protect, employ workers or own a courier. A Health Post has no desirability impact and owns one appended `HealthWorker` role (numeric 11); it carries `Good::Goods` with cargo/reservation zero. All existing enum values remain stable. Health Posts support Running/Paused and High/Normal/Low, use the shared complete-requirement staffing allocation, and are fire-eligible. An unstaffed, paused or burning Post stops new dispatch; an active trip continues.

## Visits, roads and update order

Each worker uses the existing `dynamic_target_routes` cache, bounded to 20 Houses, and its own `last_dispatched_target`. `courier_can_target()` is the common placed-House predicate for cache construction, selection, navigation and restore validation. Reachable sick Houses come first (stable ID for ties), then reachable unprotected Houses by descending risk and ascending ID, then ordinary stable cyclic patrol. There is no visit reservation or global Health cursor; Posts act independently.

Only actual arrival resets risk and grants protection. Arrival at a sick House also sets `sick_until_tick` to the current tick. Healthy visits preserve expired sickness history. Existing edge completion, `route_pending`, revision-gated rerouting and waiting behavior apply without another BFS implementation. A disconnected district receives no visits from the other district. A safe idle Post can be demolished without refund; existing Household protection survives until its normal deadline. Burning or active-owned-worker demolition remains blocked.

Tick order is staffing capture → tick increment → production → Household demand → dispatch → movement/arrivals → Health → Fire → invariants. Arrivals therefore protect before the risk step. A new illness affects the next demand evaluation. Health and Fire are independent and may coexist.

A sick House takes the ordinary single demand-miss branch, retaining Food and Pottery, paying no tax and applying at most one population loss. Fire plus illness does not double the penalty. Sick residents still supply their current population as workforce. Illness does not directly change desirability, historical development, water cap or visual House stage. The existing 800-tick move-in grace and goal (10 Houses, 8 effective Level 2, population 100) remain unchanged.

## Paid progression and measured pressure

The prepared starter uses the unchanged paid tick-0 commands: 1,280 spent from 1,300, 20 remaining, 24/24 workers, no Well, Post, protection, goods grant or hidden progress. In the synthetic compact fixture, genuine first tax and the first affordable Well occur at tick 400; the Post is bought at 800 and the first arrival occurs at 835. These are measured fixture outcomes, not universal timing promises. A fresh dry House becomes sick at 3,400; a water-covered House without visits becomes sick at 10,000. Natural dry recovery is at 4,600, with +3 risk on that qualifying boundary.

The treatment fixture, restored with valid active sickness, stays sick after dispatch and is cured only on actual arrival at tick 80; an active trip finishes even after its Post is paused. The two-district tests prove independent road service and the inability of remote Water alone to generate protection. In the compact four-House test, the longest observed worker path has 8 vertices and no protection gaps after warm-up. The connected 20-House street spans over 130 road edges; over the 20,000-tick observation the longest dispatched path has 92 vertices, minimum protected Houses is 3, and 14,999 post-warm-up ticks have incomplete protection. This measures the actually dispatched routes, not a theoretical maximum. No balance changes were made to force full-city coverage.

## UI and presentation

`J` selects **Health Post $120**; `K` toggles Health. Key audit found `H` already bound to Help; I/U/D/F and F1–F9 remain unchanged. The overlay shades only House footprints: muted green protected, amber at risk, red sick; F1 can show exact risk. House inspection prominently lists state, risk and remaining duration, separately from Water and nearest Well. Health Post inspection prioritizes assigned workers, operation, priority, worker phase, target and route status, followed by explicitly City-wide protected/sick counts. It never invents last-visit attribution or a stock requirement.

Hover uses normal buildability, with a road-connection hint for the Post and existing House desirability/water previews. It performs no route matrix or Health prediction. Sick Houses have a small ochre marker with a dark stroke in the normal painter; Fire is separate. No saved visual state, texture work or stage switch accompanies sickness.

The optional schema-1 `health_post` visual role requires supported unmirrored Emperor Type-30 side 1, width 78, base length 3,200 on both first decode and deduplicated reuse. The built-in compatibility pack has no Health Post assignment. The bounded [Health Post asset audit](reverse/health-post-visual-audit.md) closes after 205 strict candidates and three local native City-v15 comparisons at 1×/2×/4×. Original-like Health Post remains unresolved. Missing visuals use the authored timber/plaster treatment pavilion with a pitched muted roof and small herb planters in `HealthPostFallbackRenderer.*`, through existing ground/depth and alpha-128 placement paths. The polished Well mesh and stage assets are unchanged. Health Worker remains a marker; no walker research was performed.

## Schema 17 and verification

Only City-v15 rule 1 writes schema 17. All building entries contain the three authoritative Health fields; non-Houses require zero. Dynamic Health couriers retain ordinary paths, edge progress, pending state, stable IDs and private cursors. Restore rejects unknown kinds/roles, risk outside 0–99, payloads, wrong owners/targets, concurrent active illness/protection, nonzero risk under active illness/protection, excessive future deadlines and insufficient headroom. Old schemas reject nonzero Health authority and never automatically migrate. Expired deadlines remain historical values. Route caches are rebuilt without advancing a tick.

Tests cover dry/wet placement-relative boundaries, risk 98/99, threshold, exact natural recovery and protection expiry, real cure and prevention, target priority/cyclic patrol, road break/wait/repair, two districts, demand/fire independence, owner Fire/staffing/pause, safe demolition and protection retention. Schema-17 roundtrips check illness start/middle/recovery, before/after arrival and expiry with identical future continuation. Live sick autosave/recovery retains its snapshot; the view's ordinary load remains paused and performs no cure. The 20k deterministic schedule includes disease, visits, roads, operations/priorities, Fire and rebuilding. The 100k endurance fills 46 buildings/26 couriers and checks disease/treatment, water relocation, Fire, industry relocation and 20 save/autosave/recovery roundtrips.

The SDL test checks 100 Health overlay toggles at both four and the maximum 20 Houses, 200 repeated hover events and road dragging with unchanged snapshots and zero World copies/commands/restores, BFS, route refreshes, asset decode/uploads and file I/O. The 20-House expansion uses genuinely earned funds. Risk/visits reuse revision caches; route searches are absent from risk updates and rendering. Dedicated fallback tests check 1×/2×/4× visibility, one-cell base bounds, finite transforms, caller blend state and alpha 128. Existing old-profile, Water, Well, House-stage, road, Fire, demolition, menu and recovery tests remain part of full CTest.

## Validation on 2026-10-02

Starting and final HEAD: `844358dbf64860f12bbc1766e9a7783d1af1ad2e`. The starting working tree was clean. No commit, push, tag or release was created; implementation changes remain reviewable in the working tree.

| Check | Result |
|---|---|
| Debug full CTest | 72/72 passed, 866.23 seconds |
| Release full CTest | 72/72 passed; final packaging build repeated all 72, 19.53 seconds |
| ASan/UBSan full CTest | 72/72 passed, 236.93 seconds, no sanitizer findings |
| Final UI/fallback/profile checks after toolbar/help polish | 3/3 passed in Debug and ASan/UBSan; maximum-20-House UI extension also passed in both; included in final Release full suite |
| City-v15 deterministic continuation | 20,000 ticks, matching snapshots throughout |
| City-v15 maximum-city endurance | 100,000 ticks, 46 buildings / 26 couriers, all invariants and 20 persistence/recovery roundtrips passed |
| macOS package | Release arm64, dependency relocation, recursive Mach-O verification, ad-hoc signing, ZIP extraction and relocated execution passed |

Reproducible checks use the existing CMake configurations: `ctest --test-dir build --output-on-failure`, `ctest --test-dir build-release --output-on-failure`, `ctest --test-dir build-sanitize --output-on-failure`, and `tools/package_macos.sh`. The focused tests are `sandbox-city-v15-{base,persistence,districts,determinism,endurance}`, `sandbox-city-v15-view` and `health-post-fallback-presentation`. Synthetic fixtures contain no original data. Local logs, comparison runners, original-pixel captures and test preference roots remain ignored.

Native production scene review inspected all three original candidates and the authored fallback at actual 1×/2×/4×, with the required neighboring building roles. Those comparisons used scripted SDL events and ordinary paid simulation commands, and do not constitute a manual construction playthrough. Dedicated render tests separately verify the alpha-128 preview and sickness/Health overlay paths.

Actual desktop keyboard input in the local bundle verified pause/resume, H Help, K Health, paid starter revenue and F5 schema-17 save. At tick 846 the unchanged dry starter had 195 funds, 175 actual collected taxes and risk 24 on each House; there was no free Water or Health protection. At tick 4,463 all four Houses were visibly sick until 4,600, with risk zero, 7 Pottery and 8 Food each; ordinary missed demands had reduced each to three residents. F9 and a new-process load retained the exact World snapshot at 4,463, paused, without cure. The Health toolbar price is fully visible. Shortened Help lines were verified in the final bundle after restarting.

**The requested full 20–30 minute mouse-driven playtest remains open.** Coordinate clicks and drags through the available computer-control tool did not reach the intended SDL positions. An ignored diagnostic build observed actual left-button down/up at `(0,0)` and other incorrect coordinates; keyboard input worked. Fullscreen additionally produced `noWindowsAvailable`. No input workaround or simulation shortcut was added to production. Paid Well/Post construction, first visit, disconnected districts, cure, burning Post, Water removal and recovery are verified by deterministic core/SDL tests, but their complete actual-pointer playthrough and the six subjective balance questions are not claimed as accepted. City-v11 defaults remain unchanged.

Packaging remains a dirty-workspace local test candidate, without publication, project-license clearance, Developer ID or notarization. The ZIP contains the executable, actual dynamic dependencies, notices and metadata-only compatibility resources; it excludes original assets, decoded images, saves and private review files.
