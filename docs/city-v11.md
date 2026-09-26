# City-v11 Markets and sustainable economy

`sandbox-city-v11` is an OpenEmperor-authored extension of City-v10. It keeps the same goods, Household demand, levels, population, taxes, Service, goal, costs, footprints and entity limits. It changes the delivery path:

```text
ClaySource -> Pottery -> Warehouse -> Market -> Household
Farm -> Market -> Household
ServicePost -> Household
```

A Market costs 140, requires four workers, occupies one cell, stores 16 Pottery and 16 Food, and owns one courier for each outbound good. Warehouse and Farm couriers deliver only to reachable Markets. Inbound trips may finish while a Market is unstaffed; new outbound dispatch waits for staffing. Stable-ID cyclic selection, reservations, fixed active targets, BFS, road protection and rerouting are shared with City-v10. A Market creates no goods.

## Rule versions

Schema 11 stores both City-v11 rule versions. `rules.version` selects behavior independently from `schema_version`:

| Rule | Clay | Pottery work | Food | Road edge | Move-in grace |
|---|---:|---:|---:|---:|---:|
| v1 | 100 ticks | 150 processing ticks | 80 ticks | 10 ticks | none |
| v2 | 32 ticks | 64 processing ticks | 32 ticks | 5 ticks | 800 ticks |

The Pottery start tick still supplies no processing progress, so one v2 recipe occupies 65 ticks from start through completion. All capacities, two-Clay recipe input, courier load, costs, demand intervals and taxes remain unchanged. Existing v1 saves restore and continue with v1. New City-v11 Worlds use v2. Unknown versions are rejected; there is no automatic upgrade.

The 800-tick grace is derived only from `ticks - placed_tick`. Demand still runs every 400 ticks, records missing inputs normally, grants no goods, tax or growth, and consumes nothing on failure. During the grace only the population decrement is suppressed. It expires exactly at tick 800 for a House placed at tick 0.

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

The profile starts with 1,300 funds and permits 20 Houses, four Clay Sources, four Potteries, two Warehouses, two Farms, two Service Posts and four Markets. Disconnected roads define local districts because couriers consider only reachable targets. The Inspector shows Market stock and inbound reservations separately, staffing, both distributor phases/statuses, target and cargo. A House shows current missing Pottery/Food/Service, historical last demand, population/capacity and remaining grace.

Market graphics and couriers use OpenEmperor diagnostic fallbacks. These balance values and mechanics do not claim to reconstruct Emperor's economy. City-v10 remains the menu default until City-v11 receives an actual interactive desktop playtest.
