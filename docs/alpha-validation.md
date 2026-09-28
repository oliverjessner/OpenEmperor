# OpenEmperor validation history

## OpenEmperor 0.1.0-alpha.2 local candidate

Candidate: `0.1.0-alpha.2`

Review base and recorded revision: `2724ae3cc57d68e17bffd1d1bd111d6afc9fdc4f`

Validation date: `2026-09-28`

Automated validation result: **PASS** on the intentionally dirty milestone worktree. This is a local candidate, not a publication build.

The existing alpha pipeline passed 45/45 Debug, 45/45 Release, and 45/45 ASan/UBSan tests. Debug, Release, and sanitizer endurance each completed 100,000 deterministic ticks with twelve real JSON round trips, equal continued Worlds, valid balances, and sustained City-v11 supply. Both 3,000-frame render stresses and 100 menu/session lifecycles passed with project-owned texture counts returning to zero.

The original-data check kept the Industry-v5 compatibility regression and added a separate City-v11-v3 paid-starter run. Starting at tick 0 with 100 funds and 1,200 construction spending, its first complete demand generated 75 tax at tick 400 and raised treasury to 175. At tick 8,000 the same run had 178 Service visits, 79 fulfilled demands, 4,360 total tax, 4,460 treasury, unchanged construction spending, valid production/economy invariants, and exact save continuation. Nine original inputs were unchanged by size, modification time, and SHA-256.

The packaged executable was invoked in two separate processes against both synthetic and local original data. At tick 1,000 it saved population 27, treasury 320, active cargo/reservations and six moving couriers with the Farm paused and Market priority High. The next process loaded an equal recorded state, retained City-v11 rule 3 and the controls, then resumed the Farm from progress 8 to 9. The menu regression also confirms that a City-v11-v2/schema-11 save requires explicit confirmation, produces a new v3/schema-12 path, preserves all authoritative state, and leaves the source bytes unchanged.

Package validation passed for the relocated app and extracted ZIP, recursive arm64 Mach-O dependencies, metadata-only compatibility resources, negative bundle mutations, and strict ad-hoc signature verification. The app and ZIP contain no original assets, `.local` content, personal saves, screenshots, private paths, or alpha report.

- ZIP: `OpenEmperor-0.1.0-alpha.2-2724ae3cc57d-macos-arm64.zip`
- ZIP size: 3,916,594 bytes
- SHA-256: `e7a864610c3ebd4a91950bd370b2bce400da6929049587dcdef789dea708e18f`
- Executable architecture: `Mach-O 64-bit executable arm64`
- Project version: `0.1.0`
- City profile/rule: `sandbox-city-v11` / `3`
- City-v11-v3 save schema: `12`
- Build type: `Release`
- Deployment target: `26.0`
- Signature: ad-hoc; Developer ID absent; not notarized

Manual status is recorded separately from automation. The user's report that Funds increase is positive evidence for the real revenue chain. A complete visible packaged-app walkthrough covering fresh folder selection, Inspector pause/priority input, a 20–30 minute expansion, road repair, quit/relaunch, and the v2-copy dialog is **NOT RUN** for this candidate. Finder double-click/Gatekeeper behavior and an independent clean Mac are also **NOT RUN**. The repository still has no project LICENSE; publication remains blocked until the owner selects one and rebuilds from the reviewed committed state.

The machine-readable ignored reports are `.local/reports/alpha-check.json` and `dist/package-report.json`. The full command was:

```sh
python3 tools/alpha_check.py --system-sdl --package \
  --data <local-emperor-data> --jobs 8
```

## Historical OpenEmperor 0.1.0-alpha.1 validation

Candidate: `0.1.0-alpha.1`

Base revision: `1ee25bdf7347af8506e80e22dd1311d4b12cb9f2`

Validation date: `2026-09-19`

Validation result: **PASS**

This milestone is intentionally uncommitted, so there is no newer Git revision to cite. The validated working tree is `dirty: true` on the exact review base above. The final acceptance run used native macOS arm64, Apple clang 21.0.0, CMake 4.4.3, the local legally obtained Emperor installation, and no custom visual-profile arguments. The report contains no private data path or original bytes. Nine original map/archive/bitmap inputs used by the smoke test were unchanged by size, modification time, and SHA-256.

## Compatibility result

The detector recognized `gog-derived-2.0.0.2-en-assetset-1` atomically and selected `builtin` for walker, building, and road visuals. The application loaded those JSONs through the ordinary profile loaders, decoded 48 unique walker assets, four building assets, and twelve road assets, rendered 401 frames with five couriers, and recorded road draws greater than zero. Synthetic tests also passed for exact hashes, missing files, one-byte mismatch, irrelevant extra files, escaping symlinks, manifest traversal, atomic mismatch fallback, category-specific custom override and clearing back to automatic selection.

Unknown synthetic data was accepted and started a sandbox with all three categories on fallback. A deliberately broken built-in category fell back without blocking the World, while invalid explicit custom profiles retained their normal error behavior. One thousand menu frames, camera input, visual toggles, and saving caused no additional compatibility detection.

## Acceptance results

