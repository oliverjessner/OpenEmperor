# Original House graphics: bounded type identity and native placement

Actual Original `HouseType` is separate from the legacy authored House levels.
This contract uses the locally licensed installation read-only. Original pixels,
full disassembly, contact sheets and proof files remain under ignored `.local/`.
The original stage/anchor evidence is in the OE29 research directories; the
qualified fresh-purchase and bounded load follow-up are in
`.local/oe59/research-construction/` and `research-sprites/`.

## Source identity and selection

The examined Emperor.exe is PE32/i386, image base `0x400000`, SHA256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The General archive pair is the existing exact compatibility pack:

- `DATA/China_General.sg3`: `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`.
- `DATA/China_General.555`: `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`.

The House initializer `428aa0` reads initial side from
`823598 + 24*type`: Common2..10 use2; Elite11..17 use4. It does not itself
publish the generic definition's graphic. The concrete House setter `518de0`
preserves its existing side. Its graphic choice is a different context from
the fresh Type2 purchase described below:

- Common branch `518e44` reads group DWORD
  `8232f8 + 4*max(type-3,0)`, resolves it through `408170`, then adds
  `variation[house.logical_origin] & 1` from original array `f1e780`.
- Elite branch `518e8e` reads group DWORD `85410c + 4*type`, resolves through
  the same manager, and adds the same low variation bit except for empty type11.
- Both branches publish the complete same-side square through `4b72b0` at
  `518ed6`. They do not select art from an OpenEmperor level, capacity,
  population, a saved image ID or a visual resemblance.

General is independently registered as slot2 at `475c4b`. The existing shared
`RuntimeArchiveLayout` and `ResourceGroupLookup` resolve the groups below; the
catalog's IDs remain physical SG3 records. No second image-index translation or
copied artwork is introduced.

| Actual type | Side | Admitted selector context/group | Physical variants0/1 | Variant0 dimensions | Front anchor |
| --- | ---: | --- | --- | --- | --- |
| 2 Vacant, never admitted Common purchase | 2 | fresh purchase `0x47a` | 1508 only | 158×88 | 79,68 |
| 3 Shelter | 2 | `0x471` | 1509 /1510 | 158×83 | 79,63 |
| 4 Hut | 2 | `0x472` | 1511 /1512 | 158×96 | 79,76 |
| 5 Plain Cottage | 2 | `0x473` | 1513 /1514 | 158×94 | 79,74 |
| 6 Attractive Cottage | 2 | `0x474` | 1515 /1516 | 158×110 | 79,90 |
| 7 Spacious Dwelling | 2 | `0x475` | 1517 /1518 | 158×111 | 79,91 |
| 8 Elegant Dwelling | 2 | `0x476` | 1519 /1520 | 158×132 | 79,112 |
| 9 Ornate Apartment | 2 | `0x477` | 1521 /1522 | 158×139 | 79,119 |
| 10 Luxurious Apartment | 2 | `0x478` | 1523 /1524 | 158×162 | 79,142 |
| 11 Elite Unoccupied | 4 | `0x479` | 1525 only | 318×168 | 159,148; actual side4 required |
| 12 Elite Abandoned | 4 | `0x47b` | 1526 /1527 | 318×193 | 159,173 |
| 13 Modest Siheyuan | 4 | `0x47c` | 1528 /1529 | 318×194 | 159,174 |
| 14 Lavish Siheyuan | 4 | `0x4ac` | 1530 /1531 | 318×213 | 159,193 |
| 15 Humble Compound | 4 | `0x47e` | 1532 /1533 | 318×224 | 159,204 |
| 16 Impressive Compound | 4 | `0x47f` | 1534 /1535 | 318×228 | 159,208 |
| 17 Heavenly Compound | 4 | `0x480` | 1536 /1537 | 318×253 | 159,233 |

All30 physical records1508..1537 in `China_Housing.bmp` were exported with the
production SG3 decoder. The selected bodies are static, unmirrored Type30 with
no separate alpha payload. Side2 foundations have12800 base bytes; side4 have
51200. Whole-image transparency still matters: the Omega overlay and the
surrounding transparent pixels are retained exactly.

## Fresh Type2 purchase and the different setter context

The qualified normal purchase selects **1508**, rather than inferring it from
the model label or its appearance. Construction completion `405378` calls
`4b1250` with the selected type. Type2 takes the special branch at `4b17aa`,
calling `4b2680` at `4b17ee` with commit flag0. It bypasses the generic creation
and virtual+100 callback block at `4b24c9..4b2557`.

The complete special dispatcher creates literal Type2 through `42d540` at
`4b2853`, requires a positive owner ID, resolves explicit group`0x47a` through
`408170` at `4b28b4`, and publishes a 2×2 body through `4b72b0` at `4b28d0`.
There is no variant increment in that path. Its completed purchase tail does
not call the ordinary House setter or virtual+100 callback. House initialization
`428aa0` and its House-specific+90 initializer `518b70` also do not publish a
replacement picture. These are bounded complete bodies and qualified callers,
not an assertion that every original placement mechanism has been reconstructed.

