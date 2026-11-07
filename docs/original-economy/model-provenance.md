# Model provenance register v1 (OE-02)

OE-16 additionally names `ModelBuilding::HempFarm = 192` and
`ModelBuilding::HempField = 194`: M1 lines276/278 identify `Hemp Farm` and
`Hemp Field`. Their raw Cost/Employees pairs are40/18 and4/0. These are
`RAW MODEL FACT` inputs to the existing checked difficulty arithmetic.
The explicit active preparation validates both named rows; historical
seven-row preparation remains unchanged. Funds conversion, field ownership,
production timing and stock bins are separately documented authored rules in
the [runtime contracts](architecture-map.md). Resource0 does not prove a
commodity code, recipe or production period.

This register identifies file columns and ordered rows. It does not make those
identifiers World object identities, activate Rule7, or establish original-game
runtime parity. The reader remains independent of World, navigation and saves.

## Sources and evidence vocabulary

- **M1, local original model:** `Model/EmperorBuildingModels.txt`, read from the
  local installation under `.local/gog-extracted/app/`; 34,218 bytes, SHA-256
  `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
  Line numbers below are one-based after splitting the unmodified CRLF file.
  A different valid model is accepted by the reader with its own actual hash;
  the five pinned golden assertions apply only to this identified revision.
- **F1, inspected fork:** `amscova/OpenEmperor`, `windows-port` commit
  `d861d2912337c09751cb68196f85d5a30752726e`; independently inspected local
  snapshot and pinned [ModelData source][fork-model],
  [HouseEvolution source][fork-house] and [OriginalHousingView][fork-view].
  A fact about this C++ implementation is not an observed EXE fact.
- **E1, pinned original EXE:** SHA-256
  `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
  Static virtual addresses below belong to image base `0x400000`; they are
  not file offsets or RVAs. Read-only instruction-byte checks and bounded
  control-flow analysis are summarized in the
  [original model contract](../reverse/original-model-contract.md).
  Private instruction extracts remain under `.local/original-economy/oe02/`.
  This is static analysis; OE-02 did not execute the original game.
- **P1, current production:** `src/simulation/World.h`, `World.cpp`,
  `WorldDesirability.cpp`, `WorldWater.cpp` and `WorldHealth.cpp`. These contain
  authored object kinds, paid commands and their actual existing behavior.

| Status | Meaning in this version |
|---|---|
| `verified original` | Directly identified original-file label, index, row or selected value (`RAW MODEL FACT`); no further behavior is implied. |
| `reverse-engineered` | Mechanism supported by pinned original EXE static analysis; distinct from a live original-game observation. |
| `inferred` | Proposed correspondence or interpretation; the stated evidence does not establish original runtime identity. |
| `authored fallback` | Explicit OpenEmperor behavior or tooling contract, separately versioned from original evidence. |
| `unknown` | The consulted sources do not establish the mechanism, unit or identity. |

Each row below separates verified labels from unresolved units. `Unknown` units
must not be silently converted to seconds, ticks, goods batches or percentages.
The field names identify columns, rather than defining their future consumers.

## Difficulty binding and transformations

M1 has five difficulty rows in the same order for both tables: index0 Very Easy,
1 Easy, 2 Normal, 3 Hard, 4 Very Hard. Building labels/IDs occur at lines71–75;
House labels occur at lines356–360. This order and the presence of modifier
values are `verified original`. A mismatching label/ID is rejected by the
reader; invalid typed difficulty values are rejected rather than clamped.

For **every Building field**, E1 `0x5d1a55–0x5d1a88` computes
`base * modifier / 100`, with signed integer division truncating toward zero.
For **every House field**, E1 `0x5d1b7a–0x5d1ba3` computes
`clamp(base + modifier, -99, 100)` through signed clamp helper `0x445480`.
The loops cover all13/all24 columns respectively. This is a
**`reverse-engineered` original transformation**, independently supporting
F1 `ModelData.cpp:65–78`; it is no longer only a fork hypothesis. The original
Building multiplication and House addition use32-bit intermediates. The schema
adapter computes product/sum in int64 and rejects an intermediate outside int32
with `std::overflow_error` before division or clamping. This explicit checked
boundary is authored; arbitrary edited-model wraparound is not silently
emulated. Full-int32 raw access remains unchanged and free of implicit scaling.

