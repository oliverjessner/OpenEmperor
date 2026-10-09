# Road connectivity diagnostics

Status: **EXPECTED / DIAGNOSTICS IMPROVED**, 2026-10-09. Baseline HEAD
`0891513a3608d481c648c309e1a9c8aa2c03a42c` was clean before this pass.
No reproduced production routing defect warrants a rule or cache fix.
City-v16 rule 3, MapPermissions policy 1, save schema 19, building footprints,
dispatch, economy, presentation and original inputs retain their authority.

## Player check

Select a courier's building, open **F1**, then press **G**. The Inspector shows
the measured source/target IDs, complete footprints, actual entrances,
component IDs, existing route or relevant blocked edges and signed heights.
Press G again to check the next compatible courier/target pair. An idle
NoRoad courier often has no selected dispatch target; these alternatives are
explicitly **Candidate targets**, not new dispatch decisions.
Scroll the existing Inspector with the mouse wheel for all details; FireWatch
keeps its ordinary workers/operation/phase/status rows first.

The ordinary Inspector gives a short F1/G hint for NoRoad. Diagnostics are
requested only on non-repeat G in focused, unobstructed debug mode. They are
cached for the current World, command sequence and road revision, hidden for
another selected building, and discarded on session replacement/load. Road
and building commands invalidate the report. Ticks do not rerun it: **Measured
tick** and **Dispatch at check** identify frozen observations. The ordinary
Inspector continues to show current courier status. Rendering, hover and
cached frames do no new connectivity scan/BFS/commands/files/asset work.

## What the result establishes

`diagnose_road_connectivity(World, source, target)` is an SDL-free read-only
query. It reuses `building_footprint_cells`, `World::building_entrances`,
`find_building_route`, `transport_cell`, `transport_edge_allowed` and immutable
`MapPermissions::transport_edge_blocker`; it never supplies a gameplay route.
Orthogonal storage-coordinate traversal and all existing entrance/gate rules
are unchanged. The exact authoritative route is returned when one exists.

| Result | Measured fact |
| --- | --- |
| CONNECTED | A complete authoritative route exists. |
| SOURCE_NO_ENTRANCE / TARGET_NO_ENTRANCE | The named endpoint has no valid Building↔Road entrance. |
| DISCONNECTED_ROAD_COMPONENTS | Both endpoints have entrances but lack a complete transport connection. |
| BLOCKED_HEIGHT_TRANSITION | A specific measured edge has incompatible signed heights. |
| BLOCKED_ORIGINAL_STRUCTURE | A measured attachment meets protected original structure/solid gate cells. |
| BLOCKED_GATE_ENTRY | A measured attachment violates the existing gate-entry or building-from-corridor rule. |
| MISSING_ROAD_CELL | A measured local attachment lacks a Sandbox Road/fixed passage. |
| UNSUPPORTED_OR_UNKNOWN | Unsupported query or an unclassified measured permission reason; no guessed repair. |

The first three are topology results; several secondary edge categories can
coexist. All valid/rejected adjacent Road entrances are examined over the
whole authoritative footprint, including 2×2 buildings. Ordinary policy-1
Building→Road, Road→Road and Road→Building edges require equal signed heights.
Validated fixed-gate openings/corridors retain their separate existing rules;
side entry and direct building entry from a fixed passage remain forbidden.
No ramp, bridge, sprite-derived height or original-structure exception is added.

The grid is bounded to actual dimensions, at most 228×228. Component IDs follow
row-major first-cell order; neighbors retain World order up/left/right/down.
The report counts examined reasons and stores at most 64 prioritized frontier
edges. Endpoint facts and all component counts remain bounded. Missing cells
are validated with actual `World::validate(PlaceRoad)` and labeled Candidate.
A legal purchase proves only that purchase, not a complete repair. The UI
shows at most three frontier edges for disconnected queries; a connected
report shows its route and any rejected real entrances without irrelevant
frontier suggestions. No unique repair or minimum repair search is claimed.
World does not store raw original terrain; the technical map report joins
edge coordinates to the already loaded raw terrain/object arrays, without I/O.

Routing and dispatch are separate: workers, output, phase, cargo/reservation,
route-pending, actual selected target, cached revision and road revision are
reported independently through existing queries. CONNECTED can coexist with
No stock, Unstaffed, Paused, Already moving or other ordinary dispatch states.

## Reproduced original-map cases

The documented paid B mini on Handan/Badaling remains connected and delivers
Clay at actual ticks **32 dispatch → 41 arrival → 51 home**. New E uses two
paid Houses, Clay at offset (0,0), Pottery at (9,3), Roads (1..9,2) except (5,2).
At tick 64 it has population/workers 12, assigned workers 10, construction
476, Funds 824 and actual Clay **NoRoad**. Each building has one entrance;
its two components contain four Roads each.

