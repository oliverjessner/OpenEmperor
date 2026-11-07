# Original ramp assets and authored-link validation

Follow-up: the [isolated source-cell prototype](original-ramp-cell-prototype.md)
now directly renders 233–244 and investigates ordinary actor height. It supports
a continuous central-path model with a half-height source-cell center. The
production result recorded below remains historical; original pass composition,
normalization and production adoption are still open.

Investigation started at clean HEAD
`2efa8c14b76edaadc8dc9e307741a546a66e2eb8` on 2026-10-15. Original inputs
are read-only. Rule6, policy3, schema22, RampLinks, projection, path progress,
actor feet, prices and Bridge rendering are unchanged.

## Original ramp sprites do exist

The complete regular Elevation archive was decoded, numbered and visually
inspected, rather than extrapolating from the previously examined pillars.
There are verified original ramp graphics, including road-dependent variants.
Their identity has independent code, editor-producer and saved-map evidence.
That finding supersedes a broad inference that no original ramp art is known.
It does not establish original traversal or compatibility with an authored
center-to-center RampLink surface.

| Archive / regular physical records | Metadata and successful decodes | Visually inspected contact sheets |
|---|---:|---:|
| `DATA/China_Elevation.sg3`, 201–372 | 172 / 172 | 9 |
| `DATA/China_Terrain.sg3`, 201–1443 | 1243 / 1243 | 57 |

The existing catalog and SG3 decoder supply the metadata and pixels. Sheets
show every record at its original resolution and 2x nearest-neighbor scale on
neutral gray, with physical ID, group, dimensions, type and alpha length.
Individual transparent PNGs, inspector reports, complete catalog records,
per-record decode logs and input hashes remain under
`.local/original-ramp-audit/assets/`. No original pixels or disassembly are
included in Git or application resources. A successful decoder result is
recorded separately from visual inspection.

Reproduce the export with existing built tools and ImageMagick:

```sh
cmake -S . -B .local/build-ramp-audit -DCMAKE_BUILD_TYPE=Release \
  -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build .local/build-ramp-audit --target openemperor-assets openemperor-inspect
python3 tools/audit_original_ramp_assets.py \
  --data .local/gog-extracted/app --build .local/build-ramp-audit \
  --output .local/original-ramp-audit/new-assets
```

The output directory must be fresh, ignored and outside the original data tree.
The reusable exporter deliberately reports `visual_review_performed:false`;
generating sheets never claims to inspect them. This investigation's separate
`assets/visual-review.json` lists all66 sheets actually opened. A full tool
rerun in `repro-assets/` reproduces every decoded PNG SHA and every viewed
contact sheet's RGBA signature, with unchanged input hashes.

All 1415 examined regular records decoded. These records have no declared alpha
stream or mirror reference; their decoded transparent exterior is still
important. Elevation includes three regular bitmap groups, and Terrain seven.
The Terrain review includes roads, irregular ground, rocks, coast/water,
quarry, mounds and pinnacles. It found no additional clearly applicable
single-height road-ramp family. General was not scanned without a further
selector lead: the positive family is already in Elevation.

| Elevation physical records | Visual finding | Evidence boundary |
|---|---|---|
| 233–236 | Four earth/scree ramps | Verified ordinary ramp selector and real saved-map matches |
| 237–240 | Stone stair/ramp treads with banks | Verified road-dependent `+4` selector branch; saved238/240 examples on Badaling |
| 241–244 | Rough cobbled ramps with banks | Verified road-dependent `+8` selector branch; context remains partly unresolved |
| 279–282 | Lower-bank earth slopes | Visually identified; not assigned the ordinary family’s selector contract |
| 287–290, 295–298 | Lower-bank stairs / cobbled slopes | Visually identified; broader half-slope composition remains open |
| 201, 211, 229, 231, 312, 344, 349, 353, 359 | Varied cliff, cap and bank components | These historical samples alone do not characterize all ramp art |

