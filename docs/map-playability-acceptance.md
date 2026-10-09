# Multi-map playability acceptance — 2026-10-09

This pass examines `Cities/Chang-an Zhou.map`, `Cities/Handan.map` and
`Cities/Badaling.map` on starting HEAD
`f5ec197b903dcc68edb028e4a3dc70fac646462d`. The starting workspace was clean.
No production engine, simulation, presentation, input, resources or existing
tests were changed. The added launcher and excluded technical fixture exercise
existing production paths. No commit, publication, original NPC simulation,
economy adjustment or permission exception is part of this pass.

The tested configuration is `sandbox-city-v16`, rule **3**, MapPermissions
policy **1**, save schema **19**. `MapRulesChecked` and original-occupancy
preparation establish admission, not complete gameplay or Emperor parity.

## Evidence and original inputs

The source is the local pinned GOG-derived `.local/gog-extracted/app` tree.
All original inputs are read-only. The inventory contains 1,464 files,
819,711,091 bytes; its sorted path/size/SHA-256 manifest is
`dde2382fe1c348da765c8776149a73210b81f6cba7f93a0c6159d79ce3154feb`.
Reports, software-backend captures, private observers and all generated saves
remain under ignored `.local/map-playability/`.

Three kinds of evidence must be kept separate:

- Technical production replays use the real stored-map renderer's legacy mask,
  complete shared MapPermissions, ordinary accepted paid commands, actual ticks,
  and the normal save writer/reader/restore path.
- Scripted normal-application evidence uses the unchanged Application and views
  with privately observed queued SDL events, isolated roots and the actual
  dummy/software backend. It can prove normal menu/save/render behavior, but is
  not native input or a human playtest.
- Oliver's Banpo/Anyang observations are user reports. This pass cannot identify
  his exact Anyang file, Chang-an save, action history or screenshot storage cell.
  Human/native acceptance remains open.

The similar Chang-an failure is explicitly a separate city. It does not claim
to recover the screenshot's population 16 or exact selected point.

## Map identity and admission

| Exact relative file | SHA-256 | Bytes | Declared size / border | Source active / protected cells | Building / road allowed cells | Map renderer images / uploads / RGBA bytes |
| --- | --- | --- | --- | --- | --- | --- |
| `Cities/Chang-an Zhou.map` | `d5a4470213c9208779b186c61db089f169ed5458b777c8e47fe1e8d160f3e849` | 192,423 | 226 / 1 | 10 / 9 | 16,881 / 17,105 | 402 / 1206 / 21,844,848 |
| `Cities/Handan.map` | `6bf6bcf54c19a680985ce1eaf3e5846259fcaf7652bbf443fbe716845acade13` | 172,777 | 170 / 29 | 93 / 780 | 8,355 / 8,549 | 410 / 1230 / 38,419,704 |
| `Cities/Badaling.map` | `c266c0e2a711045f91eb9bfd410445503782e0274884edd36880cf20f554bb46` | 111,727 | 170 / 29 | 93 / 780 | 9,190 / 9,417 | 352 / 1056 / 35,150,496 |

| Map | Production legacy SHA-256 | Policy fingerprint |
| --- | --- | --- |
| Cities/Chang-an Zhou.map | `5f3fd9c7c915d977115de970302dec8d3b9d2c254ecb732e53f0f7c9d40ae5c1` | `40d3c7688579ce6c64bb081fd96bb93e395ebdde2db77f1b36934dcb9c9eaa21` |
| Cities/Handan.map | `26b3b71b55ec87fafddf5989d1a6d49c128690f6bf983944515f78c640ad8857` | `db9ded6793d099a378cff98338c47b4f27f93579e1fd527048fc81c6ae2362e1` |
| Cities/Badaling.map | `58fc48e7f21700f08675c1081fd196a320c462338de8a18c30b6cfba466cd832` | `0b93b6cb76cb17317ef366858bcf3f4df26c1b07fe08c54eedb3c2cab90f6bee` |

Storage is 228×228 for all three. The declared geometry and its border remain
authoritative; the candidate-layout search does not borrow Xia coordinates.
The full 4,000-record manager is checked before publication. Original source
records establish bounded protection, not operational Sandbox buildings.

