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

The field-admission follow-up is retained separately under ignored
`.local/agri-02/research/`. `verify_agri02.py` checks25 fresh bounded windows,
7,466 overlapping instruction/data lines and20,437 decoded bytes against the
pinned EXE, with51 narrow instruction/table/model assertions. All three input
files retain their byte hashes, lengths, modes and modification times. The
private `verified-agri02-source-report.json` explicitly excludes game execution,
named-month/World-clock parity and a universal Rice pump prerequisite.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02/research/verify_agri02.py
```

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
Its cell checks include terrain mask`0xaffede6f`, a typed placement predicate,
the separate checks at`0x416bb0`/`0x416bd0` and the result of
`0x59c770→0x59c8a0`. Only an accepted actual build calls manager`0x42d540` and
the accounting callback`0x5ef430`; the count/preview path does not do this.
The complete meaning of every veto and final purchase arithmetic remain open.

The typed predicate is bound through`0x4c1530`, instance`0x1031408` and
constructor-installed vtable`0x7ae2e4` to`0x4bfb80`. It first applies shared
terrain predicate`0x42e250`. For **Rice197 only**, it additionally requires the
separate per-cell byte at`0xe9f8e0` to equal100 (`0x4bfbbf`). This is the
serialized fertility operand also used by the existing landscape reader, not
terrain bit`0x80`, water adjacency, a sprite or a pump-distance inference.
This operand must enter a source-bound World terrain context before it may
authorize a Rice command; a renderer-owned buffer is insufficient.

The same placement predicate maps the field to its actual required farm type
with`0x4c3440`, then searches actual active matching farms with
`0x4c5590→0x4c55a0`. Food fields195–199 map to Farmhouse193. Each axis must be
within the farm's virtual`+0x260` range of at least one footprint endpoint.
Farmhouse193 binds that callback to`0x4c31f0`; the pinned range value at
`0x84c554` is3 and its initial side is3. Thus the bounded search covers offsets
−3 through+5 on each axis relative to the Farm origin, subject to all other
cell vetoes. The analogous work-range check is`0x4c3c60`.

This actual matching-farm/range search does not establish OpenEmperor's current
immutable one-field-per-parent ownership contract. The subsequent work search
examines compatible fields, actual retained quantity and assignment state.
Concurrent work commitments and the complete worker loop must still be
reconciled before claiming Original ownership parity. A label or nearby
Farmhouse grants no staffing, tending or harvested stock.

The pinned model rows report Farmhouse193 base cost50 and staff22; Rice197
base7, Cabbage198 base5 and Soybean199 base5, each with field staff0. These
`MODEL_OBSERVED` inputs use the existing documented model column meanings and
do not replace difficulty arithmetic, separate field payments or complete
footprints. Zero field staff does not make an untended field productive.

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

The separate start table at`0x7ae22c` and end table at`0x7ae19c` have the
following first entries in this EXE; each row's remaining three entries is−1.
The growing-period query`0x4c0760` compares the current outer slot with a start
included/end excluded interval. These are raw source slot numbers, not inferred
month names or elapsed durations in the current World contract.

| Crop index / field type | Start slot | End / harvest-mark slot |
|---|---:|---:|
|0 / Wheat195|2|6|
|1 / Millet196|6|10|
|2 / Rice197|5|9|
|3 / Cabbage198|7|11|
|4 / Soybean199|4|8|
|5 / Hemp194|3|8|

The shared source scheduler advances its phase through0–50, its subcounter
through0–15 and its outer slot through0–11. These order/count observations do not
prove an original number of seconds, a World20-Hz mapping, named calendar dates,
regional seasons or all event ordering. The existing Original-economy adapter's
816-World-tick month remains an authored integration choice.

The relative cadence is bounded more precisely: after phase50 the dispatcher
resets its phase and calls`0x4ac650`. That function increments subcounter
`0xc82ef0`; when it exceeds15, it resets it to0 and increments outer slot
`0xc82eec`, wrapping after11. Its final unconditional chain
`0x4acc21→0x4bfdb0`, `0x4acc28→0x4c0570` reaches active fields and their
`0x4c1bc0` work decay. Thus this decay belongs to every completed phase cycle,
including the ordinary subslot advances, rather than only to an outer-slot
boundary. Field growth is visited at phase20 before that cycle's decay.

The field branch reached through `0x51e50a→0x4c0f60` checks actual source state,
decrements a retained countdown, invokes field callback`+0x25c`, adds its result
to signed quantity`+0x04`, caps that quantity at10000 and republishes the field.
The ordinary Food field callback`0x4c2260` additionally reads schedule/work state;
it returns0 in the last configured end slot (`0x4c16c0`), otherwise157 when
the work flag's low byte exceeds32,79 when that byte is1–32 and0 when it is0.
It does not itself call the start-inclusive/end-exclusive growing-period query.
That separate query's existence therefore does not establish a complete four-slot
quantity-increment rule. Likewise neither this callback nor the calendar driver
contains an automatic end-slot harvest or four-portion World stock credit.
The actual work-completion callback`0x4c1d20` sets low-byte bit`0x40` and clears
the field's worker reference. The separate decay helper`0x4c1bc0` decrements
that low byte to a minimum of0 while preserving the high work/harvest bits.
These are quantity increments and retained work, not World tick counts or a
license to make a merely staffed/proximate Farm grow arbitrary fields. The
complete arrival/dispatch scheduling remains a separate boundary.

Actual tending and harvesting use separate actor-clock transitions. The grower
state dispatcher`0x4cc670` reads retained actor/source/field identities and uses
state table`0x4cd1b8`. In the tending branch, `0x4cc96d→0x4c2dc0` decrements
the actual actor's signed wait word`+0x3e`; only after it reaches0 or less does
it resolve that actor's field reference`+0x68`, verify field compatibility and
call field slot`+0x264` at`0x4c2e29` (`0x4c1d20`). The actor then enters state6.
The analogous harvest chain`0x4ccc17→0x4c2fd0` has its own wait-word gate and
calls field slot`+0x268` at`0x4c3039` (`0x4c1dd0`), then enters actor state10.
The preceding state branches initialize waits using source floating-point
operands and rounding at`0x4cc8f3..0x4cc918` and`0x4ccb92..0x4ccba6`, following
movement outcomes. Those actor transitions cannot be replaced by Farm staffing,
a calendar match, a planned field visit or a quantity threshold.

The immediate wait operands are now bound. The classifier
`0x4c0750→0x4c0640` is true only for field types26/27/28, and false for Food
fields195–199. The common float at`0x7affe0` is51; the harvest branch's doubles
at`0x7ae0f0`/`0x7ac780` are4/0.5. Conversion helper`0x7650fc` temporarily sets
x87 rounding control to truncation toward zero, converts, and restores the
previous control word. In the checked initializer branches:

| Bound field type | Tending wait word | Harvest wait word |
|---|---:|---:|
|195–199 Food fields|51|25 (truncated from0.5×51)|
|26/27/28 plant fields|204|204|

These values apply only after the relevant field predicate accepts and retained
field word`+0x2e` is nonzero. The alternate branches write0, and terrain
flag`0x100` subsequently overrides either initialized wait with0. The meaning
and complete writers of that retained field word are not inferred as staffing
or productivity from its location. The waits count subsequent eligible actor
state invocations; they do not measure World ticks, travel or a full crop season.

The shared type dispatcher`0x4c7580` indexes behavior pointers at
`0x84e784+40*actor_type`. Raw actor type26's entry`0x84eb94` points to
`0x4cc670` (the Grower behavior). This raw actor type domain is separate from
the plant-field type26 above. The registry binding does not close how often
the actual actor reaches that dispatcher or the movement/state branches.

These observations close relative phase/subslot cadence and the immediate
work/harvest wait gates, not the complete accepted field initialization,
actor update rate, movement-to-state transitions or repeated
field assignment/tending lifecycle. A fixed160-increment adapter, an assumed
63-event ripening period or a fixed four-portion seasonal harvest is therefore
**AUTHORED**, not a verified Original time rule. With the user's proof-only
timing requirement, these unclosed production transitions remain unavailable;
none may be activated as an Original seasonal chain.

The original is not a single shared elapsed-time progress counter. The checked
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

### Rice paddy topology and the inactive base

Rice197 has a separate base publisher at `0x4c2360`; it is not one of the six
ordinary quantity images. The checked query `0x4c22c0` visits eight source-grid
neighbors in this order: N, NE, E, SE, S, SW, W, NW. Their source-cell offsets
are −228, −227, +1, +229, +228, +227, −1 and −229. Each neighbor must have a
nonzero retained object ID whose actual manager object has signed model type197
at `+0x14`. There is no same-Farm, quantity, staffing, operation, calendar or
terrain test in this neighbor query. Thus adjacent Rice fields can join across
Farm ownership or pause state. Rendering never grants that field or parent.

The publisher passes the eight booleans, the actual nonnegative source-cell
variation byte and a forward-search flag to `0x4bbdc0`. The source Rice selector
at `0x84c098` consists of47 records of20 bytes. Startup `0x4b05c9` calls
`0x4bc090` to compile the records' present/absent/wildcard predicates; the mask
words in the file itself are zero. Every one of the256 neighbor masks matches
exactly one record. View values0/2/4/6 select four orientations; their effect
is rotation of the input mask by0/2/4/6 bits. Nonnegative source variation is
reduced modulo the matched variation count. All records have count1 except the
fully surrounded record, which has count2. Thus all views and variations reach
offsets0–47 of group`0x42d`; canonical view0 and variation0 reach exactly0–46.

`assets::rice_inactive_topology_offset` independently expresses these predicates
as Boolean topology classes, without embedding the Original selector table.
Bits0–7 follow the neighbor order above. Write B(i) for bit i modulo8 and let
r range0–3, starting at N and turning clockwise through the cardinal directions:

- No cardinal neighbors selects0; one cardinal at2r selects1+r.
- Two opposite cardinals select13 for N/S or14 for E/W. Two adjacent cardinals
  at2r and2r+2 select5+r, plus4 when their intervening diagonal B(2r+1) exists.
- Three consecutive cardinals starting at2r use their two intervening diagonals
  a=B(2r+1), b=B(2r+3). Select15+r when neither exists,19+r for a alone,23+r for
  b alone, or42+r for both.
- With all four cardinals, no diagonals selects27. One diagonal at2r+1 selects
  30+(r+2 modulo4). Two opposite diagonals select28 for NE/SW or29 for SE/NW;
  two adjacent diagonals starting at2r+1 select34+(r+3 modulo4). Three diagonals
  with the r-th missing select38+(r+1 modulo4). All four diagonals select46
  for canonical variation0; the alternative source variation selects47.

A diagonal affects the result only when both adjacent cardinals are present.
The private static proof independently evaluated the source predicates and
checked all256 masks ×4 views ×256 source variation bytes:262,144 exact matches,
with all47 canonical offsets reached. The receipt and bounded disassembly stay
under ignored `.local/agri-02/rice-visual-proof/`; none is shipped or committed.

The publisher resolves group`0x42d` and adds48 only if an actual matching Farm
exists, its virtual`+0x1b4` returns nonzero, and the field is in a source growing
slot or its source end slot. The checked Farm query `0x428ed0` computes integer
`100*assigned/configured` from the actual assigned count and configured Employees,
with a retained operation veto and a nonpositive-need refusal. This is not a
full-staff test. The base publisher never queries field quantity. Consequently
**quantity0 alone does not establish bank0**. Canonical bank0 is an explicitly
selected inactive source-context representative, not a claim that current
OpenEmperor staff, work or calendar state reproduces the Original bank decision.
The unclosed production clock above remains unclosed.

The separate draw routine `0x4c24d0` first queries quantity percentage and emits
no overlay at percentage≤0. Positive overlays additionally require a retained
base image at or beyond group`0x42d`+48, then choose `min(4,percent/20)` and up to
nine smaller components from group`0x49d`. Quantity0 therefore supports absence
of the overlay independently of the inactive-base choice. Positive overlay
positioning, complete preparation and active-state reproduction remain unsupported.

The pinned `China_General.sg3` header admits the entire96-image base range
2506–2601; the canonical47 entries are2506–2552. Every checked entry is native
width78, height40–46, Type30, base data length3200, animation count/offsets0,
unmirrored, internal and without an alpha plane. The existing shared Type30
ground anchor `(39,height−20)` is an **AUTHORED** projection convention; zero
animation offsets do not prove an Original ground pivot. Native dimensions
must be retained and complete production decoding must still succeed.
The private proof pins SHA-256, size, mode and modification time before and
after its read-only investigation for Emperor.exe, China_General.sg3 and its
paired555. All guards are unchanged; the Original executable was not run.

The separately prepared Cabbage198 and Soybean199 native metadata is checked
against user-supplied `DATA/China_General.sg3`, SHA-256
`c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`.
The private receipt `.local/agri-02/field-geometry.json` uses the existing SG3
record layout and contains metadata only. Cabbage's six canonical records
2434–2439 have native width78 and heights40/40/40/44/42/47; Soybean's records
2446–2451 have width78 and heights41/42/43/46/50/51. All twelve are Type30,
base data length3200 and animation count0. These facts support native preparation of
the first six quantity images, subject to complete production-loader decoding.
They do not validate the alternative variation bank, a resize, elapsed-time
growth, Rice topology/overlays or Original whole-scene composition.

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

The checked harvest callback queries irrigation after deriving the base yield
and uses it to add a field-class-dependent fertility modifier. Together with Rice's
separate100-fertility placement check, this does **not** prove that every
Original Rice field requires a Pump or Ditch to be purchased or to harvest.
An unresolved Rice watering restriction in a new World adapter must therefore
be labelled as that adapter's conservative gate, with its missing source-bound
fertility/irrigation proof stated explicitly; it must not be presented as a
verified universal Original pump rule or bypassed with render/proximity data.

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
