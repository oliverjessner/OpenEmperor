#!/usr/bin/env python3
"""Earn unchanged-model luxury consumption, then replay in a separate process.

All outputs, including failed attempts, remain in a fresh ignored proof directory.
PARTIAL is a failed positive regression, never a passing substitute.
"""

import argparse
import hashlib
import json
from pathlib import Path
import stat
import subprocess
import tempfile
import traceback


ROOT = Path(__file__).resolve().parents[1]
MODEL_SHA = "7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6"
MAP_SHA = "871266c415d7500ff3cf3fe70fc6b36caecc4d4cbf09d4d737849e4060906433"
# Historical public Rule29 Goods/Object DTO identities, independently named.
LUXURIES = {2: ("Tea", 33, 33), 3: ("Silk", 32, 35),
            5: ("Lacquerware", 30, 34)}
HOUSE_KINDS = {6, 39, 40}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(path):
    return json.loads(path.read_text())


def protected(path):
    info = path.stat()
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": info.st_size, "mtime_ns": info.st_mtime_ns,
            "mode": stat.S_IMODE(info.st_mode)}


def invoke(executable, data, output, proof, label, *arguments):
    command = [str(executable), "--data", str(data), "--output", str(output),
               *map(str, arguments)]
    (proof / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            timeout=270, check=False)
    (proof / (label + ".stdout.log")).write_text(result.stdout)
    (proof / (label + ".stderr.log")).write_text(result.stderr)
    require(result.returncode == 0,
            f"{label} must earn PASS; exit={result.returncode}: "
            + result.stdout[-3000:] + result.stderr[-3000:])
    report = read(output / "report.json")
    require(report["status"] == "PASS", f"{label} did not earn positive acceptance")
    require(report["rule"] == 29 and report["schema"] == 45,
            f"{label} changed historical Rule29/schema45")
    require(report["model_sha256"] == MODEL_SHA and report["unchanged_supplied_model"],
            f"{label} did not use the unchanged supplied Normal model")
    require(report["positive_luxury_consumption"] == [True, True, True],
            f"{label} omitted a genuinely consumed luxury")
    require(not report["snapshot_or_stock_injection"] and
            report["source_bytes_mode_mtime_unchanged"],
            f"{label} did not preserve the paid/source authority boundary")
    require(report["fresh_restore_continuation_exact"],
            f"{label} lost independent restored World continuation")
    require(report["persistence"]["hostile_controls"] >= 4 and
            report["persistence"]["recovery_exact"] and
            report["persistence"]["rejection_preserved_existing_save"],
            f"{label} did not complete meaningful hostile/Recovery controls")
    return report


