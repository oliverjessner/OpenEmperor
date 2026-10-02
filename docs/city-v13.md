# City-v13: residential quality and zoning

`sandbox-city-v13`, rule **1**, is an opt-in, OpenEmperor-authored extension of City-v12 rule 1. Economy, goods, couriers, FireWatch/FireInspector, Market distribution, Service, workforce priorities, operation controls, footprints and safe demolition are inherited. There is no new building, good, courier, original asset or map-format interpretation. These desirability rules are **not claims about Emperor's original mechanics**. City-v11 rule 3 remains the fresh-settings menu default; existing settings and CLI defaults are preserved.

## Pure spatial score

`WorldDesirability.cpp` implements the SDL-free read-only queries `household_desirability(id)`, `household_desirability_at(origin)` and `household_desirability_sources(id)`. The ID queries require a placed City-v13 Household. The preview uses its planned 2×2 footprint and existing buildings only. `building_distance(a,b)` and `footprint_distance` find the **minimum Manhattan distance between any cells of the two rectangular footprints**, using interval gaps, not origin distance. Coordinate arithmetic uses int64 intermediates.

| Existing building | Base impact |
| --- | ---: |
| Clay Source | −18 |
| Pottery | −22 |
| Warehouse | −10 |
| Farm | −4 |
| Market | +12 |
| Service Post | +10 |
| Fire Watch | +8 |
| Household | 0 |

Distance 0–2 applies four quarters, 3–4 three quarters, 5–6 two quarters, 7–8 one quarter; beyond **radius 8** the contribution is zero. Each contribution is `base * quarters / 4`, with signed integer division **toward zero**: Pottery contributes −22/−16/−11/−5 across the four bands. Sum the contributions, then clamp to **[−100,+100]**. Source inspection keeps the unclamped individual contributions. Neither roads, original terrain, staffing, operation pause, fire, goods nor delivered protection change the score. Houses exert no influence on each other.

At existing building limits the positive total can reach at most +84 (four Markets, two Service Posts, two Watches); the +100 clamp is intentionally defined and independently tested, though unreachable in a legal v13 city. A paid synthetic compact industrial fixture reaches raw −130 and clamped −100.

A nearby Service Post improves spatial quality but grants no Service coverage without a real road-based visit. Likewise, a nearby Market supplies no goods by proximity. FireWatch contributes spatial quality even while paused, unstaffed or disconnected. Its actual fire protection still requires Inspector arrival. Fire has its existing independent operational/demand effects, without a second spatial penalty.

## Effective levels, residents and tax

`historical_household_level` preserves the fulfilled-demand thresholds: 0 for fewer than two successes, 1 for two through four, 2 for five or more. `household_level` is the minimum of that historical level and the current spatial cap:

| Score | Maximum effective level | Resident capacity | Supplied-demand tax at that level |
| --- | ---: | ---: | ---: |
| Below −20 | 0 | 6 | 25 |
| −20 through 9 | 1 | 10 | 40 |
| 10 or above | 2 | 16 | 60 |

The cap changes immediately when buildings are placed or safely demolished. Demand history is never deleted. Removing negative industry can therefore restore a historically earned level immediately, without advancing time. The authored goal is still at least ten placed Houses, **eight currently effective Level-2 Houses**, and population at least 100. It is a query, without a win latch, and can be lost after a layout change.

Population is never truncated on construction. A House may temporarily hold more residents than its new capacity, up to the inherited absolute maximum 16. At each 400-tick demand deadline, after recording the ordinary supply result:

1. If over capacity, one resident leaves, whether supplied or missed.
2. Otherwise a miss removes at most one above the minimum 2, after the inherited 800-tick move-in grace.
3. Otherwise a success grows by one if below capacity.

These branches are mutually exclusive: at most one population change per deadline. Over-cap decline applies even during move-in grace. Workforce still uses the ordinary tick-start snapshot; population changes affect operations on the next tick.

Success still requires actual Pottery, Food and active Service, and a non-burning House. Consume the goods and increment historical fulfillment first conceptually; the payment uses the **resulting historical level capped by current desirability**, preserving the old milestone convention that the second and fifth successes can activate a new rate. There is no new tax, subsidy, credit, free stock, workforce or rescue mechanism. A miss pays nothing and consumes neither good.

## Schema 15 and accounting

