#!/usr/bin/env python3
"""Earned-state longplay and literal separate-process continuation proof.

This observer workflow is deliberately not a timing benchmark. All artifacts
are new private files; it never opens PrefPaths or discovers personal saves.
OE34 progression uses the production World goal witness and measures at least
20000 additional ticks; technical acceptance and operating stability are separate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[1]
PRIVATE = REPO / ".local/original-economy/oe32"
PROGRESSION_PRIVATE = REPO / ".local/original-economy/oe34"
MODEL_SHA = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"
PAIRS = tuple(f"{point}.{kind}.json" for point in ("midpoint", "result")
              for kind in ("save", "snapshot", "report"))
PROGRESSION_DIAGNOSTIC_PAIRS = tuple(f"{point}.{kind}.json" for point in ("midpoint", "result")
                                   for kind in ("evaluations", "evaluation-frames", "transport", "detailed"))


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
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest(path)}


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON key: " + key)
        result[key] = value
    return result


def load(path):
    return json.loads(path.read_text(), object_pairs_hook=unique)


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
            stat = path.stat()
            rows.append([str(path.relative_to(root)), stat.st_size, stat.st_mode, stat.st_mtime_ns, digest(path)])
    encoded = json.dumps(rows, separators=(",", ":")).encode()
    return {"files": len(rows), "bytes": sum(row[1] for row in rows),
            "manifest_sha256": hashlib.sha256(encoded).hexdigest()}


def preflight(binary, data, output, scenario):
    private = PROGRESSION_PRIVATE if scenario == "progression" else PRIVATE
    for path in (binary, data, output, private):
        safe(path)
    require(binary.is_file() and os.access(binary, os.X_OK), "Build the longplay acceptance binary first")
    require(data.is_dir(), "BLOCKED: mandatory original installation is missing")
    for relative in ("Model/EmperorBuildingModels.txt", "Cities/Xia.map"):
        require((data / relative).is_file(), "BLOCKED: mandatory original input is missing: " + relative)
    require(output != private and private in output.parents,
            "Output must be a new private " + ("OE34" if scenario == "progression" else "OE32") + " directory")
    require(not overlaps(output, data) and not overlaps(output, binary.parent), "Output overlaps original data or build tree")
    require(not output.exists(), "Preserve prior evidence: output already exists")
    if scenario in ("starter", "progression"):
        require(digest(data / "Model/EmperorBuildingModels.txt") == MODEL_SHA,
                "BLOCKED: paid Xia scenarios require the pinned unchanged original model")


def progression_evidence(value):
    """Check the C++ observer's production-goal witness, without a second goal formula."""
    progression = value["progression"]
    first = progression["first_goal_tick"]
    require(type(first) is int and 1 <= first <= 20000, "Productive first-goal witness exceeds its bound")
    require(progression["goal_reached"] is True and progression["goal_status"] == "PASS",
            "Productive World goal was not reached")
    require(progression["first_goal"]["reached"] is True and progression["first_goal"]["tick"] == first,
            "Missing first productive World goal descriptor")
    require(progression["stability_status"] in ("PASS", "PARTIAL"), "Missing separate operating stability assessment")
    require(type(progression["goal_loss_ticks"]) is int and progression["goal_loss_ticks"] >= 0,
            "Invalid observed goal-loss count")
    tax = progression["operating_tax_delta"]
    maintenance = progression["operating_maintenance_delta"]
    require(type(tax) is int and tax >= 0 and type(maintenance) is int and maintenance >= 0 and
            progression["operating_balance"] == tax - maintenance,
            "Operating tax/upkeep interval accounting differs")
    if progression["goal_loss_ticks"] or progression["operating_balance"] < 0:
        require(progression["stability_status"] == "PARTIAL", "Goal loss or operating deficit was mislabeled stable")
    return progression


