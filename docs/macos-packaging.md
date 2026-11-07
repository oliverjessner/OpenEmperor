# Local macOS arm64 development app

Run `tools/package_macos.sh` on an Apple Silicon Mac with Xcode Command Line
Tools, CMake, Homebrew SDL3, OpenSSL and nlohmann/json installed. It configures
`build-macos-app/` in Release with `OPENEMPEROR_BUILD_MACOS_APP=ON` and explicit
arm64/macOS 26.0 deployment settings by default. The ordinary `build/` CLI
configuration is untouched. Every embedded Mach-O must support the selected
minimum. When an installed dependency requires a newer OS, select that target
explicitly with `--min-os major.minor[.patch]`; values below 26.0 are refused.
For example, a libcrypto built for macOS 27.0 requires `--min-os 27.0` and a
host that supports it. This does not change the default or weaken verification.
The selected minimum and tested host OS are recorded separately in
`dist/package-report.json`.

Build and distribution destinations must be new paths; existing evidence is
preserved. For an isolated local candidate with real-data checks:

```sh
sh tools/package_macos.sh --build-dir "$PWD/.local/package-new/build" --dist-dir "$PWD/.local/package-new/dist" --data /absolute/path/to/Emperor --jobs 2
```

The defaults remain `build-macos-app/` and `dist/` when they do not yet exist.
Linked path components, overlapping outputs, maintained source directories and
paths overlapping the supplied original installation are refused before building.

The package build installs only `OpenEmperor.app` into its own temporary stage.
CMake `fixup_bundle` copies actual non-system dynamic dependencies into
`Contents/Frameworks` and changes their references to bundle-relative paths.
The verifier checks every Mach-O dependency and RPATH recursively, resolves
each non-system `@rpath`/loader/executable reference inside the bundle, checks
arm64 and minimum OS, rejects unlisted resources and escaping symlinks, and
verifies the final signature. System Apple libraries are left to macOS.
The script binds SDL and OpenSSL notices to the actual resolved linked libraries,
and the nlohmann/json notice to the configured CMake dependency. It records
versions and hashes rather than guessing a Homebrew OpenSSL major version.
zlib is supplied by macOS here, while nlohmann/json
is a header-only compile-time dependency.

The stage is signed ad hoc, inside out. It is moved to a path with spaces
outside the repository and executed from another working directory with a
restricted PATH, no DYLD or OpenSSL environment overrides, and injected
temporary app storage. The bundled executable renders fresh setup frames, then
loads independently constructed synthetic SG3/map bytes and verifies that fresh
settings select the paid City-v11-v3 starter. It runs both the historical
Industry-v5 regression and the City-v11 supply/tax/resume check. If local
original data exists, the same relocated executable repeats those checks against
Xia.map without modifying the inputs. The ZIP is extracted and checked again. Negative
tests mutate disposable copies and require the verifier to reject missing or
external libraries, symlink escape, incompatible architecture, invalid plist,
missing executable, forbidden asset and broken signature.

Outputs are `dist/OpenEmperor.app`, a version/revision-named ZIP,
`dist/package-report.json` and `dist/SHA256SUMS`. Launch the app with Finder or
`open dist/OpenEmperor.app`; choose your own legally obtained original game
data. Nothing is downloaded or embedded. Existing settings and saves remain
under SDL's per-user OpenEmperor preference path, usually
`~/Library/Application Support/OpenEmperor/OpenEmperor/`. No write into the
bundle or original data directory is required.

Personal `saves/` and `recovery/` directories are runtime preference data, not
bundle resources. The package stage and ZIP contain neither. The verifier's
extension and resource allowlists continue to reject save payloads, original
assets, decoded images and local reports; recovery metadata is never staged.

For a visible test launch without touching personal preferences, supply the
diagnostic `--app-root` option through LaunchServices, for example
`open -n -a dist/OpenEmperor.app --args --app-root /absolute/temporary/folder`.
Use a fresh folder outside the bundle and original data root. This option is
limited to the menu path; the default Finder launch still uses the established
SDL preference location. Merely setting `HOME` for `open` does not reliably
redirect macOS's application-support directory.
The automated package build does not itself perform a Finder double-click;
that desktop observation is recorded separately from its dummy-rendered tests.

This is a local developer preview, signed only ad hoc. It has no Developer ID
signature or Apple notarization, and download/Gatekeeper acceptance on another
Mac is unproved. A clean Mac without developer tools is an additional test,
not simulated by restricting PATH. The project's `LICENSE` declares
GPL-3.0-only for OpenEmperor source. Completeness of the license text, notices
and resource rights still requires a separate distribution review; the notice
does not establish public-release readiness.
# Alpha acceptance

Use new build, report and package paths for a complete isolated run:

```sh
python3 -B tools/alpha_check.py --system-sdl --jobs 2 --data /absolute/path/to/Emperor --build-dir "$PWD/.local/alpha-new/debug" --release-build-dir "$PWD/.local/alpha-new/release" --sanitize-build-dir "$PWD/.local/alpha-new/sanitize" --report "$PWD/.local/alpha-new/report.json" --package --package-build-dir "$PWD/.local/alpha-new/package-build" --package-dist-dir "$PWD/.local/alpha-new/dist"
```

`--data` also configures the original-model CTests in every build; any skip with
that explicit input fails acceptance. Without `--data`, the existing CMake model
root default is retained. Declared `--real` tests that return their registered77
because that default model is missing are individually reported `NOT_RUN`, with
the original-data status `NOT_CONFIGURED`. Completed synthetic checks remain
usable, but the overall report is `partial`/`synthetic_only`, not a full original
PASS. Unexpected skips still fail. The runner retains the
actual registered-test inventory and complete JUnit output in each build.
The outer CTest process budget uses the sum of actual registered case deadlines,
with the former 1,800-second allowance retained for the untimed group and as a
minimum. This lets a long complete suite finish; every individual CMake test
deadline remains exact, including the 120-second original-menu check. The report
records this aggregate budget separately from observed durations.
Existing builds, reports and distributions are never deleted or replaced.
Without explicit paths, the historical default names are used only if fresh.

The package step remains the authority for relocation, dependency/RPATH checks,
arm64, ad-hoc signature integrity, ZIP extraction and app smoke. The alpha runner
checks the maintained ten-file metadata pack, including the bounded original
House contract, and rejects unlisted original payloads, saves and local reports.
Shipped `buildings.json`/`roads.json` compatibility metadata is intentional; custom
local visual profiles and `.local/` reports never become bundle resources.
Current original-smoke asset counts are checked against distinct physical
identities in the selected metadata and actual role draws, not historical fixed
asset totals. Actual original-game semantic verification remains in the existing
runtime/source tests; a metadata-only bundle contains no original SG3 payload.

The package is still a local development preview. Developer ID signing, notarization, independent clean-Mac verification, and publication readiness remain outside this milestone.
