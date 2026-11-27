#!/usr/bin/env python3
"""Earn and observe an explicit Rule37 or separately selected Rule38 city from empty tick0 twice.

Creator children retain600s each. The existing observer retains1800s per
child; its three full continuations receive5460s including audit overhead.
The complete wrapper needs6720s including120 seconds of independent audits.
Protected Rule37 restored SliceV1 remains a negative Meat control; Rule38 reports its actual unchanged SliceV1 criteria.
only the sealed fresh-from-zero V2 all14 assessment earns whole-city PASS.
Failures and all child evidence are preserved.
"""

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile
import time
import traceback

from test_complete_city_integration import audit_report, YEAR
from test_food_warehouses_replay import totals as food_totals


ROOT = Path(__file__).resolve().parents[1]
CREATOR_FORMAT = "openemperor-paid-complete-city-creator-v1"
PLAN_FORMAT = "openemperor-paid-complete-city-plan-v1"
SOURCE_MAP = "Cities/Chengdu.map"
CREATOR_SECONDS = 600
OBSERVER_CHILD_SECONDS = 1800
OBSERVER_WRAPPER_SECONDS = 3 * OBSERVER_CHILD_SECONDS + 60
TOTAL_SECONDS = 2 * CREATOR_SECONDS + OBSERVER_WRAPPER_SECONDS + 60
KINDS = ("wheat", "millet", "rice", "cabbage", "soy", "hemp", "tea", "lacquer", "mulberry")
WHOLE_FORMAT = "openemperor-paid-complete-city-whole-v2"
WHOLE_CONTRACT = "paid-complete-city-from-zero-v2"
CRITERIA = {
    "installed_all_chains", "actually_supplied_21_ditches", "joint_maturity_earned",
    "post_maturity_20400_ticks", "two_full_seasonal_cycle_windows",
    "renewed_house_food_meals_and_luxuries_in_every_full_window",
    "positive_edible_physical_reserves_after_maturity", "paid_occupied_cohort_retained",
    "actual_luxury_consumer_family_retained", "no_devolution_with_actual_food_deficit",
    "real_progression_witness", "funds_never_negative",
    "post_maturity_operating_cashflow_nonnegative", "real_worker_supply_bounds",
}
LUXURIES = (2, 3, 5)
RULE, SCHEMA = 37, 53
CONTRACT = "authored-irrigated-harvest-v1"


def check(value, message):
    if not value:
        raise AssertionError(message)


def read(path):
    check(path.is_file() and not path.is_symlink() and path.stat().st_size <= 16 * 1024 * 1024,
          "Require a bounded real JSON artifact: " + str(path))
    return json.loads(path.read_text())


def fingerprint(path):
    check(path.is_file() and not path.is_symlink(), "Require an unchanged regular input: " + str(path))
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    info = path.stat()
    return {"sha256": digest.hexdigest(), "bytes": info.st_size,
            "mode": stat.S_IMODE(info.st_mode), "mtime_ns": info.st_mtime_ns}


def guard(paths):
    return {path: fingerprint(path) for path in paths}


def unchanged(before):
    return before == guard(before)


def manifest(before):
    return [{"path": str(path), **value} for path, value in sorted(before.items(), key=lambda row: str(row[0]))]


def invoke(command, proof, label, seconds, children):
    command = list(map(str, command))
    (proof / (label + ".command.json")).write_text(json.dumps(command, indent=2) + "\n")
    environment = os.environ.copy()
    # The ordinary fixture selects its own headless SDL hints.
    environment.pop("SDL_VIDEODRIVER", None)
    environment.pop("SDL_RENDER_DRIVER", None)
    begin = time.monotonic()
    record = {"label": label, "timeout_seconds": seconds}
    children.append(record)
    try:
        result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True,
                                text=True, timeout=seconds, check=False)
    except subprocess.TimeoutExpired as error:
        record.update({"elapsed_seconds": time.monotonic() - begin, "timed_out": True})
        for name, content in (("stdout", error.stdout), ("stderr", error.stderr)):
            if isinstance(content, bytes):
                content = content.decode(errors="replace")
            (proof / (label + "." + name + ".log")).write_text(content or "")
        (proof / (label + ".process.json")).write_text(json.dumps(record, indent=2) + "\n")
        raise
    record.update({"elapsed_seconds": time.monotonic() - begin, "timed_out": False,
                   "exit_code": result.returncode})
    (proof / (label + ".stdout.log")).write_text(result.stdout)
    (proof / (label + ".stderr.log")).write_text(result.stderr)
    (proof / (label + ".process.json")).write_text(json.dumps(record, indent=2) + "\n")
    return result


def generation(document, source):
    check(document["format"] == "openemperor-sandbox-save" and document["schema_version"] == SCHEMA and
          document["rules"]["version"] == RULE and document["rules"]["id"] == "sandbox-city-v16" and
          document["rules"]["agriculture_reconfiguration"] == "authored-agriculture-reconfiguration-v1" and
          document["rules"]["irrigated_harvest"] == "authored-irrigated-harvest-v1" and
          document["rules"]["irrigated_harvest_revision"] == 1 and
          document["profiles"]["map_permissions"]["version"] == 3 and
          document["map"]["relative_path"] == SOURCE_MAP and
          document["map"]["sha256"] == source["map_sha256"] and
          document["world"]["original_economy"]["model_source"]["sha256"] == source["model_sha256"],
          "Creator did not preserve the supplied Rule37/policy3/schema53 identities")
    if RULE == 38:
        check(document["rules"]["quality_aware_food_dispatch"] == "authored-quality-aware-food-dispatch-v1" and
              document["rules"]["quality_aware_food_dispatch_revision"] == 1,
              "Schema54 lost the exact separately selected quality-aware dispatch marker/revision")


def actual_returns(world):
    bodies = {row["id"]: row for row in world["buildings"]}
    couriers = {row["id"]: row for row in world["couriers"]}
    result = {kind: {"kind": kind, "returned": 0, "bindings": []} for kind in KINDS}

    def add(kind, source, leaf, amount, receipt):
        check(source in bodies and leaf in bodies and bodies[source]["placed"] and bodies[leaf]["placed"],
              "Crop return names an absent or retired paid parent/leaf")
        check(isinstance(amount, int) and amount >= 0 and (receipt is None) == (amount == 0),
              "Crop return counter and actual last receipt disagree")
        if receipt:
            check(receipt["grower_id"] > 0 and receipt["courier_id"] in couriers and
                  couriers[receipt["courier_id"]]["owner"] == source and
                  0 <= receipt["dispatched_tick"] <= receipt["returned_tick"] <= world["ticks"],
                  "Physical source receipt lost its actual worker/parent/clock")
            if kind in KINDS[:5]:
                check(receipt["crop"] == KINDS.index(kind) and
                      receipt["dispatched_tick"] <= receipt["arrived_tick"] <
                      receipt["harvested_tick"] < receipt["returned_tick"] and
                      0 < receipt["ready_tick"] < receipt["harvested_tick"],
                      "Food return lacks its earned at-field/harvest/physical-return epochs")
            else:
                check(receipt["ready_tick"] <= receipt["picked_tick"] < receipt["returned_tick"] and
                      receipt["dispatched_tick"] <= receipt["picked_tick"],
                      "Material return lacks actual readiness/pickup/return epochs")
            if kind == "hemp":
                check(receipt["field_id"] == leaf, "Hemp receipt changed the paid leaf identity")
        result[kind]["returned"] += amount
        result[kind]["bindings"].append({"source_id": source, "leaf_id": leaf,
                                        "returned": amount, "receipt": receipt})

    fields = {row["field_id"]: row for row in world["processed_food"]["fields"]}
    physical = {row["field_id"]: row for row in world["physical_food_growers"]["fields"]}
    check(fields.keys() == physical.keys(), "Food accounting and physical-return field sets differ")
    for leaf, row in fields.items():
        crop = row["crop"]
        check(0 <= crop < 5 and bodies[leaf]["kind"] == (17, 18, 42, 43, 44)[crop] and
              bodies[row["farm_id"]]["kind"] == 16 and physical[leaf]["farm_id"] == row["farm_id"],
              "Food crop or paid Farm/leaf attachment changed")
        add(KINDS[crop], row["farm_id"], leaf, physical[leaf]["returned"], physical[leaf]["last_receipt"])

    hemp = {row["farm_id"]: row for row in world["seasonal_agriculture"]["hemp_farms"]}
    for row in world["original_production"]["hemp_farms"]:
        source, leaf = row["farm_id"], row["field_id"]
        check(source in hemp and bodies[leaf]["kind"] == 15 and bodies[source]["kind"] == 14,
              "Hemp returned counter lost its typed paid source/leaf")
        add("hemp", source, leaf, row["harvested"], hemp[source]["last_receipt"])

    for row in world["plantations"]["plants"]:
        source, leaf = row["source_id"], row["plant_id"]
        leaf_kind = bodies[leaf]["kind"]
        check(leaf_kind in (36, 37, 38) and bodies[source]["kind"] == leaf_kind - 3,
              "Plantation return lost its typed source/plant attachment")
        add(("tea", "lacquer", "mulberry")[leaf_kind - 36], source, leaf, row["returned"], row["last_receipt"])
    for row in result.values():
        row["bindings"].sort(key=lambda entry: (entry["source_id"], entry["leaf_id"]))
        check(row["returned"] > 0 and row["bindings"], "Paid creator did not physically return " + row["kind"])
    return result


