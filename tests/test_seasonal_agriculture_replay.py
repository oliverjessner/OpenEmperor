#!/usr/bin/env python3
"""Verify earned nine-plant seasons and a separate-process exact continuation."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
YEAR_TICKS = 9792
CONTINUATION_TICKS = 80000


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def fingerprint(path):
    info = path.stat()
    return (info.st_size, info.st_mode, info.st_mtime_ns,
            hashlib.sha256(path.read_bytes()).hexdigest())


def load(path):
    return json.loads(path.read_text())


def snapshot(document):
    require(document["schema_version"] == 48 and document["rules"]["version"] == 32,
            "Checkpoint changed exact Rule32/schema48")
    require(document["rules"]["seasonal_agriculture"] == "authored-seasonal-agriculture-v1" and
            document["rules"]["seasonal_agriculture_revision"] == 1,
            "Checkpoint lacks exact authored seasonal identity")
    world = document["world"]
    require("agriculture" not in world and "fishing_food" not in world,
            "Seasonal checkpoint duplicated the sole seven-kind Food ledger")
    season = world["seasonal_agriculture"]
    require(season["revision"] == 1 and not {"calendar", "year", "month", "season"}.intersection(season),
            "Seasonal state introduced a second advancing calendar")
    fields = {row["field_id"]: row for row in world["hunting_food"]["fields"]}
    physical = {row["field_id"]: row for row in world["physical_food_growers"]["fields"]}
    work = {row["field_id"]: row for row in world["multi_field_agriculture"]["fields"]}
    rows = {row["field_id"]: row for row in season["fields"]}
    require(fields.keys() == physical.keys() == work.keys() == rows.keys(),
            "Seasonal source, crop, lifetime work and actual receipt identities differ")
    picked = [0] * 7
    returned = [0] * 7
    grower_cargo = [0] * 7
    held = [0] * 7
    cart_cargo = [0] * 7
    consumed = [0] * 7
    departed = [0] * 7
    arrived = [0] * 7
    cart_returned = [0] * 7
    for identity, field in fields.items():
        row = rows[identity]
        current = work[identity]["productive_ticks"] - row["wasted_work"] - 80 * field["harvested"]
        require(0 <= current <= 80 and (row["ready_tick"] is not None) == (current == 80),
                "Seasonal earned/wasted/harvested work equation failed")
        require(row["harvest_work"] == 5 * field["harvested"] + row["wasted_harvest_work"] + row["harvest_progress"] and
                0 <= row["harvest_progress"] < 5, "Physical harvest labor equation failed")
        picked[field["crop"]] += field["harvested"]
        returned[field["crop"]] += physical[identity]["returned"]
    for farm in world["physical_food_growers"]["farms"]:
        trip = farm["latest_trip"]
        if trip and trip["harvested_tick"] is not None and trip["returned_tick"] is None:
            grower_cargo[trip["crop"]] += 1
    for crop in range(5):
        require(picked[crop] == returned[crop] + grower_cargo[crop],
                "A field harvest is neither actual Grower cargo nor a physical Farm receipt")
    for quay in world["fishing"]["quays"]:
        returned[5] += quay["unloaded"]
    for tent in world["hunting"]["tents"]:
        returned[6] += tent["unloaded"]
    for stock in world["hunting_food"]["stocks"]:
        require(len(stock["amounts"]) == 7 and all(amount >= 0 for amount in stock["amounts"]),
                "Invalid sole Food stock")
        for crop in range(7):
            held[crop] += stock["amounts"][crop]
            consumed[crop] += stock["consumed"][crop]
            departed[crop] += stock["departed"][crop]
            arrived[crop] += stock["received"][crop]
            cart_returned[crop] += stock["returned"][crop]
    for trip in world["hunting_food"]["trips"]:
        for crop in range(7):
            remaining = trip["loaded"][crop] - trip["delivered"][crop] - trip["returned"][crop]
            require(0 <= remaining <= 4, "Impossible physical Food cart cargo")
            cart_cargo[crop] += remaining
    for crop in range(7):
        require(returned[crop] == held[crop] + cart_cargo[crop] + consumed[crop],
                "Food origin was credited before a genuine source return")
        require(departed[crop] == arrived[crop] + cart_returned[crop] + cart_cargo[crop],
                "Physical Food transfer equation failed")
    plants = {row["plant_id"]: row for row in world["plantations"]["plants"]}
    for row in season["plants"]:
        plant = plants[row["plant_id"]]
        require(plant["productive_ticks"] == 80 * plant["harvested"] + row["wasted_work"] + plant["growth"],
                "Plantation lifetime work equation failed")
        require(plant["grown"] == plant["harvested"] + row["expired_ready"] + (row["ready_tick"] is not None),
                "Plantation picked/ready/expired equation failed")

    buildings = {body["id"]: body for body in world["buildings"]}
    actors = {actor["id"]: actor for actor in world["couriers"]}
    hemp_rows = season["hemp_farms"]
    base_rows = world["original_production"]["hemp_farms"]
    hemp = {row["farm_id"]: row for row in hemp_rows}
    base_hemp = {row["farm_id"]: row for row in base_rows}
    paid_hemp = {body["id"] for body in world["buildings"] if body["kind"] == 14}
    require(len(hemp) == len(hemp_rows) and len(base_hemp) == len(base_rows) and
            hemp.keys() == base_hemp.keys() == paid_hemp,
            "Seasonal Hemp has missing, duplicate or foreign paid Farm identities")
    bound_fields = [row["field_id"] for row in base_rows if row["field_id"] is not None]
    require(len(bound_fields) == len(set(bound_fields)) and set(bound_fields) ==
            {body["id"] for body in world["buildings"] if body["kind"] == 15},
            "Seasonal Hemp lost its uniquely paid Field bindings")
    channels = [row["courier_id"] for row in hemp_rows]
    require(len(channels) == len(set(channels)) and set(channels) ==
            {actor["id"] for actor in world["couriers"] if actor["role"] == 31},
            "Seasonal Hemp has missing, duplicate or orphan movement channels")
    for identity, row in hemp.items():
        source, base = buildings[identity], base_hemp[identity]
        require(source["placed"] and 0 <= source["progress"] <= 80,
                "Hemp work lacks its paid body or bounded actual progress")
        for key in ("productive_ticks", "picked", "wasted_work", "grown", "expired_ready", "spawned", "retired"):
            require(isinstance(row[key], int) and row[key] >= 0, "Negative or non-integer Hemp lifetime history")
        trip = row["latest_trip"]
        live = trip is not None and trip["returned_tick"] is None
        cargo = int(live and trip["picked_tick"] is not None)
        require(row["productive_ticks"] == 80 * row["picked"] + row["wasted_work"] + source["progress"],
                "Hemp earned/picked/wasted/current-work equation failed")
        require(row["grown"] == row["picked"] + row["expired_ready"] + (row["ready_tick"] is not None) and
                (row["ready_tick"] is not None) == (source["progress"] == 80),
                "Hemp picked/ready/expired cycle equation failed")
        require(row["picked"] == base["harvested"] + cargo,
                "Hemp pickup became a source receipt before its actual physical return")
        require(row["spawned"] == row["retired"] + int(live),
                "Hemp allocated more than one live Grower or lost actual retirement history")
        actor = actors[row["courier_id"]]
        require(actor["owner"] == identity and actor["role"] == 31 and actor["cargo"] == actor["reserved"] == 0,
                "Hemp movement channel changed owner/role or duplicated typed cargo in legacy mirrors")
        if trip is not None:
            require(trip["field_id"] == base["field_id"], "Hemp worker changed its uniquely paid Field")
        if base["field_id"] is None:
            require(row["productive_ticks"] == row["grown"] == row["picked"] == row["wasted_work"] == 0 and
                    trip is None, "Hemp gained work or cargo without a paid Field")
    return world


def report(value):
    require(value["status"] == "PASS" and value["rule"] == 32 and value["schema"] == 48,
            "Seasonal positive acceptance failed exact generation")
    require(value["complete_calendar_years"] >= 5, "Fewer than five complete calendar years")
    require(value["minimum_funds"] >= 0 and value["actual_scene"]["treasury"] >= 0,
            "Seasonal city exhausted real Funds during the paid continuation")
    rows = value["nine_type_seasonal_production"]
    require(len(rows) == 9 and len({row["kind"] for row in rows}) == 9,
            "Missing or duplicate seasonal plant contributors")
    for row in rows:
        require(row["actual_productive_work"] > 0 and row["actual_source_returns"] > 0 and
                row["ready_observed"] and row["offseason_work_veto_observed"],
                "Plant lacks genuine seasonal work/readiness/veto/source-return witnesses")
    require(len(value["actual_material_warehouse_arrivals"]) == 4 and
            all(value["actual_material_warehouse_arrivals"]), "A seasonal material skipped its physical Warehouse arrival")
    require(value["actual_material_transfers"] and value["actual_food_arrivals"] and value["actual_house_consumption"],
            "Seasonal output lacks actual downstream physical transfer or consumption")
    require(value["source_bytes_mode_mtime_unchanged"] and value["recovery_exact"] and
            value["fresh_restore_continuation_exact"], "Source preservation or exact Save/Recovery witness failed")
    require(value["unchanged_supplied_model"] and not value["snapshot_stock_or_population_injection"],
            "Seasonal city changed supplied needs or injected production authority")

    # Independently sum current-row House debits, rather than accepting the
    # report's global lifetime totals as a witness of later yearly demand.
    luxury_index = {2: 0, 3: 1, 5: 2}  # Tea, Silk, Lacquerware
    luxury_by_year = {}
    for event in value["actual_luxury_consumption"]:
        require(event["good"] in luxury_index and event["amount"] > 0,
                "Unknown or empty luxury House debit")
        kind, house_type = event["good"], event["type"]
        eligible = ((kind == 2 and house_type in (10, 17)) or
                    (kind == 3 and 14 <= house_type <= 17) or
                    (kind == 5 and 15 <= house_type <= 17))
        require(eligible and event["after_tick"]["occupants"] > 0 and
                event["after_tick"]["type"] == house_type,
                "Luxury sink lacks a genuinely occupied current-row consumer in the unchanged model")
        year = (event["tick"] - 1) // YEAR_TICKS + 1
        luxury_by_year.setdefault(year, [0] * 3)[luxury_index[kind]] += event["amount"]

    annual = value["annual_physical_production_and_consumption"]
    require(len(annual) >= 5, "Missing five complete calendar-year boundaries")
    prior_returns = [0] * 9
    prior_year = None
    streak = longest = 0
    run_first = longest_first = longest_last = None
    for row in annual:
        require(row["tick"] == row["year"] * YEAR_TICKS and
                (prior_year is None or row["year"] == prior_year + 1),
                "Annual evidence skipped or relabelled a real calendar boundary")
        returned = row["actual_source_returns_this_year"]
        consumed = row["actual_luxury_consumed_this_year"]
        require(len(returned) == 9 and len(consumed) == 3 and
                all(amount >= 0 for amount in returned + consumed), "Invalid annual production/consumption shape")
        require(returned == [current - previous for current, previous in zip(row["source_returns"], prior_returns)],
                "Annual source-return deltas differ from actual cumulative receipts")
        require(consumed == luxury_by_year.get(row["year"], [0] * 3),
                "Annual luxury totals differ from individually witnessed current-row House debits")
        active = all(returned) and all(consumed)
        require(row["all_nine_and_luxuries_active_this_year"] == active,
                "Annual active flag contradicted real receipt and consumption amounts")
        # Startup may take several years. Count a run only when its entire
        # calendar interval was observed and every required source/sink is live.
        complete_interval = row["tick"] - YEAR_TICKS >= value["begin_tick"]
        if complete_interval and active:
            if not streak:
                run_first = row["year"]
            streak += 1
            if streak > longest:
                longest = streak
                longest_first, longest_last = run_first, row["year"]
        else:
            streak = 0
            run_first = None
        prior_returns = row["source_returns"]
        prior_year = row["year"]
    require(longest >= 5 and value["longest_consecutive_all_nine_and_luxury_years"] >= 5,
            "No five consecutive full years with all nine real returns and Tea/Silk/Lacquerware consumption")
    return {"first_qualifying_year": longest_first, "last_qualifying_year": longest_last,
            "consecutive_qualifying_years": longest}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--map", default="Cities/Chengdu.map")
    args = parser.parse_args()
    data = args.data.resolve()
    sources = [data / "Model/EmperorBuildingModels.txt", data / args.map]
    if not all(path.is_file() for path in sources):
        print("NOT_RUN: supplied Original map/model unavailable")
        return 77
    before = {path: fingerprint(path) for path in sources}
    parent = ROOT / ".local/agri-mac07"
    parent.mkdir(parents=True, exist_ok=True)
    proof = Path(tempfile.mkdtemp(prefix="positive-replay-", dir=parent))
    executable = args.executable.resolve(strict=True)
    annual_checks = []
    for label, extra in (("creator", []), ("replay", ["--replay", proof / "creator/seed.save.json",
                                                     "--expected", proof / "creator/final.save.json"])):
        output = proof / label
        command = [str(executable), "--data", str(data), "--map", args.map,
                   "--output", str(output), "--ticks", str(CONTINUATION_TICKS), *map(str, extra)]
        (proof / f"{label}.command.json").write_text(json.dumps(command, indent=2) + "\n")
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=600)
        (proof / f"{label}.log").write_text(result.stdout + result.stderr)
        require(result.returncode == 0, f"{label} failed; retained {proof}: {result.stdout}{result.stderr}")
        value = load(output / "report.json")
        checked_years = report(value)
        annual_checks.append({"process": label, **checked_years})
        start = snapshot(load(output / "seed.save.json"))
        final = snapshot(load(output / "final.save.json"))
        require(final["ticks"] - start["ticks"] == value["real_ticks"] == CONTINUATION_TICKS,
                "Requested 80k continuation is not backed by the sole saved World tick")
        for checkpoint in output.glob("*.save.json"):
            snapshot(load(checkpoint))
        if label == "replay":
            require(value["separate_process_replay_exact"], "No separate-process exact replay witness")
    require((proof / "creator/final.save.json").read_bytes() == (proof / "replay/final.save.json").read_bytes(),
            "Independent canonical endpoints differ")
    require(before == {path: fingerprint(path) for path in sources}, "Supplied Original source was modified")
    (proof / "independent-annual-checks.json").write_text(json.dumps(annual_checks, indent=2) + "\n")
    checked = ", ".join(f"{row['process']} years {row['first_qualifying_year']}-{row['last_qualifying_year']}"
                        for row in annual_checks)
    print(f"PASS: nine paid seasonal chains, five complete mixed years and separate-process 80k continuation; {checked}; {proof}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
