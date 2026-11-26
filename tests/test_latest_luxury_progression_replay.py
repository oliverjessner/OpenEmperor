#!/usr/bin/env python3
"""Earn the explicit Rule36 V2 city, hold 20,400 ticks and replay independently.

PASS establishes finite luxury consumption and exact persistence, without an
uninterrupted Elite17-retention claim. Preserve all attempts under .local/.
"""

import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import traceback

from test_luxury_progression_replay import (
    ROOT, MODEL_SHA, MAP_SHA, LUXURIES, positive_chain, protected, read, require,
)


RECIPE = "authored-latest-agriculture-xia-complementary-wheat-v2"
FORMAT = "openemperor-latest-agriculture-luxury-v2"
FLAGS = ("--latest-agriculture", "--latest-plan-v2")
WHEAT = {(103, (115, 128)), (105, (115, 130))}


def invoke(executable, data, output, proof, label, *arguments):
    command = [str(executable), "--data", str(data), "--output", str(output),
               *map(str, arguments)]
    (proof / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                timeout=240, check=False)
    except subprocess.TimeoutExpired as error:
        for name, value in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(value, bytes):
                value = value.decode(errors="replace")
            (proof / (label + "." + name + ".log")).write_text(value or "")
        raise
    (proof / (label + ".stdout.log")).write_text(result.stdout)
    (proof / (label + ".stderr.log")).write_text(result.stderr)
    return result


