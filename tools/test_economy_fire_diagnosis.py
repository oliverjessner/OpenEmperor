#!/usr/bin/env python3
"""Paid Rule10 economy/fire observations and complete process-continuation proof.

PASS means reproducible technical diagnosis. Housing-goal evidence and operating
finance remain separate measurements; a losing legal city is a valid diagnosis.
Original inputs are read-only and no personal save locations are discovered.
"""
import argparse
from pathlib import Path
import json
import os
import subprocess
import sys
import time

# Reuse the existing private-file and original-model readers. These helpers
# grant no World, source-admission or save-restore authority.
from test_paid_progression import (launch_argv, MODEL_SHA, REPO, digest, inventory, load,
                                   overlaps, record, require, safe)
from test_original_longplay import prepared

PRIVATE = REPO / ".local/original-economy/oe35"
VARIANTS = ("supplied", "direct", "connected", "food", "lean6", "lean12",
            "supplied12", "managed", "staged", "staggered12", "compact6")
PAIRS = tuple(f"{point}.{kind}.json" for point in ("midpoint", "result")
              for kind in ("save", "snapshot", "report"))
FORMAT = "openemperor-economy-fire-observation-v1"


def integer(value, message, minimum=None):
    require(type(value) is int and (minimum is None or value >= minimum), message)
    return value


def preflight(binary, data, output):
    for path in (binary, data, output, PRIVATE):
        safe(path)
    require(binary.is_file() and os.access(binary, os.X_OK), "Build the economy/fire diagnosis binary first")
    require(data.is_dir(), "BLOCKED: mandatory original installation is missing")
    for relative in ("Model/EmperorBuildingModels.txt", "Cities/Xia.map"):
        require((data / relative).is_file(), "BLOCKED: mandatory original input is missing: " + relative)
    require(output != PRIVATE and PRIVATE in output.parents, "Output must be a new private OE35 directory")
    require(not any(overlaps(output, protected) for protected in (data, binary.parent)),
            "Output overlaps original data or build tree")
    require(not output.exists(), "Preserve prior evidence: output already exists")
    require(digest(data / "Model/EmperorBuildingModels.txt") == MODEL_SHA,
            "BLOCKED: diagnosis requires the pinned unchanged original model")


def summary_evidence(report):
    """Validate an observation, without implementing another housing goal."""
    require(report["format"] == FORMAT, "Unexpected economy/fire observation format")
    begin = integer(report["begin_tick"], "Invalid observation begin", 0)
    end = integer(report["end_tick"], "Invalid observation end", begin)
    require(begin == 816 and report["observed_ticks"] == end - begin,
            "Diagnosis did not observe the full seed-relative interval")
    summary = report["summary"]
    for key in ("months", "invoices", "goal_changes", "fire_events", "controls", "supply_by_house", "building_costs"):
        require(isinstance(report[key], list), "Missing actual diagnostic collection: " + key)
    require(isinstance(report["transport"], (dict, list)), "Missing actual transport observations")
    first = summary["first_goal_tick"]
    require(first is None or type(first) is int and begin <= first <= end,
            "Production first-goal witness is outside the observation")
    witness = summary.get("first_goal")
    require(witness is None if first is None else isinstance(witness, dict) and
            witness.get("reached") is True and witness.get("tick") == first,
            "Missing true tick-bound production first-goal descriptor")
    goal = summary["goal_at_end"]
    require(type(goal["reached"]) is bool and goal["tick"] == end,
            "Missing tick-bound productive World goal descriptor")
    require(not goal["reached"] or first is not None,
            "Reached World goal lacks its first observed tick")
    negative = summary["first_negative_tick"]
    require(negative is None or type(negative) is int and begin <= negative <= end,
            "First-negative-Funds witness is outside the observation")
    integer(summary["minimum_funds"], "Invalid minimum Funds")
    tax = integer(summary["taxes_delta"], "Invalid tax interval", 0)
    maintenance = integer(summary["maintenance_delta"], "Invalid upkeep interval", 0)
    require(type(summary["operating_balance"]) is int and summary["operating_balance"] == tax - maintenance,
            "Operating balance differs from actual tax/upkeep deltas")
    losses = integer(summary["goal_loss_ticks"], "Invalid goal-loss observation", 0)
    require(losses <= end - begin, "Goal-loss ticks exceed observed ticks")
    require((negative is None) == (summary["minimum_funds"] >= 0),
            "Negative Funds witness differs from observed minimum")
    for key, boundary in (("funds_at_seed_plus_20000", begin + 20000),
                          ("funds_at_goal_plus_20000", None if first is None else first + 20000)):
        value = summary[key]
        if boundary is not None and boundary <= end:
            integer(value, "Missing complete 20000-tick finance observation: " + key)
        else:
            require(value is None, "Finance observation claims an unobserved 20000-tick boundary: " + key)
    for key, complete in (("funds_20k_nonnegative", end >= begin + 20000),
                          ("goal_20k_financeable", first is not None and end >= first + 20000)):
        require(type(summary[key]) is bool if complete else summary[key] is None,
                "Finance sustainability label claims an unobserved interval: " + key)
    return summary


