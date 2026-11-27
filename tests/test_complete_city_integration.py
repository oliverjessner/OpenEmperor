#!/usr/bin/env python3
"""Audit the opt-in complete-city observer against an explicitly earned save.

Default mode is compared with the opt-in mode at identical canonical epochs;
new QA must never create or alter gameplay. Outputs, including failures, stay
under a fresh ignored proof directory. No private fixture is committed.
"""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import traceback


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = "paid-complete-city-acceptance-v1"
FORMAT = "openemperor-complete-city-acceptance-v1"
YEAR = 51 * 16 * 12


def check(value, message):
    if not value:
        raise AssertionError(message)


def read(path):
    return json.loads(path.read_text())


def protected(path):
    info = path.stat()
    return (hashlib.sha256(path.read_bytes()).hexdigest(), info.st_size,
            stat.S_IMODE(info.st_mode), info.st_mtime_ns)


def invoke(executable, arguments, proof, label):
    command = [str(executable), *map(str, arguments)]
    (proof / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                timeout=1800, check=False)
    except subprocess.TimeoutExpired as error:
        for name, content in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(content, bytes):
                content = content.decode(errors="replace")
            (proof / (label + "." + name + ".log")).write_text(content or "")
        raise
    (proof / (label + ".stdout.log")).write_text(result.stdout)
    (proof / (label + ".stderr.log")).write_text(result.stderr)
    return result


