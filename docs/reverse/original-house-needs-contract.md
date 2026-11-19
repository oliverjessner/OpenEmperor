# Original House needs and capacity: bounded static evidence

OE-04 reconstructs a pure query over prepared model rows and explicit House
observations. It does not run the original executable, add a World rule, change
House stocks, grant service visits or define a consumption schedule. The separate
[evolution contract](original-house-evolution-contract.md) covers the callers,
type transitions and counters. A successful needs query alone is not an
evolution command.

## Sources and repeatable checks

| Read-only source | SHA-256 |
|---|---|
| `Emperor.exe` | `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e` |
| `Model/EmperorBuildingModels.txt` | `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6` |

The existing PE reader maps each static virtual address through its owning
section to the original file bytes. Addresses below are static VA in the
PE32 image based at `0x400000`; they are neither RVA nor file offsets. The
private read-only check
`python3 .local/original-economy/oe04/check-house-needs-evidence.py` verifies
8 bounded windows, 402 instructions and 1,014 instruction bytes, with seven
additional binding assertions and zero mismatches. Both sources remain byte
identical before and after. Its report is
`.local/original-economy/oe04/exe-house-needs-evidence.json`; original bytes,
disassemblies and the complete model tables remain ignored.

The central needs window can also be inspected independently:

```sh
objdump -d --start-address=0x51a3f0 --stop-address=0x51a651 .local/gog-extracted/app/Emperor.exe
```

The [model field register](../original-economy/model-provenance.md) binds
the 24 columns and 15 ordered labels to the text source. The
[OE-02 model contract](original-model-contract.md) separately establishes
additive difficulty preparation and its signed clamp to −99 through 100.
OE-04 uses those prepared values; it does not treat raw rows as prepared rows.

## Needs predicate

`0x51a3f0–0x51a650` takes its current row from the House's signed-short field
at `+0x16`. A nonzero argument instead obtains a row through virtual `+0x22c`.
This is row selection by the caller; it does not create a sixteenth model row.
The pure requirement checks use the selected row without a footprint or
occupant multiplier.

| Requirement | Prepared field | Acceptance condition |
|---|---:|---|
| Water | 3 | Field is zero, or the House water byte at `+0x39` is nonzero. |
| Music | 5 | Field is zero, or own-info byte `+0x2c` is nonzero. |
| Acrobat | 6 | Field is zero, or own-info byte `+0x2b` is nonzero. |
| Drama | 7 | Field is zero, or own-info byte `+0x2e` is nonzero. |
| Acupuncture | 4 | Field is zero, or own-info byte `+0x2a` is nonzero. |
| Herbalist | 2 | Field is zero, or own-info byte `+0x2d` is nonzero. |
| Ancestor | 21 | Field is zero, or own-info byte `+0x0d` is nonzero. |
| Confucian | 22 | Field is zero, or own-info byte `+0x10` is nonzero. |
| Daoist or Buddhist | 23 | Field is zero, or Daoist own-info byte `+0x0e` or Buddhist byte `+0x0f` is nonzero. |
| Food quality | 8 | The unsigned quality byte at own-info `+0x36` is at least the field value. |
| Hemp | 9 | Signed-short stock at own-info `+0x14` is at least the field value. |
| Ceramics | 10 | Signed-short stock at own-info `+0x16` is at least the field value. |
| Tea | 11 | Signed-short stock at own-info `+0x1e` is at least the field value. |
| Silk | 12 | Signed-short stock at own-info `+0x18` is at least the field value. |
| Bronzeware or Lacquerware | 13 | Either signed-short stock at own-info `+0x1a` or `+0x1c` independently meets the field value. |

The central predicate checks Water, FoodQuality, Music, Acrobat, Drama,
Acupuncture, Herbalist, Ancestor, Confucian, the religion alternative, Ceramics,
Hemp, Tea, the goods alternative, then Silk. It returns false on the first
failed check. A pure diagnostic may report all failed fields while preserving
the same overall Boolean; that does not reproduce an original UI reason order.

Presence activation is **nonzero**, not positive. A prepared service requirement
of −1 still activates its presence check. Conversely, availability is a byte
tested for nonzero; values 128 through 255 remain present. The fork uses signed
integer timer fields and rejects values `<= 0`. That is equivalent only on its
ordinary nonnegative timer domain. An implementation must preserve byte
semantics or explicitly validate its wider observation domain rather than
reinterpret negative signed values as original timer states.

