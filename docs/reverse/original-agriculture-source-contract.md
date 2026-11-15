# Original agriculture: field, calendar, irrigation and harvest boundaries

This is a maintained source contract for the agriculture preparation work. It
extends the [production candidate register](../original-economy/oe55-production.md)
and the earlier [farm/field presentation investigation](original-presentation-correction.md#farm-bodies-and-field-state-boundary).
It does not admit a building, field, source marker, material, movement channel,
growth rule or save generation to `World`. Published Rules1–25/schemas1–41
retain their separate contracts, including the authored Wheat/Millet and
material-plantation adapters where supported.

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

Fresh Food-field construction has explicit initial state. Constructor
`0x4c2220→0x4bf410→0x4bf2f0→0x51c9a0` constructs own-info at field body`+0xc8`
through`0x51c2e0`. That constructor writes quantity word`+0x04=0`
at`0x51c2fa`, countdown byte`+0x63=0` at`0x51c347`, and work word`+0x6a=0`
at`0x51c368`. Base-body clearing`0x426e60` also zeros the field-body assignment
word`+0x2e`. These are explicit constructor writes with independent source-byte
checks.

The accepted purchase branch reaches manager`0x42d540`, actual concrete
construction at`0x42d714`, and virtual`+0x94→0x428aa0` at`0x42d736`.
Shared initialization invokes field virtual`+0x90→0x4c1860` at`0x428b25`.
The field initializer separately clears twelve signed-word history slots at
own-info`+0x06..+0x1d`, clears`+0x5c`, resets body counter`+0x150` and registers
the actual field. Reset`0x4c1720` clears quantity and work flags. Fresh
constructor quantity/countdown zeroing is therefore closed. Complete
post-purchase effects, copied/restored objects and exceptional lifecycle paths
retain separate proof boundaries.

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

The field branch reached through `0x51e50a→0x4c0f60` first skips source terrain
flag`0x100`. A nonzero retained eight-bit countdown at own-info`+0x63` is
decremented and returns before quantity work. Otherwise field callback`+0x25c`
supplies the increment. The addition at`0x4c0fd7` writes a sixteen-bit word,
followed by a signed comparison with10000 at`0x4c0fed`; a larger signed result
is capped. Inputs in the supported physical domain0–10000 with increments0/79/157
cannot overflow that word. Arbitrary signed-word inputs require a separate
wrapping contract. A later per-model veto`0x4afd80` can clear quantity before
the publisher at virtual`+0x260` runs.
The ordinary Food field callback`0x4c2260` additionally reads schedule/work state;
it returns0 in the last configured end slot (`0x4c16c0`), otherwise157 when
the work flag's low byte exceeds32,79 when that byte is1–32 and0 when it is0.
It does not itself call the start-inclusive/end-exclusive growing-period query.
That separate query's existence therefore does not establish a complete four-slot
quantity-increment rule. Likewise neither this callback nor the calendar driver
contains an automatic end-slot harvest or four-portion World stock credit.
The actual work-completion callback`0x4c1d20` ORs low-byte bit`0x40` into the
retained word and clears the field's worker reference and counter`+0x150`.
The separate decay helper`0x4c1bc0` decrements the unsigned low byte to a minimum
of0, retains high bits`0x4000`/`0x8000`, and clears bits8–13. Tending therefore
preserves the existing word through its OR operation, while decay recomposes
only those two high flags and the decremented low byte.
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
field-body word`+0x2e` is nonzero. The fresh dispatch audit binds this word to an
actual Grower assignment. Successful harvest/tending spawns write the returned
ActorId at`0x4c392e`/`0x4c3bec`; completion clears it at`0x4c1d45`/`0x4c1f29`. Quota
allocation can retain one actor identity on several selected fields. Complete
concurrent commitments and exceptional writers remain open. These writes address
the actual field body retained through the manager lookup or method receiver.
The own-info pointer returned by virtual`+0x1e8` separately addresses work
word`+0x6a`, quantity`+0x04` and countdown byte`+0x63`.

The alternate wait branches write0, and terrain flag`0x100` subsequently
overrides either initialized wait with0. Each eligible work-state invocation
decrements the sixteen-bit word before its signed positive comparison. In the
initialized domain,51 needs51 such invocations and25 needs25. An initialized0
becomes−1 and permits completion on the next invocation. For arbitrary raw words,
the native decrement wraps, so−32768 becomes32767 and remains positive. These
are eligible actor invocation counts with separate movement and season state.

The shared type dispatcher`0x4c7580` indexes behavior pointers at
`0x84e784+40*actor_type`. Raw actor type26's entry`0x84eb94` points to
`0x4cc670` (the Grower behavior). This raw actor type domain is separate from
the plant-field type26 above. The AGRI-02B common-parent audit below closes one
behavior call per eligible live actor in the ordinary actor pass. Special-mode
gates and complete movement-to-state timing retain their own proof boundaries.

These observations close relative phase/subslot cadence, the ordinary relative
actor call chain and the immediate work/harvest wait gates. Complete post-purchase
and exceptional field initialization, actor gates, movement-to-state transitions
and repeated field assignment/tending lifecycle remain open. A fixed160-increment
adapter, an assumed63-event ripening period or a fixed four-portion seasonal harvest is therefore
**AUTHORED**, not a verified Original time rule. With the user's proof-only
timing requirement, these unclosed production transitions remain unavailable;
none may be activated as an Original seasonal chain.

The checked callback`0x4c1dd0` marks work state, gates yield on the schedule bit,
reads actual field quantity, applies runtime fertility, irrigation, crop/region
and other global modifiers, records a calendar-indexed result, adds it to the
actual worker's pending harvested amount and clears field quantity. Its fertility
operand is runtime byte`0xeac3f0`, whose source preparation includes a transform
from serialized fertility`0xe9f8e0` at`0x4c1c20`. The latter remains the exact
Rice construction operand. Their distinct identities and preparation must be
preserved when defining a future World yield context.

The crop/region multiplier is read from `0x84c564` indexed by three times crop
index plus the actual source region selector. A separate branch multiplies by
the retained float at `0x84c55c` when global`0xd623fc` exceeds−300. A complete
clock/year, event/technology and regional interpretation is not inferred from
these instructions or from similarly named FarmConfig entries. The original
x87 arithmetic and rounding require their own bounded differential controls
before exact output quantities are implemented.

### Bounded raw-yield and fertility arithmetic

**EXE_OBSERVED:** for physical quantity Q in0–10000, the integer branches
at`0x4c1e3a..0x4c1e78` compute base raw amount Q/100 or Q/50 with integer
truncation. The larger branch requires both actual predicates
`0x48e0f0` and`0x48e680(field_type)` to accept. Their event state and complete
lifecycle remain external inputs to this arithmetic.

Runtime fertility preparation`0x4c1c20`, particularly`0x4c1ca9..0x4c1ccc`,
reads serialized byte S from`0xe9f8e0`, computes floor(55×S/100), adds the
explicit global byte G at`0x84c560`, and retains the low byte in`0xeac3f0`.
The resulting byte is `(floor(55×S/100)+G) mod256`. Parameter initialization
`0x4bfc20` writes G=10 at`0x4bfc6f`, global multiplier float bits`0x3f933333`
at`0x4bfc65`, and crop/region defaults. Complete later writers, load preparation
and the source-file consumer for FarmConfig remain **UNKNOWN**. Future World
callers must supply admitted source context for these values explicitly.

The harvest multiplies that runtime byte by the retained float0.01 and stores
the result as binary32 at`0x4c1e97`. A true irrigation query adds float0.35 for
Food or0.2 for plant fields and stores binary32 again at`0x4c1ec2`. It then
multiplies the integer base by the crop/region float and the stored fertility
factor before conversion helper`0x7650fc`. The global`0xd623fc>−300` branch
multiplies this already converted integer by`0x84c55c` and converts again.
The intermediate stores and conversion order are part of the observed dataflow.

The private receipt includes a bounded arithmetic example under an explicitly
assumed nearest-even binary32 store. S=100 and G=10 produce runtime byte65.
With base raw100, region factor1 and the base irrigation branch, the stored
fertility factor is`0x3f266666`, approximately0.6499999761581421. Its product
before integer truncation is approximately64.99999761581421, giving raw64.
This is a controlled rounding example. Actual x87 precision/rounding during
the multiplication and binary32 stores still requires its complete runtime
contract before exact native yield reproduction is admitted.

The native Farm inspector independently divides raw bin amounts by100 at
`0x4c454d..0x4c4563` for presentation. That displayed unit leaves the World
Food-portion, partial-return and overflow conversion contracts open.

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

## AGRI-02B bounded clock and Grower audit

**EXE_OBSERVED:** the fresh relative-clock receipt is
`.local/agri-02b/clock/verified-relative-clock.json`. It checks9 bounded windows,
2,198 overlapping decoded lines /5,916 decoded bytes and29 narrow assertions.
The independent Grower receipt
`.local/agri-02b/grower/verified-grower.json` checks16 bounded windows,
4,404 overlapping decoded lines /12,770 decoded bytes and61 narrow instruction
assertions, with additional vtable/table checks.

The fresh-construction follow-up
`.local/agri-02b/initialization/verified-field-initialization.json` checks31
bounded windows,2,280 decoded lines /6,185 checked bytes and33 narrow instruction
assertions with typed-vtable controls. It closes the explicit constructor
quantity, countdown, work and assignment zero writes described above.
The yield-parameter receipt
`.local/agri-02b/clock-extra/verified-yield-parameters.json` checks8 bounded
windows,384 decoded lines /1,224 checked bytes and31 narrow instruction
assertions, plus20,002 bounded base-arithmetic identities and256 fertility
identities. It records the conditional integer base, runtime fertility transform
and retained float/conversion sequence with their remaining external inputs.

All four receipts retain identical before and after SHA-256, size, mode and
modification time for the pinned EXE, BuildingModels and FarmConfig. Original
execution and personal-save inspection are absent from these static checks.
The retained private receipts can be rechecked with the following commands.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02b/clock/verify_clock.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02b/grower/verify_grower.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02b/initialization/verify_initialization.py
PYTHONDONTWRITEBYTECODE=1 python3 .local/agri-02b/clock-extra/verify_yield_parameters.py
```

The ordinary common update parent`0x5371a0` invokes scheduler`0x4ac2b0`
at`0x5371bf`, then actor pass`0x4e27e0` at`0x5371ce`. The actor pass checks
the actual actor's active byte`+0x16` and retained gate`+0x13c`, then invokes
virtual`+0x28` once on that eligible record at`0x4e2909`. Constructor-installed
vtable`0x7afe60` binds that slot to`0x4c7580`; its raw-type26 entry`0x84eb94`
reaches Grower`0x4cc670`. This closes the ordinary relative call chain between
source phases and eligible actor work invocations. Global`0x88ec38` can skip
scheduler advancement while the actor pass still runs. Its complete meaning,
the actor`+0x13c` writers and exceptional branch relationships remain **UNKNOWN**.
OpenEmperor's existing one-World-tick-per-source-phase adapter remains
**AUTHORED**. Source seconds, named dates and unconditional special-mode parity
require separate evidence.

Farmhouse193 vtable`0x7af1e8` selects live update`0x4c4b90`. Its successful
dispatch path requires actual access`0x4ba6f0`, the staffing-percentage result
from`0x428ed0`, global type/resource veto checks and retained operation
virtual`+0x78→0x4289f0`. The operation getter reads body byte`+0x90`.
Dispatch`0x4c3550` applies percentage tiers100/75/50/25 to separately configured
harvest/tending quotas and its attempt counter. It invokes
virtual`+0x270→0x4c36a0` and`+0x274→0x4c3980`; those branches create actual
Actor26 instances, retain their source ID at Actor`+0x62`, and write the actor
identity into selected field bodies. Later target selection assigns the actor's
actual field at`+0x68`. Quota configuration, candidate ranking, distance-grid
preparation and concurrent tours still need their complete source contracts.

The shared veto`0x4afd80` resolves the actual model type and tests its global
byte at`0x10bc7e0+type`. The verified operand supports a per-type global veto.
Its complete writers and gameplay meaning remain **UNKNOWN**. Assigning this
gate object-local fire or staffing semantics would introduce an unsupported
interpretation. Original pause, fire and workforce-loss effects on outstanding
work require independent lifecycle observations.

The Grower movement branches use actual actor outcome byte`+0x19`. Outcome8
at`0x4cc8a0` admits the tending wait and state8; outcome8 at`0x4ccb53` admits
the harvest wait and state12. The actual wait helpers reach field completion
before returning to state6 or10. Later physical return outcome8 at`0x4ccca8`
alone reaches the source's virtual`+0x268` receipt through`0x4cccbf`.
Farmhouse193 binds this receipt to`0x4c4ee0`, while material-plantation sources
bind`0x4c2e50`. Their storage paths retain their own typed inventories.

These static transitions supply independently testable event arithmetic and
binding observations. Production activation still requires complete
post-purchase/exceptional initialization, movement/outcome and repeated-work
lifecycles, admitted runtime fertility/global yield inputs with controlled
rounding, and a conservative raw-yield/World-portion contract including bin overflow.
Rice/Cabbage/Soy gameplay remains **PARTIAL** while those dependencies are open.
Any separately proposed authored adapter must identify its selected clock,
movement, work, yield, unit and interruption rules explicitly before activation.

## Irrigation has separate terrain and source authority

### Ditch202

**SOURCE_VERIFIED:** BuildingModels row202 names Irrigation Ditch, gives base
cost4 per accepted cell and employee count0. The model file's column legend
independently identifies those operands. Difficulty arithmetic remains the
existing model contract.

**EXE_OBSERVED:** the exact202 purchase branch at `0x4b1739` first reaches the
shared construction veto through `0x59c770→0x59c8a0`, then invokes `0x5248c0`
at `0x4b1773`. The success path multiplies the actual accepted-cell counter
`0x101c514` by the prepared unit cost at `0x4b1792–0x4b179f`. Cell mutation
`0x524ce0` checks source coordinates through `0x408cb0`, rejects terrain40000,
increments that counter only when bit800 was previously absent, then sets or
clears bit800. Its bounded3×3 refresh reaches `0x524e90`. Ditches therefore have
chained terrain-operation identity. The registered side1 remains separate
from the actual per-cell mutation and payment path.

The two gesture endpoints reject mask`0x87375227` and terrain4000. The endpoint
at `0x524963` additionally rejects400. The reached cell predicate `0x525750`
first accepts an existing800 flag. Otherwise it rejects mask`0x87375227`,
requires an exact203 Pump or126 RoadBlock for an occupied terrain8 cell,
applies the road-crossing predicate when required, rejects manager-word mask
`0x01eb` at `0x13789c0`, and finally rejects terrain mask`0x90060201`.
Predicate `0x408ce0`, used in that occupied-cell branch, independently compares
the actual model to203. The manager word and runtime owner grid are distinct
from serialized raw map terrain and object lanes.

The route builder `0x5b1f40` uses that predicate and rejects the shared boundary
test `0x416c20(x,y,3)`. This wrapper translates local coordinates into the fixed
228-stride source grid and calls `0x4b0800`. The latter first checks Diamond
membership through `0x4b0770` and then tests its three-cell edge strip. Setup
`0x4b05f0` derives the extrema and midpoints from the actual declared dimensions.
Across every stored cell for supported sizes84/112/140/170/226, this accepted
interior equals shared `MapGeometry::contains(cell)` together with its four
axial probes at offsets`(−3,0),(3,0),(0,−3),(0,3)`. The private comparison covers
259,920 cells. The prepared-distance path driver `0x5b2470` selects mode4,
performs real cell mutations and has a400-attempt bound at `0x5b26c8`.

Road crossing predicate `0x524ab0` accepts the axial straight/end road variants
0/1/2/3/8/9/10/11 and rejects corner variants4–7 or an offset above11. In physical
view0, even accepted variants have a north/south road axis and exclude parallel
north/south ditch neighbors. Odd accepted variants have an east/west road axis
and exclude parallel east/west ditch neighbors. View directions2/6 exchange the
display axes. The RoadBlock126 branch independently requires at least one road
neighbor from a single axial family. Reciprocal road-over-ditch validation at
`0x524c20` first normalizes the eighteen-record wet/dry bank, then rejects
unsupported ditch topology and parallel road neighbors. Complete source
permissions, occupancy and shared edge legality remain separate inputs.

### Pump203

**SOURCE_VERIFIED:** BuildingModels row203 names Irrigation Pump, gives base
cost10 and employee count10.

**EXE_OBSERVED:** predicate `0x408ce0` accepts exactly203. The Industrial factory uses that
predicate at `0x51c872` and calls constructor`0x51e5e0` at`0x51c89e`.
The constructor installs `0x7b6844`, independently bound through its RTTI complete
object locator to `cIrrigationPump`. Its inherited`+0x94→0x428aa0` obtains initial
side2. Concrete initializer`+0x90→0x5243b0` registers the source with the actual
pump collection through`0x525840`. Placement`+0x100→0x5245a0` uses the
orientation-selected publisher`0x4b84b0` and stores orientation in own-info`+0x60`.
Actual purchase and preview reach shared shoreline predicate`0x4b8200`, also
reached from actual placement at`0x5245c6`. Its body is the complete2×2 rectangle
`(0,0),(1,0),(0,1),(1,1)`. Two body cells require water context and two require
land context. Six additional cells supply the surrounding intake-water strip.
Relative coordinates below use the retained physical orientation and body origin.

| Physical orientation | Eight water-context cells | Two body land cells | Four exterior ditch seeds |
| --- | --- | --- | --- |
|0|`x=−1..2, y=−1..0`|`(0,1),(1,1)`|`(0,2),(1,2),(−1,1),(2,1)`|
|1|`x=1..2, y=−1..2`|`(0,0),(0,1)`|`(−1,0),(−1,1),(0,−1),(0,2)`|
|2|`x=−1..2, y=1..2`|`(0,0),(1,0)`|`(0,−1),(1,−1),(2,0),(−1,0)`|
|3|`x=−1..0, y=−1..2`|`(1,0),(1,1)`|`(2,0),(2,1),(1,−1),(1,2)`|

The shoreline body pattern uses table`0x84bdf8`. The six exterior context
operands come from tables`0x84be38/0x84be58/0x84be78`. The four seed tables are
`0x820ce0/0x820cf0/0x820d00/0x820d10`. Ordinary unoccupied water cells use bit4.
The land branch rejects road40, mask`0x87375a67` and occupied terrain8. Shared
predicate`0x416bb0` separately reads a runtime cell-object pointer and its byte18.
Some shared shoreline branches admit that runtime context or terrain10000 under
their explicit mode. Their remaining class/state meaning stays **UNKNOWN**.

Source preflight explicitly bounds the2×2 body. Exterior reads use the original
padded228-stride addressing. OpenEmperor checks every body, water-context and
seed coordinate against its own complete source context before lookup. The
normal Pump203 mode tests the six exterior cells for water bit4 at
`0x4b83ab/0x4b83e2/0x4b841a`. These branches consult water context without applying
the body's ownership veto. The exclusive occupied footprint remains the four
body cells. The current World subset requires immutable exact policy3 normal
water for all eight water-context cells and applies existing land/owner
permissions to the complete body. It allows an independently valid paid bridge
on the six exterior water-context cells. It retains complete manager and source
identity checks. The shared runtime-object branch provides no general map-source
exemption.

Inherited operation getter`0x4289f0` reads body byte90. Eligibility
`0x4271d0` reaches that operation virtual and the status predicate`0x426d10`,
which accepts status1/3. Workforce allocator`0x51e310` clears assigned worker
word44 when its actual eligibility fails. The irrigation pass itself requires
assigned workers strictly greater than zero. It contains no full-staff ratio
test or road-entrance query. Retained field`+0x3c` and exact fire/status update
latency remain **UNKNOWN**. Existing World workforce, pause and fire integration
supplies the explicitly **AUTHORED** lifecycle adapter.

No explicit signed-height-plane read occurs in the checked shoreline predicate,
ditch admission, cardinal water traversal or field-range query. Their terrain
masks and source permissions still apply. A separate flat-height or hydraulic
height-difference condition has no evidence in these checked branches.

### Scheduler and connected wet state

**EXE_OBSERVED:** dispatcher`0x4ac2b0` loads raw phase`0xc82ef8` at`0x4ac2e4`,
decrements its temporary table index at`0x4ac2e9`, then dispatches through
`0x4ac4e8`. Table slot26 selects`0x4ac40a→0x5253a0`, so the actual raw phase
is27. Table slot27 selects`0x4ac414`, the following raw phase28. After the callback,
`0x4ac4cd` increments the raw phase, compares51 and resets it to0 before the
completed-cycle callback. With the existing World scheduler's dispatch-before-
increment position0–50, the irrigation opportunity is completed tick28 and then
every51 ticks. Wall-clock speed remains governed by the existing authored mapping.

Pass`0x5253a0` first invokes dry reset`0x525540`, then visits the actual registered
Pump collection. It gates each Pump on signed worker word44 greater than zero,
reads physical orientation from own-info60, clears visited state and traverses
the four exterior seeds above. This pass supplies its network through eligible
Pumps. Direct river adjacency has no seed branch in the checked pass.

Cardinal traversal`0x525470` requires actual ditch800, rejects source80000 and
water4, and permits an occupying object only when its actual type is126
RoadBlock. A Pump203 body stops this traversal. Qualifying unoccupied ditch
images move from the dry bank to the wet bank by subtracting18. A RoadBlock
conducts traversal while retaining its own image. The visited marker bounds
revisits across north/east/south/west edges. Dry reset also retains the original
runtime flag80 and occupied-cell image exceptions. Those raw rendering/runtime
flags remain source observations.

Group`0x435` contains eighteen wet variants followed by eighteen dry variants.
Topology publisher`0x524e90` preserves the selected bank, and`0x524f50` derives
road-crossing variants16/17. Physical view0 uses variant16 for a legal north/south
road axis and17 for east/west. These source sprite selectors describe presentation.
World derives presentation from its retained installations and supplied-water cache.

### Flood veto and field query

**EXE_OBSERVED:** `0x4c15b0` examines square perimeters at Chebyshev distance1
through5 around the actual field. The perimeter counts8/16/24/32/40 give at
most120 candidates and exclude the field cell itself. Each candidate needs
ditch800, lacks source80000, and passes wet query`0x525610`.

That query's additional global condition is now bound to **cFlood**.
Accessor`0x425260` returns the global at`0x10af4e0`, and`0x425250` reads its
active byte8. Constructor`0x4ed240` installs vtable`0x7b2bb8`. Its complete-object
locator`0x7d22d8` binds type descriptor`0x84fb78`, whose RTTI name is
`.?AVcFlood@@`. Constructor reset`0x4ed290` clears byte8 at`0x4ed2a6`, activation
sets it at`0x4ed36b`, and termination clears it at`0x4ed419`. An active flag
suppresses wet-query success. Broader Flood event admission, timing, geography
and saved-state lifetime remain **UNKNOWN**.

For an ordinary unoccupied ditch, wet query uses its retained image's offset
below18. For actual RoadBlock126, it checks the four cardinal neighboring
images. Other occupying types refuse. The original query also initializes a
zero image lazily and uses signed offset comparisons. These image dependencies
stay within Original source observation and inspection. Rule26 uses actual
World-supplied ditch state for the field query.

The checked harvest callback queries irrigation after deriving the base yield.
At`0x4c1ea3`, a false query continues directly to the existing calculation at
`0x4c1ec6`. A true query adds a field-class-dependent runtime fertility modifier,
float0.35 at`0x7ae7c4` for Food195–199 or0.2 at`0x7ae7c8` for plants26/27/28.
The source therefore has a base harvest path when the irrigation query is false.
Rice's separate serialized fertility100 construction predicate remains exact.
The construction operand and runtime fertility occupy separate domains. The
observed harvest branches provide no universal Rice Pump prerequisite. Final
crop work, yield conversion and Original harvest-clock activation retain their
existing evidence gates.

### Integrated Rule26 boundary

**AUTHORED:** Rule26/policy3/schema42 uses marker
`authored-agriculture-irrigation-v1` and irrigation revision1. Its immutable
source dependency extends the existing `OriginalTerrainFacts` with full raw
terrain and the independently checked Diamond-margin mask. It retains the
same shared map reader and complete map SHA-256. Existing model factories keep
their historical capabilities. The separate irrigation factory prepares202/203.

World owns paid ditch cell identities, placement ticks/sequences, Pump bodies
and orientations, eligibility epochs and construction/removal accounting.
Mutable wet/supplier caches are derived from the most recent phase27 opportunity,
eligible Pumps, retained installation times and legal cardinal connectivity.
Current pause, staffing, fire and cuts invalidate supply through the World
adapter. Original raw ditch flags and image banks remain source inputs.
The bounded256-cell gesture is the existing OpenEmperor interaction budget.
Atomic command validation and per-cell paid identities define its transaction.

Schema42 stores both inherited Agriculture and Plantation ledgers together with
the explicit irrigation installation state. It regenerates wet/supplier caches
after exact source-bound restore. Its own writer requires a schema42 target,
and explicit historical restore paths keep their exact generation checks.
Historical Rules1–25 and schemas1–41 retain their original contracts.

Numeric irrigation yield effects remain **PARTIAL / INACTIVE** in this generation.
Rule26 retains the approved authored Wheat/Millet Food adapter and material
plantation cycles. The observed0.35/0.2 runtime-fertility operands require the
remaining final-yield and work-clock contracts before activation. Rice/Cabbage/
Soy production retains its existing locked state.

The pinned inputs at the start of this contract cover these observations.
Ignored private Ditch receipts contain111 byte-backed assertions across34 bounded
windows, including dispatcher indexing and Flood RTTI, and the separate Pump
receipts cover shoreline/factory/workforce evidence. Original execution remains
unperformed. Full original rollback/removal history, shared runtime-object
shoreline variants, Flood lifecycle and Pump animation cadence remain open.

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

### Model29 is a submenu marker, not an Orchard producer

**MODEL_OBSERVED:** row29 is named `Orchard Shed`, with raw Cost30 and Employees10.
Those text fields do not prove a purchasable producer, final price or footprint.
The fresh bounded identity proof is retained under ignored
`.local/agri-03/orchard-proof/`. It checks39 windows /2,673 decoded instruction
lines /7,106 overlapping instruction bytes against the pinned EXE with zero
mismatches. EXE, model and FarmConfig SHA-256, sizes, modes and modification times
remain unchanged; the Original executable is not run.

**EXE_OBSERVED:** the definition at `0x823598+24*29` has initial side0 and body
group0. Its final definition field is97, also present for237/238/239, but that
shared field does not alias the runtime type or supply those producers' bodies.
The concrete237/238/239 definitions instead have side2 and distinct body groups.
The shared initializer `0x428aa0` preserves the requested model type at object
`+0x14` and copies the registered side into object`+0x07`; it contains no29-to237–239
conversion. The model's `SZE` desirability column must not replace this side.

**EXE_OBSERVED:** the UI submenu mapper`0x403c80` positively assigns29 to menu30:
the type29 selector byte at`0x403db8+(29−24)` chooses the branch`0x403cad`,
which returns30. The generic selection path loads its candidate tool type at
`0x4046c7–0x4046d9`, calls that mapper at`0x404731`, and handles its nonnegative
result by setting submenu30 at`0x40473d`, clearing the selected construction type
at`0x40475a` and returning before producer selection. Menu30's concrete options1–3
at`0x822064/0x822068/0x82206c` are238/239/237. This proves submenu semantics for29
on this path; it does not convert29 into an independently placed Orchard or
Fruit producer. The ordinary menu-table matches for the value29 lie in special
trade categories7/19, whose values undergo partner lookup rather than literal
building selection. They do not independently prove current ordinary-menu
reachability of29. The fresh UI captures remain private under
`.local/agri-03/orchard-visual-proof/`.

For an explicitly supplied type29, the checked normal manager path
`0x42d360→0x51c660` selects the broad Industrial family. Every specialized
Industrial predicate rejects29, including the field-family predicate
`0x4c11b0` and the Farm/Orchard predicate `0x4c2930`. The latter accepts exactly
192/193/237/238/239, and its constructor `0x4c2960` also excludes29. The actual
type29 path reaches the generic fallback `0x51c96f→0x51c9a0`, whose published
vtable`0x7b65e4` is qualified by RTTI as `cIndustrialBldg`. Its update slot`+0x20`
points to `0x428f00`, a literal false-returning no-op. The next RTTI table boundary
is at`+0x25c`; this generic class has none of the Farm extension slots
`+0x260/+0x264/+0x268`. Reading through that boundary as though29 were a Farm
would incorrectly attribute another class's methods to it.

The actual Orchard factory case for237/238/239 instead calls
`0x4c2a19→0x4c5cc0`, publishing RTTI-qualified `cOrchardHold` at`0x7af95c`.
It has update`+0x20→0x4c5310` and actual harvest-return receipt
`+0x268→0x4c2e50`. These are distinct concrete objects, not aliases obtained
by constructing29. Their existing Tea/Lacquer/RawSilk plant and physical-return
contracts remain separate;26/27/28 must not be relabeled as fruit plants.

**SOURCE_VERIFIED — shipped manual, printed pp.45–46:** the Orchard section
describes Tea Curing Sheds with Tea Bushes, Lacquer Refineries with Lacquer Trees,
and Silkworm Sheds with Mulberry Trees. Their products are tea, lacquer and raw
silk; these pages describe no edible Fruit product. This independently
corroborates the three material chains above, without establishing a purchasable
model29 producer or any World-tick timing. The read-only `EmperorManual.pdf`
is9,766,732 bytes, SHA-256
`b785835967acafeca73162bc21e351ed8bb0ac5fcbe96fab405d8b3fb5174fad`;
its size, mode and modification time remain unchanged during this check.

**SOURCE_VERIFIED / EXE_OBSERVED:** an independent read-only commodity audit
checks all29 name pointers at`0x872030` (IDs0–28) against all29 named
`Model/Trade.txt` DefaultPrices rows. Neither catalog contains a Fruit resource.
The actual field-output selector`0x4c18c0` maps26/27/28 to Tea13/Lacquer14/RawSilk12;
the concrete Orchard update`0x4c5310` maps237/238/239 to the same three resources.
These are typed commodity bindings, not evidence for edible fruit.

The actual Food predicate`0x5db4c0` accepts resources1–9, including Salt8 and
Spices9; it does not admit an additional unnamed fruit. The checked Market
consumer`0x5469b3` uses this predicate and then rewrites the accepted Food category
to Dinners28 at`0x5469bf`, before writing actual actor cargo at`0x546a1e`.
The House/Market path`0x543878–0x5438ec` likewise uses28. These observations
identify the source ingredient/category boundary, not a complete cooking recipe,
conversion quantity or FoodShop lifecycle. The private independent receipt is
`.local/agri-03/food-catalog-proof/verified-catalog.json`; its EXE, Trade and model
SHA-256/size/mode/mtime guards remain unchanged. The bounded catalog absence is
not a claim about every other game revision or an authorization to invent a
new Original resource ID.

**UNKNOWN:** a complete classification of external or exceptional callers, the
historical reason for retaining29, and its current ordinary-menu reachability. Generic
allocation or a side0 publication call does not establish successful player
construction, a valid footprint or a fruit lifecycle. The static factory proof
supports refusing to admit29 as an Original orchard producer; it does not prove
that every use of the numeric ID is unreachable. No fruit food, crop, yield,
calendar, worker lifecycle or paid generic orchard is inferred from its label.

#### Concrete plantation state and the Rule25 adapter

The corrected plantation scope is the three actual plant/source pairs below.
The fresh private checker
`.local/agri-03/plantation-source/verify_plantations.py` verifies26 bounded
windows /3,045 decoded lines /8,746 overlapping instruction bytes against the
pinned EXE. All EXE/model/FarmConfig SHA-256, size, mode and modification-time
guards remain unchanged. The Original program is not executed.

| Plant / actual source / commodity | Initial sides, plant / source | Raw start slots | Raw end slots | Quantity addition for work byte>32 /1–32 |
|---|---|---|---|---|
|Tea26 / Curing Shed237 / Tea13|1 /2|2,5,8|4,7,10|105 /53|
|Lacquer27 / Refinery238 / Lacquer14|1 /2|1|6,7|125 /63|
|Mulberry28 / Silkworm Shed239 / RawSilk12|1 /2|3,6|5,8|157 /79|

**EXE_OBSERVED:** plant constructor`0x4c2710` publishes vtable`0x7aef50`.
Its quantity callback`+0x25c→0x4c2730` returns0 when the work low byte is0.
Tea and Mulberry additionally suppress additions in the last configured end
slot through`0x4c16c0`; Lacquer suppresses additions in any configured end slot
through`0x4c16f0`. The shared driver`0x4c0f60` applies the retained countdown,
adds the callback result to quantity and caps it at10000. These are quantity
increments conditioned on actual work state, not ticks, elapsed seconds or
proof that staffing alone grows a plant. Raw season slots do not close the
source-to-World clock or every repeated tending/harvest transition.

The actual source range callback`+0x260→0x4c31f0` reads3 from`0x84c550`.
The endpoint tests in`0x4c55a0` allow a plant coordinate within that range of
either footprint endpoint on each axis: with an initial2×2 source, offsets
−3 through+4 relative to its origin, subject to the remaining placement vetoes.
`0x4c0800` and`0x4c3440` qualify the matching source/plant types. This proves
typed geometry, not an immutable single-owner field assignment. The model
reports Employees12 for each source and0 for each plant. The source dispatch
uses actual staffing percentage and throttles attempts; OpenEmperor's full
staffing gate is a separate authored choice.

Tending completion`0x4c1d20` sets work bit`0x40`, while shared decay removes
the low-byte work contribution over source phase cycles. The actual grower
has source reference`+0x62` and a subsequently selected plant reference`+0x68`.
The harvest-spawn branch explicitly initializes the latter to0; later work
selection assigns its actual target. Harvest callback`0x4c1dd0` requires the
retained harvest mark`0x8000`, calculates yield, adds it to that actor's`+0x7c`
and resets the retained plant's quantity. It does not delete the plant or
purchase another plant in that callback. Only actual return result8 at
`0x4ccca8` reaches source receipt`0x4c2e50` through`0x4cccbf`; the source then
adds real returned yield to stock`+0x40`.

The yield calculation includes retained quantity, fertility, crop/region and
separate global modifiers. When the plant irrigation query`0x4c15b0` succeeds,
the26/27/28 branch conditionally adds the float0.2 at`0x7ae7c8` to the yield's
fertility factor. This is a conditional yield modifier, not proof of a universal
irrigation prerequisite. Complete irrigation admission, global modifier meaning,
x87 rounding, raw-to-World unit conversion, accepted-purchase initial quantity,
full actor route/search cadence and recurring seasonal regeneration remain
**UNKNOWN**. Original100/400 cart thresholds do not establish a World inventory
capacity or a one-unit harvest rule.

**AUTHORED Rule25 boundary:** the explicitly selected plantation adapter retains
80 productive World ticks per paid plant and exactly one finite ready unit.
Picking that unit moves it onto a real grower; only physical return credits
the existing Tea/five-material ledgers. The retained plant can start another
authored cycle. Explicit chosen-source binding, shared five-tick land movement,
full staffing, road, pause and fire gates are authored World rules. The
source-proved range3 is used with an explicitly chosen matching paid source;
that binding is not claimed as Original ownership parity. The32-plant and
six-source bounds remain inside the existing82-body/58-channel budgets.
Existing buffer8, Warehouse32 and one-unit downstream recipes retain their
authored units. This adds no independent calendar or irrigation engine and does
not activate the unverified Food-field clock. Historical Rules1–24 and their
save contracts retain their exact existing behavior without automatic migration.
Separately, Rule25 expressly retains the existing **AUTHORED** Wheat/Millet
80-productive-tick Food adapter, with one productive field per Farm, real
four-portion transports and scores20/80. It uses the sole five-kind Food ledger;
it does not claim these Original work/season transitions. Rice/Cabbage/Soy remain
unproductive and all Rule24 Food stays closed.

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
2. Complete post-purchase and exceptional field lifecycle following the verified
   fresh Food-field constructor zeroes, tending/harvest transitions, worker
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
