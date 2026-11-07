# OE54: authored finite land trade

Rule21 / policy3 / schema37 is an explicit **classic Rule6** descendant. It
uses native `Good::Pottery`, normal paid Warehouses and the existing World
navigator, movement and economy. It has no Original-economy state, scheduler,
Rule19 retirement journal or Rule20 source-cell ramps. Recommended Original18,
ordinary City-v16 rule4, general City-v11 rule3 and all historical saves remain
separate. The model and difficulty bind identity; they do not select original
merchant prices or add original trade authority.

## Evidence and authored decision

| Observation | Classification and consequence |
|---|---|
| Common model reader validates 269 Building and 15 House rows | SOURCE_VERIFIED grammar and exact model identity; not a trade engine |
| Raw Warehouse (54), Trading Quay (56), Trading Station (58), CityGate (130) labels/cost/workforce columns | SOURCE_VERIFIED metadata; original footprint, final cost, partner and actor semantics UNKNOWN |
| Trade.txt raw Ceramics 75, Clay 25, Land 34/800, Sea 4/1200 | SOURCE_VERIFIED named values; quantity, currency, cadence and buy/sell normalization UNKNOWN |
| Figure labels 20/45/47 | SOURCE_VERIFIED labels; no runtime merchant or sprite admission |
| Industrial 175 and 177/178 markers; GateHouse corridors | Existing source protection/pass-through only; original foreign endpoints UNSUPPORTED |
| Two normally paid Warehouses plus a 40-Funds license, one caravan, four-unit Pottery portions at unit prices 6/3 | AUTHORED Rule21 transaction contract |
| Gateway touches the common source Diamond boundary while its full body remains permitted land | AUTHORED endpoint; no movement outside geometry and no reinterpretation of original markers |

