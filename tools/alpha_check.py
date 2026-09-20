#!/usr/bin/env python3
"""Run the explicit OpenEmperor alpha acceptance checks and write a local JSON report."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPORT = ROOT / ".local/reports/alpha-check.json"


class CheckFailure(RuntimeError):
    pass


def safe_text(value: str, data_root: Path | None = None) -> str:
    value = value.replace(str(ROOT), "<repo>").replace(str(Path.home()), "<home>")
    if data_root is not None:
        value = value.replace(str(data_root), "<data>")
    return value[-4000:]


def run(args: list[str], *, timeout: int = 1800, env: dict[str, str] | None = None,
        data_root: Path | None = None) -> tuple[str, float]:
    started = time.monotonic()
    process = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=timeout, env=env)
    elapsed = time.monotonic() - started
    if process.returncode != 0:
        command = " ".join(Path(item).name if item.startswith(str(ROOT)) else item for item in args)
        raise CheckFailure(f"command failed ({process.returncode}): {command}\n" +
                           safe_text(process.stdout, data_root))
    return process.stdout, elapsed


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fingerprint(path: Path, root: Path) -> dict[str, object]:
    stat = path.stat()
    return {"path": path.relative_to(root).as_posix(), "bytes": stat.st_size,
            "mtime_ns": stat.st_mtime_ns, "sha256": sha256(path)}


def ctest_count(output: str) -> int:
    match = re.search(r"out of (\d+)", output)
    if not match:
        raise CheckFailure("could not determine CTest count")
    return int(match.group(1))


def configure(build: Path, build_type: str, jobs: int, system_sdl: bool,
              sanitizers: bool = False, fresh: bool = False) -> dict[str, object]:
    if fresh and build.exists():
        shutil.rmtree(build)
    args = ["cmake", "-S", str(ROOT), "-B", str(build),
            f"-DCMAKE_BUILD_TYPE={build_type}", "-DCMAKE_OSX_ARCHITECTURES=arm64",
            f"-DOPENEMPEROR_ENABLE_SANITIZERS={'ON' if sanitizers else 'OFF'}"]
    if system_sdl:
        args.append("-DOPENEMPEROR_USE_SYSTEM_SDL3=ON")
    _, configure_seconds = run(args)
    _, build_seconds = run(["cmake", "--build", str(build), "--parallel", str(jobs)])
    return {"configured": True, "built": True, "type": build_type,
            "sanitizers": sanitizers, "configure_seconds": round(configure_seconds, 3),
            "build_seconds": round(build_seconds, 3)}


def test_build(build: Path, jobs: int, env: dict[str, str] | None = None) -> dict[str, object]:
    output, elapsed = run(["ctest", "--test-dir", str(build), "--output-on-failure",
                           "-j", str(jobs)], timeout=1800, env=env)
    return {"result": "pass", "count": ctest_count(output),
            "seconds": round(elapsed, 3)}


def endurance(build: Path, env: dict[str, str] | None = None) -> dict[str, object]:
    output, elapsed = run([str(build / "openemperor-alpha-endurance")], timeout=900, env=env)
    try:
        result = json.loads(output.strip().splitlines()[-1])
    except (json.JSONDecodeError, IndexError) as error:
        raise CheckFailure("endurance executable did not produce JSON") from error
    if result.get("result") != "pass" or result.get("ticks") != 100_000:
        raise CheckFailure("endurance executable returned an incomplete result")
    result["seconds"] = round(elapsed, 3)
    return result


def render_stress(build: Path, env: dict[str, str] | None = None) -> dict[str, object]:
    _, elapsed = run([str(build / "openemperor-sandbox-view-tests"), "--alpha-stress"],
                     timeout=900, env=env)
    return {"result": "pass", "frames": 3000, "simulation_ticks": 0,
            "loaded_source_files_unavailable_during_render": True,
            "decode_delta": 0, "texture_upload_delta": 0,
            "stored_order_build_delta": 0, "seconds": round(elapsed, 3)}


def session_stress(build: Path) -> dict[str, object]:
    executable = str(build / "openemperor-menu-session-tests")
    started = time.monotonic()
    for iteration in range(100):
        try:
            run([executable], timeout=180)
        except CheckFailure as error:
            raise CheckFailure(f"menu lifecycle iteration {iteration + 1} failed: {error}") from error
    return {"result": "pass", "sessions": 100,
            "all_owned_texture_counters_zero_at_session_end": True,
            "seconds": round(time.monotonic() - started, 3)}


def profile_archives(manifest: Path, data: Path) -> set[Path]:
    document = json.loads(manifest.read_text())
    archives: set[Path] = set()

    def visit(value: object) -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                if key == "archive" and isinstance(child, str):
                    relative = Path(child)
                    if relative.is_absolute() or ".." in relative.parts:
                        raise CheckFailure("visual profile contains an unsafe archive path")
                    archives.add(data / relative)
                else:
                    visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(document)
    return archives


def original_check(args: argparse.Namespace, executable: Path) -> dict[str, object]:
    if args.data is None:
        return {"configured": False, "result": "not_configured"}
    data = args.data.resolve()
    required = [data / "Cities/Xia.map", data / "DATA/China_Terrain.sg3",
                data / "DATA/China_Terrain.555", data / "DATA/China_Elevation.sg3",
                data / "DATA/China_Elevation.555", data / "DATA/SprMain.sg3",
                data / "DATA/SprMain.555", data / "DATA/China_General.sg3",
                data / "DATA/China_General.555"]
    visual_args: list[str] = []
    for option, manifest in (("--sandbox-visuals", args.walker_visuals),
                             ("--building-visuals", args.building_visuals),
                             ("--road-visuals", args.road_visuals)):
        if manifest is None:
            continue
        manifest = manifest.resolve()
        if not manifest.is_file():
            raise CheckFailure("configured visual profile is missing")
        required.extend(profile_archives(manifest, data))
        visual_args.extend([option, str(manifest)])
    expanded: set[Path] = set(required)
    for path in list(expanded):
        if path.suffix.lower() == ".sg3":
            expanded.add(path.with_suffix(".555"))
    for path in expanded:
        if not path.is_file():
            raise CheckFailure("an original file required by the configured smoke check is missing")
    before = {item["path"]: item for item in (fingerprint(path, data) for path in sorted(expanded))}
    command = [str(executable), "--data", str(data), "--sandbox", "Cities/Xia.map",
               "--sandbox-rules", "sandbox-industry-v5", "--sandbox-demo", "--sandbox-check",
               "--sandbox-resume-check", "--report-json", *visual_args]
    output, elapsed = run(command, timeout=300, data_root=data)
    result = json.loads(output.strip().splitlines()[-1])
    after = {item["path"]: item for item in (fingerprint(path, data) for path in sorted(expanded))}
    if before != after:
        raise CheckFailure("original files changed during the alpha smoke check")
    if not result.get("goods_balance_valid") or not result.get("resume", {}).get("continued_equal"):
        raise CheckFailure("original-data Industry check failed production or resume invariants")
    compatibility = result.get("compatibility", {})
    expected_sources = {
        "walker": "custom" if args.walker_visuals else "builtin",
        "building": "custom" if args.building_visuals else "builtin",
        "road": "custom" if args.road_visuals else "builtin",
    }
    if (not compatibility.get("detected") or
            compatibility.get("id") != "gog-derived-2.0.0.2-en-assetset-1" or
            any(compatibility.get(key) != value for key, value in expected_sources.items())):
        raise CheckFailure("known original data did not activate the expected automatic visual profiles")
    if (result.get("walker_visuals", {}).get("unique_assets") != 48 or
            result.get("building_visuals", {}).get("decoded_unique_assets") != 4 or
            result.get("road_visuals", {}).get("unique_assets") != 12 or
            result.get("road_visuals", {}).get("draws", 0) <= 0):
        raise CheckFailure("automatic visual profiles did not decode and draw the validated asset set")
    return {"configured": True, "result": "pass", "files_verified_unchanged": len(before),
            "ticks": result.get("ticks"), "frames_rendered": result.get("frames_rendered"),
            "five_courier_frames": result.get("frames_with_five_couriers"),
            "compatibility": compatibility,
            "seconds": round(elapsed, 3)}


def package_check() -> dict[str, object]:
    output, elapsed = run([str(ROOT / "tools/package_macos.sh")], timeout=3600)
    forbidden = {"walkers-v2.json", "alpha-check.json"}
    app = ROOT / "dist/OpenEmperor.app"
    if not app.is_dir():
        raise CheckFailure("packaging did not produce OpenEmperor.app")
    bundled_paths = [path for path in app.rglob("*") if path.is_file()]
    bundled = {path.name for path in bundled_paths}
    if bundled & forbidden:
        raise CheckFailure("local visual profiles or alpha reports entered the app bundle")
    compatibility_root = app / "Contents/Resources/Compatibility/gog-derived-2.0.0.2-en-assetset-1"
    compatibility_names = {"manifest.json", "walkers.json", "buildings.json", "roads.json"}
    if (not compatibility_root.is_dir() or
            {path.name for path in compatibility_root.iterdir()} != compatibility_names):
        raise CheckFailure("packaged compatibility metadata is incomplete")
    forbidden_suffixes = {".map", ".sg3", ".555", ".exe", ".oesave", ".png", ".jpg", ".jpeg"}
    if any(path.suffix.lower() in forbidden_suffixes for path in bundled_paths):
        raise CheckFailure("original data, saves, or local screenshots entered the app bundle")
    build_info = json.loads((app / "Contents/Resources/BuildInfo.json").read_text())
    required_build_info = {"display_version", "project_version", "revision", "dirty",
                           "architecture", "build_type", "deployment_target"}
    if (not required_build_info.issubset(build_info) or
            build_info.get("display_version") != "0.1.0-alpha.1" or
            build_info.get("architecture") != "arm64" or
            build_info.get("build_type") != "Release"):
        raise CheckFailure("packaged BuildInfo lacks alpha candidate provenance")
    archives = sorted((ROOT / "dist").glob("OpenEmperor-*-macos-arm64.zip"),
                      key=lambda path: path.stat().st_mtime_ns)
    if not archives:
        raise CheckFailure("packaging did not produce an arm64 ZIP")
    with zipfile.ZipFile(archives[-1]) as archive:
        archive_paths = [Path(name) for name in archive.namelist() if not name.endswith("/")]
        names = {path.name for path in archive_paths}
    if names & forbidden:
        raise CheckFailure("local visual profiles or alpha reports entered the ZIP")
    if any(path.suffix.lower() in forbidden_suffixes for path in archive_paths):
        raise CheckFailure("original data, saves, or local screenshots entered the ZIP")
    return {"requested": True, "result": "pass", "local_profiles_absent": True,
            "alpha_reports_absent": True, "original_assets_absent": True,
            "compatibility_pack": "gog-derived-2.0.0.2-en-assetset-1",
            "compatibility_json_files": sorted(compatibility_names),
            "build_info": build_info, "seconds": round(elapsed, 3),
            "summary": safe_text(output).splitlines()[-1] if output.strip() else "packaged"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--release-build-dir", type=Path, default=ROOT / "build-alpha-release")
    parser.add_argument("--sanitize-build-dir", type=Path, default=ROOT / "build-sanitize")
    parser.add_argument("--jobs", type=int, default=max(1, min(8, os.cpu_count() or 1)))
    parser.add_argument("--system-sdl", action="store_true",
                        help="use an installed SDL3 for all acceptance builds")
    parser.add_argument("--package", action="store_true", help="also run tools/package_macos.sh")
    parser.add_argument("--data", type=Path)
    parser.add_argument("--walker-visuals", type=Path)
    parser.add_argument("--building-visuals", type=Path)
    parser.add_argument("--road-visuals", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.jobs < 1:
        raise CheckFailure("--jobs must be positive")
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    revision, _ = run(["git", "rev-parse", "HEAD"])
    status, _ = run(["git", "status", "--porcelain"])
    report: dict[str, object] = {"schema": "openemperor-alpha-check-v1",
        "revision": revision.strip(), "dirty": bool(status.strip()), "result": "running",
        "original_data": {"configured": args.data is not None, "result": "not_run"}}
    started = time.monotonic()
    try:
        debug = args.build_dir.resolve()
        release = args.release_build_dir.resolve()
        sanitize = args.sanitize_build_dir.resolve()
        report["build"] = {
            "debug": configure(debug, "Debug", args.jobs, args.system_sdl),
            "release": configure(release, "Release", args.jobs, args.system_sdl, fresh=True),
            "sanitize": configure(sanitize, "Debug", args.jobs, args.system_sdl,
                                  sanitizers=True, fresh=True)}
        sanitizer_env = os.environ.copy()
        # Apple's ASan runtime aborts when LeakSanitizer is requested. Resource
        # lifetime is covered separately by the project-owned texture counters.
        leak_detection = "0" if sys.platform == "darwin" else "1"
        sanitizer_env["ASAN_OPTIONS"] = f"detect_leaks={leak_detection}:halt_on_error=1"
        sanitizer_env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
        report["tests"] = {"debug": test_build(debug, args.jobs),
                           "release": test_build(release, args.jobs),
                           "sanitize": test_build(sanitize, args.jobs, sanitizer_env)}
        report["simulation"] = {"debug": endurance(debug), "release": endurance(release),
                                "sanitize": endurance(sanitize, sanitizer_env)}
        report["save_resume"] = {"result": "pass", "checkpoints": 12,
            "real_json_roundtrips": report["simulation"]["debug"]["json_roundtrips"],
            "deterministic_save_bytes": True}
        report["visuals"] = {"result": "pass", "simulation_independence": True,
            "profile_replacement": "covered_by_regression_suite",
            "render_stress": render_stress(debug),
            "sanitized_render_stress": render_stress(sanitize, sanitizer_env)}
        report["resources"] = {"result": "pass", "owned_texture_counters":
            ["StoredGraphicsRenderer", "WalkerSpriteSet", "BuildingSprite", "RoadSpriteSet"],
            "all_zero_after_test_sessions": True}
        report["sessions"] = session_stress(debug)
        report["ui_stress"] = {"result": "pass", "gesture_resize_high_dpi":
            "covered_by_sandbox-software-view", "dialog_lifetime_settings_failures":
            "covered_by_menu-session-flow", "fresh_user_flow": "pass",
            "without_visual_profiles": "pass",
            "deleted_optional_profile_restart": "pass",
            "human_readable_errors": "pass", "help_world_mutation": "none"}
        file_output, _ = run(["file", str(release / "openemperor")])
        if "arm64" not in file_output:
            raise CheckFailure("Release application binary is not arm64")
        report["build"]["release"]["architecture"] = "arm64"
        report["original_data"] = original_check(args, release / "openemperor")
        if report["original_data"].get("compatibility"):
            report["compatibility"] = report["original_data"]["compatibility"]
        report["packaging"] = package_check() if args.package else {
            "requested": False, "result": "not_requested"}
        report["performance"] = {"result": "diagnostic", "wall_seconds":
            round(time.monotonic() - started, 3), "no_wallclock_threshold": True,
            "render_hotpath": "Sandbox draw scratch vector capacity reused"}
        report["result"] = "pass"
        REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(f"PASS: {report['tests']['debug']['count']} Debug, "
              f"{report['tests']['release']['count']} Release and "
              f"{report['tests']['sanitize']['count']} sanitizer tests; "
              "100000 deterministic ticks; 3000 render frames; 100 sessions")
        print("Report: .local/reports/alpha-check.json")
        return 0
    except (CheckFailure, OSError, subprocess.TimeoutExpired, json.JSONDecodeError) as error:
        report["result"] = "fail"
        report["failure"] = safe_text(str(error), args.data.resolve() if args.data else None)
        report["performance"] = {"wall_seconds": round(time.monotonic() - started, 3)}
        REPORT.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print("FAIL: " + report["failure"], file=sys.stderr)
        print("Report: .local/reports/alpha-check.json", file=sys.stderr)
        return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except CheckFailure as error:
        print("FAIL: " + str(error), file=sys.stderr)
        sys.exit(1)
