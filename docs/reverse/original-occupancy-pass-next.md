# Original occupancy pass: bounded Industrial type 183

This pass begins at clean HEAD `b8ad2ee059692e5d442a79b8e6f93e831a97070d`. It extends one load-time occupancy contract, with City-v16 rule **3**, map policy **1** and save schema **19** unchanged. Original-data reports, record details, disassembly and test state stay under ignored `.local/original-occupancy-next/`. Original inputs are read only; no original executable is run.

## Fresh baseline and candidate selection

The freshly compiled production-parser audit checks all **167** standalone maps in the local GOG tree: **93 Prepared / 66 Unknown Occupancy / 3 Known Conflict / 5 unsupported cResWall**. Complete-manager diagnosis separately finds **653 unknown records / 14 types / 68 maps** with at least one unknown record. The 66 first-failure maps and the 68 all-record maps are different, overlapping diagnostic metrics. Parsing stops atomically at unknown classes/schemas; no later records are claimed after a manager error.

The input revision contains **1,464 files / 819,711,091 bytes**, with fresh audit manifest SHA-256 `dde2382fe1c348da765c8776149a73210b81f6cba7f93a0c6159d79ce3154feb`: sorted relative path, tab, decimal size, tab, SHA-256, newline. All 1,464 path/size/hash entries match the independent starting inventory exactly; the manifest serialization differs from older report formatting. The audit retains each exact relative map path, compressed SHA-256, parser result, source state/provenance, first actual error, known conflicts and preparation result.

The table below ranks conditional map impact before choosing a type. Every row has class `cIndustrialBldg` (Industrial). “Conditional” is an explicitly labelled private diagnostic: the copied candidate group is treated as the existing conservative marker contract, while all other records remain exact, solely to run the normal rules validator. It establishes which maps still have other blockers, **not** original semantics, actual support, or published permissions. Unknown types are never exempted from production validation by this diagnostic.

| Class | Type | Records | Maps | First failures | Other unknown types on those maps | Known-conflict maps | Conditional maps |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Industrial | **183** | 30 | 11 | 9 | 138,164,173,177,178,181 | 1 | **9** |
| Industrial | 173 | 68 | 12 | 9 | 138,151,164,167,177,178,179,181,183,187 | 1 | 8 |
| Industrial | 177 | 22 | 16 | 11 | 138,151,164,167,173,178,179,181,183,187 | 1 | 5 |
| Industrial | 187 | 214 | 11 | 10 | 167,173,177,178,179,181,188 | 0 | 5 |
| Industrial | 178 | 17 | 15 | 6 | 138,151,164,167,173,177,179,181,183,187 | 1 | 5 |
| Industrial | 164 | 80 | 8 | 5 | 138,151,173,177,178,181,183 | 2 | 5 |
| Industrial | 179 | 32 | 8 | 6 | 151,167,173,177,178,186,187 | 0 | 3 |
| Industrial | 186 | 9 | 4 | 3 | 179 | 0 | 3 |
| Industrial | 189 | 14 | 2 | 2 | — | 0 | 2 |
| Industrial | 181 | 19 | 3 | 1 | 138,164,167,173,177,178,183,187 | 1 | 1 |
| Industrial | 188 | 10 | 2 | 1 | 187 | 0 | 1 |
| Industrial | 138 | 7 | 2 | 1 | 164,173,177,178,181,183 | 1 | 0 |
| Industrial | 151 | 13 | 3 | 1 | 164,173,177,178,179 | 0 | 0 |
| Industrial | 167 | 118 | 5 | 1 | 173,177,178,179,181,187 | 0 | 0 |

Type **183** is selected because it has the highest potential effect on complete maps, a bounded common saved state and independently inspectable point/restore paths. Its 30 records occur on Jiangxi, Liangzhou, Liangzhou_S, MP4, MPcanal4, NavalT-Lanzhou, Niya, STGW-Anyang, STGW-Baoji, Ying and Yulin. The nine conditional successes are all except MPcanal4 and STGW-Anyang. MPcanal4 retains known conflicts and several other unknown types; STGW-Anyang retains unknown type 138. No second type is investigated or admitted.

