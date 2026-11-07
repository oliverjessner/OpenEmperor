# OE53: authored source-cell terrain transport

Rule20 / policy3 / schema36 is a separate authored experiment. It inherits
Rule6's classic economy, five-tick road edges, BridgeSpans and RampLinks. It does
not inherit Original economy Rules7–19. The recommended Original18/schema34,
ordinary City-v16 rule4, general City-v11 rule3 and all historical saves remain
separate. Native Elevation233–244 remain an isolated prototype: this change does
not grant transport through their unsupported original raw terrain.

## Source and topology decision

The four-map census uses unchanged installed files, the common stored-map
readers and complete policy3 preparation. Every map prepared successfully.

| Original map | Earth233–236 | Stairs237–240 | Cobbled241–244 | Permitted equal-height land pairs | Permitted differing-height land pairs |
|---|---:|---:|---:|---:|---:|
| Anyi | 3 | 0 | 0 | 14639 | 0 |
| Banpo | 2 | 0 | 0 | 7946 | 0 |
| Badaling | 16 | 3 | 0 | 15563 | 0 |
| Xia | 0 | 0 | 0 | 3233 | 0 |

All24 saved ramp references have one higher cardinal neighbor and an equal low
join. The21 ordinary earth references have raw`0x600`, except one`0x602`; they
remain `unsupported_terrain`. The three separate Badaling stair references
(238 twice,240 once) have raw`0x640` and remain `unsupported_original_road`.
Neither a saved graphic nor its physical pixels supplies World permission.
The fresh twelve-record metadata check finds Type30/side1/width78/base3200,
without external, alpha-stream, animation or mirror declarations.

The pinned EXE SHA-256 remains
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
A bounded fresh PE-section/table read of rows67–70 at`0x825c28` corrects the
older asset-audit table: the opposite cardinal is pattern3, a wildcard under
the maintained selector contract. Lateral cardinals are0, high cardinal1 and
high-facing diagonals4. This is a table-byte recheck, not a new control-flow
reconstruction. An equal opposite join below is an additional authored safety
constraint. Raw EXE bytes and diagnostic tables remain private.

| Property | Original evidence / prototype | Rule20 production contract |
|---|---|---|
| Stored height | RAW / EXE: signed int8, projection40px/level | Same immutable signed source heights |
| Footprint | EXE: one low source cell; prototype full-cell surface | One source body; three exclusive axial claims L/S/H |
| Higher neighbor | EXE ordinary233/234/235/236: S/W/N/E | Explicit cardinal H=S+D, exactly one level higher |
| Low join / neighborhood | Opposite EXE wildcard; prototype adds equal join | Complete permitted3×3 rectangular step, L/S/laterals low; high-facing cells high |
| Anchor | EXE native image `(39,height−20)` at low source cell | Authored body anchored at unchanged low ground; no borrowed native image |
| Half-height | Prototype continuous `low+.5+s` inside source | Shared pure `low+clamp(s+.5,0,1)` on the two adjacent edges |
| Walker foot | EXE separate actor offset; exact rounding unresolved | Actual World path position plus the shared surface, both travel directions |
| Edge duration | EXE ordinary offset20 updates×2px; discrete endpoints unresolved | Existing authored five ticks per edge, no original timing claim |
| Road permission | Native raw ramp combinations remain unsupported | Existing exact policy3 normal land only; no source/name/ID exemptions |
| Bridge connectivity | Original deck/placement/navigation still unresolved | Existing legal paid BridgeSpans and shared World BFS only |
| Painter / selection | Prototype original crest and full compositor unresolved | Eager authored back/tread/near-bank parts and cached visible-run picking |

## Paid authority and persistence

Choose **Source-cell ramps (rule20)** in City-v16's explicit setup, or launch:

```sh
./build/openemperor --data /path/to/emperor \
  --sandbox Cities/YourAuthoredStep.map --sandbox-rules sandbox-city-v16 \
  --authored-source-cell-ramps
```

The P tool drags from the LOW source road S toward its HIGH neighbor H. Three
normal roads L=S−D, S and H must already exist. Construction costs40; this V1
has no ramp removal, refund or additional upkeep. Combined historical RampLinks
and source-cell ramps remain bounded to64. The underlying classic city retains
its existing paid workers, recipes, goods, tax and maintenance behavior.

`simulation::World` validates and publishes the complete candidate atomically.
It owns monotone typed IDs, all three permanent claims, canonical navigation,
paid accounting and begun-edge safety. Only S's low/high axis is legal; lateral
entry and direct building entry at its half-height center are refused. Building,
Bridge-bank/deck and other ramp claims cannot overlap, including after a road
cut. A purchase refuses any retained courier-path reference to the changed
three-cell interval. No renderer, prototype pathfinder or saved cache grants
movement.

