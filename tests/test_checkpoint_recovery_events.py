#!/usr/bin/env python3
"""Opt-in actual event saves/Recovery and independent paid replay.

Requires explicit supplied data and either an already-earned MAC10 command plan
or its normal paid creator executable.
All attempts remain in fresh private directories; no native/human claim.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = "qa-event-checkpoint-recovery-v1"


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(path):
    return json.loads(path.read_text())


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for part in iter(lambda: source.read(1024 * 1024), b""):
            value.update(part)
    return value.hexdigest()


def fingerprint(path):
    info = path.stat()
    return digest(path), info.st_size, info.st_mode, info.st_mtime_ns


def invoke(executable, data, output, proof, label, *arguments, expected=0, diagnostic=None):
    command = [str(executable), "--data", str(data), "--output", str(output),
               *map(str, arguments)]
    (proof / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    environment = os.environ.copy()
    # The shared real-map fixture itself selects dummy/software; an environment
    # override would make its required normal-priority SDL hint fail.
    environment.pop("SDL_VIDEODRIVER", None)
    environment.pop("SDL_RENDER_DRIVER", None)
    try:
        result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True,
                                text=True, timeout=600, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode(errors="replace")
            (proof / (label + "." + name + ".log")).write_text(value or "")
        raise
    (proof / (label + ".stdout.log")).write_text(result.stdout)
    (proof / (label + ".stderr.log")).write_text(result.stderr)
    require(result.returncode == expected, label + " failed: " + result.stdout + result.stderr)
    report = read(output / "report.json")
    if expected != 0:
        require(report["status"] == "FAIL" and
                diagnostic and diagnostic in result.stderr and
                "PASS paid" not in result.stdout,
                "Altered receipt checkpoint was not refused for its required event proof")
        return report
    require(report["status"] == "PASS", label + " did not earn positive acceptance")
    return report


def audit_recovery(output, report, required):
    proof = report["checkpoint_recovery"]
    require(proof["contract"] == CONTRACT and proof["history_count"] == 1 and
            proof["checkpoint_limit"] == 32, "Unbounded or missing event Recovery contract")
    events = {row["label"]: row for row in proof["verified_events"]}
    require(len(events) == len(proof["verified_events"]) <= 32 and required <= events.keys(),
            "Missing/duplicate selected event Recovery checkpoints")
    checkpoints = {row["label"]: row for row in report["checkpoints"]}
    for label, event in events.items():
        checkpoint = checkpoints[label]
        path = output / checkpoint["file"]
        document = read(path)
        require(event["schema"] == document["schema_version"] == 52 and
                event["tick"] == checkpoint["tick"] == document["world"]["ticks"] and
                event["canonical_payload_exact"] and event["fresh_restore_exact"] and
                path.read_bytes() == (output / (label + ".roundtrip.json")).read_bytes(),
                "Selected event save/roundtrip/Recovery proof disagrees")
    # Ordinary rotation keeps only a bounded suffix and the initial payload.
    payloads = list((output / "checkpoint-recovery").rglob("checkpoint-*.json"))
    require(0 < len(payloads) <= 5, "Recovery checkpoint rotation did not retain its bounded suffix")
    for path in payloads:
        matches = [event for event in events.values() if event["filename"] == path.name]
        require(len(matches) == 1 and digest(path) == digest(output / checkpoints[matches[0]["label"]]["file"]),
                "Retained Recovery payload is not the literal selected event save")
    return {label: read(output / checkpoints[label]["file"])["world"] for label in required}


def audit_visual(output, report):
    labels = {"food-grower-work-earned", "food-grower-loaded", "crop-Tea-growing",
              "boat-loaded-moving", "hunter-loaded-moving", "mill-processing",
              "warehouse-stock", "warehouse-outbound", "market-stock-receipt",
              "market-house-receipt", "shop-house-receipt", "pump-fire",
              "pump-extinguished", "pump-recovered-wet", "year-boundary-9792"}
    states = audit_recovery(output, report, labels)
    work = states["food-grower-work-earned"]
    require(any(farm["latest_trip"] and farm["latest_trip"]["phase"] == 3 and
                farm["latest_trip"]["work_ticks"] > 0
                for farm in work["physical_food_growers"]["farms"]),
            "Grower phase entry was substituted for actual earned work")
    loaded = states["food-grower-loaded"]
    actors = {row["id"]: row for row in loaded["couriers"]}
    require(any(farm["latest_trip"] and farm["latest_trip"]["phase"] == 4 and
                farm["latest_trip"]["harvested_tick"] is not None and
                farm["latest_trip"]["returned_tick"] is None and
                actors[farm["courier_id"]]["edge_progress"] > 0
                for farm in loaded["physical_food_growers"]["farms"]),
            "Loaded Grower event lacks actual moving return cargo")
    for label, collection, key in (("boat-loaded-moving", "fishing", "boats"),
                                    ("hunter-loaded-moving", "hunting", "hunters")):
        require(any(row["cargo"] > 0 and row["edge_progress"] > 0
                    for row in states[label][collection][key]),
                "Loaded actor event lacks physical moving cargo")
    market = states["market-stock-receipt"]
    require(any(receipt and receipt["tick"] == market["ticks"] and receipt["amount"] > 0
                for row in market["food_warehouses"]["markets"] for receipt in row["first_received"]),
            "Market checkpoint is not its actual accepted receipt epoch")
    for label, owners in (("market-house-receipt", "markets"), ("shop-house-receipt", "shops")):
        world = states[label]
        source_ids = ({row["market_id"] for row in world["food_warehouses"][owners]}
                      if owners == "markets" else
                      {row["shop_id"] for row in world["food_processing"][owners]})
        couriers = {row["id"]: row for row in world["couriers"]}
        homes = {row["id"] for row in world["buildings"] if row["kind"] in (6, 39, 40)}
        require(any(stock["building_id"] in homes and stock["quality_witness"] and
                    stock["quality_witness"]["tick"] == world["ticks"] and
                    sum(stock["quality_witness"]["accepted"]) > 0 and
                    couriers[stock["quality_witness"]["transfer"]["courier_id"]]["owner"] in source_ids
                    for stock in world["processed_food"]["stocks"]),
                "House checkpoint lacks a real Market/Shop accepted-arrival witness")
    warehouse = states["warehouse-stock"]
    require(any(receipt and receipt["tick"] == warehouse["ticks"] and receipt["amount"] > 0
                for row in warehouse["food_warehouses"]["warehouses"] for receipt in row["first_received"]),
            "Warehouse stock is not bound to its actual receipt epoch")
    mill = states["mill-processing"]
    require(any(row["active"] and row["active"]["work"] > 0 for row in mill["food_processing"]["mills"]),
            "Mill checkpoint omitted actual committed recipe work")
    require(states["year-boundary-9792"]["ticks"] == 9792,
            "Seasonal year boundary was approximated by a nearby snapshot")
    return labels


def compare_reference(output, reference, filenames):
    for filename in filenames:
        require(digest(output / filename) == digest(reference / filename),
                "Opt-in QA changed existing authoritative bytes: " + filename)


def checkpoint_files(report):
    files = []
    for row in report["checkpoints"]:
        require(Path(row["file"]).name == row["file"] and
                Path(row["label"]).name == row["label"],
                "Checkpoint comparison requires leaf filenames")
        files.extend((row["file"], row["label"] + ".roundtrip.json"))
    return files


def compare_luxury_commands(creator, replay):
    original = read(creator / "commands.json")
    repeated = read(replay / "commands.json")
    seed = read(creator / "seed.save.json")["world"]
    sequence = seed["command_sequence"]
    accepted = [row for row in original if row["accepted"]]
    prefix = [row for row in accepted if row["sequence_after"] <= sequence]
    continuation = [row for row in accepted if row["sequence_before"] >= sequence]
    replay_accepted = [row for row in repeated if row["accepted"]]
    require(prefix and continuation and accepted == prefix + continuation and
            prefix[-1]["sequence_after"] == sequence and
            continuation[0]["sequence_before"] == sequence and
            replay_accepted == continuation,
            "Accepted luxury replay commands differ from the exact paid creator continuation")
    for rows in (accepted, replay_accepted):
        require(all(row["changed"] and row["sequence_after"] == row["sequence_before"] + 1
                    for row in rows), "Accepted luxury command lost its actual sequence transition")

    refused = [row for row in original if not row["accepted"]]
    replay_refused = [row for row in repeated if not row["accepted"]]
    for row in refused + replay_refused:
        # The existing ordinary() authority additionally asserts complete
        # World snapshot equality around every actual refused execute().
        require(row["accepted"] is False and row["changed"] is False and
                row["funds_before"] == row["funds_after"] and
                row["sequence_before"] == row["sequence_after"] and
                row.get("retry_after_real_ticks") is True and
                "overlaps actual wildlife, a Hunter or its begun edge" in row["result"],
                "Actual refused luxury purchase was not an atomic wildlife retry")

    def semantics(row):
        mutable_epoch = {"tick", "funds_before", "funds_after", "sequence_before", "sequence_after"}
        return json.dumps({key: value for key, value in row.items() if key not in mutable_epoch}, sort_keys=True)

    require(Counter(map(semantics, refused)) == Counter(map(semantics, replay_refused)),
            "Creator/replay omitted or invented a semantically distinct refused attempt")
    later = [row for row in refused if row["sequence_before"] >= sequence]
    replay_later = [row for row in replay_refused if row["sequence_before"] > sequence or row["tick"] > seed["ticks"]]
    require(later == replay_later, "Post-seed refused attempts differ in their actual execution epoch")
    earlier = [row for row in refused if row["sequence_before"] < sequence]
    replay_initial = [row for row in replay_refused if row not in replay_later]
    require(len(earlier) == len(replay_initial), "Seed replay changed the number of initial refused attempts")
    for row in earlier:
        matches = [candidate for candidate in replay_initial if semantics(candidate) == semantics(row)]
        require(len(matches) == 1, "Seed refusal was not actually retried exactly once")
        retry = matches[0]
        # The seed contains the unchanged t0 wildlife positions. This extra
        # diagnostic is re-earned before any simulation tick in the replay;
        # the creator logged it earlier while buying the paid seed prefix.
        occupied = any(animal["current"] == row["cell"] or
                       animal["edge_progress"] > 0 and animal["next"] == row["cell"]
                       for animal in seed["hunting"]["animals"])
        require(row["tick"] == retry["tick"] == seed["ticks"] == 0 and
                row["command"] == retry["command"] == 0 and occupied and
                retry["funds_before"] == seed["treasury"] and
                retry["sequence_before"] == sequence,
                "Repeated seed refusal lacks its actual current wildlife/budget/sequence precondition")
    return {"accepted_seed_prefix": len(prefix), "accepted_continuation": len(continuation),
            "atomic_refusals_per_process": len(refused), "initial_refusals_reearned": len(earlier),
            "accepted_continuation_exact": True, "refusal_semantics_exact": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("visual-executable", "luxury-executable", "data"):
        parser.add_argument("--" + name, type=Path, required=True)
    seeds = parser.add_mutually_exclusive_group(required=True)
    seeds.add_argument("--seed-plan", type=Path)
    seeds.add_argument("--seed-executable", type=Path)
    parser.add_argument("--visual-reference", type=Path)
    parser.add_argument("--luxury-reference", type=Path)
    args = parser.parse_args()
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file():
        print("SKIP: supplied Original Model data unavailable")
        return 77
    data = args.data.resolve(strict=True)
    inputs = [data / "Model/EmperorBuildingModels.txt", data / "Cities/Chengdu.map", data / "Cities/Xia.map"]
    before = {path: fingerprint(path) for path in inputs}
    private = ROOT / ".local/agri-mac14"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="checkpoint-events-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    visual_base = ROOT / ".local/agri-mac12"
    visual_base.mkdir(parents=True, exist_ok=True)
    creator = visual_base / (proof.name + "-creator")
    replay = visual_base / (proof.name + "-replay")
    default = visual_base / (proof.name + "-default-creator")
    try:
        visual = args.visual_executable.resolve(strict=True)
        luxury = args.luxury_executable.resolve(strict=True)
        if args.seed_plan:
            seed = args.seed_plan.resolve(strict=True)
        else:
            seed_base = ROOT / ".local/agri-mac10"
            seed_base.mkdir(parents=True, exist_ok=True)
            seed_output = seed_base / ("seed-for-events-" + proof.name)
            invoke(args.seed_executable.resolve(strict=True), data, seed_output, proof,
                   "mac10-paid-seed", "--ticks", 24000)
            seed = seed_output / "plan.json"
        inputs.append(seed)
        before[seed] = fingerprint(seed)
        require(read(seed)["format"] == "openemperor-food-warehouses-plan-v1",
                "Event seed must be an actual paid MAC10 command plan")
        baseline = invoke(visual, data, default, proof, "visual-default-creator",
                          "--seed-plan", seed, "--ticks", 24000)
        require("checkpoint_recovery" not in baseline and
                "checkpoint_recovery" not in read(default / "plan.json"),
                "Default visual creator unexpectedly selected event Recovery")
        a = invoke(visual, data, creator, proof, "visual-creator", "--seed-plan", seed,
                   "--ticks", 24000, "--checkpoint-recovery")
        labels = audit_visual(creator, a)
        compare_reference(creator, default, checkpoint_files(baseline) + ["work.jsonl"])
        original_checkpoints = {row["label"]: row for row in baseline["checkpoints"]}
        event_checkpoints = {row["label"]: row for row in a["checkpoints"]}
        require(all(event_checkpoints.get(label) == row for label, row in original_checkpoints.items()),
                "Opt-in changed a fresh Default creator's checkpoint epoch/identity/evidence")
        original_plan, event_plan = read(default / "plan.json"), read(creator / "plan.json")
        require(original_plan["scene"] == event_plan["scene"] and
                [row for row in original_plan["events"] if row["type"] == "command"] ==
                [row for row in event_plan["events"] if row["type"] == "command"],
                "Opt-in changed the actual Default paid command sequence")
        b = invoke(visual, data, replay, proof, "visual-replay", "--replay", creator,
                   "--ticks", 24000, "--checkpoint-recovery")
        audit_visual(replay, b)
        require(b["separate_process_replay_exact"], "Visual event proof omitted separate-process replay")
        compare_reference(replay, creator, checkpoint_files(a) + ["work.jsonl", "plan.json"])
        protected_replay = None
        if args.visual_reference:
            reference = args.visual_reference.resolve(strict=True)
            reference_report = read(reference / "report.json")
            reference_files = checkpoint_files(reference_report) + ["plan.json", "report.json", "work.jsonl"]
            reference_guard = {reference / name: fingerprint(reference / name) for name in reference_files}
            protected_replay = visual_base / (proof.name + "-protected-default-replay")
            historical = invoke(visual, data, protected_replay, proof, "visual-protected-default-replay",
                                "--replay", reference, "--ticks", reference_report["real_ticks"])
            require("checkpoint_recovery" not in historical and historical["separate_process_replay_exact"] and
                    historical["checkpoints"] == reference_report["checkpoints"],
                    "Protected Default replay changed its exact historical checkpoint epochs/evidence")
            compare_reference(protected_replay, reference, checkpoint_files(reference_report) + ["work.jsonl"])
            require(reference_guard == {path: fingerprint(path) for path in reference_guard},
                    "Protected Default replay changed supplied historical artifacts")
        # Preserve the earned final/trace and ordinary commands; change only
        # the recorded receipt checkpoint to the following tick. Coverage must
        # bind it back to the independently observed real arrival, not its label.
        for attack in ("shifted", "missing"):
            hostile = visual_base / (proof.name + "-" + attack + "-input")
            hostile.mkdir()
            plan = read(creator / "plan.json")
            receipt = next(row for row in plan["events"]
                           if row["type"] == "checkpoint" and row["label"] == "market-stock-receipt")
            if attack == "shifted":
                receipt["tick"] += 1
                plan["events"].sort(key=lambda row: row["tick"])
                reason = "Event Recovery checkpoint differs from independently earned epoch/identity"
            else:
                plan["events"].remove(receipt)
                reason = "Required event Recovery checkpoint was not published"
            (hostile / "plan.json").write_text(json.dumps(plan, indent=2) + "\n")
            for name in ("final.save.json", "work.jsonl"):
                shutil.copy2(creator / name, hostile / name)
            hostile_before = {path: fingerprint(path) for path in hostile.iterdir()}
            invoke(visual, data, visual_base / (proof.name + "-" + attack + "-output"), proof,
                   "visual-" + attack + "-receipt", "--replay", hostile, "--ticks", 24000,
                   "--checkpoint-recovery", expected=1, diagnostic=reason)
            require(hostile_before == {path: fingerprint(path) for path in hostile_before},
                    "Altered checkpoint rejection changed its explicit replay inputs")
        lux_creator, lux_replay = proof / "luxury-creator", proof / "luxury-replay"
        flags = ("--latest-agriculture", "--latest-plan-v2", "--checkpoint-recovery", "--ticks", 30000, "--hold", 20400)
        for label, output, extra in (("luxury-creator", lux_creator, ()),
                                     ("luxury-replay", lux_replay, ("--input", lux_creator / "seed.save.json", "--expected", lux_creator / "final.save.json"))):
            report = invoke(luxury, data, output, proof, label, *flags, *extra)
            receipts = {"house-tea-receipt": 2, "house-silk-receipt": 3, "house-lacquerware-receipt": 5}
            states = audit_recovery(output, report, set(receipts))
            for name, good in receipts.items():
                world = states[name]
                require(any(row["tick"] == world["ticks"] and row["good"] == good and
                            row["target_kind"] in (6, 39, 40) and row["arrived_units"] > 0
                            for row in report["actual_luxury_arrivals"]),
                        "Luxury checkpoint does not match the actual House arrival epoch")
            require(report["positive_luxury_consumption"] == [True, True, True] and
                    report["actual_post_maturity_ticks"] == 20400 and report["minimum_funds"] >= 0,
                    "Luxury event mode bypassed its earned consumption/retention/finance gate")
        compare_reference(lux_replay, lux_creator, checkpoint_files(read(lux_creator / "report.json")))
        commands = compare_luxury_commands(lux_creator, lux_replay)
        require(read(lux_replay / "report.json")["separate_process_replay_exact"], "Luxury event proof omitted separate replay")
        if args.luxury_reference:
            compare_reference(lux_creator, args.luxury_reference.resolve(strict=True),
                              ["seed.save.json", "mid-cargo.save.json", "maturity.save.json", "final.save.json", "commands.json"])
        require(before == {path: fingerprint(path) for path in inputs}, "Event proof changed explicit inputs")
        (proof / "result.json").write_text(json.dumps({"status": "PASS", "contract": CONTRACT,
            "visual_creator": str(creator), "visual_replay": str(replay),
            "visual_default_creator": str(default), "visual_default_canonical_checkpoints_exact": len(baseline["checkpoints"]),
            "protected_default_replay": str(protected_replay) if protected_replay else None,
            "visual_required_events": sorted(labels), "luxury_event_kinds": [2, 3, 5],
            "luxury_commands": commands,
            "independent_canonical_replays_exact": True, "source_inputs_preserved": True,
            "native_or_human_acceptance": "NOT_RUN"}, indent=2) + "\n")
        print("PASS: exact paid event epochs, bounded Recovery and independent replay")
        return 0
    except Exception as error:
        (proof / "failure.json").write_text(json.dumps({"status": "FAIL", "reason": str(error)}, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
