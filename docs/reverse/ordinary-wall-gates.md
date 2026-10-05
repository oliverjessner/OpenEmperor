# Ordinary city-wall gate connections — bounded pass, 2026-10-05

This pass extends read-only original-map presentation. It does not add playable
walls, gates, military navigation or original Emperor gameplay. Complete original
scene fidelity remains open. Historical Pass-3/4 results remain historical.

## Baseline and real inputs

The workspace began clean at `c262c9a7a6e13d8e5a2cf0d857d277b2b26ea651`.
A fresh census of 167 supported standalone maps found 1,320 raw `0x4000` cells:
1,123 gate-free static selections and 197 ordinary wall cells with an eight-neighbor
`0x8000` context. No wall center combined `0x4000/0x8000`, and no wall center had
the excluded `0x148` bits. The 835 raw gate-bit cells are **not** 835 gates.
The original manager parser explicitly rejected 24 maps with unsupported classes
before the bounded GateHouse/Tower reader extension; unsupported managers still
fail completely rather than publishing partial entities.

| Case | Original map and wall cell | Height | Wall/gate/union masks | Proven gate object |
| --- | --- | --- | --- | --- |
| A, first vertical case | `Cities/Kaifeng.map`, `(142,81)` | 0 | `01/38/39` | Original ID 6, type 130, layout 1, origin `(141,82)`, 3×5 |
| B, other axis | `Cities/Kaifeng.map`, `(96,121)` | 0 | `40/0e/4e` | Original ID 1, type 130, layout 0, origin `(97,120)`, 5×3 |
| C, independent map | `Cities/Zhengzhou.map`, `(125,99)` | 1 | `01/38/39` | Original ID 5, type 130, layout 1, origin `(124,100)`, 3×5 |

Kaifeng's complete 4,000-record manager contains five GateHouses, five Towers and
18 Industrial records, ending at logical 1,821,368. Zhengzhou has two GateHouses,
eight Towers and two Industrial records, ending at logical 1,827,264. The other
records are base Building records. The seven proven gates claim 105 cells;
Kaifeng's remaining 20 and Zhengzhou's remaining 32 gate-bit cells are not admitted
as GateHouses. In particular, Zhengzhou's separate 2×2 cluster near `(104,78)` is
not called a gate. Gate-free controls are the actual `Cities/Luoyang Tang.map`
(75 walls), `Cities/NavalT-Jiangling.map` (53) and `Cities/NavalT-Yen.map` (58).

All raw facts, disassembly windows, private captures and pixel audits remain under
ignored `.local/ordinary-wall-gates/`. No original bytes or pixels are fixtures.

## Bounded original path

The EXE is read only, with freshly checked SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The wall trace checks 1,084 disassembly byte lines in 18 bounded windows; the
additional necessary GateHouse trace checks 2,289 lines in 27 windows. There were
zero byte mismatches. The original executable was not run.

`0x4b67b0` traverses wall cells in row-major order, first calling `0x4b8f70(0x4000)`
and then `0x4b6290` for the same eight neighbors. The latter adds each neighbor's
`0x8000` presence. `0x4bbdc0` tests nonzero, so the bounded equivalent is the union
of wall and gate presence, not a new gate-orientation table. Both bits at one
neighbor still mean one present neighbor. Center eligibility remains wall without
`0x148`. The center gate bit does not establish a gate body.

The existing 16 rows at `0x84a8f8`, four static view columns, key `0x451` and variants
0–17 remain unchanged. `0x4bee90` alternates straight 0/2 to 1/3 using freshly
generated cardinal neighbors. `0x4b6370` supplies a cardinal gate/view context to
`0x4b6440`, but that remapping affects only variants 18–40; it is identity for all
admitted static variants. No original entity ID, saved graphic or gate-object
orientation selects these ordinary wall bases. Optional model components from
`0x4be260/0x4be3e0` remain outside this pass: key `0x451` variants 18 and above,
including General 939–953 Type 256 images, are not interchangeable Type-30 bases.

