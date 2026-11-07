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

## Validation and delivery

| Gate | Result and scope |
|---|---|
| Paid import/export | SCOPED_AUTHORED_TRADE_PASS, finite Pottery orders and actual physical owner/gateway arrivals |
| Historical/Save | PASS, exact old generations plus independent schema37 continuations |
| Builds/Scripted SDL | PASS, Release 309/309, Debug 40/40, ASan/UBSan 40/40 and inspected source/fallback visuals |
| Original trade | PARTIAL research; original partners, gateway and merchant semantics remain UNSUPPORTED |
| Native/Human/Release | NOT_RUN; the inherited human/clean-machine release blockers remain |
| Git delivery | Actual normal-merge/main verification is recorded by the feature PR and merge tracker after execution |

Baseline: verified OE53 merge
`48f9602efc9bc1ed37bdaca05395f29491d6e4c4`.
Branch: `feat/oe54-intercity-trade`. Production/test commit
`d1d75f57c077776d2c49ef05925c1c96f7e08232`; reviewed parity/tool commit
`9497b19f43830a0d22eda9a3b94ac89c715aafa6`. Final source bindings cover 685
files. Tests use the isolated `.local/oe49/worktree`, Apple Clang 21.0.0
(`clang-2100.3.34.2`), CMake 4.4.3, arm64 macOS 27.0.1 and SDL 3.4.14.
Release reuses the explicit 27.0 deployment target; the project's 26.0 default
is unchanged. Scripted views use the dummy/software renderer.

The complete Release build and Acceptance target pass in 344.158 s with no
warnings. Complete CTest passes **309/309** in 395.202 s, with no failures,
skips or disabled tests. The four new tests cover World, Save/Recovery, Menu
and production SandboxView. Debug builds 37 selected programs in 208.745 s;
ASan/UBSan builds the same 37 in 594.011 s, both without warnings. Debug passes
40/40 in 343.764 s; ASan/UBSan passes 40/40 in 173.538 s. There are no failures,
skips or disabled tests. All 75 corresponding source-cell, trade and paid-building
PNG pairs are literal matches. Each configuration retains its exact 37 binaries,
13 archives and cache; Release retains 14 archives. Sanitizer options are
`detect_leaks=0:halt_on_error=1` and `halt_on_error=1:print_stacktrace=1`; no
ASan/UBSan diagnostic occurred, and this does not establish leak acceptance.
No timeout or tolerance changes were made to obtain passing results.

### Paid World and independent persistence

The World fixture purchases five Houses, Clay, Pottery, two Warehouses,
roads and the license through normal commands. Every tick checks complete
Pottery and Funds conservation. The first export comes from actual owned
production with no prior import. It debits 4 stock into cargo, earns 12 only at
the gateway and physically returns empty. The import pays 24, travels outward
empty with 4 real incoming headroom reserved, loads only at the boundary and
credits the owner after the loaded return. Two offers of each direction finish;
the next order refuses without mutation.

Negative cases include reversed, self, missing, non-Warehouse and duplicate
bindings, an interior endpoint, extreme boundary coordinates, missing roads,
stock, workforce or Funds, paused endpoints, an endpoint with a natural fire, insufficient
incoming headroom, competing orders, begun-edge road removal, future road
cut/wait/paid repair, exhausted offers and command-counter exhaustion. Hostile snapshots/JSON test
source/model/mask/boundary identities, actor/order references, cargo/reservation
consistency, clock bounds, accounting and atomic write failures. The completed-order
clock test uses a necessary footprint distance bound; it never invents historical
routes from today's roads. These new tests do not independently exercise every
original protected-marker or terrain admission predicate.

Independent schema37 creator/restore processes preserve the entire canonical
SaveDocument at tick 591 (16,173 bytes, SHA256
`a31f384e262860993b7285986f84ea10001eef159bc4f3713e722867c2599594`)
and tick 694 after 103 additional ticks (16,050 bytes, SHA256
`911931e3c2652cb703a20555a9553e50592b048c6421aec2b58c7bd311218570`).
These four processes use an earned loaded export. Separate tests round-trip
the empty state, all four physical outbound/return states and completed orders,
exact Recovery ancestry, F5/F9, wrong-generation refusal and failed atomic writes.

### Product UI and pixels

The authored 84-cell-side source-map View fixture buys the complete chain using
actual SDL keyboard/mouse actions, including 19 real budget confirmations and a
cancelled binding warning. Actual production reaches 8 completed Pottery and 4
city stock at tick 655. It completes an export of 4 for a receipt of 12, then an
import of 4 for a payment of 24. The loaded import is saved at tick 730; an
independent process re-encodes the full document and continues 103 ticks to 833
identically. The physical owner arrival finishes with zero caravan cargo/reservation
and a valid production balance.

