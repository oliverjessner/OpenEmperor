# Safe demolition in Original economy

OE51 adds a conservative authored removal contract to a separately selected
**Rule19 / schema35 / policy3**. The baseline is the verified OE50 merge
`a6b3276a0a40180cb7c4b283225e619a492f565c`, branch
`feat/oe51-safe-demolition`. This is independent OpenEmperor behavior;
original Emperor demolition fees, refunds, eviction and cancellation semantics
are not established by this implementation.

Completed evidence and remaining gates are separated below. The paid World,
replay and scripted SDL results do not establish native or human acceptance,
or original Emperor demolition parity.

## Version and authority

**Advanced / Legacy → V Safe demolition economy**, or the standalone
`--original-safe-demolition` option, creates Rule19 explicitly. **Play Original
economy** continues to select Rule18. Historical O/F/S/M/W/I/D/T/U shortcuts,
City-v11 rule3 and ordinary City-v16 rule4 defaults retain their contracts.
Rules1–18 and schemas1–34 have no automatic migration or expanded removal rights.
The [historical compatibility table](../gameplay-sandbox.md#legacy-profiles-and-save-compatibility)
and [source-bound map permissions](../map-permissions-compatibility.md) remain
mandatory; selecting Rule19 grants no new terrain or original-object rights.

Schema35 requires `rules.safe_demolition: "authored-safe-demolition-v1"`, all
inherited Rule18 markers and the bounded
`world.original_production.safe_demolition.retired` collection. Each closed row
retains its paid building ID/kind, origin and purchase/removal ticks; a retired
Clay Source also retains its never-reused courier ID, extraction total and
exact latest completed trip when it produced. Exact Rule19 create/restore
entry points precede the exact Rule18 routes;
inherited simulation capabilities do not select a save generation. Policy3,
complete map/model SHA binding, difficulty and development revision27 remain
required. A header/marker rewrite is not a valid conversion.

`World::demolition_status`, `validate` and `execute` own the decision. The UI
displays their derived reason and issues one typed command after confirmation.
It does not maintain a second kind whitelist, zero inventory, evict residents,
grant protection or create a hypothetical replacement World. A refused command
leaves ticks, Funds, IDs, stock, cargo and canonical state unchanged.

## Permission matrix

These are the contract boundaries; the validation record states their tested scope.
Every allowed case still requires exact paid ownership, accounting and history.

| Target/state | Rule19 V1 decision |
| --- | --- |
| Never-admitted Common2×2 House, all owner/needs/stock/arrival/tax records blank, no active or retained references | Eligible for no-refund removal |
| Occupied House or a House with a previous admission | Refuse; no resident/workforce deletion procedure is enabled |
| House with Food/goods/reservations, incoming/returning visits, tax/consumption/arrival history or a retained cursor/witness | Refuse with the actual first blocking reason |
| Empty Elite plot2×2 or materialized Elite4×4 | Refuse; no Elite retirement or footprint shrink |
| Clay Source, paused, progress0, all physical stocks/reservations0, its empty courier fully home, supported completed history settled | Eligible; preserve extraction and the exact completed Clay trip in retirement provenance |
| Clay Source still operating, partly extracted, stocked, on fire, delivering, returning or waiting on a road | Refuse; pause alone is insufficient |
| Another actor occupies the candidate footprint, or a typed goods/Food/service/fire/water/history row refers to it | Refuse until the contract can prove safe closure; history is not silently erased |
| Pottery, Warehouse, Market, including an empty one | Refuse; broader typed owner/recipe/transfer retirement is not implemented |
| Food/Hemp Farm and attached field; plant/source attachment | Refuse; preserve parent/child ownership atomically |
| Logging/Bronze/Bronzeware/Lacquer/Paper/Weaver/Tea/Silk production and plants | Refuse; no stock/cargo cancellation or dependent deletion |
| Service/Water/Culture/Religion provider or FireWatch, idle or active | Refuse; retain real trips, arrivals, protection and witnesses |
| Existing paid neutral typed Well72 influence source | Retain its historical separate removal path and removed-source history; it is not new productive retirement |
| Original/protected map body, source graphics, ground or walker selection | No paid building authority is granted by inspection or appearance |
| Mutable Road | Existing separate RemoveRoad rules apply; no new removal permission or bypass |
| Fixed road, protected bridge/gate, whole Bridge or Ramp | Existing infrastructure refusals remain; the building dialog does not delete spans/links |
| Missing ID, inconsistent ownership, exhausted counters, full retirement journal | Refuse atomically |
| Rebuild on illegal/occupied land or without enough Funds | Normal placement refusal; demolition grants no money or terrain permission |

Removal has **no refund** and advances no simulation tick. It removes exactly
the permitted2×2 footprint, closes the paid identity and refreshes topology once.
No building/courier ID is reused. Rebuilding uses the normal command, permission
check and full price. The retirement journal has **eight entries**; existing
lifetime building/courier budgets remain in force:82 live plus retired building
purchases and58 live plus retired courier identities. It is closed provenance,
not physical inventory, residents, a transport source or a restoration loophole.
Already incurred installed upkeep remains accounted for after a producer is
removed; that retired producer incurs no later invoices. Its purchase cost and
completed extraction/delivery history remain conserved.

Only a never-admitted House is removable. Its residents and supplied workforce
are already zero; a0→0 check verifies that bounded case and does **not** prove
occupied-House eviction. Occupied/previously admitted removal remains unsupported.
Pausing a Clay Source already releases its effective staffing before removal;
removal must not manufacture a further population or salary gain.

## Player path and cancellation

Start with a new isolated application root and your own read-only game data:

```sh
./build/openemperor --data /path/to/Emperor --app-root /path/to/new-oe51-app
```

Choose **Advanced / Legacy → V Safe demolition economy**, check the selected
map/difficulty and choose paid starter or empty city. The direct paid-starter
equivalent is:

```sh
./build/openemperor --data /path/to/Emperor --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-safe-demolition --sandbox-save /path/to/new-oe51-save.json
```

Use **5 Select** on a paid House or Clay Source. The shared Inspector footer
shows **Demolish** and its real reason. A blocked click displays that reason
without opening a confirmation. For an allowed target, **Demolish** opens a
modal: **Demolish (Enter)** executes once, **Cancel (Esc)** cancels. Focus loss,
resize, window hiding and closing also cancel. Pending confirmation blocks ticks,
map commands and Recovery capture. Press/release must match the visible button;
releasing a previous press over the map cannot confirm or build through it.
These paths retain the shared [input and inspector contract](../input-reliability.md#inspector-layout-and-scrolling).

The original-House tabs and section document retain their own scrolling. Only
Rule19 reserves the existing shared footer geometry for the demolition button
and independently scrollable refusal text. Rules10–18 keep their old House
layout. The Advanced menu fits its new choice within the existing minimum
900×600 aperture, including a retained city's Continue control.

For a producer, pause at a real settled production boundary, let its existing
loaded trip physically arrive and return, then inspect the live reason again.
Do not edit stock or save data to create eligibility. After confirmation, verify
all four cells are free and build again with the normal tool. Save using F5,
quit and load the new OpenEmperor save; F9 restores the saved generation paused.
Use new output paths or verified working copies, never overwrite historical
proof saves or original Emperor saves.

For a separate-process reload, first make a new working copy and select its
recorded generation directly, without adding a new-game generation flag:

```sh
cp -n /path/to/new-oe51-save.json /path/to/new-oe51-working-copy.json
./build/openemperor --data /path/to/Emperor --load-sandbox /path/to/new-oe51-working-copy.json
```

The `--original-safe-demolition` flag is a new-city choice and cannot be combined
with another Original-economy opt-in or `--load-sandbox`. Loading uses the save's
exact generation; it does not convert a Rule18 save.

## Validation record

The final source is `0926c96dd169d9209f58904fa5385248c54e6b4e`, following
implementation `bd4b81f` and negative-control tests `323e837` on the verified
OE50 baseline above.
Validation uses macOS27.0.1 / arm64, SDK27.0, AppleClang21.0.0, CMake4.4.3 and
SDL3.4.14. Release reuses the explicit27.0 build at
`.local/oe50/package-build-27-01`; Debug and ASan/UBSan use the retained isolated
`.local/oe49/debug` and `sanitize` builds. This is local validation, not a new
signed/notarized distribution or a Windows test. The final Acceptance executable
SHA256 is `cb4f65bed9957b806bbaccd2683581ec950a4bcceca2f9a69dafd53077ce7461`.
The actual Xia map SHA256 is
`871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433`;
the34218-byte original model SHA256 is
`7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`
at Normal difficulty. Full map/model identities remain in every save.

Final Release is **296/296 PASS in191.71s**, with no failures, skips or disabled
tests (`.local/oe51/ctest-release-lifetimes-final.log` and
`release-junit-lifetimes-final.xml`). Sequential ASan/UBSan is **12/12 PASS
in116.49s**, with no sanitizer diagnostics (`ctest-sanitize-lifetimes-final.log`
and `sanitize-junit-lifetimes-final.xml`). macOS leak detection is disabled
(`detect_leaks=0`); the external SDL library is not instrumented.

The broader final Debug invocation is **11/12 PASS in93.45s**, not12/12:
`original-menu-successor-flow` failed its unchanged five-second map-preflight
check after17.58s while eight compiler jobs were active. The isolated recheck
then passed **1/1 in116.06s**, without increasing that budget. Concurrent load
is recorded context, not a proven cause. Both results remain in
`ctest-debug-lifetimes-final.log` / `debug-junit-lifetimes-final.xml` and
`ctest-debug-menu-isolated-final.log` / `debug-menu-isolated-final.xml`.
All four new Rule19 World/save/menu/view cases passed in the broader Debug run;
the save executable also passed directly. Earlier failed fixture logs remain
preserved; private fixture roots now use exclusive directory creation.

The final save tests pass in Release, Debug and ASan/UBSan. They reject hostile
retired-purchase chronology and overlapping lifetime geometry against live or
retired purchases, closed neutral sources and the known purchase times of
Bridge banks/water cells and Ramp endpoints. Same-tick retirement/rebuild
remains legal in purchase-ID order. A real Elite extension is blocked until
the obstructing House is removed at119 and materializes at120; forging removal
at120 is refused because the extension precedes commands at that tick.
INT_MIN/INT_MAX origins are rejected before footprint-offset arithmetic.
Snapshot, restore and write refusals preserve the source save bytes. These
additional lifetime checks apply only to exact Rule19.

The retained real Xia/Normal Rule19 creator and independent replay both reach
tick26086.
They start from the explicit new factory; no Rule18 save is relabeled. The paid
journey begins at5396, retires empty House IDs67/68, then earns one Clay unit
from source69/courier47: departure5437, actual arrival5561 and complete physical
return/removal5686. The same-cell Clay rebuild gets body70/courier48 and is
paused and retired before producing. All four removals preserve monotone IDs;
the four normal purchases cost80+80+120+120=400, with no refund. The original66
live building IDs and46 live couriers remain intact after the journey.

Funds change5391→4991, construction5347→5747; tax2198 and upkeep3460 do not
change over this particular290-tick journey. Physical extraction increases by
exactly one, and the destination retains its real delivery. Population320→327
comes from ordinary ticks in the untouched city. This is not an eviction test:
the two removed Houses each had zero residents before and after removal.
Installed workforce remains349 before/after the complete journey; individual
Clay removal/rebuild closes/restores its four installed workers without granting
residents or salary credit. A separate paid fixture removes Clay at900 after
its actual816-tick invoice and verifies that closed charge persists.

The subsequent window is exactly5686→26086 (20400 ticks), with13 purchased House
containers. All13 retain positive occupants throughout the20400 recorded
post-tick states. The raw `minimum_inhabited_houses:11` is the separate goal
counter: it excludes burning Houses as well as empty ones. Its first minimum
occurs at23600 and does not indicate vacancy. Population stays at least327.
The measured target remains type17 for all
20400 post-tick states and passes400/400 actual current-needs evaluations, with
no target devolution or burning tick. The city window has zero current unmet
evaluations, zero new missed Food demands and zero devolutions; next-tier
requirements remain separate. City operating cashflow is+5180
(22630 tax−17450 installed upkeep), minimum Funds4991 and final Funds10171.
This finite result does not establish unlimited stability or broader demolition.

The read-only occupancy audit reconstructs complete House state changes from
the retained timeline, checks all five later complete checkpoints, and verifies
that the25 between-tick operating-control events leave every House state
unchanged. All400 complete evaluation groups separately contain13 positive
occupant inputs (5200 rows, minimum individual input22). It preserves the raw
goal metric and original paid audit unchanged; evidence is
`.local/oe51/paid-rule19-occupancy-audit.json`, SHA256
`f28e0c9173dc19ebaee57fffe4c35cfc45a8aac72c644a642d9b1f1a5ab5ff05`.

All eight creator/replay checkpoint SaveDocuments and complete snapshots compare
exactly, including the demolition-complete, midpoint and result states. Recovery
also preserves exact schema35, journal and complete result bytes. The private
sources are `.local/original-economy/oe51/create-02/` and `replay-02/`; the earlier
`create-01` stops honestly at5396 without retirement because its bounded spare
placement search finds no legal site. Its output remains PARTIAL evidence.

The final-source Rule19 creator and independent replay both pass at26086.
All37 creator and61 replay top-level reference files compare literally,
including eight checkpoints,48 complete document copies, three snapshot forms
per checkpoint and four Recovery payloads. Removed older replay duplicates
resolve to verified retained Creator masters through the cleanup manifest.
Final outputs are `create-final-01/` and `replay-final-01/` under
`.local/original-economy/oe51/`. The final audit is
`.local/oe51/paid-rule19-final-audit.json`, SHA256
`70a1b1b424388bc020a46bb6d3bdcb2bc21d33b609cd9078155d1215a03feb5b`.

The final-source Rule18 creator and independent replay separately pass at25795:
all93 top-level reference files and four complete Recovery payloads are literal
matches to the protected OE49 reference. Seven complete checkpoints retain the
5395→25795 window,5200/5200 current evaluations,+5180 operating Funds,
minimum5391, five House fires/2915 burning ticks and zero window Food misses or
devolutions. Only the recorded Recovery history ID, filename and wall-clock
metadata differ. The final audit is `.local/oe51/rule18-final-audit.json`, SHA256
`629f231c861704bdedb0a6ee9eb3f7556e1b6a771033b0a22a486d049706bb5c`.
These final runs omit optional per-tick digests; an earlier extra-digest replay
was deliberately interrupted and is not a Replay PASS. Both final audits verify
all1755 protected inputs,1464 original files,99 historical outputs,661 source
bindings and the executable/14 library identities unchanged across their runs.

The separate-process final save fixture passes at exact Rule19/schema35:
creator and restored tick1200 documents are byte-identical at57897 bytes,
SHA256 `8a7f3143281a12d719bbba3dad9062e2ae560dc020ff18fdc28e7eb3ac00914c`.
The restored process continues normally to1303 and writes59055 bytes, SHA256
`2e599dcbd28da37820012fb836e1f88986936a070a2276b3a955e86b531d0c5c`.
The binding report is `.local/oe51/independent-final-audit.json`, SHA256
`9c4d4ea406d311c3493f5df2253ba067a001358c5411eb111183684691b248b4`.

The current simulation/persistence archives reproduce all132 historical canonical
payloads and case indexes across schemas1–34 literally. The comparison report is
`.local/oe51/historical-final-02/comparison.json`, SHA256
`cfbef7d7f932e247730e3e3878fc6b2b41293e4fecebf751d25fe8c5e2a7c31e`.
The fresh [core comparison](../original-economy/core-parity.md) separately retains
all74/74 numerical vectors and every historical expected/observed result; only
reviewed API/source fingerprints change for the new optional journal. Its report
is `.local/original-economy/oe51/core-parity-release-final-02/report.json`, SHA256
`e2d1916ee44e5ce1d8863c0ba03c798181fefc7147530da2c5a6e1faa13acd8e`.

Both final focused SDL executables pass with fresh capture set08:27 view PNGs and5
menu PNGs were individually opened. The complete2×2 House outline fits at
1280×720,1600×900 and1920×1080 at1×/2×/4× after ordinary paused camera panning.
The shared footer, no-refund hint and confirmation controls stay separated.
At900×600, the complete Rule19 setup description and action controls fit;
the existing long bottom diagnostic/status line is clipped or bounded.

These captures render into an owned same-output-size RGBA32 target and read
before Present, with unchanged complete World snapshot and camera/viewport.
Earlier capture01/02 readback after Present is invalid acceptance evidence.
The final rebuilt Clay frame independently records16 road,6 building and2
existing idle-courier submissions. The six bodies include3 separately counted
authored House fallbacks. A read-only PNG-color audit confirms their encoded
pixels. The image tool displayed differing versions of the byte-identical
rebuilt PNG, so that viewer discrepancy is retained in the private report;
it is not an established application rendering defect. The bright green ground
and flat diagnostic bodies are independently authored fixtures, not original
art or scene-composition parity. Exact32-image hashes, per-image findings and
capture diagnostics are bound by `.local/oe51/visual-review-08.json`, SHA256
`95516613ba107585a66dff8e2fb34b73a85366dceb39c917ab24dd487e866502`.
All14 archives, the Acceptance executable and both capture-test executables
retain their complete pre/post identity through this capture and visual review.

| Evidence layer | Current OE51 status |
| --- | --- |
| STATIC / contract and source inspection | PASS within the conservative scope; independent authority, restore, paid-evidence and fixture-ownership reviews |
| PAID_WORLD | PASS within the recorded paid journey and focused authority fixtures; no occupied-House eviction or broader retirement claim |
| SCRIPTED_SDL | PASS — final menu/view executions, real Select/dialog/cancellation/refusal/rebuild and F5/F9; capture set08 |
| SAVE_RESTORE / SEPARATE_PROCESS / Recovery | PASS — final eight complete creator/replay checkpoints, four Recovery payloads, separate-process1200→1303 fixture and hostile save tests |
| HISTORICAL_COMPAT |132 canonical historical payloads/indexes,74/74 core vectors and complete final-source Rule18 creator/replay PASS |
| BUILD_TESTS | Release296/296 and ASan/UBSan12/12 PASS; broader Debug11/12 with the failed legacy preflight passing an unchanged-budget isolated1/1 recheck; all four new cases PASS |
| GRAPHICS |32/32 final08 PNGs opened; scoped authored UI/footprint review complete, with the documented image-tool discrepancy and minimum-menu diagnostic elision |
| New-generation paid20400-tick scene | PASS — final-source Rule19 creator and independent replay reproduce the paid journey, all37/61 raw reference files and full retention window |
| NATIVE_GUI / HUMAN | NOT_RUN for OE51 |
| ORIGINAL_PARITY | No new parity claim; authored safe subset only |
| RELEASE | NOT_RUN; no tag, release or distribution authorization added |
| GIT_DELIVERY | Actual branch, pushed commits, GitHub merge and fetched-main verification are recorded in [PR31](https://github.com/oliverjessner/OpenEmperor/pull/31) after each action |

The focused test sources are `OriginalSafeDemolitionMenuTests.cpp` and
`OriginalSafeDemolitionViewTests.cpp`; World and save tests cover the authority
and persistence boundaries separately. The maintained [Rule19 testing procedure](../testing.md#rule19-safe-demolition)
also describes separate-process save tests. Reproducible targeted commands,
once the selected build is complete:

```sh
ctest --test-dir build --output-on-failure -R 'original-safe-demolition'
./build/openemperor-original-safe-demolition-menu-tests --capture /path/to/new-menu-captures
./build/openemperor-original-safe-demolition-view-tests --capture /path/to/new-view-captures
```

Private saves, screenshots, manifests and command logs remain under ignored
`.local/oe51/` and `.local/original-economy/oe51/`; the explicitly supplied
original installation stays read-only at its own path. The historical Rule18 reference remains
the [OE48 assessment](../original-economy/oe48-citywide-fire-reliability.md): its
five real House fires and finite20400-tick scope are not a Rule19 demolition
result or proof of unlimited stability. Native pointer delivery and human play
must be observed independently; earlier OE50 keyboard/persistence evidence is
not new OE51 acceptance.
