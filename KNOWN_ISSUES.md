# OpenEmperor 0.1.0-alpha.2 candidate known issues

Only current limitations and outstanding acceptance checks are listed here.

- **Native input acceptance:** the Xia/Handan human checklist, including pointer selection, road dragging, modal focus/resize handling and external-display interaction, remains incomplete. Historical automation delivery failures do not establish a player-coordinate defect. See [the input checklist](docs/testing-input-acceptance.md).

- **Original-map playability evidence:** Empty City imports landscape and protected structures without original Emperor residents or NPC simulation, so zero Sandbox walkers before player construction is expected. Paid technical scenarios on Chang-an Zhou, Handan and Badaling demonstrate actual courier trips, supply and tax; native human acceptance and Oliver's exact Chang-an save/selected cell and Anyang file identity remain unverified. The similar no-Warehouse failure explains missing Pottery and declining population, but does not establish the separate reported Food gap. See [the multi-map acceptance report](docs/map-playability-acceptance.md).

- **Exact NoRoad player-state diagnosis:** F1/G now measures actual entrances, transport components, permission/height blockers and separate dispatch conditions. Paid Handan/Badaling missing-road cases are expected and a normal 2-Funds repair restores real delivery/return; no production routing defect is reproduced. Oliver's matching current save and native human acceptance remain unverified. See [road connectivity diagnostics](docs/road-connectivity-diagnostics.md).

- **Unsupported rule-3 maps:** the latest 167-map census prepares 102 policies, with 57 unknown-occupancy first failures, three known conflicts and five unsupported `cResWall` managers. Complete-manager diagnosis still finds 623 unknown records in 13 types across 59 maps. Full corpus rendering and gameplay acceptance remains open. Unsupported gate heights can also refuse a map. See [the bounded type-183 pass](docs/reverse/original-occupancy-pass-next.md) and [historical map compatibility](docs/map-permissions-compatibility.md).

- **Incomplete ordinary wall/gate presentation:** Tower and ResWall bodies, additional Type-256 wall components, and gate states, views and lifecycle outside the validated static layouts remain unsupported. See [ordinary wall/gate limits](docs/reverse/ordinary-wall-gates.md).

- **Great Wall restore context:** standalone maps lack independently verified original mode/current-player-goal context. Automatic therefore retains historical fallback; original restored rendering and unsupported Type-1 map layouts remain open. See [restore-context limits](docs/reverse/great-wall-restore-context.md).

- **Ruined phase-2 Road preview:** Badaling and Handan each contain four unsupported pieces whose selector requests variants 40/41 from a Ruined group containing only 0–39. This preview combination retains historical fallback; it is not a missing-file error. See [the bounded diagnosis](docs/reverse/great-wall-restore-context.md#ruined-phase-2-road-compatibility-2026-10-05).

- **Landscape and scene fidelity:** original first-draw occupancy/anchors, height normalization, historical-overlay eligibility and complete elevation/landscape reproduction remain unresolved. Whole-image sorting is approximate for interpenetrating artwork requiring separate front/back components. See [map fidelity](docs/reverse/map-first-draw.md) and [scene composition](docs/rendering/scene-composition.md).

- **Original visual identities:** building/walker profession mappings, House-stage identity, animation timing and pivots are not fully verified. Original one-cell Well/HealthPost identities for legacy profiles remain unresolved. Original building animation, evolving Wells and upgraded/rotated/terrain-transition road styles remain unsupported. See [building visuals](docs/building-visual-profile.md), [presentation research](docs/reverse/original-presentation-correction.md) and [walker profiles](docs/walker-visual-profile.md).

- **SG3 mirror-reference decoding:** original mirror-reference records cannot be decoded directly. See [walker format limits](docs/walker-visual-profile.md).

- **Omega shadow precision:** the byte-59 sprite shadow approximation uses 50% black RGBA; source-over rounding is not bit-identical to the original RGB555 destination-halving result. An original-game side-by-side capture remains unavailable. See [shadow presentation](docs/walker-visual-profile.md).

- **Original compatibility:** original Emperor savegames are unsupported. Automatic original-sprite profiles recognise only the pinned GOG-derived asset revision; other revisions use presentation fallbacks. Original gameplay parity remains incomplete. See [save architecture](docs/architecture.md) and [asset compatibility](docs/architecture.md#compatibility-resources-and-presentation).

- **macOS distribution acceptance:** the developer app is ad-hoc signed, without Developer ID signing or notarization. Finder/Gatekeeper launch and an independent clean-Mac run remain unverified. See [packaging limits](docs/macos-packaging.md).
