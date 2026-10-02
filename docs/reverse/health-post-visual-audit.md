# Bounded Health Post presentation audit

This is read-only presentation research using locally supplied Emperor assets. It does not identify original health behavior, registrations, building semantics or a verified clinic. No original pixels, decoded images or local profiles are committed.

## Scope and evidence

Source: locally supplied `DATA/China_General.sg3` / `.555`, under the existing exact-fingerprint GOG-derived compatibility pack. Physical indices come from the actual archive metadata, not inferred record numbering. The archive SHA-256 is already declared in `resources/compatibility/gog-derived-2.0.0.2-en-assetset-1/manifest.json`; metadata and original decode results remain distinct evidence.

The search was limited to actually present groups `China_Aesthetic.bmp` (group 1), `China_Safety.bmp` (8), `China_Government1.bmp` (10), `China_Government2.bmp` (11), `China_Guilds.bmp` (14) and `China_Aesthetic2.bmp` (18). Strict filtering required Type 30, size flag/side 1, width 78, base/uncompressed length 3,200, zero mirror offset and valid source bounds. It yielded 186 + 0 + 4 + 1 + 14 + 0 = **205 records**. All were decoded through the ordinary inspector/shared loader and inspected on five local atlas pages. Most are paving, garden elements, walls, platforms or fragments.

## Three scene candidates

Three initially plausible small roofed/civic candidates were then compared in the actual City-v15 `Cities/Xia.map` scene using the production view, ordinary paid commands and native Metal renderer. Each scene contains developing Houses, the unchanged authored Well, Service Post, Fire Watch and Market. Captures at actual camera 1×, 2× and 4×, plus Health overlay and placement, are ignored local review artifacts. Camera centering changes for the close view; pixel crops do not alter asset pixels or anchors. These were scripted native render/event checks, distinguished from subsequent actual desktop input.

| Physical record | Observed group | Decoded size | Base | Authored anchor | Visible motif / decision |
|---:|---|---:|---:|---:|---|
| 383 | Aesthetic, group 1 | 78×79 | side 1 / 3,200 | [39,59] | Small roofed civic doorway, already the curated Fire Watch. Technically valid, rejected as a confusing duplicate role. |
| 384 | Aesthetic, group 1 | 78×78 | side 1 / 3,200 | [39,58] | Related roofed red civic/doorway structure, similarly reads as the existing watch/entrance family. Rejected for weak distinction and unverified medical identity. |
| 1900 | Government1, group 10 | 78×59 | side 1 / 3,200 | [39,39] | Small ochre tiled roof with post; the scene exposes a roof/platform fragment rather than a complete freestanding treatment building. Rejected. |

All three are unmirrored, supported Type-30 records. Their anchors are OpenEmperor base-center choices, not original pivots. The previous Well audit and Fire Watch 921 rejection remain closed and unchanged; this study adds no new decoder interpretation or arbitrary asset identity.

## Result

**Original-like Health Post unresolved.** No original record is assigned in the built-in pack. Use a separate authored 1×1 timber/plaster pavilion, muted pitched roof, shaded doorway and small herb planters in `HealthPostFallbackRenderer.*`. The fallback uses the existing building ground and painter, no assets or saved presentation state; alpha 128 is shared by valid placement. Optional local `health_post` metadata can still reference a strict supported one-cell Emperor record, described only as a curated civic/health preview with original identity unverified.

The bounded audit is closed. Health Worker remains an ordinary diagnostic marker; no walker audit accompanies this milestone.
