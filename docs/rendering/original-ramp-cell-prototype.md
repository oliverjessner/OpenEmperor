# Original ramp sprites: playable source-cell prototype

The separate [OE53 authored source-cell experiment](oe53-terrain-integration.md)
uses explicit Rule20/policy3/schema36. It retains historical RampLink/Bridge
contracts and keeps native233–244 prototype art and raw-ramp permissions separate.

This isolated SDL experiment draws original Elevation records **233–244**
directly. It has its own small terrain, roads, editing tools and walking figure.
It does not use the procedural RampRenderer or start a production city. The
result supports the source-cell interpretation, but does not justify replacing
the production renderer yet.

## Run and play

From the repository root, with the locally extracted original data:

```sh
sh tools/test_original_ramp_prototype.sh .local/gog-extracted/app
```

The launcher builds the excluded Release target, runs its cell-model test,
checks the complete original-file inventory before/after the session and opens
the prototype. Original data is read-only. The initial figure repeatedly walks
between the low and high roads. The prototype has no persistence or city economy.

| Control | Action |
|---|---|
| 1 / 2 / 3 | Earth / stairs / cobbled original sprites |
| O | Cycle the higher cardinal neighbor |
| + / −, including keypad | One through four adjacent source-cell ramps |
| [ / ] | Change the signed low plateau height, −2 through +2 |
| Z or wheel | Native integer zoom 1x / 2x / 4x |
| Space / . / L | Pause / short step / automatic return trips |
| P / B / X / M | Build ramp / road / erase / place figure |
| Left / right click | Apply selected tool / choose a road destination |
| F1 / F2 | Cell grid / diagnostic foot marker |
| F4 | Compare the experimental global Base/Overlay painter |
| Arrows / R / Escape | Pan / center / close |

Style, orientation, width and height presets rebuild the small authored scene.
Ramp placement requires the selected low cell's bounded one-level neighborhood.
Use M to place the figure before erasing a ramp or road while a trip is active.
A right-click destination during a trip is queued until that trip ends.
The native keyboard smoke used keypad minus successfully; automation of letter
keys must account for the desktop's German QWERTZ layout.

For a fresh deterministic image matrix and independent pixel checks:

```sh
sh tools/test_original_ramp_prototype.sh .local/gog-extracted/app --capture
```

Results go to a new ignored `.local/original-ramp-prototype/capture-*` directory.
No original pixels, disassembly or extracted binary tables are tracked or bundled.

## What the height investigation changes

The original ordinary ramp belongs to **one low saved-height source cell**.
Its high cardinal neighbor is one signed height level above it. The sprite is
anchored at the low cell; the walking surface rises across that source cell,
starting and ending at its edges. This differs from the production RampLink's
surface between two adjacent road-cell centers.