def review(folder, label, data):
    """Bind diagnostics to the raw complete save and independent source ledger."""
    for kind in ("save", "snapshot", "report"):
        safe(folder / f"{label}.{kind}.json")
    document = load(folder / f"{label}.save.json")
    world = document["world"]
    require(document["format"] == "openemperor-sandbox-save" and document["schema_version"] == 26 and
            document["rules"] == {"id": "sandbox-city-v16", "version": 10} and
            document["profiles"]["map_permissions"]["version"] == 3 and
            world["original_production"]["development_revision"] == 27,
            "Diagnosis changed rule/schema/policy/development identity")
    require(load(folder / f"{label}.snapshot.json") == world,
            "Raw snapshot does not equal the complete saved World")
    require(document["map"]["relative_path"] == "Cities/Xia.map" and
            document["map"]["sha256"] == digest(data / "Cities/Xia.map"),
            "Saved Xia identity differs from original input")
    source = world["original_economy"]["model_source"]
    require(source["difficulty"] == 2 and source["sha256"] == MODEL_SHA,
            "Saved Normal model identity differs")
    tick = integer(world["ticks"], "Invalid raw World clock", 816)
    funds = integer(world["treasury"], "Invalid raw Funds")
    tax = integer(world["taxes_collected_total"], "Invalid raw tax ledger", 0)
    spent = integer(world["construction_spent_total"], "Invalid paid construction", 1)
    upkeep = integer(world["maintenance_spent_total"], "Invalid raw upkeep ledger", 0)
    require(funds + spent + upkeep == 12000 + tax, "Raw paid funds conservation failed")
    buildings = world["buildings"]; couriers = world["couriers"]
    houses = world["original_economy"]["houses"]
    bodies = {row["id"]: row for row in buildings}
    homes = {row["building_id"]: row["house"] for row in houses}
    require(0 < len(bodies) == len(buildings) <= 82 and len(couriers) <= 58 and
            0 < len(homes) == len(houses) <= 20, "Paid entity identity or capacity differs")
    for rows, key in ((buildings, "id"), (couriers, "id"), (houses, "building_id")):
        ids = [integer(row[key], "Invalid monotone paid entity ID", 1) for row in rows]
        require(ids == sorted(set(ids)), "Paid entity IDs are duplicated or unsorted")
    require(all(row["owner"] in bodies for row in couriers) and all(identity in bodies for identity in homes),
            "Paid entity owner is missing")
    require(world["next_building_id"] > max(bodies) and
            world["next_courier_id"] > max((row["id"] for row in couriers), default=0),
            "Next paid entity IDs are not monotone")
    require(world["command_sequence"] >= len(buildings) and all(row["placed"] for row in buildings),
            "World lacks actual paid construction commands")
    population = sum(integer(home["occupants"], "Invalid House residents", 0) for home in homes.values())
    require(population == world["original_production"]["residents_admitted"] and population > 0,
            "Raw population gained a second authority")
    require(all(not row[field] for row in buildings for field in
                ("health_risk", "health_protection_until_tick", "sick_until_tick")),
            "Rule10 gained invented legacy illness authority")
    rows, _ = prepared(data / "Model/EmperorBuildingModels.txt")
    records = world["original_production"]["finance"]["houses"]
    require([row["building_id"] for row in records] == list(homes), "Tax owner bijection differs")
    actual_tax = 0
    for owner in records:
        require(len(owner["rows"]) == 15, "Tax bucket domain changed")
        paid = sum(integer(bucket["resident_units"], "Invalid taxed resident units", 0) * rows[i][18]
                   for i, bucket in enumerate(owner["rows"]))
        require(paid == bodies[owner["building_id"]]["taxes_paid_total"], "Raw model-weighted House tax differs")
        actual_tax += paid
    require(actual_tax == tax, "Raw total tax differs from independent Normal model")
    goods = world["original_goods"]
    fields = ("hemp", "ceramics", "tea", "silk", "bronzeware", "lacquerware")
    require(len(goods["totals"]) == len(fields), "Raw commodity domain changed")
    for g, total in enumerate(goods["totals"]):
        require(total["good"] == g and total["imported"] == total["demolished"] == 0,
                "Diagnosis gained unsupported commodity grants or sinks")
        stock = sum(home[fields[g]] for home in homes.values()) + sum(row["amounts"][g] for row in goods["stocks"])
        cargo = sum(row["amount"] for row in goods["cargo"] if row["good"] == g)
        require(total["produced"] == stock + cargo + total["consumed"], "Raw commodity conservation failed")
    report = load(folder / f"{label}.report.json")
    summary = summary_evidence(report)
    descriptor = summary["goal_at_end"]
    raw = {"tick": tick, "funds": funds, "population": population, "taxes": tax,
           "maintenance": upkeep, "construction": spent}
    require(all(descriptor[key] == value for key, value in raw.items()),
            "Productive goal descriptor differs from raw saved ledger")
    require(summary["minimum_funds"] <= funds, "Observed minimum exceeds actual final Funds")
    if funds < 0:
        require(summary["first_negative_tick"] is not None, "Negative raw Funds lack a recorded first tick")
    return {"save": record(folder / f"{label}.save.json"), "tick": tick, "funds": funds,
            "taxes": tax, "maintenance": upkeep, "summary": summary}