For a courier at S+sD, −1≤s≤1, surface height is
`low+clamp(s+.5,0,1)`: flat approach, full-cell slope, flat departure. Actual
begun-edge progress determines XY. Removing either cell of an occupied begun
edge is refused; cutting a later road preserves cargo, reservations, physical
position and that begun edge. After completing it, the courier waits until an
ordinary paid repair supplies a real route. Arrivals and returns remain actual
World events.

Schema36 requires marker`authored-source-cell-ramps-v1`, including empty worlds,
plus bounded source-cell DTOs and the next ID. It binds the full map-file SHA,
mask SHA, complete unchanged policy3 fingerprint and geometry. Height, cost,
placement tick, ID sequence, necessary road purchase/removal history, complete
paths, accounting and limits are checked before publication. The explicit
create/restore APIs reject other generations; F9 cannot cross the Rule20
boundary. Unknown fields, duplicate36 keys and missing/wrong markers refuse.
Historical serializers and canonical bytes receive no new fields. The new
schema contains no saved render masks, indices or floating-point foot heights.

Managed36 Recovery checkpoints opened through the file chooser receive a fresh
manual target; F5 cannot overwrite their payload. Failed save replacement keeps
the previous file and World intact.

## Prepared presentation

Four authored orientation templates prepare flat/elevated tread and bank runs
outside frames and hover. Preview selects those templates without rasterization,
allocation, decode or upload. Production feet use the World surface while body
anchors retain low ground; the normal paid road image at S is replaced by the
source-cell body. Visible banks use the same prepared runs for picking.
East/South are edge-on under the fixed projection: a narrow authored crest gives
them visible thickness without shifting the actor's physical foot.

The new renderer retains4096 runs per part,512 prepared faces and an8MiB capacity
bound; the view also checks combined old/new ramp bytes against8MiB. Existing
64MiB physical-asset and64-ramp limits are not raised. Old RampLink templates,
Snapshot geometry, Bridge geometry and their historical painter stay unchanged.
Authored cliff sides remain a separate session-only presentation option. They
never reinterpret original wall, gate, source or water permissions.

## Validation and delivery

**SCOPED AUTHORED TRANSPORT PASS**; native delivery and human acceptance are
**NOT_RUN**. Baseline is OE52's verified main
`f72e74570c6bdcfaeb892f1d26b0f5aba93207dc`. The isolated branch is
`feat/oe53-terrain-integration`; production source froze at
`6191d3c6c324ef22bb19ada916bd8ec34f093674`, and final test/tool source at
`6db3e3358d50f018f69d7e9b272f0fdf92daf299`. The original dirty user checkout was
preserved. Build platform: macOS arm64, AppleClang21, CMake4.4.3, SDL3.4.14;
scripted views use SDL dummy/software. Release reuses the explicit OE50
minimum27.0 cache; the project's normal minimum26.0 is unchanged.

| Gate | Actual result and scope |
|---|---|
| Release | Complete **305/305 PASS**,357.70s, zero failed/skipped/disabled tests; full product build has zero warnings/errors |
| Debug | Focused **24/24 PASS**,140.632s |
| ASan/UBSan | Focused **24/24 PASS**,78.061s; no sanitizer diagnostics |
| Historical schemas1–34 | **132 complete canonical/JSON payloads and case indexes** literal-identical to the frozen pre-Rule19 corpus, using current headers/archives |
| Historical Rule18 golden | Fresh paid Creator and separate Replay retain93 complete top-level files plus four Recovery payload comparisons against OE51 masters; all seven full checkpoint documents/snapshots and independent re-encodes agree |
| Historical schema35 | Current paid Creator plus independent zero-tick restore and103-tick continuation; all three full documents literal-identical to protected OE51 masters |
| Schema36 | Separate paid Creator/Restore processes at tick47 and independently continued tick150; both complete document pairs literal-identical |
| Original core numeric parity | Fresh pinned reference/current probes: **74/74** preserved cases;46 parity,18 corrected bugs,6 unverified,4 unsupported; actual model digest unchanged |
| Original-file integrity | **2448 protected identities**, including all1464 original files, source inventories, old saves and retained evidence; no mutation or reappearance of eight previously thinned duplicates |

The first full Release attempt had300 passes and five stale test-contract
failures. Four finite version oracles still treated20 as unknown; they now
assert the new classic contract, preserve all Original predicate expectations
and reject21. The fifth failure was the parity tool's World-header pin. After
independent API review, actual fresh probes regenerated the existing report;
only the reviewed World/header/tool fingerprints changed, with all74 prior
observations, expectations, classifications and the baseline lock retained.
The complete final suite and both focused configurations then passed. No timeout
was raised and no test was skipped. Sanitizers use `halt_on_error=1`;
ASan leak detection is disabled (`detect_leaks=0`), so this is no leak acceptance.