### The necessary GateHouse body

The historical slot-2 IDs on both chosen maps had no registered image and showed
diagnostic ground. Registering General and trusting those IDs would select
unrelated images. A longer wall beside that ground was therefore not a completed
visible gate connection.

The bounded producer is `cGateHouse`, descriptor `0x850fa0`, constructor
`0x4f8dd0 → 0x4f8e30 → 0x51c9a0`. Standalone restore `0x52f030` special-cases type
130: `0x52f0ef/0x52f0f9` copies the saved entity's signed layout at runtime `+0x80`,
then passes two zero arguments into `0x4b11f0 → virtual +0x100 → 0x4f9780 → 0x4f9570`.
This is the saved placement path; the constructor's different boolean/global
context is not substituted for it. No mission/material context or saved graphic ID
is an operand here.

The serialized signed 32-bit layout is at base-record offset 112 for schemas 3/4,
113 for schema 5. Existing base parsers remain exact. The GateHouse parent wrapper
is schema 1, followed by the saved NonHouse state schema 1 (128 bytes), then its
own wrapper 1 (no payload) or 2 (eight opaque attached-unit bytes). Those unit bytes
are consumed but never used as entity IDs or selector authority. Active layouts
other than 0/1 are rejected. `cTower` is consumed only to validate the complete
chosen managers: the same base/parent/NonHouse path followed by its own wrapper 0,
without a payload. No Tower producer is implemented. `cResWall` stays unsupported.

At camera zero, the 30 rows at `0x850fc0` contain two layouts of 15 components each:

- Layout 0: 5×3, x 0–4 within y 0–2, variants 0–14.
- Layout 1: 3×5, x 2 then 1 then 0, y 0–4 within each column, variants 15–29.

`0x4f9170` chooses key `0x4af` plus the row's one-based index minus one and
`15 × layout`. `0x4f9570` places every row as one cell: the bit-2 rectangle path
`0x4b72b0` has 1×1 dimensions at `0x850fb8`; the bit-1 path is also a singleton.
Both write terrain/ownership marker `0x8008`. Raw flag `0x10` supplies no new
placement rule. Thus a gate object has one complete 15-cell claim and 15 singleton
render components, not one giant guessed Type-30 footprint. There is no Type-256
addition in this bounded GateHouse placement path.

## Implementation and activation

`WallTopology` computes separate wall/gate neighbor masks and their union. It
reports no gate context, a selected adjacent static connection, a center requiring
separate body composition, or an unsupported topology. Excluded center terrain
retains its earlier eligibility failure. Existing tor-free rows and alternation
are unchanged. Diagnostics are prepared during the existing load-time scan.

The final corpus replay selects all 197 previously refused adjacent cells. For the
1,123 gate-free cells, replaying the exact prior generated-cardinal inputs gives
identical results in every case. A complete fresh scan now propagates the existing
straight alternation through the newly generated connections: 492 variants stay
identical and 631 change only 0↔1 or 2↔3. Their semantic rows remain unchanged;
no corner, T or cross changes occur. This is scan-state propagation, not a new
gate-free rule. Wholly gate-free control maps remain completely unchanged.

`OrdinaryGateMapPresentation` joins only active, typed GateHouse records to the
centered map origin and exact serialized cell reference. It validates all 15
canonical cells, raw `0x8008`, no wall conflict and no overlap before publishing
any claim. It does not derive identity from the gate bit or historical graphics.
The full original manager is read through the existing bounded parser; failure
publishes no partial gate entities. Great Wall selection/context is unchanged.

Resolution remains `ResourceGroupLookup → RuntimeArchiveLayout → physical AssetId`.
On this revision, General key `0x4af` is runtime `[687,717)`, translating once to
physical 888–917 after the archive's existing skipped/dummy records. Variant 30
is out of bounds. All 30 images are static, unmirrored Type 30, width 78, size flag
1 and base length 3,200. Their heights range from 40 to 289. Combined/Base/Overlay
for the entire group total 3,420,144 logical RGBA bytes, within the unchanged
64 MiB budget. Production does not contain these physical record numbers.

