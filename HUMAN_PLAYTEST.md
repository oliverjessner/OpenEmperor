# Agriculture human playtest

All human checks below are **NOT_RUN**. Mark each **PASS / FAIL / NOT_RUN**
only after you perform it. Automated World tests, scripted SDL and native Metal
captures have separate results in the [technical acceptance](docs/testing.md).

The final automated matrix passes 451/451 tests in each of Debug, Release and
ASan/UBSan, independent 74-case core comparisons and historical persistence.
The real paid Rule38 city passes all 14 whole-city gates through 120000 ticks,
including ten complete post-maturity years. Native Metal/Cocoa keyboard and
full-byte F5/F9 checks pass. The resource-complete native probe also passes
with qualified Original graphics at 1×/2×/4× and unchanged input guards.
Automated mouse selection remains **NOT_VERIFIED** after the desktop
tool returned `noWindowsAvailable`. These results do not complete any human row.
House retention uses actual source-row needs; uninterrupted Elite17 is not required.

## Build and isolated session

Implementation commit: **ac10fc5782ff77a559319f4df078373cc8912949**. Record your checkout
with `git rev-parse HEAD`. The validated Release executable is
`.local/agri-mac14-close/close05-release/openemperor`, SHA256
`d0e4917afccc1456fb174a3582539d8c2045f116b48a5a6d0e78bc362c407c74`.
Its displayed build revision is merged baseline
`aa58837df2dae184669cb9b096045d7b98f93923` plus the reviewed source changes.
The exact 931-file source freeze is
`78735142a4b8e290188e5715c92f1c971f1566e1ed26a5fb100313ffcab4a4fb`;
see the [technical acceptance](docs/testing.md#final-agriculture-integration-agri-mac14).
Keep the build's adjacent `resources/Compatibility` directory: it contains
public metadata needed to admit your own Original graphics. It contains no
Original assets. This is a locally validated build, not an installed release.
Target: native macOS **arm64**, SDL3/Metal, **Rule38 / policy3 / schema54**,
marker `authored-quality-aware-food-dispatch-v1`, revision1, with the inherited
irrigated-harvest marker/revision. Loading an older save retains
its older rules; it does not test Rule38.

From the repository root, launch the validated Release executable with your
own read-only Emperor data and a fresh private session directory:

```sh
.local/agri-mac14-close/close05-release/openemperor \
  --data "$PWD/.local/gog-extracted/app" \
  --app-root "$PWD/.local/agri-mac14/human-menu-final-05"
```

Use a fresh session path. Select **8 Quality-aware Food (38)**, **Normal**,
**Cities/Chengdu.map**, then **Start sandbox**. **Up/Down** changes the selected
map; **P** switches between the paid starter and **Empty city**, and **Enter**
starts. The paid four-House Chengdu starter spends1404 Funds at tick0, with
no residents or stock; Empty city is suitable for manual paid construction.
The ordinary starter is a separate bounded layout, not the all-chain QA plan.
Current Rule38 empty-source checks also cover **Cities/Xia.map** and
**Cities/Loulan.map** with exact schema54 restores and64 continuation ticks.
The separate actual starter attempt refuses Xia because a road overlaps live
wildlife/a Hunter claim, and Loulan because no legal empty24×13 area exists. Retain the exact refusal and
choose **Empty city** explicitly for ordinary manual paid construction.
Historical Xia starter/luxury success does not establish a current starter;
a starter refusal does not mean the whole map is unsupported.

Use ordinary paid construction, immigration and production. Do not edit funds,
residents, inventory, House level or source models.

For a shorter all-chain check, the actual Release-paid city is retained privately
at `.local/agri-mac14/paid-complete-city-pcdvojm4/creator/seed.save.json`, tick20000,
SHA256 `9524df8b085ff81f82d5410a61b7dc7e52911fd45bb0ad13daee7d7ecee7de18`.
The same directory's `whole-city-final.save.json` is the earned tick120000 result,
SHA256 `814ff3d5cc0a3060e8c4ecabbef3ad52641e74a31ebe64f8d8ce9f6c7289e491`.
For loaded-Grower F5/F9, use
`.local/agri-mac14/irrigated-checkpoint-events-krufuxkw/visual-creator/food-grower-loaded.save.json`,
tick1715, SHA256 `0971dcfa75fae3a54cb077e8c3b2c92ab71e048bced130b6e217e078ab4f8cd3`.
These are earned private checkpoints, not files distributed in Git.

Copy a checkpoint to a fresh writable target before direct loading:

```sh
OE_PLAYTEST_ROOT="$PWD/.local/agri-mac14/human-loaded-final-05"
mkdir "$OE_PLAYTEST_ROOT"
cp .local/agri-mac14/paid-complete-city-pcdvojm4/creator/seed.save.json \
  "$OE_PLAYTEST_ROOT/city.save.json"
.local/agri-mac14-close/close05-release/openemperor \
  --data "$PWD/.local/gog-extracted/app" \
  --load-sandbox "$OE_PLAYTEST_ROOT/city.save.json"
```

If the directory already exists, choose a new suffix. Direct `--load-sandbox`
makes that copy the F5 target; `--app-root` is a menu-only option. Preserve the
earned originals. The mature city's lifetime counters already include all seven
raw Foods and meals; a later continuation can consume no new Meat because meals
have priority. Use a new paid city to observe fresh per-kind consumption rather
than interpreting an unchanged mature counter as a missing lifetime receipt.

## Controls and observations

Use visible toolbar buttons when a keyboard layout delivers a different key.
**Help** lists the current tools, **5** selects, arrows pan, **Space** pauses,
**.** steps one paused tick, and **F5/F9** save/load. Use the wheel to zoom;
in-game Z cycles 1×/2×/4×, while menu Z selects
the older Seasons mode. H/? opens Help; PageUp/Down and its buttons navigate.
Scroll inspectors to see lower rows. House PageUp/Down selects
**State / Needs / Goods / Visits**.

| Human check | Actions and expected evidence | Result |
| --- | --- | --- |
| Paid setup and refusal | Road **1**, House **7**, Market **0**, Well **Q**, Ancestor **Y**, Fire Watch **F**. Connect roads and allow real residents/staff. Occupied or unsupported placement changes no funds, IDs or building count. | NOT_RUN |
| Five crops and multiple fields | Food Farm **O**; select that paid Farm, then Wheat **W**, Millet **L**, Rice **Shift-R**, Cabbage **Shift-C**, Soy **Shift-S**. Up to eight active fields share one Farm's labor. Rice needs source fertility100; Soy returns BeanCurd. Inspect each field's independent progress/harvest and the real Grower's source, lifetime ID, phase and cargo. Farm stock rises only after actual return. | NOT_RUN |
| Seasons and water | Observe several full calendar years, off-season suppression, partial expiry and retained stock. **Shift-P** Pump, **R** rotate, **Shift-D** Ditch, **Shift-V** water overlay; verify a connected21-cell supplied network in the accepted scenario. Compare eligible wet/dry growth: four actual wet growth ticks earn one bonus unit, never extra cargo or a stacked Pump bonus. A cut dries affected supply immediately; repaired supply waits for a real refill. | NOT_RUN |
| Fish and Meat | **Shift-F** shore Quay, **R** rotate; **Shift-H** Hunter's Tent. Inspect finite fishing areas and living prey, depletion/regeneration/births, Boat work and Hunter pursuit/contact. Picked cargo is separate from dock/Tent stock; only actual return and unload credits Fish/Meat. | NOT_RUN |
| Seven raw Foods consumed | Follow Wheat7, Millet5, Rice6, Cabbage3, BeanCurd1, Fish2 and Meat4 through source return → physical Market/House arrival → actual House consumption. Record each kind's consumed count, not merely dispatch, cargo or held inventory. All seven must be positive in the accepted scenario. | NOT_RUN |
| Materials and household Goods | Hemp source **N**, field **M**; use **Industry** for Tea Shed/Bush, Lacquer Refinery/Tree and Silkworm Shed/Mulberry, selecting the paid source before its plant. Observe actual pickup/return, Wood+Lacquer→Lacquerware and RawSilk→Silk processing, Warehouse/Market delivery and separate source/input/output/cargo counters. | NOT_RUN |
| Higher Houses and luxury consumption | Read the occupied House's actual current/next needs: FoodQuality, Services, Water and DES. Supply its real requirements and allow paid progression; do not force its level. Tea demand begins at Common10, Silk at Elite14, the Lacquerware/Bronzeware alternative at Elite15 in the supplied Normal model. Require positive per-good Tea, Silk and Lacquerware **House consumption**, beyond receipt or Warehouse stock. For retention, observe20,400 ticks after actual maturity; record shortages, devolutions, fires, residents, Funds and upkeep as well as the highest level. | NOT_RUN |
| Meals and Food Warehouses | **Shift-M** Mill, **Shift-K** FoodShop beside an earlier paid cardinal-adjacent Market. Select Mill, **Recipe grade / Shift-G**: grades1–4 use distinct real ingredients, debit once at commitment and complete through staffed work. Observe Mill→Shop→House meals and actual meal consumption. **4** Warehouse: shared Food32, at most4 per kind including claims/return room. Follow raw Food to Mill/Market and meals to Shop, with no second inventory or House grant. | NOT_RUN |
| Route, staff, fire and full storage | Save first. Cut a future road/ditch segment, pause a source or change available labor through ordinary controls; observe a concrete blocker and preserve begun edges, loaded cargo and return capacity. Fill storage through production. Restore the route/staff and observe real continuation. For natural fire record tick, supply loss, Inspector arrival and recovery; do not alter a save to manufacture it. | NOT_RUN |
| Safe reconfiguration | Select an idle Food field, plantation plant or Pump; inspect its decision, then confirm **Demolish**. For real partial/loaded work choose **Drain**, repair any blockers, wait for settlement and remove explicitly. Retained work/receipt history survives; rebuilding costs full price and creates a new ID. Farms, sheds, Quays, Tents, Mills, Shops and Warehouses remain refused even empty. | NOT_RUN |
| Exact save/load and Recovery | Pause separately at active Grower work, loaded Grower/Boat/Hunter/Food or luxury movement, Mill work, season boundary, broken water and pending drain. Record tick, IDs, cargo, edge progress and stocks; **F5**, **.**, **F9** must restore those values. Continue without a duplicate receipt. Quit, restart the same session root and load paused. With autosave enabled, use **L Load Save → Show recovery history → Open selected** for a real listed checkpoint; it loads the same generation into a separate manual target. | NOT_RUN |
| Picking, zoom and cancellation | At each zoom select visible Farm/Field, workers, Boat, Hunter, living prey, processors and Warehouse. Check intended identity/cargo; covered pixels can select the covering roof. Test small/large windows, panel scrolling, Help, focus loss, resize and release over UI. Cancelled gestures spend nothing and the next valid click works. Record physical Retina/external-display and held-key checks separately. | NOT_RUN |

## Limits and reporting

Rule38 has **82 lifetime building purchases / 58 movement channels**, five
Farms,32 lifetime Food fields and an eight-entry pending/closed retirement
journal. Closed leaves keep history and consume lifetime budgets. Stock-owner
retirement, refunds, stock teleporting and automatic save migration are absent.
Salt, Spices and meal grade5 are unavailable. Timings, yields, ecology, recipes
and unqualified worker/Boat/Hunter artwork are authored; this is not complete
Original-game parity. Paid luxury, shortages and retention need their own
recorded latest-generation observations; older Xia evidence is historical.

Native automated mouse delivery remains **NOT_VERIFIED** until valid delivered
events and their effects are observed. This does not complete a human row.
For a pointer problem, optionally launch with
`OPENEMPEROR_INPUT_DIAGNOSTICS=1` and retain its private stdout separately from
platform stderr; report the actual window size, zoom, raw/converted coordinates
and selected identity. Never infer a DPI correction from bad automation input.

For each failure retain the unedited isolated save and a screenshot:

```text
Commit/build/architecture:
Map, difficulty, Rule/policy/schema:
Tick, window size, zoom, pause/speed:
Building / actor channel / lifetime ID:
Expected and actual counters/behavior:
Exact steps to reproduce:
Private save/Recovery path and screenshot:
Human observation or automation; raw events if available:
```
