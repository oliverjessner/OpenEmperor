# Great Wall restore context

This bounded static recheck starts from commit
`91891af8bba00e85223a80773a110edf9f1f7ad0`. It explains the material input
missing from the standalone map presentation path. It does not establish an
original mission for a selected map or complete original first-draw parity.
The historical [Pass 4 result](map-first-draw.md#pass-4-final-serialization-and-reconstruction-results)
remains a partial result with zero reproduced Great Wall instances.

**Decision:** the original material decision is understood, but its original
mode/current-player goal inputs are unavailable in OpenEmperor's standalone
map entry. Automatic presentation must therefore retain the fallback. A
separately selected material is an **explicit OpenEmperor preview** and must
retain that source through selection, claims, rendering and inspection.

## Pinned source and address spaces

**EXE-OBSERVED:** all addresses below refer to the unchanged local
`Emperor.exe`, SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The executable was read statically and was not run. No original code or data
is added to production or distribution.

The PE ImageBase is `0x400000`. `.text` starts at RVA `0x1000`, physical file
offset `0x400`, with raw size `0x3a7c26`. A code VA in this section translates
to RVA `VA - 0x400000` and physical offset `VA - 0x400c00`. `.rdata` starts at
RVA `0x3a9000`, physical `0x3a8200`, raw size `0x6a2b8`. `.data` starts at RVA
`0x414000`, physical `0x412600`, raw size `0x7a980`, virtual size `0x1646000`.

The mode global `0x88ec38`, current-player byte `0x10de118`, and goal manager
`0x12a4ba8` lie in the uninitialized virtual tail of `.data`. They have RVAs
`0x48ec38`, `0xcde118`, and `0xea4ba8`, respectively, **without physical EXE
bytes**. A physical offset cannot be obtained by extending `.data`'s raw
mapping into that tail. Runtime object offsets and decompressed map offsets
are separate address spaces.

## Exact material decision and priority

The `cMonInfo` read tail pushes `-1` at VA `0x562e2b`, calls `0x563720` at
`0x562e2d`, and overwrites runtime material `+0x5c` at `0x562e40`. A returned
zero is replaced by 1 at `0x562e44`. This occurs after the saved material was
read. For supported monument state schemas 9/10, the saved signed 32-bit
material is at state-record offset `+87`; that is a raw map fact, not the
restored material decision. See [record provenance](map-format.md).

| Observation | VA | RVA | Physical EXE offset |
| --- | --- | --- | --- |
| Restore pushes `-1` | `0x562e2b` | `0x162e2b` | `0x16222b` |
| Calls material helper | `0x562e2d` | `0x162e2d` | `0x16222d` |
| Writes derived material | `0x562e40` | `0x162e40` | `0x162240` |
| Converts zero to 1 | `0x562e44` | `0x162e44` | `0x162244` |
| Material helper entry | `0x563720` | `0x163720` | `0x162b20` |
| Reads mode | `0x5637b5` | `0x1637b5` | `0x162bb5` |
| Reads signed current-player byte | `0x5637d0` | `0x1637d0` | `0x162bd0` |
| First-goal success stops iteration | `0x563827` | `0x163827` | `0x162c27` |
| `-1` query/no-match returns zero | `0x56383a` | `0x16383a` | `0x162c3a` |

For the actual restore call with argument `-1`, priority is:

1. Known original mode `0x88ec38 == 1` selects 3 immediately. The helper
   skips current-player and goal reads on this branch.
2. Otherwise scan the current player's goals in their original list order,
   starting at index zero. The **first** goal with signed 32-bit type 2 and
   value 85 selects 2; the first type-2/value-86 goal selects 3. Both branches
   set the success byte and stop the loop. A later value 86 cannot override
   an earlier value 85.
3. With a complete valid non-1 mode/current-player list and no match, the
   helper returns zero for argument `-1`. The restore tail converts it to
   material 1. This is a valid no-match result, not missing context.

The helper's other argument modes are outside this resolver. In particular,
its ordinary default return and the special `-1` result must not be conflated.
There is no saved-material, map-name, terrain, historical graphics-ID, or
archive-presence input to this decision.

The pure resolver therefore needs a validated original mode and, only for a
non-1 mode, a complete validated ordered list for the actual current player.
An absent list differs from a present validated empty list. Missing mode
remains unavailable even if goals are supplied. An explicit preview of 1/2/3
does not turn these missing original inputs into verified inputs.

## Current player and goals

`0x55f5d0` (RVA `0x15f5d0`, physical `0x15e9d0`) returns the count at runtime
`0x12a4c38 + 24*player`. `0x55f5e0` (RVA `0x15f5e0`, physical `0x15e9e0`)
indexes the corresponding list at `0x12a4c28 + 24*player` and dereferences its
object pointer. The material helper reads signed 32-bit goal type at object
`+4` and goal value at object `+0x0c`.

The global goal-manager object is `0x12a4ba8`. Its serializer `0x55a7a0`
(RVA `0x15a7a0`, physical `0x159ba0`) serializes ten list objects with
24-byte runtime stride, beginning at manager `+0x7c`. The read loop is at
`0x55a8b2..0x55a8c5`. The list's pointer/count globals above belong to this
manager, not to the standalone map building manager or an OpenEmperor World.

The separate wrapper `0x55a430` (RVA `0x15a430`, physical `0x159830`) calls
that serializer with `ECX = 0x12a4ba8`. Its local diagnostic text identifies
it as `cGoalsLoader - serialize`, at VA `0x859918` (RVA `0x459918`, physical
`0x457f18`). This establishes a separate serialized source; this pass does
not implement its full stream format or a campaign parser.

The current-player byte is supplied by higher-level entry state. For example,
the dispatcher resets it at `0x42e456`; the mission loader conditionally sets
it to zero at `0x43acd9` before applying `0x43ab50` to the selected player at
`0x43ad01`. Saved-session load `0x534a30` later assigns it from another loaded
header field at `0x534b44`. These observations do not authorize inventing
player zero when importing a standalone map.

## Direct mode writers and load entries

A static scan of explicit 32-bit writes to `0x88ec38` finds these 14 sites.
This is a direct-reference census, not proof that no indirect writer exists.

| Writer VA | RVA | Physical EXE offset | Observed value |
| --- | --- | --- | --- |
| `0x401e24` | `0x1e24` | `0x1224` | 0, return/reset branch |
| `0x42e473` | `0x2e473` | `0x2d873` | 0, dispatcher state 0 |
| `0x42e508` | `0x2e508` | `0x2d908` | 1, dispatcher state 2 |
| `0x42f713` | `0x2f713` | `0x2eb13` | 0, entry-state transition |
| `0x430efd` | `0x30efd` | `0x302fd` | 0, also resets current player |
| `0x431372` | `0x31372` | `0x30772` | 0, conditional transition |
| `0x475765` | `0x75765` | `0x74b65` | 0, startup initialization |
| `0x4ec2c6` | `0xec2c6` | `0xeb6c6` | 0, dispatched transition |
| `0x4fb60c` | `0xfb60c` | `0xfaa0c` | 0, higher-level load entry |
| `0x51686c` | `0x11686c` | `0x115c6c` | 0, load transition |
| `0x5168d9` | `0x1168d9` | `0x115cd9` | 0, conditional load transition |
| `0x536daf` | `0x136daf` | `0x1361af` | 1, after `/map=` loader returns |
| `0x536df3` | `0x136df3` | `0x1361f3` | 0, after `/custom=` loader returns |
| `0x586b86` | `0x186b86` | `0x185f86` | 0, also resets current player |

The state-2 branch in `0x42e430` writes mode 1 and `0x10de12c = 1`, then
prepares the map-selection entry at `0x4314a0`. State zero writes mode zero.
The embedded jump table at `0x42e59c` establishes these state indexes. This
shows an explicit program/editor entry mode; the map bytes do not select it.

The editor identity is independently supported by RTTI and the UI dispatch:
the `cMapEditorMenu` type descriptor is at `0x8571d0`, its complete-object
locator at `0x7d3a88`, and its vtable at `0x7b6c0c`. Constructor
`0x53e260..0x53e28f` installs that vtable in the singleton returned by
`0x53d770`. At `0x53820f`, EDI is set to 1; `0x538516` compares mode with EDI
and the equal branch calls that editor singleton at `0x53851a` (RVA
`0x13851a`, physical `0x13791a`). Thus mode 1 selects the original editor UI.
This identifies the program mode; it does not make any standalone filename
an editor-mode restore input.

The original command-line parser distinguishes `/map=` (string VA `0x82ab30`,
RVA `0x42ab30`, physical `0x429130`) from `/custom=` (`0x82ab38`,
`0x42ab38`, `0x429138`). Both check a `.map` extension but set different
startup flags: `0xc05809` at `0x47511a` and `0xc0580a` at `0x4751eb`.
The startup caller first resets mode/player via `0x536cd6 → 0x42e430`.

The `/map=` branch calls `0x53d470` at `0x536daa`, then writes mode 1 at
`0x536daf`. `0x53d470` constructs the associated file name, calls the routine
whose diagnostic is `loadmission` at `0x53d50a → 0x5351e0`, and runs post-load
setup at `0x53d51c → 0x535540`. The order matters: the mode-1 write occurs
**after** this call. Observing mode 1 in the resulting editor state does not
by itself establish mode 1 at an earlier monument deserialization.

The `/custom=` branch instead calls `0x5d10f0` at `0x536deb`, then writes mode
zero at `0x536df3`. One branch of `0x5d10f0` enters `0x43abf0` at `0x5d120a`.
These are different surrounding program states for a `.map` selection.

The bounded mission path through `0x43abf0` is especially direct: it reads
the goal wrapper at **`0x43acc7 → 0x55a430`**, selects the current player, and
then opens another archive and reads the map at
**`0x43ad9e → 0x52e7c0`**. The respective RVAs/physical call offsets are
`0x3acc7 / 0x3a0c7` and `0x3ad9e / 0x3a19e`. Goals are loaded before the map
manager read on this path. The saved-session serializer `0x52fda0` also calls
the separate goal wrapper at `0x52fe1d` (RVA `0x12fe1d`, physical `0x12f21d`).
Neither path follows merely from choosing a filename in OpenEmperor.

## What the standalone entry can establish

The standalone map serializer `0x52e7c0` (RVA `0x12e7c0`, physical
`0x12dbc0`) reads the map header, stored grids and temporary building manager.
It reaches that manager at `0x52eb28 → 0x42d790`, with runtime manager
`0x115d4f0`. The supported manager begins at decompressed logical offset
**1,093,607** and contains the raw original building/monument state described
in [map-format.md](map-format.md). Its 4,000 records are not goal-manager
records.

The standalone header serializer `0x52e690` (RVA `0x12e690`, physical
`0x12da90`) reads a 16-bit schema followed by fields of widths 4, 4, 4, 32 and
4 bytes for schema 2, totaling 50 bytes. It does not read the mode global,
current-player byte, or the goal lists used by `0x563720`. In particular,
its mode test at `0x52e6bb` is in the **write** branch; it does not restore
mode from a serialized field in the read branch.

**OPENEMPEROR DECISION:** the current supported standalone map readers supply
no validated original load mode or selected-player goals. A Sandbox World,
its authored goals, save file and map-browser selection have no demonstrated
equivalence to any of these original higher-level entries. The data tree
contains campaign packages, but this bounded pass has not established a
part-to-mission-to-player binding for the standalone files. It does not
assert that the required information is absent from every original file;
it is absent from the currently supported input contract.

Automatic context is therefore unavailable for Badaling, MPWall1, MPWall2
and Handan through this standalone entry. A future verified loader can supply
validated mode and, where required, ordered current-player goals to the pure
resolver. Until then, a user-selected 1/2/3 input remains explicit preview.
It preserves raw positions, phase, orientation and height and invokes the
existing bounded selector; it does not prove original material authority,
complete composition, or general height normalization.

## Remeasured starting baseline

These software-render checks were rebuilt from clean HEAD `91891af` into
ignored `.local/great-wall-context/baseline-build/`. The six JSON reports in
`baseline-reports/` use **`edge-byte-4x4`** and profile
`exe-6373328b-v213-slot8-runtime-table`. Each successfully parsed manager
schema 1 with 4,000 records. All six report original material context
unavailable and original first-draw acceptance not established.

| Map | Source wall objects | Model-piece descriptors | Descriptor cells | Selected wall pieces / active instances / cells | Decoded scene assets | RGBA texture bytes | Saved-snapshot result |
| --- | ---: | ---: | ---: | --- | ---: | ---: | --- |
| Badaling | 53 | 53 | 740 | 0 / 0 / 0 | 352 | 35,150,496 | Partial: 75 diagnostic cells |
| MPWall1 | 53 | 53 | 740 | 0 / 0 / 0 | 494 | 42,222,456 | Partial: 52 diagnostic cells |
| MPWall2 | 51 | 51 | 708 | 0 / 0 / 0 | 473 | 33,461,184 | Partial: 26 diagnostic cells |
| Handan | 53 | 53 | 740 | 0 / 0 / 0 | 410 | 38,419,704 | Partial: 56 diagnostic cells |
| Xia | 0 | 0 | 0 | 0 / 0 / 0 | 364 | 18,840,384 | Complete saved snapshot |
| Chengdu | 0 | 0 | 0 | 0 / 0 / 0 | 409 | 18,658,080 | Complete saved snapshot |

The baseline cannot select new Great Wall archive registrations because it
has no material context. Its existing historical slot-8 registration remains
comparison/fallback presentation. Decoded asset and texture totals in this
table cover the entire scene, including that historical path; they are not
counts of active reconstructed wall assets. A source object, model descriptor
and active render instance remain distinct. The complete snapshot results on
Xia/Chengdu do not establish original first-draw fidelity.

The unchanged map data revisions are SHA-256 prefixes Badaling `c266c0e2`,
MPWall1 `8d9932ab`, MPWall2 `11cf5975`, Handan `6bf6bcf5`, Xia `871266c4`, and
Chengdu `cd80cd69`; the full hashes are in the ignored source manifest. These
are automated software captures of the existing normal load/render path,
not native input acceptance or original-game comparison. Explicit-preview
activation results are recorded separately after the final implementation.

## Reproducibility and limits

Ignored `.local/great-wall-context/extract-context-evidence.py` records the
pinned hash, PE sections, exact address translations and direct mode-writer
census in `static-context-evidence.json`. Ignored manifests snapshot the 51
original source files and 29 frozen project files before this milestone's
changes. Original files remain unchanged.

This document records static EXE evidence and the bounded input decision.
Real-map instance counts, normal app operation, visual acceptance and
performance belong to the separate activation report; a pure resolver test
or explicit material preview is not an original-game comparison.

## Activation report: explicit material preview

This follow-up starts at **91891af8bba00e85223a80773a110edf9f1f7ad0** with a
clean worktree. The final worktree is intentionally uncommitted. The bounded
decision is **path B**: the existing supported standalone entry cannot supply
the original higher-level inputs, so a session choice activates the existing
model/selector/archive/renderer pipeline. The pure original-input resolver is
also available for a future independently validated caller; the normal
standalone loader does not invent those inputs.

| Acceptance dimension | Final status |
| --- | --- |
| Original Restore Context | **Partially resolved**: decision and caller ordering rechecked; standalone mode/current-player goals unavailable |
| Normal App Rendering | **Active** when an explicit material preview is selected |
| Presentation Source | **Explicit preview**; Automatic remains historical fallback without verified original context |
| Visual Acceptance | **Partial**: connected supported runs are visible; frozen elevation/whole-scene composition still has occlusions |
| Original first-draw parity | **Unresolved**; no original-game image comparison was performed |

### Normal app activation and session boundary

In **New Sandbox** (`N`) or **Load Save** (`L`), use the small **G Great Wall**
button or `G` to cycle `auto`, `historical`, `preview-ruined`,
`preview-earthen`, `preview-stone`. Choose the map/save and press Enter to
load. The caption explicitly says that preview is not original restore.
The choice applies before preparation/loading; this pass adds no live
presentation switch. A new process starts at Automatic unless the CLI
explicitly supplies a mode. The choice is absent from settings, World and
save documents. Ordinary in-session F9 reload retains the current prepared
presentation. A subsequent menu load uses the current session choice; after
restarting the app, choose the preview again.

The normal executable also accepts
`--great-wall-presentation preview-stone` (or another canonical mode) with
the supported map/menu/sandbox/load paths. For example, from the repository:

```sh
.local/great-wall-context/final-dist/OpenEmperor.app/Contents/MacOS/OpenEmperor \
  --data .local/gog-extracted/app \
  --map-debug Cities/Badaling.map --view stored-graphics \
  --graphics-profile exe-6373328b-v213-slot8-runtime-table \
  --multi-tile-preview --footprint-policy edge-byte-4x4 \
  --great-wall-presentation preview-stone
```

`GreatWallContextSource` remains typed as unavailable, verified original, or
explicit preview through presentation, render instances, F1 and JSON. A raw
material without valid provenance cannot pass the original-context gate.
Preview counters are separate; original `render_instances`,
`rendered_original_objects`, `rendered_cells`, regenerated
`great_wall_instances` and `great_wall_selector_verified` remain **zero** in
the preview runs. `selected_material` is the user's input;
`restored_material` remains null. The source record's saved material stays a
separate raw fact.

Candidate sessions prepare and validate required archives and eagerly decode
the selected static components before publication. Missing required input,
archive/decode failure, or failed complete instance readiness rejects the
candidate and preserves the previous session. Unsupported individual layout
or group variants retain named historical fallback, rather than partial
cell claims. Option selection does not restart World, rebuild a starter,
advance ticks, alter funds/population or retarget a manual save. No save
schema or Recovery format changed.

### Real-map activation and resources

These final Release measurements use the unchanged original map revisions
listed above and the same `edge-byte-4x4`/Slot-8 profile as the baseline.
Each supported selected object yields one model-piece instance, not one
instance per owned cell. Source objects, supported selected pieces, active
instances and rendered claims are independently reported.

| Map | Source objects / descriptors | Stone supported pieces / active instances | Stone rendered cells | Unresolved selected stone pieces | Visible result |
| --- | ---: | ---: | ---: | ---: | --- |
| Badaling | 53 / 53 | 53 / 53 | 740 | 0 | Connected main run with gate/towers; existing boundary diagnostics retained |
| MPWall1 | 53 / 53 | 53 / 53 | 740 | 0 | Connected main run with gate/towers; existing boundary diagnostics retained |
| MPWall2 | 51 / 51 | 51 / 51 | 708 | 0 | Connected supported sections in primary captures |
| Handan | 53 / 53 | 53 / 53 | 740 | 0 | Wall active; local body occlusion by existing elevation overlays |
| Xia | 0 / 0 | 0 / 0 | 0 | 0 | No Great Wall activation; control pixels unchanged |
| Chengdu | 0 / 0 | 0 / 0 | 0 | 0 | No Great Wall activation; control pixels unchanged |

All rows use **explicit material preview**, stone/material 3, source
`explicit_preview`, with original context unverified. Earthen/material 2
also activates 53/53/51/53 instances. Ruined/material 1 activates
49/53/51/49: Badaling and Handan each have four phase-2 road pieces whose
variant 40 is outside the 40-record Ruined runtime group. Both F1 sections
name the exact missing variant/archive and retain historical preview. MP
road pieces have saved phase 1 and use the inherited Terrain road group.
No phase was changed to finish a wall, and no neighboring record was used
to hide an unsupported state.

Stone phase-10 selection requests `DATA/China_Mon_Greatwall_10.sg3` and
resolves the actual `DATA/China_Mon_GreatWall_10.sg3`. Earthen phase-10 uses
`DATA/China_Mon_Earthen_Greatwall_10.sg3`; Ruined uses
`DATA/China_Mon_Greatwall_Ruined.sg3`. The existing phase/material and
gate/road inheritance logic chooses registrations; historical Graphics-IDs
do not authorize their identities. Direct archive validation found 42
runtime records in each phase-10 Stone/Earthen group (38 static Type-30 and
four unsupported Type-1 layouts), and 40 static Type-30 Ruined records.
All selected real-map static assets decoded successfully.

The new resource helper scans only immediate `DATA` entries, bounded at
4,096, for known Great Wall SG3/555 dependencies. It prefers exact spelling,
then requires a unique ASCII case match and retains canonical containment.
Ambiguity, missing dependencies, archive/DATA symlinks and bitmap paths
escaping DATA are rejected.
Associated known external Ruins/Zeus-system 555 paths are covered. Other
resource families retain their existing path behavior. Original files were
not renamed or modified; no lookup occurs during rendering.

### Visual, picking and native operation evidence

Scripted captures use production Release objects/libraries and the actual
`StoredMapSession`/`StoredGraphicsRenderer`, SDL dummy/software at 1280×720.
The primary view is 1×; 2×, 4× and overview captures supplement it.
Automatic's 48/48 captures are byte-identical to the rebuilt clean baseline.
With Stone selected, all 32 required saved-snapshot/control captures also
match the baseline exactly. These comparisons preserve Ground/Water,
Rock/Pinnacle, roads, ordinary walls and historical snapshot presentation.

Claims have complete unique member ownership. The existing camera-zero
marker/front-cell path applies signed saved height ×40 once; culling and
cached alpha picking use that geometry. Actual alpha picks identify original
marker owners on all four maps (5/6/6/6 distinct visible original IDs in the
primary regions). Complete 4×4 claims are drawn once per piece; these are
not 16 independently drawn images. The scripted runner does not constitute
native input acceptance.

Native operation was separately performed through the final local
`OpenEmperor.app`, using its normal menu and MapDebug modes. A fresh menu
started at Automatic; `N`, ten Down presses and four `G` presses selected
Badaling and `preview-stone`, then Enter loaded the ordinary paid City-v11
starter. Space paused it at tick 39/funds 100/population 24. Native F5 showed
“Saved tick 39”; F9 showed “Loaded tick 39 (paused)” with the same values.
Only an isolated ignored `native-final-settings/` app root was used.
The save and protected recovery start point were created by the ordinary
app path; existing personal saves/recovery histories were not overwritten.

The normal final bundle was also opened on Badaling and MPWall1 via the
public MapDebug CLI. Both native overview windows visibly showed the
connected wall, gate/towers and retained surrounding terrain/water/mountains.
On Badaling a native wall selection at storage (72,100) followed by F1 showed
`explicit_preview`, `preview-stone`, material 3,
`original_context_verified:false`, original ID 13, variant 24 and resolved
Stone-10 physical record 225. Native wheel zoom visibly changed the camera
view; `R` restored its overview. Brief automation arrow/WASD presses did not
establish continuous keyboard-pan acceptance, so that specific native check
remains unconfirmed. Native screenshots were inspected in the tool session;
the ignored PNG artifacts are the separate scripted captures. No original
game was run for visual comparison.

Handan's partial appearance was traced before considering any height fix.
Original ID 1/type 256/subindex 0 has local (67,20), storage origin (96,49),
draw marker (96,52), signed marker height 4, phase 10/model piece 16/effective
view 0. Stone-10 selects variant 16/physical 217 (318×238). Its decoded sprite
has an intact brick body. That lower body is in the existing early Type-30
base pass; later historical Elevation overlays at (95,49)/physical 208 and
(95,50)/physical 355 overwrite representative pixels. The ignored
`Handan-overlap-trace.json` records exact order and color changes. Whole-scene
base/overlay ordering and original height normalization remain unresolved.
No universal offset, altered footprint, replacement sprite or global
projection change was introduced. Existing boundary diagnostics also remain
visible. This is not a gap-free whole-image visual PASS.

### Regression, performance and local package

After the final implementation changes, complete suites passed:

| Configuration | Result | Wall time |
| --- | --- | ---: |
| Debug | 92/92 | 768.74 s |
| Release | 92/92 | 70.58 s |
| ASan/UBSan, RelWithDebInfo | 92/92; no sanitizer diagnostics | 227.02 s |

They include existing endurance/determinism, City-v10 and City-v16 rule 1/2,
save/resume, recovery, road responsiveness, renderer and resource checks.
Context tests cover missing mode, ordered first matching goals, validated
no-match context, saved-material independence and invalid preview inputs.
Synthetic identity changes stay in memory. Session tests compare World and
manual-save targets across option changes and failed preparation. Renderer
tests cover explicit-preview failure before publication and texture cleanup.

The following is a sequential single local sample in the same Release
configuration/backend: 300 render/inspect/pick frames per layer and map,
six maps, 1280×720 software renderer. Concurrent test/native activity affects
wall-time, including unchanged control scenes. It does not isolate a speedup
or performance regression.

| Map | Load ms baseline → Stone | Scene RGBA MiB baseline → Stone | Extra physical assets | Regenerated ms/frame baseline → Stone |
| --- | ---: | ---: | ---: | ---: |
| Badaling | 152.92 → 346.05 | 33.52 → 53.79 | 26 | 2.256 → 3.185 |
| MPWall1 | 231.41 → 546.99 | 40.27 → 62.48 | 28 | 2.075 → 3.013 |
| MPWall2 | 234.64 → 342.25 | 31.91 → 45.56 | 18 | 2.452 → 3.418 |
| Handan | 229.80 → 375.36 | 36.64 → 58.85 | 28 | 2.268 → 3.024 |
| Xia | 262.62 → 268.55 | 17.97 → 17.97 | 0 | 0.844 → 1.390 |
| Chengdu | 201.14 → 289.33 | 17.79 → 17.79 | 0 | 2.896 → 3.295 |

Maximum Stone scene allocation is MPWall1's **65,515,872 bytes**, leaving
**1,592,992 bytes** below the unchanged 64 MiB limit. Shared physical assets
are eagerly deduplicated; no budget increase or snapshot-image removal was
used. All repeated-frame counters remain zero for file I/O, decodes,
uploads, World copies/commands, BFS and route refreshes, with one cached
painter-order build.

The final local package is
`.local/great-wall-context/final-dist/OpenEmperor.app`, arm64 Release,
display version 0.1.0-alpha.2, revision `91891af8bba0`, dirty-worktree flag
true, minimum macOS 26.0. Its ZIP SHA-256 is
`63d58cb47ff5a0a65b0faa4108eb0e77cd9f72ac9d0728f05a26a50957c44397`.
Packaging passed complete Release CTest, relocation/unzip smoke checks,
Industry/City-v11 save/restart checks, signatures/dependencies and all 13
negative bundle checks. The package-report generator's `desktop_launch`
field remains `not_checked`; the separate native observations above supply
this milestone's local desktop evidence. An independent clean Mac was not
tested. This is an ad-hoc signed, unnotarized local test candidate, containing
no original game assets. No publication or release was performed.

Final hash audit found no missing or changed files among all 51 original
sources and 29 frozen project files. All 255 source/test/resource/CMake files
matched the final tested-code snapshot. Logs, reports, manifests, runners,
decoded research and captures remain ignored under
`.local/great-wall-context/`. No commit, push or tag was made.

### Changed files and remaining boundary

The 33 changed/new files are scoped to the following groups:

- Context/activation: `GreatWallSelector.{h,cpp}`,
  `GreatWallMapPresentation.cpp`, `StoredMapSession.{h,cpp}`,
  `StoredArchiveRegistrations.cpp`, `RegeneratedMapRenderPlan.{h,cpp}`,
  `LandscapeProvenance.cpp`.
- Normal app integration: `MapBrowser.{h,cpp}`, `MapRenderCheck.{h,cpp}`,
  `MenuSession.{h,cpp}`, `main.cpp`, `StoredGraphicsRenderer.cpp`.
- Narrow dependencies/build: `GreatWallDependencyPaths.{h,cpp}`,
  `AssetCatalog.cpp`, `Sg3ImageLoader.cpp`, `CMakeLists.txt`.
- Tests: `GreatWallDependencyPathsTests.cpp`, `GreatWallSelectorTests.cpp`,
  `GreatWallPresentationTests.cpp`, `StoredGraphicsTests.cpp`,
  `MenuSessionTests.cpp`.
- Documentation: this file, `map-first-draw.md`, `architecture.md`,
  `stored-graphics-preview.md`, `KNOWN_ISSUES.md`, `AGENTS.md`.

Original mode/current-player/mission binding for the standalone entry,
unsupported Type-1 layouts and special composition, original general height
normalization, full first-draw order and whole-image acceptance remain open.
The activation gap is closed for an explicitly chosen, usable preview in
the normal app. No further gameplay or campaign-parser work belongs to this
bounded milestone.
