# OE49: explicit player entry and historical menu contracts

The main menu presents **Play Original economy** as the recommended experimental
new-city path. It explicitly selects the existing Rule18/schema34/policy3
contract. **Advanced / Legacy** retains the historical profile and experiment
choices. This is menu presentation and selection work: there is no new World
rule, save schema, paid authority or migration.

## Identity and acceptance status

| Item | Recorded value |
| --- | --- |
| Starting `main` / branch base | `1995eb1a93dba862008423be8892ffa5533b05db` |
| Implementation branch | `feat/oe49-player-entry` |
| Production implementation commit | `156fddf9da8f4284da10a2e587a4f63730e913e5` |
| Final tested source commit | `309fe5809a1024a6cb841729341094e9ef91233e` — test-only signedness correction after the production commit |
| Menu technical acceptance | **MENU_PASS** — automated menu, real paid entry, restart and scripted software-SDL review |
| Historical World/save acceptance | **HISTORY_PASS** — 132 historical files, complete golden checkpoint/Recovery equality and protected inputs verified |
| Native human acceptance | **HUMAN_NOT_RUN** |
| Original-game parity | **PARTIAL**, unchanged |
| GitHub PR / merge / verified new `main` | The PR publication record carries the actual remote identities after publication; this technical report precedes its merge. |

New private outputs belong under ignored `.local/oe49/`; original game files,
original saves, captures and private evidence are excluded from Git. The source
installation remains read-only. Completed redundant builds/captures may be
removed after their required evidence is recorded; supplied inputs and earlier
protected evidence are not disposable outputs.

## Menu selection contract

The baseline main menu exposed every economy experiment alongside New Sandbox,
Load Save, data-folder selection, autosave and Quit. Continue was conditional on
a retained in-memory session. The new main menu separates the recommended new
city from loading an existing save and from Advanced / Legacy. Continue keeps
its retained-session condition; an empty save catalog does not imply a city.

| Entry / shortcut | Existing selection retained |
| --- | --- |
| Play Original economy | Explicit Rule18/schema34/policy3 new-city setup |
| Advanced / Legacy → N New Sandbox; main-menu N | Stored general profile/starter; fresh City-v11 rule3; ordinary City-v16 rule4 |
| O Original economy | Rule10/schema26 |
| F Original fire protection | Rule11/schema27 |
| S Sustainable economy | Rule12/schema28 |
| M Needs-aware logistics | Rule13/schema29 |
| W Cardinal-water economy | Rule14/schema30 |
| I Inspector coordination | Rule15/schema31 |
| D Deadline-aware fire dispatch | Rule16/schema32 |
| T Deadline-aware services | Rule17/schema33 |
| U Deadline-aware food | Rule18/schema34, identical to the recommendation |
| Ordinary City-v16 Bridge / Ramp options | Explicit rule5/schema21 or rule6/schema22; no default change |
| L Load Save | Existing manual/recovery/external OpenEmperor save; exact saved generation |
| Enter Continue | Existing in-memory World, camera and pause/speed state |

O/F/S/M/W/I/D/T/U/N remain main-menu shortcuts even when Advanced / Legacy is
closed. D changes difficulty only in original-economy new-city setup; P toggles
paid starter versus empty there. Up/Down and wheel retain map/save navigation;
Return starts or opens the selected item, and Escape/Back retains the existing
navigation/cancellation behavior. Clicks and keys use the same menu actions.

The original-economy setup shows the data root, selected map, mode, difficulty,
starter choice, map-check state and experimental limitation before Start.
Missing/unreadable data stays in data setup with a concrete diagnostic. Start
is disabled while map rules are unchecked, checking, unsupported or invalid;
successful preflight does not waive model, graphics or paid-placement checks.
Failure leaves an existing city and its source bytes intact.

The general selector retains its historical profiles, map browser, prepared
starter, explicit Bridge/Ramp controls and advanced visual overrides. Load Save
retains manual/recovery browsing, external-file selection, Great Wall preview
choice and session-only visual overrides. Saved profile/rule/schema are
diagnostic values, not a request to reinterpret the saved World. The two existing
explicit City-v11 copy upgrades remain separate historical operations;
original-economy saves gain no upgrade option. F5/F9, Recovery and
`--original-deadline-food-dispatch` retain their existing contracts.

