#!/usr/bin/env python3
"""Explicit Rule37/schema53 or selected Rule38/schema54 paid event Save/Recovery and replay proof.

Ten existing600-second children: MAC10 command-plan creator; Visual37 ordinary
baseline, event creator, replay and two hostile plans; Luxury37 V2 creator and
replay; Reconfiguration37 creator and replay. CTest's6060-second outer budget
adds60 seconds for independent file audits. No historical timeout is relaxed.
Only MAC10 commands are reused: no schema51/52 snapshot is reinterpreted.
All attempts and logs are retained privately, including failures.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import shutil
import tempfile
import time

from test_agriculture_visual_replay import INPUTS
from test_agriculture_reconfiguration_replay import audit as audit_reconfiguration
from test_checkpoint_recovery_events import (
    ROOT, CONTRACT, require, read, digest, fingerprint, invoke,
    checkpoint_files, compare_reference,
)
from test_food_warehouses_replay import totals

HARVEST = "authored-irrigated-harvest-v1"
QUALITY = "authored-quality-aware-food-dispatch-v1"
RULE, SCHEMA = 37, 53
GENERATION_FLAG = "--irrigated-harvest"
VISUAL_LABELS = {
    "paid-city", "final", "food-grower-work-earned", "food-grower-loaded",
    "crop-Tea-growing", "boat-loaded-moving", "hunter-loaded-moving",
    "mill-processing", "warehouse-stock", "warehouse-outbound",
    "market-stock-receipt", "market-house-receipt", "shop-house-receipt",
    "pump-fire", "pump-extinguished", "pump-recovered-wet",
    "year-boundary-9792", "year-boundary-19584",
}
RECEIPTS = {"house-tea-receipt": 2, "house-silk-receipt": 3,
            "house-lacquerware-receipt": 5}


def leaf(directory, name):
    require(isinstance(name, str) and name and Path(name).name == name and
            name not in (".", ".."), "Artifact must use a leaf filename")
    path = directory / name
    require(path.is_file() and not path.is_symlink(), "Missing or symlinked artifact: " + str(path))
    return path


def guard(directory):
    paths = sorted(directory.rglob("*"))
    require(not any(path.is_symlink() for path in paths), "Creator/input tree contains a symlink")
    return {str(path.relative_to(directory)): fingerprint(path)
            for path in paths if path.is_file()}


def generation(document, sources):
    rules, world = document["rules"], document["world"]
    require(document["format"] == "openemperor-sandbox-save" and
            document["schema_version"] == SCHEMA and rules["version"] == RULE and
            rules["id"] == "sandbox-city-v16" and
            rules["irrigated_harvest"] == HARVEST and rules["irrigated_harvest_revision"] == 1 and
            rules["agriculture_reconfiguration"] == "authored-agriculture-reconfiguration-v1" and
            rules["agriculture_reconfiguration_revision"] == 1 and
            document["profiles"]["map_permissions"]["version"] == 3,
            "Checkpoint is not the exact explicit Rule37/policy3/schema53 generation")
    if RULE == 38:
        require(rules["quality_aware_food_dispatch"] == QUALITY and rules["quality_aware_food_dispatch_revision"] == 1,
                "Schema54 lost its exact separate quality-aware contract")
    relative = document["map"]["relative_path"]
    require(relative in ("Cities/Chengdu.map", "Cities/Xia.map") and
            document["map"]["sha256"] == sources[relative][0] and
            world["original_economy"]["model_source"]["sha256"] ==
            sources["Model/EmperorBuildingModels.txt"][0],
            "Checkpoint lost supplied map/model identities")
    totals(world)
    return world


def checkpoints(directory, report, sources):
    rows = {row["label"]: row for row in report["checkpoints"]}
    require(rows and len(rows) == len(report["checkpoints"]), "Duplicate/empty checkpoint manifest")
    result = {}
    for label, row in rows.items():
        path = leaf(directory, row["file"])
        require(row["file"] == label + ".save.json" and
                path.read_bytes() == leaf(directory, label + ".roundtrip.json").read_bytes(),
                "Canonical manual/roundtrip bytes differ")
        require(row.get("exact_roundtrip", True) is True and
                ("sha256" not in row or row["sha256"] == digest(path)),
                "Checkpoint claimed a false roundtrip/digest")
        world = generation(read(path), sources)
        require(row["tick"] == world["ticks"], "Checkpoint manifest changed the actual World epoch")
        result[label] = world
    return rows, result


def history(directory, count, manual, sources, initial):
    require(not directory.is_symlink() and not (directory / "recovery").is_symlink(),
            "Recovery roots must remain ordinary private directories")
    roots = list(directory.glob("recovery/history-*/history.json"))
    require(len(roots) == 1 and not roots[0].is_symlink(), "Expected one ordinary Recovery history")
    path = roots[0]
    require(not path.parent.is_symlink(), "Recovery history directory is symlinked")
    metadata = read(path)
    require(metadata["version"] == 1 and metadata["history_id"] == path.parent.name and
            metadata["parent"] is None and metadata["next_sequence"] == count + 1,
            "Recovery metadata history/sequence mismatch")
    retained = metadata["checkpoints"]
    expected = {0} | set(range(max(1, count - 4), count + 1))
    require(len(retained) == len(expected) and
            {row["sequence"] for row in retained} == expected,
            "Recovery did not retain start plus its ordinary five-checkpoint suffix")
    require({payload.name for payload in path.parent.glob("*.json")} ==
            {"history.json"} | {entry["filename"] for entry in retained},
            "Recovery retained unlisted or missing canonical payloads")
    for entry in retained:
        payload = leaf(path.parent, entry["filename"])
        document = read(payload)
        world = generation(document, sources)
        sequence = entry["sequence"]
        require(entry["schema"] == SCHEMA and entry["rule_version"] == RULE and
                entry["profile"] == "sandbox-city-v16" and
                entry["map"] == document["map"]["relative_path"] and entry["tick"] == world["ticks"] and
                entry["treasury"] == world["treasury"] and
                entry["population"] == sum(row["house"]["occupants"]
                                            for row in world["original_economy"]["houses"]) and
                entry["kind"] == ("start" if sequence == 0 else "autosave"),
                "Retained Recovery metadata differs from actual saved authority")
        target = initial if sequence == 0 else manual[sequence]
        require(payload.read_bytes() == target.read_bytes(),
                "Retained Recovery payload is not its literal manual checkpoint")
    return {"published": count, "retained_payloads": len(retained),
            "history_sha256": digest(path), "retention": "start plus last five; prior events checked at publication"}


def event_recovery(directory, report, required, sources):
    rows, worlds = checkpoints(directory, report, sources)
    proof = report["checkpoint_recovery"]
    require(proof["contract"] == CONTRACT and proof["history_count"] == 1 and
            proof["checkpoint_limit"] == 32, "Missing/broadened event Recovery QA contract")
    events = proof["verified_events"]
    labels = {event["label"] for event in events}
    require(required <= labels and len(labels) == len(events) <= 32,
            "Required selected event was absent/duplicated in Recovery publication")
    manual = {}
    for sequence, event in enumerate(events, 1):
        label = event["label"]
        require(label in rows and event["sequence"] == sequence and
                event["schema"] == SCHEMA and event["tick"] == worlds[label]["ticks"] and
                event["canonical_payload_exact"] is True and event["fresh_restore_exact"] is True and
                Path(event["filename"]).name == event["filename"],
                "Event Recovery publication lost exact schema/epoch/fresh-restore proof")
        manual[sequence] = leaf(directory, rows[label]["file"])
    retained = history(directory / "checkpoint-recovery", len(events), manual, sources, manual[1])
    # Bind actual suffix filenames as well as payload bytes to the publication manifest.
    metadata = read(next((directory / "checkpoint-recovery").glob("recovery/history-*/history.json")))
    by_sequence = {event["sequence"]: event for event in events}
    require(all(row["sequence"] == 0 or row["filename"] == by_sequence[row["sequence"]]["filename"]
                for row in metadata["checkpoints"]), "Recovery filename differs from its published event")
    return rows, worlds, retained


def report_contract(report, replay):
    require(report["status"] == "PASS" and report["rule"] == RULE and report["schema"] == SCHEMA and
            report["policy"] == 3 and report["irrigated_harvest"] == HARVEST and
            report["irrigated_harvest_revision"] == 1 and
            report["source_bytes_mode_mtime_unchanged"] is True and
            report["fresh_restore_continuation_exact"] is True and
            report["separate_process_replay_exact"] is replay and
            report["native_metal"] == report["human_acceptance"] == "NOT_RUN",
            "Report lost the explicit generation, evidence tier or process boundary")
    if RULE == 38:
        require(report["quality_aware_food_dispatch"] == QUALITY and report["quality_aware_food_dispatch_revision"] == 1,
                "Report lost its exact separate Rule38 contract")


def empty_paid_seed(world):
    _, origin = totals(world)
    require(world["ticks"] == 0 and
            not any(row["house"]["occupants"] for row in world["original_economy"]["houses"]) and
            not any(origin) and all(row["produced"] == row["imported"] == row["consumed"] == 0
                                    for row in world["original_goods"]["totals"]),
            "Paid seed imported population or already-earned Food/luxury output")


def audit_visual(directory, report, sources, replay):
    report_contract(report, replay)
    require(report["real_ticks"] == 24000 and report["minimum_funds"] >= 0 and
            report["snapshot_stock_or_population_injection"] is False,
            "Visual event mode bypassed duration/paid input boundaries")
    rows, worlds, recovery = event_recovery(directory, report, VISUAL_LABELS, sources)
    empty_paid_seed(worlds["paid-city"])
    for label in VISUAL_LABELS - {"paid-city", "final"}:
        observed = report["coverage"][label]
        require(all(rows[label][key] == observed[key] for key in ("tick", "domain", "id", "state")),
                "Selected event differs from its independently earned exact epoch/identity")
    for label in ("food-grower-work-earned", "food-grower-loaded"):
        world = worlds[label]
        identity = rows[label]["id"]
        farm = next(row for row in world["physical_food_growers"]["farms"] if row["courier_id"] == identity)
        trip = farm["latest_trip"]
        require(rows[label]["domain"] == "Courier" and trip and trip["returned_tick"] is None,
                "Grower event lacks the selected actual live worker")
        if label == "food-grower-work-earned":
            require(trip["phase"] == 3 and trip["work_ticks"] > 0 and trip["harvested_tick"] is None,
                    "Grower phase entry was substituted for earned work")
        else:
            actor = next(row for row in world["couriers"] if row["id"] == identity)
            field = next(row for row in world["processed_food"]["fields"] if row["field_id"] == trip["field_id"])
            receipt = next(row for row in world["physical_food_growers"]["fields"] if row["field_id"] == trip["field_id"])
            require(trip["phase"] == 4 and trip["harvested_tick"] is not None and
                    actor["edge_progress"] > 0 and field["harvested"] == receipt["returned"] + 1,
                    "Loaded Grower lacks real moving, uncredited cargo")
    for label, section, collection, domain in (("boat-loaded-moving", "fishing", "boats", "Boat"),
                                               ("hunter-loaded-moving", "hunting", "hunters", "Hunter")):
        actor = next(row for row in worlds[label][section][collection] if row["id"] == rows[label]["id"])
        require(rows[label]["domain"] == domain and actor["cargo"] > 0 and actor["edge_progress"] > 0,
                "Selected Boat/Hunter event lacks real moving loaded cargo")
    for label, collection, identity in (("market-stock-receipt", "markets", "market_id"),
                                         ("warehouse-stock", "warehouses", "warehouse_id")):
        world = worlds[label]
        owner = next(row for row in world["food_warehouses"][collection] if row[identity] == rows[label]["id"])
        require(rows[label]["domain"] == "Building" and
                any(receipt and receipt["tick"] == world["ticks"] and receipt["amount"] > 0
                    for receipt in owner["first_received"]), "Stock checkpoint is not its selected owner's actual receipt epoch")
    for label, role, section, collection, key in (
            ("market-house-receipt", 14, "food_warehouses", "markets", "market_id"),
            ("shop-house-receipt", 33, "food_processing", "shops", "shop_id")):
        world = worlds[label]
        house = next(row for row in world["buildings"] if row["id"] == rows[label]["id"])
        stock = next(row for row in world["processed_food"]["stocks"] if row["building_id"] == house["id"])
        witness = stock["quality_witness"]
        require(rows[label]["domain"] == "Building" and house["kind"] in (6, 39, 40) and witness and
                witness["tick"] == world["ticks"] and sum(witness["accepted"]) > 0,
                "Selected House lacks an actual accepted-arrival witness")
        courier = next(row for row in world["couriers"] if row["id"] == witness["transfer"]["courier_id"])
        require(courier["role"] == role and courier["owner"] in {row[key] for row in world[section][collection]},
                "House receipt is not from its actual Market/FoodShop role")
    world = worlds["warehouse-outbound"]
    identity = rows["warehouse-outbound"]["id"]
    courier = next(row for row in world["couriers"] if row["id"] == identity)
    require(courier["role"] == 34 and courier["edge_progress"] > 0 and
            courier["owner"] in {row["warehouse_id"] for row in world["food_warehouses"]["warehouses"]} and
            any(trip["transfer"]["courier_id"] == identity and
                sum(n - d - r for n, d, r in zip(trip["loaded"], trip["delivered"], trip["returned"])) > 0
                for trip in world["processed_food"]["trips"]), "Warehouse event lacks selected physical outbound cargo")
    mill = next(row for row in worlds["mill-processing"]["food_processing"]["mills"]
                if row["mill_id"] == rows["mill-processing"]["id"])
    require(mill["active"] and mill["active"]["work"] > 0, "Mill checkpoint lacks actual committed work")
    tea = next(row for row in worlds["crop-Tea-growing"]["plantations"]["plants"]
               if row["plant_id"] == rows["crop-Tea-growing"]["id"])
    require(0 < tea["growth"] < 80, "Tea event lacks actual live growth")
    for label, tick in (("paid-city", 0), ("final", 24000),
                        ("year-boundary-9792", 9792), ("year-boundary-19584", 19584)):
        require(worlds[label]["ticks"] == tick, "Required exact seasonal/endpoint epoch changed")
    burning, extinguished, wet = (worlds[label] for label in ("pump-fire", "pump-extinguished", "pump-recovered-wet"))
    pump = rows["pump-fire"]["id"]
    body = next(row for row in burning["buildings"] if row["id"] == pump)
    require(body["fire_until_tick"] > burning["ticks"] and
            all(rows[label]["id"] == pump for label in ("pump-extinguished", "pump-recovered-wet")) and
            any(arrival[0] == extinguished["ticks"] and arrival[1] and arrival[2][2] == pump
                for arrival in extinguished["original_production"]["fire_protection"]["arrivals"]),
            "Pump fire/extinguishing lacks its selected real arrival")
    status = rows["pump-recovered-wet"]["actual"]["pump"]
    require(burning["ticks"] <= extinguished["ticks"] < wet["ticks"] and
            status["id"] == pump and status["supplying"] is True and status["burning"] is False and
            status["phase_tick"] is not None and status["phase_tick"] <= wet["ticks"],
            "Pump restart lacks its actual derived phase27 supply witness")
    return recovery


def audit_luxury(directory, report, sources, replay):
    report_contract(report, replay)
    rows, worlds, recovery = event_recovery(directory, report, set(RECEIPTS), sources)
    empty_paid_seed(worlds["seed"])
    require(len(rows) == 7 and report["snapshot_or_stock_injection"] is False and
            report["format"] == "openemperor-latest-agriculture-luxury-v2" and
            report["recipe"] == "authored-latest-agriculture-xia-complementary-wheat-v2" and
            report["paid_recipe_without_permanent_abort"] is True and report["first_permanent_command_blocker"] is None and
            report["positive_luxury_consumption"] == [True, True, True] and
            report["actual_post_maturity_ticks"] == report["requested_post_maturity_ticks"] == 20400 and
            report["minimum_funds"] >= 0, "Luxury event mode bypassed finite paid consumption/hold gates")
    for label, good in RECEIPTS.items():
        world = worlds[label]
        arrivals = [row for row in report["actual_luxury_arrivals"] if
                    row["tick"] == world["ticks"] and row["good"] == good and
                    row["target_kind"] in (6, 39, 40) and row["arrived_units"] > 0]
        bodies = {row["id"]: row for row in world["buildings"]}
        require(arrivals and any(
            bodies[arrival["target"]]["kind"] == arrival["target_kind"] and
            any(reservation["transfer"] == {"courier_id": arrival["courier"], "sequence": arrival["sequence"]} and
                all(reservation[key] == arrival[key] for key in ("source", "target", "good", "loaded", "delivered", "returned", "phase")) and
                reservation["delivered"] >= arrival["arrived_units"] > 0
                for reservation in world["original_goods"]["reservations"])
            for arrival in arrivals),
                "Luxury checkpoint is not an actual accepted House arrival epoch")
    return recovery


def luxury_commands(creator, replay):
    """Exact accepted continuation; atomic retries keep their actual diagnostics.

    Pre-seed refusal logging is deduplicated by the ordinary player fixture;
    replay earns that diagnostic again at the same loaded tick0. Bind this case
    to the actual saved footprint claims rather than suppressing its record.
    """
    original, repeated = read(creator / "commands.json"), read(replay / "commands.json")
    seed = read(creator / "seed.save.json")["world"]
    sequence = seed["command_sequence"]
    accepted = [row for row in original if row["accepted"]]
    prefix = [row for row in accepted if row["sequence_after"] <= sequence]
    continuation = [row for row in accepted if row["sequence_before"] >= sequence]
    replay_accepted = [row for row in repeated if row["accepted"]]
    require(prefix and continuation and accepted == prefix + continuation and
            prefix[-1]["sequence_after"] == continuation[0]["sequence_before"] == sequence and
            replay_accepted == continuation, "Luxury accepted continuation differs from its actual paid seed boundary")
    for row in accepted + replay_accepted:
        require(row["changed"] and row["sequence_after"] == row["sequence_before"] + 1 and
                row["quoted_cost"] >= 0 and row["funds_after"] == row["funds_before"] - row["quoted_cost"],
                "Accepted luxury command is not an ordinary paid monotone mutation")
    original_refused = [row for row in original if not row["accepted"]]
    replay_refused = [row for row in repeated if not row["accepted"]]
    legacy = "overlaps actual wildlife, a Hunter or its begun edge"
    actor_claim = "Source footprint overlaps an actual actor or its begun edge"
    courier_claim = "Source footprint overlaps an actual courier or its begun edge"
    for row in original_refused + replay_refused:
        # C++ ordinary() also compares the complete before/after World snapshot.
        require(row["accepted"] is False and row["changed"] is False and
                row["sequence_before"] == row["sequence_after"] and row["funds_before"] == row["funds_after"] and
                row.get("retry_after_real_ticks") is True and
                (legacy in row["result"] or row["result"] in (actor_claim, courier_claim)),
                "Refused luxury retry lost its actual atomic result")

    def semantics(row):
        varying = {"tick", "funds_before", "funds_after", "sequence_before", "sequence_after"}
        return json.dumps({key: value for key, value in row.items() if key not in varying}, sort_keys=True)

    require(Counter(map(semantics, original_refused)) == Counter(map(semantics, replay_refused)),
            "Creator/replay omitted or invented a semantic refused attempt")
    later = [row for row in original_refused if row["sequence_before"] >= sequence]
    replay_later = [row for row in replay_refused if row["sequence_before"] > sequence or row["tick"] > seed["ticks"]]
    require(later == replay_later, "Post-seed refusal execution epochs/results differ")
    earlier = [row for row in original_refused if row["sequence_before"] < sequence]
    initial = [row for row in replay_refused if row not in replay_later]
    require(len(earlier) == len(initial), "Initial refused attempts were not actually re-earned")
    for row in earlier:
        matches = [candidate for candidate in initial if semantics(candidate) == semantics(row)]
        require(len(matches) == 1, "Seed retry lost a unique real refusal")
        retry = matches[0]
        require(row["tick"] == retry["tick"] == seed["ticks"] == 0 and
                retry["sequence_before"] == sequence and retry["funds_before"] == seed["treasury"],
                "Seed retry changed its actual saved epoch/budget/sequence")
        cell = row["cell"]
        footprint = [cell] if legacy in row["result"] else [[cell[0] + x, cell[1] + y] for y in range(2) for x in range(2)]

        def claimed(actor):
            if "next" in actor:  # Living wildlife reserves its actual next cell.
                return actor["current"] in footprint or actor["next"] in footprint
            following = actor.get("next")
            if "path" in actor and actor["path_vertex"] + 1 < len(actor["path"]):
                following = actor["path"][actor["path_vertex"] + 1]
            return actor["current"] in footprint or actor["edge_progress"] > 0 and following in footprint

        if legacy in row["result"]:
            require(row["command"] == 0 and any(claimed(actor) for actor in seed["hunting"]["animals"]),
                    "Initial road refusal lacks its actual saved wildlife claim")
        else:
            require(row["command"] == 20, "Neutral source claim attached to a foreign command")
            if row["result"] == actor_claim:
                require(any(claimed(actor) for actor in seed["hunting"]["animals"] + seed["hunting"]["hunters"] + seed["fishing"]["boats"]),
                        "Initial neutral source refusal lacks a real saved typed actor claim")
            else:
                require(any(actor["phase"] != 0 and actor["path"] and
                            (actor["path"][actor["path_vertex"]] in footprint or actor["edge_progress"] > 0 and
                             actor["path_vertex"] + 1 < len(actor["path"]) and actor["path"][actor["path_vertex"] + 1] in footprint)
                            for actor in seed["couriers"]),
                        "Initial neutral source refusal lacks a real saved current/begun courier claim")
    return {"accepted_seed_prefix": len(prefix), "accepted_continuation": len(continuation),
            "atomic_refusals_per_process": len(original_refused), "initial_refusals_reearned": len(earlier),
            "accepted_continuation_exact": True, "refusal_semantics_exact": True}


def main():
    global RULE, SCHEMA, GENERATION_FLAG
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("visual-executable", "luxury-executable", "reconfiguration-executable", "seed-executable", "data"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--quality-aware-food-dispatch", action="store_true")
    args = parser.parse_args()
    if args.quality_aware_food_dispatch:
        RULE, SCHEMA, GENERATION_FLAG = 38, 54, "--quality-aware-food-dispatch"
    if not (args.data / "Model/EmperorBuildingModels.txt").is_file():
        print("SKIP: supplied Original Model data unavailable")
        return 77
    private = ROOT / ".local/agri-mac14"
    private.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="irrigated-checkpoint-events-", dir=private))
    print("Preserved proof directory: " + str(proof), flush=True)
    protected, children = {}, []
    result = {"status": "FAIL", "contract": CONTRACT, "rule": RULE, "schema": SCHEMA,
              "irrigated_harvest": HARVEST, "irrigated_harvest_revision": 1,
              "child_timeout_seconds": 600, "maximum_children": 10,
              "outer_timeout_seconds": 6060, "children": children,
              "native_or_human_acceptance": "NOT_RUN"}
    try:
        data = args.data.resolve(strict=True)
        protected.update({data / name: fingerprint(data / name) for name in INPUTS})
        sources = {name: protected[data / name] for name in INPUTS}
        executables = {name: getattr(args, name + "_executable").resolve(strict=True)
                       for name in ("visual", "luxury", "reconfiguration", "seed")}
        helpers = [Path(__file__), ROOT / "tests/test_checkpoint_recovery_events.py",
                   ROOT / "tests/test_agriculture_reconfiguration_replay.py",
                   ROOT / "tests/test_agriculture_visual_replay.py",
                   ROOT / "tests/test_food_warehouses_replay.py", ROOT / "tests/test_food_processing_replay.py"]
        protected.update({path: fingerprint(path) for path in list(executables.values()) + helpers})
        (proof / "inputs.json").write_text(json.dumps({str(path): props for path, props in protected.items()}, indent=2) + "\n")

        def run(kind, output, label, *flags, **kwargs):
            require(len(children) < 10, "Event proof exceeded its ten-child budget")
            record = {"label": label, "timeout_seconds": 600}
            children.append(record)
            start = time.monotonic()
            try:
                return invoke(executables[kind], data, output, proof, label, *flags, **kwargs)
            finally:
                record["elapsed_seconds"] = time.monotonic() - start
                (proof / "children.json").write_text(json.dumps(children, indent=2) + "\n")

        seed_base = ROOT / ".local/agri-mac10"
        seed_base.mkdir(parents=True, exist_ok=True)
        seed_output = seed_base / ("seed-for-irrigated-events-" + proof.name)
        run("seed", seed_output, "mac10-paid-seed", "--ticks", 24000)
        seed = leaf(seed_output, "plan.json")
        require(read(seed)["format"] == "openemperor-food-warehouses-plan-v1",
                "Only a genuine paid MAC10 command plan may seed the new generation")
        seed_guard = guard(seed_output)
        creator, replay, baseline = (proof / name for name in ("visual-creator", "visual-replay", "visual-baseline"))
        flags = (GENERATION_FLAG, "--ticks", 24000)
        ordinary = run("visual", baseline, f"visual{RULE}-baseline", "--seed-plan", seed, *flags)
        report_contract(ordinary, False)
        checkpoints(baseline, ordinary, sources)
        require("checkpoint_recovery" not in ordinary and "checkpoint_recovery" not in read(baseline / "plan.json"),
                "Ordinary explicit37 baseline unexpectedly enabled event Recovery")
        baseline_guard = guard(baseline)
        a = run("visual", creator, f"visual{RULE}-creator", "--seed-plan", seed, *flags, "--checkpoint-recovery")
        result["visual_recovery"] = audit_visual(creator, a, sources, False)
        compare_reference(creator, baseline, checkpoint_files(ordinary) + ["work.jsonl"])
        old, added = ({row["label"]: row for row in report["checkpoints"]} for report in (ordinary, a))
        require(all(added.get(label) == row for label, row in old.items()), "Event mode changed ordinary37 checkpoint epochs/evidence")
        ordinary_plan, event_plan = read(baseline / "plan.json"), read(creator / "plan.json")
        require(ordinary_plan["scene"] == event_plan["scene"] and
                [row for row in ordinary_plan["events"] if row["type"] == "command"] ==
                [row for row in event_plan["events"] if row["type"] == "command"],
                "Event instrumentation changed the paid37 World command sequence")
        creator_guard = guard(creator)
        b = run("visual", replay, f"visual{RULE}-replay", "--replay", creator, *flags, "--checkpoint-recovery")
        audit_visual(replay, b, sources, True)
        require(a["checkpoints"] == b["checkpoints"], "Visual replay changed event epochs/identities/evidence")
        compare_reference(replay, creator, checkpoint_files(a) + ["work.jsonl", "plan.json"])
        for attack in ("shifted", "missing"):
            hostile = proof / (attack + "-input")
            hostile.mkdir()
            plan = read(creator / "plan.json")
            receipt = next(row for row in plan["events"] if row["type"] == "checkpoint" and row["label"] == "market-stock-receipt")
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
            hostile_guard = guard(hostile)
            run("visual", proof / (attack + "-output"), f"visual{RULE}-" + attack + "-receipt",
                "--replay", hostile, *flags, "--checkpoint-recovery", expected=1, diagnostic=reason)
            require(guard(hostile) == hostile_guard, "Rejected replay changed explicit hostile inputs")
        lux_creator, lux_replay = proof / "luxury-creator", proof / "luxury-replay"
        lux_flags = ("--latest-agriculture", "--latest-plan-v2", GENERATION_FLAG,
                     "--checkpoint-recovery", "--ticks", 30000, "--hold", 20400)
        la = run("luxury", lux_creator, f"luxury{RULE}-creator", *lux_flags)
        result["luxury_recovery"] = audit_luxury(lux_creator, la, sources, False)
        luxury_guard = guard(lux_creator)
        lb = run("luxury", lux_replay, f"luxury{RULE}-replay", *lux_flags,
                 "--input", lux_creator / "seed.save.json", "--expected", lux_creator / "final.save.json")
        audit_luxury(lux_replay, lb, sources, True)
        compare_reference(lux_replay, lux_creator, checkpoint_files(la))
        require(la["checkpoints"] == lb["checkpoints"], "Luxury replay changed actual receipt/maturity epochs")
        result["luxury_commands"] = luxury_commands(lux_creator, lux_replay)
        reconfig_creator, reconfig_replay = proof / "reconfiguration-creator", proof / "reconfiguration-replay"
        ra = run("reconfiguration", reconfig_creator, f"reconfiguration{RULE}-creator",
                 GENERATION_FLAG, "--seed-plan", seed, "--ticks", 24000)
        report_contract(ra, False)
        result["reconfiguration_audit"] = audit_reconfiguration(reconfig_creator, False, RULE == 37, RULE == 38)
        rr, rw = checkpoints(reconfig_creator, ra, sources)
        require(len(rr) == 20 and ra["real_ticks"] == 24000, "Reconfiguration omitted one of its20 actual paid states")
        manual = {number: leaf(reconfig_creator, row["file"]) for number, row in enumerate(ra["checkpoints"], 1)}
        result["reconfiguration_recovery"] = history(reconfig_creator / "recovery", 20, manual, sources,
                                                       leaf(reconfig_creator, rr["paid-seed"]["file"]))
        reconfiguration_guard = guard(reconfig_creator)
        rb = run("reconfiguration", reconfig_replay, f"reconfiguration{RULE}-replay",
                 GENERATION_FLAG, "--replay", reconfig_creator, "--ticks", 24000)
        report_contract(rb, True)
        audit_reconfiguration(reconfig_replay, True, RULE == 37, RULE == 38)
        checkpoints(reconfig_replay, rb, sources)
        replay_manual = {number: leaf(reconfig_replay, row["file"]) for number, row in enumerate(rb["checkpoints"], 1)}
        history(reconfig_replay / "recovery", 20, replay_manual, sources,
                leaf(reconfig_replay, "paid-seed.save.json"))
        require(ra["checkpoints"] == rb["checkpoints"], "Reconfiguration replay changed actual pending-drain/Pump epochs")
        compare_reference(reconfig_replay, reconfig_creator, checkpoint_files(ra) + ["work.jsonl", "plan.json"])
        require(guard(seed_output) == seed_guard and guard(baseline) == baseline_guard and
                guard(creator) == creator_guard and guard(lux_creator) == luxury_guard and
                guard(reconfig_creator) == reconfiguration_guard,
                "Independent replay changed protected creator/seed evidence")
        require(len(children) == 10 and protected == {path: fingerprint(path) for path in protected},
                "Child budget or read-only source/executable/helper guard changed")
        result.update({"status": "PASS", "source_inputs_preserved": True,
                       "creator_evidence_preserved": True, "canonical_independent_replays_exact": True,
                       f"visual_ordinary{RULE}_checkpoints_exact": len(ordinary["checkpoints"]),
                       "required_visual_events": sorted(VISUAL_LABELS), "actual_luxury_goods": [2, 3, 5],
                       "claim_scope": f"Explicit{RULE} canonical Save/Recovery APIs at earned states; native F5/F9 and human acceptance NOT_RUN"})
        print(f"PASS: Rule{RULE}/schema{SCHEMA} actual event checkpoints, bounded Recovery and independent replay")
        return 0
    except Exception as error:
        result["reason"] = str(error)
        (proof / "failure.json").write_text(json.dumps(result, indent=2) + "\n")
        print("FAIL: " + str(error))
        return 1
    finally:
        result["protected_inputs_after_attempt"] = {str(path): fingerprint(path) if path.is_file() else None
                                                    for path in protected}
        (proof / "result.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
