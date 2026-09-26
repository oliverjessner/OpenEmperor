# City-v11 Markets and local goods distribution

`sandbox-city-v11` is an OpenEmperor-authored extension of City-v10. It keeps the same goods, Household needs, population, taxes, Service behavior, goal, 2×2 core footprints, stable IDs and deterministic tick order. It changes only the route by which Pottery and Food reach Houses:

```text
ClaySource -> Pottery -> Warehouse -> Market -> Household
Farm -> Market -> Household
ServicePost -> Household
```

A Market costs 140, requires four workers, occupies one cell, and stores up to 16 Pottery and 16 Food. It accepts already active inbound deliveries even when unstaffed. Its own two couriers start no new Household deliveries until the Market is staffed. One courier distributes Pottery and the other distributes Food. Warehouses and Farms each keep one supplier courier, but in City-v11 those suppliers target reachable Markets instead of Houses. Every courier owns its own stable-ID cyclic target cursor; full or unreachable targets do not block later valid targets.

City-v11 starts with 1,300 funds. The paid Demo costs 1,200 and leaves 100. It contains one Clay Source, Pottery, Warehouse, Farm, Service Post and Market, four Houses and 15 roads. House placement is staggered by 100 normal simulation ticks so startup does not remove enough workers to stall both Market and Service. This is an authored demo sequence, not an Emperor rule or a state injection.

The synthetic starter first reaches Market Pottery at tick 429, Market Food at 309, a House delivery at 479/359, and fulfillment/tax/population growth at tick 800. At 30,000 ticks it has population 21 and treasury 8,540. Taxes can pay for more Houses before another Market, which supplies the workforce needed for expansion. A central placement reduced the independently constructed six-leg average road route from 8.17 to 6.17 cells. These measurements establish a gameplay consequence but do not replace the pending extended human balance review; City-v10 therefore remains the menu default.

The profile permits 20 Houses, four Clay Sources, four Potteries, two Warehouses, two Farms, two Service Posts and four Markets: at most 38 buildings and 22 couriers. A 1×1 Market uses the existing orthogonal building-entrance query. Disconnected road components naturally form local districts because couriers consider only reachable targets. Market placement does not add coverage or modify Household demand; a House still requires Pottery, Food and active Service at each 400-tick demand.

Schema 11 stores City-v11's variable-length entity vectors, Market inventories, reservations, active paths and the four appended Market courier roles. Schemas 1–10 retain their exact profiles and semantics. Schema 10 rejects Markets and Market courier roles; there is no automatic City-v10 migration.

Market graphics and couriers use OpenEmperor diagnostic fallbacks. No original asset search or file-format interpretation was performed for this feature. These mechanics are prototype balance rules and do not claim to reconstruct Emperor's market system.