The [model evidence](../reverse/original-model-contract.md#oe54-trade-metadata-boundary)
records the current bounded observations and exact source fingerprints. No
original trade route, partner city or annual quota is claimed.

## Player contract

Choose **H Land trade (rule21)** in Advanced, or launch explicitly:

```sh
./build/openemperor --data /path/to/emperor \
  --sandbox Cities/YourMap.map --sandbox-rules sandbox-city-v16 \
  --authored-land-trade
```

The mode starts paused and empty. Build two ordinary Warehouses, a connected
legal road and sufficient paid Houses/workforce. One Warehouse must touch the
actual source geometry boundary with its complete level 2×2 body inside admitted
normal land. Select the city Warehouse and use **G Bind**; the sole other
Warehouse becomes the explicitly authored gateway after World validation and a
40-Funds license payment. No map name, visible terrain, source ID, road failure
or interior obstacle supplies the boundary. Original protected structures,
markers, gates, unsupported raw terrain and water remain refused.

Both Warehouses retain classic 150-Funds construction, 2 workers each,
normal upkeep, ordinary internal storage and complete footprints. One additional
caravan uses the existing 26-courier budget; the 46-body limit also remains unchanged. No employee,
storage capacity or global resource limit increases.
The binding is permanent in this V1: no reassignment, demolition of either
bound Warehouse, mid-trip cancellation or refund. Unbound Warehouses retain
ordinary demolition with the existing confirmation.

Use **I Buy** for an import and **E Sell** for an export. Buttons and keyboard
share the same World command path. Each order is exactly 4 Pottery. The single
caravan must physically return before another order; competing orders refuse
atomically. The finite partner offers 8 import units and 8 export units, with a
24-Funds export-buying budget. Thus at most two imports and two exports are
possible. The journal is separately bounded to 8 rows; offers never replenish.

- **Import:** pay 24 on acceptance and reserve 4 units of city Warehouse incoming
  headroom. The caravan travels outward **empty**. Only actual gateway arrival
  withdraws 4 from the finite outside stock and loads the real caravan. Only
  its physical return credits Warehouse stock and releases the reservation.
  Paid external pending units are neither city stock nor city cargo.
- **Export:** remove 4 actual city Warehouse units into the caravan at the
  accepted command, while it is physically at its owner. It travels loaded.
  Only actual gateway arrival removes that cargo to the outside sink and
  credits 12 Funds. The empty caravan then returns. No dispatch, route preview
  or proximity earns money.

New commands require both endpoints operating, staffed and not burning, enough
Funds/stock/headroom, available offers and a legal prepared route. Later
pause/fire does not cancel an already departed trip; real movement and arrival
complete its established commitments. Removing a current/begun-edge road refuses;
a later cut preserves cargo, headroom, escrow and physical edge progress. The
caravan completes that edge and uses a legal reroute. When disconnected, it
waits for an actual route revision and resumes after ordinary paid repair.
There is no teleport, insurance credit or loaded export return/cancel branch.

## Accounts and persistence

`simulation::World` alone owns the paid binding, typed monotone Order IDs,
finite journal, real Courier and transactions. Existing production output, `BuildingState.pottery_stock`
and `CourierState.cargo` remain the physical Pottery inventory authority. The trade DTO
stores identities and actual order/boundary/completion ticks, not stock mirrors,
paths, derived offers, render state or navigation caches.

Every phase uses the existing production/consumption accounts plus explicit
trade terms:

```text
completed Pottery + boundary-loaded imports
  = current city stock + active cargo + consumed Pottery + boundary exports
start + taxes + actual export receipts
  = construction (including license) + installed upkeep + prepaid imports + Funds
external stock = initial 8 - boundary-loaded imports + actual exports
external Funds = initial 24 + prepaid imports - actual export receipts
```

Offers reserve accepted orders; an import's paid-but-unloaded quantity remains
an outside commitment. Export acceptance debits actual stock but earns no
receipt. Imports have no credit line. Classic installed upkeep retains its
historical debt behavior; the mode does not claim globally nonnegative Funds.
A due export receipt is preflighted with that tick's exact tax/upkeep arithmetic
before the first tick mutation.

Schema37 requires marker `rules.authored_land_trade: "authored-land-trade-v1"`
even when empty, exact Rule21/policy3, model SHA/bytes/difficulty, full map SHA,
geometry, mask and complete permission fingerprint. Shared source MapGeometry
is rederived at restore; a claimed same-hash boundary mask cannot authorize a
forged endpoint. Complete body/actor/order references, counters, finite offers,
real headroom/cargo, accounting, paths and necessary physical travel times are
validated before atomic publication. Closed routes are not invented from today's
roads. A private schema22 syntax projection is never a historical live World
restore or migration. Rules1–20/schemas1–36 retain exact serializers and authority.

F5/F9, file chooser and Recovery use exact-generation restores; Rule21 saves
cannot be loaded as an older city. Managed checkpoints receive fresh manual
save targets. Failed writes/restores preserve the previous World, IDs, Funds,
files and source bytes.

## Validation

Follow the [testing guide](../testing.md) to build the four trade test targets,
then run their registered CTest cases:

```sh
ctest --test-dir build --output-on-failure \
  -R '^authored-land-trade-(world|save|menu|view)$'
```

- `authored-land-trade-world`: normally paid production, physical orders,
  conservation, finite offers, road disconnection/repair and atomic refusals.
- `authored-land-trade-save`: complete documents, physical phases, hostile
  identities/accounting, Recovery ancestry and atomic publication failures.
- `authored-land-trade-menu`: explicit selection, unchanged defaults, exact
  generation loading, F5/F9 and protected managed-save inputs.
- `authored-land-trade-view`: shared mouse/keyboard actions, budget confirmation,
  physical cargo, save continuation and pure rendering through SandboxView.

For an independent-process persistence check, run the save test executable from
this repository root. Choose a fresh directory below `.local/oe54`; the CLI
rejects existing output files and linked paths. Stop if any command fails.

```sh
proof_dir=.local/oe54/independent-UNIQUE-RUN
mkdir -p .local/oe54
mkdir "$proof_dir"
./build/openemperor-authored-land-trade-save-tests --create "$proof_dir/creator0.json" 0
./build/openemperor-authored-land-trade-save-tests --restore "$proof_dir/creator0.json" "$proof_dir/restored0.json" 0
./build/openemperor-authored-land-trade-save-tests --create "$proof_dir/creator103.json" 103
./build/openemperor-authored-land-trade-save-tests --restore "$proof_dir/creator0.json" "$proof_dir/restored103.json" 103
cmake -E compare_files "$proof_dir/creator0.json" "$proof_dir/restored0.json"
cmake -E compare_files "$proof_dir/creator103.json" "$proof_dir/restored103.json"
```

Both comparisons must match the complete canonical SaveDocument bytes, with the
input document unchanged. The fixture earns its first loaded export through
paid production. Round-trip the empty state, all four physical outbound/return
states and completed orders; verify full snapshots and subsequent continuation,
not selected fields or digests alone.

Check the Pottery and Funds equations at each phase. An import prepays and
reserves headroom before an empty departure; imported city stock is credited only
after boundary loading and owner arrival. An export loads real city stock, earns money only at
the boundary and returns empty. Competing orders, insufficient stock/Funds/
headroom, unavailable endpoints, invalid bindings, exhausted offers and malformed
save references must refuse without changing state, IDs, reservations or bytes.
Test begun-edge safety, disconnected waiting and ordinary paid repair separately.
These trade fixtures do not independently establish every original protected-marker
or terrain admission predicate.

Run the complete regression suite in addition to the focused cases, using the
existing historical corpus and generation-specific restore paths. Preserve
complete historical schema 1–36 documents, independent demolition/ramp
continuations, the paid Rule18 Creator/Replay control and the bounded
[core-parity comparison](core-parity.md). Do not normalize unexpected output
differences, migrate old saves or replace protected masters to obtain a pass.

Open the generated Menu and View screenshots. Inspect narrow and larger windows
at 1×/2×/4× zoom for readable controls, Inspector scrolling, clipping, actor feet
and loaded/empty cargo states. Pure paused frames must leave the World unchanged
and perform no preparation or authority work. Check authored fallback and supplied
sprite presentation separately: role26's reuse of the curated Pottery-courier
visual is an authored assignment, not original Figure20/45 merchant or animation
parity. Scripted SDL and source-pixel checks do not establish native event delivery
or human acceptance.

See [PR34](https://github.com/oliverjessner/OpenEmperor/pull/34) for execution
results, compiler/configuration details, counts, fingerprints, cleanup proofs and
Git delivery/merge records. Native input, human gameplay, Finder/Gatekeeper and
clean-machine acceptance remain **NOT_RUN**.

Original partners, original gateway admission, merchant AI/quotas, trade ships,
ports, Ferries, empire-map routes, diplomacy, campaign trade, monuments and all
other commodities remain unsupported. Authored physical success does not
establish original trade parity.
