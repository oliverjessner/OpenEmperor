# OE52: source-registered static Original building bases

Baseline is the verified OE51 merge/main
`cc53e7037dac4ebd4da6b68d382a6e898890716b`. Work is on
`feat/oe52-original-visuals`, in an isolated worktree; the user's original
checkout and its uncommitted changes are preserved. Delivery and the actual
merge/main SHA are recorded in [PR32](https://github.com/oliverjessner/OpenEmperor/pull/32)
after those actions occur.
No tags or releases are created.

## Scope and evidence

This is a presentation-only addition for explicitly active Original production.
World, rules, schemas1–35, model arithmetic, costs, goods, movement and saves are
unchanged. No original executable is run. Only user-supplied source files are
read; no original pixels, binaries, saves or private diagnostics enter Git or
application resources.

The complete paid Rule18 city contains 66 building bodies and 46 courier records.
Existing Clay/Pottery/Warehouse/Market/FireWatch previews and materialized Elite
House representatives already supplied original pixels. Well72 was a blue
marker; Acupuncture208 used the historical one-cell ServicePost sign despite
its actual 2×2 footprint; the selected culture, religion, Paper and Weaver
bodies used colored markers. Ten representative gaps are addressed:

| World/model identity | General group key | Physical record | Actual side | Whole image | Geometric anchor |
| --- | --- | ---: | ---: | --- | --- |
| Water Well72 | `0x482` |1559|2|158×132|79,112|
| Acupuncture208 | `0x485` |1593|2|158×137|79,117|
| Music School211 | `0x412` |589|2|158×137|79,117|
| Acrobat School212 | `0x410` |460|3|238×162|119,142|
| Drama School213 | `0x411` |501|3|238×176|119,156|
| Daoist Shrine215 | `0x49b` |2245|2|158×134|79,114|
| Buddhist Shrine217 | `0x49c` |2258|2|158×142|79,122|
| Confucian Academy219 | `0x49f` |2309|3|238×201|119,181|
| Paper Maker45 | `0x44a` |2811|2|158×102|79,82|
| Weaver47 | `0x446` |2768|2|158×119|79,99|

The [maintained reverse contract](../reverse/original-presentation-correction.md#original-production-static-building-bases)
separates pinned EXE registration observations, shared General-slot2 resolution,
SG3 geometry and decoded pixel inspection. Model names or visual motifs alone
are not the registration proof. Fresh independent shared-API resolution agrees
with all ten metadata rows; all ten full decoded source images were opened.

Source SHA-256 identities:

- EXE: `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
- Model: `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`.
- General.sg3: `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`.
- General.555: `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`.
- Xia: `871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433`.

The static-candidate source audit is private
`.local/oe52/source-audit-01/static-candidates-final10.json`, SHA-256
`82237361815fe9d8b3bb32da4d550492baaf461118a9e0ad619f1a8239bc3edf`.
Nine bases have positive animation metadata. No frame association, composition
or original cadence is inferred from those fields. These are complete static
representatives with authored front-cell geometric anchors, rather than a
recovered original pivot or animation implementation.

## Production integration and fallbacks

The optional `original-buildings.json` is metadata only, declared independently
in the existing exact six-file compatibility pack and included in CMake's
explicit application-resource list. Both General source hashes must match
before all ten rows can activate atomically. Existing RuntimeArchiveLayout,
ResourceGroupLookup and SG3/555 readers resolve and decode the images. Bounded
JSON/containment/SHA helpers are shared with the old House loader; its previous
schema, strict keys, limits, diagnostics and static-House filter remain exact.

The historical 13 core roles and 15 additional House images remain unchanged.
Nine additional physical images are needed because Well1559 already exists in
the core pool: 36 combined building textures, not one texture per placement.
The ten bases occupy 1,083,824 decoded RGBA bytes; incremental bytes are 1,000,400.
The existing 64 MiB combined budget is unchanged. Preparation and upload are eager;
selection, draw, preview, roof attachment, culling and alpha picking reuse the
same prepared entry and existing front-cell painter order.

An explicit custom core suppresses both automatic supplements. Ordinary City-v16
keeps its historical one-cell ServicePost role. Missing files, wrong source
hashes, unsafe paths, bad registration/frame/geometry, unsupported mirror or
payloads, decode failures and exhausted budgets retain the historical display.
A failed optional replacement retains the complete working core/House/new pool;
a failed first append uploads nothing. A valid first append reuses 27 prepared
textures and adds exactly nine. F4 disables configured building artwork and
uses the historical all-art-off path for core, House and new artwork. The
separate no-new-tail control retains the core/House art and Acupuncture's old
2046 source-pixel sign. A missing new assignment can therefore use an existing
source-pixel substitution as its historical fallback.

No graphics hit grants construction, transport, occupancy, service protection,
goods or save authority. No new saved presentation state is introduced.

## Verification record

The production C++ rendering/asset implementation is commit `cbf9673`; resource
packaging and paid View coverage are committed in `13e303a` and `cbe7aa1`.
`cebf40a` only qualifies generic body-export labels and leaves production and
test assertions unchanged. The first complete Release run at `cbe7aa1` passes
300/300 in 381.94s. The **final complete Release run at `cebf40a` passes 300/300
in 338.21s**, with zero failures, skips, disabled tests or final-build warnings.
Initial smoke checks caught the missing explicit CMake resource and a legacy
control without its data root; both were corrected before the complete runs.
The final Release paid View executable SHA-256 is
`97515770929c3cb5427bc45e3994d333e725839844bf7f6ee2d0817ab9da8502`.
Final JUnit SHA-256 is
`f7bff252f8b4495a08faf51626543a255b8d777822ca0d390a6c255bde7c0b8d`.

Focused Debug and ASan/UBSan both pass **15/15**, respectively 182.814s and 82.055s,
with zero failures, skips, disabled cases, compiler warnings or sanitizer
findings. Their report is `.local/oe52/focused-validation-02/report.json`, SHA-256
`4b3e2906e615bb3450d3eed9b9c7f81fbf071ae49516e429d34fff2ec2ee20d3`.
The first private runner stopped before any test/build because CTest omitted
commands for not-yet-built executables; the repaired fresh runner preserves
that failed harness log and executes every named case. This was a harness
failure, rather than a passing/skipped product test.

After the QA-label-only commit, the changed paid View test passes again in Debug
and ASan/UBSan, 89.091s and 42.748s respectively, with warning-free single-test
builds. The other ten focused executables per configuration, production app and
all retained production libraries remain unchanged. The final-label supplement
is `.local/oe52/final-diagnostic-label-validation-01/report.json`, SHA-256
`10e85fcfd632647e936c2338bc2b07fb44eedaaac0f063dd4e7a550fb756159b`.
Each configuration's 29 complete PNGs and 90 geometry rows remain literal-equal
to its prior passed run; only 24 qualified labels and the scope field change.

The Release environment is macOS 27.0.1 arm64, Apple Clang 21, SDK 27.0, CMake 4.4.3 and
installed SDL 3.4.14. The reused isolated Release build retains its explicitly
selected macOS 27 minimum; this task does not change the repository default or
claim a newly distributable package. Focused Debug and ASan/UBSan use their
existing isolated configurations, with external SDL uninstrumented and
LeakSanitizer disabled on macOS.

The paid View test restores the complete earned Rule18/schema34/policy3 Xia
checkpoint at 5395 into the actual SandboxView. It performs 90 focused category
measurements at 1280×800, 1600×900 and 1920×1080, each at 1×/2×/4×; records per-render
counter deltas; exports all 66 body and 46 actor diagnoses; verifies ten distinct
opaque/transparent alpha-picking witnesses and F4 comparisons. It captures nine
overviews and ten Original/F4 pairs. Readback uses a separate owned RGBA32 target
before Present and verifies unchanged full World/camera/viewport state.
Captures and review metadata remain private under `.local/oe52/`.

All 29 PNGs from that complete Release run were opened and reviewed. The independent image review
is `.local/oe52/visual-review-final-01.json`, SHA-256
`6d752decc3616a9587d74b77d706bb54473de9f6658b6eee86448c6027a584e0`.

The final Release report is
`.local/oe52/ctest-Release/view-1473734040802708-0/report.json`, SHA-256
`cf38dc4cf06f309e7ae06cf8d31c70cf689d6a9bf7670d9269782effd1e3f91b`.
All 29 final PNGs are byte-equal to those actually opened, with identical 90 matrix
rows, 109 capture records, 46 actors and 10 alpha witnesses. The explicit final
capture equivalence proof is `.local/oe52/final-capture-equivalence-01.json`,
SHA-256 `965cec386a5eb7e4dbfac46d396e3845ffc5207100daa1b54b7448e7df879a10`.
The final 66-body source/pixel audit with the qualified raw rows is
`.local/oe52/complete-body-routing-audit-final-02.json`, SHA-256
`2a23a866a0214d9e40092af41dd34cfa6ba81a5fd6892489eaa7684cdcc45977`.
The ten visible category pairs have distinct source-pixel and transparent-hole
witnesses; the nine overview images show ordinary aperture clipping at 4×.
Across 99 art-enabled readbacks, the new counters record 1,683 submissions for 17
paid bodies. Submission counters include off-aperture calls; they do not count
unique visible objects. The 109 total readbacks also include ten F4-off frames.

There are 25 unassigned, seven Supplier, six Distributor, two Inspector, two Clay,
two Pottery, one HealthWorker and one Service courier records. The frozen city
has 12 outward, 21 returning and 13 idle couriers. These are complete actor records,
not a claim that all 46 were visible simultaneously. Existing source-pixel walker
submissions across the measured frames are Supplier161, Distributor117,
Inspector53, Pottery84, Clay44 and HealthWorker21; Service has no sprite submission
in this paused witness. No new Walker artwork is claimed. All 12 instrumented
counters remain zero over 100 pure prepared frames, including World copies,
commands/ticks, file reads/writes, decodes/uploads, BFS and route refreshes.

The generic body-export lookup needs a separate production-specific audit for
four existing InfluenceSource bodies. The production selector maps those to the
curated Well image; its old generic export label must not be read as a fallback
claim. The reviewed routing is17 newly mapped bodies,13 House bodies,16 existing
core bodies and20 unassigned bodies. Source-derived audit corrections remain
separate from raw runtime records and do not establish an original identity for
the neutral InfluenceSource.

The complete source/pixel routing audit with core AssetIds is
`.local/oe52/complete-body-routing-audit-final-01.json`, SHA-256
`ffea2001d498dd1bde03b06ec5ab06304c860018f4e2f287319a7c3841f211f5`.
A separate no-new-tail control keeps all 27 core/House textures active and uses
the exact same paid 5395 World. Its11 additional PNGs were all opened. They show
the old2046 ServicePost sign, blue Well72 marker and old category markers while
preserving the existing Influence-Well, House/core and Walker artwork. This is
a controlled final-renderer comparison of preserved historical paths, rather
than a claim to have executed an old binary. Full F5/F9 bytes and 100 pure-frame
counter checks pass. The control review is
`.local/oe52/baseline-control-01/visual-review.json`, SHA-256
`4411600417cae3f225654b9299231ec7eb6d70cd7e769687391838a1b0b5c123`;
42 build/library/flags artifacts and550 source files are unchanged across the
private helper compilation/execution and the complete 2,075-input guard passes.

Full independent historical comparison restores and re-encodes 132 canonical
and case-index files for schemas1–34, all literal-equal to the frozen corpus.
`.local/oe52/historical-final-01/comparison.json` SHA-256 is
`9cdf68afe5b7a157940dc691881abb383777ea1237793c45715d5795a478955f`.
The Rule18 creator and independent replay use the final Acceptance executable
`db625993e7e95c60a2808cda6665211912cd87c43daaff802770bf32ab3b5b32`.
All 93 top-level comparison files and four Recovery payloads match final OE51
literally, including seven complete SaveDocument/snapshot checkpoints. The
5395→25795 window remains 20,400 ticks, 5200/5200 current needs, five House fires,
2,915 burning House ticks, zero window Food misses/devolutions, operating +5,180
and minimum Funds 5,391. Thirteen purchased House records and minimum 11 nonburning
inhabited Houses remain distinct measures. Optional tick-digest tracing was not
requested for this repeat. The audit SHA-256 is
`ce71f29f4665987eb7f8ea871e1fafbb9b0944561392b6ef4ebe566dcfa6272a`.

Two further independent processes load the earned schema35/Rule19 city at 1200:
zero-tick complete re-encoding and 103-tick continuation match the protected prior
creator/continued documents literally, ending at 1200/1303. The audit is
`.local/oe52/schema35-final-audit.json`, SHA-256
`06a4de26564b462b5da7761825a0b3b6384fb04750ac156a60eee307e3f388ae`.
The full2,075-input guard, old corpora, sources, executables and Release archive
identities remain exact across these independent comparisons.

After the final label clarification, the source-equivalence supplement confirms
that the only changed source file is the test's JSON labeling; all 14 Release
archives and both independent engine-regression executables remain exact.
`.local/oe52/final-source-equivalence.json` SHA-256 is
`bbd9144170a7a697625eacda0b8231992ba65d3970f87e8fca89dcbf4136f488`.
The final 2,075-input guard is PASS, SHA-256
`77ef4f6ab8ed972919491095507404c886110531800c4e2d43f502aa54e5af7a`.

The source guard covers 2,075 immutable files including 1,464 original installation
files, supplied earlier saves/evidence and prior task outputs. Size, mode,
modification time, SHA-256 and complete original inventory are checked, together
with the user's exact 72,212-byte uncommitted patch. Four proven final Rule19
replay duplicates were removed after literal comparison with retained masters:
897,477,952 bytes; all saves, snapshots, Recovery payloads and masters remain.
The deletion proof is `.local/oe52/final-rule19-replay-cleanup.json`.

Four further newly generated Rule18 replay raw-metric duplicates are removed
only after full literal equality with the retained new creator masters:
888,789,369 logical bytes,893,267,968 allocated bytes. All 39 creator files and 56
other replay files, including every save, snapshot and Recovery payload, remain
exact. The lossless logical 60-file view has explicit aliases to those masters.
The cleanup proof is `.local/oe52/rule18-replay-rawmetrics-cleanup.json`, SHA-256
`fdb2b93a3881086e1da4ecbf653058887c5afea9ae569d8cdf287b87c4b19d47`.
OE52 therefore removes 1,786,267,321 logical bytes of proven own duplicates;
concurrent compiler activity means that is not a claim about the global free
space delta. The observed post-cleanup free space is 80,895,541,248 bytes.

## Remaining limits

Food/Hemp farm and field bodies, the pending 2×2 Elite plot and unassigned
Water/Ancestor/culture/religion/luxury walker families retain their existing historical
markers and fallbacks. Existing Market/Warehouse/FireWatch core previews are not newly
identified original registrations. No new animation or walker family is added.
Existing authored walker cadence, feet and fire attachment remain bounded by
their prior contracts. Static-pixel matches do not prove original shadow,
rotation, whole-scene interpenetration, general height normalization or runtime
composition parity. Partial clipping at narrow apertures is ordinary viewport
clipping, not a reason to resize source images.

NATIVE_GUI and HUMAN remain NOT_RUN for this task. Scripted SDL event delivery,
software pixels, atomic save checks and source preservation do not establish a
human playthrough. OE50's native pointer automation delivery blocker remains
separate, and public release acceptance remains open.
