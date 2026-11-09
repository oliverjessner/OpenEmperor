# OE58: human playtest evidence and visual follow-up

Recorded on **2026-11-09** from the user's OE58 account. The date and duration
of the actual play session were not supplied. This is the first concrete user
playtest feedback added to the [OE50 handoff](oe50-native-playtest.md); it does
not rewrite that earlier run's human **NOT_RUN** results.

The user reports successful paid startup, Road/House construction, inspection
and a real resident-triggered tent graphic. They also report a hard-to-recognize
Vacant plot and a clipped paid-starter error. **Human acceptance is PARTIAL.**
There is no screenshot file attached to this task, no newly opened user image,
and no directly observed native session in this record. The new work has the
bounded automated results below; it is not a blanket native or release PASS.

## Evidence labels

| Label | Meaning in this report |
| --- | --- |
| **HUMAN_OBSERVED** | A specific action/result witnessed by a human and recorded with observer/session identity. No new such observation is claimed here; AI inspection of scripted captures stays automated evidence. |
| **USER_REPORTED_PASS** | A concrete successful action described by the user. Its limits and missing identity fields remain visible; this is not independent reproduction. |
| **AUTOMATED_PASS** | A named, completed technical/scripted check, with actual build, inputs and result. It never supplies human acceptance. |
| **NOT_RUN** | No specific observation or completed check for that case or acceptance layer. |
| **PARTIAL** | Some specified evidence exists, with an explicit remaining scope. |
| **FAIL** | A concrete observed or reported failure, with its evidence origin identified. A reported UI failure is not automatically an engine-integrity failure. |

An aggregate “the other checklist points work” is retained as positive user
feedback. It supplies neither individual action records nor independent PASS
for every subcase. The source of an observation and its outcome are separate.

## Session identity and its limits

| Field | Available evidence |
| --- | --- |
| Application / host | User reports OpenEmperor **0.1.0-alpha.2**, macOS Apple Silicon. Exact OS, CPU, SDL driver/renderer, app path and binary hash are unknown. |
| Selected gameplay | User reports **City-v16, Rule18, policy3, schema34, Normal, `Cities/Xia.map`**. No supplied save is independently parsed for this human session. |
| Screenshot revision | User transmits visible prefix **`e26e`**. The screenshot itself is absent, so the prefix is reported rather than newly read from pixels. |
| OE58 source baseline | Current checkout at task entry is **`e26e674fa50778ee7a47656623cfc5edbd365ea6`**, branch `feat/oe58-playtest-visual-polish`. This source identity is independently read from Git; matching the four-character prefix does not prove the user's binary was built from this exact commit. |
| Available earlier build metadata | Read-only inspection of repo-local `.local/oe57/build-{release,debug,sanitize}/generated/BuildInfo.json` and their generated `core/Version.h` records `5a41db07dcc5`, version0.1.0-alpha.2, arm64, dirty=false, deployment27.0, with Release/Debug/RelWithDebInfo respectively. These are earlier local build records, not the reported `e26e` app. |
| Build identity conclusion | **PARTIAL**. CMake prepares revision/dirty metadata at configuration; a current checkout or old generated file cannot identify a different running binary. The human app's complete build commit, dirty state and SHA256 remain unknown. No personal app/save folders were searched. |
| Display / input | Window size, camera zoom, pixel dimensions, DPI, external display and input-event diagnostics were not supplied. |
| House identity | The newly purchased fifth House's BuildingId and exact cell are unknown. The user's report identifies a real2×2 footprint; ID continuity across immigration was not recorded. |
| Original inputs / private files | No new source-file hash, save, screenshot or command ledger was supplied with the account. No original assets or private user images are included in Git. |

## What the user actually reports

