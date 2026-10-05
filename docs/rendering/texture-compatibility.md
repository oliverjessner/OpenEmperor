# Repeated zoom texture compatibility

Current input delivery and human pointer acceptance are tracked centrally in [Native Input Reliability](../input-reliability.md) and [the short executable checklist](../testing-input-acceptance.md). The historical native observations in this report remain distinct from current technical regressions.

This bounded presentation change addresses unchanged sprites disappearing after
repeated scaled/clipped copies on SDL's software renderer. It preserves pixels,
alpha, anchors, footprints, nearest sampling, projection and scene composition.
It does not establish original Emperor rendering parity or resolve general
interpenetrating-image composition, height normalization or restore context.

## Baseline and independent reproduction

The starting workspace was clean at
`3739fd2519f98fbfba05d1c7af8c51a5969498a9`. The installed SDL headers and runtime
are 3.4.14, runtime revision `SDL-release-3.4.14-0-g147a8ee32`, on macOS arm64.
Software runs explicitly request `software` and verify `SDL_GetRendererName`;
native GPU runs request and separately verify `metal`. No SDL installation or
build dependency revision is changed.

The previous composition pass independently reproduced STATIC texture failure
at `1 → 1.125 → 1` and repeated `1 → 2 → 4 → 1`. Its structural Combined-only
workaround is historical; this pass tests the other production paths separately.
Ignored diagnostics and original-data captures live under
`.local/zoom-stability/`, outside source, resources and the app package.

## Independent measured cases

The SDL-only diagnostic has no decoder, original data, map, World or painter.
It uploads each authored pattern once: opaque A, transparent border B, inner
hole C, half-alpha D and asymmetric E, at both small and larger sizes. It tests
all three requested zoom sequences, repeated frames, fractional camera origins,
four clipped edges, outside/reentry, multiple instances and `255 → 128 → 255`
alpha modulation. Across the original matrix and minimized cases there are
686 matched cases, 30,230 frames and 102,104 pixel witnesses:

| Access | Cases | Cases with wrong pixels | Failed SDL API calls |
| --- | ---: | ---: | ---: |
| STATIC | 343 | 123 | 0 |
| STREAMING | 343 | 0 | 0 |

A minimal fully opaque 23×31 A texture, RGBA32 (ABGR8888 on this little-endian
runtime), BLEND/NEAREST, crosses a clip edge during `1 → 1.125 → 1`, repeated
100 times. STATIC has 2,086 wrong witnesses and 199 changed 1× frame-hash comparisons;
STREAMING has zero wrong witnesses and all 200 1× comparisons match (including
the original baseline comparison and the initial 1× steps).
A separate 300-frame Present-only sequence also loses nine opaque witnesses
under STATIC; STREAMING's fresh terminal frame is correct. All five patterns
and a monochrome E reproduce at 23×31. Other sizes/conditions survive, so
neither alpha nor image color is a reliable exemption from the failure.

The diagnostic explicitly requests and verifies software/dummy, arm64 Debug,
800×600 window/output, full viewport and clip `(12,19,776,562)`. The loaded
library is the existing SDL 3.4.14 Homebrew dylib. Actual texture access, blend,
scale and format properties are recorded. The pixel-loss cases have successful
Render, ReadPixels, Present and explicit Flush calls; they do **not** establish
a new `Parameter 'src' is invalid` event. That error belongs to the prior,
separately documented composition-pass reproduction.

## Actual production-path audit

Baseline core sources were hashed against `git show 3739fd…:<path>` before the
negative diagnostic build. Initial logs inadvertently compiled already-fixed
sources and are retained only as fixed-policy calibration, not baseline proof.
The corrected baseline main-backbuffer runs reproduce ten of thirteen cases:

| Production path | Software baseline finding | Fixed policy |
| --- | --- | --- |
| Building shared normal instances | Reproduced | Stable |
| Prepared House stage 0 | Reproduced; negative case stops before stages 1/2 | Stable; all three textures reused |
| Walker large/small frames | Reproduced | Stable |
| Road line/corner/cross | Reproduced | Stable |
| Stored Combined, Base, Overlay | Reproduced | Stable |
| Building/Road preview followed by normal | Not reproduced for these inputs | Stable; same shared owner policy |
| Existing structural SpatialCombined | Not reproduced with its previous STREAMING protection | Stable; folded into shared policy |
| SceneRenderer: legal 23×31 Omega object over canonical Type-30 terrain | Reproduced: 77/81 wrong frames | 81/81 correct |
| TerrainPreviewRenderer: canonical 78×40 Type-30 diamond | Reproduced: 77/81 wrong frames | 81/81 correct |

