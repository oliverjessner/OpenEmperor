# City v7 rules

`sandbox-city-v7` is an OpenEmperor-authored prototype rule set. It extends City-v6 with Food, one Farm, direct Farm-to-Household delivery, dual-supply household demand, developing Houses and level-dependent taxes. These values and mechanics are project design choices, not reconstructed _Emperor_ mechanics.

## Construction and workforce

City-v7 starts with 1,000 funds. Roads cost 2, Clay Sources 120, Potteries 180, the Warehouse 150, Households 80 and the single Farm 160. Removal is free and gives no refund. Only a fully validated changed construction command is charged.

Each Household supplies 8 workers. Clay Sources require 4, Potteries 6, the Warehouse 2 and the Farm 4. Complete staffing is allocated in ascending Building ID order at the start of every tick. An unstaffed producer pauses progress and cannot begin a new dispatch; an already moving courier completes its trip. Workforce and staffing remain derived values.

The paid demo builds one Clay Source, Pottery, Warehouse, Farm, two Households and twelve Roads for 794 funds, leaving 206. The Farm branch joins the downstream Household road network because Food travels directly from Farm to House.

## Food and delivery

The Farm produces one Food every 80 staffed ticks and holds at most 12. Courier 6 carries at most 4 Food directly to a reachable Household. It uses a separate cyclic target cursor and separate target-specific route cache from Pottery distribution. Each Household holds at most 8 Food and has an independent incoming Food reservation, so Food and Pottery may be in transit simultaneously.

The top-status Food number is Farm output plus Food already stored in placed Households. Transit cargo is shown separately by the courier marker and inspector.

The conservation check is:

```text
food produced = Farm output
              + Food courier cargo
              + Household Food stock
              + Household Food consumed
```

Reservations reserve capacity and are not counted as physical Food.

## Demand, development and tax

Every 400 ticks, a City-v7 Household requires one Pottery and one Food at the same time. It consumes both and records a fulfilled demand only when both are present. If either is absent, it records a miss and consumes neither.

House level is derived from fulfilled dual-supply demands:

- Level 0: fewer than 2 fulfilled demands.
- Level 1: 2 through 4 fulfilled demands.
- Level 2: at least 5 fulfilled demands.

Tax uses the level after the successful demand: fulfillment 1 pays 25, fulfillments 2–4 pay 40 each, and fulfillment 5 onward pays 60 each. Missed demand pays nothing. Per-House tax contribution is derived from this fixed monotonic schedule rather than saved separately.

The goal is four placed Households at Level 2. Reaching it does not stop simulation. City-v6 retains its Pottery-only demand, flat 25 tax and three-demand goal.

## Persistence and measured loop

Schema 7 stores authoritative Food inventory, reservations, production and consumption totals, Courier 6 and the Food distribution cursor. House level, workforce, staffing and goal state are derived. Schemas 1–6 retain their previous array sizes and profile meanings.

The synthetic acceptance loop uses normal commands and ticks. Its current diagnostic values are: first Food delivery tick 139, first fulfilled dual demand tick 800, first Level 1 tick 1,200, paid expansion tick 2,000, and four-House Level-2 goal tick 4,000. These are balance diagnostics, not compatibility claims.
