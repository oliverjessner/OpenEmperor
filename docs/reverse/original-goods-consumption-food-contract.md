# Original goods consumption and FoodQuality: bounded static contract (OE-06)

This records the independently checked consumption and food-quality paths used
by the isolated, SDL-free `GoodsRules`. The separate
[delivery contract](original-goods-delivery-contract.md) describes goods demand,
delivery eligibility and inventory movement. FoodQuality is a byte attribute;
it is not a seventh goods stock, a food quantity or a new World resource.
The further reconstruction below adds separate pure `FoodRules` operations for
the normal raw House Food event, score-to-grade mapping and a bounded Mill grade
choice. These operations do not activate an Original Food chain in `World`.

## Sources and reproduction

**EXE-OBSERVED:** The read-only local `Emperor.exe` has SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The accompanying model text has SHA-256
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
Addresses below are static virtual addresses with image base `0x400000`, not
RVAs or physical file offsets. The executable was not run.

**FORK-STUDY:** `HouseEvolution.cpp` at fork commit
`d861d2912337c09751cb68196f85d5a30752726e` proposes six independent goods
subtractions and a FoodQuality blend with a parameter named `food_need`.
The fork is study material. Original model arrays, instruction bytes and full
disassembly remain private and are not distributed in Git.

With the same installation and private research artifacts, reproduce the
read-only checks using:

```sh
python3 .local/original-economy/oe06/consumption-food-check-evidence.py
```

The private `consumption-food-evidence.json` checks **11 bounded windows,
465 instructions and 1,361 instruction bytes**, 40 selected semantic
instruction assertions, five pointer bindings and all four binary32 ratio
thresholds. It also checks exact rational rounding boundaries and three
counterexamples to a naive binary64-to-binary32 division in the widened API
domain. It reports zero byte mismatches and unchanged executable/model bytes.
This establishes static contracts, not an original-game run or human acceptance.

## Consumption uses the current row and six independent stocks

The concrete House own-info accessor `0x416b50`, published at virtual `+0x1e4`,
returns House `+0xc8`. Goods stocks are signed words relative to that own-info:

| Goods | Own-info offset | Prepared House model field |
|---|---:|---|
| Hemp | `+0x14` | Hemp, field 9 |
| Ceramics | `+0x16` | Ceramics, field 10 |
| Tea | `+0x1e` | Tea, field 11 |
| Silk | `+0x18` | Silk, field 12 |
| Bronzeware | `+0x1a` | Bronzeware-or-Lacquerware, field 13 |
| Lacquerware | `+0x1c` | The same field 13 |

**EXE-OBSERVED:** `0x519f90–0x51a140`, bound at House virtual `+0xa8`,
returns immediately when the signed-word occupant count equals zero. This is
an equality test: a negative raw occupant observation enters the local path.
The callback reads the current stored House row from signed word `+0x16`,
fetches fields 9, 11, 10, 12 and 13, and performs no size or population
multiplier. Nonpositive prepared model amounts leave the corresponding stock
unchanged. Each positive amount floors its own stock at zero when the stock is
smaller, otherwise subtracts that amount.

The Bronze branch at `0x51a0cb–0x51a102` is followed unconditionally by the
Lacquer branch at `0x51a103–0x51a140`. Both use the same field-13 amount and
their own stock address. **Both stocks are independently consumed.** There is
no first-available choice, shared consumption budget or stock-priority branch.
The [needs predicate](original-house-needs-contract.md) accepts Bronze **or**
Lacquer as independently sufficient for evolution; that OR does not change the
separate consumption operation. The fork's use of the same model field for
both goods is therefore supported by this original path.

The broader House callback at `0x518ef0` first checks status 1/3 and House type
2–17. After its evolution work, `0x51903b–0x519053` dispatches consumption only
when selector `0xc82ef0` equals 0 or 7. This establishes two selected events in
that callback. It does not establish months, a whole calendar or a mapping to
OpenEmperor ticks. `consume_goods` deliberately exposes one explicit operation.

