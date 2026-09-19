#!/usr/bin/env python3
"""Strict structural verifier for the local OpenEmperor developer bundle."""
import argparse
import json
import os
import plistlib
import subprocess
import sys
from pathlib import Path

SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")
RESOURCE_FILES = {"BuildInfo.json", "LICENSE-SDL.txt", "LICENSE-OpenSSL.txt",
                  "LICENSE-nlohmann-json.txt"}


def run(*args):
    return subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def verify(app, signature=True, required_arch="arm64"):
    app = app.absolute()
    if not app.is_dir() or app.is_symlink():
        raise ValueError("app missing or symbolic link")
    contents = app / "Contents"
    plist_path = contents / "Info.plist"
    with plist_path.open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleIdentifier") != "org.openemperor.OpenEmperor":
        raise ValueError("wrong bundle identifier")
    if info.get("CFBundleExecutable") != "OpenEmperor":
        raise ValueError("wrong executable name")
    min_os = info.get("LSMinimumSystemVersion")
    if not isinstance(min_os, str) or not min_os:
        raise ValueError("missing deployment target")
    executable = contents / "MacOS" / "OpenEmperor"
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError("bundle executable missing or not executable")
    allowed_dirs = {"Contents", "Contents/MacOS", "Contents/Frameworks",
                    "Contents/Resources", "Contents/_CodeSignature"}
    allowed_files = {"Contents/Info.plist", "Contents/MacOS/OpenEmperor",
                     "Contents/_CodeSignature/CodeResources"}
    for name in RESOURCE_FILES:
        allowed_files.add("Contents/Resources/" + name)
    macho = [executable]
    framework_dir = contents / "Frameworks"
    if not framework_dir.is_dir():
        raise ValueError("Frameworks directory missing")
    for path in app.rglob("*"):
        relative = path.relative_to(app).as_posix()
        if path.is_symlink():
            resolved = path.resolve(strict=True)
            if not resolved.is_relative_to(app.resolve()):
                raise ValueError("symlink escapes bundle: " + relative)
            if not path.is_relative_to(framework_dir):
                raise ValueError("symlink outside Frameworks: " + relative)
        elif path.is_dir():
            if relative not in allowed_dirs:
                raise ValueError("unlisted bundle directory: " + relative)
        elif path.is_file():
            if path.is_relative_to(framework_dir):
                if path.suffix != ".dylib":
                    raise ValueError("unlisted Frameworks file: " + relative)
                macho.append(path)
            elif relative not in allowed_files:
                raise ValueError("unlisted bundle file: " + relative)
        else:
            raise ValueError("unlisted bundle item: " + relative)
    if not RESOURCE_FILES.issubset({p.name for p in (contents / "Resources").iterdir()}):
        raise ValueError("license or build info missing")
    build_info_path = contents / "Resources" / "BuildInfo.json"
    try:
        build_info = json.loads(build_info_path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError("invalid BuildInfo.json") from error
    required_build_info = {"display_version", "project_version", "revision", "dirty",
                           "architecture", "build_type", "deployment_target"}
    if not required_build_info.issubset(build_info):
        raise ValueError("BuildInfo.json lacks release provenance")
    if (build_info["project_version"] != info.get("CFBundleShortVersionString") or
            build_info["architecture"] != required_arch or
            build_info["deployment_target"] != min_os or
            not isinstance(build_info["dirty"], bool) or
            not all(isinstance(build_info[key], str) and build_info[key]
                    for key in required_build_info - {"dirty"})):
        raise ValueError("BuildInfo.json conflicts with the bundle")
    if any(Path(value).is_absolute() for value in build_info.values() if isinstance(value, str)):
        raise ValueError("BuildInfo.json contains an absolute path")
    if len(macho) < 2:
        raise ValueError("expected embedded dynamic libraries")
    report = []
    for item in macho:
        rel = item.relative_to(app).as_posix()
        arches = run("lipo", "-archs", str(item)).strip().split()
        if required_arch not in arches:
            raise ValueError("missing architecture slice: " + rel)
        build = run("vtool", "-show-build", str(item))
        version_lines = [line.strip().split() for line in build.splitlines()]
        actual_min = next((parts[1] for parts in version_lines if len(parts) == 2 and parts[0] == "minos"), None)
        if not actual_min:
            raise ValueError("missing LC_BUILD_VERSION: " + rel)
        if tuple(map(int, actual_min.split("."))) > tuple(map(int, min_os.split("."))):
            raise ValueError("Mach-O requires newer macOS than plist: " + rel)
        load_lines = run("otool", "-L", str(item)).splitlines()[1:]
        dependencies = [line.strip().split(" (")[0] for line in load_lines]
        ids = run("otool", "-D", str(item)).splitlines()[1:]
        install_id = ids[0].strip() if ids else None
        if item != executable and (not install_id or not install_id.startswith(
                ("@executable_path/", "@loader_path/"))):
            raise ValueError("unsafe library install ID: " + rel + ": " + str(install_id))
        if item != executable:
            base = executable.parent if install_id.startswith("@executable_path/") else item.parent
            suffix = install_id.split("/", 1)[1]
            if (base / suffix).resolve() != item.resolve():
                raise ValueError("library install ID does not name itself: " + rel)
        rpaths = []
        lines = run("otool", "-l", str(item)).splitlines()
        for i, line in enumerate(lines):
            if line.strip() == "cmd LC_RPATH":
                rpaths.extend(part.strip().split()[1] for part in lines[i:i+5]
                              if part.strip().startswith("path "))
        for entry in rpaths:
            if entry.startswith("/"):
                raise ValueError("absolute RPATH: " + rel + ": " + entry)
            if entry.startswith("@executable_path/"):
                rpath_dir = executable.parent / entry[len("@executable_path/"):]
            elif entry.startswith("@loader_path/"):
                rpath_dir = item.parent / entry[len("@loader_path/"):]
            else:
                raise ValueError("unrecognized RPATH: " + rel + ": " + entry)
            if not rpath_dir.resolve().is_relative_to(app.resolve()):
                raise ValueError("RPATH escapes bundle: " + rel + ": " + entry)
        for dep in dependencies:
            if dep == install_id:
                continue
            if dep.startswith(SYSTEM_PREFIXES):
                continue
            candidates = []
            if dep.startswith("@executable_path/"):
                candidates = [executable.parent / dep[len("@executable_path/"):]]
            elif dep.startswith("@loader_path/"):
                candidates = [item.parent / dep[len("@loader_path/"):]]
            elif dep.startswith("@rpath/"):
                for rp in rpaths:
                    if rp.startswith("@executable_path/"):
                        base = executable.parent / rp[len("@executable_path/"):]
                    elif rp.startswith("@loader_path/"):
                        base = item.parent / rp[len("@loader_path/"):]
                    else:
                        continue
                    candidates.append(base / dep[len("@rpath/"):])
            if not any(candidate.exists() and candidate.resolve().is_relative_to(app.resolve())
                       for candidate in candidates):
                raise ValueError("unresolved or external dependency: " + rel + ": " + dep)
        report.append({"path": rel, "architectures": arches, "minimum_macos": actual_min,
                       "install_id": install_id, "dependencies": dependencies, "rpaths": rpaths})
    if signature:
        run("codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app))
    return {"bundle_id": info["CFBundleIdentifier"], "version": info["CFBundleShortVersionString"],
            "display_version": build_info["display_version"],
            "declared_minimum_macos": min_os, "mach_o": report,
            "ad_hoc_signature_valid": bool(signature)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("app", type=Path)
    parser.add_argument("--skip-signature", action="store_true")
    parser.add_argument("--required-arch", default="arm64")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.app, not args.skip_signature, args.required_arch), sort_keys=True))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("bundle verification failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