def prepared(path):
    sections = {name: [] for name in ("ALL HOUSES", "HOUSE MODS", "ALL BUILDINGS", "BUILDING MODS")}
    section = None
    for line in path.read_text(encoding="ascii").splitlines():
        line = line.strip()
        if line in sections:
            section = line
        elif section and "{," in line:
            prefix, body = line.split("{,", 1)
            count = 24 if section in ("ALL HOUSES", "HOUSE MODS") else 13
            parts = body.split(",")[:count]
            require(len(parts) == count and all(re.fullmatch(r"[+-]?\d+", p.strip()) for p in parts), "Malformed model row")
            sections[section].append((prefix, [int(p) for p in parts]))
    require(len(sections["ALL HOUSES"]) == 15 and len(sections["HOUSE MODS"]) == len(sections["BUILDING MODS"]) == 5,
            "Incomplete independent Normal model source")
    house_mods = sections["HOUSE MODS"][2][1]
    rows = [[max(-99, min(100, value + house_mods[i])) for i, value in enumerate(row)]
            for _, row in sections["ALL HOUSES"]]
    mods = sections["BUILDING MODS"][2][1]
    buildings = {}
    for prefix, row in sections["ALL BUILDINGS"]:
        identity = int(prefix.split(",", 1)[0])
        require(identity not in buildings, "Duplicate independent model identity")
        buildings[identity] = [(abs(value * mods[i]) // 100) * (-1 if value * mods[i] < 0 else 1)
                               for i, value in enumerate(row)]
    return rows, buildings


def review(folder, label, model, scenario, data):
    document = load(folder / f"{label}.save.json")
    w = document["world"]; p = w["original_production"]
    require(document["schema_version"] == 26 and document["rules"]["version"] == 10 and p["development_revision"] == 27,
            "Longplay authority identity changed")
    require(load(folder / f"{label}.snapshot.json") == w, "Raw snapshot does not equal the complete saved World")
    source = w["original_economy"]["model_source"]
    require(source["difficulty"] == 2 and source["sha256"] == digest(model), "Raw source differs from independent Normal model")
    if scenario in ("starter", "progression"):
        require(document["map"]["relative_path"] == "Cities/Xia.map" and document["map"]["sha256"] == digest(data / "Cities/Xia.map"),
                "Actual Xia provenance changed")
    report = load(folder / f"{label}.report.json"); final = report["final"]
    require(report["status"] == "PASS" and report["end_tick"] == final["tick"] == w["ticks"], "Observed longplay clock differs")
    require(report["observed_ticks"] == report["end_tick"] - report["begin_tick"], "Incomplete per-tick observation")
    bodies = {b["id"]: b for b in w["buildings"]}
    homes = {h["building_id"]: h["house"] for h in w["original_economy"]["houses"]}
    require(len(bodies) == len(w["buildings"]) <= 82 and len(w["couriers"]) <= 58 and len(homes) <= 20, "Collection budget exceeded")
    workloads = {"starter": (19, 15, 4), "districts": (82, 58, 20), "progression": (25, 17, 6)}
    require((len(bodies), len(w["couriers"]), len(homes)) == workloads[scenario],
            "Actual paid scenario body/courier/House workload differs")
    require(w["construction_spent_total"] > 0 and sum(h["occupants"] for h in homes.values()) > 0,
            "Longplay input lacks actual paid construction and earned residents")
    if scenario == "starter":
        require(w["construction_spent_total"] == 1404, "Paid unchanged Normal starter cost differs")
    elif scenario == "progression":
        require(w["construction_spent_total"] == 1670, "Paid Normal progression construction cost differs")
        evidence = progression_evidence(report)
        require(evidence["first_goal_tick"] <= w["ticks"], "Observed goal is later than the actual saved clock")
        if label == "seed":
            require(all(evidence["first_goal"][field] == final[field]
                        for field in ("tick", "funds", "population", "taxes", "construction", "maintenance")),
                    "First productive goal descriptor differs from the actual seed ledger")
        require(evidence["operating_tax_delta"] == w["taxes_collected_total"] - evidence["first_goal"]["taxes"] and
                evidence["operating_maintenance_delta"] == w["maintenance_spent_total"] - evidence["first_goal"]["maintenance"],
                "Post-goal operating deltas differ from the actual raw tax/upkeep ledger")
        require(all(evidence["final_goal"][field] == final[field]
                    for field in ("tick", "funds", "population", "taxes", "construction", "maintenance")),
                "Current productive goal descriptor differs from the actual checkpoint ledger")
    require(all(not b[field] for b in bodies.values() for field in ("health_risk", "health_protection_until_tick", "sick_until_tick")),
            "Original acquired invented Legacy illness authority")
    require(report["original_health_neutral_checks"] == report["observed_ticks"] * len(bodies), "Incomplete actual neutral-health checks")
    require(final["funds"] == w["treasury"] and final["taxes"] == w["taxes_collected_total"] and
            final["construction"] == w["construction_spent_total"] and final["maintenance"] == w["maintenance_spent_total"], "Observed money differs from raw ledger")
    require(w["treasury"] + w["construction_spent_total"] + w["maintenance_spent_total"] == 12000 + w["taxes_collected_total"],
            "Raw signed funds conservation failed")
    rows, models = prepared(model)
    model_ids = {8: 208, 12: 207, 14: 192, 16: 193, 19: 214, 20: 72, 21: 215, 22: 217,
                 23: 219, 24: 211, 25: 212, 26: 213, 27: 38, 28: 39, 29: 42, 30: 44,
                 31: 45, 32: 47, 33: 237, 34: 238, 35: 239}
    fixed = {3: 2, 4: 4, 5: 6, 9: 4, 10: 2}
    workers = {identity: models[model_ids[b["kind"]]][5] if b["kind"] in model_ids else fixed.get(b["kind"], 0)
               for identity, b in bodies.items()}
    require(w["maintenance_spent_total"] == sum(2 * workers[i] * ((w["ticks"] - b["placed_tick"]) // 400)
            for i, b in bodies.items()), "Raw upkeep differs from independent source workers and actual paid ages")
    tax = 0
    require(len(p["finance"]["houses"]) == len(homes), "Finance owner bijection changed")
    for owner in p["finance"]["houses"]:
        require(len(owner["rows"]) == 15, "Tax bucket domain changed")
        paid = sum(row["resident_units"] * rows[i][18] for i, row in enumerate(owner["rows"]))
        require(paid == bodies[owner["building_id"]]["taxes_paid_total"], "Raw model-weighted tax differs")
        tax += paid
    require(tax == w["taxes_collected_total"], "Raw total house taxes differ")
    population = sum(h["occupants"] for h in homes.values())
    require(population == final["population"] == final["workforce"] == p["residents_admitted"], "Population gained a second authority")
    available = population; assigned = {}
    for identity, b in sorted(bodies.items(), key=lambda item: (item[1]["workforce_priority"], item[0])):
        need = workers[identity]
        take = need if b["operating_enabled"] and b["fire_until_tick"] <= w["ticks"] and need <= available else 0
        assigned[identity] = take; available -= take
    observed = {b["id"]: b for b in final["buildings"]}
    require(set(observed) == set(bodies) and all(observed[i]["workers"] == workers[i] and observed[i]["assigned"] == assigned[i] for i in bodies),
            "Raw priority/ID/all-or-nothing workforce differs")
    goods = w["original_goods"]
    require(len(goods["stocks"]) <= 46 and len(goods["cargo"]) <= 26 and len(goods["reservations"]) <= 26, "Typed goods budget exceeded")
    stock_fields = ("hemp", "ceramics", "tea", "silk", "bronzeware", "lacquerware")
    for g, total in enumerate(goods["totals"]):
        require(total["good"] == g, "Raw goods identity order changed")
        stock = sum(h[stock_fields[g]] for h in homes.values()) + sum(row["amounts"][g] for row in goods["stocks"])
        cargo = sum(row["amount"] for row in goods["cargo"] if row["good"] == g)
        require(total["produced"] + total["imported"] == stock + cargo + total["consumed"] + total["demolished"], "Raw six-goods mass differs")
        observed_good = final["goods"][g]
        require(observed_good["stock"] == stock and observed_good["cargo"] == cargo and observed_good["consumed"] == total["consumed"] and observed_good["conserved"] is True,
                "Reported goods differ from sole raw physical stocks")
    food = p["food"]; require(len(food["farms"]) <= 2 and len(food["stocks"]) <= 26 and len(food["trips"]) <= 6, "Food budget exceeded")
    farms = {row["farm_id"]: row for row in food["farms"]}
    for crop in range(2):
        harvested = sum(row["harvested"] for row in farms.values() if row["crop"] == crop)
        stock = sum(row["amounts"][crop] for row in food["stocks"])
        consumed = sum(row["consumed"][crop] for row in food["stocks"])
        cargo = sum(row["loaded"][crop] - row["delivered"][crop] - row["returned"][crop] for row in food["trips"])
        require(harvested == stock + consumed + cargo, "Raw crop harvest/stock/cargo/sink differs")
        for row in food["stocks"]:
            own = farms.get(row["building_id"])
            produced = own["harvested"] if own and own["crop"] == crop else 0
            require(produced + row["received"][crop] + row["returned"][crop] == row["amounts"][crop] + row["departed"][crop] + row["consumed"][crop],
                    "Raw per-owner food conservation differs")
    industry = p["industry"]; outputs = {27: 0, 28: 1, 31: 4, 34: 3, 35: 2}
    require(len(industry["sources"]) <= 18 and len(industry["auxiliary"]) <= 20 and len(industry["aux_trips"]) <= 12, "Auxiliary budget exceeded")
    for material in range(5):
        produced = sum(row["completed"] for row in industry["sources"] if outputs.get(bodies[row["building_id"]]["kind"]) == material)
        stock = sum(row["amounts"][material] for row in industry["auxiliary"])
        cargo = sum(row["loaded"] - row["delivered"] - row["returned"] for row in industry["aux_trips"] if row["material"] == material)
        consumed = sum(row["consumed"][material] for row in industry["auxiliary"])
        require(produced == stock + cargo + consumed, "Raw auxiliary material conservation differs")
        item = report["materials"][material]
        require(item["material"] == material and item["conserved"] is True and item["produced"] == produced and item["stock"] == stock and item["cargo"] == cargo and item["consumed"] == consumed,
                "Auxiliary observer differs from raw physical ledger")
    for house in report["houses"]:
        require(len(house["unmet_current_by_requirement"]) == len(house["unmet_next_by_requirement"]) == 15, "Requirement domain changed")
        require(house["evaluations_with_unmet_current_needs"] <= house["evaluations"] and house["evaluations_with_unmet_next_row_needs"] <= house["evaluations"], "Invented requirement events")
        require(all(want == used + missing for want, used, missing in zip(house["goods_requested_units"], house["goods_consumed_units"], house["goods_shortfall_units"])), "Scheduled consumption accounting differs")
    for field in ("evaluations", "upgrades", "devolutions", "evaluations_with_unmet_current_needs", "evaluations_with_unmet_next_row_needs", "actual_food_missed_demands"):
        require(report[field] == sum(h[field] for h in report["houses"]), "House metric aggregate differs: " + field)
    result = {"save": record(folder / f"{label}.save.json"), "report": record(folder / f"{label}.report.json"), "tick": w["ticks"],
            "funds": w["treasury"], "population": population, "upgrades": report["upgrades"], "devolutions": report["devolutions"],
            "evaluations_with_unmet_current_needs": report["evaluations_with_unmet_current_needs"], "actual_food_missed_demands": report["actual_food_missed_demands"],
            "natural_fire_ignitions": report["natural_fire_ignitions"], "goods_delivered_units": report["goods_delivered_units"]}
    if scenario == "progression":
        result["progression"] = evidence
    return result


def workflow(binary, data, output, ticks, scenario, timeout):
    require(scenario != "progression" or 20000 <= ticks <= 100000,
            "Progression requires at least 20000 additional ticks after the productive World goal")
    preflight(binary, data, output, scenario)
    before = inventory(data); binary_before = record(binary); output.mkdir(parents=True)
    processes = []; failure = None; report = None
    def child(tag, mode, target, input_path=None, count=ticks):
        argv = [str(binary), "--mode", mode, "--scenario", scenario, "--data", str(data), "--output", str(target), "--ticks", str(count)]
        if input_path is not None:
            argv += ["--input", str(input_path)]
        log = output / f"{tag}.log"; started = time.monotonic(); code = None
        with log.open("x") as stream:
            process = subprocess.Popen(launch_argv(argv), stdout=stream, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
                failure_text = f"BLOCKED: mandatory longplay process exceeded {timeout}s: {tag}"
            else:
                failure_text = f"Actual longplay child failed ({code}): {tag}"
        processes.append({"argv": argv, "actual_pid": process.pid, "exit_code": code, "seconds": time.monotonic() - started, "console": record(log)})
        (output / "processes.json").write_text(json.dumps(processes, indent=2) + "\n")
        require(code == 0, failure_text)
        manifest = load(target / "manifest.json")
        require(manifest["status"] == "PASS" and manifest["scenario"] == scenario, "Child did not publish a completed scenario")
        require(manifest["creator_mode"] is (mode != "resume") and manifest["restored_live_world"] is (mode == "resume"),
                "Expected creator and actual restored continuation were not distinguished")
        first = progression_evidence(manifest)["first_goal_tick"] if scenario == "progression" else 2000
        require(manifest["warmup_ticks"] == (0 if mode == "resume" else first), "Paid creator warmup differs from saved seed")
        if scenario == "progression":
            require(manifest["begin_tick"] == (0 if mode == "seed" else first) and
                    manifest["end_tick"] == (first if mode == "seed" else first + count) and
                    manifest["ticks"] == (first if mode == "seed" else count),
                    "Progression did not measure the complete interval after the first productive World goal")
        if mode == "run":
            require(manifest["protected_reference_compared"] is True, "Expected run did not compare its unrestored earned World with the protected seed")
        return manifest
    try:
        seed = output / "seed"; seed_manifest = child("seed", "seed", seed)
        input_path = seed / "seed.save.json"; input_before = record(input_path)
        expected = output / "expected"; resumed = output / "resumed"
        run = child("run", "run", expected, input_path)
        creator_reference = expected / "creator-reference.save.json"
        require(creator_reference.read_bytes() == input_path.read_bytes(), "Fresh unrestored creator differs from complete protected seed bytes")
        creator_pair = {"input": input_before, "unrestored_creator": record(creator_reference)}
        restart = child("resume", "resume", resumed, input_path)
        if scenario == "progression":
            require(seed_manifest["progression"]["first_goal_tick"] == run["progression"]["first_goal_tick"] ==
                    restart["progression"]["first_goal_tick"], "Separate processes disagree on the first productive World goal")
            require(seed_manifest["progression"]["first_goal"] == run["progression"]["first_goal"] ==
                    restart["progression"]["first_goal"], "Separate processes disagree on the first productive World goal descriptor")
            require(run["progression"] == restart["progression"], "Separate processes disagree on observed progression/stability")
        require(run["ticks"] == restart["ticks"] == ticks and run["begin_tick"] == restart["begin_tick"] and run["end_tick"] == restart["end_tick"], "Continuation elapsed clocks differ")
        require(len({p["actual_pid"] for p in processes}) == 3, "Separate continuation processes did not have distinct actual PIDs")
        pairs = []
        for name in PAIRS:
            a = expected / name; b = resumed / name
            require(a.read_bytes() == b.read_bytes(), "Separate-process bytes differ: " + name)
            pairs.append({"expected": record(a), "resumed": record(b)})
        diagnostics = []
        if scenario == "progression":
            for name in PROGRESSION_DIAGNOSTIC_PAIRS:
                a = expected / name; b = resumed / name
                require(a.is_file() and b.is_file(), "Missing mandatory progression diagnostic: " + name)
                require(a.read_bytes() == b.read_bytes(), "Separate-process diagnostic bytes differ: " + name)
                diagnostics.append({"expected": record(a), "resumed": record(b)})
        require(record(input_path) == input_before, "Separate processes modified their supplied paid input")
        reviewed = []
        for folder, labels in ((seed, ("seed",)), (expected, ("midpoint", "result")), (resumed, ("midpoint", "result"))):
            manifest = load(folder / "manifest.json")
            require(manifest["snapshot_injection"] is False and manifest["rule"] == 10 and manifest["schema"] == 26 and
                    manifest["policy"] == 3 and manifest["revision"] == 27, "Unverified process authority contract")
            if scenario in ("starter", "progression"):
                require(manifest["original_map"] is True and manifest["original_model"] is True, "Starter is not the actual original Xia/model")
            model = data / "Model/EmperorBuildingModels.txt" if manifest["original_model"] else folder / "authored-model.txt"
            if manifest["original_model"]:
                require(digest(model) == MODEL_SHA, "Claimed original model is not the pinned unchanged source")
            for label in labels:
                checkpoint = review(folder, label, model, scenario, data)
                if scenario == "progression":
                    require(checkpoint["progression"]["first_goal"] == manifest["progression"]["first_goal"],
                            "Checkpoint disagrees on the first productive World goal descriptor")
                    if label in ("seed", "result"):
                        require(checkpoint["progression"] == manifest["progression"],
                                "Completed checkpoint progression differs from its process manifest")
                reviewed.append(checkpoint)
        report = {"format": "openemperor-original-longplay-workflow-v1", "status": "PASS", "scenario": scenario, "ticks": ticks,
                  "actual_processes": len(processes), "processes": processes, "literal_pairs": pairs, "reviewed_checkpoints": reviewed,
                  "creator_reference_pair": creator_pair, "expected_live_world_restored": False, "resume_live_world_restored": True,
                  "input": input_before, "input_preserved": True, "binary": binary_before, "runner": record(Path(__file__).resolve()),
                  "observations_are_performance_timings": False, "native_human": "NOT_RUN", "original_visual_parity": "PARTIAL"}
        if scenario == "progression":
            report["diagnostic_pairs"] = diagnostics
            report["progression"] = run["progression"]
            report["goal_status"] = run["progression"]["goal_status"]
            report["stability_status"] = run["progression"]["stability_status"]
    except Exception as error:
        failure = error
        report = {"status": "BLOCKED" if "BLOCKED:" in str(error) else "FAIL", "scenario": scenario, "ticks": ticks,
                  "error": str(error), "processes": processes, "binary": binary_before, "runner": record(Path(__file__).resolve())}
    finally:
        after = inventory(data)
        unchanged = before == after and binary_before == record(binary)
        report["original_inventory_before"] = before; report["original_inventory_after"] = after
        report["original_bytes_modes_mtimes_unchanged"] = before == after; report["binary_unchanged"] = binary_before == record(binary)
        if not unchanged:
            report["status"] = "FAIL"; report["error"] = "Protected original installation or executable changed"
            failure = RuntimeError(report["error"])
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if failure:
        raise failure
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--scenario", choices=("starter", "districts", "progression"), default="starter")
    parser.add_argument("--ticks", type=int, default=20000)
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args(argv)
    try:
        require(1 <= args.ticks <= 100000, "Ticks must be in1..100000; mandatory100k cannot be silently substituted")
        require(1 <= args.timeout <= 7200, "Timeout must be in1..7200 seconds")
        report = workflow(args.binary.absolute(), args.data.absolute(), args.output.absolute(), args.ticks, args.scenario, args.timeout)
        print(f"Original longplay {args.scenario}: PASS, {args.ticks}ticks, {len(report['literal_pairs'])} literal process pairs; native/human NOT_RUN")
        if args.scenario == "progression":
            print(f"Productive World goal: {report['goal_status']} at tick {report['progression']['first_goal_tick']}; "
                  f"operating stability: {report['stability_status']}")
        return 0
    except Exception as error:
        print("Original longplay " + ("BLOCKED" if "BLOCKED:" in str(error) else "FAIL") + ": " + str(error), file=sys.stderr)
        return 2 if "BLOCKED:" in str(error) else 1


if __name__ == "__main__":
    raise SystemExit(main())
