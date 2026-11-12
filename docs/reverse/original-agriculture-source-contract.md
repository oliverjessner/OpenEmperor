# Original agriculture: field, calendar, irrigation and harvest boundaries

This is a maintained source contract for the agriculture preparation work. It
extends the [production candidate register](../original-economy/oe55-production.md)
and the earlier [farm/field presentation investigation](original-presentation-correction.md#farm-bodies-and-field-state-boundary).
It does not admit a building, field, source marker, material, movement channel,
growth rule or save generation to `World`. All historical Rules1–23/schemas1–39
remain exact, including the authored Wheat/Millet contracts where supported.

## Sources and evidence

The fresh read-only investigation uses the installed PE32/i386 `Emperor.exe`,
4,809,081 bytes, SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
Static addresses below use image base `0x400000`; they are neither file offsets,
serialized save offsets nor OpenEmperor entity IDs.

`Model/EmperorBuildingModels.txt` is 34,218 bytes with SHA-256
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
`Model/FarmConfig.txt` is 771 bytes with SHA-256
`cda01409d5ec1f6a547642a1bb2c6cd8721bbc33e3d1aeda6e384caef56751d6`.
Both remain user-supplied input, not packaged configuration.

The private helper programs, bounded disassembly, table observations and source
guards are under ignored `.local/agri-01/research-crops/`. Each captured window
records its actual PE-section translation and source-byte SHA-256. EXE, model and
FarmConfig bytes, lengths, modes and modification times match before and after
each batch. No original executable was run; no personal saves were inspected.

The crop report `verified-crop-source-report.json` passes49 narrow assertions
and verifies30 captured windows against the original bytes:11,935 overlapping
decoded instruction lines /33,702 instruction bytes, with zero mismatches.
`table-observations.json` separately checks the typed crop/resource branches and
the Pump RTTI/table binding. Recheck those private observations with:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-01/research-crops/verify_contract.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-01/research-crops/audit_tables.py
```

The captured-window receipts retain exact read-only `objdump` arguments, source
section offsets and SHA-256 values. The check commands validate retained static
observations; they do not execute the original game or exercise new World rules.

- `MODEL_OBSERVED`: original model labels and raw columns; neither cost nor
  `SZE` establishes a complete purchase transaction or footprint.
- `EXE_OBSERVED`: the narrow static factory, call, dataflow and table operations
  described below. This does not establish live-game acceptance.
- `SOURCE_VERIFIED`: file identity and directly inspected source configuration or
  resource data. A configuration name does not establish that this EXE reads it.
- `AUTHORED`: existing OpenEmperor placement, clocks, ownership, yields, routes and
  units. These do not become original facts through matching graphics.
- `UNKNOWN`: the listed unclosed callers, predicates, units or lifecycles.
  `UNSUPPORTED`: they grant no new gameplay capability in this preparation.

## Farmhouse and field identities

The earlier factory chain is freshly reread: manager `0x42d540` creates through
`0x42d360`; Industrial factory `0x51c660` qualifies field predicate `0x4c11b0`
before farm predicate `0x4c2930`. Farmhouse193 takes `0x4c2960→0x4c5b80`,
installing vtable `0x7af1e8`. Its shared initializer obtains initial side3 from
the type registration at `0x823598+24*type`. Food fields195–199 take
`0x4c0660→0x4c2220`, installing `0x7aea40`; their initial side is1.
Field publisher and purchase bounds remain those of the earlier contract.

Crop indices, Building model types and resource IDs are distinct domains:

| Model type / MODEL_OBSERVED label | EXE crop index | EXE resource / original resource name |
|---|---:|---|
|195 Wheat Field|0|7 / Wheat|
|196 Millet Field|1|5 / Millet|
|197 Rice Paddy|2|6 / Rice|
|198 Cabbage Field|3|3 / Cabbage|
|199 Soybean Field|4|1 / BeanCurd|
|194 Hemp Field|5|19 / Hemp|

`0x4c1060` qualifies the actual type and resolves its crop index through byte
selector `0x4c10f4` and branch table `0x4c10cc`. The separate resource getter
`0x4c18c0` uses selector `0x4c1940` and table `0x4c1918`. Its returned IDs resolve
through resource-name pointers at `0x872030+4*resource`. Field initialization
`0x4c1860` calls that getter and retains the returned resource in runtime byte
`+0x47`. Thus Soybean199 is a source type whose observed output identity is
BeanCurd1; it is not a license to relabel an existing household good or assume a
separate processing building. These are identity bindings, not recovered yield,
FoodQuality, consumption or delivery-unit rules.

The field purchase path `0x4c1320` separately tests the selected cells, creates
accepted fields through the real object manager and publishes each actual field.
It does not establish OpenEmperor's current immutable one-field-per-parent
ownership contract. The later Farm search, work assignment and returning actor
references must be reconciled before admitting a new ownership model. A field
label or nearby Farmhouse does not grant staffing, tending or harvested stock.

## Quantity, work and the source calendar

Original fields have their own state. The checked initializer `0x4c1860` clears
twelve signed-word history slots at own-info `+0x06..+0x1d`, clears `+0x5c` and
resets runtime counter `+0x150`; it does not directly initialize quantity word
`+0x04`. Reset `0x4c1720` explicitly clears quantity and work flags. Complete
accepted-purchase and restore initialization remain separate proof obligations.

The phase dispatcher `0x4ac2b0`, using table `0x4ac4e8`, selects the field-growth
driver at source phase20: `0x4ac3bf→0x51e4a0`. That driver first calls schedule
update `0x4c1a90`. It visits up to four entries per crop from `0x7ae19c`, stops
at sentinel−1 and compares entries with calendar slot `0xc82eec`. At a matching
slot with subcounter `0xc82ef0==0`, `0x4c1a20` finds actual matching fields and
sets bit`0x8000` in their own-info word`+0x6a`.

The shared source scheduler advances its phase through0–50, its subcounter
through0–15 and its outer slot through0–11. These order/count observations do not
prove an original number of seconds, a World20-Hz mapping, named calendar dates,
regional seasons or all event ordering. The existing Original-economy adapter's
816-World-tick month remains an authored integration choice.

The field branch reached through `0x51e50a→0x4c0f60` checks actual source state,
decrements a retained countdown, invokes field callback`+0x25c`, adds its result
to signed quantity`+0x04`, caps that quantity at10000 and republishes the field.
The ordinary Food field callback`0x4c2260` additionally reads schedule/work state;
the original is not a single shared elapsed-time progress counter. The checked
callback`0x4c1dd0` marks work state, gates yield on the schedule bit, reads the
actual field quantity, includes source terrain fertility, irrigation, crop/region
and other global modifiers, records a calendar-indexed result, adds it to the
actual worker's pending harvested amount and clears field quantity.

The crop/region multiplier is read from `0x84c564` indexed by three times crop
index plus the actual source region selector. A separate branch multiplies by
the retained float at `0x84c55c` when global`0xd623fc` exceeds−300. A complete
clock/year, event/technology and regional interpretation is not inferred from
these instructions or from similarly named FarmConfig entries. The original
x87 arithmetic and rounding require their own bounded differential controls
before exact output quantities are implemented.

Rice197 also has a separate topology publisher: `0x4c2360` dispatches to an
eight-neighbor same-type query at `0x4c22c0`, uses source variation and additional
Farm/work/calendar conditions. Reusing the six ordinary field images or the
current parent-Farm80-tick stage mapping does not reconstruct Rice state.

## Farmhouse storage and actual transport

Farmhouse193's virtual`+0x268` points to `0x4c4ee0`. This callback resolves the
actual returning worker, then its referenced field from Actor`+0x68`, and derives
the field's actual resource through `0x4c18c0`. Selector `0x4c5090` examines five
resource bins, each sixteen bytes apart starting at Farm`+0x158`; it chooses an
existing matching bin or the first empty bin. There is no fixed two-crop ledger
in this source path.

The callback offers the worker's actual harvested amount from Actor`+0x7c` to
that bin through `0x5d2790`. At a retained amount of at least400 raw units and
after a real access check, it attempts to create a separate delivery Actor19
through `0x4ea050`. Only successful creation assigns the actual Farm source ID,
resource and raw cargo400, and debits400 from the source bin through
`0x5d2760`. A failed spawn does not execute that debit. This is a source-side
physical handoff; destination admission, complete road routing, waiting, return,
delivery refusal and accepted Warehouse/Market credit remain separate proofs.
Raw400 is not established as four OpenEmperor Food units.

The shared bin helper `0x5d2790` sets resource identity, adds the offered amount,
caps it at the bin's retained capacity and returns overflow. `0x5d2760` debits,
floors at zero and may clear the resource identity of an empty bin. The checked
Farm return caller does not branch on that returned overflow before checking its
new quantity. Original overflow, loss and acceptance semantics therefore remain
an explicit implementation question. Strict World mass conservation and reserved
return headroom must be documented as the new contract's authored guarantees,
not attributed to this incompletely followed original return path.

## Irrigation has separate terrain and source authority

### Ditch202

The purchase dispatcher has an exact202 branch at `0x4b1739`; it invokes
`0x5248c0` and charges according to the actual accepted-cell counter. The reached
cell operation `0x524ce0` toggles source terrain bit`0x800`, updates retained
cell state and refreshes nearby source topology. This is a chained terrain
operation. The generic registration's initial side1 does not establish a
standalone industrial building, actor or saved body for every ditch cell.

Geometry, permission, road crossing, height, removal and complete transaction
rollback predicates remain required. Neither a raw map irrigation flag nor
visible ditch artwork authorizes paid construction or an irrigation effect.

### Pump203

Predicate `0x408ce0` accepts exactly203. The Industrial factory uses that
predicate at `0x51c872` and calls constructor`0x51e5e0` at`0x51c89e`.
The constructor installs `0x7b6844`, independently bound through its RTTI complete
object locator to `cIrrigationPump`. Its inherited`+0x94→0x428aa0` obtains initial
side2; concrete initializer`+0x90→0x5243b0` registers the source with the actual
pump collection through`0x525840`. Placement`+0x100→0x5245a0` uses the
orientation-selected publisher`0x4b84b0` and stores orientation in own-info`+0x60`.
Initial side2 is not proof that the complete shoreline/body/intake shape is a
simple unconstrained2×2 square. Callback`+0x178→0x524620` is inspector UI and
does not prove a gameplay update.

Source scheduler phase27 selects `0x4ac40a→0x5253a0`. That pass first marks
eligible ditch imagery with the separate eighteen-record dry-bank offset,
then iterates registered Pumps with positive actual worker word`+0x44`. For each
pump it uses stored orientation and four source-derived seed cells. The reached
cardinal traversal`0x525470` requires ditch bit`0x800`, rejects source bit`0x80000`
and water bit4, respects bounded object exceptions, records visited cells and
changes connected qualifying ditch imagery back to its wet bank. Its complete
intake-water, fire/operation, height and shoreline-lifetime predicates have not
been closed by this bounded pass.

### Field irrigation query

`0x4c15b0` examines square perimeters at distance1 through5 around the actual
field, at most120 candidate cells. It requires ditch bit`0x800`, rejects
bit`0x80000` and calls `0x525610`. That helper additionally checks global state
through `0x425260→0x425250`, retained object identity and the source wet/dry image
bank. The meaning and full lifetime of that additional global condition remain
unknown. Harvest and estimate callers use this result as one input; proximity
alone is insufficient.

These source image-bank dependencies must not be copied into rendering as a new
World authority. A future integrated Agriculture contract needs explicit
World-owned, source-bound irrigation topology and supplied-water state, with
cached render material derived from it. Its reconstruction must cover pump
staff/operation changes, cuts, crossings, removal and restore before the effect
is admitted. Existing map readers and texture selection cannot grant irrigation.

## FarmConfig is not an activation contract

The source file directly contains groups for tending ranges, harvester/tender
counts, assigned fields, minimum fertility, an Iron Age modifier and crop/climate
regional modifiers. These are `SOURCE_VERIFIED` file facts. A fresh bounded ASCII
string/pointer scan of the pinned EXE found no FarmConfig filename or the examined
named keys; the examined field code instead reads prepared in-memory values.
That negative scan does not prove the configuration unused: alternate loaders,
indirection or build-time preparation remain possible. It also does not prove
that this shipping EXE loads or honors the text file.

Do not add a production INI parser, apply its numerical values or fingerprint it
as an active gameplay dependency until actual load/selection/use paths are
established. The complete candidate values and source observations remain private;
model or file labels do not silently populate new recipes or seasons.

## Orchard29, fishing, hunting and indexed source points (AGRI-01)

This fresh static observation uses the same pinned PE32 EXE and the existing
shared PE section mapper. The private reproduction is
`.local/agri-01/research-harvest/verify_harvest.py`; its report records 79 narrow
assertions and 18 byte-verified instruction windows (2,354 overlapping instruction
lines / 7,348 bytes, zero byte mismatches). Six inspected Original inputs retain
SHA-256, size, mode and mtime. Original execution, complete construction admission,
World-time mapping, tonnage, all-season operation and live parity are not claimed.
Raw disassembly, source bytes and intermediate queries remain private.

### Orchard Shed29 is not a proven additional producer

**MODEL_OBSERVED:** row29 is named `Orchard Shed`; its model label and existing raw
Cost/Employees observations remain available for inspection. **EXE_OBSERVED:**
its registered initial side byte at `0x823598 + 24*29` is zero. The field-family
predicate `0x4c11b0` resolves only194–199 and26–28 through its three helpers. The
independent Farm/Orchard-hold predicate `0x4c2930` accepts192/193/237/238/239,
not29; actual Farm/Orchard-hold creation is `0x51c6ad→0x4c2960`. This is not a
complete classification of every caller or UI remapping of29.

**UNKNOWN:** whether29 is a historical/menu/construction alias, how an original
UI selection of29 becomes a concrete placed source, and any independent29
footprint, crop, fruit commodity, yield or lifetime. It must not silently become a
fourth equivalent of237/238/239. The existing concrete Tea/Lacquer/RawSilk sheds
and their paid plant/actor-return contracts remain separate. No fruit food or
new paid generic orchard is inferred from the English label.

### Fishing Quay31 has physical boat work and later cart delivery

**EXE_OBSERVED:** Industrial factory `0x5f0e20` tests predicate`0x42b740` for31
and reaches constructor`0x5f52b0` through call`0x5f1077`. RTTI identifies the
published vtable`0x7c0c68` as `cFishingQuay`. Its initial model side is2. This
square alone is not the complete shore geometry: physical callback
`+0x100→0x5f3c40` retains an orientation and issues a further1×1 owner write via
`0x5f3cd7→0x4b72b0` at the orientation-derived shore/dock cell. The complete
shore preflight, orientation, access, height and attachment lifecycle remain
necessary before new paid placement can be admitted.

Shared initializer`0x5f11f0` obtains output through virtual`+0x260` and stores it
at source byte`+0x47`. This vtable selects`0x4fa3c0`, which returns resource2;
the resource-name pointer table independently names2 `Fish`. Source update
`+0x20→0x5f3dd0` creates actual Actor49 through`0x5f3edb→0x4ea050`, at its
retained dock-side position, only after its existing conditions. Actor49's
registration names`FIG_FISHING_BOAT` and callback`0x4cfbc0`; source ID is retained
in Actor word`+0x62`, and the source retains the actual boat ID. A staffed quay
or due attempt alone is not fish production.

The boat callback resolves its actual source and verifies type31. It selects an
indexed fishing-point destination through`0x4cfdc2→0x4bab00`, moves through the
existing actual-actor movement functions, performs staff-dependent work, returns
to its retained dock coordinates, and only in the relevant completed return/work
branch writes literal100 to the source stock WORD`+0x40` at`0x4cffec`. That is an
assignment, not an inferred `+=100`. The examined branch has separate outward
and dock-side staffing-dependent counters: full-staff raw comparisons include408
and204 respectively; they are callback counts, not seconds, months or World
production ticks. All lower-staff, blocked-route and capacity behavior must be
preserved when completing a formal lifecycle.

Only the later source path, with physical stock, creates a separate Actor19 cart
at`0x5f401f`. It carries actual selected resource2 and quantity in Actor`+0x13`
and`+0x82`, binds source ID`+0x62`, and debits100 raw source units at`0x5f4095`
after successful spawn. No Warehouse, Market or House credit follows from boat
existence, destination selection or that dispatch. Full destination acceptance,
reservations, rejected-arrival and cart-return semantics remain a separate
transport integration proof.

**UNKNOWN:** complete source-point depletion/regeneration, fish population,
seasonal/calendar dependencies and raw-unit conversion. Absence of a decrement
in these bounded windows is not proof of infinite fish or general replenishment.

### Hunter's Tent33 requires real prey and a real physical return

**EXE_OBSERVED:** the same Industrial factory tests`0x58d650` for33 and reaches
`0x5f5270` through call`0x5f1035`; RTTI identifies vtable`0x7c09b4` as
`cHuntersTent`. Its initial model side is2. The output selector is
`+0x260→0x416ca0→4`, independently named `Meat` by the resource pointer table.
Source update`0x5f3730` uses actual staffing to bound spawn attempts/counts and
creates Actor40 through`0x5f39c0→0x4ea050`, retaining the real source ID in
Actor word`+0x62`. Randomized Actor delay initialization is visible, but its
World-time and complete RNG contract remain unproved.

Actor40 is registered as`FIG_HUNTER`, callback`0x4d2f10`. It obtains actual prey
from `0x59cc00`, which iterates the runtime prey manager returned by`0x59a300`
(address`0x13129a0`), filters actual actor status/types/states and considers
retained source/assignment identities. Source-point proximity is not a meat
stock. The hunter pursues the selected real animal, checks identity/position and
state, marks that prey's active byte`+0x16` as2 at`0x4d336b`, then follows its
source return. Only movement result8 in the return branch credits raw50 to
source stock WORD`+0x72` at`0x4d3408`. Capturing or retiring the animal alone does
not credit its source, Warehouse or House.

The later source output branch creates an actual Actor19 cart through
`0x5f3825`, binds source and selected resource4, carries quantity in Actor`+0x82`
and debits100 raw source stock only after successful creation. This raw transport
path does not establish a50-World-unit recipe or universal mass conversion.

**UNKNOWN:** complete animal birth/depletion/repopulation and species rules,
territory/reservation conflicts, predator effects, all calendar/seasonal links,
and the original raw-unit/World-clock mappings.

Both Hunter and Fishing output paths contain a separate source-byte`+0x70`
branch which can select400 cargo while the examined source debit stays100.
The producer/meaning/lifetime of that external effect is unresolved. Do not call
it a particular god, blessing or recipe, or activate it as a free multiplier.
An integration must either prove its full source/accounting contract or keep it
unsupported explicitly; it cannot silently normalize these raw quantities.

### FishingPoint179 and PreyPoint183 remain immutable source context

Their existing exact saved Industrial identities and conservative origin claims
remain defined in the [MP34 point contract](mp34-industrial179-164-occupancy.md)
and [type183 contract](original-occupancy-pass-next.md). Freshly checked editor
selection/dispatch confirms179 chooses selector12→`0x404ddd`, writes the eight
signed-short point arrays`0xc5cd60/0xc5cd70`, and183 chooses selector14→
`0x404f0a`, writes the four coordinate slots`0xc5ce3c/0xc5ce4c`. These editor slot
counts do not cap saved-manager records or authorize discarding old source
origins. Neither point is a paid square building or a saved food inventory.

**EXE_OBSERVED, runtime link:** fishing search`0x4bab00` reads the same179 arrays
and uses existing prepared navigation-distance data to find a usable point;
a missing/unreachable selection returns failure. The point choice includes a
randomized starting index and a bounded distance comparison, not a globally
proved nearest-point policy. Prey origin helper`0x59c930` reads the same183
arrays for its family0 branch. Actual prey habitat/navigation methods
`0x59ebd0/0x59ec60` call that helper at`0x59ec0e/0x59ec9d`. The editor's183
terrain predicate and the runtime fauna-origin policy are separate from the
existing map occupancy claim.

**UNKNOWN:** the complete initial population creation and subsequent regeneration
which turns those source coordinates into every eligible animal, including
counter ownership, species inputs, random choices and scheduler cadence. Existing
source records do not provide free prey, fixed yields or re-created NPCs on restore.
No map-permission predicate, source admission or historical saved identity is
changed by these observations.

## Concrete proof gates before implementation

The [runtime architecture](../original-economy/architecture-map.md) remains the
authority for existing OpenEmperor behavior. Future agricultural work must close
these gates independently:

1. Complete accepted construction, purchase arithmetic, terrain/fertility and
   source-manager admission for each new type; ditch and pump are not ordinary
   field variants. Confirm exact footprint/intake/road-crossing geometry.
2. Initial field quantity, complete tending/harvest state transitions, worker
   assignment and concurrent-field commitments. Recover actual relationships;
   do not infer immutable ownership from adjacency or a returned field ID.
3. Original calendar/date and event ordering, crop/region and all yield modifiers,
   rounding, quantity units, regeneration and exhaustion. Phase positions and
   named configuration groups alone are insufficient.
4. Complete pump operating/intake and connectivity rules, the global irrigation
   veto, water-height behavior, wet/dry lifetime and exact reset/restore context.
5. Farm bin capacities/overflow, physical source-to-carrier transfer, valid
   destinations, cargo admission/return and transport bounds. Confirm a real
   consumption sink and FoodQuality/consumption units for every resource.
6. A new explicitly selected World contract with strict save validation, typed
   IDs, original source identities and independent creator/restore continuation.
   Original states, graphics, pending work and dispatches grant no stock or food.

AGRI-01 establishes preparation evidence and reusable identities only. These
gates are not replaced by a synthetic test, a successful parse, a visible native
sprite, a model label or the existing authored80-tick harvest loop.