The larger 318×238 SceneRenderer control survives. Both additional production
paths pass all 81 identical frames under actual Metal with STATIC. The final
reproducer loads its scene manifest through `load_scene` (canonical Type-30
default ground and a legal 23×31 Omega object) and its terrain manifest through
`load_terrain_bindings` (78×40 Type-30, side 1, 3,200-byte base/data, unmirrored,
no alpha block). Thus the reproduced inputs are reachable through normal
validation, not merely constructed renderer types. Earlier direct typed-input
Omega-ground diagnostics are retained but do not establish loader admission.
The fixed software textures report actual STREAMING; each has one upload and
zero live textures after shutdown. These are independently authored inputs,
not new asset identities or claims about original graphics registration.

The shared usage, including fully opaque failure, supports selecting access for
these eagerly prepared camera-scaled RGBA owners as a whole. Surviving preview
and size cases are not reliable exemptions or evidence that their shared
textures cannot fail under another clipped sequence. Creation in Application's
fitted image preview, AssetBrowser's thumbnail cache and MapDebugView's
on-demand BLEND_NONE diagnostic raster is **not checked** for this failure and
remains unchanged; no claim that those paths are universally unaffected is made.

## Compatibility contract

`src/renderer/TextureCompatibility.h` selects STREAMING only for the measured
combination: actual renderer name `software` and runtime version exactly
`SDL_VERSIONNUM(3,4,14)`. All other backends and versions retain STATIC. Header
version alone is not the decision. A future SDL version requires evidence before
any further compatibility exception; positive pixel tests do not require the
old bug to occur.

The policy applies at creation to unchanged eagerly prepared RGBA sprite
textures subsequently reused in camera-scaled/clipped copies. It does not depend
on map names, entity IDs, physical records, image colors or composition policy.
The former structural-wall decision is replaced by this one shared policy.
There is one existing eager `SDL_UpdateTexture` per created image/component,
with no frame uploads, locks, redecodes, recreations, extra textures or additional retained
CPU images. Existing deduplication, readiness/fallback, alpha caches and logical
RGBA budgets remain in force.

The access-mode A/B proves a compatibility effect in the measured runtime.
SDL's software implementation enables surface RLE for STATIC and changes blend
state around clipped scaled copies. RLE interaction is an explanation to
investigate, not a complete proof of SDL's internal cause. No upstream renderer
implementation is copied.

## Pixel oracle and acceptance boundary

Diagnostic readback occurs after drawing and before Present, as required by
SDL's main-target readback contract. Separate sequences present normally without
readback after each draw, then check a terminal frame before its Present.
Only a failed SDL call attributes a current error string to that call; a stale
`SDL_GetError` is not an error event. Successful submission alone is insufficient:
tests check opaque interior pixels, transparent holes, preview alpha and return
to identical paused camera/content. Fractional edge rasterization differences
are not treated as missing sprites or required to match Metal bit for bit.

The original composition regression remains independent: structural bodies draw
once at their established spatial key; genuinely foreground landscape still
wins; full-body alpha picking and F8 visibility remain unchanged. Automatic
material selection retains fallback without independently validated original
restore context. Ruined variant 40 remains a separate unresolved case.

Native pointer interaction and programmed SDL events are distinct evidence.
Unreliable pointer automation must not be reported as a player input bug or
fixed with arbitrary coordinate offsets. If native pointer delivery remains
unreliable, human acceptance is: load Xia and Handan with explicit Stone preview,
pause, repeat zoom/pan, select a building/wall, begin and cancel a road preview,
return to the same view, then repeat at 4× simulation. Technical render tests can
complete while that short pointer check remains open.

## Real scenes, resources and measured cost

A diagnostic runner uses actual 1280×720 native SDL windows with separately
verified software and Metal renderers. Xia runs City-v13 rule1's ordinary paid
1,280 starter through real ticks: stage 0 is selected at tick 0, stage 1 at 800,
stage 2 at 2400; the tick 8000 city has levels 1/1/1/2 and eight couriers. Captures
use the actual Sandbox stage selector, roads and mapped Clay/Pottery walkers.
Handan uses explicit Stone preview and the unchanged structural Combined stream.
A separately labeled direct BuildingSprite gallery checks all three curated
assets; its source-interior oracle admits only colors from a uniformly opaque
neighborhood so fractional nearest sampling is not mistaken for disappearance.
Software baseline has 23/27 missing witnesses per gallery stage; fixed software
and Metal have 0/27. Exact CPU-point differences remain recorded as raster
diagnostics.

| Backend / scene | Initialization ms | Actual textures/access | Logical RGBA bytes | Render+Present median/p95 ms |
| --- | ---: | --- | ---: | --- |
| Software / Xia | 200.82 | 1,168 STREAMING | 20,058,420 | 8.704 / 9.360 |
| Metal / Xia | 196.42 | 1,168 STATIC | 20,058,420 | 8.711 / 8.850 |
| Software / Handan Stone | 231.26 | 1,314 STREAMING | 61,713,120 | 8.183 / 9.093 |
| Metal / Handan Stone | 221.76 | 1,314 STATIC | 61,713,120 | 8.708 / 8.847 |

