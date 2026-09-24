# City v9: population and dynamic workforce

`sandbox-city-v9` is an OpenEmperor-authored sandbox profile. It extends City v8 with Household population, development-dependent capacity, population growth and decline, and workforce supply derived from residents. These rules are project balance decisions. They do not claim to reproduce Emperor's migration, housing, workforce, or service systems.

## Population and House development

A newly placed House starts with 6 residents. Population never falls below 2. House level remains a historical result of fulfilled demands and never falls:

| House level | Fulfilled demands | Population capacity |
| --- | ---: | ---: |
| 0 | 0–1 | 6 |
| 1 | 2–4 | 10 |
| 2 | 5 or more | 16 |

Population changes only when the existing 400-tick House demand is evaluated. A fulfilled demand requires one Pottery, one Food, and active Service coverage. The World consumes both goods, increments `fulfilled_demand`, derives the resulting House level and capacity, grows the House by one resident if it is below that capacity, and then pays the existing level-based tax. This order makes the second success promote a population-6 House to Level 1 and then grow it to 7.

A missed demand consumes neither ware, increments `missed_demand`, and removes one resident when population is above 2. House level is not reduced. A developed House can therefore lose residents during a long supply or Service failure and grow again after supply recovers.

## Workforce and tick order

City v9 workforce supply is the sum of the populations of all placed Houses. City v8 and every earlier profile keep their existing eight-workers-per-House rule. Worker requirements remain Clay 4, Pottery 6, Warehouse 2, Farm 4, and Service Post 2. Complete requirements are allocated in ascending stable Building ID order; partial staffing is not allowed. When workers are scarce, lower-ID buildings are staffed first.

Each City v9 tick captures one derived staffing allocation before any state advances. The tick then performs:

1. Clay, Pottery, and Farm production using the tick-start staffing allocation.
2. House demand evaluation, including population changes and taxes.
3. Courier dispatch in stable Courier ID order using the same tick-start staffing allocation.
4. Courier movement and arrival.
5. Conservation, navigation, economy, Service, and population invariant checks.

A population change affects staffing on the next tick. This prevents one building from being considered staffed for production and unstaffed for dispatch within the same tick. An active courier still finishes its trip and return after its owner loses staffing; no new trip starts while the owner remains unstaffed. An active Pottery recipe pauses without losing input or progress and resumes automatically when enough population returns. The derived tick staffing allocation is never saved.

## Starter, expansion, and goal

The paid demo reuses the City v8 layout. It spends 980 of the initial 1,000 funds and places three Houses at population 6. Its 18 residents exactly staff one Clay source, one Pottery works, one Warehouse, one Farm, and one Service Post: 18 workers used out of 18 supplied.

A full two-Clay/two-Pottery buildout requires 28 workers. Four Level-0 Houses supply only 24, so reliable supply and development are required to bring all production online. Population itself pays no tax; only fulfilled House demand pays the unchanged 25/40/60 tax schedule.

The City v9 settlement goal requires all of the following:

- four placed Houses;
- every House at Level 2;
- total population of at least 48.

The simulation continues after reaching the goal.

## Persistence and presentation

City v9 alone writes OpenEmperor save schema 9. It retains exactly 11 building entries and 7 courier entries and adds `population` to every building entry. Non-House entries store zero. Total population, capacities, workforce, staffing, and goal status are derived and are not stored. The loader validates every placed House against the capacity derived from its restored `fulfilled_demand`; unplaced Houses and all other buildings must have zero population. Loading does not advance a tick.

Schemas 1–8 keep their profile identities and serialized fields. In particular, schema 8 remains City v8, has no `population` field, and continues to supply eight workers per placed House.

The normal HUD shows population, workers, Service, and goal progress. A selected House shows its level, population/capacity, supplied workers, stocks, coverage, demand history, and taxes. An unstaffed building reports its complete worker requirement. F1 lists the stable Building-ID staffing order. House graphics remain the existing presentation mapping at every level; City v9 adds no asset mapping or original-data research.

Run the demo directly with:

```sh
./build/openemperor --data /path/to/your/game-data \
  --sandbox Cities/Xia.map \
  --sandbox-rules sandbox-city-v9 \
  --sandbox-demo
```