The World tests exercise16 signed cardinal combinations, including int8 height
extremes, with21 surface witnesses each;600 real paid ticks with Clay delivery,
Pottery production and return, both with and without an inherited BridgeSpan;
fund exhaustion, begun-edge removal refusal, a future cut,10 actual waiting
ticks, paid repair and full continuation. Retained path references refuse a
purchase. Hostile snapshots/documents cover IDs, claims, height, accounting,
necessary road history, generation/source/policy binding and atomic refusal.
Historical Rule5/6 full-document round trips retain their exact contract.

The registered production SandboxView drives actual loaded cargo to tick55
inside the slope (`s=−.2`), rather than showing a hypothetical approach. Each of
four directions has F5/F9, Recovery, independent World restore and700 further
real ticks, ending at755 with8 completed recipes and20 upkeep. SDL events cover
P drag/cancel/duplicate release, bank selection and Inspector, F8's eight states
and F7. At100 paused frames and100 held previews the forbidden frame counters
stay zero. Prepared ramp geometry is366536 bytes; the scene also contains76
authored cliff faces. The480 renderer cases cover signed bases, cardinal
orientations, three sizes, zoom1/2/4/1.73, flat/elevated state, bounds, picking,
state restoration, ID deduplication and failed replacement.

All36 production captures (four directions × three window sizes × zoom1/2/4)
were opened. The36 PNGs and four direction reports agree byte-for-byte between final
Release, Debug and sanitizer runs.
North/West expose the broad tread; East/South are edge-on and their crest/banks
can partly occlude the fallback marker. At900×600/4× the viewport clips outer
approach/bank geometry. These results establish physical foot projection and
bounded visible-run selection, not an unobscured whole-footprint promise.

A separate private production-linked check repeated all36 views with supplied
original Clay walker/cart sprites117–120. Every measured loaded actor used an
actual sprite at the unchanged World foot; the four complete direction save
documents equal the registered fallback run. All36 captures were opened. The
curated sprite anchors remain authored previews and East/South boot occlusion
remains visible. This is original actor artwork on an authored ramp, not native
233–244 ramp activation or original timing parity. Transient private sprite
copies were hash-verified and deleted after use.

Original scene controls compare empty Rule6 and20 on unchanged Badaling and
Xia:18 complete map crops (three sizes × three zooms × two maps) are
pixel-identical, excluding the explicit rule label. All18 retained captures
were opened. Badaling prepares53 supported explicit Stone-preview pieces; its captures
show a fixed wall-boundary neighborhood. Xia has no wall and serves as the
map-centre control. These maps admit zero authored
cliff faces. Four hundred pure frames keep all12 forbidden counters zero.
This preserves the scoped wall/map rendering; it does not establish general
Great Wall or interpenetration parity.

The unchanged Rule18 Xia/Normal `lean-med` paid plan enters Elite17 at5395
and completes25795, a20400-tick window. The target retains400/400 current
needs, with zero target fire/devolution/retention-loss ticks; all13 purchased
House records complete5200/5200 evaluations with zero window Food misses or
devolutions. Five House fires and2915 burning House ticks remain; the minimum
nonburning inhabited House count is11, not13. Operating cashflow is
22630 taxes minus17450 upkeep = +5180, minimum funds5391. These finite results
preserve the historical outcome and do not imply a fire-free city or profit
from lifetime tax totals. The two actual processes took113.038s and120.483s.

Storage cleanup removed four newly generated byte-identical replay bulk files
(888789369 logical bytes),274 redundant test PNGs (73630768 bytes),143504 bytes
of an own intermediate object, and110834324 bytes of transient sprite copies.
Full identity/byte-comparison manifests were written before deletion. Current
Creator/master captures, all saves/Recovery/non-PNG reports and historical
protected files remain intact;2448 protected identities were rechecked.

Private reports, captures, binary/source identities and full save comparisons
remain under ignored `.local/oe53/`; no original data is tracked. The final
Release JUnit SHA-256 is
`8a89ed95093b1c1a44b1a2189a46523013985ea994b4d3743b6f35482d40bf3f`.
The source/data checks are automation evidence. No native Cocoa/Metal pointer
acceptance, human play acceptance, Gatekeeper/notarization, clean-Mac test or
release approval is inferred. Actual PR and merge identities are recorded in
the GitHub PR delivery log after those operations occur.

Unresolved original work includes normalization and first draw, complete stairs
versus cobbles selection, precise actor offset phases, rear-crest occlusion,
neighboring native-ramp composition, Great Wall interpenetration, Ferries and
original bridge traversal. Authored transport success does not resolve them.