Normal Automatic Great Wall presentation retains historical fallback. Explicit
Stone preview uses the existing optional preparation path. On these three maps
the actual legacy masks, complete canonical permissions, tick-zero Worlds, all
1,600 subsequent complete WorldSnapshots and complete tick-1,600 SaveDocuments
are equal between Automatic and Stone. Neither display mode creates Sandbox
roads, opens a wall, grants a gate passage or changes transport edges.

Asset preparation uses the existing exact-fingerprint building, road and walker
metadata with eager decoding/upload and shared physical resources. Map renderer
counts below exclude the separately prepared dynamic building/walker profiles;
normal-application aggregate counts are recorded separately.

## Per-map results

All fresh Empty Cities have **zero Buildings, zero Houses, zero Couriers,
zero population and 1,300 Funds**. No original city/NPC simulation is imported.
Missing walkers at this stage are **EXPECTED**. Oliver's actual Handan/Badaling
start choice or loaded save remains unconfirmed.

The following compact layouts were independently found and fully validated:

| Map | Legal compact origin | Mini construction / Funds | Mini Buildings / Houses / Couriers | Population / assigned workers / required | Clay dispatch → arrival → home, relative ticks |
| --- | --- | --- | --- | --- | --- |
| Chang-an Zhou | (79,42) | 478 / 822 | 4 / 2 / 2 | 12 / 10 / 10 | 32 → 41 → 51 |
| Handan | (142,74) | 478 / 822 | 4 / 2 / 2 | 12 / 10 / 10 | 32 → 41 → 51 |
| Badaling | (112,53) | 478 / 822 | 4 / 2 / 2 | 12 / 10 / 10 | 32 → 41 → 51 |

Each mini contains two ordinary Houses, Clay Source, Pottery and nine paid
connected roads. There is no Warehouse, tax or complete supply claim for a mini.
The actual Clay target is Pottery; the retained phase name `ToWarehouse` does
not alter the typed role/target. Normal-menu placements made at tick 5 have
actual dispatch/arrival/home ticks **37/46/56**.

| Map | Map rules | Start tested | Buildings / population / staffing at paid supply tick 0 | Couriers | Roads | Actual normal supply | First tax | Buildability | Save/load | Problem status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Chang-an Zhou | Checked, rule 3/policy 1 | Empty; separate mini; paid compact starter | 11 / 24 / 24 of 24 | 8 | 15 connected paid roads | Food, Pottery, Service delivered | Tick 400 | Individual terrain/legacy/original claims preserved | Full technical readback; normal-app evidence below | PLAYER SETUP / ECONOMY; exact user point UNRESOLVED |
| Handan | Checked, rule 3/policy 1 | Empty; normal-menu paid mini; paid compact starter | 11 / 24 / 24 of 24 | 8 | 15 connected paid roads | Food, Pottery, Service delivered | Tick 400 | Original Great Wall protection preserved | Full technical readback; normal-app evidence below | EXPECTED (Empty walkers); UNRESOLVED (exact user setup) |
| Badaling | Checked, rule 3/policy 1 | Empty; normal-menu paid mini; paid compact starter | 11 / 24 / 24 of 24 | 8 | 15 connected paid roads | Food, Pottery, Service delivered | Tick 400 | Original Great Wall protection preserved | Full technical readback; normal-app evidence below | EXPECTED (Empty walkers); UNRESOLVED (exact user setup) |

The tables report bounded positive scenarios. They do not assert that every
placement, every starting layout, all original structures or the whole map are
playable. No **CONFIRMED BUG** or **FIXED** production issue was established.

## Chang-an population and staffing

The independently constructed similar failure places Clay, Pottery, five
Houses, Market, Farm and Service Post, with no Warehouse. Ordinary construction
costs 1,154; all operations are Running/Normal. The demand for 20 workers starts
fully staffed from 30 residents.

| Tick | Population | Assigned / available | Taxes | Maintenance | Funds | Actual demand |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 30 | 20 / 30 | 0 | 0 | 146 | Not due |
| 400 | 30 | 20 / 30 | 0 | 40 | 106 | Five misses; 400 ticks of move-in grace remain |
| 800 | 25 | 20 / 25 | 0 | 80 | 66 | Five misses after grace expires |
| 1200 | 20 | 20 / 20 | 0 | 120 | 26 | Five misses |
| 1600 | 15 | 14 / 15 | 0 | 160 | −14 | Five misses |
| 1878 | 15 | 14 / 15 | 0 | 160 | −14 | Next demand is at 2000 |

