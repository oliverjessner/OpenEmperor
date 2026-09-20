# OpenEmperor 0.1.0-alpha.1 known issues

- This alpha is an OpenEmperor sandbox, not Emperor gameplay parity.
- Original Emperor savegames are not supported. Only OpenEmperor sandbox saves are accepted.
- The automatic curated walker, building, and road preview is enabled only when all six files in one locally validated GOG-derived asset set match exact SHA-256 fingerprints. Other data revisions use presentation fallbacks.
- The bundled compatibility profiles contain only OpenEmperor metadata. They do not contain original pixels or other original game bytes. Their visual mappings, anchors, timing, and road topology remain curated preview conventions.
- Advanced custom JSON profiles can override each automatic category for one session. Invalid custom profiles fail explicitly; a damaged built-in profile falls back without blocking the sandbox.
- Road end masks `0x1`, `0x2`, `0x4`, and `0x8` deliberately have no curated original image and use fallback tiles.
- Presentation mode uses subdued diagnostic fallback colors. F1 exposes the brighter research diagnostics and technical visual-source status; this does not alter the World or save data.
- Whole-image depth sorting is approximate for overlapping original graphics.
- Elevation and many original map graphics are incomplete.
- Horizontal mirroring is parsed but not applied because its behavior is unverified.
- Some original animation timing and pivot semantics remain unknown.
- The developer app is ad-hoc signed and is not notarized, so macOS may show a security warning.
- Only macOS on Apple Silicon (arm64) is currently supported and tested.
- A public source license has not been selected yet.
