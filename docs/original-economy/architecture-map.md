# Original economy: architecture and contracts

The optional **City-v16 Rule10 / schema26 / policy3 / development revision27**
has paid production, transport, House evolution, taxes and recovery. Full original
parity remains partial; see [the play guide](player-handoff.md) and [Known Issues](../../KNOWN_ISSUES.md).

## Versions and source preparation

| Generation | Saved contract and scope |
|---|---|
| Rule7 / schema23 | Frozen original House state, immutable model binding and scheduler; no active economy. |
| Rule8 / schema24 | Frozen paid Well72 desirability sources; no water, population or production. |
| Rule9 / schema25 | Frozen neutral six-goods ledger; real World goods remain unsupported/zero. |
| Rule10 / schema26 / revision27 | Current explicit paid economy; incompatible development revisions are refused. |
| Rule11 / schema27 / revision27 | Separate explicit paid economy with authored FireWatch and actual Inspector visits; bounded fire/tax provenance. |
| Rule12 / schema28 / revision27 | Separate explicit sustainable-economy contract: three paid Food Farm/Field chains and installed-worker upkeep every 816 placement-relative ticks; retains paid Rule11 fire protection. |
| Rule13 / schema29 / revision27 | Separate authored needs-aware Warehouse/Market goods selection; inherits Rule12's economy, fire protection and existing limits. |
| Rule14 / schema30 / revision27 | Separate cardinal-water House query; inherits Rule13 logistics and unchanged economy/bounds, with immutable source terrain. |
| Rule15 / schema31 / revision27 | Explicit authored Inspector-coordination experiment; inherits Rule14, skips other actual incoming Inspector commitments and unexpired real protection for new dispatches. Elite17 retention remains unproved. |
| Rule16 / schema32 / revision27 | Separate authored deadline-aware Inspector dispatch; inherits the cardinal-water/market/economy contracts, ranks physical deadlines without building-class preference and coalesces usable incoming trips. Retention assessment is separate. |
| Rule17 / schema33 / revision27 | Separate authored deadline-aware service selection; inherits Rule16's fire policy and all economy/bounds, ranks actual expiry and physical travel, coalesces real same-role visits and retains a complete canonical House cursor. |
| Rule18 / schema34 / revision27 | Separate authored deadline-aware Market-to-House Food selection; inherits Rule17's services, fire and economy. Physical stock, real incoming cargo and prepared route timing determine urgency; production, Farm-to-Market selection and all bounds remain unchanged. |

Rules1–14 and schemas1–30 retain their identities and canonical bytes. City-v11
Rule3 and City-v16 Rule4 defaults are unchanged; no automatic migration opts older
saves into Original economy. Bridge/Ramp infrastructure retains its existing contracts.
Rule11 inherits the unchanged Rule10 economic values and uses its own fire guard;
Rule10 still rejects FireWatch. The [fire protection contract](oe36-fire-protection.md)
defines its authored building, actual arrivals and separate schema27 validation.
Rule12 has a separate create/restore and menu/CLI choice. It preserves original
model values, taxes, harvest periods, needs, immigration, goods, services and fire
mechanics. Only its authored Food count and upkeep interval differ; see the
[sustainable-economy contract](oe37-sustainable-economy.md). No save is migrated.

Rule13 has its own exact create/restore, **M Needs-aware logistics** menu entry
and `--original-market-logistics` CLI option. Its selection policy is authored,
not recovered Emperor behavior. Original model values, prices, production,
consumption, taxes, 816-tick installed upkeep, three Food chains, fire protection,
body/courier bounds and physical transport remain inherited from Rule12.
The [market diagnosis](oe39-market-diagnosis.md) records the Rule12 cause;
the separate [retention assessment](oe39-market-retention.md) evaluates Rule13
without treating a new selector as proof of sustained supply or fire safety.

