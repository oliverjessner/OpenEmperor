# Music performers and Theatre venues

The explicit **Rule22 / schema38 / policy3** experiment connects a paid Music
School to a paid Theatre Pavilion through an actual one-way performer. It
inherits the Rule18 economy and replaces only Music School211's targeted
School→House service in this generation. A real Theatre arrival supplies the
venue's own Music duration; it grants **no House Music96 effect**. Actor36,
other venue types and complete Original entertainment behavior remain unsupported.

The bounded chain has automated World, complete-save and scripted SDL coverage,
including paid construction, real movement, arrival, expiry and restoration.
Native delivery and human acceptance of this new chain are **NOT_RUN**. Existing
Rule18 remains the recommended player mode. Earlier rules and saves retain their
own contracts without migration; this mode does not inherit Rule19 demolition,
Rule20 source-cell ramps or Rule21 trade.

## Starting and observing the chain

Choose **Advanced / Legacy → B Music venues (rule 22)** for a new city. The
separate direct launch is:

```sh
./build/openemperor --data /path/to/game --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-music-venues
```

Use **E Music** and **G Theatre75** to select the same construction actions as
their toolbar buttons. Connect both paid bodies to legal roads and supply their
actual required workforce. Source211 occupies 2×2; Theatre75 occupies 4×4. The
source model's raw Normal Cost/Employees are 50/8 and 105/7 respectively; the
session uses its checked difficulty-prepared model values. Buildings and roads
must pass the ordinary complete-footprint, permission, height, funds and access
checks. Paused or unstaffed buildings still incur the inherited installed upkeep.

The School Inspector distinguishes its spawn counter, due attempts, actual
successful spawns, active Performer ID, real destination and arrival. The Theatre
Inspector distinguishes incoming performers, the sole Music timer and the last
accepted arrival. Construction, dispatch, proximity and an intended visit grant
no duration. A stocked venue is not a promise of House service: Music-dependent
House needs can remain unmet in this experiment. Choose Rule18 for the existing
direct Music-to-House adapter.

Music School211 retains its inherited physical desirability contribution from
the prepared model. That contribution is separate from the House Music96 visit
timer. Runtime evaluation and complete-save restoration must see the same paid
School influence source even though its lifecycle is stored in the culture
state. Theatre75 has no newly admitted desirability source; performer dispatch
and venue arrival grant no House Music96 effect.

Theatre and its performer use explicitly authored fallback graphics. A static
school image is separate from performer animation or Original scene parity.
Drawing and selection share the Theatre's complete stage geometry and the real
actor foot. At 4×, a complete 4×4 ground can exceed the map aperture; preserve and
inspect that clipping rather than shrinking source geometry. An idle movement
channel has no visible performer or position.

## Source evidence and authored choices

The [culture source contract](../reverse/original-culture-source-contract.md)
records the concrete factory, body, ordinary paid construction, performer
arrival, duration and serializer observations. Model labels and pixels alone
grant no construction or service authority.