The actual Pottery courier has no Warehouse target. Consequently House Pottery
remains zero, demand consumes nothing and grants no tax or growth. Population
declines once per House/deadline after the 800-tick grace. Treasury follows
`1300 − 1154 − 160 = −14`; no funds were injected or population repaired.

At tick 1878, Clay #1 receives 4, Pottery #2 receives 6 and Market #8 receives
4 workers. The remaining one worker cannot satisfy Farm #9's complete four or
Service #10's complete two. Both are unstaffed; the complete installed demand
is 20, five above the current 15 residents. Stable ID order at Normal priority
explains this allocation. Health and fire do not cause the observed failure.

Food and Service have independent routes: first actual House Food is tick 56,
first Service visit tick 15. In this connected similar city all five Houses
retain Food 8 and active Service at 1878. **The missing Warehouse explains the
Pottery gap, not Oliver's separate Food gap.** His full save is required to
resolve routing, staffing, placement times and prior stocks in that state.

The recorded per-House checkpoints include ID, population, effective level,
capacity, Food/Pottery/Service, health/fire, grace and actual demand outcome.
The existing Income/startup guidance already identifies the missing Warehouse,
150-Funds purchase, 164-Funds shortfall, missing House Pottery and staffing.
There is no demonstrated missing diagnosis requiring another UI engine.

## Complete supply and bounded population continuation

The existing compact paid starter fits each independently selected legal area:
Clay, Pottery, Warehouse, Farm, Market, Service Post, four Houses, FireWatch and
15 roads cost **1,280**, leaving **20**. Its 24 residents supply all 24 required
workers. All deliveries and payments come from actual production/trips/demands.

On all three maps first House Service is tick **15**, Food **71**, Pottery
**192**, and tax **400**. Houses #7–9 each pay 25 at the first deadline; House
#10 has Food and Service but no Pottery until tick **417**. Its tick-400 miss
is grace-protected and does not reduce its six residents. Through tick 2400,
#7–9 have six successes and #10 five successes/one miss: **575 total taxes**.

Dry Houses stay at effective level zero/capacity six, so the population is
24 at tick 2400 even with successful supply. This is **EXPECTED**, not a growth
bug. At that point Funds are 307 after 288 maintenance.

An ordinary **60-Funds Well** purchase at tick 2400 supplies all four Houses.
At demand 2800 population actually grows 24→28. An ordinary **120-Funds Health
Post** purchase at 2800 assigns all 26 required workers; its first actual House
arrival is 2835 on Chang-an Zhou/Handan and 2830 on Badaling. By tick 5200 each map has population **43**, capacity **46**,
taxes **1,835**, construction **1,460**, maintenance **662** and Funds **1,013**.
All four Houses have supplied demand, water, active Service and Health
protection without sickness/burning at the final observation.

The independently validated Well/Health origins are (87,47)/(91,42) in
Chang-an Zhou, (150,79)/(154,74) in Handan and (120,58)/(123,56) in Badaling.
The Badaling Health origin differs from a blind copied offset. These finite
observations do not establish indefinite sustainability or the City-v16 goal.

Two separate negative controls use normal paid construction: omit one actual
road at (84,44), observe `NoRoad`, buy it for 2 at tick 160 and obtain actual
Clay arrival at 195; and start with two Houses/12 residents, observe Farm and
Market unstaffed after Clay/Pottery use ten, buy a third House for 80 and obtain
actual Farm-to-Market/House Food at ticks 206/231. No synthetic mask, workers,
goods, courier or route is supplied.

## Same-cell placement diagnosis

Oliver's screenshot has **6 Remove** active. Its exact storage cell is
**UNVERIFIED**. The following are explicitly separate identified test points,
using the existing World validator and complete MapPermissions, not inferred
from visual grass or screenshot projection.