**AUTHORED SAFE BOUNDARY:** `HouseState` carries a canonical typed House row
rather than arbitrary stored-row memory. Every incoming goods stock must be
nonnegative; an invalid House identity or negative inventory is rejected before
any operation, including a zero-occupant no-op. Nonpositive edited model amounts
consume nothing. Signed occupant equality is preserved; the pure API grants no
simulation admission from a negative population. The result reports the amount
removed from each stock, so original stock equals resulting stock plus consumed
quantity. It creates no replacement goods and changes no other House field.

## FoodQuality's denominator is previous food stock

**EXE-OBSERVED:** The food prelude in `0x5437b0–0x543902` captures the source
score from getter `0x515770`, an int32 field at source `+0x180`. It resolves a
House food quantity through virtual `+0x21c` at `0x543850` and saves that value
before transferring food. The checked House vtable binds this getter to
`0x518da0`: when House byte `+0x09` is nonzero, it returns signed word House
`+0xda`, equivalent to own-info `+0x12`. Otherwise it returns zero.

The blend at `0x54393d` uses **that pre-delivery food stock** as its denominator.
It does not use the model's FoodQuality threshold, a required quantity or the
House population. The separate population-food minimum/maximum helper is
virtual `+0x228`, bound to `0x51a3a0`; it does not supply this denominator.
The fork's parameter name `food_need` obscures this distinction.

FoodQuality itself is unsigned own-info byte `+0x36` (House `+0xfe`). The
independent quality getter `0x518dc0`, virtual `+0x220`, confirms this byte
interpretation. Source score storage and setter `0x545140` are int32, so this
research does not prove that every arbitrary historical source score fits an
unsigned byte. Market score production and complete food inventories are not
reconstructed by `GoodsRules`.

## Observed blend and explicit safe differences

**EXE-OBSERVED:** The source callback has an outer food-eligibility condition;
elite delivery also requires a source grade of at least 3 using mapper
`0x545100`. The standalone blend receives an already resolved eligible event
and does not reproduce these actor/source gates.

Within that path, `0x54390f–0x543934` first checks whether the source score is
positive and strictly higher than the House quality. If so, it immediately
assigns the source score's low byte, even when the food quantity transferred in
this callback is zero. Otherwise a nonpositive transferred quantity preserves
the old quality. A positive transfer takes the following path:

1. If previous food stock is nonpositive, use ratio 10.
2. Otherwise divide actual transferred food by previous food stock, then
   explicitly store and reload the quotient as binary32.
3. Test strict greater-than thresholds in descending order. Equality proceeds
   to the next branch. Compute the selected weighted average with integer
   division toward zero, then assign its low byte.

Let `q` be previous House quality, `m` source score and `r` the rounded ratio:

| Selected condition | Result before byte assignment |
|---|---|
| `r > 3.0F` | `(q + 3*m) / 4` |
| Otherwise `r > 2.0F` | `(q + 2*m) / 3` |
| Otherwise `r > 0.5F` | `(q + m) / 2` |
| Otherwise `r > 0.33F` | `(m + 2*q) / 3` |
| Otherwise | `(m + 3*q) / 4` |

All four original comparisons use single-precision memory operands. In
particular, `0.33F` is the binary32 value approximately `0.33000001311302185`,
not the exact rational one third or the fork's binary64 decimal threshold.
The weighted-average stage uses integer arithmetic; it has no final x87
float-to-integer rounding step.

**AUTHORED SAFE BOUNDARIES:** The independent API takes two byte qualities and
nonnegative int32 quantities. It rejects negative quantities, and **zero actual
delivery always preserves quality**, including a higher source score. This
explicit event boundary differs from the observed early-improvement branch and
the fork. It keeps a source visit with no delivered food from granting improved
quality in this isolated rule. No claim of complete callback parity is made.

For positive deliveries, the numerical projection explicitly chooses binary32
round-to-nearest with ties to even. It does not emulate an arbitrary historical
x87 control word. Exact int64 cross-products against binary32 rounding
midpoints select the branches, independently of the host floating-point mode.
This also handles widened int32 observations deterministically: a direct
binary64 quotient cast to float can double-round near a midpoint. The original
House food stock is signed 16-bit; the widened API deliberately accepts larger
nonnegative observations for overflow and boundary testing. Byte inputs keep
every weighted result in range without wrapping, and FoodQuality blending
neither moves nor manufactures inventory.

## Scope limits

