# Original vacant Common House presentation

A newly paid, never-admitted Original-production Common2×2 House uses the
original static1508 plot when the exact compatible source pack is available.
This choice follows the qualified original purchase path, rather than a visual
guess between a vacant plot and the Shelter tent. The visible
[OE58 authored foundation](oe58-vacant-house-visual.md) remains the fallback.
Rendering does not admit residents or change the paid House.

## Source and lifecycle evidence

The read-only PE32/i386 Emperor.exe has image base `0x400000`, 4,809,081 bytes and
SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The compatibility pack binds:

- `DATA/China_General.sg3`: `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`.
- `DATA/China_General.555`: `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`.

The construction caller `405378→4b1250` takes the special Type2 branch
`4b17ee→4b2680` with commit flag0. It creates literal Type2 through `42d540`,
requires a positive owner ID, explicitly resolves group`0x47a` through `408170`
at `4b28b4`, and publishes 2×2 through `4b72b0` at `4b28d0`. There is no variant
increment in that path. The complete qualified purchase path bypasses the
generic House virtual+100 callback and does not immediately run the ordinary
setter. Initialization alone does not publish a different picture.

The shared slot2 resource resolver selects physical SG3 record1508 for this
group. It is static, unmirrored Type30, side2, 158×88, with 12800 base bytes,
479 Omega bytes and native front-cell anchor79,68. Decoding retains the source
alpha and complete image; there is no crop, optical offset or copied pixel asset
in Git. These resource facts support the proved selector; appearance alone does
not establish lifecycle semantics.

The decoded1508 raster has6506 opaque and7398 transparent pixels, using only
alpha255/0; there is no separate alpha payload. Its additional RGBA allocation
is exactly55,616 bytes. The158×83 and158×87 tent alternatives have different
Omega lengths and anchors, so neither their dimensions nor their pivots are
substituted for the fresh plot.

Ordinary setter `518de0` instead aliases Common types2 and3 to group`0x471`,
physical1509/1510 with the original cell variation low bit. Generic callback
`519f30→519060` and a qualified former-resident removal path are different
contexts from the new purchase. They do not justify applying the fresh1508
choice to every empty House. The [graphics contract](../reverse/original-house-visuals.md)
and [lifecycle contract](../reverse/original-house-lifecycle-contract.md) retain
the complete bounded distinctions.

Original-save import is unsupported. Static load research qualifies the House
post-load callback but does not close the later House image publisher after
saved image IDs are cleared. Original House restore/refresh parity remains
**PARTIAL**. Current OpenEmperor JSON restore is a separate supported path.
Original execution, all original callbacks and per-instance variation are not
claimed by the static purchase proof.

## Runtime and authority boundary

The native selector requires active Original production, an actual paid
Common `Household` with side2, a complete empty Vacant2 House state, and its
existing persisted housing row with no `first_admission_tick`. Current World
validation already enforces this relationship. Rendering reads that authority;
it does not add a “fresh” flag, rewrite history or infer it from empty stock,
an image ID or occupancy alone. A fresh Common placement preview uses the same
purchase image without creating a hypothetical World body. An actual first
immigration changes the same paid BuildingId from Vacant2 to Shelter3 and selects
the existing native1509 representative.

Missing or incompatible original files, unsupported lifecycle/footprint,
unavailable optional metadata and disabled source artwork retain the visible
authored foundation and an explicit inspector fallback reason. F4 provides a
direct native/foundation comparison. The foundation uses the same front-cell
projection and signed height, stays inside the complete2×2 footprint and
retains its own visible-pixel hit geometry. Empty Elite plots and all other
House stages keep their existing footprint and selector rules.

The metadata adds one eagerly prepared physical image to the shared building
pool: 12 legacy images plus 16 Original House images, 28 distinct source images.
The existing combined 64MiB budget and atomic source/metadata checks stay in
force. Source matching, resolve/decode/upload, bounds and alpha-hit preparation
remain outside frame and hover work. Native art uses the ordinary cached raster,
alpha picking, signed-height projection, painter order, clipping and preview
compositing paths. No frame IO, decode, upload, World copy, command, tick, BFS or
route refresh is introduced.