Chang-an cell **(80,44)** in Empty City has raw terrain `0x80`, raw objects 0,
signed height 0, off-map false, legacy 1, both permissions true, no blocker,
no original claim and an empty Sandbox object. Select is read-only; Remove
reports **No sandbox road at cell**; Road and House are accepted. All four
House cells (80,44), (81,44), (80,45), (81,45) are independently admitted.
At actual paid-supply tick 1600 the same origin contains a Road: Road is an
accepted no-op, Remove reports **Road occupied by courier**, and House reports
**Building footprint overlaps an occupied cell**. The full 2×2 report includes
the neighboring Roads, Pottery cell and empty cell.

| Map | Independent road-only point | Raw / objects / height / legacy | Road | House |
| --- | --- | --- | --- | --- |
| Chang-an Zhou | (159,62) | `0xc0` / 0 / 0 / 0 | Allowed | Legacy buildability restriction |
| Handan | (50,92) | `0xc0` / 0 / 0 / 0 | Allowed | Legacy buildability restriction |
| Badaling | (139,96) | `0xc0` / 0 / 0 / 0 | Allowed | Legacy buildability restriction |

| Map | Protected point | Height | Actual original membership | Road / House |
| --- | --- | --- | --- | --- |
| Chang-an Zhou | (219,111) | 2 | Industrial ID 1, type 162, side 0, record logical 1093809 | Protected original structure |
| Handan | (96,61) | 4 | Monument ID 4, type 256, side 4 | Protected original structure |
| Badaling | (76,85) | 2 | Monument ID 9, type 257, side 4 | Protected original structure |

The complete reports contain every raw field, off-map bit, legacy value,
building/road permission and blocker, saved original membership/provenance,
current Sandbox object and all four House cells. Empty protected points still
report **No sandbox road at cell** for Remove. Existing typed World/F1
diagnostics already distinguish these causes. No terrain whitelist, selection,
preview, picking or permission change is justified by these probes. The exact
user point and any claimed projection error remain **UNRESOLVED**.

## Normal application, walkers and persistence

The unchanged normal main/Application binary used for scripted checks has
SHA-256 `94c106f1d5c135e56d92d3b6d86b90ec46c96febf47f478001607ddc51ea1072`.
Only private observer/link instrumentation is added outside the repository's
production sources; World, snapshot, camera and asset state are not injected.
Source saves and their isolated copies are hashed before launch and after exit.

| Map | Normal application result | Sprite / marker / culling | Full supply persistence across actual OS processes |
| --- | --- | --- | --- |
| Chang-an Zhou | Similar failure through1878; normally earned supply2400, paid Well/Health continuation5200 | Actual live pipelines recorded; no original-NPC claim | Processes2244→8920;12 complete SaveDocuments from six checkpoints equal to independent controls |
| Handan | Normal New→exact checked map→Empty at5, then13 normal paid construction actions | Actual native sprite pixels, cached-alpha selection, F2 and off-viewport checks | Processes17366→17867;12 complete SaveDocuments from six checkpoints equal to independent controls |
| Badaling | Normal New→exact checked map→Empty at5, then13 normal paid construction actions | Actual native sprite pixels, cached-alpha selection, F2 and off-viewport checks | Processes16938→18824;12 complete SaveDocuments from six checkpoints equal to independent controls |

Each normal New mini first confirms zero original-derived Sandbox Buildings,
Couriers and residents. At actual tick5, construction spends478 and leaves822,
then real Clay dispatch37, Pottery arrival46 and return home56 are recorded.
Six full F5/F9 checkpoints include Empty, paid, dispatch, arrival, home and105.
The independently replayed tick5-built reference retains all fields exactly.
The UI-created paid save also undergoes separate menu loads in two fresh OS
processes, ordinary purchases of a Road2 and Warehouse150, and full identical
continuation through605: Handan21575→22415 and Badaling22749→23922.

The bounded live pipeline records Courier ID/typed role/owner/target, actual
assigned workers, phase and dispatch status, cargo/reservations, complete route,
current edge/progress/position, central live visibility, logical VisualRole,
profile/family, selected native frame/clip/flip/foot, fallback, sprite submission
and actual draw, cached alpha and topmost hit. The mini proves a genuine Clay
trip; idle Pottery having no Warehouse target is diagnosed separately.