Read-only examination of the pinned EXE
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`
establishes the following bounded facts:

- Signed cell height is projected at 40 pixels per level (`471e20..471e36`,
  also the normal ground pass at `4700f3..470103`).
- Ordinary ramp movement initializes a separate 20-update offset toward
  0 or 40 pixels (`4e7c2f..4e7cbb`). The ordinary update changes that offset
  by two pixels; the half-slope branch is separate (`4e93ea..4e9459`).
- Cell ownership changes halfway through a center-to-center movement step
  (`4e7f52..4e7f62`, `4e9100..4e929f`). The renderer subtracts the extra
  actor offset separately (`46bbe7..46bc1a`).
- The width-78 Type30 image retains anchor **(39, image height − 20)** relative
  to the low cell's projected center (`5ce9e0..5cea0a`). Its image height is
  artwork extent, not a replacement for terrain height.

The isolated continuous central-path model derived from those facts is:

```text
D = unit cardinal direction toward the higher neighbor
s = position along D inside the source cell, from −0.5 to +0.5
H(s) = low saved height + 0.5 + s
screen = ((x−y)×40, (x+y)×20 − H×40)
```

Consequently the source-cell center is at `low + 0.5`, and the two edges meet
the low and high plateaus exactly. The image anchor continues to use `low`.
Foot placement uses the continuous surface; bitmap selection uses cached native
alpha. Two surface orientations are edge-on in this projection and have no
unique analytical inverse. Their visible original banks remain selectable.

The original offset targets, update count and cell ownership are evidenced;
the prototype's continuous interpolation and full two-dimensional plane are
derived choices. Exact original discrete phase endpoints, rounding and every
actor class have not been emulated. The possible one-update boundary difference
is not presented as pixel-exact original movement.

## Native assets and bounded scene

The existing RuntimeArchiveLayout and ResourceGroupLookup resolve slot16 keys;
the program verifies the expected physical identities before rendering.

| Higher neighbor | Earth | Stairs | Cobbled | Earth key |
|---|---:|---:|---:|---|
| South | 233 | 237 | 241 | 0x204e |
| West | 234 | 238 | 242 | 0x204f |
| North | 235 | 239 | 243 | 0x2050 |
| East | 236 | 240 | 244 | 0x2051 |

The ordinary selector's road branches add four or eight to the group. The
complete original helper/global context choosing stairs versus cobbles remains
unresolved, so material selection here is an explicit diagnostic control.

A fixed rectangular one-level plateau uses original cliff/corner records
221–228. Separate offline table checks establish those selections for this
rectangle and the four straight ramp boundaries. The small `cliff_for()` helper
is sufficient for that immutable height layout; it is **not** a recovered
general cliff selector. It must not be extended to arbitrary terrain by choosing
the first higher neighbor. Ramp edits conservatively enforce the evidenced
high-facing diagonals and an extra equal-height low join. Traversal through a
ramp is limited to its low/high axis because its native side banks remain.
The private bounded route search is unrelated to production World navigation.

Surrounding grass is a labelled curated Terrain preview; ordinary road masks
and the Clay figure reuse the existing compatible visual profiles. Images and
Type30 components are prepared within a combined 64 MiB CPU RGBA limit; their
textures upload eagerly. Draw order and alpha-picking cells rebuild on edits,
not frames.
Original ramp images use nearest integer scaling without deformation, rotation,
mirroring, recoloring, RGB classification or procedural tread geometry.

## Actual visual judgment

The final software run is
`.local/original-ramp-prototype/capture-gk1ynwmp/`. Its `review/` montages,
unmarked walker captures and F4 alternatives were opened and inspected. A native
macOS window was also opened and inspected at 1x, 2x and 4x, with material,
orientation, width, zoom and pause controls delivered through native input.
This is agent-operated native evidence, not human play acceptance or an original
game capture. Native mouse delivery remains unconfirmed; the pointer-input PASS
below belongs to scripted delivery through the SDL event handler.

West/North ramps show the native earth, stone treads, rock banks and lower
support coherently. Multiple neighboring images retain their internal native
side banks: they form parallel narrow ramps, not a verified seamless wide ramp.
East/South retain a narrow rear crest; figures are visible there, but foreground
pixels partly cover their lower boot contact. An F2 cross drawn after the scene
cannot prove unobscured boots.

The global Base/Overlay experiment was actually tried and rejected as the
default: native grass/cap overlays cut across upper road pixels. F4 retains
that comparison. The selected whole-image-per-cell painter is visually better
for this bounded scene; it still simplifies the original conditional
Base/Overlay/actor passes. Full original scene composition remains open.

| Check | Recorded result |
|---|---|
| Cell model, Release / Debug / ASan+UBSan | PASS, including 160 signed round-trip cases |
| Existing Ramp/Bridge rendering, transport and view controls | 8/8 Debug CTests PASS |
| Complete scene matrix | 432: both axes, both signs, heights −2/0/+2, widths 1–4, three materials, 1x/2x/4x |
| Independent original sprite comparison | 36 stamps; 1,536,444 exact pixels and 559,755 transparent exterior pixels |
| Scene coverage | 5,400 independently projected central-path points unchanged between two background colors |
| Walker checks | 36 unmarked movement captures; independent height/projection checks; separate F2 probes |
| Scripted SDL input | 10 checks through the interactive event handler, including erase/build, route and arrival |
| Prepared-frame purity | 100 frames; all nine prohibited preparation/navigation/World counters zero |
| Original input preservation | Complete 1,464-file inventory unchanged |
| Visual / production decision | PARTIAL; retain isolated prototype, no production adoption |

Coverage proves opaque scene pixels at those central witnesses, not attribution
to a particular sprite or coverage of a complete lateral surface. Pixel-exact
native stamps, model continuity, software input, native observation and human
acceptance are deliberately separate results. ASan/UBSan also completed the
full 432-scene capture without diagnostics. Reports stay in the ignored output.

## Consequences for production integration

A future integration needs one shared cell-surface description for image anchor,
figure feet, route progress, picking and foreground composition. Copying the
native sprite onto today's RampLink quad cannot supply that description. In
particular, changing the production low endpoint to `low + 0.5` would silently
change existing saved path positions and their authority.

Before adoption, resolve the rear-crest/actor painter and the intended behavior
of neighboring native ramps; establish original first-draw/normalization and
road-style selection separately. Then choose an explicit versioned source-cell
contract or a bounded presentation adapter with compatible saved-progress
semantics, and test it against existing links and controls. The prototype does
not relax original raw-terrain admission or infer transport permissions from
sprite IDs. Rule6/policy3/schema22, original files, Bridge geometry, normal roads,
production saves and the production renderer remain unchanged.

Implementation: `tools/OriginalRampPrototype.cpp`,
`tools/original-ramp/CellTerrain.{h,cpp}` and
`tests/OriginalRampCellTerrainTests.cpp`. Reproducible local research and static
evidence are in `.local/original-ramp-prototype/research/` and `research.md`.
The [earlier asset audit](original-ramp-asset-audit.md) records the prior
center-to-center mismatch and remains the historical production result.