The bundle verifier also requires the exact current Type2 registration. Its
negative checks reject the ordinary setter group, tent records, unsupported
variation and altered geometry. Historical fallback-only runtime profiles
remain supported by the existing loader; they cannot silently replace the
native registration in the shipped compatibility metadata.

No simulation or persistence source changes are required. World, HouseType,
immigration cadence, occupancy, needs, goods/services, funds, costs, workforce,
dispatch and fire retain their existing authority. Rules1–23, schemas1–39,
defaults and historical restore contracts do not migrate.

## Reproduction and evidence categories

With user-supplied compatible original data configured through
`OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT`, the production-view regression exercises
the ordinary Xia/Normal Rule18 four-House starter, buys a real fifth House via
the shared map-click path, and observes actual immigration. It checks the paid
80-Funds purchase, complete owned 2×2 footprint, BuildingId, construction totals,
complete World equality around presentation, and real persisted F5/F9 bytes.
No residents, timers, trips or House types are injected into that paid sequence.
Its adversarial case attempts to relabel an admitted House as fresh Vacant2 and
requires atomic rejection. These are current JSON checks, not original-save
import tests.

The capture matrix covers 1280×800, 1600×900 and 1920×1080 at 1x/2x/4x for the real
starter, fifth Vacant House and admitted Shelter. Separate normal/hover frames,
translucent preview compositing, F4 foundation comparison and clearly authored
light/dark grass fixtures keep those distinct presentation cases visible.
Existing stage, foreground, signed-height, culling, picking and Inspector checks
retain their separate scope. The focused CTest cases are
`original-house-visual-profile`, `original-house-visual-profile-real`,
`original-house-inspector-view` and `original-house-visual-production-view`.
Use a fresh ignored output directory when requesting captures:

```sh
OPENEMPEROR_UI_CAPTURE_DIR="$PWD/.local/oe59/house-captures-01" \
  ctest --test-dir build --output-on-failure -R '^original-house-visual-production-view$'
```

The [testing guide](../testing.md) defines the original-data setup and broader
regression checks. Static source evidence, original-asset decode, authored
fixtures, scripted software SDL, native delivery and human acceptance are separate
categories. Original source bytes, disassembly, PNGs and proof JSON stay under
ignored `.local/oe59/`; no original pixels or private helper tables are shipped.
This finite presentation scope establishes neither general original scene
composition nor an original runtime/native-human playthrough.

## Validation

Isolated macOS arm64 builds validate the presentation change from OE58 baseline
`3c4d5b7b3b5e63958480a8bd540561d040fdfb6d`. The final registered CTest runs used
the compatible original-data root and completed without skips:

| Configuration | Passed / executed | Test time |
| --- | ---: | ---: |
| Release, complete suite | 320 / 320 | 405.99 s |
| Debug, focused selection | 86 / 86 | 1184.88 s |
| ASan/UBSan, same focused selection | 86 / 86 | 561.43 s |

Sanitizer runs halt on ASan/UBSan findings; macOS LeakSanitizer is disabled.
The selection includes the real House profile, production View, Inspector,
evolution, lifecycle, menu/input, original model/source, save/recovery and
Rule22/23 checks. Native binaries and runtime source inputs remained unchanged
during each run. All 80 simulation/persistence source files remain byte-identical
to the baseline; this presentation change introduces no rule or save migration.

Two separate current-build processes restore the actual Release-earned fifth
Vacant and Shelter saves. Each completes all nine window/zoom cases with native
1508 or 1509 respectively, unchanged complete World and F5/F9 bytes, and all
twelve measured work counters at zero. No ticks, construction, resident or type
injection are used in those restart checks. Observed fractional camera zooms
are retained; nominal 1x/2x/4x labels are checked within the existing 1e-9 wheel
tolerance rather than rewriting capture metadata to exact integers.

