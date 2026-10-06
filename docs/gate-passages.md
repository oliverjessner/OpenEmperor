# City-v16 rule 3: roads and fixed gate passages

This authored sandbox integration starts from `479076a79aebc0b10baf1f310d7f65bee6df0530`. It connects paid roads through supported original GateHouse objects; it does not reconstruct Emperor military permissions, gate operation or courier rules. Oliver's positive feedback on the two gate maps is **User-reported visual acceptance**, not passage/routing or native-input acceptance.

## Blockers and evidence

The screenshot used City-v10. Its exact drag start/end and first blocker are unknown, so it does not establish a gate-only cause. The unchanged legacy `sandbox_buildable_v1` mask requires exact `0x80` terrain, object word zero, on-map geometry and a successfully rendered historical singleton footprint. Improved gate pictures do not grant road permission.

A separately specified real Kaifeng probe at `x=99`, `y=116..126` encounters `0xC0 / objects 0` before and after the gate and `0x80C8 / objects 0` on its three corridor cells. Legacy rejects the outside road-marked ground already at `(99,116)`, despite its rendered singleton; gate cells also fail. Rule 3 admits eight paid roads and three fixed passage cells. This is an independent probe, not a reconstruction of Oliver's drag.

## Immutable map contract

New City-v16 games use rule **3**, inheriting rule 2's economy, maintenance, services and **2×2 Well/HealthPost** footprints. The general menu default remains City-v11 rule 3. City-v16 rules 1/2 and every older profile retain their old map/routing/save semantics. There is no automatic migration or copy upgrade.

`simulation::MapPermissions` holds road permission, building permission, protected original occupancy, signed saved height and fixed gate topology. It is prepared once from complete bounded original sources. World reads neither SDL, assets, alpha, render status, camera nor F8 state. The vector-mask World API represents an authored synthetic rule-3 fixture with zero heights and no gates; real sessions and loaded saves use the original-data producer.

Policy **1** admits paid roads on exactly `0x80` or the diagnosed `0xC0` ground-plus-road marking, with object word zero, on-map and without original occupancy. Original road markings do not automatically create sandbox roads. No other terrain bits are admitted: water, vegetation, walls, rocks, Pinnacles, unknown combinations and off-map remain blocked. Buildings retain the old strict mask with protected original occupancy removed; their terrain permission is not expanded.

Active original square footprints are conservatively protected independently of their pictures. The bounded zero-side Industrial map-marker types 162/163/174/180/185 additionally protect their exact origins without an invented square footprint. Duplicate marker origins are allowed; physical/gate conflicts fail. Unknown zero-side sources, manager classes, schemas, references and incomplete/conflicting claims reject preparation explicitly. Other original maps can therefore remain unsupported for rule 3.

## Gate geometry and commands

A complete active type-130 GateHouse source supplies a distinct original ID, exact origin/reference, supported signed layout and fifteen nonoverlapping raw `0x8008` cells. Graphic IDs, gate bits alone and decode activation do not select navigation. All fifteen cells remain building-protected.

| Saved layout | Protected footprint | Authored corridor relative to origin | Outside openings |
| --- | --- | --- | --- |
| 0 | 5×3 | `(2,0) → (2,1) → (2,2)` | `(2,-1)` and `(2,3)` |
| 1 | 3×5 | `(0,2) → (1,2) → (2,2)` | `(-1,2)` and `(3,2)` |

These short-axis passages are perpendicular to the evidenced wall axis in the existing ground/component presentation. They are **OpenEmperor-authored traffic rules**, not fifteen walkable render components or alpha-derived tunnels. Unsupported height variation in a gate/its openings rejects preparation; ordinary transport edges require equal signed heights.

`PlaceRoad` on a passage is an explained free no-op with the existing single-command sequence behavior. `RemoveRoad` rejects it with `Original gate passage cannot be removed.` Gates add no workers, goods, maintenance, courier, operation state or demolishable sandbox building.