def unique_rows(rows, key, limit):
    check(isinstance(rows, list) and len(rows) <= limit, "Unbounded saved DTO rows: " + key)
    result = {row[key]: row for row in rows}
    check(len(result) == len(rows) and all(type(value) is int and value > 0 for value in result),
          "Duplicate or invalid saved DTO identity: " + key)
    return result


def audit_recovery(output, source, report, selected):
    proof = report["checkpoint_recovery"]
    check(proof["contract"] == "qa-event-checkpoint-recovery-v1" and proof["history_count"] == 1 and
          proof["checkpoint_limit"] == 32, "Creator changed the ordinary selected-event Recovery proof")
    events = proof["verified_events"]
    check(isinstance(events, list) and len(events) == len(selected) and
          {row["label"] for row in events} == selected.keys(), "Recovery lost an actual selected event")
    root = output / "checkpoint-recovery/recovery"
    check(root.is_dir() and not root.is_symlink() and not root.parent.is_symlink(),
          "Recovery directory is missing or symlinked")
    histories = list(root.iterdir())
    check(len(histories) == 1 and histories[0].is_dir() and not histories[0].is_symlink(),
          "Require one actual bounded Recovery history")
    history = histories[0]
    check((history / "history.json").stat().st_size <= 1024 * 1024, "Recovery metadata exceeded its unchanged bound")
    metadata = read(history / "history.json")
    entries = metadata["checkpoints"]
    check(metadata["version"] == 1 and metadata["history_id"] == history.name and metadata["parent"] is None and
          len(entries) == len(selected) + 1 <= 6 and metadata["next_sequence"] == len(entries),
          "Recovery history lost its actual start/sequence/retention contract")
    by_sequence = {row["sequence"]: row for row in entries}
    check(set(by_sequence) == set(range(len(entries))) and len(by_sequence) == len(entries),
          "Recovery checkpoint sequence is incomplete or duplicated")
    check(by_sequence[0]["kind"] == "start" and by_sequence[0]["filename"] == "start.json",
          "Recovery lost its actual early start point")
    start, early = by_sequence[0], selected["earned-early-harvest"][1]
    check(start["rule_version"] == RULE and start["schema"] == SCHEMA and start["profile"] == "sandbox-city-v16" and
          start["map"] == SOURCE_MAP and start["tick"] == early["world"]["ticks"] and
          start["treasury"] == early["world"]["treasury"] and
          start["population"] == sum(row["house"]["occupants"] for row in early["world"]["original_economy"]["houses"]),
          "Recovery start metadata differs from the actual early manual saved DTO")
    generation(read(history / "start.json"), source)
    check((history / "start.json").read_bytes() == (output / "early-harvest.save.json").read_bytes(),
          "Recovery start payload differs from the actual early manual checkpoint")
    event_sequences = set()
    for event in events:
        manual, document = selected[event["label"]]
        sequence, filename = event["sequence"], event["filename"]
        check(type(sequence) is int and sequence > 0 and sequence in by_sequence and sequence not in event_sequences and
              isinstance(filename, str) and len(filename) <= 128 and Path(filename).name == filename and
              filename.startswith("checkpoint-") and filename.endswith(".json") and
              event["schema"] == SCHEMA and event["tick"] == document["world"]["ticks"] and
              event["canonical_payload_exact"] is True and event["fresh_restore_exact"] is True,
              "Recovery selected event lost its exact generation, clock or publication proof")
        event_sequences.add(sequence)
        entry = by_sequence[sequence]
        check(entry["kind"] == "autosave" and entry["filename"] == filename and entry["rule_version"] == RULE and
              entry["schema"] == SCHEMA and entry["profile"] == "sandbox-city-v16" and entry["map"] == SOURCE_MAP and
              entry["tick"] == document["world"]["ticks"] and entry["treasury"] == document["world"]["treasury"] and
              entry["population"] == sum(row["house"]["occupants"] for row in document["world"]["original_economy"]["houses"]),
              "Recovery metadata differs from the selected actual saved DTO")
        payload = history / filename
        generation(read(payload), source)
        check(payload.read_bytes() == manual.read_bytes(),
              "Real Rule37/schema53 Recovery payload is not byte-exact to its manual save")
    check(event_sequences == set(by_sequence) - {0}, "Recovery retained an unreported selected checkpoint")
    return {"selected_events": len(events), "history_count": 1, "schema": SCHEMA,
            "actual_payloads_equal_manual": True}