The ordinary four have native sizes78x66,78x85,78x84,78x68, Type30, side1,
base3200, group1 `China_Elevation_Tiles.bmp`, with no animation or mirroring.
Terrain782–799 is the independently verified original basic-road family already
used by RoadVisualProfile. Its fine mottled earth and irregular small curb
stones are retained as runtime material sources.

## Selection: three different index domains

The pinned EXE is SHA-256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
Fresh read-only windows confirm that resource lookup `0x408170` divides a
positive key by512 and uses a one-based group position. **Literal `0x601` is
Terrain slot3/group1**, not Elevation slot16. At `0x53ec4e → 0x53ec5d` it is
a fallback when the height pattern did not resolve and some neighbor is higher.
It sets terrain`0x200` and adds a cell to the optional normalization list.
Physical record201 in Terrain and physical201 in Elevation are distinct assets.

The actual primary height path is
`0x53eae0 → 0x473170 → 0x473210`, using147 rows of64 bytes at`0x825c28`.
It compares eight **signed neighbor-minus-current heights**, in storage order
N,NE,E,SE,S,SW,W,NW. Pattern3 is a wildcard; pattern4 accepts absolute delta<=1;
other pattern values compare exactly. The first matching row wins. Camera0
uses these explicitly ramp-qualified rows:

| Row | Higher neighbor, delta+1 | Opposite / lateral cardinal neighbors | Elevation resource key | Physical record |
|---:|---|---|---|---:|
| 67 | `(0,+1)` | wildcard / equal height | `0x204e` | 233 |
| 68 | `(-1,0)` | wildcard / equal height | `0x204f` | 234 |
| 69 | `(0,-1)` | wildcard / equal height | `0x2050` | 235 |
| 70 | `(+1,0)` | wildcard / equal height | `0x2051` | 236 |

OE53 re-read the four bounded table rows through their PE section mapping.
The opposite cardinal is pattern3 (wildcard); equal opposite/low-join height
is the prototype and Rule20 authored safety constraint. The earlier table
incorrectly called both opposite and lateral neighbors equal. See the
[production authored comparison](oe53-terrain-integration.md).

These rows require raw`0x400`; rows103–106 have looser neighborhoods with the
same family. The v213 runtime layout independently converts the resource groups
to local32–35, packed saved IDs`0x40020–0x40023` and physical233–236.
`0x473760` writes the image at the **low saved-height source cell**, sets
candidate`0x40`, and clears the draw-properties low nibble. Its separate
raw`0x800000` branch can also write a secondary neighbor.

The editor independently establishes the ramp-bit producer:
tool176 →`0x404b77 → 0x4743c0`, eligibility helper`0x4742b0`, then the
raw`0x400` toggle at`0x474424`. The actual model text labels176
`BUILD_MAP_RAMP_ELEVATION`; that label supports the independently traced code.
Tool150 instead reaches`0x4744b0` and toggles`0x800000` (Half Slope).

In the ordinary non-half-slope branch, raw road`0x40` adds4 to the selected
group if helper`0x4b7270` is false and global`0xd6240c` is zero. It adds8 if
that helper is true or the global is nonzero. This proves the stairs and
cobbled families’ road dependence without choosing a variant by appearance.
The helper returns false when terrain`0x10000` is set (the stairs branch can
still select); otherwise it tests a separate signed
cell byte at`0xf11c70` through`0x44f180` for>4. Its complete restored semantics
and the global’s context remain unresolved.

All21 saved ordinary ramp references across Banpo2, Anyi3 and Badaling16 match
rows67–70 and the exact camera0 physical records. Xia has none. Examples:
Banpo`(89,92)`235, Anyi`(142,98)`236 and`(132,160)`234,
Badaling`(131,159)`233. All have objects0/candidate`0x40` and raw`0x600`,
except one Badaling`0x602`. Badaling also has stairs238 twice and240 once
with raw`0x640`. The isolated raw comparison corroborates these saved identities;
it does not replay the original mutation order or prove every first-draw image.