| Boundary | Bounded Original observation | Rule22 contract |
|---|---|---|
| School211 | Concrete Music factory, initial 2×2 body, model Cost/Employees, Actor34 and staffing-dependent byte-counter attempt. | Paid source with the prepared model; full staffing, operation, no source fire, road access and an idle channel gate new attempts. |
| Due attempt | Increment wraps an unsigned byte, compares strictly greater than the threshold and resets before Actor creation; an attempt can fail. | At full staffing, threshold 3 makes the fourth eligible callback due. No admitted venue means no Performer ID, actor, target or timer grant. |
| Theatre75 | Separate initial 4×4 body, ordinary paid construction and independently observed raw staffing. The special existing-source attachment branch is type71 only. | Independently purchased75 body; type71 is not a free or attached alternative. At most two Music Schools and two Theatres fit within the existing 82-body/58-channel limits. |
| Destination | Original selector checks status, Access, performer acceptance and positive raw staffing; it can consider retained Music slots≤8 and then≤16. | Conservative empty-slot-only admission, full staffing, operation, no target fire, an existing legal cached route and no other actual incoming performer. Stable bounded destination rotation is authored. |
| Actual performer | Musician34 retains school and selected venue references, traverses a route and ends after arrival; it has no physical School-return leg. | Each successful spawn allocates a distinct monotone typed Performer ID. A fixed paid courier is only its reusable movement channel. The actual performer follows existing road movement and begun-edge/wait/repair rules. |
| Venue arrival | Actual Music arrival writes 32 to venue-own-info+0x5e; separate from House96. | Arrival rechecks the real venue's empty slot, operation, staffing and fire. An accepted arrival records its witness and assigns 32; either accepted or refused arrival retires that performer and clears its channel. |
| Duration | Initial venue slots are zero; an eligible update decrements nonzero bytes; the original own-info serializer retains them. Complete Original call cadence is not established. | The inherited authored phase35 clock counts down the venue timer independently of later pause, fire or staff loss. No additional calendar or saved clock is introduced. |
| Further service | The observed supplied venue has a separate Actor36 path. Its full House service and broader venue behavior are outside this slice. | No Actor36, House96 grant, passive aura, free visit, Acrobat/Drama venue state or festival effect. Existing Acrobat/Drama-to-House adapters remain separate. |

One scheduler phase is mapped to each existing World tick. Eligible culture calls
occur at completed ticks 36, 87, 138, … . Movement and real arrival precede that
tick's phase35 countdown, so an accepted arrival on a countdown tick is observed
as 31 afterwards. Source pause/fire prevents new attempts but does not manufacture
a return or cancel a performer already moving. Target admission can fail when
its operation, staffing or fire state changes during the journey. Road cuts keep
the existing begun-edge safety and ordinary waiting/paid-repair behavior.

The [timer](../reverse/original-service-timer-contract.md) and
[spawn](../reverse/original-service-spawn-contract.md) contracts remain distinct.
Their pure callbacks do not independently create a route, paid building, actor
or World-tick cadence. In earlier Original generations, the existing authored
Music/Acrobat/Drama School→House visits and their 96 timers remain unchanged.

## Saved authority and refusal boundaries

Schema38 requires exact Rule22 and
`rules.music_venues: "authored-music-venue-v1"`, plus the inherited Rule18
market, cardinal-water, fire, service and Food markers. It retains revision27,
the complete source-map/model/difficulty identities and independently rederived
terrain and policy3 context. Older schemas reject the new marker, Theatre,
performer role and culture state. Header relabels do not migrate a city.

`OriginalProductionState::culture` is mandatory only for Rule22. It owns bounded
School and venue rows, attempt/spawn totals, the successful-spawn ID highwater,
latest actual trips and real accepted-arrival witnesses. The sole Music duration
must equal the witnessed arrival followed by eligible countdowns. A channel ID
is not a Performer ID; a later spawn cannot reuse the retired identity. A venue
and source reference must bind the actual paid bodies, complete footprints,
chronological placement/spawn/arrival and physical route/progress. No return
history or visit to a House is invented to satisfy an old service validator.

Model-bound restore also rejects training before the maximum possible paid
admissions could supply the School's prepared Employees. Retained simultaneous
spawns must fit the possible pre-tick workforce for all their distinct Schools
and venues. These conservative bounds use the authored admission ceiling and
checked model capacities; they do not reconstruct every past pause, priority or
staff allocation. They run at create/restore, outside normal tick/frame work.

F5, F9, ordinary load and Recovery use explicit generation-aware validation
before publishing a replacement. Unknown, missing, duplicate, mistyped,
cross-generation, orphaned or impossible state must be refused without changing
the live World, IDs, funds, input data or prior saved bytes. New source/venue
demolition is unavailable; Rule19's retirement journal is not part of Rule22.

## Religion remains a separate service contract

No new religious action is activated. Existing paid religious sources keep
separate actual-arrival House40 timers and their historical saves.

