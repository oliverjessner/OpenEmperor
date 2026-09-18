#!/usr/bin/env python3
"""Build and verify a local, ad-hoc-signed arm64 development app."""
import hashlib
import json
import os
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

from verify_macos_bundle import verify

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-macos-app"
DIST = ROOT / "dist"
MIN_OS = "26.0"


def call(*args, cwd=ROOT, env=None, timeout=1800):
    return subprocess.run(args, cwd=cwd, env=env, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          timeout=timeout).stdout


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_industry_check(executable, data, workdir, environment):
    output = call(str(executable), "--data", str(data), "--sandbox", "Cities/Xia.map",
                  "--sandbox-rules", "sandbox-industry-v5", "--sandbox-check",
                  "--sandbox-resume-check", "--report-json", cwd=workdir,
                  env=environment, timeout=180)
    result = json.loads(output)
    if (result.get("pottery_completed_total", 0) <= 0 or
            result.get("distinct_supplied", 0) < 2 or
            not result.get("resume", {}).get("continued_equal")):
        raise RuntimeError("Industry check did not produce, deliver and resume")
    return {"ticks": result["ticks"], "pottery_completed_total": result["pottery_completed_total"],
            "distinct_supplied": result["distinct_supplied"], "resume": result["resume"]}


def u16(data, offset, value):
    struct.pack_into("<H", data, offset, value)


def u32(data, offset, value):
    struct.pack_into("<I", data, offset, value)


def sg3(tile):
    data = bytearray(40680 + (203 if tile else 202) * 64)
    u32(data, 0, len(data)); u32(data, 4, 213)
    u32(data, 12, 203 if tile else 202)
    u32(data, 16, 202 if tile else 201)
    u32(data, 20, 1)
    data[680:680 + len(b"Zeus_system.bmp")] = b"Zeus_system.bmp"
    u32(data, 680 + 124, 200); u32(data, 680 + 128, 1); u32(data, 680 + 132, 200)
    if tile:
        at = 40680 + 201 * 64
        u32(data, at + 4, 3200); u32(data, at + 8, 3200)
        u16(data, at + 20, 78); u16(data, at + 22, 40)
        u16(data, at + 50, 30); data[at + 55] = 1
    return data


def synthetic_data(root):
    # Independently constructed bytes following the existing MenuSessionTests profile.
    assets = root / "DATA"; cities = root / "Cities"
    assets.mkdir(parents=True); cities.mkdir()
    (assets / "China_Terrain.sg3").write_bytes(sg3(True))
    (assets / "China_Elevation.sg3").write_bytes(sg3(False))
    (assets / "China_Terrain.555").write_bytes(struct.pack("<H", 0x03e0) * 1600)
    (assets / "China_Elevation.555").write_bytes(b"")
    raw = bytearray(469391 + 228 * 228 * 4)
    raw[:8] = bytes((5, 0, 0xfe, 0xca, 0, 0, 2, 0))
    u32(raw, 84, 84)
    for index in range(228 * 228):
        u32(raw, 1535 + 4 * index, 0xc000)
        raw[209471 + index] = 64
        u32(raw, 261455 + 4 * index, 0x80)
    packed = bytearray((0xaa, 0xba, 0xdc, 0xfe))
    for offset in range(0, len(raw), 32768):
        block = raw[offset:offset + 32768]
        compressed = zlib.compress(block, 6)
        packed += struct.pack("<III", 0x12345678, len(compressed), len(block))
        packed += compressed
    (cities / "Xia.map").write_bytes(packed)


def check_negative(app, scratch):
    results = {}

    def reject(name, mutate, arch="arm64", signed=False):
        target = scratch / name / "OpenEmperor.app"
        target.parent.mkdir(parents=True)
        shutil.copytree(app, target, symlinks=True)
        mutate(target)
        try:
            verify(target, signed, arch)
        except (ValueError, OSError, subprocess.CalledProcessError):
            results[name] = "rejected"
        else:
            raise RuntimeError("negative case accepted: " + name)

    reject("missing-library", lambda p: next((p / "Contents/Frameworks").glob("libSDL3*.dylib")).unlink())
    def external(p):
        exe = p / "Contents/MacOS/OpenEmperor"
        call("install_name_tool", "-change", "@executable_path/../Frameworks/libSDL3.0.dylib",
             "/opt/homebrew/lib/libSDL3.0.dylib", str(exe))
    reject("external-dependency", external)
    reject("escaping-symlink", lambda p: (p / "Contents/Frameworks/escape.dylib").symlink_to("/tmp/escape.dylib"))
    reject("wrong-arch", lambda p: None, arch="x86_64")
    reject("invalid-plist", lambda p: (p / "Contents/Info.plist").write_text("invalid"))
    reject("missing-executable", lambda p: (p / "Contents/MacOS/OpenEmperor").unlink())
    reject("forbidden-asset", lambda p: (p / "Contents/Resources/original.sg3").write_bytes(b"synthetic"))
    reject("invalid-signature", lambda p: (p / "Contents/Resources/BuildInfo.json").write_text("changed"), signed=True)
    occupied = scratch / "write-failure"; occupied.write_bytes(b"occupied")
    try:
        occupied.mkdir()
    except FileExistsError:
        results["write-failure"] = "rejected"
    else:
        raise RuntimeError("write failure was not propagated")
    return results