All required metadata, complete claims and additional physical-asset bounds are
preflighted before component publication. Each gate's 15 members share a separate
composition group keyed within the bounded manager domain. Existing eager
decode/upload and atomic readiness activate all 15 or none. Failed metadata,
decode or intersecting historical-footprint readiness retains the complete
historical/diagnostic fallback; no half gate or old/new double body is admitted.
F1 exposes source/layout byte provenance, resource selection and activation for
valid claimed cells. The report also lists every source gate's refusal, including
claim failures that intentionally publish no selectable cell.

Gate components reuse singleton Type-30 origins, signed saved cell height ×40,
cached alpha, culling and the existing `EarlyBaseSpatialOverlay` painter. They are
not made `SpatialCombined`, drawn last, shifted to hide seams or used as playable
occupancy. F8 Wall/Monument visibility includes the whole admitted gate. Snapshot
keeps its historical path. Physical texture deduplication and runtime-bounded
TextureCompatibility remain unchanged. New family values are appended; old values
are not shifted. Per-frame render/inspection/picking performs no file, decode,
upload, World copy/command, full map scan, BFS or route refresh.

## Validation and evidence boundaries

Pure authored fixtures contain no original
pixels. They exercise independent neighbor/table expectations, both layouts,
schema/truncation rejection, group bounds, eager physical deduplication, atomic
claims/readiness, historical coexistence, alpha picking, F8/Snapshot, dynamic
front/back overlap and signed height. Changing only historical graphic IDs must
not change the regenerated wall/gate decisions.

### Final checks

| Final source check | Result |
| --- | --- |
| Debug | 103/103 PASS, no failures/skips/source changes; 572.73 s including build |
| Release | 103/103 PASS, no failures/skips/source changes; 119.80 s including build |
| Project ASan/UBSan | 103/103 PASS, no failures/skips/source changes; 333.51 s including build |
| macOS app package | PASS; additional full 103-test Release suite, ad-hoc signature, relocated/unzipped checks and 13 negative cases |
| Existing endurance | PASS; 100,000 ticks, deterministic save bytes, 3,000 cached renderframes, 100 sessions with zero remaining owned textures |

System SDL is uninstrumented and leak detection remains disabled under the
existing sanitizer policy; this is not a claim of sanitizing the system library.
The packaged candidate is local, dirty-workspace `0.1.0-alpha.2`, revision label
`c262c9a7a6e1`, macOS arm64. It is neither a release nor publication. Final code
hashes match all three tested source manifests. Frozen GreatWall selector/context,
composition policy, texture policy, input, World/simulation, persistence, recovery,
road and compatibility metadata files remain byteidentical. All 239 original input
files (63,974,662 bytes) match their pre-pass hashes.

### Actual visible production cases

A was completed first through `StoredMapSession → StoredGraphicsRenderer`, with
15 active key-`0x4af` parts and both adjoining walls. Its 1× capture was visually
reviewed before B/C. B (layout 0, height 0) and C (layout 1, all 15 cell heights 1)
then pass at 1×/2×/4× with complete claims, resolved physical assets, signed height
and cached alpha hits. Both connections are visibly closed. Only visible alpha
components are counted as picks; all 15 active parts need not be separately
visible. Readback/capture is observer work outside measured pure frame windows.

At 1×/2× each measured B/C gate draws 15 Base and seven nonempty Overlay textures,
zero Combined; each component is submitted at most once. Culling reduces visible
parts at 4×. Each of the 45 textures for 15 physical parts uploads exactly once.
Repeated gate layouts share assets: Kaifeng has five active gates/75 cells,
449 decodes, 1,347 uploads and 22,412,376 logical RGBA bytes; Zhengzhou has two
active gates/30 cells, 374 decodes, 1,122 uploads and 19,258,320 bytes. Selected
adjacent wall activation is 19/19 in Kaifeng and 16/16 in Zhengzhou. These measured
activation counts are separate from the corpus's 197 selector successes.

