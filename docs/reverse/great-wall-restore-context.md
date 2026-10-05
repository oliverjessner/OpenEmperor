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

## Scene-composition follow-up (2026-10-04)

The subsequent bounded milestone starts at clean
`3b834c2bbc1c999f38cf7d54e63ae8dacd1a758f`. The earlier partial Handan
acceptance and overwrite report above describe the historical activation
baseline. A fresh Release trace now captures the initial wall Base write
and subsequent Terrain/Elevation writes: original entity ID 1, Stone-10
variant 16/physical record 217 at origin `(96,49)`, marker `(96,52)`,
signed height 4, and existing front key 1880. Elevation 208/355 have earlier
spatial keys 1740/1760, but overpainted the masonry because its Base was
emitted before the entire spatial stream.

The corrective OpenEmperor preview contract gives validated Wall/Tower/Gate
model pieces an explicit `SpatialCombined` policy. Their already loaded
complete texture participates once at the existing front key. The early
Base and separate Overlay paths omit those instances. Road pieces and
other landscape preserve split passes; saved Snapshot is unchanged.
Markers, anchors, heights, material decisions and all raw original facts
remain as in the activation baseline. Cached full alpha makes visible
body pixels inspectable; F8 hides the complete structural image.

This addresses the diagnosed global-pass failure, without suppressing
historical Elevation, moving the wall, changing a comparator or assigning
walls a permanent foreground priority. Exact original restore context,
post-load height normalization, historical-overlay eligibility and
interpenetrating sprite composition remain open. The Ruined phase-2 Road
variant 40 remains a separate missing-asset limit.

See [the fresh baseline, hypothesis audit, bounded EXE evidence and final
validation](../rendering/scene-composition.md). Local traces and original
pixel captures remain ignored under `.local/scene-composition/`.

Final scene-composition validation on 2026-10-05 passes all 93 tests in
Debug, Release and optimized ASan/UBSan, plus the final local arm64 package
validation. The four-material/six-map matrix compares 192 captures with
144 byte-identical unaffected controls. Twelve complete City-v10/v16 save
documents and matched-resource public check/resume reports retain exact
baseline state. Actual native Handan/Stone loading in the ordinary Sandbox
was exercised. Full native pointer/panning acceptance remains open because
requested automation coordinates did not reach the intended SDL positions;
the linked report separates this boundary from scripted pixel/picking
acceptance. No complete native visual PASS or original-game parity is claimed.

## Ruined phase-2 road compatibility (2026-10-05)

This follow-up starts at clean
`aed9619fe2bd3aaec79118548313fa4772308a46`. The prior activation and
scene-composition results above remain historical. Their Ruined Road
fallback is now explained more precisely: **LIMITATION EXPLAINED**.
The investigation does not establish a corrected original rendering branch
or a visible transition **FIXED** result.

### Direct decision and supported limit