| Area | Check | Result |
| --- | --- | --- |
| Build | Debug CTest, 35/35 | PASS |
| Build | Release CTest, 35/35 | PASS |
| Build | ASan/UBSan CTest, 35/35 | PASS |
| Simulation | 100,000 deterministic ticks in Debug, Release, and sanitizer builds | PASS |
| Simulation | 12 save checkpoints and 12 real JSON round trips | PASS |
| Rendering | 3,000 Debug and 3,000 sanitized stress frames | PASS |
| Rendering | Decode delta 0, texture-upload delta 0, stored-order-build delta 0 | PASS |
| Rendering | Unified painter and presentation/debug palette regressions | PASS |
| Compatibility | Exact known data auto-selected all three built-in profiles | PASS |
| Compatibility | Unknown/mismatched data used atomic fallback | PASS |
| Compatibility | No hashing during 1,000-frame/input/save path | PASS |
| Lifecycle | 100 menu/session iterations with owned texture counters returning to zero | PASS |
| Lifecycle | Fresh-user setup, cancel, select data, new sandbox, save, restart, load | PASS |
| Original data | Xia Industry-v5 smoke, 8,000 ticks and 401 five-courier frames | PASS |
| Original data | Nine used original files unchanged | PASS |
| Package | Native arm64 executable and embedded dependencies | PASS |
| Package | Exact four-file compatibility-resource allowlist | PASS |
| Package | Proprietary extensions, symlinks, traversal and invalid JSON rejected | PASS |
| Package | Relocated and unzipped app launch checks | PASS |
| Package | Ad-hoc code signature verification | PASS |
| Package | No original assets, local profiles, saves, screenshots, or alpha report | PASS |
| Manual | Fresh app root showed `Compatible original data detected` without a private path | PASS |
| Manual | Packaged Xia Industry-v5 presentation at 1×, 2× and 4× | PASS |
| Manual | Curated Clay sources, potteries, warehouse, houses, roads and five walkers visible | PASS |
| Manual | Unresolved cells subdued in normal mode; bright diagnostics restored by F1 | PASS |
| Manual | Help visible, Debug absent from header, status/UI text readable | PASS |
| Manual | F1 showed pack ID and automatic sources; simulation remained usable | PASS |

The endurance run also verified 16 accepted topology changes, deterministic save bytes, equal parallel and restored Worlds, valid navigation and production balance, 998 completed pottery recipes, and supply to all four households. The presentation review used temporary captures outside the repository and deleted them afterward. It found the normal scene substantially closer to an early playable alpha than the former neon renderer-debug view. At this validation date, known opaque red pixels in some selected walker payloads had not yet been reinterpreted; the verified follow-up below supersedes that open presentation issue.

## Package

- App: `OpenEmperor.app`
- ZIP: `OpenEmperor-0.1.0-alpha.1-1ee25bdf7347-macos-arm64.zip`
- ZIP size: 3,764,514 bytes
- SHA-256: `59622140b1a2f0a8763bddbbcfc9feba1404ee7da9776f3bec6aa564a60de2ea`
- Executable architecture: `Mach-O 64-bit executable arm64`
- Project version: `0.1.0`
- Display version: `0.1.0-alpha.1`
- Build type: `Release`
- Deployment target: `26.0`
- Signature: ad-hoc; Developer ID absent; not notarized
- Compatibility resources: `Contents/Resources/Compatibility/gog-derived-2.0.0.2-en-assetset-1/{manifest,walkers,buildings,roads}.json`

The validated build records `dirty: true` because this work is intentionally uncommitted. A distributable clean-tree candidate should be rebuilt after the owner commits the reviewed changes. A project license is still absent and remains the publication blocker.

The full command was:

```sh
python3 tools/alpha_check.py --system-sdl --package \
  --data <local-emperor-data>
```

The machine-readable local report remains under ignored `.local/reports/` and is not packaged.

## 2026-09-23 red-payload blocker follow-up

Review base `3a2692ec172d89db4f5df82e7e11f46a0b916e79`; validation result: **PASS** on the intentionally dirty milestone worktree. Bounded static inspection of the hash-verified local EXE established that byte-59 flagged Omega sprites treat exact RGB555 `0x7c00` literals as destination-halving shadow markers. The general decoder remains unchanged. The Walker profile now prepares a scoped straight-RGBA shadow representation once at load; Type-30 buildings, separate alpha profiles, the World, saves, and road routing are unchanged.

The normal Debug CTest passed 36/36 after rebuilding everything. The final `alpha_check.py` run then passed 36/36 Debug, 36/36 Release, and 36/36 ASan/UBSan tests; 100,000 deterministic ticks in every configuration; 3,000 normal and 3,000 sanitized render-stress frames; and 100 menu/session lifecycles. The Xia Industry-v5 smoke ran 8,000 ticks and 401 five-courier frames with built-in compatibility visuals. It verified all nine used original files unchanged and retained valid production, navigation, deterministic saves, and continued resume. A stale triage-test expectation was corrected from `TargetFull` to `NoRoad` for the precise state where a second Pottery is free but disconnected; no simulation implementation defect was found.

The rebuilt native window was inspected directly on the local Xia Industry-v5 demo at a running tick. The former solid-red Walker blocks were presented as shadows, the Select tool showed no invalid-hover diamond, the four unassigned road end masks retained their existing muted fallback, and production was active. This verifies OpenEmperor's new presentation path; no original-game side-by-side capture was available, and the 8-bit RGBA blend remains an approximation of the verified 5-bit destination-halving result.

The package check passed with built-in automatic Walker, Building, and Road profiles. `dist/OpenEmperor.app/Contents/MacOS/OpenEmperor` is `Mach-O 64-bit executable arm64`, recursive strict signature verification passed, and the app/ZIP contain no original assets, local profiles, saves, screenshots, or alpha report. The generated ZIP is `OpenEmperor-0.1.0-alpha.1-3a2692ec172d-macos-arm64.zip`, 3,788,826 bytes, SHA-256 `61c0c99e991a292f3d2388f0caddd85c1b2e816c5b1ba6bc726dccd003a5f0f4`. It remains an ad-hoc signed local preview; the absent project license still blocks publication.

The final command was:

```sh
python3 tools/alpha_check.py --system-sdl --package \
  --data <local-emperor-data>
```

The new ignored machine report is `.local/reports/alpha-check.json` and reports overall `pass`.
