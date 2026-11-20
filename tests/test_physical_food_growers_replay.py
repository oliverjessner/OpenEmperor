#!/usr/bin/env python3
"""Earn five physical crop chains from supplied sources and replay independently."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MODEL_SHA = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"
LABELS = {"seed", "grower-outward", "grower-waiting", "grower-working",
          "grower-loaded", "food-cart", "final"}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def fingerprint(path):
    info = path.stat()
    return info.st_size, info.st_mode, info.st_mtime_ns, hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_text())


def check_snapshot(document, farm):
    require(document["schema_version"] == 47 and document["rules"]["version"] == 31,
            "Checkpoint changed its exact Rule31/schema47 generation")
    require(document["rules"]["physical_food_growers"] == "authored-physical-food-growers-v1",
            "Checkpoint lacks its explicit physical production contract")
    world = document["world"]
    require("agriculture" not in world and "fishing_food" not in world,
            "Physical Grower checkpoint duplicated the sole seven-kind Food ledger")
    physical = world["physical_food_growers"]
    require(len(physical["farms"]) == 1 and physical["farms"][0]["farm_id"] == farm,
            "Acceptance crop origin is not the single studied paid Farm")
    crops = {field["field_id"]: field for field in world["hunting_food"]["fields"]}
    require(len(crops) == 5 and {field["crop"] for field in crops.values()} == set(range(5)) and
            all(field["farm_id"] == farm for field in crops.values()),
            "Five physical crop identities have a missing or ambiguous contributor")
    returned = [0] * 7
    picked = [0] * 7
    worker_cargo = [0] * 7
    held = [0] * 7
    cart = [0] * 7
    used = [0] * 7
    departed = [0] * 7
    arrived = [0] * 7
    cart_returned = [0] * 7
    for field in crops.values():
        picked[field["crop"]] += field["harvested"]
    for field in physical["fields"]:
        require(field["field_id"] in crops and field["farm_id"] == farm, "Foreign crop receipt owner")
        returned[crops[field["field_id"]]["crop"]] += field["returned"]
    source = physical["farms"][0]
    worker = source["latest_trip"]
    if worker and worker["harvested_tick"] is not None and worker["returned_tick"] is None:
        worker_cargo[worker["crop"]] += 1
    actors = {actor["id"]: actor for actor in world["couriers"]}
    actor = actors[source["courier_id"]]
    require(actor["role"] == 30 and actor["owner"] == farm and actor["cargo"] == actor["reserved"] == 0,
            "Physical Grower changed owner/channel or duplicated typed cargo into legacy mirrors")
    for stock in world["hunting_food"]["stocks"]:
        require(len(stock["amounts"]) == 7 and all(value >= 0 for value in stock["amounts"]),
                "Invalid sole seven-kind Food stock")
        for crop in range(7):
            held[crop] += stock["amounts"][crop]
            used[crop] += stock["consumed"][crop]
            departed[crop] += stock["departed"][crop]
            arrived[crop] += stock["received"][crop]
            cart_returned[crop] += stock["returned"][crop]
    for trip in world["hunting_food"]["trips"]:
        for crop in range(7):
            remaining = trip["loaded"][crop] - trip["delivered"][crop] - trip["returned"][crop]
            require(0 <= remaining <= 4, "Physical Cart cargo outside its real capacity")
            cart[crop] += remaining
    origin = returned[:]
    origin[5] += sum(quay["unloaded"] for quay in world["fishing"]["quays"])
    origin[6] += sum(tent["unloaded"] for tent in world["hunting"]["tents"])
    for crop in range(7):
        require(picked[crop] == returned[crop] + worker_cargo[crop],
                "Earned field harvest was neither carried by its Grower nor actually returned")
        require(origin[crop] == held[crop] + cart[crop] + used[crop],
                "Returned crop/Fish/Meat origin differs from actual held, Cart and consumed Food")
        require(departed[crop] == arrived[crop] + cart_returned[crop] + cart[crop],
                "Actual Food departure/arrival/return equation failed")
    return world, source, worker, actor, crops


def verify(output, selected_map):
    report = load(output / "report.json")
    require(report["status"] == "PASS" and report["rule"] == 31 and report["schema"] == 47,
            "Physical Grower acceptance did not pass exact Rule31/schema47")
    require(report["real_ticks"] >= 12000 and report["fully_staffed_ticks"] > 0 and
            0 < report["observed_work"] <= report["real_ticks"], "Missing earned finite shared Farm labor")
    require(report["unchanged_supplied_model"] and report["model_sha256"] == MODEL_SHA and
            report["source_bytes_mode_mtime_unchanged"] and not report["snapshot_stock_or_population_injection"],
            "Supplied source or paid production authority boundary failed")
    require(report["map"] == selected_map, "Acceptance replay changed its explicit source map")
    require(report["fresh_restore_continuation_exact"] and report["recovery_exact"],
            "Missing exact restored continuation or Recovery")
    scene = report["actual_scene"]
    require(scene["farm"] == report["farm"] and scene["buildings"] <= 82 and scene["courier_channels"] <= 58,
            "Physical Growers exceeded shared body/channel budgets")
    if selected_map == "Cities/Chengdu.map":
        joint = scene["joint"]
        require(report["joint_actual_status"] == "PASS" and joint["paid_quays"] > 0 and
                joint["actual_fish_unloaded"] > 0 and joint["paid_tents"] > 0 and
                joint["actual_meat_unloaded"] > 0 and joint["paid_plantation_sources"] > 0 and
                joint["actual_plantation_returns"] > 0,
                "Joint physical Farm, Fishing, Hunting and plantation scene lacked an actual paid return")
    stages = report["five_crop_actual_stages"]
    require(len(stages) == 5 and {row["crop"] for row in stages} == set(range(5)), "Missing crop variation")
    for row in stages:
        crop = row["crop"]
        require(row["all_observed"] and len(row["outward_wait_work_loaded_return_receipt"]) == 5 and
                all(row["outward_wait_work_loaded_return_receipt"]) and row["actual_house_consumed"] > 0 and
                scene["market_received"][crop] > 0 and scene["house_received"][crop] > 0 and scene["house_consumed"][crop] > 0,
                "A crop skipped real movement, wait, work, carrying, Farm receipt or House consumption")
    require(report["actual_farm_receipts"] and report["actual_food_arrivals"] and
            report["actual_house_consumption"] and len(set(report["actual_field_assignments"])) == 5,
            "Missing actual rotating assignments, Farm receipts or accepted Food events")
    receipt_ids = set()
    for receipt in report["actual_farm_receipts"]:
        require(receipt["grower_id"] not in receipt_ids and 0 < receipt["dispatched_tick"] <= receipt["arrived_tick"] <
                receipt["harvested_tick"] < receipt["returned_tick"] and receipt["work_at_harvest"] % 80 == 0,
                "A Farm receipt reused a lifetime or bypassed physical work/return chronology")
        receipt_ids.add(receipt["grower_id"])
    checkpoints = {entry["label"]: entry for entry in report["checkpoints"]}
    require(set(checkpoints) == LABELS and len(report["checkpoints"]) == len(LABELS),
            "Missing or duplicated physical phase/Cart checkpoints")
    for label, entry in checkpoints.items():
        require(entry["file"] == label + ".save.json" and entry["exact_roundtrip"] and
                (label == "seed" or entry["exact_recovery"]), "Checkpoint was not exact or did not use Recovery")
        path = output / entry["file"]
        require(path.read_bytes() == (output / (label + ".roundtrip.json")).read_bytes(),
                "Canonical checkpoint rewrite differs")
        world, source, worker, actor, crops = check_snapshot(load(path), report["farm"])
        if label.startswith("grower-"):
            require(worker is not None and worker["returned_tick"] is None and
                    worker["field_id"] in crops and worker["crop"] == crops[worker["field_id"]]["crop"],
                    "Checkpoint contains no live immutable studied-Farm crop assignment")
            cell = next(body["cell"] for body in world["buildings"] if body["id"] == worker["field_id"])
            if label == "grower-outward":
                require(worker["phase"] == 1 and actor["edge_progress"] > 0 and worker["harvested_tick"] is None,
                        "Outward checkpoint replaced physical walking with a result flag")
            elif label in ("grower-waiting", "grower-working"):
                require(actor["path"] == [cell] and actor["path_vertex"] == actor["edge_progress"] == 0 and
                        worker["arrived_tick"] is not None and worker["harvested_tick"] is None,
                        "Waiting/working checkpoint is not physically at its assigned Field")
                require((worker["phase"] == 2 and 0 < worker["wait_left"] <= 5) if label == "grower-waiting"
                        else (worker["phase"] == 3 and worker["wait_left"] == 0 and worker["work_ticks"] >= 8),
                        "Waiting/working checkpoint lacks its actual finite wait or earned work")
            else:
                require(worker["phase"] == 4 and worker["harvested_tick"] is not None and worker["delivered"] == 0 and
                        actor["edge_progress"] > 0 and actor["path_vertex"] + 1 < len(actor["path"]),
                        "Loaded checkpoint is not the studied Farm's real moving one-crop return")
        if label == "food-cart":
            actors = {value["id"]: value for value in world["couriers"]}
            require(any(trip["source"] == report["farm"] and
                        sum(trip["loaded"]) > sum(trip["delivered"]) + sum(trip["returned"]) and
                        actors[trip["transfer"]["courier_id"]]["owner"] == report["farm"] and
                        actors[trip["transfer"]["courier_id"]]["role"] == 13 and
                        actors[trip["transfer"]["courier_id"]]["edge_progress"] > 0
                        for trip in world["hunting_food"]["trips"]),
                    "Food Cart checkpoint is not moving stock actually returned to the studied Farm")
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--map", choices=("Cities/Chengdu.map", "Cities/Xia.map"), default="Cities/Chengdu.map")
    args = parser.parse_args()
    data = args.data.resolve()
    files = [data / "Model/EmperorBuildingModels.txt", data / args.map]
    if not all(path.is_file() for path in files):
        print("NOT_RUN: supplied Original model/map not configured")
        return 77
    before = {path: fingerprint(path) for path in files}
    parent = ROOT / ".local/agri-mac06"
    parent.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="positive-replay-", dir=parent))
    executable = args.executable.resolve(strict=True)

    def execute(name, extra=(), expected=0, selected_map=None):
        command = [str(executable), "--data", str(data), "--map", args.map if selected_map is None else str(selected_map),
                   "--output", str(proof / name), "--ticks", "14000", *map(str, extra)]
        (proof / (name + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=240, check=False)
        (proof / (name + ".log")).write_text(result.stdout + result.stderr)
        require(result.returncode == expected, f"{name} exit={result.returncode}; retained proof: {proof}\n{result.stdout}{result.stderr}")

    execute("creator")
    verify(proof / "creator", args.map)
    retained = [proof / "creator" / name for name in ("seed.save.json", "final.save.json", "report.json")]
    creator_before = {path: fingerprint(path) for path in retained}
    execute("creator", expected=1)
    require(creator_before == {path: fingerprint(path) for path in retained}, "Rejected output reuse changed its existing proof")
    execute("replay", ("--replay", proof / "creator/seed.save.json", "--expected", proof / "creator/final.save.json"))
    replay = verify(proof / "replay", args.map)
    require(replay["separate_process_replay_exact"], "No independent process endpoint witness")
    require((proof / "creator/final.save.json").read_bytes() == (proof / "replay/final.save.json").read_bytes(),
            "Independent physical Grower canonical endpoints differ")
    require(creator_before == {path: fingerprint(path) for path in retained}, "Replay modified its explicit seed/expected input")
    wrong = load(proof / "creator/seed.save.json")
    wrong["schema_version"] = 46
    wrong_path = proof / "wrong-schema.save.json"
    wrong_path.write_text(json.dumps(wrong))
    wrong_before = fingerprint(wrong_path)
    execute("wrong-schema", ("--replay", wrong_path, "--expected", proof / "creator/final.save.json"), expected=1)
    require(not (proof / "wrong-schema").exists() and fingerprint(wrong_path) == wrong_before,
            "Rejected replay generation modified its input or published output")
    for name, unsafe in (("parent-map", "Cities/../Xia.map"), ("absolute-map", data / args.map)):
        execute(name, expected=1, selected_map=unsafe)
        require(not (proof / name).exists(), "Unsafe map selection published acceptance output")
    other_map = "Cities/Xia.map" if args.map == "Cities/Chengdu.map" else "Cities/Chengdu.map"
    if (data / other_map).is_file():
        other_before = fingerprint(data / other_map)
        execute("mismatched-map", ("--replay", proof / "creator/seed.save.json",
                                   "--expected", proof / "creator/final.save.json"), expected=1, selected_map=other_map)
        require(not (proof / "mismatched-map").exists() and fingerprint(data / other_map) == other_before,
                "Replay accepted a different selected map or modified its original bytes")
    require(creator_before == {path: fingerprint(path) for path in retained}, "Rejected guard modified its explicit replay inputs")
    require(before == {path: fingerprint(path) for path in files}, "Original source bytes/mode/mtime changed")
    print(f"PASS: five paid physical crop chains on {args.map}, coupled worker/Cart checkpoints and independent14k replay; {proof}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