The unchanged X-then-Y drag has at most 256 cells. Central validation checks cells and consecutive edges, including proposed outside roads, and reports the first blocker in drag order, at most sixteen additional blockers and each cell's result. Invalid batches show actual blockers red and safe pieces subdued, without trimming or partial commits. Funds-only errors retain the exact planned count/cost. Only new normal roads pay, increment road counters or mutate occupancy. A changed batch uses one transaction copy and at most one final refresh; an all-existing/fixed batch needs neither. Preview performs no copies, commands, BFS, files, decode or upload.

## Traffic and visuals

One edge predicate covers Road↔Road, each gate's exact Road↔opening and adjacent corridor cells of the same gate. It excludes solid parts, side entry, foreign-gate links and direct building service from inside the passage. Building entrances still require normal roads. Dispatch, return, passage-start reroute, caches, movement, invariants and restore use the same graph and deterministic neighbor order.

Paths retain every corridor cell with ordinary time per adjacent edge. Removing a future outside road preserves cargo, reservation, actual position and a begun edge. At the next safe waypoint the courier reroutes or waits until a changed road revision; repair permits ordinary continuation. No teleport or rescue is added.

Gate sprites/floors, heights, anchors, alpha picking and painter remain unchanged. Passage cells never receive a road replacement, including valid preview. Outside road graphics count only allowed openings as neighbors. Couriers use actual path positions and existing height projection; roofs may obscure them. Inspection reports fixed passage and live A/B connections, while selected visible couriers retain their own inspection precedence. This does not establish full original scene parity.

## Save binding

Schema **19** is exclusive to City-v16 rule 3. It retains original map SHA256, legacy building-mask SHA256 and ordinary World snapshot. `profiles.map_permissions` adds exactly `version` and `sha256`, never supplied transit links. SHA256 frames the policy version, map hash and canonical state: row-major flags, signed heights and blocker categories; sorted gate IDs/footprints; ordered corridors and openings. Explicit classic-locale values replace pointers/STL layout bytes.

Loading reconstructs policy from original data and checks fingerprints before publishing World. Unknown policy or changed permissions/topology/height/map/mask fails explicitly. Runtime captures share the prepared immutable policy for checked writes/recovery without serializing it. Recovery metadata follows verified save read-back. Schema 18 and earlier retain their exact old path and hashes.

## Verified paid gameplay and validation

Both actual maps were tested through production loading, central RoadDrag/World commands, ordinary staffing, extraction, delivery, return and Pottery production. Each new empty city starts with the normal 1,300 Funds: eight roads cost 16, Clay Source 120, Pottery 180 and two Houses 160, totaling **476**, leaving **824**. The three passage cells add no road or cost. Removing the far outside opening makes the loaded courier wait with its cargo and target reservation intact; restoring that road costs the ordinary additional 2. The courier then delivers, returns to its original owner and actual Pottery production completes. No funds, goods, workforce or progress were granted.

| Actual map / gate | Paid road drag | Clay / Pottery origins | Fixed corridor | First repaired delivery / return tick |
| --- | --- | --- | --- | --- |
| Kaifeng / original ID 1, layout 0 | `(99,116) → (99,126)` | `(100,117)` / `(100,123)` | `(99,120) → (99,121) → (99,122)` | 86 / 121 |
| Zhengzhou / original ID 5, layout 1 | `(120,102) → (130,102)` | `(122,103)` / `(127,103)` | `(124,102) → (125,102) → (126,102)` | 81 / 111 |

Schema-19 saves before entry, inside the passage, while blocked and after repaired exit restore identical snapshots and identical further 300-tick continuations on both actual maps. Authored tests additionally cover both directions, ordinary time for every corridor edge, partially begun edges, returning interruptions, resumed path origins, historical removed prefixes, autosave/recovery and a separate process. The 44 isolated legacy fixtures (11 City-v10, five City-v16 rule 1, 28 rule 2) retain identical serialized documents and identical immediate/1,000-tick continuation against the starting implementation.