def audit_report(output, expected_complete, expected_rule=36, expected_schema=52):
    report = read(output / "report.json")
    check(report["format"] == FORMAT and report["technical_status"] == "PASS" and
          report["rule"] == expected_rule and report["policy"] == 3 and report["schema"] == expected_schema,
          "Wrong opt-in QA format, generation or technical result")
    assessment = report["complete_city"]
    check(assessment["contract"] == CONTRACT and bool(assessment["criteria"]),
          "Missing explicit complete-city contract")
    passed = all(value is True for value in assessment["criteria"].values())
    check(passed is expected_complete and report["status"] == assessment["status"] ==
          report["gameplay_coverage"] == ("PASS" if passed else "PARTIAL"),
          "Complete-city criteria and published result disagree")
    check(set(assessment["failed_criteria"]) ==
          {key for key, value in assessment["criteria"].items() if value is not True},
          "Lost an actual incomplete acceptance gate")
    check(report["stock_population_model_or_arrival_injection"] is False and
          report["commands_after_restore"] == 0 and report["source_bytes_mode_mtime_unchanged"] and
          report["fresh_restored_world_continuation_exact"], "Observer granted or changed authority")
    check(len(report["checkpoints"]) == 12 and all(
        row["canonical_roundtrip_exact"] and row["recovery_exact"] for row in report["checkpoints"]),
        "Complete-city mode lost an existing canonical/Recovery checkpoint")
    check(report["persistence"]["hostile_controls"] >= 4 and
          report["persistence"]["protected_final_save_unchanged"] and
          report["persistence"]["rejected_restore_and_write"], "Missing atomic hostile controls")
    initial_document = read(output / "initial.save.json")
    check(initial_document["schema_version"] == expected_schema and
          initial_document["rules"]["version"] == expected_rule,
          "Observer changed the explicitly selected input generation")
    initial = initial_document["world"]
    progressions = sum(
        1 for house in initial["original_production"]["housing"]["houses"]
        for index, entry in enumerate(house["first_transitions"])
        if entry and entry["kind"] == 1 and
        entry["evaluation"]["input"]["occupants"] > 0 and
        index + 3 > entry["evaluation"]["input"]["type"])
    check(assessment["retained_paid_progression_history"] == progressions,
          "Admission or downward Evolution was counted as paid upward progression")
    final_document = read(output / "final.save.json")
    check(final_document["schema_version"] == expected_schema and
          final_document["rules"] == initial_document["rules"],
          "Observer migrated its canonical generation or contract markers")
    final = final_document["world"]
    # Public DTO identities: Model53 Mill is Object48; the sole ledger stores
    # seven raw kinds followed by graded Dinners. This independently excludes
    # actual Mill raw inputs, cargo and claims from edible held reserves.
    bodies = {row["id"]: row for row in final["buildings"]}
    reserve = mill_raw = 0
    for stock in final["processed_food"]["stocks"]:
        if not bodies[stock["building_id"]]["placed"]:
            continue
        for index, amount in enumerate(stock["amounts"]):
            if bodies[stock["building_id"]]["kind"] == 48 and index < 7:
                mill_raw += amount
            else:
                reserve += amount
    check(assessment["maximum_edible_physical_reserve"] >= reserve and
          assessment["maximum_separate_mill_raw_inputs"] >= mill_raw,
          "Reserve extrema contradict actual sole-ledger physical stock")
    maturity = assessment["maturity_tick"]
    if maturity is not None:
        check(assessment["actual_post_joint_maturity_ticks"] == report["final_tick"] - maturity and
              maturity >= report["food_and_meal_maturity_tick"],
              "Joint maturity preceded its actual Food/meal maturity or changed clocks")
        previous = maturity
        for window in assessment["complete_post_maturity_windows"]:
            check(window["begin_tick"] == previous and window["end_tick"] - previous == YEAR,
                  "Whole seasonal-cycle windows overlap or skip ticks")
            previous = window["end_tick"]
        if passed:
            check(assessment["actual_post_joint_maturity_ticks"] >= 20400 and
                  len(assessment["complete_post_maturity_windows"]) >= 2 and
                  assessment["minimum_edible_physical_reserve_after_maturity"] > 0 and
                  assessment["zero_edible_reserve_ticks_after_maturity"] == 0,
                  "Finite reserve/retention window did not complete")
            check(assessment["minimum_funds"] >= 0 and
                  assessment["post_maturity_taxes"] >= assessment["post_maturity_installed_upkeep"],
                  "A deficit was labelled stable operating cashflow")
    else:
        check(not passed and assessment["actual_post_joint_maturity_ticks"] == 0,
              "Complete-city success lacks joint earned maturity")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--ticks", type=int)
    parser.add_argument("--expect-complete", action="store_true")
    parser.add_argument("--cli-only", action="store_true")
    parser.add_argument("--irrigated-harvest", action="store_true")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    private = ROOT / ".local/agri-mac14"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="complete-city-regression-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    try:
        help_result = invoke(executable, ("--help",), proof, "help")
        check(help_result.returncode == 0 and "--complete-city" in help_result.stdout,
              "Complete-city opt-in is undiscoverable")
        duplicate = invoke(executable, ("--complete-city", "--complete-city"), proof, "duplicate")
        check(duplicate.returncode == 1 and "Duplicate complete-city argument" in duplicate.stderr,
              "Duplicate QA selection was accepted")
        duplicate_generation = invoke(executable, ("--irrigated-harvest", "--irrigated-harvest"), proof,
                                      "duplicate-generation")
        check(duplicate_generation.returncode == 1 and
              "Duplicate irrigated-harvest argument" in duplicate_generation.stderr,
              "Duplicate Rule37 selection was accepted")
        if args.cli_only:
            print("PASS: explicit complete-city CLI selection")
            return 0
        check(args.data and args.input, "Supply explicit --data and legitimately earned --input")
        data, seed = args.data.resolve(strict=True), args.input.resolve(strict=True)
        document = read(seed)
        source_files = (seed, data / "Model/EmperorBuildingModels.txt", data / document["map"]["relative_path"])
        before = {path: protected(path) for path in source_files}
        expected_rule, expected_schema = (37, 53) if args.irrigated_harvest else (36, 52)
        check((document["rules"]["version"], document["schema_version"]) ==
              (expected_rule, expected_schema), "Input requires its explicit generation selection")
        ticks = args.ticks if args.ticks is not None else (100000 if args.expect_complete else 20400)
        common = ("--data", data, "--input", seed, "--ticks", ticks)
        if args.irrigated_harvest:
            wrong_generation_output = proof / "wrong-generation-output"
            refused_generation = invoke(executable, (*common, "--output", wrong_generation_output),
                                        proof, "wrong-generation")
            check(refused_generation.returncode == 1 and
                  "Only explicit source-bound Rule36/schema52 city inputs" in refused_generation.stderr and
                  not list(wrong_generation_output.glob("*.save.json")),
                  "Rule37 input was silently selected or migrated without its explicit flag")
            common += ("--irrigated-harvest",)
        legacy, complete, replay = (proof / name for name in ("legacy", "complete", "replay"))
        old = invoke(executable, (*common, "--output", legacy), proof, "legacy")
        check(old.returncode == 0 and read(legacy / "report.json")["format"] ==
              "openemperor-agriculture-integration-acceptance-v1", "Legacy observer no longer works")
        created = invoke(executable, (*common, "--output", complete, "--complete-city"), proof, "complete")
        expected_exit = 0 if args.expect_complete else 2
        check(created.returncode == expected_exit, "Unexpected complete-city exit: " + created.stderr)
        report = audit_report(complete, args.expect_complete, expected_rule, expected_schema)
        for row in report["checkpoints"]:
            name = row["file"]
            check((legacy / name).read_bytes() == (complete / name).read_bytes(),
                  "Opt-in QA changed canonical gameplay: " + name)
        legacy_lines = (legacy / "telemetry.jsonl").read_text().splitlines()
        complete_lines = (complete / "telemetry.jsonl").read_text().splitlines()
        check(len(legacy_lines) == len(complete_lines), "QA changed sampling epochs")
        for a, b in zip(legacy_lines, complete_lines):
            old_row, new_row = json.loads(a), json.loads(b)
            check("complete_city" not in old_row and new_row.pop("complete_city") is not None and
                  old_row == new_row, "QA changed an existing observer field")
        source_before = {path: protected(path) for path in complete.rglob("*") if path.is_file()}
        result = invoke(executable, (*common, "--output", replay, "--complete-city", "--replay", complete), proof, "replay")
        check(result.returncode == expected_exit, "Separate complete-city replay failed: " + result.stderr)
        repeated = audit_report(replay, args.expect_complete, expected_rule, expected_schema)
        check(repeated["separate_process_replay_exact"] and report["complete_city"] == repeated["complete_city"] and
              (complete / "final.save.json").read_bytes() == (replay / "final.save.json").read_bytes() and
              (complete / "telemetry.jsonl").read_bytes() == (replay / "telemetry.jsonl").read_bytes(),
              "Independent process changed complete-city observations or canonical state")
        bad = proof / "wrong-contract-inputs"
        bad.mkdir()
        for name in ("initial.save.json", "final.save.json", "telemetry.jsonl"):
            shutil.copy2(complete / name, bad / name)
        metadata = copy.deepcopy(report)
        metadata["complete_city"]["contract"] += "-wrong"
        (bad / "report.json").write_text(json.dumps(metadata) + "\n")
        bad_before = {path: protected(path) for path in bad.iterdir()}
        rejected_output = proof / "wrong-contract-output"
        refused = invoke(executable, (*common, "--output", rejected_output, "--complete-city", "--replay", bad), proof, "wrong-contract")
        check(refused.returncode == 1 and "same explicit QA contract" in refused.stderr and
              not list(rejected_output.glob("*.save.json")), "Wrong QA contract reached checkpoint publication")
        check(before == {path: protected(path) for path in source_files} and
              source_before == {path: protected(path) for path in source_before} and
              bad_before == {path: protected(path) for path in bad_before}, "Replay changed supplied source/creator inputs")
        (proof / "result.json").write_text(json.dumps({"status": "PASS", "complete_city_earned": args.expect_complete,
            "rule": expected_rule, "schema": expected_schema,
            "legacy_and_complete_gameplay_byte_exact": True, "independent_process_exact": True,
            "wrong_contract_refused_atomically": True, "source_and_inputs_unchanged": True}, indent=2) + "\n")
        print("PASS: explicit complete-city criteria, unchanged gameplay, independent replay and contract refusal")
        return 0
    except Exception as error:
        (proof / "failure.json").write_text(json.dumps({"status": "FAIL", "reason": str(error),
            "traceback": traceback.format_exc()}, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
