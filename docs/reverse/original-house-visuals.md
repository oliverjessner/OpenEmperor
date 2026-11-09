# Original House graphics: bounded type identity and native placement

OE29 separates actual Original `HouseType` from the legacy authored House levels.
This contract uses the locally licensed installation read-only. Original pixels,
full disassembly, contact sheets and proof files remain under ignored
`.local/original-economy/oe29/research/asset-identity/` and `house-anchor/`.

## Source identity and selection

The examined Emperor.exe is PE32/i386, image base `0x400000`, SHA256
`6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`.
The General archive pair is the existing exact compatibility pack:

- `DATA/China_General.sg3`: `c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46`.
- `DATA/China_General.555`: `3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2`.

The House initializer `428aa0` reads initial side from
`823598 + 24*type`: Common2..10 use2; Elite11..17 use4. The concrete House
setter `518de0` preserves its existing side. Its graphic choice is more specific
than the generic Building definition at `8235a0 + 24*type`:

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

| Actual type | Side | Actual setter group | Physical variants0/1 | Variant0 dimensions | Front anchor |
| --- | ---: | --- | --- | --- | --- |
| 2 Vacant | 2 | `0x471` | 1509 /1510 | 158×83 | fallback; see ambiguity below |
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

**Vacant2 remains a named fallback.** Its generic initial definition names
group`0x47a`, physical1508, while its ordinary setter aliases the Shelter row
`0x471`, physical1509/1510. The complete creation/refresh lifecycle that decides
between these two contexts has not been qualified. Type alone cannot resolve
this ambiguity. Empty Elite11 has a verified side4 original graphic, but the
adapter's temporary side2 plot must also use a named fallback until actual
footprint materialization. No sprite is stretched to fit that plot.

OE58 changes only that named **authored** Vacant2 presentation. The existing
initializer/setter observations above still do not select one native creation
image; physical1508/1509/1510 are not newly admitted for Type2. A paid Common2x2
Vacant body instead draws an inset cleared-earth plot, pale foundation ring and
two short timber planks using ordinary untextured SDL geometry. Its real
front-cell ground and signed height supply the same projection as native House
art. The complete logical footprint spans160×80; the authored ground shape is
inset to144×72 and has no roof or above-footprint parts. Its submitted polygon
also supplies visible-pixel picking, while ordinary construction/selection still
uses the complete authoritative footprint. Selection and hover outlines remain
separate. Real current HouseType alone retires this foundation when immigration
admits residents and selects the existing Shelter3 sprite. No lifecycle field,
original variation, population, goods, cost, rule or save schema is added.
See [the rendering scope](../rendering/oe58-vacant-house-visual.md); original
Type2 graphics parity remains unresolved.

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

`original-houses.json` selects the verified **static variant0** for types3..17.
This is an explicit representative of the observed original family. The
adapter does not own the original variation byte, so this does not reproduce
each original House's variation. Type2 is explicitly unsupported. Unknown packs,
missing optional metadata, failed fingerprints and incompatible actual
footprints retain named authored fallbacks.

The optional table is prepared only for active Original production. It shares
the existing physical image pool, eager decoder and texture lifecycle and the
unchanged64MiB combined building budget. Legacy levels0/1/2 and their13-role
compatibility profile remain separate. Image selection uses the actual current
House type and footprint after upgrade, downgrade or restore; no new save field
or gameplay authority is introduced.

Static identity, asset decoding, synthetic rendering, paid production rendering,
native input and human acceptance are separate evidence categories. The
bounded native raster anchor and original family identity do not establish
complete original scene composition, all foreground intersections or original
game screenshot parity. Actual scene-matrix results and remaining failures are
reported in the OE29 validation record.