For example, Handan tick41 draws Clay #1 on actual edge(143,76)→(143,77),
SprMain native record128, with cargo1/reservation1 and4/4 owner workers. The
actual backend raster contains530 exact original opaque RGBA pixels; an
interior opaque witness(228.5,278.5) selects that Courier through the existing
cached-alpha path. Ordinary F2 displays the marker at the same actual position
and selects the same ID without changing cargo or World. Ordinary wheel
framing then puts the active Courier outside the viewport: live state remains
valid, and no draw is submitted. Badaling separately passes those same checks.

Earlier private framing/pixel/hit-oracle failures remain in the ignored reports.
A destination just outside the actual map rectangle is a harness limit; SDL
3.4.14 software `QueueCopy` truncates its destination `SDL_FRect` to integer
`SDL_Rect`, whereas the first private inverse pixel oracle used neighboring
fractional raster cells. Correcting only that independently evidenced private
oracle produces positive pixels. No product camera, anchor, input or rounding
change was made, and these failures are not counted as engine regressions.

Badaling's separately legal, normally paid shifted near-wall city supplies an
actual original-foreground witness under explicit CLI Stone preview. At all
nine observed ticks32–48, a submitted native opaque Courier pixel is covered
by the later source Monument19/type257, original record logical1099657.
The ordinary F1 inspection/pixel selection returns that original source with
cached opacity: `SpatialCombined` body, draw cell(72,113), source height2,
GreatWall slot10/record228 and stored key(2620,−1520,StoredMap,14638) later than
the Courier key. Five full F5/F9 checks retain the entire World/context.
This is **OPENEMPEROR EXPLICIT STONE PREVIEW** of an actual original source,
with original restore context still unverified; it is not verified Emperor
rendering or a claim that Automatic selects that reconstructed body.

Handan's initial Automatic/Stone samples do **not** establish an opaque original
wall covering a Courier. A separate bounded search tests20 explicit shadow-edge
road centers beside source IDs4/24/33/43/44 using the actual Stone legacy mask,
complete policy and ordinary Clay/Pottery/nine-road validators. None permits
that complete recipe: actual terrain/legacy restrictions and unequal transport
heights reject prerequisites. All53 prepared source bodies are type256 at
signed heights4–5 (40 Wall,5 Tower,4 Gate,4 Road); their phases, orientations,
anchors, image bounds and painter keys are retained in
`maps/Handan-20-shadow/report.json`. This is not an exhaustive impossibility
proof. Handan's **original-wall foreground acceptance remains UNVERIFIED**;
its actual sprite/F2/culling and ordinary building-foreground evidence stay
separate. No permissions, heights, wall passages or draw order were changed.

The three-map normal-menu chain publishes Handan→Badaling→Chang-an Empty Cities
with their exact source SHA and corresponding policy fingerprints, each at5,
each with full F5/F9/F5 comparison. No previous map's visual or permission state
is adopted. In separate per-map roots a fully prepared load candidate with a
deliberately invalid saved policy fingerprint is rejected after genuine eager
texture preparation (1484/1310/1460 aggregate uploads). The existing session
retains the exact SandboxView/StoredRenderer pointers, all live texture pointers
and walker Asset IDs, manual target, full World/map/policy documents; Resume
and another F5/F9/F5 continue identically. Only private invalid-save copies are
modified. Original maps/assets are untouched.

Separate normal-menu Recovery tests on each map run the actual paid supply
city1200 ticks, write a real periodic autosave, load it through Recovery into a
paused linked child and compare the complete recovered F5/F9/F5 document.
The original manual save, source file, protected parent start and copied
periodic checkpoint bytes stay exact. Recovery metadata remains external to
World/save authority; no personal histories or CLI-generated recovery are used.

Every measured ordinary production render frame has zero deltas in all twelve
tracked prohibited-work counters. Physical uploads occur only during eager
preparation. Every completed scripted app ends with zero owned live textures;
no native player process is automated or stopped. Observer-only diagnostics,
software readbacks and trace files are outside production paths and do not
establish native input performance. Budget hypotheses are independently covered
by existing CityStartGuidance and SandboxGateBudgetView tests, including shared
complete policy identity, canonical fields, signed heights, gates/transport,
Cancel purity and actual commit. Those authored tests are not relabeled as new
three-map human budget acceptance.

The local `app/app-final-report.json` pins25 successful scripted normal-app
cases, all equality artifacts and eight preserved private harness failures,
with the Handan foreground limit explicitly separate.
`app/corpus-harness/application-evidence.json` pins all seven extra
normal menu/Recovery/rollback runs and their traces; per-scenario `app/*/report.json`
pins each process, observer, plan, complete checkpoints and backend witnesses.