Rule14 has its own exact create/restore, **W Cardinal-water economy** menu entry
and `--original-cardinal-water` option. The evidenced query adds10 once when
raw original Terrain bit4 occurs directly outside a paid House's cardinal edge.
Diagonal/interior water does not count. Common bodies use2×2; an empty Elite
plot gets no bonus and the authored materialization transaction authorizes4×4.
The immutable source-hash-bound masks are prepared eagerly, never serialized or
read in frames/ticks. Restore independently reconstructs them from the original
map. Cell DES remains−100…100; only this recipient query may reach110. The
[OE40 investigation](oe40-desirability-investigation.md) separates original
query evidence and restricted adapter lifetime from actual paid Xia/Normal16→17
at tick7333 (raw70+10, current/next needs met). Its following 20,400 ticks lose
supply/type/fire retention and have −3241 operating cashflow; stable Elite17
retention and financing remain unproved. Historical Rule12/13's source-only
DES79 bound is unchanged; no old save or model is migrated.
The measured site's bit4 neighbors also carry stored`0x80000` and lie outside
the shared Diamond. Its legal body and paid result establish the immutable raw
snapshot adapter, not a visible shoreline or original post-load/runtime parity;
the evidenced helper has no off-map filter and the predicate remains unchanged.

Rule15 has separate create/restore APIs, **I Inspector coordination** and
`--original-inspector-coordination`. Its exact schema31 marker is
`authored-inspector-coordination-v1`; the inherited market/cardinal markers and
live source-bound terrain remain mandatory. The bounded selector examines
existing saved physical trips, grants no coverage before actual arrival and
adds no saved cache. Paused/burning sources do not cancel a moving Inspector;
waiting actors do not reserve targets. A road repair can legitimately resume a
trip after a replacement was dispatched, so restore does not forbid all duplicate
destinations. All fire numbers, prices, needs and installed upkeep remain exact.
The [OE44 comparison](oe44-elite17-stability.md) documents the measured duplicate
work and failed retention windows; this opt-in is not a stable-Elite17 promise.

Rule16 has separate `create_original_deadline_fire_dispatch` and
`restore_original_deadline_fire_dispatch` APIs, **D Deadline-aware fire dispatch**
and `--original-deadline-fire-dispatch`. Its exact schema32 marker is
`rules.deadline_fire_dispatch: "authored-deadline-fire-dispatch-v1"`.
The inherited market/cardinal markers, full source-map/model identities,
immutable live-rederived terrain and Policy3 remain mandatory. The exact Rule15
coordination predicate stays Rule15-only; Rule16 selects its own dispatcher.
There is no migration, new saved mutable authority or change to Rules1–15,
general defaults, original models, needs, costs, limits, production, consumption,
movement speed, fire-risk growth, incident duration, protection duration or
816-tick installed upkeep.

For an idle, operating, staffed Inspector, the selector reads existing bounded
cached routes and real saved outbound trips. It predicts arrival from physical
edge count and progress, and an ignition deadline from the unchanged
placement-relative risk kernel and actual protection expiry. A currently burning
target that can be reached before natural expiry contributes that expiry as its
response deadline; a later arrival competes to prevent the next incident.
There is one ordering for eligible Houses, Markets, Watches and other paid
buildings, with no unconditional burning-first tier or Elite preference.
The initial pivot ranks physically feasible arrivals before late ones;
feasible candidates use the smallest arrival slack, while late candidates use
the greatest lateness. Ties use earlier deadline, shorter physical round trip
and the existing canonical cursor. A matching moving Inspector coalesces a new
dispatch if its ETA meets the target deadline or is no later than the candidate's
arrival. Route-pending or waiting actors promise no arrival. Source pause/fire
does not cancel a trip already moving.

One bounded linear scan can replace a feasible pivot with an earlier-deadline
candidate `k` whose own outward arrival meets its deadline. It does so only
when visiting `k` and physically returning first still lets the pivot meet its
original arrival deadline, while the pivot's full
physical outward/return trip would make `k` miss its deadline. A response to an
existing fire must arrive strictly before natural expiry; an arrival at the
predicted ignition tick can prevent ignition because movement precedes fire.
The same canonical ranking breaks ties among qualifying alternatives. This is
one pivot and one alternate scan over existing cached routes, with no all-pairs
graph, saved visit plan, extra navigation or grant of two future visits. Only
the selected immediate trip dispatches; after its real return, ordinary World
dispatch evaluates the current state again. Predictions never grant protection
or reset risk: only actual arrivals invoke the unchanged fire kernel.

