#!/usr/bin/env python3
"""Earn source-bound multi-field Food, then replay 12k+ ticks independently."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MODEL_SHA = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"


def require(value, message):
    if not value:
        raise AssertionError(message)


def fingerprint(path):
    info = path.stat()
    return (info.st_size, info.st_mode, info.st_mtime_ns,
            hashlib.sha256(path.read_bytes()).hexdigest())


def verify(report):
    require(report["status"] == "PASS" and report["rule"] == 30 and report["schema"] == 46,
            "Multi-field positive acceptance did not pass exact Rule30/schema46")
    require(report["real_ticks"] >= 12000 and report["actual_crop_receipt_and_consumption_count"] >= 3,
            "Missing 12k continuation or three genuine crop contributors")
    require(report["unchanged_supplied_model"] and report["model_sha256"] == MODEL_SHA and
            not report["snapshot_or_stock_injection"] and report["source_bytes_mode_mtime_unchanged"],
            "Paid source authority boundary failed")
    require(report["fresh_restore_continuation_exact"] and report["recovery_exact"] and
            report["mid_independent_progress_tick"] is not None and
            report["mid_loaded_cart_edge_tick"] is not None, "Missing exact work/cargo checkpoints")
    balance = report["conservation"]
    for crop in range(7):
        require(balance["produced"][crop] == balance["held"][crop] +
                balance["physical_cart_cargo"][crop] + balance["consumed"][crop],
                "Per-kind Food origin equation failed")
        require(balance["departed"][crop] == balance["received"][crop] +
                balance["returned"][crop] + balance["physical_cart_cargo"][crop],
                "Per-kind physical transfer equation failed")
    farm = report["multi_field_farm"]
    for crop in report["actual_crop_resources"]:
        require(farm["other_farm_harvests"][crop] == 0 and farm["farm_departed"][crop] > 0 and
                farm["market_received"][crop] > 0 and farm["house_received"][crop] > 0 and
                farm["house_consumed"][crop] > 0, "Ambiguous crop origin or unearned delivery")
    require(report["physical_arrival_transfers"] and report["actual_house_consumption"],
            "Missing actual accepted Cart/House evidence")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    args = parser.parse_args()
    data = args.data.resolve()
    files = [data / "Model/EmperorBuildingModels.txt", data / "Cities/Xia.map"]
    if not all(path.is_file() for path in files):
        print("NOT_RUN: supplied Original model/map not configured")
        return 77
    before = {path: fingerprint(path) for path in files}
    parent = ROOT / ".local/agri-mac05"
    parent.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="positive-replay-", dir=parent))
    executable = args.executable.resolve(strict=True)
    for name, extra in (("creator", []), ("replay", ["--replay", proof / "creator/seed.save.json",
                                                    "--expected", proof / "creator/final.save.json"])):
        output = proof / name
        command = [str(executable), "--data", str(data), "--output", str(output), "--ticks", "14000", *map(str, extra)]
        (proof / (name + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=240)
        (proof / (name + ".log")).write_text(result.stdout + result.stderr)
        require(result.returncode == 0, f"{name} failed; proof retained at {proof}: {result.stdout}{result.stderr}")
        report = json.loads((output / "report.json").read_text())
        verify(report)
        checkpoint = json.loads((output / "mid-cargo.save.json").read_text())["world"]
        actors = {actor["id"]: actor for actor in checkpoint["couriers"]}
        farm = report["multi_field_farm"]["farm"]
        require(any(trip["source"] == farm and
                    sum(trip["loaded"]) > sum(trip["delivered"]) + sum(trip["returned"]) and
                    actors[trip["transfer"]["courier_id"]]["edge_progress"] > 0
                    for trip in checkpoint["hunting_food"]["trips"]),
                "Mid-cargo checkpoint is not a physically moving load from the studied Farm")
        if name == "replay":
            require(report["separate_process_replay_exact"], "No separate-process equality witness")
    require((proof / "creator/final.save.json").read_bytes() ==
            (proof / "replay/final.save.json").read_bytes(), "Independent canonical endpoints differ")
    require(before == {path: fingerprint(path) for path in files}, "Original source changed")
    print(f"PASS: paid three-crop Farm and independent 14k replay; {proof}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