def verify(output, result, creator):
    require(result.returncode == 0,
            f"Positive V2 city failed ({result.returncode}): {result.stdout}{result.stderr}")
    report = read(output / "report.json")
    require(report["status"] == "PASS" and report["format"] == FORMAT and
            report["recipe"] == RECIPE and report["rule"] == 36 and report["schema"] == 52,
            "Explicit V2 did not earn Rule36/schema52 acceptance")
    require(report["unchanged_supplied_model"] and report["model_sha256"] == MODEL_SHA and
            report["source_bytes_mode_mtime_unchanged"] and not report["snapshot_or_stock_injection"],
            "V2 changed supplied data or injected gameplay authority")
    require(report["positive_luxury_consumption"] == [True, True, True] and
            report["paid_recipe_without_permanent_abort"] and
            report["first_permanent_command_blocker"] is None,
            "V2 omitted a consumed luxury or permanently aborted its paid plan")
    require(report["begin_tick"] == 0 and report["search_bound"] == 30000 and
            report["requested_post_maturity_ticks"] == 20400 and
            0 < report["maturity_tick"] <= 30000 and
            report["end_tick"] == report["maturity_tick"] + 20400 and
            report["actual_post_maturity_ticks"] == 20400 and report["minimum_funds"] >= 0 and
            report["final"]["funds"] >= 0,
            "V2 did not earn maturity plus the complete nonnegative-Funds hold")
    require(report["fresh_restore_continuation_exact"] and
            report["persistence"]["hostile_controls"] >= 4 and
            report["persistence"]["recovery_exact"] and
            report["persistence"]["rejection_preserved_existing_save"],
            "V2 omitted complete continuation, hostile controls or Recovery")
    for good, (_, producer, source) in LUXURIES.items():
        positive_chain(report, good, producer, source)

    final = read(output / "final.save.json")
    world = final["world"]
    require(final["schema_version"] == 52 and final["rules"]["version"] == 36 and
            world["ticks"] == report["end_tick"], "V2 final save changed generation or clock")
    bounds = report["resource_bounds"]
    channels = (len(world["couriers"]) + len(world["fishing"]["quays"]) +
                len(world["hunting"]["tents"]) + 3 * len(world["hunting"]["habitats"]))
    require(bounds["body_limit"] == 82 and bounds["channel_limit"] == 58 and
            bounds["bodies"] == len(world["buildings"]) <= 82 and
            bounds["channels"] == channels <= 58 and
            bounds["research_navigation_tracks"] == 0 and not bounds["research_navigation_session"],
            "V2 exceeded existing bodies/channels or used research navigation")
    commands = read(output / "commands.json")
    accepted = [row for row in commands if row["accepted"]]
    require(accepted and all(row["changed"] and row["quoted_cost"] >= 0 and
                            row["funds_after"] == row["funds_before"] - row["quoted_cost"] and
                            row["sequence_after"] == row["sequence_before"] + 1 for row in accepted),
            "V2 command receipts lack ordinary paid monotone mutations")
    initial_construction = 0 if creator else report["initial"]["construction"]
    require(sum(row["quoted_cost"] for row in accepted) ==
            report["final"]["construction"] - initial_construction,
            "V2 command payments do not equal actual installed construction")
    wheat = report["v2_extra_paid_wheat"]
    require(len(wheat) == 2 and {(row["parent_plan_id"], tuple(row["cell"])) for row in wheat} == WHEAT,
            "V2 did not retain exactly its two explicit complementary Wheat purchases")
    require(len({row["actual_parent_id"] for row in wheat}) == 2 and
            len({row["actual_field_id"] for row in wheat}) == 2, "V2 Wheat identities were aliased")
    buildings = {row["id"]: row for row in world["buildings"]}
    for row in wheat:
        parent, field = row["actual_parent_id"], row["actual_field_id"]
        # Existing public DTO identities: Food Farm16, Wheat Field17, Wheat command24/crop0.
        require(row["actual_parent_binding"] and row["prepared_physical_route"] and
                row["prepared_route_cells"] > 1 and buildings[parent]["kind"] == 16 and
                buildings[field]["kind"] == 17 and buildings[field]["placed"] and
                buildings[field]["cell"] == row["cell"], "V2 Wheat lacks actual parent/body/route evidence")
        require(sum(item["farm_id"] == parent and item["field_id"] == field and item["crop"] == 0
                    for item in world["processed_food"]["fields"]) == 1,
                "V2 Wheat report disagrees with the sole typed field ledger")
        purchases = [item for item in accepted if item.get("building_id") == field]
        require(len(purchases) == 1 and purchases[0]["command"] == 24 and
                purchases[0]["owner_argument"] == parent and purchases[0]["cell"] == row["cell"] and
                purchases[0]["quoted_cost"] > 0,
                "V2 Wheat was not bought once with its actual paid parent")

    checkpoints = report["checkpoints"]
    require(len({row["file"] for row in checkpoints}) == len(checkpoints),
            "V2 checkpoint manifest contains duplicate identities")
    for row in checkpoints:
        path = output / row["file"]
        require(path.parent == output and row["exact_roundtrip"] and
                protected(path)["sha256"] == row["sha256"] and
                read(path)["world"]["ticks"] == row["tick"] and
                path.read_bytes() == (output / (row["label"] + ".roundtrip.json")).read_bytes(),
                "V2 manifest digest, tick or canonical roundtrip disagrees with its actual checkpoint")
    seed = read(output / "seed.save.json")
    require(seed["schema_version"] == 52 and seed["rules"]["version"] == 36 and
            seed["world"]["ticks"] == 0 and report["initial"]["population"] == 0 and
            all(row["produced"] == row["consumed"] == row["imported"] == 0
                for row in seed["world"]["original_goods"]["totals"]),
            "V2 seed did not retain its empty-population, unearned-luxury starting state")
    cargo = read(output / "mid-cargo.save.json")["world"]
    actors = {row["id"]: row for row in cargo["couriers"]}
    require(any(row["good"] in LUXURIES and row["amount"] > 0 and
                actors[row["transfer"]["courier_id"]]["edge_progress"] > 0
                for row in cargo["original_goods"]["cargo"]),
            "V2 mid-cargo checkpoint lacks a real moving luxury load")
    require(any(path.name.startswith("checkpoint-") for path in (output / "recovery").rglob("*.json")),
            "V2 Recovery report has no actual checkpoint artifact")
    return report