Normalization at`0x5403c0` is conditional on global`0x88ec38`, iterates the
fallback list, retains further exclusions, and calls the setter at
`0x540511 → 0x474b10` with **max(signed height−1,0)**. It is not an
unconditional decrement of all elevation or ramp cells. Complete original
height restoration, actor interpolation and first-draw composition remain open.
Fresh evidence and the reproducible extractor are in
`.local/original-ramp-audit/selector/` and `selector-findings.md`.

## Implementation decision and the measured native-fit blocker

An unchanged sprite fit was tested before choosing a replacement. The four
original sprites are pasted without scaling, cropping or warping at the existing
ordinary Type30 low-cell anchor`(39,H−20)`. The reference sheet overlays the
actual high/low cell centers and the unchanged authored one-cell-wide RampLink
quad under40/20 projection and signed height40.

The native horizontal raster covers[-39,38] relative to the low center. The
authored quad reaches[-60,20] or[-20,60], leaving21–22px beyond that raster.
Two ascending authored orientations are exactly edge-on, while original233/236
retain broad surface/support artwork. The original ramp is a **source-cell
landscape representation**, and the present link spans **two adjacent cell
centers**. Equal nominal height does not make their coverage or actor surface
equivalent. Changing the anchor cannot recover the missing original foot and
surface rule. See `selector/native-fit-sheet.png` and `native-fit-report.json`.

Consequently this revision does not stamp, stretch, rotate, mirror or relabel the
original ramp sprites as verified RampLink rendering. Original full-raster
integration remains blocked on original surface/anchor/composition evidence.
The implemented bounded fallback uses the already loaded compatible original
**road** RGBA through the existing straight/T/cross mask lookup. It adds no
physical-record selector table, asset upload, map permission or saved field.

The previous coordinate interpolation between a whole road and a reflected
12x8 earth rectangle expanded individual source texels into long beige flowing
streaks. Every neighboring middle ramp also repeated the exact same complete
material strip. The new eager material preparation instead selects unchanged
native road or earth texels with deterministic spatial coverage at joins.
Six-by-four native pieces come only from that same image’s bounded12x8 earth
rectangle. Eight cached lateral phases vary neighboring strips. The material
coordinate itself wraps, so phase7/+0.5 and phase0/−0.5 share the same field.
No RGB
classification, resampled gradient, per-frame random sampling or texture is used.
Endpoint and exterior-road sampling, fallback/F6 material, half-open geometry,
opaque cutouts, authoritative feet, picking and local painter components retain
their existing contracts. Eight phases are bounded variation, not unlimited
aperiodic material or a recovered original slope selector.

## Verification and remaining acceptance

The production fixture loads the same authored map and paid SaveDocuments,
uses actual SandboxView rendering and captures isolated, three-connected and
all-four states at1x/2x/4x/fractional zoom. Added evidence includes upper/lower
join crops, F6 at every zoom, existing F8 layers and pure preview/release/cancel,
and a fifth state with a real loaded Clay courier at tick58, edge progress2.
Five independent process reloads compare complete saves and normalized map PNGs.

The separate144-case production RampPresentation atlas covers both storage axes,
both signed gradients, starting heights−2/0/+2, one through four adjacent ramps
and1x/2x/4x. It compares two backgrounds at projected opaque-foot/seam witnesses
and detects identical full interior material strips. Edge-on ascent witnesses
use the actual center footline; collapsed lateral coordinates are not presented
as a broad tread. These are authored synthetic renderer cases, separate from
paid full Sandbox scenes and human acceptance.

The baseline atlas reproduces the previous exact interior-strip repetition.
All screenshots and machine reports stay under
`.local/original-ramp-audit/{before,after,validation}/`. Original-game captures
are unavailable; decoded original art and the native-fit sheet are the direct
asset references. Automated geometry and save checks do not substitute for
viewing the resulting images.

Original full-raster restoration, broad ascending surface appearance in the
fixed projection, native desktop/human gameplay acceptance and complete
original scene parity remain open. This work cannot receive an overall original
ramp restoration PASS on the basis of green renderer tests.

