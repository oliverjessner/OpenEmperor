# City v10: scalable city core

`sandbox-city-v10` is an OpenEmperor-authored rule profile. It scales the City-v9 systems without claiming to reconstruct Emperor's economy. City-v1 through City-v9 keep their historical limits, IDs, save schemas and behavior.

## Stable entities

City-v10 stores buildings and couriers in vectors sorted by stable 32-bit IDs. An ID identifies an entity and is never an array offset. `next_building_id` and `next_courier_id` increase monotonically and IDs are not reused. Building kind and courier role are the only authorities for simulation and presentation behavior.

The profile permits 20 Households, four Clay Sources, four Potteries, two Warehouses, two Farms and two Service Posts. This gives a maximum of 34 buildings and 14 couriers: one per Clay Source, Pottery, Warehouse, Farm and Service Post. The existing construction costs, capacities, production timing, Household demand, population, workforce, taxes and Service coverage duration are unchanged from City-v9.

## District logistics

Each Clay courier cyclically selects a reachable Pottery with capacity. Each Pottery courier does the same for Warehouses. Every Warehouse owns a Household supplier, every Farm owns a Food courier, and every Service Post owns a Service walker. Each courier has its own last-target cursor. A dispatched target remains fixed until the trip completes, including while a broken road is waiting for repair.

Route caches contain only the source-to-target pairs needed by the courier's role. They rebuild after a road or building revision and remain bounded by the profile limits. Reservations are accumulated by target, so concurrent suppliers cannot overbook a House, Pottery or Warehouse. Two disconnected road networks therefore operate as separate districts; damage in one does not stop the other.

## Goal and saves

The authored City-v10 goal requires at least ten placed Houses, eight Level-2 Houses and population 100. Reaching it does not stop the simulation. The paid Demo remains the City-v9 starter: three Houses and one of every production/service building for 980 funds.

Schema 10 writes variable-length `buildings` and `couriers` arrays in ascending ID order, plus both monotonic next-ID counters and each courier's target cursor. It rejects zero, duplicate or noncanonical IDs, missing owner/target references, invalid kinds and roles, or next IDs that do not exceed every stored ID. Schemas 1 through 9 retain their historical fixed list lengths and are not migrated to City-v10.

Synthetic acceptance tests create the full 34-building/14-courier city through normal commands, run two disconnected districts, break and repair one district, round-trip a large schema-10 save, compare 20,000 deterministic ticks and run a 100,000-tick endurance check. No original data is used.
