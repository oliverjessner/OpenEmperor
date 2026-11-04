#!/usr/bin/env python3
"""OE34 paid Xia progression, separate-process continuation and actual View captures.

Original files are read-only. Every output is new private evidence; this runner
never discovers personal saves or grants simulation resources.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[1]
PRIVATE = REPO / ".local/original-economy/oe34"
MODEL_SHA = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()



def launch_argv(argv):
    """Windows ignores shebangs; run a script "binary" (e.g. a test fake) with Python."""
    if os.name == "nt":
        with open(argv[0], "rb") as stream:
            if stream.read(2) == b"#!":
                return [sys.executable] + list(argv)
    return argv


def record(path):
    stat = path.stat()
    return {"path": str(path), "bytes": stat.st_size, "mode": stat.st_mode,
            "mtime_ns": stat.st_mtime_ns, "sha256": digest(path)}


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def load(path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def safe(path):
    require(path.is_absolute() and ".." not in path.parts, "Paths must be absolute and canonical")
    cursor = Path(path.anchor)
    for part in path.parts[1:]:
        cursor /= part
        require(not cursor.is_symlink(), "Path traverses a symlink: " + str(cursor))
    require(path.resolve() == path, "Path is not canonical: " + str(path))


def overlaps(a, b):
    return a == b or a in b.parents or b in a.parents


def inventory(root):
    rows = []
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), "Original installation contains a symlink: " + str(path))
        if path.is_file():
            row = record(path)
            row["path"] = path.relative_to(root).as_posix()
            rows.append(row)
        else:
            require(path.is_dir(), "Original installation contains a special file: " + str(path))
    encoded = json.dumps(rows, sort_keys=True, separators=(",", ":")).encode()
    return {"files": len(rows), "bytes": sum(row["bytes"] for row in rows),
            "manifest_sha256": hashlib.sha256(encoded).hexdigest(), "entries": rows}


def sources():
    paths = [REPO / "CMakeLists.txt"]
    for directory in ("src", "tests", "tools", "resources"):
        paths.extend(path for path in (REPO / directory).rglob("*")
                     if path.is_file() and "__pycache__" not in path.parts)
    return [record(path) for path in sorted(paths)]


def preflight(binary, data, output, resources):
    for path in (binary, data, output, resources, PRIVATE):
        safe(path)
    require(binary.is_file() and os.access(binary, os.X_OK), "Build the paid-progression acceptance binary first")
    require(data.is_dir(), "BLOCKED: original installation is missing")
    for relative in ("Model/EmperorBuildingModels.txt", "Cities/Xia.map"):
        require((data / relative).is_file(), "BLOCKED: mandatory original input is missing: " + relative)
    require(resources.is_dir(), "BLOCKED: rendering resource directory is missing")
    require(output != PRIVATE and PRIVATE in output.parents, "Output must be a new private OE34 directory")
    require(not any(overlaps(output, protected) for protected in (data, binary.parent, resources)),
            "Output overlaps original data, build tree or rendering resources")
    require(not output.exists(), "Preserve prior evidence: output already exists")
    require(digest(data / "Model/EmperorBuildingModels.txt") == MODEL_SHA,
            "BLOCKED: paid progression requires the pinned unchanged original model")


def review(path, data):
    document = load(path)
    world = document["world"]
    require(document["schema_version"] == 26 and document["rules"]["version"] == 10 and
            document["profiles"]["map_permissions"]["version"] == 3 and
            world["original_production"]["development_revision"] == 27,
            "Paid progression changed rule/schema/policy/development identity")
    require(document["map"]["relative_path"] == "Cities/Xia.map" and
            document["map"]["sha256"] == digest(data / "Cities/Xia.map"), "Saved Xia identity differs from original input")
    source = world["original_economy"]["model_source"]
    require(source["difficulty"] == 2 and source["sha256"] == MODEL_SHA, "Saved Normal model identity differs")
    require(world["treasury"] + world["construction_spent_total"] + world["maintenance_spent_total"] ==
            12000 + world["taxes_collected_total"], "Raw paid funds conservation failed")
    homes = world["original_economy"]["houses"]
    require(len({row["building_id"] for row in homes}) == len(homes), "Duplicate paid House identity")
    bodies = {row["id"]: row for row in world["buildings"]}
    require(len(bodies) == len(world["buildings"]) <= 82 and len(homes) <= 20,
            "Paid building/House identity or capacity differs")
    inhabited = [row["house"] for row in homes if row["house"]["occupants"] > 0 and
                 bodies[row["building_id"]]["fire_until_tick"] <= world["ticks"]]
    population = sum(row["occupants"] for row in inhabited)
    return {"save": record(path), "tick": world["ticks"], "funds": world["treasury"],
            "taxes": world["taxes_collected_total"], "population": population,
            "inhabited_houses": len(inhabited)}


def workflow(binary, data, output, resources, plan="supplied", ticks=20000, hold=20000, timeout=1800):
    require(plan in ("direct", "connected", "food", "supplied"), "Unknown paid expansion plan")
    require(1 <= ticks <= 100000 and 1 <= hold <= 100000, "Ticks and hold must be in1..100000")
    require(1 <= timeout <= 7200, "Timeout must be in1..7200 seconds")
    preflight(binary, data, output, resources)
    before = inventory(data)
    binary_before = record(binary)
    source_before = sources()
    output.mkdir(parents=True)
    processes = []
    report = {"format": "openemperor-paid-progression-workflow-v1", "status": "FAIL", "plan": plan,
              "first_goal_bound": ticks, "continuation_ticks": hold, "native_human": "NOT_RUN",
              "binary": binary_before, "sources": source_before, "processes": processes}
    failure = None

    def child(mode, target, input_path=None):
        argv = [str(binary), "--mode", mode, "--data", str(data), "--output", str(target),
                "--ticks", str(hold if mode == "resume" else ticks), "--hold", str(hold), "--plan", plan]
        if input_path is not None:
            argv += ["--input", str(input_path)]
        if mode == "render":
            argv += ["--resources", str(resources)]
        log = output / (mode + ".log")
        started = time.monotonic()
        timed_out = False
        with log.open("x") as stream:
            process = subprocess.Popen(launch_argv(argv), stdout=stream, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                timed_out = True
                process.kill()
                code = process.wait()
        processes.append({"argv": argv, "actual_pid": process.pid, "exit_code": code,
                          "timed_out": timed_out, "seconds": time.monotonic() - started, "console": record(log)})
        (output / "processes.json").write_text(json.dumps(processes, indent=2) + "\n")
        result = load(target / "report.json") if (target / "report.json").is_file() else None
        if result is not None:
            report[mode] = result
        require(not timed_out, "BLOCKED: paid progression child exceeded timeout: " + mode)
        require(code == 0, "Paid progression child failed (" + str(code) + "): " + mode)
        require(result is not None and result.get("status") in ("PASS", "PARTIAL"),
                "Child did not publish a completed measurement: " + mode)
        return result

    try:
        created = output / "create"
        creation = child("create", created)
        reviewed = [review(created / (name + ".save.json"), data)
                    for name in ("starter", "expanded", "goal", "result")]
        # Goal authority belongs solely to World::original_goal_status(). The
        # C++ creator records that result at its first reached checkpoint; raw
        # save population/occupancy above is diagnostic, not a second goal rule.
        first_goal = creation.get("first_goal")
        require(isinstance(first_goal, dict) and first_goal.get("reached") is True and
                creation.get("goal_status") == "PASS",
                "Creator did not confirm the production World housing goal")
        require(first_goal.get("tick") == creation.get("first_goal_tick") == reviewed[2]["tick"],
                "Production World housing goal differs from its saved checkpoint tick")
        starter = load(created / "starter.save.json")["world"]
        expanded = load(created / "expanded.save.json")["world"]
        require(starter["ticks"] == expanded["ticks"] == 0 and
                len(starter["original_economy"]["houses"]) == 4 and
                starter["construction_spent_total"] == 1404 and
                starter["taxes_collected_total"] == starter["maintenance_spent_total"] == 0 and
                all(row["house"]["occupants"] == 0 for row in starter["original_economy"]["houses"]),
                "Creator did not start with the genuine paid empty four-House starter")
        require(reviewed[2]["tick"] <= ticks and reviewed[3]["tick"] - reviewed[2]["tick"] == hold,
                "Creator substituted the first-goal bound or paid holding interval")
        goal = created / "goal.save.json"
        goal_before = record(goal)
        expected = record(created / "result.save.json")
        resumed = output / "resume"
        child("resume", resumed, goal)
        pairs = []
        for first, second in ((goal, resumed / "loaded.save.json"),
                              (created / "result.save.json", resumed / "result.save.json")):
            require(first.read_bytes() == second.read_bytes(), "Separate-process complete save bytes differ: " + second.name)
            pairs.append({"creator": record(first), "restored": record(second)})
        require(record(goal) == goal_before and record(created / "result.save.json") == expected,
                "Separate continuation modified protected creator inputs")
        rendered = output / "render"
        rendered_report = child("render", rendered, created / "result.save.json")
        require(rendered_report.get("input_preserved") is True, "Renderer did not confirm protected input preservation")
        require(record(created / "result.save.json") == expected, "Rendering modified the supplied paid save")
        captures = sorted(rendered.glob("*.png"))
        require(captures, "Actual Sandbox rendering produced no PNG captures")
        for path in captures:
            safe(path)
            header = path.read_bytes()[:24]
            require(len(header) == 24 and header[:8] == b"\x89PNG\r\n\x1a\n" and
                    header[12:16] == b"IHDR" and min(struct.unpack(">II", header[16:24])) > 0,
                    "Sandbox capture lacks a complete PNG dimension header: " + str(path))
        status = "PARTIAL" if any(report[mode]["status"] == "PARTIAL" for mode in ("create", "resume", "render")) else "PASS"
        report.update(status=status, measurement_completed=True, reviewed_checkpoints=reviewed, literal_pairs=pairs,
                      captures=[record(path) for path in captures], creator_inputs_preserved=True)
    except Exception as error:
        failure = error
        status = "BLOCKED" if "BLOCKED:" in str(error) else "FAIL"
        # A declared gameplay blocker may make the creator stop early. A later
        # proof failure must remain FAIL even when the measured city is PARTIAL.
        if (report.get("create", {}).get("status") == "PARTIAL" and len(processes) == 1 and
                processes[0]["exit_code"] != 0 and not processes[0]["timed_out"]):
            status = "PARTIAL"
        report.update(status=status, error=str(error))
    finally:
        try:
            after = inventory(data)
            report.update(original_inventory_before=before, original_inventory_after=after,
                          original_bytes_modes_mtimes_unchanged=before == after,
                          binary_unchanged=binary_before == record(binary), sources_unchanged=source_before == sources())
            require(report["original_bytes_modes_mtimes_unchanged"] and report["binary_unchanged"] and
                    report["sources_unchanged"], "Protected original installation, executable or source files changed")
        except Exception as error:
            failure = error
            report.update(status="FAIL", error=str(error), original_inventory_before=before)
        (output / "acceptance.json").write_text(json.dumps(report, indent=2) + "\n")
    if failure:
        raise failure
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("binary", "data", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--resources", type=Path, help="Defaults to the built executable resource directory")
    parser.add_argument("--plan", choices=("direct", "connected", "food", "supplied"), default="supplied")
    parser.add_argument("--ticks", type=int, default=20000)
    parser.add_argument("--hold", type=int, default=20000)
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args(argv)
    try:
        report = workflow(args.binary.absolute(), args.data.absolute(), args.output.absolute(), (args.resources or args.binary.parent / "resources").absolute(),
                          args.plan, args.ticks, args.hold, args.timeout)
        print("Paid progression: " + report["status"] + "; create/resume/render completed; native/human NOT_RUN")
        return 0
    except Exception as error:
        print("Paid progression " + ("BLOCKED" if "BLOCKED:" in str(error) else "FAIL") + ": " + str(error), file=sys.stderr)
        return 2 if "BLOCKED:" in str(error) else 1


if __name__ == "__main__":
    raise SystemExit(main())