Consequently the difficulty entry for each Building row below is **E1 signed
percentage**, and for each House row it is **E1 additive/signed clamp**. These
entries apply to every listed field, including unused fields. Building getter
`0x44cc50` indexes13 fields per type; House getter `0x44cc80` indexes24 per row.
Cost field0 additionally passes through `0x44c380`: a transformed model Cost is
not established as the complete price of every construction action. No price,
tax, production or risk timing is introduced into World by this register.

## Building fields: 13 fixed zero-based indices

All names/indices are `verified original` from M1's a–m legend, lines15–28.
Units qualified as unknown remain `unknown` even though the label is verified.

| Index | Typed field | Meaning and supported unit | M1 line |
|---:|---|---|---:|
| 0 | `BuildingField::Cost` | Structure cost, or cost per tile for chained structures such as walls. Model price component; E1's separate Cost hook means it is not the complete general purchase formula. Currency denomination and further charging logic unknown. | 16 |
| 1 | `BuildingField::Desirability` | Initial desirability score; E1 confirms it is passed to the spread routine. | 17 |
| 2 | `BuildingField::DesirabilityStep` | Desirability step distance, explicitly in tiles; E1 confirms ring/tile period. | 18 |
| 3 | `BuildingField::DesirabilityStepSize` | Signed desirability change at each step period, independently confirmed by E1; not a building footprint. | 19 |
| 4 | `BuildingField::DesirabilityRange` | Maximum desirability range; the accompanying note explicitly discusses range in tiles. | 20, 59 |
| 5 | `BuildingField::Employees` | Number of employed people, a count. Does not identify OpenEmperor's current worker requirement. | 21 |
| 6 | `BuildingField::FireRisk` | Fire risk increment; risk scale and update interval unknown. | 22 |
| 7 | `BuildingField::DamageRisk` | Damage risk increment; risk scale and update interval unknown. | 23 |
| 8 | `BuildingField::Resource` | Resource used; whether this integer denotes identity, quantity or another code is unknown. | 24 |
| 9 | `BuildingField::RiskReducer` | Risk reducer; affected risk, scale and interval unknown. | 25 |
| 10 | `BuildingField::EvolveDesirability` | Evolution desirability setting; label explicitly says a nonpositive value means no evolution. Further triggering logic unknown. | 26 |
| 11 | `BuildingField::Integrity` | Structural integrity value; scale and damage arithmetic unknown. | 27 |
| 12 | `BuildingField::FengShui` | Feng shui value; category/score interpretation and application unknown. | 28 |

M1 line61–62 further says a positive desirability level needs a negative step.
E1 `0x44eba2–0x44ebcc` passes fields1/2/3/4 through `0x44ec80` to
`0x44ecd0`; `0x44ed41–0x44ed70` independently establishes field2's period,
field3's signed delta and an upper limit of ten spread rings. This is bounded
`reverse-engineered` evidence for the step/range units, not a restored World
desirability integration. M1 lines56–57 exclude chained
structures except Avenues/Boulevards from desirability; their raw rows still
remain readable. `SZE` in the table header is the desirability step-size column,
not permission to infer building footprint or selectable geometry.

## House fields: 24 fixed zero-based indices

All names/indices are `verified original` from M1's a–x legend, lines30–54.
“Requirement” below means the label says it is needed for evolution. E1
`0x51a3f0–0x51a651` additionally establishes the relevant presence, food-score
and stored-quantity comparisons. These checks do not establish delivery
quantity, consumption amount, visit duration or period.
The table records that model/needs evidence scope. The later OE-06
[delivery](../reverse/original-goods-delivery-contract.md) and
[consumption/quality](../reverse/original-goods-consumption-food-contract.md)
contracts establish separate numerical operations; physical tonnage and clock
mapping remain unresolved.

