# Theatre-to-House Music service

The explicit **Rule23 / schema39 / policy3** experiment extends Rule22's paid
Music School→performer→Theatre chain with a real Theatre-owned House visit and
physical return. It requires `authored-venue-house-service-v1`; it does not
migrate Rule22 or an older city. Rule18 remains the recommended player mode.
Rule23 inherits the Rule18 economy through Rule22, without Rule19 demolition,
Rule20 source-cell ramps or Rule21 land trade.

## Starting and observing the chain

Choose **Advanced / Legacy → K Venue House music (23)**, or start explicitly:

```sh
./build/openemperor --data /path/to/game --sandbox Cities/Xia.map --sandbox-rules sandbox-city-v16 --original-venue-house-service
```

Use **E Music** and **G Theatre75**, or their matching construction buttons.
The Music School occupies 2×2 and Theatre75 occupies 4×4. Their raw Normal
Cost/Employees observations are respectively 50/8 and 105/7; actual purchases
and staffing use the checked difficulty-prepared model. Houses, workers, roads,
funds, complete footprints and access must be earned through ordinary World
authority. Installed upkeep still applies during pause or loss of staff.

A real School performer must reach an eligible Theatre before its sole venue
Music timer receives 32. While Music remains positive, an idle, fully staffed,
operating, non-burning Theatre can train a separate venue walker. The fourth
eligible phase35 callback makes an attempt due. Only a successful legal dispatch
allocates its distinct typed Walker ID. One scheduler phase runs per World tick;
phase35 calls occur at completed ticks 36, 87, 138, … . This clock is authored.

The walker follows an actual cached road route to an occupied eligible House.
Only real arrival with **then-current** Theatre Music, valid target and current
source operation, staffing and fire checks assigns the existing House Music
timer to 96. Expiry or refusal grants nothing. Movement precedes that tick's
countdown, so a new duration may already have decreased at the end of the tick.
Neither construction, proximity, dispatch, rendering nor loading grants service.

The actor returns physically before its identity retires. Road cuts preserve
begun-edge safety and ordinary waiting; repair uses normal paid construction.
Pause does not teleport a moving actor home. Fresh training for a later lifetime
starts after actual return. An idle movement channel has no visible actor or
position; its Courier ID is not a reusable Walker identity.

The School and Theatre Inspectors distinguish attempts, actual spawns, current
Music, destination, movement and accepted arrivals. House **Visits** shows the
actual Theatre/Walker receipt and the Musician input supporting that visit.
Theatre and venue walker use authored fallback presentation. Their drawing and
picking retain the full footprint and actual foot; a 4× stage can exceed the
map aperture. Software SDL inspection is separate from native input or human
acceptance.

## Original evidence and authored boundaries

The existing [culture source contract](../reverse/original-culture-source-contract.md)
and [Actor36 investigation](oe57-actor36-investigation.md) record the bounded
read-only evidence. Model labels, static images and a successful callback return
are not sufficient proof of a House grant.

| Mechanism | Bounded Original evidence | Rule23 admission |
|---|---|---|
| Paid input | School211, Musician34, separately paid Theatre75, actual venue arrival and Music32 are independently observed. | Retains Rule22's conservative paid, fully staffed, empty-slot-only input chain. No free venue stock or performer. |
| Venue actor | Theatre75 owns subtype36; source and actor references bind its lifecycle. At full staffing the fourth eligible callback can attempt creation. | A paid fixed movement channel carries one distinct monotone Walker ID per successful spawn; attempts and successful spawns remain separate. |
| House effect | Actual actor movement reaches the source's House callback. Ordinary75 checks current Music and an accessible occupied House, then assigns exactly Music96. | Real targeted House arrival is required. It uses the existing single House timer and actual service witness, with stricter authored current-source staff/operation/fire checks. |
| Route and lifetime | Original distribution scans buildings around the actor's actual cell; a source-directed return and terminal removal are observed. | Deterministic House endpoint selection and a physical outward/return route reuse existing navigation. Original roaming, RNG and the 200–249 update initializer are not mapped to World ticks. |
| Further effects | The separate source71 branch, complete clock mapping and Original binary-save serialization are unresolved here. | No source71 attachment, Acrobat/Drama venue integration, festival, aura, new goods or Original-save import. |

