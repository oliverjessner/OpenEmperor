# Original luxury sources and Academy materials

This is the bounded source proof behind the [luxury integration](../original-economy/architecture-map.md#paid-sources-inventories-and-actual-arrivals). The read-only PE32 executable has SHA256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`; the model file has SHA256 `7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6`. Addresses are static VAs, not RVAs or save offsets. The Original executable was not run. Original tables, instruction listings and asset bytes remain private.

The retained private `oe24/research/luxury-source-facts.json` records234 selected byte/call/slot assertions and25 contiguous windows containing1326 instructions/4100 bytes. Independent Logging and Orchard checkers each record52 additional narrow checks with separate PE mapping. These reports verify unchanged source bytes and metadata. They establish static linkage, not gameplay, decoded-image, native or human acceptance. See [testing](../testing.md) for current verification commands.

With the same lawful local installation, the private checks are reproduced from the repository root:

```sh
python3 -B .local/original-economy/oe24/research/check_luxury_sources.py
python3 -B .local/original-economy/oe24/research/logging-audit/check_logging_linkage.py
python3 -B .local/original-economy/oe24/research/orchard-audit/check_orchard_linkage.py
```

## Concrete sources and recipe selectors

**EXE-OBSERVED:** the actual Industrial factory at `0x5f0e20` selects specific constructors and RTTI-identified classes. For example, exact Logging38 selection passes through `0x5f0fc7→0x5f5260`, then `0x5f0ff3→0x5f5230`, publishing `cLoggingShed` vtable `0x7c06fc`. Paper45 selects `0x5f50b0→0x5f5070`, publishing `cPaperMaker` vtable `0x7bf978`. This is a factory-to-object proof, rather than a name-based association.

The common initializer `0x5f11f0` binds output from virtual `+0x260`, first input from `+0x264` and second input from `+0x268`. The selected values establish:

| Actual model | Selected output | Required material identity |
|---|---|---|
|38 Logging Shed|Wood10|physical harvesting; no incoming-goods selector|
|39 Bronze Smelter|Bronze11|Wood10 and separately extracted internal resource|
|42 Bronzeware Maker|Bronzeware23|Bronze11 **and Clay18**|
|44 Lacquerware Maker|Lacquerware22|Lacquer14 **and Wood10**|
|45 Paper Maker|Paper27|**Hemp19**, rather than Wood|
|47 Weaver|Silk24|RawSilk12|
|237 Tea Curing Shed|Tea13|matching Tea Bush26 and actual harvest return|
|238 Lacquer Refinery|Lacquer14|matching Lacquer Tree27 and actual harvest return|
|239 Silkworm Shed|RawSilk12|matching Mulberry Tree28 and actual harvest return|

Paper's `+0x264→0x6fc190` returns19. Bronzeware's second selector `0x5f5130` returns18. Lacquerware's second selector `0x4f14e0` returns10. Actual receipt `0x5f2140` compares the selected resource and credits separate bounded stocks; `0x5f13e0` debits the required input stocks separately. Shared amount callback `0x4f1540` supplies100 raw units. The output path `0x5f1280` creates a real Actor19 cart, carrying the selected commodity, quantity and source ID. A recipe callback is not direct Warehouse credit.

The resource pointer table at `0x872030+4*id`, price getter `0x408d00` and the local Trade file independently bind those commodity names. They do not authorize imports, free starter stocks, or a seventh House good. Original commodity IDs and the existing six-entry `Goods` enum are separate domains.

## Initial bodies and graphical selection

**EXE-OBSERVED:** `0x428abc` reads `0x823598+24*type` into runtime side byte `+0x07`. The ordinary square predicates and side getter bind the initial bodies: Bronze Smelter39 is3×3; the other eight producers are2×2; separately created plants26/27/28 are1×1. These are initial object facts, not dimensions inferred from image width or a claim about every restored state.

The maker bodywriter `0x5f1ee0` calls selector `0x4f13f0`, reads the type's registration and resolves the actual group through `0x408170`. Examples include Paper45 key`0x44a` and Bronzeware42 key`0x447`. The raw General records use64-byte metadata, while SprMain records use72. Actor registrations separately identify Cart19, Grower26 and Woodcutter41. No image was decoded for this proof; group metadata alone does not establish anchors, feet, loaded variants, animation or complete visual parity. Those remain OE29 work.

## Logging requires an actual physical harvest

**EXE-OBSERVED:** Logging's source callback `0x5f3260` searches through `0x5f34c3→0x4af0d0(mask1)`. That source availability wrapper forwards a minimum resource byte of0; it does not itself prove a currently harvestable255 cell.

The actual Woodcutter41 path is stronger. Source creation binds its ID in Actor word`+0x62`; update `0x4d3710` resolves that source. Search at `0x4d37c6/0x4d37cf` requires mask1 and resource byte255, and arrival `0x4d3824` checks255 again. Work clears the physical resource byte. Only actual return result8 reaches `0x4d393d/0x4d3944`, crediting source stock by20 or25 raw units according to the harvested cell's Objects-bit2 flag.

Those exact bits and byte conditions are evidenced. Full growth, tree choice, regeneration and calendar are not reconstructed. A purchased staffed shed alone is therefore not an Original physical-harvest proof.

## Smelter and Orchards retain their missing physical inputs

**EXE-OBSERVED:** concrete Smelter39 uses vtable`0x7b7e24`. Output selector `0x558d00` chooses Bronze11, and `0x558d30` chooses Wood10. Production prerequisite `0x5587e0` also requires at least100 raw units of internally extracted stock at word`+0x72`; extraction selector `0x558b50` yields search tag`0x100002`. Calling that tag a complete ore ecology, or treating Wood as the only input, would exceed this proof.

For237/238/239, factory `0x4c2960` selects `cOrchardHold`, and `0x4c5310` chooses output13/14/12. Pair selector `0x4c0800` binds each source to its actual26/27/28 plant kind. Dispatch through `0x4c3550` creates Grower26 and records both source word`+0x62` and plant word`+0x68`. Harvest work reaches the actual plant's `+0x268→0x4c1dd0`, which moves calculated yield into Actor word`+0x7c` and clears that plant's work/assignment. Only actual return state8 invokes source receipt `0x4c2e50`, resolves the retained plant ID and credits the corresponding physical stock.

This proves separate plant identity and real returned yield. It does not import the whole original planting purchase formula, autolayout, tending, fertility, seasonal timing or regional yields. Revision24 pays both model components explicitly.

## Original Academy selects Paper27 or Wood10

**EXE-OBSERVED:** initializer `0x5ab8c0` queries global resource27 through `0x480740→0x4812e0`. The two observed branches at `0x5ab8da/0x5ab8e2` assign resource27 or10 to Academy word`+0x16`. Actual receipt `0x5abb20` matches that selected resource and `0x5abb76` publishes stock word`+0x40`. Positive stock participates in the source's effective staffing/material gate.

Material consumption `0x5abaa0` uses actual staffing in a separate accumulator. At100 accumulated raw units it debits100 raw stock and floors a negative remainder at0. That is neither a proved one-Paper-per-visit formula nor an80-Worldtick rule. The [initial Confucian elite-only condition](original-religion-source-contract.md#a-proven-initial-producer-of-source0x174) is a different field and remains a separate requirement.

**AUTHORED INTEGRATION:** revision24 chooses Paper-only Academy supply and consumes1 Paper on each actual successful new tour dispatch. A genuine incoming Materialcourier is required. The original global27/10 selector and staffing accumulator are not represented as restored runtime behavior; material delivery does not resolve the remaining Elite contract.

## Bronze/Lacquer OR is not a consumption priority

The original [needs predicate](original-house-needs-contract.md) accepts Bronzeware **or** Lacquerware for the shared need. The independently checked [consumption path](original-goods-consumption-food-contract.md#consumption-uses-the-current-row-and-six-independent-stocks) executes the Bronze branch `0x51a0cb–0x51a102` and then the Lacquer branch `0x51a103–0x51a140`. Both existing stocks are consumed independently against that field. There is no first-available algorithm or shared debit budget. Prompt24's older fork-error suspicion is superseded by this original-byte finding; the pure kernel remains unchanged.

## Explicit revision24 boundary

Managed Forestry and managed internal Ore,80 productive ticks per chosen unit,1 of every recipe input, separately bought adjacent Orchardplants, stock buffers8, Warehouse32 per material and Paper1 per successful Academy dispatch are versioned authored rules. They preserve the evidenced material identities and distinct paid bodies without claiming reconstructed resource-byte255/search-tag ecology, raw100/400 scaling, full calendar, venue/Elite behavior or original production balance.

The six House stocks remain the sole household goods authority. Wood/Bronze/RawSilk/Lacquer/Paper have their own five-material authority; Clay retains native physical stock/cargo, and PaperMaker Hemp remains in the six-goods ledger. Recipe inputs commit together with output, real couriers deliver to actual endpoints, and counters bind actual consumption. No source count, model name or default market price grants material or workers. Current gameplay/save/process/SDL results must be reported separately after execution; this document asserts only the bounded source findings above.

## Additional-chain evidence boundary

All nine producer identities in this contract already have paid authored World
adapters. Reusing their existing recipe or adding another instance is not an
additional production chain. Logging's physical resource-byte harvest and the
Smelter's internal extraction selector remain incomplete source contracts;
their managed World substitutes do not prove a new original mine or forestry
lifecycle. Likewise the observed Orchard return path does not establish other
crops, their planting costs, yields or seasons.

The [production candidate register](../original-economy/oe55-production.md)
keeps further raw model labels separate from this evidence. No additional
factory, recipe selector, productive clock, legal source/field attachment or
end-to-end material path is established by those labels. This boundary adds no
World behavior or save authority.