def audit_early_harvest(output, source, report, plan, seed):
    early_path, return_path = output / "early-harvest.save.json", output / "early-return.save.json"
    early, returned, observed = read(early_path), read(return_path), read(output / "early-harvest.json")
    generation(early, source)
    generation(returned, source)
    check(early["map"] == seed["map"] == returned["map"] and early["rules"] == seed["rules"] == returned["rules"] and
          early["profiles"] == seed["profiles"] == returned["profiles"], "Early checkpoints changed exact source/generation")
    check(observed == report["early_harvest"] and observed["format"] == "openemperor-paid-early-irrigated-harvest-v1" and
          (observed["rule"], observed["schema"]) == (RULE, SCHEMA) and observed["tick"] == early["world"]["ticks"],
          "Early JSON/report lost its exact actual checkpoint binding")
    world, witness = early["world"], observed["actual"]
    bodies = unique_rows(world["buildings"], "id", 82)
    fields = unique_rows(world["processed_food"]["fields"], "field_id", 32)
    multi = unique_rows(world["multi_field_agriculture"]["fields"], "field_id", 32)
    irrigation = unique_rows(world["irrigation_productivity"]["work"], "building_id", 82)
    seasonal = unique_rows(world["seasonal_agriculture"]["fields"], "field_id", 32)
    physical = unique_rows(world["physical_food_growers"]["fields"], "field_id", 32)
    workers = unique_rows(world["physical_food_growers"]["farms"], "farm_id", 5)
    couriers = unique_rows(world["couriers"], "id", 58)
    leaf, owner = witness["field_id"], witness["farm_id"]
    check(leaf in fields and leaf in multi and leaf in irrigation and leaf in seasonal and leaf in physical and
          owner in workers and leaf in bodies and owner in bodies, "Early DTO lost a paid field/source/work attachment")
    field, body, farm, worker = fields[leaf], bodies[leaf], bodies[owner], workers[owner]
    crop, trip, courier = field["crop"], worker["latest_trip"], couriers[worker["courier_id"]]
    check(0 <= crop < 5 and body["kind"] == (17, 18, 42, 43, 44)[crop] and farm["kind"] == 16 and
          body["placed"] is True and farm["placed"] is True and field["farm_id"] == owner and
          multi[leaf]["farm_id"] == seasonal[leaf]["farm_id"] == physical[leaf]["farm_id"] == owner and
          courier["owner"] == owner and courier["role"] == 30,
          "Early harvest does not belong to its actual paid Food Farm/crop/Grower")
    age = world["ticks"] - body["placed_tick"]
    check(0 <= farm["placed_tick"] <= body["placed_tick"] < world["ticks"] and 0 < age < 80 and
          field["harvested"] == 1 and physical[leaf]["returned"] == 0 and physical[leaf]["last_receipt"] is None,
          "Early witness is not a genuinely new, still-unreturned paid harvest below80 ticks")
    base, wet, bonus, clipped = (multi[leaf]["productive_ticks"], irrigation[leaf]["wet_work_ticks"],
                                  irrigation[leaf]["bonus_work"], irrigation[leaf]["clipped_bonus"])
    waste, harvest = seasonal[leaf], field["harvested"]
    check(all(type(value) is int and value >= 0 for value in (base, wet, bonus, clipped, waste["wasted_work"])) and
          0 < base <= age and wet <= base and bonus > 0 and wet // 4 == bonus + clipped and
          base + bonus - waste["wasted_work"] == 80 * harvest,
          "Actual early harvest lacks the exact earned base+bonus-waste or wet/4 conservation equation")
    check(waste["harvest_work"] == 5 and waste["harvest_progress"] == 0 and waste["wasted_harvest_work"] == 0 and
          waste["ready_tick"] is None and trip["phase"] == 4 and trip["field_id"] == leaf and trip["crop"] == crop and
          trip["returned_tick"] is None and trip["delivered"] == 0 and trip["cancel_requested"] is False and
          trip["wait_left"] == 0 and trip["work_before"] + trip["work_ticks"] == base and
          trip["irrigation_bonus_before"] <= bonus and trip["wasted_work_before"] == waste["wasted_work"] and
          0 < trip["dispatched_tick"] <= trip["arrived_tick"] < trip["ready_tick"] <
          trip["harvested_tick"] == world["ticks"] and
          trip["harvested_tick"] - trip["ready_tick"] >= 5 and courier["phase"] == 2,
          "Early physical Trip lacks five earned harvest work ticks, actual Returning cargo or exact completed-tick epochs")
    stocks = unique_rows(world["processed_food"]["stocks"], "building_id", 82)
    check(owner in stocks and stocks[owner]["amounts"][crop] == 0,
          "Early pending harvest was prematurely credited to the source Farm crop stock")
    expected = {"field_id": leaf, "farm_id": owner, "crop": crop, "cell": body["cell"],
                "placed_tick": body["placed_tick"], "age": age, "harvested": harvest,
                "base_productive_ticks": base, "irrigation": {key: irrigation[leaf][key] for key in
                ("wet_work_ticks", "bonus_work", "clipped_bonus")},
                "seasonal": {key: waste[key] for key in ("wasted_work", "expired_ready", "harvest_work", "harvest_progress",
                                                       "wasted_harvest_work", "ready_tick")}, "physical_returned": 0,
                "latest_trip": {key: trip[key] for key in ("grower_id", "field_id", "phase", "dispatched_tick", "arrived_tick",
                    "ready_tick", "harvested_tick", "returned_tick", "work_before", "work_ticks", "irrigation_bonus_before",
                    "wasted_work_before", "wait_left", "delivered")}}
    expected["latest_trip"].update({"courier_id": worker["courier_id"], "physical_cargo": 1})
    # Current staffing is a runtime observation, not serialized new authority.
    # Bind its saved operation/fire inputs; the earned DTO equations above are
    # independently checked without substituting a claimed staff/callback count.
    staff = witness["actual_staff"]
    check(set(staff) == {"assigned", "required", "staffed", "operating", "fire_until_tick"} and
          type(staff["required"]) is int and type(staff["assigned"]) is int and
          0 < staff["required"] == staff["assigned"] and staff["staffed"] is True and
          staff["operating"] == farm["operating_enabled"] is True and
          staff["fire_until_tick"] == farm["fire_until_tick"] <= world["ticks"],
          "Early runtime staff witness disagrees with actual operation/fire inputs")
    expected["actual_staff"] = staff
    check(witness == expected, "Early JSON contains counters/IDs/epochs different from its canonical actual saved DTO")
    for document in (early, returned):
        tick = document["world"]["ticks"]
        check(document["world"]["command_sequence"] == sum(event["tick"] < tick for event in plan["events"]),
              "Early checkpoint does not contain the exact prefix of actually staged paid commands")
    actual_return = unique_rows(returned["world"]["physical_food_growers"]["fields"], "field_id", 32)[leaf]
    receipt = actual_return["last_receipt"]
    check(actual_return["farm_id"] == owner and actual_return["returned"] == observed["first_source_return_count"] == 1 and
          receipt == observed["first_source_return_receipt"] and receipt["grower_id"] == trip["grower_id"] and
          receipt["courier_id"] == worker["courier_id"] and receipt["crop"] == crop and
          all(receipt[key] == trip[key] for key in ("dispatched_tick", "arrived_tick", "ready_tick", "harvested_tick")) and
          receipt["work_at_harvest"] == base and receipt["irrigation_bonus_at_harvest"] == bonus and
          receipt["wasted_work_at_harvest"] == waste["wasted_work"] and
          trip["harvested_tick"] < receipt["returned_tick"] == observed["first_source_return_tick"] == returned["world"]["ticks"] and
          observed["manual_save_return_continuation_exact"] is True and
          type(observed["manual_save_continuation_ticks"]) is int and observed["manual_save_continuation_ticks"] >= 64 and
          receipt["returned_tick"] <= world["ticks"] + observed["manual_save_continuation_ticks"] <= seed["world"]["ticks"],
          "Early save did not continue at least64 actual ticks through this same Grower's physically saved source return")
    recovery = audit_recovery(output, source, report, {"earned-early-harvest": (early_path, early),
        "earned-early-source-return": (return_path, returned), "paid-seed": (output / "seed.save.json", seed)})
    return {"tick": world["ticks"], "field_id": leaf, "farm_id": owner, "age": age,
            "base": base, "wet": wet, "bonus": bonus, "clipped": clipped, "waste": waste["wasted_work"],
            "harvested": harvest, "physical_cargo": 1, "source_stock_before_return": 0,
            "first_source_return_tick": receipt["returned_tick"], "first_source_return_receipt": receipt,
            "manual_continuation_ticks": observed["manual_save_continuation_ticks"], "recovery": recovery}


def audit_creator(output, source, seed_ticks, separate):
    initial, seed = read(output / "initial.save.json"), read(output / "seed.save.json")
    plan, report = read(output / "player-plan.json"), read(output / "report.json")
    generation(initial, source)
    generation(seed, source)
    start, world = initial["world"], seed["world"]
    check(start["ticks"] == start["command_sequence"] == start["construction_spent_total"] == 0 and
          start["treasury"] == 12000 and not start["buildings"] and not start["couriers"] and
          not start["processed_food"]["stocks"] and not start["processed_food"]["trips"] and
          not start["processed_food"]["fields"] and not start["original_economy"]["houses"] and
          not start["original_goods"]["stocks"] and not start["original_goods"]["cargo"] and
          not start["original_goods"]["reservations"] and
          not start["original_production"]["industry"]["auxiliary"] and
          not start["original_production"]["hemp_farms"] and not start["plantations"]["plants"] and
          not start["fishing"]["quays"] and not start["hunting"]["tents"] and
          start["food_produced_total"] == start["total_produced"] == start["courier_cargo"] ==
          start["warehouse_stock"] == start["workshop_stock"] == start["clay_extracted_total"] ==
          start["pottery_completed_total"] == start["taxes_collected_total"] == start["maintenance_spent_total"] == 0 and
          all(row["produced"] == row["consumed"] == row["imported"] == 0
              for row in start["original_goods"]["totals"]) and
          start["original_production"]["residents_admitted"] == 0,
          "Creator start is not a genuinely empty, unearned World at tick0")
    check(world["ticks"] == seed_ticks and initial["rules"] == seed["rules"] and
          initial["map"] == seed["map"] and initial["profiles"] == seed["profiles"],
          "Creator changed its selected generation/source or seed endpoint")
    check(report["format"] == CREATOR_FORMAT and report["status"] == "PASS" and
          (report["rule"], report["policy"], report["schema"]) == (RULE, 3, SCHEMA) and
          report["contract"] == CONTRACT and report["contract_revision"] == 1 and
          report["model_sha256"] == source["model_sha256"] and report["map_sha256"] == source["map_sha256"] and
          report["map"] == SOURCE_MAP and report["seed_tick"] == seed_ticks and
          report["construction_complete"] is True and
          report["source_bytes_mode_mtime_unchanged"] is True and
          report["snapshot_stock_resident_level_or_arrival_injection"] is False and
          report["separate_process_replay_exact"] is separate,
          "Creator did not earn its full, unchanged paid construction contract")
    check(plan["format"] == PLAN_FORMAT and (plan["rule"], plan["policy"], plan["schema"]) == (RULE, 3, SCHEMA) and
          plan["map"] == SOURCE_MAP and plan["initial_tick"] == 0 and plan["end_tick"] == seed_ticks,
          "Paid command plan changed its exact source/generation/tick contract")
    events = plan["events"]
    check(events and len(events) <= 4096 and world["command_sequence"] == len(events) and
          all(0 <= event["tick"] < seed_ticks for event in events) and
          all(a["tick"] <= b["tick"] for a, b in zip(events, events[1:])) and
          any(event["tick"] > 0 for event in events),
          "Plan omitted actual successful commands or their real staged execution ticks")
    for event in events:
        command = event["command"]
        check(set(command) == {"type", "cell", "owner", "orientation", "recipe_grade", "operating_enabled", "workforce_priority"} and
              isinstance(command["type"], int) and command["type"] >= 0 and
              isinstance(command["owner"], int) and command["owner"] >= 0 and
              len(command["cell"]) == 2 and all(isinstance(value, int) for value in command["cell"]) and
              isinstance(command["operating_enabled"], bool), "Incomplete actual World command record")
    construction_tick = report["construction_complete_tick"]
    check(max(event["tick"] for event in events) <= construction_tick <= seed_ticks and
          all(body["placed_tick"] <= construction_tick for body in world["buildings"]) and
          0 <= report["minimum_funds"] <= min(start["treasury"], world["treasury"]) and
          world["construction_spent_total"] > 0 and
          world["treasury"] == start["treasury"] - world["construction_spent_total"] +
          world["taxes_collected_total"] - world["maintenance_spent_total"],
          "Construction clock or earned Funds/construction/tax/upkeep accounting differs")
    actual = actual_returns(world)
    recorded = {row["kind"]: row for row in report["source_returns"]}
    check(len(recorded) == len(report["source_returns"]) == 9 and recorded.keys() == actual.keys(),
          "Creator source-return report lost one of nine exact kinds")
    for kind, row in actual.items():
        reported = dict(recorded[kind])
        reported["bindings"] = sorted(reported["bindings"], key=lambda entry: (entry["source_id"], entry["leaf_id"]))
        check(reported == row, "Reported source-return count/IDs/receipt disagree with the actual save: " + kind)
    fish = sum(row["unloaded"] for row in world["fishing"]["quays"])
    meat = sum(row["unloaded"] for row in world["hunting"]["tents"])
    channels = len(world["couriers"]) + len(world["fishing"]["quays"]) + len(world["hunting"]["tents"]) + 3 * len(world["hunting"]["habitats"])
    check(fish == report["fish_unloaded"] > 0 and meat == report["meat_unloaded"] > 0 and
          len(world["buildings"]) <= 82 and channels <= 58,
          "Creator did not earn physical Fish/Meat unloads within unchanged82/58 limits")
    for filename in ("initial.save.json", "seed.save.json", "player-plan.json", "early-harvest.save.json",
                     "early-harvest.json", "early-return.save.json"):
        check(fingerprint(output / filename)["sha256"] == report["artifacts_sha256"][filename],
              "Creator SHA binding differs from the actual canonical artifact: " + filename)
    early = audit_early_harvest(output, source, report, plan, seed)
    return report, {"source_returns": actual, "fish_unloaded": fish, "meat_unloaded": meat,
                    "bodies": len(world["buildings"]), "channels": channels,
                    "paid_commands": len(events), "construction_complete_tick": construction_tick,
                    "minimum_funds": report["minimum_funds"], "seed_tick": seed_ticks, "early_harvest": early}


