# Human input checklist

The current end-to-end human handoff is the
[OE50 checklist](qa/oe50-human-checklist.md), including direct CMake/app launch,
isolated save copies, paid Rule18 play and restart. Its human rows remain
**NOT_RUN**. The checks below retain the focused historical input contract.

Build the current source and launch an isolated test session:

```sh
./build/openemperor --data /path/to/emperor --app-root "$PWD/.local/input-test"
```

Choose City-v16, a supported map and a paid prepared starter. Record the source
revision, profile, rule version and save schema. The ordinary City-v16 default
is rule 4; historical saves retain their own rules. Use the same test root for
the restart check. Allow actual supply and tax income to fund construction;
do not edit funds, goods or development in the save.

Mark each observed result **NOT RUN / PASS / FAIL**. Queued SDL events, software
pixel tests and automation requests are separate technical evidence and do not
complete a human check.

| Check | Expected behavior | Human result |
| --- | --- | --- |
| Menu and start | Only the clicked control acts; the city has the selected rules and paid starting state. | **NOT RUN** |
| Camera and focus | Held arrows pan only while held. Switching apps while a key is held leaves no sticky movement. | **NOT RUN** |
| Zoom and resize | At 1×, 2× and 4×, sprites remain visible, camera speed stays consistent and controls follow their visible bounds. | **NOT RUN** |
| Building selection | Ground cells and inspector rows select their intended owner. All cells of a multi-cell footprint select that building. | **NOT RUN** |
| Visible-pixel inspection | In F1/Select, an opaque body selects the visible item; a transparent hole admits its background. Construction continues to pick ground cells. | **NOT RUN** |
| Road commit | A legal funded drag ends at the actual release cell and commits once. A press beginning on UI never also builds on the map. | **NOT RUN** |
| Road cancellation | Escape, right-click, focus loss, resize and release over UI or outside the window leave no partial road or charge. | **NOT RUN** |
| Help and confirmations | Help blocks map actions. Cancel, focus loss or resize during an available budget/demolition confirmation never approves or leaks a click. | **NOT RUN** |
| Inspector scrolling | At 1280×800, 1600×900 and 1920×1080, scroll a large list and long details/hints to both ends. Only fully visible rows select; fixed operations/priorities/demolition remain readable. Repeat 1×/2×/4× and resize during a pressed row. | **NOT RUN** |
| Operations | Permitted pause/resume and workforce priorities affect the selected building; retained goods, progress and active trips stay coherent. | **NOT RUN** |
| Save and restart | F5, quit the whole app, restart and load through the menu. The complete saved state opens paused and accepts further input. | **NOT RUN** |

Run unavailable dialogs later in a disposable funded city, or leave that subcase
NOT RUN. See [gameplay testing](testing.md#manual-play-and-persistence) for the
safe demolition and actual-supply checks.

For the optional Service view, use
`sh tools/test_city_v16_service_coverage.sh /path/to/emperor --ui` and load its
isolated baseline. Toggle V/button, inspect green/amber/red Houses, combine the
other overlays, and repeat Help, F3, zoom, focus and save/restart checks. C means
current expiry covers the next demand, ! expires by then and X is missing now;
future visits remain unknown. F3 keeps its diagnostic role cycling. The view
must change no World state and must start OFF in a fresh session. Human Service
acceptance remains **NOT RUN**; see [coverage semantics](city-v16-service-coverage.md).

On Handan, explicitly choose Great Wall: Stone and repeat zoom, visible-body
selection, UI blocking and drag cancellation around a wall. Automatic retains
its separate historical fallback. External-display switching, Finder/Gatekeeper
and an independent clean Mac require their own actual observations.

For failures, record the check, expected/actual target, profile/rules,
pause/speed and window size. Optional `OPENEMPEROR_INPUT_DIAGNOSTICS=1` retains
bounded application input diagnostics. Incorrect automation delivery is an
automation limitation; correctly delivered input handled incorrectly is an
application defect. See [the input contract](input-reliability.md).