Goods source selection, market supply, source-score generation, food diversity,
the complete food stock layout, actor movement and arrival, monthly cadence and
the original scheduler's calendar remain outside this kernel. There is no
World, save, UI, renderer or default-rules integration. Synthetic numerical
checks and static instruction evidence remain separate from original runtime
parity and human gameplay acceptance.

## Further reconstruction: normal raw Food consumption

**EXE_OBSERVED:** the further read-only receipt
`.local/agri-original-followup/food/verified-food-chain.json` checks 93 selected
source/arithmetic assertions across 59 private bounded windows,
13,299 instruction lines and 35,466 instruction bytes, including overlapping
captures. The byte checker verifies all captured instruction bytes; only the
specific asserted instruction addresses and concrete pointer bindings establish
the semantic claims below. Exploratory capture starts are not independent
function-boundary proofs. The executable, model text and `FarmConfig.txt`
retain their original hashes, sizes, modes and modification times. The original
executable was not run and no personal save was read. Reproduce this private
receipt with:

```sh
python3 .local/agri-original-followup/food/verify.py
```

The actual Food sink is the House-manager operation `0x518690`, rather than the
six-goods callback `0x519f90`. It enumerates the manager's existing Houses and
only enters the per-House path for exact raw status byte `+0x04 == 1`. It reads
the current stored row and prepared field8, whose typed identity is
`HouseField::FoodQuality`. That field is only a positive/nonpositive gate; its
value does not become a quantity to consume.

The population operand is signed word House `+0x20`. Helper `0x503e20` returns
constant25. The actual call `0x5186f6 → 0x408b80` computes signed integer
truncation of `population × 25 / 100`. In the supported nonnegative physical
domain this is `floor(population / 4)`. No House-size or difficulty multiplier
appears in this bounded path.

Normal consumption requires raw global `0xc5cda0 == 0` and positive field8.
It subtracts the demand from signed own-info word `+0x12` when enough Food is
present; otherwise it removes the remaining stock and floors it to zero.
The final checks `0x518794–0x5187bf` retain quality for positive remaining Food
and set own-info byte `+0x36` to zero when stock is empty. This empty-stock check
also runs with zero demand: an eligible empty House with population0–3 loses
its retained quality. A status or nonpositive-field gate skips that reset.

The separate nonzero-global branch `0x518709–0x518725` overwrites stock with
the computed demand and quality with20. The provenance and activation rules of
that bypass are not closed. `FoodRules` explicitly refuses a nonzero bypass
input, including otherwise gated events; it never reproduces that resource grant.

The scheduler call is concrete: `0x4ac650` increments the raw subslot and the
comparison at `0x4ac6c4` skips the outer-boundary body while the value is at most15.
On overflow, `0x4ac760` invokes `0x518690` before the later outer-slot increment
and subslot reset. The separately proved scheduler has51 phases and16 subslots.
This establishes one normal Food-event opportunity at an outer boundary. It
does not establish named months, wall-clock seconds or a new World tick mapping.

**INDEPENDENT SAFE BOUNDARY:** `consume_raw_house_food` accepts caller-resolved
status, population, stock, quality, the current-row FoodQuality requirement and
the actual raw bypass observation. Population and stock are limited to0–32767;
invalid inputs are rejected before no-op gates. Negative native observations and
WORD wrap are unsupported. The result reports actual removed stock, preserves
population/status and satisfies `previous stock = remaining stock + removed`.
It does not inspect a model, choose a House, schedule an event or move a World
Food portion. The earlier `GoodsRules` six-stock and quality-blend contracts,
including their explicitly safe differences, remain unchanged.

## Further reconstruction: Mill, Food Shop and Dinners grade

**EXE_OBSERVED:** there is a concrete additional source stage on the inspected
native recipe path. Storage factory `0x5d3580` reaches identity helper `0x418d60`,
which compares the actual argument to53, then calls constructor `0x554d40` on
the matching branch. That constructor installs vtable `0x7b72c8`. Its recipe
slot `+0x2e4` binds `0x555330`, grade availability `+0x2e0` binds `0x5558d0`,
base availability `+0x2e8` binds `0x555e40`, ingredient builder `+0x2ec` binds
`0x555410` and base ingredient selection `+0x2f0` binds `0x5557d0`. Model53's
verified name is Mill; neither the name nor RTTI supplies these bindings alone.