def artifact(output, name):
    path = Path(name)
    check(isinstance(name, str) and path.parts and not path.is_absolute() and
          all(part not in (".", "..") for part in path.parts) and str(path) == name,
          "Whole-city artifact must have a canonical relative filename")
    target = output / path
    check(not any(parent.is_symlink() for parent in (target, *target.parents)),
          "Whole-city artifact traverses a symlink")
    return target


def tree_guard(directory):
    paths = list(directory.rglob("*"))
    check(not any(path.is_symlink() for path in paths), "Evidence tree contains a symlink")
    return guard(path for path in paths if path.is_file())


def house_counts(world):
    bodies = unique_rows(world["buildings"], "id", 82)
    food, goods = [0] * 12, [0] * 6
    for row in world["processed_food"]["stocks"]:
        check(row["building_id"] in bodies, "Food row has no actual paid owner")
        if bodies[row["building_id"]]["kind"] in (6, 39, 40):
            check(len(row["consumed"]) == 12, "House Food counter changed its fixed width")
            food = [a + b for a, b in zip(food, row["consumed"])]
    housing = unique_rows(world["original_production"]["housing"]["houses"], "building_id", 16)
    rows = unique_rows(world["original_production"]["house_consumption"], "building_id", 16)
    check(rows.keys() == housing.keys(), "House goods sink and retained per-type histories disagree")
    for identity, row in rows.items():
        check(identity in bodies and bodies[identity]["kind"] in (6, 39, 40) and len(row["consumed"]) == 6,
              "Goods debit lacks its actual paid House or fixed six-kind ledger")
        retained = [sum(entry["consumed"][index] for entry in housing[identity]["consumption_rows"])
                    for index in range(6)]
        check(row["consumed"] == retained, "Actual House goods debits differ from immutable per-type histories")
        goods = [a + b for a, b in zip(goods, row["consumed"])]
    check(all(type(value) is int and value >= 0 for value in food + goods), "House debit is not a nonnegative integer")
    return food, goods


def crop_counts(world):
    bodies = unique_rows(world["buildings"], "id", 82)
    fields = unique_rows(world["processed_food"]["fields"], "field_id", 32)
    result = [0] * 9
    for row in world["physical_food_growers"]["fields"]:
        field = fields[row["field_id"]]
        check(0 <= field["crop"] < 5 and field["farm_id"] == row["farm_id"],
              "Whole-city physical Food return lost its typed parent/crop")
        result[field["crop"]] += row["returned"]
    result[5] = sum(row["harvested"] for row in world["original_production"]["hemp_farms"])
    for row in world["plantations"]["plants"]:
        kind = bodies[row["plant_id"]]["kind"]
        check(36 <= kind <= 38, "Whole-city return names a foreign plantation body")
        result[6 + kind - 36] += row["returned"]
    return result


def difference(after, before):
    check(len(after) == len(before) and all(a >= b for a, b in zip(after, before)),
          "Lifetime actual counters decreased")
    return [a - b for a, b in zip(after, before)]


def edible_reserves(world):
    bodies = {row["id"]: row for row in world["buildings"]}
    edible, mill_raw = 0, 0
    for stock in world["processed_food"]["stocks"]:
        body = bodies[stock["building_id"]]
        if not body["placed"]:
            continue
        for index, amount in enumerate(stock["amounts"]):
            check(type(amount) is int and amount >= 0, "Physical reserve contains a negative/nonintegral amount")
            if body["kind"] == 48 and index < 7:
                mill_raw += amount
            else:
                edible += amount
    return edible, mill_raw


def installed_bindings(world, setup):
    bodies = {row["id"]: row for row in world["buildings"]}
    expected_kinds = {"Food Farm": 16, "Hemp source": 14, "Wood source": 27,
        "Silk processor": 32, "Lacquerware processor": 30, "Quay": 46, "Hunter Tent": 47,
        "Mill": 48, "FoodShop": 49, "Food Warehouse": 3, "Market": 9, "Pump": 45}
    objects = {row["name"]: row for row in setup["paid_objects"]}
    check(objects.keys() == expected_kinds.keys() and len(objects) == len(setup["paid_objects"]) and
          setup["missing"] == [], "Whole-city setup changed or omitted a required actual chain")
    for name, kind in expected_kinds.items():
        actual = sorted(row["id"] for row in bodies.values() if row["placed"] and row["kind"] == kind)
        check(actual and objects[name]["kind"] == kind and objects[name]["actual_ids"] == actual,
              "Installed chain report differs from actual paid body IDs: " + name)
    fields = [{"field": row["field_id"], "farm": row["farm_id"], "crop": row["crop"]}
              for row in world["processed_food"]["fields"] if bodies[row["field_id"]]["placed"]]
    check(setup["active_bound_fields"] == fields and {row["crop"] for row in fields} == set(range(5)) and
          any(sum(row["farm"] == field["farm"] for row in fields) >= 2 for field in fields),
          "Installed Food chains lack exact typed five-crop/multi-field bindings")
    plants = [{"plant": row["plant_id"], "source": row["source_id"], "kind": bodies[row["plant_id"]]["kind"] - 36}
              for row in world["plantations"]["plants"] if bodies[row["plant_id"]]["placed"]]
    check(setup["active_bound_plants"] == plants and {row["kind"] for row in plants} == {0, 1, 2},
          "Installed plantation chains differ from the actual saved source/plant IDs")
    check(setup["actual_hemp_binding"] is True and setup["actual_shop_market_binding"] is True and
          setup["actual_warehouse_food_row"] is True and
          setup["paid_ditch_count"] == len(world["irrigation"]["ditches"]) >= 21,
          "Whole-city setup lost the actual source/Shop/Warehouse/paid ditch bindings")
    actual_returns(world)  # Also checks all nine actual typed source/leaf/receipt identities.
    check(any(bodies[row["shop_id"]]["kind"] == 49 and bodies[row["market_id"]]["kind"] == 9 and
              row["market_id"] < row["shop_id"] for row in world["food_processing"]["shops"]) and
          any(bodies[row["building_id"]]["kind"] == 3 for row in world["processed_food"]["stocks"]),
          "An asserted Shop/Market or sole-ledger Warehouse binding was absent from the actual save")