| Source or mechanism | Current integration | Original evidence and remaining gap |
|---|---|---|
| Ancestral Shrine214 | Paid source and owned visitor; actual occupied-House arrival sets Ancestor40. | The six-case religious factory, ordinary-shrine spawn threshold and timer index0 are separately evidenced. It is not interchangeable with the215/217/219 physical-dimensions table. |
| Daoist Shrine215 / Buddhist Shrine217 | Paid 2×2 sources with independent visitors and Daoist40/Buddhist40 timers. | Concrete factories, bodies, Priest35 visitor path and distinct timer indices are observed. Targeted destination choice, full-worker gate and World clock remain authored. Daoist OR Buddhist is a House-needs test, not a combined timer. |
| Daoist Temple216 / Buddhist Pagoda218 | Not purchasable World sources. | Factory branches and different attempt thresholds exist, but a named model row or shared religion timer does not establish their complete admitted construction/lifetime. |
| Confucian Academy219 | Paid 3×3 source; real delivered Paper and occupied Elite target required. Exactly one Paper is consumed per successfully published tour; only actual eligible arrival sets Confucian40. | Initial elite-only condition and positive material-stock gate are observed. Original setup can choose resource27 Paper or10 Wood; its complete selector, raw units and consumption cadence remain unresolved. Current Paper-only/one-per-tour consumption is authored. |
| Empty Elite religion visit | The pure predicate supports the bounded Original occupied-or-Elite domain. Current paid World dispatch additionally requires positive occupants, including for Confucian. | Do not describe the narrower paid target policy as complete Original admission parity. |
| Offerings, sacrifices, festivals, Hero/God AI and happiness | Unsupported paid gameplay; no new material debit or city/House benefit. | A complete physical source, input, ownership, trigger, effect and saved lifetime is UNKNOWN in this reviewed source scope. Actor79's bounded timer projection does not implement these systems. |
| Holidays and religious calendar | No Original holiday/festival cycle. | The observed scheduler order does not prove wall time or a full Original annual religious schedule. The adapter's derived year/month mapping is authored. |

The [religion source contract](../reverse/original-religion-source-contract.md)
distinguishes its historical revision22 staffing-only adapter from current
revision27. The current paid Hemp→Paper production/physical delivery/Academy
stock is real World authority, but its one-Paper-per-tour sink does not reproduce
the observed Original operation that subtracts 100 raw stock after 100 eligible
consumption-counter calls. Neither path grants religion from proximity.

## Reusable validation

The following bounded acceptance record separates paid gameplay from startup,
historical numeric parity and unrun native/human checks.

| Scope | Observed acceptance and limit |
|---|---|
| Tested source and build | OE55 baseline `90753671748acb3b0c3b83f50ee8901f94e506c4`; tested source `29f9b241471850f9c7ce1b39cd6d37b6236b56e8`, including the physical-School-DES correction `e2ebf8d89e3e0a2677e426979958e62f5fcb079b`. AppleClang 21.0.0, SDL 3.4.14, macOS 27 arm64, deployment target 27.0. The configured Release footer reports `bca1a8076bb3`; runtime/source bindings identify the tested source separately. |
| Supplied Xia / Normal paid chain | Five normally paid Houses and connected Music School/Theatre; 21 ordinary construction commands debit 583. Performer 1 is moving on a begun edge at seed tick 291; the School is then paused. Actual accepted arrival at 345 assigns venue Music32, with exact expiry at 1974. No House Music96 timer is granted. |
| Finite continuation and finance | From paused-source seed 291, +103 ticks reaches tick 394 / Music31; +20400 reaches tick 20691 / Music0. The one actual spawn retires and the next Performer ID is 2. Funds follow `12000 − 583 construction − 750 installed upkeep + 0 taxes = 10667`. This proves one paid arrival and finite expiry, not recurring dispatch, sustainability or profit. |
| Independent persistence | Six Creator/Restore processes compare 12 complete SaveDocuments and 18 Recovery payloads through 28 literal full-byte comparisons at 0 / +103 / +20400 ticks. No migration or partial-field comparison substitutes for complete equality. |
| Actual CLI startup | Twenty invalid arguments reject before loading. Explicit Rule22 startup produces 99 rendered/presented frames and exits 0 after one SIGTERM, with no forced kill, 0 simulated ticks and no save output. This is scripted dummy/software SDL startup, not gameplay or native delivery. |
| Regression and parity | Release 314/314, Debug 33/33 and ASan/UBSan 33/33 pass, with no failures, skips or disabled tests. Sanitizers use `detect_leaks=0`; external SDL is not instrumented. The historical 132-file corpus and independent schema35–37 documents match their protected historical baselines; current schema38 Creator/Restore documents are independently full-byte equal, without a historical schema38 master. The full 99-file Rule18 comparison passes: 97 complete literal comparisons (93 top-level files + 4 Recovery payloads) and 2 documented Recovery-metadata checks. Fresh 74-case historical parity, 18 Housing observations and the 1800-value actual-model digest preserve the complete public canonical report; this is not Rule22 performer/venue gameplay parity. |
| Remaining acceptance | Native delivery and human acceptance **NOT_RUN**. Theatre/performer presentation is authored fallback. Actor36, Venue→House, complete Original clock/selector/animation parity, other venue types and new religion mechanics remain unsupported or UNKNOWN as described above. |