Market constructor `0x543450` distinguishes actual model60 from its other branch
at `0x54348e`. The Grand Market branch stores fresh upper grade5; the other
branch stores upper grade3. Both store lower grade1. The DWORD setters are
`0x545150/0x545160`, with actual getters `0x515780/0x546c60` for `+0x184/+0x188`.
The planner `0x5464e0` checks source virtual `+0xc8(53)`, then passes those
current grade bounds and a raw requested amount to source virtual `+0x2e4` at
`0x5465c3`. Complete later policy/configuration/restore writers remain open;
fresh constructor values must not be imposed on all historical observations.

The grade-choice arithmetic in `0x555330` is independently closed for supplied
nonnegative availability values and valid grade bounds:

1. Query availability for each candidate grade, then inspect grades in ascending
   order. The first nonzero availability is a fallback.
2. A higher grade replaces the retained choice only when its availability is
   **strictly greater** than signed truncation of `raw request / 3`. Equality
   does not replace the fallback.
3. Cap the selected raw budget to the minimum of selected availability, requested
   amount and600. With no available grade, return grade0.
4. A selected grade invokes the ingredient builder, even when request and budget
   are zero. The callback ignores the builder return and returns the selected
   grade. This return is not proof that any ingredient moved.

`select_food_recipe_grade` exposes only that choice over five explicitly
caller-resolved nonnegative int32 availability values, bounds
`1 <= lower <= upper <= 5` and a nonnegative int32 request. It returns grade and
raw budget, without an ingredient array, stock mutation or actor. Reversed or
out-of-range bounds and negative observations are rejected. Native availability
remains a separate operation; this component does not derive those five values
from ingredient stocks. The separate bounded request builder below implements
its own checked output arithmetic.

Helper `0x555f70` admits base ingredients1–7 and excludes Salt8 and Spices9 from
that count. These are distinct verified commodity identities. Planner
`0x5466fc–0x54670b` debits selected Salt/Spices amounts directly through source
virtual `+0x298`; it does not spawn their own assistant in that branch. Selected
base ingredients use `0x546960`, which debits `100 × requested assistant count`
before attempting Actor25 allocation. That helper normalizes Food category1–9
to Dinners28. The local failed-allocation behavior is described below; complete
upstream admission and exceptional cargo disposition remain separate gates.

### Bounded ingredient requests and failure outputs

**EXE_OBSERVED:** the independent builder receipt
`.local/agri-original-followup/recipe-builder/verified-recipe-builder.json`
checks90 source/arithmetic assertions in16 bounded windows,1,282 instruction
lines and3,397 instruction bytes, with all three source files unchanged. Its
13 independent literal arithmetic witnesses include each grade, supplement
priority, partial requests, zero requests and the opaque return value. Run:

```sh
python3 .local/agri-original-followup/recipe-builder/verify.py
```

These inputs are nine **aggregated commodity quantities**, resource1 through
Spices9. They are not nine storage bins. The actual Mill configuration is
constructed at `0x554dc0`, retained at `+0x150` and queried through its slot0.
Its concrete vtable `0x7b75c0` binds slot0 to `0x4e1d00`, which returns8. Physical
bin capacities and their save/load histories are distinct from aggregate query
inputs.

For one base-allocation attempt, `0x5557d0` receives a requested distinct count
and raw budget. With the inspected caller's budget0–600, each selected base
receives `100 × floor(floor(budget / distinct count) / 100)` units. The helper
clears six output resource IDs to native sentinel−1 and six amounts to0, then
scans resource IDs1–7 in ascending order. Each selected resource must have
enough current aggregate stock; duplicate IDs are excluded. This builder does
not read the live base-veto array consulted by native availability. The pure
builder consequently has no invented veto input.

If enough distinct bases are found, the helper returns their summed request.
Otherwise it returns0 and retains any partial entries already written. Even a
zero per-base amount can retain positive resource IDs with zero amounts. A
subsequent fallback attempt clears and replaces the previous arrays.

The checked grade branches try these alternatives in the shown order:

