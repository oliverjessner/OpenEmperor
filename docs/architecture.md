# Architecture

OpenEmperor is a native C++ application using SDL3 for windows, input and drawing.
The asset, map, simulation and persistence layers are shared with command-line
inspection and test tools; headless checks do not implement substitute gameplay.

## Module boundaries

| Module | Responsibility |
|---|---|
| `src/assets/` | Bounds-checked SG3/555 decoding, metadata catalogs and visual profiles; no SDL. |
| `src/maps/` | Original containers, raw layers, source entities, permissions and prepared render plans; no SDL. |
| `src/simulation/` | SDL-free World authority, deterministic rules, transport and accounting; separate model-source preparation. |
| `src/persistence/` | Versioned save documents, identity checks, atomic replacement and recovery storage. |
| `src/scene/` | Pure isometric projection, camera math and shared draw keys. |
| `src/renderer/` | Prepared SDL textures, drawing, culling and cached-alpha inspection. |
| `src/app/` | Application/menu lifecycle, input, view adapters and session preparation. |
| `src/platform/` | Narrow platform services such as atomic file replacement. |

## Original data and session preparation

All original game files remain read-only and outside distributed bundles.
`EmperorContainer` validates bounded multipart/zlib data; `EmperorMap` retains
raw terrain, objects and signed heights. The 228×228 storage grid, declared map size, playable Diamond and off-map
indicators are distinct; conversions use explicit storage, bitmap and display types.

`OriginalMapEntities` parses complete bounded managers with source provenance.
`SandboxPlacement` prepares immutable `MapPermissions` for original-road rules,
keeping road/building permission, protection, signed height and fixed gate edges
separate from pictures. Unknown occupancy or conflicting claims refuse the whole
candidate. Later rule versions inherit their selected policy; older profiles retain
their historical renderer-derived masks. [Map permissions](map-permissions-compatibility.md)
and [gate passages](gate-passages.md) define the admitted states.

`StoredMapSession` shares map loading across normal play, browser and checks.
`StoredGraphicsPlan` preserves historical saved-image records; the separate
`RegeneratedMapRenderPlan` prepares supported selectors and complete claims.
`RuntimeArchiveLayout` and `ResourceGroupLookup` resolve resource groups through
one physical-asset catalog. Original source identity, saved images, regenerated
claims and authored previews remain separate domains. See [map format](reverse/map-format.md)
and [first-draw evidence](reverse/map-first-draw.md).

## World and transport authority

`World` owns stable typed building/courier IDs, footprints, inventories, cargo,
reservations, population, controls, accounting and authoritative active paths.
Scalable profiles store entities in stable-ID vectors; IDs are never vector indices.
Derived occupancy indexes, workforce allocations and future route caches are
reconstructed rather than saved as independent authority.

Commands validate before mutation. Failed commands leave state unchanged; successful
road batches commit atomically and refresh topology once. Shared deterministic BFS
uses exact permitted edges and footprint entrances, with stable tie-breaking.
Active movement retains its real waypoint, begun-edge progress, cargo and target;
removing a future road cannot erase cargo or move an actor to an invented position.
Waiting couriers retry after a relevant topology change. No renderer owns navigation.

The fixed-step driver advances World at 20 Hz. Rule-specific update order and
pre-tick staffing govern production, demand, arrivals and dispatch. Reservations
reserve space without adding stock; real loading and arrival move goods. Checked
conservation and treasury invariants include historical consumption and demolition.
The UI reads these results. [City systems](city-systems.md) and [City-v16](city-v16.md) define behavior.

Explicit Bridge rule5 and Ramp rule6 add bounded typed spans/links to that same graph.
Their derived indexes do not enter saves, and an owned Ramp overrides only its exact
permitted height-transition edge. Source terrain, ordinary blockers and prior rule
contracts stay intact. Rendering interpolates authoritative path position rather
than creating another movement clock. See [Bridges](city-v16-bridges.md) and [Ramps](city-v16-ramps.md).

The separate authored Rule20/schema36 source-cell experiment inherits classic
Rule6 and uses the same World navigator. `WorldSourceCellRamp.cpp` owns paid
three-cell claims and the shared flat/slope/flat surface; its renderer prepares
only presentation. Original raw ramp cells remain refused, and Rules1–19 and
all prior schemas stay separate. See [OE53](rendering/oe53-terrain-integration.md).