| Index | Typed field | Meaning and supported unit | M1 line |
|---:|---|---|---:|
| 0 | `HouseField::DesirabilityLow` | Desirability level for devolution; score threshold, exact comparison semantics unknown from the text. | 31 |
| 1 | `HouseField::DesirabilityHigh` | Desirability level for evolution; score threshold, exact comparison semantics unknown from the text. | 32 |
| 2 | `HouseField::Herbalist` | Nonzero activates Herbalist presence check; service duration unknown. | 33 |
| 3 | `HouseField::Water` | Nonzero activates Water presence check; delivery mechanism unknown. | 34 |
| 4 | `HouseField::Acupuncture` | Nonzero activates Acupuncture presence check; service duration unknown. | 35 |
| 5 | `HouseField::Music` | Nonzero activates Music presence check; service duration unknown. | 36 |
| 6 | `HouseField::Acrobat` | Nonzero activates Acrobat presence check; service duration unknown. | 37 |
| 7 | `HouseField::Drama` | Nonzero activates Drama presence check; service duration unknown. | 38 |
| 8 | `HouseField::FoodQuality` | Threshold against a byte-sized food-quality score, not an amount of Food. Quality formula and score composition remain open here. | 39 |
| 9 | `HouseField::Hemp` | Threshold against stored signed-short Hemp quantity. Physical tonnage, delivery amount, consumption amount and period unknown. | 40 |
| 10 | `HouseField::Ceramics` | Threshold against stored signed-short Ceramics quantity. Physical tonnage, delivery amount, consumption amount and period unknown. | 41 |
| 11 | `HouseField::Tea` | Threshold against stored signed-short Tea quantity. Physical tonnage, delivery amount, consumption amount and period unknown. | 42 |
| 12 | `HouseField::Silk` | Threshold against stored signed-short Silk quantity. Physical tonnage, delivery amount, consumption amount and period unknown. | 43 |
| 13 | `HouseField::BronzewareOrLacquerware` | One shared threshold; either stored signed-short Bronzeware or Lacquerware quantity must meet it independently. The independent consumption of both stocks is established in the [OE-06 contract](../reverse/original-goods-consumption-food-contract.md); physical tonnage remains unknown. | 44 |
| 14 | `HouseField::CrimeIncrement` | Crime risk increment; risk scale and update interval unknown. | 45 |
| 15 | `HouseField::CrimeBase` | Crime risk base; scale and application unknown. | 46 |
| 16 | `HouseField::Unused16` | Explicitly unused; no unit or new authority assigned. | 47 |
| 17 | `HouseField::Capacity` | Population capacity, a count of people; area/footprint multiplication unknown from the text. | 48 |
| 18 | `HouseField::TaxRate` | Tax-rate multiplier; base rate, scale and collection interval unknown. | 49 |
| 19 | `HouseField::Unused19` | Explicitly unused; no unit or new authority assigned. | 50 |
| 20 | `HouseField::DiseaseIncrement` | Disease risk increment; risk scale and update interval unknown. | 51 |
| 21 | `HouseField::Ancestor` | Nonzero activates Ancestor-access presence check; duration unknown. | 52 |
| 22 | `HouseField::Confucian` | Nonzero activates Confucian-access presence check; duration unknown. | 53 |
| 23 | `HouseField::DaoistOrBuddhist` | Nonzero activates one shared Daoist **or** Buddhist presence requirement; duration unknown. | 54 |

### Two alternatives do not create a 25th field

There is no independent lacquerware column among the24 fields. The original
legend's enum spelling `EVO_BRONZEWARE` abbreviates a shared requirement; using
it to assert a bronze-only field would lose the stated lacquerware alternative.
Likewise `EVO_BUDDHIST` abbreviates a Daoist-or-Buddhist requirement. Typed names
retain both alternatives explicitly.

