# OE46: whole-city reliability and service coverage

## Reproduced Rule16 baseline

The clean starting point was `cf6a72652dfcc3e3eb2bf12f19303312330f114f`,
matching the checked `origin/main`, on `feat/oe46-citywide-reliability`.
The supplied original installation, installer and both retained OE45 reference
directories were sealed before execution:1541 files/3188365926 bytes, including
SHA256, size, permissions and modification time. Original data remains read-only.
New outputs belong exclusively under ignored `.local/original-economy/oe46/`.

The fresh `run-01/baseline-rule16-01` used Xia/Normal, Rule16/schema32/policy3,
`lean-med`, and the unchanged `oe45-grid-watch-near` paid controller. It reproduced
all32 complete core JSON files literally against **both** protected OE45
`rule16-final-oe45-grid-watch-near-04` and `near-creator-digests-03`. The external
OE46 output-root selection does not alter the historical recipe or World.

| Measure | Reproduced result |
| --- | --- |
| Actual paid type16→17 / endpoint |5446 /25846 |
| Retention / target evaluations |20400 actual ticks /400 of400 current needs met |
| Target fire / devolution / vacancy |0 /0 /0 |
| Operating cashflow / minimum Funds |+4951 /5017 |
| Tax / installed upkeep in retention |22201 /17250 |
| Inhabited Houses / evaluations |13 /5200;400 per House |
| City current / service failed evaluations |391 /328 |
| Food missed demands / House devolutions |18 /25 |
| City fire starts |164 |
| House25 current / service / Food / devolutions |111 /82 /8 /10 |
| Accepted paid receipts / construction Funds |612 /5209 |

`run-01/gate-a-independent-audit-01.json` independently checks every requested
value, all13 House event counts and actual Food/Goods/service/Water arrivals,
all seven complete saved mass/Funds states, six Goods and both Food crops,
five auxiliary materials,816-tick invoices, command accounting, actual fire
intervals, Inspector CSV and two Recovery payloads. Pre/post byte guards pass.
This first baseline run does not itself claim a new complete per-tick digest
comparison or a separate restore process; those are independent checks.

These are finite authored-simulation observations. They do not establish
whole-city stability, original runtime parity or native human acceptance.

## All13 Houses and the first proven causes

Every House has400 real phase39 inputs in the retention window. The private
`gate-b-service-fire-01/profile.json` retains all5200 inputs, all328 individual
service failures, actual last-arrival witnesses, phase35 expiry, current physical
trips, the next observed successful visit, source/destination fires and all25
devolution inputs. A physical earliest-arrival bound is explicitly optimistic:
future dispatch, staffing, eligibility and road continuity are not guaranteed.
An actually observed next arrival is kept separate from that bound.

| House | Type entry→end | Residents entry→end | Current fails | Service fails | Food misses | Devolutions | First Ancestor miss |
| --- | --- | --- | --- | --- | --- | --- | --- |
|1 |17→17 |20→25 |0 |0 |0 |0 |— |
|7 |6→6 |31→31 |29 |29 |0 |1 |9883 |
|8 |5→5 |22→22 |12 |12 |0 |0 |14830 |
|9 |6→6 |31→31 |12 |12 |0 |0 |14983 |
|10 |5→5 |22→22 |11 |11 |0 |0 |15085 |
|19 |4→6 |22→31 |36 |36 |0 |3 |6721 |
|20 |6→7 |31→41 |36 |36 |0 |2 |15340 |
|21 |5→5 |22→22 |14 |14 |0 |0 |15442 |
|22 |5→5 |22→22 |24 |14 |1 |1 |15544 |
|23 |5→5 |22→22 |38 |24 |2 |2 |15799 |
|24 |5→5 |22→22 |38 |28 |7 |3 |9271 |
|25 |7→7 |26→41 |111 |82 |8 |10 |9373 |
|26 |5→5 |22→22 |30 |30 |0 |3 |11311 |

The recorded devolution ticks are7:10342;19:7180/21103/21613;
20:18094/18604;22:6619;23:6262/18553;24:7078/9730/14575;
25:6262/6772/9526/10036/10546/14677/15187/15697/20032/20542;
26:11770/16564/20236. Each event's exact pre-evolution missing requirements,
counter and actual supplied state remain in the profile. A low next-stage timer
is not counted as a failed current requirement.

**Service reachability and physical capacity (G/D).** Every service-failed
current evaluation is Ancestor-only. No failed current Water, medical, culture,
Confucian or Daoist-or-Buddhist requirement was observed. Ancestor4 at(125,137),
actor3, reaches all13 Houses. Its prepared full rotation totals191 edges/1910
movement ticks. Ancestor51 at(134,131), actor33, reaches only Elite1 on two edges;
all12 Common House routes are null. Both Inspectors also lack a route to51.
The religion timer's40 phase35 countdowns provide1989–2039 ticks after arrival;
1910 movement ticks leave little interruption slack. This is a workload bound,
not a proof that every possible schedule succeeds or that stability is impossible.

**Fire and selection (F/E).** House25's first Ancestor failure9373 follows its
8960→9420 fire: the connected actor skips it at9111 while it burns. After the
fire, a separate cyclic-selector weakness prolongs the gap. Actor3 visits23 at
10485;23 burns from10540. At10641 the last destination23 is absent from the
filtered eligible list, so the historical selector restarts at Elite1. Healthy
House25 could physically be reached10675 if selected then; its actual next
successful Ancestor arrival is12185. This is a class-neutral filtered-cursor
reset, not evidence of an intentional House-ID priority. Other candidate resets
remain explicitly subject to eligibility review in the private profile.

**Food/Goods transport after fire (F/D/E).** House25's last positive Food arrival
before its first gap is5055. Its final portion is consumed5752; first FoodQuality
failure5803 precedes the next real Food arrival6935. Market11 burns5380→5980;
Market29 burns6100→6700. Their real actors finish prior returns and cyclic trips;
no proximity, outgoing reservation or physical stock elsewhere grants receipt.
House25's Hemp stock is also consumed5752; its three current Hemp failures are
5803/5854/5905. Market29 holds4 actual Hemp at5803, but the returning distributor
cannot dispatch the replacement until5855; it arrives5954. For House24, the
6420→7020 destination fire causes both Food actors to pass its position; the
next real Food arrival is8949 despite a three-edge route from Market11.

**Workforce and production are not the demonstrated shortage (A/B/C).** Frozen
pre-tick allocation gives5513041 assigned-worker ticks, exactly the complete
enabled/nonburning installed demand over all20400 ticks. Every healthy operating
source is fully staffed; resident worker capacity totals7166924 worker ticks.
Zero assignment during source fire or an ordinary pause is not a labor deficit.
The city harvests701 Food portions, consumes632 and misses18 of650 demands;
physical stock plus cargo grows51→120. Neither empty House stock nor an expired
timer establishes an aggregate production shortage. No original-model bug (H)
is inferred from these authored transport and dispatch observations.