| Case | Reported action/result | Evidence / acceptance boundary |
| --- | --- | --- |
| H58-01 Paid entry | Four-House starter loads successfully on Xia/Normal/Rule18. Tick0, paused, Funds10596, four Houses, no residents yet. | **USER_REPORTED_PASS** for this particular paid start and visible initial state. No new independent launch is claimed. |
| H58-02 Additional construction | Road construction and a fifth paid House succeed; the House counter changes4/20→5/20 and Funds become10494. | **USER_REPORTED_PASS** for the described purchases. The aggregate reported debit is102 Funds. No receipt ledger, road-cell count, exact per-command debit, duplicate-refusal result or BuildingId is inferred. |
| H58-03 Vacant inspection | The new House is Vacant/Type2 with an actual2×2 footprint; the Inspector works. No native House graphic is visible and an authored fallback is used. | **USER_REPORTED_PASS** for construction/status inspection; **PARTIAL** visual readability. This does not show a missing World building or license a guessed original Type2 sprite. Full footprint picking, all tabs and scroll endpoints have no individual human record. |
| H58-04 Admission graphic | The user says the actual tent graphic appears once residents move in, corresponding to Shelter/Type3. | **USER_REPORTED_PASS** for the described admission/art transition. Actual immigration tick, same BuildingId, exact population/Funds and full pre/post F5/F9 equality remain **NOT_RUN** in this human record. |
| H58-05 Broader checklist | The user says the other items of the supplied playtest checklist work. | Positive **USER_REPORTED_PASS** at aggregate level only. It does not fill each OE50 row or its detailed subcases. |
| H58-06 Unsupported starter layout | On `Cities/ASSR-Loulan.map`, the user reports a Map Rules Check PASS followed by paid four-House starter refusal and return to map selection. The message starts `No legal empty 24x13 level ...` and is clipped at the bottom. | **FAIL — user-reported message readability/navigation clarity**. No complete error suffix or independent image review is claimed. A correct placement refusal after general map admission is not itself a MapPermissions defect. |

The reported Type2→3 change is an ordinary gameplay event, not a request to set
a type, create residents, advance hidden ticks or pre-draw a Shelter. No
reported counter is promoted to a full accounting or persistence proof.

## Visual and starter contracts

The [House visual contract](../reverse/original-house-visuals.md) leaves
Vacant2 native selection unresolved: the generic initializer names
`0x47a`/physical1508, while the ordinary setter aliases
`0x471`/physical1509 or1510. The complete lifecycle choosing between them is
not qualified. The [lifecycle contract](../reverse/original-house-lifecycle-contract.md)
also distinguishes actual admission from pure type/model labels. A more visible
authored plot is not original Type2 parity and must disappear only when actual
World state selects an already supported occupied stage. Existing
[OE52 static artwork](../rendering/oe52-original-visuals.md) is separate evidence.

Map rules admission and paid starter placement are separate gates. A map can
meet the selected rules and still have no legal empty24×13 level starter area.
The reported Loulan refusal must remain a real refusal; an improved message
must explain the problem and the explicit Empty City/other-map alternatives
without switching the user's map or widening placement permissions.

OE58 has no new gameplay rule, schema or migration scope. Historical
Rules1–23/schemas 1–39, construction, immigration, needs, goods, staffing,
movement and fire remain existing contracts. A presentation/menu change is
not itself fresh proof of all those historical contracts.

## OE58 result ledger

This table separates the supplied account from verification of the new work.
Only completed, named results may replace a new-work **NOT_RUN** entry.

| Acceptance layer | Current result and scope |
| --- | --- |
| HUMAN_PLAYTEST_EVIDENCE | **PARTIAL** — concrete **USER_REPORTED_PASS** cases above, reported Vacant readability limit and clipped starter error. No supplied screenshot was opened. |
| VACANT_VISUAL | **AUTOMATED_PASS** for the named paid View matrix, authored light/dark contrast cases, actual opened PNGs and controlled three-zoom before/after pairs below. Native Type2 artwork remains unresolved; new human acceptance **NOT_RUN**. |
| SHELTER_TRANSITION | **USER_REPORTED_PASS** for the old account; **AUTOMATED_PASS** for the ordinary paid House20 transition at tick120 and complete F5/F9 equality. New human acceptance **NOT_RUN**. |
| STARTER_UX | Old user-reported readability **FAIL** retained; **AUTOMATED_PASS** for typed placement refusal, explicit choices, real Loulan/Xia controls and the opened three-size modal captures. New human acceptance **NOT_RUN**. |
| RENDERING / INPUT | **PARTIAL** native account. Named scripted picking, frame purity and menu cancellation checks pass; this does not establish every pointer/drag/focus/DPI configuration through native delivery. |
| SAVE_COMPATIBILITY | **AUTOMATED_PASS** within the completed historical, Golden, Recovery, separate-process culture and targeted F5/F9 comparisons below. No automatic migration or original-save support is claimed. |
| RELEASE_TESTS / DEBUG_TESTS / SANITIZER_TESTS | **AUTOMATED_PASS** — full Release 320/320, focused Debug 79/79 and focused RelWithDebInfo Address/Undefined 79/79, all with zero errors/skips; instrumentation limits below. |
| ORIGINAL_PARITY | **PARTIAL** — registered contract/real-data checks pass, but no fresh independent fork74/Housing18/model1800 comparison was run. Unresolved Type2, original animation/scene/runtime limits remain. |
| NATIVE_HUMAN | **PARTIAL** for the supplied old-version account; human acceptance of the new changes **NOT_RUN**. |
| CLEAN_MAC / DISTRIBUTION | **NOT_RUN** — no independent clean-Mac, Finder/Gatekeeper, signing/notarization or release acceptance follows from the report. |
| CI / NEW NATIVE INPUT DELIVERY | **NOT_RUN** in this local validation; scripted SDL and opened captures do not supply either result. |