Schema **15** belongs only to City-v13 rule 1. It inherits schema 14's structure and adds required `taxes_paid_total` to each Building record: actual cumulative payments on Houses, zero on other kinds. Score, cap, source list, effective/historical levels, capacity and overlay are **not saved**. Restore rebuilds occupancy, derives quality from geometry and validates the ordinary goods, population, fire, service, navigation, ID/revision and treasury invariants before publication.

Current layout cannot reconstruct past rates. `household_tax_contributed` therefore returns actual per-House authority for v13, while v12 and older retain their exact historical formulas. Necessary per-House payment bounds (25–60 per success, multiples of five), overflow-safe totals and zero non-House authority are validated. This does not pretend to reconstruct an unsaved per-demand history.

The economy identity is:

`taxes_collected_total == demolition_history.taxes + sum(active House taxes_paid_total)`.

Safe House demolition transfers its tax total once into the existing demolition-history aggregate. World lifetime taxes and treasury remain unchanged, with no refund. Schema 14 and schemas 1–13 emit no new tax field, retain their profiles and behavior, and reject a schema-15 field. There is **no automatic migration** to v13. Autosave/Recovery carries schema 15 through the ordinary atomic save and validated restore paths; its metadata and scheduler remain outside World.

## Starter, inspector and overlay

The unchanged paid 15×5 City-v12 starter costs **1,280**, leaves **20** of the normal 1,300 funds, and starts at tick 0 with **24 residents / 24 required workers**. There are no goods, coverage, taxes or fulfilled demands at placement. In stable House order its measured scores are **−14, −5, 2, 10**. Thus all Houses permit at least Level 1, one permits Level 2, and further residential planning has value. In a normal 10,000-tick synthetic supplied run their populations become **10,10,10,16**, with historical Level 2 on all four. This is a bounded supply demonstration, not an assertion that arbitrary districts are solvent or reachable.

Select **City v13 - Residential quality and zoning** in New Sandbox, or use:

```sh
./build/openemperor \
  --data "$PWD/.local/gog-extracted/app" \
  --sandbox Cities/Xia.map \
  --sandbox-rules sandbox-city-v13 \
  --sandbox-demo \
  --sandbox-save "$PWD/.local/saves/city-v13.json"
```

Use an existing legally obtained installed/extracted data folder. F5 saves, F9 loads paused, Space runs/pauses, and `.` steps. Existing original visuals are reused through the same compatibility/profile loaders; FireInspector remains a marker.

The House inspector puts effective level, historical development, exact score, cap and population/capacity first. It shows at most **three negative and three positive sources**, sorted by absolute contribution descending, with Building ID as tie-break, followed by actual goods, Service, fire, demand timing and taxes. Key text wraps. Positive spatial facilities are not described as actual supply.

**D**, or the visible **Desirability** toolbar action, toggles a transparent footprint overlay on Houses only: the presentation palette uses green for Good (≥10), amber for Neutral (−20…9), red for Poor (<−20). F1 additionally prints exact scores at the footprints. There is no full-map heatmap. In v13 use arrow keys for horizontal navigation; D is reserved for the toggle (older profiles keep WASD). The House placement tool puts its predicted score first in the bottom status before construction, including locations whose command is subsequently rejected. This prediction is a layout aid, not a build/supply guarantee.

## Performance and verification

No score cache exists. A query scans at most 40 buildings and uses constant-time rectangular interval arithmetic. Inspector source ordering is bounded by 40 entries. An enabled overlay visits at most 20 Houses and draws their four footprint cells (at most 80 diamonds); no 228×228 score grid is computed. Placement predictions are coalesced by hovered cell and topology revision, with no World copy, command, BFS, file I/O, decode or upload. Industry placement does not add a separate affected-House preview in this milestone. The existing pure road preview and one-refresh transactional road commit remain unchanged.

Synthetic tests cover footprint-vs-origin distance, exact falloff/rounding/radius/clamp, source order, neutral/good/poor caps, paid starter supply, real 25→60 payment changes, over-cap supplied/missed decline, actual relocation recovery, independent goods/Service/fire requirements, typed/JSON schema identity and malformed authority, old-schema exclusion, safe paid-House demolition, autosave/recovery, 20,000 identical-command ticks and full 40-building/24-courier 100,000-tick endurance. A separately labelled goal fixture uses normal paid geometry and independently constructed validated accounting to isolate loss/recovery of the goal; it does not claim to simulate a fully supplied 14-House city.