def main():
    if sys.platform != "darwin" or call("uname", "-m").strip() != "arm64":
        raise RuntimeError("native macOS arm64 host required")
    for tool in ("cmake", "clang", "otool", "lipo", "vtool", "install_name_tool",
                 "codesign", "ditto", "brew"):
        if not shutil.which(tool):
            raise RuntimeError("required build tool missing: " + tool)
    revision = call("git", "rev-parse", "--short=12", "HEAD").strip()
    dirty = bool(call("git", "status", "--porcelain").strip())
    cmake_args = ("cmake", "-S", str(ROOT), "-B", str(BUILD), "-DCMAKE_BUILD_TYPE=Release",
                  "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={MIN_OS}",
                  "-DOPENEMPEROR_BUILD_MACOS_APP=ON", "-DOPENEMPEROR_USE_SYSTEM_SDL3=ON")
    print(call(*cmake_args), flush=True)
    print(call("cmake", "--build", str(BUILD), "--parallel", "6"), flush=True)
    release_tests = call("ctest", "--test-dir", str(BUILD), "--output-on-failure", "-j", "6")
    print(release_tests, flush=True)
    DIST.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".openemperor-package-", dir=DIST) as temp:
        stage = Path(temp)
        print(call("cmake", "--install", str(BUILD), "--prefix", str(stage)), flush=True)
        app = stage / "OpenEmperor.app"
        exe = app / "Contents/MacOS/OpenEmperor"
        # Search directories come from the linked Mach-O, not a find_package guess.
        linked = [line.strip().split(" (")[0] for line in call("otool", "-L", str(exe)).splitlines()[1:]]
        external = [Path(dep) for dep in linked if dep.startswith("/") and not
                    dep.startswith(("/usr/lib/", "/System/Library/"))]
        if not external:
            raise RuntimeError("no non-system dynamic dependencies found")
        library_dirs = sorted({str(path.parent) for path in external})
        print(call("cmake", f"-DAPP_PATH={app}", f"-DLIBRARY_DIRS={';'.join(library_dirs)}",
                   "-P", str(ROOT / "cmake/FixupMacOSBundle.cmake")), flush=True)
        for item in [exe, *(app / "Contents/Frameworks").rglob("*.dylib")]:
            output = call("otool", "-l", str(item)).splitlines()
            rpaths = [part.strip().split()[1] for i, line in enumerate(output)
                      if line.strip() == "cmd LC_RPATH" for part in output[i:i + 5]
                      if part.strip().startswith("path ")]
            for path in rpaths:
                if path.startswith("/"):
                    call("install_name_tool", "-delete_rpath", path, str(item))
        resources = app / "Contents/Resources"; resources.mkdir()
        with (app / "Contents/Info.plist").open("rb") as stream:
            version = plistlib.load(stream)["CFBundleShortVersionString"]
        sdl_prefix = Path(call("brew", "--prefix", "sdl3").strip()).resolve()
        openssl_prefix = Path(call("brew", "--prefix", "openssl@3").strip()).resolve()
        json_prefix = Path(call("brew", "--prefix", "nlohmann-json").strip()).resolve()
        for source, name in ((sdl_prefix / "LICENSE.txt", "LICENSE-SDL.txt"),
                             (openssl_prefix / "LICENSE.txt", "LICENSE-OpenSSL.txt"),
                             (json_prefix / "LICENSE.MIT", "LICENSE-nlohmann-json.txt")):
            shutil.copyfile(source, resources / name)
        build_info = {"version": version, "revision": revision, "dirty": dirty,
                      "target_architecture": "arm64", "deployment_target": MIN_OS,
                      "sdl_version": sdl_prefix.name, "openssl_version": openssl_prefix.name,
                      "nlohmann_json_version": json_prefix.name}
        (resources / "BuildInfo.json").write_text(json.dumps(build_info, indent=2) + "\n")
        for lib in sorted((app / "Contents/Frameworks").rglob("*.dylib")):
            if not lib.is_symlink():
                call("codesign", "--force", "--sign", "-", str(lib))
        call("codesign", "--force", "--sign", "-", str(app))
        structural = verify(app)
        # Always execute the relocated and unzipped executable, never the build target.
        with tempfile.TemporaryDirectory(prefix="OpenEmperor relocated ") as move_root:
            moved = Path(move_root) / "Moved App" / "OpenEmperor.app"
            moved.parent.mkdir(); shutil.copytree(app, moved, symlinks=True)
            moved_structure = verify(moved)
            clean_env = {k: v for k, v in os.environ.items() if not k.startswith("DYLD_") and
                         not k.startswith("OPENSSL_") and not k.startswith("OPENEMPEROR_")}
            clean_env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
            clean_env["HOME"] = str(Path(move_root) / "Home")
            Path(clean_env["HOME"]).mkdir()
            relocated_exe = moved / "Contents/MacOS/OpenEmperor"
            menu_result = call(str(relocated_exe), "--menu-check", "--app-root",
                               str(Path(move_root) / "fresh preferences"), cwd=Path(move_root),
                               env=clean_env, timeout=90)
            print(menu_result, flush=True)
            # Synthetic fixture generated outside the bundle.
            fixture = Path(move_root) / "synthetic data"
            synthetic_data(fixture)
            synthetic_result = call(str(relocated_exe), "--menu-check", "--app-root",
                                    str(Path(move_root) / "synthetic preferences"), "--data",
                                    str(fixture), "--industry", cwd=Path(move_root),
                                    env=clean_env, timeout=120)
            print(synthetic_result, flush=True)
            synthetic_industry = run_industry_check(relocated_exe, fixture, Path(move_root), clean_env)
            original_data = ROOT / ".local/gog-extracted/app"
            original_result = "not_available"
            original_industry = "not_available"
            original_files_unchanged = None
            signature_after_original = None
            if (original_data.is_dir() and (original_data / "Cities/Xia.map").is_file()):
                originals = [original_data / "Cities/Xia.map",
                             original_data / "DATA/China_Terrain.sg3",
                             original_data / "DATA/China_Elevation.sg3"]
                originals_before = {path.name: sha256(path) for path in originals}
                original_result = call(str(relocated_exe), "--menu-check", "--app-root",
                                       str(Path(move_root) / "original preferences"), "--data",
                                       str(original_data), "--industry", cwd=Path(move_root),
                                       env=clean_env, timeout=180).strip()
                original_industry = run_industry_check(relocated_exe, original_data,
                                                       Path(move_root), clean_env)
                if originals_before != {path.name: sha256(path) for path in originals}:
                    raise RuntimeError("original data changed during smoke check")
                original_files_unchanged = True
                verify(moved)
                signature_after_original = True
            zip_name = f"OpenEmperor-{version}-dev-{revision}-macos-arm64.zip"
            archive = stage / zip_name
            call("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(app), str(archive))
            extracted = Path(move_root) / "Unzipped App"; extracted.mkdir()
            call("ditto", "-x", "-k", str(archive), str(extracted))
            unzipped = extracted / "OpenEmperor.app"
            zip_structure = verify(unzipped)
            zip_result = call(str(unzipped / "Contents/MacOS/OpenEmperor"), "--menu-check",
                              "--app-root", str(Path(move_root) / "unzipped preferences"),
                              cwd=Path(move_root), env=clean_env, timeout=90).strip()
            negative = check_negative(app, stage / "negative")
            report = {"build": build_info, "host": {"macos": call("sw_vers", "-productVersion").strip(),
                      "architecture": call("uname", "-m").strip(),
                      "cmake": call("cmake", "--version").splitlines()[0],
                      "compiler": call("clang", "--version").splitlines()[0],
                      "sdk": call("xcrun", "--show-sdk-version").strip()},
                      "bundle": structural, "relocated": moved_structure, "unzipped": zip_structure,
                      "checks": {"release_ctest": "passed", "fresh_menu": menu_result.strip(),
                                 "synthetic_menu_save_resume": synthetic_result.strip(),
                                 "synthetic_industry": synthetic_industry,
                                 "original_data_smoke": original_result,
                                 "original_industry": original_industry,
                                 "original_files_unchanged": original_files_unchanged,
                                 "bundle_signature_after_original": signature_after_original,
                                 "unzipped_menu": zip_result, "negative": negative},
                      "signature": "ad-hoc; Developer ID absent; not notarized",
                      "desktop_launch": "not_checked", "independent_clean_mac": "not_checked",
                      "project_license": "missing; publication blocker"}
            report["zip"] = {"filename": zip_name, "bytes": archive.stat().st_size,
                             "sha256": sha256(archive)}
            (stage / "package-report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
            (stage / "SHA256SUMS").write_text(f"{sha256(archive)}  {zip_name}\n")
            # Prepare all replacements first. Roll back exact prior outputs on failure.
            prepared = stage / "publish"; prepared.mkdir()
            shutil.copytree(app, prepared / "OpenEmperor.app", symlinks=True)
            for name, source in ((zip_name, archive),
                                 ("package-report.json", stage / "package-report.json"),
                                 ("SHA256SUMS", stage / "SHA256SUMS")):
                shutil.copyfile(source, prepared / name)
            names = ("OpenEmperor.app", zip_name, "package-report.json", "SHA256SUMS")
            previous = stage / "previous"; previous.mkdir()
            published = []
            try:
                for name in names:
                    target = DIST / name
                    if target.exists():
                        target.rename(previous / name)
                    (prepared / name).rename(target)
                    published.append(target)
            except Exception:
                for target in published:
                    if target.is_dir():
                        shutil.rmtree(target)
                    else:
                        target.unlink()
                for old in previous.iterdir():
                    old.rename(DIST / old.name)
                raise
            print(f"PACKAGED {DIST / zip_name}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as error:
        print("packaging failed: " + str(error), file=sys.stderr)
        sys.exit(1)