### Completed automated follow-up — 2026-11-09

The productive View uses the real Xia/Normal Rule18 four-House starter and
ordinary construction. This independent sequence buys one Road cell for2 and
House20 for80, giving Funds10514 and construction1486. At tick0 the fifth House
is Vacant2 with zero occupants. Ordinary simulation reaches tick120 with the
same ID, actual2×2 footprint and two residents: the existing native Shelter3
representative replaces the foundation. Complete World and F5/F9 SaveDocument
bytes agree before and after admission. These are earned automated facts,
distinct from the user's reported Funds10494 and unknown HouseID/road count.

The final productive-view check passes in targeted04; the visual-profile,
real-profile and Inspector checks pass in targeted02 with the same production
implementation. The matrix covers1280×800,1600×900,1920×1080 and camera1×/2×/4×,
plus explicitly authored light/dark grass, unselected/hover and translucent
preview cases. The rendering author individually opened the43 new captures;
a second AI reviewer individually opened all38 existing House-stage/alpha/
culling captures and22 Inspector captures. These103 distinct final PNGs have
per-file SHA/findings records. High-zoom viewport cropping of large neighboring
or Elite art and synthetic test backgrounds remain visible limits. This is
software-SDL image review, not human play or original-scene parity.

A private controlled comparison links the retained old production renderer to
the exact new paid Vacant save, using the recorded camera and selection at
1280×800/1×/2×/4×. All three complete Worlds and canonical save bytes agree;
all three old PNGs and their three new counterparts were actually opened.
The pale authored foundation is clearer than the old grass/flat marker, stays
inside the actual2×2 footprint and adds no tent or occupants. The linked old
archive's build metadata is5a41; relevant runtime sources are byte-equivalent
to baselinee26e674. Neither fact identifies the user's unknown native binary.

The new typed menu case passes, including the900×600 layout/control case.
The separate real-data test keeps Loulan's paid24×13 starter refusal and reads
the full reason at1280×800,1600×900 and1920×1080; root individually opened all
three captures. Explicit Empty startup retains Loulan and performs the map
check, then full F5/F9 documents agree. Xia remains a four-House paid control
at tick0/Funds10596. These checks do not widen placement permissions or turn
the reported old clipped message into a new human PASS.

Private evidence under ignored `.local/oe58/` includes
`render-targeted-tests-{02,04}.execution.json`, `menu-real-01/report.json`,
`menu-visual-review-01.json`, `menu-smoke-newcase-01.execution.json`,
`vacant-visual-review-fire-01.json`, `regression-visual-review-supply-01.json`
and `vacant-controlled-pair-visual-review-01.json`. The old-render helper's
`before-vacant-render-run-01.execution.json` verifies all 17,301 protected input
files before/after. Capture pixels, saves and original assets remain private.
The [visual contract](../rendering/oe58-vacant-house-visual.md) records the
presentation bounds and unresolved original lifecycle.

Earlier isolated QA attempts remain failed in their original OE58 logs: the
first omitted the required source-bound Rule18 session context; later test
witnesses used a pick point still inside the actual House ground footprint or
expected Inspector text without its real line wrapping. The harness setup,
point and text witness were corrected before the passing captures. Those
records are not relabeled PASS. No World/source-integrity violation was found,
and production admission, picking and integrity checks were not weakened.

### Final automated regression — 2026-11-09

