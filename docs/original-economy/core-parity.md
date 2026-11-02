# OE-07: independent core comparison with the study fork

This comparison binds the study fork to commit
`d861d2912337c09751cb68196f85d5a30752726e`. The independently reconstructed
OpenEmperor kernels cover model preparation, desirability, House needs and
transition intents, timer effects, dispatch attempts, six goods transfers and
consumption, and FoodQuality arithmetic. They retain explicit input limits and
do not activate a new World rule, save schema or paid original supply loop.

A matching result below means agreement on the stated input domain. It does
not establish full Emperor behavior. Static executable observations, synthetic
controls, the optional read-only model comparison, and unchanged legacy
World/save contracts remain separate evidence. This is a historical pure-kernel
comparison; [current runtime contracts](architecture-map.md) describe integration.

## Bound sources and classifications

The four fork modules are [ModelData][fork-model],
[Desirability][fork-desirability], [HouseEvolution][fork-house] and
[OriginalHousingView][fork-view]. The first three expose pure numerical rules;
the fourth maps an existing authored World into an approximate diagnostic.
The external source files remain in the ignored local study checkout and are
verified against their pinned Git blobs before comparison. No Windows files or
fork implementation is imported into production code.

Every decision uses one of these four classifications:

| Classification | Meaning in this report |
|---|---|
| `parity` | The specified bounded observations agree. This is not a universal equivalence claim. |
| `corrected bug` | A demonstrated difference is deliberate. The evidence column distinguishes a recovered original branch from an authored API safety correction. |
| `unverified` | The available evidence does not establish an original mechanism or clock interpretation. An experiment may still run with an explicit hypothesis. |
| `unsupported` | A fork wrapper or accepted input is deliberately excluded from the new API. Its assumptions grant no World authority. |

Schema1 serializes the display classification `corrected bug` as
`corrected_bug`; the other three values keep their spelling.

