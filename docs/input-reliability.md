# Native input reliability

The Native Input Reliability pass starts at
`3af52413bfef4cd479dbd2c04a3ac12aad37e5b7`, with a clean worktree. It preserves
the current rules/defaults, World/save authority, road transaction semantics,
map selectors, composition, texture compatibility and assets. There is no
global input remapping, dependency upgrade or new input framework.

## Coordinate and delivery contract

| Boundary | Coordinates/state and responsibility |
| --- | --- |
| OS/window → SDL | Native pointer events carry fractional client-window coordinates and a window ID. Desktop positions are not these coordinates. |
| SDL → Application | `SDL_PollEvent` delivers the original event. Application routes it to the active menu/view; it does not globally convert it. |
| Menu → Sandbox | Playing forwards that same raw event to Sandbox. Other menu states perform their own button hit. |
| Window → render | Each view converts its position once using `SDL_RenderCoordinatesFromWindow`; button events use their own position, wheel uses `mouse_x/mouse_y`. Invalid/outside positions are rejected. |
| Menu render → menu UI | Menu divides the converted position by its own drawing scale for logical menu-button bounds. This is a UI convention, not a replacement DPI transform. |
| Sandbox render → UI/map | The output layout blocks UI regions. Ground picking uses the camera inverse and logical terrain geometry. The existing F1 read-only visual inspector uses the alpha/depth contract; ordinary selection/building tools retain their existing ground-cell behavior. |
| Gesture → action | A matching press/release or accepted road gesture reaches the existing command path. Camera/inspection/cancellation remain view-only actions. |

Sandbox rendering restores full viewport/scale before returning to event
handling and clips the map without moving its coordinate origin. No normal
double-conversion or reachable stale map viewport was demonstrated. Relative
mouse motion is not used to pan; held-key update reads actual SDL device state.