The fully paid `paid_return_blocking_exchange` regression in
`DeadlineInspectorWorldTests.cpp` follows both real arrivals and an exact
restored continuation, distinguishing the short-first Rule16 order from Rule15.
It is a bounded dispatch regression, not city retention evidence. Rules13–15
retain their own unchanged selection contracts.

The [OE45 assessment](oe45-fire-coverage.md) separately records a paid
near-Watch/grid city with20400 consecutive Elite17 ticks,400/400 current needs,
zero target fire/devolution/vacancy and+4951 operating cashflow. All twelve other
Houses remain inhabited with measured residual deficits; other buildings still
burn. The finite paid result does not establish indefinite stability or universal
fairness, and selecting the mode alone does not reproduce the plan.

Rule17 has separate `create_original_deadline_service_dispatch` and
`restore_original_deadline_service_dispatch` APIs, **T Deadline-aware services**
and `--original-deadline-service-dispatch`. Schema33 requires
`rules.deadline_service_dispatch: "authored-deadline-service-dispatch-v1"`, plus
the exact inherited fire, market and cardinal markers. The Rule16 predicate and
restore remain exact16; its unchanged fire selector is explicitly inherited17.
All nine service roles retain their existing actual visit eligibility, Paper
input, workforce, source operation/fire gates and prepared navigation.

The service candidate scan uses the complete canonical paid House list as its
cursor, then tests present occupancy, visit admission, fire and route legality.
A burning last destination remains a cursor anchor. Deadlines come from actual
last-arrival witnesses and the existing40/96 countdowns on phase35, independent
of whether inspection occurs before or after that tick's decay. A never-visited
House uses its actual first-admission tick. Physical outward ETA includes the
ordinary dispatch/movement boundary; actual incoming ETA uses remaining edges
and progress. Paused/burning sources do not cancel already begun visits.

Negative arrival slack precedes positive slack, with greatest lateness first;
future candidates use least arrival slack. Ties use earlier expiry, shorter real
round trip, then the canonical cursor. Another enabled, nonwaiting, outbound
same-role visitor coalesces a candidate only when its real ETA meets the expiry
or is no later than the candidate's. A faster late replacement may still compete.
Returning, waiting, pending or merely selected actors grant no service. The scan
uses existing bounded vectors/routes; no new World copy, scheduler, Navigator,
all-pairs graph, mutable queue or saved field is introduced. Real arrival alone
applies the unchanged timer. Food selection, Goods logistics, costs, installed
upkeep, construction limits and fire timing are separate unchanged contracts.
The [OE46 assessment](oe46-citywide-reliability.md) separates legal paid controls,
actual citywide events and finite gameplay acceptance from this authored rule.

Rule18 inherits those service and Inspector dispatchers explicitly, retaining
their exact historical predicates. Only Market-to-House Food selection changes:
the first physical depletion deadline precedes earlier actual arrival, complete
visit/return duration and the complete canonical cursor. The deadline uses actual
portions and the existing40/397 modulo816 consumption clock. Only real loaded,
outbound, nonpending Food cargo with usable remaining path/progress can extend
the bounded forecast; late incoming cannot erase an earlier empty interval.
Ordinary headroom reserves every actual load even when it cannot be predicted as
timely coverage. Explicit source pause returns cargo; source fire or staff loss
does not invalidate already begun movement. No forecast becomes saved authority,
and only actual arrival books stock/quality. Farm-to-Market selection and all
production, capacities, costs, timers, movement and consumption remain unchanged.
The [OE47 assessment](oe47-food-reliability.md) records the finite paid improvement
and the residual target/Farm-fire and resupply gap separately.

The [OE48 assessment](oe48-citywide-fire-reliability.md) changes no production
dispatcher, saved field, rule or schema. Its explicit QA player plan
`oe48-watch-phase1-third-food-three-markets` buys the second Watch and third
Food chain earlier, plus a third paid Market within existing limits. The real
5395→25795 window retains Elite17 with400/400 needs and no target fire, and all
thirteen occupied Houses complete5200/5200 current evaluations. Five House fires/
2915 burning ticks remain, including individual fire regressions. This meets
the finite intermediate goal, not full city stability or original parity.
Its naturally later entry also changes the observation calendar;27 pre-window
Food misses remain recorded. Rule19/schema35 stays unallocated and Rules1–18/
schemas1–34, all defaults, prices, timers, limits and physical authority stay exact.

