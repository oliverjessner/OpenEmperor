# OpenEmperor 0.1.0-alpha.2 candidate known issues

- City-v12 fire safety is an opt-in authored prototype (rule 1/schema 14), not original Emperor fire parity. Incidents end naturally without destruction or goods loss. The exactly recognized asset set now supplies a curated 1×1 watchtower for FireWatch; unknown/partial profiles retain its fallback. FireInspector deliberately remains a marker. The watchtower’s original function is unverified. Protection requires road-connected, staffed patrol and is not guaranteed across arbitrary large layouts. Burning Houses retain workforce until ordinary missed-demand decline. The full native 20–30 minute fire-expansion/recovery acceptance is still pending; City-v11 remains the default. See [City-v12 validation](docs/city-v12.md).

- Fire Watch presentation passes synthetic pixels, original SDL 1×/4× readbacks, final-bundle smoke and native exact save/restart. Native Watch selection, ground/road close-up, pause/return and demolition/rebuild are still pending: current desktop pointer automation reports `noWindowsAvailable`. These are separate from the automated regressions; see [the presentation validation](docs/city-v12.md#fire-watch-presentation-pass-2026-10-02).

- Current source supports City-v11 v4 safe empty-building demolition through explicit copy upgrade/restore. New games retain v3 until native replanning acceptance. Stock, recipes and active/returning deliveries deliberately block removal; unmatched goods may require further supply to drain. No discard, refund or guaranteed recovery is implemented. Older v3 saves require an explicit copy upgrade.

- The complete native demolition/replanning playthrough remains pending: SDL event logging showed that the UI automation tool's coordinate clicks arrived at `(0,0)`. Keyboard actions work; core and SDL event tests passed. See [validation and manual acceptance](docs/testing-city-v11-v4.md). Finder/Gatekeeper launch and an independent clean-Mac test also remain unperformed.
- This alpha is an OpenEmperor sandbox, not Emperor gameplay parity.
- Original Emperor savegames are not supported. Only OpenEmperor sandbox saves are accepted.
- The automatic curated walker, building, and road preview is enabled only when all six files in one locally validated GOG-derived asset set match exact SHA-256 fingerprints. Other data revisions use presentation fallbacks.
- The bundled compatibility profiles contain only OpenEmperor metadata. They do not contain original pixels or other original game bytes. Their visual mappings, anchors, timing, and road topology remain curated preview conventions.
- Advanced custom JSON profiles can override each automatic category for one session. Invalid custom profiles fail explicitly; a damaged built-in profile falls back without blocking the sandbox.
- Presentation mode uses subdued diagnostic fallback colors. F1 exposes the brighter research diagnostics and technical visual-source status; this does not alter the World or save data.
- Whole-image depth sorting is approximate for overlapping original graphics.
- Elevation and many original map graphics are incomplete.
- Horizontal mirroring is parsed but not applied because its behavior is unverified.
- Some original animation timing and pivot semantics remain unknown.
- For the studied byte-59 flagged Omega walkers, OpenEmperor presents exact `0x7c00` shadow markers as 50% black RGBA. This closely models the verified original RGB555 destination-halving branch, but 8-bit source-over rounding is not bit-identical to its 5-bit framebuffer result. The behavior is deliberately limited to that verified sprite profile; an original-game side-by-side capture is still unavailable.
- The developer app is ad-hoc signed and is not notarized, so macOS may show a security warning.
- Only macOS on Apple Silicon (arm64) is currently supported and tested.
- A public source license has not been selected yet.