## Regression and evidence requirements

Results below distinguish complete suites, focused checks, paid sessions,
literal-byte comparisons and opened captures. No historical PASS substitutes
for a current result.

| Evidence layer | Required observation | Current result |
| --- | --- | --- |
| Release | Complete registered CTest suite, unchanged assertions/timeouts | **292/292 PASS**,183.97s; zero failures/skips/disabled; final affected2/2 PASS,2.13s |
| Debug | Affected menu/input and rule-gate checks | **10/10 PASS**,234.44s; final affected2/2 PASS,14.92s |
| ASan/UBSan | Same focused menu/input/rule-gate checks | **10/10 PASS**,97.60s; no sanitizer diagnostics |
| Scripted SDL menu | Recommendation, all historical decisions, actual click/key paths and focus/resize/modal cancellation | **PASS**, including the final capture run and preflight refusal/disabled-hit checks |
| Paid new sessions | Rule18 paid starter/funds/source/map/schema, plus City-v11 rule3 and ordinary City-v16 rule4 controls | **PASS**, actual Rule18 entry/F5/F9 plus automated historical-default controls |
| Historical persistence | Historical profile/save contracts, complete canonical bytes, negative markers/truncation and atomic refusal | **PASS**, full Release checks plus132-file literal comparison across schemas1–34 |
| Separate process | Save, application restart, independent restore and unchanged generation | **PASS** for the explicit recommended Rule18 session; historical corpus remains separate |
| Golden Rule18 World | Exact OE48 paid plan from baseline and final engine, complete authority equality | **PASS**, seven complete checkpoint documents and four Recovery payloads; independent creator/restore and measured outcomes |
| Visual review | Open baseline and final software-SDL captures; labels/clipping at 1280×800, 1600×900 and 1920×1080 | **SCRIPTED_SDL PASS** for the captured action/setup labels; disabled semantics and hitboxes require separate event tests |
| Native human | Actual observed pointer/keyboard/gameplay acceptance | **HUMAN_NOT_RUN** |

Build/test procedures remain in [testing](../testing.md); input evidence follows
the [native-input contract](../input-reliability.md). The environment is macOS
arm64, AppleClang21.0.0 and SDL3.4.14. Sanitizers use
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and halt-on-error UBSan; the external
SDL library is not instrumented. The signedness correction changes only a test
budget comparison; the two affected menu/restart checks were rebuilt and rerun
in Release and Debug after that correction. No assertion or timeout was relaxed.

The focused ten are `menu-session-flow`, `original-player-mode-menu`,
`original-menu-session-flow`, `original-menu-successor-flow`,
`original-deadline-service-menu`, `original-deadline-food-menu`,
`menu-map-rules-selection`, `menu-input-boundaries`, `original-economy-rule-gate`
and `original-player-mode-restart`. Final JUnit files are
`release-junit.xml`, `release-final-junit.xml`, `debug-final-junit.xml` and
`sanitize-junit.xml` under the implementation worktree's `.local/oe49/`;
the ten-test Debug JUnit is `.local/debug-junit.xml`. Full command output is in
the outer `.local/oe49/ctest-{release,debug,sanitize}.log` and
`ctest-{release,debug}-final.log`. Scripted SDL event delivery does not establish
native pointer delivery or human acceptance.

For reproduction, run from the source checkout with an explicit read-only
installation and unused output directories. For example:

```sh
cmake -S . -B .local/oe49/repro-release -DCMAKE_BUILD_TYPE=Release -DOPENEMPEROR_USE_SYSTEM_SDL3=ON -DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=/path/to/emperor
cmake --build .local/oe49/repro-release --parallel 2 --target all openemperor-elite-desirability-acceptance
ctest --test-dir .local/oe49/repro-release --output-on-failure
.local/oe49/repro-release/openemperor-original-menu-session-tests --player-mode-menu
python3 -B tests/test_player_mode_restart.py .local/oe49/repro-release/openemperor-original-menu-session-tests
```