The separate two-district test builds and funds both districts through ordinary commands and earned starter taxes. The residential House is beyond radius 8 of every industrial footprint and scores **+25**, while the mixed starter House scores **−14**. After genuine deliveries, both reach historical Level 2; their effective levels/populations are **2/16** and **1/10**. Five fulfilled demands each pay **300** and **200** respectively. Paid nearby industry then lowers the residential score to **−31** without truncating its 16 residents. Its next buffered supplied demand pays **25** and leaves 15 residents. Safe removal and distant rebuilding recover score +25, Level 2 and growth to 16. These are measured synthetic gameplay outcomes, not original Emperor behavior.

SDL tests toggle the overlay 100 times and move the preview pointer 200 times within a cell, checking unchanged snapshots/counters and no asset/routing work. Menu tests keep explicit v13 settings, paid startup, schema-15 save/list/load, and the unchanged fresh City-v11 default.

Run all checks:

```sh
ctest --test-dir build --output-on-failure -j 4
ctest --test-dir build-release --output-on-failure -j 4
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure -j 4
```

### Validation on 2026-10-02

Starting HEAD is `ba91bef85d157207a90af098c46b31e418874975`; the initial worktree was clean. Implementation remains uncommitted. Debug, Release and ASan/UBSan builds completed without compiler warnings. Five new CTests cover base rules, persistence, supplied districts, determinism and endurance, bringing the full suite to 58. Existing SDL/menu tests also exercise v13, and existing road-responsiveness regressions remain unchanged.

| Configuration | Full-suite result |
| --- | --- |
| Debug arm64 | 58/58, 236.31 seconds |
| Release arm64 | 58/58, 21.10 seconds |
| ASan/UBSan arm64 | 58/58, 58.69 seconds; leak detection disabled as in the existing macOS sanitizer setup |
| Separate Release package build | 58/58, followed by packaging checks |

The initial Debug run passed every C++ test but hit the existing Python corpus helper's two-second timeout. Its isolated retry and the subsequent full Debug repeat pass without source/test changes.

The new 20,000-tick determinism and 100,000-tick endurance tests pass in all three configurations. Endurance reaches 40 buildings/24 couriers and performs 20 exact manual-save/autosave/recovery roundtrips while checking ordinary goods, economy, actual tax history, population, fire, Service and navigation. A local Release measurement scanned 19 placed buildings in approximately **69 ns/query** over 100,000 queries; this is one host measurement, not a cross-platform performance promise.

`tools/package_macos.sh` produced the ignored local candidate `dist/OpenEmperor.app` and `dist/OpenEmperor-0.1.0-alpha.2-ba91bef85d15-macos-arm64.zip`. Its staging-only dependency fixups, relocated/unzipped execution, bundled notices, negative packaging fixtures and existing menu/City-v11 smokes pass. `file` reports **Mach-O 64-bit executable arm64** for Debug, Release and the bundled executable; strict recursive ad-hoc signature verification passes. The expected `install_name_tool` notices precede re-signing and are not compiler warnings. Original inputs remain unchanged. This dirty-workspace artifact is a local test candidate, with no publication, tag, commit or release.

The final bundle also passes a separate `Cities/Xia.map` City-v13 finite check using the user's local original data: normal paid starter, 1,200 ticks, actual Food/Pottery/Service supply, 380 lifetime taxes, valid economy/goods balances, and exact direct/reparsed continuation. Its ordinary check places a paid second FireWatch, so its final scores differ from the tick-0 starter. This automated SDL check is not native manual acceptance.

Actual desktop testing launched the bundle with the original data and v13 starter, ran and paused the city, and exercised D/F1/F5/F9 and a new-process save restart. At tick 1,080, schema 15 holds 220 lifetime taxes and House payment totals 65/65/65/25. The complete save document is identical after paused save/load and a new-process restart. A native screenshot exposed an initially opaque overlay; the renderer now explicitly enables and restores blend mode for that overlay, a synthetic pixel regression guards it, and the final bundle visibly preserves the original House graphics beneath transparent amber/green tint.

**Still open:** the required 20–30 minute native district-replanning and subjective balance acceptance. Desktop keyboard input works, but coordinate clicks attempted in this session do not change the selected House/panel. No product input workaround was added to accommodate automation. House inspector, placement, two-district supply and safe relocation are exercised by synthetic SDL/World tests; that does not replace manual play. The native acceptance should expand housing beside industry and along a separate residential branch, compare actual supply against spatial caps, pause/drain/demolish/relocate industry, observe recovery, allow a fire incident, exercise D/F1 and F5/F9, and assess whether threshold 10 adds understandable planning. No balance acceptance or original-game fidelity is claimed.