[The independent bounded trace](graphics-id.md#ruined-phase-2-road-compatibility-2026-10-05)
reverifies the pinned executable, the Road controller and singleton writer,
including VA/RVA/physical offsets and direction-table bytes. Its SHA-256 is
unchanged; 1,586 disassembly-byte lines in 27 inspected windows match the
binary. No original executable was run or copied into production.

Road producer `0x57d860` retrieves its own entity's extended state at
`0x57d888`, reads signed phase `+08` at `0x57d91c`, and compares it with 2
at `0x57d98a`. Within admitted phases 1/2, phase 1 selects Terrain group
`0x61e` and variant 0/1. Phase 2 selects slot-8 group `0x1001`, adds 40 at
`0x57d9cd`, and selects variant 41 for effective views 0/4 or 40 for 2/6.
The exact tables are `0x57da0c` and `0x57da28`. The existing pure selector
implements this observed branch; material does not change its choice.

The earlier virtual `+64 -> 0x570da0` tests runtime state byte `+25` and
can select a group-`0x612` placeholder before the phase branch. This flag is
loaded from one serialized byte at extended-record offset `+35`: schema
10 call `0x5620df` or schema 9 call `0x56227f`, through
`0x503d50 -> 0x4c95f0 -> 0x4c9600`. Fresh Badaling/Handan Road IDs 50..53
all have raw zero/decoded false there; the MPWall1/MPWall2 Road controls
are also false. Thus an overlooked true placeholder flag does not explain
these raw source cases. This finding does not model every later original
lifecycle writer of that flag.

The Road producer has no material `+5c` read or archive-registration call.
It inherits slot 8 from preceding wall/tower registration. For the examined
final wall/tower states, `0x57c020..0x57c04e` and
`0x57d445..0x57d46f` select Ruined for material 1, Earthen phase 10 for 2,
or Stone phase 10 for 3. The registered slot is mutable, while a physical
asset's identity includes its archive. Road phase 2 does not independently
register a phase-2 archive.

The immediate deserialization tail, placement/dispatch windows, Road
producer and singleton writer contain no material-1 phase-2-to-1
normalization. Mode-1 producer writes affect cell height/flags; the material
restore helper changes `+5c`, not phase `+08`. Producer flag `0x08` at
`0x57d9f8` makes `0x56413c` skip the rectangle writer, and
`0x57d9ec -> 0x5724e0` writes the same selected graphic into the exact
singleton cell. No hidden variant substitution was found there.

This is a bounded absence finding. Higher-level original construction and
mission invariants remain unproved, so material 1 plus phase 2 cannot be
declared globally illegal in Emperor. The standalone entry still lacks
verified original mode/current-player goal context. This milestone neither
reopens the entire campaign format nor assumes a mission/player value.

A fresh raw metadata read establishes the actual incompatibility before
texture decode: Ruined has reported in-use 240 minus the 200-record system
prefix, giving one runtime group `[0,40)`. Stone/Earthen phase 10 each have
242 minus 200, giving `[0,42)`. The separate dummy translation gives first
physical member 201. Thus Ruined 39 maps to valid physical 240; 40/41 would
be physical 241/242 outside its admitted group. Stone/Earthen admit those
two records; 42 is outside. There is no adjacent Ruined runtime group and
no evidence for borrowing a foreign transition image.

The hypotheses therefore separate as follows:

- Incorrect phase/axis selection: the current admitted 1/2 branch matches
  the direct EXE decision and tables.
- Incorrect preview registration inheritance: no such defect was found;
  each piece retains its selected archive from the existing ordered
  preparation path. Physical texture deduplication is independent.
- Missing normalization: no normalization is evidenced in the inspected
  bounded paths; a phase rewrite is unjustified. Higher-level invariants
  remain open.
- Resolver or group bounds: the existing strict check is correct for the
  freshly read 40/42-member groups, with one skip/dummy correction.
- Explicit preview incompatibility: established. PreviewRuined forces
  material 1 while keeping saved Road phase 2, whose required variants
  40/41 are absent from the inherited Ruined group.

### Load-time diagnosis and unchanged raw state

The implementation retains `select_great_wall`'s numeric group/variant
request and the existing registration path. `GreatWallMapPresentation`
captures the archive on each piece while iterating the preserved original
record collection; the render plan resolves the pair `(slot,archive)`.
This rules out an accidental last-global-slot reuse in the inspected preview
code. The original caller paths nevertheless require explicit separation:
temporary-manager `0x52f030` skips following same-type monument entries
after retaining the type at `0x52f155`. Its creation flag reaches
`0x56a124 -> 0x563850`, which constructs ascending Model subindices and
redraws ascending contiguous IDs at `0x563ab3`; its optional completion
loop at `0x563bcc..0x563bdc` assigns controller phase-count minus one to
newly created state. Road vtable `0x7b99f0 +0c -> 0x576c40` returns constant
3, without reading material or entity state, so this writes Road phase 2.
The completion condition at `0x563b14..0x563b31` is mode 1, or
`0x53a4e0(type)` plus created material 1; the raw predicate tables return
true for examined types 256/257/259. Creation's `0x5638a3 -> 0x563720(type)`
uses the same mode/current-player goal decision as restore argument -1,
with default material 1 returned directly for the type. Consequently even
this material-1 completion path supplies no phase-2-to-1 conversion. It
does not establish a global original prohibition on Ruined plus phase 2.
The separate loaded-manager redraw
`0x4afef0 -> 0x4aff5a -> 0x563fd0(id,0)` visits each active monument in the
live array. Neither establishes a saved phase-2-to-1 conversion. In
particular, one serialized record must not be described as one independent
temporary-manager original restore call.

Fresh raw-map reads show complete ascending subindices 0..52 in Badaling,
Handan and MPWall1, or 0..50 in MPWall2, with ascending contiguous original
IDs. Their four Roads are the final Model pieces, after the wall/tower
registrations and non-registering gates. The preview vector matches this
relevant Model/producer order for the examined corpus. Full equivalence of
all original standalone entry/reconstruction paths remains open.

The same bounded resolver now records its actual local group begin/end,
group status and variant refusal for load-time provenance. Only when that
resolver reports an out-of-group request for an explicit PreviewRuined,
material-1 Road with saved phase 2 is the refusal classified as
`saved_phase_preview_material_unsupported`. A missing archive remains
`archive_unavailable`; an unavailable group and an ordinary out-of-group
request retain their separate statuses. Prepared selection, decode failure
and atomic activation status are also distinct. The classification explains
the preview limit; it grants no renderer ownership or successful decode.

F1 retains serialized phase/material/height/orientation, selector phase,
effective view, group, requested variant and selected archive. Selector phase
equals the saved phase: there is no newly derived normalized phase. The
group refusal remains available alongside the more specific preview reason,
so an existing archive's unsupported request is not labeled a missing
download. A concise existing status-area notice counts the affected pieces
and confirms that historical fallback is retained. Earthen/Stone remain
separately selectable in the existing setup. There is no automatic material
switch.

Source objects, numerically selected pieces, prepared instances and
renderer-active instances/cells remain separate counters. A valid numerical
selection can still fail compatibility or eager activation. The complete
claim/readiness path retains historical fallback for such a piece, without
a partial reserved area or a second draw. No arbitrary replacement,
phase-2-to-1 conversion, clamped/modulo variant or extended Ruined archive
was introduced. Original bytes and sandbox World/save/road authority remain
outside this presentation decision.

These paragraphs describe the inspected implementation and static evidence.
Fresh real-map activation counts, rendering/picking checks, full suites,
performance and native observations follow below; earlier counts are not
silently reused as new measurements.
The user's comparison screenshot remains unassigned to a map, material mode,
storage cell or particular variant. Ignored evidence is retained under
`.local/ruined-transitions/trace/`.

### Fresh real-map baseline and final comparison

Independent probes loaded the untouched original data for five maps in all
five modes, first against the archived clean baseline, then against the
final frozen production code. These are fresh measurements, not the prior
milestone's counters. The following table describes **PreviewRuined**;
fallback counts refer only to selected Great Wall pieces that could not
activate, not all historical terrain or stored graphics.

| Map | Source pieces | Selected pieces | Active instances | Active cells | Unsupported fallback pieces |
|---|---:|---:|---:|---:|---:|
| Badaling | 53 | 53 | 49 | 736 | 4 |
| Handan | 53 | 53 | 49 | 736 | 4 |
| MPWall1 | 53 | 53 | 53 | 740 | 0 |
| MPWall2 | 51 | 51 | 51 | 708 | 0 |
| Xia | 0 | 0 | 0 | 0 | 0 |

On Badaling and Handan, original IDs 50..53 / Model subindices 49..52 are
the four refused singleton Road pieces. Badaling uses Model type 257,
Handan type 256. Each retains saved phase 2, orientation 0 and saved
material 3, with explicit preview material 1. View 0 plus the Model EAST
position gives effective view 2, requesting group `0x1001`, variant 40,
side 1. The inherited registration snapshot comes from the preceding
manager entry 49 and resolves to canonical
`DATA/China_Mon_GreatWall_Ruined.sg3`, runtime group `[0,40)`, first physical
member 201. The exact resolver refuses variant 40 before publishing a
packed/physical ID, texture or claim. F1 reports
`saved_phase_preview_material_unsupported` alongside the actual out-of-group
status. The four old `unsupported_layout` historical diagnostics remain;
there is no substituted transition or empty newly reserved footprint.
Variant 41 is covered by the opposite-axis selector/bounds tests; it is
not relabeled as an observed variant in these eight real failures.

MPWall1 IDs 50..53 and MPWall2 IDs 48..51 retain phase 1. Their four Road
pieces per map select Terrain `0x61e`, variant 1, group `[581,599)`, runtime
582 and physical 783 after the single system/dummy translation. All remain
active. Earthen and Stone retain all selected pieces: 53/53/53/51 active
instances and 740/740/740/708 active cells on Badaling/Handan/MPWall1/MPWall2,
with no selected Great Wall fallback. Auto and Historical retain zero
selected and zero active Great Wall instances on all five maps; their
source counts remain available. They preserve the historical path while
original restore context is unknown. All explicit-preview original-context
verification counters remain zero. Xia has no Great Wall source or active
pieces in any mode.

The visible wall/map rendering is deliberately unchanged. All **340/340**
matched direct production `StoredGraphicsRenderer` captures are
byte-identical: 25 overviews, 75 primary centered 1×/2×/4× captures across
all five maps,
and 240 per-Road centered captures. All 25 census, ownership, budget,
upload and alpha-picking comparisons also match. The probes check 622
active-instance height/anchor cases per stand and 582 structural instances
with exactly one spatial draw and no early Base draw. Signed height changes
shift Y by −40 exactly once. Road pieces retain the existing split-pass
composition. These are software production-render checks, not native human
visual acceptance.

Eleven ordinary `MenuSession -> Sandbox` cases exercise Badaling and Handan
in all five modes plus Xia/Auto, using scripted SDL setup input and the
unchanged City-v16 rule-2 paid starter. All 22 complete ordinary save
documents at ticks 0/1600 and all 11 Buildability masks match the baseline;
within each map the World/save state also matches across presentation
modes. The starter still costs 1,280 through 26 normal commands and starts
with 20 funds. No manual-save target or recovery checkpoint is written by
this comparison probe. Of 55 before-Present Sandbox captures, only ten
Badaling/Handan Ruined status bars change. Every image is identical outside
the existing 24-pixel status rectangle. Both the new notice and the actual
nonempty ordinary status are visible on separate rows:

> Ruined preview: 4 road pieces unsupported for saved phase 2; historical fallback retained.

The notice uses cached load-time text. For each direct case, 100 pure
render/pick/F1 frames, and for each menu case, 30 paused frames, record zero
file reads/writes, decode, upload, World copies/commands, BFS and route
refreshes. Comparison/capture writes occur outside those measured windows.
All 57 audited original files (105,235,289 bytes, including the pinned EXE)
retain identical hashes. Ignored full provenance and comparisons are in
`.local/ruined-transitions/final/bounded-final-census.json`,
`final/comparison.json`, `final/menu-comparison.json` and
`review-findings.md`.

### Final regression and package checks

After the last production/test change, all three full configurations
execute **101/101 tests**, with no failures or skips:

| Configuration | Full CTest result | CTest wall time |
|---|---|---:|
| Debug, arm64 | PASS | 551.53 s |
| Release, arm64 | PASS | 70.05 s |
| Optimized ASan/UBSan, arm64 | PASS | 195.23 s |

The suites include Great Wall selector/presentation/claims, 40/42-member
group bounds, same-slot registration changes, Handan composition,
texture/zoom, input handlers, road responsiveness, deterministic World,
save/load and Recovery regressions. The external SDL library is not
sanitizer-instrumented. The wrappers compare the same 269 production,
test, resource and CMake files before/after and report no changes.
Independent review also confirms frozen World/save/input, asset decoder,
TextureCompatibility, SceneComposition and other landscape authority.
No per-frame resolver or new unbounded cache was added.

Additional existing checks pass: 100,000 simulation ticks with 12 JSON
roundtrips and parallel deterministic Worlds; 3,000 render frames with
loaded source files unavailable, zero decode/upload/order rebuild; and 100
menu sessions with all owned texture counters zero at session end. The
local arm64 Release package validation passes its separate full suite,
bundled/relocated/unzipped dependency and signature checks, original and
synthetic save/restart checks, and all 13 negative package cases. No SDL
installation or global setting was changed.

The final local candidate is
`.local/ruined-transitions/final-dist/OpenEmperor.app` and its adjacent ZIP,
display version `0.1.0-alpha.2`, revision `aed9619fe2bd`, dirty workspace
flag true, minimum macOS 26.0. It is an ad-hoc-signed local test candidate;
Finder/Gatekeeper and independent clean-Mac acceptance remain NOT RUN.
It contains no original proprietary assets. No commit, push, tag, release
or publication was made.

### Native observations and remaining human check

A separately named, isolated clone of that final bundle uses only new app
roots under `.local/ruined-transitions/`. Its app and SDL `__TEXT,__text`
hashes match the validated bundle; only bundle identity/signing changed.
Existing user app roots, saves and running sessions were not taken over.
The observed Badaling window uses Cocoa/Metal, SDL 3.4.14 and a
1100×700 output at density/scale 1.

Badaling: native `N -> Return` opens the normal New Sandbox setup and
loads an empty City-v10 rule-1 session in explicit Ruined mode. The fitted
whole-map view visibly shows the counted four-piece notice and the existing
ordinary status on separate rows. Space pauses at tick 1236; F1 toggling and
F5 saving are exercised. The isolated saved document remains schema 10
with no construction command. The probe exits normally. A per-piece F1
selection and native 1×/2×/4× Road close-up were not completed.

One computer-control click targeting screenshot position `(217,206)`
reaches native SDL MouseDown at window position `(217,174)`, but MouseUp
arrives at `(0,0)`. The menu correctly refuses the mismatched release;
the 25-record bounded observer has no overwritten records. This is an
**automation delivery limitation**, not evidence for an original Road
selection fault or a new player-input defect. No coordinate compensation,
input change or additional pointer attempts were introduced.

Handan: the normal native setup is observed after `N -> G -> G`, selecting
Auto -> Historical -> Ruined; Return loads the empty City-v10 session.
The fitted map at tick 34 visibly shows the same four-piece notice and
ordinary status. Before the next action, computer control reports that the
user changed the app. Automation stops immediately and leaves that process
and its isolated app root untouched. Subsequent pause, F1, save, alternative
mode and camera/zoom steps are not claimed as executed. These two native
load/notice observations do not constitute a full native transition PASS.

Oliver's earlier zoom, arrow-key and resize report is recorded exactly as
**User-reported PASS for the tested City-v10 session**. It does not approve
City-v16, road drag, every building stage, demolition/Recovery, external
display changes or a complete human playthrough. The unassigned comparison
screenshot still has no proven map/material/cell association.

The executable ignored launcher
`.local/ruined-transitions/run-wall-test.command` starts the validated final
bundle with separate persistent settings/saves/recovery. On first use it
selects Badaling, PreviewRuined and an empty City-v10; global defaults stay
unchanged. Double-click it, choose New Sandbox, check the map/mode and start.
Optional Terminal arguments select a countercase or a supported alternative:

```sh
./.local/ruined-transitions/run-wall-test.command Handan preview-ruined
./.local/ruined-transitions/run-wall-test.command Badaling preview-stone
```

The remaining human transition check is **NOT RUN**: inspect Badaling and
Handan separately at 1×, then 2×/4×; use F1 on the four Road transitions to
compare saved phase, selected material/variant, actual group bounds and
fallback; move the camera and zoom; explicitly reload Earthen/Stone and
Auto/Historical; inspect MPWall1/MPWall2 phase-1 controls. Human observations
must be recorded separately from the scripted capture and native keyboard
smoke evidence above.

Final status: **LIMITATION EXPLAINED** for explicit Ruined plus saved
phase-2 Road under the actual 40-member archive. Diagnosis and the normal
app notice are implemented and verified. Compatible Earthen/Stone previews
remain usable; raw state and the strict historical fallback are preserved.
Complete original restore/lifecycle invariants, height normalization and
original per-pixel fidelity remain **UNRESOLVED**. This milestone stops at
the bounded Road compatibility problem.
