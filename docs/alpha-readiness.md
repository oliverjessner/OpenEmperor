# Alpha readiness

## Alpha scope

The first OpenEmperor alpha is a native macOS arm64 development build with the main menu and original-data setup, supported original maps, the authored `sandbox-industry-v5` sandbox, road placement/removal and live rerouting, two Clay sources, two Pottery works, one warehouse, four households, five couriers, and OpenEmperor save/load. Local curated profiles may display original walker, building, and road images supplied by the user. Stored-map and sandbox visuals share the preview depth painter. `tools/package_macos.sh` produces the development `OpenEmperor.app`.

This list is frozen for hardening. The acceptance work changes validation, diagnostics, tests, and resource handling only.

## Explicitly outside the alpha

- Emperor's original simulation or save compatibility
- Additional goods, food, markets, taxes, trade, buildings, rules, roads, or walkers
- Complete original visual fidelity, elevation, every original object, or recovered draw semantics
- Verified original pivots, timing, mirroring, road selection, or whole-image depth behavior
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
- a 3,000-frame SDL software render stress with no new decode, texture upload, or stored-order build;
- 100 successful synthetic menu/session lifecycle processes and zero OpenEmperor-owned textures after session shutdown;
- corrupt-save, settings, native-dialog lifetime, gesture, resize, small-window, and high-DPI regressions in the ordinary suite;
- an arm64 Release executable.

Use `--package` to include bundle/ZIP validation. Original-data checking is optional unless `--data` is passed. When requested, all three visual profiles are required, the real Xia Industry-v5 check runs through the application, and every used map/SG3/.555 file must retain its size, modification time, and SHA-256. The report records only that original data was configured and aggregate results; it does not contain the private root path or original bytes.

## Known visual limits

- All original images are user-supplied and selected through local curated profiles.
- Road end caps may use documented preview fallbacks.
- Depth sorting treats whole decoded images as units and does not reconstruct split sprites or height.
- Original pivots, timing, horizontal mirroring, and complete map rendering remain unverified.
- F2, F4, F6, F7, and the F3 inspector are presentation diagnostics; they never alter World state.

## Known gameplay limits

Industry-v5 is OpenEmperor's own deterministic sandbox. It is not a reconstruction of Emperor's economy. The alpha has no additional goods, food, markets, taxes, trade, campaign progression, or original-save import.

## Interpreting a pass

A pass shows deterministic behavior over the tested simulated duration, exact tested save continuations, bounded project-owned texture lifetimes, stable render initialization counters, successful tested session turnover, and successful build/package checks on that machine. It does not prove the absence of every defect or claim feature parity with Emperor.

## Release management

Public source release licensing decision pending. This does not block a local or private alpha candidate build, but the repository must not claim a specific open-source license until the owner chooses and adds one.