| Grade | First attempt | Fallbacks |
|---|---|---|
| 1 | One base | None |
| 2 | Two bases | One base plus Salt, otherwise Spices |
| 3 | Three bases | Two bases plus Salt, otherwise Spices; then one base plus both |
| 4 | Four bases | Three bases plus Salt, otherwise Spices; then two bases plus both |
| 5 | Four bases plus Spices | Three bases plus both Salt and Spices |

Salt requests are `20 × floor(base total / 100)` and Spices requests are
`10 × floor(base total / 100)`. A four-base-plus-Salt-only grade5 branch is
absent from this checked function. Base-only attempts return their raw base
total. Supplement branches often leave the Salt arithmetic intermediate in
the return register, including when only Spices were requested or when a
supplement condition failed. Thus that return is neither a produced quantity
nor a success flag. With a zero request, the outer builder skips every output
write and returns the last queried Spices stock. The actual caller ignores
this return value.

For example, grade5/request600 with four bases stocked600 each, Salt120 and
Spices0 fails both legal supplement alternatives. Its final arrays still
contain three200-unit base requests and the opaque return is120. Availability
and caller consistency must be established separately before interpreting any
such retained array as a complete recipe.

`build_food_recipe_requests` implements this bounded request-output arithmetic
over freshly zeroed planner arrays. Aggregate stocks support the nonnegative
signed-DWORD domain0–`INT32_MAX`; they are not narrowed to native WORD stock.
Generated requests remain bounded by the proved caller's0–600 budget. The API
preserves untouched0 IDs, cleared−1 IDs, zero/partial entries, source order and
the explicitly opaque `raw_return_value`. It neither debits stock nor creates
an assistant, Food amount or World recipe authority.

### Availability from actual aggregate quantities and live base vetoes

**EXE_OBSERVED:** fresh private verifier
`.local/agri-original-followup/recipe-availability/verify_availability.py`
produces `verified-recipe-availability.json`: 117 source/arithmetic assertions,
19 byte-checked windows, 1,234 decoded instruction lines and 3,152 instruction
bytes including overlaps. It also checks 24 independent literal stock/grade
vectors. The EXE, BuildingModels and FarmConfig retain their SHA-256, size, mode
and modification time. No Original execution or personal-save inspection occurs.

The actual Mill availability slot `+0x2e0→0x5558d0` is reached by selector
`0x55535f`. Its quantity slot `+0x264→0x5d4a60` sums matching nonempty physical
bins, using DWORD resource `+0x04` and quantity `+0x08`. Mill construction binds
eight physical bins, initially empty with capacity 400 each. Nine recipe inputs
are aggregate resource categories, not nine invented physical bins or a new
ledger. Complete later capacity/configuration and aggregation-overflow histories
remain external to the pure query.

Base classifier `0x555f70→0x5db4c0` admits exactly resources 1–7 and excludes
Salt 8 and Spices 9. Base availability `0x555e40` additionally requires the
current resource DWORD at `0x1312098 + 4 × resource` to be zero. A nonzero value
skips that base. Its semantic owner, later writers and restore binding remain
external; the API therefore requires seven explicit resolved veto flags.
Salt/Spices availability queries do not apply those base vetoes.

For a requested distinct base count N from 1 through 4, the native helper chooses
the N largest positive, unvetoed aggregate quantities with distinct resource
IDs. Equal quantities preserve the first ascending resource identity. It returns
zero when fewer than N exist; otherwise let
`B(N) = N × min(10000, Nth-largest selected quantity)`.
The initial minimum at `0x555ef8` is the source's 10000 cap. Base availability
does not round quantities to 100-unit blocks. Counts beyond four exceed the
observed four selection slots and are outside this qualified operation.

The five availability cases have the following ordered structure. A candidate
can replace the retained best amount only when larger, and the final result is
zero below 100 raw units.

| Grade | Initial best | Single-supplement candidate | Both-supplement candidate |
|---|---|---|---|
| 1 | B(1) | None | None |
| 2 | B(2) | B(1), Salt then Spices | None |
| 3 | B(3) | B(2), Salt then Spices | B(1) |
| 4 | B(4) | B(3), Salt then Spices | B(2) |
| 5 | 0 | B(4), Spices only | B(3) |