The independently checked original executable is bound to SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`;
the examined model to
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
Original instruction windows and balance tables remain private. The executable
is inspected statically and is never run by this comparison.

## Decisions on the numerical core

This is the decision register for the investigated surface, not a claim that
every row is a new fork-runtime test. The machine-readable report identifies
the vectors actually executed. Prior OE-01–OE-06 negative/domain CTests and
bounded static contracts support the additional input-safety and caller-context
decisions. The mapping/steady-revisit/calendar interpretations include source
audits; an unsupported wrapper is never counted as a matching implementation.

| Capability or difference | Decision | Evidence and retained boundary |
|---|---|---|
| Thirteen Building fields, twenty-four House fields, fifteen ordered House rows, five difficulties | `parity` | Independent generated grammar and raw-value controls; [model register](model-provenance.md). Raw access is distinct from prepared difficulty values. |
| Building percentage/truncation and House addition/clamp | `parity` | Safe-domain arithmetic agrees with the fork and the independently inspected [original model contract](../reverse/original-model-contract.md). |
| Duplicate Building IDs, extra House numbers, inner empty values and incomplete structure | `unsupported` | These fork-accepted inputs are excluded. The fork overwrites or drops information; the reader rejects ambiguity atomically. File/line limits and precise errors are authored safety boundaries, not recovered original parser behavior. |
| Invalid difficulty, invalid enums and overflowing edited-model arithmetic | `corrected bug` | The typed adapter rejects them; it does not clamp invalid difficulty or emulate unchecked arithmetic. These checks are authored and tested separately from valid-domain parity. |
| Square rings, footprint exclusion, clipping, ten-ring cap, sign absorption and ordered per-add saturation | `parity` | Independent distance controls and [bounded spread evidence](../reverse/original-desirability-contract.md). Source order remains significant at saturation. |
| Nonpositive spread period | `corrected bug` | The fork never advances; the original advances after each ring. The new kernel preserves that inspected branch. |
| Maximum over an on-map square, then explicit bonus | `parity` | Valid-domain maximum/bonus controls. The separately researched [cardinal-water producer](oe40-desirability-investigation.md) is outside these historical numerical vectors. |
| Off-map/oversized squares, map and source budgets, partial batch failure | `corrected bug` | Bounds, checked intermediates and atomic rejection are explicit authored guards. An out-of-map query does not invent a zero cell. |
| Sixteen uniform wall sectors | `unverified` | The fork's corrected-centre `Intended` mode and the new opt-in `ExperimentalUniform16` are authored hypotheses. Recovered mixed units, original sectors, neighbor cases and clipped-edge behavior remain unresolved; default remains wall-free. |
| Canonical House types/rows, ordinary verdicts, inclusive needs, bronze/lacquer OR and Daoist/Buddhist OR | `parity` | Independent type/requirement controls and [needs evidence](../reverse/original-house-needs-contract.md). Two insufficient stocks are never added into a sufficient alternative. |
| Ordinary transition targets and devolution counters, including no-target resets | `parity` | The bounded type/counter domain agrees; [transition evidence](../reverse/original-house-evolution-contract.md) independently establishes the handlers. A transition intent does not expand a footprint. |
| Occupied Vacant automatically becomes Shelter | `corrected bug` | The original also requires unblocked evaluation, desirability and current/next needs. The fork bypasses them. |
| Empty Shelter evaluation | `corrected bug` | The original preserves type/counter for nonpositive Shelter occupancy; the fork evaluates it. Other House types keep their separately observed gates. |
| Opaque evolution flags | `unverified` | Their control-flow effects are exposed explicitly and default false; their producers and gameplay meaning are unknown. The fork omits this context. |
| Timer grants 96/40, replacement on repeat, bounded countdown/expiry | `parity` | All nine timer-byte roles have independent grant/countdown controls and [timer evidence](../reverse/original-service-timer-contract.md). Counts are eligible operations, not World ticks. |
| One occupied-or-elite predicate for every service | `corrected bug` | The original distinguishes direct service, direct religion and shared visits, checks status/access where applicable and preserves conditional elite-only effects. The pure API requires those observations instead of supplying them. |
| One service staffing table and one religion staffing table | `corrected bug` | [Concrete provider bindings](../reverse/original-service-spawn-contract.md) establish five threshold functions for eleven providers. Herbalist, entertainment schools and stronger religious sources cannot use one generic schedule. |
| Positive-staff strict counter threshold | `parity` | On the overlapping nonwrapping domain, both compare the incremented counter strictly greater than its threshold. Due means attempt, not successful actor creation or House visit. |
| Unstaffed counter progression and unbounded integer counter | `corrected bug` | The original caller freezes on nonpositive staffing and closed prerequisites; eligible counters wrap as unsigned bytes before comparison. |
| Consumption: current canonical row, floor each stock, both bronze and lacquer use field13 | `parity` | [Independent consumption evidence](../reverse/original-goods-consumption-food-contract.md) confirms both subtractions. The evolution OR does not create a shared consumption budget. |
| Consumption at negative occupant observation | `corrected bug` | The inspected original skips exactly zero, whereas the fork skips all nonpositive values. The numerical kernel retains that equality without granting simulation admission to negative population. |
| Delivery row, size and elite factors | `corrected bug` | [Delivery evidence](../reverse/original-goods-delivery-contract.md) selects the next canonical row with row7 exception, size-byte>1 ×4 and elite ×2. The fork uses the current row and omits the elite factor. |
| Exactly zero occupants' Hemp/Tea fallback versus visit eligibility | `corrected bug` | Need10 is a query exception before factors, not permission to deliver to an empty ordinary House. Negative occupancy is not the same branch as zero. |
| Partial deliveries and refill cap | `corrected bug` | The original fallback compares requested refill with source availability, without a final need cap. The fork compares with need and can return different conserved quantities. |
| Nonnegative inventories, explicit source conservation, destination headroom | `corrected bug` | Negative stocks are rejected, nonpositive needs do not transfer and signed-word capacity cannot wrap. These are authored safety guards around the inspected transfer, not recovered source-slot capacities. |
| Positive-delivery FoodQuality weighted branches on their common ordinary domain | `parity` | Quality remains separate from goods quantities. Independent authored ratios check the weighted integer outputs. |
| FoodQuality denominator | `corrected bug` | The original denominator is food stock before delivery, not a model need. This is a static argument-binding correction; the standalone fork arithmetic cannot reveal its caller's meaning. |
| FoodQuality rounding boundary | `unverified` | The original stores a binary32 ratio; the fork uses binary64. The widened pure API chooses deterministic nearest-even midpoint comparisons. Its boundary outputs are measured, but an arbitrary historical x87 control word is not established. |
| Higher FoodQuality from zero delivered food | `unsupported` | Both the original early branch and fork can improve quality without transferred food. That grant is excluded by the new event boundary: zero delivery preserves quality. This is an authored corrective restriction, not a claim that the original behaves that way. |
| A fused `step_house`, sixteen steps/month and twelve months/year | `unsupported` | Explicit countdown/evolution/consumption operations can be composed in tests. The original phase35/39 order and consumption selectors0/7 are observed; a complete calendar and World-tick mapping are not. No guessed scheduler wrapper is added. |

## OriginalHousingView

The fork [labels its mapping an approximation][fork-mapping-contract]. Seven
current World building functions map to model candidates: ClaySource→35,
Pottery→43, Warehouse→54, Market→59, Farm→193, Well→72 and
HealthPost→207. The typed `functional_model_candidate()` retains these
analogies as `Inferred`, with mechanics identity `Unknown`. Separate recovered
Well/Herbalist image identities do not establish mechanics, cost, footprint,
constructibility or service cadence. The unmapped kinds remain unknown.
This mapping decision is **`unverified`** as original mechanics, even where
the numerical candidate IDs agree.

The report's synthetic House projection then makes additional assumptions:

- Every existing Household starts as Shelter, independent of its current
  authored level or development history.
- `occupants = max(population, 1)` turns a real zero into a positive simulated
  occupant. This permits evolution that the current observation would block.
- Water coverage is copied from the current authored World. That is a useful
  diagnosis input, not proof that original water admission and geometry match.
- Any positive `food_stock` becomes the configurable
  `food_quality_when_fed`; its default is zero. The option supplies a quality
  assumption. It does not measure diversity, source score or FoodQuality from
  the actual delivered food.
- A current `household_health_protected()` result grants Herbalist immediately
  and again on every artificial evolution step. It invents a steady revisit
  schedule from one coverage observation; no additional actor arrival is
  observed by this loop.
- `evolution_steps` is an artificial sequence of evaluations; peak/settled
  types are hypothetical reports, not restored original types or forecasts
  under a proven future supply schedule.

The complete report adapter is therefore **`unsupported`** as new gameplay
authority. Its read-only nature is **`parity`** with our boundary: neither the
fork diagnostic nor the comparison changes the supplied World. The underlying
mapping, ring, needs, evolution and timer capabilities remain available as
explicit pure operations. Reintroducing the approximate wrapper would hide
the very source, population, time and arrival facts required by later tasks.

The independent [HousingView probe](../../tools/original-rules-parity/HousingViewProbe.cpp)
executes the actual pinned fourth module against the unchanged simulation
library. Its model controls are authored and its Worlds are created by normal
commands. A legal HouseholdV3 House has a stored population field of0 (this
historical profile predates population simulation): a zero-step report starts
at Shelter, but one artificial step reports Hut because of the forced positive
observation. This does not identify an empty original-game House. A separate
CityV7 Farm supplies a House through actual production,
roads and courier travel. With the same delivered food and synthetic score
requirement1, report option0 retains Shelter and option100 reports Hut. Complete
WorldSnapshot equality before/after each report checks the read-only boundary.
No World inventory, population or snapshot is injected for these observations.
The steady-Herbalist conclusion above is a source audit of the explicit
renewal loop, rather than a claimed observation of an additional arrival.

## Clock and wrapper

The `composition.authored_phase_0`, `_1` and `_7` vectors execute the actual
fork `step_house()` against independently specified observations. The current
adapter explicitly calls countdown, evolution and, for the selected cases,
consumption. Expected timer, type/counter and stock results check that these
resolved operations can represent the same ordinary sequence. Their
classification is `unverified`: matching an authored composition neither
adopts the fused production wrapper nor proves that a selector means a month
phase, a whole original pass or any count of current World ticks. A possible
visit between original countdown phase35 and evolution phase39 must remain
an explicit event; it cannot be erased by an assumed atomic scheduler step.

## Coverage and acceptance limit

The machine-readable review artifact is [core-parity.json](core-parity.json).
Schema1 binds the fork revision and file hashes and the current kernel/probe
file hashes. Each numerical case records its ID, area, classification,
independent fixture/input, separate expected and observed fork/current outputs,
evidence path and assertion result. The fourth-module observation and optional
real-model result are separate members. Summary counts describe those executed
vectors, not the broader decision register or unimplemented gameplay.

Rule11's optional fire provenance changes the World/production DTO ABI. Its
reviewed header pins include the separate fire-state header; fresh linked
fork/current probes retain all74 previous cases, expectations and observations,
including the unchanged real-model digest. The authored FireWatch integration
does not acquire original-mechanics parity from this numerical comparison.
Rule12's separate factories and capacity/invoice queries have a reviewed World
header pin. Fresh linked probes still pass the same74 vectors with identical
cases, expected/observed values, fork pins and real-model digest. Its authored
economy changes are evaluated by the separate paid-city acceptance, not this
kernel comparison. Raw build/library provenance remains under ignored `.local/`.

Rule13/14 add explicit factories and predicates without changing the historical
HouseholdV3/CityV7 calls used by the fourth-module probe. The Rule14 review also
covers the immutable terrain pointer, appended influence/diagnostic fields and
the new terrain-facts header. The comparison now pins all three affected API
headers exactly and includes their SHA-256 identities in the public report.
Missing, changed or symlinked headers fail before compilation. The previous
World-header pin had not yet included Rule13; the review therefore checks the
complete prior Rule12-to-current Rule14 API difference, rather than assuming
the intervening generation was already certified.

Fresh Release comparison against the same pinned fork retains all74 canonical
vectors, classifications, expected/observed arrays, the fourth-module18-value
observation and the1800-value real-model digest. Only current source/API
fingerprints change. This validates compatibility of the historical comparison;
it does not certify Rule13 market retention, the Rule14 terrain mechanism or
paid Elite progression. The initial stale-header/report failures are preserved
separately from the fresh comparison under ignored OE40 evidence.

OE44's reviewed Rule15 addition changes only constexpr capability predicates
and explicit create/restore methods in `World.h`; no World data field is added.
The exact Rule14 identity and historical HouseholdV3/CityV7 calls remain intact.
A fresh source-bound Release comparison with the same pinned fork again passes
all74 cases, the unchanged18-value HousingView result and real-model digest
`[1800,477508576,4125568101]`. All cases, fork pins, classifications, summary,
HousingView, real-model results and limitations are literally unchanged in the
public report. Only the reviewed World header and runner fingerprints change;
`baseline-lock.json` is unchanged. Private commands, binaries and the complete
preservation check are under `.local/original-economy/oe44/run-01/gate-c-parity-release-01/`
and `gate-c-parity-public-review.json`. This verifies the legacy comparison ABI;
Rule15 Inspector behavior and paid retention require their separate tests.

OE45's Rule16 adds explicit deadline-dispatch factories and constexpr capability
predicates, without adding World or saved-state fields or changing the historical
HouseholdV3/CityV7 calls used by the fourth-module probe. After reviewing that API
difference, a fresh Release comparison compiled the separate fork/current
numerical probes and linked the actual pinned fork HousingView probe against the
new Release simulation archives. All74 canonical cases, classifications,
expected/observed values, the18-value HousingView result and the real-model digest
`[1800,477508576,4125568101]` remain identical to OE44. The fork pins and
`baseline-lock.json` are unchanged; only the reviewed World header and comparison
runner fingerprints change in the public report. Exact commands, compiled
binaries, archive hashes and the independent preservation comparison are under
`.local/original-economy/oe45/run-01/core-parity-release-final-01/`,
`core-parity-release-final.execution.json` and `core-parity-public-review-final.json`.
This final comparison repeats the probes after the return-blocking dispatch
amendment, with the newly linked simulation archive and unchanged public results.
This historical numerical/ABI comparison grants no original-parity or paid
retention claim to the authored Rule16 dispatcher.

OE46's explicit Rule17 adds service-dispatch factories and capability predicates
without adding World or mutable saved-state fields or changing the historical probe's
HouseholdV3/CityV7 calls. A fresh Release comparison compiled both numerical
probes and linked the pinned fork's actual HousingView probe against the new
simulation archives. All74 cases, classifications, expected/observed values,
the18-value HousingView result and real-model digest
`[1800,477508576,4125568101]` remain identical. Only the reviewed World header and
comparison-runner fingerprints change in the public report; the fork pins and
`baseline-lock.json` remain exact. The old private fork mirror was absent, so
the exact pinned public commit was fetched into a new ignored OE46 directory;
all11 fixed source hashes were verified before compilation. Actual commands,
archive hashes and the complete preservation check are retained under
`.local/original-economy/oe46/run-01/core-parity-release-02/` and
`core-parity-public-preservation-01.json`. This historical numerical/ABI
comparison does not establish original service scheduling or paid city stability.

Rule18 adds explicit Food-dispatch factories and capability predicates without
adding World or saved-state fields or changing the historical probe calls.
The fresh OE47 Release comparison links an immutable copy of all eight matching
archives and again preserves all74 cases, classifications, expected/observed
values, the18-value HousingView observation and real-model digest
`[1800,477508576,4125568101]`. The public report changes only the reviewed World
header and runner fingerprints; every other field, fork pin and
`baseline-lock.json` remains exact. Complete commands, compiled probes and the
literal preservation check are retained under
`.local/original-economy/oe47/run-01/core-parity-release-01/` and
`core-parity-public-preservation-01.json`. This historical numerical/ABI check
does not certify the authored Rule18 selector or citywide Food reliability.

The comparison uses independently specified vectors and expected outcomes for
parser values, rings, needs, House transitions, timer effects, consumption,
market-style inventory transfers and FoodQuality. A second executable runs the
pinned fork rather than treating a copied implementation as an oracle. The
fourth-module probe separately exercises its actual World diagnostic. Generated
controls are synthetic; optional real-model comparisons retain the source hash
and report an explicit SKIP when the optional installation is absent. Existing
malformed or unsafe data must fail instead of becoming a substitute fixture.

Reproduce the comparison with a matching prebuilt simulation library and the
pinned optional fork mirror. The output directory must be fresh. The tool
compiles the two numerical adapters separately and links the fourth-module
probe to the selected unchanged World library:

```sh
cmake --build .local/reswall/release-build --parallel 2 \
  --target openemperor-simulation