The baseline/final production-renderer comparison preserves all 21 Snapshot
captures and all nine regenerated gate-free control captures exactly; only the
12 intended gate/connection captures change. Complete common rendering-state
hashes on all three gate-free controls agree. All five maps preserve every raw
grid and historical plan. The independent 2×2 Tower/gate-bit auxiliary case in
Zhengzhou remains a named gap; the old private `C-height` filename refers to that
auxiliary, not the proven ID-5 gate near `(125,99)`.

The normal `MenuSession → New Sandbox → SandboxView` scripted comparison covers
13 cases: Badaling/Handan in all five Great Wall modes, plus Xia, Kaifeng and
Zhengzhou. All 26 tick-0/tick-1,600 saves and 13 buildability masks are byteidentical
under the same paid commands/ticks. All 55 Great Wall/Xia captures remain
pixelidentical, including the unchanged four-piece Ruined phase-2 notice and group
bounds. Kaifeng/Zhengzhou pixels change only where expected; their World/save
authority does not. The 390 pure Sandbox frames have zero checked asset/World/BFS
counters; the dedicated B/C proofs each run 100 pure render/pick/F1 frames with all
12 production counters zero.

### Native app evidence and the remaining human check

The final waiting-parent process was verified with its explicit isolated root and
full arguments before UI binding. Its separately named, ad-hoc re-signed copy of
the validated package has identical Mach-O section contents; only signature data
changes. Actual Cocoa/Metal Menu/New Sandbox loaded empty Kaifeng, showed all five
gate bodies and adjoining walls in the native fit overview, paused at tick 389,
toggled F1, cycled camera zoom 2×/4×/1×, reset the camera and saved via F5. The native
save has zero commands and the process exits normally. The opt-in observer printed
once after exit: 80 records of this window, fixed capacity 512, zero overwrites.

The native wheel automation delivered raw position `[0,0]`, outside the map, so
the ordinary positional guard correctly refused it. The keyboard tool's Y/Z
mapping also differs from the native layout. No input/camera correction was added.
A native 1× gate close-up, sustained-arrow panning and native pointer picking are
therefore **not** claimed PASS. The primary 1× and supplemental 2×/4× gate evidence
above are scripted production captures. The short human close-up/pointer check
remains open; it does not reopen Oliver's earlier City-v10 report as a blocker.

Private launch attempts whose subprocess ended before UI binding are excluded
from native evidence. A post-quit UI observation relaunched the named probe copy;
that copy was immediately closed. The completed native run and all saves/recovery
work use the verified isolated root; no active user game session was controlled.

To review locally, double-click the ignored
`.local/ordinary-wall-gates/run-gate-test.command`, choose New Sandbox and Start
Sandbox. It uses Kaifeng, an empty City-v10 test and its own persistent settings/
save root; pass `Zhengzhou` as the first argument for the independent case. Use the
ordinary wheel/arrows and Z for 2×/4×/1×. Inspect the roof/body, both wall ends and
transparent picking; do not infer military passage or a playable gate from pixels.

Native app observation, scripted production-frame captures and synthetic tests
are separate evidence. This pass does not turn automation into a human PASS.
Oliver's earlier City-v10 zoom/arrows/resize report stays User-reported PASS only
for that tested session, and is not a blocker or broader gate acceptance.

Gate-only maps without an ordinary wall candidate currently retain the existing General-registration fallback; this pass validates connections on maps with walls.

Open: other GateHouse states/views/lifecycle, Tower/ResWall body producers, optional
ordinary-wall Type-256/model components, original height normalization, general
interpenetrating images, independently captured original-game parity and complete
first-draw composition. The 197 newly selectable wall cells do not establish 197
completed gates or active renderings across every map. Stop at this bounded pass;
no commit, push, tag, release or publication.