For candidate C, full Salt supply means at least `20 × floor(C / 100)` units;
full Spices supply means at least `10 × floor(C / 100)`. A full-supply branch
retains C without an additional per-base minimum. A partial Salt branch
requires at least `20 × N` Salt and derives `100 × floor(Salt / 20)`; a partial
Spices branch requires at least `10 × N` Spices and derives
`100 × floor(Spices / 10)`. Partial results must reach `100 × N`. For both
supplements, full supply retains C; otherwise both minima must pass and the
smaller of those two derived caps must reach `100 × N`.

The single-supplement path has an essential ordering detail. When partial Salt
clips C, the following Spices test reuses that clipped candidate at
`0x555996`, rather than independently reconsidering the original base amount.
With one base stocked 1000 and Spices 100, Salt 0 gives grade2 availability
1000, while Salt 20 gives only 100. Grades 3 and 4 retain the same candidate
reuse. Grade 5 has no four-base-plus-Salt-only branch.

`food_recipe_grade_availability` expresses one requested grade 1–5 from nine
nonnegative signed-DWORD aggregates and seven explicit base vetoes. It preserves
the source order and does not invoke the request builder. Reached supplement-cap
multiplication overflow is refused as an API support boundary, without claiming
that Original clamps or refuses its DWORD wrap. For example, the last safe
Spices cap input is 214748369 and the next block at 214748370 overflows; the
corresponding Salt boundary is 429496739/429496740. Large stocks on full-supply
or unqueried paths remain admitted because their overflow calculation is not
reached.

The independent tests include all 128 base-veto combinations, the ordered Salt
counterexample, grade5 alternatives and exact overflow boundaries. Two bases
stocked 50 each give grade2 availability 100. Selection can consequently choose
grade2/budget100, yet the actual request builder's rounding and fallbacks leave
no ingredient amount. Availability, chosen grade, ingredient output, actual
source debit and physical arrival remain separate facts. This new query creates
no Food, cargo, purchase, World stock or save authority.

### Actual debits and assistant-allocation failure

**EXE_OBSERVED:** Mill stock-removal slot `+0x298` binds `0x555260`, which calls
`0x5d50c0`. That storage helper removes actual quantities from matching bins and
returns the **unremoved residual request** at `0x5d524c`. This differs from the
Cart target's accepted-count receipt. The Mill history operation subtracts
that residual from its request to record actual removed stock.

For a positive whole100-unit base request and unchanged sufficient source
stock, helper `0x5467a0` chooses exactly one assistant per100 requested units.
Then `0x546960` requests that full source debit before the first Actor25
allocation and does not inspect its residual. At the first allocation failure,
`0x5469fd` branches to `0x546aa5`, returns the previous linked actor ID and exits.
There is no local stock credit/refund on that branch. Planner
`0x546644–0x546658` nevertheless adds100 times the original requested assistant
count to its incoming counter. This is a concrete local loss/overcommit path;
whether upstream source/actor admission prevents it in a particular native
session is not proved. It must not be silently translated into successful
World production, delivery, stock creation or an invented refund rule.

The inspected native graph has these concrete payload and owner bindings:

| Boundary | Checked observation |
|---|---|
| Mill planner to assistant helper | Recipe return retained at `0x5465cd`, passed as the separate grade argument at `0x54663b` |
| Actor25 payload | `0x546a30` writes grade byte `+0x13`; `0x546a1e` writes normalized resource byte `+0x88`; word `+0x72` links actors |
| Actor25 non-storage receipt branch | `0x4d2b04–0x4d2b15` passes unsigned grade, category and literal raw100 to target virtual `+0x260` |
| Actual Food Shop identity | Descriptor `0x8572e8` contains model66 and Dinners28; generic Shop constructor installs `0x7b6c5c`, whose `+0x260` binds `0x541760` |
| Shop to parent Market | Creation/change writes parent ID and slot index at `0x540ef1/0x540efb`; resolver `0x490700` resolves the ID through the building manager; `0x541760` queries the parent's actual slot |
| Market to House | `0x5438d7` credits the selected raw Dinners amount to House signed-word Food stock; `0x5438e4` debits that same amount through the actual Market removal slot |

The Actor25 receipt branch depends on linked-leader status and separate veto
flags, followed by the storage/non-storage target predicate. Its complete
physical movement, arrival, failed route and retirement machine is not closed.
The payload/receipt binding therefore does not prove an ordinary successful
arrival. Nor does this inspected recipe path establish that every native Farm
cart must use a Mill: the complete Cart destination-priority fallbacks remain
an independent gate.