`ModelData` reads the selected installation's `Model/EmperorBuildingModels.txt`
within its canonical root: regular file, at most 256 KiB, lines at most 4096 bytes,
unique Building IDs and complete 13-field Building/24-field House grammar.
`OriginalRulesModel` prepares all 15 House rows and admitted named providers for
one Difficulty. Building values use checked signed `base * modifier / 100`,
truncating toward zero; House values use checked `clamp(base + modifier,-99,100)`.
The [model register](model-provenance.md) binds labels, units and evidence.

Create/restore receives a `shared_ptr<const OriginalRulesModel>`. Worlds never
search for files. Saved source identity includes format, SHA256, byte count and
Difficulty; prepared rules/tables and runtime pointers are not serialized.
F5 checks the actual file identity before atomic replacement; F9 prepares and
validates a new candidate before publication. Missing or changed sources reject
I/O without changing the live city or prior save. Frames and ticks perform no
model reading, hashing, preparation, asset loading or navigation preparation.

## Clock and update order

World ticks are the only saved clock. Mapping version 1 assigns one scheduler
phase per existing 20 Hz tick; this speed and calendar integration are authored.
For completed ticks `t`, the next dispatch is:

```text
phase = t % 51
step  = (t / 51) % 16
month = (t / 816) % 12
year  = t / 9792
```

Phase 35 decrements service timers; phase 39 evaluates evolution, followed by
six-goods and Food consumption only at step 0 or 7. First consumption completes
at ticks 40 and 397. A real arrival earlier in that tick can supply the evaluation
and consumption; the old 400-tick demand path does not run as a second authority.
Taxes use the pre-tick occupied/non-burning state every 816 ticks, before upkeep,
production, actual movement, fire, immigration and House dispatch.

The original static evidence is narrower than this integration. On the pinned
EXE recorded by [the model contract](../reverse/original-model-contract.md),
`0x4ac2e4..0x4ac2f9` and `0x4ac4c8..0x4ac4e7` establish phases 0..50;
`0x4ac4e8` targets `0x4ac45e`/`0x4ac48f` for phases 35/39.
`0x519025..0x51905b` gates post-evolution consumption on selector0/7;
`0x4ac650..0x4ac67e`, `0x4ac6b8..0x4ac6c9`, `0x4ac794..0x4ac7b4` establish
16 passes and a 12-value outer counter. The original year increment at
`0x4ac7ce..0x4ac7e9` uses outer value1, unlike the elapsed-year counter above.
These read-only findings do not establish original real-time speed or all
scheduler work. No original executable is run by this implementation.

## Paid sources, inventories and actual arrivals

The active bounds are 82 bodies, 58 couriers and 20 Houses. All bodies, fields and
plants are purchased through normal commands. Population belongs solely to
`OriginalHouseState`; old `Building.population` remains zero. Authored immigration
starts no earlier than 120 ticks after purchase and then admits up to two people
every 40 ticks, bounded by prepared capacity and an actual safe road entrance.
Residents provide deterministically allocated workers, including Elite residents;
this is an explicit adapter rather than recovered original labor policy.

Six House goods are Hemp, Ceramics, Tea, Silk, Bronzeware and Lacquerware.
HouseState is their sole House inventory; the typed ledger owns non-House stock,
cargo, reservations and historical counters. Pottery becomes Ceramics exactly
once on successful Warehouse arrival; the old Warehouse Pottery slot stays zero.
For every good, checked conservation requires:

```text
produced + imported = consumed + demolished + stock + cargo
```

Reservations commit source stock and destination/return space; they add no mass.
Only actual loading creates cargo and actual arrival credits the destination.
Transfer handles bind CourierId to a monotonic sequence; stale or duplicate
arrivals fail. Pause/disconnect preserves cargo and begun-edge safety, using the
same navigator for return and paid road repair. Goods/target rotation is bounded;
finite visit tests do not prove universal starvation freedom.

