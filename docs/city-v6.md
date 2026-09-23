# City v6 rules

`sandbox-city-v6` is an OpenEmperor-authored gameplay profile. It extends the unchanged Industry-v5 Clay → Pottery → Warehouse → Household simulation with a small economy. Its values and allocation rules are project design choices; they are not reconstructed _Emperor_ prices, workforce rules, taxes, or mission logic.

## Treasury and construction

A new city starts with 1000 integer funds. Successful changed commands cost 2 for a road, 120 for a Clay source, 180 for a Pottery, 150 for a Warehouse, and 80 for a Household. Removing a road is free and gives no refund. Replacing an existing road is an accepted no-op and costs nothing. Validation completes before money is deducted, so a rejected command cannot alter the world or treasury.

The authoritative schema-6 counters satisfy:

```text
treasury = 1000 + taxes_collected_total - construction_spent_total
```

## Workforce

Each placed Household supplies 8 workers. Clay sources require 4, Potteries require 6, and the Warehouse requires 2. Roads and Households require none. At most two Clay sources, two Potteries, one Warehouse and four Households may be placed.

Staffing is derived, never saved. At every City tick, buildings are considered in ascending stable `BuildingId` order. A building receives its complete requirement when enough workers remain; otherwise it is unstaffed, and later buildings are still considered. There is no partial staffing. An unstaffed Clay source does not extract or dispatch. An unstaffed Pottery pauses an active recipe without losing input or progress and does not dispatch. An unstaffed Warehouse does not start a Household delivery. A courier already traveling completes its outbound and return route.

## Taxes and goal

Every Household evaluates one demand after each 400 ticks following placement. A supplied demand consumes one Pottery, increments `fulfilled_demand`, and adds 25 funds and 25 to total taxes. An unmet demand increments `missed_demand` and changes no money.

The settlement goal is reached when all four Households are placed and each has at least three fulfilled demands. The goal is derived from Household state and remains reached as the simulation continues. It does not stop or mutate the world.

The City demo uses ordinary paid commands to place two Houses, one Clay source, one Pottery, one Warehouse, and nine connecting roads. It spends 628 and leaves 372 funds. The full two-source, two-Pottery, four-House layout costs another 482 in the synthetic acceptance layout, so successful supply and tax cycles are required before expansion. The acceptance scenario first collects tax at tick 800, expands at tick 1600, and reaches the goal at tick 2800.

Schema 6 saves the treasury, collected-tax total, and construction-spending total. Schemas 1–5 retain their original profiles and rules; in particular, an Industry-v5 save remains Industry-v5 and has no City economy state.
