# OpenEmperor 0.1.0-alpha.1 validation

Candidate: `0.1.0-alpha.1`  
Revision: `221c975244ae422dfa139a4ebed372977c7d3a99`  
Validation date: `2026-09-19`  
Validation result: **PASS**

The final acceptance run used native macOS arm64, Apple clang 21.0.0, CMake 4.4.3, the local legally obtained Emperor installation, and all three local visual-preview profiles. The report contained no private data path or original bytes. Nine original map/archive/bitmap inputs used by the smoke test were unchanged by size, modification time, and SHA-256.

## Acceptance results

| Area | Check | Result |
| --- | --- | --- |
| Build | Debug CTest, 34/34 | PASS |
| Build | Release CTest, 34/34 | PASS |
| Build | ASan/UBSan CTest, 34/34 | PASS |
| Simulation | 100,000 deterministic ticks in Debug, Release, and sanitizer builds | PASS |
| Simulation | 12 save checkpoints and 12 real JSON round trips | PASS |
| Rendering | 3,000 Debug and 3,000 sanitized stress frames | PASS |
| Rendering | Decode delta 0, texture-upload delta 0, stored-order-build delta 0 | PASS |
| Rendering | Unified painter regression | PASS |
| Lifecycle | 100 menu/session iterations with owned texture counters returning to zero | PASS |
| Lifecycle | Fresh-user setup, cancel, select data, new sandbox, save, restart, load | PASS |
| Lifecycle | Industry-v5 without optional visual profiles | PASS |
| Lifecycle | Deleted session-local visual profile does not block restart | PASS |
| Original data | Xia Industry-v5 smoke, 8,000 ticks and 401 five-courier frames | PASS |
| Original data | Nine used original files unchanged | PASS |
| Package | Native arm64 executable and embedded dependencies | PASS |
| Package | Relocated and unzipped app launch checks | PASS |
| Package | Ad-hoc code signature verification | PASS |
| Package | ZIP structure and checksum | PASS |
| Package | No original assets, local profiles, saves, screenshots, or alpha report | PASS |
| Manual | Main menu and alpha/version presentation | PASS |
| Manual | New Industry-v5 Xia sandbox and loading feedback | PASS |
| Manual | Save/load, pause/step, menu/continue | PASS |
| Manual | Camera movement and zoom | PASS |
| Manual | Two clay sources, two potteries, warehouse, four houses, and five walkers | PASS |
| Manual | Road/building visuals, road removal/replacement, and depth diagnostics | PASS |

The endurance run also verified 16 accepted topology changes, deterministic save bytes, equal parallel and restored Worlds, valid navigation and production balance, 998 completed pottery recipes, and supply to all four households.

## Package

- App: `OpenEmperor.app`
- ZIP: `OpenEmperor-0.1.0-alpha.1-221c975244ae-macos-arm64.zip`
- ZIP size: 3,738,027 bytes
- SHA-256: `7a26a2893e664bb5df629d6498ece24ab8071339361d1ef340dc1e890d4dc434`
- Executable architecture: `Mach-O 64-bit executable arm64`
- Project version: `0.1.0`
- Build type: `Release`
- Deployment target: `26.0`
- Signature: ad-hoc; Developer ID absent; not notarized

The validated build records `dirty: true` because this work is intentionally uncommitted. It is a valid local/private candidate artifact. A distributable clean-tree candidate should be rebuilt after the owner commits the reviewed changes.

The full command was:

```sh
python3 tools/alpha_check.py --system-sdl --package \
  --data <local-emperor-data> \
  --walker-visuals .local/visuals/walkers-v2.json \
  --building-visuals .local/visuals/buildings.json \
  --road-visuals .local/visuals/roads.json
```

The machine-readable local report remains under ignored `.local/reports/` and is not packaged.
