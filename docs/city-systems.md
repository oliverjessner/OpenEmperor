# City systems

These are authored OpenEmperor rules, not recovered Emperor simulation. Current
City-v16 inherits Markets, Food, Service, population, operation controls, safe
demolition, Fire, desirability, Water and Health. A feature applies only to the
profiles/rule versions that introduce it. Historical saves retain their saved
rules; the [profile/schema table](gameplay-sandbox.md#legacy-profiles-and-save-compatibility)
records those boundaries. City-v16 maintenance, original-map permissions and its
optional Bridge/Ramp rules are documented in [City-v16](city-v16.md).

## Markets and logistics

From City-v11, the supply chains are:

```text
Clay Source -> Pottery -> Warehouse -> Market -> Household
Farm -> Market -> Household
Service Post -> Household
```

Markets cost 140 funds, require four workers, occupy 1×1, hold 16 Pottery and 16
Food, and own independent outbound couriers for each good. There are at most four
Markets. They create no goods or proximity coverage. Warehouse/Farm couriers
target reachable Markets; Market couriers target reachable Houses. Reservations
are target-specific capacity promises, not physical inventory. Source stock
becomes cargo at dispatch; cargo becomes destination stock only on actual
arrival. Loads hold at most four units. Active targets remain fixed through road
breaks, waiting and return travel. Inbound deliveries can complete while their
destination is paused, unstaffed or burning; new owner dispatch cannot start.

Buildings and couriers have stable monotone IDs, never collection offsets or
reused identities. City-v10 and later permit twenty Houses, four Clay Sources,
four Potteries, two Warehouses, two Farms and two Service Posts. Adding Markets,
two Watches, four Wells and two Health Posts gives the current maximum of 46
buildings and 26 couriers. Core Clay/Pottery/Warehouse/House foundations are 2×2
from City-v10; Farm, Service Post, Market and Fire Watch remain 1×1. Well and
Health Post geometry is version-specific under [Water](#water).

The saved cell is the rear/top footprint origin; the front cell is
`origin + (width-1,height-1)`. All occupied cells have the same owner. Placement
validates the whole footprint before spending or allocating IDs. Couriers use
roads orthogonally outside foundations, never building interiors as transit.
The shared bounded BFS chooses the shortest entrance pair, breaking equal
lengths by source-road then target-road storage order. Each courier cycles
independently over eligible reachable targets with capacity. Revision-bound
role-specific caches are derived. Separate disconnected districts need their
own reachable supply and service chains.

## Rule versions and demand

Production and movement intervals come from the active profile/rule lookup:

| City-v11 rule | Clay | Pottery processing | Food | Road edge | Population grace |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 100 ticks | 150 ticks | 80 ticks | 10 ticks | None |
| 2, 3, 4 | 32 ticks | 64 ticks | 32 ticks | 5 ticks | 800 ticks |

City-v12 and later inherit the latter rates. Pottery consumes two delivered Clay
when a recipe starts; its start tick contributes zero processing, so 64 processing
ticks require 65 ticks including that start. Buffers pause full production rather
than accumulating a backlog. Clay output and Pottery input/output each hold eight,
a Warehouse holds 32 Pottery, a Farm twelve Food, and a House eight of each good.
Rates and available transport capacity do not
guarantee every House is supplied on long routes.

House demand occurs every 400 ticks after placement. From City-v8 it needs one
Pottery, one Food and current Service coverage; from City-v12 the House must also
be free of fire, and from City-v15 free of illness. A failure consumes neither
good and pays no tax. A success consumes both goods and increments historical
fulfillment. Historical development is Level 0 below two successes, Level 1 at
two through four, and Level 2 from five onward. Current effective level also
applies [desirability](#desirability) and [Water](#water) caps. Successful demand
pays 25/40/60 according to the resulting effective level, including a promotion
on the second or fifth success.

Fresh Houses have six residents, capacities are 6/10/16 by effective level, and
the minimum population is two. At a demand deadline the mutually exclusive
population branches are: an over-cap House loses one resident regardless of
supply; otherwise a miss loses at most one after move-in grace; otherwise a
success grows by one if below capacity. Grace suppresses only missed-demand
population loss before age 800: it grants no goods, fulfillment, growth or tax,
and does not prevent over-cap decline. Layout changes never immediately truncate
residents or erase historical development. Residents supply workforce from
City-v9; a population change affects the next tick's allocation. The current goal
is ten placed Houses, eight **currently effective** Level-2 Houses and population
at least 100. It is derived and can be lost; simulation continues after success.

Service Posts cost 100 and need two workers. Actual walker arrival grants coverage
until `arrival_tick + 1200`, active strictly while that expiry is after the current
tick. Dispatch, proximity and planned visits grant none. Scalable couriers retain
their own `last_dispatched_target`; the global Service cursor belongs to the legacy
path. Pausing or losing staffing stops new trips while existing trips finish.
See [current coverage diagnosis](city-v16-service-coverage.md).

## Operation and workforce controls

City-v11 rule 3 and later systems store `operating_enabled` and
`workforce_priority` (High/Normal/Low). Working buildings start Running/Normal;
Houses, roads and Wells have no operation controls. Commands are free and typed
by stable Building ID. An identical setting is an unchanged no-op. They do not
change stock, money, IDs, roads, route revisions or caches.

Staffing allocates complete requirements to active operations in priority order,
then stable ID order. An operation that does not fit is skipped, allowing a later
smaller requirement to fit. Requirements are Clay 4, Pottery 6, Warehouse 2,
Farm 4, Service Post 2, Market 4, Fire Watch 2 and Health Post 2. Paused/burning
operations receive no workers; installed demand still includes them. One
allocation is captured before each tick and shared by production and dispatch.

Pause retains footprint, stock, reservations, recipe input and progress. It
stops production and new owned trips, while existing outbound/return trips and
valid inbound delivery retain ordinary navigation. Resume continues retained
state. HUD available/assigned/active/installed workforce and current stock remain
separate from lifetime totals. [Startup guidance](economy-start-guidance.md)
reports real missing infrastructure, pause, staffing, routing and pipeline delay;
its reserve warning does not promise future taxes or silently build facilities.

## Safe demolition

The ordinary City-v11 rule 4 and later generations support typed demolition through the inspector's
confirmation. The modal blocks ticks and map input; Escape, focus loss, resize
or Cancel changes no World authority. Confirm revalidates once. There is no
refund, stock discard or relocation command. Rebuilding pays the normal cost
and gets a fresh monotone ID.

| Kind | Required physical state |
| --- | --- |
| Clay Source, Farm | Empty output; owned courier idle and empty |
| Pottery | No input, active recipe, output or incoming reservation; owned courier idle and empty |
| Warehouse | No Pottery or incoming reservation; owned courier idle and empty |
| Market | No Food/Pottery or incoming reservations; both owned couriers idle and empty |
| Household | No Food/Pottery or incoming reservations |
| Service Post, Fire Watch, Health Post | Owned courier idle and empty |
| Well | No transport or inventory state |

Every kind also rejects fire or another active courier retaining it as target,
including a returning courier. An active recipe blocks even at progress zero.
Pause/priority never bypass these checks. Normal deliveries/demand must drain
stock; a depleted or disconnected city has no guaranteed drain/rescue mechanism.
All residents of a removed House leave immediately. Delivered Service/Health/
Fire protection on other buildings survives removal until its own expiry.

Removal erases active entities, releases exact footprint/role limits, increments
topology once and refreshes routes once. Other trips keep identity, begun edge,
cargo and reservations. Deleted idle-target/cyclic references are cleared.
`demolition_history` preserves removed extraction, production, consumption, tax,
construction spending and removal count so lifetime conservation still reconciles.
It contains no inventory, population, refund or entity tombstones. The explicit
City-v11 v3→v4 copy upgrade preserves the source file and all authority at the
same tick. Ordinary old loads acquire neither demolition nor a new schema.

Original-economy Rules10–18 retain their separate conservative refusal contract.
Explicit Rule19/schema35 adds only never-occupied Common2x2 Houses with no
retained history and paused, physically drained Clay Sources after real return.
It preserves retired paid identities, extraction, settled Clay history and closed
upkeep in a bounded journal, with no refund or ID reuse. Occupied/Elite Houses
and other production/service kinds remain refused. The ordinary resident-removal
and reference-clearing rules above do not apply to this new original-economy
slice. See [OE51 demolition](gameplay/oe51-demolition.md).

The separate authored Rule20/schema36 terrain experiment inherits classic
Rule6 city systems and its historical demolition contract. It has no Original
economy or Rule19 retirement journal; new permanent source-cell ramp claims
remain protected even after a road cut. See [OE53](rendering/oe53-terrain-integration.md).

Rule21/schema37 [authored land trade](original-economy/oe54-trade.md) separately
inherits classic Rule6. Two existing paid Warehouses support one finite
Pottery caravan, without new workers, storage or global limits. Imports reserve
real owner headroom before dispatch; only physical boundary/return arrivals
load goods, earn export receipts or credit stock. Bound Warehouse identities
remain permanent, while ordinary pause/fire and road-repair contracts apply.

## Fire

From City-v12, eligible unprotected non-burning buildings gain one risk every
20 placement-relative ticks after placement. At risk 100, risk/protection reset
and a 600-tick incident starts. Eligible kinds are Clay Source, Pottery, Warehouse,
Household, Farm, Service Post, Market and, where available, Health Post. Fire Watch,
Well, roads and empty cells are excluded. Operation pause does not stop structural
risk. There is no RNG, destruction, stock loss or additional saved risk clock.

Fire Watches cost 80, need two workers, occupy 1×1 and have a two-instance limit.
Each owns a cargo-free Inspector with zero inventory/reservations. Shared road
navigation prefers reachable burning targets, then other eligible targets; each
phase cycles in stable-ID order after that Inspector's private cursor. Actual
arrival extinguishes, resets risk and grants 2400 ticks of protection. Merely
dispatching or finding a path grants none. Expiry queries are strict (`deadline >
tick`), preserve historical expired values, and allow risk growth on a qualifying
expiry boundary.

Burning work stops staffing, production and new dispatch without changing the
player's operation flag, goods or recipe progress. Existing trips and inbound
deliveries remain ordinary. A burning House still supplies its current workforce,
but misses demand once without consumption or tax. Fire grants no additional
population penalty. Staffing is captured before the tick; movement/arrival
precedes Fire, so an arriving Inspector protects before risk grows. Fire started
late in the tick affects working operations on the next tick. The authoritative
fields are `fire_risk`, `fire_protection_until_tick` and `fire_until_tick`; old
profiles require zero. F selects Fire Watch. [Fire visuals](fire-visuals.md) and
[Inspector presentation](fire-inspector-visuals.md) do not change these rules.

## Desirability

From City-v13, minimum Manhattan distance between rectangular footprint cells
selects each building's quality contribution. Distance uses int64 intermediates.

| Source | Base impact |
| --- | ---: |
| Clay Source | −18 |
| Pottery | −22 |
| Warehouse | −10 |
| Farm | −4 |
| Market | +12 |
| Service Post | +10 |
| Fire Watch | +8 |
| House, Well, Health Post | 0 |

Distance 0–2 applies four quarters, 3–4 three, 5–6 two, 7–8 one; beyond eight
the contribution is zero. Compute each `base * quarters / 4` with signed division
toward zero, sum, then clamp to [−100,+100]. Source inspection retains individual
unclamped contributions. Roads, original terrain, operation, staffing, fire,
stock and delivered protection do not alter the score. Nearby Markets and Posts
therefore improve quality without supplying actual goods or visits.

| Score | Effective level cap | Capacity | Supplied-demand tax |
| --- | ---: | ---: | ---: |
| Below −20 | 0 | 6 | 25 |
| −20 through 9 | 1 | 10 | 40 |
| At least 10 | 2 | 16 | 60 |

Effective level is historical development capped by current quality and, from
City-v14, Water. Caps update immediately on placement/demolition, while demand
history and existing residents remain. Actual `taxes_paid_total` is authoritative
on Houses from schema 15; non-Houses keep zero. Current geometry cannot recreate
past tax rates. Lifetime taxes reconcile as demolition-history taxes plus active
House totals. Older profiles retain their historical formulas and fields.
The D overlay and House-stage selection display World queries, never new authority.

## Water

From City-v14, Wells cost 60, have a four-instance limit, need no workers, carry no
good and own no courier or operation controls. Inclusive minimum-footprint
Manhattan distance five covers a House; ties for nearest Well use lower Building
ID. Roads, terrain, staffing, pause, fire and quality are irrelevant to proximity.
A Well neither supplies goods/Service nor cures, protects or extinguishes.

Well and Health Post footprints are **1×1** in their original City-v14/15 rules
and City-v16 rule 1. City-v16 rules **2–6** use **2×2** for both. Placement, picking,
entrances, water/quality distances, demolition and restore use the saved explicit
rule version. Prices/workers/radius/limits do not change. There is no automatic
geometry migration.

A dry House has water level cap 0; a covered House cap 2. Water is not another
demand input: a dry supplied House still earns 25 and develops historical level.
Water loss immediately changes effective level/capacity, then ordinary deadlines
apply at most one resident loss; restoration unlocks retained historical level
subject to quality. Coverage, nearest source, level and overlay are derived,
never saved or transported. Wells are fire-immune and quality-neutral. I selects
Well and U displays Water; graphics are independently prepared presentation.

## Health

From City-v15, every healthy unprotected House gains risk at each positive
100-tick placement-relative boundary: +1 with Water, +3 without. At threshold 100,
risk/protection reset and illness lasts 1200 ticks. Risk does not grow while sick
or protected. Natural recovery/protection expire at their exact deadlines; a
qualifying risk boundary then applies immediately. Arithmetic validates future
deadline headroom before mutation or restore. The fields are `health_risk`,
`health_protection_until_tick` and `sick_until_tick`, zero on non-Houses and older
profiles. Risk must be 0–99 outside the transition. There is no extra clock/RNG.

Health Posts cost 120, require two workers and have a two-instance limit. Their
footprint follows [the versioned geometry](#water). They support operation/
priority, are fire-eligible and quality-neutral. Each cargo-free Health Worker
uses ordinary roads/caches and a private target cursor. Reachable sick Houses
come first (stable ID ties), then unprotected Houses by descending risk/stable
ID, then cyclic patrol. There is no visit reservation, global Health cursor or
alternative navigator. Pause, fire or lost staffing stops only new owned trips.

Only actual arrival resets risk, cures current illness and grants 2400-tick
protection. Healthy visits retain expired sickness history. An idle safe Post can
be demolished; existing protection elsewhere persists to expiry. A sick House
retains stock/workforce, misses its normal demand once and earns no tax/growth.
Simultaneous fire and sickness do not double consumption or population loss.
Illness does not directly change quality, Water, history or House artwork.
J selects the Post, K displays Health; [Worker visuals](health-walker-visuals.md)
remain presentation only.

## Tick, persistence and presentation boundaries

The complete City-v16 tick order is staffing capture → tick increment → production
→ Household demand/tax → maintenance → dispatch → movement/arrivals → Health →
Fire → invariants. Earlier profiles omit their unavailable phases. Arrival can
protect before risk updates, while a newly ill House affects its next demand.
In-flight paths, goods, reservations and exact deadlines persist; route caches,
occupancy indices, scores, coverage, workforce, capacity and goal are derived.
Validated restore advances no tick and supplies no cure, stock or free service.

Overlays, previews and House-stage art read the same World authority. They are
view-local, do not enter saves, and add no frame-time World copies, commands,
ticks, routing, file reads, decode or texture upload. Visual fallback, scripted
pixel checks, native delivery and human play acceptance remain separate. Current
acceptance lives in [input checks](testing-input-acceptance.md),
[long-play stability](city-v16-longplay-stability.md) and focused feature guides;
historical test counts or screenshots are not current acceptance.
