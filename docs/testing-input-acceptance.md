# Native input acceptance

This is the central human input checklist. Direct renderer calls, queued SDL
events and computer-control requests do not mark its human rows PASS.
Use **NOT RUN / PASS / FAIL**, with the observed symptom for any failure.
Historical City balance, demolition and service playthroughs remain separate.

## Local candidate and isolation

Baseline revision: `3af52413bfef4cd479dbd2c04a3ac12aad37e5b7`.
The candidate contains the Native Input Reliability changes on that revision;
it is an uncommitted local test build, display version `0.1.0-alpha.2`.
The fixed local bundle is `.local/native-input/final-dist/OpenEmperor.app`.
Build metadata is in `Contents/Resources/BuildInfo.json`; package validation is
recorded in `.local/native-input/final-dist/package-report.json`.

From the repository, run the prepared ignored launcher:

```sh
.local/native-input/run-input-acceptance.command
```

It uses only `.local/native-input/human-settings` for settings, saves and
recovery, and the existing original data read-only. It starts a separate
process; do not interact with another OpenEmperor window. The launcher leaves
these test saves available for the restart step. Input diagnostics go to the
ignored log when the process exits, with a maximum of 512 retained records.
`human-input.log` contains stdout diagnostics; `human-platform.log` contains
platform stderr so asynchronous SDL messages cannot split JSON records.
Personal settings and saves are not used.

For another installation, invoke the bundle executable directly with your
own data root and a separate test root:

```sh
OPENEMPEROR_INPUT_DIAGNOSTICS=1 SDL_RENDER_DRIVER=metal \
  .local/native-input/final-dist/OpenEmperor.app/Contents/MacOS/OpenEmperor \
  --data /path/to/your/Emperor --app-root /path/to/separate/input-test-root
```

In that case select City-v16 and the prepared starter explicitly. Keep the
display version, City rule version and save schema distinct.
Enlarge the window if optional Advanced profile controls extend below its
current visible area; off-window clicks are rejected.

## Xia: about five to ten minutes

Use the normal menu, **City-v16, rule 2, Xia, prepared starter**. The starter
costs 1,280 of 1,300 funds. There are no free goods or test funds. Pause before
the first view checks, then repeat the camera/drag/focus checks at 1× and 4×.
If funds are insufficient for a paid road, allow ordinary supply and tax
collection; do not modify the save or treasury.

| Step | Action and expected result | Human result |
| --- | --- | --- |
| 1 | Use the mouse to open New Sandbox, choose Xia/City-v16/prepared starter and Start. Only the clicked button acts; the first city reports rule 2. | **NOT RUN** |
| 2 | Hold each arrow briefly, release it, and repeat. The camera moves in the expected direction only while held. Switch to another app while holding a direction; on return there is no sticky pan. | **NOT RUN** |
| 3 | Zoom in/out with the wheel over the map, pan, then return to the same paused view. Sprites remain visible and the selected world contents stay unchanged. Repeat at 1× and 4×; camera speed must not quadruple. | **NOT RUN** |
| 4 | Use Select to choose a building's ground footprint on the map, then another via the inspector list. Each inspector names the intended building; all four cells of a 2×2 footprint select its owner. Enable F1 and keep Select active for read-only visible-pixel inspection: an opaque roof/body selects the visible object, while a transparent hole admits its background. Building tools continue to target ground cells. | **NOT RUN** |
| 5 | Open/close the inspector and Help; resize smaller/larger while each is open. Buttons remain aligned with their artwork. Releasing an old press after resize must not activate a button. | **NOT RUN** |
| 6 | Select Road, start a map drag and cancel using Escape, right-click and focus loss in separate attempts. Also end over toolbar/inspector, outside the window, or after resize. No road, charge or partial batch may appear. | **NOT RUN** |
| 7 | With sufficient funds, build one short road on free ground. Its last cell matches the actual release position; the batch commits once. A press beginning on a toolbar button must not also build on the map. | **NOT RUN** |
| 8 | Select a working producer and toggle its permitted Running/Paused control. Only that building's operation changes. Restore Running afterward. | **NOT RUN** |
| 9 | Open an available budget or safe-demolition confirmation, then cancel. Resize or lose focus while a confirmation button is held. No automatic approval or map click-through occurs. If the paid city cannot legally open a particular dialog, mark that subcase NOT RUN. | **NOT RUN** |
| 10 | F5/save, fully quit this test process, rerun the same launcher, then load the test save from the normal menu. It opens paused with the saved funds/buildings/operations and accepts subsequent mouse input. | **NOT RUN** |

The initial 20-fund starter does not guarantee every dialog is available.
Automated dialog-boundary tests use independently authored synthetic fixtures;
those fixtures are not a normal paid game and do not fill missing human rows.

## Handan supplement

Return through the normal menu and start Handan with **Great Wall: Stone**
explicitly selected. Automatic remains the separate historical fallback.

| Step | Action and expected result | Human result |
| --- | --- | --- |
| H1 | Pause, enable F1 and the Select tool for read-only visual inspection, zoom at 1×/intermediate/2×/4× and pan around a visible wall base. Select the visible lower body; landscape actually in front still obscures it and wins the visible hit. | **NOT RUN** |
| H2 | Open/close the inspector, resize and change focus, then repeat selection and an aborted Road drag. UI blocks map input; the drag cannot commit after cancellation. Repeat view actions at 4×. | **NOT RUN** |

External-display movement is **NOT RUN** unless a suitable second display is
actually available. Synthetic 1:2 and fractional coordinate tests do not prove
a physical display switch. Finder/Gatekeeper and clean-Mac acceptance are also
separate from this checklist.

## Reporting a failure

Record the step, profile/rule, pause/speed, window size and expected/observed
target. A saved diagnostic event links raw window coordinates, conversion,
layout, camera, gesture and action. No text input or other window is recorded.
Distinguish wrong SDL delivery (**AUTOMATION LIMITATION**) from correctly
delivered input handled incorrectly (**INPUT BUG**). A technically passing
case without a human playthrough stays **HUMAN ACCEPTANCE PENDING**.

See [the input chain, measured defects and regression report](input-reliability.md).