python3 tools/original_rules_parity.py --configuration release \
  --data .local/gog-extracted/app \
  --output .local/original-economy/oe07/reproduction-release
```

The OE45 comparison used the same pinned fork through its existing private
mirror and explicitly selected the new Release archives:

```sh
python3 tools/original_rules_parity.py --configuration release \
  --fork .local/original-economy/oe36/pinned-fork \
  --simulation-library .local/original-economy/oe45/run-01/release/libopenemperor-simulation.a \
  --data .local/gog-extracted/app \
  --output .local/original-economy/oe45/reproduction-core-parity-release
```

The OE46 comparison uses the newly verified private mirror and explicitly selects
the Rule17 Release archives:

```sh
python3 tools/original_rules_parity.py --configuration release \
  --fork .local/original-economy/oe46/run-01/pinned-study-fork-01 \
  --simulation-library .local/original-economy/oe46/run-01/release/libopenemperor-simulation.a \
  --data .local/gog-extracted/app \
  --output .local/original-economy/oe46/reproduction-core-parity-release
```

The OE47 comparison uses the same pinned mirror and a sealed copy of the
matching Rule18 Release archives:

```sh
python3 tools/original_rules_parity.py --configuration release \
  --fork .local/original-economy/oe46/run-01/pinned-study-fork-01 \
  --simulation-library .local/original-economy/oe47/run-01/core-parity-libraries-01/libopenemperor-simulation.a \
  --data .local/gog-extracted/app \
  --output .local/original-economy/oe47/reproduction-core-parity-release
