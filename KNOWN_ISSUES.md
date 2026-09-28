# OpenEmperor 0.1.0-alpha.2 candidate known issues

- This alpha is an OpenEmperor sandbox, not Emperor gameplay parity.
- City-v11 rules v3 are the fresh-settings menu default and pass synthetic staffing, 20,000-tick determinism, natural buffered crisis/recovery, schema-12 persistence and SDL inspector-event checks. Automated packaged-app checks cover separate-process operation-state reload and the confirmed v2-copy path, and the user has manually observed real tax income. The complete visible 20–30 minute packaged-app walkthrough, Finder/Gatekeeper launch and an independent clean-Mac test remain unperformed.
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
