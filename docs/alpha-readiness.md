# Alpha readiness

## Alpha scope

The first OpenEmperor alpha is a native macOS arm64 development build with the main menu and original-data setup, supported original maps, the authored `sandbox-industry-v5` sandbox, road placement/removal and live rerouting, two Clay sources, two Pottery works, one warehouse, four households, five couriers, and OpenEmperor save/load. Stored-map and sandbox visuals share the preview depth painter.

For one exactly fingerprinted, locally validated GOG-derived asset set, the app automatically loads bundled metadata-only walker, building, and road compatibility profiles. Those profiles select original images from the user's own files and contain no original pixels. Unknown data revisions remain playable with presentation fallbacks. Advanced custom JSON profiles remain session-only per-category overrides. `tools/package_macos.sh` produces the development `OpenEmperor.app` and includes only the explicit compatibility JSON allowlist.

This list is frozen for hardening. The acceptance work changes validation, diagnostics, tests, and resource handling only.

## Explicitly outside the alpha

- Emperor's original simulation or save compatibility
- Additional goods, food, markets, taxes, trade, buildings, rules, roads, or walkers
- Complete original visual fidelity, elevation, every original object, or recovered draw semantics
- Verified original pivots, timing, mirroring, road selection, or whole-image depth behavior
- General or official GOG-version support beyond the one exact fingerprint set
- Developer ID signing, notarization, networking, multiplayer, or Windows/Linux releases

## Required checks

Run:

```sh
python3 tools/alpha_check.py --system-sdl
```

The command fails closed and writes `.local/reports/alpha-check.json`. A pass requires:

- complete ordinary CTest runs in Debug, fresh Release, and fresh ASan/UBSan builds;
- a deterministic 100,000-tick Industry-v5 run with two equal Worlds and scheduled topology changes;
- twelve snapshot and real JSON save/restore checkpoints, continued restored Worlds, deterministic save bytes, conservation, navigation, stock, reservation, identity, path, position, and target validation;
- a 3,000-frame SDL software render stress with no new decode, texture upload, stored-order build, or compatibility hashing;
- 100 successful synthetic menu/session lifecycle processes and zero OpenEmperor-owned textures after session shutdown;
- corrupt-save, settings, native-dialog lifetime, gesture, resize, small-window, high-DPI, presentation/debug, compatibility-fingerprint, and resource-location regressions;
- an arm64 Release executable.

Use `--package` to include bundle/ZIP validation. Original-data checking is optional unless `--data` is passed. For the recognized data set, this user-flow check needs only `--data`: it requires atomic exact compatibility detection, activates all three built-in profiles through their ordinary loaders, runs the real Xia Industry-v5 application check, observes 48 walker assets, four building assets, twelve road assets and actual draws, and verifies that every used map/SG3/.555 file retained its size, modification time and SHA-256. Optional profile arguments remain advanced developer overrides. The report records no private data root or original bytes.

```sh
python3 tools/alpha_check.py --system-sdl --package --data /path/to/emperor
```

## Presentation and compatibility limits

- Presentation mode is the default and uses subdued semantic fallback colors. F1 enables the previous bright technical diagnostics and reports the compatibility ID plus the automatic/custom/fallback source for each visual category.
- Compatibility is fail-closed and atomic. All six relevant SG3/.555 files must match; a missing or mismatched file enables no built-in category.
- A damaged built-in category falls back without blocking the sandbox. An explicitly selected invalid custom JSON continues to fail clearly.
- Road end masks `0x1`, `0x2`, `0x4`, and `0x8` remain fallback tiles.
- All original images are user-supplied. Bundled profiles contain only OpenEmperor mapping and fingerprint metadata.
- Depth sorting treats whole decoded images as units and does not reconstruct split sprites or height.
- Original pivots, timing, horizontal mirroring, and complete map rendering remain unverified.
- F1, F2, F4, F6, F7, and the F3 inspector are presentation diagnostics; they never alter World state.

## Known gameplay limits

Industry-v5 is OpenEmperor's own deterministic sandbox. It is not a reconstruction of Emperor's economy. The alpha has no additional goods, food, markets, taxes, trade, campaign progression, or original-save import.

## Interpreting a pass

A pass shows deterministic behavior over the tested simulated duration, exact tested save continuations, bounded project-owned texture lifetimes, stable render initialization counters, successful tested session turnover, exact known-data activation, safe unknown-data fallback, and successful build/package checks on that machine. It does not prove the absence of every defect or claim feature parity with Emperor.

## Release management

Public source release licensing decision pending. This does not block a local or private alpha candidate build, but the repository must not claim a specific open-source license until the owner chooses and adds one.