The initial gate-pass implementation passed **107/107 tests without skips** in Debug, Release and ASan/UBSan, including input, zoom, composition, RoadBatch and save/recovery checks. Existing alpha checks also passed 100,000 deterministic ticks with twelve save checkpoints, 3,000 render frames with zero further decode/upload and 100 sessions with all owned textures released. The isolated macOS arm64 candidate passed another full 107-test Release suite, bundle/signature/dependency checks, relocation/unzip, thirteen negative package cases and legacy Xia process-restart checks. These historical results did not sufficiently cover the building-purchase budget path described in the hotfix below. The package remains a dirty local test candidate, not a published release.

The final packaged native Metal executable, in an independently identified private app/root, visibly starts a new City-v16 rule-3 city and loads the actual Kaifeng inside-passage save paused at tick 51 with 824 Funds. Native-delivered keyboard F5/F9 retains an identical saved document; existing Z zoom cycles through 2×, 4× and 1× and R restores fit without changing that World. Automated pointer delivery instead supplies an incorrect top-edge point and does not activate the intended menu button: this is **automation-delivery failure**, leaving the short human mouse/road-drag check open. No input-coordinate correction was added. Direct scripted close renders of both paid gate layouts are separate evidence; a roof can obscure the actual courier.

The bounded corpus policy probe prepares **77/167 maps** and explicitly rejects 90: 83 unknown zero-side footprints, two conservative occupancy conflicts and five unsupported `cResWall` managers. In particular Xia remains unsupported for the new rule 3; its old profiles remain valid. These counts are policy preparation, not corpus-wide render or gameplay acceptance. All 239 frozen original files remain byte-identical. Detailed raw-cell diagnoses, private cities, captures and machine reports stay under ignored `.local/gate-passages/` (sanitizer/review reports under `.local/road-gate-passages/`).

## Budget-copy crash hotfix (2026-10-06)

The checked hotfix starting HEAD is `8c2ef2a634172d45de73241b780ff23604cb59ab`. Oliver reported an uncaught `std::invalid_argument: saved object outside permitted map cells` from the test starter. His terminal excerpt has no stack and does not identify the input action. The named original-data and player-root directories exist. The player root was read only: it contains settings and an empty tick-zero start recovery, which does not establish when the crash occurred. All reproductions use separate private roots.

Before changing production code, an authored rule-3 regression placed a normal paid road on a cell with road permission but no building permission and then requested a valid, untaxed building purchase. It reproduced the exception. A second reproduction loaded actual Kaifeng and used the production `SandboxView` map MouseDown/MouseUp path: pause, Houses `(100,115)` and `(97,116)`, road drag `(99,116) → (99,126)`, then Clay Source `(100,117)`. Initial view loading and both Houses succeed. The mixed road drag succeeds with eight paid roads, three unchanged passage cells and 1,124 Funds. Clicking Clay then throws. LLDB stopped at the actual throw with this pre-fix stack:

```text
__cxa_throw
World::restore_impl / put Road(99,116)             World.cpp:3189
World::restore(snapshot, vector_mask)              World.cpp:3150
starter_budget_warning(world, commands)            CityStartGuidance.cpp:178
starter_budget_warning(world, command)             CityStartGuidance.cpp:170
SandboxView::request_execute                       SandboxView.cpp:1534
SandboxView::handle_event / map MouseUp             SandboxView.cpp:1489
```

The budget hypothetical restored only `world.buildable()`. For rule 3, the vector-mask overload creates an authored simple policy; it loses separate road rights, protected original occupancy, gates, openings, edges and signed heights. The legal road on actual road-only `(99,116)` consequently becomes illegal in that clone. The strict restore correctly rejects the incorrect context. This proves a crash in the independently reproduced purchase sequence, not damage to Oliver's saves or the exact action in his session.