E1 `0x51a3f0` reads field13 once and `0x51a616` accepts either of the two stocks
at offsets `0x1a` or `0x1c` if it meets that threshold. Thus the OR is also
`reverse-engineered` original requirement behavior; adding the stocks together
is not this predicate. F1 `HouseEvolution.cpp:136` uses their maximum, while
line130 accepts either Daoist or Buddhist access for field23. Requirement
acceptance is distinct from proof of original consumption.
F1 lines50–51 independently subtract field13 from **both** goods stocks during
consumption. That behavior is not established by the shared requirement column.
OE-02 did not import it or choose a consumption priority. OE-06 independently
checks the callback at `0x519f90`: both stocks consume the same field amount
independently. Its explicit selector events do not establish a monthly cadence
or a World-tick mapping.

## House row IDs: ordered file identity, not Building type

These15 row labels and their order are `verified original` (M1 lines364–378).
The left column is zero-based `HouseRowId`, not the mixed display prefixes in
the file and not an original saved Building type. There is no sixteenth row
for a generic vacant common house. F1's type-to-row alias for Vacant/Shelter is
a separate mechanism and must not insert an extra model row.

| Row ID | Exact source label | M1 line |
|---:|---|---:|
| 0 | `1: Shelter` | 364 |
| 1 | `2: Hut` | 365 |
| 2 | `3: Plain Cottage` | 366 |
| 3 | `4: Attractive Cottage` | 367 |
| 4 | `5: Spacious Dwelling` | 368 |
| 5 | `6: Elegant Dwelling` | 369 |
| 6 | `7: Ornate Apartment` | 370 |
| 7 | `8: Lux. Apartment` | 371 |
| 8 | `Elite: Unoccupied` | 372 |
| 9 | `Elite 0: Abandoned` | 373 |
| 10 | `Elite 1: Modest Siheyuan` | 374 |
| 11 | `Elite 2: Lavish Siheyuan` | 375 |
| 12 | `Elite 3: Humble Compound` | 376 |
| 13 | `Elite 4: Impressive Compound` | 377 |
| 14 | `Elite 5: Heavenly Compound` | 378 |

The parsed file may be a valid edited model: these canonical names document M1
and do not authenticate arbitrary file contents. Row order remains the raw
reader contract. E1 `0x518b7f` independently computes
`max(original_house_type - 3, 0)`, supporting the observed type-to-row alias;
it does not merge the distinct typed model-row and saved-type domains.
Typed canonical descriptors check the actual parsed label metadata before
claiming the identified row. Old World house levels0–2 do not acquire these
identities.

## Five bounded raw golden facts

Only these five selected balance values are recorded, not the model tables.
Expected literals were read independently from the source text and refer to
the exact M1 hash above; they are raw values with no difficulty transformation.

| Source record | Field index/name | Expected raw value | M1 line |
|---|---|---:|---:|
| Building35, Clay Pit | 0 `Cost` | 75 | 119 |
| Building43, Kiln | 1 `Desirability` | −30 | 127 |
| House row0, Shelter | 17 `Capacity` | 7 | 364 |
| House row2, Plain Cottage | 8 `FoodQuality` | 20 | 366 |
| House row14, Heavenly Compound | 13 `BronzewareOrLacquerware` | 1 | 378 |

These facts test independently pinned indices, signed parsing and row identity.
They do not establish economics, timing, original executable behavior, or
semantic validity of an edited model beyond the reader's stated syntax checks.

## Functional adapter mapping is not original object identity

F1 `OriginalHousingView.cpp:17–28` maps seven authored World object kinds to
source model records for hypothetical, read-only desirability reports. The
record IDs and labels below are `verified original` in M1; each mapping from a
World kind is **`inferred`**, based on functional similarity and the fork's
explicit choice. None identifies a saved original source or alters geometry.