Ten production View screenshots cover loaded outbound export and loaded import
return at 1280×800, 1600×1000 and 1920×1080 with 1×/2×/4× zoom. Six Menu screenshots
cover Advanced/setup at 900×600, 1280×800 and 1920×1080. All first successful images
were opened and inspected. Four footer controls remain inside the Inspector;
at 1280 width lower details require its existing scroll. At 4× distant geometry
can lie outside the aperture; the centered actor foot remains visible. The
900-pixel menu retains bounded/elided bottom diagnostics. The authored flat
map, bright synthetic terrain and fallback walkers are not original graphics
parity. One hundred paused renders keep all twelve preparation/authority
counters at zero and leave the complete World unchanged. All 75 final Release
trade/source-cell/paid-building images and the 75 images from each focused
configuration match the retained masters in full PNG bytes and decoded RGBA.
The two 900-pixel Menu images also match. Four larger Menu captures differ
solely in the build-revision footer: `48f9602efc9b dirty` versus
`d1d75f57c077 dirty`. Each pair has exactly 609 different pixels within the
footer; all other RGBA pixels match. These nonidentical images were opened and
retained, without normalizing their comparison.

A separate private helper links the actual 21 production View objects and 13
core archives, binding all 14 Release archives. It selects the existing curated
Pottery-courier visual for authored role26 using the supplied SprMain pair. All
ten actual loaded-export/import sprite witnesses have cargo 4 and unchanged
physical feet; all ten captures were opened. Five complete SaveDocuments are
literal matches to the registered fallback run, including the independent
103-tick continuation. Its 100 pure frames again leave all twelve counters
at zero. The guarded helper deletes only its own 27,708,581-byte transient
source copies after use; the actual installation remains untouched. This
proves the explicit authored visual assignment, not original Figure45/20
runtime semantics, animation parity, original trade or native event delivery.

### Historical behavior and safety

Fresh authored exporters reproduce all 132 complete canonical schema 1–34
payloads/indexes literally. Old schema35's fresh creator/zero/+103 restores
and schema36's fresh creator/restore at ticks 47/150 remain full-document literal
matches to their protected masters. The actual three fresh parity probes pass
74/74 with unchanged 46/18/6/4 classifications, 18 Housing values and real-model
digest `[1800,477508576,4125568101]`; all 30 Python contract cases pass. Only
three reviewed source fingerprints change in the parity register, not values
or classifications.

The unchanged paid Rule18 creator takes 113.486 s and its separate replay 118.936 s.
Across the 99-file logical output there are 97 literal comparisons and two
Recovery history documents with only the four enumerated identity/local-time/
checkpoint-filename metadata pointers permitted in each. All seven canonical
checkpoints, full snapshots and four Recovery payloads match. The original
private helper mistakenly expected `checkpoint-30620`; both complete old and new
manifests literally identify `checkpoint-24480`. A new explicit resume runner
preserves that failed helper/log and all original bindings, uses the protected exact seven
labels/ticks, reuses the completed Creator and runs one fresh Replay. No game
source, numeric expectation, timeout or original output was changed.

The golden window remains 5395→25795: 20,400 ticks, 400/400 Elite17 and 5200/5200
current House-needs evaluations, zero target fires/devolutions/Food misses,
five residential fires/2915 burning House ticks, 65 total building fires and
operating cashflow 22630−17450=+5180 with minimum Funds 5391. Thirteen purchased
Houses are not a claim of thirteen continuously nonburning homes; the minimum
nonburning, occupied goal counter is 11. The 27 pre-entry Food misses remain.
This finite historical control does not establish a sustainable or fire-free
Rule21 city.

The before/after guard preserves 3062 immutable files, all 1464 original inputs,
the absence of 286 previously deleted duplicate paths, the original user's
72,212-byte dirty patch and all old masters. All 685 source inputs and 14 Release
archives remain unchanged across completed Release and historical checks.
Original files were never written or their EXE run. No original assets, dumps,
saves or screenshots enter Git or the bundle.
Detailed private proofs live under `.local/oe54/`, including the full Release
JUnit, independent schema audits, parity preservation and Rule18 audit. The
final evidence index has SHA256
`45a9be38994d01e7473d10b8e40419cf652207e8521358e3ef383ab6d697bb7a`.

Bounded cleanup removes four replay bulk duplicates, then four creator bulk
duplicates after full literal comparison to protected OE51 masters. The alias
chain preserves the 99-file logical evidence while the new physical corpus has
35 Creator and 56 Replay files; every complete save/checkpoint/Recovery file
remains. Exactly 227 further duplicate PNGs are removed only after fresh whole-
PNG and whole-RGBA comparisons. All 16 opened masters, four nonidentical larger
Release Menu captures, twelve focused Menu captures with configuration-footer
differences, one failed diagnostic and all ten supplied-sprite captures remain.
Together with transient source copies, these operations remove 1,904,933,002
logical bytes. Final free space is 78,115,684,352 bytes; APFS free-space change
is recorded separately from logical sizes. No protected input/master was deleted.
The final cleanup retains all 685 source bindings, 14 archives and 21 View
objects; its complete input/user-patch guard passes.


Native input, human gameplay, Finder/Gatekeeper and clean-machine acceptance
remain **NOT_RUN**. No tag, release or deployment is authorized by these checks.
Actual PR/merge metadata is recorded in the PR and private merge tracker only
after the normal GitHub merge and fresh main/tree verification.

Original partners, original gateway admission, merchant AI/quotas, trade ships,
ports, Ferries, empire-map routes, diplomacy, campaign trade, monuments and all
other commodities remain unsupported. Authored physical success does not
establish original trade parity.
