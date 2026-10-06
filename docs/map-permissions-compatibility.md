# Rule-3 map compatibility: Xia and bounded original occupancy

The subsequent [profile-aware selection pass](map-selection-compatibility.md) exposes these rule prerequisites before graphical session preparation, without changing the occupancy contracts or permissions below.

This pass starts from the actual clean HEAD `50bd31e5840fc1c7345befc9146f55898b60971d` (the supplied `d48b00aec76228d8bf820e582a2271fe256e76d9` is its parent). It extends only the load-time original occupancy decision. City-v16 rule **3**, immutable map policy **1**, save schema **19**, traffic, economics, graphics and input retain their existing contracts. There is no migration or publication.

Oliver reports after the budget hotfix: **“es läuft der walker kann durch gehen” — User-reported successful gate traversal in the tested session.** This does not establish a return trip, goods balance, Save/Load restart or acceptance of other maps. The earlier [scripted gate evidence](gate-passages.md#budget-copy-crash-hotfix-2026-10-06) remains separate.

## Baseline reproduction and original inputs

Before the change, a fresh isolated application root selected City-v16 / Empty City. Scripted SDL keys drove the actual `MenuSession` New/Start path, first opening a working Kaifeng session and then selecting `Cities/Xia.map`. The complete original manager parsed successfully; `SandboxView::initialize → load_sandbox_map_permissions → prepare_sandbox_map_permissions` rejected the saved footprint. The menu caught the preparation error and retained the previous view, complete World snapshot and immutable policy pointer. Its recovery files remained byte-identical; no Xia World, manual save or recovery was published. This is production-handler evidence using the dummy renderer, not native or human input acceptance.

**RAW MAP FACT:** Xia's compressed file is 79,450 bytes, SHA-256 `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433`. Its schema-1 manager at decompressed logical offset **1,093,607** contains 4,000 records and exactly one active record:

| Field | Value |
| --- | --- |
| Class / type | `cIndustrialBldg` / 175 |
| Original ID / manager index / MFC object reference | 1 / 1 / 4 |
| Status / saved footprint side / subindex | 3 / 0 / 0 |
| Base / wrapper / non-house state schema | 4 / 1 / 1 |
| Local origin / storage origin | `(37,78)` / `(109,150)`; border 72 |
| Serialized row-major cell reference | 34,309 |
| Record / base / extended logical offset | 1,093,808 / 1,093,829 / 1,094,012 |
| Record byte length | 332 |
| Origin raw terrain / objects / signed height | `0xc0` / 0 / 1 |

These are decompressed stream positions; runtime structure offsets and PE offsets are separate address spaces. The baseline error was `unbounded active original saved footprint`. Unsupported occupancy errors now identify the source ID, manager index, class/type, status/side/subindex, schemas, coordinates/reference, logical record position and raw origin words.

The checked data revision has **1,464 files / 819,711,091 bytes**. SHA-256 of the sorted path/size/file-hash manifest is `585fd08dbc0d0957eabce180c71e9c044e7f9620f5abe981bd03dd2d19213e77`. Original inputs are read only. Private raw records, reports, saves, disassembly and captures belong under ignored `.local/map-compatibility/`.

## Bounded original evidence

The original PE32 EXE was read only, never executed: SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`, image base `0x400000`. Fourteen targeted windows verify **638 disassembly lines / 2,246 instruction bytes**, with zero byte mismatches. Address translation uses the inspected sections (`.text` RVA `0x1000`, file `0x400`; `.data` RVA `0x414000`, file `0x412600`). Model names corroborate the investigation; they do not establish occupancy on their own.

| Observation | Static VA | RVA | PE file offset |
| --- | --- | --- | --- |
| Base initializer | `0x428aa0` | `0x28aa0` | `0x27ea0` |
| Type-175 model entry | `0x824600` | `0x424600` | `0x422c00` |
| Typed editor placement dispatcher | `0x404990` | `0x4990` | `0x3d90` |
| Type-175 point placement branch | `0x404c6e` | `0x4c6e` | `0x406e` |
| Standalone physical-restore type filter | `0x52f1d0` | `0x12f1d0` | `0x12e5d0` |
| Type-165 model entry | `0x824510` | `0x424510` | `0x422b10` |
| Water terrain-cell writer | `0x4b4af0` | `0xb4af0` | `0xb3ef0` |

**EXE-OBSERVED, type 175:** the registered saved Industrial factory `0x51c140 → 0x51c9a0` supplies the base/non-house class used by the parser. Base initialization deliberately selects status 3 and the model's side zero. Editor selection `0x404244` chooses type 175; the actual `(type-22)` dispatch selects the point branch above. It validates a cell, retains old-point undo state and stores exit coordinates at `0xc5ce00/0xc5ce02`, without a body rectangle or owner-grid write in that bounded path. The standalone physical-restore filter selects its false branch for 175, bypassing physical building reconstruction. This is evidence of a special exit point, not a broken or inactive square building.

**RAW MAP FACT:** 22 type-175 records across 21 maps have Industrial/status3/side0/subindex0/wrapper1/state1, with observed base schemas 3 and 4. Some share an origin. Their current raw origins vary, including vegetation and nonzero objects. Neither a terrain whitelist nor a unique-point assumption defines this marker. Base-5 type 175 and different source states remain unsupported.

**EXE-OBSERVED, type 165:** the most frequent related unknown group is the Water editor tool. Selection `0x40412a/0x404134` binds type 165; the explicit comparison at `0x4b58a2` calls the single-cell terrain writer. It edits raw terrain/height and dirty state at the supplied coordinate, sets terrain bit `0x4`, and writes no body rectangle or owner grid in that bounded path. The model deliberately supplies side zero; the physical-restore filter again returns false. The implementation does not replay this editor operation.

**RAW MAP FACT:** all 394 type-165 records across 34 maps have Industrial/status3/side0/subindex0/wrapper1/state1/class-wrapper0; base schemas 3/4/5 occur 71/307/16 times. Current origins need not still contain water or objects zero. Saved markers and current terrain therefore remain separate facts.

**OPENEMPEROR CONSERVATIVE POLICY:** both evidenced source states reserve their exact origin as protected original structure. A marker origin is not an invented physical 1×1 building footprint. No `NoClaim` case is introduced. All remaining raw terrain protections, identity/reference checks, bounds and order-independent physical/GateHouse conflicts remain mandatory. Duplicate validated marker origins retain the existing contract.

**UNRESOLVED:** the other fourteen unknown zero-side types, unexplained physical conflicts, `cResWall`, complete original ownership/restore and Emperor gameplay parity. None is inferred from model names, graphics, sprite dimensions, decode success, map names or acceptance percentages.

## Central change and compatibility

`SandboxPlacement.cpp` has one small typed occupancy decision: saved square, conservatively reserved marker origin, or unsupported. Existing positive-square and GateHouse paths and the five marker types **162/163/174/180/185** retain their exact previous behavior. New Industrial markers require side 0, status 3, subindex 0, wrapper/non-house state 1 and no class-specific wrapper; type 175 admits observed bases 3/4 and type 165 admits 3/4/5. Invalid combinations fail explicitly and atomically.

The exact road whitelist remains `0x80` or `0xc0`, objects zero, on-map and without original occupancy. Buildings retain legacy buildability minus original protection. Marker support creates no free road, terrain clearing, demolition, extra building, new edge or altered gate opening. Classification runs only during loading. Budget queries share the already prepared immutable policy through the [repaired full-context hypothetical](gate-passages.md#budget-copy-crash-hotfix-2026-10-06); mutable World copies remain independent.

Policy **1** remains valid because every previously prepared map retains exact canonical state. The comparison checks all 77 baseline maps using two explicit diagnostic legacy inputs: zero reproduces the historical probe; one exercises every raw/occupancy-qualified building cell. Across both inputs, **1,232 canonical/component/gate files and 154 production schema-19 policy fingerprints are identical**. Components include roads, buildings, original protection, both blocker categories, signed heights and complete gate IDs/claims/corridors/openings. The building-mask input is a per-cell boolean and occupancy/topology is independent of that input; these two extremes cover its possible effect. These are policy-preparation checks, not renderer-derived mask or corpus gameplay acceptance. Actual existing gate saves are additionally checked through production loading and continuation.

No parser, World, routing, budget, persistence, Recovery, input, renderer, texture policy, economy, rules/defaults or schema implementation changes are needed. The menu's existing candidate preparation boundary continues to retain a working session when a new map fails. No partially admitted policy is published.

## Corpus: first failure versus all diagnosed objects

The historical **77/167** result was freshly reproduced at the starting HEAD. Every supported standalone map is counted once by its first actual production failure; the complete-manager diagnostic separately collects all problematic records. Unknown record layouts stop parsing, rather than guessing the next record.

| First preparation outcome | Baseline | Final |
| --- | ---: | ---: |
| A: incomplete manager / unsupported parser class (`cResWall`) | 5 | 5 |
| B: complete manager, unsupported occupancy | 83 | 66 |
| C: known occupancy conflict | 2 | 3 |
| D: unsupported gate/opening height | 0 | 0 |
| E: policy prepared | 77 | **93** |
| Other first failures | 0 | 0 |

The preparation-only probe leaves all E entries in category **F: normal start not checked by this probe**. The normal-menu and paid UI cases below are separate acceptance evidence; E is not a render or playable-corpus PASS.

Sixteen newly prepared maps are Xia, Banpo, Bo, Erlitou, MP21, MP30, MP33, MP52, MP6, MP9, MPWall3_S, MPcanal3, NavalT-Chang-an, NavalT-Hemudu, NavalT-Suzhou and SA-Shu. All 167 map hashes remain identical and no previously supported map is lost.

The conflict count rises because **MPcanal4** previously stopped at unknown type 165. Its now-reached Industrial162 marker IDs 61/62 at `(114,112)` conflict with Monument type83 ID19's saved 4×4 claim from `(112,112)`. The baseline all-object diagnosis already saw that conflict. It remains rejected, along with its other unknown records; this is not a regression in a previously prepared map.

Baseline all-object diagnosis finds 1,069 unknown zero-side Industrial records in sixteen types; 68 maps have multiple unknown records. Final diagnosis retains **653 unknown records / fourteen types / 68 maps**, plus separately diagnosed unsupported bounds or mismatched references and known conflicts. Those map sets can overlap, and are not summed as a first-failure distribution. Fixing the first unknown object never implies that the rest of its manager is supported.

## Actual Xia and countermap acceptance

The isolated production-menu acceptance uses `New Sandbox → Start Sandbox → SandboxView`, actual map MouseDown/MouseUp through normal validation and budget preflight, and ordinary paid commands. Space Pause/Resume is exercised; subsequent simulation uses existing scripted fixed ticks. This is scripted SDL/production-handler evidence, not elapsed-time, native or human acceptance.

| Empty-city map | Two House origins | Paid road drag | Clay / Pottery origins |
| --- | --- | --- | --- |
| Xia | `(113,110)`, `(116,110)` | `(113,112) → (121,112)` | `(113,113)` / `(119,113)` |
| Banpo, independent type-175 map | `(108,110)`, `(111,110)` | `(108,112) → (116,112)` | `(108,113)` / `(114,113)` |

Coordinates are storage cells selected from the actual map and permissions. Both builds pause at tick zero, place Houses, then nine normal roads **before further building purchases**, then Clay and Pottery. Cost is **478**, leaving **822 Funds**, with ordinary **12/10 workers** and no goods/funds grants. Actual Clay delivery/return is observed at ticks **66/101** and normal Pottery production completes. A budget query with the actual active courier retains the entire source snapshot, policy pointer and canonical state.

Additional ordinary House purchases open the actual untaxed reserve modal. Opening/cancelling it preserves the full source, blocks ticks and performs no render-time simulation/asset/file work; reopening and Return commits exactly one paid House/command/ID. F9 restores the original build for traffic testing.

F5/F9 after construction restores the exact schema-19 snapshot and full policy. A completely separate process loads the same save through the menu, performs the same further paid Road (2) and Warehouse (150) purchases and 600 fixed ticks, and produces identical full snapshots and serialized save bytes. The continuation starts at tick zero with 630 total spent and 670 Funds; no grants are involved. At tick 1,200 the normal menu autosave produces a verified checkpoint; normal Recovery loading restores it paused into a child history with a separate manual target and identical snapshot/policy.

Xia's existing **Prepared Starter** also starts through the normal menu at tick zero: placement origin `(103,113)`, eleven buildings, **1,280 spent / 20 Funds / 24 workers**. A normal extra road at `(103,112)` costs 2. After 800 ordinary ticks, actual tax 175 and maintenance 96 leave 97 Funds and eleven produced Pottery. A normal House purchase at `(113,111)` costs 80 and leaves 17; F5/F9 and invariants pass. Starter costs, staffing and economy are unchanged.

**Bo**, an independent newly prepared type-165 map, passes normal menu start, F5/F9, full canonical/snapshot equality and protected schema-19 start recovery. Industrial165 ID/index10, base4/wrapper1/state1 at `(80,136)`, raw terrain4/objects0, retains a protected origin that rejects roads and Houses. This is start/protection/save acceptance, not a full production-city or visual-parity result.

Combining the separate acceptance runs gives **five distinct normal-start maps** (Xia, Banpo, Bo, Kaifeng, Zhengzhou), **four paid empty-city UI production builds**, and the additional Xia Prepared Starter. Thus **88** of the 93 prepared maps have no normal-start acceptance in this milestone. No corpus-wide render acceptance is inferred from either count.

## Gate, regression and package validation

Synthetic tests exercise every newly admitted class/type/schema combination and invalid schemas/classes/status/subindex, identities/references, bounded origins, off-map terrain, source protection, duplicate markers, and physical/gate solid/corridor/opening conflicts in both record orders. Their positive expectations are literal authored claims, not calls to the classifier as an oracle. The new exit-origin regression fails against the frozen baseline library and passes against the final producer. Focused final MapPermissions tests pass without compiler warnings; an independent source/evidence review finds no correctness blocker.

Original-data gate regressions cover Kaifeng and Zhengzhou paid UI builds, live gate courier budget purity, complete policy, F5/F9 and process continuation. Each retains the previous 476 spent / 824 Funds / eight paid roads and three fixed corridor cells. Actual delivery/return remains Kaifeng **66/101** and Zhengzhou **61/91**. Newly saved inside-passage continuations match after a separate process and 500 ticks; both existing pre-change schema-19 inside saves additionally load and produce byte-identical 500-tick saves against the earlier uninterrupted results. All baseline road/building/protection/blocker/height/gate/fingerprint values remain exact, including forbidden side entry. Input/zoom/scene composition and bounded frame-work contracts remain independent evidence.

| Final configuration | Full suite | CTest time | Warnings / skips / sanitizer findings |
| --- | --- | ---: | --- |
| Debug | **108/108 PASS** | 775.84 s | 0 / 0 / none |
| Release | **108/108 PASS** | 97.41 s | 0 / 0 / none |
| RelWithDebInfo ASan/UBSan | **108/108 PASS** | 264.57 s | 0 / 0 / 0 |
| Separate macOS arm64 Release package | **108/108 PASS** | 59.05 s | no compiler warnings / 0 skips |

The full suites include parser/MapPermissions, budget/UI, gate passages, RoadBatch, Save/Recovery/process restart, determinism/endurance, input, zoom and scene composition. ASan uses the existing macOS `detect_leaks=0` configuration; UBSan halts on errors. All **300** frozen source/test/resource/build/tool files remain identical through final validation. `git diff --check` passes. All 1,464 original files retain the complete starting hashes. Existing user settings, saves and recovery roots are not changed by the isolated tests.

The current dirty local `0.1.0-alpha.2` bundle passes dependency containment, relocation/unzip, ad-hoc signature verification and thirteen negative package cases, with no original assets or personal state packaged. A further **packaged Xia City-v16 rule-3 Prepared Starter** check passes 1,200 ticks, production/economy and direct/continued save equality with empty stderr; signature verification still passes afterward. Staging dependency fixups report two expected signature invalidations before inside-out signing; final signatures are verified. The local ZIP SHA-256 is `9c7f619bbad54c2e2ba412d73f66ff315bfbaa2bf742b68162b16bd4da035e14`. This is package validation, not publication or native/human acceptance.

The actual repository starter was exercised through its current build and production `Application` menu loop using isolated dummy/software processes and a test-only SDL event-delivery observer. Xia with the exact command below, a data path containing spaces, Banpo, Kaifeng and Zhengzhou all start and display **rule 3 / map policy 1**; the test processes quit normally and are stopped. The observer is private test instrumentation, not linked into the application or package. These delivered scripted events remain separate from native and human input evidence.

## Start a fresh Xia test

From the repository root:

```sh
sh tools/test_gate_passages.sh .local/gog-extracted/app Xia
```

The repository starter builds current Release, chooses City-v16 for new games (rule **3**), selects Xia and creates a fresh isolated settings/save/Recovery root. Choose **New Sandbox**, then **Start Sandbox**. Confirm **rule 3 / Map policy 1** in the window title/HUD. Begin with Empty City; the Prepared Starter toggle uses the same current policy. The existing Kaifeng/Zhengzhou invocations remain available, and Banpo is an independent point-marker countermap. Data paths are shell/JSON quoted, including spaces.

Visible success is a paid road followed by a further building purchase without rejection/abort, normal Clay delivery and Pottery production, then an unchanged F5/F9 restore. For the small setup above, ordinary Food/Service/Water/Health infrastructure is still required for extended city play. Full map fidelity, original elevation/composition parity and the separate human native-input checklist remain open.