def positive_chain(report, good, producer_kind, plant_source_kind):
    label = LUXURIES[good][0]
    upstream = report["actual_upstream_work_pickup_return_and_processor_debits"]
    returns = [row for row in upstream
               if row["event"] == "plantation_actual_work_pickup_or_return"
               and row["source_kind"] == plant_source_kind and row["returned"] > 0
               and row["last_receipt"] is not None]
    require(returns, label + " has no actual plantation return witness")
    for row in returns:
        receipt = row["last_receipt"]
        require(row["productive_ticks"] > 0 and
                receipt["dispatch"] < receipt["pickup"] < receipt["return"] <= row["tick"],
                label + " return lacks ordered physical work/pickup/return")
    completions = [row for row in upstream
                   if row["event"] == "processor_actual_completion_and_input_debit"
                   and row["source_kind"] == producer_kind and row["completed_delta"] > 0]
    if good in (3, 5):
        required_aux = (2,) if good == 3 else (0, 3)
        require(any(all(row["aux_debit"][index] > 0 for index in required_aux)
                    for row in completions),
                label + " lacks actual processor completion with all recipe debits")

    dispatches = report["actual_luxury_dispatches"]
    arrivals = [row for row in report["actual_luxury_arrivals"]
                if row["good"] == good and row["arrived_units"] > 0]
    for arrival in arrivals:
        matching = [row for row in dispatches
                    if (row["courier"], row["sequence"], row["good"], row["source"], row["target"])
                    == (arrival["courier"], arrival["sequence"], good,
                        arrival["source"], arrival["target"])]
        require(matching and any(row["tick"] < arrival["tick"] and
                                 row["actual_path_vertices"] > 1 for row in matching),
                label + " arrival lacks its actual earlier moving dispatch")

    consumed = [row for row in report["actual_luxury_consumption"]
                if row["good"] == good and row["amount"] > 0]
    require(consumed, label + " has no actual per-House debit")
    connected = False
    for debit in consumed:
        field = label.lower()
        same_tick_received = sum(row["arrived_units"] for row in arrivals
                                 if row["target"] == debit["house"] and
                                 row["tick"] == debit["tick"])
        require(debit["before_tick"]["occupants"] > 0 and
                debit["before_tick"][field] + same_tick_received -
                debit["after_tick"][field] == debit["amount"],
                label + " House witness does not debit its physical current stock")
        for home in arrivals:
            if not (home["target"] == debit["house"] and home["target_kind"] in HOUSE_KINDS
                    and home["source_kind"] == 9 and home["tick"] <= debit["tick"]):
                continue
            for market in arrivals:
                if not (market["source_kind"] == 3 and market["target_kind"] == 9
                        and market["target"] == home["source"] and market["tick"] < home["tick"]):
                    continue
                for warehouse in arrivals:
                    if (warehouse["source_kind"] == producer_kind and warehouse["target_kind"] == 3
                            and warehouse["target"] == market["source"]
                            and warehouse["tick"] < market["tick"]
                            and any(row["tick"] < warehouse["tick"] for row in returns)
                            and (good == 2 or any(row["source"] == warehouse["source"] and
                                                 row["tick"] < warehouse["tick"]
                                                 for row in completions))):
                        connected = True
    require(connected, label + " lacks an ordered producer/Warehouse/Market/House/consumption chain")
    balance = next(row for row in report["final"]["goods_balances"] if row["good"] == good)
    require(balance["produced"] > 0 and balance["consumed"] > 0 and
            balance["imported"] == 0 and balance["demolished"] == 0 and
            balance["conserved"] and
            balance["produced"] == balance["stock"] + balance["cargo"] + balance["consumed"],
            label + " source/stock/cargo/consumption equation failed")
    per_house = sum(row["ledger_and_staff"]["consumed"][good]
                    for row in report["final"]["houses"])
    require(per_house == balance["consumed"], label + " per-House/global sinks differ")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--real", type=Path, required=True)
    arguments = parser.parse_args()
    data = arguments.real.absolute()
    required = [data / "Model/EmperorBuildingModels.txt", data / "Cities/Xia.map"]
    if not all(path.is_file() for path in required):
        print("SKIP: supplied Original Model/Xia installation is unavailable")
        return 77
    before = {path: protected(path) for path in required}
    require(before[required[0]]["sha256"] == MODEL_SHA and
            before[required[1]]["sha256"] == MAP_SHA,
            "Present Original Model/Xia data differs from the required unchanged source")
    private = ROOT / ".local/agri-mac04"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="positive-replay-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    try:
        executable = arguments.executable.resolve(strict=True)
        creator = proof / "creator"
        earned = invoke(executable, data, creator, proof, "creator",
                        "--ticks", "6000", "--hold", "0")
        require(earned["begin_tick"] == 0 and 0 < earned["maturity_tick"] <= 6000
                and earned["end_tick"] == earned["maturity_tick"]
                and earned["actual_post_maturity_ticks"] == 0,
                "Creator did not mature within its real6000-tick bound")
        seed = read(creator / "seed.save.json")
        require(seed["schema_version"] == 45 and seed["rules"]["version"] == 29
                and seed["world"]["ticks"] == 0 and earned["initial"]["population"] == 0,
                "Creator did not start from its paid empty-population Rule29 seed")
        require(all(row["produced"] == row["consumed"] == row["imported"] == 0
                    for row in seed["world"]["original_goods"]["totals"]),
                "Creator seed already contains luxury origins or consumption")
        for good, (_, producer, plant_source) in LUXURIES.items():
            positive_chain(earned, good, producer, plant_source)
        commands = read(creator / "commands.json")
        accepted = [row for row in commands if row["accepted"]]
        require(accepted and all(row["changed"] and
                                row["funds_after"] == row["funds_before"] - row["quoted_cost"] and
                                row["sequence_after"] == row["sequence_before"] + 1
                                for row in accepted),
                "Creator command receipts lack real paid monotone mutations")
        require(sum(row["quoted_cost"] for row in accepted) == earned["final"]["construction"],
                "Creator installed construction differs from all actual paid commands")
        require(earned["actual_transitions"] and all(
                row["tick"] > 0 and row["occupants"] > 0 for row in earned["actual_transitions"]),
                "Creator has no actual occupied natural progression sequence")

        checkpoint = creator / "mid-cargo.save.json"
        final = creator / "final.save.json"
        cargo = read(checkpoint)["world"]
        actors = {row["id"]: row for row in cargo["couriers"]}
        require(any(row["good"] in LUXURIES and row["amount"] > 0 and
                    actors[row["transfer"]["courier_id"]]["edge_progress"] > 0
                    for row in cargo["original_goods"]["cargo"]),
                "Replay input lacks a real moving loaded luxury courier")
        protected_inputs = {path: protected(path) for path in (checkpoint, final)}
        replay = proof / "replay"
        repeated = invoke(executable, data, replay, proof, "replay", "--input", checkpoint,
                          "--expected", final, "--hold", "0")
        require(repeated["separate_process_replay_exact"] and
                repeated["end_tick"] == earned["end_tick"] and
                (replay / "final.save.json").read_bytes() == final.read_bytes(),
                "Separate-process replay failed complete canonical SaveDocument equality")
        for path, original in protected_inputs.items():
            require(protected(path) == original, "Replay changed protected creator checkpoint")
        for path, original in before.items():
            require(protected(path) == original, "Acceptance changed original bytes/mode/mtime")
        (proof / "result.json").write_text(json.dumps({"status": "PASS", "rule": 29,
            "schema": 45, "creator_maturity_tick": earned["maturity_tick"],
            "separate_process_canonical_bytes_equal": True,
            "source_inputs_preserved": True}, indent=2) + "\n")
        print("PASS: paid unchanged-model Tea/Silk/Lacquerware consumption and exact separate-process replay")
        return 0
    except Exception as error:
        (proof / "failure.json").write_text(json.dumps({"status": "FAIL", "reason": str(error),
            "traceback": traceback.format_exc()}, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
