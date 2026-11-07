# Original goods consumption and FoodQuality: bounded static contract (OE-06)

This records the independently checked consumption and food-quality paths used
by the isolated, SDL-free `GoodsRules`. The separate
[delivery contract](original-goods-delivery-contract.md) describes goods demand,
delivery eligibility and inventory movement. FoodQuality is a byte attribute;
it is not a seventh goods stock, a food quantity or a new World resource.

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

## Crop admission boundary

The observed House quality blend and six independent goods debits do not identify
a crop's farm, harvest period, yield, food score or season. The current World's
Wheat/Millet scores and productive clocks are explicit authored adapters, not
parameters recovered by this kernel. Further raw Field or Farm model rows
cannot inherit them by analogy. The
[production candidate register](../original-economy/oe55-production.md) records
that research boundary without adding Food stocks, consumption events or a
new World generation.
