# Bounded Food and Market figure audit

This presentation follow-up starts from clean HEAD **`40eb5da99ac4f8cb2967e60034e62ad5d8fd4cb0`**. Original files remain read-only. Decoded pixels, contact sheets, exact frame/anchor tables and machine reports remain ignored under `.local/market-walkers/`. Research stops after identifying two coherent transport figures; original profession and gameplay are not inferred from their appearance.

## A — Original pixels and separate dependencies

Both proposed families use the user's existing internal version-214 **`DATA/SprMain2.sg3`** pair. This is a separately measured dependency, not established by the existing SprMain/core fingerprints.

| Dependency | Bytes | SHA-256 |
| --- | ---: | --- |
| `DATA/SprMain2.sg3` | 991,080 | `09fc9fccddb9a30266740325b01cf69c53b26ed08016f9ecdd1759e7afb40b39` |
| `DATA/SprMain2.555` | 24,125,741 | `53b1a9b5154316e505ac1a0d46b6796c1f568f4765ec154e313be0aff82daa7e` |

The supplier candidate is a clothed human pushing an empty wheelbarrow, beginning at physical record **3605**. The distributor candidate is a different human wearing a broad hat and a back basket, beginning at **5585**. These descriptions identify viewed pixels, not an Emperor profession or proof of actual Food/Pottery in the artwork. The selected native records are Type-256 Omega sprites, unmirrored and alpha-free, with byte-59 shadow flag one. All 48 selected source images decode successfully through the unchanged shared loader.

Native skip transparency and opaque RGB555 colors remain exact. The existing bounded `prepare_omega_shadow_composition` presents the exact opaque `0x7c00` shadow marker as black alpha 128 once at load time. No other red/black color, decoder or alpha-address rule changes. See [the existing shadow evidence](research-log.md#2026-09-23-verified-omega-shadow-marker-composition).

## B — Ordered walk phases and actual facings

The archive index at position **95**, byte offset **270**, selects 3605; its next entry is 3701. Position **116**, byte offset **312**, selects 5585; the next entry is 5681. Both base records declare **12 phases** and signed stride **8** at record offset `+32`, giving separate complete 96-record intervals. This agrees with the previously bounded original base-plus-phase-times-stride helper; it does not establish the caller's profession. The selected phases are ordinary gait: legs, arms and torso change across all twelve phases and the loop boundary.

The eight columns must not be mistaken for four storage directions. Visually, column 0 faces upper-right and column 2 lower-right. Their opposite upper-left/lower-left columns 6/4 are original mirror-reference records, with zero payload and signed relative offsets **−6/−2** at `+16`. The existing SG3 reader parses that field but does not apply it; the current Walker loader rejects such records. They are never decoded as usable original images in this audit.

| Storage direction | Original column | Native source, phase `p = 0..11` | Private display comparison |
| --- | ---: | --- | --- |
| `neg_x` / upper-left | 6 | `3605 + 8p` / `5585 + 8p` | Horizontal reflection |
| `neg_y` / upper-right | 0 | `3605 + 8p` / `5585 + 8p` | Native pixels |
| `pos_x` / lower-right | 2 | `3607 + 8p` / `5587 + 8p` | Native pixels |
| `pos_y` / lower-left | 4 | `3607 + 8p` / `5587 + 8p` | Horizontal reflection |

Every one of the 24 mirror references per family has equal source/alias dimensions, equal metadata Y, and `alias X = width − source X`. Thus the private reflected-source comparison has explicit registration evidence. All four twelve-phase rows were viewed in `supplier-four-directions.png` and `distributor-four-directions.png`, with a common ground reference. They show appropriate opposite facings and distinct visible gait. This is a concrete presentation proposal, not a silently relaxed SG3-mirror decoder or current normal-app acceptance.

The bounded native check also covered SprMain and SprMain3. No stride-eight walking family in those three archives supplies all cardinal columns 0/2/4/6 as native payloads. SprMain3 contains only the named Slash/UncleSam/Spy groups. Further blind corpus search is unnecessary for this proposal.

SprMain physical **625** was initially considered from one carrier-like frame, then **rejected** after all eighteen phases showed a vessel-pouring action rather than walking. Neither that action nor the existing FireInspector is substituted for a Market gait.

## C — Curated role, cargo, feet and display contract

Original transport-profession registration is unproved. The supplier/distributor association is **curated OpenEmperor presentation**. Named bitmap groups alone do not bind these physical records to a profession. A single neutral coherent figure is proposed for actual loaded outbound and empty returning trips; no second cargo-specific sequence, added sack, invented unloading action or additional delivery is claimed.

The private supplier ground anchors use each native frame's metadata X/Y. Distributor anchors use metadata X and Y minus eight pixels. These are explicitly chosen common-reference points, not recovered original pivots. A horizontal display reflection uses `[width − source_anchor_x, source_anchor_y]`; pixel lookup would separately reflect the discrete source column as `width − 1 − column`. Different canvas dimensions never cause automatic recentering. For example, supplier 3605 uses `[34,46]` and its reflected display `[36,46]`; 3607 uses `[30,55]` and reflected `[36,55]`. Distributor 5585 uses `[18,47]` and reflected `[20,47]`.

The two families need 48 aliases each but only 24 native physical images each. Measured deduplicated RGBA is **352,952 bytes** for the supplier and **183,560 bytes** for the distributor: **48 new source assets / 536,512 bytes / 96 aliases** in total. With the existing core and Inspector this proposal totals **128 unique walker assets / 1,043,380 bytes / 192 aliases**, within the unchanged global 256-alias/256-asset/64-MiB bounds. The supplier canvases are 63–72 by 46–62; distributor canvases are 37–39 by 48–53. Each family has 24 distinct prepared native images and 48 distinct reflected/native displayed images.

Two ticks per frame would be authored timing. Any new explicit display-flip field requires a bounded visual-format contract and approval of that scope: schema 1–3 and the unsupported SG3 mirror-record policy remain exact. No resource manifest or normal-app activation is asserted by the asset study. Integration and required technical/ordinary-app acceptance are tracked separately in [the presentation contract](../market-walker-visuals.md).
