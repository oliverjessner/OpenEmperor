#!/usr/bin/env python3
"""Run each discovered standalone map through the native two-frame SDL check."""

import argparse
import json
import os
import pathlib
import selectors
import signal
import subprocess
import sys
import tempfile
import time


def invoke(argv, timeout):
    process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=(os.name == "posix"))
    output = bytearray()
    errors = bytearray()
    total = 0
    timed_out = False
    output_limited = False
    deadline = time.monotonic() + timeout
    streams = selectors.DefaultSelector()
    for pipe, name in ((process.stdout, "stdout"), (process.stderr, "stderr")):
        os.set_blocking(pipe.fileno(), False)
        streams.register(pipe, selectors.EVENT_READ, name)
    try:
        while streams.get_map():
            remaining = deadline - time.monotonic()
            if remaining <= 0 or total > 2 * 1024 * 1024:
                timed_out = remaining <= 0
                output_limited = not timed_out
                break
            for event, _ in streams.select(min(remaining, 0.25)):
                chunk = os.read(event.fileobj.fileno(), 65536)
                if not chunk:
                    streams.unregister(event.fileobj)
                    continue
                total += len(chunk)
                if event.data == "stdout":
                    output.extend(chunk)
                else:
                    errors.extend(chunk)
                    if len(errors) > 2048:
                        del errors[:-2048]
    except BaseException:
        streams.close()
        try:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
        except ProcessLookupError:
            pass
        process.wait()
        process.stdout.close()
        process.stderr.close()
        raise
    streams.close()
    if timed_out or output_limited:
        try:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
        except ProcessLookupError:
            pass
    process.wait()
    process.stdout.close()
    process.stderr.close()
    return {"exit_code": process.returncode, "stdout": output.decode("utf-8", "replace"),
            "stderr": errors.decode("utf-8", "replace"), "timed_out": timed_out,
            "output_limited": output_limited}


def classify_invocation(result):
    if result.get("timed_out"):
        return {"status": "timeout", "exit_code": result["exit_code"],
                "stderr_tail": result["stderr"]}
    if result.get("output_limited"):
        return {"status": "process_failed", "exit_code": result["exit_code"],
                "error": "subprocess output exceeded 2 MiB", "stderr_tail": result["stderr"]}
    try:
        parsed = json.loads(result["stdout"])
        if not isinstance(parsed, dict) or "status" not in parsed:
            raise ValueError("missing status")
    except (json.JSONDecodeError, ValueError):
        return {"status": "process_failed", "exit_code": result["exit_code"],
                "error": "missing or invalid JSON", "stderr_tail": result["stderr"]}
    parsed["exit_code"] = result["exit_code"]
    if result["exit_code"] != 0 and parsed["status"] in ("snapshot_complete", "snapshot_partial"):
        parsed["status"] = "process_failed"
    if result["stderr"] and parsed["status"] not in ("snapshot_complete", "snapshot_partial"):
        parsed["stderr_tail"] = result["stderr"]
    return parsed


def save_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                     prefix=path.name + ".", delete=False) as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
        output.write("\n")
        temporary = pathlib.Path(output.name)
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=pathlib.Path, default=pathlib.Path("build/openemperor"))
    parser.add_argument("--data", type=pathlib.Path, required=True)
    parser.add_argument("--report", type=pathlib.Path,
                        default=pathlib.Path(".local/reports/map-compatibility.json"))
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--build-type", default="Debug")
    parser.add_argument("--footprint-policy", choices=("disabled", "isolated", "edge-byte"),
                        default="edge-byte")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    binary = args.binary.resolve()
    data = args.data.resolve()
    listing = invoke([str(binary), "--data", str(data), "--list-maps", "--report-json"],
                     args.timeout)
    if listing.get("timed_out") or listing.get("output_limited"):
        raise SystemExit("map catalog failed: " + listing["stderr"][-500:])
    try:
        catalog = json.loads(listing["stdout"])
        entries = catalog["entries"]
        if not isinstance(entries, list):
            raise ValueError("catalog entries are not a list")
    except (ValueError, KeyError, TypeError) as error:
        raise SystemExit(f"invalid map catalog JSON: {error}") from error
    try:
        commit = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                                text=True, check=True, timeout=5).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        commit = "unknown"
    report = {"schema": "openemperor-map-corpus-v1", "starting_commit": commit,
              "binary": str(binary), "build_type": args.build_type,
              "graphics_profile": "exe-6373328b-v213-runtime-table",
              "footprint_policy": args.footprint_policy, "sdl_backend": "software/dummy",
              "render_width": 1280, "render_height": 720,
              "catalog_exit_code": listing["exit_code"],
              "discovered_files": len(entries), "checked_files": 0,
              "catalog_scan_errors": catalog.get("scan_errors", []),
              "results": [{"relative_path": entry["relative_path"], "status": "not_checked"}
                          for entry in entries],
              "summary": {"not_checked": len(entries)}, "problem_classes": {},
              "warning_classes": {}}
    save_report(args.report, report)
    try:
        for index, entry in enumerate(entries):
            relative = entry["relative_path"]
            command = [str(binary), "--data", str(data), "--map-debug", relative,
                       "--view", "stored-graphics", "--graphics-profile",
                       "exe-6373328b-v213-runtime-table", "--render-check", "--report-json"]
            if args.footprint_policy != "disabled":
                command += ["--multi-tile-preview", "--footprint-policy", args.footprint_policy]
            started = time.monotonic()
            try:
                result = classify_invocation(invoke(command, args.timeout))
            except OSError as error:
                result = {"status": "process_failed", "error": str(error)}
            for key in ("error", "stderr_tail"):
                if key in result:
                    result[key] = result[key].replace(str(data), "<data-root>")
            result["relative_path"] = relative
            result["elapsed_seconds"] = round(time.monotonic() - started, 3)
            report["results"][index] = result
            report["checked_files"] += 1
            summary = {}
            problems = {}
            warnings = {}
            for item in report["results"]:
                summary[item["status"]] = summary.get(item["status"], 0) + 1
                for reason, cells in item.get("status_counts", {}).items():
                    if reason == "rendered":
                        continue
                    bucket = problems.setdefault(reason, {"maps": 0, "cells": 0})
                    bucket["maps"] += 1
                    bucket["cells"] += cells
                for warning in ("mask_mismatches", "marker_deviations", "unknown_bit_cells"):
                    amount = item.get(warning, 0)
                    if amount:
                        bucket = warnings.setdefault(warning, {"maps": 0, "occurrences": 0})
                        bucket["maps"] += 1
                        bucket["occurrences"] += amount
            report["summary"] = summary
            report["problem_classes"] = problems
            report["warning_classes"] = warnings
            save_report(args.report, report)
            print(f"{report['checked_files']}/{len(entries)} {relative}: {result['status']}", flush=True)
    except KeyboardInterrupt:
        print("interrupted; partial report retained", file=sys.stderr)
        return 130
    print(json.dumps({"discovered": report["discovered_files"],
                      "checked": report["checked_files"], "summary": report["summary"],
                      "problem_classes": report["problem_classes"],
                      "warning_classes": report["warning_classes"]},
                     ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