## Corpus and positive controls

A fresh audit retains **102/167** prepared maps, **57** unknown-occupancy first
failures, **3** conflicts and **5** unsupported `cResWall` managers. Complete
diagnosis retains 623 unknown records in 13 types across 59 maps. There are no
new class or zero-side admissions. All **306** real/zero/maximal-mask policy
variants and **3,366** complete component files are byte-identical to the
previous bounded type-183 result. This covers building/road permissions,
protection, blockers, signed heights, gates, transport geometry and policy
fingerprints. The reference was generated before the user committed f5ec;
its report's historical baseline b8ad is not the current HEAD. The audit source
hash and resulting post-pass policies are the same as f5ec.

Xia's existing normally paid goal playthrough was rerun in Debug, Release and
ASan/UBSan. Each preserves all **14 complete SaveDocuments** and complete
`main-facts.jsonl` bytes against the prior final-Release reference.
Banpo and `Cities/Anyang.map` pass actual rule-3 preparation and corpus equality;
Oliver's successful play remains separate user-reported evidence.
`Cities/STGW-Anyang.map` fails explicitly on Industrial type 138. The exact
Anyang identity from Oliver remains unconfirmed; Xianyang is a distinct file.
Kaifeng retains five gates, Zhengzhou two, with unchanged complete heights,
gate passages, policy variants and existing road/gate test results. No new
human or map-specific save/playthrough PASS is inferred for these controls.

## Validation

| Current configuration | CTest | Separate Alpha-Endurance | Paid Xia goal and complete 14-save/facts equality | New fixture on all three maps |
| --- | --- | --- | --- | --- |
| Debug | 131/131 | PASS | PASS | PASS |
| Release | 131/131 | PASS | PASS | PASS |
| RelWithDebInfo + ASan/UBSan | 131/131 | PASS | PASS | PASS |

All registered existing tests executed; none failed or skipped. Build warnings,
sanitizer diagnostics and production/test/resource source differences are zero.
The matrix includes existing MapRules/MapPermissions/entity, menu/Sandbox,
staffing/economy/demand, road/height/gate, walker/composition, save/recovery and
City-v16 tests. The separate endurance and Xia runs are not inferred from the
CTest summary. The new fixture is `EXCLUDE_FROM_ALL` and does not change the
131-test inventory.