## Recorded result, 2026-10-15

Final production captures are in `.local/original-ramp-audit/after/captures/`;
the readable comparison is `after/before-after-cropped-matrix.png`. Original
baseline, extended walker baseline and the first material correction are retained
separately in `before/` and `iterations/native-texels-v1/`. The final source
digest is`1de0683dfd975b6c02e84fc12e699ce74bb4ba4dc261c514a72e08a51270d6e6`.
Documentation-only updates are outside that runner's source digest.

Actually opened software images include isolated/three-connected/four-Ramp
comparisons at1x/2x/4x, both endpoint crops, actual tick58 courier, green preview,
F6 authored control, flat Snapshot and closed Ground/Elevation F8 controls.
The original large curling/chevron streaks are gone. The joined tread has no
observed internal curb or retained rock strip, and no observed clear-background
gap at inspected joins. The walker’s foot line and near-shoulder composition
remain plausible in that scene. Independent review still finds broad tan areas,
small recurring flecks, regular exterior road curbs and the visibly different
authored gray cliff wall. These are recorded limits, not original-style parity.

The144-case normal atlas passes5040 required two-background witnesses and
neighbor-strip checks; the separate `--atlas-wrap`144-case atlas adds5040
witnesses across phases6/7/0/1. Both X/Y wrap images at4x were opened before and
after the coordinate correction. All104 paid-scene PNGs stay byte-identical
between material iterations1 and2. The final total prepared Ramp cache is
1,582,753 bytes, within the unchanged bounds.

| Check | Final result |
|---|---|
| Relevant Release CTest | 11/11 PASS, 3.05s |
| Relevant Debug CTest | 11/11 PASS, 86.69s |
| Relevant RelWithDebInfo ASan/UBSan CTest | 11/11 PASS, 23.77s; no sanitizer diagnostics |
| Paid production scene / F5-F9 / previews | PASS;600 pure frames, zero prohibited frame counters |
| Before/after authority and controls | PASS;41 exact complete-save/camera/F6/Snapshot/no-Ramp/Bridge comparisons |
| Five scene process reloads | PASS;10 exact complete-save/map-PNG comparisons |
| Full existing Ramp/Bridge/city regression | PASS;11 Ramp checkpoint pairs,50 frozen old complete-save byte comparisons |
| Original corpus | 167 complete /165 prepared /2 unchanged conflicts;1485 policy1/2/3 actual/zero/one summaries unchanged |
| Original inputs | Complete pinned1464-file inventory unchanged; targeted asset/EXE/map hashes unchanged |

The relevant CTests include RampRendering, RampMaterialRendering,
AuthoredTerrainSides, RampTransport, BridgeTransport/Rendering, Ramp/Bridge
views and City-v16 geometry/software-view controls. The full existing runner
also exercises14 historical Xia saves and current rule4 Xia/Handan long-play.
Policy summary comparisons supplement the unchanged authority code and existing
contracts; they are not described as a new independent literal-byte policy proof.
Logs and machine summaries are in `validation/`.

| Required assessment | Result and scope |
|---|---|
| **A. Originalgrafik-Treue** | **PARTIAL**: original ramp identities/selectors are verified; current runtime uses original road material on authored geometry, with original ramp surface/anchor integration unresolved |
| **B. Technische Funktion** | **PASS**: the final checks above retain rule6/policy3/schema22, actual transport, saves and old controls |
| **C. Visuelle Darstellung** | **FAIL for the complete requested matrix**: the inspected descending paid1/3/4-Ramp scene passes the bounded material-artifact correction; two ascending orientations still render as an edge-on crest and the original cliff/surface composition is incomplete |

The requested original/natural ramp restoration therefore remains explicitly
**incomplete**. The measured geometry/evidence blocker is preserved rather than
moving authoritative feet or presenting green software tests as full acceptance.
No native input/human playtest or original-game capture was performed, and none
is inferred. No commit, push, tag, release or publication was made.
