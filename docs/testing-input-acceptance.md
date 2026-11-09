# Human input checklist

The current end-to-end human handoff is the
[OE50 checklist](qa/oe50-human-checklist.md), including direct CMake/app launch,
isolated save copies, paid Rule18 play and restart. Its human rows remain
**NOT_RUN**. The checks below retain the focused historical input contract.

**2026-11-09 follow-up:** the [OE58 human record](qa/oe58-human-playtest.md)
adds specific **USER_REPORTED_PASS** startup, paid construction, inspection and
resident/tent feedback. No screenshot file was supplied or newly opened. This
does not retroactively fill the historical rows below or establish all DPI,
Recovery, long-play, external-display or clean-Mac cases. New behavior needs
its own recorded native result; **AUTOMATED_PASS** stays separate from
**HUMAN_OBSERVED** and **USER_REPORTED_PASS**.

The same-date OE58 follow-up now records bounded **AUTOMATED_PASS** for the
paid Vacant2→Shelter3 transition, complete targeted F5/F9 equality, actual
opened House/Inspector images and explicit Loulan refusal/Empty controls.
The [result ledger](qa/oe58-human-playtest.md#oe58-result-ledger) keeps the
exact capture/test scope separate from the still-unrun native rows below.

The final local OE58 audit is **AUTOMATED_PASS**: full Release 320/320,
focused Debug 79/79 and RelWithDebInfo Address/Undefined 79/79, all with zero
errors/skips. Instrumentation covers 62 executables and 13 core archives;
system SDL/external dependencies are uninstrumented and macOS LeakSanitizer
is disabled (`detect_leaks=0`). This supplies no leak-check result.
All 152 complete historical JSON files/case indexes for schemas 1–39 compare
literally. Separate Rule18 creator/replay checks include seven full checkpoints
per phase, 93 complete top-level comparisons, four complete Recovery payloads
and two explicitly bounded Recovery-history metadata comparisons; optional
per-tick digests were not rerun. Paid Rule22/23 checks each use six separate
creator/restore processes, with 40/31 recorded complete comparison rows
(including one seed self-check each) and 18 further historical Rule23 documents.
The final 446-file PNG inventory has 262 actually opened paths, 443 capture copies
covered by reviewed SHA-identical content, three authored icon copies and no
unreviewed unique capture content; this is still-image evidence only.

The private source is `.local/oe58/final-technical-audit-01.json`, SHA-256
`a6959d80755159b35570c498fabf96fa127acb08b7f7053ce5cdb5dcf6ce5328`.
It binds the JUnit, full-file, source-integrity and image-review reports;
the [human record](qa/oe58-human-playtest.md#final-automated-regression--2026-11-09)
retains their limits. CI, new native delivery/human acceptance and clean-Mac
remain **NOT_RUN**; original parity remains **PARTIAL**. Earlier isolated
QA-setup failures remain preserved and are not relabeled PASS.

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
| Vacant-to-Shelter follow-up (OE58) | Buy a fifth Common House on Xia/Normal/Rule18. Record ID, real2×2 footprint, zero residents and visible authored Type2 plot; ordinary immigration later selects Type3/tent on the same ID. Check light/dark ground and1×/2×/4×, before/after F5/F9, selection and occlusion without an extra purchase or World grant. | **NOT RUN** for the new changes; earlier tent appearance is separately user-reported |
| Starter refusal follow-up (OE58) | Distinguish general map-rules admission from paid24×13 starter placement. Read the complete refusal on the chosen unsuitable map and explicitly choose Empty City or another map; do not silently switch maps or grant placement. Recheck Xia and retained settings/session. | **NOT RUN** for the new changes; earlier clipped Loulan message is separately user-reported |

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