The completed independent audit is **PASS**:
`.local/oe58/final-technical-audit-01.json`, SHA-256
`a6959d80755159b35570c498fabf96fa127acb08b7f7053ce5cdb5dcf6ce5328`.
It binds 700 runtime source inputs to the final root/mirror bytes, actual JUnit
results and unchanged executable/archive/cache identities. Release runs all 320
registered tests; Debug and sanitizers each run the selected 79 distinct tests.
All 478 executions pass with zero errors and zero skips. The RelWithDebInfo
sanitizer build verifies Address/Undefined instrumentation for 62 executables
and 13 core archives; system SDL/external dependencies are uninstrumented.
macOS LeakSanitizer is off (`detect_leaks=0`), so no leak-check PASS is claimed.

Historical comparison covers 152 complete JSON files, including case indexes,
across schemas 1–39: 132 for 1–34 and 20 for 35–39, literally equal to protected
references. The fresh Rule18 creator and separate replay reproduce the earned
5395→25795 window and compare all seven complete checkpoints in each phase.
Its 99 logical outputs comprise 93 complete top-level literal comparisons,
four complete Recovery payloads and two Recovery-history metadata comparisons;
only history IDs, generated checkpoint filenames and local creation times
differ in the latter. The replay also compares the full snapshot/SaveDocument.
Optional per-tick digests were not rerun for OE58. The unchanged finite city
still has five House fires/2915 burning-House ticks; this regression is not a
new fire-free gameplay claim.

The actual paid Rule22/schema38 and Rule23/schema39 culture checks each use
six separate creator/restore processes at 0, 103 and 20400 continuation ticks.
They record 40 and 31 complete literal comparison rows respectively, including
one seed self-check each; the separate-process comparisons remain distinct.
Eighteen further complete Rule23 SaveDocuments match the historical paid
references. These are authored OpenEmperor culture contracts, not a fresh
original-runtime reconstruction. The audit also confirms all 17,301 protected
files/10,223,994,973 bytes, modes, mtimes and protected-tree membership unchanged.

The final recursive PNG inventory contains 446 files in 260 SHA groups. Reviewers
actually opened 262 physical paths; 443 capture files match reviewed contents,
with three authored static-icon copies identified separately and zero unique
capture contents left unreviewed. This transfers only a bounded still-image
observation across identical files, not an assertion that every physical copy
was opened or that native input passed. The bound reports are
`final-{release,debug,sanitize}-junit-01.xml`,
`historical-final-01/comparison.json`, `rule18-final-01-audit.json`,
`culture{22,23}/runtime-final-01/result.json` and
`all-png-content-inventory-final-01.json` under ignored `.local/oe58/`.
CI, new native input delivery, new human acceptance and clean-Mac/distribution
acceptance remain **NOT_RUN**; original parity remains **PARTIAL**.

## Targeted native follow-up

Use the [isolated human checklist](oe50-human-checklist.md) and
[input acceptance guide](../testing-input-acceptance.md), recording a fresh
build identity, own app root and only expressly supplied working-copy saves.
These follow-up cases are **NOT_RUN** until individually reported or observed:

1. Start Xia/Normal/Rule18 paid setup; buy the fifth House through ordinary
   commands. Record its ID/cell, exact debits, zero occupants and Type2. Inspect
   the unselected authored plot on light/dark grass and roads at1×/2×/4×;
   hover/selection must be distinct and no occupied tent appear early.
2. Save/load before admission, run ordinary immigration, then record the same
   ID/2×2 footprint, actual Type3/residents and original tent. Save/load again;
   compare the complete World, Funds and population, with no second purchase.
3. Check the productive view at1280×800,1600×900 and1920×1080, camera movement,
   actual clicks/Inspector scroll, terrain/occlusion and road-drag cancellation.
   Record each available display/DPI configuration separately.
4. Try the paid starter on the originally selected Loulan map. If placement is
   still illegal, read the complete refusal and use the explicit Empty City or
   other-map option. Preserve the chosen map and any retained session/settings.
   Empty startup still requires general map admission. Recheck Xia as a control.
5. Record native Recovery, whole-app restart, sustained play, external displays
   and clean-Mac/Gatekeeper separately. A general favorable report does not
   supply these results or a multi-hour stability measurement.

Keep screenshots, exact app metadata, logs and saves private under ignored
`.local/oe58/`. An actual image review must list the files opened and its
scope; generated captures alone are not visual or human acceptance. There is
no user-screenshot publication permission implicit in this documentation.