Official API contracts were checked against the installed SDL 3.4.14 headers
and [pinned SDL render source](https://github.com/libsdl-org/SDL/blob/release-3.4.14/src/render/SDL_render.c):
[window dimensions](https://wiki.libsdl.org/SDL3/SDL_GetWindowSize),
[pixel dimensions](https://wiki.libsdl.org/SDL3/SDL_GetWindowSizeInPixels),
[current render output](https://wiki.libsdl.org/SDL3/SDL_GetCurrentRenderOutputSize),
[pixel density](https://wiki.libsdl.org/SDL3/SDL_GetWindowPixelDensity),
[display scale](https://wiki.libsdl.org/SDL3/SDL_GetWindowDisplayScale),
[FromWindow](https://wiki.libsdl.org/SDL3/SDL_RenderCoordinatesFromWindow),
[ToWindow](https://wiki.libsdl.org/SDL3/SDL_RenderCoordinatesToWindow) and
[event conversion](https://wiki.libsdl.org/SDL3/SDL_ConvertEventToRenderCoordinates).
FromWindow incorporates logical presentation, user scale and viewport. The
pinned implementation uses the main-window view even while a texture target
is active; CurrentRenderOutputSize describes the current target/logical output.
Event conversion also changes relative motion; it is not added to the existing
per-field conversion. Clip rectangles alone do not transform a position.

## Demonstrated application defects

Independent baseline probes use exact HEAD sources, software/dummy SDL 3.4.14,
real SDL windows and the normal production handlers. They are synthetic input
evidence, not a human pointer playthrough. Sources and logs are ignored under
`.local/native-input/`.

| Baseline reproduction | Correction and regression |
| --- | --- |
| Menu Choose Folder Down → FocusLost → Up opens the chooser. Actual resize also leaves the press armed, including an old release now outside the window. | Window/focus boundaries disarm menu presses; invalid/outside positions do not hit buttons. |
| Budget confirm Down at (180,399), actual resize 1100×700 → 1290×780, PixelSizeChanged, old Up increments command sequence 5 → 6. | Window/layout processing precedes modal dispatch; old presses are cancelled and layout is refreshed before another input. |
| Help closes on an unmatched MouseUp over its close button. | Help requires its own matching Down/Up; closing stays consumed without map click-through. |
| NaN wheel over the inspector reaches an out-of-range float→int cast, confirmed by UBSan. | Nonfinite wheel amounts are rejected; large finite scrolling is bounded before narrowing. |
| MapDebug click on an opaque legend at (80,20) selects ground behind it; outside-right (160,60) also selects ground in a 160×120 fixture. Browser outside-X (10000,130) changes the selected row. | Map viewer UI and window/output bounds block map input; browser validates coordinates while retaining ordinary whole-row selection. |

Budget/demolition early returns alone were not a persistent layout failure:
render already called resize_camera. The defect was the interval before that
redraw, including a retained armed budget press. NaN map zoom did not reproduce
camera damage: the existing camera rejects invalid factors. No arbitrary DPI
multiplier, camera offset, footprint adjustment or map repair is introduced.

## Bounded observation

`OPENEMPEROR_INPUT_DIAGNOSTICS=1` allocates the optional event observer. Without
it, Application allocates no observer/buffer and does not query diagnostic
state. Enabled capture has a fixed 512-record ring, coalesces consecutive motion,
keeps button/focus/resize records separately, and reports overwrite counts.
It emits JSON lines once at shutdown, outside the frame loop. This pass does
not log text events, typed text, global hooks, other processes or telemetry.

Records contain event sequence/time/window ID, raw coordinates, observed render
position, actual window/pixel/current-output dimensions, viewport, scale,
logical presentation and target presence, renderer/driver, camera, UI/ground
hit, gesture, modal state and before/after action deltas. A menu request is
distinct from its later advance/transition. Diagnostic read-only conversion
observes the event; it never replaces or reconverts the delivered event.
Viewport/scale queries describe the active render target; `has_render_target`
must be considered when interpreting the main-window conversion contract.
The logged ground cell is a geometric candidate, including under a blocking
modal; the UI/modal flags say whether it may act. Actions are event-boundary
deltas and menu requests, not a complete frame/action journal. Held-key camera
movement occurs in update; later event snapshots expose the resulting camera.

## Acceptance boundaries

The [5–10 minute human checklist](testing-input-acceptance.md) is the single
current input acceptance entry. All human rows start NOT RUN. Historical
`(0,0)`/stale-coordinate delivery and `noWindowsAvailable` are automation
observations, not proof of a normal player's product defect. Native keyboard,
requested computer-control targets, queued SDL events and direct render tests
must remain separately reported.

Held-key tests use an explicit small key-state input; production still reads
SDL_GetKeyboardState. Queuing KeyDown does not alter the device state and is
not used as a held-key oracle. Focus/resize/modal transitions cancel gestures;
road preview remains pure and commit retains its existing transaction.

External display switching and clean-Mac acceptance require separate
hardware/user checks.

## Final technical validation

The same 269 source/CMake/resource files were hashed for all three runs and
remained unchanged through testing. Each run executed every one of its 101
registered tests, with no failures or skips. Builds were warning-free.

| Configuration | Actual environment | Full CTest result/time |
| --- | --- | --- |
| Debug | macOS arm64, system SDL 3.4.14 | **101/101 PASS**, 556.69 s |
| Release | macOS arm64, system SDL 3.4.14 | **101/101 PASS**, 66.99 s |
| ASan/UBSan | arm64 RelWithDebInfo; project address/undefined instrumentation | **101/101 PASS**, 207.07 s |

Sanitizers use `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; the external SDL dylib is
not instrumented. No sanitizer diagnostics were emitted. The full suites
include the new coordinate/diagnostic/menu/map/Sandbox tests plus existing
road responsiveness, texture-zoom pixels, Handan composition/alpha picking,
City profiles, determinism, save/load and recovery tests.

The independently calculated coordinate matrix covers actual software/dummy
1:1, nonzero viewport and fractional user scale, clip-neutral transformation,
letterboxing/outside coordinates, a current texture target versus the main
window, actual measured resize, and synthetic logical 2:1, 1.5:1.5 and
1.75:1.5 cases. Synthetic logical scales are not physical Mac displays.
Production gesture cases cover A–J, actual Down/Up positions, UI interception,
modal presses, focus, resize, all arrows/free A/W/S aliases, D reservation,
release, drag gating, and identical camera rate at paused/1×/4× updates.
Unpaused updates match directly ticked reference Worlds.

The existing alpha checks also passed: 100,000-tick endurance with equal
parallel Worlds and 12 save roundtrips; 3,000 production render frames with
source files unavailable and zero decode/upload/order-rebuild deltas; and 100
menu sessions with all owned texture counters zero after shutdown.
Input-only preview/view counter windows verify zero file/decode/upload,
World-copy/command, BFS and route work. Accepted construction and ordinary
simulation are measured separately. No performance improvement is claimed.
Disabled diagnostics add no observer allocation, growing buffer, state query
or output; this is a source audit, not a claim of literally zero CPU overhead.

An additional scope audit verifies 138 frozen simulation/persistence/map/asset/
renderer/scene/resource files and 55 original input files unchanged. The fixed
local bundle and ZIP passed the real packaging workflow: Release tests,
resources/licenses, dependency and ad-hoc-signature checks, relocated/unzipped
execution, original/synthetic smoke checks, save/restart/recovery and 13
negative package cases. Metadata records baseline revision `3af52413bfef`,
dirty workspace and display `0.1.0-alpha.2`; this is a local candidate, not a
release. Evidence is in `.local/native-input/final-validation-summary.json`
and `.local/native-input/final-dist/package-report.json`.

## Actual native probe and remaining human check

The probe uses a separately identified copy named **OpenEmperor Input Probe**
to avoid selecting the user's running OpenEmperor process. Only local bundle
name/ID/signatures differ; the executable and bundled SDL `__TEXT,__text`
hashes match the validated package. Its settings/save/recovery root is
`.local/native-input/native-probe-settings`; original data are read-only.
The own test process was closed after saving, with no user session operated.

The actual SDL backend is **Metal/Cocoa**, header/runtime **3.4.14**, revision
`SDL-release-3.4.14-0-g147a8ee32`. Observed window, pixel and output sizes are
all **1100×700**, density/display scale 1, full `[0,0,1100,700]` viewport,
scale `[1,1]`, logical presentation disabled and no active render target.

Two computer-control requests derived from fresh 1100×732 window screenshots
targeted the window center `[550,366]` and visible New Sandbox button
`[217,206]`. Both returned **`noWindowsAvailable`**, including after the
exposed AX Raise action. SDL received **zero button events**. These are
requested screenshot-space targets, not asserted SDL window-coordinate hits.
Pointer automation stopped; four interior edge probes, toolbar, inspector,
free cell, visible building and drag/focus/resize playthrough are **NOT RUN**.

Native keyboard requests are separate: N reached SDL and displayed City-v16
rule 2/Xia/prepared starter, although that tool call's screenshot stage failed
with ScreenCaptureKit `-3811`. A later screenshot verified the resulting menu.
Return loaded the ordinary paid starter; Space paused at tick 301/funds 20;
F5 saved only to the isolated root; Cmd-Q closed the own process cleanly.
The opt-in observer exported once: 22 events/22 records, zero overwrites,
menu and Sandbox states. The raw probe log merged stdout/stderr and one final
record was interleaved by asynchronous SDL destruction logging; it is retained
as-is and excluded from the 21 parsed JSON records. The human launcher sends
stdout diagnostics and platform stderr to separate ignored logs.

No actual held-arrow duration is claimed: the available native control API
offers a key press, not a held-device-state control. No current wrong-coordinate
SDL delivery was observed because the pointer requests failed before delivery.
Historical `(0,0)`/stale-coordinate logs remain historical evidence. There is
no demonstrated general coordinate-transform bug to repair.

**HUMAN ACCEPTANCE PENDING:** every human row in
[the executable checklist](testing-input-acceptance.md) remains **NOT RUN**.
The keyboard smoke and technical tests do not mark its mouse, held-arrow,
window, 4×, Handan or full save/restart rows PASS. External-display movement,
Finder/Gatekeeper and clean-Mac checks are also NOT RUN. No commit, push, tag,
release or publication was performed.

## Later user report: City-v10

Oliver confirms working zoom, arrow keys and window resizing: **User-reported PASS for the tested City-v10 session**. This report is separate from the historical Codex native probe above. It does not complete City-v16, road drag, selection of every building stage, demolition/Recovery, external-display changes or a full human playthrough. The Ruined Great Wall milestone makes no input behavior changes.
