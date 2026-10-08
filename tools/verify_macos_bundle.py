#!/usr/bin/env python3
"""Strict structural verifier for the local OpenEmperor developer bundle."""
import argparse
import json
import math
import os
import plistlib
import subprocess
import sys
from pathlib import Path

SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")
RESOURCE_FILES = {"BuildInfo.json", "LICENSE-SDL.txt", "LICENSE-OpenSSL.txt",
                  "LICENSE-nlohmann-json.txt"}
COMPATIBILITY_ID = "gog-derived-2.0.0.2-en-assetset-1"
COMPATIBILITY_FILES = {"manifest.json", "walkers.json", "buildings.json", "roads.json", "fire.json",
                       "fire-inspector.json", "market-walkers.json"}
FORBIDDEN_COMPATIBILITY_SUFFIXES = {".sg3", ".555", ".map", ".pak", ".exe", ".png",
                                    ".jpg", ".jpeg", ".bmp", ".rgba", ".raw"}
MAX_COMPATIBILITY_JSON_BYTES = 256 * 1024


def run(*args):
    return subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout


def validate_compatibility_json(path):
    if path.stat().st_size > MAX_COMPATIBILITY_JSON_BYTES:
        raise ValueError("compatibility JSON exceeds size limit: " + path.name)
    try:
        document = json.loads(path.read_text())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("invalid compatibility JSON: " + path.name) from error

    def visit(value, key=""):
        if isinstance(value, dict):
            for child_key, child in value.items():
                lowered = child_key.lower()
                if any(token in lowered for token in ("blob", "binary", "pixels", "base64")):
                    raise ValueError("binary/blob field in compatibility JSON: " + child_key)
                visit(child, child_key)
        elif isinstance(value, list):
            for child in value:
                visit(child, key)
        elif isinstance(value, str) and key in {"path", "archive", "walker", "building", "road", "profile", "core_files"}:
            candidate = Path(value)
            if candidate.is_absolute() or ".." in candidate.parts or "." in candidate.parts or "\\" in value:
                raise ValueError("unsafe path in compatibility JSON: " + value)
    visit(document)
    return document


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
                    "Contents/Resources", "Contents/_CodeSignature",
                    "Contents/Resources/Compatibility",
                    "Contents/Resources/Compatibility/" + COMPATIBILITY_ID}
    allowed_files = {"Contents/Info.plist", "Contents/MacOS/OpenEmperor",
                     "Contents/_CodeSignature/CodeResources"}
    for name in RESOURCE_FILES:
        allowed_files.add("Contents/Resources/" + name)
    compatibility_prefix = "Contents/Resources/Compatibility/" + COMPATIBILITY_ID + "/"
    for name in COMPATIBILITY_FILES:
        allowed_files.add(compatibility_prefix + name)
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
            if path.is_relative_to(contents / "Resources" / "Compatibility") and path.suffix.lower() in FORBIDDEN_COMPATIBILITY_SUFFIXES:
                raise ValueError("proprietary/binary asset type in Compatibility: " + relative)
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
    compatibility_dir = contents / "Resources" / "Compatibility" / COMPATIBILITY_ID
    if (not compatibility_dir.is_dir() or compatibility_dir.is_symlink() or
            {path.name for path in compatibility_dir.iterdir()} != COMPATIBILITY_FILES):
        raise ValueError("compatibility metadata set is incomplete or unexpected")
    compatibility_documents = {
        name: validate_compatibility_json(compatibility_dir / name)
        for name in sorted(COMPATIBILITY_FILES)
    }
    manifest = compatibility_documents["manifest.json"]
    if (manifest.get("schema_version") != 1 or manifest.get("id") != COMPATIBILITY_ID or
            manifest.get("profiles") != {"walker": "walkers.json",
                                          "building": "buildings.json",
                                          "road": "roads.json"}):
        raise ValueError("compatibility manifest identity or profile allowlist changed")
    optional_fire = manifest.get("optional_fire")
    if (not isinstance(optional_fire, dict) or
            set(optional_fire) != {"profile", "files"} or
            optional_fire.get("profile") != "fire.json" or
            not isinstance(optional_fire.get("files"), list) or
            len(optional_fire["files"]) != 2):
        raise ValueError("optional fire compatibility metadata is incomplete")
    fire_paths = set()
    for item in optional_fire["files"]:
        if (not isinstance(item, dict) or set(item) != {"path", "sha256"} or
                not isinstance(item.get("path"), str) or
                not isinstance(item.get("sha256"), str) or
                len(item["sha256"]) != 64 or
                any(c not in "0123456789abcdef" for c in item["sha256"])):
            raise ValueError("optional fire fingerprint is malformed")
        fire_paths.add(item["path"])
    if fire_paths != {"DATA/destruction.sg3", "DATA/destruction.555"}:
        raise ValueError("optional fire source-file set changed")
    fire = compatibility_documents["fire.json"]
    if (not isinstance(fire, dict) or fire.get("schema_version") != 1 or
            fire.get("mode") != "curated_fire_presentation" or
            not isinstance(fire.get("clip_id"), str) or not fire["clip_id"] or
            not isinstance(fire.get("evidence"), str) or not fire["evidence"] or
            type(fire.get("ticks_per_frame")) is not int or
            fire["ticks_per_frame"] != 2 or
            not isinstance(fire.get("frames"), list) or len(fire["frames"]) != 50):
        raise ValueError("fire clip metadata is malformed or unbounded")
    for number, frame in enumerate(fire["frames"]):
        if (not isinstance(frame, dict) or frame.get("archive") != "DATA/destruction.sg3" or
                type(frame.get("image_index")) is not int or
                frame["image_index"] != 201 + number or
                not isinstance(frame.get("anchor"), list) or len(frame["anchor"]) != 2 or
                not all(type(v) in (int, float) and math.isfinite(v) and abs(v) <= 256
                        for v in frame["anchor"]) or frame["anchor"] != [40, 80]):
            raise ValueError("fire frame metadata is malformed")
    optional_inspector = manifest.get("optional_fire_inspector")
    if (not isinstance(optional_inspector, dict) or
            set(optional_inspector) != {"profile", "core_files", "files"} or
            optional_inspector.get("profile") != "fire-inspector.json" or
            optional_inspector.get("core_files") != ["DATA/SprMain.sg3", "DATA/SprMain.555"] or
            optional_inspector.get("files") != []):
        raise ValueError("Inspector dependency declaration changed")
    inspector_core_hashes = {
        "DATA/SprMain.sg3": "3e2817d2644453acc92068d6c1e26532212be213b43b848ebe95ba21654adef8",
        "DATA/SprMain.555": "d84eb6759e9ce50b0ad75596773b9224f9c1a9b8dd80b691c066c3de7e8df438",
    }
    core_files = manifest.get("files")
    if not isinstance(core_files, list):
        raise ValueError("Inspector reused core fingerprint list is missing")
    for path, digest in inspector_core_hashes.items():
        pins = [item for item in core_files if isinstance(item, dict) and item.get("path") == path]
        if len(pins) != 1 or pins[0].get("sha256") != digest:
            raise ValueError("Inspector reused core fingerprint is missing or changed")
    inspector = compatibility_documents["fire-inspector.json"]
    if (not isinstance(inspector, dict) or inspector.get("schema_version") != 3 or
            inspector.get("mode") != "curated_walker_preview" or
            not isinstance(inspector.get("roles"), dict) or
            set(inspector["roles"]) != {"fire_inspector"}):
        raise ValueError("Inspector role/schema metadata is invalid")
    role = inspector["roles"]["fire_inspector"]
    if (not isinstance(role, dict) or role.get("clip_id") != "curated-sprmain-inspector-walk" or
            type(role.get("ticks_per_frame")) is not int or role["ticks_per_frame"] != 2 or
            not isinstance(role.get("evidence"), str) or not 1 <= len(role["evidence"]) <= 512 or
            role.get("idle") != "neg_x_0" or
            not isinstance(role.get("frames"), list) or len(role["frames"]) != 48 or
            not isinstance(role.get("clips"), dict) or
            set(role["clips"]) != {"pos_x", "neg_x", "pos_y", "neg_y"}):
        raise ValueError("Inspector clip metadata is invalid")
    bases = {"neg_x": 433, "neg_y": 434, "pos_x": 435, "pos_y": 436}
    frames = {}
    for frame in role["frames"]:
        if (not isinstance(frame, dict) or not isinstance(frame.get("alias"), str) or
                frame["alias"] in frames or frame.get("archive") != "DATA/SprMain.sg3" or
                type(frame.get("image_index")) is not int or frame["image_index"] <= 0 or
                not isinstance(frame.get("foot_anchor"), list) or len(frame["foot_anchor"]) != 2 or
                not all(type(v) in (int, float) and math.isfinite(v) and abs(v) <= 256
                        for v in frame["foot_anchor"])):
            raise ValueError("Inspector frame metadata is invalid")
        frames[frame["alias"]] = frame
    for direction, base in bases.items():
        aliases = [f"{direction}_{phase}" for phase in range(12)]
        if role["clips"][direction] != aliases:
            raise ValueError("Inspector required direction/sequence changed")
        for phase, alias in enumerate(aliases):
            if alias not in frames or frames[alias]["image_index"] != base + 8 * phase:
                raise ValueError("Inspector required physical frame changed")
    optional_market = manifest.get("optional_market_walkers")
    market_pins = {
        "DATA/SprMain2.sg3": "09fc9fccddb9a30266740325b01cf69c53b26ed08016f9ecdd1759e7afb40b39",
        "DATA/SprMain2.555": "53b1a9b5154316e505ac1a0d46b6796c1f568f4765ec154e313be0aff82daa7e",
    }
    if (not isinstance(optional_market, dict) or
            set(optional_market) != {"profile", "files"} or
            optional_market.get("profile") != "market-walkers.json" or
            not isinstance(optional_market.get("files"), list) or len(optional_market["files"]) != 2 or
            any(not isinstance(item, dict) or set(item) != {"path", "sha256"} or
                not isinstance(item.get("path"), str) or not isinstance(item.get("sha256"), str)
                for item in optional_market["files"]) or
            {item["path"]: item["sha256"] for item in optional_market["files"]} != market_pins):
        raise ValueError("Market separate dependency fingerprints changed")
    market = compatibility_documents["market-walkers.json"]
    if (not isinstance(market, dict) or market.get("schema_version") != 4 or market.get("mode") != "curated_walker_preview" or
            not isinstance(market.get("roles"), dict) or
            set(market["roles"]) != {"supplier", "distributor"}):
        raise ValueError("Market role/schema metadata is invalid")
    for family, base in {"supplier": 3605, "distributor": 5585}.items():
        role = market["roles"][family]
        if (not isinstance(role, dict) or role.get("clip_id") != f"curated-sprmain2-{family}-walk" or
                type(role.get("ticks_per_frame")) is not int or role["ticks_per_frame"] != 2 or
                not isinstance(role.get("evidence"), str) or not 1 <= len(role["evidence"]) <= 512 or
                not isinstance(role.get("frames"), list) or len(role["frames"]) != 48 or
                not isinstance(role.get("clips"), dict) or
                set(role["clips"]) != {"pos_x", "neg_x", "pos_y", "neg_y"}):
            raise ValueError("Market clip metadata is invalid")
        frames = {}
        for frame in role["frames"]:
            if (not isinstance(frame, dict) or
                    set(frame) != {"alias", "archive", "image_index", "foot_anchor", "flip_x"} or
                    not isinstance(frame.get("alias"), str) or frame["alias"] in frames or
                    frame.get("archive") != "DATA/SprMain2.sg3" or
                    type(frame.get("image_index")) is not int or frame["image_index"] <= 0 or
                    type(frame.get("flip_x")) is not bool or
                    not isinstance(frame.get("foot_anchor"), list) or len(frame["foot_anchor"]) != 2 or
                    not all(type(v) in (int, float) and abs(v) <= 256 and math.isfinite(v)
                            for v in frame["foot_anchor"])):
                raise ValueError("Market frame/display transform metadata is invalid")
            frames[frame["alias"]] = frame
        if not isinstance(role.get("idle"), str) or role["idle"] not in frames:
            raise ValueError("Market idle frame is missing")
        for direction, offset in {"neg_y": 0, "neg_x": 0, "pos_x": 2, "pos_y": 2}.items():
            aliases = [f"{direction}_{phase}" for phase in range(12)]
            if role["clips"][direction] != aliases:
                raise ValueError("Market required direction/sequence changed")
            for phase, alias in enumerate(aliases):
                if (alias not in frames or frames[alias]["image_index"] != base + offset + 8 * phase or
                        frames[alias]["flip_x"] != (direction in {"neg_x", "pos_y"})):
                    raise ValueError("Market native physical frame or explicit display flip changed")
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
            "ad_hoc_signature_valid": bool(signature),
            "compatibility_pack": COMPATIBILITY_ID,
            "compatibility_json_files": sorted(COMPATIBILITY_FILES),
            "proprietary_assets_in_compatibility": 0}


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