The further bounded return proof distinguishes movement outcomes rather than
equating retirement with arrival. Actual Actor24 state6 invokes movement
`0x4e47a0`, then reaches its source ingredient-request dispatcher
`0x4d1976→0x4d19ed→0x4e3c70` only on result8. That dispatcher reaches
`0x5463a0→0x5464e0`. The subsequent return setup selects state7 and the
retained origin coordinates. In state7, result8 **or10** sets actor status2;
result9 clears the route and retries. Shared movement `0x4e8b40` can produce10
when no positive route remains and the actual coordinates differ from target,
whereas8 follows the retained route index reaching its length. Complete route
admission and consistency remain external to these observations.

The Actor25 non-storage receipt branch checks its immediate predecessor's
status or separate vetoes and has no local direct comparison of its own
coordinates with the target before the virtual receipt. Helper `0x4e44e0`
walks previous-actor links until actual type24, bounded at ten links; it is
not a grade getter. Missing-link replacement remains exceptional. These facts
leave successful physical arrival distinct from failed-return retirement and
linked status. The private receipt
`.local/agri-original-followup/food-arrival/verified-food-arrival.json` passes46
assertions over18 byte-checked windows,3,231 lines and9,695 instruction bytes,
with all three pinned inputs unchanged and no Original execution.

Score consumer `0x541760` combines the retained Market score with a contribution
based on `20 × supplied grade`. Its full exceptional overflow/acceptance path
and rounding environment are not reconstructed here. The pure
`food_grade_from_raw_score` separately implements the exact signed-int32 mapper
`0x545100`:

| Raw score | Grade |
|---|---:|
| At most0 | 0 |
| 1–29 | 1 |
| 30–49 | 2 |
| 50–69 | 3 |
| 70–89 | 4 |
| At least90 | 5 |

This is a score-to-grade query, not an ingredient recipe or a crop's intrinsic
quality. In particular, the existing authored Wheat20/Millet80 adapter is not
established as an Original crop-quality rule by these observations. There is
still no raw100/raw400 to OpenEmperor Food-portion conversion, new inventory,
World production/transport authority, schema change or automatic migration.

## Separately selected authored World processing

Rule34/policy3/schema50 (`authored-food-processing-v1`) uses these observations
as bounded research, with its own explicit portion/work/transport contract.
[Food processing](../original-economy/agriculture.md#food-processing-agri-mac09)
consumes1–4 distinct available base portions and produces the same count of
grade-tagged Dinners after20 staffed ticks per input. These are authored units
and times. The score mapper maps the chosen scores20/40/60/80 to grades1–4;
this does not derive those scores or quantities from a native crop. Grade5 is
unavailable without a proved/implemented Salt/Spices origin. The debit-before-
allocation loss path above is not reproduced: the World first admits its
bounded job/output reservation, then debits actual inputs once atomically.

**EXE_OBSERVED:** a read-only inspection of the known registered-side table at
`0x823598 + 24 × model` in the pinned EXE finds side DWORD5 for Mill53 at
`0x823a90` and2 for FoodShop66 at `0x823bc8`; Warehouse54 gives3 and Market59/60
both give1 as controls. These are static registered geometry facts, not proof
of complete constructor/slot/occupancy behavior. The separately selected World
binds full5×5/2×2 paid footprints to them. The source model at Normal supplies
cost85/employees16 for Mill and16/4 for FoodShop; ordinary difficulty arithmetic
remains source-bound, rather than hardcoding those prices into the runtime.
No original executable was run. Native parent-slot ownership remains distinct:
the authored paid Shop has its sole physical account and an explicit stable
paid Market binding, with no mirrored stock or invisible parent credit.

## Crop admission boundary

The observed House quality blend and six independent goods debits do not identify
a crop's farm, harvest period, yield, food score or season. The current World's
Wheat/Millet scores and productive clocks are explicit authored adapters, not
parameters recovered by this kernel. Further raw Field or Farm model rows
cannot inherit them by analogy. The
[production candidate register](../original-economy/oe55-production.md) records
that research boundary without adding Food stocks, consumption events or a
new World generation.