The real-menu diagnostic accepts `--player-mode-real DATA FRESH_OUTPUT`, with
output strictly below the checkout's ignored `.local/oe49/`. The golden creator
and independent resume use the [existing dispatch launcher](../testing.md#rule16-rule17-and-rule18-dispatch-experiments),
explicit `--rule 18 --diagnosis oe48 --variant lean-med --paid-strategy
oe48-watch-phase1-third-food-three-markets`, and separate fresh outputs below
`.local/original-economy/oe48/`. Resume takes the creator's `seed.save.json` and
`--expected result.save.json`. The frozen historical exporters and exact input
manifest remain private beside this run; no baseline is regenerated in place.

### Baseline software-SDL capture

The immutable source snapshot of the starting commit was built separately in
`.local/oe49/baseline-build/`, without changing source or CMake options:

```sh
cmake --build /Users/oli/github/OpenEmperor/.local/oe49/baseline-build --parallel 1 --target openemperor-original-menu-session-tests
/Users/oli/github/OpenEmperor/.local/oe49/baseline-build/openemperor-original-menu-session-tests --capture /Users/oli/github/OpenEmperor/.local/oe49/baseline-golden/menu-before
```

The capture command ran with the immutable baseline source directory as its
working directory and an absolute executable/output path. Build and run both
returned0. The actual run reports `Original menu controls, defaults and manual
persistence checks passed`; here “manual persistence” names the test's save path,
not human acceptance. The executable SHA256 is
`019aff73b7df0dc00c9ed25f9491504dce9823489a9f9d1a68540351eda57d52`.
Logs are `baseline-golden/menu-build.log` and
`baseline-golden/menu-before-run.log`, relative to `.local/oe49/`.

All three actual900×600 PNGs were opened and inspected:

| Private capture under `.local/oe49/baseline-golden/menu-before/` | Observed baseline |
| --- | --- |
| `menu.main.minimum.png` | All nine economy generations occupy the main screen alongside general actions; no recommended player entry. The long diagnostic footer is shortened at this aperture. |
| `menu.original.minimum.png` | Rule10 setup, selected map, difficulty, paid starter, map-check status and save/Recovery hints are visible; the selected data root is absent. |
| `menu.original.paid.png` | Actual paused paid Rule10/schema26 synthetic-fixture city, with10566 Funds and zero residents/workers; this is not the original-data golden city. |

These captures use authored test fixtures and the test's dummy/software SDL
driver. They establish the baseline appearance and scripted test result, not
the three required final window sizes, native input delivery, human gameplay
or a final OE49 PASS.

### Current software-SDL capture review

All nine final PNGs from `--player-mode-capture` were reopened and inspected
after the final test-only correction in the isolated implementation worktree.
These are final1280×800/1600×900/1920×1080 images, distinct from the three
baseline900×600 images above. Captures and the SHA256/size/dimension
manifest are private, relative to that worktree's `.local/oe49/`:

| Captures under `menu-after-final-03/` | Actual apertures | Observed result |
| --- | --- | --- |
| `player.main.{1280x800,1600x900,1920x1080}.png` | 1280×800, 1600×900, 1920×1080 | One recommended entry; distinct Load Save and Advanced / Legacy; experimental/unsupported warnings fit; no Continue without a retained session. |
| `player.legacy.{1280x800,1600x900,1920x1080}.png` | Same three apertures | Historical opt-ins, all nine original-economy labels, N New Sandbox and Back to main menu are visible, with no action-label overlap or cutoff. |
| `player.setup.{1280x800,1600x900,1920x1080}.png` | Same three apertures | Recommended Rule18, map, data root, difficulty, starter and Start/Back/Check again fit. The long data root has bounded leading elision at1280 and fits fully at1600/1920. |

`visual-review.json` records every exact image SHA256 and the observation for
each opened file. Its SHA256 is
`ea6705ed2472561f6ec7699f90c935063eea83b2eea45a605f34d1b00f53d662`.
The existing bottom diagnostic footer deliberately limits its text to the
available column budget; its build-type suffix is elided at1280. This is bounded
diagnostic elision, not action-label overflow.

The tested Release menu-test executable SHA256 is
`895c6db077e5948af7cc0bf30354939cf1726d8e6cdc47bf79a84341201a0084`;
the normal executable is
`0b81e37224c73a6bb57eee2e7f02c35dcf56f5838ce4c963489875553318d4bf`.
The footer reports configured build metadata `156fddf9da8f dirty / Release`;
that displayed revision is distinct from the final tested source commit above.

These supported-map setup captures show a blue/enabled Start button after a
successful policy3 map check. They contain no disabled, checking or unsupported
map state. Their pixels do not prove hitbox rejection, focus/resize cancellation
or disabled semantics; actual `menu-map-rules-selection` and negative preflight
event checks provide that separate evidence. The source remains frozen during
review. This is **SCRIPTED_SDL**, using authored fixtures and dummy/software
rendering, and establishes neither native delivery nor human acceptance.

### Real recommended-session observation

The explicit recommended-menu path was separately exercised with the actual
Xia map and Normal difficulty. The paid Rule18 starter begins paused at tick0
with four Houses,1404 construction Funds spent and10596 Funds remaining.
Full-document comparison passes after F5/F9 and after a separate-process
application restart/restore. Private outputs are in
`.local/oe49/real-recommended-final-03/` in the implementation worktree. This confirms
that the menu selection reaches paid production authority; it does not certify
the later city's supply, finance or fire outcomes, nor human play.

### Historical authority and golden World

The unchanged private historical exporters were compiled against the current
libraries. All132 complete JSON payloads/case indexes across schemas1–34 match
the protected baseline literally. The comparison records source/library
bindings and successful before/after guards for1731 protected input files.
`historical-final-01/comparison.json` has SHA256
`ee7a6c4c56724845d56546225d7455a0d7231d78ebe3391a822492931a50eefb`.
This is **HISTORY_PASS**; it does not reinterpret a historical generation or
introduce Rule19/schema35.

The golden reference and fresh baseline/final creators use paid Xia/Normal Rule18 strategy
`oe48-watch-phase1-third-food-three-markets`, variant `lean-med`, documented in
the [OE48 assessment](../original-economy/oe48-citywide-fire-reliability.md).
Its first Elite17 is tick5395; the following20400 ticks end at25795. It records
13 occupied Houses,5200/5200 current needs, zero Food/service failures or House
devolutions,400/400 Elite17 needs and no target fire/vacancy. House fires remain
**5**, burning House ticks2915 and all-building ignitions65. Taxes22630 minus
upkeep17450 give **+5180 operating Funds**, with minimum Funds5391. Before this
window there are27 real Food misses. The fresh baseline and final seed,
first-success, midpoint and result save bytes are literal-equal and reproduce
these metrics. The separate final replay reaches tick25795 successfully.
Seven complete saved checkpoints are literal-equal across baseline, final
creator and replay; four complete Recovery payloads match the result document.
The raw timeline and Inspector CSV are also literal-equal. The independent
`final-audit.json` reconciles actual phase39 inputs, physical arrivals,
continuous occupancy, fire intervals and accounting, and rechecks all132
historical files and1731 protected inputs. Its SHA256 is
`d24cc4200a7c974c60835b3ffccc5aea5f0527f68ae811101113d9a9bde8a783`.
No per-tick digest equality is claimed: this run did not request that optional
instrumentation. The paid city results remain separate from the four-House
starter and human acceptance.

## Limits carried forward

The recommendation does not certify unlimited supply, fire-free play, fairness
for arbitrary layouts or profit for every city. Five measured House fires,
authored economy/service adapters, incomplete original graphics/terrain and
unsupported original campaigns/savegames remain visible limitations. All
historical Rules1–18 and schemas1–34 keep their exact contracts. Native human
acceptance remains open; no tag, release or distribution acceptance is part of
this change.