def workflow(binary, data, output, ticks=60000, variant="all", timeout=1800):
    require(variant == "all" or variant in VARIANTS, "Unknown economy/fire variant")
    require(type(ticks) is int and 20000 <= ticks <= 500000, "Ticks must be in20000..500000")
    require(type(timeout) is int and 1 <= timeout <= 7200, "Timeout must be in1..7200 seconds")
    preflight(binary, data, output)
    before = inventory(data); binary_before = record(binary)
    output.mkdir(parents=True)
    processes = []; results = []; failure = None
    report = {"format": "openemperor-economy-fire-workflow-v1", "status": "FAIL", "variant": variant,
              "ticks": ticks, "processes": processes, "variants": results, "binary": binary_before,
              "native_human": "NOT_RUN", "pass_means": "Technical reproducibility, not a profitable or permanently reached city"}

    def child(case, mode, target, input_path=None):
        argv = [str(binary), "--mode", mode, "--variant", case.name, "--data", str(data),
                "--output", str(target), "--ticks", str(ticks)]
        if input_path is not None:
            argv += ["--input", str(input_path)]
        log = case / f"{mode}.log"; started = time.monotonic(); timed_out = False
        with log.open("x") as stream:
            process = subprocess.Popen(launch_argv(argv), stdout=stream, stderr=subprocess.STDOUT)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                timed_out = True; process.kill(); code = process.wait()
        processes.append({"variant": case.name, "mode": mode, "argv": argv, "actual_pid": process.pid,
                          "exit_code": code, "timed_out": timed_out,
                          "seconds": time.monotonic() - started, "console": record(log)})
        (output / "processes.json").write_text(json.dumps(processes, indent=2) + "\n")
        require(not timed_out, "BLOCKED: mandatory diagnosis process exceeded timeout: " + mode)
        require(code == 0, f"Actual diagnosis child failed ({code}): {case.name}/{mode}")
        safe(target / "manifest.json")
        manifest = load(target / "manifest.json")
        require(manifest["status"] == "PASS" and manifest["mode"] == mode and manifest["variant"] == case.name,
                "Child did not publish its completed diagnosis mode/variant")
        require(manifest["creator_mode"] is (mode != "resume") and
                manifest["restored_live_world"] is (mode == "resume"),
                "Expected creator and restored continuation were not distinguished")
        require(manifest["begin_tick"] == (0 if mode == "seed" else 816) and
                manifest["end_tick"] == (816 if mode == "seed" else 816 + ticks) and
                manifest["ticks"] == (816 if mode == "seed" else ticks),
                "Diagnosis did not measure the complete seed-relative interval")
        require(manifest["snapshot_injection"] is False and manifest["rule"] == 10 and
                manifest["schema"] == 26 and manifest["policy"] == 3 and manifest["revision"] == 27 and
                manifest["original_map"] is True and manifest["original_model"] is True,
                "Unverified process authority contract")
        require(manifest["input_preserved"] is True, "Child did not confirm protected input preservation")
        commands = manifest.get("initial_commands", [])
        require(isinstance(commands, list) and (bool(commands) if mode != "resume" else not commands),
                "Creator paid commands and restored continuation were not distinguished")
        for command in commands:
            cost = integer(command["cost"], "Invalid actual command cost", 0)
            if "funds_before" in command and "funds_after" in command:
                require(command["funds_before"] >= cost and command["funds_after"] >= 0 and
                        command["funds_before"] - command["funds_after"] == cost,
                        "Actual paid command differs from available Funds")
        if mode == "run":
            require(manifest["protected_reference_compared"] is True,
                    "Expected run did not compare its unrestored earned World with the seed")
        return manifest

    try:
        for name in VARIANTS if variant == "all" else (variant,):
            case = output / name; case.mkdir()
            start = len(processes)
            seed = case / "seed"; seed_manifest = child(case, "seed", seed)
            seed_review = review(seed, "seed", data)
            require(seed_manifest["summary"] == seed_review["summary"], "Seed summary differs from its actual checkpoint")
            input_path = seed / "seed.save.json"; input_before = record(input_path)
            expected = case / "expected"; resumed = case / "resumed"
            run = child(case, "run", expected, input_path)
            creator_reference = expected / "creator-reference.save.json"
            safe(creator_reference)
            require(creator_reference.read_bytes() == input_path.read_bytes(),
                    "Fresh unrestored creator differs from complete protected seed bytes")
            creator_pair = {"input": input_before, "unrestored_creator": record(creator_reference)}
            restart = child(case, "resume", resumed, input_path)
            require(run["summary"] == restart["summary"], "Separate processes disagree on economy/fire observations")
            require(len({p["actual_pid"] for p in processes[start:]}) == 3,
                    "Seed, earned creator and restored continuation require three distinct actual processes")
            pairs = []
            for file_name in PAIRS:
                a = expected / file_name; b = resumed / file_name
                safe(a); safe(b)
                require(a.read_bytes() == b.read_bytes(), "Separate-process bytes differ: " + file_name)
                pairs.append({"expected": record(a), "resumed": record(b)})
            require(record(input_path) == input_before, "Separate processes modified their supplied paid input")
            reviewed = [seed_review]
            for folder in (expected, resumed):
                manifest = run if folder == expected else restart
                for label, end in (("midpoint", 816 + ticks // 2), ("result", 816 + ticks)):
                    checkpoint = review(folder, label, data)
                    require(checkpoint["tick"] == end, "Checkpoint substituted the actual observation clock")
                    require(checkpoint["summary"]["taxes_delta"] == checkpoint["taxes"] - seed_review["taxes"] and
                            checkpoint["summary"]["maintenance_delta"] == checkpoint["maintenance"] - seed_review["maintenance"],
                            "Operating intervals differ from the protected seed ledger")
                    if label == "result":
                        require(checkpoint["summary"] == manifest["summary"], "Final summary differs from its actual checkpoint")
                    reviewed.append(checkpoint)
            results.append({"variant": name, "technical_status": "PASS", "summary": run["summary"],
                            "actual_processes": 3, "literal_pairs": pairs, "creator_reference_pair": creator_pair,
                            "reviewed_checkpoints": reviewed, "input_preserved": True})
        report["status"] = "PASS"; report["actual_processes"] = len(processes)
    except Exception as error:
        failure = error
        report["status"] = "BLOCKED" if "BLOCKED:" in str(error) else "FAIL"
        report["error"] = str(error)
    finally:
        try:
            after = inventory(data); binary_after = record(binary)
            report.update(original_inventory_before=before, original_inventory_after=after,
                          original_bytes_modes_mtimes_unchanged=before == after,
                          binary_unchanged=binary_before == binary_after)
            require(before == after and binary_before == binary_after,
                    "Protected original installation or executable changed")
        except Exception as error:
            report["status"] = "FAIL"; report["error"] = str(error); failure = error
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if failure:
        raise failure
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--data", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--variant", choices=("all", *VARIANTS), default="all")
    parser.add_argument("--ticks", type=int, default=60000)
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args(argv)
    try:
        report = workflow(args.binary.absolute(), args.data.absolute(), args.output.absolute(),
                          args.ticks, args.variant, args.timeout)
        print(f"Economy/fire diagnosis: PASS technical, {len(report['variants'])} variants, "
              f"{report['actual_processes']} distinct child runs; native/human NOT_RUN")
        for case in report["variants"]:
            summary = case["summary"]
            print(f"{case['variant']}: first World goal {summary['first_goal_tick']}; "
                  f"first negative Funds {summary['first_negative_tick']}; "
                  f"operating balance {summary['operating_balance']}")
        return 0
    except Exception as error:
        blocked = "BLOCKED:" in str(error)
        print("Economy/fire diagnosis " + ("BLOCKED" if blocked else "FAIL") + ": " + str(error), file=sys.stderr)
        return 2 if blocked else 1


if __name__ == "__main__":
    raise SystemExit(main())
