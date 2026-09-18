# Local macOS arm64 development app

Run `tools/package_macos.sh` on an Apple Silicon Mac with Xcode Command Line
Tools, CMake, Homebrew SDL3, OpenSSL 3 and nlohmann/json installed. It configures
`build-macos-app/` in Release with `OPENEMPEROR_BUILD_MACOS_APP=ON` and explicit
arm64/macOS 26.0 deployment settings. The ordinary `build/` CLI configuration
is untouched. The current 26.0 minimum follows the embedded Homebrew SDL3 and
libcrypto builds; it is checked against every embedded Mach-O. The tested OS
version is recorded separately in `dist/package-report.json`.

The package build installs only `OpenEmperor.app` into its own temporary stage.
CMake `fixup_bundle` copies actual non-system dynamic dependencies into
`Contents/Frameworks` and changes their references to bundle-relative paths.
The verifier checks every Mach-O dependency and RPATH recursively, resolves
each non-system `@rpath`/loader/executable reference inside the bundle, checks
arm64 and minimum OS, rejects unlisted resources and escaping symlinks, and
verifies the final signature. System Apple libraries are left to macOS.
The script includes the installed SDL, OpenSSL and nlohmann/json license files
and records their versions. zlib is supplied by macOS here, while nlohmann/json
is a header-only compile-time dependency.

The stage is signed ad hoc, inside out. It is moved to a path with spaces
outside the repository and executed from another working directory with a
restricted PATH, no DYLD or OpenSSL environment overrides, and injected
temporary app storage. The bundled executable renders fresh setup frames,
then loads synthetic SG3/map bytes, runs an Industry-v5 sandbox, saves and
resumes it. If local original data exists, the same relocated executable runs
a separate Xia.map smoke test. The ZIP is extracted and checked again. Negative
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
not simulated by restricting PATH. The project currently has no explicit own
LICENSE file; choosing one is a prerequisite for public distribution, outside
this packaging milestone.
