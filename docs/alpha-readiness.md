# Alpha readiness

## Alpha scope

The alpha.2 candidate is a native macOS arm64 development build with the main menu and original-data setup, supported original maps, and City-v11 rule 3 as the fresh-settings entry point. Its paid prepared starter includes Markets, Food, Service, population, taxes, operation pause/resume, workforce priorities, road placement/removal, live routing, and schema-12 OpenEmperor save/load. Existing explicit profile preferences and historical saves keep their selected rules. Stored-map and sandbox visuals share the preview depth painter.

For one exactly fingerprinted, locally validated GOG-derived asset set, the app automatically loads bundled metadata-only walker, building, and road compatibility profiles. Those profiles select original images from the user's own files and contain no original pixels. Unknown data revisions remain playable with presentation fallbacks. Advanced custom JSON profiles remain session-only per-category overrides. `tools/package_macos.sh` produces the development `OpenEmperor.app` and includes only the explicit compatibility JSON allowlist.

This candidate integrates the existing City-v11 economy and operation controls; it adds no new economy, good, building, Service, format semantics, or renderer architecture. Follow [the alpha.2 tester guide](testing-alpha.2.md).

The 2026-09-28 local dirty-workspace candidate passed the full automated alpha pipeline, including separate-process packaged-app save/load with a paused Farm and High-priority Market, the confirmed v2-to-v3 copy path, and a City-v11 original-data run that first collected 75 tax at tick 400. The complete visible 20–30 minute desktop walkthrough, Finder/Gatekeeper behavior and an independent clean Mac remain **NOT RUN**; see [the validation history](alpha-validation.md).

## Explicitly outside the alpha

- Emperor's original simulation or save compatibility
- Additional goods, trade, buildings, rules, roads, walkers, or a new City/save version
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
- 100 successful synthetic menu/session lifecycles and zero OpenEmperor-owned textures after session shutdown;
- corrupt-save, settings, native-dialog lifetime, gesture, resize, small-window, high-DPI, presentation/debug, compatibility-fingerprint, and resource-location regressions;
- an arm64 Release executable.

Use `--package` to include bundle/ZIP validation. Original-data checking is optional unless `--data` is passed. For the recognized data set, this user-flow check needs only `--data`: it requires atomic exact compatibility detection, activates all three built-in profiles through their ordinary loaders, retains the real Xia Industry-v5 visual regression, and separately runs the paid City-v11-v3 starter until Service, complete demand, real tax income, and deterministic save continuation are observed. It verifies that every used map/SG3/.555 file retained its size, modification time and SHA-256. Optional profile arguments remain advanced developer overrides. The report records no private data root or original bytes.

```sh
python3 tools/alpha_check.py --system-sdl --package --data /path/to/emperor
```

## Presentation and compatibility limits

- Presentation mode is the default and uses subdued semantic fallback colors. F1 enables the previous bright technical diagnostics and reports the compatibility ID plus the automatic/custom/fallback source for each visual category.
- Compatibility is fail-closed and atomic. All six relevant SG3/.555 files must match; a missing or mismatched file enables no built-in category.
- A damaged built-in category falls back without blocking the sandbox. An explicitly selected invalid custom JSON continues to fail clearly.
- The exact-fingerprint compatibility pack configures all 16 road masks. One-neighbor masks reuse the matching straight from the same visual family because the bounded audit found no distinct matching end caps; original selection semantics remain unknown.
- All original images are user-supplied. Bundled profiles contain only OpenEmperor mapping and fingerprint metadata.
- Depth sorting treats whole decoded images as units and does not reconstruct split sprites or height.
- Original pivots, timing, horizontal mirroring, and complete map rendering remain unverified.
- F1, F2, F4, F6, F7, and the F3 inspector are presentation diagnostics; they never alter World state.

## Known gameplay limits

City-v11 is OpenEmperor's own deterministic sandbox. It is not a reconstruction of Emperor's economy. The candidate has no trade, campaign progression, original-save import, or claim of original simulation parity. Not every city depleted of both population and goods can be recovered by operation controls.

## Interpreting a pass

A pass shows deterministic behavior over the tested simulated duration, exact tested save continuations, bounded project-owned texture lifetimes, stable render initialization counters, successful tested session turnover, exact known-data activation, safe unknown-data fallback, and successful build/package checks on that machine. It does not prove the absence of every defect or claim feature parity with Emperor.

## Release management

Public source release licensing decision pending. This does not block a local or private alpha candidate build, but the repository must not claim a specific open-source license until the owner chooses and adds one.
