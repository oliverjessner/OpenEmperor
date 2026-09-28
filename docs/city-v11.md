# City-v11 Markets, operations and sustainable economy

`sandbox-city-v11` is an OpenEmperor-authored extension of City-v10. It keeps the same goods, Household demand, levels, population, taxes, Service, goal, costs, footprints and entity limits. It changes the delivery path:

```text
ClaySource -> Pottery -> Warehouse -> Market -> Household
Farm -> Market -> Household
ServicePost -> Household
```

A Market costs 140, requires four workers, occupies one cell, stores 16 Pottery and 16 Food, and owns one courier for each outbound good. Warehouse and Farm couriers deliver only to reachable Markets. Inbound trips may finish while a Market is unstaffed; new outbound dispatch waits for staffing. Stable-ID cyclic selection, reservations, fixed active targets, BFS, road protection and rerouting are shared with City-v10. A Market creates no goods.

## Rule versions

`rules.version` selects behavior independently from `schema_version`:

| Rule | Clay | Pottery work | Food | Road edge | Move-in grace |
|---|---:|---:|---:|---:|---:|
| v1 | 100 ticks | 150 processing ticks | 80 ticks | 10 ticks | none |
| v2 | 32 ticks | 64 processing ticks | 32 ticks | 5 ticks | 800 ticks |
| v3 | 32 ticks | 64 processing ticks | 32 ticks | 5 ticks | 800 ticks |

The Pottery start tick still supplies no processing progress, so one v2/v3 recipe occupies 65 ticks from start through completion. All capacities, two-Clay recipe input, courier load, costs, demand intervals and taxes remain unchanged. Schema 11 retains City-v11 v1/v2 exactly. Schema 12 belongs only to City-v11 v3 and stores the authoritative operation controls described below. Existing v1/v2 saves restore with their saved rules and never gain controls implicitly. New City-v11 Worlds use v3. Unknown rule/schema combinations are rejected.

## Operation and workforce controls

Rule v3 adds two fields to each placed Clay Source, Pottery, Warehouse, Farm, Service Post and Market: `operating_enabled` (default `true`) and `workforce_priority` (`High`, `Normal` or `Low`, default `Normal`). Houses and Roads have no controls. Inspector buttons issue typed, free commands against the selected stable `BuildingId`. These commands change no money, goods, population, entity IDs, Roads, route revision or cached routes. An identical setting is accepted as unchanged.

Staffing is one shared deterministic allocation used by production, dispatch and the UI projection. Paused operations are excluded. Active operations are ordered High, Normal, Low and then by ascending stable Building ID. Requirements remain all-or-nothing: a request that does not fit is skipped so a later smaller request may still receive workers. The allocation is captured once at tick start, so demand-driven population changes affect the next tick.

A paused operation retains its footprint, stock, reservations, recipe input and progress. It receives zero workers, makes no production progress and starts no new owner courier trip. A trip already outbound or returning finishes under the normal navigation and rerouting rules. Inbound deliveries to a paused destination are still accepted against ordinary capacity and reservation checks. Reactivation resumes the stored state without reset.

The HUD and Inspector distinguish **Available** population, **Assigned** workers, **Active demand** for enabled operations and **Installed demand** including paused operations. Current Clay, Pottery and Food diagnostics aggregate each physical storage and courier location separately from reservations and label historical production counters as totals.

The Load Save screen offers **Enable operation controls in a copy** only for a valid City-v11-v2 save. A confirmation explains that the new file uses rule v3. The copy preserves tick, population, treasury, goods, counters, service expiry, IDs, Roads, active paths, edge progress, cargo, reservations and recipes; it initializes controls to Running/Normal. It uses a new checked save path outside the original data root. The source save is not overwritten, and no simulation tick runs during conversion.

The 800-tick grace is derived only from `ticks - placed_tick`. Demand still runs every 400 ticks, records missing inputs normally, grants no goods, tax or growth, and consumes nothing on failure. During the grace only the population decrement is suppressed. It expires exactly at tick 800 for a House placed at tick 0.

## Playable-start guidance

The UI derives a read-only start diagnosis from `World`. It keeps missing building types, unstaffed buildings, courier routing status, unavailable goods, the Household demand clock and already paid taxes separate. Having enough workers for the currently placed buildings does not mean the revenue chain exists. A city with ClaySource, Pottery, Warehouse and Houses but no Farm, Market or ServicePost reports that tax is not yet possible and calculates the missing minimum as `farm_cost + market_cost + service_post_cost` (currently 400). Roads are explicitly excluded from that minimum.

The worker estimate compares actual population with the requirement after all six starter facilities exist, including the worker demand of facilities that are still missing. Fresh Houses needed for that estimate are `ceil(shortfall / household_initial_population)`. The diagnosis retains the full calculated count when the remaining House limit cannot accommodate it and reports that completion as impossible; the actionable suggestion remains bounded by the available slots. This is only a staffing estimate. It does not promise buildable land, road reachability or sustained future supply.

Before a validated City-v11-v2/v3 construction purchase, the app applies the proposed commands to a temporary restored World and diagnoses that post-purchase state. The minimum starter reserve is the cost of still-missing facilities plus the cost of the fresh Houses required to cover the resulting worker shortfall. A purchased facility therefore adds its real worker demand, while a purchased House contributes its real initial population and is neither charged nor reserved twice. Sequential duplicate/no-op roads cost zero in the temporary transaction. Roads remain explicitly outside the future reserve.