After the final diagnostic-report change, all nine Debug/Release/Sanitizer
fixture runs preserve **36 complete A/B/C/D SaveDocuments** and **nine complete
deterministic fixture reports** byte for byte against the previous positive
Release reference. Every fixture requires successful full readback and
restore, and zero owned textures before SDL teardown. The fixture source,
launcher and 210 supporting production/resource/tool sources remain unchanged
during this matrix. The sanitizer uses the project's ASan+UBSan configuration
with `detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no leak-sanitizer claim is made.

The actual versioned launcher was executed for all three exact names with
dummy/software SDL. Every run completed its Release build, fresh root, four
normally earned saves and normal-application launch, stayed alive at the menu,
then received termination only for its owned process and exited zero. This is
a launcher smoke test, separate from the scripted menu/input checks and human
acceptance. No existing native player process was operated or stopped.

Detailed local evidence:

- `core-{debug,release,sanitizer}-report.json`: exact commands, JUnit, build/test
  logs, endurance, 14-save Xia comparison and source integrity.
- `fixture-release-run-report.json`, `fixture-configurations-report.json`:
  current tool source/binary hashes, actual runs, full save/report comparison.
- `launcher-smoke-report.json`: all three actual versioned script commands,
  resulting roots/settings and owned-process lifecycle.
- `guards/guard-report.json`: 14 actual early-rejection probes for wrong or
  escaping map names, contained source paths and fresh ignored output roots.
  Owned sentinels remain byte/inode/time-identical; no app/build starts and no
  original files are used in those negative probes.
- `maps/document-facts.json`, `maps/map-evidence-index.json`,
  `maps/corpus-comparison.json`: exact map, geometry, cell/policy/asset evidence
  and unchanged complete corpus components.
- `economy/economy-report.json`: all ordinary paid scenarios, full per-House
  timelines, 83 save readbacks and deterministic 20-tick continuations.

## Reproducible player start

From the repository root, use the actual exact names:

```sh
sh tools/test_map_playability.sh .local/gog-extracted/app "Chang-an Zhou"
sh tools/test_map_playability.sh .local/gog-extracted/app Handan
sh tools/test_map_playability.sh .local/gog-extracted/app Badaling
```

Run one at a time. The versioned launcher builds the current Release main and
the on-demand excluded fixture, resolves an exact contained `Cities/*.map`
leaf, and creates a fresh `.local/map-playability/player-…` App root. It leaves
personal settings/saves/recovery and original files untouched. New Sandbox
defaults to City-v16 rule 3, Empty City; wait for **Map rules checked**, then
**Start Sandbox**. An Empty City should display scenery and no Sandbox walkers.

Optional **Load Sandbox** saves in that same root are generated only from
accepted ordinary paid commands and normal ticks:

- `A-empty`: zero Buildings/Couriers, 1,300 Funds.
- `B-paid-courier`: two Houses, Clay, Pottery and nine Roads; 478 spent,
  822 Funds, population 12, staffing 10/10. It loads paused. **Space** starts
  the actual Clay trip; first dispatch is 32 relative ticks after placement.
- `C-paid-supply`: the dry compact starter at tick 0, 1,280 spent,
  20 Funds, population/staffing 24/24.
- `D-observed-supply-1600`: the same actual supplied city after 1600 ticks,
  population 24, taxes 375 and Funds 203.

These are technical paid-command fixtures, not evidence that the user clicked
their construction or that original Emperor residents were recovered. **T /
Income** diagnoses supply/staffing, **F1** shows existing diagnostics, **F2**
compares sprite/marker presentation. No preview mode changes the saved policy.

The same uninstrumented Handan script was also launched with the ordinary
native environment in fresh root `player-Handan-4pMSDk`; process 23488 was left
available for the player. This passive launch is not a native-input or human
acceptance result, and this root contains no personal recovery history.

If the exact original map, rules or compact pattern cannot be prepared, the
helper stops with the concrete original Map-Rules/recipe error and writes
`fixture-failure.json` only after validating the fresh ignored output root.
The private diagnostic root is printed even when no application starts.
Absence of this one existing pattern does
not prove that all possible layouts are illegal. The script does not fall back
to a fabricated fully-buildable map. The actual versioned launcher was also
executed for `STGW-Anyang`: it stops with the existing Industrial type138
unsupported-occupancy record, writes only the private failure JSON and starts
no application. The positive three-map launcher checks were repeated after
that final diagnostic change.

## Explicit road connectivity check (2026-10-09)

The same launcher additionally prepares **E-road-disconnected** with ordinary
paid commands. Its Handan/Badaling Clay and Pottery each have a valid entrance
but four-cell Road components separated by offset (5,2). At tick 64 actual
Clay dispatch is NoRoad; normal Road purchase for 2 yields CONNECTED and real
dispatch 65 / arrival 114 / home 164 with cargo 2 delivered. A–D SaveDocuments and the
original fixture report remain byte-identical. An unsupported additional E
attachment is diagnosed separately without disabling A–D.

Select a courier's building, **F1 → G** to request a bounded read-only check;
repeat G checks compatible target candidates. Current workers/stock/dispatch
are separate from cached topology. No scan or BFS is added to ordinary frames
or road preview. See [road connectivity diagnostics](road-connectivity-diagnostics.md)
for measured entrances, raw edge/height facts, safe repair and evidence limits.
This is EXPECTED / DIAGNOSTICS IMPROVED; Oliver's exact failure state and a
production routing bug are not established.

## Remaining limits

Oliver's exact Chang-an save/actions/selected screenshot cell, exact Anyang
identity and Handan/Badaling Empty-versus-loaded setup are unverified. Native
human play, external-display/input acceptance, original Emperor NPC simulation,
original restore context, Handan original-wall foreground acceptance and general
scene fidelity remain separate open work.
The similar failure does not establish the cause of the reported Food gap.
Technical positive trips/supply are bounded to the listed maps/layouts/ticks.
There is no demonstrated production bug warranting a speculative rule,
population, routing, rendering or permissions change.