The minimal fix uses the existing `World::restore(snapshot, world.map_permissions())` for permission-based hypotheticals. Profiles without that context keep their exact old mask restore. The audit found one other genuine full-world copy in `SandboxCheck`'s visual control, which now follows the same conditional choice. World transactions already copy the full World and `import_snapshot` already retains the prepared policy; historical/synthetic mask fixtures and all save/recovery paths stay unchanged. Immutable map data can be shared while mutable World state remains independent. There is no rule, policy, schema, graphic or restore-validation change.

`request_execute` additionally guards only the read-only preliminary budget computation. A standard exception there clears gestures and pending purchase/modal state, displays `Purchase check failed: …`, returns a rejected result and executes no command. The real execution and confirmation commit remain outside that guard. Budget warnings remain active, including ordinary Cancel/Confirm behavior. A test-only shim throws before the actual guidance call to verify this boundary without corrupting an admitted World; it is linked solely to the new UI test and is absent from production libraries/executables.

Post-fix, both actual Kaifeng and Zhengzhou use the same Houses-first, roads-next, industry-last scripted SDL map-input path. Each pauses at tick zero, pays **476** and retains **824 Funds**, with normal 12/10 workforce and no injected goods or money. Clay delivery/return completes at ticks **66/101** for Kaifeng and **61/91** for Zhengzhou, then normal Pottery production completes. These runs have no removed-road waiting interval. A valid budget query while the actual courier is in the gate preserves the full source snapshot and its immutable policy. F5/F9 after construction and inside the passage restores schema 19 unchanged, including identical 300-tick continuations. A separate process loads the inside save and produces identical serialized state after another 500 ticks. Normal Space Resume is exercised; subsequent simulation uses scripted fixed ticks, not elapsed-time/native or human evidence. The earlier direct `World::execute()` playtests were valid production/routing evidence but did not cover this additional UI/budget copy.

New authored tests cover road-only purchase through the actual UI handler, reserve-warning cancellation and exactly one confirmed paid commit, both gate axes with active couriers, shared full topology/heights, independent hypothetical mutation, strict invalid-snapshot rejection, legacy City-v10/City-v16 rules 1/2 and unchanged RoadDrag counters. Debug, Release and ASan/UBSan each pass all **108/108 tests without skips or warnings** (CTest 492.91/41.55/208.91 seconds respectively). The actual Kaifeng `SandboxCheck` also passes 1,200 ticks with its control and save continuations equal. All 1,464 original-data files and all three files in the named player root remain hash-identical. Debugger stacks, baseline/final hashes and machine reports remain ignored under `.local/gate-hotfix/`; the authored UI/link proof is under `.local/gate-passages/hotfix/`. Human pointer acceptance remains open.

## Start a new test city

From the repository root, using your own original data:

```sh
tools/test_gate_passages.sh .local/gog-extracted/app Kaifeng
# Second layout/countermap:
tools/test_gate_passages.sh .local/gog-extracted/app Zhengzhou
```

The starter builds current Release and creates fresh isolated settings/save/recovery. Choose **New Sandbox**, then **Start Sandbox**. Confirm **City v16 v3 / Map policy 1** in the HUD/window. It starts an empty paid city; old City-v10 and City-v16-v1/v2 saves keep their rules. Build through the center short-axis passage and its two openings. Visible success means real delivery and return between staffed industry on opposite sides, continuing after F5/F9 inside the passage.

For the exact paid Kaifeng setup above, place Houses at `(100,115)` and `(97,116)`; for Zhengzhou use `(120,103)` and `(120,100)`. The origins and road drag in the table use storage coordinates. Use normal map inspection to locate them, pause while building, then continue simulation. This is a small production/traffic check, not a self-sufficient long-term city; it still needs ordinary Food, Service, Water and Health infrastructure for extended play.

Synthetic, scripted original-data rendering, native input and human evidence remain separate. The existing short human native-input check and full Emperor composition parity remain open.