If post-purchase funds fall below that combined reserve, or the required House count exceeds the remaining limit, the app pauses simulation and asks **Build anyway** or **Cancel**. Cancel changes no World field. Approval revalidates and executes the ordinary command or road transaction once. Exact reserve equality does not warn. The existing exemption after real tax collection remains scoped to already-paying cities. The warning adds no credit, free stock, tax or automatic placement and changes neither rules version nor schema.

## Capacity and measured transport

Twenty Houses require 20 Pottery and 20 Food per 400 ticks. The old v1 maximum was structurally insufficient:

```text
4 Clay Sources * 400 / 100 = 16 Clay = 8 Pottery
2 Farms        * 400 / 80  = 10 Food
```

The pre-change v1 baseline separated this production limit from startup and transport effects. The staged four-House demo ended at tick 30,000 with population 21 and treasury 8,540, but each House fulfilled only 25 of its 50 demands in the final 20,000-tick window. The identical Tick-0 build fell to population 8 and treasury 280; a houses-first/service-before-Market order fell to population 8 and never earned tax. In both Tick-0 cases the first misses reduced the workforce and left either Market or Service dispatch unstaffed while real goods remained buffered. The existing two-district 20-House v1 layout ended its 60,000-tick observation at population 48 and never reached the goal; individual Houses fulfilled only 12–26 of 75 demands. These measurements show both a cold-start workforce loop and structural underproduction, rather than a missing-road error.

For v2, integer capacity over one 400-tick demand period is:

```text
4 Clay Sources * 400 / 32 = 50 Clay = 25 raw-material-limited Pottery
4 Potteries    * 400 / 65 = 24 processed Pottery
2 Farms        * 400 / 32 = 25 Food
```

The processing limit is 24 Pottery, a 20 percent reserve above 20-House demand; Food has a 25 percent reserve. These are theoretical production limits. Real delivery also depends on road length, return travel, four-unit loads, target cycling and backpressure. The v2 five-tick edge is profile-specific: testing showed that the old ten-tick edge could produce enough goods but could not move them through the documented 22 couriers to all 20 Houses.

The paid Tick-0 starter costs 1,200, leaves 100, and uses one Clay Source, Pottery, Warehouse, Farm, Service Post, Market, four Houses and 15 roads. It performs no hidden ticks and injects no inventory, coverage, population or money. In the compact synthetic layout, first Market Food/Pottery arrive at ticks 46/167, first House Food/Pottery at 71/192, the first complete demand and tax occur at tick 400, and first growth occurs at tick 800. After a 2,000-tick warm-up every House fulfilled all ten demands in a 4,000-tick window.

A paid two-district run reached the shared City-v10 goal at tick 6,000 with population 125 and construction spending 2,844. The following six demand periods were fully supplied and population rose to 159. A separate paid maximum build reached 20 Houses, four Clay Sources, four Potteries, two Warehouses, two Farms, two Service Posts and four Markets (38 buildings/22 couriers). After warm-up, every House fulfilled all six demands in a 2,400-tick window. This result applies to the compact tested roads; long routes can still reduce delivery below production capacity.

## Recovery and persistence

Synthetic played-state checks independently cut a Market inbound bridge, a residential outbound bridge and a Service-only branch. Each test runs until the affected real buffer or coverage expires and a demand misses, repairs the same road through an ordinary paid command, then observes a later fulfilled demand. No stock, population or workforce is injected. Schema-11 checkpoints cover the paid Tick-0 state, both grace boundaries, active inbound/outbound Market trips, first taxes, each outage and recovery, and the reached goal. Immediate snapshots and continued ticks remain equal after restore.

The operation-control recovery test starts from a normally played, fully supplied v2 city. It removes the Farm entrance with the normal road command, waits for real failed demands and population decline, then creates the explicit v3 state copy. At the measured crisis point (tick 12,000) population was 13, current Clay was 16, current Pottery 40 and current Food 34 across physical locations. The player sequence paused Clay and Pottery and set Farm, Market and Service to High, yielding 12 assigned workers for 12 active demand versus 22 installed demand. After the first real complete demand paid tax and grew population, Pottery and then Clay were restarted. The rescued world reached population 25 and collected 720 additional tax while the repaired but otherwise unchanged v2 control collected no further tax and did not regain population. This proves one bounded, buffered crisis is recoverable; it does not imply every depleted low-population city can recover.

The profile starts with 1,300 funds and permits 20 Houses, four Clay Sources, four Potteries, two Warehouses, two Farms, two Service Posts and four Markets. Disconnected roads define local districts because couriers consider only reachable targets. The Inspector shows Market stock and inbound reservations separately, staffing, both distributor phases/statuses, target and cargo. A House shows current missing Pottery/Food/Service, historical last demand, population/capacity and remaining grace.

For the exactly fingerprinted compatibility pack, Farm, ServicePost and Market use the metadata-only static preview selections documented in [the building visual profile](building-visual-profile.md); unknown data sets retain honest diagnostic fallbacks. Couriers without a curated role still use OpenEmperor markers. These balance values and mechanics do not claim to reconstruct Emperor's economy. City-v10 remains the menu default until City-v11 receives an actual interactive desktop playtest.