## Original evidence and address domains

The pinned PE32 `Emperor.exe` has SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`, image base `0x400000`. `.text` RVA `0x1000` maps to PE file offset `0x400`; `.data` RVA `0x414000` maps to PE `0x412600`. A fresh bounded `objdump` extraction checks **22 windows / 648 instruction lines / 2,224 instruction bytes**, with zero byte mismatches, plus explicit selector/model table checks. Full decompilation is not used. Runtime VAs, PE offsets and decompressed map offsets below are separate address spaces.

| Observation | Runtime VA | RVA | PE file offset |
| --- | --- | --- | --- |
| Industrial MFC descriptor | `0x854438` | `0x454438` | `0x452a38` |
| Saved Industrial factory | `0x51c140` | `0x11c140` | `0x11b540` |
| Industrial constructor | `0x51c9a0` | `0x11c9a0` | `0x11b9a0` |
| Shared initializer | `0x428aa0` | `0x28aa0` | `0x27ea0` |
| Type-183 registered model | `0x8246c0` | `0x4246c0` | `0x422cc0` |
| Type-183 editor selection | `0x4043e9` | `0x43e9` | `0x37e9` |
| Editor placement selector | `0x4051cd` | `0x51cd` | `0x45cd` |
| Selected point-placement branch | `0x404f0a` | `0x4f0a` | `0x430a` |
| Existing-point lookup branch | `0x403faf` | `0x3faf` | `0x33af` |
| Point undo application branch | `0x53e614` | `0x13e614` | `0x13da14` |
| Physical-restore selector | `0x52f260` | `0x12f260` | `0x12e660` |
| Selected false-return branch | `0x52f1f0` | `0x12f1f0` | `0x12e5f0` |

**EXE-OBSERVED — registration/construction:** the MFC descriptor names `cIndustrialBldg`, supplies object size 336 and points to saved-class factory `0x51c140 → 0x51c9a0`. The constructor installs vtable `0x7b65e4`; its initializer at virtual offset `+0x94` is the shared base initializer `0x428aa0`. That initializer deliberately writes active status 3 to runtime `+0x04`, reads the registered type's footprint byte and writes it to runtime `+0x07`. The type-183 model deliberately supplies zero. The Industrial serializer `0x51ce00` uses the shared base serializer, its own wrapper and embedded NonHouse state, already bounded by `OriginalMapEntities`; no new parser or record length is inferred.

Model file `Model/EmperorBuildingModels.txt`, SHA-256 `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`, labels the type `BUILD_MAP_PREY_POINT`. This corroborates the investigation; neither the name nor side zero independently establishes occupancy semantics.

**EXE-OBSERVED — actual point placement:** editor selection `0x4043c6..0x404403` explicitly binds type 183 and a point slot 1..4. The ordinary typed dispatcher `0x404990` indexes `(type - 22)`; type 183's selector byte is 14, selecting `0x404f0a`. That branch validates the point's terrain through `0x59cbb0 → 0x59c780`, retains the old coordinates through `0x53e520`, and writes X/Y at `0xc5ce38 + 4*slot` / `0xc5ce48 + 4*slot`. The zero-based coordinate arrays begin at `0xc5ce3c` / `0xc5ce4c`. The bounded branch writes no rectangle, building-owner grid or physical body. Independent existing-point lookup selects byte 8 at `0x40403f → 0x403faf`, reading those same coordinates. Undo application selects byte 8 at `0x53e693 → 0x53e614`, writing the same point coordinates.

**EXE-OBSERVED — standalone physical restoration:** saved-record loop `0x52f030` tests status, then calls `0x52f1d0`. Type 183 selects byte 1 and false-return `0x52f1f0`; the caller skips its subsequent physical reconstruction through `0x42d540` and physical placement `0x4b11f0`. This independently distinguishes the saved source from a damaged zero-size square building. It does not prove complete original restoration or fauna behavior.

## Saved source facts

**RAW MAP FACT:** all 30 type-183 records are Industrial / status 3 / saved side 0 / subindex 0 / wrapper 1 / NonHouse state 1 / class-wrapper 0. Seven use base schema **3**, and 23 use base schema **4**. Base 5 and other source states are not evidenced. Raw origin terrain varies between `0` and `0x80`, objects between 0 and 1, signed saved height between 0 and 1. Current terrain and marker existence are separate facts.

Jiangxi is a compact base-4 source: compressed length **92,152**, SHA-256 `a49df3f2ad3540372ab40f594d6c0e7f20fbaa21db964f7212cd211ea78cb3ba`. Its type-183 ID/index 1 record starts at decompressed logical **1,093,808**, base **1,093,829**, NonHouse state **1,094,012**. Local `(13,74)` maps through border 44 to storage `(57,118)`, serialized row-major reference **26,961**, terrain `0x80`, objects 0, signed height 0. These are decompressed map positions, never EXE addresses or compressed-file offsets.

Independent base-3 source MP4 has compressed length **86,846**, SHA-256 `1cc13baad62113b42b9ffcb355f3f40ad3a44a0333a56f7fe3da91054c50b3f5`. Its first type-183 ID/index 6 starts at logical **1,095,380**, base **1,095,382**, NonHouse state **1,095,563**; local `(53,107)` maps to storage `(97,151)`, reference **34,525**. It contains three matching records. Some other maps contain five active saved type-183 records: four editor coordinate slots do **not** imply a four-record manager limit or permission to discard earlier saved origins.

## Bounded occupancy decision and remaining uncertainty

**OPENEMPEROR CONSERVATIVE POLICY:** reserve the exact saved origin through the existing `MarkerOrigin` category for only Industrial / type 183 / status 3 / side 0 / subindex 0 / base **3 or 4** / wrapper **1** / extended **1** / class-wrapper **0**. Protect every validated source origin, retaining the existing duplicate-marker contract. This is no invented physical 1×1 building footprint and no `NoClaim` exception. No original editor operation is replayed.

Complete-manager parsing, identity/reference consistency, bounded/on-map origins, physical/gate conflicts, terrain protection and every previous claim rule remain mandatory. Other variants of the newly admitted zero-side contract stay unsupported. The independent existing positive-square contract stays exact. No map name, coordinate, historical graphic, current terrain/object word or raw height becomes a type-selection shortcut.

**UNRESOLVED:** every other previously unknown type, fauna spawning, complete original ownership/restore and Emperor gameplay/rendering parity. Preparation success, ordinary application/paid construction/save/recovery success and human play acceptance remain separate evidence. The before/after audit and exact preservation of all 93 existing policy states determine whether retaining policy 1 is valid; conditional impact alone does not.

## Measured corpus result and policy preservation

The complete production audit, after the exact new state is admitted, measures:

| First actual preparation outcome | Before | After |
| --- | ---: | ---: |
| Policy prepared | 93 | **102** |
| Unknown occupancy | 66 | **57** |
| Known occupancy conflict | 3 | **3** |
| Unsupported `cResWall` manager | 5 | **5** |
| Other first failure | 0 | **0** |

All nine predicted maps actually prepare: **Jiangxi, Liangzhou, Liangzhou_S, MP4, NavalT-Lanzhou, Niya, STGW-Baoji, Ying and Yulin**. No previously prepared map is lost. Complete-manager diagnosis retains **623 unknown records / 13 types / 59 affected maps**, distinct from the 57 unknown first failures. Type 183 accounts for exactly 30 removed unknown records, without admitting another type.

No new known conflict or newly exposed first-failure type appears. MPcanal4 still first rejects its already known complete-occupancy conflict; STGW-Anyang still first rejects Industrial138 ID1. The three conflict maps remain **Luoyang WJin, MPcanal4 and SAO-Guangzhou**. The five atomic `cResWall` manager failures remain **Chang-an Sui, MP34, MP56, STGW-Xiangjun and Xiangjun**; their active-count fields are unknown and no subsequent records are reported. All other rejected maps retain explicit failure details in the full before/after lists.

All **93** old maps are compared with **three identical legacy inputs each**: their real singleton mask after ordinary `StoredMapSession` and `StoredGraphicsRenderer::initialize`, plus the explicit all-zero and all-one diagnostic boundaries. All **279 policy variants / 3,069 output files** are byte-identical: canonical permissions, literal legacy masks, roads, buildings, original protection, both blocker categories, signed heights, complete gate IDs/footprints/corridors/openings, all four cardinal edge-blocker results and schema-19 policy fingerprints. Thus policy **1** is preserved; no schema change or migration is required. All 102 final maps also complete actual renderer-derived legacy-mask and policy preparation. This is load-time preparation evidence, not rendered frames or corpus gameplay acceptance.

The versioned on-demand target `openemperor-audit-original-occupancy` uses only the shared production readers and validators. It collects every required unknown-record provenance field after a complete manager parse. Private candidate probes are labelled, never publish a partial World, and do not bypass production admission. Records remain bounded by the existing 4,000-record manager; diagnostic interacting pairs additionally stop at 65,536 unique candidates and explicitly report incomplete pair diagnosis if exceeded. Actual corpus maximum is 373 active records and twelve interacting pairs per map, so all diagnoses are complete. No per-frame or per-tick path is added.

The ignored reproducible audit, full before/after lists and priorities are under `.local/original-occupancy-next/corpus/`. The final `baseline-bounded/` and `after-bounded/` use the same final audit object against frozen baseline and current production libraries; `run-proof-bounded.json` also verifies the 3,069 original before-edit permission files against that baseline rerun. `comparison-bounded.json` checks the complete old-policy files, `countermaps.json` checks Xia/Kaifeng/Zhengzhou and negative maps, and `admission-proofs.json` additionally checks 22 actual early `check_map_rules` requests. Individual guarded diagnostics verify every one of the 30 actual type-183 origins, including the two maps that remain unsupported; those individual probes do not establish a full policy for either rejected map.

An independent final repeat compares **3,369 deterministic files** byte for byte: the corpus, priorities, input inventory and all 102 maps' three-mask outputs. There are zero differences (`determinism-bounded.json`). Nondeterministic timing/RSS metrics are separate. The full 167-map load-time audit measures **26.063 seconds / 227.14 MiB peak RSS** before and **28.611 seconds / 220.80 MiB** after. It creates zero Worlds and renders zero frames. These are local whole-audit measurements, including existing asset preparation, not a benchmark of the twelve-line classifier alone or frame throughput.

## Regression before the bounded extension

The authored `SandboxMapPermissionsTests` case first fails against frozen baseline production libraries at Industrial183/base3 with `unsupported active original occupancy`. The compile has zero warnings; the source producer still has its baseline SHA when the failure is captured. The final producer passes the same complete test executable, retaining all earlier cases.

Positive expectations explicitly protect only the literal saved point and preserve neighboring permissions and signed height. Both observed bases are covered. Independent negative mutations reject other active statuses, class/schema/wrapper/state/subindex, identity/reference mismatches, out-of-bounds/off-map origins and physical/gate solid/corridor/opening overlap in either record order. Existing validated duplicate points and inactive-record behavior remain exact. Base5 zero-side type183 stays unsupported; the pre-existing positive-square rule is unchanged. No parser, simulator, economy, routing, save implementation, renderer, assets or input implementation is changed.

## Normal application, paid construction and persistence

**SCRIPTED SDL / TECHNICAL APP PASS:** the unchanged normal Application/main is tested in five separate owned processes under SDL 3.4.14 dummy/software, with scripted `SDL_PollEvent` key/click delivery and an ignored read-only observer. Observer serialization and expected-file reads stay outside the measured renderer. No World state, goods, workers or funds are injected. This is normal menu/UI execution, separate from native event delivery, original-scene visual parity and human play acceptance.

Both **Jiangxi** (base 4) and independent **MP4** (base 3) execute Main Menu → New Sandbox → City-v16 rule 3 → actual map selection → successful early `Map rules checked.` → Empty City → Start Sandbox. The fresh default remains City-v11; the test changes it through the normal menu. Normal loading advances the ordinary clock before Space pauses at actual tick **5**, which is retained in all records. Each type-183 origin is tested with House and Road clicks; all reject `Protected original structure` with the full WorldSnapshot unchanged. A separate forbidden-terrain Road click also rejects without spending or mutation.

The paid sequence on each map places Houses at storage **(110,112)** and **(113,112)**, then nine Roads at **(110..118,114)**, then Clay Source **(110,115)** and Pottery **(116,115)**. The **Road first → Building** sequence uses the ordinary budget/UI path. Thirteen accepted commands spend **478** of the normal **1,300**, leaving **822** funds, with twelve available and ten assigned workers. No free construction or rescue grant is used. F5 → F9 → F5 preserves every typed WorldSnapshot member, map hash, complete save context, rule 3 / policy 1 / schema 19, fingerprint and entire serialized SaveDocument.

Jiangxi's actual normal F5 source remains unchanged in its owned root. Each continuation root receives an explicit private byte copy before process creation and loads it through the normal menu; no personal save is touched. The copy operation and complete typed load equality are recorded, while a separate hash of each private copy immediately before launch was not persisted. Two separate OS processes buy Road **(119,114)** for 2 and Warehouse **(113,115)** for 150, then run the same **600** ordinary ticks to actual tick **605**. Both complete snapshots/save contexts and both entire F5 SaveDocuments are identical after the purchases and after continuation. Actual Clay ID 3 extracts **18**, Pottery ID 4 completes **6** recipes and Warehouse ID 5 stores **6** Pottery; an actual Clay courier follows the constructed road with cargo/reservation 2. Funds are **646 = 1,300 − 630 construction − 24 ordinary maintenance**. This is a partial productive city; Food, Service, water and the City-v16 goal are not supplied or claimed.

The separate Recovery process loads that same initial paid save, runs **1,200** ordinary ticks to **1,205**, creates the actual periodic autosave, and selects it through normal Menu Recovery. It starts paused with a new manual target and linked child history. Full snapshot/context and the complete periodic/recovered F5 JSON match; recovered F5/F9 also matches. The original manual save, protected start and parent checkpoint remain unchanged.

Across the five successful processes, **858 App frames / 708 measured Sandbox frames** retain zero deltas for all twelve production-render counters: World copies/restores/executes, road plans, route refreshes, BFS calls/visits, asset decode, texture upload, file reads/writes and simulation ticks. Existing initialization uploads are **1,406** for Jiangxi and **1,316** for MP4, stable during each render interval; Recovery performs two separate existing preparations. Owned texture cleanup reaches zero, all owned automation processes exit, and no takeover occurs. Evidence is under `.local/original-occupancy-next/app/application-validation-report.json`, `source-input-audit.json` and `EVIDENCE.txt`. An earlier private expected-message capitalization failure is retained and correctly labelled as a harness failure; the full normal case is rerun successfully without a production change.

## Final validation and unchanged authority

All three complete project configurations pass against the same frozen **354 source/tool/resource/build files**, with zero warnings, failed or skipped tests, sanitizer diagnostics or source changes during testing. Existing CTest registration stays **131**; no test is removed or weakened. The on-demand audit and existing paid-Xia helper are also built in every configuration.

| Configuration | Full CTest | Separate Alpha Endurance | Existing paid-Xia goal helper |
| --- | --- | --- | --- |
| Debug | **131/131**, 658.58 s | PASS, 415.93 s | PASS, 140.67 s |
| Release | **131/131**, 60.73 s | PASS, 23.88 s | PASS, 3.69 s |
| Project ASan/UBSan, RelWithDebInfo | **131/131**, 184.04 s | PASS, 95.46 s | PASS, 42.98 s |

The full suites include OriginalMapEntities, SandboxMapPermissions, MapRulesPreflight, MenuMapRules, Road/Gate, budget/purchase, City-v16, save/recovery and deterministic continuation coverage. Actual Xia input is used for the unchanged ordinary paid goal sequence in each configuration. Each run compares **all fourteen complete earned SaveDocuments** and `main-facts.jsonl` byte for byte against the previous immutable Release run `.local/city-v16-playthrough/final-release-Xia-m6zsrv30`: zero differences across all three configurations. The previous goal first succeeds at tick 6,800 and continues to tick 9,200 without new purchases; the new classifier does not change any authoritative Xia state.

The new on-demand audit is additionally executed under the project ASan/UBSan build against all 167 actual maps: PASS with zero diagnostics, **172.241 seconds** including process startup, **603.97 MiB peak RSS**, zero Worlds/frames. All **3,369** deterministic output files are byte-identical to the final Release audit. Timing/RSS remain separate; the instrumented audit is not compared as a frame-performance benchmark. Evidence is `corpus/sanitizer-run-report.json`.

Xia, Kaifeng and Zhengzhou retain their complete real/zero/one-mask permissions, signed heights, gate footprints/corridors/openings, cardinal traffic edges and full policy fingerprints. Existing gate road/save regressions also pass in all three complete suites. Continuing unknown occupancy, all three known conflicts and all five `cResWall` managers still fail explicitly and atomically. Parser, simulation, economy, routing, save implementation, renderer, input and compatibility resources remain unchanged; the sole production source difference is the twelve-line bounded Industrial183 branch.

`final-debug-report.json`, `final-release-report.json` and `final-sanitizer-report.json` under ignored `.local/original-occupancy-next/` preserve commands, logs, timings, JUnit results, source inventories and whole-Xia comparisons. `final-validation-report.json` consolidates these results, the corpus proofs, normal-App evidence and exact executed player starter with artifact hashes. Independent App and final integrity checks rehash all **1,464 original files / 819,711,091 bytes** with zero changed, missing or unexpected files and match the same frozen source inputs. Of the 314 baseline source/test/resource files, only the classifier and its regression test change; simulation/save/render/input/resources remain exact. HEAD remains `b8ad2ee059692e5d442a79b8e6f93e831a97070d`; all changes remain local and uncommitted. Historical compatibility baselines remain in [map-permissions-compatibility.md](../map-permissions-compatibility.md).

## Reproducible player test

The following exact command has been executed from the repository root, built current Release successfully and opened a fresh isolated native app root:

```sh
sh tools/test_rule3_map_compatibility.sh .local/gog-extracted/app Jiangxi
```

The versioned starter validates the data/map paths, builds current Release with system SDL, and uses a fresh `.local/original-occupancy-next/player-Jiangxi-*` root with City-v16 and Empty City selected. It reads the original map unchanged and enters the normal main menu, retaining the ordinary rule-3/policy-1 preflight and session path. The executed root is `player-Jiangxi-OEdNV4`. This uninstrumented native launch is left available for human use; it is a launch check, not a native-input or human-play PASS.

Choose **New Sandbox**, confirm **City-v16 / Jiangxi / Empty City**, wait for **Map rules checked.**, then **Start Sandbox**. A visible success is the ordinary empty Jiangxi session without an occupancy error, followed by paid Houses, Road first, Clay Source and Pottery on permitted ground, and F5/F9 roundtrip. Protected original points and unsupported terrain remain unavailable. Human acceptance of the new maps remains open; no Emperor parity is claimed.