| Map / recipe origin | Source entrance building→Road | Target entrance Road→building | Measured gap / first frontier edge |
| --- | --- | --- | --- |
| Handan (142,74) | (143,75)→(143,76) | (151,76)→(151,77) | (147,76); (146,76)→(147,76) |
| Badaling (112,53) | (113,54)→(113,55) | (121,55)→(121,56) | (117,55); (116,55)→(117,55) |

These edges have raw terrain `0x80`, raw objects 0, signed height 0 and actual
road permission. Normal validated Road purchase costs **2**: Funds become
822/construction 478 and the authoritative route has 11 vertices. The helper
then observes **65 → 114 → 164**, actual Clay cargo 2 delivered to Pottery,
cleared reservation and home return. An independent tick-160 replay observes
**161 → 210 → 260**. This is a proven repair for this controlled missing cell,
not an assertion about every disconnected layout.

Xia/Banpo fresh paid minis retain real 32→41→51 trips; existing paid
Kaifeng/Zhengzhou gate cases remain connected. Similar paid no-entrance and
connected/unstaffed cases distinguish their actual causes. A loaded-height
scan of these six exact controls finds no permitted ordinary adjacent Road
pair/building attachment with unequal heights: real Handan/Badaling height
failure is not claimed. Independent authored height fixtures test source,
Road and target edge conflicts, alternative valid entrances and gate rules.

No matching current failure save was found. An isolated byte-identical copy
of a potentially player-earned Xia tick-791 save has Clay CONNECTED/NoStock
and FireWatch AlreadyMoving; it does not reproduce Oliver's screenshot.
Source saves are unchanged. Exact user-state diagnosis and human acceptance
remain open; synthetic, real-map technical and scripted App evidence stay
distinct.

## Test start and verification

Run the existing versioned launcher from the repository:

```sh
sh tools/test_map_playability.sh .local/gog-extracted/app Handan
```

It builds current Release and uses a fresh isolated app root. **Load Sandbox →
E-road-disconnected**, select Clay, **F1 → G**, buy the printed storage gap as
an ordinary Road, press G for CONNECTED, then Space to observe delivery/return.
F5/F9 tests normal save/load. **B-paid-courier** is the positive comparison.
A–D remain byte-identical. An unsupported additional E placement/attachment
is named separately and preserves existing A–D starts; it never substitutes
fabricated permissions. `road-connectivity-report.json` contains raw edge
facts and actual repair/trip proof outside the menu's save directory.

Core regressions cover A–O from the milestone, complete save/readback equality,
protected begun edges, deterministic truncation and maximum grid/work bounds.
The Inspector regression verifies explicit/cycled actions, repeat/focus/modal
gating, pure cached frames and road/building/load invalidation. Independent
real-map checks compare 37 queries, 111 repeated/restored report calls and 74
complete serialized before/after save documents: all equal. Diagnosis performs
zero World copies/restores/commands/ticks/route refreshes/files/asset operations;
existing authoritative route BFS is counted separately (0–4 calls in these
real cases). Ordinary road preview remains zero-BFS.

The fresh corpus retains all **102 prepared policies / 306 real-zero-one
variants / 3366 full components** byte-for-byte. All **1464 original files /
819,711,091 bytes** retain their complete SHA manifest. No new occupancy,
zero-side, height or gate permission is admitted. Final **Debug / Release /
ASan+UBSan (RelWithDebInfo)** each pass **133/133 tests**, with zero skips,
compiler warnings, sanitizer diagnostics or frozen-source changes. Each also
passes AlphaEndurance, all three map fixtures and the paid Xia goal:
14 complete save files and main-facts output are byte-identical to the existing
reference. A–D saves/reports are unchanged; E reproduces the same repair in
all three configurations. The earlier unoptimized sanitizer run was explicitly
aborted for its unsuitable endurance runtime and retained separately; it is
not production-failure evidence. Reports are ignored under
`.local/road-connectivity/`.

Instrumented scripted ordinary-App dummy/software runs separately load Handan/Badaling B
and E through the real menu, select Clay and request F1/G. B retains 32→41→51;
E's ordinary UI purchase proves 65→114→164 / cargo 2 delivery. Repeated cached
frames have all 12 measured production counters zero. Full F5/F9/F5 documents
remain equal across diagnosis, and F9 invalidates the cache. Another isolated
E copy buys FireWatch normally for 80 Funds: workers 2/2, actual NoRoad, and F1/G
correctly reports SOURCE_NO_ENTRANCE (0) while Clay has one target entrance.
Compatible targets cycle without dispatching. These runs establish scripted
App behavior, not native pointer/held-key or human acceptance.