def whole_recovery(output, source, whole, documents):
    proof = whole["checkpoint_recovery"]
    events = proof["verified_events"]
    check(proof["contract"] == "qa-event-checkpoint-recovery-v1" and proof["history_count"] == 1 and
          proof["checkpoint_limit"] == 32 and isinstance(events, list) and
          len(events) == len(documents) <= 32 and events and
          {row["label"] for row in events} == documents.keys(), "Whole-city Recovery lost an actual boundary publication")
    root = artifact(output, "whole-city/checkpoint-recovery/recovery")
    check(root.is_dir(), "Whole-city Recovery root changed")
    histories = list(root.iterdir())
    check(len(histories) == 1 and histories[0].is_dir() and not histories[0].is_symlink(),
          "Require one separate ordinary whole-city Recovery history")
    history = histories[0]
    metadata = read(history / "history.json")
    ledger_path = artifact(output, "whole-city/checkpoint-recovery/publication-ledger.json")
    check(ledger_path.is_file() and ledger_path.stat().st_size <= 1024 * 1024,
          "Require a bounded whole-only actual Recovery publication ledger")
    ledger = read(ledger_path)
    check(set(ledger) == {"contract", "publications"} and
          ledger["contract"] == "qa-event-recovery-publication-ledger-v1" and
          isinstance(ledger["publications"], list) and len(ledger["publications"]) == len(events) <= 32,
          "Whole-city Recovery ledger changed its exact bounded publication contract")
    by_sequence = {}
    for sequence, event in enumerate(events, 1):
        manual, document = documents[event["label"]]
        generation(document, source)
        check(type(event["sequence"]) is int and event["sequence"] == sequence and event["schema"] == SCHEMA and
              event["tick"] == document["world"]["ticks"] and
              event["canonical_payload_exact"] is True and event["fresh_restore_exact"] is True,
              "Whole-city boundary lacks an exact fresh Recovery publication")
        by_sequence[sequence] = (event, manual, document)

    def exact_entries(snapshot, through):
        check(set(snapshot) == {"version", "history_id", "parent", "next_sequence", "checkpoints"} and
              snapshot["version"] == 1 and snapshot["history_id"] == history.name and
              snapshot["parent"] is None and type(snapshot["next_sequence"]) is int and
              snapshot["next_sequence"] == through + 1 and isinstance(snapshot["checkpoints"], list),
              "Whole-city publication snapshot lost its actual history identity/sequence")
        wanted = [0] + list(range(max(1, through - 4), through + 1))
        entries = snapshot["checkpoints"]
        check([entry["sequence"] for entry in entries] == wanted and len(entries) <= 6,
              "Whole-city publication did not retain ordinary start plus last five payloads")
        for entry in entries:
            check(set(entry) == {"sequence", "filename", "kind", "rule_version", "schema", "profile", "map",
                                 "tick", "treasury", "population", "created_local"} and
                  type(entry["sequence"]) is int,
                  "Whole-city publication metadata changed its exact ordinary entry fields")
            sequence = entry["sequence"]
            event, manual, document = by_sequence[sequence or 1]
            world = document["world"]
            filename = entry["filename"]
            check(isinstance(filename, str) and len(filename) <= 128 and Path(filename).name == filename and
                  entry["kind"] == ("start" if sequence == 0 else "autosave") and
                  (filename == "start.json" if sequence == 0 else
                   filename == event["filename"] and re.fullmatch(r"checkpoint-" + str(sequence) + r"-[0-9]+\.json", filename)) and
                  entry["rule_version"] == RULE and entry["schema"] == SCHEMA and entry["profile"] == "sandbox-city-v16" and
                  entry["map"] == SOURCE_MAP and entry["tick"] == world["ticks"] and
                  entry["treasury"] == world["treasury"] and
                  entry["population"] == sum(row["house"]["occupants"] for row in world["original_economy"]["houses"]) and
                  isinstance(entry["created_local"], str) and 0 < len(entry["created_local"]) <= 64,
                  "Whole-city publication filename/metadata differs from its actual canonical event")
        return {entry["sequence"]: entry for entry in entries}

    published_entries = {}
    for sequence, (publication, event) in enumerate(zip(ledger["publications"], events), 1):
        manual, _ = documents[event["label"]]
        check(set(publication) == {"event", "canonical_payload_sha256", "history_metadata"} and
              publication["event"] == event and
              publication["canonical_payload_sha256"] == fingerprint(manual)["sha256"],
              "Whole-city ledger event/digest differs from the true report filename and canonical manual payload")
        entries = exact_entries(publication["history_metadata"], sequence)
        for number, entry in entries.items():
            check(number not in published_entries or entry == published_entries[number],
                  "Retained actual Recovery filename/timestamp/metadata changed between publication snapshots")
            published_entries[number] = entry
    check(ledger["publications"][-1]["history_metadata"] == metadata,
          "Final actual Recovery history differs from the final whole-city publication snapshot")
    retained = exact_entries(metadata, len(events))
    for sequence, entry in retained.items():
        _, manual, _ = by_sequence[sequence or 1]
        payload = history / entry["filename"]
        generation(read(payload), source)
        check(payload.read_bytes() == manual.read_bytes(), "Retained whole-city Recovery payload differs from its manual save")
    check({path.name for path in history.glob("*.json")} == {"history.json"} | {row["filename"] for row in retained.values()},
          "Whole-city Recovery retained an unlisted payload")
    return {"published_boundaries": len(events), "retained_payloads": len(retained),
            "all_publications_fresh_and_canonical_exact": True, "ordinary_rotation": "start plus last five"}