The final historical audit compares all 152 complete JSON files literally with
their protected baseline references across schemas 1–39: 123 full SaveDocuments,
25 full DTO/restore controls and four case indices. The 20 newly generated
schema 35–39 documents also have independent creator/restore comparisons at
0/+103 ticks.

The Rule18 Golden paid-city creator and independent replay pass 99 checks:
93 literal comparisons, four complete recovery payloads and two explicitly
bounded history-metadata checks, including seven complete checkpoint pairs.
Optional per-tick digests were disabled. Rule22 and Rule23 each pass six
separate creator/restore processes at 0/+103/+20,400 ticks. Their 40/31 complete
comparison rows each include one seed self-control; the remaining 39/30 rows
compare distinct files. Original inputs, historical references, source
bindings and native artifacts remain unchanged through the guarded runs and
the final live audit.

## Measured preparation and rendering

One local paired software-SDL run restores the same paid Rule18/schema34
five-House save in the baseline and current renderer. Each binary renders
20 warm frames and 120 measured frames per case: 180 warm and 1080 measured frames
in total. Camera zoom, offset, map aperture and paid World are paired. Timing
covers `SandboxView::render`, including present; motion and update0 are outside
the render clock, while all twelve work counters cover the complete batch.
PNG readback happens after measurement.

Full session preparation takes 410.191 ms before and 484.833 ms after. This includes
source/map preflight, profile selection, initialization and restore; it is not
an isolated sprite-decode measurement. The deduplicated building pool grows
from 36 to 37 textures and from 4,165,352 to 4,220,968 RGBA bytes, an exact
55,616-byte increase within the unchanged 67,108,864-byte limit.

| Window | Camera zoom | Before median / p95 (ms) | After median / p95 (ms) |
| --- | ---: | ---: | ---: |
| 1280×800 | 1x | 1.778 / 1.955 | 1.841 / 3.133 |
| 1280×800 | 2x | 5.436 / 5.644 | 5.931 / 9.191 |
| 1280×800 | 4x | 7.964 / 8.152 | 8.849 / 13.610 |
| 1600×900 | 1x | 2.316 / 2.611 | 2.389 / 3.360 |
| 1600×900 | 2x | 7.950 / 13.865 | 8.089 / 8.286 |
| 1600×900 | 4x | 10.098 / 10.597 | 10.803 / 12.210 |
| 1920×1080 | 1x | 3.250 / 3.310 | 3.382 / 4.265 |
| 1920×1080 | 2x | 12.894 / 13.426 | 13.548 / 17.842 |
| 1920×1080 | 4x | 14.392 / 15.187 | 14.603 / 16.522 |

Every final median is higher in this single pair; the p95 values also show
machine variance. This is a finite warm-cache observation, not a statistical
speed claim or universal frame-time guarantee. All twelve counters remain zero
in both measured batches. Complete World and F5/F9 bytes agree between versions;
prepared texture and navigation/preview cache ownership remain unchanged within
each run.

All six paired 1280×800 before/after PNGs at 1x/2x/4x were individually opened.
They show the foundation replaced by the original cleared plot, sign and posts
inside the same complete four-cell selection. Roads, neighboring buildings,
zero residents and paid-state header agree. The foreground kiln partly covers
the front tip in both views; this bounded painter observation does not resolve
general original interpenetration. The private pairs live under
`.local/oe59/performance-runtime-02/{baseline,final}/`.

The recursive private image inventory covers 773 PNGs and four BMPs: 777 files
with 295 distinct file hashes and no unreviewed capture contents. New distinct
contents were opened; byte-identical duplicates reuse explicitly SHA-bound
still-image reviews, including protected prior observations. The four BMPs were
reviewed through source-preserving PNG format conversions or exact BMP-source
hash matches, rather than claimed direct BMP openings. Three authored static
icon copies are classified separately from rendered frames. Existing long
legacy-menu footers clip at some narrow sizes; primary controls remain readable,
and no general UI or whole-scene acceptance follows from this finite review.
Native input delivery, a human playtest and clean-machine acceptance remain
**NOT_RUN**.