The ordinary setter still maps requests2 and3 to group`0x471`, physical1509/1510,
with the original cell variation low bit. The generic virtual+100 callback
`519f30` can call `519060` for an empty Common House; that routine sets type3
and publishes the same tent family. A qualified resident-removal path reaches
`519060` at `4adf1d` after clearing occupants. It is a different lifecycle
context from a never-admitted purchase. Neither an arbitrary empty type label
nor the existence of this callback proves which publisher has run.
The independently observed Common arrival at `4cb1a1` requests literal type2
through the setter before adding real occupants. Original Type2 itself is
therefore not a universal never-admitted or zero-occupancy identity.

The shared resolver maps `0x47a` to General slot2's filtered group position121,
SG3 group122, physical1508; `0x471` maps to position112, group113,
physical1509, with the low-bit alternative1510. Position in the filtered group
list is not an unfiltered image index. Fresh1508 is static, unmirrored Type30,
side2, 158×88, with 12800 base bytes and 479 Omega bytes. Its front anchor is79,68.
The independently decoded tent alternatives are158×83/158×87 with
1068/728 Omega bytes and anchors79,63/79,67. Their visual appearance is asset
evidence, not the reason for selecting1508.

## Current admission and remaining original-load boundary

Native1508 is admitted only for a current validated Original-production
Common2×2 paid `Household` whose complete House state is the empty Vacant2 state
and whose persisted housing row has no `first_admission_tick`. The existing
World validation and admission history already distinguish that state; no new
lifecycle bit or save field is added. A fresh Common placement preview uses the
same purchase identity without constructing a hypothetical House. Real
immigration changes the same paid body to Shelter3, selecting its existing
native representative. A previously occupied House is not relabeled as a fresh
purchase. Other current native stages retain their existing choices.

Original Emperor save import is unsupported. Bounded original-load research
qualifies House virtual+c0→`518a20` as active-House registration without a type
or picture change. Load reconstruction `52f030` excludes types2..17 through its
unsigned type-range gate, so its generic callback does not establish a House
transition. The map load chain clears saved physical image IDs through
`53d100→5355f0`; the inspected regeneration and landscape helpers do not qualify
the final House publisher after that clear. **Original House restore/refresh
parity remains PARTIAL.** Neither1508 nor1509/1510 is claimed for every original
load or later refresh. Exact current OpenEmperor JSON restore derives the same
native choice from validated paid/admission authority; it is not original-save
parity. The [lifecycle contract](original-house-lifecycle-contract.md) records
the corresponding boundaries.

The [OE58 authored foundation](../rendering/oe58-vacant-house-visual.md) remains
the visible fallback when native metadata, source fingerprints, artwork enablement
or the supported lifecycle/footprint is unavailable. It uses an inset cleared-earth
plot, pale foundation ring and short timber planks in ordinary SDL geometry.
The full logical footprint spans160×80 and the authored plot144×72; the same
front-cell projection and signed height apply. Its submitted polygon supplies
visible-pixel picking while construction/selection retains the complete footprint.
Selection and hover outlines remain separate. Empty Elite11 has native side4
art; its historical temporary side2 plot still uses a named fallback. No sprite
is stretched to fit it. See the [current Vacant presentation contract](../rendering/oe59-original-vacant-houses.md).

## Native anchor derivation

This relation is derived from bounded static code and the native grid, rather
than an optical pivot chosen from a screenshot:

1. In camera0 `4b72c8` selects the marker at local `(0,side-1)`;
   `4b73d6..4b7449` publishes size property`side-1`.
2. Ordinary body dispatcher`46b620` applies `(40,-20)` for property1 and
   `(120,-60)` for property3.
3. Normal overlay primitive`5ce800` then applies `(-40,80-height)` for
   width158, or `(-120,160-height)` for width318. Alternate primitive
   `5ceae0` uses the same adjustments. Their combination is marker-relative
   `(0,20*(side+1)-height)`.
4. The normal78×40 grid and40/20 diagonal steps place the marker tile's left
   point at `front_ground.x-(40*side-1)`, and its top at
   `front_ground.y-20*side`.

Therefore a native complete raster relative to the actual front-cell ground
uses anchor **`[40*side-1, image_height-20]`**:79 for side2 or159 for side4.
Signed terrain height remains applied once by the existing ground projection.
The anchor binds the selected record, dimensions, footprint and ground point
together. It supplies no general original painter-order or animation proof.

## Bounded presentation choice and open limits

`original-houses.json` selects the fresh-purchase **static1508** for the scoped
Type2 state and **static variant0 representatives** for types3..17. The latter
are explicit representatives of the observed original families. The adapter
does not own the original variation byte and does not reproduce each original
House's variation. Unknown packs, missing optional metadata, failed fingerprints,
unsupported lifecycle states and incompatible actual footprints retain named
authored fallbacks.

The optional table is prepared only for active Original production. It shares
the existing physical image pool, eager decoder and texture lifecycle and the
unchanged64MiB combined building budget. Legacy levels0/1/2 and their13-role
compatibility profile remain separate. Image selection uses the actual current
House type, footprint and existing first-admission history after upgrade,
downgrade or current JSON restore; no new save field or gameplay authority is
introduced.

Static identity, asset decoding, synthetic rendering, paid production rendering,
native input and human acceptance are separate evidence categories. The
bounded native raster anchor and original family identity do not establish
complete original scene composition, all foreground intersections or original
game screenshot parity. Current scene-matrix results and remaining limits are
reported in the [Vacant presentation contract](../rendering/oe59-original-vacant-houses.md).