def refused(executable, data, output, proof, label, arguments, inputs=(), diagnostic=None):
    before = {path: protected(path) for path in inputs}
    result = invoke(executable, data, output, proof, label, *arguments)
    require(result.returncode == 1 and "FAIL" in result.stderr and
            "PASS Rule" not in result.stdout and "PARTIAL Rule" not in result.stdout,
            f"{label} did not refuse the mismatched explicit QA plan: {result.stdout}{result.stderr}")
    if diagnostic:
        require(diagnostic in result.stderr, f"{label} failed for an unrelated reason: {result.stderr}")
    require(not output.exists(), f"{label} published output before provenance refusal")
    require(before == {path: protected(path) for path in inputs}, f"{label} changed an explicit input")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--real", type=Path, required=True)
    args = parser.parse_args()
    data = args.real.absolute()
    sources = [data / "Model/EmperorBuildingModels.txt", data / "Cities/Xia.map"]
    if not all(path.is_file() for path in sources):
        print("SKIP: supplied Original Model/Xia installation is unavailable")
        return 77
    source_before = {path: protected(path) for path in sources}
    require(source_before[sources[0]]["sha256"] == MODEL_SHA and
            source_before[sources[1]]["sha256"] == MAP_SHA, "Supplied Normal model/Xia identity changed")
    private = ROOT / ".local/agri-mac14"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="latest-luxury-replay-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    try:
        executable = args.executable.resolve(strict=True)
        refused(executable, data, proof / "requires-latest-output", proof, "requires-latest",
                ("--latest-plan-v2",), diagnostic="requires --latest-agriculture")
        for flag in FLAGS:
            refused(executable, data, proof / (flag[2:] + "-duplicate-output"), proof,
                    flag[2:] + "-duplicate", (*FLAGS, flag), diagnostic="Duplicate acceptance argument")

        creator = proof / "creator"
        earned = verify(creator, invoke(executable, data, creator, proof, "creator", *FLAGS,
                                       "--ticks", 30000, "--hold", 20400), True)
        protected_inputs = {path: protected(path) for path in creator.rglob("*") if path.is_file()}
        replay = proof / "replay"
        repeated = verify(replay, invoke(executable, data, replay, proof, "replay", *FLAGS,
                                         "--ticks", 30000, "--hold", 20400,
                                         "--input", creator / "seed.save.json",
                                         "--expected", creator / "final.save.json"), False)
        require(repeated["separate_process_replay_exact"] and repeated["end_tick"] == earned["end_tick"],
                "Independent V2 replay lacks exact terminal continuation")
        a = {row["file"]: row for row in earned["checkpoints"]}
        b = {row["file"]: row for row in repeated["checkpoints"]}
        common = a.keys() & b.keys()
        require({"seed.save.json", "mid-cargo.save.json", "final.save.json"} <= common,
                "Independent V2 replay omitted required common checkpoints")
        require("maturity.save.json" in a and a["maturity.save.json"]["tick"] == earned["maturity_tick"],
                "V2 creator omitted its real maturity checkpoint")
        for filename in common:
            require(a[filename] == b[filename] and (creator / filename).read_bytes() ==
                    (replay / filename).read_bytes(), "Independent V2 common checkpoint bytes/manifest differ")

        # Refusals use already-earned inputs, never another positive long run.
        early = min((row for row in earned["checkpoints"] if row["tick"] > 0), key=lambda row: row["tick"])["file"]
        refused(executable, data, proof / "missing-v2-flag-output", proof, "missing-v2-flag",
                ("--latest-agriculture", "--input", creator / "seed.save.json", "--expected", creator / early),
                inputs=(creator / "seed.save.json", creator / early, creator / "report.json"),
                diagnostic="V2 creator replay requires --latest-plan-v2")
        for case in ("missing-metadata", "wrong-recipe", "wrong-digest"):
            inputs = proof / (case + "-inputs")
            inputs.mkdir()
            for filename in ("seed.save.json", early):
                shutil.copy2(creator / filename, inputs / filename)
            metadata = copy.deepcopy(earned)
            if case == "wrong-recipe":
                metadata["recipe"] = RECIPE + "-wrong"
            if case == "wrong-digest":
                next(row for row in metadata["checkpoints"] if row["file"] == "seed.save.json")["sha256"] = "0" * 64
            if case != "missing-metadata":
                (inputs / "report.json").write_text(json.dumps(metadata, indent=2) + "\n")
            refused(executable, data, proof / (case + "-output"), proof, case,
                    (*FLAGS, "--input", inputs / "seed.save.json", "--expected", inputs / early),
                    inputs=tuple(inputs.iterdir()), diagnostic={
                        "missing-metadata": "Bounded regular QA replay artifact required",
                        "wrong-recipe": "matching explicit creator QA plan provenance",
                        "wrong-digest": "checkpoint bytes/tick do not match",
                    }[case])
        require(protected_inputs == {path: protected(path) for path in protected_inputs},
                "V2 replay/refusals changed creator bytes, mode or mtime")
        require(source_before == {path: protected(path) for path in sources},
                "V2 regression changed Original bytes, mode or mtime")
        (proof / "result.json").write_text(json.dumps({"status": "PASS", "rule": 36, "schema": 52,
            "recipe": RECIPE, "creator_maturity_tick": earned["maturity_tick"], "post_maturity_ticks": 20400,
            "all_common_checkpoints_byte_equal": True, "source_and_creator_inputs_preserved": True,
            "uninterrupted_elite17_retention_claimed": False}, indent=2) + "\n")
        print("PASS: explicit V2 paid luxury city, complete hold, exact independent replay and provenance refusals")
        return 0
    except Exception as error:
        (proof / "failure.json").write_text(json.dumps({"status": "FAIL", "reason": str(error),
            "traceback": traceback.format_exc()}, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