def audit_whole(output, source, seed_ticks, continuation, separate):
    whole = read(output / "whole-city.json")
    check(whole["format"] == WHOLE_FORMAT and whole["status"] == "PASS" and whole["technical_status"] == "PASS" and
          (whole["rule"], whole["policy"], whole["schema"]) == (RULE, 3, SCHEMA) and whole["map"] == SOURCE_MAP and
          whole["model_sha256"] == source["model_sha256"] and whole["map_sha256"] == source["map_sha256"] and
          whole["begin_tick"] == 0 and whole["seed_tick"] == seed_ticks and
          whole["continued_ticks"] == continuation and whole["end_tick"] == seed_ticks + continuation and
          whole["fresh_restored_world_continuation_exact"] is True and whole["separate_process_replay_exact"] is separate and
          whole["source_bytes_mode_mtime_unchanged"] is True and whole["stock_population_model_or_arrival_injection"] is False and
          whole["commands_after_seed"] == 0, "Whole-city proof lost its fresh0 explicit generation/authority/tick boundaries")
    for key, name in (("initial_sha256", "initial.save.json"), ("seed_sha256", "seed.save.json"),
                      ("player_plan_sha256", "player-plan.json"), ("final_sha256", "whole-city-final.save.json")):
        check(whole[key] == fingerprint(output / name)["sha256"], "Whole-city report is not bound to its actual artifact: " + name)
    assessment = whole["whole_city"]
    check(assessment["contract"] == WHOLE_CONTRACT and assessment["status"] == "PASS" and
          set(assessment["criteria"]) == CRITERIA and all(value is True for value in assessment["criteria"].values()) and
          assessment["failed_criteria"] == [] and assessment["begin_tick"] == 0 and
          assessment["observed_ticks"] == whole["end_tick"] and assessment["full_cycle_ticks"] == YEAR,
          "Sealed fresh-from-zero V2 did not earn the unchanged fourteen acceptance criteria")
    initial = read(output / "initial.save.json")["world"]
    origin = assessment["fresh_origin"]
    check(origin == {"tick": 0, "command_sequence": 0, "buildings": 0, "couriers": 0, "population": 0,
          "starting_funds": 12000, "taxes": 0, "construction": 0, "upkeep": 0,
          "food_origin": [0] * 12, "house_food_consumed": [0] * 12, "house_goods_consumed": [0] * 6,
          "pristine_world_snapshot_exact": True}, "Fresh V2 origin did not retain its exact empty unpaid World0 seal")
    identity = assessment["source_identity"]
    initial_document = read(output / "initial.save.json")
    model = initial["original_economy"]["model_source"]
    check(identity == {"model_sha256": source["model_sha256"], "model_byte_count": model["byte_count"],
          "difficulty": model["difficulty"], "source_map_sha256": source["map_sha256"],
          "map_permissions_sha256": initial_document["profiles"]["map_permissions"]["sha256"],
          "rule": RULE, "policy": 3, "schema": SCHEMA},
          "Fresh V2 immutable source identity differs from the actual canonical initial document")
    maturity = assessment["maturity_tick"]
    end = whole["end_tick"]
    check(type(maturity) is int and 0 < maturity <= end - 20400 and
          assessment["actual_post_joint_maturity_ticks"] == end - maturity,
          "Whole-city maturity is missing, late or not bound to the actual full timeline")
    expected = [("maturity", maturity)]
    expected += [("window-" + str(number), tick) for number, tick in
                 enumerate(range(maturity + YEAR, end + 1, YEAR), 1)]
    expected.append(("final", end))
    rows = whole["checkpoints"]
    check([(row["label"], row["tick"]) for row in rows] == expected,
          "Whole-city canonical corpus skipped or approximated maturity/a complete seasonal boundary/final")
    documents = {}
    seed_world = read(output / "seed.save.json")["world"]
    plan_events = read(output / "player-plan.json")["events"]
    for row in rows:
        label = row["label"]
        expected_file = "whole-city-final.save.json" if label == "final" else "whole-city/" + label + ".save.json"
        check(row["file"] == expected_file and row["roundtrip_file"] == "whole-city/" + label + ".roundtrip.json" and
              row["canonical_roundtrip_exact"] is True and row["fresh_restore_exact"] is True,
              "Whole-city checkpoint path or actual fresh-restore evidence changed")
        manual, roundtrip = artifact(output, row["file"]), artifact(output, row["roundtrip_file"])
        document = read(manual)
        generation(document, source)
        world = document["world"]
        check(world["ticks"] == row["tick"] and fingerprint(manual)["sha256"] == row["sha256"] ==
              fingerprint(roundtrip)["sha256"] == row["roundtrip_sha256"] and manual.read_bytes() == roundtrip.read_bytes(),
              "Whole-city checkpoint digest/epoch/full canonical bytes differ")
        food_totals(world)
        check(world["command_sequence"] == sum(event["tick"] < world["ticks"] for event in plan_events) and
              (world["ticks"] < seed_ticks or world["command_sequence"] == seed_world["command_sequence"] and
               world["construction_spent_total"] == seed_world["construction_spent_total"]),
              "Whole-city boundary changed the actual paid command prefix or executed a post-seed command")
        check(world["treasury"] == 12000 - world["construction_spent_total"] +
              world["taxes_collected_total"] - world["maintenance_spent_total"],
              "Whole-city actual Funds differ from paid construction/tax/upkeep accounting")
        documents[label] = (manual, document)
    mature = documents["maturity"][1]["world"]
    final = documents["final"][1]["world"]
    baseline_food, baseline_goods = house_counts(initial)
    mature_food, mature_goods = house_counts(mature)
    final_food, final_goods = house_counts(final)
    actual_food, actual_goods = difference(final_food, baseline_food), difference(final_goods, baseline_goods)
    check(assessment["whole_lifecycle_house_food_consumed"] == actual_food and
          assessment["whole_lifecycle_house_goods_consumed"] == actual_goods and
          assessment["maturity_house_food_consumed"] == mature_food and
          assessment["maturity_house_goods_consumed"] == mature_goods and
          assessment["maturity_actual_source_crop_returns"] == crop_counts(mature) and
          assessment["maturity_command_sequence"] == mature["command_sequence"],
          "Fresh V2 lifecycle or maturity counters differ from their actual canonical House/source DTOs")
    check(not any(baseline_food + baseline_goods) and all(value > 0 for value in mature_food[:7]) and
          any(value > 0 for value in mature_food[7:11]) and all(mature_goods[index] > 0 for index in LUXURIES),
          "Actual joint-maturity save lacks cumulative House seven-raw/meals/three-luxury debits from empty0 (including Meat)")
    first_food = assessment["first_actual_food_consumption_ticks"]
    first_goods = assessment["first_actual_luxury_consumption_ticks_by_goods_index"]
    check(len(first_food) == 12 and len(first_goods) == 6 and
          all(type(first_food[index]) is int and 0 < first_food[index] <= maturity for index in range(7)) and
          any(type(first_food[index]) is int and 0 < first_food[index] <= maturity for index in range(7, 11)) and
          first_food[11] is None and
          all(type(first_goods[index]) is int and 0 < first_goods[index] <= maturity for index in LUXURIES),
          "Fresh0 first actual House debit epochs are absent or contradict the canonical cumulative maturity counters")
    for index, tick in enumerate(first_food):
        check((tick is None) == (actual_food[index] == 0) and
              (tick is None or type(tick) is int and 0 < tick <= end and
               (mature_food[index] == 0 or tick <= maturity)),
              "First actual House Food epoch contradicts a canonical cumulative sink")
    crop_ticks = assessment["first_actual_source_crop_return_ticks"]
    check(len(crop_ticks) == 9 and all(type(tick) is int and 0 < tick <= maturity for tick in crop_ticks) and
          assessment["first_all_nine_source_returns_tick"] == max(crop_ticks) and
          all(first_goods[index] is None for index in range(6) if index not in LUXURIES) and
          first_food[6] <= seed_ticks and house_counts(seed_world)[0][6] > 0,
          "Fresh0 first nine-return/three-luxury/early raw Meat witnesses contradict the actual paid seed/maturity")
    installed_bindings(mature, assessment["installed_bindings"])
    check(assessment["new_actual_source_crop_returns"] == difference(crop_counts(final), crop_counts(initial)) and
          all(value > 0 for value in assessment["new_actual_source_crop_returns"]),
          "Whole-city source-return assessment differs from canonical actual nine-crop returns")
    windows = assessment["complete_post_maturity_windows"]
    check(len(windows) == len(expected) - 2 >= 2, "Whole-city assessment lost a complete actual annual window")
    previous = mature
    for number, window in enumerate(windows, 1):
        current = documents["window-" + str(number)][1]["world"]
        before_food, before_goods = house_counts(previous)
        after_food, after_goods = house_counts(current)
        food, goods = difference(after_food, before_food), difference(after_goods, before_goods)
        crops = difference(crop_counts(current), crop_counts(previous))
        check(window["begin_tick"] == previous["ticks"] and window["end_tick"] == current["ticks"] and
              current["ticks"] - previous["ticks"] == YEAR and window["new_house_food"] == food and
              window["new_house_goods"] == goods and window["actual_source_crop_returns"] == crops and
              window["all_nine_crop_returns_observed"] is all(value > 0 for value in crops) and
              window["renewed_raw_food"] is any(value > 0 for value in food[:7]) and
              window["renewed_meals"] is any(value > 0 for value in food[7:11]) and
              window["renewed_all_three_luxuries"] is all(goods[index] > 0 for index in LUXURIES) and
              all(window[key] is True for key in ("renewed_raw_food", "renewed_meals", "renewed_all_three_luxuries")),
              "Annual renewed House sinks/physical crop returns differ from their actual endpoint saves")
        previous = current
    check(assessment["minimum_funds"] >= 0 and assessment["minimum_funds"] <= min(12000, final["treasury"]) and
          assessment["funds_at_maturity"] == mature["treasury"] and
          assessment["post_maturity_taxes"] == final["taxes_collected_total"] - mature["taxes_collected_total"] and
          assessment["post_maturity_installed_upkeep"] == final["maintenance_spent_total"] - mature["maintenance_spent_total"] and
          assessment["post_maturity_taxes"] >= assessment["post_maturity_installed_upkeep"] and
          assessment["minimum_edible_physical_reserve_after_maturity"] > 0 and
          assessment["zero_edible_reserve_ticks_after_maturity"] == 0 and assessment["maximum_actual_wet_ditches"] >= 21,
          "Whole-city actual reserves/Funds/maturity cashflow/water gates disagree")
    for label, (_, document) in documents.items():
        edible, mill_raw = edible_reserves(document["world"])
        check(assessment["maximum_edible_physical_reserve"] >= edible and
              assessment["maximum_separate_mill_raw_inputs"] >= mill_raw and
              (label == "maturity" or edible >= assessment["minimum_edible_physical_reserve_after_maturity"]),
              "Whole-city reserve extrema contradict actual sole-ledger saved stock")
    homes = assessment["houses"]
    check(homes and any(row["mature_occupied"] for row in homes) and
          all(not row["mature_occupied"] or row["cohort_empty_ticks"] == 0 for row in homes) and
          any(row["mature_occupied"] and row["actual_new_luxury_consumer"] and
              row["cohort_empty_ticks"] == row["luxury_family_loss_ticks"] == 0 for row in homes) and
          sum(row["devolutions_with_actual_evaluation_food_deficit"] for row in homes) == 0 and
          assessment["retained_paid_progression_history"] == 0 and sum(row["new_evolutions"] for row in homes) > 0,
          "Whole-city paid cohort/luxury family/progression/actual Food-deficit devolution gates disagree")
    final_homes = {row["building_id"]: row for row in final["original_economy"]["houses"]}
    check({row["id"] for row in homes} == final_homes.keys() and
          all(row["final_type"] == final_homes[row["id"]]["house"]["type"] and
              (sum(entry["ticks"] for entry in row["post_maturity_type_ticks"]) == end - maturity
               if row["mature_occupied"] else
               sum(entry["ticks"] for entry in row["post_maturity_type_ticks"]) <= end - maturity)
              for row in homes), "Whole-city House family/type timeline differs from actual canonical final/cohort ticks")
    recovery = whole_recovery(output, source, whole, documents)
    return whole, {"contract": WHOLE_CONTRACT, "maturity_tick": maturity, "end_tick": end,
        "house_food_consumption_from_zero": actual_food, "house_goods_consumption_from_zero": actual_goods,
        "house_meat_at_joint_maturity": mature_food[6], "first_actual_house_meat_tick": first_food[6],
        "actual_post_maturity_ticks": end - maturity,
        "complete_seasonal_windows": len(windows), "all_fourteen_criteria_earned": True, "recovery": recovery}