```

`--configuration debug` and `--configuration sanitizer` select the matching
existing build directories; `--simulation-library` may name an explicit
matching library. The report's build provenance binds that library's hash.
Missing optional fork or explicitly requested real data returns77/SKIP;
existing wrong, incomplete or changed sources fail. Omitting `--data` records
real-model comparison as `NOT_RUN`. A synthetic comparison never replaces that
status with a claimed real-source result.

The isolated numerical capabilities are covered with stricter, explicit
boundaries. Rejected wrappers and hypotheses are listed above rather than
silently counted as implemented original gameplay. Full original wall behavior,
opaque flag producers, complete actor callbacks, original scheduler/calendar,
food-score production, migration/elite geometry and paid new goods/service
chains remain unresolved or assigned to later tasks. Gate A only establishes
this bounded comparison and unchanged legacy behavior after its required
tests; a passing comparison is not original-game or human acceptance.

[fork-model]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/ModelData.cpp
[fork-desirability]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/Desirability.cpp
[fork-house]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/HouseEvolution.cpp
[fork-view]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/OriginalHousingView.cpp#L17-L72
[fork-mapping-contract]: https://github.com/amscova/OpenEmperor/blob/d861d2912337c09751cb68196f85d5a30752726e/src/simulation/original/OriginalHousingView.h#L28-L30