The separate [OE54 Rule21/schema37 trade contract](original-economy/oe54-trade.md)
also inherits classic Rule6. World owns one finite journal and caravan;
`WorldLandTrade.cpp` reuses normal Warehouse stock, courier cargo, shared routes
and begun-edge movement. Prepared source geometry only constrains the authored
boundary; original map protection remains authoritative. Model/difficulty bind
identity without enabling Original simulation.

The optional Rule10 original economy binds one immutable model prepared from the
selected installation and Difficulty. Worlds never search for or reread that file.
Its own House, goods and service authority integrates with the same World/transport
pipeline. Source-derived rules and authored adapters remain explicit; full parity
is partial. [Original-economy architecture](original-economy/architecture-map.md)
is the maintained contract for its scheduler, model binding and schema26 state.

## Compatibility resources and presentation

Metadata-only compatibility packs activate only after every required original-file
SHA-256 matches. Canonical containment and symlink checks protect paths. Selection
is explicit custom JSON, detected built-in profile, then named fallback; invalid
explicit overrides remain errors. Profile choice and presentation toggles never
enter World or saves. Resources are located relative to the executable/bundle.

The SDL-free `Sg3ImageLoader` is the common decoder for runtime, catalog and inspector.
Color, Omega overlay and supported alpha streams are validated separately; mirror
references remain unsupported. Physical images are deduplicated and uploaded during
session initialization. Stored rendering is bounded to 2,048 assets, 16 MiB per image
and 64 MiB logical RGBA textures, including Combined/Base/Overlay variants. Asset
browsing has its own bounded lazy cache; it is not the gameplay frame path.

Stored instances use eagerly sorted front-ground draw keys. Each frame merges that
stream with a small sorted stream of live roads, buildings and walkers. Draw, alpha
picking and culling share image origins and whole-image bounds; construction picking
remains a separate logical query. Atomic composition readiness activates complete
replacements or retains historical fallback. Signed saved height shifts presentation
once by ×40; complete original height normalization remains unresolved.

Gameplay draw/hover/inspection performs no file access, decode, upload, map scan,
World copy/command/tick or route preparation. Camera changes and overlays are view
state. Prepared authored cliff/ramp components remain bounded presentation. The
[scene composition](rendering/scene-composition.md) and [texture compatibility](rendering/texture-compatibility.md)
contracts describe painter limits and the narrow software-renderer texture exception.

## Menu and session lifecycle

`Application` owns one SDL window and main-thread event loop. `MenuSession` prepares
and initializes a candidate before replacing its retained `SandboxView`; errors
leave the prior World and textures alive. Dialog callbacks deliver owned paths
through a synchronized inbox with lifetime/generation checks, never touching views
or textures themselves. Original-file roots and personal save/settings roots differ.

Menus do not advance World. Loading and I/O reset frame time; the application paces
frames against a 60 Hz target independently of fixed-step simulation. Input converts
window coordinates through the full renderer viewport before map clipping. Gestures
are pure/coalesced previews and are revalidated on release; modal/focus/layout changes
cancel pending actions. UI, camera and selection state are absent from snapshots.

## Autosave and recovery boundary

`WorldSnapshot` contains authoritative state. `World::restore` validates IDs,
placements, paths, progress, reservations and accounting before publishing a fresh
World; it rebuilds derived caches without replacing active routes. `SandboxSave`
checks the selected schema/profile, map/source fingerprints and permission identity,
then uses bounded JSON and atomic temporary-file replacement. Load starts paused.
Older contracts remain versioned; explicit copy upgrades preserve their input.

`AutosaveController` belongs to menu sessions, not direct CLI views. Every 1,200
completed ticks it may capture one reached state; unsafe gestures defer capture,
and paused frames do not produce periodic files. Capture does not change the
manual-save target. `RecoveryStore` retains a protected start plus five checkpoints,
validates a new file before publishing metadata and rotates only afterward.
Bounded relative paths, symlink rejection, an 8 MiB save limit and a 256 MiB store
budget constrain recovery. Loading a checkpoint validates a normal save, starts
paused and branches into a fresh history. Metadata is navigation, never World authority.

See [sandbox saves](gameplay-sandbox.md#openemperor-sandbox-saves), [testing](testing.md)
and [macOS packaging](macos-packaging.md). Automated, scripted SDL, native-input and
human-play acceptance remain separate evidence.