Actual same-role incoming visits can coalesce only with usable physical ETA.
A venue promise additionally requires current source admission and Music that
will remain positive at that ETA; an assumed future School refill is not a
promise. Selection remains authored rather than Original roaming parity.

Music School211 retains its inherited physical model desirability contribution.
This is separate from the new actual House Music96 visit. Theatre75 gains no
desirability source. Limits remain two Schools and two Theatres within the
existing 82 bodies, 58 courier channels and bounded service histories.

## Saved authority and refusal

Schema39 requires exact Rule23, `rules.venue_house_service` equal to
`authored-venue-house-service-v1`, the Rule22 music marker and inherited Rule18
markers. Full source-map/model/difficulty identity, prepared terrain and policy3
remain mandatory. Rules1–22 and schemas1–38 keep their separate contracts;
changing a header or adding a marker cannot upgrade a save. F5/F9, ordinary load
and Recovery select the explicit generation before publishing replacement state.

Culture state retains an immutable **first actual Music receipt** per Theatre,
bounded source/channel rows, counters and the monotone Walker highwater. Existing
service trips and witnesses retain separate dispatch Music and actual-grant
Music provenance. Later School deliveries cannot rewrite the first receipt or
manufacture earlier training. The sole current venue duration and House Music
duration must follow their actual receipts and eligible countdowns.

Validation checks typed identities, paid ownership, chronological placement,
physical travel, source-specific sequence/global-ID order, real return and fresh
training before another lifetime. Retained Music inputs and simultaneous spawns
must fit real School identities and conservative model-derived paid admission
and workforce bounds. Fire histories and retained House evaluations must agree
with the claimed events. These are bounded restore checks, not a reconstruction
of every historical staff priority or an Original binary-save decoder.

Missing, duplicate, foreign, cross-generation, impossible or forged state is
refused atomically. A rejected command or restore must preserve live state, IDs,
funds, source inputs and prior save bytes. Inspection and drawing create no
saved authority and do not tick, copy the World or refresh routes per frame.

## Validation and current acceptance

| Gate | Current scope |
|---|---|
| Source research | Concrete Actor36 ownership, physical House callback, current Music96 assignment and return are evidenced; Original roaming, complete cadence and binary-save serialization remain UNKNOWN. |
| Preliminary focused checks | **7/7 PASS**: five new World/save/menu/view/actual-CLI tests plus historical Rule22 World/save tests. Includes paused/expired incoming-visit cases. This is preliminary, not the full regression gate. |
| Scripted graphics | All 21 preliminary View and 6 Menu captures were opened. Authored fallback, bounded Inspector scrolling and map-aperture clipping remain explicit; final committed-build captures are pending. |
| Supplied Xia / Normal gameplay | **PENDING**: ordinary paid city, repeated School→Theatre cycles, actual House service and complete venue-walker lifetime, finance and finite ≥20400-tick continuation. |
| Final technical and persistence gates | **PENDING**: full Release, focused Debug and sanitizers, independent complete-save processes, current historical corpus and Rule18 regressions. Preliminary fixture coverage is not their substitute. |
| Native and human | **NOT_RUN**. No complete Original entertainment, animation, roaming or religion parity claim. |

Run the complete suite and affected historical culture/services/Housing checks,
then the five focused tests:

```sh
ctest --test-dir build --output-on-failure -R '^original-venue-house-service-(world|save|menu|view|cli)$'
```

The Save test also accepts `--create OUTPUT [TICKS]` and
`--restore INPUT OUTPUT [TICKS]`, with fresh explicit paths under this checkout's
ignored `.local/oe57/` and continuation 0–20400. Compare complete Creator and
independently restored SaveDocuments at 0, +103 and +20400 ticks, plus complete
Recovery payloads. Preserve all inputs and rejected-save bytes. Exercise actual
arrival, expiry, return, road cut/paid repair, pause/fire/staff loss, hostile
provenance and exact old-generation refusal. Open captures at each tested size
and zoom; classify software SDL, native delivery and human acceptance separately.
