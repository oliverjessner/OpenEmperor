# Original model fields and difficulty: bounded static evidence

OE-02 studies the legally supplied executable and model text read-only. This is
static reverse-engineering evidence, not execution of Emperor or whole-game
parity. Only behavioral observations, addresses and hashes are recorded here;
original bytes, complete disassemblies and analysis outputs remain ignored.
The independently written C++ implementation follows this documented contract.

## Source binding and method

| Source | SHA-256 |
|---|---|
| `Emperor.exe` | `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e` |
| `Model/EmperorBuildingModels.txt` | `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6` |

Addresses below are static virtual addresses in the PE32 image with base
`0x400000`, not RVAs, runtime addresses or file offsets. The existing
`tools/re/pe_string_refs.py` PE-section mapping and local `objdump` independently
verify each inspected instruction against its underlying file bytes. The private
OE-02 checker covers 12 bounded windows, 785 instructions and 2,120 instruction
bytes with zero mismatches. The source file is identical before and after.

For example, the two text-loader transformations can be inspected locally with:

```sh
objdump -d --start-address=0x5d1a55 --stop-address=0x5d1ba6 .local/gog-extracted/app/Emperor.exe
```

The full private repeat command is
`python3 .local/original-economy/oe02/check-exe-units.py`; its report is
`.local/original-economy/oe02/exe-units-evidence.json`. The private script and
original instruction dumps are deliberately not repository dependencies.

## Model loading and access

| Static window | Observation |
|---|---|
| `0x5d1830–0x5d18ef` | Text-model loader and source filename selection. |
| `0x5d1996–0x5d1bef` | Modifier/model loops use 13 Building columns and 24 House columns; modifier tables have five rows. |
| `0x5d1a55–0x5d1a8a` | A Building field is multiplied by its difficulty/field modifier, then divided by 100 with signed truncation toward zero. |
| `0x5d1b7a–0x5d1ba5` | A House field receives its difficulty/field modifier additively, then the result is clamped to −99 through 100. |
| `0x445480–0x445496` | Signed clamp helper used by the House loader. |
| `0x5d16d0–0x5d1825` | Embedded-data path contains corresponding difficulty transforms; no embedded data is imported into OpenEmperor. |
| `0x44cc50–0x44cc95` | Building and House table access use strides 13 and 24. Building field 0 additionally calls `0x44c380`. |
| `0x5d25a0–0x5d25a5` | Building row-count helper. |
| `0x4f8900–0x4f8905` | House count sentinel; the loader subtracts one before its row loop. |
| `0x518b70–0x518b91` | House initialization derives the stored House-model row from `max(original_type − 3, 0)`. |

These observations support difficulty preparation for every field, including
columns labeled unused. The Building transformation uses a signed 32-bit
intermediate product. The House addition also operates on 32-bit values.
OpenEmperor calculates these intermediates in int64 and explicitly refuses
values outside int32 before division/clamping. This is an authored checked
boundary around the observed arithmetic domain; arbitrary edited-model overflow
is not silently wrapped or advertised as original behavior. Generic raw parsing
still retains the entire signed int32 input domain.

The transformed Cost field is a model component. The additional Cost getter
hook prevents claiming that it alone is the final cost of every original player
action. This task does not reconstruct that hook or change OpenEmperor pricing.

The file has 15 named House rows. Source Building types 3–17 correspond to those
ordered rows, as checked against both tables and the initialization expression.
Vacant type 2 shares the engine's lower-row alias; it does not add a sixteenth
House row. The strict named schema conversion returns no independent row for
Vacant or other non-House types. That API choice preserves the distinction
between model row identity and the original initializer's aliasing behavior.

## Desirability field units

`0x44eba2–0x44ebcb` forwards Building fields 1, 2, 3 and 4 through
`0x44ec80–0x44ec8c` to the bounded spread routine at `0x44ecd0`.
Within `0x44ed41–0x44ed70`, field 2 controls the step period in tile/ring units,
and field 3 supplies the signed desirability-score change at that period. The
spread window through `0x44ed84` limits range to ten rings and handles a change
crossing zero. This agrees with the model legend's distinction between
`desirability step (in tiles)` and `desirability step size`.

Consequently `SZE` is not a footprint or a selector permission. This evidence
neither changes World geometry nor implements the spread algorithm in OE-02;
the pure Desirability task follows in OE-03.

## House requirement units and alternatives

The requirement consumer at `0x51a3f0–0x51a650` obtains its thresholds through
the 24-column House getter. Its relevant checks establish:

- Nonzero service requirements activate presence checks. This includes Water
  and the named medical, cultural and religious access requirements. No visit
  lifetime, production cycle or conversion to World ticks follows from this.
