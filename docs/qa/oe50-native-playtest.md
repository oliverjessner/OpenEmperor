# OE50: native macOS acceptance and human handoff

OE50 starts from the verified OE49 merge
`1ae2f21be399a93eadf5e1d41ddd385c47f1627a` on
`test/oe50-native-playtest`. Its purpose is to separate current technical,
scripted application, native delivery and human evidence. The
[human checklist](oe50-human-checklist.md) is prepared; no human observation
has been supplied. Preparing it does not complete acceptance.
Overall acceptance is **PARTIAL, safe QA scope**: recorded technical,
persistence and local-package gates pass; paired native pointer delivery,
human play and public distribution acceptance remain open.

## Current status and identity

| Layer / identity | OE50 result |
| --- | --- |
| TECHNICAL / REGRESSION | **PASS within recorded scope** — final-source Release292/292, bound-reuse Debug10/10, ASan/UBSan12/12,36 helper cases and strict local package checks pass |
| SCRIPTED_APP | **PARTIAL** — create/resume/inventory/inspection/layout pass within their executed scopes;18 actor cases remain NOT_RUN |
| NATIVE_GUI | **PARTIAL** — actual Cocoa/Metal startup, keyboard entry, paid starter, camera, F5/F9 and working-copy restart observed; pointer delivery blocked |
| HUMAN | **NOT_RUN** for every checklist item |
| PERSISTENCE | **PASS within recorded scope** — scripted full-document/Recovery checks and native manual-save, independent restart and earned continuation pass; native Recovery remains NOT_RUN |
| RELEASE_READINESS | **PARTIAL** — strict local relocated/unzipped ad-hoc package PASS; human, independent clean-Mac, Finder/Gatekeeper and public readiness remain open |
| ORIGINAL_PARITY | **PARTIAL**, existing bounded contracts only; no new parity result |
| GIT_PR_MERGED | [PR #30](https://github.com/oliverjessner/OpenEmperor/pull/30) created; merge verification recorded in its post-merge metadata |
| Baseline / branch | `1ae2f21be399a93eadf5e1d41ddd385c47f1627a` / `test/oe50-native-playtest` |
| Packaged/native candidate source commit | `58969c095501d0bb41e15b21e4a1fd0c02c0460a`; later Python-helper review correction does not change the C++/bundle bits |
| Final tested helper/source commit | `67ba4b999a77a35f9516c3c020f87bd6a3a0a033`; complete Release suite re-executed after the Python-only correction |
| Current host / SDK / compiler / SDL | macOS27.0.1 build26A434; Apple M2 Pro, Mac14,9/arm64/10cores/32GiB; SDK27.0, AppleClang21.0.0, CMake4.4.3, SDL3.4.14 |
| Tested binary / app | Unmodified `.local/oe50/dist-27-01/OpenEmperor.app`; executable SHA256 `c6923aa667d80b406cd81d4c4f4a3ed9c4c1de7cb38b9461c4c9497da2626628` |
| Actual video / renderer / display scale | Cocoa/Metal;1100×700 window/pixels/output, density/display scale1.0, SDL window ID2 in native-01 |
| Supplied data / model / map identities | Explicit `/Users/oli/github/OpenEmperor/.local/gog-extracted/app`; model SHA256 `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`; Xia map `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433` |

All new private outputs use fresh directories under ignored `.local/oe50/`.
Automation roots and human roots are separate. Existing source saves are
read-only inputs; F5 targets a verified working copy or a new session save.
There is no search in personal saves, no original payload in Git and no World,
rule, schema or migration change. Rules1–18/schemas1–34 and City-v11 rule3 /
ordinary City-v16 rule4 defaults remain exact.

The current [LICENSE](../../LICENSE) declares GPL-3.0-only for OpenEmperor's
source and excludes original game assets. Earlier OE43 text saying a project
license was absent is historical and is not a current finding. A license notice
does not establish a tested distribution, notarization or clean-Mac acceptance.

## B01–B20 capability matrix

Each result is bounded to its actual command/input, observed effect,
source/build identity, map/rule/schema/tick and private evidence.
**NOT_RUN** is an unexecuted acceptance layer. The evidence column identifies
completed observations and the remaining scope, without inventing a defect.
Every HUMAN and original-runtime comparison below is **NOT_RUN**.
**TECHNICAL PASS contracts** refers to the bounded registered tests in the
current292/292 Release result, not a claim that every listed user action was
performed natively or that every original-game behavior exists. Scripted
execution has its own column. Partial technical rows retain unmeasured longplay,
visual or actor scope.

| ID / actual action and expected result | TECHNICAL | SCRIPTED_APP | NATIVE_GUI | HUMAN | RELEASE_READINESS | ORIGINAL_PARITY | Evidence required / priority of open scope |
| --- | --- | --- | --- | --- | --- | --- | --- |
| B01 Data setup: choose the supplied folder; reject missing/invalid data without replacing a retained city | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native explicit data/root accepted, chooser not exercised; C invalid-folder refusal. P1 native chooser/pointer acceptance |
| B02 Main menu: explicitly choose recommended Rule18, paid/empty, difficulty, map; historical O/F/S/M/W/I/D/T/U/N remain exact | PASS contracts | PASS selections | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native U/Return starts exact Rule18 Xia/Normal paid city; pointer request fails delivery. I/C retain broader scripted selection scope; P1 acceptance |
| B03 Navigation: held arrows, zoom1×/2×/4×, pause/step/speed, focus and resize act on the correct window | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native350ms Right changes camera with purity; Step/F9 observed. Z has no observed zoom change, native zoom/resize/focus remain NOT_RUN; P1 acceptance |
| B04 Paid placement: buy one legal Road/House once; duplicate, blocked and unaffordable commands remain atomic | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | C: Road2 and House80 paid through UI, duplicate refusal exact; insufficient-Funds/other protected sites are separate tests; P1 acceptance |
| B05 Clay→Pottery→Warehouse: physical extraction/loading/arrival/conversion/storage conserve goods | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | R/native-03 audits conserve six goods-owner equations; native104 ticks retain real typed cargo, not a newly built full chain; P2 scope |
| B06 Food/Hemp: buy attached farms/fields; actual House arrivals and later consumption change real stocks | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | R/native-03 audits reconcile loaded Food and19 owner equations; fresh farms/fields not bought in either continuation; P2 scope |
| B07 Market distribution: real pickups/deliveries/returns satisfy the selected Rule18 contract | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native-03 actual TickDriver goods/Food cargo and reservations conserve amounts; no new native purchase/indefinite throughput claim; P2 scope |
| B08 Staffing/operations: pause/resume and priorities affect the selected building while retaining goods/trips and installed upkeep | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | C: Market7 pause/resume and High/Low/Normal,900 actual ticks; no claim of a whole paused-invoice experiment; P1 input/P2 economy |
| B09 Water/medicine/religion: actual allowed service visits renew existing timers; expiry remains real | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | P: actual Needs/Visits state inspected for13 Houses; a new full citywide expiry/arrival experiment is not claimed; P2 service capacity |
| B10 Culture: real authored school→House arrivals affect needs; no original venue/performer claim | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | P: existing culture arrival witnesses/timers inspected; no new complete service cycle or original venue proof; P3 original-system gap |
| B11 Common housing: observe one or two paid House upgrades with fulfilled actual needs | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | P: earned House types/Needs/Goods/Visits inspected; a fresh human one/two-upgrade journey remains NOT_RUN; P2 progression/UX |
| B12 Elite housing: continue an earned Rule18 city and report actual retention, failures and footprint | PARTIAL | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | R:5395→5555; native earned city5395→5499, full documents. Neither repeats the20400-tick retention window; P2 scope |
| B13 Fire/Watch: paid staffing and real Inspector arrival extinguish/protect; dispatch alone grants none | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | R Inspector arrivals56→58; native-03 actual arrivals56→57. Full native extinguish/protection cycle and fire-free city not claimed; P2 scope |
| B14 Roads: legal drag commits once; Esc/right-click/focus/resize/UI-release cancel; interruption/repair preserves begun edges/cargo | PASS contracts | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | C: five actual scripted cancellation journeys preserve the complete document; single paid Road works. New native multi-cell drag and repair remain separate; P1 acceptance |
| B15 Bridge/Ramp: separately selected rule5/6 retain bounded geometry and transport | PASS contracts | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | Registered route/cargo/restores remain technical evidence; no new scripted paid crossing, authored ramps distinct from original-map parity; P3 geometry scope |
| B16 Maps/rendering: second admitted original map, three apertures and zoom1×/2×/4× show bounded controls/sprites | PARTIAL | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native Xia starter visible at1100×700; zoom/second-map native cases NOT_RUN. C/R supply the separate18-image matrix/Handan; P3 scope |
| B17 Inspector/help: select real owners, scroll complete rows, read State/Needs/Goods/Visits; modal input never leaks | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | NOT_RUN | P:66/66 bodies,52/52 House tabs,28/46 actors picked;18 actor cases NOT_RUN. L:37 checks/3 operations pass,20 selected final images opened; P1 native acceptance |
| B18 Persistence: F5/F9, manual load, new process and Recovery preserve complete earned authority and source bytes | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | Native tick1 and earned5395 independent working-copy restores and full F5/Step/F9/F5 equality pass; native Recovery remains NOT_RUN; P1 scope |
| B19 Longplay/performance: finite paid continuation keeps accounting exact; native FPS/leaks require actual measurement | PARTIAL | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | R160/native104 ticks: exact owner conservation and historical Inspector staffing witnesses; citywide assigned/required counts not derived. Zero native interval cashflow; no fresh100k/FPS/leak result; P2 scope |
| B20 App lifecycle: quit only the isolated QA process, restart, load another generation and retain defaults/history | PASS contracts | PARTIAL | PARTIAL | NOT_RUN | NOT_RUN | NOT_RUN | All three own native processes quit exit0, two independent working-copy loads pass; other-generation native load remains NOT_RUN; P1 scope |

### Current technical and scripted observations

The first fresh package attempt ran the complete Release suite successfully:
**292/292 PASS**,97.35s (JUnit97s), zero skips. The package then failed the unchanged strict
dependency verifier: the installed Homebrew OpenSSL4.1.0 `libcrypto` declares
macOS27.0 while the candidate requested26.0; SDL declares26.0. `vtool`
corroborates those dependency minimums. The failed26.0 package/log is preserved.
This is a diagnosed deployment/dependency mismatch, not evidence of an app
coordinate fault. The repaired explicit `--min-os 27.0` candidate now passes
the strict checks; the existing26.0 default and verifier remain unchanged.
The initial packaging helper tests34/34 and six path-guard controls pass;
the post-review helper selection below supersedes the34-case count.

The fresh27.0 package runs **292/292 Release tests PASS**,96.89s (JUnit96s), with zero failures,
skips or disabled cases. All21 negative package cases are rejected. The original
bundle, relocated app and unzipped app pass dependency/architecture/resource and
ad-hoc-signature checks, including read-only original-data smoke and save/resume.
Actual bundled dependencies are SDL3.4.14 and OpenSSL4.1.0. Build metadata is
`58969c095501 dirty / Release / arm64 /27.0`; the dirty state includes the pending
owned documentation and known-issue updates, not a hidden World change.
`dist-27-01/package-report.json` SHA256 is
`72d8a2fa30ae8763e9e9eeac36890158e8bf933ccd088600ce9a94a7f19f5f2a`.
The local5079787-byte ZIP is
`OpenEmperor-0.1.0-alpha.2-58969c095501-macos-arm64.zip`, SHA256
`35233f6e8dbe5d285f6caea6e5754e3f943c8fb90e8a0abf96a447f18d92d9cc`.
This is a local QA artifact, not a release, notarization or public readiness.

`environment-before.json` records current OS/CPU/SDK/toolchain/linkage and the
initial clean1ae2f21 base; SHA256
`d31478fe17e1e64b8eda49032a3df01c56eaf29c6b6b8586ebef7becd0f23d84`.
Native launch binds the unchanged package executable and frameworks:
libcrypto SHA256
`0ac9b2c638853c6e50894b839c77a25fd32a8f38c0215907e2812b17c10c81cd`,
SDL SHA256
`6f462c3ad2304cba84417a7f0b6295c597a8665367e88bd855841c0388c60278`.

The focused Debug rerun is **10/10 PASS**,224.66s, using the identified OE49
artifacts after522 compiled/source/resource/CMake inputs were compared unchanged.
This is transparent binary reuse with a fresh test execution, not a new Debug
build. The first sanitizer selection completed7 tests but could not run one
missing executable; `ctest-sanitize-selection-01.log` and
`sanitize-junit-selection-01.xml` preserve that7 PASS/1 NOT_RUN apparatus result.
The missing input/inspector targets were then built. The final exact selection
completes **12/12 PASS**,106.51s, with no ASan/UBSan diagnostics: ten unchanged-
input existing binaries plus those two freshly built targets. It uses
`detect_leaks=0` on macOS; the external SDL library is uninstrumented.
`ctest-sanitize-final.log`, `sanitize-junit-final.xml` and
`build-sanitize-input.log` retain the executed scope. No product failure is
inferred from the initial absent executable, and no missing case is called PASS.
Runtime source/resources/CMake inputs are unchanged from the OE49 production
code; the current change is QA/packaging and documentation.

The bounded technical mapping includes menu/profile/input cases for B01–04,
`original-production-world`, `original-market-transport` and the goods/food/hemp
world/save cases for B05–07, finance/operation controls for B08,
services/religion/medical/culture cases for B09–10, housing-stage and source-bound
desirability guards for B11–12, fire/Inspector dispatch cases for B13,
road-connectivity/input tests for B14, bridge/ramp transport/geometry tests for
B15, render/input/layout cases for B16–17, and save/Recovery/menu-generation
checks for B18/20. These are existing scoped contracts. B19 does not acquire
native FPS/leak or a fresh100k paid run from the successful suite.

Private evidence abbreviations below are relative to the implementation
worktree's `.local/oe50/`: **C** = `scripted-create-01/`, **R** =
`scripted-resume-01/`, **L** = `scripted-layout-01/`, **I** =
`scripted-inventory-01/`, **P** = `scripted-inspect-01/`. Their `report.json`
both report PASS under SDL3.4.14 **dummy/software**, with native_gui and human
explicitly NOT_RUN. A toolbar selector PASS is not a purchase or supply PASS.

C starts the actual recommended Xia/Normal Rule18 paid city with construction1404
and Funds10596; UI buys Road2 and House80, with exact duplicate refusals. The
five Esc/right-click/focus/resize/UI-release road cancellations leave the complete
SaveDocument unchanged. Market7 operation/priorities are exercised through real
inspector actions. At tick901 the city has20 bodies,75 residents/available workforce,
construction1486, tax22, upkeep190 and Funds10346. F5/Step/F9/F5 preserves complete
canonical authority, and menu Recovery restores that paid checkpoint. A second
menu-created Handan City-v11 rule3 city starts at tick0/Funds100, construction1200,
24 residents; it is a distinct map/rule session, not a migration.

R independently loads a working copy of the protected earned first-success
Rule18/Xia state at5395 and advances160 real ticks to5555. Its resulting complete
document has66 bodies,46 couriers,325 residents/available workforce, construction5347,
tax2198, upkeep3460 and Funds5391. Those are lifetime totals at the endpoint;
they do not establish operating profit for this160-tick continuation.

The independent `paid-continuation-audit.json` reconciles320→325 residents/
available workforce, the retained Inspector-dispatch staffing witness, active
goods cargo/reservations, Food and owner equations. It does not derive new
citywide assigned/required-worker totals; those audit fields remain null.
Goods change4 loaded trips/7 cargo units→6/15, with exact corresponding active
reservations4→6. Food changes3 loaded trips/10 units→3/4. Six goods and19 Food
owner equations conserve physical amounts. Actual Inspector arrivals advance
56→58; the tick5471 dispatch has two actually assigned workers, operating=true
and population324. Treasury5391 remains positive, but the inherited+5180
operating result belongs to OE49's full golden window, not a new OE50 long run.
The audit SHA256 is
`9fc8c54a594a4b3363671b7d1f8d98af75923ba3cd076cb8226afc839af7f292`.
This paid continuation remains scripted, not native or human evidence. The
capture manifests' literal HUD “Workers” observations also mean available
workforce, not a count of assigned workers.

I registers784 menu rows and1063 toolbar observations across110 events, with10
captures. All16 ordinary profiles, the recommended entry and nine original
choices/difficulties are visited. Those are observed controls, not784 features
or1063 successful purchases. P inspects the earned tick5395 city:66/66 bodies
and52/52 House section visits pass;28 actors are selected by actual alpha-hit
events. The18 remaining actors split into11 idle/non-live frames and7 live actors
for which this sampled search found no selectable visible pixel. Both groups
remain NOT_RUN. Full World and canonical F5/F9 equality pass.
`P/inspection.json` SHA256 is
`7c991c00329df18cfb2e79a5a4181e3348b337bc9b789d21e3b10e4e7f3ad4df`.
The I/P report counts do not claim their19 captures were visually reviewed here.

### Historical payloads and protected inputs

The unchanged authored exporters were compiled against the current27.0 Release
headers/libraries. All132 complete canonical payloads/case indexes across
schemas1–34 remain literal-equal to the independently preserved corpus.
`historical-final-01/comparison.json` SHA256 is
`1ba6a2bbf8c3af625333e0877bd451ccef78f1bf18e95444c1361c494ee2aa84`.
Its before/after bindings retain13 archives and540 bound inputs:539 tracked
repository files and one ignored generated Python bytecode file, not540 tracked
source files. The classification supplement is
`historical-final-01/source-inventory-classification.json`, SHA256
`bc270f30eaccd980685f2c03b75887248a701562e9b97c1a62efb9ef21e7c7e4`.

`historical-final-01/binary-bindings-after.json` records the actual exporter
binaries after the successful compile/export, plus their current dependency
bindings; SHA256
`9d5a988c675a5457f637170fa5d10fec0a2b161051269cb11963807772852694`.
It is not a retroactive prelaunch execution seal. Explicit protected-input
verification checks1755 files, including the exact1464-file original inventory;
bytes, SHA256, mode and modification times remain unchanged, with no missing,
changed or unexpected original files. `input-after-scripted.json` SHA256 is
`214f9897ac0f5c5e29a3aeb79bb3e188ae4a592a9e0574e8b441312481883c28`.

### Reproducing the scripted modes

From this implementation worktree, use the actual supplied data root and current
app resources. The following are reproduction commands, not additional claimed
runs; every `repro-*` app/output directory must be fresh. The earned input below
is the existing protected working copy at5395; do not search personal saves or
write back to its supplied source.

```sh
cmake --build .local/oe50/package-build-27-01 --parallel 2 --target openemperor-system-playtest-acceptance
oe50_tool="$PWD/.local/oe50/package-build-27-01/openemperor-system-playtest-acceptance"
oe50_data=/Users/oli/github/OpenEmperor/.local/gog-extracted/app
oe50_resources="$PWD/.local/oe50/package-build-27-01/OpenEmperor.app/Contents/Resources"
oe50_earned="$PWD/.local/oe50/scripted-resume-01/working-input.save.json"
"$oe50_tool" --inventory "$oe50_data" "$oe50_resources" \
  "$PWD/.local/oe50/repro-inventory-app-01" "$PWD/.local/oe50/repro-inventory-01"
"$oe50_tool" --inspect-paid "$oe50_data" "$oe50_resources" \
  "$PWD/.local/oe50/repro-inspect-app-01" "$PWD/.local/oe50/repro-inspect-01" "$oe50_earned"
```

`--create` uses the same first four arguments; `--resume` adds the explicit
earned-save argument. `--inspector-layout` adds its separately supplied final
tick25795 source. Use the recorded four exact exporter commands in
`historical-final-01/comparison.json` with a fresh destination to reproduce
canonical generation checks. The binary/input bindings and existing historical
corpus must remain read-only.

### Opened scripted captures

All nine R `resumed-city-{1280x800,1600x900,1920x1080}-{1x,2x,4x}.png` and all
nine C `paid-city-{1280x800,1600x900,1920x1080}-{1x,2x,4x}.png` were opened.
C `data-refused.png`, `help.png` and `second-session.png` were also opened.
Three preliminary L samples, `1280x800-market11-1x-{top,details,end}.png`, were
inspected before the full layout execution completed. They remain recorded in
the initial manifest rather than being retroactively described as a full review.

`visual-review-scripted.json` records all24 exact paths, PNG dimensions, file
sizes, SHA256s and per-image findings. Its SHA256 is
`757d024983f4157c746fcdde85c76da69036710cdfcc6441293ea76a90786205`.
The actual observed limits are:

- C deliberately switches **Building visuals OFF**, **Road visuals OFF**, legacy
  painter and Supply overlay. Its flat building/road markers are the requested
  diagnostic view, not a fresh inferred decoder failure. The House/road art and
  primitive image states cannot establish ordinary visual parity.
- R shows original building/crop sprites with zero reported building/House
  fallback counters in those capture windows. Flat colored marker diamonds
  remain. At higher zoom the Elite body is partly cropped at the map viewport;
  only1920×1080/1× shows its full lower boundary in these camera samples. The
  toolbar/inspector stay outside that map clipping. This is no universal
  anchor/feet/occlusion proof.
- C Help text is readable in its opaque map-area panel. The invalid-folder
  message states the concrete folder error but bounds/elides its trailing
  detail. Historical Handan toolbar labels use existing ellipses.
- The three preliminary L Market11 samples show top/list, details and end text
  inside their scrolling area, with fixed controls separately visible. The
  final layout execution and separately recorded review below supersede its
  pending execution status without changing the preserved initial manifest.

### Completed inspector-layout execution and selected visual review

L completes37 checks and3 ordinary operation checks at tick25795, with complete
World equality, canonical F5/F9 equality and protected-input byte equality.
It generates46 PNGs. `inspector-layout.json` SHA256 is
`3a8dfac99576cfdd72a0a3c734d902fa6081287358a37652a853e1912eb6de08`.
This includes actual complete-row reachability, clipped-hit refusal,
resize/zoom and operation commands; these event results do not come from
static pixels alone.

After completion, all18
`{1280x800,1600x900,1920x1080}-market11-{1x,2x,4x}-{top,end}.png` images were
opened, plus `1280x536-market11-small-resize.png` and
`1920x1080-house-1x-section2-end.png`. The separate
`visual-review-layout.json` records those20 exact paths, hashes and per-image
findings; SHA256
`8cbe4c46154f98180b3d7100b45fe8e13737419f069350ee6b7ba76af74913e7`.
The original24-image manifest is preserved. Two1280×800/1× top/end images are
reopened here; together the manifests cover42 unique images, including21 unique
layout images, rather than a claim that all46 layout PNGs were inspected.

In all18 Market samples, visible list/detail text stays within its scrolling
area; Pause operation, High/Normal/Low, refused Demolish and the bounded hint
remain separate from the document and toolbar. At1280×536, the inspector has
no complete list/detail rows or Pause button visible: the independently fitting
priority row, refused Demolish and scrollable clipped hint remain visible.
That image does not claim absent controls are usable. The sampled House Goods
tab and end text fit above the toolbar; its displayed lifetime Food miss1 is
not a new finite-window miss count. Actual city flames remain visible. These
are scoped inspector/clip observations, not original visual or fire parity.

These results remain SCRIPTED_APP. No correctly delivered native pointer,
native FPS, native leak result or human play is inferred from them.

## Current native Cocoa/Metal observation

The unchanged packaged candidate was started through LaunchServices with the
explicit supplied data root and isolated `native-01-app`. This is a real native
application launch, not a Finder double-click or a clean-Mac test. Actual SDL
runtime is3.4.14, Cocoa/Metal; the diagnostic observer records44 events/44 records,
capacity512, zero overwrites. The observed client/window, pixels and output are
all1100×700, full viewport, scale1, density/display scale1.0, window ID2.

The first pointer request targets screenshot point`(445,206)` on the visible
1100×732 window image, including its titlebar. Actual delivered MouseDown sequence7
and MouseUp sequence8 are **both`(0,0)`** and select no action. This is
**BLOCKED_AUTOMATION_DELIVERY**, not evidence of an application coordinate bug.
No speculative titlebar/DPI offset or new input transform is applied. Further
pointer requests stop; native Road/House placement, drag, picking, scrolling,
resize/focus and modal behavior remain NOT_RUN.

Native keyboard U then Return opens Rule18/Xia/Normal paid setup and starts the
paused paid city at tick0, construction1404, Funds10596. F5 writes a complete
65529-byte document. A350ms held Right visibly pans, with camera-only purity
checked. Period advances to tick1/F5, then tick2/F9 restores tick1; a further F5
has full-byte equality with the tick1 document. The requested Z has no observed
zoom change, so native zoom remains NOT_RUN. Cmd-Q closes only this own isolated
QA process. There is no claimed native FPS or supported leak measurement.

A second fresh native root, `native-02-app`, loads a working copy of tick1 through
L/Return; F5 retains the complete65529-byte document. Space visibly runs the city
and a second Space pauses at tick107 after an observed intermediate tick32; F5
saves that real continuation. Its observer has37 events/29 records with motion
coalescing and zero overwrites. Source bytes remain unchanged and the own app
quits exit0. `native-02/continuation-summary.json` SHA256 is
`35adbd3041ef12d21a2fa2a04debc24300374ab253f625508e993d88795183a5`.

The third own app loads the earned tick5395 working copy through L/Return/F5:
all515622 document bytes match the protected source, SHA256
`d0c0eddeb5024554c354340a25a89f6b5aa70c11b18da8c6d2178bc27053f424`.
Actual Space shows Running at5427 and a second Space pauses at5499; F5 records
the104 real ticks, population320→325 and Funds5391. Period/F9/F5 restores all
522800 continued bytes exactly, SHA256
`69451e877462c7d78449509fedae17bd525ba650bb5f5b563650f68d9fa00a5b`.
`native-03/continuation-summary.json` SHA256 is
`bc27f3844f29fca373d8d8dd22d6235c2261fa1f2616b014b55f1910b2fa24ce`.
Its observer has32 events/32 records, zero overwrites, actual Cocoa/Metal,
1100×700 window/pixels/output and density/display scale1.0. The66-building
earned scene and Walker frames were opened through CUA. Only this own app was
quit, exit0; all three native stderr files are empty, which is not leak proof.

`native-03/paid-continuation-audit.json` verifies six goods and19 Food owner
equations. Actual goods cargo changes4 trips/7 units→3/7 and corresponding
active reservations4→3; Food changes3 trips/10 units→4/7. Actual Inspector
arrivals advance56→57. Funds5391, tax2198, upkeep3460 and construction5347 stay
unchanged, so this104-tick interval has **zero operating cashflow**, not a new
profit result. Source/save hashes, bytes, modes and modification times remain
unchanged. The audit SHA256 is
`2edc75d0db8eeb4a427026fa6a24022d163e6ba5fa13b4571aa8aae34ef404c9`.

`native-01/launch.json` SHA256 is
`dd75019ea3ec5ddf37a0dea421d82cc5b9524ccaf0a0793d95769ac1b3b58dac`;
`observer-summary.json` is
`2d635a61cd2e25e340059ab11f012b1ff7a95730cebc0488451f6e35747ca770`;
`observations.json` is
`aaf789299ec46758664b80f396078d542698e478f83c0485ca55a453876ee11b`.
The native screenshots were actually opened in the CUA tool transcript; no local
native PNG artifact is claimed. Every HUMAN row remains NOT_RUN, including
after technically observed native keyboard effects.

## Final integrity, storage and publication record

The final1755-input guard passes after native work, including the exact original
inventory and protected historical/earned sources. `input-after-native.json`
SHA256 is
`0a4613546dd225a59bba1cb1d846b69401942cc0f40cc164a891fb76d405eff3`.
The user's original checkout remains on1995eb1 with its five tracked
modifications and an empty index; its complete72212-byte `git diff --binary`
is byte-identical before/after, SHA256
`e2991e89e0841ff259bc77ef41499b18bc76b74fdb8d9c238a69b42192db081b`.
This is distinct from the isolated feature-worktree source identity.

The unchanged packaged app passes the strict verifier again after all three
native processes. Executable, frameworks and plist hashes still match native-01
launch. `bundle-after-native.json` SHA256 is
`7023908599f960500a7b9f0b9e13fe3b0ffc00b8f2bafb86e893e82a6e7106b6`.
The pre-portability `final-technical-gates.json`, SHA256
`7c5d92b1415cbfb40dce8d76c272c81e0d934511c43010d05a051b2106c37edc`,
binds its292/10/12 JUnit records, zero failed/skipped/disabled cases and
the completed evidence. No new P0/data-corruption finding is established.

After the27.0 success,195.1MiB of obsolete failed26.0 compiler products were
removed while retaining logs, executables, caches and saves. A further2394
superseded compiler files totaling249424115 bytes were removed with zero input
overlap, preserving268 archives/executables/caches/Testing artifacts and
excluding the current27.0, Debug and sanitizer trees.
`superseded-compiler-cleanup.json` SHA256 is
`bd094433e5f19fcd46092fa3cef376d256c84e0898c9030a27eab3847b0f02be`.
Available space is89.24GiB at the recorded final check. Original sources and
required private evidence are not treated as disposable outputs.

The freshly fetched `origin/main` remains the verified OE49 merge1ae2f21.
Independent review identified a portability issue in the Python source-guard
helper: `O_NOFOLLOW` is unavailable on some platforms where its CTest is
registered. Commit67ba4b9 changes only the Python package helper and its tests.
The final helper selection is **36/36 PASS**,0.049s: it actually removes the
`O_NOFOLLOW` attribute, checks literal CRLF bytes and rejects a swapped file
identity before a guarded read. This is mocked missing-flag coverage; an actual
Windows runtime remains NOT_RUN. The final-source complete Release re-execution
is **292/292 PASS**,82.55s (JUnit82s), zero failures/skips/disabled cases, recorded
in `ctest-release-final.log` and `release-final-junit.xml`. Existing C++/bundle/
native evidence remains unchanged; no fresh C++ build is claimed.
`final-source-equivalence.json`, SHA256
`a7ba781c84ee00a5f4c52d873e078df554a7f4cba27ce4f61abebf0880883f59`,
verifies the same13 archives and unchanged compiled sources/CMake/resources.
Of the earlier540 bindings,539 remain identical; one documented Python test
module changed. The earlier history binding remains truthful for its own run.
The final supplement `final-gates-after-portability.json`, SHA256
`fddcf6b569c8ac3f29b9100170926784119d0c3517a4f809411dd1415a72e39a`,
binds the final-source results without overwriting earlier evidence. The report
is ready for independent review on `test/oe50-native-playtest`.
The separate [GitHub PR #30](https://github.com/oliverjessner/OpenEmperor/pull/30)
was created from the initial push `92edf442a295c0c44a64870c81faa8d75fcca197`.
Its post-merge metadata records the actual merge SHA and verified new
`origin/main` after those actions succeed; this source document is committed
before merge and does not invent a future SHA. No tag, app release or public
deployment is part of OE50.

## Evidence and reproduction rules

Use the [testing guide](../testing.md) for complete current Release and relevant
Debug/sanitizer checks. Record the registered inventory, individual skips,
JUnit and exact command. A real-data test returning its skip code is NOT_RUN,
not original-data PASS. Check historical complete bytes against the preserved
corpus and restore an earned Rule18 state with actual active cargo/reservations
in a second process. The fresh current result must be distinguished from the
[OE49](oe49-player-mode.md) 292-test/menu/history/golden proof.

For each scripted UI purity check, compare the complete saved authority before
and after the action. Record images actually opened at1280×800,1600×900 and
1920×1080 and at1×/2×/4×. Keep zoom, selecting a tool and a planned trip distinct
from paid construction, simulation and real arrivals.

For native evidence, record actual Cocoa/Metal and the window/client/pixel sizes,
density/display scale and event window ID. Optional
`OPENEMPEROR_INPUT_DIAGNOSTICS=1` captures bounded application diagnostics;
separate stdout JSONL from platform stderr. Only actually delivered matching
Down/Up plus the visible/paid effect supports a pointer PASS. OS automation
requests alone do not. If delivery fails, record
**BLOCKED_AUTOMATION_DELIVERY**, preserve the request/event evidence and leave
the application action NOT_RUN. Do not add an offset or DPI transform from that
failure. Stop desktop automation immediately at user takeover.

The [human checklist](oe50-human-checklist.md) provides direct CMake/application
commands and isolated working-copy instructions; no private QA launcher is
required. It covers a fresh paid city, real arrivals, one or two upgrades,
operating finance, a second map and whole-application restart. Human observations
must identify the actual build and action before any row is changed to PASS.

## Five existing follow-up priorities

These are measured limitations or acceptance gaps, not five invented new bugs.
The current OE50 native-delivery finding is recorded directly below; historical
long-window measurements retain their original scope.

1. **P1 — Current native pointer and human acceptance remain incomplete.**
   OE50 observes both native Down/Up at`(0,0)` after the requested visible
   button target, with no action. This is BLOCKED_AUTOMATION_DELIVERY; native
   paid pointer construction, drag, focus/resize/modal and picking remain
   NOT_RUN. It is not an application coordinate-bug proof. The separate
   [OE43](oe43-native-playtest.md) failed-delivery/takeover observations remain
   historical. Human acceptance is still NOT_RUN.
2. **P1 — Distribution acceptance is open.** Existing guides describe ad-hoc local
   candidates. Current Finder/Gatekeeper, independent clean-Mac and human
   acceptance are not established. The GPL-3.0-only LICENSE now exists; the old
   license-absent finding is not repeated. A dependency-linked development app,
   relocated package and publicly accepted distribution are separate results.
3. **P2 — The measured Rule18 city still has residential fires.** The
   [OE48](../original-economy/oe48-citywide-fire-reliability.md) plan, reproduced
   by OE49, holds13 Houses and5200/5200 current needs in its finite window but
   records5 House starts,2915 burning-House ticks and65 all-building starts.
   Positive+5180 operating Funds and target Elite17 retention do not establish
   fire-free or indefinitely stable play.
4. **P2 — The four-House starter needs active expansion.** The
   [player guide](../original-economy/player-handoff.md) records Food misses and
   losses in the unchanged starter's measured long run. It is not the earned
   OE48 city. The human test must record confusing empty-stock/build paths,
   actual deliveries, one or two upgrades and tax minus upkeep separately from
   remaining Funds. No current human onboarding result is claimed.
5. **P3 — Original visual/terrain/system parity remains incomplete.**
   [Known issues](../../KNOWN_ISSUES.md) retain animation/fallback, anchor,
   interpenetration, height/ramp and original venue/performer gaps. Inspect the
   actual current pixels and record source/zoom; do not infer parity from a
   matching model label or a synthetic render. The historical Market7 overlap
   was fixed in OE42 and is not reported as a fresh defect without reproduction.

Original Emperor saves/campaigns, shipping/Ferry and original venue/performer
flows remain unsupported. This QA does not allocate a new rule/schema or grant
paid demolition in original-economy cities. Any new P0/data corruption or failed
mandatory integrity gate stops unsafe product merging. An honest QA-only
PARTIAL PR can retain HUMAN_NOT_RUN; it does not authorize an app release,
tag, notarization or public readiness claim.

## Dated follow-up — 2026-11-09, OE58 user account

The [OE58 human playtest record](oe58-human-playtest.md) adds the first concrete
user feedback: Rule18/Xia four-House paid startup at tick0/Funds10596, reported
Road/fifth-House purchases to Funds10494, working inspection and a tent graphic
after actual resident entry. These are **USER_REPORTED_PASS** within the stated
scope. The user also reports weak Vacant2 visibility and a clipped paid-starter
refusal on Loulan. No screenshot file accompanies that account; no fresh
**HUMAN_OBSERVED** image or session is claimed by the documentation author.

This is a later observation record, not a change to the OE50 candidate, evidence
matrix or historical **NOT_RUN** rows above. The broad statement that the other
checklist points work is positive feedback, not individual external-display,
DPI, Recovery, multi-hour, clean-Mac or historical-generation acceptance.
OE58's new presentation/menu work needs its own technical and native results.

Later on the same recorded date, OE58's paid House transition, scripted
Loulan/Xia menu controls and opened software-SDL capture matrices pass within
the [new result ledger](oe58-human-playtest.md#oe58-result-ledger). These are
**AUTOMATED_PASS**, not a retroactive human result for this OE50 candidate.