def same_report_except_process_flag(original, repeated, original_output, repeated_output, source, whole=False):
    # Reports retain their actual random Recovery filenames. Only this local
    # comparison form replaces a filename after independently proving its exact
    # history/publication metadata, canonical payload and event identity.
    def bound_comparison(report, output):
        if whole:
            documents = {row["label"]: (artifact(output, row["file"]), read(artifact(output, row["file"])))
                         for row in report["checkpoints"]}
            whole_recovery(output, source, report, documents)
        else:
            names = {"earned-early-harvest": "early-harvest.save.json",
                     "earned-early-source-return": "early-return.save.json", "paid-seed": "seed.save.json"}
            documents = {label: (output / name, read(output / name)) for label, name in names.items()}
            audit_recovery(output, source, report, documents)
        result = copy.deepcopy(report)
        for event in result["checkpoint_recovery"]["verified_events"]:
            manual, _ = documents[event["label"]]
            event["filename"] = {"canonical_payload_sha256": fingerprint(manual)["sha256"],
                                 "event_identity": {"label": event["label"], "tick": event["tick"],
                                                    "sequence": event["sequence"], "schema": event["schema"],
                                                    "rule": RULE}}
        return result
    first = bound_comparison(original, original_output)
    second = bound_comparison(repeated, repeated_output)
    check(first.pop("separate_process_replay_exact") is False and
          second.pop("separate_process_replay_exact") is True and first == second,
          "Independent fresh0 replay changed report data beyond its process flag and actual metadata-bound Recovery filenames")


def slice_passed(output):
    assessment = read(output / "report.json")["complete_city"]
    check(set(assessment["criteria"]) == CRITERIA and
          all(type(value) is bool for value in assessment["criteria"].values()),
          "Restored SliceV1 changed its unchanged14-criterion contract")
    return all(assessment["criteria"].values())


def audit_observer(output, source, seed, end, strict, replay=False):
    expected_complete = slice_passed(output) if RULE == 38 and strict else False
    report = audit_report(output, expected_complete, RULE, SCHEMA) if strict else read(output / "report.json")
    check(report["format"] == ("openemperor-complete-city-acceptance-v1" if strict else
                               "openemperor-agriculture-integration-acceptance-v1") and
          report["status"] == ("PASS" if not strict or expected_complete else "PARTIAL") and
          (report["rule"], report["policy"], report["schema"]) == (RULE, 3, SCHEMA) and
          report["initial_tick"] == seed["world"]["ticks"] and report["final_tick"] == end and
          report["continued_real_ticks"] == 100000 and
          report["stock_population_model_or_arrival_injection"] is False and report["commands_after_restore"] == 0 and
          report["fresh_restored_world_continuation_exact"] is True and
          report["separate_process_replay_exact"] is replay and report["source_bytes_mode_mtime_unchanged"] is True and
          report["native_metal"] == report["human_acceptance"] == "NOT_RUN",
          "Restored observer changed its actual generation/timeline/technical or negative-control status")
    initial, final = read(output / "initial.save.json"), read(output / "final.save.json")
    generation(initial, source)
    generation(final, source)
    check(initial == seed, "Restored observer initial document differs from this actual paid20k seed")
    before_food, before_goods = house_counts(initial["world"])
    after_food, after_goods = house_counts(final["world"])
    food, goods = difference(after_food, before_food), difference(after_goods, before_goods)
    check(food == report["new_house_food_consumption"] and goods == report["new_house_goods_consumption"],
          "Restored continuation consumption differs from actual independent saved DTO counters")
    missing = ["No new House consumption of " + name + " during this continuation"
               for index, name in enumerate(("Wheat7", "Millet5", "Rice6", "Cabbage3", "BeanCurd1", "Fish2", "Meat4"))
               if food[index] == 0]
    if not any(food[7:11]):
        missing.append("No new House Dinners consumption during this continuation")
    missing += ["No new House luxury debit for Goods index " + str(index) +
                "; inspect actual current/next needs and paid source catalog" for index in LUXURIES if goods[index] == 0]
    check(report["missing_gameplay_witnesses"] == missing,
          "Restored continuation hid or invented an actual per-kind/meal/luxury deficit")
    if not strict:
        check(report["gameplay_coverage"] == ("PARTIAL" if missing else "PASS"),
              "Ordinary consumption coverage differs from actual saved counters")
    if RULE == 37:
        check(food[6] == 0 and report["gameplay_coverage"] == "PARTIAL" and
              "No new House consumption of Meat4 during this continuation" in missing,
              "Protected37 meals-first SliceV1 Meat0 negative control changed")
    check(len(report["checkpoints"]) == 12, "Restored observer omitted an ordinary persistence checkpoint")
    for row in report["checkpoints"]:
        name = row["file"]
        check(Path(name).name == name and name.endswith(".save.json") and row["canonical_roundtrip_exact"] is True and
              row["recovery_exact"] is True, "Observer checkpoint lost ordinary exact persistence proof")
        path = output / name
        document = read(path)
        generation(document, source)
        food_totals(document["world"])
        check(document["world"]["ticks"] == row["tick"] and
              path.read_bytes() == (output / name.replace(".save.json", ".roundtrip.json")).read_bytes(),
              "Observer canonical clock or full roundtrip bytes differ")
    return report