Food quality and goods use inclusive signed threshold comparisons after loading
their observations with the widths above. A negative prepared food threshold
is met by every unsigned quality byte. A negative goods threshold is still a
threshold against the signed stock; it is not an instruction to skip that
field. Equality succeeds, and two individually insufficient alternative stocks
do not succeed by being added together. A single religion field also does not
require both alternative religions.

The query itself contains no population, empty-House, type-range or road-access
guard. Those belong to its callers and must not be inferred from this function.
It only reads stocks and presence; it performs no delivery, consumption, timer
grant or decay. Physical goods units and the production of FoodQuality remain
outside OE-04.

## Supplied-model luxury thresholds

For the model SHA above at Normal difficulty, the first current-row Tea need
is Common10 (Luxurious Apartment); Silk first appears at Elite14 (Lavish
Siheyuan), and the Bronzeware/Lacquerware alternative at Elite15 (Humble
Compound). These are **MODEL_OBSERVED** requirements, not proof that a paid
city reaches or retains those rows. Common10 cannot evolve into the Elite
family: its numeric next model row is not an eligible next-stage demand.

Common10 and Elite14 require FoodQuality50; Elite15 requires70. Reaching
Common10, Elite14 and Elite15 also requires the preceding row's evolution
DES threshold40,50 and60 respectively and the actual service/goods predicates.
Silk or Lacquerware sitting in a Warehouse is therefore neither a House
consumption witness nor evidence of broken supply to a lower House.
The [supplied-model integration check](../testing.md#high-tier-luxury-consumption-agri-mac04)
records actual paid progression separately from these model facts.

## Capacity is a row fact; effective capacity has context

`0x44cc80–0x44cc95` directly returns the prepared signed 32-bit value at
`row × 24 + field`. Field 17 is the model's population-capacity column. It has
no source footprint, House size, population or elite argument. The ordinary
capacity reads inspected below do not multiply it by four or by a footprint
area. The fork's size multiplier belongs to a separate goods-delivery helper;
it is not evidence for scaling capacity or the needs predicate.

| Bounded consumer | Observed model-row selection and surrounding behavior |
|---|---|
| Accounting, `0x4ad405–0x4ad47d` | Enters this capacity branch only if the signed-short House field `+0x24` is positive. Type 11 uses stored model row `+0x16` plus one; other types use the stored row. After field 17 is read, nonzero own-info byte `+0x3c` replaces the effective capacity with the current signed-short occupant count. |
| Admission, `0x4ca1de–0x4ca216` | Type 11 selects literal model row 11; other types use the stored row. An empty-House admission subbranch floors a negative capacity at zero before limiting arriving residents. Independent actor/state guards surround this window. |
| Independent admission, `0x4cb129–0x4cb150` | Again, type 11 selects literal row 11 and other types use the stored row. The local availability difference `capacity − occupants` is floored at zero. |
| Additional admission, `0x4ae08f–0x4ae0e4` | Reads field 17 of the stored row directly and compares the free difference against the arriving count. This window does not establish the admission guards outside it. |
| Population percentage, `0x518810–0x51883c` | Reads field 17 directly as the denominator for `100 × occupants / capacity`, and only divides when capacity is positive. |

The initializer at `0x518b70–0x518b91` stores
`max(original_type − 3, 0)` as the model row. Thus ordinary initialized
EliteUnoccupied type 11 has stored row 8: the accounting substitution selects
row 9, whereas the two admission substitutions select row 11. This distinction
is observed behavior, not a claim about why the original chose those rows.
The eligibility field, own-info override and admission guards are not assigned
new gameplay meanings here.

A row-capacity API can therefore return the prepared signed field without
guessing an area multiplier. Narrow, explicitly named context selectors may
describe the observed row substitutions. A general `capacity(type)` with an
unqualified original-parity claim would be unsupported. Even a context-specific
model lookup is not the complete effective accounting/admission result: the
population override, local floors and independent eligibility checks remain
separate. Modified negative capacity rows stay readable as signed model facts;
the local floor in one consumer is not authority to globally clamp all getters.

## Scope of the result

OE-05's independently bound religion producers clarify the individual names of
the two alternative bytes: Daoist is `+0x0e`, Buddhist is `+0x0f`. This naming
clarification leaves the commutative OR predicate and OE-04 behavior unchanged;
see the [timer contract](original-service-timer-contract.md).

These observations support deterministic, SDL-free needs and row-capacity
queries across the 15 checked source rows. They establish no runtime mapping
to the existing World House levels 0 through 2. Service lifetimes, walker
dispatch, foods and goods delivery, consumption, immigration, House merging,
actual evolution scheduling, saves and player acceptance remain separate work.