- FoodQuality field 8 is compared against a byte-sized food-quality score.
  It is a score threshold, not an amount of Food.
- Goods requirements are compared against signed-short stored amounts. Their
  physical denomination, delivery size and consumption schedule are unresolved.
- Field 13 is read once. At `0x51a616–0x51a628`, meeting the threshold with
  either of the two stock fields at offsets `0x1a` and `0x1c` succeeds. The raw
  legend identifies these alternatives as Bronzeware and Lacquerware. Adding
  the stocks together or subtracting the requirement from both is not proven
  by this acceptance predicate.

The model text also names field 23 as Daoist-or-Buddhist access. It remains one
field; a second religious column must not be invented from the abbreviated
legacy enum spelling. This task introduces no resource stock, delivery,
consumption, service timer or House evolution mutation.

The per-field facts, precise text-line references, five bounded raw golden
values, typed IDs and functional-mapping limits are in
[model-provenance.md](../original-economy/model-provenance.md). Unknown risk,
tax, resource-code, physical-quantity and time units stay unknown.

## OE54 trade metadata boundary

A fresh read-only check uses the same building-model SHA above and the common
bounded model inspector (269 Building rows,15 House rows). Raw named candidates
are Warehouse54 (Cost component40, Employees6), Trading Quay56 (140,10),
Trading Station58 (140,9) and City Gate130 (70,9). Normal Building modifiers are
100%. These columns do not establish complete purchase cost, footprint,
constructibility, a foreign partner, gateway permission or merchant behavior.

`Model/Trade.txt` is400 bytes, SHA-256
`904a7da8ef334b963941b39ca17faeb8136810179a4b52ae14ede2afba0237ed`.
Its named raw values include Clay25, Ceramics75, Land frequency34/capacity800
and Sea frequency4/capacity1200. Currency/quantity normalization, buy/sell
asymmetry, difficulty application and the clock behind those values remain
**UNKNOWN**. OE54's prices6/3, portions4 and finite offers are **AUTHORED**.
Figure labels20 (cart),45 (Traders) and47 (Trader Crew) likewise establish no
actor factory, cargo unit or route. No original trader sprite is admitted from
these labels; the authored caravan reuses an existing curated Pottery-cart
presentation. No original executable was run.

The protected Industrial175 exit-coordinate marker,177/178 river markers and
GateHouse passages retain their existing source contracts. None grants an
original external trade endpoint. The [OE54 contract](../original-economy/oe54-trade.md)
uses a separately purchased, source-geometry-bound authored Warehouse gateway;
original partners, empire routes, sea trade and merchant runtime remain unsupported.

## Additional agriculture and industry candidates

The same source model contains 269 Building records. A bounded census through
the production `ModelData` reader retains each present ID, parsed label and raw
Cost, Employees and Resource columns. The complete census stays private. The
[production evidence guide](../original-economy/oe55-production.md) lists the
selected farm, field, extraction, processing and supporting candidates together
with their current World status and unresolved requirements.

The additional labels include Orchard Shed (29), Fishing Quay (31), Hunter's
Tent (33), Stoneworks (36), Salt Mine (37), Iron Smelter (40), Steel Furnace
(41), Jade Carver's Studio (46), Shu Mint (48), Money Printer (49), Sculpture
Studio (51), Carpenters' Guild (52), Mill (53), Rice Paddy (197), Cabbage Field
(198), Soybean Field (199), Irrigation Ditch (202), Irrigation Pump (203) and
Weaponsmith (226). These are research candidates, not construction or recipe
mappings. The unused row 32 also has positive Cost 50 and Employees 10; positive
values or adjacent IDs do not identify a producer.

Across all 269 records, raw Resource is 0 except Herbalist's Stall (207), where
it is 8. Its meaning remains unresolved. Zero does not establish an input-free
producer, and neither value selects a commodity, quantity, harvest period or
recipe. No additional footprint, parent/field attachment, fertility, hunting,
fishing, seasonal clock, storage or downstream demand follows from these raw
columns. Terrain/resource-marker labels likewise grant no paid extraction or
source-admission authority.

Repeated read-only queries reuse the shared reader and preserve source
provenance. For example:

```sh
cmake --build build --target openemperor-model-inspect
./build/openemperor-model-inspect /path/to/emperor \
  --building 31 --building 197 --building 198 --building 199
```

Generic sparse-ID queries do not expand the named `OriginalRulesModel`
capability set. All nine factory/material mappings documented in the
[luxury-source contract](original-luxury-source-contract.md) already have paid
World adapters; their authored unit recipes and periods remain distinct from
original evidence. The consulted model observations establish no additional
complete original production chain and activate no new World rule or save
schema. No original executable was run for this census.