Xia consists of 1,092 stored component textures, 48 walker frames, 12 building
images and 16 road images. Each actual SDL texture property is logged by an
ignored creation hook: one eager upload, constant counts during zoom, zero live
textures after shutdown. The table's initialization includes map/profile
preparation; the 120-frame timing includes ordinary Present, with no readback or
PNG capture. These are local observations, not FPS requirements, backend-speed
comparisons or a claimed performance improvement. Diagnostic readbacks, PNG
writes and their timings are observer work recorded separately.

Enabled production counters during pure render calls report zero file reads/
writes, decodes, uploads, World copies/commands, BFS and route refreshes. Every
post-initialization draw region in all six owners is byte-identical to baseline.
The actual 4× scripted update advances 96 ordinary ticks and its World matches
the same direct authority ticks; render-only counters remain zero. Baseline and
fixed save bytes match before zoom and after 4× on both backends. Original files,
World, persistence, map readers/selectors, decoder and resources are unchanged.

## Durable regressions and native package

`texture-compatibility-sdl-pixels` uses ten authored small/large A–E cases with
one creation/upload each, 100 repeated fractional sequences and 100 normal
Present-only sequences, clipping/reentry, alpha and full return-frame hashes.
`texture-compatibility-production-pixels` calls the actual four required
owners with shared instances, every prepared House stage, walker frames, roads,
Combined/Base/Overlay/SpatialCombined, alpha picking, constant resource/upload
counts and shutdown. Backend/version assertions supplement these pixel tests.
Both executable matrices also pass separately with the optional `metal` argument
and a verified Cocoa/Metal renderer. They use the main backbuffer, inspect before
Present, and do not require the old STATIC bug on a corrected SDL runtime.

`texture-preview-failure-alpha` compiles the real Building/Road owner bodies
with a test-only draw-call interception: one preview submission returns false,
then the normal shared instance uses real SDL and must render at alpha 255. This
is fault injection, not evidence of a new failing SDL backend call. Upload,
modulation, restoration and destruction remain real SDL calls; no production
error hook is introduced.

The local dirty-workspace package is under `.local/zoom-stability/final-dist/`.
Its ordinary menu loaded Xia's paid City-v13 starter and paused through native
keyboard input. Stock SDL render logging reports `Created renderer: metal`;
this is verified backend evidence, not inference from `SDL_RENDER_DRIVER`.
The user subsequently took over and changed the city/window. Automated native
zoom/pan/selection/preview continuation stopped there to preserve that session.
The complete native pointer sequence remains open; this is distinct from the
passing scripted SDL and direct-render technical acceptance. No coordinate
correction or input-code change was made.

## Final validation (2026-10-05)

All three full configurations execute 96/96 registered tests with no failures
or skips, on the same 262 source/CMake/resource hashes:

| Configuration | Build | Full CTest wall time | Result |
| --- | --- | ---: | --- |
| Debug | arm64, system SDL 3.4.14 | 657.46 s | 96/96 PASS |
| Release | arm64, system SDL 3.4.14 | 76.02 s | 96/96 PASS |
| ASan/UBSan | arm64 RelWithDebInfo, address+undefined | 289.71 s | 96/96 PASS |

The sanitizer run uses the existing `detect_leaks=0:halt_on_error=1` and
`halt_on_error=1:print_stacktrace=1` settings; project targets are instrumented,
external SDL remains the installed library. Build and CTest logs contain no
compiler warnings or sanitizer diagnostics. Full tests include the preserved
scene-composition/foreground/F8/picking controls, road continuity/responsiveness,
save/resume, recovery and City-v12–v16 determinism/endurance cases.

Additional existing alpha drivers pass 100,000 ticks, 3,000 paused production
render frames with source files unavailable and 100 session lifecycle iterations,
with deterministic roundtrips and all owned textures released. Packaging reruns
all 96 Release tests, verifies metadata-only resources, recursive dependencies,
ad-hoc signatures, relocation/unzip, synthetic and original-data process restart
and save/resume, and 13 negative rejection cases. The bundle has no original
assets or personal histories. Native keyboard start/pause is separately recorded;
complete pointer acceptance and an independent clean-Mac test remain open.

The final read-only audit preserves 110 frozen files, 55 distinct original input
files and 157 authority/map/asset/app/profile/resource files. No commit, push,
tag, release or publication is made. Full ignored logs, JUnit output, source
hashes, stage captures and `final-validation-summary.json` retain inspectable
evidence under `.local/zoom-stability/`.

To rerun the focused registered regressions:

```sh
ctest --test-dir build-release --output-on-failure -R 'texture-compatibility|texture-preview-failure|scene-composition'
```

Optional native-backend pixel matrices:

```sh
build-release/openemperor-texture-compatibility-sdl-tests metal
build-release/openemperor-texture-compatibility-tests metal
```

These verify the actual Metal renderer explicitly. They are direct technical
tests, not the remaining native pointer playthrough.