| Actual World kind | F1 candidate record | M1 line | Evidence boundary |
|---|---|---:|---|
| `ClaySource` | 35, Clay Pit | 119 | Authored clay output is functionally similar; original building identity not established. |
| `Pottery` | 43, Kiln | 127 | Authored clay-to-pottery recipe is functionally similar; original production timing and quantities not established. |
| `Warehouse` | 54, Warehouse | 138 | Authored stock/courier role is similar; original storage and dispatcher behavior not established. |
| `Market` | 59, Common Market Square | 143 | Authored market distributes Pottery and generic Food; no complete original goods/services implementation follows. |
| `Farm` | 193, Farmhouse | 277 | Authored Food producer is similar; no identification of crop fields, farmhouse composition or footprint. |
| `Well` | 72, Well | 156 | World Water access remains authored; separate existing sprite-registration evidence identifies original model72 art. |
| `HealthPost` | 207, Herbalist's Stall | 291 | World Health visits remain authored; separate existing sprite-registration evidence identifies original model207 Herbalist art, not acupuncture. |

The separate `reverse-engineered` presentation facts are documented in
[original presentation correction](../reverse/original-presentation-correction.md):
model table `0x8235a0 + 24*model_id` identifies Well72 group`0x482`→Safety1559
and Herbalist207 group`0x484`→Safety1580. This does not promote either functional
World mapping to verified original economics, service duration, footprint or
simulation. Current visual anchors likewise remain authored.

Other World kinds are unmapped by F1, including `ServicePost`, `FireWatch`,
`Workshop`, `Road`, `Household` and `Empty`. There is no permission to assign
them a nearby numeric record or infer a source identity from a renderer asset.
The future integration must explicitly choose its functional mapping and leave
unsupported kinds unsupported. Production Rule4/5/6 and old saves remain the
existing `authored fallback` contract throughout this evidence task.

[fork-model]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/ModelData.cpp#L65-L78
[fork-house]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/HouseEvolution.cpp#L39-L51
[fork-view]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/OriginalHousingView.cpp#L17-L28

## Implemented named schema v1 and checks

`ModelSchema.h` retains `model_schema_version=1`, explicit named Building IDs
and15 named `ModelHouse` rows. Immutable model preparation distinguishes the
historical seven-row capability from24 additional production rows. The37 field
enum values and five Difficulty indices are explicit. `model_id`, `model_row` and label lookup reject undefined
enum values; raw named getters additionally compare the actual parsed source
label. The generic reader preserves labels and arbitrary syntactically valid
raw values without assigning these identities to a changed label. A matching
label is a schema identity check, not authentication of model values.

`building_value` and `house_value` explicitly prepare values for a requested
Difficulty. Invalid fields, IDs and difficulties throw `std::out_of_range`;
source-label mismatch throws `std::invalid_argument`; arithmetic outside the
checked original int32 intermediate domain throws `std::overflow_error`.
No missing modifier or model row receives a default.

`house_building_type` maps the15 rows to source Building types3–17. Its inverse
returns no distinct named row for Vacant2 or non-House source types; out-of-reader-
bounds IDs are errors. The observed initializer's lower-row alias is documented
in the reverse contract and is not a sixteenth table record.

`OriginalModelMapping.h` is a separate pure candidate lookup. Core functional
analogies remain inferred; explicit paid production mappings are classified as
authored. Unsupported kinds receive no candidate, and invalid enum values throw.
Well/Herbalist presentation evidence describes the recovered registration, not
whichever visual or custom profile happens to be selected. The lookup supplies
no capability, footprint or permission and is not called by World.

`original-model-schema-synthetic` tests named IDs, literal field order, all five
difficulties, source-label failures, checked arithmetic and every Object case.
`original-model-schema-real` checks the exact source hash, the five curated
points through a separate text-point scanner, named access, and2255 checked
transform queries across the real source fields. Missing optional dependencies
produce explicit SKIP77; an existing malformed or different golden revision
fails. These augment the always-on raw-reader tests, not replace them. See
[testing.md](../testing.md) for current build and test instructions.

## Additional production candidates

Current active preparation keeps the historical seven-row factory separate from
its explicit production extension. The wider raw reader and model inspector can
observe other rows without adding them to either prepared capability set or
granting World construction. The [production candidate register](oe55-production.md)
separates existing paid adapters from unsupported labels and records the missing
footprint, material, clock, route and sink evidence. Raw `Resource` remains an
unresolved column; it does not select an output or recipe by itself.
