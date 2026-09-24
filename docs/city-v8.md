# City v8: service coverage

`sandbox-city-v8` is an OpenEmperor-authored gameplay profile. It extends City v7 with one Service Post and a civic walker whose visits give Houses temporary service coverage. These rules are project design choices and do not claim to reconstruct _Emperor_'s original service or walker systems.

## Construction and workforce

City v8 starts with 1,000 funds and retains City v7 costs. Roads cost 2, Clay Sources 120, Potteries 180, the Warehouse 150, Houses 80 and the Farm 160. The one Service Post costs 100. Removal gives no refund, and rejected or unchanged commands spend nothing.

Each House supplies 8 workers. Clay Sources require 4, Potteries 6, the Warehouse 2, the Farm 4 and the Service Post 2. Staffing allocates complete requirements in ascending stable Building ID order. The Service Post is ID 11, so it loses staffing first when two Houses provide only 16 workers after the core and Farm consume all 16. Three Houses provide 24 workers and staff the 18-worker starter completely.

The paid demo places one Clay Source, Pottery, Warehouse, Farm, Service Post, three Houses and fifteen Roads. It spends 980 and leaves 20 funds. It searches the buildability mask for its 12×5 pattern and runs ordinary paid commands.

## Service walker and roads

Courier 7 belongs to the Service Post and carries no physical good: cargo and reservations remain zero. It uses the ordinary bounded road BFS, edge movement, road-revision invalidation, protected current edge and rerouting logic. Directly adjacent buildings without an intervening sandbox Road are not connected.

When idle and staffed, the walker selects the next placed, reachable House cyclically after its separate `last_dispatched_service_household` cursor. An unreachable House is skipped, so it cannot block service to later reachable Houses. On actual arrival, the House receives coverage until `arrival_tick + 1200`. Coverage is active exactly while the current tick is lower than that value. It expires during other journeys and can be renewed only by another arrival. An active trip finishes even if later workforce allocation leaves the Service Post unstaffed.

## Demand, levels, taxes and goal

Every 400 ticks, a City-v8 House fulfills demand only when it simultaneously has one Pottery, one Food and active Service coverage. Success consumes one of each physical good, records one fulfillment and pays the unchanged 25/40/60 level tax. If any input is absent, the demand misses and consumes neither Food nor Pottery.

House levels remain based on historical fulfilled demand: level 0 below 2, level 1 from 2 through 4, and level 2 from 5 onward. The goal is four placed level-2 Houses. It remains true after reaching it even if later service coverage expires.

## Persistence and boundaries

Schema 8 stores exactly 11 building entries and 7 courier entries, including each House's authoritative `service_until_tick`, Courier 7's normal route state and the separate service target cursor. Active/remaining/covered values are derived. Schemas 1–7 retain their historical lengths and profiles; schema 7 remains City v7 and never acquires a service requirement or phantom Service objects.

The service system never reads or changes original map or asset data. The Service Post and walker use OpenEmperor fallback markers; this milestone adds no original-asset mapping and no new ware.