def main():
    global RULE, SCHEMA, CONTRACT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--creator-executable", type=Path, required=True)
    parser.add_argument("--observer-executable", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--seed-ticks", type=int, required=True)
    parser.add_argument("--quality-aware-food-dispatch", action="store_true")
    args = parser.parse_args()
    if args.quality_aware_food_dispatch:
        RULE, SCHEMA, CONTRACT = 38, 54, "authored-quality-aware-food-dispatch-v1"
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file() or not (args.data / SOURCE_MAP).is_file():
        print("SKIP: supplied Original Model/Chengdu data unavailable")
        return 77
    private = ROOT / ".local/agri-mac14"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="paid-complete-city-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    children, source_guard, creator_guard = [], {}, {}
    observer_root = proof / "observer"
    budgets = {"creator_child_seconds": CREATOR_SECONDS, "observer_child_seconds": OBSERVER_CHILD_SECONDS,
               "observer_wrapper_seconds": OBSERVER_WRAPPER_SECONDS, "total_ctest_seconds": TOTAL_SECONDS,
               "reason": "Two fresh0 full creators600s each plus three unchanged1800s restored observers and120s bounded audits"}
    try:
        check(args.seed_ticks == 20000, "This paid whole-city recipe requires its explicit20000-tick seed")
        data = args.data.resolve(strict=True)
        creator = args.creator_executable.resolve(strict=True)
        observer = args.observer_executable.resolve(strict=True)
        observer_script = ROOT / "tests/test_complete_city_integration.py"
        assessment_header = ROOT / "tests/CompleteCityAssessment.h"
        source_guard = guard((data / "Model/EmperorBuildingModels.txt", data / SOURCE_MAP,
                              creator, observer, observer_script, assessment_header, Path(__file__).resolve(),
                              ROOT / "tests/test_food_warehouses_replay.py", ROOT / "tests/test_food_processing_replay.py"))
        source = {"model_sha256": source_guard[data / "Model/EmperorBuildingModels.txt"]["sha256"],
                  "map_sha256": source_guard[data / SOURCE_MAP]["sha256"]}
        (proof / "inputs.json").write_text(json.dumps({"inputs": manifest(source_guard), "budgets": budgets}, indent=2) + "\n")
        created, repeated = proof / "creator", proof / "creator-replay"
        command = (creator, "--data", data, "--ticks", args.seed_ticks, "--whole-city-ticks", 100000)
        if args.quality_aware_food_dispatch:
            command += ("--quality-aware-food-dispatch",)
        result = invoke((*command, "--output", created), proof, "creator", CREATOR_SECONDS, children)
        check(result.returncode == 0, "Actual paid creator failed: " + result.stderr)
        report, counters = audit_creator(created, source, args.seed_ticks, False)
        whole, whole_counters = audit_whole(created, source, args.seed_ticks, 100000, False)
        creator_guard = tree_guard(created)
        result = invoke((*command, "--output", repeated, "--replay", created), proof,
                        "creator-replay", CREATOR_SECONDS, children)
        check(result.returncode == 0, "Independent fresh paid creator replay failed: " + result.stderr)
        replay_report, replay_counters = audit_creator(repeated, source, args.seed_ticks, True)
        replay_whole, replay_whole_counters = audit_whole(repeated, source, args.seed_ticks, 100000, True)
        check(counters == replay_counters and whole_counters == replay_whole_counters,
              "Independent fresh0 creator process changed actual paid/whole counters or receipt epochs")
        same_report_except_process_flag(report, replay_report, created, repeated, source)
        same_report_except_process_flag(whole, replay_whole, created, repeated, source, whole=True)
        for filename in ("initial.save.json", "seed.save.json", "player-plan.json", "early-harvest.save.json",
                         "early-harvest.json", "early-return.save.json"):
            check((created / filename).read_bytes() == (repeated / filename).read_bytes(),
                  "Fresh-process creator replay changed complete canonical bytes: " + filename)
        for row in whole["checkpoints"]:
            for name in (row["file"], row["roundtrip_file"]):
                check(artifact(created, name).read_bytes() == artifact(repeated, name).read_bytes(),
                      "Fresh0 full replay changed an entire canonical boundary/roundtrip: " + name)
        check(unchanged(source_guard) and unchanged(creator_guard), "Creator replay changed explicit inputs")
        replay_guard = tree_guard(repeated)
        seed = read(created / "seed.save.json")
        common = (observer, "--data", data, "--input", created / "seed.save.json",
                  "--quality-aware-food-dispatch" if args.quality_aware_food_dispatch else "--irrigated-harvest", "--ticks", 100000)
        ordinary, complete, replay = (observer_root / name for name in ("ordinary", "slice-v1", "slice-v1-replay"))
        result = invoke((*common, "--output", ordinary), proof, "observer-ordinary", OBSERVER_CHILD_SECONDS, children)
        check(result.returncode == 0, "Ordinary restored observer lost technical acceptance: " + result.stderr)
        ordinary_report = audit_observer(ordinary, source, seed, args.seed_ticks + 100000, False)
        result = invoke((*common, "--output", complete, "--complete-city"), proof,
                        "observer-slice-v1", OBSERVER_CHILD_SECONDS, children)
        complete_report = audit_observer(complete, source, seed, args.seed_ticks + 100000, True)
        check(result.returncode == (0 if complete_report["status"] == "PASS" else 2),
              "SliceV1 process exit contradicts its independently audited actual criteria: " + result.stderr)
        complete_guard = tree_guard(complete)
        result = invoke((*common, "--output", replay, "--complete-city", "--replay", complete), proof,
                        "observer-slice-v1-replay", OBSERVER_CHILD_SECONDS, children)
        repeated_report = audit_observer(replay, source, seed, args.seed_ticks + 100000, True, True)
        check(result.returncode == (0 if repeated_report["status"] == "PASS" else 2),
              "SliceV1 replay process exit contradicts the actual unchanged criteria: " + result.stderr)
        check(complete_report["complete_city"] == repeated_report["complete_city"],
              "Independent SliceV1 replay changed unchanged v1 criteria or its actual negative assessment")
        for row in ordinary_report["checkpoints"]:
            for name in (row["file"], row["file"].replace(".save.json", ".roundtrip.json")):
                check((ordinary / name).read_bytes() == (complete / name).read_bytes() == (replay / name).read_bytes(),
                      "Read-only SliceV1 QA changed ordinary canonical gameplay/persistence bytes: " + name)
        check(ordinary_report["checkpoints"] == complete_report["checkpoints"] == repeated_report["checkpoints"],
              "SliceV1 QA changed ordinary checkpoint epochs or exact persistence flags")
        ordinary_lines = (ordinary / "telemetry.jsonl").read_text().splitlines()
        complete_lines = (complete / "telemetry.jsonl").read_text().splitlines()
        check(len(ordinary_lines) == len(complete_lines), "SliceV1 QA changed ordinary telemetry epochs")
        for old, new in zip(ordinary_lines, complete_lines):
            old_row, new_row = json.loads(old), json.loads(new)
            check("complete_city" not in old_row and new_row.pop("complete_city") is not None and old_row == new_row,
                  "SliceV1 QA changed an existing ordinary observation field")
        check((complete / "telemetry.jsonl").read_bytes() == (replay / "telemetry.jsonl").read_bytes() and
              (created / "seed.save.json").read_bytes() == (ordinary / "initial.save.json").read_bytes() and
              (created / "whole-city-final.save.json").read_bytes() == (ordinary / "final.save.json").read_bytes() ==
              (complete / "final.save.json").read_bytes() == (replay / "final.save.json").read_bytes(),
              "Fresh0 full run and actual restored20k→120k ordinary/strict processes did not reach the identical complete canonical state")
        check(len(children) == 5 and unchanged(replay_guard) and unchanged(complete_guard),
              "Observer replay changed its creator evidence or exceeded the fixed five-child budget")
        check(unchanged(source_guard) and unchanged(creator_guard), "Final verification changed Original, executable, script or creator inputs")
        observer_evidence = tree_guard(observer_root)
        (proof / "result.json").write_text(json.dumps({"status": "PASS", "format": "openemperor-paid-complete-city-regression-v2",
            "creator_contract": CREATOR_FORMAT, "rule": RULE, "policy": 3, "schema": SCHEMA,
            "budgets": budgets, "children": children, "creator": str(created), "creator_replay": str(repeated),
            "observer_proof": str(observer_root), "actual_creator_counters": counters,
            "fresh_from_zero_assessment": whole_counters,
            "creator_initial_seed_plan_byte_exact": True, "creator_early_checkpoints_byte_exact": True,
            "early_earned_harvest_and_actual_source_return": True, "fresh_from_zero_v2_complete_city_earned": True,
            "fresh_from_zero_canonical_boundaries_and_independent_replay_exact": True,
            "ordinary_restored_final_equals_fresh_from_zero_final": True,
            "slice_v1_contract": complete_report["complete_city"]["contract"], "slice_v1_status": complete_report["status"],
            "slice_v1_failed_criteria": complete_report["complete_city"]["failed_criteria"],
            "slice_v1_new_house_meat_consumption": complete_report["new_house_food_consumption"][6],
            "slice_v1_negative_control_and_replay_exact": RULE == 37, "slice_v1_actual_diagnostic_and_replay_exact": True, "observer_real_ticks": 100000,
            "inputs_unchanged": True, "source_and_executables": manifest(source_guard),
            "creator_artifacts": manifest(creator_guard), "observer_artifacts": manifest(observer_evidence),
            "native_and_human_acceptance": "NOT_RUN"}, indent=2) + "\n")
        print(f"PASS: explicit Rule{RULE}/schema{SCHEMA} fresh0 V2 all14 criteria/replay; restored SliceV1 actual status={complete_report['status']} Meat4={complete_report['new_house_food_consumption'][6]}")
        return 0
    except Exception as error:
        checks = {}
        for label, before in (("source_inputs", source_guard), ("creator_inputs", creator_guard)):
            try:
                checks[label] = unchanged(before) if before else "NOT_CAPTURED"
            except Exception as guard_error:
                checks[label] = str(guard_error)
        (proof / "failure.json").write_text(json.dumps({"status": "FAIL", "reason": str(error),
            "traceback": traceback.format_exc(), "children": children, "budgets": budgets,
            "observer_proof": str(observer_root) if observer_root else None,
            "input_preservation": checks}, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
