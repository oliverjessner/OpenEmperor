# OpenEmperor 0.1.0-alpha.1 validation

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

The endurance run also verified 16 accepted topology changes, deterministic save bytes, equal parallel and restored Worlds, valid navigation and production balance, 998 completed pottery recipes, and supply to all four households. The presentation review used temporary captures outside the repository and deleted them afterward. It found the normal scene substantially closer to an early playable alpha than the former neon renderer-debug view. Known opaque red pixels in some selected walker payloads remain documented and were not reinterpreted.

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