Run the complete Release suite and the four focused tests alongside the affected
historical culture, services, housing and model checks:

```sh
ctest --test-dir build --output-on-failure -R '^original-music-venue-(world|save|menu|view)$'
```

The Save test provides isolated authored-input Creator/Restore processes. Use
fresh paths under this checkout's ignored `.local/oe56/` and compare complete
canonical files at both initial and continued states:

```sh
./build/openemperor-original-music-venue-save-tests --create .local/oe56/check/create-0.json 0
./build/openemperor-original-music-venue-save-tests --restore .local/oe56/check/create-0.json .local/oe56/check/restore-0.json 0
cmp .local/oe56/check/create-0.json .local/oe56/check/restore-0.json
./build/openemperor-original-music-venue-save-tests --create .local/oe56/check/create-103.json 103
./build/openemperor-original-music-venue-save-tests --restore .local/oe56/check/create-0.json .local/oe56/check/restore-103.json 103
cmp .local/oe56/check/create-103.json .local/oe56/check/restore-103.json
```

Observe a paid connected city from zero population through actual staffing,
attempt, successful spawn, begun movement, real venue arrival and exact expiry.
Check the construction debit, installed upkeep, empty cargo, actor retirement,
sole venue timer and absence of House Music grants. Exercise absent/full/wrong
venue, unstaffed/paused/burning source or venue, illegal 4×4 interior, insufficient
funds, road cut/wait/repair, impossible saved counters, checked model arithmetic
and hostile source/reference/save refusals. Rejected operations preserve complete
state and prior saved bytes.

Include nonzero signed School desirability fields in complete-save restoration
checks, so a zero-influence fixture cannot conceal a missing physical source.
Keep the inherited static School influence distinct from the performer's
venue-only Music duration. With supplied data, prepare the actual culture model
and legal map context through the shared readers, buy the complete scene with
ordinary commands and earn staffing and a begun performer edge before saving.
Compare independent Creator/Restore documents and Recovery payloads immediately
and after continued ticks. A paused source with one earned arrival tests
retirement and expiry; it does not establish recurring dispatch or citywide
sustainability.

Open the generated MenuSession/SandboxView captures at all tested window sizes
and 1×/2×/4×. Inspect the actual visible performer foot/direction, full physical
claims and selection, stage/Inspector clipping, arrival lines and fallback labels.
Confirm F5/F9, Recovery and pure frames against full World/document bytes and
the existing preparation counters. Keep authored/synthetic inputs, scripted SDL,
native delivery, human acceptance and Original parity separate. Repeat relevant
Debug and ASan/UBSan checks and preserve historical schemas 1–37, the independent
schema35/36/37 checks and the paid Rule18 Golden City without migration. See
[testing](../testing.md) for the shared validation setup.
