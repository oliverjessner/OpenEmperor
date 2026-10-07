# Bounded FireInspector figure audit

The presentation pass starts from **`eba0decf1c4879b349e803600aa544656d50b8ea`**, with a clean worktree. Research is read-only and stops after one complete human walking set. Original files, decoded frames, contact sheets, foot-reference comparisons and machine reports remain ignored under `.local/fire-inspector/`. FireWatch artwork and the original `fire1` animation are not researched or changed by this pass.

## A — Original human pixels

The selected figure comes from the user's existing **`DATA/SprMain.sg3`**, version 214, and its internal same-stem `.555`. These are already part of the recognized core asset set; their own exact fingerprints, rather than hashes of unrelated files, establish identity.

| Dependency | Bytes | SHA-256 |
| --- | ---: | --- |
| `DATA/SprMain.sg3` | 991,080 | `3e2817d2644453acc92068d6c1e26532212be213b43b848ebe95ba21654adef8` |
| `DATA/SprMain.555` | 26,717,501 | `d84eb6759e9ce50b0ad75596773b9224f9c1a9b8dd80b691c066c3de7e8df438` |

All **48 required physical records** decode successfully with the unchanged shared loader. They are unmirrored Type-256 Omega sprites, with canvas widths **27–40** and heights **44–53**. Every raw and prepared RGBA buffer is distinct. The images show the same clothed human figure, carrying a round gong-like object, across complete changing arm/leg phases. This is a useful patrol/inspection figure; no sack, cart or diagnostic cargo decoration is added.

Every selected record has no separate alpha stream and has byte-59 shadow flag one. Native Omega skip transparency gives raw RGBA alpha 0/255. The existing validated `prepare_omega_shadow_composition` applies unchanged: exact decoded opaque `0x7c00` markers become black alpha 128 once at load time. There are **129–244** such marker pixels per selected frame; prepared alpha is 0/128/255. Other red shades and ordinary pixels remain unchanged. This is the already bounded alpha-free flagged Sprite presentation rule, not a new color key, decoder interpretation or general red/black transparency rule. See [the shadow evidence](research-log.md#2026-09-23-verified-omega-shadow-marker-composition).

## B — Four complete walking sequences

The archive index word at position **8**, file byte offset **96**, is **433**; the next entry is **529**. Base physical record 433 declares **12** animation phases and a signed stride word at record offset `+32` of **8**. Thus the 96-record interval contains twelve interleaved phases for eight facing columns. The four columns needed by the current cardinal storage edges are explicit:

| Storage direction | Ordered physical records | Observed screen facing in the existing projection |
| --- | --- | --- |
| `neg_x` | 433, 441, 449, 457, 465, 473, 481, 489, 497, 505, 513, 521 | Upper-left |
| `neg_y` | 434, 442, 450, 458, 466, 474, 482, 490, 498, 506, 514, 522 | Upper-right |
| `pos_x` | 435, 443, 451, 459, 467, 475, 483, 491, 499, 507, 515, 523 | Lower-right |
| `pos_y` | 436, 444, 452, 460, 468, 476, 484, 492, 500, 508, 516, 524 | Lower-left |

The previously bounded original helper at VA **`0x5cfdf0`–`0x5cfe58`** bounds the phase and secondary index, then uses base plus phase × stride plus secondary index. Its count/stride relation agrees with these explicit columns. The pinned EXE was rehashed read-only and still has SHA-256 `6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e`; [the earlier bounded helper audit](fire-visual-audit.md) records that observation. This reuse does not establish which original profession calls the helper with this base.

All four twelve-phase rows were viewed, including their loop boundary. The facing remains consistent while arms, knees and foot contacts change, rather than merely changing a frame index or shadow. A second contact sheet aligns each frame to one common ground reference and confirms a stable body/foot placement with ordinary gait motion. Together, the exact index bound, count/stride, observed sequential helper and complete decoded visual comparison support **B**. No direction is synthesized by mirroring, and no diagonal simulation is introduced.

The current three-role preview already uses the first four phases of these columns as its curated Household figure. That historical profession choice is not an original registration. The Inspector adds the remaining complete walk phases and its own foot anchors without rewriting the existing Household role or its anchors. Physical texture sharing does not require equal role assignments or foot points.

## C — Profession, timing and feet are curated

Named bitmap entry **4** is `Inspector`, which helped locate this candidate. Its count/first/last range fields are zero, and selected records report `group_id=0`. No independently verified original profession-to-resource registration or original-game comparison establishes that this is Emperor's fire inspector. The FireInspector association therefore remains **curated OpenEmperor presentation**, even though the pixels and coherent walk sequences are original.

Each explicit foot anchor uses the frame's metadata X offset and its Y offset minus eight pixels, chosen and checked as a common foot reference. For example, physical 433 uses **[17,43]**, and 436 uses **[14,45]**. The offset fields remain raw original facts; the eight-pixel reference and their interpretation as sandbox feet are authored placement, not recovered original pivots. The profile stores every chosen anchor explicitly, so different canvas sizes do not move the figure through bounding-box centering.

Timing, the global tick-loop origin, static waiting/idle pose and the sandbox profession assignment are separately documented in [the integration contract](../fire-inspector-visuals.md). No original firefighting lifecycle, action, water use or extinguishing duration is reconstructed. A/B evidence does not by itself establish ordinary-app activation; production and actual-application acceptance are recorded separately there.
