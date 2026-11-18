# Original agriculture: field, calendar, irrigation and harvest boundaries

This is a maintained source contract for the agriculture preparation work. It
extends the [production candidate register](../original-economy/oe55-production.md)
and the earlier [farm/field presentation investigation](original-presentation-correction.md#farm-bodies-and-field-state-boundary).
It does not admit a building, field, source marker, material, movement channel,
growth rule or save generation to `World`. Published Rules1–26/schemas1–42
retain their separate contracts, including the authored Wheat/Millet and
material-plantation adapters where supported.

The separate [Playable Agriculture Rule27/schema43 contract](../original-economy/agriculture.md#playable-agriculture-agri-mac01)
uses explicitly AUTHORED production rules on native macOS. It does not claim
the unresolved Original raw-yield/FP lifecycle described here. The Original
runtime investigation is not a prerequisite for that new gameplay mode;
historical references below to unused27/43 describe their earlier research
stage. AGRI-MAC01 executes no Original binary and supplies no new live x87
observation. This document retains the static evidence and unknown Original
inputs without treating authored Food portions as native raw Harvest units.

The separate [Playable Fishing Rule28/schema44 contract](../original-economy/agriculture.md#playable-fishing-agri-mac02)
adds AUTHORED physical boat work, finite ecology and six-kind Food logistics on
native macOS. FishingQuay31/Fish2 identities and five-cell shore geometry reuse
these documented facts. World portions,5-tick cardinal water edges,40-tick catch
work,20-tick actual dock unload and8-capacity/240-tick ecology do not reinterpret
Original raw100/400 or callback408/204. Validated179 manager records provide only
geography; the native header-array/navigation-plane binding below is still
unreconstructed. Static Original Quay pixels use an authored pose/anchor choice;
Boat49 animation remains an explicitly labelled fallback. This gameplay does not
activate the Original callbacks or claim their parity, and runs no Original EXE.

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
AGRI-05 follow-up below closes the four oriented claimed-cell layouts and the
bounded fishing-component preflight. Its navigation-byte inputs, full World
admission and attachment lifecycle remain activation gates.

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

### AGRI-05 bounded harvesting rules and remaining activation gates

The follow-up uses the same pinned EXE/model/FarmConfig identities. The private
receipt `.local/agri-05/fishing/verified-fishing-source-report.json` checks 50
assertions across 13 windows, 1,668 decoded instruction lines and 4,796 instruction
bytes including overlaps. The expanded navigation/header/callback receipt
`.local/agri-05/fishing/verified-fishing-followup-report.json` checks 124
assertions across 20 windows, 2,580 lines and 8,672 bytes. The hunting/counter
receipt `.local/agri-05/hunting/verified-hunting.json` checks 70 assertions
across 16 windows, 1,631 lines and 4,941 bytes. The prey-lifecycle receipt
`.local/agri-05/hunting/verified-prey-lifecycle.json` checks 81 assertions across
13 windows, 1,513 lines and 5,086 bytes. The independent raw-unit receipt
`.local/agri-05/units/verified-units-report.json` checks 43 assertions across 25
windows, 4,990 lines and 13,583 bytes. Every recorded window has zero byte
mismatches. Original execution and personal-save inspection remain unperformed.
The three supplied files retain exact SHA-256, size, mode and modification time.

**MODEL_OBSERVED:** Quay31 has raw Cost50/Employees10 and Hunter33 has raw
Cost32/Employees15. The virtual output selectors independently identify
Fish resource2 and Meat resource4. The model's raw Resource column is a separate
operand. A modified label, cost, staffing value or Resource column does not
establish a new runtime recipe, final purchase transaction or World Food unit.

**EXE_OBSERVED, complete Quay claims:** actual purchase calls the31 predicate at
`0x4b19ab→0x42b740`, then the shared shore validator at`0x4b19c5→0x4b8200`.
Construction callback`0x5f3c40` calls that validator at`0x5f3c54`, resolves its
orientation through`0x5f3c85→0x4b84b0`, and issues the additional1×1 owner claim
at`0x5f3cd7→0x4b72b0`. The extra write uses mask`0x0400000c`. The four offsets
come from table`0x875e00`. Each lies outside the ordinary2×2 body, giving five
distinct occupied cells. Coordinates below are relative to the body origin.

| Orientation | Extra occupied dock cell | Required eight-water context | Two land cells |
|---|---|---|---|
|0 |(0,−1) |x−1..2, y−1..0 |(0,1), (1,1) |
|1 |(2,0) |x1..2, y−1..2 |(0,0), (0,1) |
|2 |(1,2) |x−1..2, y1..2 |(0,0), (1,0) |
|3 |(−1,1) |x−1..0, y−1..2 |(1,0), (1,1) |

The shore contexts share the separately observed pump geometry. Two water cells
belong to the base body and the exterior dock claims another water-context cell.
The remaining context cells participate in admission. A prospective World
building must retain the body and dock together for placement, occupancy,
navigation endpoints, inspection and restore.

**EXE_OBSERVED, fishing-component admission:** the purchase path obtains the
attribute map through`0x4b19f5→0x5aca50` at global`0x1313018`, then calls
`0x4b19fc→0x5acac0`. This helper requires bit2 at any of the body's eight
cardinal perimeter cells. The table pairs at`0x820038/0x8204f0` order those cells
N0,N1,E0,E1,S1,S0,W1,W0 and exclude corners. It checks access to a prepared
fishing component.

The attribute update`0x5acb80` clears that map, reads all eight signed
FishingPoint179 coordinate pairs at`0xc5cd60/0xc5cd70`, and calls the flood helper
at`0x5acc09→0x5accd0`. That helper traverses cardinal storage neighbors±228/±1,
excludes navigation-plane bytes`0xff/0xfe` from`0x136beb0`, and writes bit2
through`0x5acd28→0x5aca60`. This closes the bounded component operation for
explicit navigation bytes and point operands.

The separate navigation update `0x5ad970–0x5add50` requires raw water bit4 at
the candidate cell and all eight neighbors. Further tests involve raw terrain
bits, retained navigation values and another plane/helper. Model56 and210 have
separate exclusion branches, and31 has its own handling. These are necessary
conditions in that path, not a sufficient general boat-admission predicate.
The full rebuild calls navigation preparation at `0x53d202` before component
preparation at `0x53d249`. The boat-distance preparation `0x5b2c30` differs from
the origin-component flood: `0xff` is blocked, but `0xfe` contributes an extra
raw cost4 in each of the four cardinal-neighbor branches. Its distance plane is
DWORD-sized and its deferral plane BYTE-sized. Reusing the component flood's
`0xfe/0xff` exclusion as a complete boat route rule would be incorrect. The pure
navigation helpers expose only these explicit byte/cost operands. Raw water
bit4, a displayed shoreline or a parsed marker list cannot supply the unresolved
production planes, route endpoints or mutable navigation history.

The eight coordinate pairs come from distinct Original header fields at
header `+0x280/+0x290`. Serializer `0x52cd90` writes each 16-byte array at
`0x52cef2–0x52cf0d`; the checked header3/header6 load branches restore them at
`0x52d326–0x52d341`. The standalone load caller at `0x52e9cc` is checked.
Reset `0x53cec0` initializes all eight pairs to −1. This proves the checked
header-field provenance, not reconstruction from source-manager179 objects.
Other header versions and the complete relationship between manager markers,
editor operations and the live arrays remain separate gates. Saved IDs,
positions or labels grant no fishing origin or World construction authority.

**EXE_OBSERVED, Hunter source throttling:** update`0x5f3730` chooses these
staffing bands and maximum active-hunter counts. It increments the byte counter
before the strict threshold comparison, then resets the counter before the
subsequent cart/spawn checks. A passed throttle is an attempt boundary.

| Minimum staffing percentage | Strict counter threshold | Active hunter cap |
|---|---|---|
|100 |1 |3 |
|75 |4 |2 |
|50 |16 |1 |
|25 |32 |1 |
|1 |64 |0 |

The table gives the staffing cap before source headroom. The operative quota
also takes the minimum with`floor((400−raw_stock)/100)` in the resolved
nonnegative source-stock domain. A positive staffing percentage can therefore
have zero permitted additional hunters. Source selection, a passed throttle
and a positive quota still require actual eligible prey and successful creation.

The source's stock-capacity callback`+0xe0→0x4f1520` returns raw400. The active
source update consumes that result at`0x5f38fa` when checking stock headroom.
Hunter material initialization`0x5f3a30` starts its retained material stock at
zero. Its completed physical-return branch at`0x4d3404/0x4d3408` adds raw50.
This observed raw source capacity is independent of World Food capacity.

**EXE_OBSERVED, Boat work counters:** state8 increments its signed WORD before
testing the selected fishing comparison. State10 tests the old dock-side WORD
and increments it only when that comparison has not completed. A dock counter
starting at zero therefore reaches its full-staff completion on invocation205.
The completed state10 branch resets its counter and assigns raw100 to the
Quay stock at`0x4cffec`.

| Minimum staffing percentage | Fishing comparison | Dock comparison |
|---|---|---|
|100 |408 |204 |
|75 |816 |408 |
|50 |1224 |612 |
|25 |1632 |816 |
|1 |2040 |1020 |

Zero staffing bypasses these work-counter/completion paths and invokes the
separate interruption/global-override helper`0x4cfb70`. Its lifetime and global
inputs still require a complete actor-state contract. These comparison counts
retain their source callback units. Actor49's actual factory branch installs
base vtable `0x7afe60`; virtual `+0x28→0x4c7580` reaches its registered Boat
callback `0x4cfbc0`. The shared actor loop invokes that slot once per eligible
record at `0x4e2909`, with the active/suspension gates described above. The
ordinary parent calls the scheduler and actor pass in the same iteration, and
the normal scheduler advances/reset phases0–50. This closes the relative
ordinary callback path. Complete suspension/exception histories, seconds,
months and a corresponding World-tick duration are not established here. The
existing one-World-tick adapter remains **AUTHORED**.

**EXE_OBSERVED, bounded huntable identities and initial attempts:** helper
`0x59a6a0` admits actual Actor75 `FIG_WILD_PIG`,76 `FIG_PHEASANT` and77
`FIG_ANTELOPE`. The initializer `0x4eb010`, called at `0x53d1df`, requires
global `0x88ec38` to be zero. It visits the four positive PreyPoint183 coordinate
pairs at `0xc5ce3c/0xc5ce4c`. Helper `0x59cad0` selects one properties row from
raw WORD `0xc5cd58`: 0 selects Antelope77, 1 Pheasant76, every other value
WildPig75. Each row has the observed desired-count field7. Successful group
allocation through `0x54c4f0` permits seven actual creation attempts per valid
point. Only successful `0x4eb348→0x4ea050` calls create actors. Those objects
receive state19, a group ID, an ordinal and a wait value computed as the signed
remainder of the actual random result divided by25. No uniform RNG range is
proved, and seven attempts do not guarantee seven living animals.

Existing-prey registration `0x59a330` enumerates already-created actors.
Actual factory `0x4e1420` selects separate derived constructors for75/76/77;
their virtual `+0x28` binds to `0x59f310`. The separately registered callback
`0x59cf10` and candidate routine `0x59bdc0` therefore do not, by themselves,
prove that an ordinary animal dispatch runs that candidate as a live birth
step. Habitat lookup `0x59ebd0` and terrain predicate `0x59c780` remain separate
operations.

**EXE_OBSERVED, conditional replacement attempts:** actual raw scheduler phases5
and29 select `0x54c6f0→0x4ea8e0`. The dispatcher first subtracts1, so their
zero-based table entries are4 and28. For initializer-written group kind3, the
replacement path requires active group byte1, zero retained gates `+0x42/+0x98`
and retained count `+0x28` below the desired count7 at `+0x29`. It increments
counter `+0x75`, checks strict greater-than26, and resets before candidate
search. Starting at zero requires 27 eligible missing-count callbacks. Up to16
candidate positions must pass further terrain/occupancy checks before the
actual factory call `0x4eaa69→0x4ea050`. Successful replacements receive state19,
the group ID, raw wait10 and raw field `+0x64=1800`. Failed creation grants no
animal. The initializer requires `0x88ec38==0`; this replacement branch instead
skips `0x88ec38==1`. The replacement function also has other group-kind paths
outside this bounded kind3 support. Complete group allocation, candidate
terrain/RNG, retained-count writers/recount, state19 lifetime, reservations,
pursuit interruption and group retirement remain activation gates. Neither
these relative phase calls nor source coordinates establish a World population
ledger, seasonal respawn rule or recreated meat stock on restore.

**EXE_OBSERVED, raw transport continuity and category boundaries:** Actor19's
arrival branches at `0x4cc36b–0x4cc37d` and `0x4cc413–0x4cc425` pass the
retained signed-WORD cargo and resource byte unchanged to the resolved target's
virtual `+0x154`, after its current eligibility check. Market vtable `0x7b6f3c`
binds that receipt to `0x543bc0`. Its accepted-count loop adds one raw unit at
a time through `0x543d13→0x5d2790`, preserving the category. That slot helper
clamps to its explicit capacity and returns the excess. No division by100 or
conversion to World portions occurs in these checked slices. This is a
conditional receipt path, not a complete proof of target selection, all
reservations, overflow routing or return histories.

The separately checked carrier helper `0x546960` removes `100×caller_count`
from the source before checking resources1–9 and rewriting an admitted category
to Dinners28. It then attempts that many Actor25 creations and retains the
rewritten category on each successful object. These actors are vendor
assistants, not Actor19 carts. Their callback has a literal100 branch calling
target `+0x260` and a separately gated literal400 branch calling `+0x154`.
The Food/Dinners boundary therefore contains real category changes and distinct
objects; a raw100 source debit alone does not prove a World portion count or
one final House dinner.

The score consumer `0x541760` receives an external caller-supplied grade. In
its Dinners28 branch, its stock blend includes `20×grade`, the old source score
and actual retained stock, followed by a separate rounding helper and the score
setter `0x545140`. This closes another raw arithmetic input, not the production
of that grade or the complete shop/ingredient recipe. The downstream House
FoodQuality blend remains the separately bounded
[consumption/FoodQuality contract](original-goods-consumption-food-contract.md).
Fish2 and Meat4 still have no proved standalone score or World-unit mapping.

**Implementation boundary:** `HarvestRules` and `FishingRules` express bounded
source arithmetic, identities, geometry and the fishing-component query with
explicit supplied operands. The model-inspector's read-only `--harvest-source`
selection reports raw model columns separately from those source observations,
with inactive gameplay/conversion/clock flags and concrete blockers. The public
CLI syntax uses `--harvest-source 31` or `--harvest-source 33`. It creates no new
World, inventory, source population, work actor, route or save authority.
Each source row identifies the fixed-rule evidence as `EXE_OBSERVED` and names
the pinned EXE SHA-256. The inspector reads the selected model file; those static
rule observations do not imply verification or execution of a selected EXE.

The pure return helper supports Hunter stock0..350 so its added50 remains within
the observed400 source capacity. The actual return instruction performs an
uncapped WORD addition. Source over-capacity/wrap histories remain unsupported
by the capacity-safe helper. The Quay helper supports nonnegative signed-WORD
stock, assigns100 and retains the overwritten raw amount in its result. No
operational Fish capacity is inferred. Ordinary cart-debit helpers require an
explicitly resolved effect byte0; every nonzero effect remains refused.

AGRI-05 remains **PARTIAL** at the playable-chain boundary. Both raw-to-Food
conversions, FoodQuality inputs, complete work/lifetime and ecology contracts,
Actor19 destination acceptance/reservations/overflow/return, and source byte70's
400-cargo/100-debit effect remain unclosed. Source Quay stock assignment and
Hunter stock addition must retain their distinct operations. The five existing
Food columns, Rule26 irrigation, approved harvest adapters and schemas1–42 keep
their contracts while these independently evidenced helpers are integrated.

## AGRI-06 seasonal reference rules and common calendar

The fresh private checker `.local/agri-06/seasons/verify_seasons.py` produces
`verified-seasons.json` with66 explicit checks,9 byte-checked windows,414 decoded
instruction lines and1,141 instruction bytes including overlaps. The pinned
EXE, model and FarmConfig retain their SHA-256, size, mode and modification time.
The Original is not executed and personal saves are not read. The receipt
qualifies the fixed default EXE tables and concrete callbacks below. A selected
model context or raw configuration label does not prove live calendar values,
runtime table edits or complete climate/event binding.

**EXE_OBSERVED**

`0x4c0760` visits four matching positions in start table
`0x7ae22c` and end table `0x7ae19c`. A position contributes only when both
operands are nonnegative and `start<=current_slot<end`. It does not wrap an
interval or pair an extra end with a different start. The descriptors retain
all four positions, sentinel−1 and separate start/end counts. Lacquer's
`[1,−1,−1,−1]` starts and `[6,7,−1,−1]` ends therefore describe one growing
interval `[1,6)`. End7 is not a second growing interval.

The quantity callback's suppression query is a different operation.
`0x4c16c0` returns equality with the last configured end before sentinel−1,
within four entries. `0x4c16f0` accepts equality with any configured end before
that sentinel. Neither helper determines a successful harvest or a completed
worker visit.

| Model | Growing intervals in raw slots | Quantity suppression | Increment for low work byte>32 /1–32 /0 |
|---|---|---|---|
|Wheat195|[2,6)|6|157 /79 /0|
|Millet196|[6,10)|10|157 /79 /0|
|Rice197|[5,9)|9|157 /79 /0|
|Cabbage198|[7,11)|11|157 /79 /0|
|Soybean199 / BeanCurd1|[4,8)|8|157 /79 /0|
|Hemp194 / Hemp19|[3,8)|None in its concrete quantity callback|125 /63 /0|
|Tea26 / Tea13|[2,4),[5,7),[8,10)|Last end10 only|105 /53 /0|
|Lacquer27 / Lacquer14|[1,6)|Any end6 or7|125 /63 /0|
|Mulberry28 / RawSilk12|[3,5),[6,8)|Last end8 only|157 /79 /0|

The Hemp-specific predicate `0x4c0630` admits194. Concrete factory call
`0x4c06e7→0x4c25e0` publishes vtable `0x7aecc8`. Its quantity callback
`+0x25c→0x4c2600` reads only the actual own-info work low byte for the125/63/0
choice. The complete bounded function contains no calendar read, growing-period
query or end-slot suppression. Hemp's `[3,8)` query cannot be substituted as a
veto for that callback. Food callback `0x4c2260` instead uses last-end query
`0x4c16c0`. Plant callback `0x4c2730` uses that query for Tea/Mulberry and
any-end query `0x4c16f0` for Lacquer. Earlier Tea ends4/7 and Mulberry end5 thus
remain unsuppressed by those quantity callbacks despite failing the separate
growing-period query. High work flags or bits8–15 alone do not contribute work.

`original::SeasonRules` implements these bounded reference descriptors and
queries. Food195–199 forwards period and increment arithmetic to the existing
`CropRules` functions. The helpers do not run the shared terrain/countdown/veto
driver, assign workers, advance a clock, mark harvest readiness, consume a
plant, credit cargo or convert raw amounts to World inventories.

**AUTHORED integration**

`AgricultureCalendarStatus` is a disposable projection
of the existing World `completed_ticks` through the `OriginalScheduler` mapping of51 phases,
16 subslots,12 outer slots, elapsed year0 and mapping version1. One World tick
per phase and that epoch remain the established authored interpretation.
No second advancing date, new saved season field, rule, schema or migration is
introduced. Next phase20/27 observations identify the inherited schedule's
next opportunities. Phase27 is exposed only for the irrigation generation.
They do not execute Original field growth or grant an
irrigation yield. Pause, speed and restore continue to use the sole World tick
authority. Raw slots have no newly assigned month names or historical dates.

Per-building seasonal inspection keeps reference growing/suppression queries
apart from actual World staff, operation, fire, roads, water, productive progress,
ready state, current Grower and harvest/return accounting. The existing approved
80-productive-tick Wheat/Millet, Hemp and material-plantation adapters retain
their quantities and clocks. Their progress is not an Original work word;
authored readiness is not proof of the Original harvest mark.

**UNKNOWN / inactive**

Open prerequisites include complete live climate/region inputs, exceptional
calendar/event ordering, Grower targeting and interrupted returns, actual
recurring seasonal readiness/regeneration, complete fertility/irrigation/global
yield modifiers and rounding, source overflow, and raw-to-Food/material units.
Rice/Cabbage/Soy production, Original seasonal replacement of the approved
adapters, hunting/fishing activation and new seasonal sprites remain closed.
AGRI-06 supplies a common derived calendar and tested reference queries. It does
not establish a complete playable Original seasonal production cycle.

## AGRI-02C crop yield inputs and conversion boundary

The fresh private checker `.local/agri-02c/yield/verify_yield.py` produces
`verified-yield.json`. It passes88 explicit checks over8 byte-checked windows,
334 decoded instruction lines and1,291 instruction bytes including overlaps.
The pinned EXE, BuildingModels and FarmConfig retain their SHA-256, size, mode
and modification time. The Original executable is not run and personal saves
are not read. Private exploratory windows and a list of candidate floating-point
control instructions remain separate from the eight qualified receipt windows.

### Crop-qualified doubled integer base

**EXE_OBSERVED** actual harvest `0x4c1dd0` invokes event enable predicate
`0x48e0f0` at `0x4c1e21`. Getter `0x608620` returns the actual object's byte
`+0x50`. The enable predicate requires that byte to be nonzero and DWORD
`+0x54` to equal3. Only then does `0x4c1e31→0x48e680` qualify the actual field
model against the object's DWORD `+0x58`. These are raw object operands.
Their event name, admission, owner, lifetime and restore behavior are not
established by the predicate.

The crop qualifier first rejects unsigned selector values above4. For the
supported Food models195–199 its complete matrix is as follows.

| Raw DWORD selector | Admitted Food models |
|---|---|
|0|Wheat195 only|
|1|Soybean199, whose output remains BeanCurd1|
|2|Millet196 only|
|3|None of Food195–199|
|4|Millet196 only|
|5–4294967295|None|

The dispatch bounds are part of this result. Selector0's branch
`0x48e693` bounds the model to27–195 before its secondary table. Selector2's
`0x48e6c0` bounds it to35–196. Selector3's `0x48e6dd` bounds it to39–194.
Reading adjacent secondary-table bytes beyond those ranges would incorrectly
admit unsupported models. Selector1 directly tests199 or226 and selector4
tests37 or196. The new Food-only component admits neither model226 nor37.
Rice197 and Cabbage198 never qualify through this checked predicate, even
when the enable byte and raw state pass.

`crop_doubled_base` takes a typed Food field, the explicit raw enable byte,
state DWORD and crop selector DWORD. It rejects unsupported field models
before evaluating the raw gates. It does not create or advance an event.
The existing `raw_base_yield` remains unchanged and takes the resolved result
for its already proved physical quantity domain0–10000. A false result uses
integer `Q/100` and a true result uses `Q/50`. This intermediate is not final
yield and provides no conversion to a World Food portion.

The independent tests enumerate all256 enable-byte values, the literal five
by five field/selector matrix, unsigned selector boundaries and nonqualifying
raw states. They also exercise all0–10000 quantity inputs through the unchanged
integer base helper. A nonzero event byte alone never grants the larger base.

### Integer conversion and ambient arithmetic precision

**EXE_OBSERVED** conversion helper `0x7650fc` saves the x87 control word at
`0x765103`, ORs its rounding-control bits with `0x0c00`, installs that temporary
control word, converts the current x87 value to a signed64-bit integer through
`FISTP` at `0x765115`, and restores the saved word at `0x765118`. It returns
the low and high DWORDs in EAX and EDX. This proves truncation toward zero for
that conversion in its valid numeric domain. It does not choose the precision
of earlier arithmetic or the rounding used by earlier binary32 stores.
The precision-control mask `0x0300` is distinct from the modified rounding bits.

The field callback first multiplies the actual runtime fertility byte by
binary32 `0.01` and stores binary32 at `0x4c1e97`. Successful irrigation adds
the separately selected Food0.35 or plant0.2 factor and stores binary32 again
at `0x4c1ec2`. The callback multiplies the integer base by the crop/region factor
and that stored fertility factor before the first truncating call at
`0x4c1eee`. A raw date operand greater than−300 admits the global float multiply
and a second truncating call at `0x4c1f0f`. Neither conversion retroactively
fixes the earlier arithmetic precision or floating-point store rounding.

The fresh independent exact-rational control demonstrates why that distinction
changes an actual integer result. Assume nearest-even binary32 stores, runtime
fertility65, base100, region factor1 and no irrigation addition. The stored
fertility factor is binary32 `0x3f266666`, approximately0.6499999761581421.
With24 significand bits the subsequent multiplication by100 rounds to65,
so truncation returns65. With53 or64 significand bits it remains below65,
so truncation returns64. Both paths start from the same stored binary32 factor
and use the same proved truncating conversion. This is a controlled arithmetic
differential and does not claim that the Original harvest uses any one of
those three ambient precision modes.

The later startup proof below establishes initial53-bit precision while
preserving the preceding rounding bits. The actual source control word during
every admitted harvest, reachable external-call restoration, exception masks
and earlier store rounding remain unclosed at the live-admission boundary.
AGRI-02D below adds exact resolved-context arithmetic after closing the local
transitive call graph. It supplies no guessed live precision default or
production activation.

### Retained harvest effects and remaining inputs

**EXE_OBSERVED** `0x4c1df3` ORs work bit`0x4000` and clears field-body
counter`+0x150` before the harvest-mark test. Actual yield then requires
retained bit`0x8000`. The accepted path consumes that bit before computing the
raw result. At `0x4c1f14` the callback writes the low result WORD to the current
calendar history slot, adds that WORD to the actual Grower's pending amount
`+0x7c`, then clears retained field quantity and field-body assignment. These
effects do not establish eligibility from an arbitrary quantity or a calendar
match. The pending WORD addition has no newly inferred clamp or World capacity.

Runtime fertility still requires the distinct source preparation from
serialized `0xe9f8e0` to runtime `0xeac3f0`. Crop/region factor indexing remains
`3×crop_index+signed_selector` through `0x84c564`. The observed writer
`0x5f5360` checks a value against2 before writing selector WORD `0xc5cd58`,
but that writer alone provides no lower-bound check or complete source/load
validation. Literal initializer values and matching FarmConfig names do not
close all subsequent writes, runtime configuration, date or climate inputs.

Activation still requires admitted live event/region/global inputs, controlled
floating-point operations and complete yield units. Actual Grower assignment,
work, interruption and return, Farm bin overflow, raw-to-Food conversion,
Market/House delivery and active-work persistence retain their separate
evidence gates. Existing approved authored harvest adapters and all historical
rules and schemas remain exact. Rice/Cabbage/BeanCurd production remains
**PARTIAL** while those complete contracts are open.

## AGRI-02C Grower quotas, field tours and raw transport

The independent Grower receipt
`.local/agri-02c/grower/verified-grower-quotas.json` passes104 assertions over19
byte-checked windows,6,070 decoded lines and17,289 instruction bytes including
overlaps. The raw transport receipt
`.local/agri-02c/units/verified-crop-transport-units.json` passes89 assertions
over30 byte-checked windows,4,521 decoded lines and11,965 instruction bytes
including overlaps. Exploratory boundaries are retained privately. The receipt
qualifies those bounded observations and leaves the full native state machine open.
Both receipts preserve the pinned EXE, BuildingModels and FarmConfig hashes,
sizes, modes and modification times. Neither executes the Original executable
or reads personal saves. The same private source installation supports both
reproducer commands below.

```sh
python3 .local/agri-02c/grower/verify_grower_quotas.py
python3 .local/agri-02c/units/verify_units.py
```

### Dispatch arithmetic is separate from live actor admission

**EXE_OBSERVED** parameter initializer`0x4bfc20` supplies Food harvest/tending
limits5/2 at`0x4bfc2a`/`0x4bfc36`, and per-worker candidate budgets4 harvest
and10 tending through configuration offsets`+0x24`/`+0x18`. These are observed
initializer values. Complete mutable configuration and restore writers remain
external. They are not model Employees or an independently admitted FarmConfig
parser.

The actual source dispatcher`0x4c3550` computes the following bounded staffing
tiers from explicitly resolved percentages and configured limits.

| Staff percentage | Harvest limit | Tending limit | Old-byte attempt threshold | Eligible calls from counter0 |
|---|---|---|---:|---:|
|100|configured limit|configured limit|1|2|
|75–99|floor(3×limit/4)|max(1,floor(3×limit/4))|4|5|
|50–74|floor(limit/2)|max(1,floor(limit/2))|8|9|
|25–49|floor(limit/4)|max(1,floor(limit/4))|16|17|
|1–24|0|0|32|33, with no positive quota|
|0|0|0|No advance|Counter retained|

At`0x4c363b–0x4c364d`, the incremented byte is retained but the comparison
uses the old byte. A due attempt resets the counter at`0x4c3666`. The
`crop_dispatch_rule` and `advance_crop_dispatch_attempt` helpers express this
operation with supplied nonnegative configuration. Partial-tier signed-DWORD
multiplication overflow is outside their safe domain and is refused. An attempt
or quota does not establish a successful worker spawn, source eligibility,
field commitment or a World-tick duration.

Initial harvest/tending allocation`0x4c36a0`/`0x4c3980` subtracts retained
source counters and bounds candidate collection by the configured per-worker
budget multiplied by the requested quota. Candidate distribution divides the
actual selected count by remaining worker quota. A successful actual Actor26
creation can stamp that same identity on several fields at`0x4c3839`/`0x4c392e`
or`0x4c3b03`/`0x4c3bec`. Failed creation bypasses the corresponding assignment
and successful-count increment. New actors receive raw role2 harvest or1 tending
and initial wait`10×successful spawn ordinal` at`0x4c388b–0x4c389b` and
`0x4c3b55–0x4c3b65`. These spawn waits are separate from the already bounded
51/25 field-work waits.

Retained Farm counters`+0x150`/`+0x154` participate in quota subtraction,
failed selection and retirement decrement paths. They are **not proved to be
strict live-actor censuses**. Complete cleanup callers, shared assignments,
exceptional histories and native save/restore relationships remain unclosed.
Neither fixed field rotation nor a one-field-per-worker rule follows from
these observations. Equal-score queue ordering is also not qualified by this
receipt.

### Actual repeated selection, work and source return

**EXE_OBSERVED** the initial harvest selector`0x4c0130` requires the retained
harvest mark, exact assignment identity and prepared distance context. Zero
quantity clears the mark rather than yielding food. Its bounded candidate score
includes prepared distance plus half of100 minus quantity percentage. Initial
tending selector`0x4c0380` separately requires a growing-period query, exact
assignment, absence of the harvest mark and its work, runtime-fertility-query
and distance inputs. See the corrected AGRI-02D score qualification below.
These reference operands do not initialize an independent World route engine.

The role distinction is important. Harvest selection state10 calls
`0x4c2b20→0x4c0060`. Tending selection state6 calls
`0x4c2c70→0x4bff80` and rechecks the growing-period query. The active-target and
raw terrain`0x100` exclusion at`0x4bffc9`/`0x4bffd9` belong to repeated
**tending** selection. They must not be relabeled as a universal harvest veto.
Farm field-range callback`+0x26c→0x4c3c60` tests the supplied radius relative
to the actual footprint edges on both axes. Complete source-manager, distance
plane, admission and exceptional operation/veto inputs remain separate gates.

Failed harvest selection inspects actual pending Grower cargo`+0x7c` at
`0x4c2bbc`. Positive cargo selects source-return state13. Zero cargo selects
state9, and the retained harvest counter is decremented. Failed tending
selection chooses state9 and decrements its separate retained counter.
Finished tending returns to selection state6 at`0x4c2e3e`. Finished harvest
returns to selection state10 at`0x4c304e`. The actual source receipt requires
movement outcome8 at`0x4ccca8` before virtual`+0x268` at`0x4cccbf`.
Source staffing, proximity or a calendar match cannot substitute for that
physical return. The observed move/work branches and their failures still do
not constitute a complete World-ready actor machine, including interruption,
retirement and restored concurrent commitments.

### Five native Farm bins and actual Cart acceptance

**EXE_OBSERVED** Farm constructor`0x4c5b80` constructs five16-byte bins at
body`+0x158`, using bin constructor`0x4c43b0`. Each starts with resource0,
quantity0 and **capacity400**. Native resource, quantity and capacity members
are DWORDs.400 is the constructor's initial capacity, not proof that every
subsequent loaded or mutated capacity remains400.

Selector`0x4c5090` returns the first resource match before a remembered first
empty bin. Predicate`0x4b04f0` requires **both resource0 and quantity0** for
emptiness. A retained nonzero resource identity with quantity0 remains a
matching typed bin, not a generic free bin. Credit`0x5d2790` assigns the caller's
resource, adds the offered amount, clamps to the bin's explicit capacity and
returns excess. Debit`0x5d2760` floors an insufficient result at0 and clears
the empty identity only when its separate flag is true.

Actual Farm return`0x4c4ee0` reads the field-derived resource and the returning
Grower's signed-WORD cargo`+0x7c`. It offers that raw amount unchanged to the
selected bin at`0x4c4f46`. The caller immediately reads bin quantity and does
not use the returned excess. The harvest's preceding WORD accumulation at
`0x4c1f1f` likewise has no newly inferred saturation. Thus safe World overflow
conservation cannot be claimed from these native operations alone.

At bin quantity at least400, after actual access checks, the Farm attempts a
separate Actor19 cart at`0x4c4f99`. Failure bypasses debit. Only successful
creation assigns source/resource, raw cargo400 at Actor`+0x82`, and debits400
at`0x4c4fde`. Actor19 is`FIG_CART`, with callback`0x4cbec0` bound at`0x84ea7c`.
Raw400 remains raw400 through the checked target receipt calls. It is neither
four OpenEmperor portions nor evidence for the proposed1000-raw mapping.

The target's `+0x154` return is an **accepted count**, not overflow.
`0x4e2a20` compares it to original cargo at`0x4e2aef`. Full acceptance clears
cargo and chooses source-return state13. Partial acceptance subtracts only that
count at`0x4e2b24`, then calls destination reselection`0x4e2960`. Failed
reselection chooses search state6 and retains residual cargo. This closes the
local acceptance arithmetic, not the target's complete admission or all route,
waiting and retirement histories. Helper`0x4e8a30` only clears route ownership
and Actor route-slot`+0x42`. Stock refunds and retirement require separate
operations and caller evidence.

Market receipt`0x543bc0` conditionally adds individual raw units through
`0x543d13→0x5d2790` and returns its admitted count on its completed path. Its
earlier policy, slot, capacity and reservation predicates must remain qualified.
The premature no-candidate path returns0. Proving that partial slot mutation
cannot reach it requires those complete preconditions. Source return, rejected
arrival, exceptional cargo disposal and final actor cleanup remain unclosed.

`CropTransportRules` provides a Food-only safe subset for resources1/3/5/6/7,
resolved through the existing `CropRules`. Its `RawFarmBin` carries explicit
capacity, with constructor default400. Its nonnegative0–32767 quantity/capacity
domain is a word-safe API restriction, not the width of the native DWORD bins.
It rejects unsupported resources, mismatched retained identities, hostile bins
and accepted counts above physical cargo before any result is returned.
Credit reports accepted and overflow separately. Debit reports actual removed
quantity and optional empty-identity clearing. Cart receipt reports accepted,
residual and only the observed return/reselection branch. These pure helpers
create no World stock, worker, delivery, reservation, portion or save authority.

### FoodShop/Dinners and House units remain a separate boundary

**EXE_OBSERVED** carrier helper`0x546960` debits100 multiplied by requested
assistant count through source virtual`+0x298` before attempting actual Actor25
creations. Its Food-resource predicate can rewrite category1–9 to Dinners28.
The count helpers`0x5467a0`/`0x5468a0` use100-unit source-quantity steps and
separate bounded assistant counts. These are native carrier operations, not
OpenEmperor Food portions. Helper`0x4e3d60` follows linked actor identities at
`+0x72`. The Food grade producer remains a separate proof dependency.

The existing source-score operation receives an external grade and the House
FoodQuality blend uses actual previous House stock, as described in the
[FoodQuality contract](original-goods-consumption-food-contract.md). Complete
ingredient recipe, grade production, category rewrite, House quantity and
consumption scaling are still required before treating any raw harvest amount
as one household portion. Neither raw100 carrier debits, raw400 carts nor a
display division by100 proves the proposed raw1000 conversion.

### Fourteen remaining AGRI-02C activation gates

| Gate | Closed bounded observation | Remaining activation prerequisite |
|---|---|---|
|1. Spawn quotas|Staff tiers, default limits and old-byte attempts|Admitted live configuration, source gates and complete counter history|
|2. Assignment and multiple visits|Candidate bounds, shared assignments and role selectors|Complete commitments, distance context, queue ties and cleanup|
|3. Worker machine|Checked select/move/work/return branches and51/25 waits|All shared movement outcomes, exceptional actor gates and lifecycle|
|4. Failed selection|Cargo-dependent harvest return and separate tending return|Complete assignment release, counter reconciliation and repeated retries|
|5. Interrupted work|Separate source/field operation and veto operands|All pause, fire, road-cut, invalid source and retirement histories|
|6. Harvest trigger|Actual marked-field callback and real source receipt|Complete mark/readiness lifecycle with valid work and returning actor|
|7. Fertility and climate|Separate serialized/runtime bytes and bounded transform|Admitted live modifier, preparation ordering and complete climate context|
|8. Crop/region factors|Observed raw index and selected event matrix|Complete source/load selector validation and live regional factors|
|9. Global yield effects|Observed event/date predicates and arithmetic sequence|Actual event/global inputs, writers and restore lifecycle|
|10. x87 arithmetic|Truncating conversion helper and precision counterexample|Ambient arithmetic precision, earlier store rounding and control restoration|
|11. Raw yield to Food|Unchanged raw source/bin/Cart operands and Dinners boundary|Complete recipe, grade, House-consumption units and explicit World mapping|
|12. Overflow|Initial capacity400, clamp/excess and WORD accumulation|Later capacities, source-ignored excess, pending wrap and conservation policy|
|13. Physical return/storage|Successful400 handoff and accepted/residual counts|Complete target admission, reservations, rejected arrival and exceptional cargo fate|
|14. Active-work persistence|Existing historical schemas remain exact|A new explicit World contract validating all worker, assignment, cargo and replay state|

The independently tested dispatch, event and raw-transport components narrow
these gaps. They do not activate Rice, Cabbage or BeanCurd production. A new
World generation and any authored field rotation, calendar adaptation, overflow
policy or raw-to-portion mapping still require their own explicit decision.

## Further Original reconstruction: Food field marking and expiration

The fresh private verifier
`.local/agri-original-followup/grower/verify_lifecycle.py` produces
`verified-field-lifecycle.json`: 92 assertions over 11 qualified byte-checked
windows, 1,062 decoded instruction lines and 3,415 instruction bytes including
overlaps. The pinned EXE, BuildingModels and FarmConfig retain their SHA-256,
size, mode and modification time. Neither the Original executable nor personal
saves are used as runtime inputs. Exploratory captures remain separate from the
qualified receipt windows.

### A schedule mark is separate from productive work and quantity

**EXE_OBSERVED** the actual field driver calls schedule update at
`0x51e4a2→0x4c1a90` before visiting eligible field growth at
`0x51e50a→0x4c0f60`. Schedule update scans up to four configured end slots per
crop, stopping at −1. A current-slot match reaches setter `0x4c1a20` only when
the actual subslot `0xc82ef0` is 0. The setter visits retained fields whose active
byte is exactly 1 and whose actual crop classification matches its argument.
Its sole field-word effect at `0x4c1a6e` ORs high-byte bit `0x80`, or work-word
bit `0x8000`. It has no quantity, staffing, assignment or terrain predicate.

The five Food rows retain the single end slots already documented above:
Wheat 6, Millet 10, Rice 9, Cabbage 11 and Soybean 8; each row's next three entries
are −1. A zero-quantity field can therefore acquire this schedule mark. That fact
does not prove harvest readiness, successful dispatch or a food origin. The
initial harvest candidate path explicitly detects zero quantity at `0x4c020b`
and clears the mark at `0x4c022f`. It does not use 1000 as a readiness threshold.

The actual Food concrete vtable `0x7aea40` binds growth publisher `+0x260` to
`0x4c2360`. Its complete checked method selects Rice topology or the ordinary
crop image/quantity stage and reaches image publisher `0x4b72b0`. It does not
write the field work word. Thus the publisher called after growth is not a
second harvest-mark or productive-readiness writer.

Actual harvest callback `0x4c1dd0` remains separate. Before testing the schedule
mark it sets work bit `0x4000` and clears field-body counter `+0x150`. An unmarked
call returns without consuming quantity or clearing assignment. A marked call
consumes `0x8000`, computes the separately gated yield, writes the current history
slot, adds the raw result WORD to the actual Grower's pending cargo and clears
quantity/assignment. These operations retain their existing worker, live yield,
rounding and pending-WORD overflow gates.

### Completed old end slots expire unharvested field quantity

**EXE_OBSERVED** parent `0x4ac650` increments the retained subslot before its
comparison with 15. On an ordinary rollover, while the old outer slot is still
current, slots 6–11 reach `0x4ac6fb→0x4bff20`. That manager visits active fields,
derives each actual crop index and uses `0x4c16c0` to compare the supplied slot
with the last nonsentinel configured end slot. Only matching fields reach reset
`0x4c1720`.

The complete reset clears the entire work/harvest word and quantity. When the
field retains a nonzero assignment, it resolves that actual actor, clears its
signed wait WORD `+0x3e` and then clears the field assignment WORD `+0x2e`.
An unassigned field bypasses actor lookup and wait mutation. The method has no
countdown, history or field-body `+0x150` store. It does not clear the actor's
retained field reference, pending cargo or work state, retire that actor or
reconcile the Farm's retained dispatch counters. Those later histories remain
outside this reset observation.

Only later does the parent clear the subslot at `0x4ac799`, increment the outer
slot at `0x4ac79f` and publish it at `0x4ac7a3`, wrapping after 11. The checked
ordinary path reaches work decay still later at `0x4acc28`. Other callbacks and
exceptional parent paths remain external. The resulting bounded sequence is an
end-slot/subslot 0 mark, its retained harvest window, then expiration before the
next outer slot. Native unharvested field quantity expiration is distinct from
conservation of already produced Food, Farm stock or physical actor cargo.

### Stateless Food-only event operations

`CropLifecycleRules` extends the existing crop-rules library without changing
historical `CropRules.h`. It resolves Food identities through `crop_rule` and
owns no scheduler, World actor, inventory or save state.
`schedule_crop_harvest_mark` takes explicit slot 0–11, subslot 0–15, active-field
fact and work word. `expire_crop_completed_slot` takes an explicitly resolved
completed-old-slot event, active-field fact and only quantity/work/assignment
presence. Its supported quantity is 0–10000. The result reports the updated
fields, expired field quantity, whether reset applied and whether the caller
must clear its actual assigned actor's wait. It does not resolve or admit an
actor. Countdown/history/body-counter retention is source evidence, rather than
additional mutable helper fields.

Independent tests use literal crop/end-slot expectations, every 16-bit work
word across all twelve slots, first/last subslots, inactive fields, assignment
presence, zero quantity and boundaries around 1000. They preserve the observed
mark-before-expiration order and reject unsupported models, calendar inputs and
quantities. No invocation maps these events to World ticks or activates Original
production. Full worker pause/fire/path/removal cleanup, restored commitments,
yield inputs and Food units remain separate activation gates.

## Further Original reconstruction: startup and persisted yield context

The private startup receipt
`.local/agri-original-followup/yield/verified-startup.json` records55 assertions
over12 byte-checked windows,319 instruction lines and1,123 instruction bytes.
The independent provenance receipt `verified-provenance.json` records55
assertions over15 windows,1,302 lines and4,363 bytes. Both preserve the pinned
EXE, BuildingModels and FarmConfig hashes, lengths, modes and modification times.
Their verifiers are `verify_startup.py` and `verify_provenance.py` in the same
ignored directory.

### Actual startup precision and configuration

**EXE_OBSERVED** the PE entry at`0x7688ba` reaches initializer driver`0x769f80`.
Its fixed FPU initialization pointer at`0x88a914` selects`0x7650aa`, which invokes
`0x76d3a7`. That path supplies control flags`0x10000` under mask`0x30000` through
`0x773f3a→0x773f05`. The checked flag conversion selects raw x87 precision bits
`0x0200`, establishing an initial53-bit precision setting. The selected mask
preserves the preceding rounding-control bits. Their initial value remains a
separate input to the subsequent arithmetic and binary32 stores.

The actual constructor-array range`0x814000..0x815788` includes slot`0x8143ec`.
It reaches`0x4bf100→0x4bf110→0x4bfc00→0x4bfc20`, qualifying the Crop parameter
initializer as an executed startup dependency. Its modifier10, global float
bits`0x3f933333` and crop/region default stores have startup consumers. Complete
later indirect writes, configuration loading and restore ownership remain open.

The ordinary WinMain loop reaches the actor pass synchronously on that entry
thread through`0x475a4b→0x475b00→0x536fe0→0x536b20→0x5371a0→0x4e27e0`.
This qualifies the ordinary call relationship. The loop also contains indirect
and external calls. The finite instruction census includes control-word
changes, temporary CRT restores and x87 environment saves. A linear candidate
list alone leaves reachable writers, exception paths and restoration open.

**REFERENCE_DERIVED** Microsoft's x86 calling convention describes control-word
preservation as an ABI obligation. The verified Original import is
`DirectDrawCreate`, with a thunk at`0x5fb4c0`. Microsoft's documented Direct3D
FPU changes belong to a separate API contract. The startup proof and these
references leave the actual PC/RC at an arbitrary harvest dependent on its
admitted path and environment. [Microsoft x86 floating-point calling convention](https://learn.microsoft.com/en-us/cpp/cpp/floating-point-coprocessor-and-calling-conventions?view=msvc-170),
[DirectDrawCreate](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-directdrawcreate),
[Direct3D floating-point issues](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/top-issues-for-windows-titles).

A concrete next proof is local control-word stability from first arithmetic
`0x4c1e8d` to history store`0x4c1f14`, with an explicitly supplied starting
PC/RC. The callback calls`0x4c15b0`, conditionally`0x4c0750`, then`0x4c1060`
and the two restoring conversions. The transitive queries to qualify are
`0x4c15b0→0x525610→{0x425260,0x425250,0x47f1b0,0x408170}`,
`0x47f1b0→0x4f8210`, `0x408170→0x4081d0`,
`0x4c0750→0x4c0640` and Food classification
`0x4c1060→0x4c11b0→0x4c0600`, including their switch data. The AGRI-02D receipt
below now qualifies this bounded transitive graph and
supports resolved-context arithmetic. Admission of the actual live control word
and production authority remain separate.

### Source region operand from the supported map header

**EXE_OBSERVED** startup slot`0x81477c` reaches the header object constructor
`0x52e440` at`0x1158e18`. It installs vtable`0x7b6be8`, whose serialization slot
selects`0x52e690`. Header schemas1/2 contain the object's DWORD`+8`. The existing
standalone schema5/header2 profile places it at decompressed logical12. On the
checked load path,`0x52eb52` copies its low WORD to the signed crop/region
selector at`0xc5cd58`. The source factor index later uses that signed value.

The bounded reader and raw/signed/index distinctions are documented in the
[map contract](map-format.md). Numeric0–2 can index the checked factor table.
Textual climate identity, every other header/save profile and complete later
selector writes remain separate proof dependencies.

### Farm bin capacity is a serialized native operand

**EXE_OBSERVED** the Farmhouse vtable's slot`+8` selects`0x4c4e40`. After its
parent archive call, the store branch and version1 load branch each traverse
five bins starting at body`+0x158`, stride16, invoking each bin's own slot`+8`.
The bin vtable`0x7af1d8` binds that slot to`0x4c43e0→0x5d26c0`.

The bin archive writes resource`+4`, quantity`+8` and capacity`+0xc` as three
separate4-byte operands through`0x780642`. Its signed-version1 load branch
reads those same three DWORDs through`0x780533`. Archive direction getter
`0x40cf90` returns the complement of the archive flag's low bit. The complete
native stream framing and the other archive versions remain separate.

This closes an actual capacity restore writer. A fresh constructor sets
resource/quantity0 and capacity400. A restored capacity is an explicit native
input, and the checked bin load retains its archived value. The existing
`RawFarmBin` safe API consequently keeps capacity supplied by the caller.
Native restored-value validation, later runtime writes and complete overflow
disposition still require their own qualified context. World JSON identity,
inventories and migration retain their current contracts.

The private `verify_bins.py` checker emits
`bins/verified-native-bin-archive.json` with42 assertions over4 complete
byte-checked windows. It preserves the same three Original input identities.
It reads executable bytes and constructs no Original save fixture or importer.

## AGRI-02D implementation boundary

**PARTIAL at World activation.** The following sections qualify additional
source events for five Food fields and Actor26. They close resolved candidate,
spawn-commit, ordinary worker, field and numerical harvest operations, with
explicit native counter/WORD/receipt distinctions. They do not create a parallel
navigator, actor registry, calendar, inventory or save authority. Source callback
composition is not evidence of an admitted physical World route.

Historical Rule26/schema42 remains exact. This research step allocated no saved
generation; Rule27/schema43 now belong to the separate Playable Agriculture
mode. No automatic migration is introduced. Mill/FoodShop
and household portions are downstream of this task's Farm raw-stock boundary.
The absence of their conversion is not used to claim that a complete admitted
raw Farm cycle has already been achieved.


### Field driver ordering and quantity balance

**EXE_OBSERVED:** the fresh private verifier
`.local/agri-02d/field/verify.py` passes32 semantic assertions across five
byte-checked complete function windows,215 decoded lines and642 instruction
bytes. The pinned EXE, BuildingModels and FarmConfig retain their SHA-256,
length, mode and modification time. No Original execution or personal-save
inspection occurs.

Actual Food-field driver `0x4c0f60..0x4c1033` first qualifies the field type,
then skips terrain bit100. After actual own-info resolution, a nonzero countdown
byte63 decrements and returns at `0x4c0fb9`. It does not invoke the increment,
per-type veto or visual publisher on that branch. With countdown0, callback25c
supplies the increment; a nonzero result adds to quantity WORD04 and caps signed
results above10000. The supported physical domain0..10000 and Food deltas0/79/157
cannot wrap. The per-type query `0x4afd80` follows this addition even when the
increment was0. A true query clears quantity. Publisher260 runs after that
query even when quantity remains0.

`advance_crop_field_growth` composes the existing `crop_growth_increment` and
`apply_crop_growth` operations in this order. It reports actual quantity added
and quantity removed by the resolved type veto. Thus initial field quantity
plus added quantity equals final field quantity plus veto-removed quantity.
These quantities belong to the field plane, not raw harvested units, actor
cargo, Farm stocks or household portions. Terrain skip and countdown return
leave this balance unchanged. The function owns no clock, field registration,
actor or World permission.

Manager `0x51e4a0` performs schedule marking before field traversal. Its actual
object query, retained resource byte and field-classification result precede
the driver call at `0x51e50a`. The ordinary common parent `0x5371a0` invokes the
scheduler at `0x5371bf`, then the actor pass at `0x5371ce`; global88ec38 can skip
that scheduler while the actor pass remains. A tending completion in the later
actor pass cannot retroactively grow a field already visited by that scheduler.
The actual tending callback `0x4c1d20` ORs work40, clears field assignment2e and
clears body counter150. Seasonal expiration and actor cleanup remain distinct
events. Neither a harvest mark nor a visual publication creates harvested stock.

The fresh driver tests cover every countdown byte, terrain/veto precedence,
zero-work publication, growth bands, end slots, saturation, explicit tending
then next-growth ordering and unharvested end-slot expiration. Existing complete
work-word/slot tests remain in the same executable.

### AGRI-02D initial candidate selection and successful-spawn arithmetic

**EXE_OBSERVED:** private verifier
`.local/agri-02d/spawn/verify_selection.py` issues `verified-selection.json`:
145 source/arithmetic assertions across23 qualified byte-checked windows,
1,686 instruction lines and5,020 instruction bytes including overlaps. The
EXE, BuildingModels and FarmConfig retain hashes, sizes, modes and modification
times. No Original executable or personal save was run/read. Exploratory
captures, including misaligned data regions, are excluded from this receipt.

The complete initial selectors are `0x4c0130..0x4c02f6` for harvest and
`0x4c0380..0x4c055e` for tending. Both require actual active status1, the typed
source/field-family match, nonzero prepared distance, actual source-range
callback and matching retained assignment. Farm193's checked classifier entry
at`0x4c0885` selects `0x4c0841→0x4c0600`, accepting all five Food195–199 models.

Harvest additionally checks the field resource's global veto and operation
callback before its harvest mark. After the nonzero-distance gate within the
supported nonnegative domain, zero field quantity
clears only bit8000 at`0x4c022f`, before the range and assignment tests. Thus
an ineligible zero-quantity field can still lose its harvest mark on this path.
Positive quantity's score is prepared distance plus
`floor((100−floor(quantity/100))/2)` in the supported0–10000 domain. The
percentage helper is actual signed integer arithmetic, not a floating estimate.

Initial tending checks growing-period success and absence of bit8000. It has
no resource-veto or operation callback in this initial-selector body; allocation
rechecks those facts later. Food-field score is prepared distance plus half the
low work byte plus100 minus the explicitly resolved query`0x4c1af0`. That query
reads runtime fertility bytes through `0x4c1b30`, or a separately resolved global
override, then caps at100. It is not a countdown percentage or the serialized
construction-fertility byte. Material plants have a separate score branch and
are outside this Food-only operation. Current distance-plane preparation,
source-range admission, live global/configuration writers and callback eligibility
remain explicit source inputs.

The queue is a fresh native score queue. `0x4c5740` clears its retained insertion
cache and node count. `0x4c5780` reuses the last inserted node, chooses direction
with signed<= and walks with strict< or>. Iterator`0x4c5940/0x4c5950` starts at
the smallest head and follows larger neighbors. Equal-score order therefore
depends on earlier insertions: scores10,10,0,10 yield input-index order2,3,0,1,
while10,10,20,10 yield0,1,3,2. A stable sort would change the first result.
Node allocation`0x4c5980` checks count2000 and returns0 at that exact bound.
The pure operation refuses larger input queues rather than claiming the native
caller's unchecked allocation-failure behavior. This native research bound does
not enlarge World's82-body/58-channel budgets.

The complete allocation loops consume at most one selected entry per iteration
and contain one actor-creation call site each. Harvest's stable-frame selected
count/index are retained at`0x4c3736/0x4c3785`; its shared tail advances the
index and entry pointer at`0x4c394b/0x4c394c` before the sole backedge
`0x4c3959`. Tending has the analogous stores`0x4c3a16/0x4c3a63`, advances
`0x4c3c28/0x4c3c29` and backedge`0x4c3c36`. Actual successful-spawn ordinal
starts at0 and increments only on success at`0x4c38f9` or`0x4c3bc3`. Shared
assignment, candidate rejection and failed creation converge on that same entry
advance. A larger quota, zero harvest grouping or negative tending attachment
count cannot multiply creation attempts within an entry. Thus local successful
spawn count cannot exceed the retained selected-entry count.

The API's ordinal0–1999 is conditional support for successfully populated
selections with at most2000 entries. It is not a universal native ordinal limit.
Initial harvest increments its accepted-candidate count immediately after the
queue call at`0x4c0286→0x4c028b`; tending loads and increments its accepted
count at`0x4c04cf/0x4c04d7` after insertion`0x4c04ca`. Neither tests the
returned node first. Capacity-failure, corrupted-count or wrapped histories can
therefore separate selected-count accounting from actual inserted nodes and
remain unsupported. No broader successful-allocation guarantee is inferred.

Actual allocation calls shared actor factory`0x4ea050→0x4e1420`. Its slot search
pops at mostfive candidate IDs and requires a positive ID and an actual inactive
slot. Raw type26 takes the generic actor constructor. A nonzero returned ID
alone enters the Farm's commit branch: harvest writes state10/role2, tending
state6/role1; both set wait10 times the prior successful-spawn ordinal, retain
the actual source ID and increment only their own retained source counter.
Failed creation skips these assignment/counter writes. Shared fields can retain
the same returned actor ID. Harvest allocation separately checks the Farm's
signed WORD`+0x40<400`; this operand is not proved as Grower cargo or bin capacity.
Full native allocation/distribution, runtime slot lifetime and actual World
admission are not implemented by the post-success arithmetic query.

**VERIFIED REPRODUCTION:** `CropSelectionRules` supplies a Food-only candidate
evaluation, exact fresh-queue index ordering and resolved successful-spawn
initialization/counter arithmetic. Quantity, runtime-query and score domains are
explicit; reached signed-DWORD score/counter overflow is refused. These queries
create no ID, route, claim, work, cargo or stock and grant no World/save authority.
Tests include every physical quantity, every Food work word, independent gate
truth tables, zero-quantity side-effect ordering,13 literal queue vectors,
all2,187 seven-item ternary score sequences for ordering/permutation invariants,
full2000-node bounds and all supported spawn ordinals.

Neither complete selector nor initial Farm allocation contains a per-resource
partition: fields match Farm193's complete Food family and test their own global
resource veto. Repeated selector`0x4c0060` likewise checks the actual actor
assignment without comparing crop resources. The Grower can accumulate pending
raw yield and the Farm return labels it from the last retained field. This is a
missing lifecycle invariant, not proof that ordinary active play mixes crops:
distinct end slots and active-field expiration may constrain eligible tours to
one crop. AGRI-02E must close that temporal homogeneity and exceptional inactive/
reactivation/expiration history, or keep mixed-crop returns unadmitted. No silent
commodity conversion or invented ownership rule follows from this research.

## AGRI-02D: resolved Food Grower callbacks and manager cleanup

**EXE_OBSERVED:** the private verifier
`.local/agri-02d/actor/verify_actor.py` emits
`verified-food-grower-lifecycle.json`:139 assertions over20 qualified,
byte-checked windows,2,169 decoded instruction lines and7,477 instruction
bytes including overlaps. It qualifies complete callback/helper boundaries
and the actual state, behavior and cleanup selector data separately. The
pinned EXE, BuildingModels and FarmConfig retain their SHA-256, length, mode
and modification time. No Original execution or personal-save inspection occurs.

### Actual dispatch and callback ordering

The shared actor constructor`0x4c71d0` installs vtable`0x7afe60` and invokes
reset`0x4c72b0`. Its behavior slot`+0x28→0x4c7580` indexes40-byte raw-type
descriptors. Actual Actor26's entry`0x84eb94` selects`0x4cc670`.
That callback resolves the retained source WORD`+0x62`, tests exact source
status byte`+4==1`, and otherwise sets actor status`+0x16=2` at`0x4cc711`.
It **continues into the body**: source inactivity is not an early body return.
The callback also clears raw flag`+0x6f` before dispatch. Its other rendering
prefix effects do not grant gameplay.

`begin_crop_grower_callback` exposes this early status/flag operation before
any resolved work or movement body. Movement and field observations must apply
to that post-begin actor, including status2 where set. A caller must not first
compute a status1 movement outcome and retroactively mark it retiring. The
separate epilogue follows the actual body and any actual target-result operation.

Raw state table`0x4cd1b8` has the following complete4–16 dispatch bindings:

| Raw state | Actual branch | Qualified behavior |
|---:|---|---|
|4|`0x4cc7bb→0x4e6470`|Separate signed-WORD wait operation below|
|5|`0x4cc7a7→0x4e6280`|Delegates a larger shared actor helper; not admitted as ordinary Food work|
|6|`0x4cc7ce`|Tending selection wait, then actual Farm selector|
|7|`0x4cc832`|Shared movement toward tending target|
|8|`0x4cc966→0x4c2dc0`|Tending wait and compatible-field callback|
|9|`0x4cc97a`|Source return without receipt; retained WORD may be nonpositive|
|10|`0x4cca1f`|Harvest selection wait, then actual Farm selector|
|11|`0x4cca82`|Shared movement toward harvest target|
|12|`0x4ccc10→0x4c2fd0`|Harvest wait and compatible-field callback|
|13|`0x4ccc24`|Raw-yield source return and actual source receipt|
|14|`0x4cccca`|Exceptional movement/coordinate-query branch|
|15|`0x4ccdf2`|Exceptional signed-WORD increment/100-bound branch|
|16|`0x4cce30`|Exceptional linked-actor checks and shared operation|

`CropGrowerRules` implements ordinary6–13 resolved events and the independent
state4 arithmetic. State5 and14–16 remain explicitly excluded from its
ordinary body API. The callback's default branch retires other raw states;
accepting every arbitrary state would not reconstruct their external entry or
shared behavior.

### Actual wait, selection and movement events

Selection states6/10 set raw flag`+0x6f=1`, decrement the same signed wait
WORD`+0x3e`, and remain while it is positive. When due, they reset that WORD to0
before calling `0x4c2c70`/`0x4c2b20`. Work states8/12 instead retain the
**decremented** WORD, invoke field slot`+0x264`/`+0x268` only when its actual
compatibility query succeeds, and move to6/10 even if compatibility fails.
Their completion helpers do not compare the field assignment with the current
actor, require nonzero assignment, inspect quantity or require a harvest mark.
Those distinct field effects and yield gates stay in `CropRules` and
`CropHarvestRules`. Seasonal expiration can therefore clear the assignment and
set the actor wait to0; a later compatible work invocation decrements to−1 and
still calls the field, whose cleared harvest mark then prevents raw yield.

`apply_crop_grower_target` consumes an actual resolved selector result. It does
not rank fields or prepare a route. Every attempt calls shared route release
`0x4e8a30`. Tending additionally rechecks the selected field's growing-period
query; successful results set actual target coordinates, retain the selected
field WORD`+0x68` and select movement7/11. Failure targets the source's retained
return coordinates and **keeps the last field reference**. Failed tending
selects9 and decrements source counter`+0x154`. Failed harvest compares the
signed WORD`+0x7c`: positive selects13, while zero or negative selects9, then
decrements counter`+0x150`. Both counters floor negative results at0.

The movement operations consume an already resolved shared-movement result;
they do not create a physical route or infer arrival from proximity:

| Current states | Result9 | Result10 | Result8 |
|---|---|---|---|
|7/11, target travel|Release route; retain state|Set raw flag6f and retiring status2; no work|Enter8/12; initialize the existing Food wait51/25, subject to actual compatibility, nonzero assignment and terrain100 override|
|9, no-receipt return|Release route; retain state|Retire without source receipt|Set raw flag6f and retire without source receipt|
|13, carried raw-yield return|Release route; retain state|Retire **without source receipt**|Call the actual source slot`+0x268` at`0x4cccbf`, then retire|

All other movement results retain the state. Both work-wait initializers remain
shared with `initial_crop_work_wait`; wait decrement/wrap remains shared with
`advance_crop_work_wait`. No authored World edge time is inferred from the
source waits or result bytes.

### Counter epilogue and actual reference cleanup

After any supported callback body, actual actor status2 selects the retirement
counter debit. Native role byte`+0x89==2` calls`0x4d6be0` for harvest; the other
ordinary role1 calls`0x4d6c00` for tending. Each floors at0. This is **in
addition to** the failed-selection debit above. A failed harvest selection can
therefore decrement5→4, and its eventual retirement decrements4→3. These
retained counters cannot be treated as an exact census of live Growers or
repaired into one without changing semantics. The safe API admits the proved
ordinary roles1/2 and nonnegative signed-DWORD counters; other roles and native
signed-overflow histories remain unsupported.

The ordinary actor pass tests status2 after behavior at`0x4e291b` and invokes
removal`0x4e292d→0x4ea080→0x4e1fa0`. Manager removal passes the actual actor ID
to its virtual slot`+0x158`, bound to`0x4c8b70`. Its exact raw-type26 selector
`0x4c9110+(26−1)` is2 and selects`0x4c8bfa`. That branch resolves the field
manager through`0x4bfdb0` and calls`0x4bfdc0`. The pass visits the complete
registered collection and clears **every** field with status1 and retained
actor WORD`+0x2e` equal to the removed actor ID. It does not clear inactive
fields, another actor's assignment, work words, field quantities or field-body
counters. The bounded `clear_crop_grower_assignments` operation preserves those
exact represented distinctions; it grants no claim or actor identity.

Shared route release, additional generic links and rendering hooks remain
outside that value operation. Following cleanup, actual manager slot`+0xe8`
selects`0x4c72b0`, which resets actor status, source/field references, wait,
role, target coordinates and the same raw-yield WORD`+0x7c`. The pure reset
reports the cleared word without labeling it automatically lost cargo or a
refund: successful source receipt leaves that word retained until reset too.
A failed return10 followed by reset, in contrast, has no checked source receipt.
The actual bin-accepted amount, ignored overflow and other accounting effects
require their separate source operations before a conserved World transfer can
be claimed.

### Separate state4 arithmetic and excluded exceptional states

The complete state4 helper`0x4e6470..0x4e64a4` first sets negative wait to0,
then increments the WORD. A signed result at least128 is replaced by127 and
sets status2. Its16-bit increment means32767 wraps to−32768 before the signed
comparison; it is not pre-clamped. `advance_crop_grower_state4_wait` implements
this operation alone, preserving raw flag6f and every other represented field
apart from wait/status. The separate begin operation owns flag clearing. It does
not identify why another subsystem entered4.

The qualified exceptional branches remain specific rather than being collapsed
into ordinary Food return. State14 runs shared movement. Result8 clears wait,
calls the separate coordinate query`0x5b2730`, retains its result and enters15;
result10 or9 enters16. State15 increments its WORD and enters16 only after the
signed value exceeds100. State16 resolves linked actor WORD`+0x72`; a nonactive
linked actor retires the Grower. An active actor of raw type55 must additionally
have raw state38/39/40; other states retire. An eligible linked record reaches
`0x4e7460`. The complete entry writers, missing-link/identity history and shared
callee semantics remain unclosed. No World interruption/combat meaning is
assigned to these states by name.

### Explicit remaining activation dependencies

The existing scheduler precedes the actor pass, but complete original user
pause/suspension semantics remain external: actor field`+0x13c` skips behavior,
its complete writers are not closed, and global`0x88ec38` can skip scheduler
advancement while actor behavior still runs. Actor26 has no independently proved
object-local fire or current staffing gate equivalent to existing authored
World adapters. Source-status, per-type veto, operation, spawn/target admission
and native movement outcomes must retain their actual meanings and histories.
No source inactive-status check may be replaced by an invented early rollback.

Actor`+0x7c` is one carried raw word, not an independently accumulating pending
stock plus another physical cargo ledger. Farm return derives its resource
from the last retained field identity. Initial/repeated selection has no newly
inferred resource partition; the complete temporal homogeneity of assignments
across distinct Food end slots, inactive/reactivated fields, expiration and
restored claims must be proved separately. No multi-crop word may silently be
retyped by a test or World adapter.

The new APIs remain source-event arithmetic with caller-resolved observations.
They own no Navigator, advancing actor registry, saved state, raw stock or World
production. Original movement/result binding, all active actor gates, live yield
context, simultaneous-assignment admission, source overflow and conservative
raw-resource ownership still gate a new explicit version. Rule26/schema42 and
all older gameplay retain their current authority. Supplied movement8 events
in component tests establish arithmetic composition, not actual World arrival
or a completed paid agriculture chain.

## AGRI-02D resolved Food harvest arithmetic and callback effects

The private checker `.local/agri-02d/yield/verify.py` emits
`verified-harvest-context.json`:189 assertions across16 complete byte-checked
windows,477 decoded instruction lines and1,375 instruction bytes. Jump tables
are qualified separately as raw data, never decoded as instructions. The pinned
EXE, BuildingModels and FarmConfig retain SHA-256, length, mode and modification
time. The Original executable is not run and personal saves are not read.

### Local control-word stability is now qualified

**EXE_OBSERVED:** the nonfaulting arithmetic interval begins with first integer
load`0x4c1e8d` and ends at result publication`0x4c1f14`. Its direct calls are
`0x4c15b0`, optionally`0x4c0750`, `0x4c1060` and the two conversions at
`0x4c1eee`/`0x4c1f0f`. The complete checked transitive query graph is:

- `0x4c15b0→0x525610`;
- `0x525610→0x425260,0x425250,0x47f1b0,0x408170`;
- `0x47f1b0→0x4f8210` and`0x408170→0x4081d0`;
- `0x4c0750→0x4c0640`;
- `0x4c1060→0x4c11b0→0x4c0600,0x4c0630,0x4c0640`.

Every query helper is integer-only and has no further external/indirect call.
The irrigation perimeter query's only indirect branch is its bounded four-way
switch at`0x4c15f6`, backed by four checked addresses at`0x4c16ac`. Crop indexing
uses its checked model bounds, byte selectors and ten-entry jump table. Each
Food195–199 selector reaches its actual index0–4. For those five inputs the
field classifier succeeds at the first Food predicate; the additional Hemp and
plant leaves are also covered by the finite graph.

Only conversion helper`0x7650fc` changes the control word in that graph. It
saves the supplied word, installs its temporary truncation RC, performs signed
64-bit`FISTP`, and restores the saved word before returning. Thus one resolved
PC/RC at the first arithmetic instruction governs this local calculation.
This closes the proposed bounded transitive proof from the preceding section.
It does **not** establish the actual live control word before that first load,
preexisting exception/stack state, fault paths, or all preceding calls. The
pre-arithmetic virtual/event predicates remain outside this local interval.

### Exact resolved arithmetic, with explicit support boundaries

**REFERENCE_DERIVED — instruction semantics:** Intel's
[SDM Volume1, sections8.1.5.2–3](https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf)
defines the PC24/53/64 encodings and applies precision control to arithmetic
including FADD/FMUL; RC supplies the rounding direction. Its
[Volume2A FST/FSTP reference](https://www.intel.la/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf)
separately specifies destination-width rounding for binary32 stores under RC.
These architectural references support the decoded instruction semantics;
they do not establish the game's live control word or replace its source proof.

**VERIFIED REPRODUCTION:** `CropHarvestRules` reuses `crop_rule`,
`crop_doubled_base` and `raw_base_yield`. Its `CropHarvestContext` requires the
actual runtime fertility byte, resolved irrigation result, selected raw binary32
region factor, raw date operand, conditional global factor, control word and
crop-event inputs. It does not infer them from construction, names, a calendar
slot, startup defaults or FarmConfig text.

The independent integer implementation represents exact values as a uint64
significand and power-of-two exponent. It rounds at each source arithmetic
instruction according to explicit PC24/53/64 and RC nearest-even/down/up/toward
zero. It performs no host floating-point operations and does not modify the host
FPU. The supported chain has at most56 product-significand bits: the integer
base is at most200, raw binary32 factors have at most24 bits, and the later
reload is explicitly signed-int32-safe. Both fertility stores are normal
binary32 values in their actual byte/irrigation domain.

The implemented order is:

1. Derive the typed crop-qualified integer base from quantity0–10000 and the
   explicit event operands; Rice/Cabbage cannot acquire the doubled branch.
2. Multiply runtime fertility by source binary32`0x3c23d70a` (0.01), round at
   supplied PC/RC, then store binary32 with that RC.
3. When the actual irrigation query succeeds, add Food binary32`0x3eb33333`
   (0.35), round and store binary32 again. The other plants'0.2 is outside this
   Food-only component.
4. Multiply the integer base by the selected region binary32 factor, round,
   multiply by the stored fertility binary32 factor, then round and truncate
   toward zero through the qualified conversion.
5. Only when signed raw date`0xd623fc > -300`, reload the first integer result,
   multiply by the selected global binary32 factor, round and truncate again.
   The global factor is not decoded or restricted when this branch is absent.

Factors actually read must be finite and nonnegative; both signs of zero are
supported, as are positive binary32 subnormals. All six FP exception masks must
be present in the explicit control word, and reserved PC01 is refused. Reached
conversions must yield0–INT32_MAX. The native conversion itself is signed64 and
the caller consumes its low DWORD/WORD; this API refuses histories outside its
nonwrapping signed32 support domain rather than claiming native clamping.

`CropHarvestYield` keeps the integer base, stored fertility bits, first integer
result and final numeric yield separate. Its `retained_yield_word` exposes the
source's low16-bit history/pending operand. For example, numeric65536 retains
WORD0. That narrowing is not a safe cargo/storage capacity or a Food portion
conversion.

The independent exact-rational controls retain the prior64/65 counterexample:
quantity10000, fertility65, region1 and no irrigation/global branch give65 at
PC24 nearest-even,64 at PC53/64 nearest-even. A second control includes the
actual irrigation store and global reload: fertility65 with irrigation stores1
at nearest-even, then global factor`0x3f933333` gives115 at PC24 and114 at
PC53/64. This distinguishes both binary32 stores and both integer truncations.
Tests include32 literal vectors, all PC/RC combinations, every quantity for the
literal fertility100/unit-region case, and an independent8,192-case rational
reference checksum over authored inputs. They make no claim of admitted live
World parameters.

### Actual callback field/history and one actor WORD

**EXE_OBSERVED:** `0x4c1df3` ORs work`0x4000` and clears body DWORD`+0x150`
before the mark test. An unmarked callback returns without changing field
quantity, assignment, history or the actual actor's yield word. A marked
callback consumes`0x8000` at`0x4c1e14`, computes the resolved amount, writes AX
to current history WORD at`0x4c1f1a`, adds AX to the same actual actor WORD
`+0x7c` at`0x4c1f1f`, then clears field quantity and body assignment at
`0x4c1f25`/`0x4c1f29`. No assignment-equality check exists in this callback.
Its actual caller eligibility and resolved actor identity remain separate.

**VERIFIED REPRODUCTION:** `apply_crop_harvest_callback` takes the current
12-slot history, bounded quantity, full work word, body counter and raw
assignment WORD plus the actual actor's one retained yield WORD. It returns
those effects as values, with an optional numeric calculation only when a mark
was present. It reports numeric-to-WORD narrowing and separate actor-add carry.
The latter is modeled modulo65536 exactly; it creates no second pending/cargo
inventory, Farm stock or safe capacity. Unsupported arithmetic publishes no
partial result and leaves the supplied inputs unchanged.

An expired field with cleared mark can still reach a later callback through
its retained worker path. That unmarked callback sets the work flag and clears
the body counter, but cannot recreate the expired quantity, history receipt or
actor amount. Tests enumerate all65,536 work words for every Food field, check
history-slot preservation, repeat-callback refusal of a second yield, expiration
composition, numeric65536→WORD0, and actor65535+1→WORD0 as distinct outcomes.

### Remaining AGRI-02E admission dependencies

This closes resolved local arithmetic and the represented callback mutations.
Before World activation, qualify the actual control word and normal FP stack/
exception state at`0x4c1e8d`; bind every accepted runtime fertility, region,
event, date and global operand to its source/load/update owners; and complete
actual worker movement, interruption, return and Farm-bin conservation. Native
history/pending WORD narrowing and full-bin loss must not silently become
successful World stock or an invented refund. A new explicit rule/schema is
still required for any additional mutable gameplay, with no migration of
Rule26/schema42. No raw-yield-to-Food-portion conversion is supplied here.

The separately qualified full tending function`0x4c1d20..0x4c1d54` in the
root's field-event receipt supports `apply_crop_tending_callback` over the
same resolved field value. It delegates the existing `complete_crop_tending`
work rule, ORs low work bit`0x40`, clears assignment/body counter, and preserves
quantity and every history WORD. It adds no mark, yield or actor inventory.
Independent tests enumerate all65,536 work words for each Food field, repeated
callbacks, quantity0/10000 boundaries and unsupported-model/quantity refusal.

### Farm-return bookkeeping is not accepted-stock authority

The separately qualified full Farm return `0x4c4ee0..0x4c5088` derives its
resource from the actor's last retained field, sign-extends that same actor
WORD7c and offers it to a selected compatible bin. A missing bin skips both
credit and the common bookkeeping footer. Bin overflow is ignored by the
native caller. A stock of at least400 can trigger actual source-access checks
and a separate Actor19 allocation; only successful Cart creation reaches the
400 debit. This later Cart uses WORD82, not the Grower's WORD7c.

There is a further identity caveat in the common footer. ESI initially contains
the Grower ID, but `0x4c4f9e` replaces it with the Cart factory result, including
zero on failure. Footer `0x4c5069` resolves that current ESI and reads WORD7c at
`0x4c5077` for the per-resource DWORD addition at `0x4c507b`. It cannot generally
be labeled a count of the Grower's offered or bin-accepted amount. The semantic
owner of that bookkeeping and the null-slot context remain separate. Neither
this counter nor the still-retained Grower word becomes a second physical stock.

Private `field/verify_return.py` passes19 source assertions over one complete
byte-checked window,134 lines and425 bytes, with the same three input guards
unchanged. It introduces no World counter, Cart, refund or inferred loss policy.


## AGRI-02E physical World integration audit

**PARTIAL.** The source investigations below close additional concrete paths,
not the paid World Grower chain. All existing `CropRules`, `CropSelectionRules`,
`CropGrowerRules`, `CropHarvestRules`, `CropLifecycleRules` and
`CropTransportRules` were checked against these boundaries. No source operation
is reimplemented merely to claim integration. Rules1–26/schemas1–42 stay exact;
This research step allocated no saved generation, create/restore/menu/CLI
selection, raw inventory or Food origin. Rule27/schema43 now belong to the
separate Playable Agriculture mode.

### AGRI-02E source-distance preparation and dispatch inputs

**EXE_OBSERVED.** `spawn/verify_spawn.py` re-extracts12 complete bounded
windows, validates51 address/vtable/dataflow checks and checks426 decoded
instruction lines /1,133 instruction bytes. The pinned EXE, BuildingModels and
FarmConfig retain SHA-256, size, mode and modification time. No live World or
Original executable is run by this source verifier.

A due `0x4c3550` dispatch clears its old-byte throttle, reads actual Farm
return coordinates WORD2a/2c and calls `0x5aea60` at`0x4c366a` with literal
target coordinates−2/−2. The return value is not tested before the source's
harvest slot270 and then tending slot274. Both initial selectors consume the
same prepared DWORD plane `0x1391fe0`; neither distance is a new World BFS or a
nearest-road approximation.

The complete flood `0x5aea60..0x5aeb51` calls `0x521140` to clear51,984 DWORDs,
seeds its source distance to1, and uses a bounded ring queue with indices0..51983.
`0x5aeb60` expands neighbors in order−228,+1,+228,−1. `0x5aebb0` skips already
visited cells and requires a nonzero intersection of the prepared terrain WORD
at`0x13789c0` with mask13d. Terrain DWORD bit10000000 additionally requires
`0x4c11b0`'s field-family predicate on the resolved body. Accepted edges get
predecessor-distance+1 and enter the queue. The shared helper `0x4fa3d0` is
literally constanttrue; its call is **not** an independent coordinate/bounds
proof. The flood can stop at its target or at queue exhaustion. Source seed1
must not be silently replaced by a zero-distance World origin.

Farm193's actual operation slot78 binds `0x4289f0`, which reads body byte90;
the generic toggle `0x428a00` writes it and invokes shared invalidation. Its
staff-percentage slot1b4 binds `0x428ed0`: nonpositive required staff returns0;
otherwise the native integer query computes100×actual signed-WORD44 divided by
required staff. Slot1b0 (`0x428eb0`) returns0 while raw byte6e is set, otherwise
queries model property5. These are actual dispatch inputs, not proof that every
fire/pause/staff change cancels an already spawned Grower. The previously
qualified source operation/type/resource gates and retained-counter allocation
remain distinct from the actor callback's early source-status check.

**UNKNOWN / UNSUPPORTED:** complete prepared-plane creation/update/restore
provenance, all boundary and occupancy predicates, mutable quota/configuration
owners, raw Farm WORD40's harvest-allocation gate, and their mapping into shared
World limits/edge legality. The source range query's actual vtable binding is
qualified, but no new immutable ownership rule is inferred. This closes the
local distance preparation and staffing/operation operand path, not bounded
World entity allocation, physical work or raw stock authority.

### AGRI-02E shared physical routing and actor gates

**EXE_OBSERVED, not World activation.** Private
`.local/agri-02e/routing/verify.py` re-extracts25 complete code windows with
switch/descriptor data qualified separately, verifies their instruction bytes
against the pinned PE, and emits `verified-routing.json`:160 assertions,
4,752 instruction lines and15,486 instruction bytes including overlaps. The
EXE, BuildingModels, FarmConfig and FigureModels retain SHA-256, length, mode
and modification time; no Original executable or personal save is opened for
execution. This qualifies these functions and direct bindings, not every
transitive global/model writer or a paid World arrival.

Actor26 overwrites native traversal mode byte`+0x80=17` on **every callback**
at`0x4cc6a1`. Its states7/9/11/13/14 all call shared`0x4e47a0`; their movement
result is not an arbitrary caller-assigned arrival flag in the Original.
The actual vtable property query at`0x4eb9da` binds to`0x4c9310`, which indexes
the mutable actor property table by raw type and requested property8. The
source FigureModels row26 has speed-column8, but its load/update provenance
is not thereby admitted permanently. For a resolved returned speed value of8, the shared cadence
switch retains byte`+0x170` and supplies1,1,2 microsteps cyclically when starting
from0. That is not a five-tick World edge rule.

`0x4e7eb0` prepares a route when WORD`+0x42<=0`, queries its direction, then
validates the next edge. It increments native byte`+0x41` per microstep,
commits the real cell through`0x4e9100` at phase10 and advances route cursor
WORD`+0x44` at phase20. The cell commit updates current coordinates, retains
old coordinates`+0x20/+0x22`, updates the spatial chain and invokes shared
interaction`0x4e57a0`. This transitive interaction can enter raw state5; legal
movement therefore cannot replace state5 with an invented no-op.

The complete preparation switch`0x4e89cc` maps traversal17 to`0x4e88db`.
That branch's helper`0x4fa3d0` is constanttrue, then the target's prepared
terrain WORD must intersect`0x0b3c`. It calls the separately qualified cardinal
`0x5aea60` flood. On success the initial BL=1 takes the ordinary extraction
branch with argument8 into`0x5b18b0`. The extractor consumes that flood plane,
walks descending distances backward from the target, excludes the previous
reverse direction, and prefers the direct coordinate direction on an equal
minimum; otherwise it preserves the first equal candidate. Argument8 scans
all eight directions; argument4 scans0/2/4/6. It reverses scratch directions
into the actual route slot and refuses a count at least500. Successful paths
therefore have fewer than500 stored direction bytes. This distinct traversal
and tie policy is not proof that a nearest current World road route is the
Original route.

Actual route-slot allocation`0x4e25a0` reclaims a matching prior slot or scans
slots1..999. It repairs specific stale ownership/actor-status states and fails
with outputslot0 when exhausted. This route registry bound is separate from
native actor bounds and OpenEmperor's existing body/courier budgets. Successful
preparation records actual actor ownership, route slot, length and phase20.
Shared route release`0x4e8a30` clears the registry owner only when it matches
the current actor, then clears that actor's retained slot word.

`0x4e8b40` establishes the exact terminal outcomes:

- With a positive route and cursor at least its retained length, release the
  route and write8. This branch does not independently compare the final cell
  with the target; its arrival meaning depends on valid route/endpoint history.
- Without a positive route, call the complete coordinate-direction helper
  `0x5b2730`. It returns8 only for equal current/target coordinates. Unequal
  coordinates are overwritten to10 by`0x4e8b7f`.
- A live route reads the direction byte at its actual slot/cursor. Separate
  `0x4e8bc0` next-edge checks can replace a0..7 direction with retry9. They
  inspect prepared runtime terrain, occupancy, links and per-type context;
  these planes are not granted by visual/raw terrain similarity.

The existing World plantation path uses cardinal `plantation_edge_allowed`,
paid source/plant ownership and shared source-aware routes; five-tick movement,
source staffing/operation/fire gates and output headroom are authored there.
It is a reusable engine, but no exact mode17 predicate/phase/outcome mapping
has yet been proved. No second navigator is warranted. A future World binding
must prepare source-bound legal edges in the shared navigator, preserve begun
edges and prove route/cell/cursor/endpoint invariants before invoking work.

#### Suspension and carrier lifetime

The actor pass`0x4e27e0` reads DWORD`+0x13c` at`0x4e2899`. A nonzero value
branches past both behavior **and** the ordinary post-behavior status2 removal.
A specifically proved writer is transport, not a user-pause assumption:
`0x4e8bc0`'s gated special-link branch allocates raw Actor48 at`0x4e8e02`.
Only successful allocation writes that actual carrier ID into the passenger's
`+0x13c` at`0x4e8eae`, sets its flag6f and releases its route. It follows the
passenger's positive WORD`+0x66` chain and applies the same suspension to each
member at`0x4e8f06`. Failed allocation performs no such write.

The exact raw Actor48 descriptor`0x84ef04→0x4cf5f0` selects the carrier body.
Invalid source handling marks the passenger and linked members status2 and
clears13c (`0x4cf730/0x4cf752`). Its disembark path restores passenger physical
coordinates/spatial-chain state, clears13c (`0x4cf8c1/0x4cf945`), and retains
native edge phase20 for the first passenger or10/retry9 for linked members.
The source FigureModels48 label is `Ferry Sampan`; that label supplements this
observed actor-link behavior, not a complete ferry geometry/admission proof.
All remaining carrier/source/linked-chain identity writers and failure cleanup
must close before this path can become a World interruption rule.

Global`0x88ec38` has multiple recorded writers. The qualified menu dispatcher
`0x42e430` writes0 in one branch and1 in another; these values are not an
unconditional runtime constant. Parent`0x5371a0` skips source scheduler and
later dispatch when the global is nonzero, but still invokes the actor pass
at`0x5371ce`. Its complete semantic ownership remains unadmitted: treating this
as the normal World pause flag would change the observed ordering.

#### State5 and exceptional14–16

The complete interaction initializer`0x4e54e0` preserves the actor's prior raw
state in`+0x18`, enters5, binds actual peer words`+0x9e/+0x9a`, selects linked
mode`+0x99=1`, and can place the other actor in5 too. An already linked peer can
add a second word`+0x9c` and mode2. State5's`0x4e6280` follows the retained
mode, peer activity/types/states and shared predicates, updates phase/counter
bytes, and can call the distinct shared operation`0x4e5a60` after its separate
24-attempt counter. Exit`0x4e65e0` for Actor26 restores the prior state from byte`+0x18`,
clears mode99/peer words9e/9a/9c and releases the route. It does not itself grant
a Farm receipt or counter rollback. The full peer/model/RNG/damage/reentry
context remains unclosed; source staffing/fire may not be substituted for it.

The state14 arrival's`0x5b2730` is now qualified as a direction query from the
actual current cell to retained WORDs38/3a; state15 increments its signed wait
until greater than100. State16's actual linked helper`0x4e7460` compares saved
34/36 and linked current cells, runs its own20-phase movement toward the linked
actor's **previous**20/22 cell, and commits through the same`0x4e9100` at10.
Actor26 calls it with step argument1 at`0x4ccecb`. It does not return to ordinary
Farm work by an inferred animation completion. Complete state14 entry writers,
linked72 lifetime/missing-link history and every transitive shared effect still
prevent admitting14–16 as supported World states.

### AGRI-02E: repeated targets, deactivation and raw identity

**EXE_OBSERVED.** Complete repeated harvest selector `0x4c0060..0x4c0129`
walks the current registered field vector in collection order. An optional
nonzero preceding field ID acts as a cursor; ordinary caller `0x4c2b20` supplies
zero, so each ordinary attempt restarts at the beginning. It requires status1,
Farm-compatible field family, the candidate resource's global enable flag,
actual operation query, type veto and exact retained actor assignment. This
local selector performs no quantity, harvest-mark, calendar, distance or
carried-resource-homogeneity query. Initial harvest admission retains its
separate mark/quantity/distance gates; the repeated selector must not be
replaced by that queue or by a new nearest-field search. Successful selection
updates Actor WORD68 to this actual target. The harvest completion callback
clears that field's assignment before state10 reselects.

One ordinary lifecycle concern is now narrower. The actual Food vtable
`0x7aea40` binds `+ac→0x428e70`, `+b0→0x4c17d0`, `+b4→0x4c1840` and
`+c0→0x4c1770`. Concrete field cell-removal `0x4c17a0` calls `+ac` after its
map-cell callback. `0x428e70` marks body status0 and invokes subtype cleanup.
That cleanup delegates `0x51cb50→0x428e10→+b4`; Food `0x4c1840` removes the
actual field pointer from manager `0x1031460` through pointer erasure
`0x5f4f20`. Only after that unregister, the local Food cleanup resolves its
assigned actor, clears that actor's wait WORD3e, and clears its own assignment
WORD2e. Its local body does not clear Actor68 or Actor7c. Thus ordinary bound
deactivation removes the field from repeated scans; a blanket claim that all
ordinary deleted fields simply remain as inactive rows in that vector would be
incorrect. Exceptional direct status writers and restored collection admission
remain external.

This still does not prove the returning raw amount's identity. Farm return
`0x4c4ee0` reads retained Actor68, uses `0x47f1b0` to index the current building
vector, reads that slot's current model14 and maps it through `0x4c18c0`.
The complete getter has no status, generation or identity-history check. The
complete generic building factory `0x42d540..0x42d781` explicitly accepts an
eligible inactive chosen slot or scans eligible inactive slots (including
reservation/history veto `0x5ef4f0`), destroys its old concrete object,
replaces the vector pointer and assigns the same numeric body IDb4 to the new
object. The complete admission/order of deletion and rebuilding while a loaded
Grower retains68 must therefore be proved before native numeric references can
be mapped to World monotone identities. This is a missing temporal proof, not
proof of a reachable ordinary mixed-crop bug.

Initial active harvest marks may ordinarily constrain one tour to one crop,
because the five Food end slots differ. However, none of the checked repeated
selector, harvest callback or Farm-return body compares an accumulated amount's
resource with each later field. No pooled multi-crop amount can be silently
accepted as the last field's resource without the complete initial/temporal
invariant. Maintaining a second per-crop cargo copy would also change the one
native WORD's authority.

### AGRI-02E: explicit resource equations

For a supported nonnegative numeric yield `N`, let `H=N mod65536` and incoming
actor raw WORD `C`. History records `H`, and the same actor word becomes
`C'=(C+H) mod65536`. The independent narrowing and carry amounts are
`N-H` and `C+H-C'`; they are not additional Farm credits or retained cargo.
Signed-positive selection of C' is a separate gate: WORD32768..65535 selects
state9, which returns without a source receipt, just like zero. The actual
source return sign-extends its actor word; negative source offers are outside
the checked nonnegative Farm-bin API domain and must remain unsupported.

For an actual same-resource source receipt in the checked bin API domain
0≤B≤K≤32767 and0≤O≤32767 (no signed-DWORD wrapping), `A=min(O,K-B)` and `X=O-A`, so
`O=A+X` and `B'=B+A`. Original credit returns X, but Farm does not inspect it.
If actual access and stock>=400 permit a Cart attempt, failed factory creation
performs no bin debit and supplies no refund. Only successful creation performs
`B''=B'-400` and initializes the separate Cart WORD82 to400, preserving
`B''+400=B'` for that handoff. This Cart word is not a copy of Grower WORD7c.
A failed Grower return10 has no source receipt: its retained word is unreceived
terminal cargo. After a receipt, the offered word remains historical memory;
A was credited and X was unaccepted. Reset must neither credit that word nor
count all of it as another loss. Partial acceptance does not make X accepted
stock; account for X once, without an invented refund.

The Farm footer remains a bookkeeping caveat: ESI is replaced with the Cart
factory result, including0 on failure, before it resolves an actor and reads
WORD7c for resource-indexed DWORD addition. This counter cannot stand in for
O or A. The unchecked slot getter also means that the identity0 record's
initialization/load provenance must be established before claiming that branch's
bookkeeping value. No extra World resource counter is introduced for it.

Private `identity/verify_identity.py` independently recaptures complete bounded
bodies and validates 91 address/branch/vtable assertions over22 byte-checked
windows, with the three guarded input files unchanged in SHA-256, length, mode
and mtime. It executes no Original binary and reads no personal save. This is
source evidence, not a physical World test or proof that an adversarial combined
field history is reachable in an ordinary original city.

### AGRI-02E live operand provenance

**EXE_OBSERVED / PARTIAL.** The private checks
`.local/agri-02e/yield/verify_live.py` and `verify_date.py` qualify additional
entry, serializer, update and switch-table paths. Their independent receipts
`verified-live-provenance.json` and `verified-date-factor-provenance.json` record
180 and251 checks,15 and13 byte-checked windows,1,652 and5,459 decoded
instructions, and5,536 and20,070 instruction bytes respectively. Original EXE,
BuildingModels and FarmConfig hashes, sizes, modes and modification times remain
unchanged. No Original executable was executed and no personal save was inspected. Complete code
ranges are qualified independently from private exploratory snippets; raw switch
entries are not decoded as executable instructions.

Runtime fertility has an actual archive provenance distinct from construction
fertility. Standalone map archive `0x52e7c0` writes the51,984-byte
`0xeac3f0` runtime plane at `0x52e8df` and separately writes the
`0xe9f8e0` construction plane at `0x52e901`; corresponding reads are
`0x52ea84` and `0x52eaa6`. Its header-only argument can skip both planes.
The complete checked archive has no direct call to the fertility preparation
functions. This does not prove that its later callers leave runtime fertility
unchanged. The rectangle update `0x4c1c20` and all-map wrapper `0x4c1d00`
remain real mutating operations. Reset block `0x4b08d3` independently zeroes the
runtime plane. Native session archive `0x52fda0` likewise stores runtime
fertility at `0x53035a`, and its checked current-version19 read branch loads
it at `0x530b5a`. A future World may not always replace that input with the
construction byte plus startup modifier10; it needs a proved initialization and
update ordering for its admitted source profile.

The actual event getter `0x48df30` returns singleton `0xc701d8`.
Its constructor `0x48df40` installs vtable `0x7adb88`; slot+8 selects
`0x48ed30`. The archive's writer retains enable byte+50, state DWORD+54
and selector DWORD+58 separately. Load versions3/4 read those fields; versions1/2
omit them. Its version upgrade sets4. Runtime operation `0x48f820` copies an
actual argument into+58 and sets+54=3. `0x48fcb0`'s bounded four-way
dispatch reaches this operation through `0x48fd23→0x48df30` and
`0x48fd2a→0x48f820`. The conditional `0x48ea40` path sets singleton+50=1
and+54=0; `0x48eb90` clears both. These owners and writes qualify real operands,
not a permanently disabled event or a default event state. Native session
archive explicitly calls this singleton's actual slot+8 on store and current19
load. Complete command admission, enable lifetime and mode-dependent source
operands remain separate.

The raw date operand also has a separate owner and epoch. Reset `0x4ac240`
selects month1 through exact getter `0x4f8d50` and writes temporary rawdate2098.
Configuration preparation `0x53d470` separately sign-extends the native
configuration's first WORD `0xc5cae0` into `0xd623fc` at `0x53d5ca`.
Within complete calendar driver `0x4ac650..0x4acc35`, month wraps after11,
but raw date increments when the resulting month equals1, through
`0x4ac7d0..0x4ac7ea`. Current native session store/load retain raw date
DWORD separately from the month at `0x52feac`/`0x5306ac` and
`0x52feba`/`0x5306ba`. The existing World calendar explicitly counts authored
elapsed zero-origin periods. Its elapsed year is not this native date, and
its11→0 wrap is not sufficient proof of the native rawdate update.

The event's date selectors are now bounded operations: when the separate
mode predicate is false, `0x52ca40` computes signed remainders from
`rawdate+10017` modulo12, and `0x52caa0` arithmetic-shifts that adjusted
operand right once before signed remainder modulo5. Getter `0x408ac0` selects actual singleton `0x10bc348`, and predicate
`0x4fae50` tests its DWORD+10 for nonzero. The true branches
instead read `0x854f98` and `0x115d508`/`0x115d50c`. Those additional runtime
owners, integer domains and command ordering still need admission. Their zodiac,
element or event names are not inferred from these numeric operations.

New-map entry `0x53cec0` copies an actual supplied argument's low WORD into
region selector `0xc5cd58` at `0x53cf01`, then stores that argument through
the actual header's+8 setter. This entry has no scalar0..2 range test before
the store. Editor `0x5f5360` is a different entry with an upper-bound<=2 test
and a conditional all-map fertility recomputation. The finite exact-address
census across `.text`, `.rdata` and `.data` for `0x84c550..0x84c5cf`
qualifies39 direct operand occurrences: the parameter initializer's stores and
the identified read sites. It identifies no later exact absolute store in that
range. This finite result does not establish a general indirect-write invariant
or a FarmConfig loader. Numeric factor rows remain source initial values;
matching FarmConfig labels do not prove names for region0/1/2 or live constants.

Three earlier floating-point-control candidates can now be removed from the
writer list. Bytes at `0x6fdef9`, `0x6fdefd` and `0x702708` belong to the
bounded25-entry and7-entry jump tables selected at `0x6fd7f0` and
`0x7025d2`. Their complete enclosing switch functions contain no FP-control
operation; every table destination and direct branch remains in actual code.
These data bytes do not execute `FLDCW`/`FLDENV`. Actual CRT control-word updates
and restores still exist. Eliminating false candidates does not establish live
PC/RC, normal FP stack or exception state at the first harvest arithmetic.

The existing Rule26 wet-ditch query remains an explicitly authored World adapter.
The Original harvest's `0x4c15b0→0x525610` graph additionally queries actual
Flood state and native per-cell timestamps/cached supplier histories. Matching
five-cell query reach alone cannot substitute Rule26's operation/staff/fire
eligibility and eager invalidation history for that entire Original query.

**Remaining live activation prerequisites:**

1. Bind admitted raw source/save context to runtime-fertility initialization,
   rectangle/all-map rewrites, numeric region validation and the native rawdate
   epoch/update order. Do not substitute immutable construction fertility,
   elapsed World year or a startup default.
2. Close the event singleton's complete enable/update/old-load lifecycle and
   accepted selector/date/mode domains; retain one owner for each operand.
3. Prove the selected parameter/configuration lifetime, including any accepted
   indirect writes and load behavior. Treat the39 exact-reference result as a
   bounded census, not a proof over every alias or a FarmConfig import contract.
4. Qualify actual harvest entry PC/RC and FP stack/exception state across reachable
   native/external/indirect calls. Existing local stability and startupPC53 are
   insufficient for that admission.
5. Bind actual water/Flood/supplier/timestamp histories to the accepted field
   query without silently replacing them with Rule26's authored cache contract.
6. Complete the separate physical Actor26 routing, source/target lifetime and
   strict crop-identity/WORD/bin resource balance before any source callbacks
   create World stock.

## World mode17 motion binding (AGRI-02E2)

This closes a bounded **movement-only** integration. `World::find_road_route`
now has an explicit mode17 policy within its existing queue/storage/diagnostics;
there is no second navigator. `WorldGrowerNavigation.cpp` owns transient World
motion tracks and actual committed cells. This creates no native Actor26,
field assignment/work, raw cargo, Farm receipt or Food origin.

### Narrow ordinary-land mode17 predicate (AGRI-02E2)

**EXE_OBSERVED / bounded interpretation.** The pinned PE's actual prepared
WORD writer `0x5ad440..0x5ad8eb`, whole-grid caller `0x5ad8f0` and clearing
function `0x5ad920` close the relevant ordinary-land branches. The clear writes
25,992 DWORDs, exactly51,984 WORD entries. The writer consumes runtime terrain
DWORDs at `0xf6a9e0`; its occupancy branch resolves the signed owner WORD at
`0xfc3750` through the actual body manager. It is not a raw-map-only bit mask.

| Restricted effective cell facts | Prepared WORD | Flood13d | Targetb3c | Runtime nextedge |
|---|---:|---|---|---|
| Unoccupied ordinary raw0/80 with absent native region object890210 or its byte18 nonzero |10|accept|accept|accept|
| Flat ordinary road40/c0 |04|accept|accept|accept|
| Complete Food field195..199 body over raw0/80, effective8/88 |10|accept|accept|accept through actual field-family|
| Complete Farm193 body over raw0/80, effective8/88 |02|refuse|refuse|retry9|

The prepared writer explicitly masks80 in its ordinary branch, accepts zero
remaining terrain at`0x5ad826`, and writes10 at`0x5ad82a`. Road40 is handled
earlier at`0x5ad578` and writes04. Body footprint-pass query`+0xcc` is actual:
the verified Farm and Food constructors install vtables7af1e8/7aea40, both
binding that slot to literalfalse416a50. Food fields then pass the separately
qualified4c11b0 family (195..199,194,26..28); Farm193 doesnot. The narrower
World binding admits only typed paid Food195..199. The native10000000 marker
can independently produce20, with an extra family check in the flood, but its
creation/update is not needed or inferred by this binding.

The empty-land branch additionally reads the native per-cell region-object grid
at890210. If an object exists with byte18 zero, the historical61b ground-image
offset can instead produce prepared1/10/2. Raw terrain0/80 and raw objects0 are
therefore **not by themselves proof** of native prepared10. This region owner
contains30 bounded28-byte records, publisher41f820, and a36-byte standalone
header read at52e906..e915. Complete source-region/height-object recreation is
unclosed for arbitrary region states. A positive local absence predicate is now
qualified separately: ordinary postload535683 calls41f400, which starts by
clearing all51,984 native pointer entries through41f3b0. Its only observed direct
publisher41f820 is called at41f5af, and only for cells whose own terrain10000 or
one of the complete eight neighboring10000 flags is nonzero. The exact neighbor
counter4b4520 uses offsets−228,−227,+1,+229,+228,+227,−1,−229. Rebuild terrain
writes only mask the old value and add80; they cannot create10000 in a clean
ring. Therefore a complete in-bounds3x3 raw terrain neighborhood with no10000
closes the no-region counterpart after this qualified ordinary rebuild.

World can prepare this local absence predicate once from immutable source facts
and require it alongside the flat/source/occupancy gates. World admits no native
region operation or later native terrain normalization. No extra source parser or
invented all-zero-header meaning is necessary. An all-zero36-byte header is **not**
proof of zero region records. Road and typed-field branches bypass this branch.

`.local/agri-02e2/predicates/verify_region_absence.py` separately bytequalifies
the header getter, actual standalone read, ordinary reset/rebuild, publisher and
eight-neighbor counter. Its receipt records41 assertions across8 windows and
10 clean/one-flag negative-control interpretations, with original EXE unchanged.

The same raw family is checked by the actual ordinary runtime nextedge at
4e8f99. Plain land and road have no blocking low4,1000,100 flags, road4+40
link combination or prepared0200 ferry state. Mode17 uses ordinary domain84=0;
unsupported domains, special contexts and peers must be refused explicitly.
Native peer prefilter4e2560 excludes Actor26, but the actual cell-commit
interaction still traverses the real actor chain: empty/self-only chains can
exit without entering state5; no general peer no-op is inferred.

**World admission / AUTHORED restriction:** derive the effective facts only
from an immutable full-map-SHA-bound raw source context plus World's actual
complete paid footprints and roads. Retain map policy, original occupancy,
objects, bounds, exact source identity and flat signed-height checks as separate
gates; unsupported source combinations fail closed. Farm193's whole3x3 body
is a blocker, including the origin. Native return WORD2a/2c must not be replaced
by the Farm origin or a convenient visual-nearby cell. This task can admit a
supported external endpoint and a real paid field endpoint without granting
Farm receipt authority.

This closes a reusable positive cell subset. It doesnot close every alias
writer of the native mutable plane, native height transitions, peers/state5,
Ferry48, suspension13c, linked66/72, exceptional14..16, global88ec38 semantics,
autonomous Actor26 creation, work, harvest or Food production. Routes must
preserve the original cardinal flood and eight-way gradient winner; adding a
movement-edge filter during tie selection would change that route. Reject an
unsupported extracted winner rather than silently choosing another route.

Private read-only verifier `.local/agri-02e2/predicates/verify.py` freshly
extracts and byte-checks13 windows,60 assertions,1,193 instruction lines and4,002
instruction bytes;`verified-predicates.json` retains source before/after SHA,
size, mode andmtime, all unchanged.
It interprets16 bounded cases. It runs neither the original program nor a World
and is not a substitute for actual shared-navigation movement acceptance.

### Exact phase progression and its World driver

The separately qualified `verify_movement.py` receipt has31 address assertions
across7 byte-checked windows,1,146 instruction lines and3,470 bytes. A newly
prepared route has phase20/cursor0. On its first microstep, the current direction
is read and checked **before** the cursor increments; phase becomes0. There is
no dummy direction. Cursor counts accepted directions, not completed edges.

| Microstep | Two-edge route observation |
|---:|---|
|0|Prepared phase20, cursor0, actual source cell|
|1|First direction accepted, cursor1, phase0; cell unchanged|
|11|Phase10 commits path cell1|
|20|Phase19; no next-edge check|
|21|Next boundary accepts direction1, cursor2, phase0|
|31|Phase10 commits path cell2|
|41|Native phase20 sees exhausted route→8; World additionally requires actual target equality|

The initial preparation also checks its first direction before entering the
ordinary microstep loop. Begun phases1..19 do not revalidate the next edge;
cell commit at10 therefore preserves the begun section. At a phase20 denial,
result9 retains cursor/current cell and phase20. The next eligible preparation
uses the actual committed cell. The native helper `0x4eacd0` additionally has
source/statistical side effects; this movement-only policy does not reproduce
or grant those effects.

`GrowerNavigationPolicy::Mode17MicrostepV1` binds **one existing World tick to
one native microstep**. That driver binding is AUTHORED and explicit, not proof
of Original Actor26 property8 lifetime,1/1/2 cadence, original scheduler/global
gate ordering or real-time speed. Historical courier/plantation five-tick edges
remain unchanged. No separate calendar or advancing external simulation exists.

### Admitted World context and conservative refusals

The first explicit request prepares the immutable source context outside normal
tick/frame work. The initial contract requires complete Rule26 raw terrain,
source SHA, boundary/policy and a complete plane without bit10000 **anywhere**.
That is deliberately stricter than the proved local3x3 absence predicate. The
World then models the qualified ordinary rebuilt, region-absent context and
admits no native region operations. It neither imports native save state nor
uses an all-zero-header shortcut. Signed height must be0. Bridges, ramps,
ditches, unknown source combinations and unsupported occupancy are excluded.

Current World roads and complete paid Food footprints supply the effective
source facts; images, distances and names do not. A paid Farm193's entire3x3
footprint is blocked. Both the outward source and returning destination use a
caller-explicit supported external cell cardinally adjacent to that complete
Farm; they are not inferred native WORD2a/2c. The field endpoint is the actual
paid typed field belonging to the requested Farm. This is a motion request,
not a source spawn, target-selection or Farm-return proof.

Unknown prepared predicates are distinct from proved blockers: encountering an
unknown cell during the cardinal flood refuses the **whole** query. It is not
turned into a wall that might change the native gradient winner. Known outside
World boundary/Farm cells stay blocked. Extract first with original ties, then
refuse an unsupported winning edge; never select an invented alternate winner.
The eight-direction extractor retains fewer than500 direction bytes. Combined
ordinary couriers and transient tracks stay within the existing58 channels.

Real current/begun track cells are protected from overlapping paid construction,
road removal, irrigation changes and Elite materialization. Future unbegun
geometry may change. A failed next edge produces9; actual shared re-preparation
then produces a new route, failure10 for proved unreachability, or a separate
Unsupported11 for unqualified context. Exact endpoint equality is required for8,
including an exhausted route; proximity or a supplied result is insufficient.
An unsupported source/actor/terrain context seen during a begun section is
retained, lets that section finish physically, then stops without an arrival8.

State5, suspension13c, Ferry48, linked66/72, exceptional14–16 and global88ec38
remain unsupported interactions. Explicit requests for them are rejected before
publication. Real peer claims block the next section; ordinary active courier
contexts are conservatively excluded. No pause flag stands in for13c, no peer
no-op is inferred, and no interrupted motion grants field work, cargo or stock.

### Persistence boundary

Tracks are explicit non-production sessions, not a Rule27/schema43 generation.
`WorldSnapshot::navigation_session_present` is a default-false publication guard,
never a historical JSON field. Internal snapshot/invariant observations remain
available. Save-document creation, writer and full restore refuse a snapshot
carrying a session; an active or retained experiment must be cleared first.
Cleanup cannot discard an active/begun track. After terminal cleanup, ordinary
historical snapshots/JSON and independent continuation remain exact. Session IDs
are monotone within the World lifetime and never consume/reuse gameplay IDs.

A future persistent Actor26 contract must separately admit native actor identity,
source/field claims, speed/cadence and ordinary/exceptional lifetimes, route owner,
accepted-direction cursor, phase/current/previous cells, work/waits, raw cargo,
reservations and source counters. It needs a separately checked free rule/schema
pair: Rule27/schema43 now belong to Playable Agriculture. No Original Grower
generation or automatic migration is activated here.

## Autonomous transient tending binding (AGRI-02E3)

This extends the explicit movement experiment with World-owned FoodGrower26
dispatch, actual field selection, physical tending and retirement under
`GrowerLifecyclePolicy::AutonomousTendingV1`. It is a transient, tending-only
acceptance policy. It does not select Rule27/schema43 or activate Original crop
growth, harvest, cargo, Farm receipts or Food production. Rules1–26 and
schemas1–42 retain their existing behavior and defaults. The implementation and
static source qualification below are distinct from the executed World/full QA
recorded in the testing guide and from native/human acceptance.

### Source dispatch, staffing and allocation

**EXE_OBSERVED, bounded fresh-session interpretation.** Native scheduler
`0x4ac2b0` reads phase `0xc82ef8`, subtracts one and indexes `0x4ac4e8`.
Phase31's slot30 selects `0x4ac43c→0x416d20`. Its complete registered-building
pass checks actual live bodies and calls virtual slot20 at `0x416eba`;
Farm193's `0x7af1e8+20` binds `0x4c4b90`. Phase35 is not this Farm update's
cadence. The Farm checks actual access, staffing, the separate type-global and
Food-resource vetoes, and operation before entering the dispatcher.

Actual eligible dispatch calls advance retained BYTE36. The due comparison
uses its old unsigned value after storing the increment. Full staffing's
threshold1 therefore attempts on the second eligible call from0, resetting
the byte0 when due. Other qualified staffing tiers retain their separate
thresholds and quota arithmetic; zero staffing does not advance the byte.
Actual operation slot78 reads body BYTE90. Actual staff percentage is integer
`100*assigned/required` through signed-WORD44 and the required-staff/model
query, with nonpositive requirement returning0. These source operands do not
by themselves prove that every native staff/fire change cancels a live actor.

The fresh configuration initializer supplies Food harvest/tending quotas5/2
and harvest/tending field budgets4/10. These are qualified defaults, not a
completed mutable FarmConfig import or native restore contract. Tending
allocation subtracts retained DWORD154 from the requested quota; its selected
field budget uses10 times the requested quota, rather than the remainder.
World's supported counter domain is nonnegative with positive remaining quota.
Initial selection and native allocation-time resource, type-global veto and
operation checks remain separate.

For the admitted single-candidate case, requested2/retained0/selected1 gives
remaining quota2 and computed grouping `floor(1/2)-1=-1`. That grouping is
distinct from actual extra-assignment count: `0x4c3a56` initializes stack+10
to0, while `0x4c3a6a` writes the computed group to stack+20. The first entry
tests actual0 at `0x4c3ae2`, reaches `0x4c3b0c` and calls Actor26 factory
`0x4c3b36` exactly once after the real gates. It does not attach to actor0.
Only success later copies the group into the actual attachment count; the sole
entry then ends without another iteration. Failure publishes no identity,
assignment or counter increment. Private `verify_single_candidate.py` adds33
assertions over the complete allocation window,214 instruction lines and721
byte-checked bytes, with independent success/failure traces and input guards.

Factory failure consumes its selected entry without creating an actor,
assignment, counter increment or successful ordinal. Actual success alone
creates state6/role1, wait `10*prior-successful-ordinal`, actual source62 and
initial field68=0, then increments the retained counter and assigns the actual
actor ID to the selected field. Initial commitments are not yet the actor's
current task. Retained counters are not active-worker counts: failed state6
selection debits tending before physical return, and actual retirement debits
it again with a floor at0. Occupied movement channels must remain independent
of those counters.

The initial selector `0x4c0380` invokes Farm slot26c, bound to `0x4c3c60`.
It reads source model width3 and Farm slot260→`0x4c31f0`; exact Food193
family `0x4c3350` selects global `0x84c554`, initialized3 at `0x4bfc7e`.
For field-origin minus Farm-origin `(dx,dy)`, the predicate is
`(abs(dx)<=3 || abs(dx-2)<=3) && (abs(dy)<=3 || abs(dy-2)<=3)`, equivalently
`-3<=dx<=5 && -3<=dy<=5` in this fresh width/radius profile. It uses the
complete Farm footprint's origin and extent, not a radius around its external
road or the authored field-parent binding.

**PARTIAL runtime score, positive single-candidate result.** Initial tending
score calls `0x4c1af0→0x4c1b30`, using runtime fertility bytes or a separately
resolved global override. Runtime and serialized construction fertility remain
different operands. Standalone runtime-plane reads do not close subsequent
reset/recompute/global ordering, and accepting Properties6 does not authorize
a live score. The supported World scope has exactly one initially eligible
candidate: eligibility is independent of fertility, and the sole queue element
cannot change its selected identity or allocation with that unresolved score.
Only this winner independence is established; no numeric score is admitted.
Multiple initial candidates remain unsupported, without substituting
construction fertility100 or an invented tie order.

### Native entry and external Farm coordinates

**EXE_OBSERVED.** Actual Farm193 construction installs vtable `0x7af1e8`.
Its access-refresh slot84 binds `0x51ce50→0x426df0`. Manager `0x4acfc0`
sign-extends actual EntryX/Y `0xc5cdfc/0xc5cdfe`, prepares distances through
`0x5ae140` with target `(-2,-2)`, clears active sources' WORD2a/2c and calls
their actual slot84. Shared `0x426df0` passes source signed X/Y and full side
to `0x4baf40..0x4bb156`; success alone copies selector outputs
`0x10c72ac/0x10c72a8` into source WORD2a/2c. Failure creates no nearby point.
Those retained coordinates feed the actual Grower factory, source-distance
preparation and exhausted tending selection's return target. The Farm origin
is not a passable substitute.

Entry words belong to MapProperties owner `0xc5cae0` at offsets31c/31e.
For outer Map5/Header2 and Properties3 or6, selector slots0/3 both reach read
branch `0x52d1fc`. Its packed body entry offsets are792/794; native structure
padding is not serialized. Map WORD2 + cafe DWORD4 + Header WORD2 + Header2
body48 + Properties WORD2 gives body base58. Signed EntryX/Y are therefore
logical WORDs850/852. Complete afterload `0x52c9c0..0x52ca3f` copies separate
preserved fields without overwriting either entry word. Other properties
versions are outside this source adapter.

Native coordinate preparation `0x53ce60` selects actual Header+4 context
through `0x52e7b0→0x4f8210`. Table `0x856c64` supplies sizes
56,84,112,140,170,226 and linear base `border*229`, where
`border=(228-size)/2`. The admitted shared geometry supports84,112,140,170,226.
Header context index at logical DWORD8 must agree with the separately
serialized MapProperties size at logical84 before entry translation
`(x+border,y+border)` and playable-geometry containment. No map name, coordinate
exemption or visual road supplies this fact.

Native `0x5ae140` clears51,984 DWORD distances at `0x1391fe0` through
`0x521140`, seeds the exact source distance1 unconditionally and uses one
51,984-cell ring queue. Sole helper `0x5ae240` expands unvisited destinations
with nonzero `preparedWORD & 0x0b7c`, north(-228), east(+1), south(+228),
west(-1), writing parent distance+1. This mask admits ordinary empty-land
WORD10 and road WORD04; Farm WORD02 fails. It differs from mode17's flood13d
and is not road-only navigation. A real target can stop it when popped; queue
exhaustion also ends it. Manager's local sentinel(-2,-2) lies outside playable
geometry, but its converted storage index need not be negative after adding
the border. No sentinel arrival or exact reconstruction of the whole mutable
native manager plane is inferred by the World witness below.

For side3, actual table `0x820038+96*3` contains twelve cardinal perimeter
offsets, clockwise from the northern row: (0,-1),(1,-1),(2,-1),(3,0),(3,1),
(3,2),(2,3),(1,3),(0,3),(-1,2),(-1,1),(-1,0), followed by twelve zero entries.
Preferred selection requires runtime Road40 without special bit4, a positive
distance and road-region `0xec5a10` against mutable priority table `0x1312588`.
Strictly better priority replaces the candidate; ties retain the first.
Occupied contexts may invoke actual body slotd0 and are not assumed ordinary
unoccupied Roads. The fallback separately chooses the least positive perimeter
distance, retaining the first tie, without repeating the Road predicate.
A unique positive ordinary perimeter Road avoids that priority ambiguity and
fallback; adjacency alone does not prove the positive distance.

### Native tending, return and cleanup

The qualified ordinary tending lifecycle is6→7→8→6, followed by no-receipt
return9 when selection fails. State6 decrements wait and clears it when due
before calling the source selector. Actual tending caller `0x4c2c70` pushes
literal previous-ID0 at `0x4c2c7f` before `0x4c2c8b→0x4bff80`. Each tending
scan therefore starts at the registered collection's beginning. It requires
status1, raw terrain100 exclusion, actual field family, resource/operation/veto
operands and assignment equality to the actual actor. The first selected result
is separately rechecked for growing period. Repeated tending has no additional
source-range query. Ordinary harvest caller `0x4c2b20` likewise supplies
literal0 at `0x4c2b2e` before `0x4c2b38→0x4c0060`; its helper's optional
nonzero preceding-ID cursor is not the ordinary caller's policy. Both ordinary
scans restart at the registered collection's beginning.

Success stores the actual field reference/coordinates and enters7. Failure
retains the last field reference, targets actual source WORD2a/2c, enters9 and
separately debits retained tending. Actual shared movement8 admits work state8
and Food wait51. After51 eligible state8 callbacks, compatibility alone gates
field slot264; the completion helper adds no assignment-equality, quantity,
staffing or harvest-mark gate. Actual `0x4c1d20` ORs work40, clears assignment2e
and body counter150, and preserves quantity, history and sole actor cargo7c.
The actor re-enters6 even if compatibility failed. Field callback compatibility
and the stricter supported World domain must not be conflated.

Return9 movement8 or10 retires without a Farm resource receipt. Retirement's
role1 counter debit is additional to any failed-selection debit and floors at0.
Actual manager removal cleans assignments before reset: every active registered
field assigned to that actor is cleared; inactive fields and another actor's
assignments are unchanged. Reset clears source, last field, wait, role and the
same cargo word. Route, link and peer cleanup remain engine responsibilities.

**E4 remains separate.** A role1 tending actor with cargo7c=0 reaches no Farm
stock receipt. Native tending can visit different typed Food fields without
converting a resource, but this does not prove mixed-crop harvest. Repeated
harvest can overwrite Actor68 while adding into the same7c; Farm return derives
resource from the current last-field model. Food deactivation unregisters the
field, clears its assigned actor wait and assignment, but locally retains
Actor68/7c. Reusable native slots have no generation check in the return getter.
E4 still needs a same-resource temporal invariant, source/field lifetimes and
explicit narrowed/carry/accepted/overflow accounting. No second per-crop cargo
copy or relabeled last-field return is established.

### Explicit World binding and unchanged historical authority

**AUTHORED driver and conservative World domain.**
`WorldGrowerLifecycle.cpp` owns the controller, typed monotone FoodGrower IDs,
actual fields, assignments, work observations, source counters and movement
channels. Explicit start requires the Rule26 prepared context, complete paid
Farm193 and typed Food footprints in the fresh native range, and sealed
`GrowerSourceFacts`. Its sole factory uses the shared path resolver,
`EmperorContainer`, bounded `read_range`, map probe and `MapGeometry`; it binds
the retained complete source SHA, coherent Map5/Header2/Properties3or6 operands
and contained storage entry, then rechecks the source SHA. Callers cannot
construct or restore an entry grant or supply a movement/creation result.

World requires one ordinary Farm perimeter Road and a supported ordinary entry
Road. Its existing shared `find_road_route` prepares a positive cardinal b7c
witness from that exact entry to the unique port, reusing its queue/storage.
The witness can stop at the port and does not claim the complete native manager
plane. Supported empty land can participate; a road-only route is not
substituted. The qualified unique-road predicate determines the source endpoint
without native road-region priority or fallback guesses. Access is refreshed
against actual World topology revisions, rather than supplied as a spawn point.

Source dispatch uses phase31 of the existing scheduler and actual workforce
captured before the ordinary tick's population changes. The retained fresh
configuration5/2 and tending fieldbudget10 define this explicitly bounded
session; they do not establish arbitrary native mutable configuration parity.
The single-candidate check retains only eligibility at fertility extrema0/100,
discards both numeric scores and rejects multiple candidates. Only actual
accepted World allocation consumes a FoodGrower ID, commits the field
assignment and increments the native retained counter. One live actor at a
time is an explicit conservative peer fence, **not** an Original quota or
worker-limit finding. Actual occupied channels remain separate from counters,
including the interval after failed selection but before physical retirement.
The existing58-channel budget and bounded eight-record actor audit are retained.

One World tick drives one supported actor callback and the shared mode17
microstep when movement is needed. This clock remains AUTHORED, not a native
Actor26 speed claim. The actor selects its assigned field in state6, moves its
actual committed cell through7, receives wait51 only at the exact real endpoint,
then applies the tending callback through8. Its cleared assignment allows the
next actual scan to fail, debit the counter and prepare return9. Natural return
uses real shared motion to the retained external source cell before retirement,
the second counter debit, assignment cleanup and reference/channel reset. No
caller-triggered worker body, supplied arrival8, proximity or rendering grants
work.
The value-operation adapter normalizes copy-only target/reference operands to
checked World IDs and storage-cell coordinates. Those slots are not imported
native actor IDs/local-coordinate WORDs and are never raw saved authority.
All physical navigation operands use the same frame; the source reader applies
the verified local-entry border translation.

The session owns separate transient field work/assignment state. It does not
advance native growth, quantity or harvest history; create harvest cargo;
credit Farm bins or Food; or replace the existing historical Agriculture
accounting. Inherited authored Wheat/Millet production remains its historical
contract and is not evidence of this tending path. Field-parent membership is
a supported World fence, not an inferred immutable native ownership rule.

Source/access, staffing, operation/fire, peers, the admitted raw month slot and
field membership bound the session. A newly introduced in-range field or an
unqualified context conservatively stops further work. State5, suspension13c,
Ferry48, linked66/72, exceptional14–16 and global88ec38 remain unsupported.
Actual native source-status failure marks retirement and continues its body;
World's stricter stop policy is not relabeled as that native behavior. Stop
preserves a begun physical edge, then retires without another work callback or
invented successful return. Current/begun actor claims remain protected through
this drain. Clear is refused while an actor remains live.

The existing `WorldSnapshot::navigation_session_present` publication guard also
covers a retained controller with zero actors. It is not a historical JSON
field. Save-document preparation, publication and restore refuse that transient
context until explicit cleanup; no actor, assignment, shadow work or source
counter is serialized into schemas1–42. Historical snapshots, rules and schema
markers remain unchanged. This research step allocated no saved generation;
Rule27/schema43 now belong to Playable Agriculture. Persistent Original actors
or full native production still need a separate explicit contract.

### Reproducible static evidence and remaining acceptance

Private read-only dispatch verifiers freshly recapture PE windows:
`verify_dispatch.py` records90 assertions/13 windows/877 decoded instructions/
2,790 bytes; `verify_entry.py` records80/17/960/3,199; `verify_range.py`
records28/6/398/1,387 plus4,225 bounded range identities. Independent
`lifecycle/verify_lifecycle.py` records76 assertions/15 windows/1,650 decoded
instructions/5,622 bytes, including overlaps. Guarded Original EXE, model,
configuration and supplied-map inputs retain their SHA-256, length, mode and
mtime, as applicable. No Original executable runs and no personal save is read.
These receipts qualify source behavior; they are not World/full QA, native
delivery or human acceptance. Physical World verification, historical
noninterference and persistence refusal checks remain separate acceptance work.

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


## First physical harvest boundary (AGRI-02E4)

**PARTIAL at positive World harvest.** An explicitly transient single-field
World policy now binds actual field development and harvest movement through
wait25. The marked callback remains blocked before any field/yield mutation;
empty cancellation at the field is not a physical Farm return or receipt.
Rules1–26/schemas1–42 remain exact. No positive saved generation is activated.

The private lifecycle verifier qualifies128 assertions over37 byte-checked
windows,3,387 decoded instruction lines and11,306 bytes including overlaps,
with separate single-candidate factory-success/failure traces. Source inputs
remain read-only. These static proofs and resolved traces are separate from
the paid World observations recorded in the testing guide.

### First fresh Farm193 WORD40 gate

Farm193 constructor4c5b80 calls4c5c20, then51c9a0,426c90 and426e60. Base clear zeroes45DWORDs startingbody+4, endingbody+b7 inclusive; therefore signedWORD40 starts0. The actual Farm193 vtable is7af1e8. Its initializer+90 is4c30f0, not an imported body/archive copy. Generic base-init428c10's optional virtual150 branch is skipped because the Farm binding413a00 returnsfalse. Industrial initialization keeps its separately allocated scene object; Farm initializer clears owninfo5c and counters150/154, then registers the source. These operations do not supply a nonzero stockWORD40.

Actual Farm source+20 is4c4b90 and dispatch4c3550. Actual Farm receipt+268 is4c4ee0. Both actual Farm transport bodies work through five separate bins atbody+158; neither writes FarmWORD40. Empty bins begin resourceDWORD0/stockDWORD0/capacity400 via4c43b0. Before the first supported harvest-yield completion, the bounded fresh trace has tending work on actual FIELD bodies, source counters, shared navigation and no raw credit/Cart. Generic return4c2e50 DOES update inherited sourceWORD40, but it is not Farm193+268. Other +40 stock/cart bodies4c5100/4c5300 are not this Farm vtable's +20 or +268. The permitted fresh first-allocation trace therefore retains FarmWORD40=0 and passes the signed<400 comparison4c37ec..4c37f2.

This is a first-fresh-trace invariant, not a semantic name forWORD40, not a Farm-bin-capacity proxy, and not closure of arbitrary Native restore or externally edited body state. World must own the fresh0 counterpart and keep Native archive/import/general stock writers outside this policy. A later positive yield/receipt would require its own explicit lifetime audit.

### Harvest N1/K5 factory branch

For actual selectedN1, requestedquota5 and retainedharvestcounter0, K=5. Harvest4c377f divides1/5→0, decrements→-1, then4c378a..4c3790 clamps the computed group to0. This differs from the separate tending body's uncapped group-1. Actual extra-attach stack+10 was independently initialized0 at4c3781; computed grouping is stack+20. K5 skips the K1 override. First iteration reads actual0 at4c3818 and takes4c3842. With retained0<requested5, actual Actor26 factory call4c386c is reached once.

Factory0 goes straight to entry-tail without assignments/counter/actor commit. Actual success alone initializes state10,role2,wait10*prior-successful-ordinal (0 here), source reference, increments150, and assigns the actual actor to the sole selected field. It does not preselect Actor68. After either result, selectedentry ordinal becomes1 and the N1 backedge is not taken. Verifier has separate interpreted success/failure cases; these cases do not themselves count as live allocation witnesses.

### Field callback order and admitted statuses

Actual phase20 jump-table index19 binds4ac3bf→51e4a0. That function first calls mark-schedule4c1a90, THEN traverses live bodies. Common active-query426d10 accepts1 or3; resourceBYTE47 must be nonzero, actualfield-family test must pass, then51e50a calls4c0f60. Field initializer4c189c writes resource47 from actual field-model classification: Food195..199 maps7/5/6/3/1, all nonzero. Complete freshly paid World fields can bind the narrower status1 domain; this is not evidence that native status3 receives mark/expiration.

Mark setter4c1a20 requires actualstatus1 and matching crop, and ORs8000 at endslot/subslot0. Quantity, staff, assignment and terrain do not gate this setter. Food ends are6,10,9,11,8; starts2,6,5,7,4. The driver4c0f60 skips everything on effective terrain100. Otherwise positive countdown63 decrements once and returns before growth/veto/publication. Fresh owninfo constructor51c2e0 sets countdown0, Q0 and workword0. Zero countdown calls concrete increment+25c, adds WORD Q and clamps signed-above10000, THEN evaluates type-veto4afd80 and clears Q if true, THEN publisher+260. Zero delta still reaches veto/publisher. Food delta is0 at last endslot; otherwise worklowbyte>32 gives157,1..32 gives79,0 gives0. It does not directly require growing-period membership.

At completion of each51-phase cycle,4ac650 advances the subcounter. For oldsubslot15, it processes the still-current completed outer slot6..11 through4bff20 before publishing newsubslot0/newoutermonth. Status1 fields matching their last endslot reset Q, entirework and assignment; a real assigned actor's wait3e becomes0 first. Countdown/history/body150 are untouched. The later unconditional calendar tail4acc28→4c0570 visits active status1 fields for4c1bc0 work decay every cycle, including nonrollover cycles. Decay decrements unsignedlowbyte floor0, preserves onlyc000 highflags, clearsbits8..13. Expiration therefore precedes that cycle's decay. Growthphase20 precedes its cycle's decay.

Common parent5371a0 calls scheduler5371bf BEFORE actual actorpass5371ce. Thus phase50 expiration/waitclear and decay precede the actor callback; that actor may already see the new outer slot after rollover. World `advance_grower_fields` using oldposition=ticks-1 atphase20/50 and runningbeforeactor matches this ordering. Actual later tending cannot retroactively affect a field already visited atphase20.

### Type/resource veto separation

4afd80 resolves actual body modelWORD14 and returns BYTE[10bc7e0+type]>0. It is not a staff/fire/operation alias. Actual fresh after-load/reset5355f0 contains5356f4..535711 clearing all269 bytes (67DWORDs+1BYTE) to0. Native archives separately serialize/load this table. Explicit World fresh-session scope with no Native global restore/edit writers can bind this zeroepoch; an imported unknown table cannot silently usefalse. World actual operating/fire checks are additional AUTHORED domain restrictions.

Resource-veto is a DIFFERENT global signedWORD table1312014[resource], with zero permitting resource. Initial harvest, allocation and repeated selectors query it. UI40d320 toggles0/1;4ae3a0 can deactivate Farm193 when any Food resource1/3/5/6/7 is disabled. Operationtrue alone does not prove resource-veto0. The existing fresh E3 World domain uses enabledFood resources without importing Native globals; keep that explicitly scoped/authored. This E4 proof closes fresh TYPE-veto epoch, not every resource-table Native initialization/archive writer. Field growth itself uses only type-veto, so no resource-veto assumption is needed to apply safe independent growth.

### Exact harvest boundary and cargo identity

Native ordinary state10 selects→11, actualmovement8→12 with Foodwait25; due state12 calls actual field+268→4c1dd0 then returns10. Repeated target failure retains lastField68 and uses actual source2a/2c; cargo7c interpreted signedpositive selects13, otherwise9. State13 movement8 alone reaches Farm+268 receipt; movement10 retires without receipt. Native role2 retained-counter debit on failed selection and actual retirement are separate and floor0. Cleanup clears actual assignments before resetting same7c/lastField/source/role.

4c1dd0 FIRST setswork4000 and bodycounter150=0, BEFORE testing8000. Marked path consumes8000, evaluates live yield, writes currenthistoryWORD H, adds H modulo65536 into sole actorWORD7c C, clears Q and assignment. A LiveYieldContext block must happen before entering4c1dd0, including its unmarked-prefix mutations. A World physical state10→11→12/Wait25 witness followed by explicit AUTHORED no-cargo cancel/retirement is possible without claiming native harvest completion, physical return13, raw yield, bin credit or Food. Keep blocked Q/mark/history until their actual expiration event; only real cleanup clears assignment.

One field, one stable WorldID/NativeWORD mapping, one crop and one live actor protect Actor68/resource identity until cancellation. Native repeated harvest itself lacks that same-resource invariant, and native return dereferences retained68's CURRENT type, without generation proof. Do not generalize this scope to mixed crops, slot reuse/deletion or imported actor references.

### Ordinary harvest selection starts at cursor0


Fresh byte qualification adds complete actual caller4c2b20..4c2c65 and helper4c0060..4c0129. Ordinary state10 dispatch calls4c2b20 at4cca7b. That source caller pushes source-model family, literal previous-ID0 at4c2b2e, and actualActorID separately at4c2b30, then invokes4c0060 at4c2b38. ActorWORD68 is NOT supplied as the search cursor.

The helper reads its previous-ID argument at4c0061 and tests it at4c0068. Zero sets its local search-enabled flag at4c006d;4c0092 then jumps directly to candidate status/family checks at4c00a9. Every ordinary invocation consequently restarts from the first registered field. The same helper supports an OPTIONAL nonzero preceding-ID argument: only that path compares each actual bodyID with the supplied priorID, sets search-enabled on the matching entry, skips that matched entry itself, and begins testing following collection entries. This optional helper behavior is not the ordinary caller policy and is not a numeric fieldID-greater comparison.

World uses literal cursor0 for both state6 and10. Its one admitted field remains safe because completed native harvest clears assignment, and the E4 gate cancels before callback/any reselection; Actor68 remains the retained identity for work and potential return labeling, not an ordinary repeat-search cursor. After the first live-yield block, the session refuses further harvest allocation while field expiration continues.

### AGRI-02E4: qualified live-yield boundaries, production still blocked

**EXE_OBSERVED / PARTIAL.** Fresh private
`.local/agri-02e4/yield/verify_live_boundary.py` independently recaptures16
bounded PE windows and verifies73 address/call/graph assertions,953 decoded
instructions and3,545 instruction bytes, including overlaps. Its receipt
`verified-live-boundary.json` includes65,536 source/modifier-byte arithmetic
identities, the exact120-cell dry-query walk and12 explicit PC/RC arithmetic
interpretations. EXE, BuildingModels and FarmConfig retain SHA-256, size, mode
and mtime. The Original program is never executed and no personal save is read.
This is static source evidence, not a positive World harvest or yield grant.

**A concrete ordinary-map fertility epoch is now qualified.** In the complete
actual `43abf0..43aea4` caller, EBX is zeroed at43ac56. Its successful
standalone-map branch passes header-only0 and reaches43ad9e→52e7c0. The
actual archive reads the runtime plane eac3f0 at52ea84 and the distinct
construction plane e9f8e0 at52eaa6. The same successful caller then reaches
43ae05→53d100→53d1e4→5355f0→5356bc→4c1d00→4c1c20. Thus this particular
ordinary initialization performs a later all-map runtime-fertility recompute;
its archived runtime bytes are not the final post-recompute values.

The real rectangle writer reads construction bytes at e9f8e0 and actual
modifier BYTE84c560, then stores
`(floor(55*constructionByte/100)+modifierByte) mod256` in eac3f0. It does not
read an immediate10. Modifier10 is separately qualified as an executed startup
initializer value; selecting it for a World epoch still needs the admitted
fresh configuration lifetime. The formula is exact for every byte pair. The
all-map wrapper uses the selected native map width/height and coordinate base.
This closes the ordinary recompute epoch and conversion, not arbitrary saved-
game, editor, reload or subsequent update semantics. Existing4b5b2b rectangle
updates, editor5f539d and the additional all-map callers remain distinct real
operations. A World may not turn the existence of this path into a universal
construction-fertility substitution for live harvests.

**A useful dry-irrigation branch is independent of the unresolved wet owners.**
Complete4c15b0..4c16ab walks square perimeters1..5, excluding the field center:
8+16+24+32+40=120 unique cells. For each reached in-bounds cell it reads actual
runtime terrain f6a9e0, skips it when bit800 is absent at4c164c/4c164f, and
also excludes off-map80000 before calling525610. When every tested cell lacks
800, the full query returnsfalse at4c169a without invoking525610. Flood state,
RoadBlock126 neighbors, supplier histories and timestamps in the latter helper
are then unqueried operands; no guessed values are needed for those branches.

A World dry admission must bind that actual **effective runtime-terrain**
predicate at every queried cell, not merely inspect immutable raw terrain or
read Rule26's derived wet cache. At minimum, use the complete source SHA-bound
map and actual World footprints/overlays: source raw0/80 and supported ordinary
roads plus the already qualified Farm/Food effective8/88 overlays cannot supply
800. A paid Ditch202 in the query area must be refused even if its World cache
currently says dry. Pump203 footprints and other unqualified overlays must be
excluded until their effective marker/write lifetime is admitted; source-native
800 markers also refuse. Require complete supported bounds/source/occupancy
for the whole query neighborhood, and invalidate on relevant construction or
source changes. A conservative query can require all120 cells be supported;
it need not invent off-map, Flood or wet-history behavior. This is a proved
`irrigated=false` subset only. It does not equate current Rule26 water caches
with the Original wet query or close a positive irrigation bonus.

**The smallest independent live numeric blocker remains incoming FP context.**
The actual marked field callback4c1dd0 resolves its actor and Food own-info
pointer (Food slot1e8→416b50), reads actual event singleton state through
48e0f0→608620, conditionally classifies the crop through48e680, and reads the
runtime fertility byte at4c1e81 before first FP load4c1e8d. Its immediate
integer predicates do not establish a new x87 environment. The first scaling
multiply4c1e91 and binary32 store4c1e97 consume the incoming PC/RC. There is
no local control-word, stack or exception reset before them. The already
qualified local arithmetic graph preserves the supplied control word and its
converters restore it; neither fact supplies its actual live value or proves
normal incoming stack/exception state. The ordinary actor-pass parent5371a0
also contains no such entry reset. Startup76d3a7 selects53-bit precision under
a precision-only mask; it is not a complete harvest-entry CW/FP-state proof.

The distinction changes physical yield. With separately supplied conditional
Rice197 operands quantity10000, runtime fertility65, dry irrigation,
region-factor bits3f800000 and raw date-300 (global stage unqueried), the
existing exact arithmetic produces65 for supplied CW007f and64 for027f.
Across the12 supported normal PC24/53/64 and RC choices the result still spans
64/65. These are explicit arithmetic counterexamples, not a claim that either
control word or the whole tuple is admitted in an actual World. A host-default
or startup53 substitute cannot establish which amount an Original harvest
would carry. Single-candidate winner independence from E3 does not remove this
numeric dependency.

**Region, event/date and factors remain separate owners.** The admitted
standalone Header+8 region operand is logicalDWORD12; actual52eb52 publishes
its low WORD to signed c5cd58. Validate the actual admitted0..2 numeric domain
and prevent unadmitted region/editor changes; the unrelated map-size context
index is logicalDWORD8. Actual harvest4c1ed0 uses that signed region,4c1ef3
uses raw date d623fc, and date>-300 reaches actual global factor84c55c. Native
configuration preparation53d5ca supplies a signed source WORD to raw date;
calendar4ac7d0..4ac7ea increments it at resulting month1. The existing elapsed
World year is not that native epoch. Actual event enable BYTE50, state DWORD54
and selector DWORD58 have startup/archive/command/update owners; standalone
map entry facts do not bind their live values. Source factor initializer bits
and the finite39-reference census likewise do not establish every later
configuration/alias lifetime. The new fertility epoch and dry branch must not
silently fill any of these operands with defaults.

**World activation consequence:** these proofs can support an explicitly
bounded source-epoch/dry predicate and improve the refusal diagnostic. They
do not authorize an actual CropHarvestContext or a positive Raw yield. Without
actual incoming CW (PC/RC and all exception masks), normal FP stack/exception
state, and the remaining admitted runtime numeric owners, stop at the harvest
boundary before applying the context-dependent callback. Do not consume the
mark/quantity, add cargo/history or credit a Farm bin from invented inputs.
Independent paid growth, mark, movement and wait observations can progress;
positive Original Raw production and any new persistent generation remain
blocked. The next precise proof is the native actor/field callback entry
postcondition on its actual admitted execution path, together with the source
initialization/update fence for the remaining live operands. Passing an
arbitrary context DTO or rational arithmetic test cannot satisfy that gate.


## Live harvest context admission (AGRI-02E5)

**PARTIAL, with no admitted numeric context or positive World Raw cargo.**
The actual E4 Wait25 worker invocation now consumes shared read-only World
preparation. A private constructor and const bindings/failures/context prevent
a freely supplied Pure-Rule DTO from asserting live authority. Current
preparation reports actual identity, region, dry-query and missing numeric
provenance. Its context is empty. The first marked callback mutation remains
behind this boundary. Rules1–26/schemas1–42 and their defaults retain their
contracts.

Two private static receipts preserve the supplied Original input identities:
`fp/verify_normal_entry.py` qualifies121 assertions over43 byte-checked windows,
824 instruction lines and2,721 bytes; `operands/verify_operands.py` qualifies117
assertions over43 windows,1,944 instruction lines and7,295 bytes, including
overlaps, with9 Properties-preservation cases. Neither runs the Original or
reads personal saves. The native source facts, reference-derived ABI evidence,
conditional arithmetic and actual World tests have distinct claims below.

### Complete ordinary callback prefix and incoming FP authority

The pinned Original now has a byte-qualified ordinary Actor callback path, not
only a direct invocation of the harvest Pure Rules. The ordinary frame invokes
`0x4e27e0`; the actual Actor vtable slot `+0x28` binds to `0x4c7580`, whose
40-byte descriptor26 entry is **`0x4cc670`**. The latter first performs the
actual property8 query (`0x4eb9c0`, Actor `+0x114` -> integer reader
`0x4c9310`), resolves the actual source/field objects and dispatches raw
state12 through `0x4ccc10` to `0x4c2fd0`. A current waitWORD1 is decremented
to0. The actual retained Food field accepts class-4 through its `+0xc8`
predicate, so the helper invokes that field's `+0x268` -> `0x4c1dd0`.
The field harvest mark8000 must be present to reach arithmetic. Its absence
returns without any FP operation.

Every direct helper and concrete indirect edge from the return of the last
timing import through that marked entry to `0x4c1e8d` has been qualified. The
object-array accessors, Actor property query, concrete field-resource switch,
field-compatibility predicate, own-info accessor and all event-enable/crop
classification branches in this interval are integer-only. They neither load
an FP value nor alter/control/initialize an FP environment. The first FP
operations remain `FILD` at `0x4c1e8d`, `FMUL binary32` at `0x4c1e91` and
`FSTP binary32` at `0x4c1e97`. Consequently this complete immediate native
prefix **preserves** the incoming CW, tag/TOP and exception status; it does
not establish a particular numeric live CW or a fresh FP state.

The timing calls are concrete, rather than unnamed external unknowns.
`0x4e26c9` unconditionally invokes the actual PE import
`KERNEL32.dll!GetTickCount`; only afterward does the actual flag
`0x10aeef4` select an optional `QueryPerformanceCounter` call at
`0x4e26e2`. That flag is initialized from the actual
`QueryPerformanceFrequency` result, so zero is not assumed. The bounded
slot1 sole eligible Actor26, with other array entries valid status0 objects,
valid retained objects, bounded age, no linked actors and no suspension, has
integer-only Actor-pass setup before this timing wrapper.
This excludes an earlier Actor callback in that same pass, not previous
frames or the ordinary frame's preceding scheduler. The timing-finish body
after a prior callback can perform floating division and set sticky status.

Microsoft's documented x86 convention requires an ordinary callee to preserve
the FP control word and clean its FP stack except for an FP return. This is
REFERENCE_DERIVED ABI evidence for normal integer-returning timing calls;
it does not assign RC/masks or prove that a particular runtime frame supplied
those values. It is not evidence that `GetTickCount` corrupts FP state, nor a
requirement to prove every hypothetical external exception path.
([Microsoft x86 FP calling convention](https://learn.microsoft.com/en-us/cpp/cpp/floating-point-coprocessor-and-calling-conventions?view=msvc-170))

The actual startup initializer `0x7650aa` calls processor-feature detection
(`IsProcessorFeaturePresent` when available, otherwise a balanced arithmetic
probe), issues only the PC mask request through `0x76d3a7`, then performs
`FNCLEX`. The request selects nativePC0200/53 significand bits while retaining
recognized incoming RC and exception-mask flags. The startup routine has no
`FNINIT`, complete-CW constant or stack-tag initialization. Its `FNCLEX`
clears startup exception status, not an arbitrarily later harvest's status.
The marked-harvest conversion helper `0x7650fc` temporarily selects truncate
and restores its saved incoming CW; its first invocation follows the first
fertility multiplication/store and supplies no authority for their earlier
rounding. Examined ordinary CRT math-control groups likewise save/temporarily
change/restore a caller environment. This is not an exhaustive all-program
writer proof or a numeric harvest epoch.

The smallest concrete remaining numeric FP gate is the actual later **RC
bits0c00 and low-six exception masks003f**. The startup PC-only request does
not determine them. PC53 still needs the admitted later preservation epoch;
actual empty stack tags/TOP and no pending unmasked exception need a separate
entry postcondition. Masked sticky flags alone are not an error or a demand
for an entirely zero status word. No current sealed map/source context owns
these live native FP facts. Therefore normal-prefix qualification does not
authorize a numeric live yield, host defaults, a freely supplied context or
automatic selection of CW027f. Unknown FP authority still rejects the whole
harvest-context preparation before the actual marked callback mutates a
field, cargo or quantities. Pure-Rule tests with explicit CW controls remain
conditional arithmetic evidence.

The RC gap is material even if PC53 and all six masks are granted
conditionally: Rice197,quantity10000,runtime-fertility65,dry,region-factor
bits3f800000,raw-date-300 yields64 with suppliedCW027f and65 with supplied
CW0a7f. This is the same PC53 in both cases; upward rounding alone changes the
result. These are conditional arithmetic controls, not observed live CWs or
a current World harvest acceptance case.

Private reproducible receipt: `python3 .local/agri-02e5/fp/verify_normal_entry.py`:
121 assertions;43 byte-qualified windows;824 instruction lines;2721 instruction
bytes including overlapping windows. EXE, Models and FarmConfig bytes, mode
and timestamps were unchanged. This is static EXE observation, with no
Original execution, build, gameplay QA or native harvest acceptance claim.


### Actual numeric startup owners

The executed CRT range includes three concrete pointer slots:

| Slot | Target | Owned global constructor |
| --- | --- | --- |
| `8142f4` | `480040` | Properties (`c5cae0`) |
| `8143ec` | `4bf100` | Farm configuration (`1031418`, plus scalar parameter bank) |
| `81437c` | `48dea0` | Event singleton (`c701d8`) |

The Properties chain is `480040 -> 480050 -> 52c990 -> 52cb80`.
The reset writes signed WORD **-2500** to the actual first WORD of the global
Properties object at `52cb8a`. This is a proved fresh constructor value, not a
general map-file date or a guarantee about a later mission/editor/load epoch.

The farm chain is `4bf100 -> 4bf110 -> 4bfc00 -> 4bfc20`. The actual scalar
initializer writes modifier DWORD **10** to `84c560` at `4bfc6f`, global factor
bits **0x3f933333** to `84c55c` at `4bfc65`, and the per-crop regional float bit
rows below. The object `1031418` is separate from this absolute parameter bank;
calling the object a configuration owner does not establish a text-file loader.

| Actual crop type | Region 0 | Region 1 | Region 2 |
| --- | --- | --- | --- |
| Wheat 195 | `3f333333` | `3f4ccccd` | `3f266666` |
| Millet 196 | `3f333333` | `3f4ccccd` | `3f266666` |
| Rice 197 | `3ee66666` | `3f266666` | `3f800000` |
| Cabbage 198 | `3f19999a` | `3f4ccccd` | `3f266666` |
| Soy 199 | `3f333333` | `3f4ccccd` | `3f266666` |

The current exact absolute-address census finds 39 parameter references, all
identified initializer stores or reads. This is an address census, not a proof
that arbitrary aliases or all later native configuration contexts cannot write
them. A current context must bind a qualified initialization/configuration epoch;
it must not merely copy values from an independently supplied DTO or text file.

### Properties preservation and raw date

Standalone archive reading snapshots the **pre-load** global Properties object:
`52e9bd` supplies `c5cae0`, `52e9c6 -> 52ebb0` snapshots it,
`52e9d1 -> 52cd90` reads file Properties, and `52e9e0 -> 52c9c0` restores the
preserved subset. The first WORD is copied at `52ebb8/52ebbc` and restored at
`52c9cc/52c9cf`. The file's first Properties WORD is therefore overwritten by the
pre-load WORD. Entry coordinate WORDs `31c/31e` are not part of this restored
subset; the E3 entry-coordinate observation is unchanged.

**Do not bind a Map file's logical year WORD at 58 as the current raw native date.**
Nor does the calendar reset establish it: `4ac240` initializes month 1 and
temporary raw date **2098** at `4ac260`, a separate owner.

`53d470` is a configuration loader and ends at `53d527`. The next function starts
at **`53d530`** and performs the actual date seed: at `53d5ca` it sign-extends the
then-current Properties WORD at `c5cae0`; at `53d5d1` it stores the result in the
raw date DWORD `d623fc`.

The real start action `406410` first calls its prelude `406870` at `406421`, then
calls date seed `53d530` at `406426`. The play action explicitly sets load mode 1
and configuration flag 1. The prelude has mode-dependent native loads (including
`53d470 -> 5351e0`, plus `534660/534410` and `535060` alternatives). On the
successful non-`0x26` branch, `4069ab -> 53d470`, `53d50a -> 5351e0`,
`5352ae -> 52f4d0`, and `52f516/52f51d` directly read the archive's Properties
into actual `c5cae0` using `52cd90`, **without** the Standalone-preservation
wrapper. The later Play start can therefore replace the retained initial year
with a second archive's actual year. The .map tag is real (`815828`), but the
current branch/archive identity must be bound before a supplied file's year can
be claimed as that actual owner. Merely proving the constructor or initial
Standalone read does not close this later start epoch. Fresh-game `5d1400` calls
calendar reset first, then:

* flag nonzero: `5d14cb -> 53d470`, separately `5d14d0 -> 53d530`;
* flag zero: `5d14e4 -> 43abf0` Standalone map load, skipping those two calls.

Other concrete Properties/raw-date writers exist: mission path `42eb04`, mission
setup/copy `42edc9` followed by `42f0ba -> 53d530`, date UI `425423`, and new-map
`53cf9b` (WORD **-500**). A fresh-first-load profile must exclude or qualify these
reachable predecessors; a later Native Restore is outside this profile.

The yearly counter increments when the **resulting native month is 1**:
`4ac7d0 -> 4f8d50` supplies literal 1, `4ac7d5` compares the resulting month,
`4ac7db` skips the update otherwise, and `4ac7e4/4ac7e5` increment/store the raw
DWORD. An authored elapsed World year or month 11-to-0 boundary is insufficient.
The optional global multiplier is reached only if this actual signed raw date
is greater than **-300** (`4c1efd`). A proved earlier date removes that numeric
dependency; an invented or file-derived earlier date does not.

### Runtime fertility and signed region

Ordinary full-archive Standalone map loading is a real recomputation path:
`43abf0 -> 52e7c0` at `43ad9e`, followed by `43ae05 -> 53d100`,
`53d1e4 -> 5355f0`, `5356bc -> 4c1d00`, `4c1d13 -> 4c1c20`.
Archive loading separately reads the saved runtime plane (`52ea84`) and the
construction plane (`52eaa6`); the later recomputation replaces runtime values
for the qualified rectangle. `4c1c20` performs integer computation:

`runtime_BYTE = (floor(55 * construction_BYTE / 100) + modifier_BYTE) mod 256`.

The modifier is the live byte at `84c560`; it is read at `4c1c8a` and the runtime
byte is written at `4c1ccc`. Construction fertility alone is not a live runtime
operand. A valid World binding must carry the actual sealed source construction
byte, the qualified modifier epoch, and the actual post-load derived epoch.

Other recomputation paths exist and must invalidate or be excluded by an explicit
profile: editor-wide refresh at `407a73` (requires both `10de045 == 1` and
`88ec38 == 1`), terrain-edit rectangle at `4b5b2b`, region editor at `5f539d`, and
Native save/preparation at `533fba`. No claim is made for editor/native-save
epochs or imported runtime planes.

The region used by Harvest is a **signed WORD** at `c5cd58` (`4c1ed0`), not an
unsigned fallback. A Standalone map publishes the Header's region low WORD after
Properties restoration (`52eb52`). This is Header **logical DWORD 12 / native
Header+8**, distinct from the coordinate-context index in logical DWORD 8.
The narrow supported source region is **0..2**; negative values (including
`ffff`) and values at least 3 must be refused before table indexing. New-map and
region-editor writers are separate epochs (`53cf03`, `5f5377`). An admitted
immutable Map/geometry/source hash can bind this region; it does not by itself
prove current factor, date, event or FP operands.

### Rice/Cabbage event reduction and mutable event lifetime

Harvest first queries enable via `48e0f0 -> 608620` (BYTE +50), then requires
DWORD +54 equal to 3; the crop selector helper `48e680` reads DWORD +58. The
selector uses an unsigned bound 0..4, with all others returning false. Each of
the five actual selector branches excludes crops **Rice197 and Cabbage198**:

* selector 0 admits no crop above 195;
* selector 1 admits only 199 or 226;
* selector 2 admits no crop above 196;
* selector 3 admits no crop above 194;
* selector 4 admits only 37 or 196.

Therefore the event-derived doubled-base result is **false for Rice197/Cabbage198 for every
possible actual Enable, State and Selector value**. This is a proved numeric
branch reduction by the actual crop identity. It claims neither that those live
globals are zero nor that their reads do not happen. A sealed Rice/Cabbage context
may carry that semantic reduction explicitly without manufacturing Event inputs.

For the remaining crops, a no-command policy does not freeze Events. The actual yearly
caller `4ac904 -> 48eb90` clears/reconsiders enable/state and may automatically
call `48ea40` via mutable eligibility `48e930`. That helper depends on other
actual global owners (`48a340/48a420`, `5177b0/517cc0`); those are not proved
equivalent to authored World population. Actual command `48f820` also writes
selector and state3. The Event constructor clears older controls, but does not
explicitly initialize all three Harvest members; PE fresh storage alone does not
establish their lifetime through yearly updates.

### Smallest honest admission boundary

Closed source facts are the signed region source/index relation, integer
recomputation and its actual Standalone post-load caller, concrete startup
parameter bits, concrete fresh Properties constructor and preservation behavior,
the explicit native date-seed/update rules, and Rice/Cabbage's event-result reduction.

A **current** positive live Harvest context still requires an actual bounded
initialization/start epoch, its raw-date lifetime, a factor/modifier epoch tied to
the current derived fertility, and the independent FP callback-entry proof. A
map's year, a free DTO, passing the executable's constructor constants, elapsed
World time, or absence of user Event commands cannot fill a missing owner. If any
needed owner is unresolved, retain the actual E4 Wait25 trigger and refuse before
the first callback mutation.

### World admission and publication boundary

`World::prepare_grower_harvest_context` accepts only an actual lifetime ID.
It checks the controller policy, current cancellation/retirement, complete
matching map/model context, Farm/field/registered crop, actual retained actor
references and assignment, real arrived owned movement channel, due work,
mark, quantity and signed region. It retains actual tick/slot, road revision,
source/model identity/difficulty, construction fertility and Q/work/C as
observations. None is a substitute for current runtime fertility or a native
date/FP epoch.

All120 cells in the exact square-perimeter1..5 query must pass the existing
shared effective-cell preparation as known ordinary Farm02/Road04/Food-land10
masks. Those qualified source/overlay facts have no800. A ditch, pump, boundary
or unknown/source800 cell refuses, retaining its actual coordinate and the
number of previously checked cells. Only a fully supported query records
`irrigated=false`. The separate derived World wet cache supplies no operand.
The supported source region is the actual Header+8 low signedWORD; preparation
refuses outside0..2. The source reader preserves it without imposing a new
navigation admission rule.

For Rice/Cabbage the result may record only the proved event-doublingfalse
predicate. Other crops retain missing event-input provenance. Current actual
FP/numeric-owner failures are typed individually. An unresolved current domain
never manufactures a usable numeric context. The real work invocation stores
these failures in its bounded actor audit before the existing empty cancellation
clears the assignment/channel and native actor. No work4000, quantity/history,
Raw word, Farm credit or Food grant is published by that refusal. Positive C
and the return/bin lifecycle require separate implementation after full numeric
admission. Controller presence continues to block historical document/writer/
restore/Recovery publication even after the worker has retired.

### Reproducible proof plan for the remaining RC and FP postcondition

The static proof closes the immediate real Actor26 branch. It leaves a
specific numeric FP epoch and FP entry postcondition unobserved. A future
authorized Original run can qualify that bounded frontier without collecting
every possible external exception case. No supplied bytes need modification.

1. Pin the three supplied input SHAs from `verified-normal-entry.json` and the
   actual Windows target's loaded OS/CRT/graphics modules and versions/hashes.
   Use an explicitly provided map/new session; do not search personal saves.
   Record the same main thread, actual map/load epoch and current Actor26 ID.
   Do not edit the process's FPCW, status, tag word, field or Actor values.
2. Use execution hardware breakpoints, at runtime image-base plus these RVAs:
   `0xe26cf` (after mandatory `GetTickCount`), `0xe26e8` (after optional QPC,
   before return), `0xc1dd0` (actual field callback entry), `0xc1e8d` (before
   first `FILD`). These four fit the normal x86 execution-breakpoint budget.
   Hardware breakpoints avoid changing code bytes, including in memory.
3. Log `r`, `rF`, thread ID and a native call stack at each relevant stop. In
   the x86 FP display retain the complete numeric `fpcw`, `fpsw`, `fptw`, TOP
   and ST0..ST7/tag information. Decode PC=`(CW>>8)&3`, RC=`(CW>>10)&3`, masks
   `CW&0x3f`, exception flags `SW&0x3f`, SF=`SW&0x40`, ES=`SW&0x80`, and
   TOP=`(SW>>11)&7`. A required normal empty tag word is `0xffff`; TOP alone
   never establishes emptiness. Keep masked sticky flags distinguishable from
   a pending unmasked exception; do not rewrite them to zero in the report.
4. Admit the current branch only after verifying actual status1/type26,
   valid retained Farm193 and Food field identities/vtables, rawstate12,
   wait1 before the decrement, and harvest mark8000. Record actual profiling
   flag and take the corresponding last timing-return snapshot. Do not force
   profiling off, delete other Actors or treat a different direct-call harness
   as the normal Original Actor callback path.
5. Compare the same-thread FP snapshot at the last timing return, actual
   `4c1dd0` and first `4c1e8d`; the closed integer prefix predicts no FP change.
   At `4c1e8d`, single-step the first FILD, FMUL and FSTP and capture the actual
   binary32 store plus any exception. Capture before/after actual later
   `7650fc` as a separate temporary-CW restore check, not a substitute for the
   earlier FP entry snapshot. Bind numeric field/live-owner snapshots from
   the sibling E5 operand receipt to the same Actor callback and load epoch.
6. If an unexplained CW/tag/status transition appears, add a bounded execution
   or control-write breakpoint around that concrete transition and identify
   its actual module/caller. Follow only that ordinary observed writer chain.
   The source prefix already excludes an intervening Original integer helper
   as the writer. Never infer all-time authority from a finite capture.

`rF` is Microsoft's documented display of all floating-point registers:
[WinDbg r command](https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/r--registers-).
The normal CW/stack calling convention is separately documented:
[Microsoft x86 FP convention](https://learn.microsoft.com/en-us/cpp/cpp/floating-point-coprocessor-and-calling-conventions?view=msvc-170).
These primary references explain the planned observation; they do not supply
the missing current Original values.

An eventual World admission needs either a source-qualified complete normal
epoch with these postconditions or an explicitly bound observed native
context within its proven lifetime. A synthetic World boolean, a host-native
FP read or an arbitrary CW027f constant cannot fill the missing provenance.