Rule13 and its explicit Rule14/15/16 descendants replace Warehouse-to-Market/Paper and Market-to-House commodity
selection. It queries actual occupied, operating, non-burning Houses, their
current model needs, incoming reservations and physically reachable cached routes.
Missing current goods precede current-stock buffers and usable next-stage goods;
next-stage eligibility uses actual DES and non-goods requirements, without a
Common-to-Elite conversion. A half-full House buffer triggers refill; a missing
current need takes precedence even above that threshold. Bronze/Lacquer
alternatives include actual stock and incoming commitments. Within a tier,
normalized stock per need precedes the existing saved target/good cursor for
deterministic tie rotation. Warehouse selection accounts for a staffed Market's
reachable House demand, its pool and incoming cargo; the staffed Paper input
chain remains available. This adds no actor, stock, grant or saved demand cache.
House capacity remains eight per good, cargo four and road movement five ticks
per edge. Loading, reservation, cancellation, return and actual-arrival authority
remain unchanged; no finite comparison proves universal fairness.

Recipes and 80-productive-tick harvest periods are authored. Every required input
is debited atomically with output credit: Wood+internal Ore→Bronze;
Bronze+Clay→Bronzeware; Lacquer+Wood→Lacquerware; RawSilk→Silk; Hemp→Paper.
Plants are separately purchased and owned. Wood/Bronze/RawSilk/Lacquer/Paper are
five auxiliary materials, not extra House goods. Exact source evidence is in
[the luxury contract](../reverse/original-luxury-source-contract.md).

Food has separate Wheat/Millet quantities, cargo and arrival witnesses. Authored
crop scores 20/80 feed the original quality blend only on positive House delivery.
Rules10/11 admit two paid Food Farms with one uniquely owned adjacent Field
each; Rules12/13/14/15 admit three within the unchanged global body/courier bounds.
Stock depletion resets quality; an authored sink consumes one unit per occupied
House at 0/7 boundaries and records real misses. Six-goods consumption uses the
current model row independently per good: Bronze/Lacquer satisfy needs as OR,
but both stocks are consumed. See [goods evidence](../reverse/original-goods-consumption-food-contract.md).

Nine services use paid 207/208/211/212/213/214/215/217/219 providers, real workers,
operation/fire/road gates and actual walker arrival. Medicine/culture grant only
their corresponding 96 timer; religions their own 40 timer. Availability or a
future trip grants nothing. Confucian 219 serves occupied Elite Houses and requires
real Paper delivery, consuming one Paper per new successful dispatch. Targeted
School→House visits and this Paper policy are authored; original Performer/Venue
chains remain incomplete. See [spawn](../reverse/original-service-spawn-contract.md),
[culture](../reverse/original-culture-source-contract.md) and
[religion](../reverse/original-religion-source-contract.md) evidence.

## Houses, geometry, finance and presentation

Common types 2–10 remain 2×2 and terminal at 10; Elite 11–17 are a separate family.
The paid 80-Funds Elite plot starts empty 11/2×2. At its first actual admission,
all 16 future claims, equal signed height, original protection, infrastructure,
road entrance and begun actor edges must pass before atomic 4×4/11→13 publication.
A conflict leaves the plot empty for a later normal attempt. Identity, stock,
paths and history stay intact; later 13→12 devolution never shrinks the body.
No Common→Elite conversion is inferred. [Lifecycle evidence](../reverse/original-house-lifecycle-contract.md)
distinguishes original family geometry from this authored placement transaction.

Evolution uses actual DES, water, quality, goods and nine timers at phase 39.
Prepared DES sources are paid Well72 and admitted service bodies in BuildingId
order; build/restore or permitted source removal rebuilds the cache, never frames
or ticks. Original wall blocking and full source-manager lifecycle remain open.
[Source evidence](../reverse/original-desirability-source-admission.md) does not
grant construction permission by analogy. Production/House demolition is blocked;
only neutral Well72 influence sources have the existing no-refund removal path.

Tax is authored direct collection of pre-tick `occupants * prepared TaxRate`
every 816 ticks, without an invented TaxOffice. Upkeep is 2 Funds per installed
required worker every 400 placement-relative ticks in Rules10/11 and every 816
in Rules12/13/14/15, including pause, fire and unstaffing.
Cumulative per-type taxable units and actual arrival/transition witnesses constrain
restore; they are bounded proofs rather than full event journals. Existing Funds
and accounting counters are the sole monetary authority. Purchases need available
Funds; real upkeep may cause debt. Overflow is checked before tick mutation.
The authored live goal needs six occupied non-burning Houses, one current Common
PlainCottage 5+, 50 residents, actual positive tax and nonnegative Funds.
[Tax evidence](../reverse/original-house-tax-contract.md) does not establish full
original taxation or original campaign goals.

Original House art uses verified type/body-compatible sprites for types 3–17;
empty 2×2 Elite plots retain named fallbacks. Eager preparation validates source,
layout, anchors and alpha within existing budgets. Drawing/picking share prepared
resolution and actual signed-height body; no graphical field enters a save.
Other buildings/walkers retain fallbacks; [image evidence](../reverse/original-house-visuals.md)
is separate from native human acceptance.

## Save safety and developer checks

Schemas26–33 validate complete identities, map/mask/policy/model binding, paid origin,
geometry, route/progress, capacities, conservation, timing and finance before
publication. Unknown/duplicate/missing/mistyped fields, incompatible revisions,
unsupported authority and impossible histories fail atomically; 8 MiB document and
32-level nesting limits remain. Recovery requires explicit OriginalProduction
access: a protected start and up to five 1200-tick checkpoints; opening one forks
history while preserving the source. LegacyOnly remains the API default.

Schema29 requires exact Rule13 identity and
`rules.market_logistics: "authored-needs-aware-market-v1"`. A missing or unknown
marker, or merely relabelling an old rule/schema header, fails closed. The marker
identifies immutable policy, not additional mutable authority. Rule12 remains exactly schema28;
F5/F9, menu load and Recovery retain each saved generation without conversion.

Schema30/Rule14, Schema31/Rule15 and Schema32/Rule16 keep their exact distinct
markers and create/restore boundaries. Schema32 requires its deadline-dispatch
marker plus the inherited market/cardinal markers; the Rule15 coordination marker
is not a Schema32 field. Changed/missing/duplicate/mistyped markers or header-only
relabels reject before publication. Terrain remains unsaved immutable context and
is rederived from the live complete source map on restore. F5/F9 and Recovery use
the selected generation's full source, geometry, accounting and trip checks.

Schema33/Rule17 adds only its exact service marker and inherits schema32's fire,
market, cardinal, policy3, model and complete-map terrain identities. No additional
mutable saved authority exists. Missing/unknown/duplicate/mistyped markers,
cross16/17 restores and header-only relabels fail closed. Historical schemas1–32
retain their exact bytes and generation; F5/F9, normal menu load and Recovery
select the explicit33 path without migration.

Schema34/Rule18 adds the exact
`rules.deadline_food_dispatch: "authored-deadline-food-dispatch-v1"` marker and
inherits schema33's service/fire/market/cardinal identities. Separate
`create_original_deadline_food_dispatch` and
`restore_original_deadline_food_dispatch` paths retain source-bound terrain and
model validation. No queue, forecast, deadline or cache becomes saved authority.
Historical Rules1–17 and schemas1–33 remain exact, without migration. The
[OE47 assessment](oe47-food-reliability.md) records causal Food evidence, paid
controls and the independent gameplay and compatibility gates.

See [testing.md](../testing.md) for build/sanitizer setup. With a configured `build` and your own data:

```sh
ctest --test-dir build --output-on-failure -R '^original-'
cmake --build build --parallel 2 --target openemperor-original-longplay-acceptance
python3 tools/test_original_longplay.py --binary build/openemperor-original-longplay-acceptance --data /path/to/game --output .local/original-economy/oe32/longplay-check --scenario starter --ticks 20000
```

The [paid progression check](paid-progression.md) uses the same runner with
`--scenario progression` and a new output under `.local/original-economy/oe34/`.
It waits for the actual World goal, then measures at least 20000 additional ticks;
goal acceptance and long-term operating stability are reported separately.

`sh tools/play_original_economy.sh --help` documents the empty developer session;
the play guide covers the menu starter. Dedicated runners remain under
`tools/test_original_*.py`; [core comparison](core-parity.md) covers pinned fork checks.
`tools/original_economy_baseline.py verify` needs the immutable [baseline lock](baseline-lock.json)
and private reference; never regenerate hashes to conceal legacy changes.
Reports belong under ignored `.local/`. Missing data, synthetic controls, software
screenshots, native delivery and human acceptance retain separate statuses.
